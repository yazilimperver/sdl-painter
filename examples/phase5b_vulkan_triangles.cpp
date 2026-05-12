/// @brief Phase 5b demo — Vulkan backend ile untextured çizim.
///
/// DrawTriangles, FillRect, FillCircle, FillPolygon üzerinden renkli şekiller
/// çizer. Transform stack (translate + rotate) ve SetOpacity da test edilir.
/// ESC veya pencere kapat ile sonlandırılır.

#include <SDL3/SDL.h>
#include <spdlog/sinks/ansicolor_sink.h>
#include <spdlog/spdlog.h>

#ifdef _WIN32
#include <windows.h>
#endif

#include <cmath>
#include <vector>

#include "sdl_painter/brush.h"
#include "sdl_painter/color.h"
#include "sdl_painter/geometry.h"
#include "sdl_painter/painter.h"
#include "sdl_painter/pen.h"
#include "sdl_painter/renderer.h"

namespace {

/// @brief Logger'ı renkli çıktı ile başlatır.
void InitLogger() {
#ifdef _WIN32
  HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
  DWORD dwMode = 0;
  GetConsoleMode(hOut, &dwMode);
  SetConsoleMode(hOut, dwMode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
#endif

  auto sink = std::make_shared<spdlog::sinks::ansicolor_stdout_sink_mt>();
  sink->set_pattern("[%H:%M:%S][%L] %v");
  auto logger = std::make_shared<spdlog::logger>("sdlpainter", sink);
  logger->set_level(spdlog::level::debug);
  spdlog::set_default_logger(logger);
}

}  // namespace

int main() {
  InitLogger();
  spdlog::info("SDLPainter Phase 5b (Vulkan triangles) starting...");

  if (!SDL_Init(SDL_INIT_VIDEO)) {
    spdlog::error("SDL_Init failed: {}", SDL_GetError());
    return 1;
  }

  SDL_Window* window = SDL_CreateWindow(
      "SDLPainter — Phase 5b (Vulkan Untextured)", 800, 600,
      SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);
  if (!window) {
    spdlog::error("SDL_CreateWindow failed: {}", SDL_GetError());
    SDL_Quit();
    return 1;
  }

  {  // Painter scope: window'dan önce destroy edilmeli (surface leak önleme).
    sdl_painter::Painter painter(window,
                                 sdl_painter::RendererBackend::kVulkan);
    if (!painter.IsValid()) {
      spdlog::error("Vulkan painter initialization failed.");
      SDL_DestroyWindow(window);
      SDL_Quit();
      return 1;
    }

    spdlog::info("Close the window or press ESC to exit.");

    const sdl_painter::Color kBg{30, 30, 46, 255};
    const sdl_painter::Color kRed{235, 66, 66, 255};
    const sdl_painter::Color kGreen{166, 227, 161, 255};
    const sdl_painter::Color kBlue{137, 180, 250, 255};
    const sdl_painter::Color kYellow{249, 226, 175, 255};
    const sdl_painter::Color kPeach{250, 179, 135, 180};

    float angle = 0.0f;

    bool running = true;
    while (running) {
      SDL_Event event;
      while (SDL_PollEvent(&event)) {
        if (event.type == SDL_EVENT_QUIT) running = false;
        if (event.type == SDL_EVENT_KEY_DOWN &&
            event.key.key == SDLK_ESCAPE) {
          running = false;
        }
      }

      angle += 0.5f;
      if (angle >= 360.0f) angle -= 360.0f;

      painter.Begin();
      painter.Clear(kBg);

      painter.SetBrush(sdl_painter::Brush(kRed));
      painter.SetPen(sdl_painter::Pen(kRed, 0.0f));
      painter.FillRect(50.0f, 50.0f, 160.0f, 100.0f);

      painter.SetBrush(sdl_painter::Brush(kGreen));
      painter.SetPen(sdl_painter::Pen(kGreen, 0.0f));
      painter.FillCircle(400.0f, 300.0f, 80.0f);

      painter.Save();
      painter.Translate(600.0f, 150.0f);
      painter.Rotate(angle);
      painter.SetBrush(sdl_painter::Brush(kBlue));
      painter.SetPen(sdl_painter::Pen(kBlue, 0.0f));
      painter.FillRect(-50.0f, -40.0f, 100.0f, 80.0f);
      painter.Restore();

      painter.SetOpacity(0.7f);
      painter.SetBrush(sdl_painter::Brush(kPeach));
      painter.SetPen(sdl_painter::Pen(kPeach, 0.0f));
      {
        std::vector<sdl_painter::Point> hex;
        constexpr float kR = 60.0f;
        constexpr float kCx = 130.0f;
        constexpr float kCy = 430.0f;
        for (int i = 0; i < 6; ++i) {
          const float theta =
              static_cast<float>(i) * (2.0f * 3.14159265f / 6.0f);
          hex.push_back({kCx + kR * std::cos(theta),
                         kCy + kR * std::sin(theta)});
        }
        painter.FillPolygon(hex);
      }
      painter.SetOpacity(1.0f);

      painter.SetBrush(sdl_painter::Brush(kYellow));
      painter.SetPen(sdl_painter::Pen(kYellow, 0.0f));
      for (int i = 0; i < 4; ++i) {
        const float x = 560.0f + static_cast<float>(i) * 55.0f;
        painter.FillRect(x, 470.0f, 40.0f, 40.0f);
      }

      painter.End();
    }
  }  // painter destructor burada çalışır — VkSurface henüz geçerli.

  spdlog::info("Shutting down.");
  SDL_DestroyWindow(window);
  SDL_Quit();
  return 0;
}
