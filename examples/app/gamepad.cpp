/// @brief gamepad - bağlı gamepad'in bütün girdilerini ve bilgilerini gösterir.
///
/// Kol SDLPainter ile çizilir: gövde kübik Bézier'li bir `Path`, gölgesi
/// gradient, tuşlar yuvarlatılmış dikdörtgen ve daire, etiketler metin.
/// Hiçbir görsel dosyası yüklenmez. Çizim sabit bir tasarım alanında
/// (1100×640) yapılır ve pencereye tek bir `Scale` ile sığdırılır.
///
/// Veriler iki katmandan gelir; dosya da bu ayrımla düzenlendi:
///
///   1. Çatının verdikleri (`AppConfig::enable_gamepad`): tak-çıkar, standart
///      15 düğme, 6 normalize eksen, gamepad ailesi, titreşim. Kolun çizimi
///      bunları kullanır; durum `PadState`'te olaylardan kurulur.
///   2. SDL'in doğrudan kullanımı: çatının kapsamadığı her şey. Çatı
///      gamepad'i zaten açtığı için `SDL_GetGamepadFromID` aynı tutamacı
///      verir. Ad, vendor/product, bağlantı, pil, ham eksen değerleri, yüz
///      düğmelerinin etiketi, ek düğmeler (Share, paddle, touchpad tuşu),
///      touchpad, jiroskop, ivmeölçer ve tetik titreşimi buradan okunur; ek
///      düğme olayları `OnRawEvent` ile alınır. SDL, sdl_painter'ın PUBLIC
///      bağımlılığı olduğu için `<SDL3/SDL.h>` doğrudan kullanılabilir.
///
/// Yüz düğmelerinin etiketi neden SDL'den? Çatının `GamepadType`'ı aileyi
/// verir, ama aile içinde de fark var: Switch'te güneydeki düğme B, GameCube'de
/// A. `SDL_GetGamepadButtonLabel` cihaza göre doğru etiketi verir.
/// PlayStation'ın ✕ ○ □ △ sembolleri çoğu sistem fontunda olmadığı için
/// çizilir.
///
/// Birden çok gamepad bağlıysa son kullanılan gösterilir.
///
/// Kontroller:
///   T   - titreşim testi: açıkken sol tetik ağır, sağ tetik hafif motoru
///         sürer; destekleniyorsa tetik motorlarını da
///   ESC - çıkış
///
/// `--screenshot=DOSYA` verilirse kısa süre sonra ekran görüntüsü alıp çıkar.

#include "sdl_painter/app/application.h"
#include "sdl_painter/font.h"
#include "sdl_painter/path.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <deque>
#include <initializer_list>
#include <memory>
#include <spdlog/spdlog.h>
#include <string>
#include <utility>
#include <vector>

#include "benchmarks/screenshot.h"
#include "example_font.h"

// windows.h (spdlog uzerinden dolayli gelebilir) DrawText'i DrawTextA/W
// makrosuna cevirir ve Painter::DrawText cagrisini bozar. painter.h bunu
// kendi icinde temizler, ancak windows.h ondan SONRA dahil edilirse makro
// geri doner - bu yuzden include sirasindan bagimsiz olarak burada da temizle.
#ifdef DrawText
#undef DrawText
#endif

namespace sp = sdl_painter;

