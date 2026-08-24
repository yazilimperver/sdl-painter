/// @brief app_basics — Application çatısı.
///
/// Geliştirme Fazı 6 demosu (eski ad: phase6_app_demo).
///
/// Aynı görsel içeriğin (dönen dikdörtgen + nabız gibi ölçeklenen daire)
/// transforms örneğindeki ~250 satır SDL boilerplate'i olmadan nasıl yazıldığını
/// gösterir. SDL init, pencere, GL context, olay döngüsü ve yıkım
/// sdl_painter::Application tarafından yönetilir.
///
/// ESC veya pencere kapat çıkmak için.

#include "sdl_painter/app/application.h"

#include <cmath>

namespace sp = sdl_painter;

namespace {

constexpr float kPi = 3.14159265358979323846F;

}  // namespace

/// @brief Basit animasyonlu demo uygulaması.
class AppBasicsDemo : public sp::Application {
 public:
  using Application::Application;

 protected:
  void OnUpdate(float dt) override {
    mAngle += 90.0F * dt;  // saniyede 90 derece
    if (mAngle >= 360.0F) {
      mAngle -= 360.0F;
    }
  }

  void OnRender(sp::Painter& painter) override {
    painter.Clear(sp::Color{20, 20, 30, 255});

    // Dönen dikdörtgen.
    painter.Save();
    painter.Translate(300.0F, 350.0F);
    painter.Rotate(mAngle);
    painter.SetBrush(sp::Brush(sp::Color{255, 200, 50, 220}));
    painter.SetPen(sp::Pen(sp::Color{255, 240, 180, 255}, 2.0F));
    painter.FillRect(-70.0F, -45.0F, 140.0F, 90.0F);
    painter.DrawRect(-70.0F, -45.0F, 140.0F, 90.0F);
    painter.Restore();

    // Nabız gibi ölçeklenen daire.
    const float scale = 1.0F + 0.4F * std::sin(mAngle * kPi / 180.0F);
    painter.Save();
    painter.Translate(620.0F, 350.0F);
    painter.Scale(scale, scale);
    painter.SetBrush(sp::Brush(sp::Color{150, 80, 220, 220}));
    painter.SetPen(sp::Pen(sp::Color{200, 150, 255, 255}, 2.0F));
    painter.FillCircle(0.0F, 0.0F, 60.0F);
    painter.DrawCircle(0.0F, 0.0F, 60.0F);
    painter.Restore();
  }

  void OnKeyDown(const sp::KeyEvent& event) override {
    if (event.key == sp::Key::kEscape) {
      Quit();
    }
  }

 private:
  float mAngle{0.0F};
};

int main() {
  sp::AppConfig config;
  config.title = "SDLPainter — app_basics: Application Framework (Phase 6)";
  config.width = 900;
  config.height = 700;

  AppBasicsDemo app(config);
  return app.Run();
}
