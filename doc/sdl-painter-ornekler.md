# SDLPainter — Örnekler Rehberi

Her demo bir öncekinin üzerine inşa edilir. Örnekler fazlandırma usulü geliştirildi.
Her biri, bir diğerinin üzerine bir ekleme yaparak gider. Bu doküman her örneğin **neyi doğruladığını**, **hangi API
yeteneklerini kullandığını** ve **mühendislik açısından neden önemli
olduğunu** açıklar.

> **Kapsam:** Bu rehber fazlandırma sırasıyla gelen çekirdek demoları anlatır
> (`hello_window` → `tictactoe`). Sonradan eklenen `graphics/`, `games/`,
> `benchmarks/` demoları ve `hero` showcase'i henüz burada yok; hepsinin güncel
> listesi ve tek satırlık açıklamaları için
> [examples/README.md](../examples/README.md).

---

## hello_window — Altyapı Doğrulaması

**Amaç:** Derleme, SDL3 başlatma ve spdlog entegrasyonunun çalıştığını
doğrular. Herhangi bir çizim yapmaz.

### Kullanılan yetenekler

| Yetenek | Detay |
|---------|-------|
| SDL3 başlatma | `SDL_Init(SDL_INIT_VIDEO)` |
| OpenGL context | 3.3 Core Profile nitelik ayarları |
| spdlog | ANSI renkli çıktı, `[SS:DD:SS][Seviye]` formatı |
| Event loop | `SDL_PollEvent` + ESC / `SDL_EVENT_QUIT` |
| Windows uyumluluğu | `ENABLE_VIRTUAL_TERMINAL_PROCESSING` ile ANSI escape etkinleştirme |

### Mühendislik notu

Phase 0 demosunun çizim yapmaması bilinçlidir: altyapı (CMake, Conan,
CI/CD pipeline, `IRenderer` arayüzü) çalışmadan üst fazlara geçmek
anlam taşımaz. Demo bu altyapının entegrasyon testi işlevi görür.

---

## primitives — Temel Primitifler

**Amaç:** OpenGL backend'in tüm temel çizim primitiflerini doğrular.
Tek bir pencerede stroke/fill çiftleri, kalın çizgiler, polyline ve
konkav poligon bir arada gösterilir.

### Kullanılan yetenekler

| Yetenek | Gösterim |
|---------|---------|
| `FillRect` / `DrawRect` | Dolu ve çerçeve dikdörtgenler |
| `FillCircle` / `DrawCircle` | Farklı renk ve kalınlıkta |
| `FillEllipse` / `DrawEllipse` | rx ≠ ry ile asimetrik elips |
| `DrawLine` | İki kesişen kalın çizgi (5px) |
| `DrawPolyline` | 5 noktalı kırık çizgi |
| `FillPolygon` | L şeklinde konkav poligon — ear clipping |
| `DrawPolygon` | Aynı poligonun çerçevesi |
| `Save` / `Rotate` / `Scale` / `Restore` | Döndürülmüş ve ölçeklenmiş dikdörtgen/daire |
| Düzenli çokgen üretimi | Döngüyle hesaplanan pentagon köşeleri |
| MSAA | `SDL_GL_MULTISAMPLESAMPLES = 4` |

### Mühendislik notu

L şeklindeki konkav poligon, ear clipping algoritmasının
doğrudan testidir; triangle fan bu geometriyi yanlış render
ederdi. Pentagon köşeleri trigonometrik hesapla üretilir — sabit
koordinat listesi değil. Demo her frame'de tamamen yeniden çizilir;
stateful render state birikimi olmaz.

---

## transforms — Transform Stack

**Amaç:** Transform stack'in tüm operasyonlarını animasyonlu olarak
doğrular. Yedi bağımsız bölüm her operasyonu izole eder.

### Kullanılan yetenekler

| Bölüm | Test edilen yetenek |
|-------|---------------------|
| 1 | Zincirleme `Translate` — iç içe kaydırma |
| 2 | `Rotate` animasyonu — dönen dikdörtgen |
| 3 | `Scale` animasyonu — `sin` ile nefes efekti |
| 4 | 3 katlı iç içe `Save` / `Restore` — bağımsız hız ve yön |
| 5 | `SetClipRect` / `ClearClip` — scissor içine kırpılan daire ve dönen dikdörtgen |
| 6 | `ResetTransform` — kasıtlı yanlış transform sonrası sıfırlama |
| 7 | Bağımsız X/Y `Scale` animasyonu — elips üzerinde |

