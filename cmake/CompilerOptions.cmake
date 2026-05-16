# Sanitizer ve güvenlik derleyici seçenekleri.
# Kullanım: set_target_sanitizers(<target>)
#
# Etkinleştirmek için: cmake -B build -DENABLE_SANITIZERS=ON
# Yalnızca GCC/Clang + Linux/macOS'ta etkin; MSVC ve Windows'ta atlanır.

option(ENABLE_SANITIZERS "Enable AddressSanitizer + UBSan (GCC/Clang only)" OFF)

function(set_target_sanitizers target)
    if(NOT ENABLE_SANITIZERS)
        return()
    endif()

    if(MSVC OR WIN32)
        message(STATUS "Sanitizers: MSVC/Windows'ta desteklenmez, atlanıyor.")
        return()
    endif()

    # PUBLIC: bayraklar tüketicilere (tests/examples) de yayılmalı.
    # ${target} static lib; PRIVATE kalırsa enstrümante objeler enstrümante
    # olmayan exe'lere linklenir → "undefined reference to __ubsan_*/__asan_*".
    target_compile_options(${target} PUBLIC
        -fsanitize=address
        -fsanitize=undefined
        -fno-omit-frame-pointer
    )
    target_link_options(${target} PUBLIC
        -fsanitize=address
        -fsanitize=undefined
    )
    message(STATUS "Sanitizers: ${target} için ASan + UBSan etkin.")
endfunction()
