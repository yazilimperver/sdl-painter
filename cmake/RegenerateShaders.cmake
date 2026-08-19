# Vulkan SPIR-V çıktılarının glslc ile yeniden üretimi — GELİŞTİRİCİ ARACI.
#
# Derlenmiş .spv dosyaları repo'da tutulur (bkz. ADR-009); normal derlemede
# glslc HİÇ aranmaz. GLSL kaynağını değiştirdiysen:
#
#   cmake -S . -B build/x -DSDLPAINTER_REGENERATE_SHADERS=ON ...
#   cmake --build build/x --target regenerate_shaders
#
# ve üretilen .spv dosyaları ile sources.sha256'yı commit'le.
#
# Girdi: SDLPAINTER_VULKAN_SHADER_SPIRV_DIR (kök CMakeLists.txt'te tanımlı).
# NOT: Shader'lar içerisindeki yorumlar da değiştiğinde tekrar oluşturmak gerekir

option(SDLPAINTER_REGENERATE_SHADERS
       "Vulkan SPIR-V ciktilarini glslc ile yeniden uret (gelistirici araci)" OFF)

if(NOT SDLPAINTER_REGENERATE_SHADERS)
    return()
endif()

find_program(GLSLC_EXECUTABLE glslc
    HINTS "$ENV{VULKAN_SDK}/Bin" "$ENV{VULKAN_SDK}/bin"
    DOC "glslc SPIR-V compiler")
if(NOT GLSLC_EXECUTABLE)
    message(FATAL_ERROR
        "SDLPAINTER_REGENERATE_SHADERS=ON ancak glslc bulunamadi. "
        "Vulkan SDK kurun veya VULKAN_SDK ortam degiskenini ayarlayin.")
endif()

set(_regenerated_spvs "")
foreach(_shader untextured.vert untextured.frag textured.vert textured.frag)
    set(_src "${CMAKE_CURRENT_SOURCE_DIR}/src/vulkan/shaders/${_shader}")
    set(_spv "${SDLPAINTER_VULKAN_SHADER_SPIRV_DIR}/${_shader}.spv")
    add_custom_command(
        OUTPUT  "${_spv}"
        COMMAND "${GLSLC_EXECUTABLE}" "${_src}" -o "${_spv}"
        DEPENDS "${_src}"
        COMMENT "glslc: ${_shader} -> ${_shader}.spv (kaynak agacina yaziliyor)")
    list(APPEND _regenerated_spvs "${_spv}")
endforeach()

# Manifest, CI'daki guncellik kontrolunun dayanagi (bkz.
# cmake/CheckShaderFreshness.cmake). SPIR-V ile birlikte guncellenmeli,
# yoksa kontrol anlamsizlasir.
add_custom_target(regenerate_shaders
    DEPENDS ${_regenerated_spvs}
    COMMAND ${CMAKE_COMMAND}
            -DSDLPAINTER_SHADER_DIR=${CMAKE_CURRENT_SOURCE_DIR}/src/vulkan/shaders
            -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/WriteShaderManifest.cmake
    COMMENT "SPIR-V ciktilari + sources.sha256 yeniden uretildi — commit'leyin")
