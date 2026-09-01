/// @brief color_showcase — gradient, karıştırma modu, opaklık ve ear clipping.
///
/// Kontroller:
///   SPACE — animasyonu duraklat
///   F1    — kare istatistiği (draw call / vertex)
///   ESC   — çıkış

#include "sdl_painter/app/application.h"
#include "sdl_painter/font.h"

#include <cmath>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "example_font.h"

namespace sp = sdl_painter;

namespace {

constexpr float kPi = 3.14159265358979F;

/// Bir animasyon turunun süresi (saniye).
constexpr float kPeriod = 3.0F;

constexpr float kBodyX = 30.0F;
constexpr float kBodyY = 62.0F;
constexpr float kBodyW = 840.0F;
constexpr float kBodyH = 420.0F;
constexpr float kCaptionH = 24.0F;

const sp::Color kBackground{14, 16, 24, 255};
const sp::Color kPanel{26, 30, 44, 255};
const sp::Color kInk{228, 234, 246, 255};
const sp::Color kMuted{124, 137, 166, 255};
const sp::Color kLine{48, 55, 76, 255};

const sp::Color kCoral{240, 110, 110, 255};
const sp::Color kMint{120, 220, 175, 255};
const sp::Color kAzure{110, 170, 250, 255};
const sp::Color kAmber{248, 200, 110, 255};
const sp::Color kViolet{186, 150, 248, 255};
const sp::Color kCyan{110, 215, 235, 255};

/// @brief [0,1) tur konumundan yumuşak, periyodik bir 0→1→0 zarfı.
float Pulse(float phase) {
  return 0.5F - 0.5F * std::cos(phase * 2.0F * kPi);
}

/// @brief Merkezi (cx,cy) olan beş köşeli yıldız — konkav poligon örneği.
std::vector<sp::Point> MakeStar(float cx, float cy, float outer, float inner) {
  std::vector<sp::Point> pts;
  pts.reserve(10);
  for (int32_t i = 0; i < 10; ++i) {
    const float r = (i % 2 == 0) ? outer : inner;
    const float a = static_cast<float>(i) * kPi / 5.0F - kPi / 2.0F;
    pts.push_back({cx + r * std::cos(a), cy + r * std::sin(a)});
  }
  return pts;
}

/// @brief Gövdedeki bir kart: panel dikdörtgeni + açıklama hariç çizim alanı.
struct Cell {
  sp::Rect box;
  sp::Rect content;

  [[nodiscard]] sp::Point Center() const { return content.Center(); }
};

/// @brief Gövdeyi @p cols × @p rows ızgaraya böl ve (col,row) kartını ver.
Cell GridCell(int32_t col, int32_t row, int32_t cols, int32_t rows) {
  constexpr float kGap = 10.0F;
  const float w =
      (kBodyW - kGap * static_cast<float>(cols - 1)) / static_cast<float>(cols);
  const float h =
      (kBodyH - kGap * static_cast<float>(rows - 1)) / static_cast<float>(rows);
  const sp::Rect box{kBodyX + static_cast<float>(col) * (w + kGap),
                     kBodyY + static_cast<float>(row) * (h + kGap), w, h};
  return {box, sp::Rect{box.x, box.y, box.w, box.h - kCaptionH}};
}

void Panel(sp::Painter& p, const Cell& cell) {
  p.SetPen(sp::Pen::NoPen());
  p.SetBrush(sp::Brush(kPanel));
  p.FillRoundedRect(cell.box.x, cell.box.y, cell.box.w, cell.box.h, 10.0F);
}

void Label(sp::Painter& p, const std::shared_ptr<sp::Font>& font,
           const sp::Rect& rect, const std::string& text, sp::Alignment align,
           const sp::Color& color) {
  if (!font) {
    return;
  }
  p.SetFont(font);
  p.SetPen(sp::Pen(color, 0.0F));
  p.DrawText(rect, text, align);
}

void Caption(sp::Painter& p, const std::shared_ptr<sp::Font>& font,
             const Cell& cell, const std::string& text) {
  Label(p, font,
        sp::Rect{cell.box.x, cell.box.Bottom() - kCaptionH, cell.box.w,
                 kCaptionH - 4.0F},
        text, sp::Alignment::kCenter, kMuted);
}

class ColorShowcase : public sp::Application {
 public:
  using Application::Application;

