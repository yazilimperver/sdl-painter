# Changelog

## [1.3.0] - 2026-08-28

### Performans
- **Transform artık batch'i kırmıyor (2000 → 2 draw call).** Model matrisi bir
  shader uniform'u olduğu için her `Translate`/`Rotate`/`Scale` çağrısı
  `RenderBatcher::Flush()` tetikliyordu; şekil başına transform kullanan tipik
  kod batch'lemeden hiç fayda görmüyordu. Dönüşüm artık `RenderBatcher` içinde,
  rengin yazıldığı **aynı kopyalama döngüsünde** CPU'da uygulanıyor (ek geçiş
  veya tahsis yok) ve `u_model` daima birim gönderiliyor. Ölçüm (Windows,
  MSVC Release, 2000 şekil): şekil başına `Save`+`Translate`+`Restore` ile
  **9.44 ms → 0.30 ms (31×)**; `Rotate` ekli hâlinde **9.37 ms → 0.37 ms**.
  Kare başına `SetModelMatrix` yüklemesi 2001 → 1.
  Ölçüm düzeneği ve tüm senaryolar: `examples/benchmarks/`.
- **`Save()` artık flush etmiyor**, `Restore()` ise yalnızca kırpma (clip)
  durumu **gerçekten** değiştiyse flush ediyor. Önceden her `Restore` gereksiz
  bir `ClearScissor` çağrısı da üretiyordu (2000 şekilde kare başına 2000 adet).
- **`Painter::SetOpacity` artık renderer'a doğrudan yazmıyor.** Opaklık
  uniform'unun tek sahibi `RenderBatcher`; her flush öncesi o batch'in değerini
  yazıyor. Öncekiler bir sonraki flush'ta nasıl olsa eziliyordu.

### Düzeltildi

> Aşağıdaki ilk üç madde, backend parite testi tarafından **yazıldığı gün**
> bulundu. Üçü de ekranda görünmüyordu; ancak iki backend'in çıktısı piksel
> piksel karşılaştırılınca ortaya çıktılar.

- **Vulkan'da dokular sRGB formatındaydı, OpenGL'de doğrusal.** `VulkanTexture`
  image'ları `VK_FORMAT_R8G8B8A8_SRGB` ile yaratıyordu; GPU örneklerken
  sRGB→doğrusal dönüşüm uyguluyor, swapchain (`B8G8R8A8_UNORM`) ise geri
  kodlama yapmıyordu. OpenGL tarafı `GL_RGBA` ile hiç dönüşüm yapmadığından
  **aynı PNG iki backend'de farklı renkte** çiziliyordu. Format
  `R8G8B8A8_UNORM` oldu. Glyph atlası da aynı yoldan geçtiği için metin de
  etkileniyordu.
- **Karıştırma modlarının alfa faktörleri iki backend'de ayrışıyordu.**
  OpenGL'de `glBlendFunc` alfa kanalına da **renk** faktörlerini uygular;
  Vulkan'da alfa faktörleri ayrı alanlardır ve orada farklı değerler seçilmişti
  (`vk_blend.h` kendi dokümanında "birebir aynı olmalı" dediği hâlde). Ekranda
  görünmüyordu çünkü swapchain'in alfası sunumda yok sayılır — bir çizim
  hedefine çizilince ortaya çıktı. Artık GL `glBlendFuncSeparate` kullanıyor ve
  iki backend de aynı formülü uyguluyor: `aOut = aSrc + aDst·(1 - aSrc)`
  (standart "over" bileşimi). Eski davranış (`aSrc² + aDst(1-aSrc)`) alfayı
  eksik biriktiriyordu; yarı saydam bir şekil çizilen hedef olması gerekenden
  saydam kalıyordu.
- **OpenGL'de başlangıç karıştırma durumu `SetBlendMode`'u atlıyordu.**
  `Initialize()` içinde ayrı bir `glBlendFunc` çağrısı vardı; `SetBlendMode`
  düzeltilince o satır sessizce eskidi ve **modu hiç değiştirmeyen** sahnelerde
  eski faktörler yürürlükte kaldı (batcher yalnızca mod değiştiğinde
  `SetBlendMode` çağırıyor). Başlangıç durumu artık tek kaynaktan geliyor.
- **Windows'ta paylaşımlı (shared) kütüphane derlenemiyordu.** Projede hiçbir
  sembol export mekanizması yoktu; `BUILD_SHARED_LIBS=ON` ile MSVC
  `sdl_painter.dll` üretiyor ama **import library (`.lib`) üretmiyordu**, yani
  hiçbir tüketici link edemiyordu. Linux/GCC'de varsayılan sembol görünürlüğü
  bunu gizlediği için fark edilmemişti. Artık `generate_export_header` ile
  `SDLPAINTER_API` / `SDLPAINTER_APP_API` makroları üretiliyor ve dışa açık
  tipler (`Painter`, `Image`, `Font`, `CreateRenderer`, `Application`) bunlarla
  işaretli. Doğrulandı: shared derleme → `cmake --install` → `find_package` →
  tüketici derlenip **çalışıyor**.
- **spdlog hedef adına sıkı bağımlılık.** `CMakeLists.txt` doğrudan
  `spdlog::spdlog_header_only` hedefine link ediyordu; bu hedef yalnızca
  `header_only=True` iken tanımlı. `header_only=False` (Conan varsayılanı) ile
  configure aşaması `target was not found` ile düşüyordu. Artık hangi hedef
  tanımlıysa o seçiliyor, ikisi de yoksa açık bir hata veriliyor. Bu, paket
  yöneticisi tarafında bağımlılık seçeneği zorlama ihtiyacını ortadan kaldırıyor
  (Conan Center bunu yasaklıyor).

### Eklendi
- **Yol ve Bézier eğrileri.** `sdl_painter::Path` — `MoveTo` / `LineTo` /
  `QuadTo` / `CubicTo` / `Close`, ve `Painter::DrawPath` / `Painter::FillPath`.
  Eğriler yola **eklenirken** kırık çizgiye çevrilir; böylece mevcut kalın
  çizgi ve ear clipping altyapısı olduğu gibi kullanılır ve kalemin tüm stil
  eksenleri (uç, birleşim, kesikli desen) yolda da çalışır. Parça sayısı
  ikinci türev sınırından hesaplanır — `n = ceil(sqrt(max|B''| / (8·flatness)))`
  — yani tolerans bir tahmin değil, garanti; testte sayısal olarak
  doğrulanıyor. **Sınır:** her alt yol bağımsız doldurulur, even-odd/nonzero
  dolgu kuralı yoktur; delikli şekiller bu sürümün kapsamı dışında. Yeni örnek:
  `paths`. 24 yeni test.
