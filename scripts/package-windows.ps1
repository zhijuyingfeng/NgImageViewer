param(
    [string]$QtPrefix = $env:QT_PREFIX,
    [string]$BuildDir = $env:NGIMAGEVIEWER_WINDOWS_BUILD_DIR,
    [string]$DistDir = $env:NGIMAGEVIEWER_WINDOWS_DIST_DIR,
    [string]$Generator = $env:CMAKE_GENERATOR,
    [string]$CMake = $env:CMAKE,
    [string]$ArchivePath = $env:NGIMAGEVIEWER_WINDOWS_ARCHIVE,
    [switch]$NoZip
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$ScriptDir = Split-Path -Parent $PSCommandPath
$RepoRoot = (Resolve-Path (Join-Path $ScriptDir "..")).Path

if ([string]::IsNullOrWhiteSpace($BuildDir)) {
    $BuildDir = Join-Path $RepoRoot "build\windows-release"
}
if ([string]::IsNullOrWhiteSpace($DistDir)) {
    $DistDir = Join-Path $RepoRoot "dist\windows"
}
if ([string]::IsNullOrWhiteSpace($CMake)) {
    $CMake = "cmake"
}

$PackageDir = Join-Path $DistDir "NgImageViewer"
if ([string]::IsNullOrWhiteSpace($ArchivePath)) {
    $ArchivePath = Join-Path $DistDir "NgImageViewer-windows-x64.zip"
}

function Die {
    param([string]$Message)
    Write-Error "error: $Message"
    exit 1
}

function Info {
    param([string]$Message)
    Write-Host "==> $Message"
}

function Require-Command {
    param([string]$Name)
    if (-not (Get-Command $Name -ErrorAction SilentlyContinue)) {
        Die "required tool not found: $Name"
    }
}

function Test-QtPrefix {
    param([string]$Path)
    if ([string]::IsNullOrWhiteSpace($Path)) {
        return $false
    }

    $deploy = Join-Path $Path "bin\windeployqt.exe"
    return (Test-Path -LiteralPath $deploy -PathType Leaf)
}

function Find-QtPrefix {
    if (Test-QtPrefix $QtPrefix) {
        return (Resolve-Path $QtPrefix).Path
    }

    foreach ($candidate in @($env:CMAKE_PREFIX_PATH, $env:QT_PREFIX)) {
        if ([string]::IsNullOrWhiteSpace($candidate)) {
            continue
        }

        foreach ($part in ($candidate -split ";")) {
            if (Test-QtPrefix $part) {
                return (Resolve-Path $part).Path
            }
        }
    }

    if (Test-Path -LiteralPath "C:\Qt" -PathType Container) {
        $qtCandidates = Get-ChildItem -LiteralPath "C:\Qt" -Directory |
            Sort-Object Name -Descending |
            ForEach-Object {
                @(
                    (Join-Path $_.FullName "msvc2022_64"),
                    (Join-Path $_.FullName "msvc2019_64"),
                    (Join-Path $_.FullName "mingw_64")
                )
            }

        foreach ($candidate in $qtCandidates) {
            if (Test-QtPrefix $candidate) {
                return (Resolve-Path $candidate).Path
            }
        }
    }

    Die "Qt prefix not found. Pass -QtPrefix C:\Qt\6.x.x\msvc2022_64 or set QT_PREFIX/CMAKE_PREFIX_PATH."
}

function Require-Submodules {
    $required = @(
        "third_party\LibRaw\libraw\libraw.h",
        "third_party\LibRaw-cmake\CMakeLists.txt",
        "third_party\libheif\libheif\api\libheif\heif.h",
        "third_party\libde265\libde265\de265.h"
    )

    foreach ($relativePath in $required) {
        $path = Join-Path $RepoRoot $relativePath
        if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
            Die "missing submodules. Run: git submodule update --init --recursive"
        }
    }
}

function Select-Generator {
    if (-not [string]::IsNullOrWhiteSpace($Generator)) {
        return $Generator
    }

    if (Get-Command "ninja" -ErrorAction SilentlyContinue) {
        return "Ninja"
    }

    return ""
}

if ([System.Environment]::OSVersion.Platform -ne [System.PlatformID]::Win32NT) {
    Die "Windows packaging must run on Windows"
}

Require-Command $CMake
Require-Submodules

$QtPrefix = Find-QtPrefix
$WinDeployQt = Join-Path $QtPrefix "bin\windeployqt.exe"
$SelectedGenerator = Select-Generator
$Jobs = [System.Environment]::ProcessorCount

Info "Packaging Release build"
Write-Host "Repo: $RepoRoot"
Write-Host "Qt: $QtPrefix"
Write-Host "Build: $BuildDir"
Write-Host "Package: $PackageDir"
if (-not [string]::IsNullOrWhiteSpace($SelectedGenerator)) {
    Write-Host "Generator: $SelectedGenerator"
}

Info "Configuring"
$configureArgs = @(
    "-S", $RepoRoot,
    "-B", $BuildDir,
    "-DCMAKE_BUILD_TYPE=Release",
    "-DCMAKE_INSTALL_PREFIX=$PackageDir",
    "-DBUILD_SHARED_LIBS=OFF",
    "-DCMAKE_PREFIX_PATH=$QtPrefix"
)

if (-not [string]::IsNullOrWhiteSpace($SelectedGenerator)) {
    $configureArgs = @("-G", $SelectedGenerator) + $configureArgs
}

& $CMake @configureArgs
if ($LASTEXITCODE -ne 0) {
    Die "CMake configure failed"
}

Info "Building"
& $CMake --build $BuildDir --target NgImageViewer --config Release --parallel $Jobs
if ($LASTEXITCODE -ne 0) {
    Die "CMake build failed"
}

Info "Installing into package directory"
if (Test-Path -LiteralPath $PackageDir) {
    Remove-Item -LiteralPath $PackageDir -Recurse -Force
}
New-Item -ItemType Directory -Force -Path $DistDir | Out-Null

& $CMake --install $BuildDir --config Release
if ($LASTEXITCODE -ne 0) {
    Die "CMake install failed"
}

$Exe = Join-Path $PackageDir "bin\NgImageViewer.exe"
if (-not (Test-Path -LiteralPath $Exe -PathType Leaf)) {
    Die "packaged executable not found: $Exe"
}

Info "Deploying Qt runtime with windeployqt"
& $WinDeployQt --release --compiler-runtime $Exe
if ($LASTEXITCODE -ne 0) {
    Die "windeployqt failed"
}

$WindowsPlatformPlugin = Join-Path $PackageDir "bin\platforms\qwindows.dll"
if (-not (Test-Path -LiteralPath $WindowsPlatformPlugin -PathType Leaf)) {
    Die "Qt Windows platform plugin missing after deployment: $WindowsPlatformPlugin"
}

if (-not $NoZip) {
    Info "Creating zip archive"
    if (Test-Path -LiteralPath $ArchivePath) {
        Remove-Item -LiteralPath $ArchivePath -Force
    }
    Compress-Archive -Path $PackageDir -DestinationPath $ArchivePath -CompressionLevel Optimal
}

Info "Done"
Write-Host "Package: $PackageDir"
if (-not $NoZip) {
    Write-Host "Archive: $ArchivePath"
}
