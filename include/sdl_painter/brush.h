#pragma once

#include "sdl_painter/color.h"
#include "sdl_painter/geometry.h"

#include <cstdint>

namespace sdl_painter {

/// @brief Fırçanın dolgu türü.
enum class BrushType : uint8_t {
  kSolid,   ///< Tek düz renk. **Varsayılan.**
  kLinear,  ///< İki nokta arasında doğrusal geçiş.
  kRadial,  ///< Merkezden dışa doğru ışınsal geçiş.
};

/// @brief Dolgu stili — düz renk veya gradient.
///
/// Gradient shader gerektirmez. Vertex'ler zaten renk taşıyor (batch'leme
/// bunun için yapıldı), bu yüzden geçiş tessellation sırasında köşe renkleri
/// hesaplanarak üretilir ve enterpolasyonu donanım yapar. Sonuç: gradient
/// hiçbir backend değişikliği gerektirmez ve batch'i kırmaz — opaklığın
/// aksine.
///
/// Bunun bedeli, geçişin şeklin köşe yoğunluğu kadar hassas olmasıdır:
/// iki üçgenden ibaret bir dikdörtgende doğrusal geçiş kusursuzdur (tam da
/// donanımın enterpole ettiği şey), ama az segmentli bir şekilde ışınsal
/// geçiş bantlanabilir. Daire ve elips segment sayısı yarıçapa göre
/// uyarlandığı için pratikte sorun çıkarmaz.
///
/// Gradient koordinatları çizim koordinatlarıyla aynı uzaydadır (şekil
/// yerel), yani transform yığınından etkilenir — Qt'nin `QLinearGradient`
/// davranışı gibi.
class Brush {
 public:
  /// @brief Varsayılan fırça: siyah, tam opak.
  Brush() = default;

  /// @brief Renk ile fırça oluştur.
  explicit Brush(const Color& color) : mColor(color) {}

  /// @brief Doğrusal gradient fırça.
  ///
  /// Renk `start` noktasında `from`, `end` noktasında `to` olur; aradaki
  /// değerler bu iki nokta arasındaki izdüşüme göre enterpole edilir. Çizgiye
  /// dik yönde renk sabittir.
  ///
  /// @param start Geçişin başladığı nokta (çizim koordinatı).
  /// @param end   Geçişin bittiği nokta. `start` ile aynıysa fırça düz
  ///              `from` rengine döner (sıfıra bölme yok).
  [[nodiscard]] static Brush LinearGradient(const Point& start,
                                            const Point& end, const Color& from,
                                            const Color& to) noexcept {
    Brush b;
    b.mType = BrushType::kLinear;
    b.mColor = from;
    b.mColor2 = to;
    b.mStart = start;
    b.mEnd = end;
    return b;
  }

  /// @brief Işınsal gradient fırça.
  ///
  /// Renk merkezde `from`, `radius` uzaklığında `to` olur; ötesi `to` ile
  /// doldurulur.
  ///
  /// @param radius Yarıçap. Pozitif değilse fırça düz `from` rengine döner.
  [[nodiscard]] static Brush RadialGradient(const Point& center, float radius,
                                            const Color& from,
                                            const Color& to) noexcept {
    Brush b;
    b.mType = BrushType::kRadial;
    b.mColor = from;
    b.mColor2 = to;
    b.mStart = center;
    b.mRadius = radius;
    return b;
  }

  /// @brief Fırçanın dolgu türü.
  [[nodiscard]] BrushType GetType() const noexcept { return mType; }

  /// @brief Gradient mi? (düz renk değil)
  [[nodiscard]] bool IsGradient() const noexcept {
    return mType != BrushType::kSolid;
  }

  /// @brief Gradient bitiş rengi (düz fırçada anlamsız).
  [[nodiscard]] const Color& GetColor2() const noexcept { return mColor2; }

  /// @brief Doğrusal gradientte başlangıç, ışınsalda merkez.
  [[nodiscard]] const Point& GetStart() const noexcept { return mStart; }

  /// @brief Doğrusal gradientte bitiş noktası.
  [[nodiscard]] const Point& GetEnd() const noexcept { return mEnd; }

  /// @brief Işınsal gradientte yarıçap.
  [[nodiscard]] float GetRadius() const noexcept { return mRadius; }

  /// @brief Dolgu rengini döndür.
  [[nodiscard]] const Color& GetColor() const noexcept { return mColor; }

  /// @brief Dolgu rengini ayarla.
  void SetColor(const Color& color) noexcept { mColor = color; }

  /// @brief Fırça görünür mü? (alpha > 0)
  /// @brief Fırça görünür mü?
  ///
  /// Gradient'te iki uçtan biri bile opaksa görünürdür; yalnızca başlangıç
  /// rengine bakmak, saydamdan opağa giden bir geçişi yanlışlıkla görünmez
  /// sayardı.
  [[nodiscard]] bool IsVisible() const noexcept {
    return mColor.a > 0 || (IsGradient() && mColor2.a > 0);
  }

  [[nodiscard]] bool operator==(const Brush& other) const noexcept {
    if (mColor != other.mColor || mType != other.mType) {
      return false;
    }
    if (mType == BrushType::kSolid) {
      return true;
    }
    return mColor2 == other.mColor2 && mStart.x == other.mStart.x &&
           mStart.y == other.mStart.y && mEnd.x == other.mEnd.x &&
           mEnd.y == other.mEnd.y && mRadius == other.mRadius;
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
  Color mColor2{Color::White()};
  Point mStart;
  Point mEnd;
  float mRadius{0.0F};
  BrushType mType{BrushType::kSolid};
};

}  // namespace sdl_painter
