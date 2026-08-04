<div align="center">
  <img src="sdl-logo.png" alt="SDLPainter" width="120">
  <h1>SDLPainter</h1>
  <p><strong>SDL3 + OpenGL/Vulkan dual backend destekli C++17 2B çizim kütüphanesi.</strong></p>
  <p>
    <img src="https://img.shields.io/badge/C%2B%2B-17-blue" alt="C++17">
    <img src="https://img.shields.io/badge/SDL-3.2-green" alt="SDL3">
    <img src="https://img.shields.io/badge/OpenGL-3.3%20Core-orange" alt="OpenGL 3.3">
    <img src="https://img.shields.io/badge/Vulkan-1.1-red" alt="Vulkan 1.1">
    <img src="https://img.shields.io/badge/license-MIT-lightgrey" alt="MIT">
  </p>
</div>

![sdl-painter-overview](doc/sdl-painter-general-overview.png)

## Neden SDLPainter?

SDL3 ile 2B çizim yapmayı kolay bir hale getitir. Vertex üretimi, shader derleme, buffer ve state yönetimi,
pipeline kurulumu, OpenGL/Vulkan farklılıklar ile uğraşmak durumunda kalmazsınız.

SDLPainter **sadece çizime** odaklanmanıza imkan sağlar:

- **Tek API, çoklu backend** — Aynı kod OpenGL 3.3 ve Vulkan 1.1'de aynı sonucu üretir; backend'i değiştirmek tek satır.
- **Doğru geometri** — Kalın çizgiler `glLineWidth` yerine quad tabanlı (platformlar arası tutarlı), konkav çokgenler ear clipping ile doğru dolduruluyor.
- **Draw call'ları biriktirir** — `RenderBatcher` aynı mod/texture/opacity'deki çizimleri birleştirir; binlerce küçük şekil ucuzlar.
- **İsteğe bağlı uygulama çatısı** — `sdl_painter_app` ile pencere, olay döngüsü ve zamanlama da hazır gelir; istemezsen hiç kullanma.
- **Tanıdık API** — QPainter kullandıysanız, çoğu API tanıdık gelecektir. Duymadıysanız ya da kullanmadıysanız ise zaten aşikar API'ler: `DrawRect`, `FillCircle`, `Save`/`Restore`.

## Hızlı Başlangıç

```bash
# 1) Bağımlılıklar (ilk seferde: conan profile detect)
conan install . --output-folder=build/linux-debug/generators \
    --build=missing -s build_type=Debug

# 2) Derle
cmake --preset linux-debug
cmake --build --preset linux-debug

# 3) Testler + ilk demo
ctest --preset linux-debug
./build/linux-debug/examples/phase1_demo
```

Çizim kodu şuna benzer:

```cpp
sdl_painter::Painter painter(window, sdl_painter::RendererBackend::kOpenGL);

painter.Begin();
painter.Clear({30, 30, 30, 255});

painter.SetPen(sdl_painter::Pen({255, 0, 0, 255}, 2.0f));
painter.SetBrush(sdl_painter::Brush({100, 100, 255, 128}));
painter.DrawCircle(400, 300, 80);

painter.Save();
painter.Translate(400, 300);
painter.Rotate(45.0f);
painter.DrawRect(-50, -50, 100, 100);
painter.Restore();

painter.End();
```

Windows, script tabanlı derleme ve Vulkan'lı build için:
[Hızlı Başlangıç Rehberi](doc/hizli-baslangic.md).

## Örnek Ekran Görüntüleri

| | |
|:---:|:---:|
| ![Primitifler](doc/screenshots/primitifler.png) | ![Texture](doc/screenshots/texture.png) |
| **Temel primitifler** — stroke/fill, kalın çizgiler, konkav poligon (`phase1_demo`) | **Image / texture** — ölçekleme, atlas dilimleme, döndürme (`phase3_demo`) |
| ![Metin](doc/screenshots/metin.png) | ![Uygulama](doc/screenshots/uygulama.png) |
| **Metin** — SDL_ttf, hizalama, rect içine yerleştirme (`phase4_demo`) | **Uygulama çatısı** — `sdl_painter_app` ile oyun döngüsü (`phase7_game_demo`) |

Tüm demoların ne yaptığı: [Örnekler Rehberi](doc/sdl-painter-ornekler.md).

## Özellikler

| Alan | Desteklenen |
|------|-------------|
| **Primitifler** | Çizgi, dikdörtgen, daire, elips, çokgen, polyline — hepsi stroke + fill |
| **Stiller** | Pen (renk, kalınlık, outline), Brush (dolgu rengi), global opacity |
| **Transform** | `Translate` / `Rotate` / `Scale`, `Save`/`Restore` yığını — QPainter semantiği |
| **Clipping** | Scissor tabanlı dikdörtgen kırpma |
| **Image** | PNG / JPG yükleme (stb_image), kaynak→hedef ölçekleme, alpha blending |
| **Metin** | SDL_ttf 3.x, glyph cache, left/center/right hizalama |
| **Backend** | OpenGL 3.3 Core ve Vulkan 1.1 — `IRenderer` ile değiştirilebilir |
| **Platform** | Linux (GCC/Clang), Windows (MSVC), Linux'ta MinGW cross-compile |

Ayrıntılı liste: [Özellik Listesi](doc/sdl-painter-ozellikler.md).

## Mimari

![SDLPainter mimarisi](doc/sdl-painter-architecture.png)

Beş katman, her biri tek bir sorumluluğa odaklı:

