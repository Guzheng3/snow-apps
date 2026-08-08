[CmdletBinding()]
param(
    [string]$QtVersion = "6.10.3",
    [Parameter(Mandatory = $true)][string]$InstallPrefix,
    [string]$SourceDirectory = ""
)

$ErrorActionPreference = "Stop"

function Invoke-Checked {
    param(
        [Parameter(Mandatory = $true)][string]$Command,
        [Parameter(Mandatory = $true)][string[]]$Arguments,
        [string]$WorkingDirectory = (Get-Location).Path
    )

    Push-Location $WorkingDirectory
    try {
        & $Command @Arguments
        if ($LASTEXITCODE -ne 0) {
            throw "Command failed ($LASTEXITCODE): $Command $($Arguments -join ' ')"
        }
    }
    finally {
        Pop-Location
    }
}

$installPrefix = [System.IO.Path]::GetFullPath($InstallPrefix)
$qtConfig = Join-Path $installPrefix "lib\cmake\Qt6\Qt6Config.cmake"
if (Test-Path -LiteralPath $qtConfig -PathType Leaf) {
    Write-Output "Static Qt $QtVersion is already available at $installPrefix"
    exit 0
}

if ([string]::IsNullOrWhiteSpace($SourceDirectory)) {
    $SourceDirectory = Join-Path $env:RUNNER_TEMP "qt-everywhere-src-$QtVersion"
}
$sourceDirectory = [System.IO.Path]::GetFullPath($SourceDirectory)
$archivePath = Join-Path ([System.IO.Path]::GetDirectoryName($sourceDirectory)) "qt-everywhere-src-$QtVersion.tar.xz"
$sourceUrl = "https://download.qt.io/official_releases/qt/$($QtVersion.Substring(0, $QtVersion.LastIndexOf('.')))/$QtVersion/single/qt-everywhere-src-$QtVersion.tar.xz"

if (-not (Test-Path -LiteralPath $sourceDirectory -PathType Container)) {
    if (-not (Test-Path -LiteralPath $archivePath -PathType Leaf)) {
        Write-Output "Downloading Qt $QtVersion sources from $sourceUrl"
        Invoke-WebRequest -UseBasicParsing -Uri $sourceUrl -OutFile $archivePath
    }
    New-Item -ItemType Directory -Force -Path (Split-Path -Parent $sourceDirectory) | Out-Null
    Invoke-Checked -Command "tar" -Arguments @("-xf", $archivePath, "-C", (Split-Path -Parent $sourceDirectory))
}

New-Item -ItemType Directory -Force -Path $installPrefix | Out-Null
$configureArguments = @(
    "-static",
    "-release",
    "-static-runtime",
    "-opensource",
    "-confirm-license",
    "-prefix", $installPrefix,
    "-nomake", "tests",
    "-nomake", "examples",
    "-skip", "qt3d",
    "-skip", "qtactiveqt",
    "-skip", "qtandroidextras",
    "-skip", "qtconnectivity",
    "-skip", "qtdatavis3d",
    "-skip", "qtgraphs",
    "-skip", "qtlocation",
    "-skip", "qtmultimedia",
    "-skip", "qtnetworkauth",
    "-skip", "qtopcua",
    "-skip", "qtpositioning",
    "-skip", "qtquick3d",
    "-skip", "qtquicktimeline",
    "-skip", "qtremoteobjects",
    "-skip", "qtscxml",
    "-skip", "qtsensors",
    "-skip", "qtserialbus",
    "-skip", "qtserialport",
    "-skip", "qtspeech",
    "-skip", "qtvirtualkeyboard",
    "-skip", "qtwebchannel",
    "-skip", "qtwebengine",
    "-skip", "qtwebsockets",
    "-skip", "qtwebview"
)
Invoke-Checked -Command (Join-Path $sourceDirectory "configure.bat") -Arguments $configureArguments -WorkingDirectory $sourceDirectory
Invoke-Checked -Command "cmake" -Arguments @("--build", ".", "--parallel") -WorkingDirectory $sourceDirectory
Invoke-Checked -Command "cmake" -Arguments @("--install", ".") -WorkingDirectory $sourceDirectory

if (-not (Test-Path -LiteralPath $qtConfig -PathType Leaf)) {
    throw "Qt $QtVersion installation did not produce $qtConfig"
}
Write-Output "Static Qt $QtVersion installed at $installPrefix"
