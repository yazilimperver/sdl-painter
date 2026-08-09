# GLSL kaynaklarının SHA-256 manifestini yazar.
#
#   cmake -DSDLPAINTER_SHADER_DIR=<dizin> -P cmake/WriteShaderManifest.cmake
#
# `regenerate_shaders` hedefi tarafından çağrılır. Manifest,
# cmake/CheckShaderFreshness.cmake'in karşılaştırdığı referanstır.

cmake_minimum_required(VERSION 3.20)

if(NOT DEFINED SDLPAINTER_SHADER_DIR)
    message(FATAL_ERROR "SDLPAINTER_SHADER_DIR tanimlanmali")
endif()

set(_shaders untextured.vert untextured.frag textured.vert textured.frag)
set(_content "# GENERATED — regenerate_shaders hedefi uretir, elle duzenlemeyin.\n")
string(APPEND _content
       "# GLSL kaynaklarinin SHA-256'lari; spirv/*.spv bunlardan uretildi.\n")

foreach(_shader IN LISTS _shaders)
    set(_src "${SDLPAINTER_SHADER_DIR}/${_shader}")
    if(NOT EXISTS "${_src}")
        message(FATAL_ERROR "GLSL kaynagi bulunamadi: ${_src}")
    endif()
    file(SHA256 "${_src}" _hash)
    string(APPEND _content "${_hash}  ${_shader}\n")
endforeach()

file(WRITE "${SDLPAINTER_SHADER_DIR}/spirv/sources.sha256" "${_content}")
message(STATUS "sources.sha256 yazildi (${SDLPAINTER_SHADER_DIR}/spirv)")
