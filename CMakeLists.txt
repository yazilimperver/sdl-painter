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

# spdlog hedef adi `header_only` secenegine gore DEGISIYOR ve iki adtan yalnizca
# BIRI tanimli oluyor:
#   header_only=True  -> spdlog::spdlog_header_only  (spdlog::spdlog YOK)
#   header_only=False -> spdlog::spdlog              (header_only YOK)
# Conan Center'da bagimlilik secenegi zorlamak yasak oldugundan sabit bir ada
# baglanamayiz; hangisi varsa o kullanilir. (vcpkg/upstream spdlog ikisini de
# tanimlar; orada header-only dal secilir.)
if(TARGET spdlog::spdlog_header_only)
    set(SDLPAINTER_SPDLOG_TARGET spdlog::spdlog_header_only)
elseif(TARGET spdlog::spdlog)
    set(SDLPAINTER_SPDLOG_TARGET spdlog::spdlog)
else()
    message(FATAL_ERROR
        "spdlog bulundu ama ne spdlog::spdlog_header_only ne de spdlog::spdlog "
        "hedefi tanimli. Beklenmeyen bir spdlog paketi kullaniliyor.")
endif()
message(STATUS "spdlog hedefi: ${SDLPAINTER_SPDLOG_TARGET}")

# Compiler warnings + sanitizer seçenekleri
include(CompilerWarnings)
include(CompilerOptions)
include(CodeCoverage)

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
    src/path.cpp
    src/render_target.cpp
    src/tessellator.cpp
    src/render_batcher.cpp
    src/font.cpp
    src/glyph_atlas.cpp
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
        src/vulkan/vulkan_render_target.cpp
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

# --- Sembol export makrolari ---
#
# Windows/MSVC bir DLL'den hicbir sembolu varsayilan olarak disari vermez.
# Export isaretlemesi olmadan `BUILD_SHARED_LIBS=ON` derlemesi .dll uretir ama
# .lib (import library) URETMEZ; yani hicbir tuketici link edemez. Olculdu:
# isaretleme oncesi sdl_painter.dll olusuyor, sdl_painter.lib olusmuyordu.
# Conan Center shared derlemeyi zorunlu olarak test ettiginden bu bir paketleme
# engeliydi.
#
# `SDLPAINTER_EXPORTS_DIR` ayri tutuluyor: `generated/` icinde gomulu shader
# basliklari var ve onlar DAHILI. Export basligi public include yolunda olmak
# zorunda oldugundan ikisi karistirilmiyor.
include(GenerateExportHeader)

set(SDLPAINTER_EXPORTS_DIR "${CMAKE_CURRENT_BINARY_DIR}/exports")

generate_export_header(sdl_painter
    BASE_NAME SDLPAINTER
    EXPORT_MACRO_NAME SDLPAINTER_API
    NO_EXPORT_MACRO_NAME SDLPAINTER_NO_EXPORT
    EXPORT_FILE_NAME "${SDLPAINTER_EXPORTS_DIR}/sdl_painter/export.h")

# Static derlemede makrolar bos olmali; uretilen baslik bunu
# SDLPAINTER_STATIC_DEFINE ile anliyor. PUBLIC: tuketici de ayni tanimi gormeli.
if(NOT BUILD_SHARED_LIBS)
    target_compile_definitions(sdl_painter PUBLIC SDLPAINTER_STATIC_DEFINE)
endif()

target_include_directories(sdl_painter
    PUBLIC
        # Build agacinda kaynak include/, kurulumda CMAKE_INSTALL_INCLUDEDIR.
        # Goreli yol export edilemez; generator expression sart.
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        # Uretilen export basligi (sdl_painter/export.h) — kurulumda ayni
        # include koku altina kopyalanir, bkz. InstallRules.cmake.
        $<BUILD_INTERFACE:${SDLPAINTER_EXPORTS_DIR}>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    PRIVATE src
    # Gomulu shader basliklari (embedded_gl_shaders.h / embedded_vk_shaders.h)
    PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/generated"
)

target_link_libraries(sdl_painter
    PUBLIC  SDL3::SDL3
    PRIVATE OpenGL::GL
    PUBLIC  glm::glm
    PRIVATE glad::glad
    PRIVATE stb::stb
    PRIVATE ${SDLPAINTER_SPDLOG_TARGET}
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
set_target_coverage(sdl_painter)

# --- Uygulama çatısı katmanı (opsiyonsuz — her zaman derlenir ve paketlenir) bağımlılık; static lib olduğundan kullanmayan tüketiciye maliyeti yoktur
add_library(sdl_painter_app
    src/app/application.cpp
    src/app/event_translator.cpp
    src/app/stats_overlay.cpp
)

# Ayri bir kutuphane, yani ayri bir DLL: kendi export makrosu olmali.
generate_export_header(sdl_painter_app
    BASE_NAME SDLPAINTER_APP
    EXPORT_MACRO_NAME SDLPAINTER_APP_API
    NO_EXPORT_MACRO_NAME SDLPAINTER_APP_NO_EXPORT
    EXPORT_FILE_NAME "${SDLPAINTER_EXPORTS_DIR}/sdl_painter/app/export.h")

if(NOT BUILD_SHARED_LIBS)
    target_compile_definitions(sdl_painter_app PUBLIC SDLPAINTER_APP_STATIC_DEFINE)
endif()

target_include_directories(sdl_painter_app
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${SDLPAINTER_EXPORTS_DIR}>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    PRIVATE src
)
target_link_libraries(sdl_painter_app
    PUBLIC  sdl_painter
    PRIVATE ${SDLPAINTER_SPDLOG_TARGET}
)
set_target_warnings(sdl_painter_app)
set_target_sanitizers(sdl_painter_app)
set_target_coverage(sdl_painter_app)

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
