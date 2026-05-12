# SDLPainter — Özellik Listesi

## Çizim Primitifleri

- Çizgi (`DrawLine`)
- Dikdörtgen — stroke ve fill (`DrawRect` / `FillRect`)
- Daire — stroke ve fill (`DrawCircle` / `FillCircle`)
- Elips — stroke ve fill (`DrawEllipse` / `FillEllipse`)
- Çokgen — stroke ve fill (`DrawPolygon` / `FillPolygon`)
- Polyline (`DrawPolyline`)

## Görsel Stiller

- **Pen:** renk + kalınlık (geometry-based quad; `glLineWidth` kullanılmıyor)
- **Brush:** dolgu rengi
- **Opacity:** global saydamlık `[0.0, 1.0]`

## Transform Stack

- `Translate`, `Rotate`, `Scale`
- `Save` / `Restore` — QPainter ile birebir aynı semantik
- `ResetTransform`
- 3×3 affine matris — GLM bağımlılığı yok, özel implementasyon

## Clipping

- Scissor-based rect clip (`SetClipRect` / `ClearClip`)

## Image / Texture

- PNG ve JPG yükleme (stb_image)
- Tam kaynak rect → hedef rect ölçekleme
- Alpha blending desteği

## Metin (Phase 4)

- SDL_ttf 3.x üzerinden font yükleme
- Glyph cache
- Alignment: left / center / right
- Rect içine metin yerleştirme

## Tessellator

- Backend'den bağımsız vertex üretimi
- Daire/elips için adaptif segment sayısı: `max(16, int(radius * 0.5f))`
- Konkav çokgenler için ear clipping triangulation

## Render Batcher

Her çizim çağrısı doğrudan GPU'ya gitmez; `RenderBatcher` vertex'leri bir arabellekte biriktirir ve yalnızca mod, texture veya opacity değiştiğinde (ya da 8192 vertex limiti dolduğunda) `Flush()` ile toplu olarak renderer'a iletir. Bu yaklaşım, özellikle çok sayıda küçük şeklin ard arda çizildiği sahnelerde draw call sayısını ciddi ölçüde azaltır.

## Backend Desteği

- **OpenGL 3.3 Core** — GLAD loader, GLSL shader'lar
- **Vulkan 1.1** — SPIR-V pipeline (Phase 5, opsiyonel)
- `IRenderer` arayüzü — yeni backend eklemek için tek implementasyon noktası

## Platform Desteği

- Linux (GCC / Clang, Ninja)
- Windows (MSVC, Visual Studio 2022)
- Linux container içinde Windows cross-compile (MinGW-w64)

## Altyapı

- CMake Presets (5 preset: `linux-debug`, `linux-release`, `linux-debug-asan`, `windows-debug`, `windows-release`)
- Conan 2 bağımlılık yönetimi — opsiyonel Vulkan ve metin bağımlılıkları
- Docker multi-stage: geliştirme / headless CI / cross-compile
- GitLab CI/CD: build → test → quality (clang-format zorunlu, clang-tidy soft)
- AddressSanitizer + UBSan preset (`linux-debug-asan`)
- Google C++ Style, clang-format-18
- Birim testler (gtest)