/// @brief Phase 5c demo — Vulkan backend ile texture çizimi.
///
/// DrawTextured (textured pipeline) ile iki farklı texture çizer:
///   1. Programatik olarak oluşturulan 256x256 dama tahtası deseni.
///   2. assets/ altından yüklenen PNG (varsa); yoksa ikinci dama çizilir.
///
/// Ayrıca Phase 5b'den gelen untextured çizim (FillRect, FillCircle) ve
/// transform stack (Translate + Rotate) da test edilir.
/// ESC veya pencere kapat ile sonlandırılır.

#include <SDL3/SDL.h>
#include <spdlog/sinks/ansicolor_sink.h>
#include <spdlog/spdlog.h>

#ifdef _WIN32
#include <windows.h>
#endif

#include <cmath>
#include <cstring>
#include <vector>

#include "sdl_painter/brush.h"
#include "sdl_painter/color.h"
#include "sdl_painter/geometry.h"
#include "sdl_painter/image.h"
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

/// @brief Programatik 256x256 RGBA dama tahtası oluşturur.
///
/// @param tile_size Her kare boyutu (piksel).
/// @param col_a Birinci renk.
/// @param col_b İkinci renk.
/// @return RGBA piksel verisi (256*256*4 byte).
std::vector<uint8_t> MakeCheckerboard(int32_t tile_size,
                                       sdl_painter::Color col_a,
                                       sdl_painter::Color col_b) {
  constexpr int32_t kSize = 256;
  std::vector<uint8_t> data(static_cast<size_t>(kSize * kSize * 4));
  for (int32_t y = 0; y < kSize; ++y) {
    for (int32_t x = 0; x < kSize; ++x) {
      const bool is_a = ((x / tile_size) + (y / tile_size)) % 2 == 0;
      const sdl_painter::Color& c = is_a ? col_a : col_b;
      const size_t idx = static_cast<size_t>((y * kSize + x) * 4);
      data[idx + 0] = c.r;
      data[idx + 1] = c.g;
      data[idx + 2] = c.b;
      data[idx + 3] = c.a;
    }
  }
  return data;
}

}  // namespace

int main() {
  InitLogger();
  spdlog::info("SDLPainter Phase 5c (Vulkan textured) starting...");

  if (!SDL_Init(SDL_INIT_VIDEO)) {
    spdlog::error("SDL_Init failed: {}", SDL_GetError());
    return 1;
  }

  SDL_Window* window = SDL_CreateWindow(
      "SDLPainter — Phase 5c (Vulkan Textured)", 800, 600,
      SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);
  if (!window) {
    spdlog::error("SDL_CreateWindow failed: {}", SDL_GetError());
    SDL_Quit();
    return 1;
  }

  {  // Painter scope: window'dan önce destroy edilmeli (surface leak önleme).
    sdl_painter::Painter painter(window, sdl_painter::RendererBackend::kVulkan);
    if (!painter.IsValid()) {
      spdlog::error("Vulkan painter initialization failed.");
      SDL_DestroyWindow(window);
      SDL_Quit();
      return 1;
    }

    const sdl_painter::Color kBlack{10, 10, 10, 255};
    const sdl_painter::Color kWhite{240, 240, 240, 255};
    auto checker1_data = MakeCheckerboard(32, kBlack, kWhite);
    sdl_painter::Image checker1 =
        sdl_painter::Image::CreateFromData(checker1_data.data(), 256, 256, 4);

    const sdl_painter::Color kMauve{203, 166, 247, 255};
    const sdl_painter::Color kPeach{250, 179, 135, 255};
    auto checker2_data = MakeCheckerboard(16, kMauve, kPeach);
    sdl_painter::Image checker2 =
        sdl_painter::Image::CreateFromData(checker2_data.data(), 256, 256, 4);

    const sdl_painter::Color kBg{30, 30, 46, 255};
    const sdl_painter::Color kRed{235, 66, 66, 255};
    const sdl_painter::Color kBlue{137, 180, 250, 255};

    float angle = 0.0f;

    spdlog::info("Close the window or press ESC to exit.");

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

      angle += 0.4f;
      if (angle >= 360.0f) angle -= 360.0f;

      painter.Begin();
      painter.Clear(kBg);

      painter.SetBrush(sdl_painter::Brush(kRed));
      painter.SetPen(sdl_painter::Pen(kRed, 0.0f));
      painter.FillRect(20.0f, 20.0f, 120.0f, 80.0f);

      painter.DrawImage(checker1,
                        sdl_painter::Rect{180.0f, 20.0f, 256.0f, 256.0f});

      painter.SetOpacity(0.7f);
      painter.DrawImage(checker2,
                        sdl_painter::Rect{460.0f, 20.0f, 256.0f, 256.0f});
      painter.SetOpacity(1.0f);

      painter.Save();
      painter.Translate(120.0f, 420.0f);
      painter.Rotate(angle);
      painter.SetBrush(sdl_painter::Brush(kBlue));
      painter.SetPen(sdl_painter::Pen(kBlue, 0.0f));
      painter.FillRect(-60.0f, -40.0f, 120.0f, 80.0f);
      painter.Restore();

      painter.Save();
      painter.Translate(400.0f, 420.0f);
      painter.Rotate(-angle * 0.5f);
      painter.DrawImage(checker1,
                        sdl_painter::Rect{-80.0f, -80.0f, 160.0f, 160.0f});
      painter.Restore();

      painter.DrawImage(checker2,
                        sdl_painter::Rect{600.0f, 320.0f, 180.0f, 180.0f});

      painter.End();
    }
  }  // painter destructor burada çalışır — VkSurface henüz geçerli.

  spdlog::info("Shutting down.");
  SDL_DestroyWindow(window);
  SDL_Quit();
  return 0;
}
