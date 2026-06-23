# RAW Support

NgImageViewer supports RAW images through LibRaw when the dependency is available at configure time.
The project still builds without LibRaw; in that case RAW extensions appear in the file picker, but opening
a RAW file shows an explicit "RAW support is not enabled" message.

## Supported Extensions

The RAW file picker and directory navigation include:

`3fr`, `arw`, `bay`, `cr2`, `cr3`, `crw`, `dcr`, `dng`, `erf`, `kdc`, `mos`, `mrw`,
`nef`, `nrw`, `orf`, `pef`, `raf`, `raw`, `rw2`, `rwl`, `sr2`, `srf`, `srw`, `x3f`.

## Local Debug

### macOS

Install LibRaw with Homebrew:

```bash
brew install libraw
```

Then delete or reconfigure the CMake build directory so CMake can discover the new package:

```bash
/Users/nigao/Qt/Tools/CMake/CMake.app/Contents/bin/cmake \
  -S . \
  -B build/Qt_6_11_1_for_macOS_Debug
```

The configure output should include:

```text
RAW support: enabled with ...
```

### Windows with vcpkg

Install LibRaw with the same architecture and compiler family as the Qt kit:

```bat
vcpkg install libraw:x64-windows
```

Configure CMake with the vcpkg toolchain:

```bat
-DCMAKE_TOOLCHAIN_FILE=C:/dev/vcpkg/scripts/buildsystems/vcpkg.cmake
-DVCPKG_TARGET_TRIPLET=x64-windows
```

If Qt Creator has already configured the build directory, clear the CMake cache or create a new build
directory after adding the toolchain file.

### Windows with Manual LibRaw

If LibRaw is installed manually, pass the include and library locations to CMake:

```bat
-DLIBRAW_INCLUDE_DIR=C:/dev/libraw/include
-DLIBRAW_LIBRARY=C:/dev/libraw/lib/raw.lib
```

## Release Packaging

### macOS

The app links to LibRaw when enabled. For a redistributable `.app`, make sure the LibRaw dynamic library
and its dependent codec libraries are bundled and their install names are valid for the app bundle.
Run `otool -L` on the executable to inspect unresolved dynamic library paths.

The default packaging script only handles the Release app and refuses Debug builds:

```bash
scripts/package-macos.sh
```

It runs `macdeployqt` first for Qt frameworks/plugins, then `dylibbundler` for LibRaw and other
third-party dylibs. Install `dylibbundler` before packaging:

```bash
brew install dylibbundler
```

The default macOS deployment target is `15.0`, matching the Homebrew LibRaw dylib used for local release
packaging. If you need to support older macOS versions, build LibRaw and its codec dependencies yourself
with the same lower `CMAKE_OSX_DEPLOYMENT_TARGET`, then reconfigure this project with that target.

### Windows

Run `windeployqt` for Qt dependencies, then copy LibRaw runtime DLLs next to `NgImageViewer.exe`.
The exact DLL list depends on how LibRaw was built, but commonly includes:

```text
raw.dll / libraw.dll
libjpeg*.dll
jasper*.dll
lcms2.dll
zlib*.dll
```

If LibRaw was built with extra codec backends, copy those runtime DLLs as well. Use a dependency scanner
or run the app from a clean machine/VM to verify no DLL is missing.

## CMake Behavior

RAW is controlled by:

```cmake
NGIMAGEVIEWER_ENABLE_RAW=ON
```

When enabled, CMake tries these discovery paths:

1. `pkg-config` package `libraw`
2. CMake config packages named `libraw` or `LibRaw`
3. Manual `LIBRAW_INCLUDE_DIR` and `LIBRAW_LIBRARY`

If none are found, the build continues with `NGIMAGEVIEWER_HAS_LIBRAW=0`.
