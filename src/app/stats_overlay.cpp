#include "stats_overlay.h"

#include "sdl_painter/brush.h"
#include "sdl_painter/color.h"
#include "sdl_painter/font.h"
#include "sdl_painter/geometry.h"
#include "sdl_painter/painter.h"
#include "sdl_painter/pen.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <array>
#include <cstdio>
#include <memory>
#include <spdlog/spdlog.h>
#include <utility>
#include <vector>

// spdlog (Windows'ta) Windows.h ceker ve `DrawText -> DrawTextA` makrosunu
// tanimlar; painter.cpp'deki ile ayni gerekce.
#ifdef DrawText
#undef DrawText
#endif

namespace sdl_painter {
namespace app_detail {

namespace {

/// @brief FPS ortalamasinin penceresi. Daha kisasi okunamayacak kadar
///        ziplar, daha uzunu ani dususleri gizler.
constexpr uint64_t kFpsWindowNs = 250'000'000ULL;

/// @brief Ilk FPS degeri icin yeterli sayilan kare sayisi.
constexpr int32_t kBootstrapFrames = 5;

constexpr float kPadding = 8.0F;
constexpr float kLineGap = 2.0F;
constexpr Color kPanelColor{0, 0, 0, 160};
constexpr Color kTextColor{240, 240, 240, 255};
constexpr Color kLabelColor{150, 200, 255, 255};

/// @brief Sistemde mevcut bir TTF fontu bul.
///
/// Kütüphane font gömmez (lisans ve paket boyutu). Ekran üstü gösterge
/// isteğe bağlı bir hata ayıklama aracı olduğu için sistem fontuna güvenmek
/// kabul edilebilir; bulunamazsa gösterge sessizce kapanır.
std::string FindSystemFont() {
  static constexpr std::array<const char*, 8> kCandidates{{
#ifdef _WIN32
      "C:/Windows/Fonts/consola.ttf",
      "C:/Windows/Fonts/segoeui.ttf",
      "C:/Windows/Fonts/arial.ttf",
      "C:/Windows/Fonts/tahoma.ttf",
      nullptr,
      nullptr,
      nullptr,
      nullptr,
#else
      "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf",
      "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
      "/usr/share/fonts/truetype/liberation/LiberationMono-Regular.ttf",
      "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf",
      "/usr/share/fonts/TTF/DejaVuSansMono.ttf",
      "/usr/share/fonts/TTF/DejaVuSans.ttf",
      "/System/Library/Fonts/Menlo.ttc",
      "/System/Library/Fonts/Helvetica.ttc",
#endif
  }};
  for (const char* path : kCandidates) {
    if (path == nullptr) {
      continue;
    }
    SDL_IOStream* io = SDL_IOFromFile(path, "rb");
    if (io != nullptr) {
      SDL_CloseIO(io);
      return path;
    }
  }
  return {};
}

/// @brief Gosterge satiri: etiket + deger.
struct Line {
  std::string label;
  std::string value;
};

std::string FormatDouble(double v, int32_t decimals) {
  std::array<char, 32> buf{};
  std::snprintf(buf.data(), buf.size(), "%.*f", decimals, v);
  return buf.data();
}

std::vector<Line> BuildLines(StatsOverlayMode mode, double fps,
                             const FrameStats& stats) {
  std::vector<Line> lines;
  lines.push_back({"FPS", FormatDouble(fps, 1)});
  if (mode != StatsOverlayMode::kDetailed) {
    return lines;
  }
  lines.push_back({"CPU", FormatDouble(stats.cpu_frame_ms, 2) + " ms"});
  // 0 = olculmuyor (Vulkan backend veya timer query yok); yaniltmamak icin
  // sifir yerine acikca "n/a" yaz.
  lines.push_back({"GPU", stats.gpu_frame_ms > 0.0
                              ? FormatDouble(stats.gpu_frame_ms, 2) + " ms"
                              : std::string("n/a")});
  lines.push_back({"draw", std::to_string(stats.draw_calls)});
  lines.push_back({"batch", std::to_string(stats.batches)});
  lines.push_back({"vertex", std::to_string(stats.vertices)});
  lines.push_back({"state", std::to_string(stats.state_changes)});
  return lines;
}

}  // namespace

StatsOverlay::StatsOverlay(const std::string& font_path, int32_t point_size) {
  const std::string path = font_path.empty() ? FindSystemFont() : font_path;
  if (path.empty()) {
    spdlog::warn(
        "[StatsOverlay] TTF font bulunamadi; ekran ustu gosterge devre disi "
        "(pencere basligi gostergesi calismaya devam eder).");
    return;
  }
  auto font = std::make_shared<Font>(path, point_size);
  if (!font->IsValid()) {
    spdlog::warn("[StatsOverlay] Font yuklenemedi: {}", path);
    return;
  }
  mFont = std::move(font);
}

StatsOverlay::~StatsOverlay() = default;

void StatsOverlay::Sample(uint64_t frame_ns) {
  mAccumNs += frame_ns;
  ++mAccumFrames;
  if (mAccumNs == 0) {
    return;
  }
  // Ilk deger icin pencerenin dolmasini bekleme: aksi halde uygulama
  // acilisinda ceyrek saniye boyunca "0.0 FPS" yaziyor.
  const bool bootstrap = (mFps == 0.0) && (mAccumFrames >= kBootstrapFrames);
  if (mAccumNs < kFpsWindowNs && !bootstrap) {
    return;
  }
  mFps =
      static_cast<double>(mAccumFrames) * 1.0e9 / static_cast<double>(mAccumNs);
  mAccumNs = 0;
  mAccumFrames = 0;
}

void StatsOverlay::Draw(Painter& painter, StatsOverlayMode mode,
                        const FrameStats& stats) {
  if (mode == StatsOverlayMode::kNone || mFont == nullptr) {
    return;
  }

  const std::vector<Line> lines = BuildLines(mode, mFps, stats);

  // Font yuksekligi: "Ag" hem ascender hem descender icerir, satir yuksekligi
  // icin temsili bir olcum verir (Font dogrudan bir Height() sunmuyor).
  int32_t ref_w = 0;
  int32_t ref_h = 0;
  mFont->MeasureText("Ag", ref_w, ref_h);
  const auto font_h = static_cast<float>(ref_h);
  const float line_h = font_h + kLineGap;

  // Etiket sutunu, en genis etikete gore hizalanir.
  float label_w = 0.0F;
  float value_w = 0.0F;
  for (const auto& line : lines) {
    int32_t w = 0;
    int32_t h = 0;
    mFont->MeasureText(line.label, w, h);
    label_w = std::max(label_w, static_cast<float>(w));
    mFont->MeasureText(line.value, w, h);
    value_w = std::max(value_w, static_cast<float>(w));
  }
  const float column_gap = font_h * 0.6F;
  // Metin panelin 2*kPadding'inde basliyor, panel kPadding'de; sag kenarda
  // ayni bosluk kalsin diye toplam 3 birim ayrilir.
  const float panel_w = (kPadding * 3.0F) + label_w + column_gap + value_w;
  const float panel_h =
      (kPadding * 2.0F) + (line_h * static_cast<float>(lines.size()));

  // Uygulamanin cizim durumunu bozma: transform, clip ve opaklik geri alinir.
  painter.Save();
  painter.ResetTransform();
  painter.ClearClip();
  painter.SetOpacity(1.0F);

  painter.SetBrush(Brush(kPanelColor));
  painter.FillRect(kPadding, kPadding, panel_w, panel_h);

  // Metin taban cizgisine gore konumlanir.
  float baseline = kPadding + kPadding + static_cast<float>(mFont->Ascent());

  // Font, Save/Restore kapsaminda DEGIL (paylasilan kaynak); elle geri konur.
  std::shared_ptr<Font> previous_font = painter.GetFont();
  painter.SetFont(mFont);
  for (const auto& line : lines) {
    painter.SetPen(Pen(kLabelColor, 1.0F));
    painter.DrawText(kPadding * 2.0F, baseline, line.label);
    painter.SetPen(Pen(kTextColor, 1.0F));
    painter.DrawText(kPadding * 2.0F + label_w + column_gap, baseline,
                     line.value);
    baseline += line_h;
  }
  painter.SetFont(std::move(previous_font));

  painter.Restore();
}

}  // namespace app_detail
}  // namespace sdl_painter
