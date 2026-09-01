#pragma once

#include <cstdint>
#include <string_view>

/// @file version.h
/// @brief SDLPainter sürüm bilgisi — projenin tek sürüm kaynağı.
///
/// Sürüm numarası yalnızca bu dosyada tutulur. `CMakeLists.txt` ve
/// `conanfile.py` değeri buradan okur; başka hiçbir yerde tekrarlanmaz.
/// Sürüm yükseltirken dört sabiti birlikte güncelle — CMake, metin ile sayısal
/// bileşenlerin tutarlılığını configure aşamasında doğrular.
///
/// Bilinçli olarak önişlemci makrosu kullanılmaz; sabitler tip güvenlidir ve
/// `sdl_painter` namespace'i içindedir. Bunun bedeli, sürümün `#if` içinde
/// kullanılamamasıdır — tüketici tarafında sürüm kısıtı CMake üzerinden
/// ifade edilir:
///
/// @code
/// find_package(sdl_painter 1.1 CONFIG REQUIRED)
/// @endcode
///
/// Kod içinde kullanım:
///
/// @code
/// #include <sdl_painter/version.h>
///
/// spdlog::info("SDLPainter {}", sdl_painter::kVersionString);
/// static_assert(sdl_painter::kVersionMajor >= 1, "SDLPainter 1.0+ gerekli");
/// @endcode
namespace sdl_painter {

/// @brief Ana sürüm numarası. Geriye dönük uyumsuz değişikliklerde artar.
inline constexpr int32_t kVersionMajor = 1;

/// @brief Alt sürüm numarası. Geriye dönük uyumlu eklemelerde artar.
inline constexpr int32_t kVersionMinor = 3;

/// @brief Yama sürüm numarası. Yalnızca hata düzeltmelerinde artar.
inline constexpr int32_t kVersionPatch = 0;

/// @brief "MAJOR.MINOR.PATCH" biçiminde sürüm metni.
///
/// Sayısal bileşenlerle tutarlılığı CMake configure aşamasında ve
/// `tests/test_version.cpp` içinde doğrulanır.
inline constexpr std::string_view kVersionString = "1.3.0";

}  // namespace sdl_painter
