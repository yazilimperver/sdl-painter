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
  const Color& GetColor() const { return mColor; }

  /// @brief Dolgu rengini ayarla.
  void SetColor(const Color& color) { mColor = color; }

  /// @brief Fırça görünür mü? (alpha > 0)
  bool IsVisible() const { return mColor.a > 0; }

  bool operator==(const Brush& other) const { return mColor == other.mColor; }
  bool operator!=(const Brush& other) const { return !(*this == other); }

  /// @brief Şeffaf (dolgu yapmayan) fırça.
  static Brush NoBrush() { return Brush(Color::Transparent()); }

 private:
  Color mColor{Color::Black()};
};

}  // namespace sdl_painter
