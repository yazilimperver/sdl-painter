# Örnek (demo) hedefi tanımlamaya yönelik yardımcıdır.
#
# Kullanım:
#   sdlpainter_add_example(<ad> <kaynak> [<ek kaynaklar>...]
#                          [APP] [VULKAN] [TTF]
#                          [LIBS <hedef>...])
#
#   APP     → sdl_painter yerine sdl_painter_app'e bağlanır (uygulama çatısı).
#   VULKAN  → SDLPAINTER_WITH_VULKAN kapalıysa hedef hiç tanımlanmaz.
#   TTF     → SDL_ttf API'sini DOĞRUDAN çağıran örnekler için açık link.
#             sdl_painter'a PRIVATE bağlı olduğundan link-zamanı bağımlılığı
#             statik kütüphanede $<LINK_ONLY:...> ile zaten yayılır; yalnızca
#             SDL_ttf BAŞLIĞINI dahil eden hedeflerin include yoluna ihtiyacı
#             vardır.
#   LIBS    → ek link hedefleri (ör. stb::stb).
#   ASSETS  → `examples/assets/` dizinini çıktının yanına kopyalar; örnek
#             dosyaları `SDL_GetBasePath()` üzerinden bulur. Kütüphanenin
#             kendisi varlık dosyası taşımaz (ADR-009), bu yalnızca örnekler
#             içindir ve kurulmaz.
#
# Örnek kaynakları alt dizinlerde durur ama çıktı tek bir dizine yazılır:
# `${CMAKE_BINARY_DIR}/examples`. Bu bilinçlidir — dokümanlar, CI artifact
# glob'ları ve blog yazıları `build/<preset>/examples/<ad>` yolunu kullanıyor;
# dizin yeniden yapılandırması bu yolu kırmamalı.

function(sdlpainter_add_example name)
    cmake_parse_arguments(PARSE_ARGV 1 arg "APP;VULKAN;TTF;ASSETS" "" "LIBS")

    if(arg_VULKAN AND NOT SDLPAINTER_WITH_VULKAN)
        return()
    endif()

    if(NOT arg_UNPARSED_ARGUMENTS)
        message(FATAL_ERROR "sdlpainter_add_example(${name}): kaynak dosya verilmedi")
    endif()

    add_executable(${name} ${arg_UNPARSED_ARGUMENTS})

    if(arg_APP)
        set(_core sdl_painter_app)
    else()
        set(_core sdl_painter)
    endif()

    target_link_libraries(${name} PRIVATE ${_core} spdlog::spdlog_header_only)

    if(arg_LIBS)
        target_link_libraries(${name} PRIVATE ${arg_LIBS})
    endif()

    if(arg_TTF)
        if(TARGET SDL3_ttf::SDL3_ttf-static)
            target_link_libraries(${name} PRIVATE SDL3_ttf::SDL3_ttf-static)
        else()
            target_link_libraries(${name} PRIVATE SDL3_ttf::SDL3_ttf)
        endif()
    endif()

    # Kardeş başlıkların çıplak adla dahil edilebilmesi için (ör. stats_overlay
    # → "benchmarks/screenshot.h").
    target_include_directories(${name} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

    set_target_properties(${name} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/examples")

    if(arg_ASSETS)
        # Çıktı dizini tüm örnekler için ortak; kopyalama hedef başına
        # tanımlansa da aynı yere yazar. Çok yapılandırmalı üreticilerde
        # (MSVC) $<TARGET_FILE_DIR:...> doğru alt dizini verir.
        add_custom_command(TARGET ${name} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                    "${CMAKE_CURRENT_SOURCE_DIR}/assets"
                    "$<TARGET_FILE_DIR:${name}>/assets"
            COMMENT "Örnek varlıkları kopyalanıyor → ${name}")
    endif()

    sdlpainter_copy_runtime_dlls(${name})
endfunction()
