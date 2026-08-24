/// @brief charts — yay/dilim ve çok satırlı metnin birlikte kabul testi.
///
/// Bu örnek iki kabiliyetin varlık sebebini bir arada gösterir:
///
///   * **Dilim (pie)** — `FillPie` / `DrawPie` olmadan pasta grafik çizmek
///     için elle üçgen fanı kurmak gerekirdi.
///   * **Çok satırlı metin ve sözcük kaydırma** — `TextWrap::kWord` olmadan
///     bir açıklama kutusunu satırlara elle bölmek gerekirdi.
///
/// Ayrıca kesikli çizgi (ızgara), yuvarlatılmış olmayan çubuklar, polyline
/// ve `Font::MeasureText` ile hizalama kullanılıyor — yani P0'da eklenen
/// her şey tek sahnede.
///
/// SDLPainter'ın hedef kitlesinin bir kısmı tam olarak budur: oyun değil,
/// **araç ve gösterge paneli** çizimi.
///
/// Metin sistemde bulunan bir TTF fontuyla çizilir; font yoksa grafikler
/// etiketsiz çizilir.
///
/// Kontroller:
///   SPACE — veri setini değiştir
///   W     — açıklama kutusunda sözcük kaydırmayı aç/kapat (farkı gör)
///   ESC   — çıkış

#include "sdl_painter/app/application.h"
#include "sdl_painter/font.h"

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "example_font.h"

namespace sp = sdl_painter;

namespace {

constexpr std::size_t kSliceCount = 5;

const std::array<sp::Color, kSliceCount> kPalette = {
    sp::Color{86, 156, 240, 235},  sp::Color{240, 140, 90, 235},
    sp::Color{110, 210, 140, 235}, sp::Color{225, 200, 90, 235},
    sp::Color{175, 130, 235, 235},
};

const std::array<const char*, kSliceCount> kLabels = {"OpenGL", "Vulkan",
                                                      "Metal", "D3D12", "Web"};

/// @brief Açıklama metni — bilinçli olarak uzun, sözcük kaydırma görünsün.
constexpr const char* kCaption =
    "Bu kutudaki metin tek bir dize olarak veriliyor ve satirlara "
    "SDLPainter tarafindan bolunuyor. W tusuyla sozcuk kaydirmayi "
    "kapatirsan ayni dize tek satir olarak cizilir ve kutudan tasar.";

}  // namespace

class ChartsDemo : public sp::Application {
 public:
  using Application::Application;

 protected:
  bool OnInit() override {
    const std::string path = example::FindSystemFont();
    if (!path.empty()) {
      mFont = std::make_shared<sp::Font>(path, 16);
      mSmall = std::make_shared<sp::Font>(path, 13);
      if (!mFont->IsValid()) {
        mFont.reset();
      }
      if (!mSmall->IsValid()) {
        mSmall.reset();
      }
    }
    NextDataSet();
    return true;
  }

  void OnUpdate(float dt) override {
    // Değerler hedeflerine yumuşak yaklaşsın: geçiş, yay açılarının doğru
    // hesaplandığını hareket hâlinde de gösterir.
    for (std::size_t i = 0; i < kSliceCount; ++i) {
      mValues[i] += (mTargets[i] - mValues[i]) * std::fmin(1.0F, dt * 4.0F);
    }
  }

  void OnRender(sp::Painter& painter) override {
    painter.Clear(sp::Color{22, 24, 34, 255});

    DrawPieChart(painter, 210.0F, 230.0F, 130.0F);
    DrawBarChart(painter, sp::Rect{400.0F, 100.0F, 540.0F, 260.0F});
    DrawLineChart(painter, sp::Rect{400.0F, 400.0F, 540.0F, 200.0F});
    DrawCaption(painter, sp::Rect{60.0F, 430.0F, 290.0F, 160.0F});
  }

  void OnKeyDown(const sp::KeyEvent& event) override {
    if (event.repeat) {
      return;
    }
    switch (event.key) {
      case sp::Key::kSpace:
        NextDataSet();
        break;
      case sp::Key::kW:
        mWrap = (mWrap == sp::TextWrap::kWord) ? sp::TextWrap::kNone
                                               : sp::TextWrap::kWord;
        break;
      case sp::Key::kEscape:
        Quit();
        break;
      default:
        break;
    }
  }

 private:
  void NextDataSet() {
    ++mDataSet;
    for (std::size_t i = 0; i < kSliceCount; ++i) {
      // Deterministik ama her sette farklı değerler.
      const float t = static_cast<float>(i + 1) * 1.7F +
                      static_cast<float>(mDataSet) * 0.9F;
      mTargets[i] = 10.0F + 40.0F * (0.5F + 0.5F * std::sin(t));
    }
  }

  [[nodiscard]] float Total() const {
    float sum = 0.0F;
    for (float v : mValues) {
      sum += v;
    }
    return sum > 0.0F ? sum : 1.0F;
  }

  /// @brief Pasta grafik — `FillPie` / `DrawPie`'nin varlık sebebi.
  void DrawPieChart(sp::Painter& painter, float cx, float cy, float r) {
    const float total = Total();
    float start = -90.0F;  // saat 12 yönünden başla

    for (std::size_t i = 0; i < kSliceCount; ++i) {
      const float sweep = 360.0F * mValues[i] / total;

      painter.SetPen(sp::Pen::NoPen());
      painter.SetBrush(sp::Brush(kPalette[i]));
      painter.FillPie(cx, cy, r, r, start, sweep);

      // Dilim çerçevesi: yay + merkeze giden iki yarıçap.
      painter.SetPen(sp::Pen(sp::Color{22, 24, 34, 255}, 2.0F));
      painter.DrawPie(cx, cy, r, r, start, sweep);

      // Etiket dilimin ortasında, dışa doğru.
      if (mSmall) {
        const float mid = (start + sweep * 0.5F) * 3.14159265F / 180.0F;
        const float lx = cx + std::cos(mid) * (r + 34.0F);
        const float ly = cy + std::sin(mid) * (r + 34.0F);
        painter.SetFont(mSmall);
        painter.SetPen(sp::Pen(kPalette[i], 1.0F));
        // Kutuyu etiketin etrafına ortalayarak yerleştir.
        painter.DrawText(sp::Rect{lx - 50.0F, ly - 10.0F, 100.0F, 20.0F},
                         kLabels[i], sp::Alignment::kCenter);
      }
      start += sweep;
    }
  }