- **Dokuya çizim (render target).** `Painter::CreateRenderTarget` ile üretilen
  `sdl_painter::RenderTarget`, çizimi ekran yerine bir dokuya yönlendirir
  (`SetRenderTarget` / `ResetRenderTarget` / `DrawRenderTarget`). Mini harita,
  son işlem efektleri, iz efekti ve pahalı katmanların önbelleklenmesi için.
  **Y ekseni tuzağı ve çözümü:** OpenGL'in ekran framebuffer'ı aşağıdan yukarı
  adreslenir, Vulkan'ınki yukarıdan aşağı. Bir hedefe çizerken GL'de
  projeksiyon/viewport/scissor çevirmesi **yapılmaz**; böylece hedefin 0.
  satırı her iki backend'de de `y = 0`'a denk gelir. Aksi halde doku bellekte
  baş aşağı durur, ekranda ters görünür ve geri okuma iki backend'de farklı
  çıkardı. Vulkan tarafında hedefler kendi render pass'lerini ve ona bağlı
  ikinci bir pipeline takımını gerektirir (pipeline yalnızca **uyumlu** render
  pass ile kullanılabilir, uyumluluk attachment formatını kapsar); kare
  ortasında ekrana dönüş için `loadOp = LOAD` yapan bir ikiz render pass
  eklendi — asıl pass `CLEAR` olduğu için onu yeniden başlatmak o karede
  çizilmiş her şeyi silerdi. Yeni örnek: `render_target`. 17 yeni test.
- **`Painter::ReadRenderTarget`** — bir hedefin piksellerini ana belleğe okur.
  Çıktı her platformda **sıkı paketlenmiş, doğrusal RGBA8 ve yukarıdan aşağı**;
  hedefler bu yüzden ekran yüzeyinin formatını devralmaz (yüzey formatı
  sürücüye göre BGRA veya sRGB olabilirdi). Bloklar — kare döngüsü için değil,
  ekran görüntüsü ve testler için.
- **Backend parite testi** (`tests/test_backend_parity.cpp`). Aynı zengin sahne
  OpenGL ve Vulkan'da bir hedefe çizilip pikselleri karşılaştırılıyor. Repoya
  referans PNG gömülmedi: doğrulanmak istenen iddia "iki backend aynı sonucu
  verir" ve gömülü bir referans sürücüye bağımlı olurdu. **Ölçülen fark: 0** —
  19.200 pikselin tamamı bayt bayt aynı. Test düştüğünde farkın nerede
  toplandığını gösteren bir ASCII harita ve en kötü pikselin komşuluğunu
  basıyor. Bu test, aşağıdaki üç hatayı yazıldığı gün buldu.
- **Vulkan validation hataları artık testleri düşürüyor.** Mesajlar yalnızca
  log'a yazılıyordu; yeşil bir takımın altında gerçek spec ihlalleri
  birikebiliyordu. `vk_detail::ValidationErrorCount()` sayacı ve testlerdeki
  `ValidationGuard` bunu bir hata sinyaline çeviriyor.
- **Karıştırma (blend) modu.** `Painter::SetBlendMode` — `kAlpha` (varsayılan),
  `kAdditive`, `kMultiply`, `kNone`. `Save`/`Restore` kapsamında.
  **Backend farkı ve nasıl çözüldüğü:** OpenGL'de mod `glBlendFunc` ile çalışma
  zamanında değişir; Vulkan 1.1'de blend, grafik pipeline'ının **sabit
  durumudur** ve dinamik olarak değiştirilemez. Bu yüzden Vulkan tarafında mod
  başına ayrı bir pipeline varyantı **başlangıçta** üretiliyor (dördü tek
  `vkCreateGraphicsPipelines` çağrısında) ve çizim anında doğrusu bağlanıyor —
  çizim sırasında pipeline derlenmiyor. İki backend'in blend faktörleri
  `src/vulkan/vk_blend.h` içinde birebir eşleştirildi; aksi halde aynı çizim
  iki backend'de farklı görünürdü. Vulkan tarafı validation layer açıkken dört
  örnekle doğrulandı.
  Mod bir GPU durumu olduğu için **batch'i kırar** (renk ve tint'in aksine,
  vertex'te taşınamaz); `blend_modes` örneği bunun maliyetini canlı gösteriyor.
  6 yeni test.
- **Doku filtresi.** `Image::SetFilter(TextureFilter::kNearest)` — piksel
  sanatı büyütüldüğünde keskin kalır. GL'de `glTexParameteri`, Vulkan'da
  sampler; ikisi de MIN ve MAG için aynı filtreyi kullanır ki backend'ler aynı
  görüntüyü versin. Filtre doku **yaratılırken** uygulanır, yani ilk çizimden
  önce ayarlanmalı — belgelendi ve testlendi. 3 yeni test.
- **`Painter::DrawImageMesh`** — dokuyu serbest biçimli bir ızgara üzerine
  çizer, yani dörtgen köşeleri bağımsız hareket edebilir. `DrawImage`'ın üç
  aşırı yüklemesi de eksen hizalı `Rect` aldığı için dalgalanan bayrak, sayfa
  kıvrımı gibi efektler mümkün değildi. 6 yeni test.
- **`IRenderer` genişletme yöntemi.** Bu sürümde arayüze eklenen üç metotun
  (`SetBlendMode`, filtreli `CreateTexture`) hepsi **varsayılan gövdeli**;
  yani bu arayüzü dışarıda implemente etmiş kod derlenmeye devam eder ve
  desteklenmeyen kabiliyette makul varsayılana düşer. `v1.2.0`'da saf sanal
  `UpdateTexture` eklenmesi bu tür kodu kırıyordu.
- **Gradient fırça — shader'sız.** `Brush::LinearGradient(start, end, from, to)`
  ve `Brush::RadialGradient(center, radius, from, to)`. Geçiş, tessellation
  sonrası köşe renkleri hesaplanarak üretiliyor; enterpolasyonu donanım yapıyor.
  Sonuç: **hiçbir backend değişikliği yok, shader değişmedi ve gradient batch'i
  kırmıyor** — farklı gradientlerle çizilen üç şekil tek draw call'da kalıyor
  (testi var). Bedeli, geçişin şeklin köşe yoğunluğu kadar hassas olması;
  daire/elips segment sayısı yarıçapa göre uyarlandığı için pratikte sorun
  çıkarmıyor. Gradient koordinatları çizim koordinatlarıyla aynı uzayda,
  yani transform yığınından etkileniyor (Qt davranışı). 10 yeni test.
- **`Painter::SetViewport` / `ResetViewport`.** Çizimi pencerenin bir alt
  dikdörtgeniyle sınırlar ve koordinatları **o alt dikdörtgene yerelleştirir** —
  bölünmüş ekran, mini harita, kenar paneli için. Kırpmadan farkı: kırpma
  koordinat sistemini değiştirmeden piksel maskeler, viewport koordinat
  sisteminin kendisini yeniden tanımlar. `IRenderer::SetViewport` zaten vardı,
  eksik olan `Painter` yüzeyiydi. Bu değişiklikle `Painter` artık **çizim
  yüzeyi boyutu** ile **yürürlükteki viewport** ayrımını tutuyor; kırpma
  dikdörtgeni viewport-yerel verilir ve scissor kutusuna çevrilirken viewport
  orijini eklenir (aksi halde bölünmüş ekranda kırpma yanlış panele düşerdi).
  Yeniden boyutlandırma, kullanıcının seçtiği viewport'u ezmiyor. 8 yeni test.
