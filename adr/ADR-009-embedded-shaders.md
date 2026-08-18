# ADR-009: Shader'ların Binary'ye Gömülmesi

- **Durum:** Kabul edildi
- **Tarih:** 2026-08-08

## Bağlam

Bu zamana kadar shader'lar, çalışma zamanında diskten okunuyordu. İki backend'de de aynı desen
vardı:

- **OpenGL:** `SDLPAINTER_OPENGL_SHADER_DIR` makrosu build sırasında
  `${CMAKE_CURRENT_SOURCE_DIR}/src/opengl/shaders` olarak tanımlanıyor, yani
  **geliştirici makinesinin mutlak yolu binary'ye gömülüyordu**. Makro yoksa
  (Windows'ta bilinçli olarak tanımlanmıyordu) `SDL_GetBasePath() + "shaders/"`
  fallback'i devreye giriyordu.
- **Vulkan:** Aynı şey `.spv` dosyaları için — `SDLPAINTER_VULKAN_SHADER_DIR`
  ya da `SDL_GetBasePath() + "vulkan_shaders"`.

Bu, repo içinde çalışıyordu: `sdlpainter_copy_shaders()` ve
`sdlpainter_copy_vulkan_shaders()` CMake yardımcıları shader dizinlerini her
örnek executable'ın yanına kopyalıyordu. MSVC'de paralel build sırasında beş
Vulkan exe'sinin aynı dizine aynı anda yazması race condition yarattığı için
ayrıca merkezi kopyalama target'ları gerekmişti.

Sorun, kütüphane **repo dışından** kullanıldığında ortaya çıkıyor:

1. Gömülü mutlak yol başka makinede mevcut değil.
2. `SDL_GetBasePath()` **tüketicinin executable'ının** dizinini döndürür. Yani
   SDLPainter'ı Conan/vcpkg/sistem kurulumu ile alan herkesin, kütüphanenin
   shader dosyalarını kendi çıktı dizinine elle kopyalaması gerekiyordu. Bu durum bazı koşullar "Failed to build basic shader" hatasına sebebiyet verebilmektedir.
3. Build makinesinin yolunun pakete sızması, Conan Center'ın relocatable paket
   kuralını ihlal ediyor. En önemli konularda birisi de bu. Bu kütüphanenin conan ile dağıtılmasını hedefliyoruz.
4. Vulkan tarafında `glslc` build-time bağımlılığıydı; Vulkan SDK kurulu
   olmayan bir ortamda (ör. Conan Center derleyicileri) SPIR-V hiç üretilemiyordu.

| Kriter | Diskten okuma (mevcut) | Kaynak dizinini pakete kur | Binary'ye göm (seçilen) |
|--------|------------------------|-----------------------------|--------------------------|
| Tüketici ek dosya kopyalar mı | Evet | Hayır | Hayır |
| Paket relocatable mı | Hayır | Kısmen (yol çözümü gerekir) | Evet |
| Runtime dosya G/Ç | Var | Var | Yok |
| Shader dosyası kaybolursa | Sessiz başarısızlık | Sessiz başarısızlık | İmkânsız |
| Vulkan SDK build'de gerekli mi | Evet | Evet | Hayır |
| Shader'ı kullanıcı değiştirebilir mi | Evet | Evet | Hayır (yeniden derleme gerekir) |

## Karar

Shader'lar kütüphane binary'sine gömülür. Çalışma zamanında hiçbir shader
dosyası aranmaz.

- **GLSL:** `cmake/EmbedShaders.cmake` içindeki `sdlpainter_embed_glsl()`,
  kaynakları ham string literal olarak bir başlığa yazar
  (`sdl_painter::detail::kBasicVert` vb.). Mevcut
  `ShaderProgram::Build(vert_src, frag_src)` API'si zaten kaynaktan derliyordu;
  yeni bir arayüz gerekmedi.
- **SPIR-V:** Derlenmiş `.spv` çıktıları `src/vulkan/shaders/spirv/` altında
  **repo'da tutulur**. `sdlpainter_embed_spirv()` bunları `uint32_t` dizisi
  olarak gömer.
- **`glslc`** yalnızca `-DSDLPAINTER_REGENERATE_SHADERS=ON` ile gelen
  `regenerate_shaders` hedefinde aranır. Shader kaynağını değiştiren geliştirici
  bu hedefi çalıştırır ve üretilen `.spv` dosyalarını commit'ler.

Üretim configure aşamasında yapılır; shader dosyaları `CMAKE_CONFIGURE_DEPENDS`
listesine eklendiği için kaynak değiştiğinde CMake kendini yeniden çalıştırır.

## Gerekçe

**Neden `uint32_t` dizisi, `unsigned char` değil?**
`VkShaderModuleCreateInfo::pCode` zaten `const uint32_t*` bekler ve 4 bayt
hizalama şarttır. Diziyi doğrudan `uint32_t` olarak üretmek hizalamayı dilin
kendisiyle garanti eder ve önceki koddaki `reinterpret_cast<const uint32_t*>` +
`// NOLINT` çiftini tamamen kaldırır. Dosya bayt sırası little-endian
varsayılarak kelimelere çevrilir — bu varsayım zaten önceki `reinterpret_cast`
tabanlı kodda da vardı ve Vulkan'ın hedeflediği platformlarda geçerlidir.

**Neden `.spv` repo'ya commit'leniyor?**
Alternatif, `shaderc`'yi `tool_requires` olarak çekmekti. Bu, Conan Center'da
derlenebilirliği sağlar ama derleme süresini belirgin uzatır ve kırılganlık
ekler. `.spv` dosyaları küçük (448–1992 bayt) ve nadiren değişir. Bedeli,
üretilen çıktıyı kaynakla senkron tutma sorumluluğudur; `regenerate_shaders`
hedefi bunu tek komuta indirir.

**Neden kaynak dizinini pakete kurmak değil?**
Kurulum yolu çalışma zamanında yine çözülmek zorunda kalırdı (`SDL_GetBasePath`
tüketicinin exe'sini gösterdiği için işe yaramaz; mutlak kurulum yolu gömmek
paketi relocatable olmaktan çıkarır). Sorunu bir katman öteye taşırdı.

**Kaybedilen esneklik.** Kullanıcı artık shader'ı kütüphaneyi yeniden derlemeden
değiştiremez. Bu bilinçli: SDLPainter özel shader desteği sunmuyor, shader'lar
uygulama detayı. Özel shader ileride bir özellik olursa, doğru çözüm dosya
aramak değil, `Painter` API'sinden shader kaynağı geçirmektir.

## Sonuçlar

**Olumlu**

- Paketlenmiş kütüphane hiçbir ek dosya olmadan çalışır — Conan Center ve vcpkg
  dağıtımının önkoşulu.
- Build makinesinin yolu binary'ye sızmaz.
- Kütüphaneyi derlemek için Vulkan SDK gerekmez.
- Çalışma zamanında shader dosyası G/Ç'si ve "dosya bulunamadı" hata yolu yok.
- ~60 satır CMake kopyalama yardımcısı ve MSVC race condition çözümü silindi.
- `VulkanPipeline::Init()` / `VulkanTexturedPipeline::Init()` imzalarından
  `shader_dir` parametresi düştü; `VulkanRenderer::ResolveShaderDir()` ve
  `mShaderDir` üyesi kalktı.
- **`VulkanRenderer::Initialize()` artık pipeline kurulumu başarısız olursa
  `false` dönüyor.** Eskiden uyarı loglayıp `reset()` ederek devam ediyordu;
  gerekçesi `.spv` dosyalarının çalışma zamanında eksik olabilmesiydi. O senaryo
  imkânsız hâle geldiği için sessiz degradasyon kaldırıldı — `Initialize()`
  içindeki diğer tüm adımlar (context, swapchain, frame sync, vertex ring'ler)
  zaten sert hata veriyordu, pipeline'lar tek istisnaydı. Hata `Painter`
  üzerinden `IsValid() == false` olarak temiz şekilde yukarı taşınıyor.

**Olumsuz / dikkat edilecekler**

- `.spv` dosyaları repo'da binary olarak duruyor; kaynak GLSL ile senkron
  kalmaları elle sağlanır. `regenerate_shaders` çalıştırılmadan GLSL değişirse
  Vulkan tarafı sessizce eski shader'ı kullanır. **CI'da bir "SPIR-V güncel mi"
  kontrolü eklenmesi önerilir** (yeniden üret + `git diff --exit-code`).
- Shader değişikliği artık kütüphanenin yeniden derlenmesini gerektiriyor.
- Gömülü GLSL'de ham string sonlandırıcısı (`)SDLP"`) çakışırsa üretim
  bozulurdu; `sdlpainter_embed_glsl()` bunu configure aşamasında kontrol edip
  hata veriyor.

## Doğrulama

- Binary bayt taraması: `src/opengl/shaders` ve `src/vulkan/shaders` yolları
  **0** eşleşme; gömülü GLSL ve SPIR-V magic (`0x07230203`) mevcut.
- OpenGL örneği yanında `shaders/` dizini **olmayan** izole bir
  dizinde sorunsuz çalıştı.
- Vulkan örneği yanında `vulkan_shaders/` dizini
  **olmayan** izole bir dizinde, **validation layer'lar açıkken hatasız**
  çalıştı; her iki pipeline da başarıyla kuruldu.
- `PATH`'ten Vulkan SDK çıkarılmış ve `VULKAN_SDK` tanımsız bir ortamda,
  preset kullanmadan, `SDLPAINTER_WITH_VULKAN=ON` ile configure + derleme geçti.
- `tests/test_renderer_smoke.cpp` gerçek bir backend ayağa kaldırarak gömülü
  shader'ları doğruluyor. Mutasyon testiyle sınandı: `basic.frag` kasıtlı
  bozulduğunda test doğru mesajla başarısız oldu.
