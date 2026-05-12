# In-source build koruması. CMakeLists.txt'in en başında include edilmeli.
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR)
    message(FATAL_ERROR
        "In-source build tespit edildi!\n"
        "Lutfen ayri bir build dizini olusturun:\n"
        "  cmake -B build\n"
        "Kaynak dizinindeki CMakeCache.txt ve CMakeFiles/ dizinini silmeniz gerekebilir.")
endif()
