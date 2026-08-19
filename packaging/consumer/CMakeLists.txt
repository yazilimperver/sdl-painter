cmake_minimum_required(VERSION 3.21)
project(sdl_painter_consumer LANGUAGES CXX)

# Bu proje SDLPainter'i DISARIDAN tuketen bir kullaniciyi taklit ederek test eder.
# Repo'nun kendi CMake'ine bagli degildir.
#
# Iki yol da ayni hedef ismini kullanabilmeli: sdl_painter::sdl_painter
#   -DCONSUMER_MODE=find_package  -> kurulmus paketi bul
#   -DCONSUMER_MODE=subdirectory  -> kaynagi alt proje olarak ekle

set(CONSUMER_MODE "find_package" CACHE STRING "find_package | subdirectory")

if(CONSUMER_MODE STREQUAL "find_package")
    find_package(sdl_painter CONFIG REQUIRED)
    message(STATUS "consumer: sdl_painter ${sdl_painter_VERSION} bulundu")
elseif(CONSUMER_MODE STREQUAL "subdirectory")
    if(NOT DEFINED SDLPAINTER_SOURCE_DIR)
        message(FATAL_ERROR "SDLPAINTER_SOURCE_DIR verilmeli")
    endif()
    add_subdirectory("${SDLPAINTER_SOURCE_DIR}" sdl_painter_build)
else()
    message(FATAL_ERROR "Bilinmeyen CONSUMER_MODE: ${CONSUMER_MODE}")
endif()

add_executable(consumer main.cpp)
target_link_libraries(consumer PRIVATE sdl_painter::sdl_painter)
target_compile_features(consumer PRIVATE cxx_std_17)

# Windows'ta calisma zamani DLL'leri (SDL3.dll vb.) exe'nin yaninda olmali.
# sdlpainter_copy_runtime_dlls() SDLPainter'in CMakeLists'inde tanimli bir
# fonksiyon; CMake fonksiyonlari global oldugu icin add_subdirectory ile
# ekleyen tuketici bunu cagirabilir. find_package yolunda ise TANIMLI DEGIL —
# config dosyasi fonksiyon export etmez.
if(COMMAND sdlpainter_copy_runtime_dlls)
    message(STATUS "consumer: sdlpainter_copy_runtime_dlls MEVCUT, cagriliyor")
    sdlpainter_copy_runtime_dlls(consumer)
else()
    # find_package yolunda yardimci tanimli degil. Burasi README'nin
    # "Windows: runtime DLLs" bolumunde onerilen desenin BIREBIR aynisi —
    # dokumanin dogrulugu bu testle bagli.
    message(STATUS "consumer: yardimci YOK — README'deki TARGET_RUNTIME_DLLS deseni kullaniliyor")
    if(WIN32)
        add_custom_command(TARGET consumer POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    $<TARGET_RUNTIME_DLLS:consumer> $<TARGET_FILE_DIR:consumer>
            COMMAND_EXPAND_LISTS)
    endif()
endif()
