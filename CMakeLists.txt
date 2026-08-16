cmake_minimum_required(VERSION 3.20)

# --- Sürüm: tek kaynak include/sdl_painter/version.h ---
set(SDLPAINTER_VERSION_HEADER "${CMAKE_CURRENT_SOURCE_DIR}/include/sdl_painter/version.h")
file(READ "${SDLPAINTER_VERSION_HEADER}" SDLPAINTER_VERSION_HEADER_CONTENT)

# Surum bilesenleri: `inline constexpr int32_t kVersionMajor = 1;` gibi
# satirlardan okunur. `constexpr int32_t` capasi kasitli — dosyada yalnizca
# tanimda gecer, dokuman yorumlarinda gecmez.
foreach(_component Major Minor Patch)
    if(NOT SDLPAINTER_VERSION_HEADER_CONTENT MATCHES
       "constexpr int32_t kVersion${_component} = ([0-9]+)")
        message(FATAL_ERROR
            "kVersion${_component} okunamadi: ${SDLPAINTER_VERSION_HEADER}")
    endif()
    string(TOUPPER "${_component}" _component_upper)
    set(SDLPAINTER_VERSION_${_component_upper} "${CMAKE_MATCH_1}")
endforeach()

set(SDLPAINTER_VERSION
    "${SDLPAINTER_VERSION_MAJOR}.${SDLPAINTER_VERSION_MINOR}.${SDLPAINTER_VERSION_PATCH}")

# kVersionString sayisal bilesenlerle uyusmali — elle guncellemede kacan
# tutarsizligi configure asamasinda yakalar.
if(NOT SDLPAINTER_VERSION_HEADER_CONTENT MATCHES
   "constexpr std::string_view kVersionString = \"([^\"]+)\"")
    message(FATAL_ERROR
        "kVersionString okunamadi: ${SDLPAINTER_VERSION_HEADER}")
endif()

if(NOT CMAKE_MATCH_1 STREQUAL SDLPAINTER_VERSION)
    message(FATAL_ERROR
        "version.h tutarsiz: kVersionString=\"${CMAKE_MATCH_1}\" "
        "ancak sayisal sabitler ${SDLPAINTER_VERSION} diyor.")
endif()

project(SDLPainter VERSION ${SDLPAINTER_VERSION} LANGUAGES CXX)

# Kok Doxyfile (dogrudan `doxygen Doxyfile` icin) surumu kendi icinde tutmak
# zorunda — Doxygen header okuyamaz. Bu kontrol sessizce ayrismasini onler.
# cmake/Doxyfile.in zaten @PROJECT_VERSION@ kullaniyor, o dosya etkilenmez.
#
# EXISTS kontrolu sart: paket derlemelerinde (Conan/vcpkg) kaynak agacinin
# yalnizca derleme icin gereken kismi kopyalanir, Doxyfile bulunmaz.
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile")
    file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile"
         SDLPAINTER_DOXYFILE_VERSION REGEX "^PROJECT_NUMBER[ \t]*=")
    if(NOT SDLPAINTER_DOXYFILE_VERSION MATCHES "${PROJECT_VERSION}")
        message(WARNING
            "Doxyfile PROJECT_NUMBER (${SDLPAINTER_DOXYFILE_VERSION}) "
            "version.h surumu ${PROJECT_VERSION} ile uyusmuyor.")
    endif()
endif()

include(cmake/InsourceGuard.cmake)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Yalnizca ust proje iken: alt proje olarak eklendigimizde tuketicinin
# ayarini ezmeyelim.
if(PROJECT_IS_TOP_LEVEL)
    set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
endif()

