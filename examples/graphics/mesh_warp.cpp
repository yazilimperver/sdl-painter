/// @brief mesh_warp — dokulu ızgara: dörtgen köşelerinin bağımsız hareketi.
///
/// `DrawImage`'ın üç aşırı yüklemesi de eksen hizalı `Rect` alır, yani bir
/// dokuyu ancak dikdörtgen olarak çizebilirsin. Dalgalanan bayrak, sayfa
/// kıvrımı veya sahte perspektif bunun dışına çıkmayı gerektirir; bu örnek
/// `DrawImageMesh` ile o kapıyı açıyor.
///
/// İşleyiş: ızgara köşeleri her karede sinüsle deforme edilir, doku
/// koordinatları ızgara konumundan düzgün türetilir. Yani deformasyon yalnızca
/// konumda; doku hücrelere eşit dağılır ve gerilir.
///
/// Izgara çözünürlüğünün ne kadar önemli olduğu 1/2/3 tuşlarıyla görülür:
/// az hücrede dalga köşeli görünür, çok hücrede yumuşar. Doku enterpolasyonu
/// hücre içinde doğrusaldır, yani eğrilik ancak köşe sayısı kadar
/// hassastır — gradient'teki takasın aynısı.
///
/// Kontroller:
///   1/2/3 — ızgara çözünürlüğü (4x4 / 12x12 / 32x32)
///   W     — tel kafes (ızgarayı göster)
///   SPACE — durdur
///   ESC   — çıkış

#include "sdl_painter/app/application.h"
#include "sdl_painter/font.h"
#include "sdl_painter/image.h"

#include <SDL3/SDL.h>

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "example_font.h"

namespace sp = sdl_painter;

namespace {

constexpr float kPi = 3.14159265358979323846F;

std::string AssetPath(const char* name) {
  const char* base = SDL_GetBasePath();
  return (base != nullptr ? std::string(base) : std::string()) + "assets/" +
         name;
}

}  // namespace

class MeshWarpDemo : public sp::Application {
 public:
  using Application::Application;

 protected:
  bool OnInit() override {
    const std::string path = AssetPath("rpg_character_walk.png");
    mImage = sp::Image(path);
    if (!mImage.IsValid()) {
      SDL_Log("mesh_warp: doku yuklenemedi: %s", path.c_str());
      return false;
    }
    // Piksel sanati: buyutulurken keskin kalsin.
    mImage.SetFilter(sp::TextureFilter::kNearest);

    const std::string font = example::FindSystemFont();
    if (!font.empty()) {
      mFont = std::make_shared<sp::Font>(font, 15);
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
    painter.Clear(sp::Color{20, 22, 32, 255});

    const auto w = static_cast<float>(Width());
    const auto h = static_cast<float>(Height());
    const float quad_w = w * 0.72F;
    const float quad_h = h * 0.58F;
    const float ox = (w - quad_w) * 0.5F;
    const float oy = (h - quad_h) * 0.5F - 10.0F;

    const std::vector<sp::Point> grid = BuildWarpedGrid(ox, oy, quad_w, quad_h);

    painter.DrawImageMesh(mImage, mCols, mRows, grid);

    if (mWireframe) {
      DrawWireframe(painter, grid);
    }

    if (mFont) {
      painter.SetFont(mFont);
      painter.SetPen(sp::Pen(sp::Color{170, 182, 205, 230}, 1.0F));
      painter.DrawText(sp::Rect{0.0F, h - 38.0F, w, 22.0F},
                       "izgara " + std::to_string(mCols) + "x" +
                           std::to_string(mRows) +
                           "  —  1/2/3 cozunurluk, W tel kafes, SPACE durdur",
                       sp::Alignment::kCenter);
    }
  }

  void OnKeyDown(const sp::KeyEvent& event) override {
    if (event.repeat) {
      return;
    }
    switch (event.key) {
      case sp::Key::k1:
        SetGrid(4, 4);
        break;
      case sp::Key::k2:
        SetGrid(12, 12);
        break;
      case sp::Key::k3:
        SetGrid(32, 32);
        break;
      case sp::Key::kW:
        mWireframe = !mWireframe;
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
  void SetGrid(int32_t cols, int32_t rows) {
    mCols = cols;
    mRows = rows;
  }

  /// @brief Izgara köşelerini iki sinüs dalgasıyla deforme et.
  ///
  /// Sol kenar sabit tutulur (bayrak direği hissi); sağa gidildikçe genlik
  /// artar.
  [[nodiscard]] std::vector<sp::Point> BuildWarpedGrid(float ox, float oy,
                                                       float w, float h) const {
    std::vector<sp::Point> pts;
    pts.reserve(static_cast<std::size_t>(mCols + 1) *
                static_cast<std::size_t>(mRows + 1));

    for (int32_t r = 0; r <= mRows; ++r) {
      const float v = static_cast<float>(r) / static_cast<float>(mRows);
      for (int32_t c = 0; c <= mCols; ++c) {
        const float u = static_cast<float>(c) / static_cast<float>(mCols);

        // Genlik soldan saga buyur: direk sabit, ucu serbest.
        const float amp = 26.0F * u;
        const float wave =
            std::sin(u * 3.0F * kPi - mTime * 2.6F) * amp +
            std::sin((u + v) * 2.0F * kPi - mTime * 1.7F) * amp * 0.45F;

        // Yatayda hafif buzulme: bez gerildikce daralir.
        const float squeeze =
            std::cos(u * 2.0F * kPi - mTime * 2.2F) * 6.0F * u;

        pts.push_back({ox + u * w + squeeze, oy + v * h + wave});
      }
    }
    return pts;
  }

  void DrawWireframe(sp::Painter& painter, const std::vector<sp::Point>& grid) {
    const auto stride = static_cast<std::size_t>(mCols) + 1;
    sp::Pen pen(sp::Color{120, 255, 190, 150}, 1.0F);
    painter.SetPen(pen);

    for (int32_t r = 0; r <= mRows; ++r) {
      std::vector<sp::Point> row;
      row.reserve(stride);
      for (int32_t c = 0; c <= mCols; ++c) {
        row.push_back(grid[static_cast<std::size_t>(r) * stride + c]);
      }
      painter.DrawPolyline(row);
    }
    for (int32_t c = 0; c <= mCols; ++c) {
      std::vector<sp::Point> col;
      col.reserve(static_cast<std::size_t>(mRows) + 1);
      for (int32_t r = 0; r <= mRows; ++r) {
        col.push_back(grid[static_cast<std::size_t>(r) * stride + c]);
      }
      painter.DrawPolyline(col);
    }
  }

  sp::Image mImage;
  std::shared_ptr<sp::Font> mFont;
  float mTime{0.0F};
  int32_t mCols{12};
  int32_t mRows{12};
  bool mWireframe{false};
  bool mPaused{false};
};

int main() {
  sp::AppConfig config;
  config.title = "SDLPainter — mesh_warp: DrawImageMesh";
  config.width = 900;
  config.height = 640;
  config.stats_overlay = sp::StatsOverlayMode::kDetailed;

  MeshWarpDemo app(config);
  return app.Run();
}
