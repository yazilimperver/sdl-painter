/// @brief transform_showcase — transform yığını, kırpma, viewport, batch'leme.
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

#include "example_font.h"

namespace sp = sdl_painter;

namespace {

constexpr float kPi = 3.14159265358979F;

constexpr float kPeriod = 3.0F;

constexpr float kBodyX = 30.0F;
constexpr float kBodyY = 62.0F;
constexpr float kBodyW = 840.0F;
constexpr float kBodyH = 420.0F;
constexpr float kCaptionH = 24.0F;

/// Alttaki batch şeridinin yüksekliği ve kartlar arası boşluk.
constexpr float kStripH = 96.0F;
constexpr float kGap = 10.0F;

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

/// @brief HSV benzeri hızlı renk çarkı — batch'leme sürüsünü renklendirir.
sp::Color Wheel(float h) {
  const float x = h - std::floor(h);
  const auto channel = [x](float offset) {
    const float v = 0.5F + 0.5F * std::cos(2.0F * kPi * (x + offset));
    return static_cast<uint8_t>(70.0F + 185.0F * v);
  };
  return {channel(0.0F), channel(0.33F), channel(0.66F), 255};
}

/// @brief Gövdedeki bir kart: panel dikdörtgeni + açıklama hariç çizim alanı.
struct Cell {
  sp::Rect box;
  sp::Rect content;

  [[nodiscard]] sp::Point Center() const { return content.Center(); }
};

Cell MakeCell(const sp::Rect& box) {
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

class TransformShowcase : public sp::Application {
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
    const float top_h = kBodyH - kStripH - kGap;
    const float cell_w = (kBodyW - 2.0F * kGap) / 3.0F;

    p.Clear(kBackground);
    DrawChrome(p);

    DrawNestedTransform(p, t, sp::Rect{kBodyX, kBodyY, cell_w, top_h});
    DrawClipRect(p, t, sp::Rect{kBodyX + cell_w + kGap, kBodyY, cell_w, top_h});
    DrawViewport(
        p, t, sp::Rect{kBodyX + 2.0F * (cell_w + kGap), kBodyY, cell_w, top_h});
    DrawBatchStrip(p, t,
                   sp::Rect{kBodyX, kBodyY + top_h + kGap, kBodyW, kStripH});
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
          "03  ·  TRANSFORM & CLIP", sp::Alignment::kLeft, kInk);
    Label(p, mCaptionFont, sp::Rect{kBodyX + 340.0F, 22.0F, 500.0F, 20.0F},
          "nested transforms · scissor clip · viewport · batching",
          sp::Alignment::kRight, kMuted);

    p.SetPen(sp::Pen::NoPen());
    p.SetBrush(sp::Brush(kLine));
    p.FillRect(kBodyX, 50.0F, kBodyW, 1.0F);
  }

  /// Beş seviye iç içe transform: her adımda dönüş ve ölçek bir öncekinin
  /// üstüne binerek birikiyor, tek `Restore` hepsini geri alıyor.
  void DrawNestedTransform(sp::Painter& p, float t, const sp::Rect& box) {
    const Cell cell = MakeCell(box);
    Panel(p, cell);
    const sp::Point m = cell.Center();

    const sp::Color colors[5] = {kAzure, kMint, kAmber, kCoral, kViolet};
    p.Save();
    p.Translate(m.x, m.y);
    p.SetBrush(sp::Brush::NoBrush());
    for (int32_t i = 0; i < 5; ++i) {
      p.Rotate(12.0F + t * 90.0F);
      p.Scale(0.78F, 0.78F);
      p.SetPen(sp::Pen(colors[i], 3.0F));
      // 140 px kare 45°'ye geldiğinde köşegeni ~198 px; karta sığması için
      // taban boy bundan büyük seçilmemeli.
      p.DrawRect(-70.0F, -70.0F, 140.0F, 140.0F);
    }
    p.Restore();

    Caption(p, mCaptionFont, cell, "save / restore · nested transform");
  }

  /// Kırpma dikdörtgeni sağa sola kayıyor; kesikli çerçeve pencerenin nerede
  /// olduğunu gösteriyor, içerik yalnızca orada görünüyor.
  void DrawClipRect(sp::Painter& p, float t, const sp::Rect& box) {
    const Cell cell = MakeCell(box);
    Panel(p, cell);
    const sp::Rect c = cell.content;
    const float pulse = Pulse(t);

    const float win_w = c.w * 0.52F;
    const float win_x = c.x + 10.0F + (c.w - win_w - 20.0F) * pulse;
    const sp::Rect window{win_x, c.y + 14.0F, win_w, c.h - 28.0F};

    p.SetClipRect(window);
    p.SetPen(sp::Pen::NoPen());
    p.SetBrush(sp::Brush(sp::Color{40, 48, 72, 255}));
    p.FillRect(c.x, c.y, c.w, c.h);

    p.SetBrush(sp::Brush(kCyan));
    for (int32_t i = -6; i < 18; ++i) {
      p.Save();
      p.Translate(c.x + static_cast<float>(i) * 22.0F + 22.0F * pulse, c.y);
      p.Rotate(24.0F);
      p.FillRect(0.0F, -20.0F, 9.0F, c.h + 60.0F);
      p.Restore();
    }
    p.SetBrush(sp::Brush(kAmber));
    p.FillCircle(c.Center().x, c.Center().y, 34.0F);
    p.ClearClip();

    sp::Pen frame(kInk, 1.5F);
    frame.SetDashPattern({6.0F, 5.0F});
    p.SetPen(frame);
    p.SetBrush(sp::Brush::NoBrush());
    p.DrawRect(window.x, window.y, window.w, window.h);

    Caption(p, mCaptionFont, cell, "clip rect · scissor");
  }

