[CmdletBinding()]
param(
    [string]$Qt6Dir = "",
    [switch]$Reset,
    [switch]$SkipVcpkgInstall,
    [switch]$SkipDependencyInstall
)

$ErrorActionPreference = "Stop"

function Invoke-Checked {
    param(
        [Parameter(Mandatory = $true)][string]$Command,
        [Parameter(Mandatory = $true)][string[]]$Arguments,
        [Parameter(Mandatory = $true)][string]$WorkingDirectory
    )

    Push-Location $WorkingDirectory
    try {
        & $Command @Arguments
        if ($LASTEXITCODE -ne 0) {
            throw "Command failed ($LASTEXITCODE): $Command $($Arguments -join ' ')"
        }
    }
    finally {
        Pop-Location
    }
}

function Require-Command {
    param([Parameter(Mandatory = $true)][string]$Name)
    $command = Get-Command $Name -ErrorAction SilentlyContinue
    if (-not $command) {
        throw "Required command was not found: $Name"
    }
    return $command
}

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$toolsRoot = Join-Path $repoRoot ".tools"
$vcpkgRoot = Join-Path $toolsRoot "vcpkg"
$vcpkgExe = Join-Path $vcpkgRoot "vcpkg.exe"
$vcpkgInstalledRoot = Join-Path $vcpkgRoot "installed"
$vcpkgBaseline = "ffc071e0c08432c60c9b64f00334c0227667931b"
$rustToolchain = "1.93.1"
$rustTarget = "x86_64-pc-windows-msvc"

$git = Require-Command "git"
$cmake = Require-Command "cmake"
$cargo = Require-Command "cargo"
$rustup = Require-Command "rustup"

$cmakeVersionLine = (& $cmake.Source --version | Select-Object -First 1)
if ($cmakeVersionLine -notmatch "cmake version (\d+)\.(\d+)") {
    throw "Unable to determine the installed CMake version: $cmakeVersionLine"
}
if ([int]$Matches[1] -lt 3 -or ([int]$Matches[1] -eq 3 -and [int]$Matches[2] -lt 30)) {
    throw "CMake 3.30 or newer is required; found $cmakeVersionLine"
}

if ($Reset) {
    foreach ($path in @($vcpkgRoot, (Join-Path $repoRoot "build"))) {
        if (Test-Path -LiteralPath $path) {
            $resolved = (Resolve-Path -LiteralPath $path).Path
            if (-not $resolved.StartsWith($repoRoot, [System.StringComparison]::OrdinalIgnoreCase)) {
                throw "Refusing to remove a path outside the repository: $resolved"
            }
            Remove-Item -LiteralPath $resolved -Recurse -Force
        }
    }
}

New-Item -ItemType Directory -Path $toolsRoot -Force | Out-Null

if (-not (Test-Path -LiteralPath $vcpkgExe)) {
    Invoke-Checked -Command $git.Source -Arguments @(
        "clone", "https://github.com/microsoft/vcpkg.git", $vcpkgRoot
    ) -WorkingDirectory $repoRoot
}

$vcpkgGitDirectory = Join-Path $vcpkgRoot ".git"
if (-not (Test-Path -LiteralPath $vcpkgGitDirectory)) {
    throw "Repository-local vcpkg must be a Git checkout so the pinned baseline can be enforced: $vcpkgRoot. Rerun with -Reset."
}
if (Test-Path -LiteralPath $vcpkgGitDirectory) {
    Invoke-Checked -Command $git.Source -Arguments @(
        "fetch", "origin", $vcpkgBaseline, "--depth=1"
    ) -WorkingDirectory $vcpkgRoot
    Invoke-Checked -Command $git.Source -Arguments @(
        "checkout", "--detach", $vcpkgBaseline
    ) -WorkingDirectory $vcpkgRoot
}

if (-not (Test-Path -LiteralPath $vcpkgExe) -and -not $SkipVcpkgInstall) {
    $bootstrap = Join-Path $vcpkgRoot "bootstrap-vcpkg.bat"
    if (-not (Test-Path -LiteralPath $bootstrap)) {
        throw "vcpkg bootstrap script was not found at $bootstrap"
    }
    Invoke-Checked -Command $env:ComSpec -Arguments @(
        "/d", "/c", $bootstrap, "-disableMetrics"
    ) -WorkingDirectory $vcpkgRoot
}

