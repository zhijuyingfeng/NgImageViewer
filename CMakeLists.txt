cmake_minimum_required(VERSION 3.19)

if(APPLE AND NOT DEFINED CMAKE_OSX_DEPLOYMENT_TARGET)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "15.0" CACHE STRING "Minimum macOS version")
endif()

project(NgImageViewer LANGUAGES CXX)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Gui Widgets Svg LinguistTools)

qt_standard_project_setup()
set(CMAKE_AUTORCC ON)
option(NGIMAGEVIEWER_ENABLE_RAW "Enable RAW image decoding through LibRaw when available" ON)

set(NGIMAGEVIEWER_MACOS_ICON "${CMAKE_CURRENT_SOURCE_DIR}/resources/macos/NgImageViewer.icns")

qt_add_executable(NgImageViewer
    WIN32 MACOSX_BUNDLE
    main.cpp
    imageformats.cpp
    imageformats.h
    imagewidgets.cpp
    imagewidgets.h
    mainwindow.cpp
    mainwindow.h
    rawdecoder.cpp
    rawdecoder.h
    ${NGIMAGEVIEWER_MACOS_ICON}
    resources.qrc
)

set_source_files_properties(${NGIMAGEVIEWER_MACOS_ICON} PROPERTIES
    MACOSX_PACKAGE_LOCATION Resources
)

set_target_properties(NgImageViewer PROPERTIES
    MACOSX_BUNDLE_ICON_FILE NgImageViewer.icns
)

qt_add_translations(
    TARGETS NgImageViewer
    TS_FILES NgImageViewer_zh_CN.ts
)

target_link_libraries(NgImageViewer
    PRIVATE
        Qt::Core
        Qt::Gui
        Qt::Widgets
        Qt::Svg
)

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

install(TARGETS NgImageViewer
    BUNDLE  DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

qt_generate_deploy_app_script(
    TARGET NgImageViewer
    OUTPUT_SCRIPT deploy_script
    NO_UNSUPPORTED_PLATFORM_ERROR
)
install(SCRIPT ${deploy_script})
