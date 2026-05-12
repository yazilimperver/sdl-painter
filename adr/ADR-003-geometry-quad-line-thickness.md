# ADR-003: Kalın Çizgiler için Geometry Quad Yaklaşımı

- **Durum:** Kabul edildi
- **Tarih:** 2026-04-17

## Bağlam

2D çizim kütüphanesi kalınlığı ayarlanabilir çizgiler çizme ihtiyacı olabilmektedir. OpenGL'in `glLineWidth` API'si var; neden kullanılmasın?

### `glLineWidth` Sorunları

- OpenGL Core Profile'da `glLineWidth(x > 1.0f)` **deprecated**, yani artık kullanılmıyor; sadece 1.0 garantili.
- Desteklenen maksimum kalınlık sürücüye göre değişir: bazı GPU'larda yalnızca 1px.
- Çizgi uçları (line cap) ve birleşim noktaları (line join) kontrol edilemez.

## Karar

Kalın çizgiler **geometry-based quad** olarak tessellate edilir; `glLineWidth` hiçbir koşulda kullanılmaz.

## Gerekçe

Geometry quad yaklaşımı:
- Platform bağımsız, tutarlı piksel genişliği.
- Hem OpenGL hem Vulkan backend'de aynı vertex verisi kullanılır.
- Çizgi ucu (butt/round/square) ve birleşim (miter/bevel) ileride eklenebilir.
- Tessellator bu dönüşümü yapar; renderer sadece üçgen alır.

## Uygulama

```
A ──────────────── B   (orijinal çizgi)

    n (normalize normal)
    ↑
A1 ─────────────── B1  (+width/2 · n)
|                   |
A2 ─────────────── B2  (-width/2 · n)

Üçgenler: A1-A2-B1, A2-B2-B1
```

```cpp
// Tessellator::TessellateThickLine içinde
glm::vec2 dir = glm::normalize(end - start);
glm::vec2 normal = glm::vec2(-dir.y, dir.x);
float half = width * 0.5f;
// 4 köşe → 2 üçgen
```

## Sonuçlar

- `Tessellator::TessellateThickLine` her çizgi segmentini 2 üçgene dönüştürür.
- `DrawPolyline` her segment çifti arasında miter join hesaplar.
- Performans: Tek bir ince çizgi için 6 vertex (ince çizgilerde de quad — tutarlılık için).
