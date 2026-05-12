# SDLPainter

QPainter benzeri, **SDL3 + OpenGL/Vulkan** dual backend destekli C++17 2D çizim kütüphanesi.

```cpp
sdl_painter::Painter painter(window, sdl_painter::RendererBackend::kOpenGL);

painter.Begin();
painter.Clear({30, 30, 30, 255});

painter.SetPen(sdl_painter::Pen({255, 0, 0, 255}, 2.0f));
painter.DrawCircle(400, 300, 80);

painter.Save();
painter.Translate(400, 300);
painter.Rotate(45.0f);
painter.DrawRect(-50, -50, 100, 100);
painter.Restore();

painter.End();
```

## Ön Koşullar

| Araç | Minimum Sürüm |
|------|--------------|
| CMake | 3.20 |
| Conan | 2.x |
| GCC / Clang | C++17 desteği |
| MSVC | VS 2022 (v143) |
| Ninja | Herhangi bir sürüm (Linux) |

**Opsiyonel:**
- Vulkan SDK (`glslc` için) — Vulkan backend kullanılacaksa
- `clang-format-18`, `clang-tidy-18` — Kalite kontrolü için
- `doxygen` — API referans dokümantasyonu üretmek için

## Derleme

### Linux — Manuel

```bash
# 1. Conan profili oluştur (ilk seferde)
conan profile detect

# 2. Bağımlılıkları yükle
conan install . --output-folder=build/linux-debug/generators --build=missing -s build_type=Debug

# 3. Derle
cmake --preset linux-debug
cmake --build --preset linux-debug

# 4. Testleri çalıştır
ctest --preset linux-debug
```

### Linux — Script ile

```bash
chmod +x scripts/*.sh

# Derle (Debug, varsayılan)
./scripts/build.sh

# Release derle
./scripts/build.sh Release

# Build + API dokümantasyonu oluştur
./scripts/build.sh --docs

# Testleri çalıştır
./scripts/run-tests.sh

# Format kontrolü
./scripts/format-check.sh
```

### Windows (Visual Studio 2022) — Manuel

```powershell
# 0. VS 2022 ortam değişkenlerini yükle
$vsInstallPath = & "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" -latest -property installationPath
Import-Module "$vsInstallPath\Common7\Tools\Microsoft.VisualStudio.DevShell.dll"
Enter-VsDevShell -VsInstallPath $vsInstallPath -DevCmdArguments "-arch=x64"

# 1. Conan profili oluştur (ilk seferde)
conan profile detect

# 2. Bağımlılıkları yükle
conan install . --output-folder=build/windows-debug/generators --build=missing -s build_type=Debug

# 3. Derle
cmake --preset windows-debug
cmake --build --preset windows-debug
```

### Windows — Script ile

```powershell
# Derle (Debug, varsayılan)
.\scripts\Build.ps1

# Release derle
.\scripts\Build.ps1 Release

# Build + API dokümantasyonu oluştur
.\scripts\Build.ps1 -Docs

# Testleri çalıştır
.\scripts\Run-Tests.ps1

# Format kontrolü
.\scripts\Format-Check.ps1
```

### CMake Preset Referansı

| Preset | Platform | Build Type | Notlar |
|--------|----------|------------|--------|
| `linux-debug` | Linux | Debug | CI'da kullanılır |
| `linux-release` | Linux | Release | CI'da kullanılır |
| `linux-debug-asan` | Linux | Debug | ASan + UBSan aktif |
| `windows-debug` | Windows | Debug | MSVC, Visual Studio 17 2022 |
| `windows-release` | Windows | Release | MSVC, Visual Studio 17 2022 |

### CMake Seçenekleri

| Seçenek | Varsayılan | Açıklama |
|---------|-----------|----------|
| `SDLPAINTER_WITH_VULKAN` | `OFF` | Vulkan backend |
| `SDLPAINTER_BUILD_EXAMPLES` | `ON` | Örnek uygulamalar |
| `SDLPAINTER_BUILD_TESTS` | `ON` | GTest birim testleri |
| `ENABLE_SANITIZERS` | `OFF` | ASan + UBSan (GCC/Clang) |

Tüm özelliklerle (Vulkan + metin) derleme:

```bash
conan install . --output-folder=build/linux-debug/generators --build=missing \
    -s build_type=Debug -o "&:with_vulkan=True" -o "&:with_text=True"
cmake --preset linux-debug
cmake --build --preset linux-debug
```

## Script Referansı

