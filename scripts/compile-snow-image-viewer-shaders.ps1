[CmdletBinding()]
param([Parameter(ValueFromRemainingArguments = $true)][object[]]$Arguments)
$ErrorActionPreference = "Stop"
. (Join-Path $PSScriptRoot "snow-build-environment.ps1")
Set-SnowBuildEnvironment | Out-Null
& (Join-Path $PSScriptRoot "../snow_image_viewer/scripts/compile-shaders.ps1") @Arguments
exit $LASTEXITCODE
