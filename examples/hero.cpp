/// @brief hero — README tanıtım GIF'ini üreten koreografili showcase sahnesi.
///
/// Diğer demolardan farkı: burada amaç bir yeteneği izole etmek değil,
/// kütüphanenin **bütün** maharetlerini tek tanıtımda sırayla göstermek.
/// Sahne dört perdeye bölünmüştür; her perde 3 saniye sürer, kendi içinde
/// açılıp kapanır:
///
///   01 · SHAPES & PATHS     — şekiller, yay/dilim/kiriş, kesik çizgi,
///                             uç ve birleşim stilleri, Bézier yolu
///   02 · COLOR & BLENDING   — doğrusal/ışınsal gradient, karıştırma modları,
///                             global opaklık, konkav poligon (ear clipping)
///   03 · TRANSFORM & CLIP   — iç içe transform yığını, kırpma, viewport ve
///                             batch'leme (yüzlerce şekil, birkaç draw call)
///   04 · IMAGES & TEXT      — doku, filtre, mesh warp, çizim hedefi, metin
///
/// İçerik **döngüye uygun** tasarlandı: her perde kendi başında ve sonunda
/// karartıldığı için son kare ilk kareyle örtüşür ve GIF'te görünür bir
/// atlama olmaz. Perde geçişi `SetOpacity` ile değil, renklerin alfasını
/// ölçekleyerek yapılır (bkz. @ref Fade) — renk vertex'te taşındığı için
/// batch kırılmaz; alttaki draw call sayacı bunu görünür kılıyor.
///
/// İki modu var:
///   hero                          → pencerede oynat (ESC ile çık)
///   hero --dump-frames <dizin>    → kLoopFrames kareyi PPM olarak yaz ve çık
///
/// Kare dökümü ekran kaydına göre tercih edilir: imleç ve pencere çerçevesi
/// karışmaz, kare atlaması olmaz, döngü tam kapanır. Üretim akışı:
///   ./hero --dump-frames build/hero_frames
///   ./scripts/make-hero-gif.sh --fps 12   (veya .\scripts\Make-HeroGif.ps1 -Fps 12)

#include <SDL3/SDL.h>

#include <spdlog/sinks/ansicolor_sink.h>
#include <spdlog/spdlog.h>

#ifdef _WIN32
#include <windows.h>
#endif

#include "sdl_painter/brush.h"
#include "sdl_painter/color.h"
#include "sdl_painter/font.h"
#include "sdl_painter/geometry.h"
#include "sdl_painter/image.h"
#include "sdl_painter/painter.h"
#include "sdl_painter/path.h"
#include "sdl_painter/pen.h"
#include "sdl_painter/render_target.h"
#include "sdl_painter/renderer.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

#include "example_font.h"

// windows.h (spdlog üzerinden dolaylı gelebilir) DrawText'i DrawTextA/W
// makrosuna çevirir ve Painter::DrawText çağrısını bozar.
#ifdef DrawText
#undef DrawText
#endif

namespace sp = sdl_painter;

namespace {

constexpr int32_t kWidth = 800;
constexpr int32_t kHeight = 450;

/// Perde sayısı ve perde başına kare: 30 fps × 90 kare = 3 sn, toplam 12 sn.
constexpr int32_t kActCount = 4;
constexpr int32_t kActFrames = 90;
constexpr int32_t kLoopFrames = kActCount * kActFrames;

/// Perdenin açılma/kapanma süresi (kare).
constexpr float kFadeFrames = 9.0F;

constexpr float kPi = 3.14159265358979F;

// --- Yerleşim ---------------------------------------------------------------
// Başlık şeridi | perde gövdesi | künye şeridi. Şeritler her karede aynı
// yerde ve aynı renkte kalır: hem sahneye çerçeve verir hem de GIF'te
// değişmeyen piksel oldukları için dosyayı küçültür.
constexpr float kBodyX = 30.0F;
constexpr float kBodyY = 60.0F;
constexpr float kBodyW = 740.0F;
constexpr float kBodyH = 330.0F;

/// Hücre altındaki açıklama satırına ayrılan yükseklik.
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

/// Perde başlıkları — sırasıyla numara, ad ve alt satır.
struct ActLabel {
  const char* index;
  const char* title;
  const char* subtitle;
};

const ActLabel kActs[kActCount] = {
    {"01", "SHAPES & PATHS",
     "stroke · fill · arcs · dashes · caps · joins · béziers"},
    {"02", "COLOR & BLENDING",
     "linear & radial gradients · blend modes · opacity · ear clipping"},
    {"03", "TRANSFORM & CLIP",
     "nested transforms · scissor clip · viewport · batching"},
    {"04", "IMAGES & TEXT",
     "textures · filters · mesh warp · render targets · SDL_ttf"},
};

/// @brief Sahnenin kullandığı font boyutları.
struct Fonts {
  std::shared_ptr<sp::Font> brand;    ///< Künyedeki isim (22 px).
  std::shared_ptr<sp::Font> title;    ///< Perde başlığı (17 px).
  std::shared_ptr<sp::Font> caption;  ///< Hücre açıklamaları ve künye (12 px).

