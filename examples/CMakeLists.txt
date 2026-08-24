# Örnek hedefleri. Tanım yardımcısı: cmake/AddExample.cmake
#
# Kaynaklar kategoriye göre alt dizinlerde durur, çıktı tek dizine yazılır
# (`build/<preset>/examples/`) — gerekçesi yardımcı modülün başında.

include(AddExample)

# --- Temel çizim: SDL boilerplate'i açıkta, uygulama çatısı kullanılmaz ---
sdlpainter_add_example(minimal      basics/minimal.cpp)
sdlpainter_add_example(hello_window basics/hello_window.cpp)
sdlpainter_add_example(primitives   basics/primitives.cpp)
sdlpainter_add_example(transforms   basics/transforms.cpp)
sdlpainter_add_example(clipping     basics/clipping.cpp)
sdlpainter_add_example(input        basics/input.cpp     APP)
sdlpainter_add_example(viewports    basics/viewports.cpp APP)

# --- Görseller ve metin ---
sdlpainter_add_example(images graphics/images.cpp)
sdlpainter_add_example(text   graphics/text.cpp)
sdlpainter_add_example(plasma graphics/plasma.cpp APP)
sdlpainter_add_example(particles        graphics/particles.cpp        APP)
sdlpainter_add_example(sprite_animation graphics/sprite_animation.cpp APP ASSETS)
sdlpainter_add_example(morph            graphics/morph.cpp            APP)
sdlpainter_add_example(charts           graphics/charts.cpp           APP)
sdlpainter_add_example(strokes          graphics/strokes.cpp          APP)
sdlpainter_add_example(gradients        graphics/gradients.cpp        APP)
sdlpainter_add_example(blend_modes      graphics/blend_modes.cpp      APP)
sdlpainter_add_example(pixel_art        graphics/pixel_art.cpp        APP ASSETS)
sdlpainter_add_example(mesh_warp        graphics/mesh_warp.cpp        APP ASSETS)
sdlpainter_add_example(physics_rope     graphics/physics_rope.cpp     APP)

# --- Uygulama çatısı (sdl_painter_app, bkz. ADR-008) ---
sdlpainter_add_example(app_basics app/app_basics.cpp APP)
sdlpainter_add_example(game_loop  app/game_loop.cpp  APP)
sdlpainter_add_example(paint      app/paint.cpp      APP)

# stats_overlay, benchmarks/screenshot.cpp'yi paylaşır: doküman görseli üretmek
# için aynı glReadPixels+PNG yardımcısı kullanılır, ikinci bir kopya yazılmaz.
sdlpainter_add_example(stats_overlay
    app/stats_overlay.cpp benchmarks/screenshot.cpp
    APP LIBS stb::stb)

# --- Tam uygulamalar ---
# tictactoe ve breakout metin çizer; sdl_painter::Font public başlık olduğu
# için SDL_ttf'e AÇIK link gerekmez (bkz. AddExample.cmake TTF seçeneği).
sdlpainter_add_example(tictactoe     games/tictactoe.cpp     APP)
sdlpainter_add_example(breakout      games/breakout.cpp      APP)
sdlpainter_add_example(camera_scroll games/camera_scroll.cpp APP)
sdlpainter_add_example(tilemap       games/tilemap.cpp       APP)

# --- Vulkan backend doğrulamaları ---
sdlpainter_add_example(vulkan_clear     vulkan/vulkan_clear.cpp     VULKAN)
sdlpainter_add_example(vulkan_triangles vulkan/vulkan_triangles.cpp VULKAN)
sdlpainter_add_example(vulkan_textured  vulkan/vulkan_textured.cpp  VULKAN)
sdlpainter_add_example(vulkan_demo      vulkan/vulkan_demo.cpp      VULKAN)

# Tek istisna: vulkan_text.cpp SDL_ttf'i DOĞRUDAN kullanıyor (TTF_Init/TTF_Quit).
sdlpainter_add_example(vulkan_text vulkan/vulkan_text.cpp VULKAN TTF)

# --- README tanıtım GIF'ini üreten koreografili sahne ---
sdlpainter_add_example(hero hero.cpp)

# Ölçüm hedefleri (draw call / batch verimliliği) — bkz. benchmarks/README.md
add_subdirectory(benchmarks)
