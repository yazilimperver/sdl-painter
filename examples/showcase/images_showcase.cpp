/// @brief images_showcase — doku, örnekleme filtresi, mesh warp, hedef, metin.
///
/// README tanıtım GIF'inin (`examples/hero.cpp`) dördüncü perdesi, tek başına
/// çalışan bir örnek hâline getirilmiş hâli. Dokular repodan okunmuyor,
/// prosedürel üretiliyor: örnek varlık dosyası olmadan çalışır.
///
/// Kontroller:
///   SPACE — animasyonu duraklat
///   F1    — kare istatistiği (draw call / vertex)
///   ESC   — çıkış

#include "sdl_painter/app/application.h"
#include "sdl_painter/font.h"
#include "sdl_painter/image.h"
#include "sdl_painter/render_target.h"

#include <cmath>
#include <cstdint>
#include <memory>
#include <spdlog/spdlog.h>
#include <string>
#include <vector>

#include "example_font.h"

#ifdef DrawText
#undef DrawText
#endif

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
constexpr float kGap = 10.0F;

const sp::Color kBackground{14, 16, 24, 255};
const sp::Color kPanel{26, 30, 44, 255};
const sp::Color kInk{228, 234, 246, 255};
const sp::Color kMuted{124, 137, 166, 255};
const sp::Color kLine{48, 55, 76, 255};

const sp::Color kCoral{240, 110, 110, 255};
const sp::Color kMint{120, 220, 175, 255};
const sp::Color kAmber{248, 200, 110, 255};
const sp::Color kCyan{110, 215, 235, 255};

/// @brief [0,1) tur konumundan yumuşak, periyodik bir 0→1→0 zarfı.
float Pulse(float phase) {
  return 0.5F - 0.5F * std::cos(phase * 2.0F * kPi);
}

/// @brief Prosedürel dama tahtası + degrade doku.
sp::Image MakeCheckerImage() {
  constexpr int32_t kSize = 128;
  std::vector<uint8_t> pixels(static_cast<std::size_t>(kSize) * kSize * 4);
  for (int32_t y = 0; y < kSize; ++y) {
    for (int32_t x = 0; x < kSize; ++x) {
      const bool dark = ((x / 16) + (y / 16)) % 2 == 0;
      const auto ramp = static_cast<uint8_t>(x * 255 / (kSize - 1));
      const auto idx = static_cast<std::size_t>(y * kSize + x) * 4;
      pixels[idx + 0] = dark ? 60 : ramp;
      pixels[idx + 1] = dark ? 90 : static_cast<uint8_t>(140 + ramp / 4);
      pixels[idx + 2] = dark ? 150 : 235;
      pixels[idx + 3] = 255;
    }
  }
  return sp::Image::CreateFromData(pixels.data(), kSize, kSize, 4);
}

constexpr int32_t kSpriteW = 11;
constexpr int32_t kSpriteH = 8;

/// Filtre farkı ancak böyle sert kenarlı, küçük bir dokuda görünür oluyor.
const char* const kSpriteRows[kSpriteH] = {
    "..X.....X..", "...X...X...", "..XXXXXXX..", ".XX.XXX.XX.",
    "XXXXXXXXXXX", "X.XXXXXXX.X", "X.X.....X.X", "...XX.XX...",
};

