[CmdletBinding()]
param(
    [ValidateSet("windows-msvc-debug", "windows-msvc-performance", "snow-shot-msvc-release", "snow-shot-msvc-fast")]
    [string]$Preset = "windows-msvc-debug",
    [string]$Target = "snow-all",
    [switch]$Clean,
    [switch]$Fresh,
    [switch]$SkipBootstrap
)

$ErrorActionPreference = "Stop"
. (Join-Path $PSScriptRoot "snow-build-environment.ps1")
$toolchain = Set-SnowBuildEnvironment
$repoRoot = $toolchain.RepoRoot
$buildDirectory = Join-Path $repoRoot "build/$Preset"

if ($Clean -and (Test-Path -LiteralPath $buildDirectory)) {
    $resolvedBuild = (Resolve-Path -LiteralPath $buildDirectory).Path
    $resolvedRoot = (Resolve-Path -LiteralPath (Join-Path $repoRoot "build")).Path
    if (-not $resolvedBuild.StartsWith($resolvedRoot, [System.StringComparison]::OrdinalIgnoreCase)) {
        throw "Refusing to remove a build directory outside $resolvedRoot"
    }
    Remove-Item -LiteralPath $resolvedBuild -Recurse -Force
}

if (-not $SkipBootstrap) {
    & (Join-Path $PSScriptRoot "bootstrap.ps1") -SkipDependencyInstall -SkipVcpkgInstall -Qt6Dir $toolchain.Qt6Dir -VcpkgVariants Static
    if ($LASTEXITCODE -ne 0) {
        throw "Build environment bootstrap failed."
    }
}

Push-Location $repoRoot
try {
    $cachePath = Join-Path $buildDirectory "CMakeCache.txt"
    $configureArguments = if ($Fresh -or -not (Test-SnowCacheAlignment -CachePath $cachePath -Preset $Preset)) {
        @("--fresh", "--preset", $Preset)
    }
    else {
        @("--preset", $Preset)
    }
    & cmake @configureArguments
    if ($LASTEXITCODE -ne 0) { throw "CMake configure failed for preset $Preset." }

    $buildArguments = @("--build", "--preset", "build-$Preset", "--parallel")
    if (-not [string]::IsNullOrWhiteSpace($Target)) {
        $buildArguments += @("--target", $Target)
    }
    & cmake @buildArguments
    if ($LASTEXITCODE -ne 0) {
        throw "CMake build failed for preset $Preset."
    }
}
finally {
    Pop-Location
}
