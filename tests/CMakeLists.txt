function(ngimageviewer_add_qt_test target)
    qt_add_executable(${target}
        ${ARGN}
    )
    target_include_directories(${target}
        PRIVATE
            "${NGIMAGEVIEWER_SOURCE_DIR}"
    )
    target_link_libraries(${target}
        PRIVATE
            Qt::Core
            Qt::Gui
            Qt::Test
    )
    add_test(NAME ${target} COMMAND ${target})
endfunction()

ngimageviewer_add_qt_test(test_imageformats
    test_imageformats.cpp
    "${NGIMAGEVIEWER_SOURCE_DIR}/imageformats.cpp"
    "${NGIMAGEVIEWER_SOURCE_DIR}/imageformats.h"
)

ngimageviewer_add_qt_test(test_imagesequence
    test_imagesequence.cpp
    "${NGIMAGEVIEWER_SOURCE_DIR}/imageformats.cpp"
    "${NGIMAGEVIEWER_SOURCE_DIR}/imageformats.h"
    "${NGIMAGEVIEWER_SOURCE_DIR}/imagesequence.cpp"
    "${NGIMAGEVIEWER_SOURCE_DIR}/imagesequence.h"
)