namespace {

// --- Tasarım alanı ----------------------------------------------------------
// Her şey bu sabit koordinatlarda çizilir; OnRender pencereye ölçekler.

constexpr float kDesignW = 1100.0F;
constexpr float kDesignH = 640.0F;
constexpr float kAxisX = 350.0F;   ///< Kol çiziminin simetri ekseni.
constexpr float kPanelX = 730.0F;  ///< Sağdaki bilgi panelinin sol kenarı.
constexpr float kPanelW = 340.0F;
constexpr float kLineH = 20.0F;

constexpr float kWellRadius = 40.0F;  ///< Çubuk yuvası.
constexpr float kCapRadius = 24.0F;   ///< Çubuk başlığı.
constexpr float kCapTravel = 14.0F;   ///< Başlığın en büyük kayması.
constexpr float kDotRange = 34.0F;    ///< Ham konum noktasının erişimi.
/// Gösterilen ölü bölge. Yalnızca bilgi: değerler ham gösterilir.
constexpr float kDeadZone = 0.2F;

constexpr std::size_t kLogSize = 6;
constexpr float kScreenshotAfterSeconds = 1.5F;

constexpr sp::Color kBackground{22, 24, 30};
constexpr sp::Color kIdle{58, 62, 72};
constexpr sp::Color kEdge{104, 110, 124};
constexpr sp::Color kActive{250, 190, 70};
constexpr sp::Color kAccent{120, 210, 255};
constexpr sp::Color kText{226, 229, 236};
constexpr sp::Color kDim{138, 144, 158};

/// @brief printf biçimli kısa metin.
std::string Format(const char* format, ...) {
  std::array<char, 160> buffer{};
  va_list args;
  va_start(args, format);
  std::vsnprintf(buffer.data(), buffer.size(), format, args);
  va_end(args);
  return buffer.data();
}

// --- Gövde ------------------------------------------------------------------

/// @brief Kübik Bézier parçası: iki kontrol noktası ve bitiş noktası.
struct Cubic {
  sp::Point c1;
  sp::Point c2;
  sp::Point end;
};

/// Gövdenin sağ yarısı, üst ortadan saat yönünde alt ortaya. Sol yarı bunun
/// simetri eksenine göre aynası olduğu için ayrıca yazılmaz.
constexpr sp::Point kBodyTop{kAxisX, 175.0F};
constexpr std::array<Cubic, 5> kBodyRightHalf = {{
    {{420.0F, 175.0F}, {470.0F, 155.0F}, {520.0F, 160.0F}},  // üst kenar
    {{575.0F, 165.0F}, {605.0F, 205.0F}, {610.0F, 260.0F}},  // omuz
    {{618.0F, 330.0F}, {630.0F, 430.0F}, {585.0F, 480.0F}},  // dış kenar
    {{560.0F, 520.0F}, {515.0F, 515.0F}, {500.0F, 470.0F}},  // tutamak ucu
    {{480.0F, 430.0F}, {420.0F, 420.0F}, {kAxisX, 420.0F}},  // iç kenar
}};

constexpr sp::Point Mirror(const sp::Point& p) {
  return {(2.0F * kAxisX) - p.x, p.y};
}

sp::Path BodyPath() {
  sp::Path path;
  path.MoveTo(kBodyTop);
  for (const Cubic& c : kBodyRightHalf) {
    path.CubicTo(c.c1, c.c2, c.end);
  }
  // Sol yarı: sağ yarının parçaları ters sırayla ve aynalanarak. Parça ters
  // yönde gezildiği için iki kontrol noktası da yer değiştirir.
  for (std::size_t i = kBodyRightHalf.size(); i-- > 0;) {
    const Cubic& c = kBodyRightHalf[i];
    const sp::Point start = (i == 0) ? kBodyTop : kBodyRightHalf[i - 1].end;
    path.CubicTo(Mirror(c.c2), Mirror(c.c1), Mirror(start));
  }
  path.Close();
  return path;
}

// --- Adlandırma -------------------------------------------------------------

/// @brief Aileye göre omuz ve tetik adları (çatının `GamepadType`'ı).
struct FamilyNames {
  const char* family;
  const char* shoulder_left;
  const char* shoulder_right;
  const char* trigger_left;
  const char* trigger_right;
};

FamilyNames NamesFor(sp::GamepadType type) {
  switch (type) {
    case sp::GamepadType::kPlayStation:
      return {"PlayStation", "L1", "R1", "L2", "R2"};
    case sp::GamepadType::kNintendo:
      return {"Nintendo", "L", "R", "ZL", "ZR"};
    case sp::GamepadType::kXbox:
      return {"Xbox", "LB", "RB", "LT", "RT"};
    case sp::GamepadType::kStandard:
      return {"standart", "LB", "RB", "LT", "RT"};
    default:
      return {"bilinmeyen", "LB", "RB", "LT", "RT"};
  }
}

/// @brief Yüz düğmesinin etiketi (SDL'in doğrudan kullanımı).
///
/// Gamepad yokken çizim yine A/B/X/Y gösterir.
SDL_GamepadButtonLabel FaceLabel(SDL_Gamepad* gamepad,
                                 SDL_GamepadButton button) {
  if (gamepad == nullptr) {
    return SDL_GetGamepadButtonLabelForType(SDL_GAMEPAD_TYPE_STANDARD, button);
  }
  return SDL_GetGamepadButtonLabel(gamepad, button);
}

const char* LabelName(SDL_GamepadButtonLabel label) {
  switch (label) {
    case SDL_GAMEPAD_BUTTON_LABEL_A:
      return "A";
    case SDL_GAMEPAD_BUTTON_LABEL_B:
      return "B";
    case SDL_GAMEPAD_BUTTON_LABEL_X:
      return "X";
    case SDL_GAMEPAD_BUTTON_LABEL_Y:
      return "Y";
    case SDL_GAMEPAD_BUTTON_LABEL_CROSS:
      return "Çarpı";
    case SDL_GAMEPAD_BUTTON_LABEL_CIRCLE:
      return "Daire";
    case SDL_GAMEPAD_BUTTON_LABEL_SQUARE:
      return "Kare";
    case SDL_GAMEPAD_BUTTON_LABEL_TRIANGLE:
      return "Üçgen";
    default:
      return "?";
  }
}

/// @brief Çatının düğmesinin SDL karşılığı; ek düğme yoksa geçersiz.
SDL_GamepadButton ToSdl(sp::GamepadButton button) {
  // GamepadButton, SDL'in ilk 15 düğmesiyle aynı sırada ve bir kaydırılmış.
  return static_cast<SDL_GamepadButton>(static_cast<int>(button) - 1);
}

const char* ButtonName(sp::GamepadButton button, const FamilyNames& names,
                       SDL_Gamepad* gamepad) {
  using B = sp::GamepadButton;
  switch (button) {
    case B::kSouth:
    case B::kEast:
    case B::kWest:
    case B::kNorth:
      return LabelName(FaceLabel(gamepad, ToSdl(button)));
    case B::kBack:
      return "Back";
    case B::kGuide:
      return "Guide";
    case B::kStart:
      return "Start";
    case B::kLeftStick:
      return "Sol çubuk";
    case B::kRightStick:
      return "Sağ çubuk";
    case B::kLeftShoulder:
      return names.shoulder_left;
    case B::kRightShoulder:
      return names.shoulder_right;
    case B::kDpadUp:
      return "D-pad yukarı";
    case B::kDpadDown:
      return "D-pad aşağı";
    case B::kDpadLeft:
      return "D-pad sol";
    case B::kDpadRight:
      return "D-pad sağ";
    default:
      return "?";
  }
}

const char* ConnectionName(SDL_JoystickConnectionState state) {
  switch (state) {
    case SDL_JOYSTICK_CONNECTION_WIRED:
      return "kablolu";
    case SDL_JOYSTICK_CONNECTION_WIRELESS:
      return "kablosuz";
    case SDL_JOYSTICK_CONNECTION_UNKNOWN:
      return "bilinmiyor";
    default:
      return "okunamadı";
  }
}

std::string PowerText(SDL_PowerState state, int percent) {
  const std::string kPercent = percent >= 0 ? Format(" %%%d", percent) : "";
  switch (state) {
    case SDL_POWERSTATE_ON_BATTERY:
      return "pilde" + kPercent;
    case SDL_POWERSTATE_NO_BATTERY:
      return "pil yok";
    case SDL_POWERSTATE_CHARGING:
      return "şarj oluyor" + kPercent;
    case SDL_POWERSTATE_CHARGED:
      return "dolu";
    case SDL_POWERSTATE_UNKNOWN:
      return "bilinmiyor";
    default:
      return "okunamadı";
  }
}

const char* YesNo(bool value) {
  return value ? "var" : "yok";
}

// --- Durum ------------------------------------------------------------------

/// @brief Bir gamepad'in durumu - çatının olaylarından kurulur.
///
/// Çatı yalnızca olay verir (bkz. input örneği); "şu an basılı mı?" bilgisini
/// burada tutarız.
struct PadState {
  sp::GamepadDeviceEvent device;
  std::array<bool, 16> buttons{};  ///< GamepadButton değeriyle indekslenir.
  std::array<float, 7> axes{};     ///< GamepadAxis değeriyle indekslenir.
  /// SDL'in doğrudan kullanımı: çatının alt kümesinde olmayan düğmeler,
  /// SDL_GamepadButton indeksine göre bit maskesi.
  uint32_t extra_buttons{0};