### Mühendislik notu

Bölüm 6'da `Translate(9999, 9999)` ile ekran dışına çıkıldıktan sonra
`ResetTransform()` çağrılır ve daire ekranın beklenen konumunda görünür.
Bu, matrisin sıfırlanmasının üst stack katmanlarını bozmadığını doğrulayan
regresyon testidir. Bölüm 4'teki 3 katlı iç içe stack, push/pop sırasının
doğruluğunu gösterir: en içteki `Rotate` dışarıdakilerden bağımsız hareket
eder.

---

## clipping — Merkez Rotasyon Doğrulaması

**Amaç:** `Translate → Rotate` sırasının doğru uygulandığını izole
biçimde doğrular. Pencere yeniden boyutlandırılsa bile dikdörtgen daima
ekranın tam ortasında kendi merkezi etrafında döner.

### Kullanılan yetenekler

| Yetenek | Detay |
|---------|-------|
| Dinamik pencere boyutu | `SDL_GetWindowSize` her frame'de çağrılır |
| `Translate` + `Rotate` | `(-w/2, -h/2)` ofseti ile merkez döndürme |
| Referans arti işareti | İnce çizgilerle ekran merkezi işaretlenir |

### Mühendislik notu

Bu örnek, transforms içinde gösterilip geçilebilecek bir senaryonun
**ayrı bir demo olarak çıkarılmasının** neden gerekli olduğunu gösterir:
döndürme pivotunun doğru noktada olması, transform matrisinin sıralamasına
son derece duyarlıdır. `Rotate → Translate` yerine `Translate → Rotate`
yazıldığında pivot kayar ve daire merkezinin köşeyi işaret etmesi bu hatayı
anında ortaya çıkarır. Demo, regresyon kapsamının neden geniş tutulması
gerektiğinin somut örneğidir.

---

## images — Image / Texture

**Amaç:** Image yükleme, ölçekleme, atlas dilimleme ve alpha blending
pipeline'ını doğrular. Harici dosyaya bağımlı olmamak için tüm dokular
prosedürel olarak üretilir.

### Prosedürel doku üreticileri

| Fonksiyon | Boyut | Kanal | İçerik |
|-----------|-------|-------|--------|
| `MakeCheckerboard` | 128×128 | RGBA | Damalı desen |
| `MakeGradient` | 256×64 | RGB | Yatay renk geçişi |
| `MakeAlphaCircle` | 128×128 | RGBA | Merkeze doğru yükselen alfa |
| `MakeColorAtlas` | 256×256 | RGBA | 2×2 renk atlası |

### Kullanılan yetenekler

| Bölüm | Test edilen yetenek |
|-------|---------------------|
| 1 | `DrawImage(image, x, y)` — orijinal piksel boyutu |
| 2 | `DrawImage(image, dest_rect)` — ölçeklendirilmiş çizim |
| 3 | `DrawImage(image, src_rect, dest_rect)` — src crop + dst scale |
| 4 | Texture atlas dilimleme — 4 karenin ayrı `src_rect` ile çizimi |
| 5 | Alpha blending — üst üste bindirilen yarı saydam dokular |
| 6 | `Transform + DrawImage` — dönen texture animasyonu |
| 7 | `Scale` animasyonu — texture'ın kendi merkezinden büyüyüp küçülmesi |

### Mühendislik notu

`Image::CreateFromData` API'si, test senaryolarını harici dosyadan
bağımsız kılar — CI ortamında asset yokken bile doku pipeline'ı test
edilebilir. Atlas dilimleme (Bölüm 4) sprite sheet kullanım
senaryosunu simüle eder. Alpha circle dokusu (Bölüm 5) GPU tarafındaki
blending sırasını doğrular: arkaplan dikdörtgen önce, saydam doku
sonra çizilir.

---

## text — Metin Çizimi

**Amaç:** SDL_ttf üzerinden font yükleme, ölçüm, hizalama ve transform
entegrasyonunu doğrular.

### Kullanılan yetenekler

