# Shader gömme yardımcıları.
#
# Shader'ları kütüphane binary'sine gömer; böylece çalışma zamanında hiçbir
# dosya aranmaz. Bu, paketlenmiş (Conan/vcpkg/sistem kurulumu) kütüphanenin
# tüketici tarafında ek dosya kopyalamadan çalışabilmesi için zorunludur —
# aksi halde kaynak ağacı yolu binary'ye gömülür ve başka makinede çözülemez.
#
# Üretim configure aşamasında yapılır; kaynak shader değiştiğinde CMake
# kendini yeniden çalıştırsın diye dosyalar CMAKE_CONFIGURE_DEPENDS'e eklenir.

# "basic.vert" -> "kBasicVert",  "untextured.frag.spv" -> "kUntexturedFrag"
function(_sdlpainter_shader_symbol OUT_VAR FILE_NAME)
    string(REGEX REPLACE "\\.spv$" "" _stem "${FILE_NAME}")
    string(REPLACE "." ";" _parts "${_stem}")
    set(_symbol "k")
    foreach(_part IN LISTS _parts)
        string(SUBSTRING "${_part}" 0 1 _head)
        string(SUBSTRING "${_part}" 1 -1 _tail)
        string(TOUPPER "${_head}" _head)
        string(APPEND _symbol "${_head}${_tail}")
    endforeach()
    set(${OUT_VAR} "${_symbol}" PARENT_SCOPE)
endfunction()

# GLSL kaynaklarını ham string literal olarak gömer.
#
# sdlpainter_embed_glsl(OUTPUT <header> FILES <a.vert> <b.frag> ...)
function(sdlpainter_embed_glsl)
    cmake_parse_arguments(ARG "" "OUTPUT" "FILES" ${ARGN})
    if(NOT ARG_OUTPUT OR NOT ARG_FILES)
        message(FATAL_ERROR "sdlpainter_embed_glsl: OUTPUT ve FILES zorunlu")
    endif()

    set(_content "// GENERATED — elle duzenlemeyin.\n")
    string(APPEND _content "// Kaynak: cmake/EmbedShaders.cmake\n\n")
    string(APPEND _content "#pragma once\n\n")
    string(APPEND _content "namespace sdl_painter::detail {\n\n")

    foreach(_file IN LISTS ARG_FILES)
        if(NOT EXISTS "${_file}")
            message(FATAL_ERROR "sdlpainter_embed_glsl: bulunamadi: ${_file}")
        endif()
        get_filename_component(_name "${_file}" NAME)
        _sdlpainter_shader_symbol(_symbol "${_name}")
        file(READ "${_file}" _source)

        # Ham string sonlandiricisi kaynakta gecerse literal bozulur.
        if(_source MATCHES "\\)SDLP\"")
            message(FATAL_ERROR
                "sdlpainter_embed_glsl: ${_name} icinde )SDLP\" dizisi var; "
                "ham string sonlandiricisi cakisiyor")
        endif()

        # Bas kisimda newline birakilmaz: GLSL `#version` direktifi shader'da
        # yorum/bosluk disinda hicbir seyden sonra gelemez. Bosluk teknik olarak
        # serbest olsa da bazi suruculer katidir; riski hic almiyoruz.
        string(APPEND _content
            "/// @brief Gomulu GLSL kaynagi: ${_name}\n"
            "inline constexpr const char* ${_symbol} = R\"SDLP(${_source})SDLP\";\n\n")
        set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${_file}")
    endforeach()

    string(APPEND _content "}  // namespace sdl_painter::detail\n")

    # Icerik degismediyse dosyaya dokunma — gereksiz yeniden derlemeyi onler.
    file(GENERATE OUTPUT "${ARG_OUTPUT}" CONTENT "${_content}")
endfunction()

# Derlenmis SPIR-V modullerini uint32 dizisi olarak gomer.
#
# uint32 secimi kasitli: VkShaderModuleCreateInfo::pCode `const uint32_t*`
# bekler ve 4 bayt hizalama ister. Diziyi dogrudan uint32 olarak uretmek hem
# hizalamayi garanti eder hem de reinterpret_cast ihtiyacini kaldirir.
#
# sdlpainter_embed_spirv(OUTPUT <header> FILES <a.vert.spv> ...)
function(sdlpainter_embed_spirv)
    cmake_parse_arguments(ARG "" "OUTPUT" "FILES" ${ARGN})
    if(NOT ARG_OUTPUT OR NOT ARG_FILES)
        message(FATAL_ERROR "sdlpainter_embed_spirv: OUTPUT ve FILES zorunlu")
    endif()

    set(_content "// GENERATED — elle duzenlemeyin.\n")
    string(APPEND _content "// Kaynak: cmake/EmbedShaders.cmake\n\n")
    string(APPEND _content "#pragma once\n\n#include <cstdint>\n\n")
    string(APPEND _content "namespace sdl_painter::detail {\n\n")

    foreach(_file IN LISTS ARG_FILES)
        if(NOT EXISTS "${_file}")
            message(FATAL_ERROR
                "sdlpainter_embed_spirv: bulunamadi: ${_file}\n"
                "SPIR-V ciktilari repo'da tutulur; shader kaynagini "
                "degistirdiyseniz -DSDLPAINTER_REGENERATE_SHADERS=ON ile "
                "yeniden uretin.")
        endif()
        get_filename_component(_name "${_file}" NAME)
        _sdlpainter_shader_symbol(_symbol "${_name}")

        file(READ "${_file}" _hex HEX)
        string(LENGTH "${_hex}" _hex_length)
        math(EXPR _remainder "${_hex_length} % 8")
        if(NOT _remainder EQUAL 0)
            message(FATAL_ERROR
                "sdlpainter_embed_spirv: ${_name} boyutu 4'un kati degil "
                "(${_hex_length} hex hane) — gecerli bir SPIR-V degil")
        endif()
        math(EXPR _word_count "${_hex_length} / 8")

        string(APPEND _content
            "/// @brief Gomulu SPIR-V modulu: ${_name} (${_word_count} kelime)\n"
            "inline constexpr uint32_t ${_symbol}[] = {\n")

        set(_line "   ")
        set(_index 0)
        while(_index LESS _word_count)
            math(EXPR _offset "${_index} * 8")
            # Dosya bayt sirasi little-endian; uint32 kelimeye cevirmek icin
            # bayt ciftlerini ters sirala.
            string(SUBSTRING "${_hex}" "${_offset}" 2 _b0)
            math(EXPR _offset "${_offset} + 2")
            string(SUBSTRING "${_hex}" "${_offset}" 2 _b1)
            math(EXPR _offset "${_offset} + 2")
            string(SUBSTRING "${_hex}" "${_offset}" 2 _b2)
            math(EXPR _offset "${_offset} + 2")
            string(SUBSTRING "${_hex}" "${_offset}" 2 _b3)
            string(APPEND _line " 0x${_b3}${_b2}${_b1}${_b0}u,")

            math(EXPR _index "${_index} + 1")
            math(EXPR _column "${_index} % 6")
            if(_column EQUAL 0)
                string(APPEND _content "${_line}\n")
                set(_line "   ")
            endif()
        endwhile()
        if(NOT _line STREQUAL "   ")
            string(APPEND _content "${_line}\n")
        endif()

        string(APPEND _content "};\n\n")
        set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${_file}")
    endforeach()

    string(APPEND _content "}  // namespace sdl_painter::detail\n")
    file(GENERATE OUTPUT "${ARG_OUTPUT}" CONTENT "${_content}")
endfunction()
