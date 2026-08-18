# ADR-004: Tessellator'ın Backend-Agnostic Tasarımı

- **Durum:** Kabul edildi
- **Tarih:** 2026-04-17

## Bağlam

2B şekillerin (daire, poligon, çizgi) GPU'ya gönderilmesi için vertex üretimi gerekmektedir, daha önceki painter örneklerinde bunlar her bir şekil için ayrı ayrı yapılmaktaydı (uEngine'de de). Hem vulkan hem de modern opengl geçişi ile alternatif bir yöntem değerlendirilecektir.

### Alternatifler

| Yaklaşım | Açıklama | Sorun |
|----------|----------|-------|
| Rendererlar içinde tessellate | Her backend kendi vertex'ini üretir | Kod tekrarı; OpenGL ve Vulkan aynı geometriyi iki kez yazar |
| -> **Ayrı Tessellator** | Backend-agnostic vertex üretimi | <- **Seçilen yol** |
| GPU tessellation shader | OpenGL 4.0+ tessellation shader | 3.3'te yok; Vulkan'da karmaşık |

## Karar

`Tessellator` sınıfı **backend bilmez**; sadece `std::vector<Vertex>` ve `std::vector<TexturedVertex>` üretir. `IRenderer` bu vektörleri alır.

## Gerekçe

- OpenGL → Vulkan geçişinde tessellation kodu sıfır değişiklik.
- Testler: Tessellator birim testleri GPU gerektirmez (sadece vertex koordinatları doğrulanır).
- Tek sorumluluk: Tessellator geometri bilir, renderer API bilir.
- `IRenderer::DrawTriangles(vertices, color)` — sözleşme minimal, stabil.

## Arayüz

```cpp
// Tessellator sadece vertex üretir, renderer bilmez:
class Tessellator {
 public:
  static std::vector<Vertex> TessellateFilledCircle(float cx, float cy,
                                                     float radius);
  static std::vector<Vertex> TessellateThickLine(float x1, float y1,
                                                  float x2, float y2,
                                                  float width);
  static std::vector<Vertex> TessellateFilledPolygon(
      const std::vector<Point>& points);
  // ...
};

// Renderer sadece triangle listesi alır:
renderer_->DrawTriangles(vertices, pen_.color());
```

## Sonuçlar

- Tessellator `src/tessellator.h/cpp` — internal, public API değil.
- Her shape metodu statik; state tutmaz.
- Vulkan backend için tessellator dosyaları değişmez.
