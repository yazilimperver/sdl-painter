# Changelog

Bu projenin tüm önemli değişiklikleri bu dosyada belgelenir.

Format [Keep a Changelog 1.1.0](https://keepachangelog.com/tr-TR/1.1.0/),
sürümlendirme [Semantic Versioning 2.0.0](https://semver.org/lang/tr/) standartlarına uyar.

## [Unreleased]

## [0.1.0] - 2026-05-11

İlk yayın. v1 kapsamı QPainter'a benzer temel 2B çizim API'sini
sunar; ileri seviye özellikler (path, gradient, bezier) v2 için planlıdır.

### Eklendi
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

### Bilinen Sınırlamalar
- Path, Bezier eğrileri ve gradient desteklenmez (v2 kapsamı).
- Kırpma yalnızca dikdörtgenseldir (scissor); path-based kırpma yoktur.
- Anti-aliasing yalnızca MSAA (donanım/sürücü ayarına bağlıdır).
- Tek thread'den kullanılması beklenir.

### Bağımlılıklar
- C++17, CMake ≥ 3.20, Conan ≥ 2.0
- SDL 3.2, SDL_ttf 3.2, GLAD 0.1.36, stb (cci.20240531), spdlog 1.15
- Opsiyonel: Vulkan loader/headers 1.3.290

[Unreleased]: https://gitlab.com/yazilimperver/sdl-painter/-/compare/v0.1.0...main
[0.1.0]: https://gitlab.com/yazilimperver/sdl-painter/-/tags/v0.1.0