Tüm scriptler proje kökünden çalıştırılmalıdır.

### build.sh / Build.ps1 — Derleme

```bash
# Kullanım: ./scripts/build.sh [Debug|Release|ASan] [bayraklar]
./scripts/build.sh                              # Debug (varsayılan)
./scripts/build.sh Release                      # Release
./scripts/build.sh ASan                         # Debug + ASan/UBSan
./scripts/build.sh Debug --vulkan               # Vulkan backend
./scripts/build.sh Release --no-examples        # Örneksiz derleme
./scripts/build.sh Debug --target phase2_demo   # Tek hedef
./scripts/build.sh --clean                      # Build dizinini sil, baştan derle
./scripts/build.sh --jobs 8                     # 8 paralel iş
./scripts/build.sh --skip-conan                 # Sadece CMake (Conan atla)
./scripts/build.sh --docs                       # Build + Doxygen HTML dokümantasyonu
```

```powershell
# Kullanım: .\scripts\Build.ps1 [Debug|Release] [bayraklar]
.\scripts\Build.ps1                             # Debug (varsayılan)
.\scripts\Build.ps1 Release                     # Release
.\scripts\Build.ps1 -Vulkan                     # Vulkan backend
.\scripts\Build.ps1 Release -NoExamples         # Örneksiz derleme
.\scripts\Build.ps1 -Target phase2_demo         # Tek hedef
.\scripts\Build.ps1 -Clean                      # Temiz build
.\scripts\Build.ps1 -Jobs 8                     # 8 paralel iş
.\scripts\Build.ps1 -SkipConan                  # Sadece CMake
.\scripts\Build.ps1 -Docs                       # Build + Doxygen HTML dokümantasyonu
```

> Toolchain eksikse veya bayraklar değiştiyse `build` scriptleri `conan-install` scriptini otomatik çağırır.

### conan-install.sh / Conan-Install.ps1 — Bağımlılıklar

```bash
# Kullanım: ./scripts/conan-install.sh [Debug|Release|ASan] [bayraklar]
./scripts/conan-install.sh                       # Debug (varsayılan)
./scripts/conan-install.sh Release               # Release
./scripts/conan-install.sh Debug --vulkan        # Vulkan bağımlılıkları dahil
./scripts/conan-install.sh Release --no-tests    # Test bağımlılıkları hariç
./scripts/conan-install.sh Debug --no-examples   # Örnek bağımlılıkları hariç
```

```powershell
.\scripts\Conan-Install.ps1                      # Debug (varsayılan)
.\scripts\Conan-Install.ps1 Release              # Release
.\scripts\Conan-Install.ps1 -Vulkan              # Vulkan bağımlılıkları dahil
.\scripts\Conan-Install.ps1 -NoTests             # Test bağımlılıkları hariç
.\scripts\Conan-Install.ps1 -NoExamples          # Örnek bağımlılıkları hariç
```

### run-tests.sh / Run-Tests.ps1 — Testler

```bash
# Kullanım: ./scripts/run-tests.sh [Debug|Release|ASan] [bayraklar]
./scripts/run-tests.sh                           # Tüm testler, Debug
./scripts/run-tests.sh Release                   # Tüm testler, Release
./scripts/run-tests.sh ASan                      # Sanitizer'lı testler
./scripts/run-tests.sh --filter Transform        # Sadece Transform testleri
```

```powershell
.\scripts\Run-Tests.ps1                          # Tüm testler, Debug
.\scripts\Run-Tests.ps1 Release                  # Tüm testler, Release
.\scripts\Run-Tests.ps1 -Filter Transform        # Sadece Transform testleri
```

### format-check.sh / Format-Check.ps1 — Format Kontrolü

```bash
./scripts/format-check.sh           # Format kontrolü (hata varsa çıkar)
./scripts/format-check.sh --fix     # Otomatik düzelt
```

```powershell
.\scripts\Format-Check.ps1                            # Format kontrolü
.\scripts\Format-Check.ps1 -FixMode                   # Otomatik düzelt
.\scripts\Format-Check.ps1 -ClangFormat clang-format-18  # Özel binary
```

## Mimari

```
SDLPainter (Public API)
  └── Transform Stack / RenderState
        └── Tessellator  (backend-agnostic vertex üretimi)
              └── IRenderer  (soyut arayüz)
                    ├── OpenGLRenderer  (OpenGL 3.3 Core)
                    └── VulkanRenderer  (Vulkan 1.1)
                          └── SDL3 Platform Layer
```

