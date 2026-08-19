# Kod kapsama (code coverage) enstrümantasyonu.
# Kullanım: set_target_coverage(<target>)
#
# Etkinleştirmek için: cmake --preset linux-debug-coverage
#                 veya cmake -B build -DENABLE_COVERAGE=ON
#
# Yalnızca GCC/Clang'da etkin; MSVC'de sessizce atlanır. Rapor üretimi CI'da
# `gcovr` ile yapılır (bkz. .github/workflows/ci.yml `coverage` job'ı).

option(ENABLE_COVERAGE "Enable gcov/llvm-cov instrumentation (GCC/Clang)" OFF)

function(set_target_coverage target)
    if(NOT ENABLE_COVERAGE)
        return()
    endif()

    if(MSVC)
        message(STATUS "Coverage: MSVC'de desteklenmez, atlanıyor.")
        return()
    endif()

    # PUBLIC: sanitizer bayraklarındaki gerekçenin aynısı — ${target} static
    # lib olduğundan enstrümante objeler enstrümante olmayan exe'lere
    # linklenirse `__gcov_*` sembolleri çözülemez.
    target_compile_options(${target} PUBLIC --coverage -O0 -g)
    target_link_options(${target} PUBLIC --coverage)
    message(STATUS "Coverage: ${target} için gcov enstrümantasyonu etkin.")
endfunction()
