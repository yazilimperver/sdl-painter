// SDLPainter'i disaridan tuketen minimal program.
//
// Neyi kanitlar:
//   1. Public header'lar kurulum/build agacindan bulunabiliyor
//   2. Kutuphane link ediliyor ve derlenmis sembol cozuluyor (CreateRenderer)
//   3. Surum bilgisi tuketiciye ulasiyor
//
// Pencere acmaz, GPU istemez.

#include <sdl_painter/color.h>
#include <sdl_painter/geometry.h>
#include <sdl_painter/pen.h>
#include <sdl_painter/renderer.h>
#include <sdl_painter/version.h>

#include <cstdio>
#include <string>

int main() {
  std::printf("SDLPainter surumu: %s (major=%d)\n",
              std::string(sdl_painter::kVersionString).c_str(),
              static_cast<int>(sdl_painter::kVersionMajor));

  const sdl_painter::Pen pen(sdl_painter::Color::Red(), 2.0F);
  const sdl_painter::Rect rect{0.0F, 0.0F, 100.0F, 50.0F};
  std::printf("Pen genisligi: %.1f, Rect: %.0fx%.0f (sag=%.0f)\n",
              pen.GetWidth(), rect.w, rect.h, rect.Right());

  // Derlenmis sembol — basarili link olmadan cozulmez.
  auto renderer =
      sdl_painter::CreateRenderer(sdl_painter::RendererBackend::kOpenGL);
  if (renderer == nullptr) {
    std::printf("HATA: CreateRenderer null dondu\n");
    return 1;
  }
  std::printf("CreateRenderer(kOpenGL): OK\n");

  static_assert(sdl_painter::kVersionMajor >= 1, "SDLPainter 1.0+ gerekli");
  return 0;
}