  [[nodiscard]] bool Down(sp::GamepadButton button) const {
    return buttons[static_cast<std::size_t>(button)];
  }
  [[nodiscard]] float Axis(sp::GamepadAxis axis) const {
    return axes[static_cast<std::size_t>(axis)];
  }
};

std::shared_ptr<sp::Font> LoadFont(const std::string& path, int32_t size) {
  auto font = std::make_shared<sp::Font>(path, size);
  return font->IsValid() ? font : nullptr;
}

}  // namespace

class GamepadViewer : public sp::Application {
 public:
  GamepadViewer(sp::AppConfig config, std::string screenshot_path)
      : Application(std::move(config)),
        mScreenshotPath(std::move(screenshot_path)) {}

 protected:
  bool OnInit() override {
    mBody = BodyPath();
    const std::string path = example::FindSystemFont();
    if (!path.empty()) {
      mTitleFont = LoadFont(path, 20);
      mFont = LoadFont(path, 15);
      mSmall = LoadFont(path, 13);
    }
    if (mFont == nullptr) {
      spdlog::warn("Sistem fontu bulunamadi; metinler cizilmeyecek.");
    }
    return true;
  }

  void OnUpdate(float dt) override {
    mTime += dt;
    if (mRumbleTest) {
      DriveRumbleFromTriggers();
    }
    if (!mScreenshotPath.empty() && !mCaptured &&
        mTime >= kScreenshotAfterSeconds) {
      mCaptured = true;
      // Ön tampon okunur: bir önceki karenin sunulmuş hâli (bkz.
      // stats_overlay örneği).
      if (bench::SaveBackBufferPng(mScreenshotPath, Width(), Height(), true)) {
        spdlog::info("Ekran goruntusu: {}", mScreenshotPath);
      }
      Quit();
    }
  }

  void OnRender(sp::Painter& painter) override {
    painter.Clear(kBackground);

    // Tasarım alanını en-boy oranını koruyarak pencereye sığdır ve ortala.
    const auto w = static_cast<float>(Width());
    const auto h = static_cast<float>(Height());
    const float scale = std::min(w / kDesignW, h / kDesignH);
    painter.Save();
    painter.Translate((w - (kDesignW * scale)) * 0.5F,
                      (h - (kDesignH * scale)) * 0.5F);
    painter.Scale(scale, scale);

    PadState* pad = Find(mActiveId);
    SDL_Gamepad* gamepad =
        pad != nullptr ? SDL_GetGamepadFromID(pad->device.id) : nullptr;

    // Gamepad yokken kol soluk çizilir.
    painter.SetOpacity(pad != nullptr ? 1.0F : 0.35F);
    DrawController(painter, pad != nullptr ? *pad : mNoPad, gamepad);
    painter.SetOpacity(1.0F);
    DrawExtraButtons(painter, pad, gamepad);
    DrawPanel(painter, pad, gamepad);

    painter.Restore();
  }

  void OnKeyDown(const sp::KeyEvent& event) override {
    if (event.repeat) {
      return;
    }
    if (event.key == sp::Key::kEscape) {
      Quit();
    } else if (event.key == sp::Key::kT) {
      mRumbleTest = !mRumbleTest;
      if (!mRumbleTest) {
        StopRumble();
      }
      Log(mRumbleTest ? "Titreşim testi açık" : "Titreşim testi kapalı");
    }
  }

  // --- 1. Çatının verdikleri ------------------------------------------------

  void OnGamepadAdded(const sp::GamepadDeviceEvent& event) override {
    PadState pad;
    pad.device = event;
    mPads.push_back(pad);
    mActiveId = event.id;

    // SDL'in doğrudan kullanımı: sensörler varsayılan olarak kapalı gelir.
    SDL_Gamepad* gamepad = SDL_GetGamepadFromID(event.id);
    for (const SDL_SensorType sensor : {SDL_SENSOR_GYRO, SDL_SENSOR_ACCEL}) {
      if (SDL_GamepadHasSensor(gamepad, sensor)) {
        SDL_SetGamepadSensorEnabled(gamepad, sensor, true);
      }
    }
    const char* name = SDL_GetGamepadName(gamepad);
    spdlog::info("Gamepad baglandi: {} (kimlik {}, aile {})",
                 name != nullptr ? name : "?", event.id,
                 NamesFor(event.type).family);
    Log(Format("%s bağlandı", name != nullptr ? name : "Gamepad"));
  }

