# ADR-001: OpenGL 3.3 Core Profile Seçimi

- **Durum:** Kabul edildi
- **Tarih:** 2026-04-17

## Bağlam

SDLPainter, 2D çizim için bir GPU backend'e ihtiyaç duyuyor. Seçenekler:

| Seçenek | Artılar | Eksiler |
|---------|---------|---------|
| OpenGL 2.1 | Çok geniş donanım desteği | Deprecated; fixed pipeline; VAO yok |
| **OpenGL 3.3 Core** | Modern API; Windows/Linux/macOS; geniş destek | Legacy donanımlar dışarıda |
| OpenGL 4.5+ | DSA, compute shader | macOS desteği yok (4.1'de takılı) |
| OpenGL ES 2.0 | Mobil/gömülü | Desktop'ta sınırlı |
| Vulkan | Tam kontrol; cross-platform | Yüksek kurulum maliyeti |

## Karar

**OpenGL 3.3 Core Profile** kullanmaya karar verilmiştir.

## Gerekçe

- 2012 sonrası tüm masaüstü GPU'larda desteklenir — hedef kitle için yeterli taban.
- VAO, VBO, UBO, GLSL 330 — 2D kütüphane için gereken her şey mevcut.
- Core Profile: fixed-function pipeline kaldırılmış → daha temiz, tahmin edilebilir davranış.
- Vulkan desteği de `IRenderer` arayüzü sayesinde sunulacak.

## Sonuçlar

- GLAD `glad/0.1.36` ile OpenGL 3.3 core fonksiyonları yüklenir.
- SDL3 window `SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE)` ile oluşturulur.
- Geliştiriciler için grafik uygulamalarında kullanılabilecek, temel bir kütüphane oluşturulmuştur.