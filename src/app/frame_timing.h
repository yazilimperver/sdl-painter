#pragma once

/// @file frame_timing.h
/// @brief Application döngüsünün zamanlama hesabı (dahili).
///
/// `Application::Run` gerçek saati okur ve bu fonksiyonları çağırır. Hesap
/// saatten ayrı tutulur ki sınır davranışları (delta sınırı, kare başına adım
/// sınırı, artık zaman, alpha) gerçek uyku olmadan test edilebilsin.

#include "sdl_painter/app/app_config.h"

#include <cstdint>

namespace sdl_painter::app_detail {

/// @brief Ardışık frame'ler arası azami delta zaman (nanosaniye).
///
/// Debugger duraklaması / pencere sürüklemesi gibi büyük sıçramaları sınırlar
/// ("spiral of death" koruması).
inline constexpr uint64_t kMaxDeltaNs = 250'000'000ULL;  // 0.25 s

/// @brief kFixed modda bir frame'de izin verilen azami sabit güncelleme sayısı.
inline constexpr int32_t kMaxFixedStepsPerFrame = 5;

/// @brief Saniyedeki nanosaniye.
inline constexpr uint64_t kNsPerSecond = 1'000'000'000ULL;

/// @brief Sabit adım süresi (nanosaniye).
/// @return @ref TimingMode::kFixed ve pozitif frekansta adım süresi; aksi
///         halde 0 (değişken adım).
[[nodiscard]] constexpr uint64_t FixedStepNs(TimingMode timing,
                                             int32_t fixed_update_hz) noexcept {
  return (timing == TimingMode::kFixed && fixed_update_hz > 0)
             ? kNsPerSecond / static_cast<uint64_t>(fixed_update_hz)
             : 0;
}

/// @brief Kareler arası geçen süreyi @ref kMaxDeltaNs ile sınırla.
[[nodiscard]] constexpr uint64_t ClampFrameDelta(uint64_t elapsed_ns) noexcept {
  return elapsed_ns > kMaxDeltaNs ? kMaxDeltaNs : elapsed_ns;
}

/// @brief Sabit adım biriktiricisinin bir karedeki sonucu.
struct FixedStepResult {
  int32_t steps{0};    ///< Bu karede çağrılacak sabit güncelleme sayısı.
  uint64_t lag_ns{0};  ///< Sonraki kareye kalan biriken süre.
  float alpha{0.0F};   ///< Çizim için ara değer oranı, [0, 1).
};

/// @brief Sabit adım biriktiricisini bir kare ilerlet.
/// @param lag_ns Önceki kareden kalan biriken süre.
/// @param elapsed_ns Bu karede geçen (sınırlanmış) süre.
/// @param step_ns Sabit adım süresi; sıfırdan büyük olmalı.
///
/// Kare başına en fazla @ref kMaxFixedStepsPerFrame adım üretilir. Sınıra
/// ulaşıldıysa biriken artık süre düşürülür: aksi halde yavaş makinede
/// geride kalan zaman her karede büyür.
[[nodiscard]] constexpr FixedStepResult AdvanceFixedStep(
    uint64_t lag_ns, uint64_t elapsed_ns, uint64_t step_ns) noexcept {
  FixedStepResult result;
  result.lag_ns = lag_ns + elapsed_ns;
  while (result.lag_ns >= step_ns && result.steps < kMaxFixedStepsPerFrame) {
    result.lag_ns -= step_ns;
    ++result.steps;
  }
  if (result.steps == kMaxFixedStepsPerFrame) {
    result.lag_ns = 0;
  }
  result.alpha =
      static_cast<float>(result.lag_ns) / static_cast<float>(step_ns);
  return result;
}

}  // namespace sdl_painter::app_detail
