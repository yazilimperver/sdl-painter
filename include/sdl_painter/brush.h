#pragma once

#include "sdl_painter/color.h"

namespace sdl_painter {

/// @brief Dolgu stili — renk tabanlı düz dolgu.
class Brush {
 public:
  /// @brief Varsayılan fırça: siyah, tam opak.
  Brush() = default;

  /// @brief Renk ile fırça oluştur.
  explicit Brush(const Color& color) : mColor(color) {}

  /// @brief Dolgu rengini döndür.
  [[nodiscard]] const Color& GetColor() const noexcept { return mColor; }

  /// @brief Dolgu rengini ayarla.
  void SetColor(const Color& color) noexcept { mColor = color; }

  /// @brief Fırça görünür mü? (alpha > 0)
  [[nodiscard]] bool IsVisible() const noexcept { return mColor.a > 0; }

  [[nodiscard]] bool operator==(const Brush& other) const noexcept {
    return mColor == other.mColor;
  }
  [[nodiscard]] bool operator!=(const Brush& other) const noexcept {
    return !(*this == other);
  }

  /// @brief Şeffaf (dolgu yapmayan) fırça.
  [[nodiscard]] static Brush NoBrush() noexcept {
    return Brush(Color::Transparent());
  }

 private:
  Color mColor{Color::Black()};
};

}  // namespace sdl_painter
