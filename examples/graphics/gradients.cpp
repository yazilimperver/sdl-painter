/// @brief gradients — shader'sız gradient ve sınırının dürüst gösterimi.
///
/// SDLPainter'da gradient bir shader özelliği **değil**: vertex'ler zaten renk
/// taşıyor (batch'leme bunun için yapıldı), bu yüzden geçiş tessellation
/// sırasında köşe renkleri hesaplanarak üretiliyor ve enterpolasyonu donanım
/// yapıyor. İki sonucu var:
///
///   * **Batch kırılmıyor.** Sahnedeki bütün gradientli şekiller, düz renkli
///     olanlarla birlikte tek draw call'a giriyor — F1 katmanındaki sayaç bunu
///     gösteriyor. Gradient shader uniform'u olsaydı her renk rampası bir
///     flush demek olurdu.
///   * **Geçiş, şeklin köşe yoğunluğu kadar hassas.** Dikdörtgende doğrusal
///     geçiş kusursuzdur (tam da donanımın enterpole ettiği şey). Işınsal
///     geçişte ise yarıçapa göre uyarlanan segment sayısı belirleyicidir;
///     G tuşuyla köşe noktalarını görebilirsin.
///
/// Bu sınır bilinçli bir takas: shader tabanlı gradient daha doğru olurdu ama
/// backend'e dokunmayı ve batch'i kırmayı gerektirirdi.
///
/// Gradient koordinatları **çizim koordinatlarıyla aynı uzayda**, yani
/// transform yığınından etkilenir — sağ alttaki dönen kare bunu gösteriyor.
///
/// Kontroller:
///   G     — köşe noktalarını göster (geçişin neden orada olduğu anlaşılsın)
///   SPACE — animasyonu duraklat
///   F1    — kare istatistiği (draw call sayısına bak)
///   ESC   — çıkış

#include "sdl_painter/app/application.h"
#include "sdl_painter/font.h"

#include <cmath>
#include <memory>
#include <string>
#include <vector>

#include "example_font.h"

namespace sp = sdl_painter;

class GradientsDemo : public sp::Application {
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

  void OnUpdate(float dt) override {
    if (!mPaused) {
      mTime += dt;
    }
  }

  void OnRender(sp::Painter& painter) override {
    painter.Clear(sp::Color{18, 20, 28, 255});
    painter.SetPen(sp::Pen::NoPen());

    DrawLinearRow(painter, 60.0F);
    DrawRadialRow(painter, 260.0F);
    DrawSegmentDensityRow(painter, 450.0F);
    DrawTransformedGradient(painter, 640.0F, 120.0F);
  }

  void OnKeyDown(const sp::KeyEvent& event) override {
    if (event.repeat) {
      return;
    }
    switch (event.key) {
      case sp::Key::kG:
        mShowVertices = !mShowVertices;
        break;
      case sp::Key::kSpace:
        mPaused = !mPaused;
        break;
      case sp::Key::kEscape:
        Quit();
        break;
      default:
        break;
    }
  }

 private:
  void Label(sp::Painter& painter, float x, float y, float w,
             const std::string& text) {
    if (!mFont) {
      return;
    }
    painter.SetFont(mFont);
    painter.SetPen(sp::Pen(sp::Color{170, 182, 205, 235}, 1.0F));
    painter.DrawText(sp::Rect{x, y, w, 18.0F}, text, sp::Alignment::kCenter);
    painter.SetPen(sp::Pen::NoPen());
  }

  /// @brief Doğrusal gradient — dikdörtgende kusursuz.
  void DrawLinearRow(sp::Painter& painter, float y) {
    const float w = 200.0F;
    const float h = 110.0F;

    // Dikey.
    painter.SetBrush(sp::Brush::LinearGradient({60.0F, y}, {60.0F, y + h},
                                               sp::Color{250, 200, 80, 255},
                                               sp::Color{220, 60, 120, 255}));
    painter.FillRect(60.0F, y, w, h);
    Label(painter, 60.0F, y + h + 6.0F, w, "dikey");

    // Yatay.
    const float x2 = 300.0F;
    painter.SetBrush(sp::Brush::LinearGradient({x2, y}, {x2 + w, y},
                                               sp::Color{80, 200, 255, 255},
                                               sp::Color{40, 60, 200, 255}));
    painter.FillRect(x2, y, w, h);
    Label(painter, x2, y + h + 6.0F, w, "yatay");

    // Capraz + saydamdan opaga: alfa da enterpole edilir.
    const float x3 = 540.0F;
    painter.SetBrush(sp::Brush::LinearGradient({x3, y}, {x3 + w, y + h},
                                               sp::Color{120, 255, 170, 0},
                                               sp::Color{120, 255, 170, 255}));
    painter.FillRect(x3, y, w, h);
    Label(painter, x3, y + h + 6.0F, w, "capraz + alfa");
  }

