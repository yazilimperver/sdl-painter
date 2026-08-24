/// @brief plasma — her karede CPU'da üretilip GPU'ya yüklenen doku.
///
/// Bu örneğin varlık sebebi: `IRenderer::UpdateTexture` (v1.2.0) kütüphanede
/// var ama hiçbir örnekte kullanılmıyordu. Glyph atlası onu içeriden
/// kullanıyor; dışarıdan nasıl kullanıldığını gösteren tek yer burası.
///
/// Gösterilen:
///   - `Image::CreateFromData` ile bir kez doku ayırmak
///   - Aynı dokuyu her karede `Painter::UpdateImage` ile yeniden doldurmak
///     (yeniden yaratmak DEĞİL — tahsis/serbest bırakma döngüsü olmaz)
///   - Piksel verisini CPU'da üretmek, doku ölçeklenerek ekrana çizilir
///
/// Kontroller:
///   SPACE — animasyonu duraklat / sürdür
///   1/2/3 — doku çözünürlüğü 64 / 128 / 256 (yükleme maliyetini görmek için)
///   F1    — kare istatistiği katmanı (CPU süresi burada okunur)
///   ESC   — çıkış

#include "sdl_painter/app/application.h"
#include "sdl_painter/image.h"

#include <cmath>
#include <cstdint>
#include <vector>

namespace sp = sdl_painter;

namespace {

constexpr int32_t kMinSize = 32;
constexpr int32_t kMaxSize = 512;

}  // namespace

class PlasmaDemo : public sp::Application {
 public:
  using Application::Application;

 protected:
  bool OnInit() override {
    Allocate(mSize);
    return true;
  }

  void OnUpdate(float dt) override {
    if (!mPaused) {
      mTime += dt;
    }
  }

  void OnRender(sp::Painter& painter) override {
    painter.Clear(sp::Color{10, 10, 16, 255});

    FillPixels();
    // Doku YENIDEN YARATILMAZ; var olan doku yerinde güncellenir.
    painter.UpdateImage(mImage, mPixels.data());

    // Ekranı kaplayacak şekilde ölçekle — doku çözünürlüğü ile ekran
    // çözünürlüğü birbirinden bağımsız.
    painter.DrawImage(mImage, sp::Rect{0.0F, 0.0F, static_cast<float>(Width()),
                                       static_cast<float>(Height())});
  }

  void OnKeyDown(const sp::KeyEvent& event) override {
    if (event.repeat) {
      return;
    }
    switch (event.key) {
      case sp::Key::kSpace:
        mPaused = !mPaused;
        break;
      case sp::Key::kEscape:
        Quit();
        break;
      default:
        break;
    }
    // Doku cozunurlugu: kucuk doku = ucuz CPU dongusu + ucuz yukleme,
    // buyuk doku = keskin gorunum. Farki F1 katmanindaki CPU suresinde gor.
    if (event.key == sp::Key::k1) {
      Resize(64);
    } else if (event.key == sp::Key::k2) {
      Resize(128);
    } else if (event.key == sp::Key::k3) {
      Resize(256);
    }
  }

 private:
  void Allocate(int32_t size) {
    mSize = size;
    mPixels.assign(static_cast<std::size_t>(size) * size * 4, 0);
    FillPixels();
    mImage = sp::Image::CreateFromData(mPixels.data(), size, size, 4);
  }

  void Resize(int32_t size) {
    if (size < kMinSize || size > kMaxSize || size == mSize) {
      return;
    }
    Allocate(size);
  }

  /// @brief Klasik plazma: birkaç sinüs dalgasının toplamı.
  void FillPixels() {
    const float t = mTime;
    for (int32_t y = 0; y < mSize; ++y) {
      const auto fy = static_cast<float>(y) * 0.06F;
      for (int32_t x = 0; x < mSize; ++x) {
        const auto fx = static_cast<float>(x) * 0.06F;

        float v = std::sin(fx + t);
        v += std::sin(fy + t * 0.8F);
        v += std::sin((fx + fy + t) * 0.5F);
        v += std::sin(std::sqrt(fx * fx + fy * fy) + t * 1.3F);
        v *= 0.25F;  // [-1, 1] aralığına indir

        const std::size_t idx = (static_cast<std::size_t>(y) * mSize + x) * 4;
        mPixels[idx + 0] = Channel(v, 0.0F);
        mPixels[idx + 1] = Channel(v, 2.094F);  // 2π/3
        mPixels[idx + 2] = Channel(v, 4.188F);  // 4π/3
        mPixels[idx + 3] = 255;
      }
    }
  }

  /// @brief [-1,1] değerini faz kaydırmalı bir renk kanalına çevir.
  static uint8_t Channel(float v, float phase) {
    const float c = 0.5F + 0.5F * std::sin(v * 3.14159265F + phase);
    return static_cast<uint8_t>(c * 255.0F);
  }

  sp::Image mImage;
  std::vector<uint8_t> mPixels;
  int32_t mSize{128};
  float mTime{0.0F};
  bool mPaused{false};
};

int main() {
  sp::AppConfig config;
  config.title = "SDLPainter — plasma: her karede doku güncelleme";
  config.width = 800;
  config.height = 600;
  config.stats_overlay = sp::StatsOverlayMode::kDetailed;

  PlasmaDemo app(config);
  return app.Run();
}
