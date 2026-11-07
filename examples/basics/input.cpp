/// @brief input — klavye, fare ve gamepad girdisinin iki farklı okunuşu.
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
/// Gamepad de aynı kuralla çalışır: düğmeler ve analog eksenler olay olarak
/// gelir, "çubuk şu an nerede?" bilgisini `PadState` tutar. Analog çubuk
/// bırakıldığında tam sıfıra dönmez; bu küçük sapma kareyi kendi kendine
/// kaydırmasın diye dairesel bir ölü bölge (dead zone) uygulanır. Sağ alttaki
/// gösterge çubuğun ham konumunu ve ölü bölgeyi çizer; rengi gamepad ailesini
/// (Xbox, PlayStation, Nintendo) gösterir.
///
/// Kontroller:
///   WASD / ok tuşları — kareyi sürekli hareket ettirir (DURUM)
///   SPACE             — daireyi bir kademe büyütür (OLAY, tek atım)
///   Fare              — nişangâh fareyi izler, tıklama iz bırakır
///   R                 — sıfırla
///   ESC               — çıkış
///
///   Gamepad (ilk bağlanan kareyi sürer):
///   Sol çubuk / d-pad — kareyi hareket ettirir (DURUM; çubuk analog)
///   South (A / ✕)     — daireyi büyütür, kısa titreşir (OLAY)
///   Start             — sıfırla

#include "sdl_painter/app/application.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <vector>

namespace sp = sdl_painter;

namespace {

constexpr float kMoveSpeed = 320.0F;  ///< piksel/saniye
constexpr float kBoxSize = 60.0F;
constexpr std::size_t kMaxTrailPoints = 32;
constexpr float kDeadZone = 0.2F;  ///< Çubuk ölü bölgesinin yarıçapı, [0, 1].
constexpr float kGaugeRadius = 48.0F;  ///< Sağ alttaki çubuk göstergesi.

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

/// @brief Etkin gamepad'in kare-kare durumu.
///
/// `KeyState`'in gamepad karşılığı. Eksen olayı yalnızca değer değişince
/// gelir; son değeri burada tutarız.
struct PadState {
  float stick_x{0.0F};  ///< Sol çubuk, [-1, 1], sağ pozitif.
  float stick_y{0.0F};  ///< Sol çubuk, [-1, 1], aşağı pozitif (ekran gibi).
  bool dpad_up{false};
  bool dpad_down{false};
  bool dpad_left{false};
  bool dpad_right{false};
};

/// @brief Dairesel ölü bölge.
///
/// Çubuk merkeze @ref kDeadZone'dan yakınsa sıfır döner. Dışında yön korunur,
/// büyüklük [kDeadZone, 1] aralığından [0, 1]'e yeniden ölçeklenir; hareket
/// ölü bölgenin kenarında sıçramadan başlar. Eksenleri ayrı ayrı kırpmak
/// (kare ölü bölge) çaprazda köşeli bir his verir.
sp::Point ApplyRadialDeadZone(float x, float y) {
  const float length = std::sqrt(x * x + y * y);
  if (length < kDeadZone) {
    return {0.0F, 0.0F};
  }
  const float scaled =
      std::min(1.0F, (length - kDeadZone) / (1.0F - kDeadZone));
  return {x / length * scaled, y / length * scaled};
}

/// @brief Gamepad ailesinin adı ve gösterge rengi.
///
/// Gerçek bir oyunda ekrandaki tuş ipuçları (A mı ✕ mi?) buradan seçilir.
struct PadStyle {
  const char* name;
  sp::Color color;
};

PadStyle StyleFor(sp::GamepadType type) {
  switch (type) {
    case sp::GamepadType::kXbox:
      return {"Xbox", sp::Color{90, 200, 90, 255}};
    case sp::GamepadType::kPlayStation:
      return {"PlayStation", sp::Color{90, 150, 255, 255}};
    case sp::GamepadType::kNintendo:
      return {"Nintendo", sp::Color{255, 90, 90, 255}};
    case sp::GamepadType::kStandard:
      return {"standart", sp::Color{200, 200, 200, 255}};
    default:
      return {"bilinmeyen", sp::Color{200, 200, 200, 255}};
  }
}

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
    // D-pad, ok tuşlarıyla aynı: dijital, basılı ya da değil.
    float dx = 0.0F;
    float dy = 0.0F;
    if (mKeys.IsDown(sp::Key::kA) || mKeys.IsDown(sp::Key::kLeft) ||
        mPad.dpad_left) {
      dx -= 1.0F;
    }
    if (mKeys.IsDown(sp::Key::kD) || mKeys.IsDown(sp::Key::kRight) ||
        mPad.dpad_right) {
      dx += 1.0F;
    }
    if (mKeys.IsDown(sp::Key::kW) || mKeys.IsDown(sp::Key::kUp) ||
        mPad.dpad_up) {
      dy -= 1.0F;
    }
    if (mKeys.IsDown(sp::Key::kS) || mKeys.IsDown(sp::Key::kDown) ||
        mPad.dpad_down) {
      dy += 1.0F;
    }