  /// @brief Çubuk grafik — kesikli ızgara ve ölçülerek hizalanmış etiketler.
  void DrawBarChart(sp::Painter& painter, const sp::Rect& box) {
    DrawGrid(painter, box);

    const float slot = box.w / static_cast<float>(kSliceCount);
    const float bar_w = slot * 0.55F;
    float max_v = 1.0F;
    for (float v : mValues) {
      max_v = std::fmax(max_v, v);
    }

    painter.SetPen(sp::Pen::NoPen());
    for (std::size_t i = 0; i < kSliceCount; ++i) {
      const float h = box.h * (mValues[i] / (max_v * 1.15F));
      const float x =
          box.x + static_cast<float>(i) * slot + (slot - bar_w) * 0.5F;
      painter.SetBrush(sp::Brush(kPalette[i]));
      painter.FillRect(x, box.y + box.h - h, bar_w, h);

      if (mSmall) {
        painter.SetFont(mSmall);
        painter.SetPen(sp::Pen(sp::Color{215, 222, 236, 235}, 1.0F));
        painter.DrawText(sp::Rect{x, box.y + box.h + 4.0F, bar_w, 18.0F},
                         kLabels[i], sp::Alignment::kCenter);
        painter.SetPen(sp::Pen::NoPen());
      }
    }
  }

  /// @brief Çizgi grafik — polyline, yuvarlak uç/birleşim ve nokta işaretleri.
  void DrawLineChart(sp::Painter& painter, const sp::Rect& box) {
    DrawGrid(painter, box);

    float max_v = 1.0F;
    for (float v : mValues) {
      max_v = std::fmax(max_v, v);
    }

    std::vector<sp::Point> pts;
    pts.reserve(kSliceCount);
    for (std::size_t i = 0; i < kSliceCount; ++i) {
      const float x = box.x + box.w * static_cast<float>(i) /
                                  static_cast<float>(kSliceCount - 1);
      const float y = box.y + box.h - box.h * (mValues[i] / (max_v * 1.15F));
      pts.push_back({x, y});
    }

    sp::Pen line(sp::Color{120, 210, 255, 240}, 4.0F);
    line.SetCapStyle(sp::LineCap::kRound);
    line.SetJoinStyle(sp::LineJoin::kRound);
    painter.SetPen(line);
    painter.DrawPolyline(pts);

    painter.SetPen(sp::Pen::NoPen());
    painter.SetBrush(sp::Brush(sp::Color{255, 255, 255, 240}));
    for (const auto& p : pts) {
      painter.FillCircle(p.x, p.y, 5.0F);
    }
  }

  /// @brief Kesikli yatay ızgara — grafik zeminini okunur kılar.
  static void DrawGrid(sp::Painter& painter, const sp::Rect& box) {
    sp::Pen grid(sp::Color{255, 255, 255, 45}, 1.0F);
    grid.SetDashPattern({5.0F, 5.0F});
    painter.SetPen(grid);
    for (int32_t i = 0; i <= 4; ++i) {
      const float y = box.y + box.h * static_cast<float>(i) / 4.0F;
      painter.DrawLine(box.x, y, box.x + box.w, y);
    }
    painter.SetPen(sp::Pen(sp::Color{255, 255, 255, 90}, 1.5F));
    painter.DrawLine(box.x, box.y + box.h, box.x + box.w, box.y + box.h);
  }

  /// @brief Açıklama kutusu — sözcük kaydırmanın kabul testi.
  void DrawCaption(sp::Painter& painter, const sp::Rect& box) {
    painter.SetPen(sp::Pen::NoPen());
    painter.SetBrush(sp::Brush(sp::Color{34, 38, 52, 230}));
    painter.FillRect(box.x, box.y, box.w, box.h);

    sp::Pen frame(sp::Color{120, 210, 255, 160}, 1.5F);
    frame.SetDashPattern({7.0F, 5.0F});
    painter.SetPen(frame);
    painter.DrawRect(box.x, box.y, box.w, box.h);

    if (!mFont) {
      return;
    }
    painter.SetFont(mFont);
    painter.SetPen(sp::Pen(sp::Color{225, 232, 245, 240}, 1.0F));

    const sp::Rect inner{box.x + 12.0F, box.y + 12.0F, box.w - 24.0F,
                         box.h - 24.0F};
    painter.DrawText(inner, kCaption, sp::Alignment::kLeft, mWrap);
  }

  std::array<float, kSliceCount> mValues{};
  std::array<float, kSliceCount> mTargets{};
  std::shared_ptr<sp::Font> mFont;
  std::shared_ptr<sp::Font> mSmall;
  sp::TextWrap mWrap{sp::TextWrap::kWord};
  int32_t mDataSet{0};
};

int main() {
  sp::AppConfig config;
  config.title = "SDLPainter — charts: SPACE veri seti, W sozcuk kaydirma";
  config.width = 1000;
  config.height = 640;
  config.stats_overlay = sp::StatsOverlayMode::kDetailed;

  ChartsDemo app(config);
  return app.Run();
}
