#include "sdl_painter/version.h"

#include <gtest/gtest.h>
#include <string>

namespace {

/// kVersionString ile sayisal sabitler ayrisamaz. Ayni kontrol CMake configure
/// ve conanfile.py set_version asamasinda da yapilir; bu test derlenmis
/// kutuphanenin icinden ucuncu bir ag gerer.
TEST(VersionTest, StringMatchesNumericComponents) {
  const std::string expected = std::to_string(sdl_painter::kVersionMajor) +
                               "." +
                               std::to_string(sdl_painter::kVersionMinor) +
                               "." + std::to_string(sdl_painter::kVersionPatch);
  EXPECT_EQ(expected, std::string(sdl_painter::kVersionString));
}

/// Sabitler sabit ifadede kullanilabilmeli — surum kontrolunun makro olmadan
/// da derleme zamaninda ifade edilebildigini gosterir.
TEST(VersionTest, ConstantsUsableInStaticAssert) {
  static_assert(sdl_painter::kVersionMajor >= 1,
                "SDLPainter 1.0 veya uzeri bekleniyor");
  static_assert(sdl_painter::kVersionMinor >= 0, "Alt surum negatif olamaz");
  static_assert(!sdl_painter::kVersionString.empty(), "Surum metni bos olamaz");
  SUCCEED();
}

}  // namespace
