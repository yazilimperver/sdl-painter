#pragma once

#include "sdl_painter/app/events.h"
#include "sdl_painter/renderer.h"

#include <cstdint>
#include <string>

namespace sdl_painter {

/// @brief Uygulama döngüsünün zamanlama modeli.
enum class TimingMode : uint8_t {
  /// @brief Değişken adım — her frame'de bir kez `OnUpdate(dt)` (varsayılan).
  /// Basit görselleştirme/çizim uygulamaları için yeterlidir.
  kVariable,

  /// @brief Sabit adım + interpolasyon (Game Programming Patterns "play catch up").
  /// `OnUpdate` sabit `dt` ile 0..N kez çağrılır; `OnRender(Painter&, alpha)`
  /// biriken artık zamanı [0,1] interpolasyon faktörü olarak alır. Fizik/oyun
  /// mantığı için deterministik.
  kFixed,
};

/// @brief Ekran üstü istatistik göstergesinin modu.
enum class StatsOverlayMode : uint8_t {
  /// @brief Kapalı (varsayılan).
  kNone,
  /// @brief Yalnızca FPS.
  kFps,
  /// @brief FPS + CPU/GPU süresi + draw call / batch / vertex / durum sayacı.
  kDetailed,
};

/// @brief Application penceresi ve backend yapılandırması.
///
/// C++17 aggregate — varsayılan değerlerle oluşturulup alan alan doldurulur:
/// @code
/// AppConfig cfg;
/// cfg.title = "Demo";
/// cfg.width = 900;
/// cfg.height = 700;
/// @endcode
struct AppConfig {
  /// @brief Pencere başlığı.
  std::string title{"SDLPainter Application"};

  /// @brief Pencere genişliği (piksel).
  int32_t width{800};

  /// @brief Pencere yüksekliği (piksel).
  int32_t height{600};

  /// @brief Pencere yeniden boyutlandırılabilir mi?
  bool resizable{true};

  /// @brief Yüksek piksel yoğunluğu (HiDPI / Retina) desteği.
  ///
  /// `true` ise pencere `SDL_WINDOW_HIGH_PIXEL_DENSITY` ile oluşturulur ve
  /// framebuffer, ekran ölçek faktörü kadar büyür (örn. 200% ölçekte
  /// 800x600 pencere → 1600x1200 framebuffer). @ref Application::Width ve
  /// @ref Application::Height daima piksel cinsindendir; @ref Painter
  /// koordinat sistemi de piksel tabanlıdır. Dolayısıyla bu seçenek açıkken
  /// çizim koordinatlarınızı @ref Application::Width / @ref Height üzerinden
  /// hesaplamanız gerekir — sabit sayılar küçük görünür.
  ///
  /// Varsayılan `false`: framebuffer, mantıksal pencere boyutuyla aynıdır.
  bool high_dpi{false};

  /// @brief Dikey senkronizasyon.
  /// @note Yalnızca OpenGL backend'de `SDL_GL_SetSwapInterval` ile uygulanır.
  ///       Vulkan'da yoksayılır — sunum modu swapchain tarafından seçilir.
  bool vsync{true};

  /// @brief MSAA örnek sayısı; 0 = kapalı.
  /// @note Yalnızca OpenGL backend'de geçerlidir ve pencere oluşturulmadan
  ///       önce GL attribute olarak uygulanır. Vulkan'da yoksayılır.
  int32_t msaa_samples{4};

  /// @brief Kullanılacak render backend'i.
  RendererBackend backend{RendererBackend::kOpenGL};

  /// @brief Döngü zamanlama modeli.
  TimingMode timing{TimingMode::kVariable};

  /// @brief Sabit güncelleme frekansı (Hz) — yalnızca @ref TimingMode::kFixed.
  /// `OnUpdate` bu sıklıkta, `1/fixed_update_hz` saniyelik sabit adımla çağrılır.
  int32_t fixed_update_hz{60};

  /// @brief Hedef kare hızı (FPS) freni; 0 = sınırsız (yalnızca vsync sınırlar).
  /// >0 ise her frame sonunda `SDL_DelayNS` ile kare süresi bu değere frenlenir.
  /// vsync ile birlikte kullanılabilir (hangisi daha kısıtlayıcıysa o baskındır).
  int32_t target_fps{0};

  /// @brief spdlog varsayılan logger'ını kur (renkli konsol + Win32 VT).
  /// @note Host uygulama kendi logger'ını kullanacaksa `false` yapın.
  bool init_logger{true};

  // --- Ekran üstü istatistik göstergesi ---

  /// @brief Başlangıçtaki gösterge modu.
  ///
  /// Çalışma zamanında @ref AppConfig::stats_overlay_key ile döngüsel olarak
  /// değiştirilebilir (varsayılan: F1). Sahnenin sol üstüne çizilir ve
  /// uygulamanın kendi çizim durumunu (transform, clip, opaklık) bozmaz.
  StatsOverlayMode stats_overlay{StatsOverlayMode::kNone};

  /// @brief Gösterge modunu döngüleyen tuş; @ref Key::kUnknown = devre dışı.
  ///
  /// Sıralama: kapalı → FPS → detaylı → kapalı. Tuş, uygulamanın
  /// `OnKeyDown` metoduna da iletilir (çatı olayı tüketmez).
  Key stats_overlay_key{Key::kF1};

  /// @brief FPS'i pencere başlığında da göster (`"<başlık> — 142 FPS"`).
  ///
  /// Font gerektirmez; sistemde TTF bulunamasa bile çalışır. Başlık saniyede
  /// birkaç kez güncellenir.
  bool show_fps_in_title{false};

  /// @brief Gösterge yazı tipi; boşsa sistem fontları aranır.
  ///
  /// Kütüphane font gömmez. Boş bırakıldığında yaygın Windows/Linux/macOS
  /// yolları denenir; hiçbiri bulunamazsa ekran üstü gösterge sessizce
  /// devre dışı kalır (başlık göstergesi çalışmaya devam eder).
  std::string stats_overlay_font;

  /// @brief Gösterge yazı boyutu (punto).
  int32_t stats_overlay_font_size{14};
};

}  // namespace sdl_painter
