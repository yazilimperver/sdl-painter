#include "app/event_translator.h"

#include <SDL3/SDL.h>

#include <algorithm>

namespace sdl_painter::internal {

// GamepadButton ve GamepadAxis, SDL'in ilk değerleriyle aynı sırada; 0
// kUnknown'a ayrıldığı için çeviri bir kaydırmadır. SDL bu sırayı ABI
// gereği değiştirmez, yine de kayarsa derleme dursun.
static_assert(SDL_GAMEPAD_BUTTON_SOUTH == 0 &&
                  static_cast<int>(GamepadButton::kDpadRight) ==
                      SDL_GAMEPAD_BUTTON_DPAD_RIGHT + 1,
              "GamepadButton, SDL_GamepadButton sirasini izlemeli");
static_assert(SDL_GAMEPAD_AXIS_LEFTX == 0 &&
                  static_cast<int>(GamepadAxis::kRightTrigger) ==
                      SDL_GAMEPAD_AXIS_RIGHT_TRIGGER + 1,
              "GamepadAxis, SDL_GamepadAxis sirasini izlemeli");

Key TranslateKey(uint32_t sdl_keycode) noexcept {
  // Harfler ve rakamlar SDL3'te ASCII olarak bitişik; Key enum'unda da
  // bitişik olduklarından aralık kaydırmasıyla eşlenirler.
  if (sdl_keycode >= SDLK_A && sdl_keycode <= SDLK_Z) {
    return static_cast<Key>(static_cast<uint16_t>(Key::kA) +
                            (sdl_keycode - SDLK_A));
  }
  if (sdl_keycode >= SDLK_0 && sdl_keycode <= SDLK_9) {
    return static_cast<Key>(static_cast<uint16_t>(Key::k0) +
                            (sdl_keycode - SDLK_0));
  }
  if (sdl_keycode >= SDLK_F1 && sdl_keycode <= SDLK_F12) {
    return static_cast<Key>(static_cast<uint16_t>(Key::kF1) +
                            (sdl_keycode - SDLK_F1));
  }

  switch (sdl_keycode) {
    case SDLK_UP:
      return Key::kUp;
    case SDLK_DOWN:
      return Key::kDown;
    case SDLK_LEFT:
      return Key::kLeft;
    case SDLK_RIGHT:
      return Key::kRight;

    case SDLK_ESCAPE:
      return Key::kEscape;
    case SDLK_RETURN:
      return Key::kEnter;
    case SDLK_SPACE:
      return Key::kSpace;
    case SDLK_TAB:
      return Key::kTab;
    case SDLK_BACKSPACE:
      return Key::kBackspace;
    case SDLK_DELETE:
      return Key::kDelete;
    case SDLK_HOME:
      return Key::kHome;
    case SDLK_END:
      return Key::kEnd;
    case SDLK_PAGEUP:
      return Key::kPageUp;
    case SDLK_PAGEDOWN:
      return Key::kPageDown;

    case SDLK_LSHIFT:
      return Key::kLeftShift;
    case SDLK_RSHIFT:
      return Key::kRightShift;
    case SDLK_LCTRL:
      return Key::kLeftCtrl;
    case SDLK_RCTRL:
      return Key::kRightCtrl;
    case SDLK_LALT:
      return Key::kLeftAlt;
    case SDLK_RALT:
      return Key::kRightAlt;
    case SDLK_LGUI:
      return Key::kLeftGui;
    case SDLK_RGUI:
      return Key::kRightGui;

    default:
      return Key::kUnknown;
  }
}

KeyModifier TranslateModifiers(uint16_t sdl_mod) noexcept {
  KeyModifier result = KeyModifier::kNone;
  if ((sdl_mod & SDL_KMOD_SHIFT) != 0) {
    result = result | KeyModifier::kShift;
  }
  if ((sdl_mod & SDL_KMOD_CTRL) != 0) {
    result = result | KeyModifier::kCtrl;
  }
  if ((sdl_mod & SDL_KMOD_ALT) != 0) {
    result = result | KeyModifier::kAlt;
  }
  if ((sdl_mod & SDL_KMOD_GUI) != 0) {
    result = result | KeyModifier::kGui;
  }
  return result;
}

MouseButton TranslateMouseButton(uint8_t sdl_button) noexcept {
  switch (sdl_button) {
    case SDL_BUTTON_LEFT:
      return MouseButton::kLeft;
    case SDL_BUTTON_MIDDLE:
      return MouseButton::kMiddle;
    case SDL_BUTTON_RIGHT:
      return MouseButton::kRight;
    case SDL_BUTTON_X1:
      return MouseButton::kX1;
    case SDL_BUTTON_X2:
      return MouseButton::kX2;
    default:
      return MouseButton::kUnknown;
  }
}

MouseButtonEvent MakeMouseButtonEvent(uint8_t sdl_button, float x, float y,
                                      uint8_t clicks,
                                      float pixel_density) noexcept {
  return MouseButtonEvent{TranslateMouseButton(sdl_button), x * pixel_density,
                          y * pixel_density, clicks};
}

MouseMoveEvent MakeMouseMoveEvent(float x, float y, float xrel, float yrel,
                                  float pixel_density) noexcept {
  return MouseMoveEvent{x * pixel_density, y * pixel_density,
                        xrel * pixel_density, yrel * pixel_density};
}

MouseWheelEvent MakeMouseWheelEvent(float dx, float dy, float mouse_x,
                                    float mouse_y,
                                    float pixel_density) noexcept {
  return MouseWheelEvent{dx, dy, mouse_x * pixel_density,
                         mouse_y * pixel_density};
}

GamepadButton TranslateGamepadButton(uint8_t sdl_button) noexcept {
  if (static_cast<int>(sdl_button) <= SDL_GAMEPAD_BUTTON_DPAD_RIGHT) {
    return static_cast<GamepadButton>(sdl_button + 1);
  }
  return GamepadButton::kUnknown;
}

GamepadAxis TranslateGamepadAxis(uint8_t sdl_axis) noexcept {
  if (static_cast<int>(sdl_axis) <= SDL_GAMEPAD_AXIS_RIGHT_TRIGGER) {
    return static_cast<GamepadAxis>(sdl_axis + 1);
  }
  return GamepadAxis::kUnknown;
}

GamepadType TranslateGamepadType(int32_t sdl_type) noexcept {
  switch (sdl_type) {
    case SDL_GAMEPAD_TYPE_STANDARD:
      return GamepadType::kStandard;
    case SDL_GAMEPAD_TYPE_XBOX360:
    case SDL_GAMEPAD_TYPE_XBOXONE:
      return GamepadType::kXbox;
    case SDL_GAMEPAD_TYPE_PS3:
    case SDL_GAMEPAD_TYPE_PS4:
    case SDL_GAMEPAD_TYPE_PS5:
      return GamepadType::kPlayStation;
    case SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_PRO:
    case SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_LEFT:
    case SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_RIGHT:
    case SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_PAIR:
    case SDL_GAMEPAD_TYPE_GAMECUBE:
      return GamepadType::kNintendo;
    default:
      return GamepadType::kUnknown;
  }
}

float NormalizeGamepadAxis(int16_t value) noexcept {
  constexpr float kAxisMax = static_cast<float>(SDL_JOYSTICK_AXIS_MAX);
  // Aralık simetrik değil (−32768…32767); en uç negatif değer −1'i aşmasın.
  return std::max(-1.0F, static_cast<float>(value) / kAxisMax);
}

uint16_t RumbleIntensity(float intensity) noexcept {
  constexpr float kMax = 65535.0F;
  // `!(x > 0)` NaN'ı da yakalar; NaN'ın tam sayıya dönüşümü tanımsızdır.
  if (!(intensity > 0.0F)) {
    return 0;
  }
  if (intensity >= 1.0F) {
    return UINT16_MAX;
  }
  return static_cast<uint16_t>(intensity * kMax + 0.5F);
}

}  // namespace sdl_painter::internal
