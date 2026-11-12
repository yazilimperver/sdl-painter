# Olcum hedefleri. Ornekler gibi derlenir ama gorsel bir demo degildir:
# amaclari cizim desenlerinin draw call maliyetini sayiyla raporlamaktir.
#
# Kutuphaneye sayac EKLENMEZ; olcum CountingRenderer sarmalayicisiyla
# disaridan yapilir, bu yuzden yalnizca public API'ye baglidir.
#
# stb: yalnizca ekran goruntusu yazmak icin (stb_image_write). Kutuphanenin
# kendi stb kullanimindan bagimsizdir; orada PRIVATE oldugu icin burada
# ayrica link edilir.

add_executable(batching_benchmark
    batching_benchmark.cpp
    screenshot.cpp)
target_link_libraries(batching_benchmark
    PRIVATE sdl_painter stb::stb spdlog::spdlog_header_only)
sdlpainter_copy_runtime_dlls(batching_benchmark)

# Ayni sahneyi SDL_Renderer ile karsilastirir. SDL_Renderer SDL3'un parcasi;
# yeni bagimlilik yok. Kendi global operator new'unu tanimlar (tahsis sayimi).
add_executable(renderer_comparison
    renderer_comparison.cpp
    screenshot.cpp)
target_link_libraries(renderer_comparison
    PRIVATE sdl_painter stb::stb spdlog::spdlog_header_only)
sdlpainter_copy_runtime_dlls(renderer_comparison)