| Bölüm | Test edilen yetenek |
|-------|---------------------|
| 1 | `DrawText(rect, text, kCenter)` — 56pt başlık, ortada |
| 2 | `DrawText(rect, text, kRight)` — sağa hizalı alt başlık |
| 3 | Üç hizalama yan yana — kLeft / kCenter / kRight karşılaştırması |
| 4 | 4 farklı punto boyutu — 14 / 22 / 36pt |
| 5 | Pen rengi ile metin rengi — 5 farklı renk |
| 6 | `SetOpacity` — 4 farklı saydamlık seviyesi (100% → 15%) |
| 7 | `Transform + DrawText` — `MeasureText` ile pivot hesabı, dönen metin |
| 8 | `DrawText(x, y, text)` — konum bazlı çizim |

### Platform bağımsız font keşfi

```
Windows: arial.ttf → calibri.ttf → segoeui.ttf → tahoma.ttf
Linux  : DejaVuSans.ttf → LiberationSans → FreeSans
```

`FindSystemFont` fonksiyonu olası yolları sırayla dener; hiçbiri
bulunamazsa demo hata vererek çıkar. Bu, harici asset bağımlılığını
açık biçimde yönetmenin örneğidir.

### CMakeLists.txt notu

`text`, SDL_ttf'in static veya shared linklenmiş olmasına göre
iki farklı `target_link_libraries` dalından birini seçer. Opsiyonel
bağımlılıkların build sistemi seviyesinde ele alınması burada görülür.

### Mühendislik notu

Bölüm 7'deki dönen metin, `MeasureText` sonucunun yarısı kadar negatif
ofset uygulanarak pivot'u metin merkezine taşır. Bu, transform stack'in
sadece geometriyle değil, text rendering pipeline'ıyla da doğru
çalıştığını doğrular.

---

## vulkan_* — Vulkan Backend

Bu beş demo, Vulkan backend'in OpenGL ile aynı davranışı sergilediğini
kademeli olarak doğrular. Hepsi `SDLPAINTER_WITH_VULKAN=ON` ile
derlenir.

| Demo | Doğrulanan yetenek |
|------|--------------------|
| `vulkan_clear` | Vulkan context başlatma, swapchain, `Clear` |
| `vulkan_triangles` | Untextured primitifler (tüm şekil tipleri) |
| `vulkan_textured` | `DrawImage` — texture upload, sampling |
| `vulkan_demo` | Swapchain recreate (pencere yeniden boyutlandırma), tüm primitifler, opacity, validation layer çıktısı |
| `vulkan_text` | SDL_ttf + Vulkan backend entegrasyonu |

### Mühendislik notu

`vulkan_demo` özellikle **swapchain recreate** senaryosunu test
eder: pencere boyutu değiştiğinde Vulkan swapchain yeniden oluşturulmalı,
bu süreçte çizim bozulmamalıdır. `IRenderer` arayüzünün bu senaryoyu
soyutlaması, `Painter` katmanında hiçbir ek kod gerektirmez.

`'R'` tuşu projeksiyon güncellemesini manuel olarak tetikler — bu, headless
ortamda `SDL_WINDOWEVENT_RESIZED` olmadan aynı kod yolunun test edilmesini
sağlar.

---

## app_basics — Application Çatısı

**Amaç:** `transforms` ile aynı görsel içeriği (dönen dikdörtgen + nabız
gibi ölçeklenen daire) SDL boilerplate'i olmadan üretir.

### Kullanılan yetenekler

| Yetenek | Detay |
|---------|-------|
| `Application` türetme | `OnInit` / `OnUpdate` / `OnRender` / `OnKeyDown` override |
| Otomatik yaşam döngüsü | SDL init, pencere, GL context, olay döngüsü ve yıkım çatıda |
| `TimingMode::kVariable` | Her frame bir `OnUpdate`, `dt` gerçek geçen süre |

### Mühendislik notu

Karşılaştırma noktası `transforms`'dur: aynı çıktı, ~250 satır daha az kod.
Çatının değeri burada ölçülebilir hale gelir. `Application` yıkım sırası
(Painter → pencere → SDL_Quit) türeyen sınıfın üyelerini de kapsayacak
şekilde tasarlandığı için `Image`/`Font` üyeleri düz üye olarak tutulabilir
(bkz. [ADR-008](../adr/ADR-008-application-framework-layer.md)).

---

## game_loop — Sabit Adımlı Oyun Döngüsü

**Amaç:** `TimingMode::kFixed` ile deterministik simülasyonu ve render
interpolasyonunu doğrular. Bir top yerçekimiyle düşüp zemine çarparak seker.

### Kullanılan yetenekler

