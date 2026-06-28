# NgImageViewer

NgImageViewer is a cross-platform desktop image viewer built with Qt 6. It focuses on fast local image browsing, smooth zooming and panning, common viewer shortcuts, and bundled support for RAW and HEIF/HEIC formats.

![NgImageViewer main window](screenshots/mainWindow.png)

## Features

- Open JPG, PNG, BMP, GIF, WEBP, SVG, HEIF/HEIC, and common RAW formats.
- Fit-to-window and actual-size viewing.
- Mouse wheel, touchpad gesture, `+` and `-` zoom controls.
- Mouse-centered zooming and drag-to-pan when zoomed in.
- Keyboard navigation for previous/next image and panning.
- Delete current image, copy image/path, and reveal in Finder or the platform file manager.
- Zoom status overlay, overview indicator for zoomed images, and image information dialog.
- Bundled LibRaw, libheif, and libde265 integration for consistent RAW and HEIF/HEIC behavior across platforms.

## Requirements

- CMake 3.19 or newer.
- Qt 6.5 or newer with `Core`, `Gui`, `Widgets`, `Svg`, and `LinguistTools`.
- A C++17-capable compiler.
- Git submodules initialized.

NgImageViewer intentionally uses bundled third-party image libraries for RAW and HEIF/HEIC. Do not rely on system `libraw`, `libheif`, or `libde265`.

```bash
git submodule update --init --recursive
```

## Build Options

Both options are enabled by default:

```bash
-DNGIMAGEVIEWER_ENABLE_RAW=ON
-DNGIMAGEVIEWER_ENABLE_HEIF=ON
```

If enabled, missing submodules cause CMake to fail early instead of falling back to system libraries.

## Tests

Automated tests use Qt Test and can run without opening the main window:

```bash
cmake -S . -B build/tests \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_PREFIX_PATH=/path/to/Qt/6.x.x/macos \
  -DNGIMAGEVIEWER_BUILD_TESTS=ON
cmake --build build/tests --target test_imageformats test_imagesequence -j
ctest --test-dir build/tests --output-on-failure
```

The current tests cover supported image format metadata and directory navigation sequencing. GitHub Actions runs these tests on Linux, macOS, and Windows before packaging release artifacts.

## Build On macOS

Install Qt 6 and configure a Release build:

```bash
git submodule update --init --recursive
cmake -S . -B build/Qt_6_11_1_for_macOS_Release \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_PREFIX_PATH=/path/to/Qt/6.x.x/macos
cmake --build build/Qt_6_11_1_for_macOS_Release --target NgImageViewer -j
```

The app bundle is generated at:

```bash
build/Qt_6_11_1_for_macOS_Release/NgImageViewer.app
```

To package a redistributable Release app, install `dylibbundler` and run:

```bash
brew install dylibbundler
scripts/package-macos.sh
```

To thin the packaged app to one architecture:

```bash
NGIMAGEVIEWER_THIN_ARCH=arm64 scripts/package-macos.sh
```

The packaging script refuses to package Debug builds.

## Build On Linux

Install Qt 6 development packages and build tools. On Debian/Ubuntu, package names are typically:

```bash
sudo apt update
sudo apt install build-essential cmake ninja-build pkg-config qt6-base-dev qt6-svg-dev qt6-tools-dev libgl-dev libopengl-dev libegl-dev libglx-dev
```

Then configure and build:

