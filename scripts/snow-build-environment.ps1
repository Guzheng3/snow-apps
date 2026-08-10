Set-StrictMode -Version Latest

$script:SnowRepoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$script:SnowQtVersion = "6.11.1"
$script:SnowMsvcToolset = "14.51"
$script:SnowRustToolchain = "1.97.1"
$script:SnowRustTarget = "x86_64-pc-windows-msvc"

function Resolve-SnowQtStaticDir {
    $candidates = @(
        $env:SNOW_QT_STATIC_DIR,
        $env:Qt6_DIR,
        (Join-Path $env:ProgramFiles "Qt\6.11.1\msvc2026_64-static\lib\cmake\Qt6"),
        "C:\Qt\6.11.1\msvc2026_64-static\lib\cmake\Qt6"
    ) | Where-Object { -not [string]::IsNullOrWhiteSpace($_) }

    foreach ($candidate in $candidates) {
        $resolved = [System.IO.Path]::GetFullPath($candidate)
        $config = Join-Path $resolved "Qt6Config.cmake"
        if (-not (Test-Path -LiteralPath $config -PathType Leaf)) { continue }
        $versionFiles = @(
            (Join-Path $resolved "Qt6ConfigVersion.cmake"),
            (Join-Path $resolved "Qt6ConfigVersionImpl.cmake")
        ) | Where-Object { Test-Path -LiteralPath $_ -PathType Leaf }
        $versionText = ($versionFiles | ForEach-Object { Get-Content -LiteralPath $_ -Raw }) -join "`n"
        if ($versionText -match 'PACKAGE_VERSION\s+"6\.11\.1"') { return $resolved }
    }
    throw "Qt $script:SnowQtVersion static CMake package was not found. Set SNOW_QT_STATIC_DIR."
}

function Add-SnowMsvcToolsToPath {
    $visualStudioRoot = @(
        $env:VSINSTALLDIR,
        "C:\VS2026\BuildTools",
        "C:\Program Files\Microsoft Visual Studio\2026\BuildTools"
    ) | Where-Object { -not [string]::IsNullOrWhiteSpace($_) } |
        Where-Object { Test-Path -LiteralPath (Join-Path $_ "VC\Tools\MSVC") } |
        Select-Object -First 1
    if (-not $visualStudioRoot) {
        throw "Visual Studio 2026 Build Tools were not found."
    }

    $env:VSINSTALLDIR = [System.IO.Path]::GetFullPath($visualStudioRoot)
    $env:VCINSTALLDIR = Join-Path $env:VSINSTALLDIR "VC"
    $msvcTools = Get-ChildItem -LiteralPath (Join-Path $env:VCINSTALLDIR "Tools\MSVC") -Directory |
        Where-Object { $_.Name -match '^14\.51' } |
        Sort-Object Name -Descending |
        Select-Object -First 1
    if (-not $msvcTools) {
        throw "MSVC toolset $script:SnowMsvcToolset was not found under $env:VCINSTALLDIR."
    }

    $env:VCToolsInstallDir = "$($msvcTools.FullName)\"
    $msvcBin = Join-Path $msvcTools.FullName "bin\Hostx64\x64"
    $env:Path = "$msvcBin;$env:Path"
    return $msvcBin
}

function Set-SnowBuildEnvironment {
    $qtDir = Resolve-SnowQtStaticDir
    $env:SNOW_QT_STATIC_DIR = $qtDir
    $env:Qt6_DIR = $qtDir
    $env:QTDIR = [System.IO.Path]::GetFullPath((Join-Path $qtDir "..\..\.."))
    $env:VCPKG_ROOT = Join-Path $script:SnowRepoRoot ".tools\vcpkg"
    Add-SnowMsvcToolsToPath | Out-Null
    $libclang = Join-Path $script:SnowRepoRoot ".tools\llvm\bin"
    if (Test-Path -LiteralPath (Join-Path $libclang "libclang.dll") -PathType Leaf) {
        $env:LIBCLANG_PATH = $libclang
    }
    $env:Path = "$(Join-Path $env:QTDIR 'bin');$(Join-Path $libclang '..');$env:Path"
    return [pscustomobject]@{
        RepoRoot = $script:SnowRepoRoot
        Qt6Dir = $qtDir
        QtVersion = $script:SnowQtVersion
        MsvcToolset = $script:SnowMsvcToolset
        RustToolchain = $script:SnowRustToolchain
        RustTarget = $script:SnowRustTarget
        VcpkgRoot = $env:VCPKG_ROOT
    }
}

function Resolve-SnowPreset {
    param([ValidateSet("Debug", "Release", "Performance", "Fast")][string]$Configuration)
    switch ($Configuration) {
        "Debug" { return "windows-msvc-debug" }
        "Release" { return "snow-shot-msvc-release" }
        "Performance" { return "windows-msvc-performance" }
        "Fast" { return "snow-shot-msvc-fast" }
    }
}

function Invoke-SnowCMake {
    param([Parameter(Mandatory = $true)][string[]]$Arguments)
    Push-Location $script:SnowRepoRoot
    try {
        & cmake @Arguments
        if ($LASTEXITCODE -ne 0) { throw "CMake failed ($LASTEXITCODE): cmake $($Arguments -join ' ')" }
    }
    finally { Pop-Location }
}

function Test-SnowCacheAlignment {
    param(
        [Parameter(Mandatory = $true)][string]$CachePath,
        [Parameter(Mandatory = $true)][string]$Preset
    )
    if (-not (Test-Path -LiteralPath $CachePath -PathType Leaf)) { return $false }
    $cache = Get-Content -LiteralPath $CachePath -Raw
    $qtNeedle = [regex]::Escape(($env:SNOW_QT_STATIC_DIR -replace '\\', '/'))
    $strictAligned = if ($Preset -eq "windows-msvc-debug") {
        $cache -match '(?m)^SNOW_APPS_STRICT_COMPILE:BOOL=OFF$'
    }
    else { $true }
    return $strictAligned -and
        $cache -match "(?m)^Qt6_DIR:PATH=$qtNeedle$" -and
        $cache -match '(?m)^SNOW_QT_STATIC_DIR:PATH=' -and
        $cache -match '(?m)^VCPKG_TARGET_TRIPLET:.*=x64-windows-static$' -and
        $cache -match '(?m)^CMAKE_GENERATOR_TOOLSET:INTERNAL=host=x64,version=14\.51$'
}

function Resolve-SnowExecutable {
    param(
        [Parameter(Mandatory = $true)][string]$Preset,
        [Parameter(Mandatory = $true)][string]$Name
    )
    $configuration = if ($Preset -eq "windows-msvc-debug") { "Debug" } else { "Release" }
    $buildRoot = Join-Path $script:SnowRepoRoot "build\$Preset"
    $match = Get-ChildItem -LiteralPath $buildRoot -Filter $Name -File -Recurse -ErrorAction SilentlyContinue |
        Where-Object { $_.FullName -match "\\$configuration\\" } |
        Sort-Object FullName |
        Select-Object -First 1
    if (-not $match) { throw "$Name was not found under $buildRoot ($configuration)." }
    return $match
}