| Yetenek | Detay |
|---------|-------|
| `TimingMode::kFixed` | Fizik sabit adımla; `OnUpdate` frame başına 0..N kez |
| `OnRender(Painter&, alpha)` | Önceki/geçerli konum arasında interpolasyon |
| `fixed_update_hz = 30` | Kasıtlı düşük sim hızı — interpolasyonun etkisi görünür |

### Mühendislik notu

Sim hızı 30 Hz'e düşürülmesine rağmen hareket akıcı görünür; çünkü çizim
`alpha` ile son iki durum arasında interpolasyon yapar (Game Programming
Patterns "play catch up"). `alpha` yok sayılırsa aynı demo takılmalı görünür —
interpolasyonun neden gerekli olduğunu doğrudan gösteren bir karşıtlık.

---

## tictactoe — Eksiksiz Uygulama: Girdi + Durum + Yerleşim

**Amaç:** Çatının **girdi** tarafını ve gerçek bir uygulamanın akışını
gösterir. app_basics çatının temelini, game_loop zamanlamayı gösterirken bu demo
diğer örneklerin hiçbirinde bulunmayan yetenekleri kapsar.

### Kullanılan yetenekler

| Yetenek | Detay |
|---------|-------|
| `OnMouseButtonDown` | Tıklama koordinatı → tahta hücresi (hit testing) |
| `OnMouseMove` | İmlecin üzerindeki boş hücreyi yarı saydam vurgulama |
| `OnResize` | Tahta geometrisi pencere boyutundan yeniden hesaplanır |
| Durum makinesi | oynanıyor → kazanan / berabere → yeniden başlat |
| `DrawText(Rect, ..., kCenter)` | Alt durum çubuğunda ortalanmış metin |
| `SetTitle` | Pencere başlığı sırayı/sonucu yansıtır |
| Kalın çizgi + daire | Izgara ve X çapraz çizgilerle, O daireyle çizilir |

**Kontroller:** Sol tık hamle · `R` yeniden başlat · `ESC` çıkış.
Oyun bittiğinde herhangi bir tık yeni oyun başlatır.

### Mühendislik notu

Oyun mantığı (`examples/games/tictactoe_logic.h`) çizimden ve SDL'den **tamamen
bağımsız** saf fonksiyonlardır: kazanan tespiti, hamle geçerliliği ve hit
testing. Bu ayrım sayesinde mantık headless olarak birim testlerle
doğrulanabilir (`tests/test_tictactoe_logic.cpp`) — demo dosyası yalnızca
girdi ve çizimden sorumlu kalır. Örnek uygulamalardan test edilen ilk mantık
budur.

Sıra tabanlı bir oyun olduğu için `TimingMode::kVariable` kullanılır; sabit
adıma ihtiyaç yoktur. game_loop ile yan yana okunduğunda iki zamanlama modunun
ne zaman seçileceği somutlaşır: sürekli simülasyon → `kFixed`,
olay güdümlü etkileşim → `kVariable`.

Duyarlı yerleşim `OnResize` içinde tek bir hesapla çözülür; tahta her zaman
pencereye ortalanır ve kısa kenara göre ölçeklenir. Hit testing aynı
geometriyi kullandığı için pencere boyutundan bağımsız olarak doğru çalışır.

---

## Örneklerin Genel Mühendislik Deseni

Aşağıdaki desen `hello_window`–`vulkan_text` demoları içindir; `app_basics`,
`game_loop` ve `tictactoe` bu
boilerplate'i `Application` çatısına devreder:

```
1. InitLogger()          → renkli spdlog başlat
2. SDL_Init              → hata varsa erken çık
3. GL/Vulkan nitelikleri → context türünü belirle
4. SDL_CreateWindow      → hata varsa erken çık
5. { Painter scope }     → RAII: Painter pencereden önce yok edilir
6. Event loop            → SDL_PollEvent, ESC / QUIT
7. painter.Begin() ... painter.End()
8. } ← Painter yok edilir; GL kaynakları context geçerliyken silinir
9. SDL_DestroyWindow / SDL_Quit
```

**Painter scope'u ayrı tutmanın nedeni:** OpenGL nesneleri (`glDeleteBuffers`,
`glDeleteTextures` vb.) context geçerliyken silinmelidir. `SDL_DestroyWindow`
context'i yok eder; Painter bu noktadan önce yok edilmezse `glDelete*` çağrıları
`GL_INVALID_OPERATION` (1282) hatası üretir. Açık scope bu sıralamayı garanti eder.
