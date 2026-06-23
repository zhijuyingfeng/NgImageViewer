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
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        pkg_check_modules(LIBRAW QUIET IMPORTED_TARGET libraw)
    endif()

    if(NOT TARGET PkgConfig::LIBRAW)
        find_package(libraw CONFIG QUIET)
        find_package(LibRaw CONFIG QUIET)
    endif()

    foreach(candidate
            PkgConfig::LIBRAW
            libraw::raw
            libraw::libraw
            LibRaw::raw
            LibRaw::LibRaw
            unofficial::libraw::libraw)
        if(NOT NGIMAGEVIEWER_HAS_LIBRAW AND TARGET ${candidate})
            set(NGIMAGEVIEWER_LIBRAW_TARGET ${candidate})
            set(NGIMAGEVIEWER_HAS_LIBRAW ON)
        endif()
    endforeach()

    if(NOT NGIMAGEVIEWER_HAS_LIBRAW)
        find_path(LIBRAW_INCLUDE_DIR NAMES libraw/libraw.h)
        find_library(LIBRAW_LIBRARY NAMES raw raw_r libraw LibRaw)
        if(LIBRAW_INCLUDE_DIR AND LIBRAW_LIBRARY)
            add_library(NgImageViewerLibRaw UNKNOWN IMPORTED)
            set_target_properties(NgImageViewerLibRaw PROPERTIES
                IMPORTED_LOCATION "${LIBRAW_LIBRARY}"
                INTERFACE_INCLUDE_DIRECTORIES "${LIBRAW_INCLUDE_DIR}"
            )
            set(NGIMAGEVIEWER_LIBRAW_TARGET NgImageViewerLibRaw)
            set(NGIMAGEVIEWER_HAS_LIBRAW ON)
        endif()
    endif()
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