  void OnGamepadRemoved(const sp::GamepadDeviceEvent& event) override {
    mPads.erase(std::remove_if(mPads.begin(), mPads.end(),
                               [&event](const PadState& pad) {
                                 return pad.device.id == event.id;
                               }),
                mPads.end());
    if (mActiveId == event.id) {
      mActiveId = mPads.empty() ? 0 : mPads.back().device.id;
    }
    Log(Format("Kimlik %u çıkarıldı", static_cast<unsigned>(event.id)));
  }

  void OnGamepadButtonDown(const sp::GamepadButtonEvent& event) override {
    OnButton(event, true);
  }

  void OnGamepadButtonUp(const sp::GamepadButtonEvent& event) override {
    OnButton(event, false);
  }

  void OnGamepadAxis(const sp::GamepadAxisEvent& event) override {
    PadState* pad = Find(event.id);
    if (pad == nullptr || event.axis == sp::GamepadAxis::kUnknown) {
      return;
    }
    pad->axes[static_cast<std::size_t>(event.axis)] = event.value;
    // Belirgin hareket etkin gamepad'i değiştirir; durağan çubuğun
    // titremesi değiştirmez.
    if (std::fabs(event.value) > 0.5F) {
      mActiveId = event.id;
    }
  }

  // --- 2. SDL'in doğrudan kullanımı: ham olaylar -----------------------------

  bool OnRawEvent(const SDL_Event& event) override {
    // Çatının 15 düğmelik alt kümesinde olmayan düğmeler (Share, paddle,
    // touchpad tuşu) kGamepadButton::kUnknown olarak gelir; gerçek
    // kimliklerini ham olaydan okuruz.
    const bool is_button = event.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN ||
                           event.type == SDL_EVENT_GAMEPAD_BUTTON_UP;
    if (is_button && event.gbutton.button >= SDL_GAMEPAD_BUTTON_MISC1) {
      if (PadState* pad = Find(event.gbutton.which)) {
        const uint32_t bit = 1U << event.gbutton.button;
        if (event.gbutton.down) {
          pad->extra_buttons |= bit;
        } else {
          pad->extra_buttons &= ~bit;
        }
        mActiveId = event.gbutton.which;
        const char* name = SDL_GetGamepadStringForButton(
            static_cast<SDL_GamepadButton>(event.gbutton.button));
        Log(Format("%s %s", name != nullptr ? name : "?",
                   event.gbutton.down ? "basıldı" : "bırakıldı"));
      }
    }
    return false;  // Tüketme: çatı da işlesin.
  }

 private:
  void OnButton(const sp::GamepadButtonEvent& event, bool down) {
    PadState* pad = Find(event.id);
    // kUnknown ek düğmelerdir; OnRawEvent ele alır.
    if (pad == nullptr || event.button == sp::GamepadButton::kUnknown) {
      return;
    }
    pad->buttons[static_cast<std::size_t>(event.button)] = down;
    mActiveId = event.id;
    Log(Format("%s %s",
               ButtonName(event.button, NamesFor(pad->device.type),
                          SDL_GetGamepadFromID(event.id)),
               down ? "basıldı" : "bırakıldı"));
  }

  PadState* Find(sp::GamepadId id) {
    for (PadState& pad : mPads) {
      if (pad.device.id == id) {
        return &pad;
      }
    }
    return nullptr;
  }

  void Log(const std::string& text) {
    mLog.push_front(Format("%6.1f s   ", static_cast<double>(mTime)) + text);
    if (mLog.size() > kLogSize) {
      mLog.pop_back();
    }
  }

  /// @brief Titreşim testi: tetik ne kadar basılıysa motor o kadar güçlü.
  void DriveRumbleFromTriggers() {
    const PadState* pad = Find(mActiveId);
    if (pad == nullptr) {
      return;
    }
    const float left = pad->Axis(sp::GamepadAxis::kLeftTrigger);
    const float right = pad->Axis(sp::GamepadAxis::kRightTrigger);
    // Her kare yeniden istenir; kare kesilirse 100 ms sonra kendiliğinden
    // durur.
    RumbleGamepad(pad->device.id, left, right, 100);
    // SDL'in doğrudan kullanımı: tetik motorları çatıda yok.
    SDL_RumbleGamepadTriggers(SDL_GetGamepadFromID(pad->device.id),
                              static_cast<Uint16>(left * 65535.0F),
                              static_cast<Uint16>(right * 65535.0F), 100);
  }

  void StopRumble() {
    for (const PadState& pad : mPads) {
      RumbleGamepad(pad.device.id, 0.0F, 0.0F, 0);
      SDL_RumbleGamepadTriggers(SDL_GetGamepadFromID(pad.device.id), 0, 0, 0);
    }
  }

  // --- Çizim: ortak parçalar --------------------------------------------------

  void Label(sp::Painter& p, const std::shared_ptr<sp::Font>& font,
             const sp::Color& color, const sp::Rect& rect,
             const std::string& text,
             sp::Alignment alignment = sp::Alignment::kLeft) {
    if (font == nullptr) {
      return;
    }
    p.SetFont(font);
    p.SetPen(sp::Pen(color, 1.0F));
    p.DrawText(rect, text, alignment);
  }