    // Analog çubuk: ölü bölgeden geçip tuşlarla toplanır, eksen başına kırpılır.
    const sp::Point stick = ApplyRadialDeadZone(mPad.stick_x, mPad.stick_y);
    dx = std::clamp(dx + stick.x, -1.0F, 1.0F);
    dy = std::clamp(dy + stick.y, -1.0F, 1.0F);

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

    if (!mPads.empty()) {
      DrawStickGauge(painter);
    }

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

  // --- Gamepad: AppConfig::enable_gamepad açıkken gelir ---

  void OnGamepadAdded(const sp::GamepadDeviceEvent& event) override {
    mPads.push_back(event);
    std::printf("Gamepad baglandi: kimlik %u, aile %s\n",
                static_cast<unsigned>(event.id), StyleFor(event.type).name);
  }

  void OnGamepadRemoved(const sp::GamepadDeviceEvent& event) override {
    const bool was_active = IsActivePad(event.id);
    mPads.erase(std::remove_if(mPads.begin(), mPads.end(),
                               [&event](const sp::GamepadDeviceEvent& pad) {
                                 return pad.id == event.id;
                               }),
                mPads.end());
    // Kareyi süren gamepad gitti: basılı kalmış durum kareyi sürüklemesin.
    if (was_active) {
      mPad = PadState{};
    }
    std::printf("Gamepad cikarildi: kimlik %u\n",
                static_cast<unsigned>(event.id));
  }

  void OnGamepadButtonDown(const sp::GamepadButtonEvent& event) override {
    if (!IsActivePad(event.id)) {
      return;
    }
    SetDpad(event.button, true);
    switch (event.button) {
      case sp::GamepadButton::kSouth:
        ++mSpacePresses;
        // Desteklemeyen gamepad'de false döner; örnek için önemsiz.
        RumbleGamepad(event.id, 0.3F, 0.6F, 90);
        break;
      case sp::GamepadButton::kStart:
        Reset();
        break;
      default:
        break;
    }
  }

  void OnGamepadButtonUp(const sp::GamepadButtonEvent& event) override {
    if (IsActivePad(event.id)) {
      SetDpad(event.button, false);
    }
  }

  void OnGamepadAxis(const sp::GamepadAxisEvent& event) override {
    if (!IsActivePad(event.id)) {
      return;
    }
    if (event.axis == sp::GamepadAxis::kLeftX) {
      mPad.stick_x = event.value;
    } else if (event.axis == sp::GamepadAxis::kLeftY) {
      mPad.stick_y = event.value;
    }
  }

 private:
  void Reset() {
    mBoxX = 60.0F;
    mBoxY = 60.0F;
    mSpacePresses = 0;
    mTrail.clear();
    mKeys.Clear();
    // mPad temizlenmez: eksen olayı yalnızca değer değişince gelir; basılı
    // tutulan çubuk sıfırlansaydı bir sonraki kıpırtıya kadar ölü kalırdı.
  }

  /// @brief Yalnızca ilk bağlanan gamepad kareyi sürer.
  [[nodiscard]] bool IsActivePad(sp::GamepadId id) const {
    return !mPads.empty() && mPads.front().id == id;
  }

  void SetDpad(sp::GamepadButton button, bool down) {
    switch (button) {
      case sp::GamepadButton::kDpadUp:
        mPad.dpad_up = down;
        break;
      case sp::GamepadButton::kDpadDown:
        mPad.dpad_down = down;
        break;
      case sp::GamepadButton::kDpadLeft:
        mPad.dpad_left = down;
        break;
      case sp::GamepadButton::kDpadRight:
        mPad.dpad_right = down;
        break;
      default:
        break;
    }
  }

  /// @brief Sağ alt köşede çubuğun ham konumu ve ölü bölge.
  void DrawStickGauge(sp::Painter& painter) const {
    const float cx = static_cast<float>(Width()) - kGaugeRadius - 24.0F;
    const float cy = static_cast<float>(Height()) - kGaugeRadius - 24.0F;
    const sp::Color ring = StyleFor(mPads.front().type).color;

    // Ölü bölge: içindeki her sapma sıfır sayılır.
    painter.SetBrush(sp::Brush(sp::Color{255, 255, 255, 30}));
    painter.FillCircle(cx, cy, kGaugeRadius * kDeadZone);
    // Çubuğun tam sapması.
    painter.SetPen(sp::Pen(ring, 2.0F));
    painter.DrawCircle(cx, cy, kGaugeRadius);

    // Ham konum; ölü bölgedeyken soluk.
    const bool in_dead_zone =
        std::hypot(mPad.stick_x, mPad.stick_y) < kDeadZone;
    painter.SetBrush(
        sp::Brush(in_dead_zone ? sp::Color{255, 255, 255, 90} : ring));
    painter.FillCircle(cx + mPad.stick_x * kGaugeRadius,
                       cy + mPad.stick_y * kGaugeRadius, 6.0F);
  }

  KeyState mKeys;
  PadState mPad;
  std::vector<sp::GamepadDeviceEvent> mPads;  ///< Bağlı gamepad'ler, sırayla.
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
  config.enable_gamepad = true;

  InputDemo app(config);
  return app.Run();
}