- **`DrawRoundedRect` / `FillRoundedRect`.** Arayüz çiziminin en sık kullanılan
  şekli; dış hat konveks olduğu için mevcut ear clipping ve kalın çizgi yolları
  ek mantık gerektirmedi. Köşe çözünürlüğü daire ile aynı adaptif kurala uyar.
  Kenar durumları belgelenmiş ve testli: `radius <= 0` → `DrawRect` ile birebir
  aynı, `radius > min(w,h)/2` → oraya kırpılır (kare girdide daireye
  dejenere olur), `w`/`h` pozitif değilse hiçbir şey çizilmez. 14 yeni test.
- **Çok satırlı metin, sözcük kaydırma ve `Font::LineHeight()`.**
  `DrawText`'in her iki aşırı yüklemesi artık satır sonu karakterini satır
  bölme olarak işliyor (eskiden görünmez bir glyph gibi ele alınıyordu);
  satırlar arası mesafe fontun kendi metriği olan `LineHeight()` kadar.
  Dikdörtgen aşırı yüklemesine `TextWrap` parametresi eklendi: `kWord` ile
  satırlar sözcük sınırlarından bölünür, tek bir sözcük bile sığmazsa
  **UTF-8 kod noktası sınırında** karakterden bölünür. Varsayılan `kNone`,
  yani mevcut davranış birebir korunuyor — sarmalamayı varsayılan yapmak her
  mevcut çizimin görünümünü sessizce değiştirirdi.
  Yerleşim hesabı için `Painter::CountTextLines()` eklendi: metnin kaç satır
  tutacağını çizmeden söyler. Demo: `examples/graphics/charts.cpp`.
  10 yeni test.
- **Üç yeni örnek — eklenen kabiliyetlerin kabul testleri:**
  `strokes` (uç/birleşim/kesik/yuvarlatılmış dikdörtgen/yay tek sahnede; uç
  taşmasını referans çizgisiyle, miter'ın bevel'a düşüşünü daralan açı
  dizisiyle gösterir), `gradients` (shader'sız gradient ve **sınırının dürüst
  gösterimi**: 3'ten 64 köşeye giden poligon dizisi geçişin köşe yoğunluğuna
  bağlı olduğunu görünür kılar; hepsi yine tek draw call), `viewports`
  (bölünmüş ekran + mini harita; dört panel aynı çizim fonksiyonunu ofset
  hesabı olmadan çağırıyor).
- **`charts` örneği** — yay/dilim ve çok satırlı metnin birlikte kabul testi:
  pasta, çubuk ve çizgi grafiği, kesikli ızgara, ölçülerek hizalanmış
  etiketler ve sarmalanan bir açıklama kutusu. Kütüphanenin oyun dışı hedef
  kitlesini (araç / gösterge paneli çizimi) gösteren ilk örnek.
- **`Painter::UpdateImage`** — yüklenmiş bir görüntünün doku içeriğini yerinde
  günceller. `IRenderer::UpdateTexture` `v1.2.0`'da eklenmişti ama yalnızca
  glyph atlası tarafından **içeriden** kullanılıyordu; tüketicinin ona
  erişebileceği bir yol yoktu. `DrawImage` tint'iyle aynı durum: yetenek vardı,
  yüzeyi yoktu. Çağrı biriken çizimleri flush eder — doku anında değiştiği
  için o karede aynı dokudan yapılmış bekleyen çizimlerin eski içerikle
  gitmesi gerekir. Demo: `examples/graphics/plasma.cpp`.
- **`breakout` artık metin çiziyor** — skor, can ve menü/kazandın/kaybettin
  başlıkları. Font sistemden aranır (`examples/example_font.h`); bulunamazsa
  oyun şekil tabanlı göstergelerle metinsiz oynanmaya devam eder.
