# SDLPainter — Dokümantasyon Rehberi

Bu belge kütüphaneye katkıda bulunanlar için **Doxygen yorum standardını**,
yerel build adımlarını ve GitLab Pages entegrasyonunu açıklar.

---

## 1. Araç Zinciri

```
include/sdl_painter/*.h   →  Doxygen  →  HTML (public/)
doc/*.md                  →  Doxygen  →  HTML (public/)
                                           ↓
                                    GitLab Pages
                              https://<namespace>.gitlab.io/<repo>/
```

Doxygen iki kaynağı birleştirir: **header yorumları** (API referansı)
ve **doc/ markdown dosyaları** (rehberler, diyagramlar). Sonuç tek bir
HTML sitesidir.

Mermaid diyagramları Doxygen'in HTML çıktısında **güncel Doxygen
sürümlerinde (≈1.10+) native olarak render edilir**: ` ```mermaid ` blokları
`<pre class="mermaid">` içine sarılır ve sayfaya CDN üzerinden mermaid.js
enjekte edilir. CI'daki `docs` job'u bu nedenle Ubuntu'nun apt deposundaki
(donmuş, eski) paket yerine Doxygen'in resmi Linux binary'sini indirir.
Eski/donmuş Doxygen sürümleriyle (apt'tan gelen gibi) diyagramlar düz kaynak
kod bloku olarak görünür — bu durumda `doc/*.md` dosyaları yine de
GitLab/GitHub markdown viewer'ında okunabilir kalır.

---

## 2. Kurulum (ilk seferinde)

### doxygen-awesome-css submodule'ü ekle

```bash
git submodule add \
  https://github.com/jothepro/doxygen-awesome-css.git \
  third_party/doxygen-awesome-css
git commit -m "chore: doxygen-awesome-css submodule eklendi"
```

Klonlama sırasında submodule'ü de çekmek için:

```bash
git clone --recurse-submodules <repo-url>
# Veya mevcut klonda:
git submodule update --init
```

GitLab CI'da `GIT_SUBMODULE_STRATEGY: recursive` zaten tanımlı; ek
yapılandırma gerekmez.

---

## 3. Yerel Build

```bash
# CMake configure (bir kere yeterli — normal build ile aynı komut)
cmake --preset linux-debug

# Doxygen çalıştır
cmake --build --preset linux-debug --target sdlpainter_docs

# Tarayıcıda aç
xdg-open build/linux-debug/docs/public/index.html   # Linux
start build\linux-debug\docs\public\index.html       # Windows
```

Doxygen kurulu değilse CMake `sdlpainter_docs` hedefini tanımlamaz
(projenin build'ini engellemez). Kurulum:

```bash
# Ubuntu / Debian
sudo apt install doxygen graphviz

# macOS
brew install doxygen graphviz

# Windows (winget)
winget install doxygen.doxygen
winget install graphviz.graphviz
```

---

## 4. Ne Belgelenmeli, Ne Belgelenmemeli

| Kapsam | Kural |
|--------|-------|
| `include/sdl_painter/*.h` — tüm `public` üyeler | `@brief` **zorunlu**; parametreler ve dönüş değeri belgelenmeli |
| `include/sdl_painter/*.h` — sınıf/struct tanımı | Sınıf düzeyi blok (`/// @brief` + genel davranış açıklaması) **zorunlu** |
| `src/tessellator.h`, `src/render_batcher.h` (internal) | `@brief` yazılabilir; Doxygen çıktısına girmez (`EXCLUDE_PATTERNS` kapsar) |
| `private` metodlar | Gerekmedikçe yorum yazma — iyi isimlendirilmiş kod yeterli |
| Implementasyon (`*.cpp`) | Sadece **neden** kısmı için satır içi yorum; Doxygen çıktısına girmez |

**Kural:** "Bu yorumu silsem başka bir geliştirici kayıptan zarar görür mü?"
Cevap hayırsa yorum yazma.

---

## 5. Yorum Formatı

Tüm yorumlar `///` üçlü slash ile yazılır. `/** */` blok formatı
kullanılmaz (Google Style tutarlılığı için).

### 5.1 Temel Yorum Bloku

```cpp
/// @brief Tek cümlelik özet. Büyük harfle başlar, nokta ile biter.
///
/// İkinci paragraf: neden bu metod var, ne garanti eder, nasıl
/// çağrılmalı. Bir satırdan uzunsa ek paragraf açılır.
///
/// @param cx   Merkez X koordinatı (piksel, Y=0 üstte).
/// @param cy   Merkez Y koordinatı (piksel).
/// @param radius  Yarıçap (piksel). 0 veya negatif → boş liste döner.
/// @return Tessellate edilmiş vertex listesi.
///         Segment sayısı adaptif: `max(16, int(radius * 0.5f))`.
/// @note Stateless — aynı input her zaman aynı output verir.
static std::vector<Vertex> TessellateFilledCircle(float cx, float cy,
                                                  float radius);
