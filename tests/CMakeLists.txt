set(TEST_SOURCES
    test_version.cpp
    test_renderer_smoke.cpp
    test_color.cpp
    test_geometry.cpp
    test_pen_brush.cpp
    test_transform.cpp
    test_tessellator.cpp
    test_image.cpp
    test_render_batcher.cpp
    test_texture_image_upload.cpp
    test_event_translator.cpp
    test_app_config.cpp
    test_tictactoe_logic.cpp
)

add_executable(sdl_painter_tests ${TEST_SOURCES})

target_include_directories(sdl_painter_tests PRIVATE
    ${PROJECT_SOURCE_DIR}/src
    # tictactoe demosunun saf oyun mantığı (tictactoe_logic.h) burada.
    ${PROJECT_SOURCE_DIR}/examples
    # Gömülü shader başlıkları — test_renderer_smoke.cpp SPIR-V'yi doğruluyor.
    ${PROJECT_BINARY_DIR}/generated
)

target_link_libraries(sdl_painter_tests
    PRIVATE sdl_painter
    PRIVATE sdl_painter_app
    PRIVATE GTest::gtest
    PRIVATE GTest::gtest_main
)

sdlpainter_copy_runtime_dlls(sdl_painter_tests)

include(GoogleTest)
gtest_add_tests(TARGET sdl_painter_tests)
