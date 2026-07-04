#include "app/event_translator.h"

#include <SDL3/SDL.h>

namespace sdl_painter::internal {

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

}  // namespace sdl_painter::internal
