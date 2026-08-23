#pragma once

/// @file stats_overlay.h
/// @brief Ekran üstü FPS / kare istatistiği göstergesi (dahili).
///
/// `Application`'a ait bir yardımcıdır; public API'de görünmez.

#include "sdl_painter/app/app_config.h"
#include "sdl_painter/frame_stats.h"

#include <cstdint>
#include <memory>
#include <string>

namespace sdl_painter {

class Font;
class Painter;

namespace app_detail {

/// @brief Kare süresi biriktirip FPS hesaplayan ve göstergeyi çizen sınıf.
///
/// Font gerektiren tek parça ekran üstü çizimdir; font bulunamazsa sınıf
/// sessizce çizim yapmaz (FPS hesabı ve başlık göstergesi çalışmaya devam
/// eder).
class StatsOverlay {
 public:
  /// @param font_path Kullanılacak TTF; boşsa sistem fontları aranır.
  /// @param point_size Yazı boyutu.
  StatsOverlay(const std::string& font_path, int32_t point_size);
  ~StatsOverlay();

  StatsOverlay(const StatsOverlay&) = delete;
  StatsOverlay& operator=(const StatsOverlay&) = delete;

  /// @brief Kare süresini kaydet ve FPS ortalamasını güncelle.
  /// @param frame_ns Bir önceki karenin toplam süresi (nanosaniye).
  void Sample(uint64_t frame_ns);

  /// @brief Yumuşatılmış kare hızı (kare/saniye).
  [[nodiscard]] double Fps() const noexcept { return mFps; }

  /// @brief Göstergeyi çiz.
  ///
  /// Painter durumunu (transform, clip, opaklık, kalem, fırça) bozmaz:
  /// `Save()`/`Restore()` arasında çalışır.
  void Draw(Painter& painter, StatsOverlayMode mode, const FrameStats& stats);

  /// @brief Yazı tipi yüklenebildi mi? `false` ise @ref Draw hiçbir şey çizmez.
  [[nodiscard]] bool HasFont() const noexcept { return mFont != nullptr; }

 private:
  std::shared_ptr<Font> mFont;

  // FPS, sabit bir zaman penceresinde biriktirilir: kare kare gösterilen
  // deger okunamayacak kadar zipliyor.
  uint64_t mAccumNs{0};
  int32_t mAccumFrames{0};
  double mFps{0.0};
};

}  // namespace app_detail
}  // namespace sdl_painter
