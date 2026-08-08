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
$projectRoot = Join-Path $repoRoot "ant_design_qt"
$buildRoot = Join-Path $repoRoot "build/ant-design-theme"
$configName = $Config.ToLowerInvariant()
$buildDirectory = Join-Path $buildRoot $configName
$projectFile = Join-Path $projectRoot "examples/theme-demo/theme-demo.pro"
$executable = Join-Path $buildDirectory "theme-demo.exe"

if (-not (Test-Path -LiteralPath $projectFile)) {
    throw "Theme demo project was not found at $projectFile"
}

$qtRoot = $env:QTDIR
if (-not $qtRoot -and $env:Qt6_DIR) {
    $qtRoot = (Resolve-Path (Join-Path $env:Qt6_DIR "../../..")).Path
}
$qmakePath = $null
$qmake = Get-Command qmake.exe -ErrorAction SilentlyContinue
if ($qmake) { $qmakePath = $qmake.Source }
if (-not $qmakePath -and $qtRoot) {
    $candidate = Join-Path $qtRoot "bin/qmake.exe"
    if (Test-Path -LiteralPath $candidate) { $qmakePath = $candidate }
}
if (-not $qmakePath) { throw "qmake was not found. Set QTDIR or add the Qt bin directory to PATH." }

if ($Clean -and (Test-Path -LiteralPath $buildDirectory)) {
    Remove-Item -LiteralPath $buildDirectory -Recurse -Force
}
New-Item -ItemType Directory -Path $buildDirectory -Force | Out-Null

if (-not $NoBuild) {
    Push-Location $buildDirectory
    try {
        & $qmakePath $projectFile "CONFIG+=$configName"
        if ($LASTEXITCODE -ne 0) { throw "qmake failed for the theme demo." }
        & nmake
        if ($LASTEXITCODE -ne 0) { throw "nmake failed for the theme demo." }
    }
    finally { Pop-Location }
}

if (-not (Test-Path -LiteralPath $executable)) {
    throw "Theme demo executable was not found at $executable. Build it first or omit -NoBuild only after a successful build."
}
if ($Detached) { Start-Process -FilePath $executable -WorkingDirectory $buildDirectory | Out-Null }
else { Push-Location $buildDirectory; try { & $executable } finally { Pop-Location } }
