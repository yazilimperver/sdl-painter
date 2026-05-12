# SDLPainter — Yazılım Mühendisliği Perspektifi

Bu belge SDLPainter'ın teknik özellik listesi değil; kütüphanenin
**neden böyle tasarlandığını** ele alır. Mimari kararlar, tasarım desenleri,
performans tercihleri ve kapsam yönetimi yazılım mühendisliği perspektifinden
incelenmektedir.

---

## Mimari: Katmanlı Sorumluluk Ayrımı

Kütüphane dört katmandan oluşur ve her katmanın tek bir sorumluluğu vardır:

```
Painter          → Kullanıcı API'si, stil ve transform yönetimi
  Tessellator    → Şekil → vertex dönüşümü (backend bilmez)
    IRenderer    → Backend sözleşmesi (GPU bilmez, sadece triangle alır)
      SDL3       → Pencere, context, platform
```

Bu ayrım pratikte şu anlama gelir: `OpenGLRenderer`'ı `VulkanRenderer` ile
değiştirirken `Painter`, `Tessellator` veya `RenderState` dosyalarında
tek satır değişiklik yapılmaz. Değişim sınırı `IRenderer` arayüzüdür.

---

## Tasarım Desenleri

### Strategy — IRenderer

Backend seçimi compile-time değil, constructor'da çözülür.
`Painter(window, RendererBackend::kVulkan)` yazmak yeterli;
üst katmanlar backend'den tamamen habersizdir.

```cpp
// Factory fonksiyonu IRenderer implementasyonunu döner
std::unique_ptr<IRenderer> CreateRenderer(RendererBackend backend);
```

Yeni bir backend eklemek `IRenderer`'ı implement eden bir sınıf yazmaktan
ibarettir — mevcut hiçbir kodu değiştirmek gerekmez (Açık/Kapalı Prensibi).

### Memento — Transform Stack

`Save()` / `Restore()` çifti `std::vector<RenderState>` üzerinde push/pop
semantiğiyle çalışır. `RenderState` transform matrisi, pen, brush, opacity ve
clip rect'i birlikte taşır; bu sayede iç içe transform bloklarında durum
tutarlılığı garanti altındadır. QPainter kullanıcıları bu semantiği zaten bilir.

### Command Buffer — RenderBatcher

Her `DrawCircle` çağrısı anında GPU'ya gitmez. `RenderBatcher` vertex'leri
bir arabellekte biriktirir; yalnızca şu durumlarda `Flush()` tetiklenir:

- Çizim modu değiştiğinde (düz renk ↔ textured)
- Aktif texture değiştiğinde
- Opacity değiştiğinde
- 8 192 vertex limiti dolduğunda

Bu yaklaşım draw call sayısını sahnedeki şekil sayısından bağımsız kılar;
performans ölçeği bireysel GPU komutlarına değil, state değişim sıklığına bağlıdır.

### Stateless Tessellator

`Tessellator` sınıfının tüm metodları statiktir ve state tutmaz.
Aynı parametrelerle çağrılan `TessellateFilledCircle` her zaman aynı
vertex listesini üretir. Bu özellik birim testleri GPU gerektirmeden
yazılabilmesini sağlar: giriş koordinatları → beklenen vertex sayısı ve
değerleri doğrulanır.

---

## Bağımlılık Tasarımı

### Dışa Bağımlılıkları Minimize Etme

| Karar | Tercih | Neden |
|-------|--------|-------|
| Image yükleme | stb_image | Header-only, sıfır transitive bağımlılık; SDL_image ekstra kurulum ister |
| OpenGL yükleme | GLAD | Sadece 3.3 Core fonksiyonları dahil; tam GLEW yüklemesi gerekmez |
| Font | SDL_ttf | Opsiyonel; Phase 4'e kadar derlemeye dahil edilmez |
| Vulkan | Opsiyonel Conan paketi | `with_vulkan=False` ile Vulkan loader hiç indirilmez |

### Zorunlu ve Opsiyonel Bağımlılıklar

`conanfile.py`'de bağımlılıklar zorunlu ve opsiyonel olarak ikiye ayrılmıştır.
Vulkan loader, SDL_ttf ve GTest yalnızca ilgili seçenek etkinleştirildiğinde
çekilir. Bu ayrım CI süresini ve image boyutunu küçük tutar.

---

## Performans Kararları

### glLineWidth Kullanılmaması

OpenGL 3.3 Core Profile'da `glLineWidth(x > 1.0f)` deprecated olup
sürücüden sürücüye değişen maksimum değer garanti edilemez. Kalın çizgiler
bunun yerine her segmenti iki üçgene (quad) dönüştüren
`TessellateThickLine` ile çizilir:

```
A1 ─────────────── B1   (+half_width · normal)
A2 ─────────────── B2   (-half_width · normal)
Üçgenler: A1-A2-B1  ve  A2-B2-B1
```

Bu sayede piksel genişliği Linux, Windows ve Docker headless ortamda tutarlıdır.
Tessellator bu dönüşümü yapar; renderer sadece üçgen listesi alır.

### Adaptif Tessellation

Daire ve elips, yarıçapa göre değişen segment sayısıyla çizilir:

```cpp
int segments = std::max(16, static_cast<int>(radius * 0.5f));
```

Küçük daireler az segment (verimlilik), büyük daireler çok segment (kalite) kullanır.
Sabit segment sayısı seçmek bu dengeyi kuramaz.

