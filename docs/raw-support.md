# RAW Support

NgImageViewer supports RAW images through the bundled LibRaw submodules when RAW support is enabled.
System LibRaw is intentionally not used because distribution versions differ and may not support the same
camera models.

RAW files are opened with a preview-first strategy. The app first tries to display the embedded JPEG/preview
stored inside the RAW file and uses it immediately when available. If LibRaw cannot extract the preview, the
app scans the RAW container for embedded JPEG data before falling back to full LibRaw processing. This is
intentional for image browsing: embedded previews are usually faster, closer to the camera's own rendering,
and more compatible with new camera models.

## Supported Extensions

The RAW file picker and directory navigation include:

`3fr`, `arw`, `bay`, `cr2`, `cr3`, `crw`, `dcr`, `dng`, `erf`, `kdc`, `mos`, `mrw`,
`nef`, `nrw`, `orf`, `pef`, `raf`, `raw`, `rw2`, `rwl`, `sr2`, `srf`, `srw`, `x3f`.

## Local Debug

### Bundled LibRaw Submodules

NgImageViewer requires the bundled LibRaw submodules when `NGIMAGEVIEWER_ENABLE_RAW=ON`:

```bash
git submodule update --init --recursive
```

The bundled version is currently pinned to LibRaw `0.22.1` with the community CMake wrapper from
`LibRaw-cmake`. CMake builds it as a static library by default, disables LibRaw examples and OpenMP, and links
the app against `libraw::libraw`. This keeps Linux builds independent from old distribution packages such as
Debian's `libraw-dev 0.20.x`.

If the submodules are missing, CMake fails immediately instead of falling back to a local or system LibRaw.

### macOS

Initialize submodules, then delete or reconfigure the CMake build directory:

```bash
/Users/nigao/Qt/Tools/CMake/CMake.app/Contents/bin/cmake \
  -S . \
  -B build/Qt_6_11_1_for_macOS_Debug
```

The configure output should include:

```text
RAW support: enabled with ...
```

### Windows

Use the same submodule flow as macOS/Linux. `LibRaw-cmake` defines the `libraw::libraw` CMake target and can
build the bundled LibRaw sources with MSVC or MinGW. The project does not link against vcpkg or manually
installed LibRaw.

## Release Packaging

### macOS

The app links to LibRaw when enabled. For a redistributable `.app`, make sure the LibRaw dynamic library
and its dependent codec libraries are bundled and their install names are valid for the app bundle.
Run `otool -L` on the executable to inspect unresolved dynamic library paths.

LibRaw itself is linked statically into the app executable. Its dynamic codec dependencies, such as `lcms2`,
`jpeg`, or `zlib` when present, still need to be handled by the packaging step.

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

Run `windeployqt` for Qt dependencies. LibRaw itself is static, so there is no `raw.dll` to copy. If CMake
found dynamic codec dependencies while building LibRaw, copy those DLLs next to `NgImageViewer.exe`. The exact
DLL list depends on the Windows kit and dependency setup, but commonly includes:

```text
libjpeg*.dll
lcms2.dll
zlib*.dll
```

Use a dependency scanner or run the app from a clean machine/VM to verify no DLL is missing.

## CMake Behavior

RAW is controlled by:

```cmake
NGIMAGEVIEWER_ENABLE_RAW=ON
```

When enabled, CMake only uses bundled `third_party/LibRaw` + `third_party/LibRaw-cmake`. If those submodules
are not initialized, configuration fails with an explicit error. To build without RAW support, configure with:

```bash
cmake -S . -B build/no-raw -DNGIMAGEVIEWER_ENABLE_RAW=OFF
```
