cmake_minimum_required(VERSION 3.20)
project(SDLPainter VERSION 0.1.0 LANGUAGES CXX)

include(cmake/InsourceGuard.cmake)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

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

if(SDLPAINTER_WITH_VULKAN)
    target_compile_definitions(sdl_painter PUBLIC SDLPAINTER_HAS_VULKAN)
    target_link_libraries(sdl_painter
        PRIVATE Vulkan::Loader
        PRIVATE vulkan-headers::vulkan-headers
    )
    # Shader binary dizinini kütüphaneye makro olarak ver (add_library'den sonra).
    if(DEFINED VULKAN_SHADER_BINARY_DIR)
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
