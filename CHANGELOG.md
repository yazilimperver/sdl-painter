# Changelog

## [Yayınlanmadı]

### Değişti
- **Örnek uygulamalar anlamlı isimler aldı:** `phase0_demo` → `hello_window`,
  `phase1_demo` → `primitives`, `phase2_demo` → `transforms`,
  `phase2b_demo` → `clipping`, `phase3_demo` → `images`, `phase4_demo` → `text`,
  `phase5a–5e_vulkan_*` → `vulkan_clear` / `vulkan_triangles` /
  `vulkan_textured` / `vulkan_demo` / `vulkan_text`,
  `phase6_app_demo` → `app_basics`, `phase7_game_demo` → `game_loop`,
  `phase8_tictactoe` → `tictactoe`. Faz numaraları projeyi içeriden bilmeyenler için pek bir şey 
  ifade etmiyordu. Demoları adıyla çalıştıran veya `--target` ile
  derleyen herkesi etkiler; kütüphane API'si değişmedi.
  Eski→yeni eşleme tablosu `examples/README.md` içinde ve her örneğin dosya
  başı yorumunda korunuyor (özellikle yazılarımda değindiğim için bu şekilde bıraktım). Pencere başlıkları da yeni adı öne alacak şekilde
  güncellendi, faz numarası parantez içinde duruyor
  (ör. `SDLPainter — vulkan_triangles (Phase 5b)`).
- **README yeniden yapılandırıldı:** `README.md` 550 → 242 satır. Derleme,
  script referansı, Docker, dizin yapısı, kalite kontrolleri ve CI/CD bölümleri
  `doc/building.md`, `doc/scripts.md` ve `doc/development.md`'ye taşındı
  (silinmedi); ADR tablosu `adr/README.md`'ye. `README.tr.md` aynı yapıya
  çekildi. Yeni bölümler: "Is SDLPainter for you?" ve
  "SDL_Renderer vs SDLPainter".

### Eklendi
- **`examples/README.md`:** Her demonun ne gösterdiği, gerektirdiği bağımlılık
  (Vulkan / SDL_ttf), çalıştırma satırı ve faz eşleme tablosu.
- **`hero` örneği + GIF üretim script'leri:** README tanıtım görseli için
  koreografili, 8 saniyede tam bir periyot tamamlayan (yani kusursuz döngü
  yapan) sahne. `--dump-frames` ile gizli pencereden 240 PPM karesi yazıyor;
  `scripts/make-hero-gif.sh` / `Make-HeroGif.ps1` bunları iki geçişli palet ile
  GIF'e (isteğe bağlı mp4'e) çeviriyor. Ekran kaydına göre imleç/pencere
  çerçevesi karışmıyor ve kare atlaması olmuyor.
- **`doc/getting-started.md` ve `doc/architecture.md`:** `hizli-baslangic.md` ve
  `mimari-genel-bakis.md` dokümanlarının İngilizce sürümleri. Türkçe
  orijinaller yerinde; her iki tarafa karşılıklı dil değiştirici satırı eklendi.
- **`doc/building.md`, `doc/scripts.md`, `doc/development.md`, `adr/README.md`:**
  README'den taşınan içerik + eksik olanlar (`changelog-section.sh`, presetsiz
  CMake derlemesi, CI'ın shader tazelik ve paketleme job'ları).

### Düzeltildi
- **CI artifact glob'ları sessizce boş artifact üretecekti:** `examples/phase*`
  deseni yeni adlarla eşleşmiyordu ve GitHub tarafında `if-no-files-found: warn`
  olduğu için pipeline yeşil kalırdı. `examples/*` + `CMakeFiles`/`*.cmake`
  dışlaması olarak düzeltildi (`.github/workflows/ci.yml`, `.gitlab-ci.yml`).
- **README'nin CI/CD tablosu yanlıştı:** `quality:clang-format` "soft fail"
  yazıyordu; Faz 1.5'te hard-fail'e çevrilmişti. `build:standalone-cmake`,
  `package:conan-create` ve `quality:shader-freshness` job'ları da tabloda
  yoktu. Doğru hâli `doc/development.md`'de.
- **`doc/hizli-baslangic.md`** artık var olmayan `README.tr.md#script-referansı`
  bölümüne link veriyordu → `doc/scripts.md`.
