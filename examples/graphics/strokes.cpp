/// @brief strokes — kalemin tüm stil eksenleri tek sahnede.
///
/// `Pen` uzun süre yalnızca renk ve kalınlıktan ibaretti. Bu örnek eklenen
/// dört ekseni yan yana gösterir, çünkü hepsi ancak karşılaştırıldığında
/// anlaşılır:
///
///   1. Uç stili (cap) — çizginin gerçek uç noktası referans çizgisiyle
///      işaretli; `kSquare` ve `kRound` bunun yarım kalınlık kadar dışına
///      taşar, `kButt` taşmaz.
///   2. Birleşim stili (join) — keskin bir köşede miter, bevel ve round
///      belirgin biçimde ayrışır. Çok keskin açıda miter, sivri çıkıntı
///      üretmemek için kendiliğinden bevel'a düşer (alt satır).
///   3. Kesikli çizgi (dash) — desen yol boyunca sürekli ilerler, köşede
///      sıfırlanmaz; bir kesik köşenin üzerinden geçebilir ve orada birleşim
///      alır.
///   4. Yuvarlatılmış dikdörtgen ve yay — aynı kalem stilleriyle çalışan
///      yeni şekiller.
///
/// Kontroller:
///   ↑ / ↓ — çizgi kalınlığı (stillerin farkı kalınlıkla büyür)
///   D     — kesikli / kesiksiz
///   ESC   — çıkış

#include "sdl_painter/app/application.h"
#include "sdl_painter/font.h"

#include <array>
#include <cmath>
#include <cstddef>
#include <memory>
#include <string>
#include <vector>

#include "example_font.h"

namespace sp = sdl_painter;

namespace {

constexpr float kMinWidth = 2.0F;
constexpr float kMaxWidth = 26.0F;

const std::array<sp::LineCap, 3> kCaps = {
    sp::LineCap::kButt, sp::LineCap::kSquare, sp::LineCap::kRound};
const std::array<const char*, 3> kCapNames = {"butt", "square", "round"};

const std::array<sp::LineJoin, 3> kJoins = {
    sp::LineJoin::kMiter, sp::LineJoin::kBevel, sp::LineJoin::kRound};
const std::array<const char*, 3> kJoinNames = {"miter", "bevel", "round"};

/// @brief Gösterilen kesik desenleri ve adları.
struct DashSample {
  const char* name;
  std::vector<float> pattern;
};

}  // namespace

class StrokesDemo : public sp::Application {
 public:
  using Application::Application;

 protected:
  bool OnInit() override {
    const std::string path = example::FindSystemFont();
    if (!path.empty()) {
      mFont = std::make_shared<sp::Font>(path, 14);
      if (!mFont->IsValid()) {
        mFont.reset();
      }
    }
    return true;
  }

  void OnRender(sp::Painter& painter) override {
    painter.Clear(sp::Color{20, 22, 32, 255});

    DrawCapRow(painter, 70.0F);
    DrawJoinRow(painter, 220.0F);
    DrawMiterLimitRow(painter, 360.0F);
    DrawDashRow(painter, 470.0F);
    DrawShapeRow(painter, 580.0F);
  }

