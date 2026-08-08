[CmdletBinding()]
param(
    [ValidateSet("windows-msvc-debug", "windows-msvc-release", "portable-debug")]
    [string]$Preset = "windows-msvc-debug",
    [switch]$Clean,
    [switch]$SkipBootstrap
)

$ErrorActionPreference = "Stop"
& (Join-Path $PSScriptRoot "build.ps1") -Preset $Preset -Target snow_image_static -Clean:$Clean -SkipBootstrap:$SkipBootstrap
if ($LASTEXITCODE -ne 0) { throw "Snow Image build failed." }
