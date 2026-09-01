/// @brief pixel_art — doku filtresinin tek satırlık ama belirleyici farkı.
///
/// Aynı sprite sheet, aynı ölçek, tek fark `Image::SetFilter`. Solda
/// varsayılan `kLinear`, sağda `kNearest`. Piksel sanatı büyütüldüğünde
/// doğrusal filtre komşu pikselleri harmanlar ve keskin kenarları bulanık bir
/// bulamaca çevirir; en yakın komşu filtresi pikseli piksel olarak bırakır.
///
/// `sprite_animation` örneği bu filtre yokken yazılmıştı ve başlığında
/// "kenarlar yumuşak görünür, filtre seçimi P1.2'de" notu vardı. Bu örnek o
/// notun kapanışıdır.
///
/// Filtrenin doku yaratılırken uygulandığına dikkat: `SetFilter` ilk
/// çizimden önce çağrılmalıdır, sonrası önbelleklenmiş dokuyu etkilemez.
/// Bu yüzden iki ayrı `Image` nesnesi var — aynı dosya, iki farklı doku.
///
/// Kontroller:
///   ↑ / ↓ — büyütme oranı (fark oranla büyür)
///   SPACE — ayırıcıyı süpür / durdur
///   ESC   — çıkış

#include "sdl_painter/app/application.h"
#include "sdl_painter/font.h"
#include "sdl_painter/image.h"

#include <SDL3/SDL.h>

#include <cmath>
#include <memory>
#include <string>

#include "example_font.h"

namespace sp = sdl_painter;

namespace {

constexpr float kMinZoom = 2.0F;
constexpr float kMaxZoom = 16.0F;

std::string AssetPath(const char* name) {
  const char* base = SDL_GetBasePath();
  return (base != nullptr ? std::string(base) : std::string()) + "assets/" +
         name;
}

}  // namespace

class PixelArtDemo : public sp::Application {
 public:
  using Application::Application;

 protected:
  bool OnInit() override {
    const std::string path = AssetPath("rpg_character_walk.png");

    // Ayni dosya, IKI ayri Image: filtre doku yaratilirken uygulandigi icin
    // tek nesneyle iki filtreyi ayni karede gosteremeyiz.
    mLinear = sp::Image(path);
    mNearest = sp::Image(path);
    if (!mLinear.IsValid() || !mNearest.IsValid()) {
      SDL_Log("pixel_art: sprite sheet yuklenemedi: %s", path.c_str());
      return false;
    }
    mNearest.SetFilter(sp::TextureFilter::kNearest);

    const std::string font = example::FindSystemFont();
    if (!font.empty()) {
      mFont = std::make_shared<sp::Font>(font, 16);
      if (!mFont->IsValid()) {
        mFont.reset();
      }
    }
    return true;
  }

  void OnUpdate(float dt) override {
    if (mSweeping) {
      mSplit += dt * 0.35F * mSweepDir;
      if (mSplit > 0.92F) {
        mSplit = 0.92F;
        mSweepDir = -1.0F;
      } else if (mSplit < 0.08F) {
        mSplit = 0.08F;
        mSweepDir = 1.0F;
      }
    }
    mFrameTime += dt;
    if (mFrameTime >= 0.12F) {
      mFrameTime -= 0.12F;
      mFrame = (mFrame + 1) % 8;
    }
  }

  void OnRender(sp::Painter& painter) override {
    painter.Clear(sp::Color{24, 26, 36, 255});

    const auto w = static_cast<float>(Width());
    const auto h = static_cast<float>(Height());
    const float split_x = w * mSplit;

    // Karakteri buyuk olcekte, ekranin ortasinda ciz. Ayni hedef dikdortgen
    // iki kez cizilir; kirpma hangi yarinin hangi dokudan geldigini belirler.
    const float dw = 24.0F * mZoom;
    const float dh = 32.0F * mZoom;
    const sp::Rect dst{(w - dw) * 0.5F, (h - dh) * 0.5F + 20.0F, dw, dh};
    const sp::Rect src{static_cast<float>(mFrame * 24), 0.0F, 24.0F, 32.0F};

    painter.SetClipRect(sp::Rect{0.0F, 0.0F, split_x, h});
    painter.DrawImage(mLinear, src, dst);
    painter.ClearClip();

    painter.SetClipRect(sp::Rect{split_x, 0.0F, w - split_x, h});
    painter.DrawImage(mNearest, src, dst);
    painter.ClearClip();

    // Ayirici.
    sp::Pen divider(sp::Color{255, 255, 255, 200}, 2.0F);
    divider.SetDashPattern({10.0F, 7.0F});
    painter.SetPen(divider);
    painter.DrawLine(split_x, 0.0F, split_x, h);

    if (mFont) {
      painter.SetFont(mFont);
      painter.SetPen(sp::Pen(sp::Color{225, 232, 245, 235}, 1.0F));
      painter.DrawText(sp::Rect{0.0F, 24.0F, split_x, 22.0F},
                       "kLinear (varsayilan)", sp::Alignment::kCenter);
      painter.DrawText(sp::Rect{split_x, 24.0F, w - split_x, 22.0F}, "kNearest",
                       sp::Alignment::kCenter);
      painter.SetPen(sp::Pen(sp::Color{150, 162, 185, 220}, 1.0F));
      painter.DrawText(sp::Rect{0.0F, h - 40.0F, w, 22.0F},
                       "olcek x" + std::to_string(static_cast<int32_t>(mZoom)) +
                           "  —  yukari/asagi ile degistir, SPACE ile ayirici",
                       sp::Alignment::kCenter);
    }
  }

  void OnKeyDown(const sp::KeyEvent& event) override {
    switch (event.key) {
      case sp::Key::kUp:
        mZoom = std::fmin(kMaxZoom, mZoom + 1.0F);
        break;
      case sp::Key::kDown:
        mZoom = std::fmax(kMinZoom, mZoom - 1.0F);
        break;
      case sp::Key::kSpace:
        if (!event.repeat) {
          mSweeping = !mSweeping;
        }
        break;
      case sp::Key::kEscape:
        Quit();
        break;
      default:
        break;
    }
  }

  void OnMouseMove(const sp::MouseMoveEvent& event) override {
    if (!mSweeping && Width() > 0) {
      mSplit = event.x / static_cast<float>(Width());
    }
  }

 private:
  sp::Image mLinear;
  sp::Image mNearest;
  std::shared_ptr<sp::Font> mFont;
  float mZoom{8.0F};
  float mSplit{0.5F};
  float mSweepDir{1.0F};
  float mFrameTime{0.0F};
  int32_t mFrame{0};
  bool mSweeping{true};
};

int main() {
  sp::AppConfig config;
  config.title = "SDLPainter — pixel_art: kLinear ve kNearest yan yana";
  config.width = 900;
  config.height = 640;
  config.stats_overlay = sp::StatsOverlayMode::kDetailed;

  PixelArtDemo app(config);
  return app.Run();
}