- **`IRenderer`** — Backend soyutlama katmanı. Yeni backend eklemek için sadece bu arayüzü implement et.
- **`Tessellator`** — Şekilleri vertex listesine dönüştürür; backend bilmez.
- **`RenderState`** — Transform matrisi + pen + brush + opacity + clip rect.
- Transform matrisi **3×3 affine** — GLM kullanılmıyor, özel implementasyon.

## Dizin Yapısı

```
sdl-painter/
├── include/sdl_painter/   # Public header'lar
├── src/                   # Implementasyon
│   ├── opengl/            # OpenGL backend + GLSL shader'lar
│   └── vulkan/            # Vulkan backend + SPIR-V shader'lar
├── tests/                 # GTest birim testleri
├── examples/              # Phase demo uygulamaları (phase0..phase5e)
├── cmake/                 # CMake yardımcı modülleri
├── scripts/               # Build/test yardımcı scriptleri (.sh + .ps1)
└── adr/                   # Architecture Decision Records
```

## Örnekler

```bash
# OpenGL demo'ları (build sonrası)
./build/linux-debug/examples/phase0_demo
./build/linux-debug/examples/phase1_demo
./build/linux-debug/examples/phase2_demo
./build/linux-debug/examples/phase2b_demo
./build/linux-debug/examples/phase3_demo
./build/linux-debug/examples/phase4_demo

# Vulkan demo'ları (--vulkan ile derlenmişse)
./build/linux-debug/examples/phase5a_vulkan_clear
./build/linux-debug/examples/phase5b_vulkan_triangles
./build/linux-debug/examples/phase5c_vulkan_textured
./build/linux-debug/examples/phase5d_vulkan_demo
./build/linux-debug/examples/phase5e_vulkan_text
```

## Docker

Dockerfile üç ayrı stage içerir:

| Stage | Tag örneği | İçerik |
|-------|-----------|--------|
| `builder` | `sdl-painter:dev` | GCC 13, Clang 18, CMake, Ninja, Conan 2, SDL3 sistem bağımlılıkları |
| `ci` | `sdl-painter:ci` | `builder` + lcov/gcovr + headless OpenGL (`SDL_VIDEODRIVER=offscreen`) |
| `windows-cross` | `sdl-painter:windows-cross` | `builder` + MinGW-w64 + Wine + Conan `windows-mingw` profili |

### Image'ları build etme

```bash
# Geliştirme ortamı (Linux)
docker build --target builder -t sdl-painter:dev .

# CI (headless OpenGL testi)
docker build --target ci -t sdl-painter:ci .

# Windows cross-compile
docker build --target windows-cross -t sdl-painter:windows-cross .
```

### Geliştirme shell'i

```bash
docker run --rm -it -v $(pwd):/workspace sdl-painter:dev bash
```

### Headless Linux testleri

```bash
docker run --rm -v $(pwd):/workspace sdl-painter:ci bash -c \
  "conan install . --output-folder=build/linux-debug/generators \
     --build=missing -s build_type=Debug && \
   cmake --preset linux-debug && \
   cmake --build --preset linux-debug && \
   ctest --preset linux-debug --output-on-failure"
```

### Windows cross-compile (Linux host'ta)

```bash
docker run --rm -v $(pwd):/workspace sdl-painter:windows-cross bash -c \
  "conan install . --output-folder=build/linux-debug/generators \
     --build=missing -s build_type=Debug \
     --profile:build=default \
     --profile:host=windows-mingw && \
   cmake --preset linux-debug \
     -DCMAKE_TOOLCHAIN_FILE=/usr/local/share/MinGwToolchain.cmake && \
   cmake --build --preset linux-debug"
```

## Dokümantasyon

### Tasarım Dokümanları

| Doküman | Açıklama |
|---------|----------|
| [Hızlı Başlangıç](doc/hizli-baslangic.md) | Kurulum ve ilk derleme adımları |
| [Mimari Genel Bakış](doc/mimari-genel-bakis.md) | Katman yapısı ve bileşenler |
| [Özellik Listesi](doc/sdl-painter-ozellikler.md) | Desteklenen ve planlanan özellikler |
| [Örnekler Rehberi](doc/sdl-painter-ornekler.md) | Örnek uygulama açıklamaları |
| [Sınıf Diyagramları](doc/sinif-diyagrami.md) | UML sınıf diyagramları |
| [Akış Diyagramları](doc/akislar.md) | Çizim ve durum akışları |
| [Backend İç Yapısı](doc/backend-ic-yapisi.md) | OpenGL / Vulkan implementasyon detayları |
| [Yazılım Mühendisliği](doc/sdl-painter-yazilim-muhendisligi.md) | Tasarım kararları ve teknik gerekçeler |
| [Dokümantasyon Rehberi](doc/dokumantasyon-rehberi.md) | Doxygen kurulum ve kullanımı |