  /// @brief Yuvarlak uçlu tuş; basılıyken yanar.
  void DrawKey(sp::Painter& p, const sp::Rect& r, bool down,
               const char* label) {
    const float radius = r.h * 0.5F;
    p.SetBrush(sp::Brush(down ? kActive : kIdle));
    p.FillRoundedRect(r, radius);
    p.SetPen(sp::Pen(kEdge, 1.5F));
    p.DrawRoundedRect(r, radius);
    if (label != nullptr) {
      Label(p, mSmall, down ? kBackground : kText, r, label,
            sp::Alignment::kCenter);
    }
  }

  // --- Çizim: kol (yalnızca çatının verdikleri + yüz etiketi) -------------------

  void DrawController(sp::Painter& p, const PadState& st,
                      SDL_Gamepad* gamepad) {
    using A = sp::GamepadAxis;
    using B = sp::GamepadButton;
    const FamilyNames names = NamesFor(st.device.type);

    DrawTrigger(p, sp::Rect{150.0F, 70.0F, 110.0F, 46.0F},
                st.Axis(A::kLeftTrigger), names.trigger_left);
    DrawTrigger(p, sp::Rect{440.0F, 70.0F, 110.0F, 46.0F},
                st.Axis(A::kRightTrigger), names.trigger_right);
    DrawKey(p, sp::Rect{140.0F, 126.0F, 140.0F, 26.0F},
            st.Down(B::kLeftShoulder), names.shoulder_left);
    DrawKey(p, sp::Rect{420.0F, 126.0F, 140.0F, 26.0F},
            st.Down(B::kRightShoulder), names.shoulder_right);

    // Gövde: üstten alta koyulaşan gradient.
    p.SetBrush(sp::Brush::LinearGradient(
        sp::Point{kAxisX, 150.0F}, sp::Point{kAxisX, 520.0F},
        sp::Color{78, 83, 96}, sp::Color{40, 43, 51}));
    p.FillPath(mBody);
    p.SetPen(sp::Pen(kEdge, 2.0F));
    p.DrawPath(mBody);

    DrawStick(p, sp::Point{205.0F, 265.0F}, st.Axis(A::kLeftX),
              st.Axis(A::kLeftY), st.Down(B::kLeftStick));
    DrawStick(p, sp::Point{430.0F, 355.0F}, st.Axis(A::kRightX),
              st.Axis(A::kRightY), st.Down(B::kRightStick));
    DrawDpad(p, sp::Point{268.0F, 355.0F}, st);
    DrawFaceButtons(p, sp::Point{495.0F, 265.0F}, st, gamepad);

    // Orta düğmeler.
    DrawKey(p, sp::Rect{288.0F, 246.0F, 38.0F, 20.0F}, st.Down(B::kBack),
            nullptr);
    DrawKey(p, sp::Rect{374.0F, 246.0F, 38.0F, 20.0F}, st.Down(B::kStart),
            nullptr);
    Label(p, mSmall, kDim, sp::Rect{268.0F, 268.0F, 78.0F, 18.0F}, "Back",
          sp::Alignment::kCenter);
    Label(p, mSmall, kDim, sp::Rect{354.0F, 268.0F, 78.0F, 18.0F}, "Start",
          sp::Alignment::kCenter);
    p.SetBrush(sp::Brush(st.Down(B::kGuide) ? kActive : kIdle));
    p.FillCircle(kAxisX, 212.0F, 17.0F);
    p.SetPen(sp::Pen(kEdge, 1.5F));
    p.DrawCircle(kAxisX, 212.0F, 17.0F);
  }

  /// @brief Tetik: basıldıkça alttan dolan kutu.
  void DrawTrigger(sp::Painter& p, const sp::Rect& r, float value,
                   const char* name) {
    constexpr float kRadius = 8.0F;
    p.SetBrush(sp::Brush(kIdle));
    p.FillRoundedRect(r, kRadius);
    const float fill = r.h * std::clamp(value, 0.0F, 1.0F);
    if (fill > 0.5F) {
      p.SetBrush(sp::Brush(kActive));
      p.FillRoundedRect(sp::Rect{r.x, r.y + r.h - fill, r.w, fill},
                        std::min(kRadius, fill * 0.5F));
    }
    p.SetPen(sp::Pen(kEdge, 1.5F));
    p.DrawRoundedRect(r, kRadius);
    Label(p, mFont, kText, r,
          Format("%s  %3.0f%%", name, static_cast<double>(value * 100.0F)),
          sp::Alignment::kCenter);
  }

  /// @brief Çubuk: başlık fiziksel çubuk gibi az kayar; nokta ham değeri tam
  ///        erişimle gösterir, halka ölü bölgedir.
  void DrawStick(sp::Painter& p, const sp::Point& c, float x, float y,
                 bool pressed) {
    p.SetBrush(sp::Brush(sp::Color{30, 32, 38}));
    p.FillCircle(c, kWellRadius);
    p.SetPen(sp::Pen(kEdge, 1.5F));
    p.DrawCircle(c, kWellRadius);

    const sp::Point cap{c.x + (x * kCapTravel), c.y + (y * kCapTravel)};
    p.SetBrush(sp::Brush(pressed ? kActive : kIdle));
    p.FillCircle(cap, kCapRadius);
    p.DrawCircle(cap, kCapRadius);

    p.SetPen(sp::Pen(sp::Color{255, 255, 255, 70}, 1.0F));
    p.DrawCircle(c, kDotRange * kDeadZone);
    const bool in_dead_zone = std::hypot(x, y) < kDeadZone;
    p.SetBrush(sp::Brush(in_dead_zone ? kDim : kAccent));
    p.FillCircle(c.x + (x * kDotRange), c.y + (y * kDotRange), 4.0F);
  }

