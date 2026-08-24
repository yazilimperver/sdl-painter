/// @brief particles — on binlerce parçacık, tek batch.
///
/// Burada 20.000 parçacık **iki** draw call'a iniyor ve sayaç ekranda
/// canlı duruyor. `examples/benchmarks/` aynı şeyi sayıyla raporlar; bu ise
/// gözle görülür hâli.
///
/// Kritik ayrım — SPACE ile canlı karşılaştırılabilir:
///
///   * **Batch dostu:** her parçacık aynı çizim durumuyla çizilir → hepsi tek
///     tampona birikir, tek draw call.
///   * **Batch kırıcı:** her parçacıktan önce `SetOpacity` çağrılır. Opaklık
///     bir shader uniform'u olduğu için batcher **her değişimde flush etmek
///     zorundadır** → parçacık başına bir draw call.
///
/// Renk bu farkı yaratmaz: renk vertex'te taşınır, bu yüzden her parçacık
/// farklı renkte olsa bile batch bozulmaz. Uygulamada ikisini de görürsün.
///
/// Kontroller:
///   SPACE — batch dostu / batch kırıcı çizim deseni
///   1/2/3 — 5.000 / 20.000 / 50.000 parçacık
///   R     — yeniden başlat
///   F1    — kare istatistiği katmanı
///   ESC   — çıkış

#include "sdl_painter/app/application.h"

#include <cmath>
#include <cstdint>
#include <random>
#include <vector>

namespace sp = sdl_painter;

namespace {

constexpr float kGravity = 260.0F;
constexpr float kParticleRadius = 2.5F;

struct Particle {
  float x{0.0F};
  float y{0.0F};
  float vx{0.0F};
  float vy{0.0F};
  float life{1.0F};       ///< 1 → yeni doğdu, 0 → öldü.
  float life_span{1.0F};  ///< Toplam ömür (saniye).
  sp::Color color;
};

}  // namespace

class ParticlesDemo : public sp::Application {
 public:
  using Application::Application;

 protected:
  bool OnInit() override {
    mParticles.resize(20000);
    RespawnAll();
    return true;
  }

  void OnUpdate(float dt) override {
    for (auto& p : mParticles) {
      p.vy += kGravity * dt;
      p.x += p.vx * dt;
      p.y += p.vy * dt;
      p.life -= dt / p.life_span;

      const bool out_of_view = p.y > static_cast<float>(Height()) + 20.0F ||
                               p.x < -20.0F ||
                               p.x > static_cast<float>(Width()) + 20.0F;
      if (p.life <= 0.0F || out_of_view) {
        Respawn(p);
      }
    }
  }

  void OnRender(sp::Painter& painter) override {
    painter.Clear(sp::Color{12, 12, 20, 255});
    painter.SetPen(sp::Pen::NoPen());

    if (mBatchFriendly) {
      // Tek çizim durumu: renk vertex'te taşındığı için parçacık başına
      // farklı renk vermek batch'i KIRMAZ.
      for (const auto& p : mParticles) {
        painter.SetBrush(sp::Brush(FadeColor(p)));
        painter.FillCircle(p.x, p.y, kParticleRadius);
      }
    } else {
      // Aynı görsel sonuç, ama opaklık uniform'u parçacık başına değişiyor.
      // Batcher her değişimde flush etmek zorunda → draw call sayısı patlar.
      for (const auto& p : mParticles) {
        painter.SetOpacity(Clamp01(p.life));
        painter.SetBrush(sp::Brush(p.color));
        painter.FillCircle(p.x, p.y, kParticleRadius);
      }
      painter.SetOpacity(1.0F);
    }
  }

  void OnKeyDown(const sp::KeyEvent& event) override {
    if (event.repeat) {
      return;
    }
    switch (event.key) {
      case sp::Key::kSpace:
        mBatchFriendly = !mBatchFriendly;
        break;
      case sp::Key::kR:
        RespawnAll();
        break;
      case sp::Key::kEscape:
        Quit();
        break;
      case sp::Key::k1:
        Resize(5000);
        break;
      case sp::Key::k2:
        Resize(20000);
        break;
      case sp::Key::k3:
        Resize(50000);
        break;
      default:
        break;
    }
  }

 private:
  static float Clamp01(float v) {
    return v < 0.0F ? 0.0F : (v > 1.0F ? 1.0F : v);
  }

  /// @brief Ömre göre solmuş renk — alfa vertex'te taşınır, batch'i kırmaz.
  static sp::Color FadeColor(const Particle& p) {
    sp::Color c = p.color;
    c.a = static_cast<uint8_t>(255.0F * Clamp01(p.life));
    return c;
  }

  void Resize(std::size_t count) {
    if (count == mParticles.size()) {
      return;
    }
    mParticles.resize(count);
    RespawnAll();
  }

  void RespawnAll() {
    for (auto& p : mParticles) {
      Respawn(p);
      // İlk karede hepsi aynı anda fışkırmasın: ömürleri dağıt.
      p.life = mUnit(mRng);
    }
  }

  void Respawn(Particle& p) {
    const auto w = static_cast<float>(Width());
    const auto h = static_cast<float>(Height());

    p.x = w * 0.5F;
    p.y = h * 0.75F;

    const float angle = -3.14159265F * (0.15F + 0.7F * mUnit(mRng));
    const float speed = 120.0F + 260.0F * mUnit(mRng);
    p.vx = std::cos(angle) * speed;
    p.vy = std::sin(angle) * speed;

    p.life_span = 1.2F + 2.0F * mUnit(mRng);
    p.life = 1.0F;

    // Sıcak renk paleti: sarıdan kırmızıya.
    const float t = mUnit(mRng);
    p.color = sp::Color{255, static_cast<uint8_t>(90.0F + 150.0F * t),
                        static_cast<uint8_t>(40.0F + 60.0F * t), 255};
  }

  std::vector<Particle> mParticles;
  std::mt19937 mRng{1234};
  std::uniform_real_distribution<float> mUnit{0.0F, 1.0F};
  bool mBatchFriendly{true};
};

int main() {
  sp::AppConfig config;
  config.title = "SDLPainter — particles: SPACE ile batch farkini gor";
  config.width = 1000;
  config.height = 700;
  config.vsync = false;  // Ölçüm görünür olsun; vsync farkı gizler.
  config.stats_overlay = sp::StatsOverlayMode::kDetailed;

  ParticlesDemo app(config);
  return app.Run();
}