- **İlk örnek varlık dosyası: `examples/assets/rpg_character_walk.png`**
  (CC0, arikel — kaynak ve ızgara düzeni `examples/assets/README.md`'de).
  `sprite_animation` gerçek bir sprite sheet kullanıyor. Sheet'in sola ve sağa bakan satırları **birbirinin tam
  aynası** olduğu için örnek, sağa bakışı ya sheet'ten ya da sol satırı
  `ImageFlip` ile çevirerek çiziyor — sonuç piksel piksel aynı. Aynalamanın
  ne işe yaradığının tek karelik kanıtı.
  `sdlpainter_add_example(... ASSETS)` varlıkları çalıştırılabilirin yanına
  kopyalar; örnek `SDL_GetBasePath()` ile bulur. **Kütüphane hâlâ hiçbir
  varlık dosyası taşımıyor** — bu yalnızca örneklere ait ve kurulmuyor.
- **On bir yeni örnek** (`examples/README.md` tam listeyi taşıyor):
  `minimal` (25 satırlık kopyala-çalıştır iskelet), `input` (tuş olayı ve tuş
  durumu farkı), `plasma`, `particles` (SPACE ile batch farkı canlı),
  `sprite_animation`, `physics_rope` (uç/birleşim stilinin hareket hâlindeki
  sınavı), `morph`, `paint` (fırça/palet/geri al), `breakout`,
  `camera_scroll`, `tilemap` (C ile eleme kapatılabiliyor).
- **`examples/games/collision_logic.h` ve 18 testi.** `breakout`'un çarpışma
  matematiği çizimden tamamen ayrık saf fonksiyonlar hâlinde — pencere
  açmadan sınanabiliyor. `tictactoe_logic.h` ile aynı kalıp.
- **Yay, dilim (pie) ve kiriş (chord).** `Painter::DrawArc`, `DrawPie` /
  `FillPie`, `DrawChord` / `FillChord`. Açı birimi **derece**; 0° = +x ekseni
  ve açı `Rotate()` ile aynı yönde artar. Segment sayısı
  hem yarıçapa hem **taranan açıya** göre uyarlanır: 10°'lik bir yay, tam
  çemberle aynı segment bütçesini harcamaz. 360°'yi aşan taramalar kırpılır.
  Dilim merkezden üçgen fanı, kiriş ise yayın ilk noktasından fan ile
  doldurulur. 
- **Kesikli çizgi deseni.** `Pen::SetDashPattern({12, 6})` — uzunluklar sırayla
  çizili/boş, piksel cinsinden. Desen **yol boyunca sürekli** ilerler: köşede
  sıfırlanmaz, dolayısıyla bir kesik köşenin üzerinden geçebilir ve o parça
  kendi köşesinde birleşim alır. Tek sayıda uzunluk verilirse desen SVG'deki
  gibi iki tur boyunca kendini tersine çevirerek tamamlanır. Kesik; çizgi,
  polyline, dikdörtgen, daire, elips, poligon, yay, dilim ve kirişin
  **hepsinde** çalışır; her kesik parçası açık bir yol olduğu için kalemin uç
  stilini de alır. Desen `Pen` içinde sabit boyutlu bir dizide (en fazla
  `kMaxDashSegments` = 8) tutulur — `Pen`, `RenderState` üyesi olduğu ve her
  `Save`/`Restore` ile kopyalandığı için orada yığın tahsisi istenmiyor.
  15 yeni test.
- **Uç (cap) ve birleşim (join) stilleri.** `Pen::SetCapStyle` (`kButt` /
  `kSquare` / `kRound`) ve `Pen::SetJoinStyle` (`kRound` / `kMiter` /
  `kBevel`). Uç stili yalnızca **açık** geometriye uygulanır (`DrawLine`,
  `DrawPolyline`) ve orada da yalnızca iki uç noktaya; kapalı şekillerde uç
  yoktur. Miter, `kMiterLimit` (4.0, SVG varsayılanı) aşıldığında keskin
  açılarda sivri çıkıntı üretmemek için kendiliğinden bevel'a düşer.
  Varsayılanlar (`kButt` / `kRound`) önceki davranışı **birebir** korur — Qt'nin
  varsayılanları farklıdır ama onlara geçmek her mevcut çizimin görünümünü
  sessizce değiştirirdi. Demo: `examples/basics/primitives.cpp`. 15 yeni test.

- **`DrawImage` renk tonlaması (tint) ve aynalama.** Her üç `DrawImage` aşırı
  yüklemesi artık isteğe bağlı `const Color& tint` ve `ImageFlip flip`
  parametrelerini alıyor; varsayılanları (beyaz / `kNone`) mevcut davranışı
  birebir koruyor. Tint **vertex'te** taşınıyor — aynı dokuyu farklı renklerle
  çizmek batch'i bozmuyor (opaklık için aynısı geçerli değil). Aynalama, UV
  uçlarını takas ederek yapılıyor: ek vertex, ek draw call ya da negatif
  ölçekli transform gerektirmiyor ve hedef dikdörtgeni yerinde bırakıyor.
  Demo: `examples/graphics/images.cpp` (Bölüm 5b). 7 yeni test.
- **`ImageFlip` enum'u** (`sdl_painter/image.h`): `kNone`, `kHorizontal`,
  `kVertical`, `kBoth`.
- **`sdl_painter/export.h` ve `sdl_painter/app/export.h`:** derleme sırasında
  üretilen, kurulan public başlıklar. Static derlemede makrolar boş
  (`SDLPAINTER_STATIC_DEFINE`).
- **`packaging/conan-center/`:** Conan Center'a gönderilecek recipe'in repo
  içindeki kopyası (`config.yml`, `conandata.yml`, `conanfile.py`,
  `test_package/`) ve gönderim öncesi doğrulama adımları.
- **Public ABI testleri (`sdl_painter_abi_tests`, 14 test).** Yalnızca public
  başlıkları kullanan, `src/` include yoluna **sahip olmayan** ayrı bir hedef;
  kütüphaneye tüketicinin gördüğü yüzeyden bağlanır. Bir public sembol
  `SDLPAINTER_API` ile işaretlenmeyi unutulursa paylaşımlı derlemede link
  hatasıyla düşer — doğrulandı: `Image`'ten makro kaldırıldığında `LNK2019`
  veriyor. `IRenderer`'ın tüketici tarafında implemente edilip kütüphane
  içinden çağrılabildiğini de sınar (README'nin "yeni backend = yalnızca
  `IRenderer`" vaadi). Toplam test sayısı 286 → **300**.
- **`FrameStats` ve `Painter::GetFrameStats()`** (`sdl_painter/frame_stats.h`).
  Kare başına CPU/GPU süresi, draw call, batch, vertex ve GPU durum değişikliği
  sayacı. GPU süresi OpenGL backend'de `GL_TIME_ELAPSED` timer query ile,
  **çift tamponlu** olarak ölçülür (sonuç bir kare gecikmeli gelir; beklemek
  ölçtüğü şeyi bozan bir CPU–GPU senkronizasyonu yaratırdı). Vulkan'da 0.
- **Ekran üstü FPS / istatistik göstergesi (`Application`).**
  `AppConfig::stats_overlay` ile açılır, çalışma zamanında **F1** ile
  döngülenir (kapalı → FPS → detaylı). `AppConfig::show_fps_in_title` FPS'i
  pencere başlığında gösterir ve font gerektirmez. Gösterge, uygulamanın çizim
  durumunu (transform, clip, opaklık, font) bozmaz. Yazı tipi
  `AppConfig::stats_overlay_font` ile verilebilir; boşsa sistem fontları
  aranır, hiçbiri yoksa yalnızca ekran üstü kısım sessizce devre dışı kalır.
  `Application::Fps()` ve `Application::GetFrameStats()` değerleri programatik
  olarak da sunar. Demo: `examples/stats_overlay.cpp`.
- **`Painter::GetFont()`** — `Save`/`Restore` font'u kapsamaz (font bir çizim
  durumu değil, paylaşılan kaynaktır); geçici olarak başka fontla çizen kodun
  öncekini geri koyabilmesi için gerekli.
- **`examples/benchmarks/` — batch verimliliği ölçüm düzeneği.** 10 senaryo,
  CSV çıktısı, senaryo başına PNG ekran görüntüsü ve `--null` (GPU'suz) modu.
  Kütüphaneye sayaç eklenmeden, `Painter`'ın renderer enjekte eden ctor'u ve
  bir `CountingRenderer` sarmalayıcısıyla ölçer. Yukarıdaki transform
  değişikliği bu ölçümün sonucudur.
- **`build:shared` CI job'ı:** `BUILD_SHARED_LIBS=ON` ile derleme, ABI
  testlerinin koşturulması, kurulum ve `find_package` ile tüketici doğrulaması.
  Paylaşımlı yol daha önce hiç CI'da derlenmiyordu; kırık olduğu bu yüzden
  fark edilmemişti.

### Değişti
- **`hero` tanıtım sahnesi dört perdeli showcase olarak yeniden yazıldı.**
  Eski sahne tek karede yalnızca v1.0 yeteneklerini gösteriyordu; aradan geçen
  sürümlerde eklenen yol/Bézier, gradient, karıştırma modları, kesik desenleri,
  uç ve birleşim stilleri, viewport, doku filtresi, mesh warp ve çizim hedefi
  tanıtımda hiç görünmüyordu. Yeni sahne 3'er saniyelik dört perdeye bölünüyor
  (şekiller ve yollar · renk ve karıştırma · transform ve kırpma · görüntü ve
  metin), toplam 12 saniye; künye şeridinde canlı `FrameStats` sayacı var, yani
  276 ayrı transform'lu quad ekrandayken draw call sayısının tek haneli kaldığı
  GIF'in kendisinde görünüyor. Perde geçişi `SetOpacity` yerine renk alfası
  ölçeklenerek yapılıyor — opaklık GPU durumu ve batch'i kırardı, sayaç bunu ele
  verirdi. Döngü artık her animasyonun tam periyot tamamlamasına değil, perde
  sınırlarında sahnenin tamamen kararmasına dayanıyor. `doc/hero.gif` 12 fps /
  720 px ile yeniden üretildi.
- **`IRenderer`'a `GetLastGpuFrameMs()` eklendi** (saf sanal değil, varsayılan
  `0.0` döner). Kaynak düzeyinde geriye dönük uyumludur — mevcut
  implementasyonlar değişmeden derlenir — ancak **vtable büyüdüğü için ABI
  kırıcıdır**: kendi `IRenderer` implementasyonunu ayrı derlenmiş bir ikilikte
  taşıyan tüketicilerin yeniden derlemesi gerekir.
- **`RenderBatcher::PushTriangles` / `PushTexturedTriangles` artık bir
  `glm::mat3` transform parametresi alıyor.** Dahili başlık; public ABI'ye
  dokunmaz.
- `Font`'un varsayılan kurucusu satır içinden `.cpp`'ye taşındı. Sınıf dışa
  açık işaretlendiğinde derleyici bu kurucuyu her çeviri biriminde üretiyor ve
  temizlik yolu için `unique_ptr<GlyphAtlas>` yıkıcısını örneklemeye
  çalışıyordu — `GlyphAtlas` başlıkta eksik tip.
- **`sdl_painter_tests` paylaşımlı derlemede atlanıyor.** Bu takım beyaz kutu:
  `src/` altındaki dahili başlıkları include edip export edilmeyen sembolleri
  (`Tessellator`, `RenderBatcher`, `vk_detail::VkResultToString` …) doğrudan
  çağırıyor, dolayısıyla DLL üzerinden link edilemez — ve edilmemeli, dahili
  detaylar ABI yüzeyine çıkmamalı. CMake artık bunu bir link hatası duvarı
  yerine açık bir mesajla bildiriyor; public yüzeyi `sdl_painter_abi_tests`
  doğruluyor.
- MSVC uyarısı **C4251** susturuldu (`/wd4251`). Standart kütüphane üyesi taşıyan
  bir sınıfı dışa açarken kaçınılmaz; DLL ile tüketici aynı derleyici ve runtime
  ile derlendiğinde (paket yöneticisi bunu garanti eder) zararsız. Gerekçe
  `cmake/CompilerWarnings.cmake` içinde yazılı.

## [1.2.0] - 2026-08-20

### ⚠️ Kırıcı değişiklikler

Bu sürüm, `v1.1.0`'a göre iki noktada public API'yi kırıyor. İkisi de metin
çiziminin glyph atlasına geçişinden kaynaklanıyor.

- **`Glyph` artık texture'ın sahibi değil.** Eskiden her glyph kendi `Texture`
  nesnesini tutuyordu; artık glyph'ler ortak bir atlas sayfasında toplanıyor ve
  `Glyph`, o sayfaya bir `TextureHandle` ile atlas içindeki normalize UV bölgesini
  (`u0`, `v0`, `u1`, `v1`) tutuyor. Texture sahipliği atlasta.

  ```cpp
  // Eskiden
  struct Glyph { Texture texture; int32_t width, height, advance, bearing_x, bearing_y; };

  // Şimdi
  struct Glyph {
    TextureHandle texture{kInvalidTexture};
    float u0, v0, u1, v1;
    int32_t width, height, advance, bearing_x, bearing_y;
  };
  ```

  **Ne yapılmalı:** `Glyph::texture`'ı `Texture` gibi kullanan kod derlenmez.
  Glyph'i kendiniz çiziyorsanız, texture'ı handle üzerinden bağlayıp UV
  bölgesini kullanın; tüm atlas sayfası yerine yalnızca o dikdörtgen çizilmeli.

- **`IRenderer`'a saf sanal `UpdateTexture` eklendi.**

  ```cpp
  virtual void UpdateTexture(TextureHandle handle, int32_t x, int32_t y,
                             int32_t width, int32_t height,
                             const uint8_t* data) = 0;
  ```

  **Ne yapılmalı:** Kendi backend'ini yazan herkes, bu metodu implemente etmeden
  derleyemez. Sıkı paketlenmiş **RGBA8** veri (`width * height * 4` bayt) bekler
  ve var olan bir texture'ın alt bölgesini günceller. Atlas, sayfayı her yeni
  glyph'te yeniden yaratmak yerine bunu kullanıyor.

### Değişti
- **`DrawText` artık transform stack'i uyguluyor.** Döndürülmüş veya ötelenmiş
  metin ekran dışına düşüyor, yani hiç görünmüyordu. Metin de artık diğer
  şekillerle aynı `Save`/`Restore`/`Translate`/`Rotate`/`Scale` semantiğine tabi.
- **`Application::Width()` / `Height()` artık framebuffer piksel döndürüyor**,
  mantıksal pencere boyutunu değil. `AppConfig::high_dpi` kapalıyken (varsayılan)
  ikisi aynı; açıkken framebuffer, ekran ölçek faktörü kadar büyüktür. Çizim
  koordinatları piksel tabanlı olduğundan, HiDPI açıkken sabit sayılar yerine bu
  değerler üzerinden hesap yapılmalı.
- **Metin tek draw call ile çiziliyor.** Glyph'ler karakter başına ayrı texture
  yerine ortak bir atlas sayfasında toplanıyor.
- **CMake ve Conan dosyaları sadeleştirildi.** Kök `CMakeLists.txt` 266 satır
  küçüldü; tek seferlik kurulum işleri `cmake/` altındaki modüllere taşındı:
  `ProjectVersion.cmake`, `InstallRules.cmake`, `RuntimeDlls.cmake`,
  `RegenerateShaders.cmake`, `CodeCoverage.cmake`. Kök dosya artık yalnızca
  hedefleri tanımlıyor.
- **`OpenGL::GL` artık `PRIVATE`.** GL bir implementasyon detayı (glad zaten
  private linkliydi); `PUBLIC` olması Vulkan-only tüketiciyi de GL'e link etmeye
  zorluyordu.
- **Boş derleme birimi stub'ları kaldırıldı:** `src/brush.cpp`, `src/color.cpp`,
  `src/geometry.cpp`, `src/pen.cpp`. Dördü de hiçbir tanım içermiyordu; ilgili
  tipler tamamen header'da.
- **Örnek uygulamalar anlamlı isimler aldı:** `phase0_demo` → `hello_window`,
  `phase1_demo` → `primitives`, `phase2_demo` → `transforms`,
  `phase2b_demo` → `clipping`, `phase3_demo` → `images`, `phase4_demo` → `text`,
  `phase5a–5e_vulkan_*` → `vulkan_clear` / `vulkan_triangles` /
  `vulkan_textured` / `vulkan_demo` / `vulkan_text`,
  `phase6_app_demo` → `app_basics`, `phase7_game_demo` → `game_loop`,
  `phase8_tictactoe` → `tictactoe`. Faz numaraları projeyi içeriden bilmeyenler için pek bir şey 
  ifade etmiyordu. Demoları adıyla çalıştıran veya `--target` ile
  derleyen herkesi etkiler; kütüphane API'si değişmedi.
  Eski→yeni eşleme tablosu `examples/README.md` içinde ve her örneğin dosya
  başı yorumunda korunuyor (özellikle yazılarımda değindiğim için bu şekilde bıraktım). Pencere başlıkları da yeni adı öne alacak şekilde
  güncellendi, faz numarası parantez içinde duruyor
  (ör. `SDLPainter — vulkan_triangles (Phase 5b)`).
- **README yeniden yapılandırıldı:** `README.md` 550 → 242 satır. Derleme,
  script referansı, Docker, dizin yapısı, kalite kontrolleri ve CI/CD bölümleri
  `doc/building.md`, `doc/scripts.md` ve `doc/development.md`'ye taşındı
  (silinmedi); ADR tablosu `adr/README.md`'ye. `README.tr.md` aynı yapıya
  çekildi. Yeni bölümler: "Is SDLPainter for you?" ve
  "SDL_Renderer vs SDLPainter".

### Eklendi
- **Glyph atlası** (`src/glyph_atlas.{h,cpp}`, dahili): glyph'ler şerit
  paketlemesiyle ortak texture sayfalarına yerleştiriliyor. Metin artık karakter
  başına değil tek draw call ile çiziliyor.
- **`IRenderer::UpdateTexture(...)`:** var olan bir texture'ın alt bölgesini
  güncelleme (sub-image yükleme). Atlas gibi artımlı doldurulan texture'lar için
  gerekli. Her iki backend'de implemente edildi.
- **HiDPI desteği:** `AppConfig::high_dpi` (varsayılan `false`) penceyi
  `SDL_WINDOW_HIGH_PIXEL_DENSITY` ile açıyor; `Painter::SetDrawableSize(w, h)`
  ile çizim yüzeyi boyutu açıkça bildirilebiliyor. `Application`,
  `SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED` olayında bunu çağırıyor; ilk açık
  bildirimden sonra `Painter` kare başına pencere yoklamasını bırakıyor. Kendi
  olay döngüsünü yazan uygulamalar için otomatik yoklama emniyet ağı olarak
  duruyor (geriye dönük uyumlu).
- **Kalın çizgilerde yuvarlak birleşim (round join):** poligon ve polyline
  köşelerinde kalan boşluklar kapandı.
- **`Painter(std::unique_ptr<IRenderer>, int32_t, int32_t)` ctor'u:** renderer
  dışarıdan enjekte edilebiliyor. `Painter` artık sahte renderer ile, pencere
  açmadan test edilebiliyor — bu sınıfın daha önce hiç birim testi yoktu.
- **Kod kapsama ölçümü:** `cmake/CodeCoverage.cmake` (`ENABLE_COVERAGE`, gcov
  enstrümantasyonu), `linux-debug-coverage` preset üçlüsü ve her iki pipeline'da
  `quality:coverage` job'ı. Eşik **zorlanmıyor**; amaç görünürlük. Yalnızca
  `src/` + `include/sdl_painter/` ölçülüyor.
- **`SDLPAINTER_REQUIRE_FONT` sözleşmesi:** ortam değişkeni `1` iken font
  bulunamazsa metin testleri atlanmak (SKIP) yerine **düşüyor**. CI'da açık, ki
  font eksikliği sessizce kapsam kaybına dönüşmesin.
- **`examples/README.md`:** Her demonun ne gösterdiği, gerektirdiği bağımlılık
  (Vulkan / SDL_ttf), çalıştırma satırı ve faz eşleme tablosu.
- **`hero` örneği + GIF üretim script'leri:** README tanıtım görseli için
  koreografili, 8 saniyede tam bir periyot tamamlayan (yani kusursuz döngü
  yapan) sahne. `--dump-frames` ile gizli pencereden 240 PPM karesi yazıyor;
  `scripts/make-hero-gif.sh` / `Make-HeroGif.ps1` bunları iki geçişli palet ile
  GIF'e (isteğe bağlı mp4'e) çeviriyor. Ekran kaydına göre imleç/pencere
  çerçevesi karışmıyor ve kare atlaması olmuyor.
