# Her target için compiler warning'lerini ayarlar.
# Kullanım: set_target_warnings(<target>)

function(set_target_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE
            /W4
            /WX
            /w14640  # thread-unsafe static member initialization
            /w14826  # conversion from 'type1' to 'type2' is sign-extended
            /w14905  # wide string literal cast to 'LPSTR'
            /w14906  # string literal cast to 'LPWSTR'
            /w14928  # illegal copy-initialization
        )
    else()
        target_compile_options(${target} PRIVATE
            -Wall
            -Wextra
            -Werror
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
