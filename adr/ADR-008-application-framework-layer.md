# ADR-008: Uygulama Çatısı Katmanı (`sdl_painter_app`)

- **Durum:** Kabul edildi
- **Tarih:** 2026-07-04
- **Güncelleme:** 2026-11-07, gamepad desteği (en altta)

## Bağlam

SDLPainter yalnızca çizim API'sini (`Painter`) sunar; pencere oluşturma, SDL
init, GL context, olay döngüsü ve yıkım kullanıcının sorumluluğunda olduğu kabulüyle hep ilerledik. Bunun sonucunda, her uygulama ~40–50 satır aynı boilerplate'i tekrar ettiğini gördük: `InitLogger`,
`SDL_Init`, 5 GL attribute (3.3 core + MSAA), backend'e göre pencere flag'i,
Painter'ın pencereden önce yıkılması için scope dansı, `SDL_PollEvent` döngüsü,
`SDL_DestroyWindow` + `SDL_Quit`. Bu tekrar repo'daki örneklerde de görülebilir.

Kütüphaneyi kullananların SDL başlıkları ve ayarlarıyla uğraşmaması hedeflediğimizden ötürü, bu konuya el atmak istedim.

Referans olarak da, uEngine4'teki `SdlApplication` (SDL2)'dan esinlendik; ancak o, asset service, gamepad, touch, JSON config ve fixed-timestep ticker içeren nispeten ağır bir altyapı. Bu proje için çok daha ince bir soyutlamanın işimizi göreceğini değerlendirdim.

| Kriter | Çatı yok (mevcut) | uEngine4 (listener + ticker) | İnce kalıtım (seçilen) |
|--------|-------------------|------------------------------|------------------------|
| Boilerplate | Her uygulamada ~50 satır | Gizli | Gizli |
| API modeli | — | Client interface + listener kaydı | `Application`'dan türet |
| Zamanlama | Elle | Fixed timestep + interpolasyon | Değişken delta-time |
| Bağımlılık | — | Asset/gamepad/touch/JSON | Sıfır yeni bağımlılık |
| Esneklik | Tam kontrol | Yüksek | Yeterli + ham olayla SDL'in doğrudan kullanımı |

## Karar

`sdl_painter_app` adında **ayrı bir static kütüphane** eklenir. Core
`sdl_painter` saf çizim API'si olarak dokunulmadan kalır; kendi döngüsünü isteyen
uygulamalar yalnızca core'a link etmeye devam eder.

- **Kalıtım tabanlı API:** Kullanıcı `sdl_painter::Application`'dan türeyip
  korumalı sanal metotları (`OnRender` zorunlu; `OnInit`/`OnUpdate`/`OnKeyDown`/
  fare/`OnResize`/`OnShutdown` opsiyonel) override eder. `Run()` → `int` (main).
- **Kendi olay tipleri + SDL'in doğrudan kullanımı:** `Key`/`KeyModifier`/`MouseButton` enum'ları
  ve `KeyEvent`/`MouseButtonEvent`/... struct'ları SDL'den bağımsızdır. Public app
  başlıkları SDL başlığı **içermez**. İleri ihtiyaçlarda SDL doğrudan
  kullanılabilsin diye `virtual bool OnRawEvent(const SDL_Event&)` sunulur; başlıkta
  `union SDL_Event;` forward-decl kullanılır (SDL3'te `SDL_Event` bir union'dır,
  mevcut `struct SDL_Window;` deseniyle aynı).
- **Opsiyon yok:** `SDLPAINTER_BUILD_APP` gibi bir CMake/Conan opsiyonu
  eklenmez — katman iki `.cpp`, sıfır yeni bağımlılık ve static lib olduğundan
  kullanmayan tüketiciye maliyeti yoktur. Koşulsuz derlenir ve paketlenir.

## Gerekçe

- **Ayrı target:** Çizim ile ilgisi olmayan sorumlulukların (event loop, timing)
  core'a karışmasını önler; core'un saf ve testlenebilir kalmasını sağlar.
