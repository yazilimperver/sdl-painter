/// @brief hello_window — SDL penceresi açar; ESC veya pencere kapatma ile çıkar.
///
/// Geliştirme Fazı 0 demosu (eski ad: phase0_demo).
///
/// Bu demo altyapının derlendiğini doğrular; gerçek çizim primitives
/// örneğinde başlar.

#include <SDL3/SDL.h>

#include <spdlog/sinks/ansicolor_sink.h>
#include <spdlog/spdlog.h>

#ifdef _WIN32
#include <windows.h>
#endif

/// @brief Logger'ı renkli çıktı ve özel format ile başlatır.
///
/// Windows'ta ANSI escape kodları etkinleştirilir (Windows Terminal,
/// VS Code ve modern PowerShell destekler).
/// Format: [SS:DD:SS][Seviye] Mesaj
/// Renkler: I=yeşil, W=sarı, E=kırmızı, C=koyu kırmızı
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

  spdlog::info("SDLPainter hello_window demo starting...");

  if (!SDL_Init(SDL_INIT_VIDEO)) {
    spdlog::error("SDL_Init failed: {}", SDL_GetError());
    return 1;
  }
  spdlog::info("SDL initialized successfully.");

  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);

  SDL_Window* window = SDL_CreateWindow("SDLPainter — hello_window (Phase 0)",
                                        800, 600, SDL_WINDOW_OPENGL);
  if (!window) {
    spdlog::error("SDL_CreateWindow failed: {}", SDL_GetError());
    SDL_Quit();
    return 1;
  }
  spdlog::info("Window created (800x600).");
  spdlog::warn("This is a warn-level log sample.");
  spdlog::info("Close the window or press ESC to exit.");

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
  }

  spdlog::info("Shutting down.");
  SDL_DestroyWindow(window);
  SDL_Quit();
  return 0;
}
