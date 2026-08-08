[CmdletBinding()]
param(
    [string]$QtVersion = "6.10.3",
    [Parameter(Mandatory = $true)][string]$InstallPrefix,
    [string]$SourceDirectory = "",
    [string]$QtMirrorBaseUrl = "https://qt.mirror.constant.com/official_releases"
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

function Save-RemoteFile {
    param(
        [Parameter(Mandatory = $true)][string]$Uri,
        [Parameter(Mandatory = $true)][string]$Destination
    )

    $client = [System.Net.Http.HttpClient]::new()
    $client.Timeout = [TimeSpan]::FromHours(2)
    $response = $null
    $inputStream = $null
    $outputStream = $null
    try {
        Write-Output "Downloading $Uri"
        $response = $client.GetAsync(
            $Uri,
            [System.Net.Http.HttpCompletionOption]::ResponseHeadersRead
        ).GetAwaiter().GetResult()
        $response.EnsureSuccessStatusCode()
        $contentLength = $response.Content.Headers.ContentLength
        $inputStream = $response.Content.ReadAsStreamAsync().GetAwaiter().GetResult()
        $outputStream = [System.IO.File]::Open(
            $Destination,
            [System.IO.FileMode]::Create,
            [System.IO.FileAccess]::Write,
            [System.IO.FileShare]::None
        )
        $buffer = [byte[]]::new(1024 * 1024)
        $downloaded = [int64]0
        $lastLog = [DateTime]::UtcNow
        while (($read = $inputStream.Read($buffer, 0, $buffer.Length)) -gt 0) {
            $outputStream.Write($buffer, 0, $read)
            $downloaded += $read
            $now = [DateTime]::UtcNow
            if (($now - $lastLog).TotalSeconds -ge 2) {
                $downloadedMiB = [math]::Round($downloaded / 1MB, 1)
                if ($contentLength) {
                    $percent = [math]::Round(($downloaded * 100.0) / $contentLength, 1)
                    $totalMiB = [math]::Round($contentLength / 1MB, 1)
                    Write-Progress -Activity "Downloading Qt $QtVersion sources" `
                        -Status "$downloadedMiB / $totalMiB MiB ($percent%)" `
                        -PercentComplete $percent
                    Write-Output "Qt source download: $downloadedMiB / $totalMiB MiB ($percent%)"
                }
                else {
                    Write-Progress -Activity "Downloading Qt $QtVersion sources" `
                        -Status "$downloadedMiB MiB received"
                    Write-Output "Qt source download: $downloadedMiB MiB received"
                }
                $lastLog = $now
            }
        }
        Write-Progress -Activity "Downloading Qt $QtVersion sources" -Completed
        Write-Output "Qt source download complete: $([math]::Round($downloaded / 1MB, 1)) MiB"
    }
    finally {
        if ($outputStream) { $outputStream.Dispose() }
        if ($inputStream) { $inputStream.Dispose() }
        if ($response) { $response.Dispose() }
        $client.Dispose()
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
$sourceRelativePath = "qt/$($QtVersion.Substring(0, $QtVersion.LastIndexOf('.')))/$QtVersion/single/qt-everywhere-src-$QtVersion.tar.xz"
$sourceUrls = @(
    "$($QtMirrorBaseUrl.TrimEnd('/'))/$sourceRelativePath",
    "https://download.qt.io/official_releases/$sourceRelativePath"
) | Select-Object -Unique

if (-not (Test-Path -LiteralPath $sourceDirectory -PathType Container)) {
    if (-not (Test-Path -LiteralPath $archivePath -PathType Leaf)) {
        $downloaded = $false
        foreach ($sourceUrl in $sourceUrls) {
            try {
                Save-RemoteFile -Uri $sourceUrl -Destination $archivePath
                $downloaded = $true
                break
            }
            catch {
                Write-Warning "Qt source mirror failed ($sourceUrl): $($_.Exception.Message)"
                Remove-Item -LiteralPath $archivePath -Force -ErrorAction SilentlyContinue
            }
        }
        if (-not $downloaded) {
            throw "All Qt source mirrors failed."
        }
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
