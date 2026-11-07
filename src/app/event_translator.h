#pragma once

#include "sdl_painter/app/events.h"

#include <cstdint>

namespace sdl_painter::internal {

/// @brief SDL keycode değerini @ref Key değerine çevir.
/// @param sdl_keycode SDL_Keycode (SDL_KeyboardEvent::key).
/// @return Eşleşme yoksa @ref Key::kUnknown.
/// @note Parametre düz tam sayıdır — başlık SDL'den bağımsız kalsın diye
///       (birim testleri pencere/context olmadan çalışabilsin).
Key TranslateKey(uint32_t sdl_keycode) noexcept;

/// @brief SDL keymod maskesini @ref KeyModifier maskesine çevir.
/// @param sdl_mod SDL_Keymod (SDL_KeyboardEvent::mod).
KeyModifier TranslateModifiers(uint16_t sdl_mod) noexcept;

/// @brief SDL fare düğmesi indeksini @ref MouseButton değerine çevir.
/// @param sdl_button SDL_MouseButtonEvent::button (1=sol, 2=orta, 3=sağ, ...).
MouseButton TranslateMouseButton(uint8_t sdl_button) noexcept;

/// @brief SDL fare düğmesi olayının alanlarından @ref MouseButtonEvent kur.
/// @param pixel_density Pencerenin piksel yoğunluğu; mantıksal konum bununla
///        çarpılarak çizim (piksel) koordinatına çevrilir.
MouseButtonEvent MakeMouseButtonEvent(uint8_t sdl_button, float x, float y,
                                      uint8_t clicks,
                                      float pixel_density) noexcept;

/// @brief SDL fare hareketi olayının alanlarından @ref MouseMoveEvent kur.
///
/// Konum ve hareket farkı piksel yoğunluğuyla çarpılır.
MouseMoveEvent MakeMouseMoveEvent(float x, float y, float xrel, float yrel,
                                  float pixel_density) noexcept;

/// @brief SDL tekerlek olayının alanlarından @ref MouseWheelEvent kur.
///
/// Fare konumu piksel yoğunluğuyla çarpılır; `dx`/`dy` tekerlek adımıdır,
/// konum değil, ölçeklenmez.
MouseWheelEvent MakeMouseWheelEvent(float dx, float dy, float mouse_x,
                                    float mouse_y,
                                    float pixel_density) noexcept;

/// @brief SDL gamepad düğmesini @ref GamepadButton değerine çevir.
/// @param sdl_button SDL_GamepadButtonEvent::button (SDL_GamepadButton).
/// @return İlk on beş düğmenin dışındakiler için @ref GamepadButton::kUnknown.
GamepadButton TranslateGamepadButton(uint8_t sdl_button) noexcept;

/// @brief SDL gamepad eksenini @ref GamepadAxis değerine çevir.
/// @param sdl_axis SDL_GamepadAxisEvent::axis (SDL_GamepadAxis).
GamepadAxis TranslateGamepadAxis(uint8_t sdl_axis) noexcept;

/// @brief SDL gamepad türünü @ref GamepadType ailesine indir.
/// @param sdl_type SDL_GamepadType değeri.
GamepadType TranslateGamepadType(int32_t sdl_type) noexcept;

/// @brief Ham eksen değerini (−32768…32767) [-1, 1] aralığına çevir.
///
/// 32767'ye bölünür ve −1'de kırpılır; tetikler (0…32767) böylece [0, 1]
/// olur.
float NormalizeGamepadAxis(int16_t value) noexcept;

/// @brief [0, 1] titreşim şiddetini SDL'in 0…0xFFFF aralığına çevir.
///
/// Aralık dışı değerler kırpılır; NaN 0 sayılır.
uint16_t RumbleIntensity(float intensity) noexcept;

}  // namespace sdl_painter::internal
