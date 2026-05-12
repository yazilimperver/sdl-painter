# Katkı Rehberi

SDLPainter'a katkıda bulunmak istediğin için teşekkürler! Bu doküman,
projeye nasıl katkı yapabileceğini ve hangi standartlara uyman gerektiğini
özetler.

## İçindekiler

- [Davranış Kuralları](#davranış-kuralları)
- [Geliştirme Ortamı](#geliştirme-ortamı)
- [Branch Stratejisi](#branch-stratejisi)
- [Commit Mesajları](#commit-mesajları)
- [Kod Stili](#kod-stili)
- [Test Beklentileri](#test-beklentileri)
- [Merge Request (MR) Süreci](#merge-request-mr-süreci)
- [Hata Bildirimi](#hata-bildirimi)
- [Özellik Önerisi](#özellik-önerisi)

## Davranış Kuralları

Bu proje [Contributor Covenant 2.1](CODE_OF_CONDUCT.md) davranış kurallarını
benimser. Katkı yaparken bu kurallara uyman beklenir.

## Geliştirme Ortamı

### Önkoşullar

| Araç | Minimum Sürüm |
|------|--------------|
| CMake | 3.20 |
| Conan | 2.x |
| GCC / Clang | C++17 |
| MSVC | VS 2022 (v143) |
| clang-format | 18 (zorunlu) |
| clang-tidy | 18 (opsiyonel) |
| Doxygen | 1.9+ |

### Hızlı Başlangıç

```bash
# Bağımlılıkları yükle (tüm özellikler aktif)
conan install . --output-folder=build/linux-debug/generators --build=missing \
    -s build_type=Debug -o "&:with_vulkan=True" -o "&:with_text=True"

# Derle
cmake --preset linux-debug
cmake --build --preset linux-debug

# Testleri çalıştır
ctest --preset linux-debug --output-on-failure
```

Daha ayrıntılı kurulum için [doc/hizli-baslangic.md](doc/hizli-baslangic.md).

## Branch Stratejisi

| Branch | Amaç |
|--------|------|
| `main` | Stabil, yayınlanmış sürüm. Doğrudan push yapılmaz; yalnızca MR ile değişir. |
| `develop` | Aktif geliştirme dalı. Yeni özellikler buraya birleştirilir. |
| `feature/<kısa-ad>` | Yeni özellik geliştirme. `develop`'tan açılır, `develop`'a birleşir. |
| `fix/<kısa-ad>` | Hata düzeltme dalı. |
| `chore/<kısa-ad>` | Bakım, refactor, doküman vs. |

## Commit Mesajları

[Conventional Commits 1.0.0](https://www.conventionalcommits.org/tr/v1.0.0/)
standardını kullan:

```
<tip>(<kapsam>): <kısa açıklama>

[opsiyonel uzun açıklama]

[opsiyonel footer: BREAKING CHANGE / Closes #123]
```

**Tipler:**

| Tip | Anlam |
|-----|-------|
| `feat` | Yeni özellik |
| `fix` | Hata düzeltme |
| `refactor` | İşlevsellik değişmeden yapılan iç temizlik |
| `docs` | Yalnızca doküman değişikliği |
| `test` | Test ekleme/düzenleme |
| `chore` | Bakım (build, CI, formatter, vb.) |
| `perf` | Performans iyileştirmesi |
| `style` | Format/whitespace değişikliği |

**Örnek:**

```
feat(painter): add FillRect with rounded corners

DrawRoundedRect ve FillRoundedRect API'leri eklendi.
Köşe yarıçapı pixel cinsinden alınır; tessellator
köşeleri yay olarak üretir.

Closes #42
```

## Kod Stili

Bu proje **Google C++ Style Guide** standartlarına uyar. Özet:

- **Sınıflar/struct'lar:** `PascalCase` → `Painter`, `RenderState`
- **Fonksiyonlar:** `PascalCase` → `DrawCircle`, `SetPen`
- **Yerel değişkenler:** `snake_case` → `float line_width`
- **Sınıf üyeleri:** `mPascalCase` → `int32_t mLineWidth`
- **Sabitler:** `kPascalCase` → `constexpr int kMaxSegments = 256`
- **Enum değerleri:** `kPascalCase` → `kOpenGL`, `kVulkan`
- **Namespace:** `sdl_painter::` (alt: `sdl_painter::detail::`)
- **Dosya uzantıları:** `.h` (header), `.cpp` (source)
- **Dosya isimleri:** `snake_case` → `opengl_renderer.cpp`
- **Include guard:** `#pragma once`
- **Yorumlar:** Türkçe, Doxygen stili. Her public fonksiyon için `/// @brief`.

**Format kontrolü zorunludur** ve MR'lar bu kontrolden geçmek zorundadır:

```bash
./scripts/format-check.sh             # Kontrol
./scripts/format-check.sh --fix       # Otomatik düzelt
```

CI'da `quality:clang-format` job'u başarısız olursa MR birleştirilemez.

### Ek Kurallar

- `const` ve `constexpr` mümkün olan her yerde kullan.
- Raw pointer yerine `std::unique_ptr` / `std::shared_ptr` tercih et.
- Temel tipler için `int`/`short` yerine `<cstdint>` tipleri (`int32_t`, `int16_t`).
- Template metaprogramming'den kaçın; basit ve okunabilir tut.
- Logging için `spdlog` kullan; `printf`/`std::cout` kullanma.

## Test Beklentileri

- **Yeni özellik için birim testi zorunludur.** GTest framework'ü kullanılır.
- Backend bilgisi gerektiren testler için `tests/mock_renderer.h` mock'unu kullan.
- Pull Request açmadan önce tüm testler yerel olarak geçmeli:
  ```bash
  ctest --preset linux-debug --output-on-failure
  ```
- Mümkün olduğunda sanitizer (ASan/UBSan) ile de doğrula:
  ```bash
  cmake --preset linux-debug-asan
  cmake --build --preset linux-debug-asan
  ctest --preset linux-debug-asan --output-on-failure
  ```

## Merge Request (MR) Süreci

1. **Issue aç** — Büyük değişikliklerden önce tartışma için.
2. **Feature branch oluştur** — `feature/<kısa-ad>` formatında.
3. **Değişiklikleri yap, test yaz.**
4. **Format ve testleri çalıştır.**
5. **CHANGELOG güncelle** — `[Unreleased]` bölümüne girdi ekle.
6. **MR aç** — Şablonu doldur; bağlı olduğu issue'yu referans et.
7. **CI'ı yeşil bekle.** Format zorunlu, clang-tidy uyarılar bilgilendirme amaçlıdır.
8. **Review** — En az bir review onayı gerekir.

### Mimari Değişiklikler

Backend, API veya bağımlılık etkileyen değişiklikler için `adr/` dizinine
yeni bir Architecture Decision Record (ADR) ekle. ADR şablonu için mevcut
dosyaları örnek alabilirsin.

## Hata Bildirimi

Hata bildirirken [Bug Report şablonunu](.gitlab/issue_templates/Bug.md)
kullan. Bilgi olarak şunları ekle:

- SDLPainter sürümü
- Platform (OS, derleyici, sürücü)
- Backend (OpenGL / Vulkan)
- Reprodüksiyon adımları + minimum kod örneği
- Beklenen ve gözlenen davranış
- Log çıktısı (varsa)

## Özellik Önerisi

Yeni özellik önerirken
[Feature Request şablonunu](.gitlab/issue_templates/Feature.md) kullan. v1
kapsamında olmayan bazı özellikler (path, gradient, bezier) bilinçli olarak
ertelenmiştir — bkz.
[doc/sdl-painter-yazilim-muhendisligi.md → Kapsam Yönetimi](doc/sdl-painter-yazilim-muhendisligi.md).

## Lisans

Katkın bu projenin lisansı ([MIT](LICENSE)) altında dağıtılacaktır. MR
göndererek bunu kabul etmiş sayılırsın.

## Teşekkürler

Her türden katkı (kod, doküman, hata bildirimi, öneri) değerlidir.
İletişim: <yazilimperver@gmail.com>, [Yazılımperber](www.yazilimperver.com)
