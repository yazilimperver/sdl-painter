/// @brief paths — Bézier eğrileri ve `Path` ile serbest biçimli çizim.
///
/// `Path`, doğru parçalarını ve Bézier eğrilerini tek bir yolda toplar.
/// Eğriler yola eklenirken kırık çizgiye çevrilir (düzleştirme), sonra
/// kütüphanenin mevcut kalın çizgi ve ear clipping tessellation'ı olduğu gibi
/// kullanılır — yani kalemin **tüm** stil eksenleri (uç, birleşim, kesik) yolda
/// da çalışır.
///
/// Sahnede beş bölüm var:
///
///   1. **Düzleştirme toleransı** — aynı eğri üç farklı toleransla; kaba
///      değerde köşelenme çıplak gözle görünür. Nokta sayıları yazılı.
///   2. **Quadratic ve cubic** — aynı uçlar, farklı kontrol noktası düzeni.
///      Kontrol poligonu ince çizgiyle gösterilir.
///   3. **Kalem stilleri yolda da geçerli** — kesikli, yuvarlak uçlu bir eğri.
///   4. **Dolgu ve çok parçalı yol** — her @ref MoveTo yeni bir alt yol açar;
///      `FillPath` her alt yolu **bağımsız** doldurur.
///   5. **Bilinen sınır: delik yok** — iç içe iki halka çizilir; even-odd
///      dolgu kuralı uygulanmadığı için iç halka delik açmaz, üzerine ikinci
///      bir dolgu biner. Bu, gizlenmesi değil gösterilmesi gereken bir sınır.
///
/// Kontroller:
///   ↑ / ↓ — düzleştirme toleransı (1. bölümdeki orta eğri hariç hepsi)
///   SPACE — dalgayı dondur / devam ettir
///   ESC   — çıkış

#include "sdl_painter/app/application.h"
#include "sdl_painter/font.h"
#include "sdl_painter/path.h"

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

#include "example_font.h"

namespace sp = sdl_painter;

namespace {

constexpr float kMinFlatness = 0.05F;
constexpr float kMaxFlatness = 20.0F;

/// @brief Bir eğriyi kontrol noktalarıyla birlikte tanımlar.
struct CurveSpec {
  sp::Point start;
  sp::Point control1;
  sp::Point control2;
  sp::Point end;
};

/// @brief Cubic eğriden yol kur.
sp::Path MakeCubic(const CurveSpec& spec, float flatness) {
  sp::Path path(flatness);
  path.MoveTo(spec.start);
  path.CubicTo(spec.control1, spec.control2, spec.end);
  return path;
}

}  // namespace

class PathsDemo : public sp::Application {
 public:
  using Application::Application;

 protected:
  bool OnInit() override {
    const std::string path = example::FindSystemFont();
    if (!path.empty()) {
      mFont = std::make_shared<sp::Font>(path, 13);
      if (!mFont->IsValid()) {
        mFont.reset();
      }
    }
    return true;
  }

  void OnUpdate(float delta_seconds) override {
    if (!mPaused) {
      mPhase += delta_seconds * 1.4F;
    }
  }

  void OnRender(sp::Painter& painter) override {
    painter.Clear(sp::Color{18, 20, 30, 255});

    DrawFlatnessRow(painter, 60.0F);
    DrawCurveKindRow(painter, 210.0F);
    DrawPenStyleRow(painter, 360.0F);
    DrawFillRow(painter, 470.0F);
    DrawHoleLimitRow(painter, 620.0F);
  }

  void OnKeyDown(const sp::KeyEvent& event) override {
    switch (event.key) {
      case sp::Key::kUp:
        mFlatness = std::fmin(kMaxFlatness, mFlatness * 1.5F);
        break;
      case sp::Key::kDown:
        mFlatness = std::fmax(kMinFlatness, mFlatness / 1.5F);
        break;
      case sp::Key::kSpace:
        if (!event.repeat) {
          mPaused = !mPaused;
        }
        break;
      case sp::Key::kEscape:
        Quit();
        break;
      default:
        break;
    }
  }

 private:
  /// @brief Etiket — font yoksa sessizce atlanır.
  void Label(sp::Painter& painter, float x, float y, float w,
             const std::string& text) {
    if (!mFont) {
      return;
    }
    painter.SetFont(mFont);
    painter.SetPen(sp::Pen(sp::Color{170, 182, 205, 235}, 1.0F));
    painter.DrawText(sp::Rect{x, y, w, 18.0F}, text, sp::Alignment::kCenter);
  }

