[CmdletBinding()]
param(
    [ValidateSet("windows-msvc-debug", "windows-msvc-performance")]
    [string]$Preset = "windows-msvc-debug",
    [switch]$Clean,
    [switch]$NoBuild,
    [switch]$Detached
)

$ErrorActionPreference = "Stop"
. (Join-Path $PSScriptRoot "snow-build-environment.ps1")
$toolchain = Set-SnowBuildEnvironment
$repoRoot = $toolchain.RepoRoot
if (-not $NoBuild) {
    & (Join-Path $PSScriptRoot "build.ps1") -Preset $Preset -Target snow_draw_engine_qt_canvas -Clean:$Clean
    if ($LASTEXITCODE -ne 0) { throw "Snow Draw Engine library build failed." }
}
Write-Output "Snow Draw Engine canvas target is available in build\$Preset (Qt $($toolchain.QtVersion), MSVC $($toolchain.MsvcToolset))."
