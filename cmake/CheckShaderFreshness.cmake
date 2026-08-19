# SPIR-V güncellik kontrolü — bağımsız script.
#
#   cmake -P cmake/CheckShaderFreshness.cmake
#
# Neyi çözer: derlenmiş `.spv` dosyaları repo'da tutuluyor (bkz. ADR-009).
# Biri GLSL kaynağını değiştirip `regenerate_shaders` hedefini çalıştırmayı
# unutursa, Vulkan backend'i sessizce ESKİ shader'ı kullanmaya devam eder —
# hata vermez, sadece yanlış çizer. Bu script o durumu yakalar.
#
# Nasıl: üretim anındaki GLSL kaynaklarının SHA-256'ları
# `src/vulkan/shaders/spirv/sources.sha256` içinde tutulur. Burada yeniden
# hesaplanıp karşılaştırılır. glslc veya Vulkan SDK GEREKTİRMEZ, bu yüzden
# CI'da ek araç kurulumu olmadan çalışır.

cmake_minimum_required(VERSION 3.21)

get_filename_component(_repo_root "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
set(_shader_dir "${_repo_root}/src/vulkan/shaders")
set(_manifest "${_shader_dir}/spirv/sources.sha256")

set(_shaders untextured.vert untextured.frag textured.vert textured.frag)

if(NOT EXISTS "${_manifest}")
    message(FATAL_ERROR
        "Manifest bulunamadi: ${_manifest}\n"
        "SPIR-V ciktilarini uretip manifest'i olusturun:\n"
        "  cmake -S . -B build/x -DSDLPAINTER_REGENERATE_SHADERS=ON ...\n"
        "  cmake --build build/x --target regenerate_shaders")
endif()

file(STRINGS "${_manifest}" _recorded)

set(_problems "")
foreach(_shader IN LISTS _shaders)
    set(_src "${_shader_dir}/${_shader}")
    set(_spv "${_shader_dir}/spirv/${_shader}.spv")

    if(NOT EXISTS "${_src}")
        list(APPEND _problems "GLSL kaynagi yok: ${_shader}")
        continue()
    endif()
    if(NOT EXISTS "${_spv}")
        list(APPEND _problems "SPIR-V ciktisi yok: ${_shader}.spv")
        continue()
    endif()

    file(SHA256 "${_src}" _actual)

    set(_expected "")
    foreach(_line IN LISTS _recorded)
        if(_line MATCHES "^([0-9a-f]+)  ${_shader}$")
            set(_expected "${CMAKE_MATCH_1}")
        endif()
    endforeach()

    if(_expected STREQUAL "")
        list(APPEND _problems "manifest'te kayit yok: ${_shader}")
    elseif(NOT _actual STREQUAL _expected)
        list(APPEND _problems
             "${_shader} degismis ama .spv yeniden uretilmemis")
    endif()
endforeach()

if(_problems)
    string(REPLACE ";" "\n  - " _problem_text "${_problems}")
    message(FATAL_ERROR
        "SPIR-V ciktilari GLSL kaynagiyla uyusmuyor:\n  - ${_problem_text}\n\n"
        "Duzeltmek icin:\n"
        "  cmake -S . -B build/x -DSDLPAINTER_REGENERATE_SHADERS=ON <toolchain>\n"
        "  cmake --build build/x --target regenerate_shaders\n"
        "ve uretilen .spv dosyalari ile sources.sha256'yi commit'leyin.")
endif()

message(STATUS "SPIR-V ciktilari guncel (${_shaders}).")