  /// @brief 1. bölüm — aynı eğri, üç tolerans. Köşelenme kabada görünür.
  void DrawFlatnessRow(sp::Painter& painter, float y) {
    Label(painter, 20.0F, y - 30.0F, 760.0F,
          "duzlestirme toleransi: kucuk tolerans = daha cok nokta, daha "
          "yumusak egri");

    const std::array<float, 3> kSamples = {8.0F, 1.0F, mFlatness};
    for (std::size_t i = 0; i < kSamples.size(); ++i) {
      const float x = 40.0F + (static_cast<float>(i) * 260.0F);
      const CurveSpec spec{{x, y + 90.0F},
                           {x + 40.0F, y - 20.0F},
                           {x + 160.0F, y + 190.0F},
                           {x + 200.0F, y + 90.0F}};
      const sp::Path path = MakeCubic(spec, kSamples[i]);

      painter.SetPen(sp::Pen(sp::Color{120, 200, 255, 235}, 3.0F));
      painter.DrawPath(path);

      // Uretilen noktalar: kaba toleransta seyrek oldugu dogrudan gorunur.
      painter.SetBrush(sp::Brush(sp::Color{255, 235, 120, 220}));
      for (const sp::SubPath& sub : path.SubPaths()) {
        for (const sp::Point& point : sub.points) {
          painter.FillCircle(point.x, point.y, 2.5F);
        }
      }

      const std::string caption = "flatness " + Format(kSamples[i]) + " / " +
                                  std::to_string(path.PointCount()) + " nokta" +
                                  (i == 2 ? "  <- ok tuslari" : "");
      Label(painter, x - 20.0F, y + 110.0F, 240.0F, caption);
    }
  }

  /// @brief 2. bölüm — quadratic ile cubic yan yana, kontrol poligonuyla.
  void DrawCurveKindRow(sp::Painter& painter, float y) {
    Label(painter, 20.0F, y - 30.0F, 760.0F,
          "quadratic (tek kontrol noktasi) ve cubic (iki kontrol noktasi)");

    // --- Quadratic ---
    const sp::Point q0{60.0F, y + 90.0F};
    const sp::Point qc{160.0F, y - 30.0F};
    const sp::Point q1{260.0F, y + 90.0F};

    sp::Path quad(mFlatness);
    quad.MoveTo(q0);
    quad.QuadTo(qc, q1);

    DrawControlPolygon(painter, {q0, qc, q1});
    painter.SetPen(sp::Pen(sp::Color{130, 235, 170, 240}, 3.0F));
    painter.DrawPath(quad);
    Label(painter, 60.0F, y + 100.0F, 200.0F, "QuadTo");

    // --- Cubic ---
    const sp::Point c0{440.0F, y + 90.0F};
    const sp::Point cc1{500.0F, y - 40.0F};
    const sp::Point cc2{640.0F, y + 200.0F};
    const sp::Point c1{700.0F, y + 90.0F};

    sp::Path cubic(mFlatness);
    cubic.MoveTo(c0);
    cubic.CubicTo(cc1, cc2, c1);

    DrawControlPolygon(painter, {c0, cc1, cc2, c1});
    painter.SetPen(sp::Pen(sp::Color{255, 190, 110, 240}, 3.0F));
    painter.DrawPath(cubic);
    Label(painter, 500.0F, y + 100.0F, 200.0F, "CubicTo");
  }

  /// @brief 3. bölüm — kesik, uç ve birleşim stilleri yolda da çalışır.
  void DrawPenStyleRow(sp::Painter& painter, float y) {
    Label(painter, 20.0F, y - 28.0F, 760.0F,
          "kalem stilleri yolda da gecerli: kesik desen egri boyunca ilerler");

    // Zincirlenmis cubic'lerden bir dalga; faz ile animasyonlu.
    sp::Path wave(mFlatness);
    wave.MoveTo(40.0F, y + 40.0F);
    for (int32_t i = 0; i < 6; ++i) {
      const float x0 = 40.0F + (static_cast<float>(i) * 120.0F);
      const float dir = (i % 2 == 0) ? -1.0F : 1.0F;
      const float amp =
          34.0F * std::sin(mPhase + (static_cast<float>(i) * 0.6F));
      wave.CubicTo(x0 + 40.0F, y + 40.0F + (dir * amp), x0 + 80.0F,
                   y + 40.0F - (dir * amp), x0 + 120.0F, y + 40.0F);
    }

    sp::Pen pen(sp::Color{200, 150, 255, 240}, 6.0F);
    pen.SetCapStyle(sp::LineCap::kRound);
    pen.SetJoinStyle(sp::LineJoin::kRound);
    pen.SetDashPattern({22.0F, 12.0F});
    painter.SetPen(pen);
    painter.DrawPath(wave);
  }