1. **Application** *(opsiyonel)* — pencere, olay döngüsü, zamanlama ([ADR-008](adr/ADR-008-application-framework-layer.md))
2. **Painter** — public API; çizim komutlarını toplar ve state'e göre işler
3. **RenderState + Tessellator** — transform/pen/brush/opacity/clip yığını; şekilleri vertex'e çevirir. Transform matrisi 3×3 affine `glm::mat3`, column-major ([ADR-007](adr/ADR-007-glm-transform-matrix.md))
4. **RenderBatcher → IRenderer** — draw call'ları birleştirir, backend'e gönderir
5. **Backend** — `OpenGLRenderer` / `VulkanRenderer`, altta SDL3 platform katmanı

Yeni bir backend eklemek için yalnızca `IRenderer` implement edilir; Painter kodu değişmez.
Detay: [Mimari Genel Bakış](doc/mimari-genel-bakis.md) · [Backend İç Yapısı](doc/backend-ic-yapisi.md) · [ADR'ler](adr/)

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

Platform bazlı adımlar, CMake preset/seçenek tabloları ve Vulkan'lı derleme
için: **[Hızlı Başlangıç Rehberi](doc/hizli-baslangic.md)**

| Konu | Nerede |
|------|--------|
| Windows (VS 2022) manuel derleme | [§2.1](doc/hizli-baslangic.md#21-windows-visual-studio-2022--manuel) |
| Script ile derleme (Linux + Windows) | [§2.2](doc/hizli-baslangic.md#22-script-ile-derleme) |
| CMake preset referansı (7 preset) | [§2.3](doc/hizli-baslangic.md#23-cmake-preset-referansı) |
| CMake seçenekleri + Vulkan'lı derleme | [§2.4](doc/hizli-baslangic.md#24-cmake-seçenekleri) |
| Sık karşılaşılan sorunlar | [§7](doc/hizli-baslangic.md#7-sık-karşılaşılan-sorunlar) |

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

# Application çatısı demo'ları (bkz. ADR-008)
./build/linux-debug/examples/phase6_app_demo
./build/linux-debug/examples/phase7_game_demo

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

### Bind mount söz dizimi — kabuğa göre değişir

Aşağıdaki komutlar proje dizinini konteynerdeki `/workspace`'e bağlar.
**Mount ifadesinin yazımı kullandığın kabuğa göre değişir**; yanlış kabukta
kopyalanan komut sessizce yanlış dizini bağlar veya hata verir:

| Kabuk | Doğru yazım | Yanlış yazımda ne olur |
|-------|-------------|------------------------|
| Linux / macOS | `-v $(pwd):/workspace` | — |
| Windows PowerShell | `-v "${PWD}:/workspace"` (tırnak **zorunlu**) | Tırnaksız `$(pwd):/workspace` iki ayrı argümana bölünür, `:/workspace` imaj adı sanılır → `docker: invalid reference format` |
| Windows Git Bash | `MSYS_NO_PATHCONV=1 docker run ... -v "$(pwd):/workspace"` | MSYS, konteyner içindeki `/workspace` yolunu `C:/Program Files/Git/workspace`'e çevirir; mount yanlış dizine bağlanır ve konteyner içinde `CMakePresets.json bulunamadı` hatası alınır |

Aşağıdaki örnekler Linux/macOS söz dizimindedir. Windows'ta yukarıdaki
tabloya göre uyarla — PowerShell akışlarının tamamı için
[Docker Kullanım Kılavuzu](doc/docker.md).

### Geliştirme shell'i

```bash
docker run --rm -it -v $(pwd):/workspace sdl-painter:dev bash
```

```powershell
docker run --rm -it -v "${PWD}:/workspace" sdl-painter:dev bash
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

MinGW hedefinin kendi output-folder'ı ve preset'i vardır — Linux preset'i
kullanılmaz. Conan profilinde `os=Windows` ve MinGW derleyicileri tanımlı
olduğu için üretilen `conan_toolchain.cmake` yeterlidir; ayrıca bir
`-DCMAKE_TOOLCHAIN_FILE` vermeye gerek yoktur.

```bash
docker run --rm -v $(pwd):/workspace sdl-painter:windows-cross bash -c \
  "conan install . --output-folder=build/windows-mingw-debug/generators \
     --build=missing -s build_type=Debug \
     --profile:build=default \
     --profile:host=windows-mingw && \
   cmake --preset windows-mingw-debug && \
   cmake --build --preset windows-mingw-debug"
```

> Bu hedefte Vulkan desteklenmez — `vulkan-loader` recipe'ı Windows'ta
> `USE_MASM`'i zorunlu kılıyor ve MinGW gcc bunu derleyemiyor.
> `conanfile.py` `configure()` bu hedefte `with_vulkan`'ı otomatik kapatır.
> Windows'ta Vulkan için native MSVC build (`windows-release` preset) kullan.

Ayrıntılı kullanım akışları ve dağıtım adımları için:
[Docker Kullanım Kılavuzu](doc/docker.md) · [Docker Hub'a Dağıtım](doc/docker-hub-deployment.md)

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
| [Docker Kullanım Kılavuzu](doc/docker.md) | İmaj hiyerarşisi, aşama açıklamaları, CI entegrasyonu, Dockerfile.windows |
| [Docker Hub'a Dağıtım](doc/docker-hub-deployment.md) | İmaj build/push adımları, Hub ve GitLab Registry, otomatik CI akışı |

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
| [ADR-007](adr/ADR-007-glm-transform-matrix.md) | Transform Matrisi için GLM Kullanımı |
| [ADR-008](adr/ADR-008-application-framework-layer.md) | Application Çatısı Katmanı |

## Lisans

MIT License — bkz. [LICENSE](LICENSE)