- **`doc/getting-started.md` ve `doc/architecture.md`:** `hizli-baslangic.md` ve
  `mimari-genel-bakis.md` dokümanlarının İngilizce sürümleri. Türkçe
  orijinaller yerinde; her iki tarafa karşılıklı dil değiştirici satırı eklendi.
- **`doc/building.md`, `doc/scripts.md`, `doc/development.md`, `adr/README.md`:**
  README'den taşınan içerik + eksik olanlar (`changelog-section.sh`, presetsiz
  CMake derlemesi, CI'ın shader tazelik ve paketleme job'ları).

### Düzeltildi
- **Vulkan'da kırpma hiç çalışmıyordu.** `Painter::SetClipRect` kare ortasında
  çağrıldığında Vulkan tarafında hiçbir `vkCmdSetScissor` üretilmiyordu; scissor
  ve viewport artık kare ortasında da komut buffer'ına yazılıyor.
- **Pencere simge durumuna küçültüldüğünde her karede validation hatası:** 0x0
  swapchain ile çizim deneniyordu. Artık kare atlanıyor ve geri dönüşte swapchain
  yeniden oluşturuluyor.
- **Vulkan acquire semaphore'u** artık frame-in-flight indeksiyle seçiliyor.
  Bağımsız sayaç fence beklemesiyle ilişkisiz olduğu için yeniden kullanım
  güvenliği garanti değildi.
