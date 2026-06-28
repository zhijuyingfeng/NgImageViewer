function(ngimageviewer_add_qt_test target)
    qt_add_executable(${target}
        ${ARGN}
    )
    target_include_directories(${target}
        PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}"
    )
    target_link_libraries(${target}
        PRIVATE
            ngiv_image
            Qt::Test
    )
    add_test(NAME ${target} COMMAND ${target})
endfunction()

ngimageviewer_add_qt_test(test_imageformats
    test_imageformats.cpp
)

ngimageviewer_add_qt_test(test_imageloader
    test_imageloader.cpp
)

ngimageviewer_add_qt_test(test_imagesequence
    test_imagesequence.cpp
)
