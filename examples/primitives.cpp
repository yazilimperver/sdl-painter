/// @brief primitives — OpenGL backend ile tüm temel primitifleri çizer.
///
/// Geliştirme Fazı 1 demosu (eski ad: phase1_demo).
///
/// Pencerede gösterilen şekiller:
///   - Dolu dikdörtgenler, daire ve elips
///   - Çerçeve dikdörtgen, daire ve elips
///   - Kalın çizgi ve polyline
///   - Konkav poligon (dolu, ear clipping)
///   - Transform: döndürülmüş dikdörtgen
///
/// ESC veya pencere kapat → çıkış.

#include "sdl_painter/brush.h"
#include "sdl_painter/color.h"
#include "sdl_painter/geometry.h"
#include "sdl_painter/painter.h"
#include "sdl_painter/pen.h"

#include <SDL3/SDL.h>

#include <spdlog/sinks/ansicolor_sink.h>
#include <spdlog/spdlog.h>

#ifdef _WIN32
#include <windows.h>
#endif

/// @brief Logger'ı renkli çıktı ile başlatır.
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
  spdlog::info("SDLPainter primitives demo baslatiliyor...");

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
      SDL_CreateWindow("SDLPainter — primitives (Phase 1)", 900, 700,
                       SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
  if (!window) {
    spdlog::error("SDL_CreateWindow basarisiz: {}", SDL_GetError());
    SDL_Quit();
    return 1;
  }

  // Painter'ı ayrı bir scope'a al; SDL_DestroyWindow'dan ÖNCE yok edilmeli.
  // GL context pencereye bağlı — pencere önce silinirse glDelete* 1282 verir.
  {
    sdl_painter::Painter painter(window, sdl_painter::RendererBackend::kOpenGL);
    if (!painter.IsValid()) {
      spdlog::error("Painter baslatilamadi.");
      SDL_DestroyWindow(window);
      SDL_Quit();
      return 1;
    }
    spdlog::info("Painter hazir. ESC veya pencere kapat sonra cikis.");

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

      painter.Begin();
      painter.Clear(sdl_painter::Color{25, 25, 35, 255});

      // --- Dolu dikdörtgenler ---
      painter.SetBrush(
          sdl_painter::Brush(sdl_painter::Color{60, 120, 200, 200}));
      painter.FillRect(30.0f, 70.0f, 180.0f, 100.0f);

      painter.SetBrush(
          sdl_painter::Brush(sdl_painter::Color{200, 80, 60, 200}));
      painter.FillRect(230.0f, 70.0f, 180.0f, 100.0f);

      // --- Çerçeve dikdörtgen ---
      painter.SetPen(
          sdl_painter::Pen(sdl_painter::Color{255, 220, 50, 255}, 3.0f));
      painter.DrawRect(430.0f, 70.0f, 180.0f, 100.0f);

      // --- Dolu daire ---
      painter.SetBrush(
          sdl_painter::Brush(sdl_painter::Color{50, 200, 120, 220}));
      painter.FillCircle(100.0f, 260.0f, 70.0f);

      // --- Çerçeve daire ---
      painter.SetPen(
          sdl_painter::Pen(sdl_painter::Color{255, 180, 50, 255}, 4.0f));
      painter.DrawCircle(280.0f, 260.0f, 70.0f);

      // --- Dolu elips ---
      painter.SetBrush(
          sdl_painter::Brush(sdl_painter::Color{180, 80, 220, 200}));
      painter.FillEllipse(480.0f, 260.0f, 110.0f, 55.0f);

      // --- Çerçeve elips ---
      painter.SetPen(
          sdl_painter::Pen(sdl_painter::Color{100, 220, 255, 255}, 3.0f));
      painter.DrawEllipse(700.0f, 260.0f, 90.0f, 50.0f);

      // --- Kalın çizgiler ---
      painter.SetPen(
          sdl_painter::Pen(sdl_painter::Color{255, 255, 255, 200}, 5.0f));
      painter.DrawLine(30.0f, 370.0f, 250.0f, 430.0f);
      painter.DrawLine(30.0f, 430.0f, 250.0f, 370.0f);

      // --- Polyline ---
      painter.SetPen(
          sdl_painter::Pen(sdl_painter::Color{255, 140, 60, 255}, 4.0f));
      painter.DrawPolyline({
          {300.0f, 380.0f},
          {360.0f, 350.0f},
          {420.0f, 400.0f},
          {480.0f, 360.0f},
          {540.0f, 420.0f},
      });

      // --- Dolu poligon (concave — L şekli) ---
      painter.SetBrush(
          sdl_painter::Brush(sdl_painter::Color{200, 180, 50, 210}));
      painter.FillPolygon({
          {600.0f, 350.0f},
          {750.0f, 350.0f},
          {750.0f, 420.0f},
          {680.0f, 420.0f},
          {680.0f, 480.0f},
          {600.0f, 480.0f},
      });

      // --- Çerçeve poligon ---
      painter.SetPen(
          sdl_painter::Pen(sdl_painter::Color{150, 230, 150, 255}, 2.5f));
      painter.DrawPolygon({
          {600.0f, 350.0f},
          {750.0f, 350.0f},
          {750.0f, 420.0f},
          {680.0f, 420.0f},
          {680.0f, 480.0f},
          {600.0f, 480.0f},
      });

      // --- Transform: döndürülmüş dikdörtgen ---
      painter.Save();
      painter.Translate(150.0f, 580.0f);
      painter.Rotate(30.0f);
      painter.SetBrush(
          sdl_painter::Brush(sdl_painter::Color{80, 180, 255, 180}));
      painter.FillRect(-60.0f, -35.0f, 120.0f, 70.0f);
      painter.SetPen(
          sdl_painter::Pen(sdl_painter::Color{200, 240, 255, 255}, 2.0f));
      painter.DrawRect(-60.0f, -35.0f, 120.0f, 70.0f);
      painter.Restore();

      // --- Transform: ölçeklenmiş daire ---
      painter.Save();
      painter.Translate(400.0f, 580.0f);
      painter.Scale(1.5f, 0.6f);
      painter.SetBrush(
          sdl_painter::Brush(sdl_painter::Color{255, 100, 150, 200}));
      painter.FillCircle(0.0f, 0.0f, 50.0f);
      painter.Restore();

      // --- Çok kenarlı düzenli poligon (5-gen) ---
      {
        constexpr float kPi = 3.14159265358979323846f;
        std::vector<sdl_painter::Point> pentagon;
        for (int32_t i = 0; i < 5; ++i) {
          float a = 2.0f * kPi * static_cast<float>(i) / 5.0f - kPi / 2.0f;
          pentagon.push_back(
              {650.0f + std::cos(a) * 55.0f, 580.0f + std::sin(a) * 55.0f});
        }
        painter.SetBrush(
            sdl_painter::Brush(sdl_painter::Color{255, 160, 50, 200}));
        painter.FillPolygon(pentagon);
        painter.SetPen(
            sdl_painter::Pen(sdl_painter::Color{255, 220, 120, 255}, 2.0f));
        painter.DrawPolygon(pentagon);
      }

      painter.End();
    }
  }  // ← Painter burada yok edilir; GL kaynakları context geçerliyken silinir.

  spdlog::info("Demo kapatiliyor.");
  SDL_DestroyWindow(window);
  SDL_Quit();
  return 0;
}