- **`DestroyTexture` başına tam GPU stall:** `vkDeviceWaitIdle` yerine gecikmeli
  silme kuyruğu. Bir font kapatılırken atlas sayfası başına stall yaşanmıyor.
- **Vulkan texture yükleme hata yolundaki kaynak sızıntıları** kapatıldı.
- **OpenGL texture yüklemede buffer taşması:** unpack hizalaması
  (`GL_UNPACK_ALIGNMENT`) ayarlanmıyordu ve 1/2 kanallı görüntülerde kanal
  eşlemesi yanlıştı.
- **`Font` taşıma işlemleri glyph önbelleğini bırakmıyordu.** Move-assign
  sonrasında eski fontun karakterleri çizilebiliyordu.
- **Ear clipping tekrarlı köşe içeren poligonu sessizce yutuyordu** (ölçüldü:
  beklenen 9 vertex yerine 3). Girişte dejenere/tekrarlı nokta temizliği eklendi.
- **Daire ve elips segment sayısına üst sınır kondu.** Çok büyük yarıçapta segment
  sayısı sınırsız büyüyordu.
- **UTF-8 çözümleyici ayrıştırıldı ve sıkılaştırıldı** (`src/text_utf8.h`):
  overlong kodlamalar, surrogate aralığı ve aralık dışı kod noktaları artık
  reddediliyor.
- **Görüntü yüklemede boyut sınırı ve hata raporlaması:** `Image(file_path)` artık
  piksel verisini ayırmadan önce başlığı okuyup boyutu doğruluyor; yükleme
  hataları loglanıyor (eskiden sessizce geçersiz `Image` dönüyordu).
- **Kapsama job'ında gcov sürüm uyuşmazlığı** ve **özetin kırpılması** giderildi
  (`quality:coverage`, her iki pipeline).
- **GitLab `build:linux:release` job'ı yanlış artifact yolu veriyordu:**
  `build/linux-debug/libsdl_painter.a` → `build/linux-release/...`.
- **CI artifact glob'ları sessizce boş artifact üretecekti:** `examples/phase*`
  deseni yeni adlarla eşleşmiyordu ve GitHub tarafında `if-no-files-found: warn`
  olduğu için pipeline yeşil kalırdı. `examples/*` + `CMakeFiles`/`*.cmake`
  dışlaması olarak düzeltildi (`.github/workflows/ci.yml`, `.gitlab-ci.yml`).
- **README'nin CI/CD tablosu yanlıştı:** `quality:clang-format` "soft fail"
  yazıyordu; Faz 1.5'te hard-fail'e çevrilmişti. `build:standalone-cmake`,
  `package:conan-create` ve `quality:shader-freshness` job'ları da tabloda
  yoktu. Doğru hâli `doc/development.md`'de.
- **`doc/hizli-baslangic.md`** artık var olmayan `README.tr.md#script-referansı`
  bölümüne link veriyordu → `doc/scripts.md`.
