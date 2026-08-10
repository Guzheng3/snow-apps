[CmdletBinding()]
param(
    [ValidateSet("Debug", "Release")]
    [string]$Config = "Debug",
    [switch]$Clean,
    [switch]$NoBuild,
    [switch]$Detached
)

$ErrorActionPreference = "Stop"
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
. (Join-Path $PSScriptRoot "snow-build-environment.ps1")
$toolchain = Set-SnowBuildEnvironment
$preset = if ($Config -eq "Debug") { "windows-msvc-debug" } else { "windows-msvc-performance" }
if (-not $NoBuild) {
    & (Join-Path $PSScriptRoot "build.ps1") -Preset $preset -Target ant_design_qt -Clean:$Clean
    if ($LASTEXITCODE -ne 0) { throw "Ant Design Qt build failed." }
}
Write-Output "Ant Design Qt target is available in build\$preset (Qt $($toolchain.QtVersion), MSVC $($toolchain.MsvcToolset))."
