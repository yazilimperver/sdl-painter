# ADR-002: OpenGL + Vulkan Çift Backend Kararı

- **Durum:** Kabul edildi
- **Tarih:** 2026-04-17

## Bağlam

ADR-001 ile OpenGL 3.3 Core Profile temel backend olarak seçildi. Bununla birlikte
SDLPainter'ın orta vadeli hedefleri arasında modern düşük seviyeli Vulkan tarzı GPU
API'lerini kullanma hedefi de mevcut. Ayrıca bazı platformlarda OpenGL sürücü kalitesi düşmüş durumda (örn. Windows güncel sürücüler Vulkan'a öncelik veriyor; macOS Apple Silicon'da
OpenGL deprecate edildi). Bu nedenle **tek bir backend'e bağımlı kalmak**
projeyi hem öğrenme hem de uzun ömür açısından kısıtlayabileceğini değerlendirdim. 

Karar noktası: Tek backend mi, çok backend mi? Çoksa hangileri?

### Alternatifler

| Yaklaşım | Artılar | Eksiler |
|----------|---------|---------|
| Yalnızca OpenGL 3.3 | En basit kod tabanı; daha az test yüzeyi | Modern API deneyimi yok; uzun vadede platform riski |
| Yalnızca Vulkan | Tek API; modern her şey | Yüksek başlangıç maliyeti; GL'i bilen kullanıcı eşiği yüksek |
| OpenGL + Direct3D 11/12 | Windows'ta birinci sınıf | Cross-platform değil; Linux/macOS'ta ek backend gerekir |
| OpenGL + Metal | macOS'ta birinci sınıf | Apple dışında işe yaramaz; Windows ekstra backend ister |
| **OpenGL 3.3 + Vulkan 1.1** | Cross-platform; eski + modern API; aynı tessellator | İki backend = iki kez test |
| OpenGL + WebGPU | Geleceğe yatırım | Standart hâlâ olgunlaşıyor; native runtime sınırlı |

## Karar

SDLPainter **iki birinci sınıf backend** sunar:

1. **OpenGL 3.3 Core Profile** — varsayılan, taşınabilir, düşük giriş eşiği
2. **Vulkan 1.1** — opsiyonel (Conan `with_vulkan=True`), modern explicit API

Backend seçimi runtime'da `Painter` constructor'ında yapılır:

```cpp
Painter p(window, RendererBackend::kOpenGL);   // varsayılan
Painter p(window, RendererBackend::kVulkan);   // opsiyonel
```

İki backend de `IRenderer` arayüzünü implement eder; yukarı katmandaki
`Painter`, `Tessellator`, `RenderBatcher` ve `RenderState` backend
bilmez.

## Gerekçe

- **Soyutlama uygulaması:** `IRenderer` arayüzünün gerçek bir kullanım testi —
  birden fazla bağımsız implementasyon olmadan arayüz "tek implementasyona
  şekillenir" tuzağına düşebilir. Vulkan'ın açıkça yapmak zorunda olduğu
  şeyler (sync, memory, pipeline) OpenGL'in örtük yaptığı işlerdir; ikisini
  desteklemek soyutlamayı disipline eder.
- **Cross-platform yelpaze:** Linux/Windows'ta her ikisi de birinci sınıf
  çalışır. macOS için Vulkan, MoltenVK üzerinden çalıştırılabilir
  (gelecek hedef; şu an aktif desteklenmiyor).
- **Öğrenme/pedagojik değer:** Proje aynı zamanda QPainter benzeri bir API'nin
  iki farklı GPU API üzerinde nasıl uygulandığını gösteren referans olmayı
  hedefliyor. Vulkan olmadan bu hikâye eksik kalır.
- **Gelecekte üçüncü backend (D3D12, Metal, WebGPU) eklemek istenirse**
  arayüz iki bağımsız implementasyon ile zaten "test edilmiş" durumdadır.
- **Vulkan'ı opsiyonel tutmak:** Vulkan SDK ve loader bağımlılıkları
  istemeyen kullanıcı için (`with_vulkan=False`) varsayılan yapıdır;
  ekstra ağırlık yalnızca isteyen taşır. Ayrıca ileride 1.4 gibi sürümlere geçişte değerlendirilecektir.

## Sonuçlar

### Public API

- `enum class RendererBackend { kOpenGL, kVulkan }` — `include/sdl_painter/renderer.h`
- `IRenderer* CreateRenderer(RendererBackend backend)` — factory
- `Painter` constructor backend parametresi alır; geri kalan API backend'den bağımsız

### Build sistemi

- `SDLPAINTER_WITH_VULKAN` CMake seçeneği (varsayılan `OFF`).
- Conan seçeneği `with_vulkan=True` etkinleştirildiğinde:
  - `vulkan-loader/1.3.290.0` ve `vulkan-headers/1.3.290.0` çekilir
  - `vulkan_renderer.cpp`, `vulkan_pipeline.cpp`, `vulkan_buffer.cpp`,
    `vulkan_texture.cpp`, `vk_*.cpp` derlemeye dahil olur
  - `glslc` ile shader'lar offline SPIR-V'ye derlenir
  - `SDLPAINTER_HAS_VULKAN` makrosu tanımlanır

### Kod organizasyonu

```
src/opengl/   ← OpenGL 3.3 implementasyonu (her zaman derlenir)
src/vulkan/   ← Vulkan 1.1 implementasyonu (opsiyonel)
```

İki dizin birbirinden bağımsızdır; ortak kod `src/` kökündedir (`painter.cpp`,
`tessellator.cpp`, `render_batcher.cpp`).

### Test ve CI

- GitLab CI matrisi her iki backend'i de aktif derler (`with_vulkan=True`).
- Linux test job'u `lavapipe` (mesa-vulkan-drivers) ile headless çalışır —
  fiziksel GPU gerektirmez.
- Windows test job'u SDL `offscreen` driver kullanır.
- Unit testlerin önemli bir kısmı `MockRenderer` üzerinden GPU'suz çalışır.

### Maliyet (kabul edilen)

- İki backend, iki kez test yüzeyi (birim test + headless smoke).
- Shader'lar iki form: GLSL (`src/opengl/shaders/`) + SPIR-V
  (`src/vulkan/shaders/`, build-time `glslc` ile derlenir).
- Vulkan backend daha karmaşık (manuel pipeline, descriptor set, swapchain);
  yeni geliştirici için giriş eşiği yüksektir → mimari dokümantasyonu
  (`doc/backend-ic-yapisi.md`) bu yüzden gereklidir.

### Reddedilen seçenekler (şimdilik)

- **WebGPU:** Native runtime (Dawn, wgpu-native) henüz olgunlaşma sürecinde;
  v2 değerlendirmesi için açık tutulmuştur.
- **D3D12 / Metal:** Cross-platform değil; mevcut iki backend'in kapsamadığı
  bir kullanım senaryosu doğmadıkça eklenmeyecektir.

## İlgili Kararlar

- **ADR-001:** OpenGL 3.3 Core Profile seçimi (temel backend)
- **ADR-004:** Tessellator'ın backend-agnostic tasarımı (bu kararın
  uygulanabilir olmasını sağlar)
