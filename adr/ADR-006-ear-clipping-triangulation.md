# ADR-006: Poligon Triangulation — Ear Clipping Seçimi

- **Durum:** Kabul edildi
- **Tarih:** 2026-04-17

## Bağlam

`FillPolygon` konkav (içbükey) poligonların da kullanılmasına yönelik desteğe ilişkin hazırlanmıştır. Triangle fan yalnızca konveks poligonlarda doğru çalışır. Burada buna yönelik olası yöntemler sunulacaktır.

### Triangulation Algoritmaları

| Algoritma | Karmaşıklık | Konkav | Delik | Uygulama |
|-----------|-------------|--------|-------|----------|
| Triangle Fan | O(n) | Hayır | Hayır | Trivial |
| -> **Ear Clipping** | O(n²) | Evet | Hayır | Orta  <- **Seçilen**|
| Monotone Partition | O(n log n) | Evet | Hayır | Karmaşık |
| Delaunay (CDT) | O(n log n) | Evet | Evet | Çok karmaşık / kütüphane gerekir |
| Tessellation shader | GPU | Evet | Evet | OpenGL 4.0+, Vulkan pipeline state |

## Karar

**Ear Clipping** algoritması `Tessellator` içinde implement edilir.

## Gerekçe

- 2B arayüz çiziminde poligon nokta sayısı tipik olarak < 200; O(n²) kabul edilebilir.
- Sıfır dış bağımlılık — sadece kendi implementasyonumuz.
- Algoritma sade: linked list üzerinde kulak (ear) bul → kes → tekrarla.
- Delik (hole) desteği şimdilik yok.
- Daha hızlı algoritmalar (monotone partition, CDT) gerektiğinde Tessellator arayüzü değişmeden içi değiştirilebilir.

## Uygulama Özeti

```cpp
// Ear: P[i-1], P[i], P[i+1] üçgeni konveks ve diğer noktaları içermiyorsa.
// 1. Nokta listesini döngüsel linked list'e al.
// 2. Her iterasyonda bir "ear" bul (IsEar kontrolü).
// 3. Ear üçgenini kaydet, listedan çıkar.
// 4. n-2 üçgen üretilince biter.
```

## Sınırlar

- Delik (hole) içeren poligonlar desteklenmez.
- Self-intersecting (kendisiyle kesişen) poligonlar tanımsız davranış üretir.
- Konveks poligonlarda Triangle Fan optimizasyonu `Tessellator` tarafından otomatik seçilir.

## Sonuçlar

- `Tessellator::TessellateFilledPolygon()` önce konvekslik kontrolü yapar; konveksse fan, değilse ear clipping kullanır.
- Ear clipping O(n²) — 1000+ noktalı poligonlar için uyarı belgelenmiştir.