```bash
git submodule update --init --recursive
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

To package a redistributable AppDir/AppImage without installing to the system, install `linuxdeploy` and `linuxdeploy-plugin-qt`, then run:

```bash
scripts/package-linux.sh
```

The script builds Release, stages the app into `dist/linux/NgImageViewer.AppDir`, runs `linuxdeploy` with the Qt plugin, checks the staged binary and Qt `xcb` platform plugin for missing libraries, and writes `dist/linux/NgImageViewer-x86_64.AppImage` by default. It does not install files to your system.

If the tools are not on `PATH`, pass them explicitly:

```bash
LINUXDEPLOY=/path/to/linuxdeploy-x86_64.AppImage \
LINUXDEPLOY_PLUGIN_QT=/path/to/linuxdeploy-plugin-qt-x86_64.AppImage \
scripts/package-linux.sh
```

The script also looks for `linuxdeploy-x86_64.AppImage`, `linuxdeploy-plugin-qt-x86_64.AppImage`, and AppImage `runtime-x86_64` in `tools/` and `~/Downloads`. If the build machine cannot run AppImages through FUSE, it uses extract-and-run mode. If `linuxdeploy` cannot download the AppImage runtime automatically, place `runtime-x86_64` in `tools/` or `~/Downloads`, or set `APPIMAGE_RUNTIME=/path/to/runtime-x86_64`.

Set `NGIMAGEVIEWER_LINUX_APPIMAGE=0` to generate only the AppDir. `LibRaw`, `libheif`, and `libde265` are built from bundled submodules and linked statically by default; Qt and its plugins are collected into the AppDir/AppImage.

To install the binary, desktop file, and hicolor icon:

```bash
cmake --install build --prefix ~/.local
gtk-update-icon-cache ~/.local/share/icons/hicolor 2>/dev/null || true
update-desktop-database ~/.local/share/applications 2>/dev/null || true
```

If you use the Qt online installer instead of system Qt packages, pass `CMAKE_PREFIX_PATH`:

```bash
cmake -S . -B build \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_PREFIX_PATH=/path/to/Qt/6.x.x/gcc_64
```

## Build On Windows

Install Qt 6 for the same compiler you plan to use, such as MSVC or MinGW. Keep Qt, CMake, and the compiler toolchain consistent.

To create a redistributable Windows zip package, run from a Developer PowerShell:

```powershell
git submodule update --init --recursive
powershell -ExecutionPolicy Bypass -File scripts\package-windows.ps1 `
  -QtPrefix C:\Qt\6.x.x\msvc2022_64
```

The script builds Release, installs the app into `dist\windows\NgImageViewer`, runs `windeployqt` from the same Qt kit, and writes `dist\windows\NgImageViewer-windows-x64.zip` by default. Pass `-NoZip` to keep only the package directory.

By default the Windows package is size-optimized: it copies the required MSVC runtime DLLs instead of bundling the full `vc_redist.x64.exe` installer, and it skips Qt's software OpenGL fallback, system D3D compiler, and full Qt translation bundle. For a larger compatibility-first package, pass any of:

```powershell
powershell -ExecutionPolicy Bypass -File scripts\package-windows.ps1 `
  -QtPrefix C:\Qt\6.x.x\msvc2022_64 `
  -IncludeCompilerRuntimeInstaller `
  -IncludeOpenGLSoftwareRenderer `
  -IncludeSystemD3DCompiler `
  -IncludeQtTranslations
```

Example with MSVC from a Developer PowerShell:

```powershell
git submodule update --init --recursive
cmake -S . -B build -G "Ninja" `
  -DCMAKE_BUILD_TYPE=Release `
  -DCMAKE_PREFIX_PATH=C:\Qt\6.x.x\msvc2022_64
cmake --build build --target NgImageViewer -j
```

Example with MinGW:

```powershell
git submodule update --init --recursive
cmake -S . -B build -G "Ninja" `
  -DCMAKE_BUILD_TYPE=Release `
  -DCMAKE_PREFIX_PATH=C:\Qt\6.x.x\mingw_64
cmake --build build --target NgImageViewer -j
```

After building, use `windeployqt` from the same Qt kit:

```powershell
C:\Qt\6.x.x\msvc2022_64\bin\windeployqt.exe build\NgImageViewer.exe
```

The Windows executable includes the application icon through a `.rc` resource file.

## Development Notes

- RAW support is implemented with bundled LibRaw submodules.
- HEIF/HEIC support is implemented with bundled libheif and libde265; no Qt HEIF image plugin is required.
- SVG images are rendered as vector content and can be zoomed without raster quality loss.
- Large bitmap zooming uses viewport-aware rendering to avoid full-size pixmap allocation.

More format-specific details are available in:

- [RAW support](docs/raw-support.md)
- [HEIF/HEIC support](docs/heif-support.md)
