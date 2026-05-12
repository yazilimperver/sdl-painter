# ADR-005: Image Yükleme — stb_image vs SDL_image

- **Durum:** Kabul edildi
- **Tarih:** 2026-04-17

## Bağlam

SDLPainter için PNG/JPG benzeri dokuların yüklenmesi gerekmektedir. İki aday:

| Kriter | stb_image | SDL_image |
|--------|-----------|-----------|
| Kurulum | Header-only, tek `.h` | Ayrı kütüphane, Conan bağımlılığı |
| Format desteği | PNG, JPG, BMP, TGA, HDR, PSD, GIF... | PNG, JPG, BMP, TGA, TIFF, WebP... |
| API sadeliği | `stbi_load()` — tek fonksiyon | SDL_Surface tabanlı |
| SDL entegrasyonu | Yok — ham pixel verir | SDL_Surface doğrudan |
| Bağımlılık zinciri | Sıfır | libpng, libjpeg... (platforma göre) |
| Lisans | MIT / Public Domain | Zlib |
| Conan paketi | `stb/cci.*` (mevcut) | `sdl_image` (mevcut ama ek bağımlılık) |

## Karar

**stb_image** kullanılır.

## Gerekçe

- Header-only: `#define STB_IMAGE_IMPLEMENTATION` + include — derleme sistemi değişikliği yok.
- SDLPainter SDL_Surface kullanmaz; ham `uint8_t*` pixel verisi doğrudan OpenGL/Vulkan texture'a yüklenir. SDL_image'ın SDL_Surface entegrasyonu avantajı ortadan kalkar.
- Bağımlılık zinciri sıfır — CI image boyutu ve derleme süresi korunur.
- PNG ve JPG yeterli; ekstra format ihtiyacı doğarsa stb zaten destekler.

## Sonuçlar

- `Image::Load()` içinde `stbi_load()` kullanılır; sonuç `uint8_t*` olarak renderer'a iletilir.
- `stbi_image_free()` RAII wrapper ile yönetilir.
- SDL_image bağımlılığı `conanfile.py`'e eklenmez.
