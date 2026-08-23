# SDL3_ttf'e acik link, yalnizca SDL_ttf API'sini DOGRUDAN cagiran orneklerde
# yapilir. sdl_painter'a PRIVATE bagli oldugundan link-zamani bagimliligi
# statik kutuphanede $<LINK_ONLY:...> ile zaten yayilir; yalnizca SDL_ttf
# BASLIGINI dahil eden hedeflerin include yoluna ihtiyaci vardir.

add_executable(hello_window hello_window.cpp)
target_link_libraries(hello_window PRIVATE sdl_painter spdlog::spdlog_header_only)
sdlpainter_copy_runtime_dlls(hello_window)

add_executable(primitives primitives.cpp)
target_link_libraries(primitives PRIVATE sdl_painter spdlog::spdlog_header_only)
sdlpainter_copy_runtime_dlls(primitives)

add_executable(transforms transforms.cpp)
target_link_libraries(transforms PRIVATE sdl_painter spdlog::spdlog_header_only)
sdlpainter_copy_runtime_dlls(transforms)

add_executable(clipping clipping.cpp)
target_link_libraries(clipping PRIVATE sdl_painter spdlog::spdlog_header_only)
sdlpainter_copy_runtime_dlls(clipping)

add_executable(images images.cpp)
target_link_libraries(images PRIVATE sdl_painter spdlog::spdlog_header_only)
sdlpainter_copy_runtime_dlls(images)

add_executable(text text.cpp)
target_link_libraries(text PRIVATE sdl_painter spdlog::spdlog_header_only)
sdlpainter_copy_runtime_dlls(text)

# Application çatısı demosu — SDL boilerplate'i olmadan animasyonlu çizim.
add_executable(app_basics app_basics.cpp)
target_link_libraries(app_basics PRIVATE sdl_painter_app spdlog::spdlog_header_only)
sdlpainter_copy_runtime_dlls(app_basics)

# Sabit adımlı (fixed-timestep) oyun döngüsü + interpolasyon demosu.
add_executable(game_loop game_loop.cpp)
target_link_libraries(game_loop PRIVATE sdl_painter_app spdlog::spdlog_header_only)
sdlpainter_copy_runtime_dlls(game_loop)

# Fare girdisi + durum makinesi + duyarlı yerleşim: eksiksiz uygulama örneği.
# Metin çizdiği için text gibi SDL3_ttf'e de bağlanır.
add_executable(tictactoe tictactoe.cpp)
target_link_libraries(tictactoe PRIVATE sdl_painter_app spdlog::spdlog_header_only)
sdlpainter_copy_runtime_dlls(tictactoe)

# README tanıtım GIF'ini üreten koreografili sahne. Metin çizdiği için
# text/tictactoe gibi SDL3_ttf'e de bağlanır.
add_executable(hero hero.cpp)
target_link_libraries(hero PRIVATE sdl_painter spdlog::spdlog_header_only)
sdlpainter_copy_runtime_dlls(hero)

if(SDLPAINTER_WITH_VULKAN)
    # Tek istisna: vulkan_text.cpp SDL_ttf'i DOGRUDAN kullaniyor
    # (TTF_Init/TTF_Quit), bu yuzden acik link ve baslik yolu gerekli.
    add_executable(vulkan_text vulkan_text.cpp)
    if(TARGET SDL3_ttf::SDL3_ttf-static)
        target_link_libraries(vulkan_text
            PRIVATE sdl_painter SDL3_ttf::SDL3_ttf-static spdlog::spdlog_header_only)
    else()
        target_link_libraries(vulkan_text
            PRIVATE sdl_painter SDL3_ttf::SDL3_ttf spdlog::spdlog_header_only)
    endif()
    sdlpainter_copy_runtime_dlls(vulkan_text)
endif()

if(SDLPAINTER_WITH_VULKAN)
    add_executable(vulkan_clear vulkan_clear.cpp)
    target_link_libraries(vulkan_clear
        PRIVATE sdl_painter spdlog::spdlog_header_only)
    sdlpainter_copy_runtime_dlls(vulkan_clear)

    add_executable(vulkan_triangles vulkan_triangles.cpp)
    target_link_libraries(vulkan_triangles
        PRIVATE sdl_painter spdlog::spdlog_header_only)
    sdlpainter_copy_runtime_dlls(vulkan_triangles)

    add_executable(vulkan_textured vulkan_textured.cpp)
    target_link_libraries(vulkan_textured
        PRIVATE sdl_painter spdlog::spdlog_header_only)
    sdlpainter_copy_runtime_dlls(vulkan_textured)

    add_executable(vulkan_demo vulkan_demo.cpp)
    target_link_libraries(vulkan_demo
        PRIVATE sdl_painter spdlog::spdlog_header_only)
    sdlpainter_copy_runtime_dlls(vulkan_demo)
endif()

# Ekran ustu FPS / kare istatistigi gostergesi demosu.
# benchmarks/screenshot.cpp'yi paylasir: dokuman gorseli uretmek icin ayni
# glReadPixels+PNG yardimcisi kullanilir, ikinci bir kopya yazilmaz.
add_executable(stats_overlay stats_overlay.cpp benchmarks/screenshot.cpp)
target_include_directories(stats_overlay PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(stats_overlay
    PRIVATE sdl_painter_app stb::stb spdlog::spdlog_header_only)
sdlpainter_copy_runtime_dlls(stats_overlay)

# Olcum hedefleri (draw call / batch verimliligi) — bkz. benchmarks/README.md
add_subdirectory(benchmarks)
