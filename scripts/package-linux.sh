#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
BUILD_DIR="${NGIMAGEVIEWER_LINUX_BUILD_DIR:-$REPO_ROOT/build/linux-release}"
DIST_DIR="${NGIMAGEVIEWER_LINUX_DIST_DIR:-$REPO_ROOT/dist/linux}"
APPDIR="${NGIMAGEVIEWER_LINUX_APPDIR:-$DIST_DIR/NgImageViewer.AppDir}"
APPIMAGE_OUTPUT="$DIST_DIR/NgImageViewer-x86_64.AppImage"
CMAKE_BIN="${CMAKE:-cmake}"

die() {
    printf 'error: %s\n' "$*" >&2
    exit 1
}

info() {
    printf '==> %s\n' "$*"
}

require_command() {
    command -v "$1" >/dev/null 2>&1 || die "required tool not found: $1"
}

build_jobs() {
    if command -v nproc >/dev/null 2>&1; then
        nproc
    else
        getconf _NPROCESSORS_ONLN 2>/dev/null || printf '1\n'
    fi
}

usable_file() {
    local path="$1"
    [[ -f "$path" && -s "$path" ]] || return 1
    if [[ ! -x "$path" ]]; then
        chmod +x "$path" || return 1
    fi
    [[ -x "$path" ]]
}

require_linux() {
    [[ "$(uname -s)" == "Linux" ]] || die "Linux packaging must run on Linux"
}

require_packaging_tools() {
    require_command "$CMAKE_BIN"
    require_command ldd
    require_command find
    require_command sed
    require_command sort
}

