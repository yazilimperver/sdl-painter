set(TEST_SOURCES
    test_version.cpp
    test_renderer_smoke.cpp
    test_frame_render.cpp
    test_color.cpp
    test_geometry.cpp
    test_pen_brush.cpp
    test_transform.cpp
    test_tessellator.cpp
    test_path.cpp
    test_render_target.cpp
    test_backend_parity.cpp
    test_painter.cpp
    test_font.cpp
    test_text_utf8.cpp
    test_vk_check.cpp
    test_image.cpp
    test_render_batcher.cpp
    test_texture_image_upload.cpp
    test_event_translator.cpp
    test_app_config.cpp
    test_tictactoe_logic.cpp
    test_collision_logic.cpp
)

include(GoogleTest)

# --- Beyaz kutu birim testleri ------------------------------------------------
#
# Bu takim `src/` altindaki DAHILI basliklari include eder ve export EDILMEYEN
# sembolleri (Tessellator, RenderBatcher, vk_detail::VkResultToString ...)
# dogrudan cagirir. Paylasimli derlemede bunlar DLL'den disari verilmedigi icin
# hedef link EDILEMEZ — ve edilmemeli: dahili detaylar ABI yuzeyine cikmamali.
#
# Bu yuzden paylasimli derlemede bu takim atlanir; public yuzeyi dogrulayan
# `sdl_painter_abi_tests` her iki modda da derlenir ve kosar.
if(BUILD_SHARED_LIBS)
    message(STATUS
        "sdl_painter_tests atlandi: beyaz kutu testleri dahili sembollere "
        "erisir, paylasimli derlemede link edilemez. Public yuzey "
        "sdl_painter_abi_tests ile dogrulanir.")
else()
    add_executable(sdl_painter_tests ${TEST_SOURCES})

    target_include_directories(sdl_painter_tests PRIVATE
        ${PROJECT_SOURCE_DIR}/src
        # Örneklerin saf oyun mantığı burada: tictactoe_logic.h,
        # collision_logic.h (breakout).
        ${PROJECT_SOURCE_DIR}/examples/games
        # Gömülü shader başlıkları — test_renderer_smoke.cpp SPIR-V'yi doğruluyor.
        ${PROJECT_BINARY_DIR}/generated
    )

    target_link_libraries(sdl_painter_tests
        PRIVATE sdl_painter
        PRIVATE sdl_painter_app
        PRIVATE GTest::gtest
        PRIVATE GTest::gtest_main
    )

    # test_vk_check.cpp dahili vk_check.h'yi test ediyor; o baslik
    # <vulkan/vulkan.h> ve spdlog cekiyor. Ikisi de sdl_painter'a PRIVATE bagli
    # oldugundan test hedefine ayrica verilmeli.
    if(SDLPAINTER_WITH_VULKAN)
        target_link_libraries(sdl_painter_tests
            PRIVATE vulkan-headers::vulkan-headers
            PRIVATE ${SDLPAINTER_SPDLOG_TARGET}
        )
    endif()

    sdlpainter_copy_runtime_dlls(sdl_painter_tests)

    gtest_add_tests(TARGET sdl_painter_tests)
endif()

# --- Public ABI / DLL testleri ------------------------------------------------
#
# AYRI bir hedef olmasinin sebebi yukaridaki takimla arasindaki tek farkta:
# `sdl_painter_tests` DAHILI basliklari (src/) include eder ve Tessellator,
# RenderBatcher, vk_detail::VkResultToString gibi export EDILMEYEN sembolleri
# cagirir. Bu bilincli — dahili detaylar ABI yuzeyine cikmamali. Sonucu ise
# `BUILD_SHARED_LIBS=ON` ile o hedefin link EDILEMEMESI.
#
# Bu hedef yalnizca public basliklari kullanir ve `src/` include yoluna sahip
# DEGILDIR; yani kutuphaneye tuketicinin gordugu yuzeyden baglanir. Shared
# derlemede bir public sembol SDLPAINTER_API ile isaretlenmeyi unutulursa
# LNK2019 ile duser.
add_executable(sdl_painter_abi_tests test_public_abi.cpp)

# DIKKAT: buraya ${PROJECT_SOURCE_DIR}/src EKLENMEYECEK. Testin degeri, public
# yuzeyle sinirli kalmasindan geliyor.
target_include_directories(sdl_painter_abi_tests PRIVATE
    # mock_renderer.h — yalnizca public basliklari include eder, IRenderer'i
    # tuketici gibi implemente eder.
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(sdl_painter_abi_tests
    PRIVATE sdl_painter::sdl_painter
    PRIVATE sdl_painter::app
    PRIVATE GTest::gtest
    PRIVATE GTest::gtest_main
)

set_target_warnings(sdl_painter_abi_tests)
set_target_sanitizers(sdl_painter_abi_tests)

sdlpainter_copy_runtime_dlls(sdl_painter_abi_tests)
gtest_add_tests(TARGET sdl_painter_abi_tests)
