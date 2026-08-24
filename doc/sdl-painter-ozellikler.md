# SDLPainter — Özellik Listesi

Bu sayfa içerisinde SDLPainter kütüphanesine yönelik önce çıkan özellikler listelenecektir.

## Çizim Primitifleri

- Çizgi (`DrawLine`)
- Dikdörtgen — stroke ve fill (`DrawRect` / `FillRect`)
- Yuvarlatılmış dikdörtgen — stroke ve fill
  (`DrawRoundedRect` / `FillRoundedRect`)
- Daire — stroke ve fill (`DrawCircle` / `FillCircle`)
- Elips — stroke ve fill (`DrawEllipse` / `FillEllipse`)
- Yay (`DrawArc`), dilim (`DrawPie` / `FillPie`), kiriş
  (`DrawChord` / `FillChord`) — açı birimi derece, 0° = +x
- Çokgen — stroke ve fill (`DrawPolygon` / `FillPolygon`)
- Polyline (`DrawPolyline`)

## Görsel Stiller

- **Pen:** renk + kalınlık (geometry-based quad; `glLineWidth` kullanılmıyor),
  kesik deseni (`SetDashPattern`), uç stili (butt/square/round), birleşim
  stili (miter/bevel/round — miter sınırı aşılınca kendiliğinden bevel'a düşer)
- **Brush:** düz dolgu rengi veya iki uçlu doğrusal/ışınsal gradient.
  Gradient shader gerektirmez: geçiş köşe renklerine yazılır, enterpolasyonu
  donanım yapar — bu yüzden **batch'i kırmaz**, ama hassasiyeti şeklin köşe
  yoğunluğu kadardır
- **Opacity:** global saydamlık `[0.0, 1.0]`
- **Karıştırma modu:** `kAlpha` / `kAdditive` / `kMultiply` / `kNone`.
  GPU durumu olduğu için batch'i kırar; Vulkan'da mod başına pipeline varyantı
  başlangıçta üretilir

## Transform Stack

- `Translate`, `Rotate`, `Scale`
- `Save` / `Restore`
- `ResetTransform`
- 3×3 affine matris — `glm::mat3` (column-major), bkz. ADR-007

## Clipping

- Scissor-based rect clip (`SetClipRect` / `ClearClip`)

## Viewport

- `SetViewport` / `ResetViewport` — çizimi bir alt dikdörtgenle sınırlar ve
  koordinatları ona yerelleştirir (bölünmüş ekran, mini harita)
- Kırpmadan farkı: kırpma koordinat sistemini değiştirmeden maskeler,
  viewport sistemin kendisini yeniden tanımlar

## Image / Texture

- PNG ve JPG yükleme (stb_image)
- Tam kaynak rect → hedef rect ölçekleme (atlas dilimleme)
- Renk tonlaması (tint) ve aynalama (`ImageFlip`) — ikisi de vertex'te
  taşındığı için batch'i kırmaz
- Örnekleme filtresi: `kLinear` (varsayılan) / `kNearest` (piksel sanatı)
- `Painter::UpdateImage` ile yerinde doku güncelleme (her karede değişen
  prosedürel dokular)
- `Painter::DrawImageMesh` ile serbest biçimli dokulu ızgara — köşeler
  bağımsız hareket edebilir
- Alpha blending desteği

## Metin

- SDL_ttf 3.x üzerinden font yükleme
- Glyph cache
- Alignment: left / center / right
- Rect içine metin yerleştirme
- Çok satırlı metin (satır sonu karakteri) ve `Font::LineHeight()`
- Sözcük kaydırma (`TextWrap::kWord`) — UTF-8 güvenli; `CountTextLines()` ile
  çizmeden satır sayısı

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

- CMake Presets (7 preset: `linux-debug`, `linux-release`, `linux-debug-asan`, `windows-debug`, `windows-release`, `windows-mingw-debug`, `windows-mingw-release`)
- Conan 2 bağımlılık yönetimi — opsiyonel Vulkan ve metin bağımlılıkları
- Docker multi-stage: geliştirme / headless CI / cross-compile
- GitLab CI/CD: build → test → quality (clang-format zorunlu, clang-tidy soft)
- AddressSanitizer + UBSan preset (`linux-debug-asan`)
- Google C++ Style, clang-format-18
- Birim testler (gtest)