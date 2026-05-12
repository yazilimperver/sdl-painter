from conan import ConanFile
from conan.tools.cmake import CMakeToolchain, CMakeDeps, cmake_layout, CMake
from conan.tools.files import copy
import os


class SDLPainterConan(ConanFile):
    name = "sdl_painter"
    version = "0.1.0"
    settings = "os", "compiler", "build_type", "arch"
    options = {
        "with_vulkan": [True, False],
        "build_examples": [True, False],
        "build_tests": [True, False],
    }
    default_options = {
        "with_vulkan": False,
        "build_examples": True,
        "build_tests": True,
        # SDL3_ttf (static) + SDL3 (shared) farklı zlib versiyonu çekmemesi için
        # tüm bağımlılıklarda aynı shared zlib kullan.
        "zlib/*:shared": True,
    }

    def requirements(self):
        self.requires("sdl/3.2.14")
        self.requires("glad/0.1.36")
        self.requires("stb/cci.20240531")
        self.requires("spdlog/1.15.3", options={"spdlog/*:header_only": True})
        self.requires("sdl_ttf/3.2.2")

        if self.options.with_vulkan:
            self.requires("vulkan-loader/1.3.290.0")
            self.requires("vulkan-headers/1.3.290.0")

        if self.options.build_tests:
            self.requires("gtest/1.15.0")

    def layout(self):
        cmake_layout(self)

    def generate(self):
        deps = CMakeDeps(self)
        deps.generate()
        tc = CMakeToolchain(self)
        tc.variables["SDLPAINTER_WITH_VULKAN"] = self.options.with_vulkan
        tc.variables["SDLPAINTER_BUILD_EXAMPLES"] = self.options.build_examples
        tc.variables["SDLPAINTER_BUILD_TESTS"] = self.options.build_tests
        tc.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        copy(self, "*.h",
             src=os.path.join(self.source_folder, "include"),
             dst=os.path.join(self.package_folder, "include"))
        copy(self, "*.a",
             src=self.build_folder,
             dst=os.path.join(self.package_folder, "lib"),
             keep_path=False)
        copy(self, "*.lib",
             src=self.build_folder,
             dst=os.path.join(self.package_folder, "lib"),
             keep_path=False)
        copy(self, "*.so*",
             src=self.build_folder,
             dst=os.path.join(self.package_folder, "lib"),
             keep_path=False)
        copy(self, "*.dylib",
             src=self.build_folder,
             dst=os.path.join(self.package_folder, "lib"),
             keep_path=False)
        copy(self, "*.dll",
             src=self.build_folder,
             dst=os.path.join(self.package_folder, "bin"),
             keep_path=False)

    def package_info(self):
        self.cpp_info.libs = ["sdl_painter"]
        self.cpp_info.set_property("cmake_target_name", "sdl_painter::sdl_painter")
        if self.options.with_vulkan:
            self.cpp_info.defines.append("SDLPAINTER_HAS_VULKAN")
