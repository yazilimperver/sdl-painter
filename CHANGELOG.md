# Changelog

## [Yayınlanmadı]

## [1.1.0] - 2026-08-08

### Eklendi
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