require_linux_build_dependencies() {
    local missing=()

    [[ -f /usr/include/GL/gl.h ]] || missing+=("OpenGL headers")
    [[ -e /usr/lib/x86_64-linux-gnu/libOpenGL.so || -e /usr/lib64/libOpenGL.so || -e /usr/lib/libOpenGL.so ]] \
        || missing+=("libOpenGL development symlink")
    [[ -e /usr/lib/x86_64-linux-gnu/libGLX.so || -e /usr/lib64/libGLX.so || -e /usr/lib/libGLX.so ]] \
        || missing+=("libGLX development symlink")
    [[ -e /usr/lib/x86_64-linux-gnu/libEGL.so || -e /usr/lib64/libEGL.so || -e /usr/lib/libEGL.so ]] \
        || missing+=("libEGL development symlink")

    if ((${#missing[@]})); then
        printf 'Missing Linux build dependencies:\n' >&2
        printf '  - %s\n' "${missing[@]}" >&2
        printf '\nOn Debian/Ubuntu, install:\n' >&2
        printf '  sudo apt install libgl-dev libopengl-dev libegl-dev libglx-dev\n' >&2
        die "QtGui needs OpenGL/EGL development files to configure the Linux package build."
    fi
}

require_submodules() {
    [[ -f "$REPO_ROOT/third_party/LibRaw/libraw/libraw.h" ]] \
        || die "missing LibRaw submodule. Run: git submodule update --init --recursive"
    [[ -f "$REPO_ROOT/third_party/LibRaw-cmake/CMakeLists.txt" ]] \
        || die "missing LibRaw-cmake submodule. Run: git submodule update --init --recursive"
    [[ -f "$REPO_ROOT/third_party/libheif/libheif/api/libheif/heif.h" ]] \
        || die "missing libheif submodule. Run: git submodule update --init --recursive"
    [[ -f "$REPO_ROOT/third_party/libde265/libde265/de265.h" ]] \
        || die "missing libde265 submodule. Run: git submodule update --init --recursive"
}

find_linuxdeploy() {
    if [[ -n "${LINUXDEPLOY:-}" && -x "$LINUXDEPLOY" ]]; then
        printf '%s\n' "$LINUXDEPLOY"
        return
    fi

    if command -v linuxdeploy >/dev/null 2>&1; then
        command -v linuxdeploy
        return
    fi

    local candidate
    for candidate in \
        "$REPO_ROOT/tools/linuxdeploy-x86_64.AppImage" \
        "$REPO_ROOT/linuxdeploy-x86_64.AppImage" \
        "$HOME/Downloads/linuxdeploy-x86_64.AppImage"; do
        if usable_file "$candidate"; then
            printf '%s\n' "$candidate"
            return
        fi
    done

    die "linuxdeploy not found. Put linuxdeploy-x86_64.AppImage in tools/ or ~/Downloads, or set LINUXDEPLOY=/path/to/linuxdeploy-x86_64.AppImage."
}

find_qt_plugin() {
    if [[ -n "${LINUXDEPLOY_PLUGIN_QT:-}" && -x "$LINUXDEPLOY_PLUGIN_QT" ]]; then
        printf '%s\n' "$LINUXDEPLOY_PLUGIN_QT"
        return
    fi

    if command -v linuxdeploy-plugin-qt >/dev/null 2>&1; then
        command -v linuxdeploy-plugin-qt
        return
    fi

    local candidate
    for candidate in \
        "$REPO_ROOT/tools/linuxdeploy-plugin-qt-x86_64.AppImage" \
        "$REPO_ROOT/linuxdeploy-plugin-qt-x86_64.AppImage" \
        "$HOME/Downloads/linuxdeploy-plugin-qt-x86_64.AppImage"; do
        if usable_file "$candidate"; then
            printf '%s\n' "$candidate"
            return
        fi
    done

    die "linuxdeploy Qt plugin not found. Put linuxdeploy-plugin-qt-x86_64.AppImage in tools/ or ~/Downloads, or set LINUXDEPLOY_PLUGIN_QT=/path/to/linuxdeploy-plugin-qt-x86_64.AppImage."
}

find_appimage_runtime() {
    if [[ -n "${APPIMAGE_RUNTIME:-}" && -f "$APPIMAGE_RUNTIME" && -s "$APPIMAGE_RUNTIME" ]]; then
        printf '%s\n' "$APPIMAGE_RUNTIME"
        return
    fi

    local candidate
    for candidate in \
        "$REPO_ROOT/tools/runtime-x86_64" \
        "$REPO_ROOT/runtime-x86_64" \
        "$HOME/Downloads/runtime-x86_64"; do
        if [[ -f "$candidate" && -s "$candidate" ]]; then
            printf '%s\n' "$candidate"
            return
        fi
    done

    return 1
}

find_appimagetool() {
    if [[ -n "${APPIMAGETOOL:-}" && -x "$APPIMAGETOOL" ]]; then
        printf '%s\n' "$APPIMAGETOOL"
        return
    fi

    if command -v appimagetool >/dev/null 2>&1; then
        command -v appimagetool
        return
    fi

    return 1
}

extract_appimagetool_from_linuxdeploy() {
    local linuxdeploy_bin="$1"
    local extract_dir="$BUILD_DIR/linuxdeploy-extracted"
    local tool

    [[ "$linuxdeploy_bin" == *.AppImage ]] \
        || die "appimagetool not found. Install appimagetool, set APPIMAGETOOL, or use linuxdeploy-x86_64.AppImage."

    info "Extracting appimagetool from linuxdeploy" >&2
    rm -rf "$extract_dir"
    mkdir -p "$extract_dir"
    (cd "$extract_dir" && "$linuxdeploy_bin" --appimage-extract >/dev/null)

    tool="$(find "$extract_dir/squashfs-root" -path '*/appimagetool' -type f -perm -u+x | sort | head -n 1 || true)"
    [[ -n "$tool" && -x "$tool" ]] || die "appimagetool not found inside linuxdeploy AppImage"
    printf '%s\n' "$tool"
}

candidate_qt_lib_dirs() {
    local candidates="${QT_PREFIX:-}:${CMAKE_PREFIX_PATH:-}"
    local candidate
    local -a candidate_array

    candidates="${candidates//;/:}"
    IFS=':' read -r -a candidate_array <<< "$candidates"
    for candidate in "${candidate_array[@]}"; do
        [[ -n "$candidate" && -d "$candidate/lib" ]] || continue
        printf '%s\n' "$candidate/lib"
    done
}

runtime_library_path() {
    local path="$APPDIR/usr/lib:$APPDIR/usr/lib/x86_64-linux-gnu"
    local qt_lib_dir

    while IFS= read -r qt_lib_dir; do
        case ":$path:" in
            *":$qt_lib_dir:"*) ;;
            *) path="$path:$qt_lib_dir" ;;
        esac
    done < <(candidate_qt_lib_dirs)

    if [[ -n "${LD_LIBRARY_PATH:-}" ]]; then
        path="$path:$LD_LIBRARY_PATH"
    fi

    printf '%s\n' "$path"
}

run_linuxdeploy() {
    local linuxdeploy_bin="$1"
    shift

    APPIMAGE_EXTRACT_AND_RUN="${APPIMAGE_EXTRACT_AND_RUN:-1}" \
    LD_LIBRARY_PATH="$(runtime_library_path)" \
        "$linuxdeploy_bin" "$@"
}

configure_release_build() {
    local args=(
        -S "$REPO_ROOT"
        -B "$BUILD_DIR"
        -DCMAKE_BUILD_TYPE=Release
        -DCMAKE_INSTALL_PREFIX=/usr
        -DBUILD_SHARED_LIBS=OFF
    )

    if [[ -n "${CMAKE_GENERATOR:-}" ]]; then
        args=(-G "$CMAKE_GENERATOR" "${args[@]}")
    fi

    if [[ -n "${CMAKE_PREFIX_PATH:-}" ]]; then
        args+=("-DCMAKE_PREFIX_PATH=$CMAKE_PREFIX_PATH")
        if [[ -z "${QMAKE:-}" && -x "$CMAKE_PREFIX_PATH/bin/qmake6" ]]; then
            export QMAKE="$CMAKE_PREFIX_PATH/bin/qmake6"
        fi
    fi

    if [[ -n "${NGIMAGEVIEWER_EXTRA_CMAKE_ARGS:-}" ]]; then
        # shellcheck disable=SC2206
        local extra_args=($NGIMAGEVIEWER_EXTRA_CMAKE_ARGS)
        args+=("${extra_args[@]}")
    fi

    info "Configuring Release build"
    "$CMAKE_BIN" "${args[@]}"
}

build_release_binary() {
    info "Building NgImageViewer"
    "$CMAKE_BIN" --build "$BUILD_DIR" --target NgImageViewer -j"${NGIMAGEVIEWER_BUILD_JOBS:-$(build_jobs)}"
    [[ -x "$BUILD_DIR/NgImageViewer" ]] || die "expected executable not found: $BUILD_DIR/NgImageViewer"
}

install_to_appdir() {
    info "Installing into AppDir staging area"
    rm -rf "$APPDIR"
    mkdir -p "$APPDIR"
    DESTDIR="$APPDIR" "$CMAKE_BIN" --install "$BUILD_DIR"
    [[ -x "$APPDIR/usr/bin/NgImageViewer" ]] || die "staged executable not found: $APPDIR/usr/bin/NgImageViewer"
}

deploy_appdir() {
    local linuxdeploy_bin="$1"
    local qt_plugin="$2"
    local desktop_file="$APPDIR/usr/share/applications/ngimageviewer.desktop"
    local icon_file="$APPDIR/usr/share/icons/hicolor/scalable/apps/ngimageviewer.svg"

    [[ -f "$desktop_file" ]] || die "desktop file missing from AppDir: $desktop_file"
    [[ -f "$icon_file" ]] || die "icon missing from AppDir: $icon_file"

    info "Deploying Qt/runtime libraries into AppDir"
    NO_STRIP="${NO_STRIP:-0}" \
    LINUXDEPLOY_PLUGIN_QT="$qt_plugin" \
        run_linuxdeploy "$linuxdeploy_bin" \
            --appdir "$APPDIR" \
            --executable "$APPDIR/usr/bin/NgImageViewer" \
            --desktop-file "$desktop_file" \
            --icon-file "$icon_file" \
            --plugin qt
}

build_appimage() {
    local linuxdeploy_bin="$1"
    local runtime_file
    local appimagetool_bin
    if [[ "${NGIMAGEVIEWER_LINUX_APPIMAGE:-1}" != "1" ]]; then
        printf 'AppImage generation skipped by NGIMAGEVIEWER_LINUX_APPIMAGE=0.\n'
        return
    fi

    info "Building AppImage"
    rm -f "$APPIMAGE_OUTPUT"

    if LDAI_OUTPUT="$APPIMAGE_OUTPUT" OUTPUT="$APPIMAGE_OUTPUT" \
            run_linuxdeploy "$linuxdeploy_bin" --appdir "$APPDIR" --output appimage; then
        [[ -f "$APPIMAGE_OUTPUT" ]] || die "expected AppImage not found: $APPIMAGE_OUTPUT"
        return
    fi

    info "linuxdeploy AppImage output failed; trying appimagetool fallback"
    runtime_file="$(find_appimage_runtime || true)"
    if [[ -z "$runtime_file" ]]; then
        die "AppImage runtime not found. Download runtime-x86_64 from https://github.com/AppImage/type2-runtime/releases/download/continuous/runtime-x86_64 into tools/ or ~/Downloads, or set APPIMAGE_RUNTIME=/path/to/runtime-x86_64."
    fi

    appimagetool_bin="$(find_appimagetool || true)"
    if [[ -z "$appimagetool_bin" ]]; then
        appimagetool_bin="$(extract_appimagetool_from_linuxdeploy "$linuxdeploy_bin")"
    fi

    "$appimagetool_bin" --runtime-file "$runtime_file" "$APPDIR" "$APPIMAGE_OUTPUT"
    [[ -f "$APPIMAGE_OUTPUT" ]] || die "expected AppImage not found: $APPIMAGE_OUTPUT"
}

check_appdir_dependencies() {
    local missing
    info "Checking AppDir runtime dependencies"
    missing="$(LD_LIBRARY_PATH="$(runtime_library_path)" \
        ldd "$APPDIR/usr/bin/NgImageViewer" | sed -n '/not found/p' || true)"

    if [[ -f "$APPDIR/usr/plugins/platforms/libqxcb.so" ]]; then
        missing+="$(
            LD_LIBRARY_PATH="$(runtime_library_path)" \
                ldd "$APPDIR/usr/plugins/platforms/libqxcb.so" | sed -n '/not found/p' || true
        )"
    fi

    if [[ -n "$missing" ]]; then
        printf '%s\n' "$missing" >&2
        die "AppDir still has missing shared libraries. Install the missing runtime packages on the build machine or add them to $APPDIR/usr/lib."
    fi
}

print_dependency_report() {
    info "Main binary dependency report"
    if command -v ldd >/dev/null 2>&1; then
        ldd "$APPDIR/usr/bin/NgImageViewer" || true
    else
        printf 'ldd not found; dependency report skipped.\n'
    fi
}

require_linux
require_packaging_tools
require_linux_build_dependencies
require_submodules
LINUXDEPLOY_BIN="$(find_linuxdeploy)"
QT_PLUGIN_BIN="$(find_qt_plugin)"

mkdir -p "$DIST_DIR"
configure_release_build
build_release_binary
install_to_appdir
deploy_appdir "$LINUXDEPLOY_BIN" "$QT_PLUGIN_BIN"
check_appdir_dependencies
build_appimage "$LINUXDEPLOY_BIN"
print_dependency_report

info "Done"
printf 'AppDir: %s\n' "$APPDIR"
if [[ -f "$APPIMAGE_OUTPUT" ]]; then
    printf 'AppImage: %s\n' "$APPIMAGE_OUTPUT"
fi
