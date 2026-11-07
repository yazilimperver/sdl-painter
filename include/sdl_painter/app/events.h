#pragma once

#include <cstdint>

namespace sdl_painter {

/// @brief Klavye tuşu — SDL keycode'larından bağımsız, taşınabilir alt küme.
///
/// Değerler kararlı sıralı bir kimlik taşır; harf/rakam grupları bitişiktir.
/// SDL keycode'undan çeviri @ref internal::TranslateKey ile yapılır.
enum class Key : uint16_t {
  kUnknown = 0,

  // Harfler (bitişik).
  kA,
  kB,
  kC,
  kD,
  kE,
  kF,
  kG,
  kH,
  kI,
  kJ,
  kK,
  kL,
  kM,
  kN,
  kO,
  kP,
  kQ,
  kR,
  kS,
  kT,
  kU,
  kV,
  kW,
  kX,
  kY,
  kZ,

  // Rakamlar (bitişik).
  k0,
  k1,
  k2,
  k3,
  k4,
  k5,
  k6,
  k7,
  k8,
  k9,

  // Fonksiyon tuşları (bitişik).
  kF1,
  kF2,
  kF3,
  kF4,
  kF5,
  kF6,
  kF7,
  kF8,
  kF9,
  kF10,
  kF11,
  kF12,

  // Yön tuşları.
  kUp,
  kDown,
  kLeft,
  kRight,

  // Kontrol / düzenleme tuşları.
  kEscape,
  kEnter,
  kSpace,
  kTab,
  kBackspace,
  kDelete,
  kHome,
  kEnd,
  kPageUp,
  kPageDown,

  // Değiştirici (modifier) tuşları.
  kLeftShift,
  kRightShift,
  kLeftCtrl,
  kRightCtrl,
  kLeftAlt,
  kRightAlt,
  kLeftGui,
  kRightGui,
};

/// @brief Değiştirici tuş maskesi (bit bayrağı olarak birleştirilebilir).
enum class KeyModifier : uint8_t {
  kNone = 0,
  kShift = 1U << 0U,
  kCtrl = 1U << 1U,
  kAlt = 1U << 2U,
  kGui = 1U << 3U,
};

/// @brief İki değiştirici maskesini birleştir.
constexpr KeyModifier operator|(KeyModifier lhs, KeyModifier rhs) noexcept {
  return static_cast<KeyModifier>(static_cast<uint8_t>(lhs) |
                                  static_cast<uint8_t>(rhs));
}

/// @brief İki değiştirici maskesinin kesişimi.
constexpr KeyModifier operator&(KeyModifier lhs, KeyModifier rhs) noexcept {
  return static_cast<KeyModifier>(static_cast<uint8_t>(lhs) &
                                  static_cast<uint8_t>(rhs));
}

/// @brief Verilen değiştirici bayrağı @p mods içinde ayarlı mı?
constexpr bool HasModifier(KeyModifier mods, KeyModifier flag) noexcept {
  return (mods & flag) == flag && flag != KeyModifier::kNone;
}

/// @brief Fare düğmesi.
enum class MouseButton : uint8_t {
  kUnknown = 0,
  kLeft,
  kMiddle,
  kRight,
  kX1,
  kX2,
};

/// @brief Klavye olayı.
struct KeyEvent {
  Key key{Key::kUnknown};
  KeyModifier modifiers{KeyModifier::kNone};
  bool repeat{false};  ///< Basılı tutmadan üretilen tekrar olayı mı?
};
/// @brief Fare düğmesi olayı.
struct MouseButtonEvent {
  MouseButton button{MouseButton::kUnknown};
  float x{0.0F};
  float y{0.0F};
  uint8_t clicks{1};  ///< 1 = tek tık, 2 = çift tık.
};

/// @brief Fare hareketi olayı.
struct MouseMoveEvent {
  float x{0.0F};
  float y{0.0F};
  float dx{0.0F};  ///< Son olaydan bu yana yatay değişim.
  float dy{0.0F};  ///< Son olaydan bu yana dikey değişim.
};

/// @brief Fare tekerleği olayı.
struct MouseWheelEvent {
  float dx{0.0F};       ///< Yatay tekerlek kaydırması.
  float dy{0.0F};       ///< Dikey tekerlek kaydırması.
  float mouse_x{0.0F};  ///< Olay anındaki fare X konumu.
  float mouse_y{0.0F};  ///< Olay anındaki fare Y konumu.
};

/// @brief Pencere yeniden boyutlandırma olayı.
struct ResizeEvent {
  int32_t width{0};
  int32_t height{0};
};

/// @brief Bağlı bir gamepad'in kimliği (SDL_JoystickID ile aynı değer).
///
/// Bağlantı süresince sabittir; cihaz çıkarılıp yeniden takılınca yeni bir
/// kimlik alır.
using GamepadId = uint32_t;

/// @brief Gamepad düğmesi — etikete göre değil, konuma göre adlandırılır.
///
/// @ref GamepadButton::kSouth Xbox'ta A, PlayStation'da ✕, Nintendo'da B'dir.
/// Sıra SDL_GamepadButton'ın ilk on beş değeriyle aynıdır. Ek düğmeler
/// (paddle, misc, touchpad) @ref GamepadButton::kUnknown olarak gelir; ham
/// değerleri `OnRawEvent` ile okunabilir.
enum class GamepadButton : uint8_t {
  kUnknown = 0,
  kSouth,
  kEast,
  kWest,
  kNorth,
  kBack,
  kGuide,
  kStart,
  kLeftStick,
  kRightStick,
  kLeftShoulder,
  kRightShoulder,
  kDpadUp,
  kDpadDown,
  kDpadLeft,
  kDpadRight,
};

/// @brief Gamepad analog ekseni.
enum class GamepadAxis : uint8_t {
  kUnknown = 0,
  kLeftX,
  kLeftY,
  kRightX,
  kRightY,
  kLeftTrigger,
  kRightTrigger,
};

/// @brief Gamepad ailesi — ekrandaki tuş ipuçlarını seçmek için.
enum class GamepadType : uint8_t {
  kUnknown = 0,
  kStandard,     ///< Tanınan ama belirli bir aileye girmeyen gamepad.
  kXbox,         ///< Xbox 360, Xbox One ve uyumluları.
  kPlayStation,  ///< PS3, PS4, PS5 ve uyumluları.
  kNintendo,     ///< Switch Pro, Joy-Con, GameCube ve uyumluları.
};

/// @brief Gamepad takıldı / çıkarıldı olayı.
struct GamepadDeviceEvent {
  GamepadId id{0};
  GamepadType type{GamepadType::kUnknown};
};

/// @brief Gamepad düğme olayı.
struct GamepadButtonEvent {
  GamepadId id{0};
  GamepadButton button{GamepadButton::kUnknown};
};

/// @brief Gamepad eksen olayı.
struct GamepadAxisEvent {
  GamepadId id{0};
  GamepadAxis axis{GamepadAxis::kUnknown};
  /// @brief Normalize değer: çubuklarda [-1, 1], tetiklerde [0, 1].
  ///
  /// Ölü bölge (dead zone) uygulanmaz; çubuklar bırakıldığında da sıfırın
  /// biraz yanında durabilir.
  float value{0.0F};
};

}  // namespace sdl_painter
