/// @brief game_loop — Sabit adımlı (fixed-timestep) oyun döngüsü.
///
/// Geliştirme Fazı 7 demosu (eski ad: phase7_game_demo).
///
/// TimingMode::kFixed ile deterministik simülasyon: fizik sabit adımla
/// (fixed_update_hz) güncellenir, render ise OnRender(Painter&, alpha) ile
/// önceki ve geçerli konum arasında interpolasyon yaparak akıcı çizilir.
/// Game Programming Patterns "play catch up" yaklaşımı.
///
/// Bir top yerçekimiyle düşer, zemine çarpınca seker. Düşük fixed_update_hz
/// (ör. 20) seçilse bile interpolasyon sayesinde hareket akıcı görünür.
///
/// SPACE: topu yeniden fırlat. ESC: çıkış.

#include "sdl_painter/app/application.h"

namespace sp = sdl_painter;

namespace {

constexpr float kGravity = 1400.0F;      // piksel/sn^2
constexpr float kFloorY = 620.0F;        // zemin yüksekliği
constexpr float kRadius = 30.0F;         // top yarıçapı
constexpr float kRestitution = 0.80F;    // sekme kaybı
constexpr float kLaunchSpeed = -900.0F;  // ilk yukarı hız

/// @brief İki değer arasında doğrusal interpolasyon.
constexpr float Lerp(float a, float b, float t) {
  return a + (b - a) * t;
}

}  // namespace

/// @brief Sabit adımlı fizik + interpolasyonlu render örneği.
class GameLoopDemo : public sp::Application {
 public:
  using Application::Application;

 protected:
  bool OnInit() override {
    Reset();
    return true;
  }

  // Sabit adımla (deterministik) çağrılır — dt her seferinde aynıdır.
  void OnUpdate(float dt) override {
    mPrevY = mY;  // interpolasyon için önceki durumu sakla
    mVelY += kGravity * dt;
    mY += mVelY * dt;

    if (mY > kFloorY - kRadius) {
      mY = kFloorY - kRadius;
      mVelY = -mVelY * kRestitution;
    }
  }

  // Her frame çağrılır; alpha = son sabit adımdan bu yana geçen oran [0,1].
  void OnRender(sp::Painter& painter, float alpha) override {
    painter.Clear(sp::Color{18, 22, 30, 255});

    // Zemin.
    painter.SetBrush(sp::Brush(sp::Color{60, 70, 90, 255}));
    painter.FillRect(0.0F, kFloorY, static_cast<float>(Width()),
                     static_cast<float>(Height()) - kFloorY);

    // Önceki ve geçerli konum arasında interpolasyonla akıcı çizim.
    const float draw_y = Lerp(mPrevY, mY, alpha);
    painter.SetBrush(sp::Brush(sp::Color{255, 180, 60, 255}));
    painter.SetPen(sp::Pen(sp::Color{255, 230, 150, 255}, 2.0F));
    painter.FillCircle(450.0F, draw_y, kRadius);
    painter.DrawCircle(450.0F, draw_y, kRadius);
  }

  void OnKeyDown(const sp::KeyEvent& event) override {
    if (event.key == sp::Key::kEscape) {
      Quit();
    } else if (event.key == sp::Key::kSpace) {
      Reset();
    }
  }

 private:
  void Reset() {
    mY = 120.0F;
    mPrevY = mY;
    mVelY = kLaunchSpeed;
  }

  float mY{0.0F};
  float mPrevY{0.0F};
  float mVelY{0.0F};
};

int main() {
  sp::AppConfig config;
  config.title = "SDLPainter — game_loop: Fixed-Timestep Game Loop (Phase 7)";
  config.width = 900;
  config.height = 700;
  config.timing = sp::TimingMode::kFixed;
  config.fixed_update_hz =
      30;                 // düşük sim hızı; interpolasyon akıcılığı sağlar
  config.target_fps = 0;  // vsync sınırlasın

  GameLoopDemo app(config);
  return app.Run();
}
