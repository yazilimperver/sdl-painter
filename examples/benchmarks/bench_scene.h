#pragma once

/// @file bench_scene.h
/// @brief Ölçümlerin ortak sahnesi: deterministik şekil yerleşimi.
///
/// `batching_benchmark` ve `renderer_comparison` aynı yerleşimi kullanır;
/// böylece iki ölçümün sayıları aynı kareye aittir.

#include "sdl_painter/color.h"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace bench {

constexpr int32_t kViewportWidth = 1280;
constexpr int32_t kViewportHeight = 720;

/// @brief Tekrarlanabilir sonuç için basit LCG: std::rand implementasyona
///        göre değiştiğinden ölçüm makineler arasında karşılaştırılamaz olur.
class Lcg {
 public:
  explicit Lcg(uint32_t seed) : mState(seed) {}

  uint32_t Next() noexcept {
    mState = (mState * 1664525U) + 1013904223U;
    return mState;
  }

  /// @brief [0, n) aralığında değer.
  uint32_t Below(uint32_t n) noexcept { return Next() % n; }

  /// @brief [0, 1] aralığında değer.
  float Unit() noexcept {
    return static_cast<float>(Next() >> 8U) / 16777215.0F;
  }

 private:
  uint32_t mState;
};

/// @brief Şeklin ekrandaki konumu; tüm senaryolarda aynı dağılım kullanılır.
struct Placement {
  float x{0.0F};
  float y{0.0F};
  float size{0.0F};
  sdl_painter::Color color;
};

/// @brief Senaryolar arasında karşılaştırma yapılabilmesi için ortak,
///        deterministik yerleşim üret.
inline std::vector<Placement> MakePlacements(int32_t count) {
  Lcg rng(12345U);
  std::vector<Placement> out;
  out.reserve(static_cast<std::size_t>(count));
  for (int32_t i = 0; i < count; ++i) {
    Placement p;
    p.size = 6.0F + (rng.Unit() * 10.0F);
    p.x = rng.Unit() * (static_cast<float>(kViewportWidth) - p.size);
    p.y = rng.Unit() * (static_cast<float>(kViewportHeight) - p.size);
    p.color = sdl_painter::Color{static_cast<uint8_t>(rng.Below(256)),
                                 static_cast<uint8_t>(rng.Below(256)),
                                 static_cast<uint8_t>(rng.Below(256)), 255};
    out.push_back(p);
  }
  return out;
}

}  // namespace bench
