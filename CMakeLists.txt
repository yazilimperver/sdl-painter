cmake_minimum_required(VERSION 3.20)
project(SDLPainter VERSION 0.1.0 LANGUAGES CXX)

include(cmake/InsourceGuard.cmake)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# MinGW cross-compile: libgcc_s_seh-1.dll ve libstdc++-6.dll çalışma zamanı
# bağımlılıklarını exe'ye gömerek dağıtımı tek-dosyaya indir.
if(MINGW)
    add_link_options(-static-libgcc -static-libstdc++)
endif()

# MinGW cross-compile'da Conan'dan gelen shared DLL'ler libgcc_s_seh-1.dll'e
# dinamik bağımlı kalabiliyor.
set(SDLPAINTER_MINGW_RUNTIME_DLLS "" CACHE INTERNAL "MinGW runtime DLLs to ship")
if(MINGW AND CMAKE_CROSSCOMPILING)
    execute_process(
        COMMAND ${CMAKE_CXX_COMPILER} -print-file-name=libgcc_s_seh-1.dll
        OUTPUT_VARIABLE _libgcc_s_path
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    if(_libgcc_s_path AND EXISTS "${_libgcc_s_path}")
        list(APPEND SDLPAINTER_MINGW_RUNTIME_DLLS "${_libgcc_s_path}")
        message(STATUS "MinGW runtime DLL: ${_libgcc_s_path}")
    else()
        message(WARNING "libgcc_s_seh-1.dll not found via -print-file-name "
                        "(got: '${_libgcc_s_path}'). Runtime may fail.")
    endif()
endif()

# Options (Conan tarafından da set edilebilir)
option(SDLPAINTER_WITH_VULKAN "Enable Vulkan backend" OFF)
option(SDLPAINTER_BUILD_EXAMPLES "Build examples" ON)
option(SDLPAINTER_BUILD_TESTS "Build tests" ON)

# Conan ile gelen paketleri bul
find_package(SDL3 REQUIRED)
find_package(glad REQUIRED)
find_package(stb REQUIRED)
find_package(spdlog REQUIRED)
find_package(OpenGL REQUIRED)

# Compiler warnings + sanitizer seçenekleri
include(cmake/CompilerWarnings.cmake)
include(cmake/CompilerOptions.cmake)

# Ana kütüphane kaynak dosyaları
set(SDLPAINTER_SOURCES
    src/painter.cpp
    src/renderer.cpp
    src/color.cpp
    src/pen.cpp
    src/brush.cpp
    src/transform.cpp
    src/image.cpp
    src/geometry.cpp
    src/tessellator.cpp
    src/render_batcher.cpp
    src/font.cpp
    src/opengl/opengl_renderer.cpp
    src/opengl/shader_program.cpp
)

if(SDLPAINTER_WITH_VULKAN)
    find_package(VulkanLoader REQUIRED)
    find_package(VulkanHeaders REQUIRED)
    list(APPEND SDLPAINTER_SOURCES
        src/vulkan/vk_check.cpp
        src/vulkan/vk_memory.cpp
        src/vulkan/vk_context.cpp
        src/vulkan/vk_swapchain.cpp
        src/vulkan/vk_frame_sync.cpp
        src/vulkan/vulkan_renderer.cpp
        src/vulkan/vulkan_pipeline.cpp
        src/vulkan/vulkan_buffer.cpp
        src/vulkan/vulkan_texture.cpp
        src/vulkan/vulkan_textured_pipeline.cpp
    )

    # --- Vulkan shader derleme (offline, glslc) ---
    find_program(GLSLC_EXECUTABLE glslc
        HINTS
            "$ENV{VULKAN_SDK}/Bin"
            "$ENV{VULKAN_SDK}/bin"
        DOC "glslc SPIR-V compiler")

    if(NOT GLSLC_EXECUTABLE)
        message(WARNING "glslc not found — Vulkan shaders will not be compiled. "
                        "Install the Vulkan SDK and ensure glslc is on PATH or "
                        "set VULKAN_SDK environment variable.")
    else()
        set(VULKAN_SHADER_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src/vulkan/shaders)
        set(VULKAN_SHADER_BINARY_DIR ${CMAKE_CURRENT_BINARY_DIR}/vulkan_shaders CACHE STRING
            "Directory where compiled Vulkan SPIR-V shaders are stored")
        file(MAKE_DIRECTORY ${VULKAN_SHADER_BINARY_DIR})

        # compile_vulkan_shader(TARGET shader.vert) → TARGET_FILE_DIR/vulkan_shaders/shader.vert.spv
        function(compile_vulkan_shader SHADER_FILE)
            set(SRC "${VULKAN_SHADER_SOURCE_DIR}/${SHADER_FILE}")
            set(SPV "${VULKAN_SHADER_BINARY_DIR}/${SHADER_FILE}.spv")
            add_custom_command(
                OUTPUT  "${SPV}"
                COMMAND ${GLSLC_EXECUTABLE} "${SRC}" -o "${SPV}"
                DEPENDS "${SRC}"
                COMMENT "Compiling Vulkan shader: ${SHADER_FILE}"
            )
            set_source_files_properties("${SPV}" PROPERTIES GENERATED TRUE)
            # SPV dosyasını global listene ekle.
            set(VULKAN_SHADER_SPVS ${VULKAN_SHADER_SPVS} "${SPV}" PARENT_SCOPE)
        endfunction()

        compile_vulkan_shader(untextured.vert)
        compile_vulkan_shader(untextured.frag)
        compile_vulkan_shader(textured.vert)
        compile_vulkan_shader(textured.frag)

        add_custom_target(vulkan_shaders ALL DEPENDS ${VULKAN_SHADER_SPVS})
    endif()
endif()

find_package(SDL3_ttf REQUIRED)

add_library(sdl_painter ${SDLPAINTER_SOURCES})

if(SDLPAINTER_WITH_VULKAN AND TARGET vulkan_shaders)
    add_dependencies(sdl_painter vulkan_shaders)
endif()

target_include_directories(sdl_painter
    PUBLIC  include
    PRIVATE src
)

target_link_libraries(sdl_painter
    PUBLIC  SDL3::SDL3
    PUBLIC  OpenGL::GL
    PRIVATE glad::glad
    PRIVATE stb::stb
    PRIVATE spdlog::spdlog_header_only
)

# OpenGL shader kaynak dizinini makro olarak ver (Vulkan ile tutarlı pattern).
# Development build'inde shader'ları doğrudan source ağacından okumayı sağlar;
# install build'inde fallback olarak SDL_GetBasePath() + "shaders/" devreye girer.
#
# Windows hedeflerinde tanımlanmıyor: cross-compile'da Linux source path'i
# (örn. /workspace/src/opengl/shaders) Windows exe'sine gömülüp runtime'da
# çözülemiyor. SDL_GetBasePath() fallback'i devreye girsin —
# sdlpainter_copy_shaders() exe yanına shaders/ dizinini zaten kopyalıyor.
if(NOT WIN32)
    target_compile_definitions(sdl_painter PRIVATE
        SDLPAINTER_OPENGL_SHADER_DIR="${CMAKE_CURRENT_SOURCE_DIR}/src/opengl/shaders")
endif()

if(SDLPAINTER_WITH_VULKAN)
    target_compile_definitions(sdl_painter PUBLIC SDLPAINTER_HAS_VULKAN)
    target_link_libraries(sdl_painter
        PRIVATE Vulkan::Loader
        PRIVATE vulkan-headers::vulkan-headers
    )
    # Shader binary dizinini kütüphaneye makro olarak ver (add_library'den sonra).
    # Windows'ta tanımlama: OpenGL ile aynı cross-compile path-gömme tuzağı.
    # SDL_GetBasePath() + "vulkan_shaders" fallback'i devreye girsin.
    if(DEFINED VULKAN_SHADER_BINARY_DIR AND NOT WIN32)
        target_compile_definitions(sdl_painter PRIVATE
            SDLPAINTER_VULKAN_SHADER_DIR="${VULKAN_SHADER_BINARY_DIR}")
    endif()
endif()

if(TARGET SDL3_ttf::SDL3_ttf-static)
    target_link_libraries(sdl_painter PRIVATE SDL3_ttf::SDL3_ttf-static)
else()
    target_link_libraries(sdl_painter PRIVATE SDL3_ttf::SDL3_ttf)
endif()

set_target_warnings(sdl_painter)
set_target_sanitizers(sdl_painter)

# Helper: Vulkan SPIR-V dosyalarını executable'ın yanındaki vulkan_shaders/ dizinine kopyalar.
#
# MSVC: OpenGL shader'larıyla aynı race condition riski var (5 Vulkan exe aynı dizine
# paralel yazıyor). Tek merkez target ile çözülür.
if(SDLPAINTER_WITH_VULKAN AND TARGET vulkan_shaders)
    if(MSVC)
        add_custom_target(copy_vulkan_shaders ALL
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${VULKAN_SHADER_BINARY_DIR}"
                "${CMAKE_CURRENT_BINARY_DIR}/examples/$<CONFIG>/vulkan_shaders"
            COMMENT "Copying Vulkan shaders"
        )
        add_dependencies(copy_vulkan_shaders vulkan_shaders)
    endif()
endif()

function(sdlpainter_copy_vulkan_shaders target)
    if(SDLPAINTER_WITH_VULKAN AND TARGET vulkan_shaders)
        if(MSVC)
            if(TARGET copy_vulkan_shaders)
                add_dependencies(${target} copy_vulkan_shaders)
            endif()
        else()
            # Ninja/Make: binary dir sabit, $<TARGET_FILE_DIR> WSL mount sorunu yaratır
            add_dependencies(${target} vulkan_shaders)
            add_custom_command(TARGET ${target} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_directory
                    "${VULKAN_SHADER_BINARY_DIR}"
                    "${CMAKE_CURRENT_BINARY_DIR}/vulkan_shaders"
                COMMENT "Copying Vulkan shaders for ${target}"
            )
        endif()
    endif()
endfunction()

# Helper: OpenGL shader dosyalarını her executable'ın binary dizinine kopyalar.
#
# MSVC: Paralel build'de birden fazla exe aynı dizine aynı anda yazmayı
# denediğinde race condition çıkıyor. Tek bir merkez target ile çözülür —
# CMake jobserver bu target'ın yalnızca bir kez ve sıralı çalışmasını garantiler.
if(MSVC)
    add_custom_target(copy_opengl_shaders ALL
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_SOURCE_DIR}/src/opengl/shaders"
            "${CMAKE_CURRENT_BINARY_DIR}/examples/$<CONFIG>/shaders"
        COMMENT "Copying OpenGL shaders"
    )