/// @brief 11x8 piksel sprite; @p filter doku yaratılırken uygulanır.
sp::Image MakeSpriteImage(sp::TextureFilter filter) {
  std::vector<uint8_t> pixels(static_cast<std::size_t>(kSpriteW) * kSpriteH * 4,
                              0);
  for (int32_t y = 0; y < kSpriteH; ++y) {
    for (int32_t x = 0; x < kSpriteW; ++x) {
      if (kSpriteRows[y][x] != 'X') {
        continue;
      }
      const auto idx = static_cast<std::size_t>(y * kSpriteW + x) * 4;
      const sp::Color c = (y < 3) ? kCyan : kMint;
      pixels[idx + 0] = c.r;
      pixels[idx + 1] = c.g;
      pixels[idx + 2] = c.b;
      pixels[idx + 3] = 255;
    }
  }
  sp::Image image =
      sp::Image::CreateFromData(pixels.data(), kSpriteW, kSpriteH, 4);
  image.SetFilter(filter);
  return image;
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

class ImagesShowcase : public sp::Application {
 public:
  using Application::Application;

 protected:
  bool OnInit() override {
    mChecker = MakeCheckerImage();
    mSpriteNearest = MakeSpriteImage(sp::TextureFilter::kNearest);
    mSpriteLinear = MakeSpriteImage(sp::TextureFilter::kLinear);

    mTarget = GetPainter().CreateRenderTarget(192, 128);
    if (!mTarget.IsValid()) {
      spdlog::warn("Cizim hedefi olusturulamadi; o kart bos kalacak.");
    }

    const std::string path = example::FindSystemFont();
    if (path.empty()) {
      spdlog::warn("Sistem fontu bulunamadi; metin karti bos kalacak.");
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
    const float top_h = kBodyH * 0.58F;
    const float bottom_h = kBodyH - top_h - kGap;
    const float top_w = (kBodyW - 2.0F * kGap) / 3.0F;
    const float bottom_w = (kBodyW - kGap) / 2.0F;

    p.Clear(kBackground);

    // Hedef değiştirmek bir GPU durumu geçişi; biriken çizimleri boşaltıyor.
    // En az sayıda geçiş için sahnenin en başında dolduruluyor.
    FillRenderTarget(p, t);

    DrawChrome(p);

    DrawTexture(p, t, sp::Rect{kBodyX, kBodyY, top_w, top_h});
    DrawFilters(p, t, sp::Rect{kBodyX + top_w + kGap, kBodyY, top_w, top_h});
    DrawMeshWarp(
        p, t, sp::Rect{kBodyX + 2.0F * (top_w + kGap), kBodyY, top_w, top_h});
    DrawRenderTargetStamps(
        p, sp::Rect{kBodyX, kBodyY + top_h + kGap, bottom_w, bottom_h});
    DrawTextCard(p, sp::Rect{kBodyX + bottom_w + kGap, kBodyY + top_h + kGap,
                             bottom_w, bottom_h});
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
          "04  ·  IMAGES & TEXT", sp::Alignment::kLeft, kInk);
    Label(p, mCaptionFont, sp::Rect{kBodyX + 340.0F, 22.0F, 500.0F, 20.0F},
          "textures · filters · mesh warp · render targets · SDL_ttf",
          sp::Alignment::kRight, kMuted);

    p.SetPen(sp::Pen::NoPen());
    p.SetBrush(sp::Brush(kLine));
    p.FillRect(kBodyX, 50.0F, kBodyW, 1.0F);
  }

  /// Ekran yerine dokuya çizim: küçük bir sahne bir kez üretiliyor.
  void FillRenderTarget(sp::Painter& p, float t) {
    if (!mTarget.IsValid() || !p.SetRenderTarget(mTarget)) {
      return;
    }
    const auto w = static_cast<float>(mTarget.Width());
    const auto h = static_cast<float>(mTarget.Height());

    p.Clear(sp::Color{22, 26, 40, 255});
    p.SetPen(sp::Pen::NoPen());
    p.SetBrush(sp::Brush::LinearGradient({0.0F, 0.0F}, {w, h},
                                         sp::Color{46, 58, 96, 255},
                                         sp::Color{20, 24, 38, 255}));
    p.FillRect(0.0F, 0.0F, w, h);

    const sp::Color colors[3] = {kCoral, kMint, kAmber};
    for (int32_t i = 0; i < 3; ++i) {
      const float a =
          t * 2.0F * kPi + static_cast<float>(i) * 2.0F * kPi / 3.0F;
      p.SetBrush(sp::Brush(colors[i]));
      p.FillCircle(w * 0.5F + w * 0.24F * std::cos(a),
                   h * 0.5F + h * 0.24F * std::sin(a), w * 0.12F);
    }

    p.Save();
    p.Translate(w * 0.5F, h * 0.5F);
    p.Rotate(-t * 360.0F);
    p.SetPen(sp::Pen(kCyan, 3.0F));
    p.SetBrush(sp::Brush::NoBrush());
    p.DrawRect(-w * 0.28F, -h * 0.28F, w * 0.56F, h * 0.56F);
    p.Restore();

    p.ResetRenderTarget();
  }

  /// Aynı doku solda transform yığınıyla dönüyor, sağda yatay aynalanmış ve
  /// nabız gibi değişen bir tint ile basılıyor.
  void DrawTexture(sp::Painter& p, float t, const sp::Rect& box) {
    const Cell cell = MakeCell(box);
    Panel(p, cell);
    const sp::Point m = cell.Center();
    const sp::Color white = sp::Color::White();

    p.Save();
    p.Translate(m.x - 44.0F, m.y);
    p.Rotate(t * 360.0F);
    p.DrawImage(mChecker, sp::Rect{-52.0F, -52.0F, 104.0F, 104.0F}, white);
    p.Restore();

    const float pulse = Pulse(t);
    const sp::Color tint{255, static_cast<uint8_t>(140 + 100 * pulse),
                         static_cast<uint8_t>(120 + 80 * pulse), 255};
    p.DrawImage(mChecker, sp::Rect{m.x + 26.0F, m.y - 40.0F, 80.0F, 80.0F},
                tint, sp::ImageFlip::kHorizontal);

    Caption(p, mCaptionFont, cell, "texture · tint · flip");
  }

  /// Aynı 11x8 sprite iki örnekleme filtresiyle büyütülüyor: nearest piksel
  /// sınırlarını korur, linear komşu piksellere karışır.
  void DrawFilters(sp::Painter& p, float t, const sp::Rect& box) {
    const Cell cell = MakeCell(box);
    Panel(p, cell);
    const sp::Point m = cell.Center();
    const sp::Color white = sp::Color::White();

    const float scale = 6.0F + 1.6F * Pulse(t);
    const float w = kSpriteW * scale;
    const float h = kSpriteH * scale;

    p.DrawImage(mSpriteNearest, sp::Rect{m.x - w - 8.0F, m.y - h * 0.5F, w, h},
                white);
    p.DrawImage(mSpriteLinear, sp::Rect{m.x + 8.0F, m.y - h * 0.5F, w, h},
                white);

    const sp::Rect c = cell.content;
    Label(p, mCaptionFont, sp::Rect{c.x, c.Bottom() - 20.0F, c.w * 0.5F, 16.0F},
          "nearest", sp::Alignment::kCenter, kMuted);
    Label(p, mCaptionFont,
          sp::Rect{c.x + c.w * 0.5F, c.Bottom() - 20.0F, c.w * 0.5F, 16.0F},
          "linear", sp::Alignment::kCenter, kMuted);

    Caption(p, mCaptionFont, cell, "pixel art · sampling filter");
  }

  /// Doku düz bir dikdörtgene değil, dalgalanan bir 12x8 ızgaraya basılıyor;
  /// ızgara noktaları serbestçe konumlandırılabiliyor.
  void DrawMeshWarp(sp::Painter& p, float t, const sp::Rect& box) {
    const Cell cell = MakeCell(box);
    Panel(p, cell);
    const sp::Rect c = cell.content;

    constexpr int32_t kCols = 12;
    constexpr int32_t kRows = 8;
    const float w = c.w - 26.0F;
    const float h = c.h - 34.0F;
    const float x0 = c.x + 13.0F;
    const float y0 = c.y + 17.0F;

    std::vector<sp::Point> grid;
    grid.reserve(static_cast<std::size_t>(kCols + 1) * (kRows + 1));
    for (int32_t row = 0; row <= kRows; ++row) {
      for (int32_t col = 0; col <= kCols; ++col) {
        const float u = static_cast<float>(col) / kCols;
        const float v = static_cast<float>(row) / kRows;
        const float wave =
            std::sin((u * 2.2F + t) * 2.0F * kPi) * 11.0F * (0.25F + u);
        grid.push_back({x0 + u * w, y0 + v * h + wave});
      }
    }
    p.DrawImageMesh(mChecker, kCols, kRows, grid, sp::Color::White());

    Caption(p, mCaptionFont, cell, "image mesh · free-form warp");
  }

  /// Hedefteki sahne bir kez çizildi; buraya üç farklı boyut ve tonda
  /// basılıyor — normal bir doku gibi davranıyor.
  void DrawRenderTargetStamps(sp::Painter& p, const sp::Rect& box) {
    const Cell cell = MakeCell(box);
    Panel(p, cell);
    const sp::Rect c = cell.content;

    if (!mTarget.IsValid()) {
      Label(p, mCaptionFont, c, "render target unsupported",
            sp::Alignment::kCenter, kMuted);
      Caption(p, mCaptionFont, cell, "render target · draw once, stamp many");
      return;
    }

    const sp::Color white = sp::Color::White();
    // Üç damganın toplam eni w * (1 + 0.6 + 0.38) + aradaki boşluklar; en
    // büyüğünün boyu buna göre sınırlanmalı, yoksa üçüncüsü karttan taşar.
    const float h = c.h - 24.0F;
    const float w = h * static_cast<float>(mTarget.Width()) /
                    static_cast<float>(mTarget.Height());

    p.DrawRenderTarget(mTarget, sp::Rect{c.x + 10.0F, c.y + 8.0F, w, h}, white);
    p.DrawRenderTarget(
        mTarget,
        sp::Rect{c.x + 24.0F + w, c.y + 8.0F + h * 0.22F, w * 0.6F, h * 0.6F},
        kMint);
    p.DrawRenderTarget(mTarget,
                       sp::Rect{c.x + 36.0F + w * 1.6F, c.y + 8.0F + h * 0.36F,
                                w * 0.38F, h * 0.38F},
                       kCoral, sp::ImageFlip::kHorizontal);

    Caption(p, mCaptionFont, cell, "render target · draw once, stamp many");
  }

  /// Metin bir dikdörtgene hizalanıyor ve sözcük sınırından kaydırılıyor;
  /// glyph'ler ortak bir atlas sayfasından geliyor.
  void DrawTextCard(sp::Painter& p, const sp::Rect& box) {
    const Cell cell = MakeCell(box);
    Panel(p, cell);
    const sp::Rect c = cell.content;

    Label(p, mTitleFont, sp::Rect{c.x + 16.0F, c.y + 6.0F, c.w - 32.0F, 24.0F},
          "SDL_ttf · glyph atlas", sp::Alignment::kLeft, kInk);

    if (mCaptionFont) {
      p.SetFont(mCaptionFont);
      p.SetPen(sp::Pen(kMuted, 0.0F));
      p.DrawText(sp::Rect{c.x + 16.0F, c.y + 28.0F, c.w - 32.0F, c.h - 40.0F},
                 "Left, center and right alignment inside a rectangle, with "
                 "word wrap and multi-line layout.",
                 sp::Alignment::kLeft, sp::TextWrap::kWord);
    }

    Caption(p, mCaptionFont, cell, "text · alignment · word wrap");
  }

  sp::Image mChecker;
  sp::Image mSpriteNearest;
  sp::Image mSpriteLinear;
  sp::RenderTarget mTarget;
  std::shared_ptr<sp::Font> mTitleFont;
  std::shared_ptr<sp::Font> mCaptionFont;
  float mTime{0.0F};
  bool mPaused{false};
};

}  // namespace

int main() {
  sp::AppConfig config;
  config.title = "SDLPainter — images_showcase: SPACE durdur, F1 istatistik";
  config.width = 900;
  config.height = 512;
  config.stats_overlay = sp::StatsOverlayMode::kDetailed;

  ImagesShowcase app(config);
  return app.Run();
}
