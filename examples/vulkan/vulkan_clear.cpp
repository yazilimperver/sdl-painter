/// @brief vulkan_clear — Vulkan backend ile pencere temizleme.
///
/// Geliştirme Fazı 5a demosu (eski ad: phase5a_vulkan_clear).
///
/// SDL_WINDOW_VULKAN bayrağıyla pencere açar, Painter'ı Vulkan backend ile
/// oluşturur ve her frame'de pencereyi CornflowerBlue renge boyar.
/// ESC veya pencere kapat ile sonlandırılır.

#include <SDL3/SDL.h>
#include <spdlog/sinks/ansicolor_sink.h>
#include <spdlog/spdlog.h>

#ifdef _WIN32
#include <windows.h>
#endif

#include "sdl_painter/color.h"
#include "sdl_painter/painter.h"
#include "sdl_painter/renderer.h"

namespace {

/// @brief Logger'ı renkli çıktı ve özel format ile başlatır.
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
  spdlog::info("SDLPainter vulkan_clear demo starting...");

  if (!SDL_Init(SDL_INIT_VIDEO)) {
    spdlog::error("SDL_Init failed: {}", SDL_GetError());
    return 1;
  }

  SDL_Window* window = SDL_CreateWindow(
      "SDLPainter — vulkan_clear (Phase 5a)", 800, 600,
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

    // CornflowerBlue (100, 149, 237)
    const sdl_painter::Color kClearColor{100, 149, 237, 255};

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

      painter.Begin();
      painter.Clear(kClearColor);
      painter.End();
    }
  }  // painter destructor burada çalışır — VkSurface henüz geçerli.

  spdlog::info("Shutting down.");
  SDL_DestroyWindow(window);
  SDL_Quit();
  return 0;
}