endif()

function(sdlpainter_copy_shaders target)
    if(MSVC)
        add_dependencies(${target} copy_opengl_shaders)
    else()
        # Ninja/Make: configure zamanında kopyala — WSL /mnt/ mount sorununu önler
        set(SHADER_SRC_DIR "${CMAKE_SOURCE_DIR}/src/opengl/shaders")
        file(COPY "${SHADER_SRC_DIR}" DESTINATION "${CMAKE_CURRENT_BINARY_DIR}")
    endif()
endfunction()

# Helper: Windows hedeflerinde Conan'dan gelen shared DLL'leri (SDL3, SDL3_ttf,
# harfbuzz, freetype, plutosvg, plutovg, zlib, ...) executable'ın yanına
# kopyalar. İki adımda çalışır:
#   1) $<TARGET_RUNTIME_DLLS> (CMake 3.21+): IMPORTED_LOCATION'ı düzgün set
#      edilmiş imported target'ları (örn. SDL3) otomatik yakalar.
#   2) Fallback: bazı Conan recipe'ları (harfbuzz, freetype, plutosvg, plutovg)
#      MinGW'de imported target'a sadece .dll.a (IMPORTED_IMPLIB) atıyor,
#      .dll yolunu (IMPORTED_LOCATION) atlıyorlar → TARGET_RUNTIME_DLLS bunları
#      göremiyor. CMAKE_PREFIX_PATH'taki tüm Conan paket bin/ dizinlerini
#      tarayıp eksik DLL'leri tamamlıyoruz.
#
# copy_if_different idempotent olduğu için iki adım çakışmaz.
function(sdlpainter_copy_runtime_dlls target)
    if(NOT WIN32)
        return()
    endif()

    if(CMAKE_VERSION VERSION_GREATER_EQUAL 3.21)
        add_custom_command(TARGET ${target} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_RUNTIME_DLLS:${target}>
                $<TARGET_FILE_DIR:${target}>
            COMMAND_EXPAND_LISTS
            COMMENT "Copying detected runtime DLLs for ${target}"
        )
    endif()

    set(_extra_dlls "")
    foreach(prefix IN LISTS CMAKE_PREFIX_PATH)
        # Conan generators klasörü: conanfile.py generate() bağımlılıkların
        # *.dll'lerini buraya stage'liyor (Windows hedefinde).
        # bin/, lib/: bazı toolchain'lerde paket root'u prefix path'e
        # eklendiğinde geçerli olabiliyor — düşmesi için tutuyoruz.
        file(GLOB _pkg_dlls
            "${prefix}/*.dll"
            "${prefix}/bin/*.dll"
            "${prefix}/lib/*.dll"
        )
        if(_pkg_dlls)
            list(APPEND _extra_dlls ${_pkg_dlls})
        endif()
    endforeach()

    # MinGW cross-compile: exe'ler -static-libgcc ile derleniyor, ama
    # Conan'dan gelen shared DLL'ler (SDL3.dll vb.) hâlâ libgcc_s_seh-1.dll'e
    # dinamik bağımlı. Üst kısımda configure-time tespit edilen DLL'i ekle.
    if(SDLPAINTER_MINGW_RUNTIME_DLLS)
        list(APPEND _extra_dlls ${SDLPAINTER_MINGW_RUNTIME_DLLS})
    endif()
    list(REMOVE_DUPLICATES _extra_dlls)
    list(LENGTH _extra_dlls _extra_dlls_count)
    message(STATUS "sdlpainter_copy_runtime_dlls(${target}): ${_extra_dlls_count} DLL found in CMAKE_PREFIX_PATH")
    if(_extra_dlls)
        add_custom_command(TARGET ${target} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                ${_extra_dlls} $<TARGET_FILE_DIR:${target}>
            COMMENT "Copying ${_extra_dlls_count} Conan dependency DLLs for ${target}"
        )
    endif()
endfunction()

# Examples
if(SDLPAINTER_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

# Tests
if(SDLPAINTER_BUILD_TESTS)
    enable_testing()
    find_package(GTest REQUIRED)
    add_subdirectory(tests)
endif()

# Docs — Doxygen varsa sdlpainter_docs target'ı oluşturur, yoksa sessizce atlar.
include(cmake/Docs.cmake)
