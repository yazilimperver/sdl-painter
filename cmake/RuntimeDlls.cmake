# Windows'ta çalışma zamanı DLL'lerinin executable yanına kopyalanmasına ilişkin betiklerdir.
#
# Kullanım: sdlpainter_copy_runtime_dlls(<target>)
#
# Yalnızca repo içi derleme kolaylığı içindir; kurulan/pakete giren tüketiciye
# export EDİLMEZ (bkz. doc/building.md).

# MinGW cross-compile'da Conan'dan gelen shared DLL'ler libgcc_s_seh-1.dll'e
# dinamik bağımlı kalabiliyor — exe'ler -static-libgcc ile derlense bile.
set(_mingw_runtime_dlls "")
if(MINGW AND CMAKE_CROSSCOMPILING)
    execute_process(
        COMMAND ${CMAKE_CXX_COMPILER} -print-file-name=libgcc_s_seh-1.dll
        OUTPUT_VARIABLE _libgcc_s_path
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    if(_libgcc_s_path AND EXISTS "${_libgcc_s_path}")
        list(APPEND _mingw_runtime_dlls "${_libgcc_s_path}")
        message(STATUS "MinGW runtime DLL: ${_libgcc_s_path}")
    else()
        message(WARNING "libgcc_s_seh-1.dll not found via -print-file-name "
                        "(got: '${_libgcc_s_path}'). Runtime may fail.")
    endif()
endif()

# Conan bağımlılık DLL'lerinin taranması.
#
# $<TARGET_RUNTIME_DLLS> IMPORTED_LOCATION'ı düzgün set edilmiş
# imported target'ları (örn. SDL3) otomatik yakalar; ama bazı Conan recipe'ları
# (harfbuzz, freetype, plutosvg, plutovg) MinGW'de imported target'a sadece
# .dll.a (IMPORTED_IMPLIB) atıp .dll yolunu atlıyor → o DLL'ler görünmüyor.
# Bu yüzden CMAKE_PREFIX_PATH'taki paket dizinleri ayrıca taranıyor.
#
# Tarama sonucu configure boyunca değişmediği için hedef başına DEĞİL, bir kez
# yapılır: 15+ örnek hedefiyle hem tekrar eden glob'u hem de configure
# çıktısındaki hedef başına STATUS satırını ortadan kaldırır.
set(_runtime_dlls "")
if(WIN32)
    foreach(_prefix IN LISTS CMAKE_PREFIX_PATH)
        # Conan generators klasörü: conanfile.py generate() bağımlılıkların
        # *.dll'lerini buraya stage'liyor (Windows hedefinde).
        # bin/, lib/: bazı toolchain'lerde paket root'u prefix path'e
        # eklendiğinde geçerli olabiliyor — düşmesi için tutuyoruz.
        file(GLOB _pkg_dlls
            "${_prefix}/*.dll"
            "${_prefix}/bin/*.dll"
            "${_prefix}/lib/*.dll"
        )
        if(_pkg_dlls)
            list(APPEND _runtime_dlls ${_pkg_dlls})
        endif()
    endforeach()

    if(_mingw_runtime_dlls)
        list(APPEND _runtime_dlls ${_mingw_runtime_dlls})
    endif()
    if(_runtime_dlls)
        list(REMOVE_DUPLICATES _runtime_dlls)
    endif()

    list(LENGTH _runtime_dlls _runtime_dlls_count)
    message(STATUS
        "Runtime DLL: CMAKE_PREFIX_PATH icinde ${_runtime_dlls_count} DLL bulundu")
endif()

# CACHE INTERNAL: alt dizinlerden (examples/, tests/) cagrilan fonksiyon
# icin kapsam suprizi olmasin.
set(SDLPAINTER_RUNTIME_DLLS "${_runtime_dlls}"
    CACHE INTERNAL "Executable yanina kopyalanacak calisma zamani DLL'leri")

unset(_mingw_runtime_dlls)
unset(_runtime_dlls)

# Bagimlilik DLL'lerini <target>'in ciktisinin yanina kopyalar.
# copy_if_different idempotent oldugu icin iki adim cakismaz.
function(sdlpainter_copy_runtime_dlls target)
    if(NOT WIN32)
        return()
    endif()

    add_custom_command(TARGET ${target} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_RUNTIME_DLLS:${target}>
            $<TARGET_FILE_DIR:${target}>
        COMMAND_EXPAND_LISTS
        COMMENT "Copying detected runtime DLLs for ${target}"
    )

    if(SDLPAINTER_RUNTIME_DLLS)
        add_custom_command(TARGET ${target} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                ${SDLPAINTER_RUNTIME_DLLS} $<TARGET_FILE_DIR:${target}>
            COMMENT "Copying Conan dependency DLLs for ${target}"
        )
    endif()
endfunction()