 protected:
  bool OnInit() override {
    const std::string path = example::FindSystemFont();
    if (path.empty()) {
      return true;
    }
    mTitleFont = std::make_shared<sp::Font>(path, 17);
    mCaptionFont = std::make_shared<sp::Font>(path, 12);
    if (!mTitleFont->IsValid() || !mCaptionFont->IsValid()) {
      mTitleFont.reset();
      mCaptionFont.reset();
    }
    return true;
  }

  void OnUpdate(float dt) override {
    if (!mPaused) {
      mTime += dt;
    }
  }

  void OnRender(sp::Painter& p) override {
    const float t = std::fmod(mTime / kPeriod, 1.0F);

    p.Clear(kBackground);
    DrawChrome(p);

    DrawLinearGradient(p, t);
    DrawRadialGradient(p, t);
    DrawConcavePolygon(p, t);
    DrawAdditiveBlend(p, t);
    DrawMultiplyBlend(p, t);
    DrawOpacityLadder(p);
  }

  void OnKeyDown(const sp::KeyEvent& event) override {
    if (event.repeat) {
      return;
    }
    if (event.key == sp::Key::kEscape) {
      Quit();
    } else if (event.key == sp::Key::kSpace) {
      mPaused = !mPaused;
    }
  }

 private:
  void DrawChrome(sp::Painter& p) {
    Label(p, mTitleFont, sp::Rect{kBodyX, 16.0F, 420.0F, 26.0F},
          "02  ·  COLOR & BLENDING", sp::Alignment::kLeft, kInk);
    Label(p, mCaptionFont, sp::Rect{kBodyX + 340.0F, 22.0F, 500.0F, 20.0F},
          "linear & radial gradients · blend modes · opacity · ear clipping",
          sp::Alignment::kRight, kMuted);

    p.SetPen(sp::Pen::NoPen());
    p.SetBrush(sp::Brush(kLine));
    p.FillRect(kBodyX, 50.0F, kBodyW, 1.0F);
  }

  /// Doğrusal geçişin ekseni tur boyunca dönüyor. İki üçgenden ibaret bir
  /// dikdörtgende geçiş kusursuz: tam da donanımın enterpole ettiği şey.
  void DrawLinearGradient(sp::Painter& p, float t) {
    const Cell cell = GridCell(0, 0, 3, 2);
    Panel(p, cell);
    const sp::Point m = cell.Center();

    const float a = t * 2.0F * kPi;
    const sp::Point from{m.x - 96.0F * std::cos(a), m.y - 44.0F * std::sin(a)};
    const sp::Point to{m.x + 96.0F * std::cos(a), m.y + 44.0F * std::sin(a)};

    p.SetPen(sp::Pen::NoPen());
    p.SetBrush(sp::Brush::LinearGradient(from, to, kViolet, kCyan));
    p.FillRect(m.x - 96.0F, m.y - 44.0F, 192.0F, 88.0F);

    Caption(p, mCaptionFont, cell, "linear gradient");
  }

  /// Işınsal geçişin yarıçapı nefes alıyor; yarıçap daireninkini aştığında
  /// kenar rengi doygunlaşmadan kesiliyor.
  void DrawRadialGradient(sp::Painter& p, float t) {
    const Cell cell = GridCell(1, 0, 3, 2);
    Panel(p, cell);
    const sp::Point m = cell.Center();

    p.SetPen(sp::Pen::NoPen());
    p.SetBrush(sp::Brush::RadialGradient({m.x, m.y}, 30.0F + 30.0F * Pulse(t),
                                         sp::Color{255, 236, 190, 255},
                                         sp::Color{225, 70, 120, 255}));
    p.FillCircle(m.x, m.y, 56.0F);

    Caption(p, mCaptionFont, cell, "radial gradient");
  }

