cmake_minimum_required(VERSION 3.19)
project(NgImageViewer LANGUAGES CXX)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Gui Widgets Svg LinguistTools)

qt_standard_project_setup()
set(CMAKE_AUTORCC ON)

qt_add_executable(NgImageViewer
    WIN32 MACOSX_BUNDLE
    main.cpp
    mainwindow.cpp
    mainwindow.h
    resources.qrc
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