  void DrawDpad(sp::Painter& p, const sp::Point& c, const PadState& st) {
    struct Arm {
      sp::GamepadButton button;
      sp::Rect rect;
    };
    // Kollar ortadaki kareye kadar uzanır; kare çerçevesiz kalır.
    constexpr float kHalf = 13.0F;
    constexpr float kReach = 40.0F;
    constexpr float kArm = kReach - kHalf;
    const std::array<Arm, 4> arms = {{
        {sp::GamepadButton::kDpadUp,
         sp::Rect{c.x - kHalf, c.y - kReach, 2.0F * kHalf, kArm}},
        {sp::GamepadButton::kDpadDown,
         sp::Rect{c.x - kHalf, c.y + kHalf, 2.0F * kHalf, kArm}},
        {sp::GamepadButton::kDpadLeft,
         sp::Rect{c.x - kReach, c.y - kHalf, kArm, 2.0F * kHalf}},
        {sp::GamepadButton::kDpadRight,
         sp::Rect{c.x + kHalf, c.y - kHalf, kArm, 2.0F * kHalf}},
    }};
    p.SetBrush(sp::Brush(kIdle));
    p.FillRect(c.x - kHalf, c.y - kHalf, 2.0F * kHalf, 2.0F * kHalf);
    for (const Arm& arm : arms) {
      p.SetBrush(sp::Brush(st.Down(arm.button) ? kActive : kIdle));
      p.FillRoundedRect(arm.rect, 5.0F);
      p.SetPen(sp::Pen(kEdge, 1.5F));
      p.DrawRoundedRect(arm.rect, 5.0F);
    }
  }

  void DrawFaceButtons(sp::Painter& p, const sp::Point& c, const PadState& st,
                       SDL_Gamepad* gamepad) {
    constexpr float kGap = 36.0F;
    constexpr float kRadius = 17.0F;
    struct Face {
      sp::GamepadButton button;
      sp::Point pos;
    };
    const std::array<Face, 4> faces = {{
        {sp::GamepadButton::kSouth, sp::Point{c.x, c.y + kGap}},
        {sp::GamepadButton::kEast, sp::Point{c.x + kGap, c.y}},
        {sp::GamepadButton::kWest, sp::Point{c.x - kGap, c.y}},
        {sp::GamepadButton::kNorth, sp::Point{c.x, c.y - kGap}},
    }};
    for (const Face& face : faces) {
      const bool down = st.Down(face.button);
      p.SetBrush(sp::Brush(down ? kActive : kIdle));
      p.FillCircle(face.pos, kRadius);
      p.SetPen(sp::Pen(kEdge, 1.5F));
      p.DrawCircle(face.pos, kRadius);

      const sp::Color ink = down ? kBackground : kText;
      const SDL_GamepadButtonLabel label =
          FaceLabel(gamepad, ToSdl(face.button));
      if (label >= SDL_GAMEPAD_BUTTON_LABEL_CROSS) {
        DrawSonySymbol(p, face.pos, label, ink);
      } else {
        Label(p, mFont, ink,
              sp::Rect{face.pos.x - kRadius, face.pos.y - kRadius,
                       2.0F * kRadius, 2.0F * kRadius},
              LabelName(label), sp::Alignment::kCenter);
      }
    }
  }

  /// @brief ✕ ○ □ △: fontlarda çoğu zaman olmadıkları için çizilir.
  static void DrawSonySymbol(sp::Painter& p, const sp::Point& c,
                             SDL_GamepadButtonLabel label,
                             const sp::Color& ink) {
    p.SetPen(sp::Pen(ink, 2.5F));
    switch (label) {
      case SDL_GAMEPAD_BUTTON_LABEL_CROSS:
        p.DrawLine(c.x - 7.0F, c.y - 7.0F, c.x + 7.0F, c.y + 7.0F);
        p.DrawLine(c.x - 7.0F, c.y + 7.0F, c.x + 7.0F, c.y - 7.0F);
        break;
      case SDL_GAMEPAD_BUTTON_LABEL_CIRCLE:
        p.DrawCircle(c, 7.5F);
        break;
      case SDL_GAMEPAD_BUTTON_LABEL_SQUARE:
        p.DrawRect(c.x - 6.5F, c.y - 6.5F, 13.0F, 13.0F);
        break;
      default:  // Üçgen.
        p.DrawPolygon({sp::Point{c.x, c.y - 8.0F},
                       sp::Point{c.x + 7.5F, c.y + 5.5F},
                       sp::Point{c.x - 7.5F, c.y + 5.5F}});
        break;
    }
  }

  // --- Çizim: SDL'in doğrudan kullanımı ---------------------------------------

  /// @brief Ek düğmeler: gamepad'de bulunanlar listelenir, basılı olan yanar.
  void DrawExtraButtons(sp::Painter& p, const PadState* pad,
                        SDL_Gamepad* gamepad) {
    Label(p, mFont, kDim, sp::Rect{40.0F, 536.0F, 640.0F, kLineH},
          "Ek düğmeler (SDL'den doğrudan)");
    if (pad != nullptr) {
      int32_t shown = 0;
      for (int b = SDL_GAMEPAD_BUTTON_MISC1; b < SDL_GAMEPAD_BUTTON_COUNT;
           ++b) {
        const auto button = static_cast<SDL_GamepadButton>(b);
        if (!SDL_GamepadHasButton(gamepad, button)) {
          continue;
        }
        const bool down =
            (pad->extra_buttons & (1U << static_cast<unsigned>(b))) != 0;
        const float x = 40.0F + (112.0F * static_cast<float>(shown));
        DrawKey(p, sp::Rect{x, 562.0F, 104.0F, 24.0F}, down,
                SDL_GetGamepadStringForButton(button));
        ++shown;
      }
      if (shown == 0) {
        Label(p, mSmall, kDim, sp::Rect{40.0F, 562.0F, 300.0F, 24.0F}, "yok");
      }
    }
    Label(p, mSmall, kDim, sp::Rect{40.0F, 604.0F, 640.0F, kLineH},
          mRumbleTest ? "T: titreşim testi AÇIK - tetiklere bas"
                      : "T: titreşim testi (kapalı)   ESC: çıkış");
  }

