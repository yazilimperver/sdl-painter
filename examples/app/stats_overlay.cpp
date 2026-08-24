/// @brief stats_overlay — ekran üstü FPS / kare istatistiği göstergesi.
///
/// Gösterilen özellikler:
///   - `AppConfig::stats_overlay` — gösterge başlangıç modu
///   - `AppConfig::show_fps_in_title` — FPS'i pencere başlığında göster
///   - F1 ile mod döngüsü: kapalı → FPS → detaylı → kapalı
///   - `Application::GetFrameStats()` — draw call / batch / vertex / durum
///
/// Sahne bilinçli olarak "batch'e kötü davranan" ve "iyi davranan" iki mod
/// arasında geçiş yapabilir (SPACE): aradaki draw call farkı göstergede
/// doğrudan okunur. Ölçümün tamamı `examples/benchmarks/README.md`'de.
///
/// SPACE → çizim desenini değiştir, F1 → gösterge modu, ESC → çıkış.
///
/// `--screenshot=DOSYA` verilirse kısa bir süre sonra ekran görüntüsü alıp
/// çıkar; `--pattern=opacity` batch'i kıran desenle başlatır (doküman
/// görselleri bu şekilde üretilir).

#include <SDL3/SDL.h>
#include <spdlog/spdlog.h>

#include <cmath>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include "benchmarks/screenshot.h"
#include "sdl_painter/app/application.h"
#include "sdl_painter/brush.h"
#include "sdl_painter/color.h"
#include "sdl_painter/painter.h"
#include "sdl_painter/pen.h"

namespace {

constexpr int32_t kShapeCount = 900;
/// Ekran goruntusu icin beklenen sure: FPS ortalamasinin oturmasi ve
/// GPU timer sorgusunun ilk sonucunu vermesi icin yeterli olmali.
constexpr float kScreenshotAfterSeconds = 1.5F;

/// @brief Tekrarlanabilir yerleşim için basit LCG.
class Lcg {
 public:
  explicit Lcg(uint32_t seed) : mState(seed) {}
  uint32_t Next() noexcept {
    mState = (mState * 1664525U) + 1013904223U;
    return mState;
  }
  float Unit() noexcept {
    return static_cast<float>(Next() >> 8U) / 16777215.0F;
  }

 private:
  uint32_t mState;
};

struct Shape {
  float x{0.0F};
  float y{0.0F};
  float size{0.0F};
  float phase{0.0F};
  sdl_painter::Color color;
};

class StatsOverlayDemo : public sdl_painter::Application {
 public:
  StatsOverlayDemo(sdl_painter::AppConfig config, std::string screenshot_path,
                   bool batch_friendly)
      : Application(std::move(config)),
        mScreenshotPath(std::move(screenshot_path)),
        mBatchFriendly(batch_friendly) {}

 protected:
  bool OnInit() override {
    Lcg rng(2024U);
    mShapes.reserve(kShapeCount);
    for (int32_t i = 0; i < kShapeCount; ++i) {
      Shape s;
      s.size = 10.0F + (rng.Unit() * 18.0F);
      s.x = rng.Unit() * static_cast<float>(Width());
      s.y = rng.Unit() * static_cast<float>(Height());
      s.phase = rng.Unit() * 6.28318F;
      s.color = sdl_painter::Color{
          static_cast<uint8_t>(60 + static_cast<int32_t>(rng.Unit() * 195.0F)),
          static_cast<uint8_t>(60 + static_cast<int32_t>(rng.Unit() * 195.0F)),
          static_cast<uint8_t>(60 + static_cast<int32_t>(rng.Unit() * 195.0F)),
          255};
      mShapes.push_back(s);
    }
    return true;
  }

  void OnUpdate(float dt) override {
    mTime += dt;
    if (!mScreenshotPath.empty() && !mCaptured &&
        mTime >= kScreenshotAfterSeconds) {
      mCaptured = true;
      // Ön tampon okunur: Application, sunumdan önceye girecek bir kanca
      // sunmuyor. Bu yüzden bir önceki karenin sunulmuş hâli alınır.
      if (bench::SaveBackBufferPng(mScreenshotPath, Width(), Height(), true)) {
        spdlog::info("Ekran goruntusu: {}", mScreenshotPath);
      }
      Quit();
    }
  }

  void OnRender(sdl_painter::Painter& p) override {
    p.Clear(sdl_painter::Color{18, 20, 26, 255});

    for (const auto& s : mShapes) {
      const float wobble = std::sin(mTime + s.phase) * 6.0F;
      p.SetBrush(sdl_painter::Brush(s.color));
      if (mBatchFriendly) {
        // Batch dostu: transform yok, doğrudan mutlak koordinat.
        p.FillRect(s.x + wobble, s.y, s.size, s.size);
      } else {
        // Batch'i kıran desen: şekil başına opaklık değişimi.
        p.SetOpacity(0.55F + (0.45F * std::sin(mTime + s.phase)));
        p.FillRect(s.x + wobble, s.y, s.size, s.size);
      }
    }
    p.SetOpacity(1.0F);
  }

  void OnKeyDown(const sdl_painter::KeyEvent& e) override {
    if (e.key == sdl_painter::Key::kEscape) {
      Quit();
    } else if (e.key == sdl_painter::Key::kSpace) {
      mBatchFriendly = !mBatchFriendly;
      spdlog::info("Desen: {}",
                   mBatchFriendly ? "batch dostu" : "sekil basina opaklik");
    }
  }

 private:
  std::vector<Shape> mShapes;
  std::string mScreenshotPath;
  float mTime{0.0F};
  bool mCaptured{false};
  bool mBatchFriendly{true};  ///< ctor'da ezilir
};

}  // namespace

int main(int argc, char** argv) {
  std::string screenshot_path;
  bool batch_friendly = true;
  for (int i = 1; i < argc; ++i) {
    if (std::strncmp(argv[i], "--screenshot=", 13) == 0) {
      screenshot_path = argv[i] + 13;
    } else if (std::strcmp(argv[i], "--pattern=opacity") == 0) {
      // Batch'i kiran desenle basla (SPACE ile zaten degistirilebiliyor);
      // dokuman gorselini uretebilmek icin komut satirindan da secilebilir.
      batch_friendly = false;
    }
  }

  sdl_painter::AppConfig cfg;
  cfg.title = "SDLPainter — istatistik gostergesi";
  cfg.width = 1000;
  cfg.height = 640;
  // Gösterge açık başlasın; F1 ile döngülenir.
  cfg.stats_overlay = sdl_painter::StatsOverlayMode::kDetailed;
  cfg.show_fps_in_title = true;
  // Ölçüm anlamlı olsun diye vsync kapalı: aksi halde FPS ekran tazeleme
  // hızına sabitlenir ve desen değişiminin etkisi görünmez.
  cfg.vsync = false;

  StatsOverlayDemo app(std::move(cfg), std::move(screenshot_path),
                       batch_friendly);
  return app.Run();
}