  /// Viewport içinde (0,0) kartın sol üst köşesi: sahne kendi
  /// koordinatlarında, yerleşimden habersiz yazılıyor.
  void DrawViewport(sp::Painter& p, float t, const sp::Rect& box) {
    const Cell cell = MakeCell(box);
    Panel(p, cell);
    const sp::Rect c = cell.content;
    const float pulse = Pulse(t);

    const sp::Rect vp{c.x + 10.0F, c.y + 12.0F, c.w - 20.0F, c.h - 24.0F};
    p.SetViewport(static_cast<int32_t>(vp.x), static_cast<int32_t>(vp.y),
                  static_cast<int32_t>(vp.w), static_cast<int32_t>(vp.h));
    {
      const float w = vp.w;
      const float h = vp.h;

      p.SetPen(sp::Pen::NoPen());
      p.SetBrush(sp::Brush::LinearGradient({0.0F, 0.0F}, {0.0F, h},
                                           sp::Color{34, 42, 66, 255},
                                           sp::Color{18, 22, 36, 255}));
      p.FillRect(0.0F, 0.0F, w, h);

      p.SetBrush(sp::Brush(kMint));
      p.FillCircle(w * (0.15F + 0.7F * pulse), h * 0.72F - 40.0F * pulse,
                   12.0F);

      p.Save();
      p.Translate(w * 0.5F, h * 0.35F);
      p.Rotate(-t * 360.0F);
      p.SetBrush(sp::Brush(kAmber));
      p.FillPolygon({{0.0F, -26.0F}, {24.0F, 18.0F}, {-24.0F, 18.0F}});
      p.Restore();

      p.SetBrush(sp::Brush(sp::Color{60, 72, 104, 255}));
      p.FillRect(0.0F, h - 14.0F, w, 14.0F);
    }
    p.ResetViewport();

    Caption(p, mCaptionFont, cell, "viewport · local coordinates");
  }

  /// 46×6 kare; her biri kendi Save/Translate/Rotate'i ile çiziliyor ama
  /// tek batch'e giriyor — dönüşüm vertex kopyalanırken CPU'da uygulanıyor.
  void DrawBatchStrip(sp::Painter& p, float t, const sp::Rect& box) {
    const Cell cell = MakeCell(box);
    Panel(p, cell);
    const sp::Rect c = cell.content;

    constexpr int32_t kCols = 46;
    constexpr int32_t kRows = 6;
    const float step_x = (c.w - 24.0F) / static_cast<float>(kCols - 1);
    const float step_y = (c.h - 22.0F) / static_cast<float>(kRows - 1);

    p.SetPen(sp::Pen::NoPen());
    for (int32_t row = 0; row < kRows; ++row) {
      for (int32_t col = 0; col < kCols; ++col) {
        const float u = static_cast<float>(col) / static_cast<float>(kCols - 1);
        const float v = static_cast<float>(row) / static_cast<float>(kRows - 1);
        const float wave = std::sin((u * 3.0F + v * 0.7F + t) * 2.0F * kPi);

        p.Save();
        p.Translate(c.x + 12.0F + static_cast<float>(col) * step_x,
                    c.y + 11.0F + static_cast<float>(row) * step_y);
        p.Rotate(wave * 90.0F);
        p.SetBrush(sp::Brush(Wheel(u * 0.6F + v * 0.2F + t)));
        const float s = 4.0F + 2.5F * (1.0F + wave);
        p.FillRect(-s * 0.5F, -s * 0.5F, s, s);
        p.Restore();
      }
    }

    Caption(p, mCaptionFont, cell, "276 transformed quads · batched");
  }

  std::shared_ptr<sp::Font> mTitleFont;
  std::shared_ptr<sp::Font> mCaptionFont;
  float mTime{0.0F};
  bool mPaused{false};
};

}  // namespace

int main() {
  sp::AppConfig config;
  config.title = "SDLPainter — transform_showcase: SPACE durdur, F1 istatistik";
  config.width = 900;
  config.height = 512;
  config.stats_overlay = sp::StatsOverlayMode::kDetailed;

  TransformShowcase app(config);
  return app.Run();
}