if (-not $SkipDependencyInstall) {
    if (-not (Test-Path -LiteralPath $vcpkgExe)) {
        throw "Repository-local vcpkg is not installed. Rerun without -SkipVcpkgInstall."
    }
    foreach ($triplet in @("x64-windows", "x64-windows-static")) {
        $installVariant = if ($triplet -eq "x64-windows-static") { "static" } else { "dynamic" }
        $tripletInstallRoot = Join-Path $vcpkgInstalledRoot $installVariant
        Invoke-Checked -Command $vcpkgExe -Arguments @(
            "install",
            "--x-manifest-root=$repoRoot",
            "--x-install-root=$tripletInstallRoot",
            "--triplet=$triplet",
            "--overlay-ports=$(Join-Path $repoRoot 'cmake/vcpkg-overlay-ports')",
            "--clean-after-build"
        ) -WorkingDirectory $repoRoot
    }
}

Invoke-Checked -Command $rustup.Source -Arguments @(
    "toolchain", "install", $rustToolchain,
    "--profile", "minimal",
    "--component", "rustfmt",
    "--component", "clippy",
    "--target", $rustTarget
) -WorkingDirectory $repoRoot

Invoke-Checked -Command $cargo.Source -Arguments @(
    "+$rustToolchain", "metadata", "--format-version", "1", "--no-deps"
) -WorkingDirectory (Join-Path $repoRoot "snow-crates")
Invoke-Checked -Command $cargo.Source -Arguments @(
    "+$rustToolchain", "metadata", "--format-version", "1", "--no-deps"
) -WorkingDirectory (Join-Path $repoRoot "snow_draw_engine_qt")

if ([string]::IsNullOrWhiteSpace($Qt6Dir)) {
    $Qt6Dir = $env:SNOW_QT_STATIC_DIR
}
if ([string]::IsNullOrWhiteSpace($Qt6Dir)) {
    $Qt6Dir = $env:Qt6_DIR
}
if ([string]::IsNullOrWhiteSpace($Qt6Dir) -and $env:QTDIR) {
    $Qt6Dir = Join-Path $env:QTDIR "lib\cmake\Qt6"
}
if ([string]::IsNullOrWhiteSpace($Qt6Dir) -or
    -not (Test-Path -LiteralPath (Join-Path $Qt6Dir "Qt6Config.cmake"))) {
    throw "Qt 6.10.3 was not found. Set -Qt6Dir, Qt6_DIR, SNOW_QT_STATIC_DIR, or QTDIR."
}
$qt6VersionFiles = @(
    (Join-Path $Qt6Dir "Qt6ConfigVersion.cmake"),
    (Join-Path $Qt6Dir "Qt6ConfigVersionImpl.cmake")
)
$qt6VersionText = ($qt6VersionFiles |
    Where-Object { Test-Path -LiteralPath $_ } |
    ForEach-Object { Get-Content -LiteralPath $_ -Raw }) -join "`n"
if ([string]::IsNullOrWhiteSpace($qt6VersionText)) {
    throw "Qt 6 version metadata is missing under $Qt6Dir"
}
$qt6VersionMatch = [regex]::Match($qt6VersionText, 'PACKAGE_VERSION\s+"([^"]+)"')
if (-not $qt6VersionMatch.Success -or $qt6VersionMatch.Groups[1].Value -ne "6.10.3") {
    $detectedQtVersion = if ($qt6VersionMatch.Success) { $qt6VersionMatch.Groups[1].Value } else { "unknown" }
    throw "Qt 6.10.3 is required; detected $detectedQtVersion at $Qt6Dir"
}

$libclangDirectory = Join-Path $toolsRoot "llvm\bin"
if (-not (Test-Path -LiteralPath (Join-Path $libclangDirectory "libclang.dll"))) {
    Write-Warning "libclang.dll was not found under $libclangDirectory; Rust bindgen builds may fail."
}

Write-Host "Snow Apps build environment is ready."
Write-Host "Repository: $repoRoot"
Write-Host "CMake: $cmakeVersionLine"
Write-Host "vcpkg: $vcpkgRoot"
Write-Host "vcpkg dynamic installed: $(Join-Path $vcpkgInstalledRoot 'dynamic')"
Write-Host "vcpkg static installed: $(Join-Path $vcpkgInstalledRoot 'static')"
Write-Host "Qt6_DIR: $Qt6Dir"
if (-not [string]::IsNullOrWhiteSpace($env:SNOW_QT_STATIC_DIR)) {
    Write-Host "SNOW_QT_STATIC_DIR: $env:SNOW_QT_STATIC_DIR"
}
Write-Host "Rust: $rustToolchain ($rustTarget)"
