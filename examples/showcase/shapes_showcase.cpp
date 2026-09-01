/// @brief shapes_showcase — şekiller, yaylar, kesikli çizgiler ve Bézier yolları.
/// Kontroller:
///   SPACE — animasyonu duraklat
///   F1    — kare istatistiği (draw call / vertex)
///   ESC   — çıkış

#include "sdl_painter/app/application.h"
#include "sdl_painter/font.h"
#include "sdl_painter/path.h"

#include <cmath>
#include <cstdint>
#include <memory>
#include <string>

#include "example_font.h"

namespace sp = sdl_painter;

namespace {

constexpr float kPi = 3.14159265358979F;

/// Bir animasyon turunun süresi (saniye).
constexpr float kPeriod = 3.0F;

// Yerleşim: başlık şeridi | kart ızgarası. Kartlar gövdeyi eşit böler.
constexpr float kBodyX = 30.0F;
constexpr float kBodyY = 62.0F;
constexpr float kBodyW = 840.0F;
constexpr float kBodyH = 420.0F;

/// Kartın altında açıklama satırına ayrılan yükseklik.
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

/// @brief Kartın arka planı — aynı zamanda yuvarlatılmış dikdörtgen örneği.
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

class ShapesShowcase : public sp::Application {
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

    DrawRects(p, t);
    DrawRoundShapes(p, t);
    DrawArcs(p, t);
    DrawDashesAndCaps(p, t);
    DrawJoins(p, t);
    DrawPaths(p, t);
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
          "01  ·  SHAPES & PATHS", sp::Alignment::kLeft, kInk);
    Label(p, mCaptionFont, sp::Rect{kBodyX + 340.0F, 22.0F, 500.0F, 20.0F},
          "stroke · fill · arcs · dashes · caps · joins · béziers",
          sp::Alignment::kRight, kMuted);