### API Referansı (Doxygen)

API referans dokümantasyonu [Doxygen](https://www.doxygen.nl/) ile üretilir.

**Online:** `main` branch'e her push'ta GitLab CI otomatik olarak yayınlar.
URL formatı: `https://<namespace>.gitlab.io/sdl-painter`
_(namespace: GitLab kullanıcı adın veya grup adın)_

**Yerel üretim:**

```bash
# Linux — sadece dokümantasyon
doxygen Doxyfile

# Linux — build + dokümantasyon birlikte
./scripts/build.sh --docs

# Tarayıcıda aç
xdg-open build/docs/index.html
```

```powershell
# Windows — sadece dokümantasyon
doxygen Doxyfile

# Windows — build + dokümantasyon birlikte
.\scripts\Build.ps1 -Docs

# Tarayıcıda aç
Start-Process build\docs\index.html
```

> Doxygen kurulu değilse: `apt install doxygen` (Linux) veya [doxygen.nl](https://www.doxygen.nl/download.html) (Windows).

## Kalite Kontrolü

```bash
# Format kontrolü
./scripts/format-check.sh

# Format otomatik düzeltme
./scripts/format-check.sh --fix

# clang-tidy (build sonrası)
find src/ -name '*.cpp' | xargs clang-tidy-18 -p build/linux-debug/
```

## CI/CD

GitLab CI pipeline aşamaları (tüm özellikler aktif: OpenGL + Vulkan + metin):

| Stage | Job | Platform | Açıklama |
|-------|-----|----------|----------|
| build | `build:linux:debug` | Linux (Docker) | Debug, Vulkan + metin dahil |
| build | `build:linux:release` | Linux (Docker) | Release, Vulkan + metin dahil |
| build | `build:windows:debug` | Windows (SaaS, MSVC) | Debug, Vulkan + metin dahil |
| build | `build:windows:release` | Windows (SaaS, MSVC) | Release, Vulkan + metin dahil |
| test | `test:unit` | Linux | GTest — headless (lavapipe + offscreen) |
| test | `test:unit:asan` | Linux | ASan + UBSan — headless |
| test | `test:unit:windows` | Windows | GTest — SDL offscreen |
| quality | `quality:clang-format` | Linux | Google Style format kontrolü (zorunlu) |
| quality | `quality:clang-tidy` | Linux | Static analysis (opsiyonel) |
| docs | `pages` | Linux | Doxygen → GitLab Pages (yalnızca `main`) |

## Mimari Kararlar (ADR)

Büyük tasarım kararları [Architecture Decision Records](adr/) formatında belgelenmiştir.

| ADR | Karar |
|-----|-------|
| [ADR-001](adr/ADR-001-opengl-33-core-profile.md) | OpenGL 3.3 Core Profile Seçimi |
| [ADR-002](adr/ADR-002-opengl-vulkan-dual-backend.md) | OpenGL + Vulkan Çift Backend Kararı |
| [ADR-003](adr/ADR-003-geometry-quad-line-thickness.md) | Kalın Çizgiler için Geometry Quad Yaklaşımı |
| [ADR-004](adr/ADR-004-tessellator-backend-agnostic.md) | Tessellator'ın Backend-Agnostic Tasarımı |
| [ADR-005](adr/ADR-005-stb-image-vs-sdl-image.md) | Image Yükleme — stb_image vs SDL_image |
| [ADR-006](adr/ADR-006-ear-clipping-triangulation.md) | Poligon Triangulation — Ear Clipping Seçimi |

## Roadmap

| Özellik | Durum |
|---------|-------|
| Vulkan Backend İyileştirme | Henüz eklenmedi |
| Basit uygulama örnekleri | Henüz eklenmedi |
| SDLPainter Skill.md | Henüz eklenmedi |
| Android desteği | Henüz eklenmedi |
| Path ve Bezier eğri desteği | Henüz eklenmedi |
| uEngine4 Concurrency desteği | Henüz eklenmedi |

## Lisans

MIT License — bkz. [LICENSE](LICENSE)
