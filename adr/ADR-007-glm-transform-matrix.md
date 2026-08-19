# ADR-007: Transform Matrisi — Kendi Implementasyon vs GLM

- **Durum:** Kabul edildi
- **Tarih:** 2026-07-03

## Bağlam

SDLPainter başlangıçta 2B affine dönüşümler için kendi 3x3 matris sınıfını
(`sdl_painter::Transform`, ~188 satır, **row-major** `float[3][3]`) kullanıyordu
(bkz. eski ADR notları). Bu, minimal 2B affine ihtiyacı için
yeterliydi ve harici bağımlılık gerektirmiyordu.

Ancak uzun vadede **3B / karmaşık matematik** hesaplamalarda bu kütüphaneyi kullanmayı hedefliyoruz: perspektif projeksiyon,
ters dönüşüm (inverse), hit-testing, yoğun vektör matematiği. Bu ihtiyaçlar için kendi
matris kodunu genişletmek, olgun ve test edilmiş bir kütüphaneyi yeniden icat etmek olacağı değerlendirildi.

| Kriter | Kendi `Transform` | GLM |
|--------|-------------------|-----|
| Kurulum | Sıfır bağımlılık, ~188 satır | Header-only, `glm/1.0.3` (Conan) |
| 2B affine | Yeterli | Yeterli (`glm::mat3`) |
| 3B / perspektif / inverse | Elle yazılmalı | Hazır, test edilmiş |
| Bellek düzeni | Row-major | **Column-major** (GPU standardı) |
| API sızıntısı | Yok | Public header'da `glm::mat3` |
| Bakım | Bize ait | Topluluk |

## Karar

**GLM** kullanılır. `Transform` sınıfı **tamamen kaldırıldı**; `RenderState` ve `Painter`
doğrudan `glm::mat3` tutar. `glm::glm` hedefi CMake'te **PUBLIC** link edilir (public
header'da `glm::mat3` göründüğü için zorunlu). Public API'de glm sızıntısı bilinçli olarak
kabul edildi.

## Gerekçe

- 3B/karmaşık matematik yatırımı: `glm::mat4`, `glm::inverse`, `glm::ortho`, quaternion vb.
  ihtiyaç doğduğunda hazır. Sınıfı ileride `glm::mat4`'e evriltmek kolaylaşır.
- GLM column-major düzeni GPU'nun (OpenGL/Vulkan) beklediği düzenle birebir uyumlu —
  eskiden gereken elle transpoze/repack işleri sadeleşiyor.
- Header-only: derleme/dağıtım maliyeti düşük.

## Sonuçlar

- **Row-major → column-major geçişi** üç noktada senkron değişiklik gerektirdi (en kritik risk):
  - OpenGL model matrisi upload'u `glUniformMatrix3fv(..., GL_TRUE, ...)` → **`GL_FALSE`**
    (`src/opengl/shader_program.cpp`) — GLM zaten GPU'nun beklediği düzeni verdiğinden
    transpose gerekmez.
  - Vulkan `SetModelMatrix` repack indeksleri column-major girdiye göre yeniden yazıldı
    (`src/vulkan/vulkan_renderer.cpp`): tx=`mat3[6]`, ty=`mat3[7]`.
  - `IRenderer::SetModelMatrix` dokümanı "column-major" olarak güncellendi.
- `Painter::Translate/Rotate/Scale` post-multiply semantiğini korur:
  `M = M * op`. Affine matrisler gtx/experimental'a bağlanmadan elle kurulur
  (public header'a `GLM_ENABLE_EXPERIMENTAL` sızmasını önlemek için).
- `Painter::UpdateProjection` değişmedi — zaten column-major ve backend Y-flip mantığını
  içeriyor (`glm::ortho` bu ayrımı tek çağrıyla veremez).
- `tests/test_transform.cpp`, `glm::mat3` davranışını ve column-major byte düzenini
  (`glm::value_ptr`) doğrulayan testlere dönüştürüldü.