  void KeyValue(sp::Painter& p, float y, const char* key,
                const std::string& value) {
    Label(p, mFont, kDim, sp::Rect{kPanelX, y, 128.0F, kLineH}, key);
    Label(p, mFont, kText,
          sp::Rect{kPanelX + 128.0F, y, kPanelW - 128.0F, kLineH}, value);
  }

  void Header(sp::Painter& p, float y, const char* text) {
    Label(p, mSmall, kAccent, sp::Rect{kPanelX, y, kPanelW, 18.0F}, text);
  }

  void DrawPanel(sp::Painter& p, const PadState* pad, SDL_Gamepad* gamepad) {
    if (pad == nullptr || gamepad == nullptr) {
      Label(p, mTitleFont, kText, sp::Rect{kPanelX, 30.0F, kPanelW, 28.0F},
            "Gamepad bağlı değil");
      Label(p, mFont, kDim, sp::Rect{kPanelX, 62.0F, kPanelW, kLineH},
            "Bir gamepad bağlayın; tak-çıkar desteklenir.");
      DrawLog(p, 504.0F);
      return;
    }

    const char* name = SDL_GetGamepadName(gamepad);
    Label(p, mTitleFont, kText, sp::Rect{kPanelX, 30.0F, kPanelW, 28.0F},
          name != nullptr ? name : "(adsız gamepad)");
    // Çatının verdikleri.
    Label(p, mFont, kText, sp::Rect{kPanelX, 62.0F, kPanelW, kLineH},
          Format("Aile: %s  ·  Kimlik: %u  ·  Bağlı: %zu",
                 NamesFor(pad->device.type).family,
                 static_cast<unsigned>(pad->device.id), mPads.size()));

    float y = 94.0F;
    Header(p, y, "SDL'DEN DOĞRUDAN");
    y += 20.0F;
    KeyValue(p, y, "Vendor:Product",
             Format("%04x:%04x",
                    static_cast<unsigned>(SDL_GetGamepadVendor(gamepad)),
                    static_cast<unsigned>(SDL_GetGamepadProduct(gamepad))));
    y += kLineH;
    KeyValue(p, y, "Bağlantı",
             ConnectionName(SDL_GetGamepadConnectionState(gamepad)));
    y += kLineH;
    int percent = -1;
    const SDL_PowerState power = SDL_GetGamepadPowerInfo(gamepad, &percent);
    KeyValue(p, y, "Pil", PowerText(power, percent));
    y += kLineH;
    const int player = SDL_GetGamepadPlayerIndex(gamepad);
    KeyValue(p, y, "Oyuncu sırası",
             player >= 0 ? Format("%d", player) : "atanmamış");
    y += kLineH;
    const char* serial = SDL_GetGamepadSerial(gamepad);
    KeyValue(p, y, "Seri no",
             serial != nullptr && serial[0] != '\0' ? serial : "-");
    y += kLineH;
    const SDL_PropertiesID props = SDL_GetGamepadProperties(gamepad);
    KeyValue(
        p, y, "Yetenekler",
        Format("titreşim %s · tetik %s · LED %s",
               YesNo(SDL_GetBooleanProperty(
                   props, SDL_PROP_GAMEPAD_CAP_RUMBLE_BOOLEAN, false)),
               YesNo(SDL_GetBooleanProperty(
                   props, SDL_PROP_GAMEPAD_CAP_TRIGGER_RUMBLE_BOOLEAN, false)),
               YesNo(SDL_GetBooleanProperty(
                   props, SDL_PROP_GAMEPAD_CAP_RGB_LED_BOOLEAN, false))));
    y += kLineH + 10.0F;

    Header(p, y, "EKSENLER   çatı: normalize  ·  SDL: ham");
    y += 20.0F;
    DrawAxes(p, y, *pad, gamepad);
    y += (6.0F * kLineH) + 10.0F;

    Header(p, y, "SENSÖRLER VE TOUCHPAD (SDL)");
    y += 20.0F;
    DrawSensor(p, y, gamepad, SDL_SENSOR_GYRO, "Jiroskop");
    y += kLineH;
    DrawSensor(p, y, gamepad, SDL_SENSOR_ACCEL, "İvmeölçer");
    y += kLineH;
    y += DrawTouchpad(p, y, gamepad) + 10.0F;

    DrawLog(p, y);
  }