- **`doc/hizli-baslangic.md`'deki klonlama komutu placeholder URL kullanıyordu**
  (`https://example.com/sdl-painter.git`) → gerçek repo adresi. Aynı dosyadaki
  Vulkan opsiyonu `-o sdl_painter/*:with_vulkan=True` yerine projenin her yerde
  kullandığı `-o "&:with_vulkan=True"` biçimine getirildi.
- **`doc/mimari-genel-bakis.md` var olmayan `transform.h`'yi listeliyordu**
  (ADR-007 ile `glm::mat3`'e geçilmişti). Bağımlılık grafiğinde SDL_ttf hâlâ
  opsiyonel görünüyordu ve GLM hiç yoktu — ikisi de zorunlu listeye alındı.

### Test
- Test takımı **187'den 286 teste**, 16'dan 18 test dosyasına çıktı (286/286
  geçiyor).
- **`Painter` ilk kez test kapsamında.** Kütüphanenin tüm public yüzeyini taşıyan
  bu sınıfın hiç birim testi yoktu; ctor'u renderer'ı kendi içinde yarattığı için
  sahte renderer enjekte edilemiyordu. Yeni ctor bunu açtı
  (`tests/test_painter.cpp`, 44 test): transform stack, scissor Y-flip, UTF-8
  çözümleme, hizalama ve opacity yayılımı artık doğrulanıyor.
- **`tests/test_frame_render.cpp`:** gerçek `OpenGLRenderer`/`VulkanRenderer`
  örnekleyip offscreen kare çiziyor. Daha önce test takımında gerçek renderer
  ayağa kaldıran hiçbir test yoktu.
- Vulkan tarafı için `tests/test_vk_check.cpp`; `tests/test_font.cpp`,
  `test_tessellator.cpp` ve yeni `test_text_utf8.cpp` genişletildi.
- Ölçülen satır kapsaması: **%76** (`src/` + `include/sdl_painter/`).

## [1.1.0] - 2026-08-09

### Değişti
- **Shader'lar binary'ye gömüldü:** GLSL kaynakları ve derlenmiş SPIR-V modülleri
  artık kütüphane binary'sine gömülüyor; çalışma zamanında hiçbir shader dosyası
  aranmıyor. Daha önce shader dizini ya kaynak ağacı yolu olarak binary'ye
  gömülüyor ya da `SDL_GetBasePath()` ile executable'ın yanında aranıyordu — bu,
  paketlenmiş kütüphaneyi kullanan tüketicinin shader dosyalarını kendi çıktı
  dizinine elle kopyalamasını gerektiriyordu (bkz. ADR-009).
- **Vulkan SDK artık derleme için gerekli değil:** SPIR-V çıktıları
  `src/vulkan/shaders/spirv/` altında repo'da tutuluyor. `glslc` yalnızca shader
  kaynağını değiştirenler için, `-DSDLPAINTER_REGENERATE_SHADERS=ON` ile
  gelen `regenerate_shaders` hedefinde aranıyor.
- **`VulkanRenderer::Initialize()` pipeline hatasında artık başarısız oluyor:**
  Eskiden uyarı loglayıp devam ediyor, kullanıcıya sebebi log'a gömülü boş bir
  pencere bırakıyordu. Shader dosyaları artık eksik olamayacağı için bu sessiz
  degradasyonun gerekçesi kalmadı; hata `Painter::IsValid() == false` olarak
  yukarı taşınıyor.
- **Sürüm sabitleri makrodan `constexpr`'a taşındı:** `SDLPAINTER_VERSION_*`
  makroları kaldırıldı; yerine `sdl_painter::kVersionMajor/Minor/Patch` ve
  `kVersionString` geldi. Sürüm artık `#if` içinde kullanılamaz; tüketici
  tarafında sürüm kısıtı `find_package(sdl_painter 1.1 CONFIG REQUIRED)` ile
  ifade edilir.

### Eklendi
- **CMake kurulum/export katmanı:** Kütüphane artık `cmake --install` ile
  kurulabiliyor ve dışarıdan tüketilebiliyor:
  ```cmake
  find_package(sdl_painter CONFIG REQUIRED)
  target_link_libraries(my_app PRIVATE sdl_painter::sdl_painter)
  ```
  `FetchContent`/`add_subdirectory` ile ekleyen tüketici de **aynı hedef ismini**
  kullanır (`sdl_painter::sdl_painter`, opsiyonel uygulama çatısı için
  `sdl_painter::app`). Örnekler, testler ve Doxygen hedefi yalnızca SDLPainter
  üst proje iken varsayılan olarak açık.
- **Başsız Vulkan desteği (`VK_EXT_headless_surface`):** SDL, offscreen/dummy
  video sürücüsünde surface'i bu extension ile oluşturur ama onu
  `SDL_Vulkan_GetInstanceExtensions()` listesinde bildirmez; sonuç, GPU'suz
  ortamda `SDL_Vulkan_CreateSurface`'in başarısız olmasıydı. Extension mevcutsa
  `VkContext` artık kendisi ekliyor. Böylece Vulkan backend'i CI ve WSL2 gibi
  ekransız ortamlarda gerçekten test edilebiliyor. Extension bulunmayan
  platformlarda sessizce atlanır, masaüstü davranışı değişmez.
- **Renderer smoke testleri (`tests/test_renderer_smoke.cpp`):** Gerçek bir
  backend ayağa kaldırıp gömülü shader'ların sürücü tarafından kabul edildiğini
  doğrular. Daha önce hiçbir test gerçek bir renderer örneklemiyordu; shader'lar
  test kapsamı dışındaydı. Pencere/context kurulamayan ortamlarda testler
  başarısız olmak yerine atlanır.

- **SPIR-V güncellik kontrolü:** `cmake -P cmake/CheckShaderFreshness.cmake`,
  GLSL kaynakları değişip `.spv` çıktıları yeniden üretilmediğinde hata verir.
  Karşılaştırma `sources.sha256` manifesti üzerinden yapıldığı için `glslc`
  veya Vulkan SDK gerektirmez. CI'da `quality:shader-freshness` job'ı koşar.
- **CI:** Windows'ta artık `ctest` çalışıyor (daha önce yalnızca derleniyordu);
  `build:standalone-cmake` (preset'siz derleme + kurulum + tüketici doğrulaması)
  ve `package:conan-create` job'ları eklendi; clang-format kontrolü zorunlu
  hâle getirildi.

- **Uygulama çatısı (`sdl_painter_app`):** SDL pencere/olay-döngüsü
  boilerplate'ini soyutlayan ayrı static kütüphane. `sdl_painter::Application`'dan
  türeyip `OnRender`/`OnUpdate`/`OnKeyDown` gibi sanal metotları override etmek
  yeterli; SDL init, pencere, GL context, olay döngüsü ve yıkım çatı tarafından
  yönetilir. SDL'den bağımsız `Key`/`KeyEvent`/`MouseButtonEvent` tipleri ve ileri
  kullanım için `OnRawEvent(const SDL_Event&)` kaçış kapısı sunar. Core
  `sdl_painter` saf çizim API'si olarak korunur (bkz. ADR-008).
- **Zamanlama modları:** `AppConfig::timing` ile `kVariable` (varsayılan, değişken
  delta-time) veya `kFixed` (sabit adımlı deterministik `OnUpdate` +
  `OnRender(Painter&, alpha)` interpolasyonu — Game Programming Patterns "play
  catch up"). Ayrıca `target_fps` ile `SDL_DelayNS` tabanlı kare hızı freni.
- **Örnekler:** `phase6_app_demo` (kVariable), `phase7_game_demo` (kFixed +
  interpolasyon, seken top) ve `phase8_tictactoe` (fare girdisi + durum makinesi)
  çatının kullanımını gösterir.
- **Dokümantasyon:** Ana `README.md` İngilizce oldu; Türkçe sürüm `README.tr.md`
  olarak ayrıldı.

### Düzeltildi
- **Conan paketi doğru üretiliyor:** `package()` artık binary glob'lamak yerine
  projenin install kurallarını kullanıyor (`cmake.install()`), `LICENSE` pakete
  kopyalanıyor ve GTest artifact'ları pakete sızmıyor. `gtest` bağımlılığı
  `test_requires`'a alındı, böylece tüketicinin bağımlılık grafiğine girmiyor.
  `test_package` çalıştırma yolu düzeltildi.
- **lavapipe ICD yolu:** CI ve Dockerfile `lvp_icd.x86_64.json` yolunu sabit
  yazıyordu; mesa sürümüne göre dosya adı `lvp_icd.json` olabiliyor ve yanlış
  yol verildiğinde Vulkan loader hiçbir sürücü yüklemiyor — Vulkan testleri
  sessizce atlanıyordu. Yol artık çalışma anında çözülüyor.
- **Alt proje olarak eklenince yanlış dizin kullanımı:** `cmake/Docs.cmake`,
  `cmake/Doxyfile.in`, `cmake/InsourceGuard.cmake` ve `tests/CMakeLists.txt`
  `CMAKE_SOURCE_DIR` kullanıyordu; `add_subdirectory` ile eklendiğinde bu
  tüketicinin kök dizinini gösterip configure'u kırıyordu. `PROJECT_SOURCE_DIR`
  ile değiştirildi.
- **MinGW link seçenekleri dizin kapsamındaydı:** `add_link_options()` ile
  verilen `-static-libgcc -static-libstdc++`, projeyi alt proje olarak ekleyen
  tüketicinin kendi hedeflerini de etkiliyordu. Hedef kapsamına alındı.

### Kaldırıldı
- `sdlpainter_copy_shaders()` / `sdlpainter_copy_vulkan_shaders()` CMake
  yardımcıları ve bunların MSVC paralel-build race condition'ı için gereken
  merkezi kopyalama target'ları — shader'lar gömülü olduğu için gereksiz.
- `VulkanRenderer::ResolveShaderDir()`; `VulkanPipeline` ve
  `VulkanTexturedPipeline` `Init()` imzalarından `shader_dir` parametresi.

## [1.0.0] - 2026-05-17

İlk yayın. Önceki commit'ler.

### Eklendi
- **CI/CD — Windows build:** GitLab SaaS MSVC runner üzerinde Debug + Release
  Windows derlemesi pipeline'a dahil edildi.
- **Docker — Windows cross imajı:** MinGW tabanlı `windows-cross` stage
  yeniden düzenlendi; eksik dosyalar eklendi.
- **CI/CD — Conan önbellek paylaşımı:** Pipeline aşamaları arasında Conan
  paket önbelleği artifact olarak aktarılıyor; tekrar indirme süresi azaldı.
- **Painter API:** Yüksek seviye 2B çizim arayüzü
  (`SetPen`, `SetBrush`, `DrawLine`, `DrawRect`, `FillRect`, `DrawCircle`,
  `FillCircle`, `DrawEllipse`, `FillEllipse`, `DrawPolygon`, `FillPolygon`,
  `DrawPolyline`, `DrawImage`, `DrawText`).
- **Transform stack:** `Save`/`Restore` ile state yönetimi; `Translate`,
  `Rotate`, `Scale`, `ResetTransform` 3×3 affine matris üzerinden.
- **Backend soyutlama:** `IRenderer` arayüzü; yeni backend eklemek için
  yalnızca bu arayüzü implement etmek yeterli.
- **OpenGL backend:** OpenGL 3.3 core profile, GLAD loader, custom shader
  pipeline (basic + textured).
- **Vulkan backend:** Vulkan 1.1, manuel pipeline yönetimi, frame
  synchronization, SPIR-V shader derleme (offline `glslc`).
- **Tessellator:** Backend-agnostic vertex üretimi; daire/elips adaptif
  segment sayısı, konkav poligonlar için ear-clipping triangulation,
  geometry-based quad kalın çizgi.
- **Render batcher:** Aynı state ile gelen draw call'ları tek GPU çağrısına
  birleştirir.
- **Görsel desteği:** stb_image üzerinden PNG/JPG yükleme; RAII `Texture`
  sarmalayıcı; tint ile çizim.
- **Metin:** SDL_ttf 3.x üzerinden glyph cache ve hizalama (left/center/right).
- **Clip:** Scissor tabanlı dikdörtgensel kırpma.
- **Opaklık:** Global opacity (`SetOpacity`) shader uniform'u üzerinden.
- **Build sistemi:** CMake 3.20+ presets (linux/windows × debug/release/asan)
  ve Conan 2 paket yönetimi.
- **CI/CD:** GitLab pipeline — Linux (Debug/Release/ASan) + Windows
  (Debug/Release) build, GTest unit test koşumu, clang-format zorunlu kontrol,
  clang-tidy (soft fail), Doxygen → GitLab Pages.
- **Docker:** `builder`, `ci`, `windows-cross` (MinGW) stage'leri.
- **Dokümantasyon:** Doxygen yapılandırması, doxygen-awesome-css teması,
  Türkçe teknik dokümantasyon (`doc/`) ve karar kayıtları (`adr/`).
- **Test:** GTest birim testleri (color, geometry, pen/brush, transform,
  tessellator, image, render batcher, texture upload).
- **Örnekler:** `phase0`–`phase5e` demo uygulamaları (OpenGL ve Vulkan).

### Değişti
- **SDL_ttf zorunlu bağımlılık:** `with_text` Conan opsiyonu kaldırıldı;
  SDL_ttf 3.x artık her zaman derlenir. Metin desteği opsiyonel değil.
- **Vulkan altyapısı:** `vk_context`, `vk_swapchain`, `vk_frame_sync`
  bileşenlerinde kararlılık düzeltmeleri uygulandı.

### Düzeltildi
- Derleyici uyarıları (`-Wall -Wextra`) tamamen giderildi.
- clang-format-18 ve clang-tidy-18 uyumsuzlukları çözüldü.

### Bilinen Eksikler
- Path, Bezier eğrileri ve gradient desteklenmez (v2 kapsamı).
- Kırpma yalnızca dikdörtgenseldir (scissor); path-based kırpma yoktur.
- Anti-aliasing yalnızca MSAA (donanım/sürücü ayarına bağlıdır).
- Tek thread'den kullanılması beklenir.
- Test ve coverage çıktıları henüz gitlab'a verilmiyor.

### Bağımlılıklar
- C++17, CMake ≥ 3.20, Conan ≥ 2.0
- SDL 3.2, SDL_ttf 3.2, GLAD 0.1.36, stb (cci.20240531), spdlog 1.15
- Opsiyonel: Vulkan loader/headers 1.3.290
