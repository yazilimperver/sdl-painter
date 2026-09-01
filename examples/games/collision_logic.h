#pragma once

/// @file collision_logic.h
/// @brief `breakout` örneğinin çarpışma matematiği (çizimden bağımsız).
///
/// `tictactoe_logic.h` ile aynı kalıp: oyun mantığı SDL'den, `Painter`'dan ve
/// pencereden tamamen ayrık tutulur; böylece pencere açmadan birim testi
/// yazılabilir (`tests/test_collision_logic.cpp`).
///
/// Buradaki her fonksiyon saf: durum tutmaz, yalnızca girdiden çıktı üretir.

#include <algorithm>
#include <cmath>

namespace breakout {

/// @brief Eksen hizalı dikdörtgen (AAKK).
struct Aabb {
  float x{0.0F};
  float y{0.0F};
  float w{0.0F};
  float h{0.0F};

  [[nodiscard]] constexpr float Left() const { return x; }
  [[nodiscard]] constexpr float Right() const { return x + w; }
  [[nodiscard]] constexpr float Top() const { return y; }
  [[nodiscard]] constexpr float Bottom() const { return y + h; }
  [[nodiscard]] constexpr float CenterX() const { return x + w * 0.5F; }
  [[nodiscard]] constexpr float CenterY() const { return y + h * 0.5F; }
};

/// @brief Daire.
struct Circle {
  float x{0.0F};
  float y{0.0F};
  float r{0.0F};
};

/// @brief Çarpışmanın hangi eksende çözüleceği.
enum class Axis {
  kNone,        ///< Çarpışma yok.
  kHorizontal,  ///< Sol/sağ kenar — x hızı ters çevrilmeli.
  kVertical,    ///< Üst/alt kenar — y hızı ters çevrilmeli.
};

/// @brief İki dikdörtgen kesişiyor mu?
[[nodiscard]] inline bool Intersects(const Aabb& a, const Aabb& b) {
  return a.Left() < b.Right() && a.Right() > b.Left() && a.Top() < b.Bottom() &&
         a.Bottom() > b.Top();
}

/// @brief Daire ile dikdörtgen kesişiyor mu?
///
/// Klasik yöntem: dairenin merkezini dikdörtgene doğru kırp, kalan mesafeyi
/// yarıçapla karşılaştır. Köşe durumlarını da doğru çözer.
[[nodiscard]] inline bool Intersects(const Circle& c, const Aabb& r) {
  const float nx = std::clamp(c.x, r.Left(), r.Right());
  const float ny = std::clamp(c.y, r.Top(), r.Bottom());
  const float dx = c.x - nx;
  const float dy = c.y - ny;
  return (dx * dx + dy * dy) <= (c.r * c.r);
}

/// @brief Çarpışma hangi eksende çözülmeli?
///
/// En sığ örtüşme (penetration) hangi eksendeyse çarpışma o eksendedir: top
/// bir tuğlanın üstüne değdiyse dikey örtüşme yataydan küçüktür.
///
/// Bu ayrım olmadan top, yandan çarptığında da yukarı seker ve oyun
/// tuhaflaşır — bu yüzden ayrı bir fonksiyon ve ayrı bir testi var.
[[nodiscard]] inline Axis ResolveAxis(const Circle& c, const Aabb& r) {
  if (!Intersects(c, r)) {
    return Axis::kNone;
  }
  // Örtüşme derinlikleri: merkezler arası mesafe ile toplam yarı-boyutların
  // farkı.
  const float overlap_x = (r.w * 0.5F + c.r) - std::fabs(c.x - r.CenterX());
  const float overlap_y = (r.h * 0.5F + c.r) - std::fabs(c.y - r.CenterY());
  return (overlap_x < overlap_y) ? Axis::kHorizontal : Axis::kVertical;
}

/// @brief Raketten sekme açısı: değme noktası, sekişin yönünü belirler.
///
/// Dönen değer [-1, 1] aralığında normalize edilmiş yatay sapmadır:
/// -1 = raketin sol ucu, 0 = tam ortası, +1 = sağ ucu. Oyunu oynanabilir
/// kılan şey budur; düz yansıma top ile raketi sonsuz bir döngüye sokabilir.
[[nodiscard]] inline float PaddleBounce(float ball_x, const Aabb& paddle) {
  if (paddle.w <= 0.0F) {
    return 0.0F;
  }
  const float t = (ball_x - paddle.CenterX()) / (paddle.w * 0.5F);
  return std::clamp(t, -1.0F, 1.0F);
}

/// @brief Değeri [lo, hi] aralığında tut.
[[nodiscard]] inline float ClampTo(float v, float lo, float hi) {
  return std::clamp(v, lo, hi);
}

}  // namespace breakout