# MinGW cross-compile: libgcc_s_seh-1.dll ve libstdc++-6.dll çalışma zamanı
# bağımlılıklarını exe'ye gömerek dağıtımı tek-dosyaya indir.
#
# Dizin kapsamli add_link_options() KULLANILMAZ: add_subdirectory() ile ekleyen
# tuketicinin kendi hedefleri de bundan etkilenirdi. Bayraklar asagida
# dogrudan sdl_painter / sdl_painter_app hedeflerine veriliyor.

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
# Ornek ve testler yalnizca SDLPainter ust proje iken varsayilan olarak acik.
# add_subdirectory()/FetchContent ile ekleyen tuketici, istemedigi 15 ornegi ve
# GTest bagimliligini derlemek zorunda kalmasin.
option(SDLPAINTER_BUILD_EXAMPLES "Build examples" ${PROJECT_IS_TOP_LEVEL})
option(SDLPAINTER_BUILD_TESTS "Build tests" ${PROJECT_IS_TOP_LEVEL})

# Conan ile gelen paketleri bul
find_package(SDL3 REQUIRED)
find_package(glad REQUIRED)
find_package(stb REQUIRED)
find_package(spdlog REQUIRED)
find_package(glm REQUIRED)
find_package(OpenGL REQUIRED)

# Compiler warnings + sanitizer seçenekleri
include(cmake/CompilerWarnings.cmake)
include(cmake/CompilerOptions.cmake)

# Kurulum dizinleri (CMAKE_INSTALL_INCLUDEDIR/LIBDIR/BINDIR) — hedef arayuzleri
# bunlari kullandigi icin hedeflerden ONCE include edilmeli.
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

# --- Shader gomme ---
# Shader'lar binary'ye gomulur; calisma zamaninda dosya aranmaz. Paketlenmis
# kutuphanenin (Conan/vcpkg/sistem kurulumu) tuketici tarafinda ek dosya
# kopyalamadan calisabilmesi icin zorunlu.
include(cmake/EmbedShaders.cmake)

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

# --- Shader yeniden uretimi (yalnizca shader kaynagini degistiren gelistirici) ---
# GLSL kaynagini degistirdiysen: -DSDLPAINTER_REGENERATE_SHADERS=ON ile configure
# et, `regenerate_shaders` hedefini derle ve uretilen .spv dosyalarini commit'le.
# Normal derlemede glslc HIC aranmaz.
option(SDLPAINTER_REGENERATE_SHADERS
       "Vulkan SPIR-V ciktilarini glslc ile yeniden uret (gelistirici araci)" OFF)

if(SDLPAINTER_REGENERATE_SHADERS)
    find_program(GLSLC_EXECUTABLE glslc
        HINTS "$ENV{VULKAN_SDK}/Bin" "$ENV{VULKAN_SDK}/bin"
        DOC "glslc SPIR-V compiler")
    if(NOT GLSLC_EXECUTABLE)
        message(FATAL_ERROR
            "SDLPAINTER_REGENERATE_SHADERS=ON ancak glslc bulunamadi. "
            "Vulkan SDK kurun veya VULKAN_SDK ortam degiskenini ayarlayin.")
    endif()

    set(SDLPAINTER_REGENERATED_SPVS "")
    foreach(_shader untextured.vert untextured.frag textured.vert textured.frag)
        set(_src "${CMAKE_CURRENT_SOURCE_DIR}/src/vulkan/shaders/${_shader}")
        set(_spv "${SDLPAINTER_VULKAN_SHADER_SPIRV_DIR}/${_shader}.spv")
        add_custom_command(
            OUTPUT  "${_spv}"
            COMMAND "${GLSLC_EXECUTABLE}" "${_src}" -o "${_spv}"
            DEPENDS "${_src}"
            COMMENT "glslc: ${_shader} -> ${_shader}.spv (kaynak agacina yaziliyor)")
        list(APPEND SDLPAINTER_REGENERATED_SPVS "${_spv}")
    endforeach()

    # Manifest, CI'daki guncellik kontrolunun dayanagi (bkz.
    # cmake/CheckShaderFreshness.cmake). SPIR-V ile birlikte guncellenmeli,
    # yoksa kontrol anlamsizlasir.
    add_custom_target(regenerate_shaders
        DEPENDS ${SDLPAINTER_REGENERATED_SPVS}
        COMMAND ${CMAKE_COMMAND}
                -DSDLPAINTER_SHADER_DIR=${CMAKE_CURRENT_SOURCE_DIR}/src/vulkan/shaders
                -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/WriteShaderManifest.cmake
        COMMENT "SPIR-V ciktilari + sources.sha256 yeniden uretildi — commit'leyin")