  /// @brief 4. bölüm — dolgu ve çok parçalı yol.
  void DrawFillRow(sp::Painter& painter, float y) {
    Label(painter, 20.0F, y - 28.0F, 760.0F,
          "FillPath: her MoveTo yeni bir alt yol acar, her alt yol bagimsiz "
          "doldurulur");

    // Tek parca: egrilerle sinirlanmis bir yaprak.
    sp::Path leaf(mFlatness);
    leaf.MoveTo(60.0F, y + 100.0F);
    leaf.CubicTo(60.0F, y + 20.0F, 180.0F, y + 20.0F, 180.0F, y + 100.0F);
    leaf.CubicTo(180.0F, y + 20.0F, 60.0F, y + 20.0F, 60.0F, y + 100.0F);
    leaf.Close();

    painter.SetBrush(sp::Brush(sp::Color{90, 190, 255, 200}));
    painter.FillPath(leaf);
    painter.SetPen(sp::Pen(sp::Color{220, 240, 255, 235}, 2.0F));
    painter.DrawPath(leaf);
    Label(painter, 60.0F, y + 108.0F, 120.0F, "tek alt yol");

    // Uc parca tek yolda: tek FillPath cagrisi ucunu de doldurur.
    sp::Path petals(mFlatness);
    for (int32_t i = 0; i < 3; ++i) {
      const float cx = 320.0F + (static_cast<float>(i) * 130.0F);
      const float cy = y + 60.0F;
      petals.MoveTo(cx, cy + 40.0F);
      petals.QuadTo(cx - 60.0F, cy - 20.0F, cx, cy - 40.0F);
      petals.QuadTo(cx + 60.0F, cy - 20.0F, cx, cy + 40.0F);
      petals.Close();
    }

    painter.SetBrush(sp::Brush(sp::Color{255, 160, 120, 190}));
    painter.FillPath(petals);
    painter.SetPen(sp::Pen(sp::Color{255, 220, 200, 220}, 1.5F));
    painter.DrawPath(petals);
    Label(painter, 320.0F, y + 108.0F, 390.0F,
          "uc alt yol, tek FillPath cagrisi");
  }

  /// @brief 5. bölüm — belgelenen sınır: iç alt yol delik açmaz.
  void DrawHoleLimitRow(sp::Painter& painter, float y) {
    Label(painter, 20.0F, y - 28.0F, 760.0F,
          "bilinen sinir: even-odd dolgu kurali YOK — ic halka delik acmaz");

    sp::Path ring(mFlatness);
    AppendCircle(ring, 140.0F, y + 40.0F, 55.0F);
    AppendCircle(ring, 140.0F, y + 40.0F, 26.0F);

    painter.SetBrush(sp::Brush(sp::Color{255, 120, 140, 170}));
    painter.FillPath(ring);
    painter.SetPen(sp::Pen(sp::Color{255, 220, 230, 220}, 1.5F));
    painter.DrawPath(ring);
    Label(painter, 60.0F, y + 100.0F, 170.0F, "iki alt yol ust uste doldu");

    // Cerceve olarak cizmek dogru sonucu verir — sinir yalnizca DOLGUDA.
    painter.SetPen(sp::Pen(sp::Color{140, 235, 190, 240}, 3.0F));
    sp::Path outline(mFlatness);
    AppendCircle(outline, 400.0F, y + 40.0F, 55.0F);
    AppendCircle(outline, 400.0F, y + 40.0F, 26.0F);
    painter.DrawPath(outline);
    Label(painter, 320.0F, y + 100.0F, 170.0F, "cerceve dogru cikar");
  }

  /// @brief Kontrol poligonunu ince kesikli çizgiyle göster.
  void DrawControlPolygon(sp::Painter& painter,
                          const std::vector<sp::Point>& points) {
    sp::Pen guide(sp::Color{255, 255, 255, 70}, 1.0F);
    guide.SetDashPattern({5.0F, 5.0F});
    painter.SetPen(guide);
    painter.DrawPolyline(points);

    painter.SetBrush(sp::Brush(sp::Color{255, 255, 255, 120}));
    for (const sp::Point& point : points) {
      painter.FillCircle(point.x, point.y, 3.0F);
    }
  }

  /// @brief Yola dört cubic ile bir çember alt yolu ekle.
  ///
  /// Çeyrek çemberin cubic yaklaşıklığında kullanılan klasik sabit;
  /// azami hata yarıçapın ~%0.02'sidir.
  static void AppendCircle(sp::Path& path, float cx, float cy, float r) {
    constexpr float kKappa = 0.5522848F;
    const float k = r * kKappa;

    path.MoveTo(cx + r, cy);
    path.CubicTo(cx + r, cy + k, cx + k, cy + r, cx, cy + r);
    path.CubicTo(cx - k, cy + r, cx - r, cy + k, cx - r, cy);
    path.CubicTo(cx - r, cy - k, cx - k, cy - r, cx, cy - r);
    path.CubicTo(cx + k, cy - r, cx + r, cy - k, cx + r, cy);
    path.Close();
  }

  /// @brief Kısa ondalık biçimlendirme (etiketler için).
  static std::string Format(float value) {
    std::array<char, 32> buffer{};
    std::snprintf(buffer.data(), buffer.size(), "%.2f",
                  static_cast<double>(value));
    return {buffer.data()};
  }

  std::shared_ptr<sp::Font> mFont;
  float mFlatness{sp::kDefaultFlatness};
  float mPhase{0.0F};
  bool mPaused{false};
};

int main() {
  sp::AppConfig config;
  config.title = "SDLPainter — paths: ok tuslari tolerans, SPACE dondur";
  config.width = 800;
  config.height = 760;
  config.stats_overlay = sp::StatsOverlayMode::kDetailed;

  PathsDemo app(config);
  return app.Run();
}
