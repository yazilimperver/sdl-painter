# Batch Verimliliği Ölçümü

`RenderBatcher`'ın hangi çizim desenlerinde işe yaradığını, hangilerinde
yaramadığını **değerlerle** gösteren uygulamadır/ölçümdür.

Buradaki hedefimiz: *"Farklı renklerde çok sayıda şekil çizerken
batch'lemenin faydası oluyor mu gerçekten?"* Cevap hem evet hem hayır oldu. 
"hayır" kısmını iyileştirmek adına transform'un **CPU tarafına taşınması** karar verdik.

## Nasıl ölçülüyoruz

Kütüphanenin direk içerisine sayaç eklemedik. `Painter`'ın hazır bir renderer kabul eden
ctor'u sayesinde (`Painter(std::unique_ptr<IRenderer>, w, h)`), gerçek
`OpenGLRenderer` bir sarmalayıcının içine konur (diğer bir ifade ile ilave bir katman):

```
Painter → RenderBatcher → CountingRenderer → OpenGLRenderer → GPU
                            (sayaçlar)
```

`CountingRenderer` (`counting_renderer.h`) yalnızca public API'ye bağlı;
`DrawTriangles` / `DrawTextured` çağrılarını, gönderilen vertex sayısını ve
`SetModelMatrix` / `SetOpacity` / `SetScissor` uniform-durum yüklemelerini
sayıyor, sonra çağrıyı olduğu gibi iletiyor.

**Kilit metrik `draw/kare`.** 1'e ne kadar yakınsa batch o kadar verimli.
2000 şekil için 2000 çıkıyorsa batch'leme o desende hiç çalışmıyor demektir.

> Uygulama içinde aynı sayaçlara `Painter::GetFrameStats()` ile de
> erişilebilir; `Application` tabanlı uygulamalarda F1 ekran üstü göstergeyi
> açar (bkz. `examples/app/stats_overlay.cpp`).

## Çalıştırma

```bash
cmake --build --preset windows-release --target batching_benchmark
./build/windows-release/examples/benchmarks/Release/batching_benchmark.exe \
    --shapes=2000 --frames=300 --csv=results/windows-release-opengl.csv
```

| Argüman | Varsayılan | Anlam |
|---------|-----------|-------|
| `--shapes=N` | 2000 | Kare başına çizilen şekil sayısı |
| `--frames=N` | 200 | Ölçülen kare sayısı (öncesinde 5 kare ısınma) |
| `--null` | kapalı | Pencere/OpenGL açma; yalnızca CPU yolunu ölç |
| `--csv=DOSYA` | — | Sonuçları CSV olarak da yaz |
| `--screenshot=DIZIN` | — | Her senaryonun çıktısını `<senaryo>.png` olarak kaydet |

`--null` modu GPU'yu tamamen devre dışı bırakır. `opengl` ile arasındaki
süre farkı **doğrudan draw call maliyetidir** — bu ikisini karşılaştırmak
ölçümün asıl amacı.

Notlar:
- Vsync `SDL_GL_SetSwapInterval(0)` ile kapatılır; aksi halde ölçülen şey
  ekranın tazeleme hızı olurdu.
- Yerleşim ve renkler sabit tohumlu bir LCG ile üretilir (`std::rand`
  implementasyona göre değişir, karşılaştırma bozulurdu).
- **Release derleme zorunlu.** Debug'da CPU yolu ~15× şişer ve draw call
  maliyeti gerçek oranından küçük görünebilir.
- Ekran görüntüsü, `CountingRenderer`'ın sunumdan önceki kancasıyla arka
  tampondan okunur (`screenshot.cpp`) — `examples/hero.cpp`'deki
  `glReadPixels` + `SDL_GL_GetProcAddress` yaklaşımının aynısı.

## Senaryolar

