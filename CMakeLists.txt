cmake_minimum_required(VERSION 3.21)

list(INSERT CMAKE_MODULE_PATH 0 "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

include(ProjectVersion)

project(SDLPainter VERSION ${SDLPAINTER_VERSION} LANGUAGES CXX)

include(InsourceGuard)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Yalnizca ust proje iken: alt proje olarak eklendigimizde tuketicinin
# ayarini ezmeyelim.
if(PROJECT_IS_TOP_LEVEL)
    set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
endif()

# Options (Conan tarafından da set edilebilir)
option(SDLPAINTER_WITH_VULKAN "Enable Vulkan backend" OFF)
# Ornek ve testler yalnizca SDLPainter ust proje iken varsayilan olarak acik.
# add_subdirectory()/FetchContent ile ekleyen tuketici, test bagimliligini derlemek zorunda kalmasin.
option(SDLPAINTER_BUILD_EXAMPLES "Build examples" ${PROJECT_IS_TOP_LEVEL})
option(SDLPAINTER_BUILD_TESTS "Build tests" ${PROJECT_IS_TOP_LEVEL})

# Conan ile gelen paketleri bul
find_package(SDL3 REQUIRED)
find_package(SDL3_ttf REQUIRED)
find_package(glad REQUIRED)
find_package(stb REQUIRED)
find_package(spdlog REQUIRED)
find_package(glm REQUIRED)
find_package(OpenGL REQUIRED)

# Compiler warnings + sanitizer seçenekleri
include(CompilerWarnings)
include(CompilerOptions)

# Windows'ta bagimlilik DLL'lerini exe yaninda toplayan yardimci
include(RuntimeDlls)

# Kurulum dizinleri (CMAKE_INSTALL_INCLUDEDIR/LIBDIR/BINDIR) — hedef arayuzleri
# bunlari kullandigi icin hedeflerden ONCE include edilmeli.
include(GNUInstallDirs)

# --- Shader gomme ---
# Shader'lar binary'ye gomulur; calisma zamaninda dosya aranmaz. Paketlenmis
# kutuphanenin (Conan/vcpkg/sistem kurulumu) tuketici tarafinda ek dosya
# kopyalamadan calisabilmesi icin zorunlu.
include(EmbedShaders)

set(SDLPAINTER_GENERATED_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated/sdl_painter")
set(SDLPAINTER_VULKAN_SHADER_SPIRV_DIR
    "${CMAKE_CURRENT_SOURCE_DIR}/src/vulkan/shaders/spirv")

sdlpainter_embed_glsl(
    OUTPUT "${SDLPAINTER_GENERATED_DIR}/embedded_gl_shaders.h"
    FILES
        "${CMAKE_CURRENT_SOURCE_DIR}/src/opengl/shaders/basic.vert"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/opengl/shaders/basic.frag"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/opengl/shaders/textured.vert"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/opengl/shaders/textured.frag")

# Ana kütüphane kaynak dosyaları
set(SDLPAINTER_SOURCES
    src/painter.cpp
    src/renderer.cpp
    src/image.cpp
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

    # --- SPIR-V gomme ---
    # Derlenmis .spv dosyalari repo'da tutulur (src/vulkan/shaders/spirv/) ve
    # binary'ye gomulur. Boylece kutuphaneyi derlemek icin Vulkan SDK / glslc
    # GEREKMEZ ve calisma zamaninda hicbir .spv dosyasi aranmaz.
    sdlpainter_embed_spirv(
        OUTPUT "${SDLPAINTER_GENERATED_DIR}/embedded_vk_shaders.h"
        FILES
            "${SDLPAINTER_VULKAN_SHADER_SPIRV_DIR}/untextured.vert.spv"
            "${SDLPAINTER_VULKAN_SHADER_SPIRV_DIR}/untextured.frag.spv"
            "${SDLPAINTER_VULKAN_SHADER_SPIRV_DIR}/textured.vert.spv"
            "${SDLPAINTER_VULKAN_SHADER_SPIRV_DIR}/textured.frag.spv")
endif()

# Shader kaynagini degistiren gelistirici icin `regenerate_shaders` hedefi
# (-DSDLPAINTER_REGENERATE_SHADERS=ON). Normal derlemede glslc HIC aranmaz.
include(RegenerateShaders)

add_library(sdl_painter ${SDLPAINTER_SOURCES})

target_include_directories(sdl_painter
    PUBLIC
        # Build agacinda kaynak include/, kurulumda CMAKE_INSTALL_INCLUDEDIR.
        # Goreli yol export edilemez; generator expression sart.
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    PRIVATE src
    # Gomulu shader basliklari (embedded_gl_shaders.h / embedded_vk_shaders.h)
    PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/generated"
)

target_link_libraries(sdl_painter
    PUBLIC  SDL3::SDL3
    PUBLIC  OpenGL::GL
    PUBLIC  glm::glm
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
endif()

if(TARGET SDL3_ttf::SDL3_ttf-static)
    target_link_libraries(sdl_painter PRIVATE SDL3_ttf::SDL3_ttf-static)
else()
    target_link_libraries(sdl_painter PRIVATE SDL3_ttf::SDL3_ttf)
endif()

set_target_warnings(sdl_painter)
set_target_sanitizers(sdl_painter)

# --- Uygulama çatısı katmanı (opsiyonsuz — her zaman derlenir ve paketlenir) bağımlılık; static lib olduğundan kullanmayan tüketiciye maliyeti yoktur
add_library(sdl_painter_app
    src/app/application.cpp
    src/app/event_translator.cpp
)
target_include_directories(sdl_painter_app
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    PRIVATE src
)
target_link_libraries(sdl_painter_app
    PUBLIC  sdl_painter
    PRIVATE spdlog::spdlog_header_only
)
set_target_warnings(sdl_painter_app)
set_target_sanitizers(sdl_painter_app)

# --- Namespace'li ALIAS hedefler ---
# add_subdirectory()/FetchContent ile ekleyen tuketici de kurulmus paketle
add_library(sdl_painter::sdl_painter ALIAS sdl_painter)
add_library(sdl_painter::app         ALIAS sdl_painter_app)

# sdl_painter_app export'ta `sdl_painter::app` olarak gorunsun (varsayilan `sdl_painter::sdl_painter_app` olurdu).
set_target_properties(sdl_painter_app PROPERTIES EXPORT_NAME app)

set_target_properties(sdl_painter sdl_painter_app PROPERTIES
    POSITION_INDEPENDENT_CODE ON
    VERSION   ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
)

# MinGW cross-compile: libgcc_s_seh-1.dll ve libstdc++-6.dll çalışma zamanı
# bağımlılıklarını binary'ye gömerek dağıtımı tek-dosyaya indir.
#
# Dizin kapsamli add_link_options() KULLANILMAZ: add_subdirectory() ile ekleyen
# tuketicinin kendi hedefleri de bundan etkilenirdi — bayraklar dogrudan
# hedeflere veriliyor.
if(MINGW)
    target_link_options(sdl_painter      PRIVATE -static-libgcc -static-libstdc++)
    target_link_options(sdl_painter_app  PRIVATE -static-libgcc -static-libstdc++)
endif()

# Kurulum + export (find_package(sdl_painter) ve Conan/vcpkg paketlemesi bunun
# uzerine biner) — hedefler tanimlandiktan SONRA.
include(InstallRules)

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
# Yalnizca ust proje iken: alt proje olarak eklenen tuketicinin hedef listesini
# kirletmeyelim, ayrica gereksiz find_package(Doxygen) maliyeti olmasin.
if(PROJECT_IS_TOP_LEVEL)
    include(Docs)
endif()