    p.SetPen(sp::Pen::NoPen());
    p.SetBrush(sp::Brush(kLine));
    p.FillRect(kBodyX, 50.0F, kBodyW, 1.0F);
  }

  /// Dolu dikdörtgen ile çerçeveli yuvarlatılmış dikdörtgen; köşe yarıçapı
  /// tur boyunca 4 → 28 px arasında gidip geliyor.
  void DrawRects(sp::Painter& p, float t) {
    const Cell cell = GridCell(0, 0, 3, 2);
    Panel(p, cell);
    const sp::Point m = cell.Center();

    p.SetPen(sp::Pen::NoPen());
    p.SetBrush(sp::Brush(kCoral));
    p.FillRect(m.x - 98.0F, m.y - 32.0F, 76.0F, 64.0F);

    p.SetBrush(sp::Brush::NoBrush());
    p.SetPen(sp::Pen(kAmber, 3.0F));
    p.DrawRoundedRect(m.x + 22.0F, m.y - 32.0F, 76.0F, 64.0F,
                      4.0F + 24.0F * Pulse(t));

    Caption(p, mCaptionFont, cell, "rect · rounded rect");
  }

  /// Daire ve elips. Segment sayısı yarıçapa göre uyarlandığı için büyüyen
  /// dairenin kenarı kabalaşmıyor.
  void DrawRoundShapes(sp::Painter& p, float t) {
    const Cell cell = GridCell(1, 0, 3, 2);
    Panel(p, cell);
    const sp::Point m = cell.Center();

    p.SetPen(sp::Pen::NoPen());
    p.SetBrush(sp::Brush(kMint));
    p.FillCircle(m.x - 54.0F, m.y, 24.0F + 8.0F * Pulse(t));

    p.SetBrush(sp::Brush::NoBrush());
    p.SetPen(sp::Pen(kAzure, 3.0F));
    p.DrawEllipse(m.x + 50.0F, m.y, 46.0F, 26.0F);

    Caption(p, mCaptionFont, cell, "circle · ellipse");
  }

  /// Aynı açı taramasının üç okunuşu: yay yalnız kenarı çizer, dilim merkeze
  /// kapanır, kiriş uçları doğruyla birleştirir.
  void DrawArcs(sp::Painter& p, float t) {
    const Cell cell = GridCell(2, 0, 3, 2);
    Panel(p, cell);
    const sp::Point m = cell.Center();
    const float sweep = 40.0F + 280.0F * Pulse(t);

    sp::Pen arc(kViolet, 5.0F);
    arc.SetCapStyle(sp::LineCap::kRound);
    p.SetPen(arc);
    p.SetBrush(sp::Brush::NoBrush());
    p.DrawArc(m.x - 72.0F, m.y, 30.0F, 30.0F, -90.0F, sweep);

    p.SetPen(sp::Pen::NoPen());
    p.SetBrush(sp::Brush(kAmber));
    p.FillPie(m.x, m.y, 30.0F, 30.0F, -90.0F, sweep);

    p.SetBrush(sp::Brush(kCyan));
    p.FillChord(m.x + 72.0F, m.y, 30.0F, 30.0F, 20.0F + sweep * 0.25F, 200.0F);

    Caption(p, mCaptionFont, cell, "arc · pie · chord");
  }

  /// Üstte kesik deseni, altta üç uç stili. Alttaki iki çizginin uç noktaları
  /// aynı; square ve round uçlar çizgiyi yarım kalınlık kadar uzatır.
  void DrawDashesAndCaps(sp::Painter& p, float t) {
    const Cell cell = GridCell(0, 1, 3, 2);
    Panel(p, cell);
    const sp::Point m = cell.Center();

    sp::Pen dashed(kViolet, 6.0F);
    dashed.SetDashPattern({12.0F + 10.0F * Pulse(t), 9.0F});
    p.SetPen(dashed);
    p.DrawLine(m.x - 92.0F, m.y - 32.0F, m.x + 92.0F, m.y - 32.0F);

    sp::Pen square(kAzure, 11.0F);
    square.SetCapStyle(sp::LineCap::kSquare);
    p.SetPen(square);
    p.DrawLine(m.x - 66.0F, m.y + 2.0F, m.x + 66.0F, m.y + 2.0F);

    sp::Pen round(kMint, 11.0F);
    round.SetCapStyle(sp::LineCap::kRound);
    p.SetPen(round);
    p.DrawLine(m.x - 66.0F, m.y + 34.0F, m.x + 66.0F, m.y + 34.0F);

    Caption(p, mCaptionFont, cell, "dash · butt / square / round caps");
  }

  /// Üç köşe birleşim stili aynı kırıklıkta yan yana; açı sivrildikçe miter
  /// uzar, round ve bevel köşeyi kısa keser.
  void DrawJoins(sp::Painter& p, float t) {
    const Cell cell = GridCell(1, 1, 3, 2);
    Panel(p, cell);
    const sp::Point m = cell.Center();
    const float peak = 20.0F + 20.0F * Pulse(t);

    const sp::LineJoin joins[3] = {sp::LineJoin::kMiter, sp::LineJoin::kRound,
                                   sp::LineJoin::kBevel};
    const sp::Color colors[3] = {kCoral, kAmber, kCyan};
    for (int32_t i = 0; i < 3; ++i) {
      const float cx = m.x + static_cast<float>(i - 1) * 84.0F;
      sp::Pen pen(colors[i], 12.0F);
      pen.SetJoinStyle(joins[i]);
      pen.SetCapStyle(sp::LineCap::kButt);
      p.SetPen(pen);
      p.DrawPolyline({{cx - 28.0F, m.y + 26.0F},
                      {cx, m.y - peak},
                      {cx + 28.0F, m.y + 26.0F}});
    }

    Caption(p, mCaptionFont, cell, "miter · round · bevel joins");
  }

  /// Kapalı yol dolduruluyor (iki quadratic eğriden yaprak), açık yol
  /// çiziliyor (tek kübik Bézier). Eğriler yola eklenirken düzleştirilir.
  void DrawPaths(sp::Painter& p, float t) {
    const Cell cell = GridCell(2, 1, 3, 2);
    Panel(p, cell);
    const sp::Point m = cell.Center();
    const float pulse = Pulse(t);

    const float bend = 44.0F + 16.0F * pulse;
    sp::Path leaf;
    leaf.MoveTo(m.x - 58.0F, m.y + 26.0F);
    leaf.QuadTo(m.x - 40.0F, m.y - bend, m.x + 58.0F, m.y - 26.0F);
    leaf.QuadTo(m.x + 40.0F, m.y + bend, m.x - 58.0F, m.y + 26.0F);
    leaf.Close();
    p.SetPen(sp::Pen::NoPen());
    p.SetBrush(sp::Brush(sp::Color{72, 104, 152, 210}));
    p.FillPath(leaf);

    sp::Path wave;
    wave.MoveTo(m.x - 96.0F, m.y + 18.0F);
    wave.CubicTo(m.x - 40.0F, m.y - 66.0F + 44.0F * pulse, m.x + 40.0F,
                 m.y + 66.0F - 44.0F * pulse, m.x + 96.0F, m.y - 18.0F);
    sp::Pen stroke(kAmber, 5.0F);
    stroke.SetCapStyle(sp::LineCap::kRound);
    stroke.SetJoinStyle(sp::LineJoin::kRound);
    p.SetPen(stroke);
    p.SetBrush(sp::Brush::NoBrush());
    p.DrawPath(wave);

    Caption(p, mCaptionFont, cell, "bézier path · stroke + fill");
  }

  std::shared_ptr<sp::Font> mTitleFont;
  std::shared_ptr<sp::Font> mCaptionFont;
  float mTime{0.0F};
  bool mPaused{false};
};

}  // namespace

int main() {
  sp::AppConfig config;
  config.title = "SDLPainter — shapes_showcase: SPACE durdur, F1 istatistik";
  config.width = 900;
  config.height = 512;
  config.stats_overlay = sp::StatsOverlayMode::kDetailed;

  ShapesShowcase app(config);
  return app.Run();
}
