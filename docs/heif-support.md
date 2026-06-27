# HEIF/HEIC Support

NgImageViewer supports HEIF/HEIC images through bundled `libheif` and `libde265` submodules.
System `libheif` is intentionally not used because distribution versions and enabled codec backends vary.

## Submodules

HEIF support requires these submodules:

```bash
git submodule update --init --recursive
```

The bundled versions are currently pinned to:

- `third_party/libheif`: `v1.23.1`
- `third_party/libde265`: `v1.1.1`

`libde265` is built as the HEVC/H.265 decoder backend and is compiled into `libheif` as a built-in backend.
Runtime codec plugin loading is disabled, so release packages do not need a separate libheif plugin directory.

## CMake

HEIF support is enabled by default:

```bash
cmake -S . -B build -DNGIMAGEVIEWER_ENABLE_HEIF=ON
```

If either submodule is missing, CMake fails immediately and prints the submodule initialization command.
It does not fall back to Homebrew, apt, vcpkg, or any other system `libheif`.

The bundled build disables encoders, AVIF/VVC/JPEG2000 backends, examples, tests, documentation, and dynamic
codec plugin loading. The app currently supports HEIF/HEIC reading through the primary image only.

## Application Path

`.heic` and `.heif` files are decoded by `HeifDecoder`, not by Qt image format plugins:

```text
HEIC/HEIF file
  -> HeifDecoder
    -> bundled libheif
      -> bundled libde265
        -> QImage
```

This means Linux and Windows releases do not need `qheif.dll`, `libqheif.so`, or a Qt HEIF imageformats plugin.

## Release Notes

The current configuration builds `libheif` and `libde265` as static libraries. If a platform/toolchain changes
that to dynamic linking, the package script must copy the corresponding runtime libraries next to the app.

HEIC uses HEVC/H.265 compression. Review codec licensing and patent requirements before external distribution.
