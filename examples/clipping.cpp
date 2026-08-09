/// @brief clipping — Merkez rotasyon dogrulama testi.
///
/// Gelistirme Fazi 2b demosu (eski ad: phase2b_demo).
///
/// Pencere boyutu degisse de dikdortgen daima ekranin tam ortasinda
/// kendi merkezi etrafinda doner. Referans olarak ekran merkezinde kucuk
/// bir arti isareti cizilir; dikdortgenin merkezi bu artiyla cakismalidir.
///
/// ESC veya pencere kapat cikmak icin.

#include "sdl_painter/brush.h"
#include "sdl_painter/color.h"
#include "sdl_painter/painter.h"
#include "sdl_painter/pen.h"

#include <SDL3/SDL.h>

#include <spdlog/sinks/ansicolor_sink.h>
#include <spdlog/spdlog.h>

#ifdef _WIN32
#include <windows.h>
#endif

static void InitLogger() {
#ifdef _WIN32
  HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
  DWORD dwMode = 0;
  GetConsoleMode(hOut, &dwMode);
  SetConsoleMode(hOut, dwMode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
#endif
  auto sink = std::make_shared<spdlog::sinks::ansicolor_stdout_sink_mt>();
  sink->set_pattern("[%H:%M:%S][%L] %v");
  auto logger = std::make_shared<spdlog::logger>("sdlpainter", sink);
  logger->set_level(spdlog::level::trace);
  spdlog::set_default_logger(logger);
}

int main() {
  InitLogger();
  spdlog::info("SDLPainter clipping demo baslatiliyor...");

  if (!SDL_Init(SDL_INIT_VIDEO)) {
    spdlog::error("SDL_Init basarisiz: {}", SDL_GetError());
    return 1;
  }

  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
  SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1);
  SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 4);

  SDL_Window* window =
      SDL_CreateWindow("SDLPainter — clipping: Merkez Rotasyon (Phase 2b)", 800, 600,
                       SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
  if (!window) {
    spdlog::error("SDL_CreateWindow basarisiz: {}", SDL_GetError());
    SDL_Quit();
    return 1;
  }

  {
    sdl_painter::Painter painter(window, sdl_painter::RendererBackend::kOpenGL);
    if (!painter.IsValid()) {
      spdlog::error("Painter baslatilamadi.");
      SDL_DestroyWindow(window);
      SDL_Quit();
      return 1;
    }
    spdlog::info("Painter hazir. ESC veya pencere kapat sonra cikis.");

    float angle = 0.0f;

    bool running = true;
    while (running) {
      SDL_Event event;
      while (SDL_PollEvent(&event)) {
        if (event.type == SDL_EVENT_QUIT)
          running = false;
        if (event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_ESCAPE) {
          running = false;
        }
      }

      angle += 0.5f;
      if (angle >= 360.0f)
        angle -= 360.0f;

      // Pencere boyutunu her karede oku — boyut degisirse merkez de tasinir.
      int win_w = 0, win_h = 0;
      SDL_GetWindowSize(window, &win_w, &win_h);
      const float cx = win_w * 0.5f;
      const float cy = win_h * 0.5f;

      painter.Begin();
      painter.Clear(sdl_painter::Color{20, 20, 30, 255});

      // Ekran merkezini gosteren arti isareti (referans).
      painter.SetPen(
          sdl_painter::Pen(sdl_painter::Color{200, 200, 200, 200}, 1.0f));
      painter.DrawLine(cx - 15.0f, cy, cx + 15.0f, cy);
      painter.DrawLine(cx, cy - 15.0f, cx, cy + 15.0f);

      // Merkez etrafinda donen dikdortgen.
      painter.Save();
      painter.Translate(cx, cy);
      painter.Rotate(angle);
      painter.SetBrush(
          sdl_painter::Brush(sdl_painter::Color{255, 180, 60, 220}));
      painter.SetPen(
          sdl_painter::Pen(sdl_painter::Color{255, 230, 140, 255}, 2.0f));
      painter.FillRect(-100.0f, -50.0f, 200.0f, 100.0f);
      painter.DrawRect(-100.0f, -50.0f, 200.0f, 100.0f);
      painter.Restore();

      painter.End();
    }
  }

  spdlog::info("Demo kapatiliyor.");
  SDL_DestroyWindow(window);
  SDL_Quit();
  return 0;
}
