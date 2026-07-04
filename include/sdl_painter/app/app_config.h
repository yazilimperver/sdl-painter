#pragma once

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
};

}  // namespace sdl_painter
