find_package(Doxygen OPTIONAL_COMPONENTS dot)

if(NOT DOXYGEN_FOUND)
    message(STATUS "Doxygen bulunamadi — 'sdlpainter_docs' hedefi olusturulmadi")
    return()
endif()

set(DOXYGEN_OUTPUT_DIR "${PROJECT_BINARY_DIR}/docs/public")

# doxygen-awesome-css submodule'ü varsa kullan, yoksa tema olmadan devam et.
set(DOXYGEN_AWESOME_CSS "")
set(_AWESOME "${PROJECT_SOURCE_DIR}/third_party/doxygen-awesome-css/doxygen-awesome.css")
if(EXISTS "${_AWESOME}")
    set(DOXYGEN_AWESOME_CSS "${_AWESOME}")
else()
    message(STATUS "doxygen-awesome-css submodule bulunamadi — varsayilan Doxygen temasi kullanilacak")
    message(STATUS "  Eklemek icin: git submodule add https://github.com/jothepro/doxygen-awesome-css.git third_party/doxygen-awesome-css")
endif()

if(DOXYGEN_DOT_FOUND)
    set(DOXYGEN_HAVE_DOT YES)
else()
    set(DOXYGEN_HAVE_DOT NO)
endif()

configure_file(
    "${PROJECT_SOURCE_DIR}/cmake/Doxyfile.in"
    "${PROJECT_BINARY_DIR}/Doxyfile"
    @ONLY
)

add_custom_target(sdlpainter_docs
    COMMAND ${CMAKE_COMMAND} -E make_directory "${DOXYGEN_OUTPUT_DIR}"
    COMMAND ${DOXYGEN_EXECUTABLE} "${PROJECT_BINARY_DIR}/Doxyfile"
    WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
    COMMENT "Doxygen ile API dokumantasyonu olusturuluyor..."
    VERBATIM
)

message(STATUS "Doxygen ${DOXYGEN_VERSION} bulundu — hedef: sdlpainter_docs")
if(DOXYGEN_DOT_FOUND)
    message(STATUS "  Graphviz dot bulundu — sinif diyagramlari etkin")
endif()
