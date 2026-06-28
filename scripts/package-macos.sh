#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
DEFAULT_APP="$REPO_ROOT/build/Qt_6_11_1_for_macOS_Release/NgImageViewer.app"
APP="${1:-$DEFAULT_APP}"
APP="$(cd "$(dirname "$APP")" && pwd)/$(basename "$APP")"
EXE="$APP/Contents/MacOS/NgImageViewer"
FRAMEWORKS="$APP/Contents/Frameworks"
BUILD_DIR="$(dirname "$APP")"
CMAKE_CACHE="$BUILD_DIR/CMakeCache.txt"

die() {
    printf 'error: %s\n' "$*" >&2
    exit 1
}

info() {
    printf '==> %s\n' "$*"
}

cache_value() {
    local key="$1"
    local file="$2"
    if [[ -f "$file" ]]; then
        awk -F= -v key="$key" '$1 ~ "^" key ":" { print $2; exit }' "$file"
    fi
}

require_release_build() {
    case "$APP" in
        *Debug*|*debug*)
            die "refusing to package a Debug app: $APP"
            ;;
    esac

    if [[ -f "$CMAKE_CACHE" ]]; then
        local build_type
        build_type="$(cache_value CMAKE_BUILD_TYPE "$CMAKE_CACHE")"
        if [[ "$build_type" != "Release" ]]; then
            die "refusing to package non-Release CMake build type: ${build_type:-<empty>}"
        fi
        return
    fi

    case "$BUILD_DIR" in
        *Release*|*release*)
            return
            ;;
    esac

    die "cannot verify this is a Release build: missing $CMAKE_CACHE"
}

find_macdeployqt() {
    if [[ -n "${MACDEPLOYQT:-}" && -x "$MACDEPLOYQT" ]]; then
        printf '%s\n' "$MACDEPLOYQT"
        return
    fi

    local from_cache
    from_cache="$(cache_value MACDEPLOYQT_EXECUTABLE "$CMAKE_CACHE")"
    if [[ -n "$from_cache" && -x "$from_cache" ]]; then
        printf '%s\n' "$from_cache"
        return
    fi

    if command -v macdeployqt >/dev/null 2>&1; then
        command -v macdeployqt
        return
    fi

    local prefix
    prefix="$(cache_value CMAKE_PREFIX_PATH "$CMAKE_CACHE")"
    if [[ -n "$prefix" && -x "$prefix/bin/macdeployqt" ]]; then
        printf '%s\n' "$prefix/bin/macdeployqt"
        return
    fi

    if [[ -n "${QT_PREFIX:-}" && -x "$QT_PREFIX/bin/macdeployqt" ]]; then
        printf '%s\n' "$QT_PREFIX/bin/macdeployqt"
        return
    fi

    die "macdeployqt not found. Set MACDEPLOYQT=/path/to/macdeployqt."
}

find_dylibbundler() {
    if [[ -n "${DYLIBBUNDLER:-}" && -x "$DYLIBBUNDLER" ]]; then
        printf '%s\n' "$DYLIBBUNDLER"
        return
    fi

    if command -v dylibbundler >/dev/null 2>&1; then
        command -v dylibbundler
        return
    fi

    die "dylibbundler not found. Install it with: brew install dylibbundler"
}

thin_macho_binaries() {
    local arch="${NGIMAGEVIEWER_THIN_ARCH:-}"
    if [[ -z "$arch" ]]; then
        return
    fi

    case "$arch" in
        arm64|x86_64)
            ;;
        *)
            die "unsupported NGIMAGEVIEWER_THIN_ARCH: $arch. Expected arm64 or x86_64."
            ;;
    esac

    command -v lipo >/dev/null 2>&1 || die "lipo not found"
    command -v codesign >/dev/null 2>&1 || die "codesign not found"

    info "Thinning Mach-O binaries to $arch"

    local file
    local lipo_info
    local mode
    local tmp
    local thinned=0

    while IFS= read -r -d '' file; do
        lipo_info="$(lipo -info "$file" 2>/dev/null || true)"
        if [[ -z "$lipo_info" ]]; then
            continue
        fi

        if [[ "$lipo_info" == *"Non-fat file:"* ]]; then
            if [[ "$lipo_info" != *"architecture: $arch"* ]]; then
                die "found non-$arch Mach-O binary after thinning was requested: $file ($lipo_info)"
            fi
            continue
        fi

        if [[ "$lipo_info" != *" $arch"* ]]; then
            die "cannot thin Mach-O binary without $arch slice: $file ($lipo_info)"
        fi

        mode="$(stat -f '%Lp' "$file")"
        tmp="$(mktemp "${TMPDIR:-/tmp}/ngimageviewer-thin.XXXXXX")"
        lipo "$file" -thin "$arch" -output "$tmp"
        chmod "$mode" "$tmp"
        mv "$tmp" "$file"
        thinned=$((thinned + 1))
    done < <(find "$APP/Contents" -type f -print0)

    printf 'Thinned Mach-O binaries: %s\n' "$thinned"

    info "Re-signing app after architecture thinning"
    codesign --force --deep --sign - "$APP"
}

print_local_dependency_report() {
    local title="$1"
    info "$title"

    local output
    output="$(mktemp)"
    {
        otool -L "$EXE"
        find "$APP/Contents" -type f \( -name '*.dylib' -o -perm -111 \) -print0 \
            | xargs -0 -n 1 otool -L 2>/dev/null || true
    } > "$output"

    if grep -E '^[[:space:]]+/(opt/homebrew|usr/local)/' "$output"; then
        rm -f "$output"
        die "found unresolved Homebrew/local dylib references after packaging"
    fi

    rm -f "$output"
    printf 'No /opt/homebrew or /usr/local dylib references found.\n'
}

assert_qt_frameworks_present() {
    local missing=0
    local framework
    for framework in QtCore QtGui QtWidgets QtSvg; do
        if [[ ! -x "$FRAMEWORKS/$framework.framework/Versions/A/$framework" ]]; then
            printf 'missing Qt framework: %s\n' "$FRAMEWORKS/$framework.framework/Versions/A/$framework" >&2
            missing=1
        fi
    done

    if [[ "$missing" -ne 0 ]]; then
        die "Qt frameworks are missing from the app bundle"
    fi
}

[[ "$(uname -s)" == "Darwin" ]] || die "macOS packaging must run on macOS"
[[ -d "$APP" ]] || die "app bundle not found: $APP"
[[ -x "$EXE" ]] || die "app executable not found or not executable: $EXE"

require_release_build

MACDEPLOYQT_BIN="$(find_macdeployqt)"
DYLIBBUNDLER_BIN="$(find_dylibbundler)"

info "Packaging Release app"
printf 'App: %s\n' "$APP"
printf 'macdeployqt: %s\n' "$MACDEPLOYQT_BIN"
printf 'dylibbundler: %s\n' "$DYLIBBUNDLER_BIN"

info "Running macdeployqt"
"$MACDEPLOYQT_BIN" "$APP" -verbose=2
assert_qt_frameworks_present

mkdir -p "$FRAMEWORKS"

info "Running dylibbundler for third-party dylibs"
"$DYLIBBUNDLER_BIN" \
    -of \
    -cd \
    -b \
    -x "$EXE" \
    -d "$FRAMEWORKS" \
    -p @executable_path/../Frameworks
assert_qt_frameworks_present

thin_macho_binaries

print_local_dependency_report "Checking bundled dependency references"

info "Done"
printf 'Packaged app: %s\n' "$APP"
