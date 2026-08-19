#pragma once

#include "sdl_painter/color.h"

namespace sdl_painter {

/// @brief Çizgi stili — renk, kalınlık ve isteğe bağlı dış kontur.
class Pen {
 public:
  /// @brief Varsayılan kalem: siyah, 1 piksel kalınlık.
  Pen() = default;

  /// @brief Renk ve kalınlıkla kalem oluştur.
  /// @param color Çizgi rengi.
  /// @param width Çizgi kalınlığı (piksel, >= 0.0).
  explicit Pen(const Color& color, float width = 1.0F)
      : mColor(color), mWidth(width) {}

  /// @brief Dış konturlu (outline) kalem oluştur.
  ///
  /// Çizim sırasında önce outline_color ile daha kalın bir geometri,
  /// ardından üzerine color/width ile normal geometri çizilir. Bu sayede
  /// çizgi, resim veya karmaşık arka planlar üzerinde daha belirgin olur.
  /// @param color Çizgi rengi.
  /// @param width Çizgi kalınlığı (piksel, >= 0.0).
  /// @param outline_color Dış kontur rengi.
  /// @param outline_width Dış kontur kalınlığı (piksel, her iki tarafa
  /// eklenir; >= 0.0).
  Pen(const Color& color, float width, const Color& outline_color,
      float outline_width)
      : mColor(color),
        mWidth(width),
        mOutlineColor(outline_color),
        mOutlineWidth(outline_width) {}

  /// @brief Çizgi rengini döndür.
  [[nodiscard]] const Color& GetColor() const noexcept { return mColor; }

  /// @brief Çizgi kalınlığını döndür.
  [[nodiscard]] float GetWidth() const noexcept { return mWidth; }

  /// @brief Dış kontur rengini döndür.
  [[nodiscard]] const Color& GetOutlineColor() const noexcept {
    return mOutlineColor;
  }

  /// @brief Dış kontur kalınlığını döndür.
  [[nodiscard]] float GetOutlineWidth() const noexcept { return mOutlineWidth; }

  /// @brief Çizgi rengini ayarla.
  void SetColor(const Color& color) noexcept { mColor = color; }

  /// @brief Çizgi kalınlığını ayarla.
  void SetWidth(float width) noexcept { mWidth = width > 0.0F ? width : 0.0F; }

  /// @brief Dış kontur rengini ayarla.
  void SetOutlineColor(const Color& color) noexcept { mOutlineColor = color; }

  /// @brief Dış kontur kalınlığını ayarla.
  void SetOutlineWidth(float width) noexcept { mOutlineWidth = width; }

  /// @brief Kalem görünür mü? (alpha > 0 ve width > 0)
  [[nodiscard]] bool IsVisible() const noexcept {
    return mColor.a > 0 && mWidth > 0.0F;
  }

  /// @brief Kalemin görünür bir dış konturu var mı?
  [[nodiscard]] bool HasOutline() const noexcept {
    return mOutlineColor.a > 0 && mOutlineWidth > 0.0F;
  }

  [[nodiscard]] bool operator==(const Pen& other) const noexcept {
    return mColor == other.mColor && mWidth == other.mWidth &&
           mOutlineColor == other.mOutlineColor &&
           mOutlineWidth == other.mOutlineWidth;
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
  Color mOutlineColor{Color::Transparent()};
  float mOutlineWidth{0.0F};
};

}  // namespace sdl_painter