- **İki zamanlama modu (`AppConfig::timing`):** Varsayılan `kVariable` — değişken
  delta-time (`SDL_GetTicksNS`), her frame bir kez `OnUpdate(dt)`; basit
  görselleştirme/çizim için yeterli. Opsiyonel `kFixed` — Game Programming
  Patterns "play catch up" yaklaşımı: sabit adımlı (`fixed_update_hz`)
  deterministik `OnUpdate` + `OnRender(Painter&, alpha)` ile interpolasyonlu
  render; fizik/oyunvari uygulamalar için. Her iki modda da `OnUpdate` büyük
  sıçramalara karşı 0.25 s'ye, kFixed catch-up ise frame başına 5 adıma
  sınırlanır ("spiral of death" koruması). Ek olarak `target_fps` (>0) ile
  `SDL_DelayNS` tabanlı kare freni sunulur (vsync ile birlikte kullanılabilir).
  Tek `Application` sınıfı korunur; uEngine4'ün listener + asset + touch
  ağırlığından kaçınılır.
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
- `OnRender(Painter&)` artık pure değil (boş varsayılan); interpolasyon için
  `OnRender(Painter&, float alpha)` eklendi ve varsayılanı tek-argümanlıya delege
  eder. Mevcut tek-argümanlı override'lar değişmeden çalışır; oyun uygulamaları
  iki-argümanlıyı override eder (`-Woverloaded-virtual` etkin katı build'lerde
  `using Application::OnRender;` gerekebilir — header'da belgelendi).
- `conanfile.py` `package_info` artık `["sdl_painter_app", "sdl_painter"]` (sıra:
  app → core) yayınlar; `exports_sources`/`package` zaten `include/*`, `src/*` ve
  `*.lib/*.a`'yı kapsadığından ek değişiklik gerekmedi.
- **vsync/MSAA yalnızca OpenGL'de** uygulanır; Vulkan'da sunum modu swapchain
  tarafından seçildiğinden yoksayılır (`AppConfig` Doxygen'inde belgelendi).
- Risk: `union SDL_Event;` forward-decl SDL 3.2.x'e karşı doğrulandı; ileride
  kırılırsa opt-in `application_sdl.h` başlığına geçilebilir.

## Güncelleme (2026-11-07): Gamepad desteği

### Bağlam

İlk karar gamepad'i uEngine4'ün ağır kısımları arasında sayıp dışarıda
bırakmıştı; ihtiyacı olan uygulama `OnRawEvent` ile kendisi çözecekti. Planlanan
shoot 'em up oyunu ve örnekler gamepad istiyor. Her uygulamanın aynı
tak-çıkar, açma-kapama ve eksen normalizasyonu kodunu tekrar yazması, bu
katmanın kaldırmak için kurulduğu boilerplate'in ta kendisi.

### Karar

Gamepad, klavye ve fareyle aynı kalıpla eklenir; katman ince kalır.

- **Açık rıza:** `AppConfig::enable_gamepad` (varsayılan `false`). Açıkken
  `SDL_INIT_GAMEPAD` başlatılır; başlatılamazsa uyarı loglanır ve uygulama
  gamepad olmadan devam eder. Kapalıyken çatı gamepad olaylarına hiç
  dokunmaz: alt sistemi kendisi başlatıp `OnRawEvent` ile yöneten uygulamada
  çift açılış olmaz.
- **Cihaz yönetimi çatıda:** SDL düğme ve eksen olaylarını yalnızca açılmış
  gamepad'ler için üretir. Çatı `SDL_EVENT_GAMEPAD_ADDED`'da gamepad'i açar,
  `REMOVED`'da ve yıkımda kapatır. Eşleme değişince aynı cihaz için tekrar
  gelen ADDED yeniden açılmaz. Açılamayan cihaz uygulamaya bildirilmez.
- **SDL'den bağımsız türler** (`events.h`): `GamepadButton` konuma göre
  adlandırılır ve SDL'in standart 15 düğmesini kapsar (ek düğmeler
  `kUnknown`); `GamepadAxis`; `GamepadType` SDL türlerini ekrandaki tuş
  ipuçları için aileye indirir (Xbox, PlayStation, Nintendo).
- **Olaylar:** `OnGamepadAdded/Removed`, `OnGamepadButtonDown/Up`,
  `OnGamepadAxis`. Eksen değeri normalize gelir: çubuk −1…1, tetik 0…1.
- **Titreşim:** `RumbleGamepad(id, low, high, duration_ms)`, şiddetler 0…1.

**Kapsam dışı:** eylem eşleme, ölü bölge, durum sorgusu (`IsButtonDown`),
touchpad, jiroskop, LED. Dairesel ölü bölge iki ekseni birlikte ister ama
olaylar eksen eksen gelir; klavyede olduğu gibi durumu uygulama tutar.
Gerekirse SDL doğrudan kullanılabilir: `OnRawEvent` ve `GetWindow` duruyor.

### Sonuçlar

- `Application`'a sanal metot ve üye eklendi: shared derlemede binary
  uyumluluk değişti, uygulamalar yeniden derlenmeli (CHANGELOG).
- Cihaz tutamaçları public başlıkta `struct SDL_Gamepad;` forward-decl'i ile
  tutulur; başlık SDL içermez.
- SDL, pencere odakta değilken joystick girdisini atar
  (`SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS`). `AppConfig` belgesinde
  yazılı; testler bu ipucuyla çalışır.
- Testler donanım gerektirmez: çeviriler saf fonksiyon
  (`test_event_translator.cpp`), olay iletimi sentetik `SDL_PushEvent` ile,
  tak-çıkar → düğme → eksen → titreşim → çıkarma zinciri SDL'in sanal
  joystick'iyle (`SDL_AttachVirtualJoystick`) sınanır (`test_application.cpp`).
