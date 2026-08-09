from conan import ConanFile
from conan.errors import ConanInvalidConfiguration
from conan.tools.build import check_min_cppstd
from conan.tools.cmake import CMakeToolchain, CMakeDeps, cmake_layout, CMake
from conan.tools.files import copy, load, rmdir
from conan.tools.scm import Version
import os
import re


class SDLPainterConan(ConanFile):
    """SDLPainter'ın repo içi Conan recipe'ı.

    NOT: Bu dosya Conan Center'a gönderilecek recipe DEĞİLDİR. CCI recipe'i
    `conan-io/conan-center-index` deposunda `recipes/sdl_painter/all/` altında
    yaşar, kaynağı `conandata.yml` üzerinden tarball'dan indirir ve
    `build_examples`/`build_tests` gibi seçenekleri barındıramaz.
    Bu dosya geliştiricinin bağımlılıkları çekmesi ve yerel `conan create`
    denemesi içindir (bkz. roadmap/03-conan-center-plani.md).
    """

    author = "yazilimperver"
    name = "sdl_painter"
    description = ("Modern C++17 2D drawing library for SDL3 with "
                   "interchangeable OpenGL 3.3 and Vulkan 1.1 backends.")
    license = "MIT"
    homepage = "https://github.com/yazilimperver/sdl-painter"
    url = "https://github.com/yazilimperver/sdl-painter"
    topics = ("sdl", "sdl3", "graphics", "2d", "rendering", "opengl", "vulkan")

    package_type = "library"
    settings = "os", "compiler", "build_type", "arch"
    exports_sources = (
        "CMakeLists.txt", "cmake/*", "include/*", "src/*",
        # conan create sırasında CMake bu dizinlere de giriyor (secenekler
        # varsayilan olarak acik); export edilmezlerse configure duser.
        "examples/*", "tests/*",
        # package() lisansi pakete kopyaliyor.
        "LICENSE",
    )

    options = {
        "shared": [True, False],
        "fPIC": [True, False],
        "with_vulkan": [True, False],
        "build_examples": [True, False],
        "build_tests": [True, False],
    }
    default_options = {
        "shared": False,
        "fPIC": True,
        "with_vulkan": False,
        "build_examples": True,
        "build_tests": True,
        # SDL3_ttf (static) + SDL3 (shared) farklı zlib versiyonu çekmemesi için
        # tüm bağımlılıklarda aynı shared zlib kullan.
        "zlib/*:shared": True,
    }

    @property
    def _min_cppstd(self):
        return 17

    @property
    def _compilers_minimum_version(self):
        return {"gcc": "8", "clang": "7", "apple-clang": "12", "msvc": "192"}

    def set_version(self):
        # Versiyonun tek kaynağı include/sdl_painter/version.h. CMake de aynı
        # dosyadan okur; böylece Conan ve CMake sürümleri ayrışamaz.
        header = load(self, os.path.join(
            self.recipe_folder, "include", "sdl_painter", "version.h"))
        parts = []
        for component in ("Major", "Minor", "Patch"):
            match = re.search(
                r"constexpr\s+int32_t\s+kVersion%s\s*=\s*(\d+)" % component, header)
            if not match:
                raise RuntimeError(
                    "version.h icinde kVersion%s bulunamadi" % component)
            parts.append(match.group(1))
        version = ".".join(parts)

        # kVersionString ile sayisal sabitler ayrisamaz — CMake de ayni kontrolu
        # yapiyor, ama conan create CMake'ten once set_version cagiriyor.
        match = re.search(
            r"constexpr\s+std::string_view\s+kVersionString\s*=\s*\"([^\"]+)\"",
            header)
        if not match:
            raise RuntimeError("version.h icinde kVersionString bulunamadi")
        if match.group(1) != version:
            raise RuntimeError(
                "version.h tutarsiz: kVersionString=%r ancak sayisal sabitler "
                "%s diyor" % (match.group(1), version))

        self.version = version

    def config_options(self):
        if self.settings.os == "Windows":
            del self.options.fPIC

    def validate(self):
        if self.settings.os not in ("Linux", "Windows", "FreeBSD"):
            raise ConanInvalidConfiguration(
                "%s henuz %s desteklemiyor (macOS v1.1 hedefinde)."
                % (self.ref, self.settings.os))

        if self.settings.compiler.get_safe("cppstd"):
            check_min_cppstd(self, self._min_cppstd)

        minimum = self._compilers_minimum_version.get(
            str(self.settings.compiler))
        if minimum and Version(self.settings.compiler.version) < minimum:
            raise ConanInvalidConfiguration(
                "%s C++%d gerektiriyor; %s %s bunu desteklemiyor."
                % (self.ref, self._min_cppstd, self.settings.compiler,
                   self.settings.compiler.version))

    def configure(self):
        if self.options.shared:
            self.options.rm_safe("fPIC")

        if self.settings.os == "Windows":
            # MinGW cross-compile (Windows + gcc): vulkan-loader Conan recipe'ı
            # Windows'ta USE_MASM=True'yu hardcoded set ediyor ve kapatan bir
            # option sunmuyor. MinGW gcc, asm_offset.c'ye MSVC assembler
            # flag'leri (/Fa /FA /Od) geçirilince derleme patlıyor. Bu yüzden
            # MinGW hedefinde Vulkan'ı otomatik kapat. Windows'ta Vulkan için
            # native MSVC build (windows-release preset) kullanılmalı —
            # MSVC'de USE_MASM sorunsuz çalışır.
            if self.settings.compiler == "gcc" and self.options.with_vulkan:
                self.output.warning(
                    "MinGW cross-compile vulkan-loader'ı desteklemiyor; "
                    "with_vulkan=False'a çekildi. Windows'ta Vulkan için "
                    "native MSVC build kullanın.")
                self.options.with_vulkan = False

            # SDL ve SDL3_ttf shared olmak zorunda (recipe validation aynı
            # shared değeri gerektiriyor).
            self.options["sdl"].shared = True
            self.options["sdl_ttf"].shared = True
            # SDL3_ttf'nin transitive bağımlılıkları MinGW cross-compile'da
            # statik linkte çakışıyor:
            #   - harfbuzz: hb-uniscribe Windows rpcrt4 (UuidCreate) ister
            #   - plutosvg ⇄ plutovg: __imp_ DLL import beklentisi var
            #   - freetype: harfbuzz shared olunca FT_* sembollerini
            #     re-export ediyor; statik freetype ile çakışıyor
            # Tüm SDL3_ttf zincirini shared'a alarak duplicate symbol/
            # import sorunlarını çözüyoruz.
            self.options["harfbuzz"].shared = True
            self.options["freetype"].shared = True
            self.options["plutosvg"].shared = True
            self.options["plutovg"].shared = True

    def requirements(self):
        # SDL3: tuketici pencereyi kendisi olusturup Painter'a veriyor, yani
        # SDL basliklarina ihtiyaci var.
        self.requires("sdl/3.2.14", transitive_headers=True,
                      transitive_libs=True)
        # glm: painter.h dogrudan <glm/glm.hpp> include ediyor — public.
        self.requires("glm/1.0.3", transitive_headers=True)
        self.requires("glad/0.1.36")
        self.requires("stb/cci.20240531")
        self.requires("spdlog/1.15.3", options={"spdlog/*:header_only": True})
        self.requires("sdl_ttf/3.2.2")

        if self.options.with_vulkan:
            self.requires("vulkan-loader/1.3.290.0")
            self.requires("vulkan-headers/1.3.290.0")

    def build_requirements(self):
        # GTest yalnizca bu recipe'in kendi testlerini derlemek icin gerekli;
        # `requires` olsaydi tuketicinin bagimlilik grafigine sizardi.
        if self.options.build_tests:
            self.test_requires("gtest/1.15.0")

    # NOT: Eskiden burada `tool_requires("shaderc")` vardi (Vulkan shader'larini
    # glslc ile derlemek icin). SPIR-V ciktilari artik repo'da tutulup binary'ye
    # gomuldugu icin gerekmiyor — bkz. ADR-009. glslc yalnizca
    # -DSDLPAINTER_REGENERATE_SHADERS=ON ile aranir.

    def layout(self):
        cmake_layout(self)

    def generate(self):
        deps = CMakeDeps(self)
        deps.generate()
        tc = CMakeToolchain(self)
        # Conan'ın ürettiği CMakeUserPresets.json'ı devre dışı bırak.
        # Conan preset adını yalnızca build_type'tan türetiyor (conan-debug /
        # conan-release); --output-folder ve hedef platform adı etkilemiyor.
        # Bu yüzden aynı ağaçta İKİ Debug install'ı yapmak yeterli:
        # windows-debug + linux-debug (Docker/WSL) ya da sadece Linux'ta
        # linux-debug + linux-debug-asan. Her iki fragment de "conan-debug"
        # tanımlayınca CMake "Duplicate preset" hatasıyla HİÇBİR preset'i
        # yükleyemiyor — mevcut windows-debug preset'i bile kırılıyor.
        # Projenin kendi CMakePresets.json'ı toolchain dosyasının yolunu
        # zaten açıkça veriyor, bu yüzden bu dosyaya ihtiyaç yok.
        tc.user_presets_path = False
        tc.variables["SDLPAINTER_WITH_VULKAN"] = self.options.with_vulkan
        tc.variables["SDLPAINTER_BUILD_EXAMPLES"] = self.options.build_examples
        tc.variables["SDLPAINTER_BUILD_TESTS"] = self.options.build_tests
        tc.generate()

        # Windows hedefinde tüm transitive shared bağımlılıkların DLL'lerini
        # generators klasörüne stage'le. CMake helper'ı orayı glob'layıp
        # executable yanına kopyalıyor. Conan 2 + MinGW kombinasyonunda bazı
        # recipe'ler IMPORTED_LOCATION'ı set etmediğinden
        # $<TARGET_RUNTIME_DLLS> yetersiz kalıyor — bu staging fallback'i
        # garanti çözüm.
        if self.settings.os == "Windows":
            for dep in self.dependencies.host.values():
                for bindir in dep.cpp_info.bindirs:
                    copy(self, "*.dll",
                         src=bindir,
                         dst=self.generators_folder,
                         keep_path=False)

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        # Lisans pakete kopyalanmali (Conan Center zorunlu tutuyor, ve dogru
        # olan da bu — ikili dagitiliyorsa lisansi da dagitilmali).
        copy(self, "LICENSE",
             src=self.source_folder,
             dst=os.path.join(self.package_folder, "licenses"))

        # Binary glob'lamak yerine projenin kendi install kurallarini kullan
        # (Faz 1.2 ile eklendi). Boylece paket icerigi ile `cmake --install`
        # ciktisi ayrisamaz ve test binary'leri yanlislikla pakete girmez.
        cmake = CMake(self)
        cmake.install()

        # Projenin urettigi CMake config'i pakete girmemeli: Conan kendi
        # config'ini CMakeDeps ile uretir, ikisi cakisir.
        rmdir(self, os.path.join(self.package_folder, "lib", "cmake"))

    def package_info(self):
        self.cpp_info.set_property("cmake_file_name", "sdl_painter")

        # --- core: cizim kutuphanesi ---
        core = self.cpp_info.components["core"]
        core.set_property("cmake_target_name", "sdl_painter::sdl_painter")
        core.libs = ["sdl_painter"]
        core.requires = ["sdl::sdl", "glm::glm", "sdl_ttf::sdl_ttf",
                         "glad::glad", "stb::stb", "spdlog::spdlog"]
        if self.options.with_vulkan:
            core.requires += ["vulkan-loader::vulkan-loader",
                              "vulkan-headers::vulkan-headers"]
            core.defines.append("SDLPAINTER_HAS_VULKAN")

        # OpenGL sistemden geliyor (CMakeLists find_package(OpenGL)).
        if self.settings.os == "Windows":
            core.system_libs = ["opengl32"]
        elif self.settings.os in ("Linux", "FreeBSD"):
            core.system_libs = ["GL", "m", "dl", "pthread"]

        # --- app: opsiyonel pencere/olay-dongusu katmani (bkz. ADR-008) ---
        # Ayri component: yalnizca cizim isteyen tuketici bunu linklemez.
        app = self.cpp_info.components["app"]
        app.set_property("cmake_target_name", "sdl_painter::app")
        app.libs = ["sdl_painter_app"]
        app.requires = ["core"]
