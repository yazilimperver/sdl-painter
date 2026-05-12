#pragma once

#include "sdl_painter/color.h"

namespace sdl_painter {

/// @brief Çizgi stili — renk ve kalınlık.
class Pen {
 public:
  /// @brief Varsayılan kalem: siyah, 1 piksel kalınlık.
  Pen() = default;

  /// @brief Renk ve kalınlıkla kalem oluştur.
  /// @param color Çizgi rengi.
  /// @param width Çizgi kalınlığı (piksel, >= 0.0).
  explicit Pen(const Color& color, float width = 1.0f)
      : mColor(color), mWidth(width) {}

  /// @brief Çizgi rengini döndür.
  const Color& GetColor() const { return mColor; }

  /// @brief Çizgi kalınlığını döndür.
  float GetWidth() const { return mWidth; }

  /// @brief Çizgi rengini ayarla.
  void SetColor(const Color& color) { mColor = color; }

  /// @brief Çizgi kalınlığını ayarla.
  void SetWidth(float width) { mWidth = width; }

  /// @brief Kalem görünür mü? (alpha > 0 ve width > 0)
  bool IsVisible() const { return mColor.a > 0 && mWidth > 0.0f; }

  bool operator==(const Pen& other) const {
    return mColor == other.mColor && mWidth == other.mWidth;
  }
  bool operator!=(const Pen& other) const { return !(*this == other); }

  /// @brief Şeffaf (çizim yapmayan) kalem.
  static Pen NoPen() { return Pen(Color::Transparent(), 0.0f); }

 private:
  Color mColor{Color::Black()};
  float mWidth{1.0f};
};

}  // namespace sdl_painter
