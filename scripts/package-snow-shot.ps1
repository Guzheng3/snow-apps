[CmdletBinding()]
param(
    [string]$BuildDirectory = "build\snow-shot-msvc-release",
    [string]$InstallDirectory = "artifacts\snow-shot",
    [switch]$SkipBuild
)

$ErrorActionPreference = "Stop"
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path

function Initialize-ReleaseToolchainEnvironment {
    if ([string]::IsNullOrWhiteSpace($env:VCINSTALLDIR)) {
        $vswhereCandidates = @(
            (Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"),
            (Join-Path ${env:ProgramFiles} "Microsoft Visual Studio\Installer\vswhere.exe")
        )
        $vswherePath = $vswhereCandidates |
            Where-Object { Test-Path -LiteralPath $_ } |
            Select-Object -First 1
        if (-not $vswherePath) {
            throw "Visual Studio locator 'vswhere.exe' was not found. Install Visual Studio 2022 with the MSVC x64 build tools."
        }

        $visualStudioCandidates = @(& $vswherePath -latest -products * `
            -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
            -property installationPath)
        $vswhereExitCode = $LASTEXITCODE
        $visualStudioPath = $visualStudioCandidates | Select-Object -First 1
        if ($vswhereExitCode -ne 0 -or [string]::IsNullOrWhiteSpace($visualStudioPath)) {
            throw "Visual Studio 2022 with the MSVC x64 build tools was not found."
        }

        $vcInstallDirectory = Join-Path $visualStudioPath.Trim() "VC"
        if (-not (Test-Path -LiteralPath $vcInstallDirectory -PathType Container)) {
            throw "Visual Studio C++ tools directory was not found: $vcInstallDirectory"
        }
        $env:VCINSTALLDIR = $vcInstallDirectory
    }

    $vcRedistRoot = Join-Path $env:VCINSTALLDIR "Redist\MSVC"
    $vcRedistDirectory = Get-ChildItem -LiteralPath $vcRedistRoot -Directory -ErrorAction SilentlyContinue |
        Where-Object { $_.Name -match '^\d+(\.\d+)+$' } |
        Sort-Object { [version]$_.Name } -Descending |
        Select-Object -First 1
    if (-not $vcRedistDirectory) {
        throw "The x64 MSVC redistributable directory was not found under $vcRedistRoot"
    }
    $vcRuntimeDirectory = Join-Path $vcRedistDirectory.FullName "x64\Microsoft.VC143.CRT"
    if (-not (Test-Path -LiteralPath $vcRuntimeDirectory -PathType Container)) {
        throw "The x64 MSVC redistributable runtime was not found: $vcRuntimeDirectory"
    }
    $env:VCToolsRedistDir = $vcRedistDirectory.FullName + [System.IO.Path]::DirectorySeparatorChar

    $msvcToolsRoot = Join-Path $env:VCINSTALLDIR "Tools\MSVC"
    $msvcToolsDirectory = Get-ChildItem -LiteralPath $msvcToolsRoot -Directory -ErrorAction SilentlyContinue |
        Where-Object { $_.Name -match '^\d+(\.\d+)+$' } |
        Sort-Object { [version]$_.Name } -Descending |
        Select-Object -First 1
    if (-not $msvcToolsDirectory) {
        throw "The MSVC toolset directory was not found under $msvcToolsRoot"
    }
    $script:DumpbinPath = Join-Path $msvcToolsDirectory.FullName "bin\Hostx64\x64\dumpbin.exe"
    if (-not (Test-Path -LiteralPath $script:DumpbinPath -PathType Leaf)) {
        throw "The x64 PE inspection tool was not found: $script:DumpbinPath"
    }

    $windowsSdkRoot = Join-Path ${env:ProgramFiles(x86)} "Windows Kits\10"
    $windowsSdkBinRoot = Join-Path $windowsSdkRoot "bin"
    $windowsSdkDirectory = Get-ChildItem -LiteralPath $windowsSdkBinRoot -Directory -ErrorAction SilentlyContinue |
        Where-Object { $_.Name -match '^\d+\.\d+\.\d+\.\d+$' } |
        Sort-Object { [version]$_.Name } -Descending |
        Where-Object {
            $x64Directory = Join-Path $_.FullName "x64"
            (Test-Path -LiteralPath (Join-Path $x64Directory "dxcompiler.dll")) -and
                (Test-Path -LiteralPath (Join-Path $x64Directory "dxil.dll"))
        } |
        Select-Object -First 1
    if (-not $windowsSdkDirectory) {
        throw "A Windows 10/11 SDK containing the x64 DirectX Shader Compiler was not found under $windowsSdkBinRoot"
    }

    $windowsSdkX64Directory = Join-Path $windowsSdkDirectory.FullName "x64"
    $env:WindowsSdkDir = $windowsSdkRoot + [System.IO.Path]::DirectorySeparatorChar
    $env:WindowsSDKVersion = $windowsSdkDirectory.Name + [System.IO.Path]::DirectorySeparatorChar
    $env:Path = "$windowsSdkX64Directory;$env:Path"

    Write-Output "Visual Studio C++ tools: $env:VCINSTALLDIR"
    Write-Output "MSVC redistributable: $env:VCToolsRedistDir"
    Write-Output "Windows SDK runtime tools: $windowsSdkX64Directory"
    Write-Output "PE dependency inspector: $script:DumpbinPath"
}

Initialize-ReleaseToolchainEnvironment

$nsisCommand = Get-Command makensis -ErrorAction SilentlyContinue
if (-not $nsisCommand) {
    $nsisCandidates = @(
        "${env:ProgramFiles(x86)}\NSIS\makensis.exe",
        "${env:ProgramFiles}\NSIS\makensis.exe"
    )
    $nsisPath = $nsisCandidates |
        Where-Object { Test-Path -LiteralPath $_ } |
        Select-Object -First 1
    if ($nsisPath) {
        $env:Path = "$(Split-Path -Parent $nsisPath);$env:Path"
        $nsisCommand = Get-Command makensis -ErrorAction SilentlyContinue
    }
}
if (-not $nsisCommand) {
    throw "NSIS compiler 'makensis' was not found. Install NSIS before packaging Snow Shot."
}

function Resolve-RepoPath {
    param([Parameter(Mandatory = $true)][string]$Path)
    if ([System.IO.Path]::IsPathRooted($Path)) {
        return [System.IO.Path]::GetFullPath($Path)
    }
    return [System.IO.Path]::GetFullPath((Join-Path $repoRoot $Path))
}

$buildDirectory = Resolve-RepoPath $BuildDirectory
$installDirectory = Resolve-RepoPath $InstallDirectory
$artifactRoot = Resolve-RepoPath "artifacts"
$artifactPrefix = $artifactRoot + [System.IO.Path]::DirectorySeparatorChar
if (-not $installDirectory.StartsWith($artifactPrefix, [System.StringComparison]::OrdinalIgnoreCase)) {
    throw "InstallDirectory must be a child of $artifactRoot"
}

$cachePath = Join-Path $buildDirectory "CMakeCache.txt"
if (-not $SkipBuild) {
    & cmake --fresh --preset snow-shot-msvc-release
    if ($LASTEXITCODE -ne 0) {
        throw "Snow Shot release configuration failed."
    }
}
elseif (-not (Test-Path -LiteralPath $cachePath)) {
    throw "CMake cache was not found: $cachePath"
}

$requiredCacheEntries = @(
    "SNOW_APPS_BUILD_TESTS:BOOL=OFF",
    "SNOW_APPS_BUILD_BENCHMARKS:BOOL=OFF",
    "SNOW_APPS_RELEASE_STATIC:BOOL=ON",
    "SNOW_APPS_QT_STATIC:BOOL=ON",
    "SNOW_APPS_PACKAGE_SNOW_SHOT:BOOL=ON",
    "QT_FEATURE_static:INTERNAL=ON"
)
$cache = Get-Content -LiteralPath $cachePath
foreach ($entry in $requiredCacheEntries) {
    if ($cache -notcontains $entry) {
        throw "Release cache is not production-safe; missing '$entry'."
    }
}

if (-not $SkipBuild) {
    & cmake --build --preset build-snow-shot-msvc-release --parallel
    if ($LASTEXITCODE -ne 0) {
        throw "Snow Shot release build failed."
    }
}

if (Test-Path -LiteralPath $installDirectory) {
    Remove-Item -LiteralPath $installDirectory -Recurse -Force
}
New-Item -ItemType Directory -Force -Path $installDirectory | Out-Null

& cmake --install $buildDirectory --config Release --prefix $installDirectory
if ($LASTEXITCODE -ne 0) {
    throw "Snow Shot install step failed."
}

$mainExecutable = Join-Path $installDirectory "bin\snow_shot.exe"
if (-not (Test-Path -LiteralPath $mainExecutable)) {
    throw "The staged application was not found: $mainExecutable"
}

$versionInfo = (Get-Item -LiteralPath $mainExecutable).VersionInfo
$expectedBinaryMetadata = @{
    CompanyName = "Snow Apps"
    FileDescription = "Snow Shot screenshot utility"
    InternalName = "snow_shot"
    LegalCopyright = "Copyright (C) 2026 mg-chao"
    OriginalFilename = "snow_shot.exe"
    ProductName = "Snow Shot"
}
foreach ($property in $expectedBinaryMetadata.Keys) {
    if ($versionInfo.$property -ne $expectedBinaryMetadata[$property]) {
        throw "Snow Shot binary metadata '$property' is '$($versionInfo.$property)'; expected '$($expectedBinaryMetadata[$property])'."
    }
}

$requiredStageFiles = @(
    "bin\snow_shot.exe",
    "bin\MSVCP140.dll",
    "bin\MSVCP140_1.dll",
    "bin\VCRUNTIME140.dll",
    "bin\VCRUNTIME140_1.dll",
    "bin\dxcompiler.dll",
    "bin\dxil.dll"
)
$missingStageFiles = @($requiredStageFiles | Where-Object {
    -not (Test-Path -LiteralPath (Join-Path $installDirectory $_) -PathType Leaf)
})
if ($missingStageFiles.Count -gt 0) {
    throw "Release staging is missing required runtime files: $($missingStageFiles -join ', ')"
}

$stagedQtDlls = @(Get-ChildItem -LiteralPath $installDirectory -Recurse -File -Filter "Qt6*.dll")
if ($stagedQtDlls.Count -gt 0) {
    throw "Static Qt release staging contains Qt DLLs: $($stagedQtDlls.FullName -join ', ')"
}
$stagedQtPluginDirectory = Join-Path $installDirectory "plugins"
if (Test-Path -LiteralPath $stagedQtPluginDirectory -PathType Container) {
    throw "Static Qt release staging contains a Qt plugin directory: $stagedQtPluginDirectory"
}

$stagedExecutables = @(Get-ChildItem -LiteralPath $installDirectory -Recurse -File -Filter "*.exe")
$unexpectedExecutables = @($stagedExecutables | Where-Object { $_.Name -ne "snow_shot.exe" })
if ($unexpectedExecutables.Count -gt 0) {
    throw "Release staging contains unexpected executables: $($unexpectedExecutables.FullName -join ', ')"
}

$testArtifacts = @(Get-ChildItem -LiteralPath $installDirectory -Recurse -File |
    Where-Object { $_.Name -match "(?i)(test|benchmark)" })
if ($testArtifacts.Count -gt 0) {
    throw "Release staging contains test or benchmark artifacts: $($testArtifacts.FullName -join ', ')"
}

$debugArtifacts = @(Get-ChildItem -LiteralPath $installDirectory -Recurse -File |
    Where-Object { $_.Extension.ToLowerInvariant() -in @(".pdb", ".ilk", ".iobj", ".ipdb") })
if ($debugArtifacts.Count -gt 0) {
    throw "Release staging contains debug artifacts: $($debugArtifacts.FullName -join ', ')"
}

$stagedBinaries = @(Get-ChildItem -LiteralPath $installDirectory -Recurse -File |
    Where-Object { $_.Extension.ToLowerInvariant() -in @(".dll", ".exe") })
$stagedBinDirectory = Join-Path $installDirectory "bin"
$windowsSystemDirectory = [Environment]::GetFolderPath([Environment+SpecialFolder]::System)
$debugRuntimeImports = [System.Collections.Generic.List[string]]::new()
$unresolvedImports = [System.Collections.Generic.List[string]]::new()
foreach ($binary in $stagedBinaries) {
    $dependencyOutput = @(& $script:DumpbinPath /nologo /dependents $binary.FullName 2>&1)
    if ($LASTEXITCODE -ne 0) {
        throw "PE dependency inspection failed for $($binary.FullName)"
    }

    foreach ($line in $dependencyOutput) {
        if ($line -notmatch '^\s+([A-Za-z0-9_.-]+\.dll)\s*$') {
            continue
        }
        $dependencyName = $Matches[1]
        if ($dependencyName -match '(?i)^(?:Qt6.+d|(?:msvcp|vcruntime|concrt)\d+(?:(?:_\d+)?d(?:_.*)?|_threadsd)|ucrtbased)\.dll$') {
            $debugRuntimeImports.Add("$($binary.Name) -> $dependencyName")
        }

        if ($dependencyName -match '(?i)^(?:api|ext)-ms-') {
            continue
        }
        $resolutionCandidates = @(
            (Join-Path $binary.DirectoryName $dependencyName),
            (Join-Path $stagedBinDirectory $dependencyName),
            (Join-Path $windowsSystemDirectory $dependencyName)
        )
        if (-not ($resolutionCandidates | Where-Object { Test-Path -LiteralPath $_ -PathType Leaf } | Select-Object -First 1)) {
            $unresolvedImports.Add("$($binary.Name) -> $dependencyName")
        }
    }
}
if ($debugRuntimeImports.Count -gt 0) {
    throw "Release staging imports debug runtime libraries: $($debugRuntimeImports -join ', ')"
}
if ($unresolvedImports.Count -gt 0) {
    throw "Release staging has unresolved PE dependencies: $($unresolvedImports -join ', ')"
}
Write-Output "PE dependency audit: $($stagedBinaries.Count) binaries checked"

$cpackConfig = Join-Path $buildDirectory "CPackConfig.cmake"
if (-not (Test-Path -LiteralPath $cpackConfig)) {
    throw "CPack configuration was not generated: $cpackConfig"
}

$cpackConfiguration = Get-Content -LiteralPath $cpackConfig -Raw
$requiredCpackSettings = @{
    CPACK_CREATE_DESKTOP_LINKS = "snow_shot"
    CPACK_PACKAGE_EXECUTABLES = "snow_shot;Snow Shot"
    CPACK_PACKAGE_HOMEPAGE_URL = "https://snowshot.top"
    CPACK_PACKAGE_INSTALL_DIRECTORY = "SnowShot"
    CPACK_PACKAGE_INSTALL_REGISTRY_KEY = "SnowShot"
    CPACK_NSIS_INSTALLED_ICON_NAME = "bin\\snow_shot.exe"
}
foreach ($setting in $requiredCpackSettings.Keys) {
    $escapedSetting = [regex]::Escape($setting)
    $escapedValue = [regex]::Escape($requiredCpackSettings[$setting])
    $settingPresent = if ($setting -eq "CPACK_NSIS_INSTALLED_ICON_NAME") {
        $cpackConfiguration -match 'set\(CPACK_NSIS_INSTALLED_ICON_NAME "bin\\+snow_shot\.exe"\)'
    }
    else {
        $cpackConfiguration -match "set\($escapedSetting `"$escapedValue`"\)"
    }
    if (-not $settingPresent) {
        throw "CPack configuration is missing '$setting=$($requiredCpackSettings[$setting])'."
    }
}
if ($cpackConfiguration -notmatch 'set\(CPACK_PACKAGE_VERSION "([^"]+)"\)') {
    throw "CPack configuration does not declare the Snow Shot package version."
}
$packageVersion = $Matches[1]
if ($versionInfo.FileVersion -ne "$packageVersion.0" -or
    $versionInfo.ProductVersion -ne $packageVersion) {
    throw "Snow Shot binary version '$($versionInfo.FileVersion)'/'$($versionInfo.ProductVersion)' does not match package version '$packageVersion'."
}

Push-Location $buildDirectory
try {
    & cpack --config $cpackConfig -G NSIS -C Release
}
finally {
    Pop-Location
}
if ($LASTEXITCODE -ne 0) {
    throw "NSIS packaging failed."
}

$packages = @(Get-ChildItem -LiteralPath $buildDirectory -File -Filter "snow-shot-*.exe" |
    Sort-Object LastWriteTime)
if ($packages.Count -eq 0) {
    throw "NSIS did not produce a Snow Shot installer under $buildDirectory"
}

$installerVersionInfo = $packages[-1].VersionInfo
$expectedInstallerMetadata = @{
    CompanyName = "Snow Apps"
    FileDescription = "Snow Shot installer"
    FileVersion = "$packageVersion.0"
    InternalName = "snow-shot-installer"
    LegalCopyright = "Copyright (C) 2026 mg-chao"
    OriginalFilename = "snow-shot-$packageVersion-windows-x64.exe"
    ProductName = "Snow Shot"
    ProductVersion = $packageVersion
}
foreach ($property in $expectedInstallerMetadata.Keys) {
    if ($installerVersionInfo.$property -ne $expectedInstallerMetadata[$property]) {
        throw "Snow Shot installer metadata '$property' is '$($installerVersionInfo.$property)'; expected '$($expectedInstallerMetadata[$property])'."
    }
}

$checksumPath = "$($packages[-1].FullName).sha256"
if (-not (Test-Path -LiteralPath $checksumPath -PathType Leaf)) {
    throw "CPack did not produce the installer checksum: $checksumPath"
}

Write-Output "Snow Shot install tree: $installDirectory"
Write-Output "Snow Shot installer: $($packages[-1].FullName)"
Write-Output "Snow Shot installer checksum: $checksumPath"