endif()

find_package(SDL3_ttf REQUIRED)

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

# --- Uygulama çatısı katmanı (opsiyonsuz — her zaman derlenir ve paketlenir) ---
# SDL pencere/olay-döngüsü boilerplate'ini soyutlar. İki .cpp, sıfır yeni
# bağımlılık; static lib olduğundan kullanmayan tüketiciye maliyeti yoktur
# (bkz. ADR-008).
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
# AYNI ismi kullanabilsin diye. Ismin iki yolda ayni olmasi, ornek kodun
# her iki senaryoda da calismasini saglar.
add_library(sdl_painter::sdl_painter ALIAS sdl_painter)
add_library(sdl_painter::app         ALIAS sdl_painter_app)

# sdl_painter_app export'ta `sdl_painter::app` olarak gorunsun
# (varsayilan `sdl_painter::sdl_painter_app` olurdu).
set_target_properties(sdl_painter_app PROPERTIES EXPORT_NAME app)

set_target_properties(sdl_painter sdl_painter_app PROPERTIES
    POSITION_INDEPENDENT_CODE ON
    VERSION   ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
)

# MinGW runtime'ini exe'ye gom — hedef kapsamli, tuketiciye sizmaz.
if(MINGW)
    target_link_options(sdl_painter      PRIVATE -static-libgcc -static-libstdc++)
    target_link_options(sdl_painter_app  PRIVATE -static-libgcc -static-libstdc++)
endif()

# --- Kurulum ve export ---
#
# Bu blok olmadan kutuphane repo disindan tuketilemez: `cmake --install`
# hicbir sey kurmaz, `find_package(sdl_painter)` calismaz. Conan/vcpkg
# paketlemesi de bunun ustune biner (recipe `cmake.install()` cagirir).

install(TARGETS sdl_painter sdl_painter_app
        EXPORT  sdl_painterTargets
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

install(EXPORT sdl_painterTargets
        FILE        sdl_painterTargets.cmake
        NAMESPACE   sdl_painter::
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/sdl_painter)

# Config dosyasi, tuketicinin PUBLIC bagimliliklari bulabilmesi icin
# find_dependency cagirir. STATIC kutuphanede PRIVATE bagimliliklar da
# $<LINK_ONLY:...> olarak export'a girer ve tuketici tarafinda hedeflerinin
# var olmasi gerekir — bu yuzden hepsi sablonda aranir.
configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/sdl_painterConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/sdl_painterConfig.cmake"
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/sdl_painter)

write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/sdl_painterConfigVersion.cmake"
    VERSION       ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion)

install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/sdl_painterConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/sdl_painterConfigVersion.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/sdl_painter)

# Not: Eskiden burada sdlpainter_copy_shaders() ve
# sdlpainter_copy_vulkan_shaders() yardimcilari vardi; shader dosyalarini her
# executable'in yanina kopyaliyorlardi (MSVC'de paralel yazma race condition'i
# icin ayrica merkezi target'lar gerekiyordu). Shader'lar artik binary'ye
# gomulu oldugu icin hicbiri gerekmiyor — bkz. cmake/EmbedShaders.cmake.

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
# Yalnizca ust proje iken: alt proje olarak eklenen tuketicinin hedef listesini
# kirletmeyelim, ayrica gereksiz find_package(Doxygen) maliyeti olmasin.
if(PROJECT_IS_TOP_LEVEL)
    include(cmake/Docs.cmake)
endif()