  /// Beş köşeli yıldız konkav; triangle fan yetmez, ear clipping gerekir.
  /// Gradient koordinatları şekil yerel olduğu için geçiş yıldızla dönüyor.
  void DrawConcavePolygon(sp::Painter& p, float t) {
    const Cell cell = GridCell(2, 0, 3, 2);
    Panel(p, cell);
    const sp::Point m = cell.Center();

    p.Save();
    p.Translate(m.x, m.y);
    p.Rotate(t * 360.0F);
    p.SetPen(sp::Pen::NoPen());
    p.SetBrush(sp::Brush::RadialGradient({0.0F, 0.0F}, 58.0F, kMint, kAzure));
    p.FillPolygon(MakeStar(0.0F, 0.0F, 58.0F, 24.0F));
    p.Restore();

    Caption(p, mCaptionFont, cell, "concave polygon · ear clipping");
  }

  /// Toplamalı karıştırma: üst üste binen daireler beyaza doğru yığılıyor.
  void DrawAdditiveBlend(sp::Painter& p, float t) {
    const Cell cell = GridCell(0, 1, 3, 2);
    Panel(p, cell);
    const sp::Point m = cell.Center();

    p.SetBlendMode(sp::BlendMode::kAdditive);
    p.SetPen(sp::Pen::NoPen());
    const sp::Color glow[3] = {
        {200, 60, 60, 255}, {60, 200, 90, 255}, {70, 110, 220, 255}};
    for (int32_t i = 0; i < 3; ++i) {
      const float a =
          t * 2.0F * kPi + static_cast<float>(i) * 2.0F * kPi / 3.0F;
      p.SetBrush(sp::Brush(glow[i]));
      p.FillCircle(m.x + 24.0F * std::cos(a), m.y + 24.0F * std::sin(a), 34.0F);
    }
    p.SetBlendMode(sp::BlendMode::kAlpha);

    Caption(p, mCaptionFont, cell, "additive blend");
  }

  /// Çarpımsal karıştırma: aynı daireler açık zemin üzerinde koyulaşıyor —
  /// mürekkep üst üste basılmış gibi.
  void DrawMultiplyBlend(sp::Painter& p, float t) {
    const Cell cell = GridCell(1, 1, 3, 2);
    Panel(p, cell);
    const sp::Point m = cell.Center();

    p.SetPen(sp::Pen::NoPen());
    p.SetBrush(sp::Brush(sp::Color{236, 233, 222, 255}));
    p.FillRoundedRect(m.x - 96.0F, m.y - 40.0F, 192.0F, 80.0F, 8.0F);

    p.SetBlendMode(sp::BlendMode::kMultiply);
    const sp::Color inks[3] = {kAmber, kCyan, kCoral};
    for (int32_t i = 0; i < 3; ++i) {
      const float a =
          -t * 2.0F * kPi + static_cast<float>(i) * 2.0F * kPi / 3.0F;
      p.SetBrush(sp::Brush(inks[i]));
      p.FillCircle(m.x + 22.0F * std::cos(a), m.y + 14.0F * std::sin(a), 28.0F);
    }
    p.SetBlendMode(sp::BlendMode::kAlpha);

    Caption(p, mCaptionFont, cell, "multiply blend");
  }

  /// %20'den %100'e opaklık merdiveni. Her basamak yeni bir draw call:
  /// opaklık vertex'te değil, çizim durumunda taşınıyor.
  void DrawOpacityLadder(sp::Painter& p) {
    const Cell cell = GridCell(2, 1, 3, 2);
    Panel(p, cell);
    const sp::Point m = cell.Center();

    p.SetPen(sp::Pen::NoPen());
    p.SetBrush(sp::Brush(kMint));
    for (int32_t i = 0; i < 5; ++i) {
      p.SetOpacity(0.2F + 0.2F * static_cast<float>(i));
      p.FillCircle(m.x + static_cast<float>(i - 2) * 50.0F, m.y, 19.0F);
    }
    p.SetOpacity(1.0F);

    Caption(p, mCaptionFont, cell, "global opacity");
  }

  std::shared_ptr<sp::Font> mTitleFont;
  std::shared_ptr<sp::Font> mCaptionFont;
  float mTime{0.0F};
  bool mPaused{false};
};

}  // namespace

int main() {
  sp::AppConfig config;
  config.title = "SDLPainter — color_showcase: SPACE durdur, F1 istatistik";
  config.width = 900;
  config.height = 512;
  config.stats_overlay = sp::StatsOverlayMode::kDetailed;

  ColorShowcase app(config);
  return app.Run();
}
