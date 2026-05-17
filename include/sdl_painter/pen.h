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
  explicit Pen(const Color& color, float width = 1.0F)
      : mColor(color), mWidth(width) {}

  /// @brief Çizgi rengini döndür.
  [[nodiscard]] const Color& GetColor() const noexcept { return mColor; }

  /// @brief Çizgi kalınlığını döndür.
  [[nodiscard]] float GetWidth() const noexcept { return mWidth; }

  /// @brief Çizgi rengini ayarla.
  void SetColor(const Color& color) noexcept { mColor = color; }

  /// @brief Çizgi kalınlığını ayarla.
  void SetWidth(float width) noexcept { mWidth = width; }

  /// @brief Kalem görünür mü? (alpha > 0 ve width > 0)
  [[nodiscard]] bool IsVisible() const noexcept {
    return mColor.a > 0 && mWidth > 0.0F;
  }

  [[nodiscard]] bool operator==(const Pen& other) const noexcept {
    return mColor == other.mColor && mWidth == other.mWidth;
  }
  [[nodiscard]] bool operator!=(const Pen& other) const noexcept {
    return !(*this == other);
  }

  /// @brief Şeffaf (çizim yapmayan) kalem.
  [[nodiscard]] static Pen NoPen() noexcept {
    return Pen(Color::Transparent(), 0.0F);
  }

 private:
  Color mColor{Color::Black()};
  float mWidth{1.0F};
};

}  // namespace sdl_painter
