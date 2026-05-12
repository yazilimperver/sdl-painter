<!--
Yeni bir özellik önermeden önce: doc/sdl-painter-yazilim-muhendisligi.md →
Kapsam Yönetimi bölümünü inceledin mi? Bazı özellikler (path, gradient,
bezier) bilinçli olarak v2 için ertelenmiştir.
-->

## Motivasyon

<!-- Bu özellik hangi problemi çözüyor? Senin kullanım senaryon ne? -->

## Önerilen API

<!-- Mümkün olduğunca somut. QPainter'da nasıldı? -->

```cpp
// Önerilen kullanım örneği
sdl_painter::Painter p(window, RendererBackend::kOpenGL);
// ...
```

## Alternatifler

<!-- Düşündüğün başka tasarımlar var mı? Neden bunu seçtin? -->

## Uyumluluk

- [ ] OpenGL backend'inde mümkün
- [ ] Vulkan backend'inde mümkün
- [ ] ABI/API kıran bir değişiklik mi?
- [ ] Yeni bir bağımlılık gerektiriyor mu?

## Etki

<!-- Hangi sınıfları/dosyaları etkiler? ADR gerekecek mi? -->

## Ek Kaynaklar

<!-- QPainter dokümantasyonu, benzer kütüphanelerin yaklaşımı, vb. -->

/label ~enhancement
