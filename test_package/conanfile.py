import os

from conan import ConanFile
from conan.tools.build import can_run
from conan.tools.cmake import CMake, CMakeDeps, CMakeToolchain, cmake_layout


class SDLPainterTestConan(ConanFile):
    """Paketin tuketilebilirligini dogrular: header bulunuyor mu, link
    ediliyor mu, derlenmis sembol cozuluyor mu.

    GPU veya ekran GEREKTIRMEZ — Conan Center derleyicilerinde ikisi de yok.
    Pencere acan bir test_package orada duser.
    """

    settings = "os", "compiler", "build_type", "arch"
    generators = "VirtualRunEnv"
    test_type = "explicit"

    def requirements(self):
        self.requires(self.tested_reference_str)

    def layout(self):
        cmake_layout(self)

    def generate(self):
        CMakeToolchain(self).generate()
        CMakeDeps(self).generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def test(self):
        if can_run(self):
            # Onceki hali `f".{self.package_folder}/test_package"` idi;
            # package_folder mutlak yol oldugu icin basina nokta eklenince
            # ".C:/..." gibi gecersiz bir yol uretiyordu.
            bin_path = os.path.join(self.cpp.build.bindir, "test_package")
            self.run(bin_path, env="conanrun")
