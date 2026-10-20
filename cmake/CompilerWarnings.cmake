# Her target için compiler warning'lerini ayarlar.
# Kullanım: set_target_warnings(<target>)

# Uyarıları hata say (-Werror / /WX). Yalnızca üst projede varsayılan olarak
# açık: paket yöneticileri (Conan Center, vcpkg) ve add_subdirectory ile
# ekleyen tüketici, bizim CI'da denemediğimiz derleyici ve bağımlılık
# sürümleriyle derler; orada yeni bir uyarı derlemeyi kırmamalı.
option(SDLPAINTER_WARNINGS_AS_ERRORS "Treat compiler warnings as errors"
       ${PROJECT_IS_TOP_LEVEL})

function(set_target_warnings target)
    if(SDLPAINTER_WARNINGS_AS_ERRORS)
        if(MSVC)
            target_compile_options(${target} PRIVATE /WX)
        else()
            target_compile_options(${target} PRIVATE -Werror)
        endif()
    endif()

    if(MSVC)
        target_compile_options(${target} PRIVATE
            /W4
            /w14640  # thread-unsafe static member initialization
            /w14826  # conversion from 'type1' to 'type2' is sign-extended
            /w14905  # wide string literal cast to 'LPSTR'
            /w14906  # string literal cast to 'LPWSTR'
            /w14928  # illegal copy-initialization

            # C4251: "std::X uyesi, Y'nin istemcileri icin dll-interface'e
            # sahip olmali". Export edilen bir sinifin standart kutuphane
            # uyeleri (std::vector, std::unique_ptr, std::string ...) oldugunda
            # KACINILMAZ; std tipleri dllexport edilemez.
            #
            # Bu kurulumda zararsiz: DLL ile tuketici ayni derleyici ve ayni
            # runtime ile derleniyor (Conan/vcpkg paket kimligi bunu zaten
            # garanti eder — farkli ayar = farkli paket). Uyari, ancak DLL ve
            # exe farkli STL/runtime kullaniyorsa gercek bir riske isaret eder.
            #
            # Kalici cozum pimpl'e gecmek olurdu; bu, ABI stabilite beyani
            # (Faz 4.3) kapsaminda ayrica degerlendirilecek.
            /wd4251
        )
    else()
        target_compile_options(${target} PRIVATE
            -Wall
            -Wextra
            -Wshadow
            -Wnon-virtual-dtor
            -Wpedantic
            -Wold-style-cast
            -Wcast-align
            -Wunused
            -Woverloaded-virtual
            -Wconversion
            -Wsign-conversion
            -Wmisleading-indentation
            -Wnull-dereference
            -Wdouble-promotion
            -Wformat=2
        )
        # GCC-özel flag'ler. Clang (ve clang-tidy CI job'ı) bunları tanımıyor;
        # koşulsuz verilince her dosyada clang-diagnostic-unknown-warning-option
        # üretiyor. Yalnız GNU derleyicide ekle.
        if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
            target_compile_options(${target} PRIVATE
                -Wduplicated-cond
                -Wduplicated-branches
                -Wlogical-op
            )
        endif()
    endif()
endfunction()
