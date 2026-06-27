param(
    [string]$QtPrefix = $env:QT_PREFIX,
    [string]$BuildDir = $env:NGIMAGEVIEWER_WINDOWS_BUILD_DIR,
    [string]$DistDir = $env:NGIMAGEVIEWER_WINDOWS_DIST_DIR,
    [string]$Generator = $env:CMAKE_GENERATOR,
    [string]$CMake = $env:CMAKE,
    [string]$ArchivePath = $env:NGIMAGEVIEWER_WINDOWS_ARCHIVE,
    [switch]$IncludeCompilerRuntimeInstaller,
    [switch]$IncludeOpenGLSoftwareRenderer,
    [switch]$IncludeSystemD3DCompiler,
    [switch]$IncludeQtTranslations,
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

function Copy-MsvcRuntimeDlls {
    param([string]$DestinationDir)

    $runtimeDir = ""
    if (-not [string]::IsNullOrWhiteSpace($env:VCToolsRedistDir)) {
        $candidate = Join-Path $env:VCToolsRedistDir "x64\Microsoft.VC143.CRT"
        if (Test-Path -LiteralPath $candidate -PathType Container) {
            $runtimeDir = $candidate
        }
    }

    if ([string]::IsNullOrWhiteSpace($runtimeDir)) {
        $vsRoots = @(
            "${env:ProgramFiles}\Microsoft Visual Studio\2022",
            "${env:ProgramFiles(x86)}\Microsoft Visual Studio\2019"
        )

        foreach ($root in $vsRoots) {
            if (-not (Test-Path -LiteralPath $root -PathType Container)) {
                continue
            }

            $candidate = Get-ChildItem -LiteralPath $root -Directory -Recurse -ErrorAction SilentlyContinue |
                Where-Object { $_.FullName -match "\\VC\\Redist\\MSVC\\[^\\]+\\x64\\Microsoft\.VC14[0-9]\.CRT$" } |
                Sort-Object FullName -Descending |
                Select-Object -First 1

            if ($candidate) {
                $runtimeDir = $candidate.FullName
                break
            }
        }
    }

    if ([string]::IsNullOrWhiteSpace($runtimeDir)) {
        Write-Warning "MSVC runtime directory not found; the package may require the Microsoft Visual C++ Redistributable to be installed."
        return
    }

    Info "Copying MSVC runtime DLLs"
    $runtimeDlls = @(
        "concrt140.dll",
        "msvcp140.dll",
        "msvcp140_1.dll",
        "msvcp140_2.dll",
        "vccorlib140.dll",
        "vcruntime140.dll",
        "vcruntime140_1.dll"
    )

    foreach ($dll in $runtimeDlls) {
        $source = Join-Path $runtimeDir $dll
        if (Test-Path -LiteralPath $source -PathType Leaf) {
            Copy-Item -LiteralPath $source -Destination $DestinationDir -Force
        }
    }
}

function Remove-PathIfExists {
    param([string]$Path)

    if (Test-Path -LiteralPath $Path) {
        Remove-Item -LiteralPath $Path -Recurse -Force
    }
}

function Get-DirectorySizeBytes {
    param([string]$Path)

    if (-not (Test-Path -LiteralPath $Path)) {
        return 0
    }

    $size = 0
    Get-ChildItem -LiteralPath $Path -File -Recurse | ForEach-Object {
        $size += $_.Length
    }
    return $size
}

function Format-ByteSize {
    param([long]$Bytes)

    if ($Bytes -ge 1GB) {
        return "{0:N1} GB" -f ($Bytes / 1GB)
    }
    if ($Bytes -ge 1MB) {
        return "{0:N1} MB" -f ($Bytes / 1MB)
    }
    if ($Bytes -ge 1KB) {
        return "{0:N1} KB" -f ($Bytes / 1KB)
    }
    return "$Bytes B"
}

function Create-ZipArchive {
    param(
        [string]$SourceDir,
        [string]$Destination
    )

    if (Test-Path -LiteralPath $Destination) {
        Remove-Item -LiteralPath $Destination -Force
    }

    $sevenZip = Get-Command "7z" -ErrorAction SilentlyContinue
    if ($sevenZip) {
        Push-Location (Split-Path -Parent $SourceDir)
        try {
            & $sevenZip.Source a -tzip -mx=9 $Destination (Split-Path -Leaf $SourceDir)
            if ($LASTEXITCODE -ne 0) {
                Die "7z archive creation failed"
            }
        }
        finally {
            Pop-Location
        }
        return
    }

    Compress-Archive -Path $SourceDir -DestinationPath $Destination -CompressionLevel Optimal
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
$IsMingwKit = $QtPrefix -match "(?i)mingw"

Info "Packaging Release build"
Write-Host "Repo: $RepoRoot"
Write-Host "Qt: $QtPrefix"
Write-Host "Qt kit: $(if ($IsMingwKit) { "MinGW" } else { "MSVC" })"
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
$deployArgs = @("--release")
if ($IncludeCompilerRuntimeInstaller -or $IsMingwKit) {
    $deployArgs += "--compiler-runtime"
}
else {
    $deployArgs += "--no-compiler-runtime"
}
if (-not $IncludeOpenGLSoftwareRenderer) {
    $deployArgs += "--no-opengl-sw"
}
if (-not $IncludeSystemD3DCompiler) {
    $deployArgs += "--no-system-d3d-compiler"
}
if (-not $IncludeQtTranslations) {
    $deployArgs += "--no-translations"
}
$deployArgs += $Exe

& $WinDeployQt @deployArgs
if ($LASTEXITCODE -ne 0) {
    Die "windeployqt failed"
}

if (-not $IncludeCompilerRuntimeInstaller -and -not $IsMingwKit) {
    Remove-PathIfExists (Join-Path $PackageDir "bin\vc_redist.x64.exe")
    Copy-MsvcRuntimeDlls (Join-Path $PackageDir "bin")
}
if (-not $IncludeOpenGLSoftwareRenderer) {
    Remove-PathIfExists (Join-Path $PackageDir "bin\opengl32sw.dll")
}
if (-not $IncludeSystemD3DCompiler) {
    Remove-PathIfExists (Join-Path $PackageDir "bin\d3dcompiler_47.dll")
}
if (-not $IncludeQtTranslations) {
    Remove-PathIfExists (Join-Path $PackageDir "bin\translations")
}

$WindowsPlatformPlugin = Join-Path $PackageDir "bin\platforms\qwindows.dll"
if (-not (Test-Path -LiteralPath $WindowsPlatformPlugin -PathType Leaf)) {
    Die "Qt Windows platform plugin missing after deployment: $WindowsPlatformPlugin"
}

$packageSize = Get-DirectorySizeBytes $PackageDir
Write-Host "Package directory size: $(Format-ByteSize $packageSize)"

if (-not $NoZip) {
    Info "Creating zip archive"
    Create-ZipArchive $PackageDir $ArchivePath
    $archiveSize = (Get-Item -LiteralPath $ArchivePath).Length
    Write-Host "Archive size: $(Format-ByteSize $archiveSize)"
}

Info "Done"
Write-Host "Package: $PackageDir"
if (-not $NoZip) {
    Write-Host "Archive: $ArchivePath"
}