- **`doc/hizli-baslangic.md`'deki klonlama komutu placeholder URL kullanıyordu**
  (`https://example.com/sdl-painter.git`) → gerçek repo adresi. Aynı dosyadaki
  Vulkan opsiyonu `-o sdl_painter/*:with_vulkan=True` yerine projenin her yerde
  kullandığı `-o "&:with_vulkan=True"` biçimine getirildi.
- **`doc/mimari-genel-bakis.md` var olmayan `transform.h`'yi listeliyordu**
  (ADR-007 ile `glm::mat3`'e geçilmişti). Bağımlılık grafiğinde SDL_ttf hâlâ
  opsiyonel görünüyordu ve GLM hiç yoktu — ikisi de zorunlu listeye alındı.

## [1.1.0] - 2026-08-09

### Değişti
- **Shader'lar binary'ye gömüldü:** GLSL kaynakları ve derlenmiş SPIR-V modülleri
  artık kütüphane binary'sine gömülüyor; çalışma zamanında hiçbir shader dosyası
  aranmıyor. Daha önce shader dizini ya kaynak ağacı yolu olarak binary'ye
  gömülüyor ya da `SDL_GetBasePath()` ile executable'ın yanında aranıyordu — bu,
  paketlenmiş kütüphaneyi kullanan tüketicinin shader dosyalarını kendi çıktı
  dizinine elle kopyalamasını gerektiriyordu (bkz. ADR-009).
- **Vulkan SDK artık derleme için gerekli değil:** SPIR-V çıktıları
  `src/vulkan/shaders/spirv/` altında repo'da tutuluyor. `glslc` yalnızca shader
  kaynağını değiştirenler için, `-DSDLPAINTER_REGENERATE_SHADERS=ON` ile
  gelen `regenerate_shaders` hedefinde aranıyor.
- **`VulkanRenderer::Initialize()` pipeline hatasında artık başarısız oluyor:**
  Eskiden uyarı loglayıp devam ediyor, kullanıcıya sebebi log'a gömülü boş bir
  pencere bırakıyordu. Shader dosyaları artık eksik olamayacağı için bu sessiz
  degradasyonun gerekçesi kalmadı; hata `Painter::IsValid() == false` olarak
  yukarı taşınıyor.
- **Sürüm sabitleri makrodan `constexpr`'a taşındı:** `SDLPAINTER_VERSION_*`
  makroları kaldırıldı; yerine `sdl_painter::kVersionMajor/Minor/Patch` ve
  `kVersionString` geldi. Sürüm artık `#if` içinde kullanılamaz; tüketici
  tarafında sürüm kısıtı `find_package(sdl_painter 1.1 CONFIG REQUIRED)` ile
  ifade edilir.

### Eklendi
- **CMake kurulum/export katmanı:** Kütüphane artık `cmake --install` ile
  kurulabiliyor ve dışarıdan tüketilebiliyor:
  ```cmake
  find_package(sdl_painter CONFIG REQUIRED)
  target_link_libraries(my_app PRIVATE sdl_painter::sdl_painter)
  ```
  `FetchContent`/`add_subdirectory` ile ekleyen tüketici de **aynı hedef ismini**
  kullanır (`sdl_painter::sdl_painter`, opsiyonel uygulama çatısı için
  `sdl_painter::app`). Örnekler, testler ve Doxygen hedefi yalnızca SDLPainter
  üst proje iken varsayılan olarak açık.
- **Başsız Vulkan desteği (`VK_EXT_headless_surface`):** SDL, offscreen/dummy
  video sürücüsünde surface'i bu extension ile oluşturur ama onu
  `SDL_Vulkan_GetInstanceExtensions()` listesinde bildirmez; sonuç, GPU'suz
  ortamda `SDL_Vulkan_CreateSurface`'in başarısız olmasıydı. Extension mevcutsa
  `VkContext` artık kendisi ekliyor. Böylece Vulkan backend'i CI ve WSL2 gibi
  ekransız ortamlarda gerçekten test edilebiliyor. Extension bulunmayan
  platformlarda sessizce atlanır, masaüstü davranışı değişmez.
- **Renderer smoke testleri (`tests/test_renderer_smoke.cpp`):** Gerçek bir
  backend ayağa kaldırıp gömülü shader'ların sürücü tarafından kabul edildiğini
  doğrular. Daha önce hiçbir test gerçek bir renderer örneklemiyordu; shader'lar
  test kapsamı dışındaydı. Pencere/context kurulamayan ortamlarda testler
  başarısız olmak yerine atlanır.

- **SPIR-V güncellik kontrolü:** `cmake -P cmake/CheckShaderFreshness.cmake`,
  GLSL kaynakları değişip `.spv` çıktıları yeniden üretilmediğinde hata verir.
  Karşılaştırma `sources.sha256` manifesti üzerinden yapıldığı için `glslc`
  veya Vulkan SDK gerektirmez. CI'da `quality:shader-freshness` job'ı koşar.
- **CI:** Windows'ta artık `ctest` çalışıyor (daha önce yalnızca derleniyordu);
  `build:standalone-cmake` (preset'siz derleme + kurulum + tüketici doğrulaması)
  ve `package:conan-create` job'ları eklendi; clang-format kontrolü zorunlu
  hâle getirildi.

- **Uygulama çatısı (`sdl_painter_app`):** SDL pencere/olay-döngüsü
  boilerplate'ini soyutlayan ayrı static kütüphane. `sdl_painter::Application`'dan
  türeyip `OnRender`/`OnUpdate`/`OnKeyDown` gibi sanal metotları override etmek
  yeterli; SDL init, pencere, GL context, olay döngüsü ve yıkım çatı tarafından
  yönetilir. SDL'den bağımsız `Key`/`KeyEvent`/`MouseButtonEvent` tipleri ve ileri
  kullanım için `OnRawEvent(const SDL_Event&)` kaçış kapısı sunar. Core
  `sdl_painter` saf çizim API'si olarak korunur (bkz. ADR-008).
- **Zamanlama modları:** `AppConfig::timing` ile `kVariable` (varsayılan, değişken
  delta-time) veya `kFixed` (sabit adımlı deterministik `OnUpdate` +
  `OnRender(Painter&, alpha)` interpolasyonu — Game Programming Patterns "play
  catch up"). Ayrıca `target_fps` ile `SDL_DelayNS` tabanlı kare hızı freni.
- **Örnekler:** `phase6_app_demo` (kVariable), `phase7_game_demo` (kFixed +
  interpolasyon, seken top) ve `phase8_tictactoe` (fare girdisi + durum makinesi)
  çatının kullanımını gösterir.
- **Dokümantasyon:** Ana `README.md` İngilizce oldu; Türkçe sürüm `README.tr.md`
  olarak ayrıldı.

### Düzeltildi
- **Conan paketi doğru üretiliyor:** `package()` artık binary glob'lamak yerine
  projenin install kurallarını kullanıyor (`cmake.install()`), `LICENSE` pakete
  kopyalanıyor ve GTest artifact'ları pakete sızmıyor. `gtest` bağımlılığı
  `test_requires`'a alındı, böylece tüketicinin bağımlılık grafiğine girmiyor.
  `test_package` çalıştırma yolu düzeltildi.
- **lavapipe ICD yolu:** CI ve Dockerfile `lvp_icd.x86_64.json` yolunu sabit
  yazıyordu; mesa sürümüne göre dosya adı `lvp_icd.json` olabiliyor ve yanlış
  yol verildiğinde Vulkan loader hiçbir sürücü yüklemiyor — Vulkan testleri
  sessizce atlanıyordu. Yol artık çalışma anında çözülüyor.
- **Alt proje olarak eklenince yanlış dizin kullanımı:** `cmake/Docs.cmake`,
  `cmake/Doxyfile.in`, `cmake/InsourceGuard.cmake` ve `tests/CMakeLists.txt`
  `CMAKE_SOURCE_DIR` kullanıyordu; `add_subdirectory` ile eklendiğinde bu
  tüketicinin kök dizinini gösterip configure'u kırıyordu. `PROJECT_SOURCE_DIR`
  ile değiştirildi.
- **MinGW link seçenekleri dizin kapsamındaydı:** `add_link_options()` ile
  verilen `-static-libgcc -static-libstdc++`, projeyi alt proje olarak ekleyen
  tüketicinin kendi hedeflerini de etkiliyordu. Hedef kapsamına alındı.

### Kaldırıldı
- `sdlpainter_copy_shaders()` / `sdlpainter_copy_vulkan_shaders()` CMake
  yardımcıları ve bunların MSVC paralel-build race condition'ı için gereken
  merkezi kopyalama target'ları — shader'lar gömülü olduğu için gereksiz.
- `VulkanRenderer::ResolveShaderDir()`; `VulkanPipeline` ve
  `VulkanTexturedPipeline` `Init()` imzalarından `shader_dir` parametresi.

## [1.0.0] - 2026-05-17

İlk yayın. Önceki commit'ler.

### Eklendi
- **CI/CD — Windows build:** GitLab SaaS MSVC runner üzerinde Debug + Release
  Windows derlemesi pipeline'a dahil edildi.
- **Docker — Windows cross imajı:** MinGW tabanlı `windows-cross` stage
  yeniden düzenlendi; eksik dosyalar eklendi.
- **CI/CD — Conan önbellek paylaşımı:** Pipeline aşamaları arasında Conan
  paket önbelleği artifact olarak aktarılıyor; tekrar indirme süresi azaldı.
- **Painter API:** QPainter benzeri yüksek seviye 2B çizim arayüzü
  (`SetPen`, `SetBrush`, `DrawLine`, `DrawRect`, `FillRect`, `DrawCircle`,
  `FillCircle`, `DrawEllipse`, `FillEllipse`, `DrawPolygon`, `FillPolygon`,
  `DrawPolyline`, `DrawImage`, `DrawText`).
- **Transform stack:** `Save`/`Restore` ile state yönetimi; `Translate`,
  `Rotate`, `Scale`, `ResetTransform` 3×3 affine matris üzerinden.
- **Backend soyutlama:** `IRenderer` arayüzü; yeni backend eklemek için
  yalnızca bu arayüzü implement etmek yeterli.
- **OpenGL backend:** OpenGL 3.3 core profile, GLAD loader, custom shader
  pipeline (basic + textured).
- **Vulkan backend:** Vulkan 1.1, manuel pipeline yönetimi, frame
  synchronization, SPIR-V shader derleme (offline `glslc`).
- **Tessellator:** Backend-agnostic vertex üretimi; daire/elips adaptif
  segment sayısı, konkav poligonlar için ear-clipping triangulation,
  geometry-based quad kalın çizgi.
- **Render batcher:** Aynı state ile gelen draw call'ları tek GPU çağrısına
  birleştirir.
- **Görsel desteği:** stb_image üzerinden PNG/JPG yükleme; RAII `Texture`
  sarmalayıcı; tint ile çizim.
- **Metin:** SDL_ttf 3.x üzerinden glyph cache ve hizalama (left/center/right).
- **Clip:** Scissor tabanlı dikdörtgensel kırpma.
- **Opaklık:** Global opacity (`SetOpacity`) shader uniform'u üzerinden.
- **Build sistemi:** CMake 3.20+ presets (linux/windows × debug/release/asan)
  ve Conan 2 paket yönetimi.
- **CI/CD:** GitLab pipeline — Linux (Debug/Release/ASan) + Windows
  (Debug/Release) build, GTest unit test koşumu, clang-format zorunlu kontrol,
  clang-tidy (soft fail), Doxygen → GitLab Pages.
- **Docker:** `builder`, `ci`, `windows-cross` (MinGW) stage'leri.
- **Dokümantasyon:** Doxygen yapılandırması, doxygen-awesome-css teması,
  Türkçe teknik dokümantasyon (`doc/`) ve karar kayıtları (`adr/`).
- **Test:** GTest birim testleri (color, geometry, pen/brush, transform,
  tessellator, image, render batcher, texture upload).
- **Örnekler:** `phase0`–`phase5e` demo uygulamaları (OpenGL ve Vulkan).

### Değişti
- **SDL_ttf zorunlu bağımlılık:** `with_text` Conan opsiyonu kaldırıldı;
  SDL_ttf 3.x artık her zaman derlenir. Metin desteği opsiyonel değil.
- **Vulkan altyapısı:** `vk_context`, `vk_swapchain`, `vk_frame_sync`
  bileşenlerinde kararlılık düzeltmeleri uygulandı.

### Düzeltildi
- Derleyici uyarıları (`-Wall -Wextra`) tamamen giderildi.
- clang-format-18 ve clang-tidy-18 uyumsuzlukları çözüldü.

### Bilinen Eksikler
- Path, Bezier eğrileri ve gradient desteklenmez (v2 kapsamı).
- Kırpma yalnızca dikdörtgenseldir (scissor); path-based kırpma yoktur.
- Anti-aliasing yalnızca MSAA (donanım/sürücü ayarına bağlıdır).
- Tek thread'den kullanılması beklenir.
- Test ve coverage çıktıları henüz gitlab'a verilmiyor.

### Bağımlılıklar
- C++17, CMake ≥ 3.20, Conan ≥ 2.0
- SDL 3.2, SDL_ttf 3.2, GLAD 0.1.36, stb (cci.20240531), spdlog 1.15
- Opsiyonel: Vulkan loader/headers 1.3.290
