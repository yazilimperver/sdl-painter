vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO yazilimperver/sdl-painter
    REF "v${VERSION}"
    SHA512 f5a263274a267b1cadcce5d5d361bd2775d296c3897cb26b7f50eeb12a229e7619520824e81923b17c852400c57973eae2cf6cd2fc9c8ac1502e786e97bd0d64
    HEAD_REF main
    PATCHES
        vcpkg-dependencies.patch
)

vcpkg_check_features(OUT_FEATURE_OPTIONS FEATURE_OPTIONS
    FEATURES
        vulkan SDLPAINTER_WITH_VULKAN
)

vcpkg_cmake_configure(
    SOURCE_PATH "${SOURCE_PATH}"
    OPTIONS
        ${FEATURE_OPTIONS}
        -DSDLPAINTER_BUILD_EXAMPLES=OFF
        -DSDLPAINTER_BUILD_TESTS=OFF
        -DSDLPAINTER_WARNINGS_AS_ERRORS=OFF
        -DCMAKE_DISABLE_FIND_PACKAGE_Doxygen=ON
)

vcpkg_cmake_install()
vcpkg_cmake_config_fixup(PACKAGE_NAME sdl_painter CONFIG_PATH lib/cmake/sdl_painter)
vcpkg_copy_pdbs()

file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include")

file(INSTALL "${CMAKE_CURRENT_LIST_DIR}/usage" DESTINATION "${CURRENT_PACKAGES_DIR}/share/${PORT}")
vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/LICENSE")
