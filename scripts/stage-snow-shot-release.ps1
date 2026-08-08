[CmdletBinding()]
param([Parameter(ValueFromRemainingArguments = $true)][object[]]$Arguments)
$ErrorActionPreference = "Stop"
if ($Arguments.Count -eq 0) {
    & (Join-Path $PSScriptRoot "package-snow-shot.ps1")
}
else {
    & (Join-Path $PSScriptRoot "package-snow-shot.ps1") @Arguments
}
exit $LASTEXITCODE
