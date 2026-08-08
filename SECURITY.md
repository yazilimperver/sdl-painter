# Güvenlik Politikası

## Desteklenen Sürümler

Güvenlik düzeltmeleri yalnızca aktif olarak desteklenen sürümler için
yayınlanır.

| Sürüm | Destek Durumu |
|-------|--------------|
| 1.1.x | ✅ Aktif |
| 1.0.x | ⚠️ Yalnızca kritik açıklar |
| < 1.0 | ❌ Desteklenmiyor |

## Güvenlik Açığı Bildirimi

Bir güvenlik açığı tespit ettiysen, lütfen **kamuya açık bir issue olarak
açma**. Bunun yerine aşağıdaki kanallardan biriyle bize özel olarak bildir:

### Tercih edilen yol: GitHub Security Advisory

[Yeni bir güvenlik danışma kaydı aç](https://github.com/yazilimperver/sdl-painter/security/advisories/new)
— **Security → Report a vulnerability**. Yalnızca proje sürdürücüleri görür.

### Alternatif: E-posta

<yazilimperver@gmail.com> adresine açıklayıcı bir e-posta gönder. Konu satırında
`[SDLPainter Security]` ön ekini kullan.

## Bildiriminde Şunları Belirt

- Açığın türü (örn. buffer overflow, integer overflow, use-after-free,
  bilgi sızıntısı vb.)
- Etkilenen dosya/sınıf/fonksiyon
- Yeniden üretmek için minimum kod örneği
- Etki (örn. crash, RCE, DoS, bilgi açığa çıkması)
- Önerilen düzeltme (varsa)
- Etkilenen sürümler
- Platform (OS, derleyici, sürücü, backend)

## Süreç

1. Bildiriminizi **48 saat içinde** aldığımızı onaylarız.
2. Açığı doğrular ve etki düzeyini değerlendiririz (CVSS v3.1).
3. Bir düzeltme planı paylaşırız (genelde 7 iş günü içinde).
4. Düzeltme hazırlanır, test edilir ve embargo süresi belirlenir.
5. Açıklama tarihinde:
   - Yamayı yayınlarız (patch sürüm)
   - CHANGELOG'a güvenlik notu eklenir
   - GitHub Security Advisory (gerekirse CVE) yayınlanır
6. Bildirim yapan kişi, isterse, açıklamada kredilendirilir.

## Etki Alanı

Bu politika SDLPainter kütüphane kodunu kapsar. Aşağıdakiler bu politikanın
**dışındadır** ve ilgili projelere bildirilmelidir:

- **SDL3** — <https://github.com/libsdl-org/SDL>
- **Vulkan loader / drivers** — Khronos / sürücü sağlayıcısı
- **GLAD / OpenGL sürücüleri** — sürücü sağlayıcısı
- **stb_image** — <https://github.com/nothings/stb>
- **SDL_ttf / FreeType** — ilgili projeler

## Güvenlik Pratiği

SDLPainter aşağıdaki pratiklerle güvenliği gözetir:

- Modern C++17 idiomları; raw pointer yerine `std::unique_ptr` / RAII
- Bağımlılıklarda sabitlenmiş sürümler (Conan)
- CI'da ASan + UBSan ile her commit'te doğrulama
- clang-format zorunlu, clang-tidy bilgilendirme amaçlı

## Teşekkür

Sorumlu açıklama yapan araştırmacılara teşekkür ederiz.
