set(TEST_SOURCES
    test_color.cpp
    test_geometry.cpp
    test_pen_brush.cpp
    test_transform.cpp
    test_tessellator.cpp
    test_image.cpp
    test_render_batcher.cpp
    test_texture_image_upload.cpp
)

add_executable(sdl_painter_tests ${TEST_SOURCES})

target_include_directories(sdl_painter_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(sdl_painter_tests
    PRIVATE sdl_painter
    PRIVATE GTest::gtest
    PRIVATE GTest::gtest_main
)

include(GoogleTest)
gtest_add_tests(TARGET sdl_painter_tests)