| Senaryo | Ne test ediyor | Görüntü |
|---------|----------------|---------|
| `fill_same_color` | Referans — ulaşılabilir en iyi durum | ![](../../doc/images/benchmarks/fill_same_color.png) |
| `fill_many_colors` | Şekil başına farklı renk | ![](../../doc/images/benchmarks/fill_many_colors.png) |
| `stroke_many_colors` | Kalem çerçevesi — tessellation maliyeti | ![](../../doc/images/benchmarks/stroke_many_colors.png) |
| `circles_many_colors` | Adaptif segmentli daire | ![](../../doc/images/benchmarks/circles_many_colors.png) |
| `translate_per_shape` | `Save`/`Translate`/`Restore` | ![](../../doc/images/benchmarks/translate_per_shape.png) |
| `translate_rotate_per_shape` | "Nesne başına yerel uzay" deseni | ![](../../doc/images/benchmarks/translate_rotate_per_shape.png) |
| `opacity_alternating` | `SetOpacity` (opaklık uniform) | ![](../../doc/images/benchmarks/opacity_alternating.png) |
| `clip_per_shape` | `SetClipRect` (scissor GPU durumu) | ![](../../doc/images/benchmarks/clip_per_shape.png) |
| `text_only` | Glyph atlası tek batch'e giriyor mu? | ![](../../doc/images/benchmarks/text_only.png) |
| `shapes_and_text` | Şekil ↔ metin mod değişimi | ![](../../doc/images/benchmarks/shapes_and_text.png) |

> `fill_many_colors` ile `translate_per_shape` görüntüleri **birebir aynı** —
> aynı kareyi üreten iki farklı çizim deseni. Transform CPU'ya taşınmadan
> önce biri 2, diğeri 2000 draw call harcıyordu. Ölçmeden bakıldığında
> görülemeyen fark tam olarak bu.

Görüntüler `--shapes=400` ile üretilmiştir (2000 şekilde okunaksız oluyor):

```bash
batching_benchmark --shapes=400 --frames=60 --screenshot=doc/images/benchmarks
```

## Sonuçlar

Ölçüm ortamı: Windows 11, MSVC Release, 1280×720, vsync kapalı,
2000 şekil × 300 kare. Ham çıktılar `results/` altında.

### Güncel

Her senaryo **aynı kareyi** üretir: 1280×720'lik alana sabit tohumlu bir LCG
ile serpiştirilmiş **2000 küçük şekil** — kenarı 6–16 piksel, her biri rastgele
renkli (metin senaryolarında aynı konumlara `"Ab"` yazısı). Yerleşim ve renkler
tüm senaryolarda birebir aynıdır; tek değişen şekillerin *nasıl* çizildiğidir
(renk değişimi, transform, opaklık, clip, metin). Böylece sayılardaki fark
doğrudan çizim deseninden gelir.

Sütunlar (**kare** = frame, yani ekrana bir kez çizilen tam görüntü):

