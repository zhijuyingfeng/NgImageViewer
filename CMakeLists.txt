cmake_minimum_required(VERSION 3.19)

if(APPLE AND NOT DEFINED CMAKE_OSX_DEPLOYMENT_TARGET)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "15.0" CACHE STRING "Minimum macOS version")
endif()

project(NgImageViewer LANGUAGES CXX)

include(GNUInstallDirs)
if(WIN32)
    enable_language(RC)
endif()

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Gui Widgets Svg LinguistTools)

qt_standard_project_setup()
set(CMAKE_AUTORCC ON)
option(NGIMAGEVIEWER_ENABLE_RAW "Enable RAW image decoding through LibRaw when available" ON)
option(NGIMAGEVIEWER_ENABLE_HEIF "Enable HEIF/HEIC decoding through bundled libheif when available" ON)

set(NGIMAGEVIEWER_MACOS_ICON "${CMAKE_CURRENT_SOURCE_DIR}/resources/macos/NgImageViewer.icns")
set(NGIMAGEVIEWER_LINUX_ICON "${CMAKE_CURRENT_SOURCE_DIR}/resources/icons/app-icon.svg")
set(NGIMAGEVIEWER_LINUX_DESKTOP_FILE "${CMAKE_CURRENT_SOURCE_DIR}/resources/linux/ngimageviewer.desktop")
set(NGIMAGEVIEWER_WINDOWS_RC "${CMAKE_CURRENT_SOURCE_DIR}/resources/windows/NgImageViewer.rc")
set(NGIMAGEVIEWER_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src")

set(NGIMAGEVIEWER_PLATFORM_SOURCES)
if(WIN32)
    list(APPEND NGIMAGEVIEWER_PLATFORM_SOURCES "${NGIMAGEVIEWER_WINDOWS_RC}")
endif()

qt_add_executable(NgImageViewer
    WIN32 MACOSX_BUNDLE
    ${NGIMAGEVIEWER_SOURCE_DIR}/main.cpp
    ${NGIMAGEVIEWER_SOURCE_DIR}/imageformats.cpp
    ${NGIMAGEVIEWER_SOURCE_DIR}/imageformats.h
    ${NGIMAGEVIEWER_SOURCE_DIR}/imagewidgets.cpp
    ${NGIMAGEVIEWER_SOURCE_DIR}/imagewidgets.h
    ${NGIMAGEVIEWER_SOURCE_DIR}/mainwindow.cpp
    ${NGIMAGEVIEWER_SOURCE_DIR}/mainwindow.h
    ${NGIMAGEVIEWER_SOURCE_DIR}/heifdecoder.cpp
    ${NGIMAGEVIEWER_SOURCE_DIR}/heifdecoder.h
    ${NGIMAGEVIEWER_SOURCE_DIR}/rawdecoder.cpp
    ${NGIMAGEVIEWER_SOURCE_DIR}/rawdecoder.h
    ${NGIMAGEVIEWER_MACOS_ICON}
    ${NGIMAGEVIEWER_PLATFORM_SOURCES}
    resources.qrc
)

set_source_files_properties(${NGIMAGEVIEWER_MACOS_ICON} PROPERTIES
    MACOSX_PACKAGE_LOCATION Resources
)

set_target_properties(NgImageViewer PROPERTIES
    MACOSX_BUNDLE_ICON_FILE NgImageViewer.icns
)

qt_add_translations(NgImageViewer
    TS_FILES NgImageViewer_zh_CN.ts
)

target_link_libraries(NgImageViewer
    PRIVATE
        Qt::Core
        Qt::Gui
        Qt::Widgets
        Qt::Svg
)

target_include_directories(NgImageViewer
    PRIVATE
        "${NGIMAGEVIEWER_SOURCE_DIR}"
)

if(WIN32)
    target_include_directories(NgImageViewer PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/resources/windows")
endif()

set(NGIMAGEVIEWER_HAS_LIBRAW OFF)
if(NGIMAGEVIEWER_ENABLE_RAW)
    set(NGIMAGEVIEWER_BUNDLED_LIBRAW_PATH "${CMAKE_CURRENT_SOURCE_DIR}/third_party/LibRaw")
    set(NGIMAGEVIEWER_BUNDLED_LIBRAW_CMAKE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/third_party/LibRaw-cmake")

    if(NOT EXISTS "${NGIMAGEVIEWER_BUNDLED_LIBRAW_PATH}/libraw/libraw.h"
            OR NOT EXISTS "${NGIMAGEVIEWER_BUNDLED_LIBRAW_CMAKE_PATH}/CMakeLists.txt")
        message(FATAL_ERROR
            "RAW support requires bundled LibRaw submodules. Run:\n"
            "  git submodule update --init --recursive\n"
            "System LibRaw is intentionally not used because distribution versions are not reliable enough.")
    endif()

    set(LIBRAW_PATH "${NGIMAGEVIEWER_BUNDLED_LIBRAW_PATH}" CACHE PATH "Bundled LibRaw source path" FORCE)
    set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build bundled LibRaw as a static library" FORCE)
    set(ENABLE_EXAMPLES OFF CACHE BOOL "Do not build bundled LibRaw examples" FORCE)
    set(ENABLE_OPENMP OFF CACHE BOOL "Do not build bundled LibRaw with OpenMP" FORCE)
    set(ENABLE_JASPER OFF CACHE BOOL "Do not build bundled LibRaw with Jasper" FORCE)
    set(ENABLE_RAWSPEED OFF CACHE BOOL "Do not build bundled LibRaw with RawSpeed" FORCE)
    set(LIBRAW_INSTALL OFF CACHE BOOL "Do not install bundled LibRaw targets" FORCE)
    add_subdirectory(
        "${NGIMAGEVIEWER_BUNDLED_LIBRAW_CMAKE_PATH}"
        "${CMAKE_CURRENT_BINARY_DIR}/third_party/LibRaw-cmake"
        EXCLUDE_FROM_ALL)
    foreach(libraw_target raw raw_r)
        if(TARGET ${libraw_target})
            if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|AppleClang")
                target_compile_options(${libraw_target} PRIVATE
                    -Wno-deprecated-declarations
                    -Wno-macro-redefined)
            elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
                target_compile_options(${libraw_target} PRIVATE
                    -Wno-deprecated-declarations)
            endif()
        endif()
    endforeach()
    set(NGIMAGEVIEWER_LIBRAW_TARGET libraw::libraw)
    set(NGIMAGEVIEWER_HAS_LIBRAW ON)
endif()

if(NGIMAGEVIEWER_HAS_LIBRAW)
    target_compile_definitions(NgImageViewer PRIVATE NGIMAGEVIEWER_HAS_LIBRAW=1)
    target_link_libraries(NgImageViewer PRIVATE ${NGIMAGEVIEWER_LIBRAW_TARGET})
    message(STATUS "RAW support: enabled with ${NGIMAGEVIEWER_LIBRAW_TARGET}")
else()
    target_compile_definitions(NgImageViewer PRIVATE NGIMAGEVIEWER_HAS_LIBRAW=0)
    message(STATUS "RAW support: disabled. Install LibRaw to enable RAW decoding.")
endif()

set(NGIMAGEVIEWER_HAS_LIBHEIF OFF)
if(NGIMAGEVIEWER_ENABLE_HEIF)
    set(NGIMAGEVIEWER_BUNDLED_LIBDE265_PATH "${CMAKE_CURRENT_SOURCE_DIR}/third_party/libde265")
    set(NGIMAGEVIEWER_BUNDLED_LIBHEIF_PATH "${CMAKE_CURRENT_SOURCE_DIR}/third_party/libheif")

    if(NOT EXISTS "${NGIMAGEVIEWER_BUNDLED_LIBDE265_PATH}/libde265/de265.h"
            OR NOT EXISTS "${NGIMAGEVIEWER_BUNDLED_LIBHEIF_PATH}/libheif/api/libheif/heif.h")
        message(FATAL_ERROR
            "HEIF support requires bundled libde265/libheif submodules. Run:\n"
            "  git submodule update --init --recursive\n"
            "System libheif is intentionally not used because distribution versions and codec backends are not reliable enough.")
    endif()

    set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build bundled HEIF dependencies as static libraries" FORCE)

    set(ENABLE_SDL OFF CACHE BOOL "Do not build bundled libde265 SDL tools" FORCE)
    set(ENABLE_DECODER ON CACHE BOOL "Build bundled libde265 decoder" FORCE)
    set(ENABLE_ENCODER OFF CACHE BOOL "Do not build bundled libde265 encoder" FORCE)
    set(ENABLE_SHERLOCK265 OFF CACHE BOOL "Do not build bundled libde265 visual tool" FORCE)
    set(ENABLE_INTERNAL_DEVELOPMENT_TOOLS OFF CACHE BOOL "Do not build bundled libde265 development tools" FORCE)
    set(WITH_FUZZERS OFF CACHE BOOL "Do not build bundled libde265 fuzzers" FORCE)
    add_subdirectory(
        "${NGIMAGEVIEWER_BUNDLED_LIBDE265_PATH}"
        "${CMAKE_CURRENT_BINARY_DIR}/third_party/libde265"
        EXCLUDE_FROM_ALL)
    if(TARGET de265)
        target_include_directories(de265 PRIVATE
            "${CMAKE_CURRENT_BINARY_DIR}/third_party/libde265")
    endif()

    set(ENABLE_PLUGIN_LOADING OFF CACHE BOOL "Do not load libheif codec plugins at runtime" FORCE)
    set(WITH_LIBDE265 ON CACHE BOOL "Build libheif with bundled libde265 HEVC decoder" FORCE)
    set(WITH_LIBDE265_PLUGIN OFF CACHE BOOL "Build libde265 backend into libheif" FORCE)
    set(WITH_X265 OFF CACHE BOOL "Do not build HEVC encoder" FORCE)
    set(WITH_KVAZAAR OFF CACHE BOOL "Do not build kvazaar encoder" FORCE)
    set(WITH_UVG266 OFF CACHE BOOL "Do not build VVC encoder" FORCE)
    set(WITH_VVDEC OFF CACHE BOOL "Do not build VVC decoder" FORCE)
    set(WITH_VVENC OFF CACHE BOOL "Do not build VVC encoder" FORCE)
    set(WITH_X264 OFF CACHE BOOL "Do not build AVC encoder" FORCE)
    set(WITH_OpenH264_DECODER OFF CACHE BOOL "Do not build OpenH264 decoder" FORCE)
    set(WITH_AOM_DECODER OFF CACHE BOOL "Do not build AV1 decoder" FORCE)
    set(WITH_AOM_ENCODER OFF CACHE BOOL "Do not build AV1 encoder" FORCE)
    set(WITH_DAV1D OFF CACHE BOOL "Do not build dav1d decoder" FORCE)
    set(WITH_SvtEnc OFF CACHE BOOL "Do not build SVT encoder" FORCE)
    set(WITH_RAV1E OFF CACHE BOOL "Do not build rav1e encoder" FORCE)
    set(WITH_JPEG_DECODER OFF CACHE BOOL "Do not build JPEG decoder" FORCE)
    set(WITH_JPEG_ENCODER OFF CACHE BOOL "Do not build JPEG encoder" FORCE)
    set(WITH_OpenJPEG_DECODER OFF CACHE BOOL "Do not build OpenJPEG decoder" FORCE)
    set(WITH_OpenJPEG_ENCODER OFF CACHE BOOL "Do not build OpenJPEG encoder" FORCE)
    set(WITH_FFMPEG_DECODER OFF CACHE BOOL "Do not build ffmpeg decoder" FORCE)
    set(WITH_OPENJPH_DECODER OFF CACHE BOOL "Do not build OpenJPH decoder" FORCE)
    set(WITH_OPENJPH_ENCODER OFF CACHE BOOL "Do not build OpenJPH encoder" FORCE)
    set(WITH_UNCOMPRESSED_CODEC OFF CACHE BOOL "Do not build uncompressed HEIF codec" FORCE)
    set(WITH_WEBCODECS OFF CACHE BOOL "Do not build web codecs backend" FORCE)
    set(WITH_LIBSHARPYUV OFF CACHE BOOL "Do not build libsharpyuv" FORCE)
    set(WITH_EXAMPLES OFF CACHE BOOL "Do not build libheif examples" FORCE)
    set(WITH_EXAMPLE_HEIF_THUMB OFF CACHE BOOL "Do not build libheif thumbnailer" FORCE)
    set(WITH_EXAMPLE_HEIF_VIEW OFF CACHE BOOL "Do not build libheif viewer" FORCE)
    set(WITH_GDK_PIXBUF OFF CACHE BOOL "Do not build gdk-pixbuf plugin" FORCE)
    set(BUILD_DEVELOPMENT_TOOLS OFF CACHE BOOL "Do not build libheif development tools" FORCE)
    set(BUILD_DOCUMENTATION OFF CACHE BOOL "Do not build libheif documentation" FORCE)
    set(BUILD_TESTING OFF CACHE BOOL "Do not build libheif tests" FORCE)
    set(CMAKE_DISABLE_FIND_PACKAGE_TIFF TRUE CACHE BOOL "Do not let bundled libheif use system TIFF" FORCE)
    set(CMAKE_DISABLE_FIND_PACKAGE_JPEG TRUE CACHE BOOL "Do not let bundled libheif use system JPEG" FORCE)
    set(CMAKE_DISABLE_FIND_PACKAGE_PNG TRUE CACHE BOOL "Do not let bundled libheif use system PNG" FORCE)
    set(CMAKE_DISABLE_FIND_PACKAGE_WEBP TRUE CACHE BOOL "Do not let bundled libheif use system WebP" FORCE)

    set(LIBDE265_INCLUDE_DIR "${NGIMAGEVIEWER_BUNDLED_LIBDE265_PATH}" CACHE PATH "Bundled libde265 include path" FORCE)
    set(LIBDE265_LIBRARY de265 CACHE STRING "Bundled libde265 target" FORCE)
    add_subdirectory(
        "${NGIMAGEVIEWER_BUNDLED_LIBHEIF_PATH}"
        "${CMAKE_CURRENT_BINARY_DIR}/third_party/libheif"
        EXCLUDE_FROM_ALL)
    if(TARGET heif)
        target_include_directories(heif PRIVATE
            "${CMAKE_CURRENT_BINARY_DIR}/third_party/libde265")
        set(NGIMAGEVIEWER_HAS_LIBHEIF ON)
    endif()
endif()

if(NGIMAGEVIEWER_HAS_LIBHEIF)
    target_compile_definitions(NgImageViewer PRIVATE NGIMAGEVIEWER_HAS_LIBHEIF=1)
    target_include_directories(NgImageViewer PRIVATE
        "${NGIMAGEVIEWER_BUNDLED_LIBHEIF_PATH}/libheif/api"
        "${CMAKE_CURRENT_BINARY_DIR}/third_party/libheif")
    target_link_libraries(NgImageViewer PRIVATE heif)
    message(STATUS "HEIF support: enabled with bundled libheif + libde265")
else()
    target_compile_definitions(NgImageViewer PRIVATE NGIMAGEVIEWER_HAS_LIBHEIF=0)
    message(STATUS "HEIF support: disabled")
endif()

install(TARGETS NgImageViewer
    BUNDLE  DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

if(UNIX AND NOT APPLE)
    install(FILES "${NGIMAGEVIEWER_LINUX_ICON}"
        DESTINATION "${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps"
        RENAME ngimageviewer.svg)
    install(FILES "${NGIMAGEVIEWER_LINUX_DESKTOP_FILE}"
        DESTINATION "${CMAKE_INSTALL_DATADIR}/applications")
endif()

qt_generate_deploy_app_script(
    TARGET NgImageViewer
    OUTPUT_SCRIPT deploy_script
    NO_UNSUPPORTED_PLATFORM_ERROR
)
install(SCRIPT ${deploy_script})
