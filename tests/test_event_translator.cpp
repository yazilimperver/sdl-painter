#include <SDL3/SDL.h>

#include <gtest/gtest.h>

#include "app/event_translator.h"

namespace sdl_painter {

// --- TranslateKey ---

TEST(EventTranslatorTest, LettersMapToKeys) {
  EXPECT_EQ(internal::TranslateKey(SDLK_A), Key::kA);
  EXPECT_EQ(internal::TranslateKey(SDLK_Z), Key::kZ);
  EXPECT_EQ(internal::TranslateKey(SDLK_M), Key::kM);
}

TEST(EventTranslatorTest, DigitsMapToKeys) {
  EXPECT_EQ(internal::TranslateKey(SDLK_0), Key::k0);
  EXPECT_EQ(internal::TranslateKey(SDLK_9), Key::k9);
  EXPECT_EQ(internal::TranslateKey(SDLK_5), Key::k5);
}

TEST(EventTranslatorTest, FunctionKeysMapToKeys) {
  EXPECT_EQ(internal::TranslateKey(SDLK_F1), Key::kF1);
  EXPECT_EQ(internal::TranslateKey(SDLK_F12), Key::kF12);
}

TEST(EventTranslatorTest, ArrowKeysMapToKeys) {
  EXPECT_EQ(internal::TranslateKey(SDLK_UP), Key::kUp);
  EXPECT_EQ(internal::TranslateKey(SDLK_DOWN), Key::kDown);
  EXPECT_EQ(internal::TranslateKey(SDLK_LEFT), Key::kLeft);
  EXPECT_EQ(internal::TranslateKey(SDLK_RIGHT), Key::kRight);
}

TEST(EventTranslatorTest, SpecialKeysMapToKeys) {
  EXPECT_EQ(internal::TranslateKey(SDLK_ESCAPE), Key::kEscape);
  EXPECT_EQ(internal::TranslateKey(SDLK_RETURN), Key::kEnter);
  EXPECT_EQ(internal::TranslateKey(SDLK_SPACE), Key::kSpace);
  EXPECT_EQ(internal::TranslateKey(SDLK_TAB), Key::kTab);
  EXPECT_EQ(internal::TranslateKey(SDLK_BACKSPACE), Key::kBackspace);
  EXPECT_EQ(internal::TranslateKey(SDLK_DELETE), Key::kDelete);
  EXPECT_EQ(internal::TranslateKey(SDLK_HOME), Key::kHome);
  EXPECT_EQ(internal::TranslateKey(SDLK_END), Key::kEnd);
  EXPECT_EQ(internal::TranslateKey(SDLK_PAGEUP), Key::kPageUp);
  EXPECT_EQ(internal::TranslateKey(SDLK_PAGEDOWN), Key::kPageDown);
}

TEST(EventTranslatorTest, ModifierKeysMapToKeys) {
  EXPECT_EQ(internal::TranslateKey(SDLK_LSHIFT), Key::kLeftShift);
  EXPECT_EQ(internal::TranslateKey(SDLK_RSHIFT), Key::kRightShift);
  EXPECT_EQ(internal::TranslateKey(SDLK_LCTRL), Key::kLeftCtrl);
  EXPECT_EQ(internal::TranslateKey(SDLK_RCTRL), Key::kRightCtrl);
  EXPECT_EQ(internal::TranslateKey(SDLK_LALT), Key::kLeftAlt);
  EXPECT_EQ(internal::TranslateKey(SDLK_RALT), Key::kRightAlt);
  EXPECT_EQ(internal::TranslateKey(SDLK_LGUI), Key::kLeftGui);
  EXPECT_EQ(internal::TranslateKey(SDLK_RGUI), Key::kRightGui);
}

TEST(EventTranslatorTest, UnknownKeycodeReturnsUnknown) {
  EXPECT_EQ(internal::TranslateKey(SDLK_KP_MEMSTORE), Key::kUnknown);
  EXPECT_EQ(internal::TranslateKey(0), Key::kUnknown);
}

// --- TranslateModifiers ---

TEST(EventTranslatorTest, NoModifierReturnsNone) {
  EXPECT_EQ(internal::TranslateModifiers(SDL_KMOD_NONE), KeyModifier::kNone);
}

TEST(EventTranslatorTest, SingleModifierTranslates) {
  EXPECT_EQ(internal::TranslateModifiers(SDL_KMOD_LSHIFT), KeyModifier::kShift);
  EXPECT_EQ(internal::TranslateModifiers(SDL_KMOD_RSHIFT), KeyModifier::kShift);
  EXPECT_EQ(internal::TranslateModifiers(SDL_KMOD_LCTRL), KeyModifier::kCtrl);
  EXPECT_EQ(internal::TranslateModifiers(SDL_KMOD_RCTRL), KeyModifier::kCtrl);
  EXPECT_EQ(internal::TranslateModifiers(SDL_KMOD_LALT), KeyModifier::kAlt);
  EXPECT_EQ(internal::TranslateModifiers(SDL_KMOD_RALT), KeyModifier::kAlt);
  EXPECT_EQ(internal::TranslateModifiers(SDL_KMOD_LGUI), KeyModifier::kGui);
  EXPECT_EQ(internal::TranslateModifiers(SDL_KMOD_RGUI), KeyModifier::kGui);
}

TEST(EventTranslatorTest, CombinedModifiersTranslate) {
  const KeyModifier mods =
      internal::TranslateModifiers(SDL_KMOD_LSHIFT | SDL_KMOD_LCTRL);
  EXPECT_EQ(mods, KeyModifier::kShift | KeyModifier::kCtrl);
  EXPECT_EQ(internal::TranslateModifiers(SDL_KMOD_RSHIFT | SDL_KMOD_LCTRL |
                                         SDL_KMOD_RALT | SDL_KMOD_LGUI),
            KeyModifier::kShift | KeyModifier::kCtrl | KeyModifier::kAlt |
                KeyModifier::kGui);
}

// --- TranslateMouseButton ---

TEST(EventTranslatorTest, MouseButtonsTranslate) {
  EXPECT_EQ(internal::TranslateMouseButton(SDL_BUTTON_LEFT),
            MouseButton::kLeft);
  EXPECT_EQ(internal::TranslateMouseButton(SDL_BUTTON_MIDDLE),
            MouseButton::kMiddle);
  EXPECT_EQ(internal::TranslateMouseButton(SDL_BUTTON_RIGHT),
            MouseButton::kRight);
  EXPECT_EQ(internal::TranslateMouseButton(SDL_BUTTON_X1), MouseButton::kX1);
  EXPECT_EQ(internal::TranslateMouseButton(SDL_BUTTON_X2), MouseButton::kX2);
}

TEST(EventTranslatorTest, UnknownMouseButtonReturnsUnknown) {
  EXPECT_EQ(internal::TranslateMouseButton(0), MouseButton::kUnknown);
  EXPECT_EQ(internal::TranslateMouseButton(99), MouseButton::kUnknown);
}

}  // namespace sdl_painter