```

### 5.2 Kullanılan Etiketler

| Etiket | Ne zaman |
|--------|----------|
| `@brief` | Her public üyede **zorunlu** — tek satır özet |
| `@param name` | Her parametre için; ismi ve beklenen değer aralığını yaz |
| `@return` | Void dışı her fonksiyonda; hata durumu dahil |
| `@note` | Beklenmedik davranış, önemli kısıtlama, sözleşme |
| `@warning` | Yanlış kullanım crash / tanımsız davranış yaratıyorsa |
| `@code` / `@endcode` | Kullanım örneği (birden fazla satırsa) |
| `@see` | İlgili başka sınıf/metod (opsiyonel) |
| `@since` | API'ye faz bazlı eklendiyse (örn. `@since Phase 4`) |

`@todo` ve `@deprecated` repo içinde kullanılmaz; bunlar ADR veya
issue tracker'a aittir.

### 5.3 Sınıf Düzeyi Blok

```cpp
/// @brief Görüntü yöneticisi — stb_image üzerinden dosya yükler.
///
/// Image nesnesi oluşturulduğunda piksel verisi CPU belleğine alınır.
/// GPU texture'ı ise ilk `DrawImage` çağrısında lazy olarak yüklenir
/// ve önbelleklenir; ikinci çağrıda maliyetsizdir.
///
/// Bir Image tek bir IRenderer'a bağlanmalıdır: farklı renderer'lara
/// yükleme desteklenmez (texture handle renderer'a özgüdür).
///
/// @see Texture, IRenderer::CreateTexture
class Image {
```

### 5.4 Enum Değerleri

```cpp
/// @brief Render backend seçeneği.
enum class RendererBackend {
  kOpenGL,  ///< OpenGL 3.3 Core Profile — GLAD loader ile.
  kVulkan,  ///< Vulkan 1.1 — SPIR-V pipeline (SDLPAINTER_HAS_VULKAN gerekir).
};
```

Satır sonu `///<` formatı Doxygen'de enum değerlerine yorum ekler.

### 5.5 `@param` İçin Birim ve Aralık Kuralı

Belirsizlik yaratabilecek her parametrede birim veya aralık belirt:

```cpp
// İyi:
/// @param angle_degrees  Döndürme açısı (derece, saat yönü pozitif).
/// @param alpha          Global opaklık [0.0 = tam şeffaf, 1.0 = tam opak].
/// @param width          Çizgi kalınlığı (piksel, >= 0; 0 → çizim atlanır).

// Kötü (neyi belirtmiyor?):
/// @param angle  Açı.
/// @param alpha  Alpha değeri.
/// @param width  Genişlik.
```

---

## 6. Sık Yapılan Hatalar

### 6.1 @brief'i Tekrar Etmek

```cpp
// Kötü: imza zaten bunu söylüyor
/// @brief DrawRect fonksiyonu dikdörtgen çizer.
void DrawRect(float x, float y, float w, float h);

// İyi: imzanın söylemediğini ekle
/// @brief Dikdörtgen çerçeve çiz; dolgu için FillRect kullan.
/// @note Pen görünmezse (alpha=0 veya width=0) çizim atlanır.
void DrawRect(float x, float y, float w, float h);
```

### 6.2 Implementation Detayını Public Yoruma Koymak

```cpp
// Kötü: kullanıcı bunu bilmek zorunda değil
/// @brief Tessellator::TessellateStrokedRect çağırır ve batcher'a gönderir.
void DrawRect(float x, float y, float w, float h);

// İyi: davranış sözleşmesi
/// @brief Dikdörtgen çerçeve çiz.
/// @note Pen görünmezse çizim atlanır; Begin/End arasında çağrılmalı.
void DrawRect(float x, float y, float w, float h);
```

### 6.3 Her Şeye Yorum Yazmak

```cpp
// Kötü: gürültü
/// @brief Width'i döndürür.
int32_t Width() const { return mWidth; }

// İyi: getter'lar öz açıklayıcı, yorum gereksiz
int32_t Width() const { return mWidth; }
```

---

## 7. WARN_IF_UNDOCUMENTED

`Doxyfile.in` içinde `WARN_IF_UNDOCUMENTED = YES` ayarlıdır. Bu,
`@brief` eksik olan her public üye için Doxygen'in uyarı üretmesi
anlamına gelir. CI `pages` job'u bu uyarıları stderr'e yazar; şimdilik
build'i kırmaz (uyarı ≠ hata) ama gözden geçirilmeli.

İleride katı hale getirmek için `Doxyfile.in`'de:

```ini
WARN_AS_ERROR = YES   # Tüm uyarıları hata say → CI job başarısız olur
# veya daha seçici:
WARN_AS_ERROR = FAIL_ON_WARNINGS
```

---

## 8. GitLab Pages

`pages` CI job'u yalnızca **default branch** (`main`) üzerinde çalışır.
Her merge sonrası dokümantasyon otomatik güncellenir.

Yayınlanan adres:
```
https://<gitlab-namespace>.gitlab.io/<repo-name>/
```

Proje henüz public değilse GitLab Pages ayarlarında
**"Everyone"** erişimi verilmesi gerekebilir
(`Settings → General → Visibility → Pages`).

---

## 9. Doküman Türleri ve Sorumlulukları

```
include/sdl_painter/*.h   ← API referansı (Doxygen)     Kim: kodu yazan
doc/hizli-baslangic.md    ← Kullanım rehberi (Markdown) Kim: kodu yazan
doc/mimari-genel-bakis.md ← Mimari (Markdown+Mermaid)   Kim: mimar / kıdemli
doc/akislar.md            ← Akışlar (Markdown+Mermaid)  Kim: mimar / kıdemli
adr/                      ← Karar gerekçeleri (ADR)     Kim: karar veren
```

İki belge türü birbirinin yerini tutmaz:
- **Doxygen yorumları** → "Bu metod ne yapar, nasıl çağrılır?"
- **doc/ rehberleri** → "Bu bileşen neden var, büyük resimde nerede durur?"

---

## 10. İlgili Dosyalar

| Dosya | Rol |
|-------|-----|
| `cmake/Doxyfile.in` | Doxygen şablonu (CMake değişkenleri ile konfigure edilir) |
| `cmake/Docs.cmake` | `sdlpainter_docs` CMake hedefini tanımlar |
| `CMakeLists.txt` | `SDLPAINTER_BUILD_DOCS` option |
| `.gitlab-ci.yml` | `pages` job — main branch'te GitLab Pages'e yayınlar |
| `third_party/doxygen-awesome-css/` | Tema submodule'ü (git submodule add ile eklenir) |
