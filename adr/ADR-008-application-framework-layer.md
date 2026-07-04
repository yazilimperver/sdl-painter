# ADR-008: Uygulama Çatısı Katmanı (`sdl_painter_app`)

- **Durum:** Kabul edildi
- **Tarih:** 2026-07-04

## Bağlam

SDLPainter yalnızca çizim API'sini (`Painter`) sunar; pencere oluşturma, SDL
init, GL context, olay döngüsü ve yıkım kullanıcının sorumluluğundadır. Sonuç
olarak her uygulama ~40–50 satır aynı boilerplate'i tekrar eder: `InitLogger`,
`SDL_Init`, 5 GL attribute (3.3 core + MSAA), backend'e göre pencere flag'i,
Painter'ın pencereden önce yıkılması için scope dansı, `SDL_PollEvent` döngüsü,
`SDL_DestroyWindow` + `SDL_Quit`. Bu tekrar repo'daki 11 örnekte ve dış
uygulamalarda (ör. `uforces_viewer`) birebir görülüyor.

Kütüphaneyi kullananların SDL başlıkları ve ayarlarıyla uğraşmaması hedefleniyor.
Referans olarak uEngine4'teki `SdlApplication` (SDL2) incelendi; ancak o çatı
asset service, gamepad, touch, JSON config ve fixed-timestep ticker içeren ağır
bir yapı. Bu proje için çok daha ince bir soyutlama isteniyor.

| Kriter | Çatı yok (mevcut) | uEngine4 (listener + ticker) | İnce kalıtım (seçilen) |
|--------|-------------------|------------------------------|------------------------|
| Boilerplate | Her uygulamada ~50 satır | Gizli | Gizli |
| API modeli | — | Client interface + listener kaydı | `Application`'dan türet |
| Zamanlama | Elle | Fixed timestep + interpolasyon | Değişken delta-time |
| Bağımlılık | — | Asset/gamepad/touch/JSON | Sıfır yeni bağımlılık |
| Esneklik | Tam kontrol | Yüksek | Yeterli + ham olay kaçış kapısı |

## Karar

`sdl_painter_app` adında **ayrı bir static kütüphane** eklenir. Core
`sdl_painter` saf çizim API'si olarak dokunulmadan kalır; kendi döngüsünü isteyen
uygulamalar (ör. `uforces_viewer`) yalnızca core'a link etmeye devam eder.

- **Kalıtım tabanlı API:** Kullanıcı `sdl_painter::Application`'dan türeyip
  korumalı sanal metotları (`OnRender` zorunlu; `OnInit`/`OnUpdate`/`OnKeyDown`/
  fare/`OnResize`/`OnShutdown` opsiyonel) override eder. `Run()` → `int` (main).
- **Kendi olay tipleri + kaçış kapısı:** `Key`/`KeyModifier`/`MouseButton` enum'ları
  ve `KeyEvent`/`MouseButtonEvent`/... struct'ları SDL'den bağımsızdır. Public app
  başlıkları SDL başlığı **içermez**. İleri ihtiyaçlar için
  `virtual bool OnRawEvent(const SDL_Event&)` kaçış kapısı sunulur; başlıkta
  `union SDL_Event;` forward-decl kullanılır (SDL3'te `SDL_Event` bir union'dır,
  mevcut `struct SDL_Window;` deseniyle aynı).
- **Opsiyon yok:** `SDLPAINTER_BUILD_APP` gibi bir CMake/Conan opsiyonu
  eklenmez — katman iki `.cpp`, sıfır yeni bağımlılık ve static lib olduğundan
  kullanmayan tüketiciye maliyeti yoktur. Koşulsuz derlenir ve paketlenir.

## Gerekçe

- **Ayrı target:** Çizim ile ilgisi olmayan sorumlulukların (event loop, timing)
  core'a karışmasını önler; core'un saf ve testlenebilir kalmasını sağlar.
- **Değişken delta-time (`SDL_GetTicksNS`):** vsync yaygın durumu karşılar;
  fixed-timestep uEngine4'ün kaçınılmak istenen ağırlığıdır. `OnUpdate(dt)` büyük
  sıçramalara karşı 0.25 s'ye clamp edilir. Fixed-timestep gelecekte eklenebilir.
- **dtor tabanlı yıkım sıralaması:** Painter, `~Application()` içinde yıkılır.
  Türeyen sınıfın `Image`/`Font` üyeleri temel sınıf yıkıcısından **önce** yok
  edildiğinden, Painter'ın yaşam sözleşmesi (kaynaklar Painter'dan önce yıkılmalı)
  kullanıcı kaynakları düz üye olarak tutsa bile otomatik sağlanır.

## Sonuçlar

- Yeni dosyalar: `include/sdl_painter/app/{application,app_config,events}.h`,
  `src/app/{application.cpp,event_translator.h,event_translator.cpp}`.
- SDL keycode → `Key` çevirisi internal `event_translator` içinde; düz tam sayı
  imzasıyla pencere/context olmadan headless test edilebilir
  (`tests/test_event_translator.cpp`, `tests/test_app_config.cpp`).
- `examples/phase6_app_demo.cpp` çatıyı gösterir (~70 satır, phase2'nin ~250
  satırlık boilerplate'i olmadan). Mevcut phase0–phase5 örnekleri **kasıtlı
  olarak** ham SDL+Painter kullanımını belgelemeye devam eder; migrasyon yapılmadı.
- `conanfile.py` `package_info` artık `["sdl_painter_app", "sdl_painter"]` (sıra:
  app → core) yayınlar; `exports_sources`/`package` zaten `include/*`, `src/*` ve
  `*.lib/*.a`'yı kapsadığından ek değişiklik gerekmedi.
- **vsync/MSAA yalnızca OpenGL'de** uygulanır; Vulkan'da sunum modu swapchain
  tarafından seçildiğinden yoksayılır (`AppConfig` Doxygen'inde belgelendi).
- Risk: `union SDL_Event;` forward-decl SDL 3.2.x'e karşı doğrulandı; ileride
  kırılırsa opt-in `application_sdl.h` başlığına geçilebilir.