  [[nodiscard]] bool Valid() const { return brand && title && caption; }
};

void InitLogger() {
#ifdef _WIN32
  HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
  DWORD dwMode = 0;
  GetConsoleMode(hOut, &dwMode);
  SetConsoleMode(hOut, dwMode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
#endif
  auto sink = std::make_shared<spdlog::sinks::ansicolor_stdout_sink_mt>();
  sink->set_pattern("[%H:%M:%S][%L] %v");
  auto logger = std::make_shared<spdlog::logger>("sdlpainter", sink);
  logger->set_level(spdlog::level::info);
  spdlog::set_default_logger(logger);
}

// ---------------------------------------------------------------------------
// Küçük yardımcılar
// ---------------------------------------------------------------------------

float Smoothstep(float t) {
  const float x = std::clamp(t, 0.0F, 1.0F);
  return x * x * (3.0F - 2.0F * x);
}

/// @brief [0,1) döngü konumundan yumuşak, periyodik bir 0→1→0 zarfı.
float Pulse(float phase) {
  return 0.5F - 0.5F * std::cos(phase * 2.0F * kPi);
}

/// @brief Rengin alfasını perde geçiş katsayısıyla ölçekle.
///
/// Geçiş için `SetOpacity` yerine bu kullanılıyor: opaklık bir GPU durumu ve
/// batch'i kırıyor, renk ise vertex'te taşınıyor. Yani perde kararması
/// sahnenin draw call sayısına hiçbir şey eklemiyor.
sp::Color Fade(const sp::Color& color, float f) {
  return {color.r, color.g, color.b,
          static_cast<uint8_t>(static_cast<float>(color.a) *
                               std::clamp(f, 0.0F, 1.0F))};
}

/// @brief HSV benzeri hızlı renk çarkı — batch'leme sürüsünü renklendirmek için.
sp::Color Wheel(float h) {
  const float x = h - std::floor(h);
  const auto channel = [x](float offset) {
    const float v = 0.5F + 0.5F * std::cos(2.0F * kPi * (x + offset));
    return static_cast<uint8_t>(70.0F + 185.0F * v);
  };
  return {channel(0.0F), channel(0.33F), channel(0.66F), 255};
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

/// Piksel sanatı örneği — filtre farkı ancak böyle sert kenarlı bir dokuda
/// görünür hâle geliyor.
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

// ---------------------------------------------------------------------------
// Hücre ızgarası
// ---------------------------------------------------------------------------

/// @brief Perde gövdesindeki bir kart: panel dikdörtgeni + içerik alanı.
struct Cell {
  sp::Rect box;      ///< Panelin tamamı.
  sp::Rect content;  ///< Açıklama satırı hariç çizim alanı.

  [[nodiscard]] sp::Point Center() const { return content.Center(); }
};

Cell MakeCell(const sp::Rect& box) {
  return {box, sp::Rect{box.x, box.y, box.w, box.h - kCaptionH}};
}

/// @brief Gövdeyi @p cols × @p rows ızgaraya böl ve (col,row) hücresini ver.
Cell GridCell(int32_t col, int32_t row, int32_t cols, int32_t rows) {
  constexpr float kGap = 10.0F;
  const float w =
      (kBodyW - kGap * static_cast<float>(cols - 1)) / static_cast<float>(cols);
  const float h =
      (kBodyH - kGap * static_cast<float>(rows - 1)) / static_cast<float>(rows);
  return MakeCell(sp::Rect{kBodyX + static_cast<float>(col) * (w + kGap),
                           kBodyY + static_cast<float>(row) * (h + kGap), w,
                           h});
}

/// @brief Kartın arka planı — aynı zamanda yuvarlatılmış dikdörtgen örneği.
void Panel(sp::Painter& p, const Cell& cell, float fade) {
  p.SetPen(sp::Pen::NoPen());
  p.SetBrush(sp::Brush(Fade(kPanel, fade)));
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

void Caption(sp::Painter& p, const Fonts& fonts, const Cell& cell,
             const std::string& text, float fade) {
  Label(p, fonts.caption,
        sp::Rect{cell.box.x, cell.box.Bottom() - kCaptionH, cell.box.w,
                 kCaptionH - 4.0F},
        text, sp::Alignment::kCenter, Fade(kMuted, fade));
}

// ---------------------------------------------------------------------------
// 01 · SHAPES & PATHS
// ---------------------------------------------------------------------------

void ActShapes(sp::Painter& p, const Fonts& fonts, float t, float fade) {
  const float pulse = Pulse(t);
  const float spin = t * 360.0F;

  // --- Dikdörtgen + yuvarlatılmış dikdörtgen ------------------------------
  {
    const Cell cell = GridCell(0, 0, 3, 2);
    Panel(p, cell, fade);
    const sp::Point m = cell.Center();

    p.SetPen(sp::Pen::NoPen());
    p.SetBrush(sp::Brush(Fade(kCoral, fade)));
    p.FillRect(m.x - 98.0F, m.y - 32.0F, 76.0F, 64.0F);

    p.SetBrush(sp::Brush::NoBrush());
    p.SetPen(sp::Pen(Fade(kAmber, fade), 3.0F));
    p.DrawRoundedRect(m.x + 22.0F, m.y - 32.0F, 76.0F, 64.0F,
                      4.0F + 24.0F * pulse);

    Caption(p, fonts, cell, "rect · rounded rect", fade);
  }

  // --- Daire + elips (yarıçapa göre adaptif tessellation) -----------------
  {
    const Cell cell = GridCell(1, 0, 3, 2);
    Panel(p, cell, fade);
    const sp::Point m = cell.Center();

    p.SetPen(sp::Pen::NoPen());
    p.SetBrush(sp::Brush(Fade(kMint, fade)));
    p.FillCircle(m.x - 54.0F, m.y, 24.0F + 8.0F * pulse);

    p.SetBrush(sp::Brush::NoBrush());
    p.SetPen(sp::Pen(Fade(kAzure, fade), 3.0F));
    p.DrawEllipse(m.x + 50.0F, m.y, 46.0F, 26.0F);

    Caption(p, fonts, cell, "circle · ellipse", fade);
  }

  // --- Yay, dilim, kiriş ---------------------------------------------------
  {
    const Cell cell = GridCell(2, 0, 3, 2);
    Panel(p, cell, fade);
    const sp::Point m = cell.Center();
    const float sweep = 40.0F + 280.0F * pulse;

    sp::Pen arc(Fade(kViolet, fade), 5.0F);
    arc.SetCapStyle(sp::LineCap::kRound);
    p.SetPen(arc);
    p.SetBrush(sp::Brush::NoBrush());
    p.DrawArc(m.x - 72.0F, m.y, 30.0F, 30.0F, -90.0F, sweep);

    p.SetPen(sp::Pen::NoPen());
    p.SetBrush(sp::Brush(Fade(kAmber, fade)));
    p.FillPie(m.x, m.y, 30.0F, 30.0F, -90.0F, sweep);

    p.SetBrush(sp::Brush(Fade(kCyan, fade)));
    p.FillChord(m.x + 72.0F, m.y, 30.0F, 30.0F, 20.0F + sweep * 0.25F, 200.0F);

    Caption(p, fonts, cell, "arc · pie · chord", fade);
  }

  // --- Kesikli çizgi + uç stilleri ----------------------------------------
  {
    const Cell cell = GridCell(0, 1, 3, 2);
    Panel(p, cell, fade);
    const sp::Point m = cell.Center();

    sp::Pen dashed(Fade(kViolet, fade), 6.0F);
    dashed.SetDashPattern({12.0F + 10.0F * pulse, 9.0F});
    p.SetPen(dashed);
    p.DrawLine(m.x - 92.0F, m.y - 32.0F, m.x + 92.0F, m.y - 32.0F);

    sp::Pen square(Fade(kAzure, fade), 11.0F);
    square.SetCapStyle(sp::LineCap::kSquare);
    p.SetPen(square);
    p.DrawLine(m.x - 66.0F, m.y + 2.0F, m.x + 66.0F, m.y + 2.0F);

    sp::Pen round(Fade(kMint, fade), 11.0F);
    round.SetCapStyle(sp::LineCap::kRound);
    p.SetPen(round);
    p.DrawLine(m.x - 66.0F, m.y + 34.0F, m.x + 66.0F, m.y + 34.0F);

    Caption(p, fonts, cell, "dash · butt / square / round caps", fade);
  }

  // --- Birleşim stilleri ---------------------------------------------------
  {
    const Cell cell = GridCell(1, 1, 3, 2);
    Panel(p, cell, fade);
    const sp::Point m = cell.Center();
    const float peak = 20.0F + 20.0F * pulse;

    const sp::LineJoin joins[3] = {sp::LineJoin::kMiter, sp::LineJoin::kRound,
                                   sp::LineJoin::kBevel};
    const sp::Color colors[3] = {kCoral, kAmber, kCyan};
    for (int32_t i = 0; i < 3; ++i) {
      const float cx = m.x + static_cast<float>(i - 1) * 74.0F;
      sp::Pen pen(Fade(colors[i], fade), 12.0F);
      pen.SetJoinStyle(joins[i]);
      pen.SetCapStyle(sp::LineCap::kButt);
      p.SetPen(pen);
      p.DrawPolyline({{cx - 28.0F, m.y + 26.0F},
                      {cx, m.y - peak},
                      {cx + 28.0F, m.y + 26.0F}});
    }

    Caption(p, fonts, cell, "miter · round · bevel joins", fade);
  }

  // --- Bézier yolu (çerçeve + dolgu) --------------------------------------
  {
    const Cell cell = GridCell(2, 1, 3, 2);
    Panel(p, cell, fade);
    const sp::Point m = cell.Center();

    // Kapalı yol → FillPath. İki quadratic eğriden yaprak biçimi; kontrol
    // noktaları nefes alıyor.
    const float bend = 44.0F + 16.0F * pulse;
    sp::Path leaf;
    leaf.MoveTo(m.x - 58.0F, m.y + 26.0F);
    leaf.QuadTo(m.x - 40.0F, m.y - bend, m.x + 58.0F, m.y - 26.0F);
    leaf.QuadTo(m.x + 40.0F, m.y + bend, m.x - 58.0F, m.y + 26.0F);
    leaf.Close();
    p.SetPen(sp::Pen::NoPen());
    p.SetBrush(sp::Brush(Fade(sp::Color{72, 104, 152, 210}, fade)));
    p.FillPath(leaf);

    // Açık yol → DrawPath. Kübik Bézier, kalınlık 5, yuvarlak uç.
    sp::Path wave;
    wave.MoveTo(m.x - 96.0F, m.y + 18.0F);
    wave.CubicTo(m.x - 40.0F, m.y - 66.0F + 44.0F * pulse, m.x + 40.0F,
                 m.y + 66.0F - 44.0F * pulse, m.x + 96.0F, m.y - 18.0F);
    sp::Pen stroke(Fade(kAmber, fade), 5.0F);
    stroke.SetCapStyle(sp::LineCap::kRound);
    stroke.SetJoinStyle(sp::LineJoin::kRound);
    p.SetPen(stroke);
    p.SetBrush(sp::Brush::NoBrush());
    p.DrawPath(wave);

    Caption(p, fonts, cell, "bézier path · stroke + fill", fade);
  }

  // Dönen yıldız, hücrelerin dışında değil — 02'de gradient ile geliyor.
  (void)spin;
}

// ---------------------------------------------------------------------------
// 02 · COLOR & BLENDING
// ---------------------------------------------------------------------------

void ActColor(sp::Painter& p, const Fonts& fonts, float t, float fade) {
  const float pulse = Pulse(t);
  const float spin = t * 360.0F;

  // --- Doğrusal gradient (yönü dönüyor) -----------------------------------
  {
    const Cell cell = GridCell(0, 0, 3, 2);
    Panel(p, cell, fade);
    const sp::Point m = cell.Center();
    const float a = t * 2.0F * kPi;
    const sp::Point from{m.x - 96.0F * std::cos(a), m.y - 44.0F * std::sin(a)};
    const sp::Point to{m.x + 96.0F * std::cos(a), m.y + 44.0F * std::sin(a)};

    p.SetPen(sp::Pen::NoPen());
    p.SetBrush(sp::Brush::LinearGradient(from, to, Fade(kViolet, fade),
                                         Fade(kCyan, fade)));
    p.FillRect(m.x - 96.0F, m.y - 44.0F, 192.0F, 88.0F);

    Caption(p, fonts, cell, "linear gradient", fade);
  }

  // --- Işınsal gradient ----------------------------------------------------
  {
    const Cell cell = GridCell(1, 0, 3, 2);
    Panel(p, cell, fade);
    const sp::Point m = cell.Center();

    p.SetPen(sp::Pen::NoPen());
    p.SetBrush(
        sp::Brush::RadialGradient({m.x, m.y}, 30.0F + 30.0F * pulse,
                                  Fade(sp::Color{255, 236, 190, 255}, fade),
                                  Fade(sp::Color{225, 70, 120, 255}, fade)));
    p.FillCircle(m.x, m.y, 56.0F);

    Caption(p, fonts, cell, "radial gradient", fade);
  }

  // --- Konkav poligon + gradient (transform ile birlikte dönüyor) ---------
  {
    const Cell cell = GridCell(2, 0, 3, 2);
    Panel(p, cell, fade);
    const sp::Point m = cell.Center();

    p.Save();
    p.Translate(m.x, m.y);
    p.Rotate(spin);
    p.SetPen(sp::Pen::NoPen());
    // Gradient koordinatları şekil yereldir; yıldızla birlikte dönüyor.
    p.SetBrush(sp::Brush::RadialGradient({0.0F, 0.0F}, 58.0F, Fade(kMint, fade),
                                         Fade(kAzure, fade)));
    p.FillPolygon(MakeStar(0.0F, 0.0F, 58.0F, 24.0F));
    p.Restore();

    Caption(p, fonts, cell, "concave polygon · ear clipping", fade);
  }

  // --- Toplamalı karıştırma ------------------------------------------------
  {
    const Cell cell = GridCell(0, 1, 3, 2);
    Panel(p, cell, fade);
    const sp::Point m = cell.Center();

    p.SetBlendMode(sp::BlendMode::kAdditive);
    p.SetPen(sp::Pen::NoPen());
    const sp::Color glow[3] = {
        {200, 60, 60, 255}, {60, 200, 90, 255}, {70, 110, 220, 255}};
    for (int32_t i = 0; i < 3; ++i) {
      const float a =
          t * 2.0F * kPi + static_cast<float>(i) * 2.0F * kPi / 3.0F;
      p.SetBrush(sp::Brush(Fade(glow[i], fade)));
      p.FillCircle(m.x + 24.0F * std::cos(a), m.y + 24.0F * std::sin(a), 34.0F);
    }
    p.SetBlendMode(sp::BlendMode::kAlpha);

    Caption(p, fonts, cell, "additive blend", fade);
  }

  // --- Çarpımsal karıştırma ------------------------------------------------
  {
    const Cell cell = GridCell(1, 1, 3, 2);
    Panel(p, cell, fade);
    const sp::Point m = cell.Center();

    p.SetPen(sp::Pen::NoPen());
    p.SetBrush(sp::Brush(Fade(sp::Color{236, 233, 222, 255}, fade)));
    p.FillRoundedRect(m.x - 96.0F, m.y - 40.0F, 192.0F, 80.0F, 8.0F);

    p.SetBlendMode(sp::BlendMode::kMultiply);
    const sp::Color inks[3] = {kAmber, kCyan, kCoral};
    for (int32_t i = 0; i < 3; ++i) {
      const float a =
          -t * 2.0F * kPi + static_cast<float>(i) * 2.0F * kPi / 3.0F;
      p.SetBrush(sp::Brush(Fade(inks[i], fade)));
      p.FillCircle(m.x + 22.0F * std::cos(a), m.y + 14.0F * std::sin(a), 28.0F);
    }
    p.SetBlendMode(sp::BlendMode::kAlpha);

    Caption(p, fonts, cell, "multiply blend", fade);
  }

  // --- Global opaklık merdiveni -------------------------------------------
  {
    const Cell cell = GridCell(2, 1, 3, 2);
    Panel(p, cell, fade);
    const sp::Point m = cell.Center();

    p.SetPen(sp::Pen::NoPen());
    p.SetBrush(sp::Brush(kMint));
    for (int32_t i = 0; i < 5; ++i) {
      // Burada gerçekten SetOpacity kullanılıyor — gösterilen şey o. Perde
      // kararması da aynı değere çarpım olarak giriyor.
      p.SetOpacity((0.2F + 0.2F * static_cast<float>(i)) * fade);
      p.FillCircle(m.x + static_cast<float>(i - 2) * 44.0F, m.y, 19.0F);
    }
    p.SetOpacity(1.0F);

    Caption(p, fonts, cell, "global opacity", fade);
  }
}

// ---------------------------------------------------------------------------
// 03 · TRANSFORM & CLIP
// ---------------------------------------------------------------------------

void ActTransform(sp::Painter& p, const Fonts& fonts, float t, float fade) {
  constexpr float kStripH = 84.0F;
  constexpr float kGap = 10.0F;
  const float top_h = kBodyH - kStripH - kGap;
  const float cell_w = (kBodyW - 2.0F * kGap) / 3.0F;

  const float pulse = Pulse(t);
  const float spin = t * 360.0F;

  // --- İç içe transform yığını --------------------------------------------
  {
    const Cell cell = MakeCell(sp::Rect{kBodyX, kBodyY, cell_w, top_h});
    Panel(p, cell, fade);
    const sp::Point m = cell.Center();

    const sp::Color colors[5] = {kAzure, kMint, kAmber, kCoral, kViolet};
    p.Save();
    p.Translate(m.x, m.y);
    p.SetBrush(sp::Brush::NoBrush());
    for (int32_t i = 0; i < 5; ++i) {
      // Her seviye bir öncekinin üstüne biniyor: dönüş ve ölçek birikiyor.
      p.Rotate(12.0F + spin * 0.25F);
      p.Scale(0.78F, 0.78F);
      p.SetPen(sp::Pen(Fade(colors[i], fade), 3.0F));
      // 140 px kare 45°'ye geldiğinde köşegeni ~198 px; hücreye sığması için
      // taban boy bundan büyük seçilmemeli.
      p.DrawRect(-70.0F, -70.0F, 140.0F, 140.0F);
    }
    p.Restore();

    Caption(p, fonts, cell, "save / restore · nested transform", fade);
  }

  // --- Kırpma (scissor) ----------------------------------------------------
  {
    const Cell cell =
        MakeCell(sp::Rect{kBodyX + cell_w + kGap, kBodyY, cell_w, top_h});
    Panel(p, cell, fade);
    const sp::Rect c = cell.content;

    const float win_w = c.w * 0.52F;
    const float win_x = c.x + 10.0F + (c.w - win_w - 20.0F) * pulse;
    const sp::Rect window{win_x, c.y + 14.0F, win_w, c.h - 28.0F};

    p.SetClipRect(window);
    p.SetPen(sp::Pen::NoPen());
    p.SetBrush(sp::Brush(Fade(sp::Color{40, 48, 72, 255}, fade)));
    p.FillRect(c.x, c.y, c.w, c.h);
    // Kırpma penceresinin içinde kalan kısmı görünen çapraz şeritler.
    p.SetBrush(sp::Brush(Fade(kCyan, fade)));
    for (int32_t i = -6; i < 18; ++i) {
      p.Save();
      p.Translate(c.x + static_cast<float>(i) * 22.0F + 22.0F * pulse, c.y);
      p.Rotate(24.0F);
      p.FillRect(0.0F, -20.0F, 9.0F, c.h + 60.0F);
      p.Restore();
    }
    p.SetBrush(sp::Brush(Fade(kAmber, fade)));
    p.FillCircle(c.Center().x, c.Center().y, 34.0F);
    p.ClearClip();

    sp::Pen frame(Fade(kInk, fade), 1.5F);
    frame.SetDashPattern({6.0F, 5.0F});
    p.SetPen(frame);
    p.SetBrush(sp::Brush::NoBrush());
    p.DrawRect(window.x, window.y, window.w, window.h);

    Caption(p, fonts, cell, "clip rect · scissor", fade);
  }

  // --- Viewport (yerel koordinatlar) --------------------------------------
  {
    const Cell cell = MakeCell(
        sp::Rect{kBodyX + 2.0F * (cell_w + kGap), kBodyY, cell_w, top_h});
    Panel(p, cell, fade);
    const sp::Rect c = cell.content;

    const sp::Rect vp{c.x + 10.0F, c.y + 12.0F, c.w - 20.0F, c.h - 24.0F};
    p.SetViewport(static_cast<int32_t>(vp.x), static_cast<int32_t>(vp.y),
                  static_cast<int32_t>(vp.w), static_cast<int32_t>(vp.h));
    {
      // Bu blokta (0,0) viewport'un sol üst köşesi; sahne kendi
      // koordinatlarında yazılmış gibi çiziliyor.
      const float w = vp.w;
      const float h = vp.h;
      p.SetPen(sp::Pen::NoPen());
      p.SetBrush(sp::Brush::LinearGradient(
          {0.0F, 0.0F}, {0.0F, h}, Fade(sp::Color{34, 42, 66, 255}, fade),
          Fade(sp::Color{18, 22, 36, 255}, fade)));
      p.FillRect(0.0F, 0.0F, w, h);

      p.SetBrush(sp::Brush(Fade(kMint, fade)));
      p.FillCircle(w * (0.15F + 0.7F * pulse), h * 0.72F - 40.0F * pulse,
                   12.0F);

      p.Save();
      p.Translate(w * 0.5F, h * 0.35F);
      p.Rotate(-spin);
      p.SetBrush(sp::Brush(Fade(kAmber, fade)));
      p.FillPolygon({{0.0F, -26.0F}, {24.0F, 18.0F}, {-24.0F, 18.0F}});
      p.Restore();

      p.SetBrush(sp::Brush(Fade(sp::Color{60, 72, 104, 255}, fade)));
      p.FillRect(0.0F, h - 14.0F, w, 14.0F);
    }
    p.ResetViewport();

    Caption(p, fonts, cell, "viewport · local coordinates", fade);
  }

  // --- Batch'leme sürüsü ---------------------------------------------------
  {
    const Cell cell =
        MakeCell(sp::Rect{kBodyX, kBodyY + top_h + kGap, kBodyW, kStripH});
    Panel(p, cell, fade);
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
        // Şekil başına Save/Translate/Rotate — buna rağmen tek batch:
        // dönüşüm CPU'da, vertex kopyalanırken uygulanıyor.
        p.Save();
        p.Translate(c.x + 12.0F + static_cast<float>(col) * step_x,
                    c.y + 11.0F + static_cast<float>(row) * step_y);
        p.Rotate(wave * 90.0F);
        p.SetBrush(sp::Brush(Fade(Wheel(u * 0.6F + v * 0.2F + t), fade)));
        const float s = 4.0F + 2.5F * (1.0F + wave);
        p.FillRect(-s * 0.5F, -s * 0.5F, s, s);
        p.Restore();
      }
    }

    Caption(p, fonts, cell, "276 transformed quads · batched", fade);
  }
}

// ---------------------------------------------------------------------------
// 04 · IMAGES & TEXT
// ---------------------------------------------------------------------------

/// @brief Çizim hedefinin içeriğini üret — ekran yerine dokuya çizim.
void RenderTargetScene(sp::Painter& p, const sp::RenderTarget& target,
                       float t) {
  if (!target.IsValid() || !p.SetRenderTarget(target)) {
    return;
  }
  const float w = static_cast<float>(target.Width());
  const float h = static_cast<float>(target.Height());

  p.Clear(sp::Color{22, 26, 40, 255});
  p.SetPen(sp::Pen::NoPen());
  p.SetBrush(sp::Brush::LinearGradient({0.0F, 0.0F}, {w, h},
                                       sp::Color{46, 58, 96, 255},
                                       sp::Color{20, 24, 38, 255}));
  p.FillRect(0.0F, 0.0F, w, h);

  for (int32_t i = 0; i < 3; ++i) {
    const float a = t * 2.0F * kPi + static_cast<float>(i) * 2.0F * kPi / 3.0F;
    const sp::Color colors[3] = {kCoral, kMint, kAmber};
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

void ActImages(sp::Painter& p, const Fonts& fonts, const sp::Image& checker,
               const sp::Image& sprite_nearest, const sp::Image& sprite_linear,
               const sp::RenderTarget& target, float t, float fade) {
  constexpr float kGap = 10.0F;
  const float top_h = kBodyH * 0.58F;
  const float bottom_h = kBodyH - top_h - kGap;
  const float top_w = (kBodyW - 2.0F * kGap) / 3.0F;
  const float bottom_w = (kBodyW - kGap) / 2.0F;

  const float pulse = Pulse(t);
  const float spin = t * 360.0F;
  const sp::Color white = Fade(sp::Color::White(), fade);

  // --- Doku: ölçek, dönüş, tint, aynalama ---------------------------------
  {
    const Cell cell = MakeCell(sp::Rect{kBodyX, kBodyY, top_w, top_h});
    Panel(p, cell, fade);
    const sp::Point m = cell.Center();

    p.Save();
    p.Translate(m.x - 44.0F, m.y);
    p.Rotate(spin);
    p.DrawImage(checker, sp::Rect{-52.0F, -52.0F, 104.0F, 104.0F}, white);
    p.Restore();

    const sp::Color tint =
        Fade(sp::Color{255, static_cast<uint8_t>(140 + 100 * pulse),
                       static_cast<uint8_t>(120 + 80 * pulse), 255},
             fade);
    p.DrawImage(checker, sp::Rect{m.x + 26.0F, m.y - 40.0F, 80.0F, 80.0F}, tint,
                sp::ImageFlip::kHorizontal);

    Caption(p, fonts, cell, "texture · tint · flip", fade);
  }

  // --- Filtre farkı: nearest vs linear ------------------------------------
  {
    const Cell cell =
        MakeCell(sp::Rect{kBodyX + top_w + kGap, kBodyY, top_w, top_h});
    Panel(p, cell, fade);
    const sp::Point m = cell.Center();
    const float scale = 6.0F + 1.6F * pulse;
    const float w = kSpriteW * scale;
    const float h = kSpriteH * scale;

    p.DrawImage(sprite_nearest, sp::Rect{m.x - w - 8.0F, m.y - h * 0.5F, w, h},
                white);
    p.DrawImage(sprite_linear, sp::Rect{m.x + 8.0F, m.y - h * 0.5F, w, h},
                white);

    Label(p, fonts.caption,
          sp::Rect{cell.content.x, cell.content.Bottom() - 20.0F,
                   cell.content.w * 0.5F, 16.0F},
          "nearest", sp::Alignment::kCenter, Fade(kMuted, fade));
    Label(p, fonts.caption,
          sp::Rect{cell.content.x + cell.content.w * 0.5F,
                   cell.content.Bottom() - 20.0F, cell.content.w * 0.5F, 16.0F},
          "linear", sp::Alignment::kCenter, Fade(kMuted, fade));

    Caption(p, fonts, cell, "pixel art · sampling filter", fade);
  }

  // --- Serbest biçimli doku ızgarası (mesh warp) --------------------------
  {
    const Cell cell = MakeCell(
        sp::Rect{kBodyX + 2.0F * (top_w + kGap), kBodyY, top_w, top_h});
    Panel(p, cell, fade);
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
    p.DrawImageMesh(checker, kCols, kRows, grid, white);

    Caption(p, fonts, cell, "image mesh · free-form warp", fade);
  }

  // --- Çizim hedefi (offscreen) -------------------------------------------
  {
    const Cell cell =
        MakeCell(sp::Rect{kBodyX, kBodyY + top_h + kGap, bottom_w, bottom_h});
    Panel(p, cell, fade);
    const sp::Rect c = cell.content;

    if (target.IsValid()) {
      // Sahne bir kez çizildi; buraya üç farklı boyut ve tonda basılıyor.
      const float h = c.h - 16.0F;
      const float w = h * static_cast<float>(target.Width()) /
                      static_cast<float>(target.Height());
      p.DrawRenderTarget(target, sp::Rect{c.x + 14.0F, c.y + 8.0F, w, h},
                         white);
      p.DrawRenderTarget(
          target,
          sp::Rect{c.x + 28.0F + w, c.y + 8.0F + h * 0.22F, w * 0.6F, h * 0.6F},
          Fade(kMint, fade));
      p.DrawRenderTarget(target,
                         sp::Rect{c.x + 40.0F + w * 1.6F,
                                  c.y + 8.0F + h * 0.36F, w * 0.38F, h * 0.38F},
                         Fade(kCoral, fade), sp::ImageFlip::kHorizontal);
    } else {
      Label(p, fonts.caption, c, "render target unsupported",
            sp::Alignment::kCenter, Fade(kMuted, fade));
    }

    Caption(p, fonts, cell, "render target · draw once, stamp many", fade);
  }

  // --- Metin ---------------------------------------------------------------
  {
    const Cell cell = MakeCell(sp::Rect{
        kBodyX + bottom_w + kGap, kBodyY + top_h + kGap, bottom_w, bottom_h});
    Panel(p, cell, fade);
    const sp::Rect c = cell.content;

    Label(p, fonts.title, sp::Rect{c.x + 16.0F, c.y + 6.0F, c.w - 32.0F, 24.0F},
          "SDL_ttf · glyph atlas", sp::Alignment::kLeft, Fade(kInk, fade));
    if (fonts.caption) {
      p.SetFont(fonts.caption);
      p.SetPen(sp::Pen(Fade(kMuted, fade), 0.0F));
      p.DrawText(sp::Rect{c.x + 16.0F, c.y + 28.0F, c.w - 32.0F, c.h - 40.0F},
                 "Left, center and right alignment inside a rectangle, with "
                 "word wrap and multi-line layout.",
                 sp::Alignment::kLeft, sp::TextWrap::kWord);
    }

    Caption(p, fonts, cell, "text · alignment · word wrap", fade);
  }
}

// ---------------------------------------------------------------------------
// Sabit çerçeve: başlık ve künye
// ---------------------------------------------------------------------------

void DrawChrome(sp::Painter& p, const Fonts& fonts, int32_t act, float fade,
                float progress, const sp::FrameStats& stats) {
  const ActLabel& label = kActs[act];

  // Başlık — perdeyle birlikte açılıp kapanıyor.
  Label(p, fonts.title, sp::Rect{kBodyX, 14.0F, 420.0F, 26.0F},
        std::string(label.index) + "  ·  " + label.title, sp::Alignment::kLeft,
        Fade(kInk, fade));
  Label(p, fonts.caption, sp::Rect{300.0F, 20.0F, 470.0F, 20.0F},
        label.subtitle, sp::Alignment::kRight, Fade(kMuted, fade));

  p.SetPen(sp::Pen::NoPen());
  p.SetBrush(sp::Brush(kLine));
  p.FillRect(kBodyX, 48.0F, kBodyW, 1.0F);
  p.FillRect(kBodyX, 400.0F, kBodyW, 1.0F);

  // Künye — hiç kararmıyor, GIF boyunca sabit.
  Label(p, fonts.brand, sp::Rect{kBodyX, 408.0F, 200.0F, 26.0F}, "SDLPainter",
        sp::Alignment::kLeft, kInk);
  Label(p, fonts.caption, sp::Rect{kBodyX + 122.0F, 414.0F, 320.0F, 20.0F},
        "2D drawing for SDL3  ·  OpenGL + Vulkan", sp::Alignment::kLeft,
        kMuted);

  char stats_text[96];
  std::snprintf(stats_text, sizeof(stats_text), "%u draw calls  ·  %u vertices",
                stats.draw_calls, stats.vertices);
  Label(p, fonts.caption, sp::Rect{460.0F, 414.0F, 310.0F, 20.0F}, stats_text,
        sp::Alignment::kRight, kMuted);

  // İlerleme çubuğu + perde işaretleri.
  p.SetBrush(sp::Brush(kLine));
  p.FillRect(kBodyX, 440.0F, kBodyW, 3.0F);
  p.SetBrush(sp::Brush(kAzure));
  p.FillRect(kBodyX, 440.0F, kBodyW * progress, 3.0F);
  p.SetBrush(sp::Brush(kBackground));
  for (int32_t i = 1; i < kActCount; ++i) {
    p.FillRect(kBodyX + kBodyW * static_cast<float>(i) / kActCount - 1.0F,
               440.0F, 2.0F, 3.0F);
  }
}

// ---------------------------------------------------------------------------
// Sahne
// ---------------------------------------------------------------------------

struct SceneAssets {
  const sp::Image* checker{nullptr};
  const sp::Image* sprite_nearest{nullptr};
  const sp::Image* sprite_linear{nullptr};
  const sp::RenderTarget* target{nullptr};
};

/// @brief Tek bir kareyi çizer. `frame` [0, kLoopFrames) aralığında.
void DrawScene(sp::Painter& painter, const SceneAssets& assets,
               const Fonts& fonts, int32_t frame) {
  const int32_t act = frame / kActFrames;
  const int32_t local = frame % kActFrames;
  const float t = static_cast<float>(local) / kActFrames;

  // Perde başında açıl, sonunda kapan — böylece son kare ilk kareyle örtüşür.
  const float fade = Smoothstep(
      std::min(static_cast<float>(local) / kFadeFrames,
               static_cast<float>(kActFrames - 1 - local) / kFadeFrames));

  painter.Clear(kBackground);

  // Çizim hedefi ekran temizlendikten sonra, gövdeden önce dolduruluyor:
  // hedef bir GPU durumu, kare içinde geçiş yapmak biriken çizimleri flush
  // ediyor — en az sayıda geçiş için sahnenin başında.
  if (act == 3 && assets.target != nullptr) {
    RenderTargetScene(painter, *assets.target, t);
  }

  switch (act) {
    case 0:
      ActShapes(painter, fonts, t, fade);
      break;
    case 1:
      ActColor(painter, fonts, t, fade);
      break;
    case 2:
      ActTransform(painter, fonts, t, fade);
      break;
    default:
      ActImages(painter, fonts, *assets.checker, *assets.sprite_nearest,
                *assets.sprite_linear, *assets.target, t, fade);
      break;
  }

  DrawChrome(painter, fonts, act, fade,
             static_cast<float>(frame + 1) / kLoopFrames,
             painter.GetFrameStats());
}

// ---------------------------------------------------------------------------
// Kare dökümü
// ---------------------------------------------------------------------------

// GL fonksiyonları Windows'ta __stdcall (APIENTRY) kullanır; SDLCALL (__cdecl)
// değil. x64'te tek çağırma kuralı olduğu için fark görünmez, 32-bit derlemede
// stack'i bozar — bu yüzden doğru kural açıkça yazılıyor.
#ifdef APIENTRY
#define SDLPAINTER_GLCALL APIENTRY
#else
#define SDLPAINTER_GLCALL
#endif

using ReadPixelsFn = void(SDLPAINTER_GLCALL*)(int32_t, int32_t, int32_t,
                                              int32_t, uint32_t, uint32_t,
                                              void*);

constexpr uint32_t kGlRgb = 0x1907U;
constexpr uint32_t kGlUnsignedByte = 0x1401U;

/// @brief Geçerli back buffer'ı binary PPM (P6) olarak yazar.
///
/// glReadPixels'e glad üzerinden erişemiyoruz — glad kütüphaneye PRIVATE
/// linklenmiş durumda. SDL zaten public bağımlılık olduğu için fonksiyon
/// işaretçisini ondan alıyoruz; ek bağımlılık gerekmiyor.
///
/// PPM seçildi çünkü kayıpsız, tek başlıklı ve hiçbir kütüphane istemiyor;
/// ffmpeg diziyi doğrudan okuyor.
bool WriteFramePpm(ReadPixelsFn read_pixels, const std::string& path,
                   int32_t width, int32_t height,
                   std::vector<uint8_t>& scratch) {
  const auto row = static_cast<std::size_t>(width) * 3;
  scratch.resize(row * static_cast<std::size_t>(height));
  read_pixels(0, 0, width, height, kGlRgb, kGlUnsignedByte, scratch.data());

  FILE* f = std::fopen(path.c_str(), "wb");
  if (f == nullptr) {
    return false;
  }
  std::fprintf(f, "P6\n%d %d\n255\n", width, height);
  // OpenGL'in orijini sol ALT köşede; PPM sol üstten başlar → satırları ters yaz.
  for (int32_t y = height - 1; y >= 0; --y) {
    std::fwrite(scratch.data() + static_cast<std::size_t>(y) * row, 1, row, f);
  }
  std::fclose(f);
  return true;
}

}  // namespace

int main(int argc, char** argv) {
  InitLogger();

  std::string dump_dir;
  for (int32_t i = 1; i < argc; ++i) {
    const std::string arg = argv[i];
    if (arg == "--dump-frames" && i + 1 < argc) {
      dump_dir = argv[++i];
    } else if (arg == "--help" || arg == "-h") {
      spdlog::info("kullanim: hero [--dump-frames <dizin>]");
      return 0;
    }
  }
  const bool dumping = !dump_dir.empty();

  if (!SDL_Init(SDL_INIT_VIDEO)) {
    spdlog::error("SDL_Init basarisiz: {}", SDL_GetError());
    return 1;
  }

  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
  SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1);
  SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 4);

  // Döküm modunda pencere görünmez: ekranda titremesin, önüne bir şey
  // gelirse kareler bozulmasın.
  SDL_WindowFlags flags = SDL_WINDOW_OPENGL;
  if (dumping) {
    flags |= SDL_WINDOW_HIDDEN;
  }

  SDL_Window* window =
      SDL_CreateWindow("SDLPainter — hero", kWidth, kHeight, flags);
  if (window == nullptr) {
    spdlog::error("SDL_CreateWindow basarisiz: {}", SDL_GetError());
    SDL_Quit();
    return 1;
  }

  auto* read_pixels =
      reinterpret_cast<ReadPixelsFn>(SDL_GL_GetProcAddress("glReadPixels"));
  if (dumping && read_pixels == nullptr) {
    spdlog::error("glReadPixels alinamadi; kare dokumu yapilamiyor.");
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 1;
  }

  int32_t exit_code = 0;
  {
    sp::Painter painter(window, sp::RendererBackend::kOpenGL);
    if (!painter.IsValid()) {
      spdlog::error("Painter baslatilamadi.");
      SDL_DestroyWindow(window);
      SDL_Quit();
      return 1;
    }

    const sp::Image checker = MakeCheckerImage();
    const sp::Image sprite_nearest =
        MakeSpriteImage(sp::TextureFilter::kNearest);
    const sp::Image sprite_linear = MakeSpriteImage(sp::TextureFilter::kLinear);
    const sp::RenderTarget target = painter.CreateRenderTarget(192, 128);
    if (!target.IsValid()) {
      spdlog::warn("Cizim hedefi olusturulamadi; o kart bos kalacak.");
    }

    Fonts fonts;
    const std::string font_path = example::FindSystemFont();
    if (!font_path.empty()) {
      fonts.brand = std::make_shared<sp::Font>(font_path, 22);
      fonts.title = std::make_shared<sp::Font>(font_path, 17);
      fonts.caption = std::make_shared<sp::Font>(font_path, 12);
      if (!fonts.Valid() || !fonts.brand->IsValid() ||
          !fonts.title->IsValid() || !fonts.caption->IsValid()) {
        fonts = Fonts{};
      }
    }
    if (!fonts.Valid()) {
      spdlog::warn("Sistem fontu bulunamadi; sahne metinsiz cizilecek.");
    }

    const SceneAssets assets{&checker, &sprite_nearest, &sprite_linear,
                             &target};

    std::vector<uint8_t> scratch;
    int32_t frame = 0;
    bool running = true;

    while (running) {
      SDL_Event event;
      while (SDL_PollEvent(&event)) {
        if (event.type == SDL_EVENT_QUIT) {
          running = false;
        }
        if (event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_ESCAPE) {
          running = false;
        }
      }

      painter.Begin();
      DrawScene(painter, assets, fonts, frame % kLoopFrames);

      if (dumping) {
        // ClearClip() batcher'i bosaltir ama swap ETMEZ. End() ise once
        // flush edip hemen ardindan SDL_GL_SwapWindow cagirir; swap'ten
        // sonra back buffer'in icerigi tanimsizdir. Bu yuzden kareyi
        // buradan, flush edilmis ve henuz swap edilmemis back buffer'dan
        // okuyoruz.
        painter.ClearClip();
        char name[64];
        std::snprintf(name, sizeof(name), "/frame_%04d.ppm", frame);
        if (!WriteFramePpm(read_pixels, dump_dir + name, kWidth, kHeight,
                           scratch)) {
          spdlog::error("Kare yazilamadi: {}{} (dizin var mi?)", dump_dir,
                        name);
          exit_code = 1;
          running = false;
        }
      }

      painter.End();

      ++frame;
      if (dumping && frame >= kLoopFrames) {
        running = false;
      }
    }

    if (dumping && exit_code == 0) {
      spdlog::info("{} kare yazildi ({}x{}) -> {}", kLoopFrames, kWidth,
                   kHeight, dump_dir);
      spdlog::info(
          "Simdi: scripts/make-hero-gif.sh --fps 12 (veya Make-HeroGif.ps1 "
          "-Fps 12)");
    }
  }

  SDL_DestroyWindow(window);
  SDL_Quit();
  return exit_code;
}
