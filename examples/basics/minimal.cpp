/// @brief minimal — kopyala-çalıştır en küçük SDLPainter uygulaması.
///
/// Çalışan bir uygulamanın gerektirdiği asgari iskelet: SDL kurulumu, pencere,
/// Painter, olay döngüsü, çizim, yıkım. README'nin ilk kod bloğu bu dosyadan
/// gelir ve "projeme nasıl eklerim" sorusunun cevabı da budur.
///
/// Uygulama çatısını (`sdl_painter::app`) kullanan daha da kısa hâli için
/// bkz. `examples/app/app_basics.cpp`.

#include "sdl_painter/painter.h"

#include <SDL3/SDL.h>

int main() {
  SDL_Init(SDL_INIT_VIDEO);
  SDL_Window* window =
      SDL_CreateWindow("SDLPainter — minimal", 640, 480, SDL_WINDOW_OPENGL);

  // Painter kendi scope'unda: GL context pencereye bağlı olduğu için
  // SDL_DestroyWindow'dan ÖNCE yıkılmalı.
  {
    sdl_painter::Painter painter(window, sdl_painter::RendererBackend::kOpenGL);

    bool running = true;
    while (running) {
      SDL_Event event;
      while (SDL_PollEvent(&event)) {
        if (event.type == SDL_EVENT_QUIT) {
          running = false;
        }
      }

      painter.Begin();
      painter.Clear(sdl_painter::Color{30, 30, 40, 255});
      painter.SetBrush(
          sdl_painter::Brush(sdl_painter::Color{100, 160, 255, 255}));
      painter.FillCircle(320.0F, 240.0F, 80.0F);
      painter.End();
    }
  }

  SDL_DestroyWindow(window);
  SDL_Quit();
  return 0;
}
