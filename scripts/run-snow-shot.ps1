[CmdletBinding()]
param(
    [ValidateSet("windows-msvc-debug", "snow-shot-msvc-release")]
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
    & (Join-Path $PSScriptRoot "build.ps1") -Preset $Preset -Target snow_shot -Clean:$Clean
    if ($LASTEXITCODE -ne 0) { throw "Snow Shot build failed." }
}

$executable = Resolve-SnowExecutable -Preset $Preset -Name "snow_shot.exe"

if ($Detached) {
    Start-Process -FilePath $executable.FullName -WorkingDirectory $executable.DirectoryName | Out-Null
} else {
    Push-Location $executable.DirectoryName
    try { & $executable.FullName } finally { Pop-Location }
}