  /// @brief Işınsal gradient — farklı şekiller üzerinde.
  void DrawRadialRow(sp::Painter& painter, float y) {
    // Daire: segment sayisi yaricapa gore uyarlandigi icin duzgun.
    painter.SetBrush(sp::Brush::RadialGradient({140.0F, y + 60.0F}, 60.0F,
                                               sp::Color{255, 250, 220, 255},
                                               sp::Color{200, 90, 40, 255}));
    painter.FillCircle(140.0F, y + 60.0F, 60.0F);
    Label(painter, 80.0F, y + 128.0F, 120.0F, "daire");
    MaybeMarkCircleVertices(painter, 140.0F, y + 60.0F, 60.0F);

    // Dikdortgen: yalnizca dort kose oldugu icin isinsal gecis KABA gorunur.
    // Bu, ornegin anlattigi sinirin ta kendisi.
    painter.SetBrush(sp::Brush::RadialGradient({400.0F, y + 60.0F}, 90.0F,
                                               sp::Color{255, 250, 220, 255},
                                               sp::Color{200, 90, 40, 255}));
    painter.FillRect(310.0F, y, 180.0F, 120.0F);
    Label(painter, 310.0F, y + 128.0F, 180.0F,
          "dikdortgen: 4 kose, gecis kaba");

    // Ayni bolge cok kenarli bir sekille: kose sayisi artinca duzelir.
    std::vector<sp::Point> poly;
    const int32_t kSides = 48;
    for (int32_t i = 0; i < kSides; ++i) {
      const float a =
          6.28318F * static_cast<float>(i) / static_cast<float>(kSides);
      poly.push_back(
          {660.0F + std::cos(a) * 90.0F, y + 60.0F + std::sin(a) * 60.0F});
    }
    painter.SetBrush(sp::Brush::RadialGradient({660.0F, y + 60.0F}, 90.0F,
                                               sp::Color{255, 250, 220, 255},
                                               sp::Color{200, 90, 40, 255}));
    painter.FillPolygon(poly);
    Label(painter, 570.0F, y + 128.0F, 180.0F, "48 kose: duzgun");
  }

  /// @brief Aynı ışınsal gradient, artan köşe sayısıyla — sınırın kanıtı.
  void DrawSegmentDensityRow(sp::Painter& painter, float y) {
    Label(
        painter, 60.0F, y - 24.0F, 700.0F,
        "ayni isinsal gecis, artan kose sayisi: gecis kose yogunluguna bagli");

    const int32_t kCounts[] = {3, 5, 8, 16, 64};
    for (int32_t i = 0; i < 5; ++i) {
      const float cx = 130.0F + static_cast<float>(i) * 150.0F;
      const float cy = y + 60.0F;
      const float r = 55.0F;

      std::vector<sp::Point> poly;
      for (int32_t k = 0; k < kCounts[i]; ++k) {
        const float a =
            6.28318F * static_cast<float>(k) / static_cast<float>(kCounts[i]);
        poly.push_back({cx + std::cos(a) * r, cy + std::sin(a) * r});
      }
      painter.SetBrush(sp::Brush::RadialGradient({cx, cy}, r,
                                                 sp::Color{120, 220, 255, 255},
                                                 sp::Color{30, 40, 110, 255}));
      painter.FillPolygon(poly);

      if (mShowVertices) {
        painter.SetBrush(sp::Brush(sp::Color{255, 90, 90, 255}));
        for (const auto& p : poly) {
          painter.FillCircle(p.x, p.y, 3.0F);
        }
      }
      Label(painter, cx - 40.0F, y + 122.0F, 80.0F, std::to_string(kCounts[i]));
    }
  }

  /// @brief Gradient koordinatları şekil-yereldir: transform ile birlikte döner.
  void DrawTransformedGradient(sp::Painter& painter, float x, float y) {
    const float angle = mTime * 40.0F;
    painter.Save();
    painter.Translate(x, y);
    painter.Rotate(angle);
    // Gradient sekil-yerel koordinatta tanimli, bu yuzden sekille beraber
    // doner — ekrana sabitlenmis gibi durmaz.
    painter.SetBrush(sp::Brush::LinearGradient({-60.0F, -60.0F}, {60.0F, 60.0F},
                                               sp::Color{255, 210, 90, 255},
                                               sp::Color{140, 60, 220, 255}));
    painter.FillRoundedRect(-60.0F, -60.0F, 120.0F, 120.0F, 22.0F);
    painter.Restore();
    Label(painter, x - 90.0F, y + 74.0F, 180.0F, "transform ile doner");
  }

  void MaybeMarkCircleVertices(sp::Painter& painter, float cx, float cy,
                               float r) {
    if (!mShowVertices) {
      return;
    }
    // Tessellator'in adaptif segment kurali: max(16, r * 0.5)
    const auto segments = static_cast<int32_t>(std::fmax(16.0F, r * 0.5F));
    painter.SetBrush(sp::Brush(sp::Color{255, 90, 90, 255}));
    for (int32_t i = 0; i < segments; ++i) {
      const float a =
          6.28318F * static_cast<float>(i) / static_cast<float>(segments);
      painter.FillCircle(cx + std::cos(a) * r, cy + std::sin(a) * r, 2.5F);
    }
  }

  std::shared_ptr<sp::Font> mFont;
  float mTime{0.0F};
  bool mShowVertices{false};
  bool mPaused{false};
};

int main() {
  sp::AppConfig config;
  config.title = "SDLPainter — gradients: G kose noktalari, F1 draw call";
  config.width = 820;
  config.height = 780;
  config.stats_overlay = sp::StatsOverlayMode::kDetailed;

  GradientsDemo app(config);
  return app.Run();
}
