#pragma once

#include <SDL3/SDL.h>

#include <cstdlib>
#include <string>

namespace sdl_painter::testing {

/// @brief Font bulunamadığında test atlanmak yerine başarısız mı olmalı?
///
/// Metin testleri sistemde bir TTF fontu yoksa `GTEST_SKIP()` eder. Bu,
/// geliştiricinin çıplak makinesinde makul; **CI'da ise tehlikeli**: testler
/// yeşil görünürken hiç koşmaz. Nitekim ilk kapsama ölçümünde
/// `glyph_atlas.cpp` %0 çıktı — CI imajında font paketi yoktu ve 28 test
/// sessizce atlanıyordu.
///
/// `SDLPAINTER_REQUIRE_FONT=1` ayarlandığında (CI bunu yapar) font yokluğu
/// artık atlama değil **hata**dır; sorun ilk çalıştırmada görünür olur.
inline bool FontIsRequired() {
  const char* env = std::getenv("SDLPAINTER_REQUIRE_FONT");
  return env != nullptr && env[0] != '\0' && env[0] != '0';
}

/// @brief Sistemde kullanılabilir bir TTF fontu arar.
/// @return Bulunan fontun yolu; hiçbiri yoksa boş string.
inline std::string FindSystemFont() {
  const char* kCandidates[] = {
#ifdef _WIN32
      "C:/Windows/Fonts/arial.ttf",
      "C:/Windows/Fonts/tahoma.ttf",
      "C:/Windows/Fonts/segoeui.ttf",
      "C:/Windows/Fonts/calibri.ttf",
#else
      "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
      "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf",
      "/usr/share/fonts/truetype/freefont/FreeSans.ttf",
      "/usr/share/fonts/TTF/DejaVuSans.ttf",
      "/usr/share/fonts/dejavu/DejaVuSans.ttf",
#endif
  };
  for (const char* path : kCandidates) {
    SDL_IOStream* io = SDL_IOFromFile(path, "rb");
    if (io != nullptr) {
      SDL_CloseIO(io);
      return path;
    }
  }
  return {};
}

}  // namespace sdl_painter::testing

/// @brief Font gerektiren testin başına konur: yolu `var` içine alır.
///
/// Font yoksa `SDLPAINTER_REQUIRE_FONT` ayarlıysa testi **düşürür**, değilse
/// atlar. Böylece CI'da sessiz atlama olmaz, yerelde geliştirici engellenmez.
#define SDLPAINTER_REQUIRE_FONT_OR_SKIP(var)                               \
  const std::string var = sdl_painter::testing::FindSystemFont();          \
  if ((var).empty()) {                                                     \
    if (sdl_painter::testing::FontIsRequired()) {                          \
      FAIL() << "Sistemde TTF font bulunamadı, ancak "                     \
                "SDLPAINTER_REQUIRE_FONT ayarlı. Metin testleri sessizce " \
                "atlanamaz — ortama bir font paketi kurun "                \
                "(örn. fonts-dejavu-core).";                               \
    }                                                                      \
    GTEST_SKIP() << "Sistemde TTF font bulunamadı.";                       \
  }                                                                        \
  static_assert(true, "")
