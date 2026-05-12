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
  constexpr Color(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255)
      : r(r), g(g), b(b), a(a) {}

  /// @brief Rengi [0.0, 1.0] aralığında normalize edilmiş float olarak döndür.
  float RedF() const { return r / 255.0f; }
  float GreenF() const { return g / 255.0f; }
  float BlueF() const { return b / 255.0f; }
  float AlphaF() const { return a / 255.0f; }

  bool operator==(const Color& other) const {
    return r == other.r && g == other.g && b == other.b && a == other.a;
  }
  bool operator!=(const Color& other) const { return !(*this == other); }

  // Yaygın renkler
  static constexpr Color Black() { return {0, 0, 0}; }
  static constexpr Color White() { return {255, 255, 255}; }
  static constexpr Color Red() { return {255, 0, 0}; }
  static constexpr Color Green() { return {0, 255, 0}; }
  static constexpr Color Blue() { return {0, 0, 255}; }
  static constexpr Color Transparent() { return {0, 0, 0, 0}; }
};

}  // namespace sdl_painter
