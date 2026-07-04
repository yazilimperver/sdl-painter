#pragma once

#include "sdl_painter/renderer.h"

#include <cstdint>
#include <string>

namespace sdl_painter {

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

  /// @brief spdlog varsayılan logger'ını kur (renkli konsol + Win32 VT).
  /// @note Host uygulama kendi logger'ını kullanacaksa `false` yapın.
  bool init_logger{true};
};

}  // namespace sdl_painter
