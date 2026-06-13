#pragma once

#include <cstdint>

namespace sdl_painter {

/// @brief RGBA renk temsili. Her kanal [0, 255] aralığında.
struct Color {
  uint8_t r{0};
  uint8_t g{0};
  uint8_t b{0};
  uint8_t a{255};

  /// @brief Varsayılan siyah, tam opak renk.
  constexpr Color() = default;

  /// @brief RGBA değerleriyle renk oluştur.
  constexpr Color(uint8_t r_, uint8_t g_, uint8_t b_, uint8_t a_ = 255)
      : r(r_), g(g_), b(b_), a(a_) {}

  /// @brief Rengi [0.0, 1.0] aralığında normalize edilmiş float olarak döndür.
  [[nodiscard]] float RedF() const noexcept { return static_cast<float>(r) / 255.0F; }
  [[nodiscard]] float GreenF() const noexcept { return static_cast<float>(g) / 255.0F; }
  [[nodiscard]] float BlueF() const noexcept { return static_cast<float>(b) / 255.0F; }
  [[nodiscard]] float AlphaF() const noexcept { return static_cast<float>(a) / 255.0F; }

  [[nodiscard]] bool operator==(const Color& other) const noexcept {
    return r == other.r && g == other.g && b == other.b && a == other.a;
  }
  [[nodiscard]] bool operator!=(const Color& other) const noexcept {
    return !(*this == other);
  }

  // Yaygın renkler
  [[nodiscard]] static constexpr Color Black() noexcept { return {0, 0, 0}; }
  [[nodiscard]] static constexpr Color White() noexcept {
    return {255, 255, 255};
  }
  [[nodiscard]] static constexpr Color Red() noexcept { return {255, 0, 0}; }
  [[nodiscard]] static constexpr Color Green() noexcept { return {0, 255, 0}; }
  [[nodiscard]] static constexpr Color Blue() noexcept { return {0, 0, 255}; }
  [[nodiscard]] static constexpr Color Transparent() noexcept {
    return {0, 0, 0, 0};
  }
};

}  // namespace sdl_painter
