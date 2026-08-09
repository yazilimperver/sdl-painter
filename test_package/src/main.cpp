// Paketin tuketilebilirligini dogrular. Pencere acmaz, GPU istemez.

#include "sdl_painter/brush.h"
#include "sdl_painter/color.h"
#include "sdl_painter/pen.h"
#include "sdl_painter/renderer.h"
#include "sdl_painter/version.h"

#include <cstdio>

int main() {
  const sdl_painter::Color kRed{255, 0, 0, 255};
  const sdl_painter::Pen kPen(kRed, 2.0F);
  const sdl_painter::Brush kBrush(kRed);

  std::printf("sdl_painter %.*s, pen=%.1f, brush alpha=%u\n",
              static_cast<int>(sdl_painter::kVersionString.size()),
              sdl_painter::kVersionString.data(), kPen.GetWidth(),
              static_cast<unsigned>(kBrush.GetColor().a));

  // Derlenmis sembol — yalnizca header degil, kutuphane de link edilmis olmali.
  auto renderer =
      sdl_painter::CreateRenderer(sdl_painter::RendererBackend::kOpenGL);
  if (renderer == nullptr) {
    std::printf("HATA: CreateRenderer null dondu\n");
    return 1;
  }

  std::printf("test_package OK\n");
  return 0;
}
