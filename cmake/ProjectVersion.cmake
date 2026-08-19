# Sürüm bilgisi — tek kaynak: include/sdl_painter/version.h
#
# Bu modül `project()` çağrısından ÖNCE include edilmeli; çıktı olarak
# `SDLPAINTER_VERSION` ve `SDLPAINTER_VERSION_{MAJOR,MINOR,PATCH}` bırakır:
#
#   list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
#   include(ProjectVersion)
#   project(SDLPainter VERSION ${SDLPAINTER_VERSION} LANGUAGES CXX)
#
# Ayrıca sürümün tekrarlandığı iki yeri (version.h içindeki metin sabiti ve
# kök Doxyfile) sayısal bileşenlerle karşılaştırır — elle güncellemede kaçan
# tutarsızlığı configure aşamasında yakalar.

set(SDLPAINTER_VERSION_HEADER
    "${CMAKE_CURRENT_SOURCE_DIR}/include/sdl_painter/version.h")
file(READ "${SDLPAINTER_VERSION_HEADER}" _version_header_content)

# Sürüm yükseltilince CMake kendini yeniden çalıştırsın.
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
             "${SDLPAINTER_VERSION_HEADER}")

# Surum bilesenleri: `inline constexpr int32_t kVersionMajor = 1;` gibi
# satirlardan okunur. `constexpr int32_t` capasi kasitli — dosyada yalnizca
# tanimda gecer, dokuman yorumlarinda gecmez.
foreach(_component Major Minor Patch)
    if(NOT _version_header_content MATCHES
       "constexpr int32_t kVersion${_component} = ([0-9]+)")
        message(FATAL_ERROR
            "kVersion${_component} okunamadi: ${SDLPAINTER_VERSION_HEADER}")
    endif()
    string(TOUPPER "${_component}" _component_upper)
    set(SDLPAINTER_VERSION_${_component_upper} "${CMAKE_MATCH_1}")
endforeach()

set(SDLPAINTER_VERSION
    "${SDLPAINTER_VERSION_MAJOR}.${SDLPAINTER_VERSION_MINOR}.${SDLPAINTER_VERSION_PATCH}")

# kVersionString sayisal bilesenlerle uyusmali.
if(NOT _version_header_content MATCHES
   "constexpr std::string_view kVersionString = \"([^\"]+)\"")
    message(FATAL_ERROR
        "kVersionString okunamadi: ${SDLPAINTER_VERSION_HEADER}")
endif()

if(NOT CMAKE_MATCH_1 STREQUAL SDLPAINTER_VERSION)
    message(FATAL_ERROR
        "version.h tutarsiz: kVersionString=\"${CMAKE_MATCH_1}\" "
        "ancak sayisal sabitler ${SDLPAINTER_VERSION} diyor.")
endif()

unset(_version_header_content)

# Kok Doxyfile (dogrudan `doxygen Doxyfile` icin) surumu kendi icinde tutmak
# zorunda — Doxygen header okuyamaz. Bu kontrol sessizce ayrismasini onler.
# cmake/Doxyfile.in zaten @PROJECT_VERSION@ kullaniyor, o dosya etkilenmez.
#
# EXISTS kontrolu sart: paket derlemelerinde (Conan/vcpkg) kaynak agacinin
# yalnizca derleme icin gereken kismi kopyalanir, Doxyfile bulunmaz.
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile")
    file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile"
         _doxyfile_version REGEX "^PROJECT_NUMBER[ \t]*=")
    if(NOT _doxyfile_version MATCHES "${SDLPAINTER_VERSION}")
        message(WARNING
            "Doxyfile PROJECT_NUMBER (${_doxyfile_version}) "
            "version.h surumu ${SDLPAINTER_VERSION} ile uyusmuyor.")
    endif()
    unset(_doxyfile_version)
endif()
