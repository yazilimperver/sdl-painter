#pragma once

/// @file example_font.h
/// @brief Örneklerin metin çizebilmesi için sistemde bir TTF fontu arar.
///
/// Kütüphane font **dosyası paketlemez** — shader'lar binary'ye gömülür
/// (ADR-009) ama font bir kullanıcı varlığıdır: hangi fontun çizileceği
/// uygulamanın kararıdır, kütüphanenin değil. Örnekler de repoya font
/// koymamak için sistemdekini arar.
///
/// Font bulunamazsa örnek **metinsiz çalışmaya devam etmelidir**; çağıran
/// boş string'i bu şekilde ele almalıdır.
///
/// @note `examples/graphics/text.cpp` ve `examples/games/tictactoe.cpp` bu
///       yardımcıdan önce yazıldıkları için hâlâ kendi kopyalarını taşıyor.
///       Onları da buraya taşımak ayrı ve küçük bir temizlik işi.

#include <fstream>
#include <string>

namespace example {

/// @brief Sistemde mevcut bir TTF fontunun yolunu döndür; yoksa boş string.
inline std::string FindSystemFont() {
  const char* candidates[] = {
#ifdef _WIN32
      "C:/Windows/Fonts/arial.ttf",
      "C:/Windows/Fonts/calibri.ttf",
      "C:/Windows/Fonts/segoeui.ttf",
      "C:/Windows/Fonts/tahoma.ttf",
#elif defined(__APPLE__)
      "/System/Library/Fonts/Supplemental/Arial.ttf",
      "/Library/Fonts/Arial.ttf",
#else
      "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
      "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf",
      "/usr/share/fonts/truetype/freefont/FreeSans.ttf",
      "/usr/share/fonts/TTF/DejaVuSans.ttf",
#endif
  };
  for (const char* path : candidates) {
    if (std::ifstream(path).good()) {
      return path;
    }
  }
  return {};
}

}  // namespace example
