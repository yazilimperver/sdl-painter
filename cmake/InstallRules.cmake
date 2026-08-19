# Kurulum ve export kuralları.
#
# Bu modül olmadan kütüphane repo dışından tüketilemez: `cmake --install`
# hiçbir şey kurmaz, `find_package(sdl_painter)` çalışmaz. Conan/vcpkg
# paketlemesi de bunun üstüne biner (recipe `cmake.install()` çağırır).
#
# Not: Hedefler tanımlandıktan SONRA include edilmeli. GNUInstallDirs kökte,
# hedeflerden önce include edilir (target arayüzleri CMAKE_INSTALL_INCLUDEDIR
# kullanıyor).

include(CMakePackageConfigHelpers)

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
