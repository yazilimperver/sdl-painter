#pragma once

#include <SDL3/SDL.h>

#include <string>

namespace sdl_painter::testing {

/// @brief Sistemde kullanılabilir bir TTF fontu arar.
/// @return Bulunan fontun yolu; hiçbiri yoksa boş string.
///
/// Metin yolunu (Font / Painter::DrawText) doğrulayan testler gerçek bir
/// font dosyasına ihtiyaç duyar. Repoda font tutulmadığı için sistem
/// fontlarına başvurulur; bulunamazsa test `GTEST_SKIP()` ile atlanır.
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