  void OnKeyDown(const sp::KeyEvent& event) override {
    switch (event.key) {
      case sp::Key::kUp:
        mWidth = std::fmin(kMaxWidth, mWidth + 1.0F);
        break;
      case sp::Key::kDown:
        mWidth = std::fmax(kMinWidth, mWidth - 1.0F);
        break;
      case sp::Key::kD:
        if (!event.repeat) {
          mDashed = !mDashed;
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

  /// @brief Kalemi yürürlükteki ayarlarla kur.
  [[nodiscard]] sp::Pen MakePen(const sp::Color& color, sp::LineCap cap,
                                sp::LineJoin join) const {
    sp::Pen pen(color, mWidth);
    pen.SetCapStyle(cap);
    pen.SetJoinStyle(join);
    if (mDashed) {
      pen.SetDashPattern({mWidth * 1.6F, mWidth * 1.1F});
    }
    return pen;
  }

  /// @brief Uç stilleri — referans çizgisi taşmayı görünür kılar.
  void DrawCapRow(sp::Painter& painter, float y) {
    Label(painter, 20.0F, y - 26.0F, 300.0F, "uc stili (cap)");
    for (std::size_t i = 0; i < kCaps.size(); ++i) {
      const float x = 60.0F + static_cast<float>(i) * 200.0F;
      painter.SetPen(MakePen(sp::Color{120, 200, 255, 225}, kCaps[i],
                             sp::LineJoin::kRound));
      painter.DrawLine(x, y, x + 130.0F, y);

      // Gercek uc noktalari: ince dikey isaretler.
      painter.SetPen(sp::Pen(sp::Color{255, 255, 255, 140}, 1.0F));
      painter.DrawLine(x, y - 26.0F, x, y + 26.0F);
      painter.DrawLine(x + 130.0F, y - 26.0F, x + 130.0F, y + 26.0F);

      Label(painter, x, y + 30.0F, 130.0F, kCapNames[i]);
    }
  }

  /// @brief Birleşim stilleri — keskin bir V köşesinde.
  void DrawJoinRow(sp::Painter& painter, float y) {
    Label(painter, 20.0F, y - 26.0F, 300.0F, "birlesim stili (join)");
    for (std::size_t i = 0; i < kJoins.size(); ++i) {
      const float x = 60.0F + static_cast<float>(i) * 200.0F;
      painter.SetPen(MakePen(sp::Color{255, 190, 110, 230}, sp::LineCap::kButt,
                             kJoins[i]));
      painter.DrawPolyline(
          {{x, y + 60.0F}, {x + 65.0F, y}, {x + 130.0F, y + 60.0F}});
      Label(painter, x, y + 66.0F, 130.0F, kJoinNames[i]);
    }
  }

  /// @brief Miter sınırı — açı daraldıkça miter bevel'a düşer.
  void DrawMiterLimitRow(sp::Painter& painter, float y) {
    Label(painter, 20.0F, y - 26.0F, 420.0F,
          "miter siniri: aci daraldikca kendiliginden bevel'a duser");

    const std::array<float, 4> kNarrow = {70.0F, 40.0F, 20.0F, 8.0F};
    for (std::size_t i = 0; i < kNarrow.size(); ++i) {
      const float x = 60.0F + static_cast<float>(i) * 150.0F;
      const float half = kNarrow[i];
      painter.SetPen(MakePen(sp::Color{200, 150, 255, 235}, sp::LineCap::kButt,
                             sp::LineJoin::kMiter));
      // Aci daraldikca miter noktasi uzaklasir; sinir asilinca kirpilir.
      painter.DrawPolyline(
          {{x, y + 70.0F}, {x + half, y}, {x + half * 2.0F, y + 70.0F}});
    }
  }

  /// @brief Kesik desenleri — köşede sıfırlanmadığı görünür.
  void DrawDashRow(sp::Painter& painter, float y) {
    Label(painter, 20.0F, y - 26.0F, 420.0F,
          "kesik deseni yol boyunca surekli: kosede sifirlanmaz");

    const std::array<DashSample, 3> kSamples = {
        DashSample{"12 / 6", {12.0F, 6.0F}},
        DashSample{"nokta-tire", {16.0F, 5.0F, 3.0F, 5.0F}},
        DashSample{"sik", {4.0F, 4.0F}},
    };

    for (std::size_t i = 0; i < kSamples.size(); ++i) {
      const float x = 60.0F + static_cast<float>(i) * 200.0F;
      sp::Pen pen(sp::Color{130, 235, 170, 235}, 3.0F);
      pen.SetCapStyle(sp::LineCap::kRound);
      pen.SetJoinStyle(sp::LineJoin::kRound);
      // initializer_list alan API: desen dogrudan yazilir.
      if (i == 0) {
        pen.SetDashPattern({12.0F, 6.0F});
      } else if (i == 1) {
        pen.SetDashPattern({16.0F, 5.0F, 3.0F, 5.0F});
      } else {
        pen.SetDashPattern({4.0F, 4.0F});
      }
      painter.SetPen(pen);
      // Kose iceren yol: desenin kose uzerinden gectigi gorulur.
      painter.DrawPolyline(
          {{x, y + 40.0F}, {x + 70.0F, y}, {x + 140.0F, y + 40.0F}});
      Label(painter, x, y + 46.0F, 140.0F, kSamples[i].name);
    }
  }

  /// @brief Yeni şekiller: yuvarlatılmış dikdörtgen ve yay.
  void DrawShapeRow(sp::Painter& painter, float y) {
    painter.SetPen(MakePen(sp::Color{255, 220, 120, 235}, sp::LineCap::kRound,
                           sp::LineJoin::kRound));
    painter.DrawRoundedRect(60.0F, y, 170.0F, 90.0F, 26.0F);
    Label(painter, 60.0F, y + 96.0F, 170.0F, "DrawRoundedRect");

    // Azami yaricap: kare girdide daireye dejenere olur.
    painter.DrawRoundedRect(270.0F, y, 90.0F, 90.0F, 45.0F);
    Label(painter, 260.0F, y + 96.0F, 110.0F, "yaricap = boy/2");

    painter.SetPen(MakePen(sp::Color{120, 220, 255, 235}, sp::LineCap::kRound,
                           sp::LineJoin::kRound));
    painter.DrawArc(470.0F, y + 45.0F, 55.0F, 55.0F, 200.0F, 250.0F);
    Label(painter, 410.0F, y + 96.0F, 120.0F, "DrawArc");

    painter.DrawPie(650.0F, y + 45.0F, 55.0F, 55.0F, -60.0F, 140.0F);
    Label(painter, 590.0F, y + 96.0F, 120.0F, "DrawPie");
  }

  std::shared_ptr<sp::Font> mFont;
  float mWidth{8.0F};
  bool mDashed{false};
};

int main() {
  sp::AppConfig config;
  config.title = "SDLPainter — strokes: yukari/asagi kalinlik, D kesik";
  config.width = 820;
  config.height = 720;
  config.stats_overlay = sp::StatsOverlayMode::kDetailed;

  StrokesDemo app(config);
  return app.Run();
}