  void DrawAxes(sp::Painter& p, float y, const PadState& pad,
                SDL_Gamepad* gamepad) {
    const FamilyNames names = NamesFor(pad.device.type);
    struct Row {
      sp::GamepadAxis axis;
      const char* name;
      bool trigger;
    };
    const std::array<Row, 6> rows = {{
        {sp::GamepadAxis::kLeftX, "Sol X", false},
        {sp::GamepadAxis::kLeftY, "Sol Y", false},
        {sp::GamepadAxis::kRightX, "Sağ X", false},
        {sp::GamepadAxis::kRightY, "Sağ Y", false},
        {sp::GamepadAxis::kLeftTrigger, names.trigger_left, true},
        {sp::GamepadAxis::kRightTrigger, names.trigger_right, true},
    }};
    for (const Row& row : rows) {
      const float value = pad.Axis(row.axis);
      const Sint16 raw = SDL_GetGamepadAxis(
          gamepad,
          static_cast<SDL_GamepadAxis>(static_cast<int>(row.axis) - 1));
      Label(p, mFont, kDim, sp::Rect{kPanelX, y, 60.0F, kLineH}, row.name);
      DrawAxisBar(p, sp::Rect{kPanelX + 62.0F, y + 6.0F, 130.0F, 8.0F}, value,
                  row.trigger);
      Label(p, mFont, kText, sp::Rect{kPanelX + 196.0F, y, 60.0F, kLineH},
            Format("%+.2f", static_cast<double>(value)), sp::Alignment::kRight);
      Label(p, mFont, kDim, sp::Rect{kPanelX + 262.0F, y, 74.0F, kLineH},
            Format("%d", static_cast<int>(raw)), sp::Alignment::kRight);
      y += kLineH;
    }
  }

  /// @brief Çubuk eksenleri ortadan iki yana, tetikler soldan dolar.
  static void DrawAxisBar(sp::Painter& p, const sp::Rect& r, float value,
                          bool trigger) {
    p.SetBrush(sp::Brush(kIdle));
    p.FillRoundedRect(r, r.h * 0.5F);
    p.SetBrush(sp::Brush(kAccent));
    if (trigger) {
      const float w = r.w * std::clamp(value, 0.0F, 1.0F);
      p.FillRect(r.x, r.y, w, r.h);
    } else {
      const float mid = r.x + (r.w * 0.5F);
      const float w = r.w * 0.5F * std::clamp(value, -1.0F, 1.0F);
      p.FillRect(std::min(mid, mid + w), r.y, std::fabs(w), r.h);
      p.SetPen(sp::Pen(kDim, 1.0F));
      p.DrawLine(mid, r.y - 2.0F, mid, r.y + r.h + 2.0F);
    }
  }

  void DrawSensor(sp::Painter& p, float y, SDL_Gamepad* gamepad,
                  SDL_SensorType sensor, const char* name) {
    std::array<float, 3> data{};
    if (!SDL_GamepadHasSensor(gamepad, sensor) ||
        !SDL_GetGamepadSensorData(gamepad, sensor, data.data(), 3)) {
      KeyValue(p, y, name, "yok");
      return;
    }
    KeyValue(
        p, y, name,
        Format("%+6.2f  %+6.2f  %+6.2f", static_cast<double>(data[0]),
               static_cast<double>(data[1]), static_cast<double>(data[2])));
  }

  /// @return Kullanılan yükseklik.
  float DrawTouchpad(sp::Painter& p, float y, SDL_Gamepad* gamepad) {
    if (SDL_GetNumGamepadTouchpads(gamepad) <= 0) {
      KeyValue(p, y, "Touchpad", "yok");
      return kLineH;
    }
    const int fingers = SDL_GetNumGamepadTouchpadFingers(gamepad, 0);
    KeyValue(p, y, "Touchpad", Format("%d parmak", fingers));
    const sp::Rect pad{kPanelX + 220.0F, y, 110.0F, 50.0F};
    p.SetBrush(sp::Brush(sp::Color{30, 32, 38}));
    p.FillRoundedRect(pad, 6.0F);
    p.SetPen(sp::Pen(kEdge, 1.0F));
    p.DrawRoundedRect(pad, 6.0F);
    for (int f = 0; f < fingers; ++f) {
      bool down = false;
      float fx = 0.0F;
      float fy = 0.0F;
      float pressure = 0.0F;
      if (SDL_GetGamepadTouchpadFinger(gamepad, 0, f, &down, &fx, &fy,
                                       &pressure) &&
          down) {
        p.SetBrush(sp::Brush(kActive));
        p.FillCircle(pad.x + (fx * pad.w), pad.y + (fy * pad.h), 5.0F);
      }
    }
    return 54.0F;
  }

  void DrawLog(sp::Painter& p, float y) {
    Header(p, y, "SON OLAYLAR");
    y += 20.0F;
    for (const std::string& line : mLog) {
      Label(p, mSmall, kText, sp::Rect{kPanelX, y, kPanelW, 17.0F}, line);
      y += 17.0F;
    }
  }

  std::string mScreenshotPath;
  sp::Path mBody;
  std::shared_ptr<sp::Font> mTitleFont;
  std::shared_ptr<sp::Font> mFont;
  std::shared_ptr<sp::Font> mSmall;

  std::vector<PadState> mPads;  ///< Bağlı gamepad'ler.
  PadState mNoPad;              ///< Gamepad yokken çizilen boş durum.
  sp::GamepadId mActiveId{0};   ///< Gösterilen gamepad; 0 = yok.
  std::deque<std::string> mLog;
  float mTime{0.0F};
  bool mRumbleTest{false};
  bool mCaptured{false};
};

int main(int argc, char** argv) {
  std::string screenshot_path;
  for (int i = 1; i < argc; ++i) {
    if (std::strncmp(argv[i], "--screenshot=", 13) == 0) {
      screenshot_path = argv[i] + 13;
    }
  }

  sp::AppConfig config;
  config.title = "SDLPainter - gamepad";
  config.width = static_cast<int32_t>(kDesignW);
  config.height = static_cast<int32_t>(kDesignH);
  config.enable_gamepad = true;

  GamepadViewer app(std::move(config), std::move(screenshot_path));
  return app.Run();
}
