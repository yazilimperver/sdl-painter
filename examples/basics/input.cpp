/// @brief input — klavye ve fare girdisinin iki farklı okunuşu.
///
/// Gösterilen ayrım — bu örneğin varlık sebebi:
///
///   * Olay (event): tuşa basıldığı an bir kez gelir. Menü seçimi,
///     zıplama, ateş etme gibi tek atımlık eylemler içindir.
///   * Durum (state): tuş o karede basılı mı. Sürekli hareket içindir.
///
/// Sürekli hareketi olayla yazmak, işletim sisteminin tuş tekrar gecikmesine
/// takılır: karakter bir adım atar, kısa bir duraklama olur, sonra akmaya
/// başlar. `Application` ham olayları verir; durumu uygulamanın kendisi
/// `OnKeyDown`/`OnKeyUp` ile tutar — bu örnekteki `KeyState` sınıfı tam olarak
/// bunu yapar.
///
/// Kontroller:
///   WASD / ok tuşları — kareyi sürekli hareket ettirir (DURUM)
///   SPACE             — daireyi bir kademe büyütür (OLAY, tek atım)
///   Fare              — nişangâh fareyi izler, tıklama iz bırakır
///   R                 — sıfırla
///   ESC               — çıkış

#include "sdl_painter/app/application.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace sp = sdl_painter;

namespace {

constexpr float kMoveSpeed = 320.0F;  ///< piksel/saniye
constexpr float kBoxSize = 60.0F;
constexpr std::size_t kMaxTrailPoints = 32;

/// @brief Basılı tutulan tuşların kare-kare durumu.
///
/// `Application` yalnızca olay verir; "şu an basılı mı?" bilgisini tutmak
/// uygulamanın işidir. Küçük bir sabit dizi, tuş başına bir bayrak.
class KeyState {
 public:
  void Press(sp::Key key) { Set(key, true); }
  void Release(sp::Key key) { Set(key, false); }

  [[nodiscard]] bool IsDown(sp::Key key) const {
    const auto index = static_cast<std::size_t>(key);
    return index < mDown.size() && mDown[index];
  }

  void Clear() { mDown.fill(false); }

 private:
  void Set(sp::Key key, bool down) {
    const auto index = static_cast<std::size_t>(key);
    if (index < mDown.size()) {
      mDown[index] = down;
    }
  }

  // Key enum'unun tamamını kapsayacak kadar büyük; enum sıralı ve küçük.
  std::array<bool, 128> mDown{};
};

}  // namespace

class InputDemo : public sp::Application {
 public:
  using Application::Application;

 protected:
  bool OnInit() override {
    Reset();
    return true;
  }

  void OnUpdate(float dt) override {
    // --- DURUM okuma: sürekli hareket ---
    float dx = 0.0F;
    float dy = 0.0F;
    if (mKeys.IsDown(sp::Key::kA) || mKeys.IsDown(sp::Key::kLeft)) {
      dx -= 1.0F;
    }
    if (mKeys.IsDown(sp::Key::kD) || mKeys.IsDown(sp::Key::kRight)) {
      dx += 1.0F;
    }
    if (mKeys.IsDown(sp::Key::kW) || mKeys.IsDown(sp::Key::kUp)) {
      dy -= 1.0F;
    }
    if (mKeys.IsDown(sp::Key::kS) || mKeys.IsDown(sp::Key::kDown)) {
      dy += 1.0F;
    }

    mBoxX += dx * kMoveSpeed * dt;
    mBoxY += dy * kMoveSpeed * dt;

    // Ekran içinde tut.
    const auto w = static_cast<float>(Width());
    const auto h = static_cast<float>(Height());
    mBoxX = std::max(0.0F, std::min(mBoxX, w - kBoxSize));
    mBoxY = std::max(0.0F, std::min(mBoxY, h - kBoxSize));
  }

  void OnRender(sp::Painter& painter) override {
    painter.Clear(sp::Color{22, 24, 32, 255});

    // Fare izinin noktaları — eskiler soluklaşır.
    for (std::size_t i = 0; i < mTrail.size(); ++i) {
      const float t =
          static_cast<float>(i + 1) / static_cast<float>(mTrail.size());
      const auto alpha = static_cast<uint8_t>(40.0F + 160.0F * t);
      painter.SetBrush(sp::Brush(sp::Color{255, 160, 90, alpha}));
      painter.FillCircle(mTrail[i].x, mTrail[i].y, 4.0F + 6.0F * t);
    }

    // DURUM ile hareket eden kare.
    painter.SetBrush(sp::Brush(sp::Color{90, 170, 255, 220}));
    painter.SetPen(sp::Pen(sp::Color{190, 220, 255, 255}, 2.0F));
    painter.FillRect(mBoxX, mBoxY, kBoxSize, kBoxSize);
    painter.DrawRect(mBoxX, mBoxY, kBoxSize, kBoxSize);

    // OLAY ile büyüyen daire — SPACE her basışta bir kademe.
    const float radius = 20.0F + 6.0F * static_cast<float>(mSpacePresses);
    painter.SetBrush(sp::Brush(sp::Color{140, 230, 150, 200}));
    painter.FillCircle(static_cast<float>(Width()) * 0.5F,
                       static_cast<float>(Height()) * 0.5F, radius);

    // Fare nişangâhı: kesikli çizgiler, kalemin dash desenini kullanır.
    sp::Pen crosshair(sp::Color{255, 255, 255, 120}, 1.0F);
    crosshair.SetDashPattern({6.0F, 6.0F});
    painter.SetPen(crosshair);
    painter.DrawLine(0.0F, mMouseY, static_cast<float>(Width()), mMouseY);
    painter.DrawLine(mMouseX, 0.0F, mMouseX, static_cast<float>(Height()));
  }

  void OnKeyDown(const sp::KeyEvent& event) override {
    mKeys.Press(event.key);

    if (event.repeat) {
      return;
    }
    switch (event.key) {
      case sp::Key::kSpace:
        ++mSpacePresses;
        break;
      case sp::Key::kR:
        Reset();
        break;
      case sp::Key::kEscape:
        Quit();
        break;
      default:
        break;
    }
  }

  void OnKeyUp(const sp::KeyEvent& event) override { mKeys.Release(event.key); }

  void OnMouseMove(const sp::MouseMoveEvent& event) override {
    mMouseX = event.x;
    mMouseY = event.y;
  }

  void OnMouseButtonDown(const sp::MouseButtonEvent& event) override {
    mTrail.push_back({event.x, event.y});
    if (mTrail.size() > kMaxTrailPoints) {
      mTrail.erase(mTrail.begin());
    }
  }

 private:
  void Reset() {
    mBoxX = 60.0F;
    mBoxY = 60.0F;
    mSpacePresses = 0;
    mTrail.clear();
    mKeys.Clear();
  }

  KeyState mKeys;
  std::vector<sp::Point> mTrail;
  float mBoxX{60.0F};
  float mBoxY{60.0F};
  float mMouseX{0.0F};
  float mMouseY{0.0F};
  int32_t mSpacePresses{0};
};

int main() {
  sp::AppConfig config;
  config.title = "SDLPainter — input: olay ve durum farkı";
  config.width = 900;
  config.height = 640;

  InputDemo app(config);
  return app.Run();
}
