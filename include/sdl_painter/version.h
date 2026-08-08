#pragma once

/// @file version.h
/// @brief SDLPainter sürüm bilgisi — projenin **tek** sürüm kaynağı.
///
/// Sürüm numarası yalnızca bu dosyada tutulur. `CMakeLists.txt` ve
/// `conanfile.py` değeri buradan okur; başka hiçbir yerde tekrarlanmaz.
/// Sürüm yükseltirken aşağıdaki üç makroyu ve @ref SDLPAINTER_VERSION_STRING
/// değerini birlikte güncelle — CMake ikisinin tutarlılığını configure
/// aşamasında doğrular.
///
/// Tüketici tarafında kullanım:
/// @code
/// #include <sdl_painter/version.h>
///
/// #if SDLPAINTER_VERSION_MAJOR < 2
/// // 1.x API'si
/// #endif
/// @endcode

/// @brief Ana sürüm numarası. Geriye dönük uyumsuz değişikliklerde artar.
#define SDLPAINTER_VERSION_MAJOR 1

/// @brief Alt sürüm numarası. Geriye dönük uyumlu eklemelerde artar.
#define SDLPAINTER_VERSION_MINOR 1

/// @brief Yama sürüm numarası. Yalnızca hata düzeltmelerinde artar.
#define SDLPAINTER_VERSION_PATCH 0

/// @brief "MAJOR.MINOR.PATCH" biçiminde sürüm metni.
#define SDLPAINTER_VERSION_STRING "1.1.0"