### Konveks Optimizasyonu

`TessellateFilledPolygon` önce konvekslik kontrolü yapar. Konveks
poligonlar O(n) triangle fan ile işlenir; konkav olanlar O(n²)
ear clipping'e yönlendirilir. 2D arayüz çiziminde tipik poligon
boyutları (n < 200) için bu fark ihmal edilebilir düzeydedir.

---

## Test Edilebilirlik

### GPU Gerektirmeyen Birim Testler

`Tessellator` stateless ve `IRenderer`'dan bağımsız olduğundan
birim testleri saf CPU üzerinde çalışır. Daire için üretilen vertex sayısı,
thick line quad köşelerinin koordinatları, ear clipping sonucu üçgen sayısı
doğrudan sayısal karşılaştırmayla doğrulanabilir.

### IRenderer Mock Edilebilirliği

`RenderBatcher` ve `Painter` saf arayüz üzerinden çalıştığından
testlerde gerçek OpenGL/Vulkan yerine mock `IRenderer` kullanılabilir.
Draw call sayısı ve parametreleri test edilebilir hale gelir.

### Sanitizer Entegrasyonu

`linux-debug-asan` CMake preset'i AddressSanitizer ve UBSan'ı etkinleştirir.
CI pipeline'ında ayrı bir `test:unit:asan` job'u bu preset üzerinden koşar.
Bellek hataları ve undefined behavior, normal testler geçse dahi bu aşamada
yakalanır.

---

## Kapsam Yönetimi

Kasıtlı olarak kapsam dışı bırakılan özellikler belgelenmiştir:

| Özellik | Durum | Gerekçe |
|---------|-------|---------|
| Path / Bezier eğrileri | Kapsam dışı (v1) | Tessellator karmaşıklığını katlar; v1 için gereksiz |
| Gradient dolgu | Kapsam dışı (v1) | Shader karmaşıklığı; v1 tek renk yeterli |
| Delikli poligonlar | Kapsam dışı | Ear clipping hole desteklemez; Delaunay CDT gerektirirdi |
| Tam anti-aliasing | Kısmi (MSAA) | Path tabanlı AA scope dışı |

Bu sınırlar ADR belgelerinde gerekçesiyle kayıt altına alınmıştır.
"Neden yapılmadı?" sorusu ileride projeye dahil olan bir geliştirici
için cevapsız kalmaz.

---

## Mimari Karar Kayıtları (ADR)

Projedeki her kritik mimari karar bir ADR dosyasına bağlanmıştır:

| ADR | Karar |
|-----|-------|
| ADR-001 | OpenGL 4.5 veya ES yerine 3.3 Core Profile |
| ADR-003 | `glLineWidth` yerine geometry quad |
| ADR-004 | Tessellator'ın backend-agnostic tasarımı |
| ADR-005 | SDL_image yerine stb_image |
| ADR-006 | Triangulation için ear clipping |

ADR formatı: bağlam → değerlendirilen alternatifler → karar → gerekçe → sonuçlar.
"Ne yapıldı" değil, "neden yapıldı" sorusu yanıtlanmaktadır.

---

## Altyapı Tasarımı

### Tekrarlanabilirlik

`CMakePresets.json` + Conan 2 kombinasyonu şunu sağlar: bir geliştirici
`linux-debug` preset'ini çalıştırdığında, CI pipeline'ı çalıştırdığında
ve Docker container'ı çalıştırdığında tamamen aynı araç zinciri ve
bağımlılık sürümleri kullanılır. "Benim makinemde çalışıyor" sorununu
yapısal olarak ortadan kaldırır.

### Docker Multi-Stage ve Headless OpenGL

`ci` Docker stage'i `SDL_VIDEODRIVER=offscreen` ortam değişkeniyle
başlatılır. SDL3 bu modda gerçek bir görüntü sunucusu aramaz; OpenGL
context offscreen olarak oluşturulur. GitLab runner'lar başsız ortamda
çalıştığından bu olmadan renderer testleri context oluşturma aşamasında
çökerdi.

Multi-stage yapının ikinci avantajı cache verimliliğidir: `ci` ve
`windows-cross` stage'leri `builder`'ı baz alır; GCC, Clang ve Conan
kurulum katmanları her build'de yeniden indirilmez.

### Kalite Kapısı

`quality:clang-format` job'u `--Werror` ile çalışır ve pipeline'ı kırar;
merge edilecek hiçbir kod format ihlali taşıyamaz. `clang-tidy` ise
başlangıç fazlarında `allow_failure: true` ile işaretlidir — erken
aşamalarda tüm uyarıları kapatmak geliştirme hızını düşürür. Bu yapı
bilinçli bir denge tercihidir.

---

## Tasarım Prensipleri Özeti

| Prensip | SDLPainter'daki karşılığı |
|---------|--------------------------|
| Tek Sorumluluk | Tessellator geometri bilir, renderer API bilir, Painter stil bilir |
| Açık/Kapalı | Yeni backend → sadece `IRenderer` implement et, mevcut kod değişmez |
| Liskov Yerine Geçme | `OpenGLRenderer` ve `VulkanRenderer` `Painter` için değiştirilebilir |
| Bağımlılık Tersine Çevirme | `Painter` somut renderer'a değil, `IRenderer` soyutlamasına bağlıdır |
| Kapsam Kontrolü | Dokümante edilmiş sınırlar; spekülatif özellik yok |
