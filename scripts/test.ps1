[CmdletBinding()]
param(
    [ValidateSet("windows-msvc-debug", "windows-msvc-performance")]
    [string]$Preset = "windows-msvc-debug",
    [switch]$Interactive,
    [switch]$SkipBuild
)

$ErrorActionPreference = "Stop"
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path

if (-not $SkipBuild) {
    & (Join-Path $PSScriptRoot "build.ps1") -Preset $Preset -Target "" -SkipBootstrap
    if ($LASTEXITCODE -ne 0) {
        throw "Build failed before tests."
    }
}

$testPreset = if ($Interactive) {
    if ($Preset -ne "windows-msvc-debug") {
        throw "Interactive tests use the windows-msvc-debug preset."
    }
    "test-windows-msvc-interactive"
}
else {
    "test-$Preset"
}

Push-Location $repoRoot
try {
    & ctest --preset $testPreset
    if ($LASTEXITCODE -ne 0) {
        throw "Tests failed for preset $testPreset."
    }
}
finally {
    Pop-Location
}