| Sütun | Anlamı |
|-------|--------|
| `draw/kare` | Bir karede GPU'ya giden çizim çağrısı sayısı (`DrawTriangles` + `DrawTextured`). Batch verimliliğinin doğrudan ölçüsü: aynı 2000 şekil tek karede **2** çağrıya sığıyorsa batch çalışıyor, **2000** çağrıya çıkıyorsa şekil başına bir çağrı düşmüş, yani batch hiç çalışmamış demektir. |
| `vertex` | Bir karede gönderilen toplam vertex sayısı — 2000 şeklin üçgenlere bölünmüş hâli (dikdörtgen 6, daire onlarca vertex). Tessellation maliyetinin göstergesidir ve draw call sayısından bağımsızdır: aynı vertex yığını ya tek çağrıya sığar ya da onlarca çağrıya bölünür. |
| `ms (OpenGL)` | Gerçek GPU yoluyla kare süresi: `Begin()`…`End()` arasının 300 kare ortalaması (CSV'de `ms_avg`). |
| `ms (null)` | Aynı karenin `--null` ile ölçülmüş hâli: yalnızca `Painter` + `Tessellator` + `RenderBatcher` CPU maliyeti, hiçbir GPU çağrısı yok. |
| `fark` | `ms (OpenGL) − ms (null)`. Sürücüye/GPU'ya ödenen bedel; bu ölçümde pratik olarak **draw call maliyeti**. |

CSV'de üç sütun daha var: `model_uploads_per_frame` / `opacity_uploads_per_frame` /
`scissor_changes_per_frame` (kare başına uniform ve GPU durumu yükleme sayısı —
bir batch'in neden kırıldığını gösterirler) ve `ms_min` (en hızlı kare; arka plan
gürültüsünden en az etkilenen değer).

| Senaryo | draw/kare | vertex | ms (OpenGL) | ms (null) | fark = draw call maliyeti |
|---------|----------:|-------:|------------:|----------:|--------------------------:|
| `fill_same_color` | **2** | 12 000 | 0.32 | 0.19 | 0.13 |
| `fill_many_colors` | **2** | 12 000 | 0.38 | 0.20 | 0.18 |
| `stroke_many_colors` | 24 | 192 000 | 3.85 | 3.53 | 0.32 |
| `circles_many_colors` | 12 | 96 000 | 1.53 | 1.43 | 0.10 |
| `translate_per_shape` | **2** | 12 000 | 0.30 | 0.23 | 0.07 |
| `translate_rotate_per_shape` | **2** | 12 000 | 0.37 | 0.30 | 0.07 |
| `opacity_alternating` | **2000** | 12 000 | 8.35 | 0.20 | **8.15** |
| `clip_per_shape` | **2000** | 12 000 | 8.86 | 0.23 | **8.63** |
| `text_only` | 3 | 24 000 | 0.61 | 0.45 | 0.16 |
| `shapes_and_text` | **4000** | 36 000 | 19.07 | 0.70 | **18.37** |

### Transform CPU'ya taşınmadan önce

Karşılaştırma verisi: `results/windows-release-opengl-before-cpu-transform.csv`
(model matrisinin hâlâ bir shader uniform'u olduğu hâl).

Sütunlar yukarıdakiyle aynı, yalnızca iki ölçüm yan yana yazılmıştır; `kazanç`
= `ms (önce) / ms (sonra)` oranıdır.

| Senaryo | draw/kare (önce → sonra) | ms (önce → sonra) | kazanç |
|---------|--------------------------|-------------------|-------:|
| `translate_per_shape` | 2000 → **2** | 9.44 → **0.30** | **31×** |
| `translate_rotate_per_shape` | 2000 → **2** | 9.37 → **0.37** | **25×** |
| `fill_same_color` | 2 → 2 | 0.36 → 0.32 | — |

Yan etkiler: kare başına `SetModelMatrix` yüklemesi **2001 → 1**,
`translate_per_shape`'te gereksiz `ClearScissor` çağrısı **2000 → 0**.
Diğer senaryolarda ölçülebilir bir yavaşlama yok — vertex başına 4 çarpma
+ 2 toplama, tasarruf edilen draw call'ların yanında görünmüyor.

### Çıkarımlar

**1. Farklı renk batch'i kırmıyor.** `fill_same_color` ile
`fill_many_colors` aynı: 2 draw call. Renk `Vertex` içinde taşındığı için
(`render_batcher.cpp`, `basic.vert`'teki `a_color`) `SetBrush` bedava.
İki draw call'ın sebebi renk değil, `kMaxVertices = 8192` tavanı
(12 000 vertex → 2 batch).

**2. Transform de artık batch'i kırmıyor.** Model matrisi bir shader
uniform'u olduğu sürece her `Translate`/`Rotate` bir flush tetikliyordu.
Artık vertex'ler `RenderBatcher` içinde, rengin yazıldığı kopyalama
döngüsünde CPU'da dönüştürülüyor; model matrisi daima birim.

**3. Geriye kalan batch kırıcılar: opaklık, clip, mod değişimi.** Üçü de
gerçek GPU durumu:

| Kırıcı | Neden | Kaldırılabilir mi? |
|--------|-------|--------------------|
| `SetOpacity` | `u_opacity` uniform'u | Evet — vertex alfasına katlanabilir |
| `SetClipRect` | scissor box, GPU durumu | Hayır (shader tabanlı clip olmadan) |
| şekil ↔ metin | ayrı shader + VAO | Evet — tek shader + 1×1 beyaz texture |

**4. Draw call başına maliyet ~4.1 µs, ölçekle doğrusal**
(`opacity_alternating` üzerinden, tek batch kırıcısı o):

| şekil | draw/kare | ms |
|------:|----------:|---:|
| 500 | 500 | 2.09 |
| 2 000 | 2 000 | 8.35 |
| 8 000 | 8 000 | 32.27 |

**5. Şekil ↔ metin geçişi şu an en pahalı desen.** 2000 şekil + 2000 metin
= 4000 draw call → 19 ms. Metin tek başına (`text_only`) 3 draw call ile
0.61 ms; yani glyph atlası kendi işini yapıyor, sorun **karışım**.

**6. `stroke_many_colors` ve `circles_many_colors` farklı bir problem.**
Draw call sayıları düşük (24 / 12) ama süreleri yüksek — ve `--null`
modunda da yüksek. Burada darboğaz batch değil, **CPU tessellation**.
Batch'leme iyileştirmesi bu iki senaryoya dokunmaz.

## SDL_Renderer ile karşılaştırma

`renderer_comparison`, aynı sahneyi SDLPainter'la ve SDL3'ün kendi 2D
renderer'ı (SDL_Renderer) ile çizip kare süresini ölçer. Soru şu: *"SDL'in
yerleşik renderer'ı yerine SDLPainter kullanmanın bir hız bedeli var mı?"*
Yalnızca iki kütüphanenin de yapabildiği şeyler karşılaştırılır; kalın çizgi,
dönüşüm, gradient ve kenar yumuşatma SDL_Renderer'da olmadığı için kapsam
dışıdır (kenar yumuşatma bu ölçümde kapalı).

| Senaryo | SDLPainter | SDL_Renderer |
|---------|------------|--------------|
| `fill_rects` | `SetBrush` + `FillRect` | `SDL_SetRenderDrawColor` + `SDL_RenderFillRect` |
| `fill_circles` | `SetBrush` + `FillCircle` | SDL'de daire yok: her karede hesaplanan 16 dilimli üçgen fanı + daire başına bir `SDL_RenderGeometry` (vertex sayısı SDLPainter'ınkiyle aynı) |
| `sprites` | `DrawImage` (16×16 doku) | `SDL_RenderTexture` (aynı pikseller) |

Yerleşim `batching_benchmark` ile aynıdır (`bench_scene.h`): 1280×720 alanda
kenarı 6–16 piksel, şekil başına rastgele renk. İki kütüphanenin aynı kareyi
çizdiği `--screenshot` ile doğrulandı: `painter-gl` ile SDL `opengl` üç
senaryoda da piksel piksel aynı; `direct3d11` ile birkaç düzine kenar pikseli
farklı (rasterleştirme kuralları).

```bash
cmake --build --preset windows-release --target renderer_comparison
renderer_comparison --impl=painter-gl --shapes=2000 --csv=sonuc.csv
renderer_comparison --impl=sdl --sdl-driver=opengl --shapes=2000 --csv=sonuc.csv
```

| Argüman | Varsayılan | Anlam |
|---------|-----------|-------|
| `--impl=` | `painter-gl` | `painter-gl`, `painter-vk` ya da `sdl` |
| `--sdl-driver=` | SDL'in seçimi | SDL_Renderer arka ucu: `opengl`, `direct3d11`, `vulkan`… |
| `--shapes=N` | 2000 | Kare başına şekil sayısı |
| `--frames=N` | 300 | Ölçülen kare sayısı (öncesinde 30 kare ısınma) |
| `--csv=DOSYA` | yok | Sonuçları dosyaya **ekler**; farklı `--impl` koşuları tek dosyada birleşir |
| `--screenshot=DIZIN` | yok | Her senaryonun karesini PNG olarak kaydet (`painter-gl` ve `sdl`) |

**Ölçüm yöntemi.** Süre, sunum dahil tam karedir: `Painter::End` swap/present
yapar, SDL tarafında karşılığı `SDL_RenderPresent`. Vsync kapalıdır. Tabloda
her hücre, 300 karenin medyanının üç ayrı geçişteki medyanıdır; ham değerler
`results/windows-release-comparison-*.csv` içinde (`pass` sütunu).

Ortam: Windows 11 Pro, AMD Ryzen 5 3600, NVIDIA GeForce RTX 5060 Ti (sürücü
32.0.16.1088), MSVC 19.44 Release, SDL 3.4.16, 1280×720. İki OpenGL koşusunun
da bu kartta çizdiği `GL_RENDERER` ile kayıtlı.

### Kare süresi (ms, küçük daha iyi)

2000 şekil:

| Senaryo | painter-gl | SDL opengl | painter-vk | SDL vulkan | SDL direct3d11 |
|---------|-----------:|-----------:|-----------:|-----------:|---------------:|
| `fill_rects` | 0.43 | 0.42 | 0.35 | 0.43 | 0.42 |
| `fill_circles` | **1.53** | 1.88 | **1.39** | 2.00 | 1.80 |
| `sprites` | 0.49 | **0.32** | 0.38 | 0.38 | **0.33** |

50 000 şekil:

| Senaryo | painter-gl | SDL opengl | painter-vk | SDL vulkan | SDL direct3d11 |
|---------|-----------:|-----------:|-----------:|-----------:|---------------:|
| `fill_rects` | **5.6** | 9.4 | **5.3** | 9.6 | 9.5 |
| `fill_circles` | **34.4** | 46.4 | **31.5** | 46.6 | 46.3 |
| `sprites` | 7.6 | 8.9 | 6.7 | 7.3 | 7.1 |

### SDLPainter'ın draw call ve tahsis sayısı

| Senaryo | draw/kare (2000 / 50 000) | tahsis/kare (2000 / 50 000) | tahsis/şekil |
|---------|--------------------------:|----------------------------:|-------------:|
| `fill_rects` | 2 / 37 | 2 000 / 50 000 | 1 |
| `fill_circles` | 12 / 295 | 4 000 / 100 000 | 2 |
| `sprites` | 2 / 37 | 2 000 / 50 000 | 1 |

Tahsis sayısı programın global `operator new`'undan, ayrı bir 10 karelik
geçişte sayılır; süre geçişine atomik sayaç maliyeti girmez. Kütüphane
varsayılan olarak statik bağlandığı için onun tahsisleri de sayılır
(paylaşımlı derlemede Windows'ta sayılmaz). SDL kendi `SDL_malloc`'unu
kullandığı için SDL_Renderer için bu sütun yok.

### Çıkarımlar

**1. 2000 şekilde fark küçük ve senaryoya göre yön değiştiriyor.**
Dikdörtgende üç geçişin aralıkları üst üste biniyor (painter-gl 0.39–0.51 ms,
SDL opengl 0.39–0.52 ms); bu ölçekte ayırt edilebilir bir fark yok. Dairede
SDLPainter önde (1.53'e 1.88 ms). Sprite'ta SDL_Renderer önde (0.32'ye 0.49
ms); bu farkın nedeni ayrıca profillenmedi.

**2. 50 000 şekilde SDLPainter üç senaryoda da önde.** Dikdörtgende 5.6'ya 9.4
ms (aynı API'de yaklaşık 1,7 kat), dairede 34'e 46 ms, sprite'ta 7.6'ya 8.9 ms.
Sprite'ta SDL direct3d11 (7.1 ms) painter-gl'den biraz önde, painter-vk'nin
(6.7 ms) biraz gerisinde.

**3. Şekil başına heap tahsisi CPU maliyetinin yaklaşık yarısı.** Her
`FillRect` ve `DrawImage` bir, her `FillCircle` iki `std::vector` tahsisi
yapıyor (`Tessellator`'ın döndürdüğü vertex ve nokta dizileri); 50 000 şekilde
kare başına 50 000-100 000 tahsis. Payını görmek için bir deneyde (kodda değil)
`FillRect` her çağrıda yeni dizi almak yerine yeniden kullanılan bir tampona
yazdırıldı, üç geçişin medyanı:

| Ölçüm | 2000 şekil | 50 000 şekil |
|-------|-----------:|-------------:|
| CPU yolu, GPU yok (`batching_benchmark --null`, `fill_many_colors`) | 0.200 → 0.102 ms | 5.14 → 2.54 ms |
| Tam kare (`renderer_comparison`, painter-gl `fill_rects`) | 0.46 → 0.34 ms | 5.8 → 3.2 ms |

Yani 50 000 şekilde SDLPainter CPU'ya bağlı ve sürenin yaklaşık yarısı
tahsis. Yukarıdaki tablolar bu iyileştirme olmadan ölçüldü; tahsisleri
kaldırmak SDLPainter'ın en büyük CPU iyileştirme fırsatı.

**4. Karşılaştırmanın sınırları.**
- SDL tarafındaki daire, kullanıcının yazacağı en doğal kod: daire başına bir
  `SDL_RenderGeometry`. Bütün daireleri tek çağrıda toplamak daha hızlı
  olabilir; ölçülmedi.
- Ölçüm tek makinede ve yalnızca Windows'ta yapıldı. Başka GPU'da ve sürücüde
  oranlar değişebilir.
- SDLPainter'ın kalın çizgi, dönüşüm, gradient ve kenar yumuşatma gibi
  SDL_Renderer'da olmayan işleri burada yok; bu tablo yalnızca ortak alt
  kümenin maliyetini gösterir.

### Hız bir yana: SDLPainter ne kazandırır?

Bu ölçüm SDLPainter'ın "daha hızlı" olduğunu iddia etmek için değil, ortak
alt kümede belirgin bir hız bedeli ödenmediğini göstermek için var. SDLPainter'ı
seçmenin asıl gerekçesi, SDL_Renderer'da olmayan ve kullanıcının aksi halde
kendisinin yazacağı çizim kabiliyetleri:

| İhtiyaç | SDL_Renderer (SDL 3.4) | SDLPainter |
|---------|------------------------|------------|
| Daire, elips, yay, dilim, kiriş, yuvarlatılmış dikdörtgen | Yok; üçgenleri kullanıcı üretir (bu ölçümdeki 16 dilimli fan gibi) | Tek çağrı, yarıçapa göre uyarlanan segment sayısı |
| Konkav çokgen dolgusu | Yok; üçgenleme kullanıcıda | Ear clipping ile `FillPolygon` |
| Kalın çizgi ve kalem stili | Çizgiler 1 piksel, kalınlık parametresi yok | Kalınlık, birleşim (miter, round, bevel), uç (butt, square, round), kesik desen, dış kontur |
| Dönüşüm | Tüm çizime ölçek (`SDL_SetRenderScale`); döndürme yalnızca dokularda | `Save`/`Restore` yığınıyla `Translate`/`Rotate`/`Scale`, her şekle uygulanır ve batch'i kırmaz |
| Gradient ve eğri | Yok; vertex renkleriyle elle | Doğrusal ve radyal gradient, Bézier eğrili `Path` |
| Kenar yumuşatma | Yok | Backend'den bağımsız, `SetAntialiasing` (1.5.0 ile) |

Ortak alt kümede, yani SDL_Renderer'ın da doğrudan çizebildiği dikdörtgen ve
sprite'ta, 2000 şekilde dikdörtgende fark yok, sprite'ta SDL_Renderer önde;
50 000 şekilde SDLPainter önde (sprite'ta SDL direct3d11 ile başa baş). Daire gibi SDL'de kullanıcının kendisinin
kurması gereken şekillerde ise SDLPainter hem kodu hem süreyi kısaltıyor.
SDLPainter SDL'i gizlemiyor; pencere, olay ve ses gibi işlerde SDL'in
doğrudan kullanımı sürer.
