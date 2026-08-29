/// @brief transforms — Transform stack ve scissor clip.
///
/// Geliştirme Fazı 2 demosu (eski ad: phase2_demo).
///
/// Gosterilen ozellikler:
///   - Translate / Rotate / Scale kombinasyonlari
///   - Save / Restore ile ic ice transform stack
///   - SetClipRect ile dikdortgen kispi (scissor)
///
/// ESC veya pencere kapat cikmak icin.

#include "sdl_painter/brush.h"
#include "sdl_painter/color.h"
#include "sdl_painter/geometry.h"
#include "sdl_painter/painter.h"
#include "sdl_painter/pen.h"

#include <SDL3/SDL.h>

#include <cmath>
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
  spdlog::info("SDLPainter transforms demo baslatiliyor...");

  if (!SDL_Init(SDL_INIT_VIDEO)) {
    spdlog::error("SDL_Init basarisiz: {}", SDL_GetError());
    return 1;
  }

  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
  SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1);
  SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 4);

  SDL_Window* window = SDL_CreateWindow(
      "SDLPainter — transforms: Transform Stack & Clip (Phase 2)", 900, 700,
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

    float angle = 0.0f;  // animasyon acisi

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

      painter.Begin();
      painter.Clear(sdl_painter::Color{20, 20, 30, 255});

      // ----------------------------------------------------------------
      // Bolum 1: Basit Translate
      // Sol ust kose — farkli renkli dikdortgenler kaydirilmis
      // ----------------------------------------------------------------
      painter.SetPen(sdl_painter::Pen(sdl_painter::Color{0, 0, 0, 0}, 0.0f));

      painter.Save();
      painter.Translate(110.0f, 190.0f);
      painter.SetBrush(
          sdl_painter::Brush(sdl_painter::Color{80, 140, 220, 220}));
      painter.FillRect(0.0f, 0.0f, 100.0f, 60.0f);
      painter.Translate(30.0f, 25.0f);
      painter.SetBrush(
          sdl_painter::Brush(sdl_painter::Color{220, 100, 80, 200}));
      painter.FillRect(0.0f, 0.0f, 100.0f, 60.0f);
      painter.Translate(30.0f, 25.0f);
      painter.SetBrush(
          sdl_painter::Brush(sdl_painter::Color{80, 200, 130, 200}));
      painter.FillRect(0.0f, 0.0f, 100.0f, 60.0f);
      painter.Restore();

      // ----------------------------------------------------------------
      // Bolum 2: Donus animasyonu — Save/Restore ile merkez etrafinda
      // ----------------------------------------------------------------
      painter.Save();
      painter.Translate(320.0f, 190.0f);
      painter.Rotate(angle);
      painter.SetBrush(
          sdl_painter::Brush(sdl_painter::Color{255, 200, 50, 220}));
      painter.SetPen(sdl_painter::Pen(sdl_painter::Color{255, 240, 180, 255},
                                      2.0f, sdl_painter::Color::Black(), 1.5f));
      painter.FillRect(-55.0f, -35.0f, 110.0f, 70.0f);
      painter.DrawRect(-55.0f, -35.0f, 110.0f, 70.0f);
      painter.Restore();

      // ----------------------------------------------------------------
      // Bolum 3: Olcekleme animasyonu
      // ----------------------------------------------------------------
      {
        constexpr float kPi = 3.14159265358979323846f;
        float scale_factor = 1.0f + 0.4f * std::sin(angle * kPi / 180.0f);
        painter.Save();
        painter.Translate(520.0f, 190.0f);
        painter.Scale(scale_factor, scale_factor);
        painter.SetBrush(
            sdl_painter::Brush(sdl_painter::Color{150, 80, 220, 220}));
        painter.FillCircle(0.0f, 0.0f, 50.0f);
        painter.SetPen(
            sdl_painter::Pen(sdl_painter::Color{200, 150, 255, 255}, 2.0f));
        painter.DrawCircle(0.0f, 0.0f, 50.0f);
        painter.Restore();
      }

      // ----------------------------------------------------------------
      // Bolum 4: Ic ice Save/Restore — donus + olcekleme birlestirme
      // ----------------------------------------------------------------
      painter.Save();
      painter.Translate(730.0f, 190.0f);
      painter.Rotate(angle * 0.7f);
      painter.SetBrush(
          sdl_painter::Brush(sdl_painter::Color{60, 180, 200, 200}));
      painter.FillRect(-50.0f, -50.0f, 100.0f, 100.0f);

      painter.Save();
      painter.Rotate(angle * 1.5f);
      painter.Scale(0.5f, 0.5f);
      painter.SetBrush(
          sdl_painter::Brush(sdl_painter::Color{255, 120, 60, 220}));
      painter.FillRect(-50.0f, -50.0f, 100.0f, 100.0f);

      painter.Save();
      painter.Rotate(-angle * 2.0f);
      painter.Scale(0.5f, 0.5f);
      painter.SetBrush(
          sdl_painter::Brush(sdl_painter::Color{50, 220, 120, 220}));
      painter.FillRect(-50.0f, -50.0f, 100.0f, 100.0f);
      painter.Restore();
      painter.Restore();
      painter.Restore();

      // ----------------------------------------------------------------
      // Bolum 5: Scissor clip — dikdortgen kispi icine cizim
      // ----------------------------------------------------------------
      // Kispi alani: kispi cercevesi
      painter.SetPen(
          sdl_painter::Pen(sdl_painter::Color{200, 200, 200, 180}, 1.5f));
      painter.DrawRect(100.0f, 380.0f, 320.0f, 200.0f);

      // Kispiyi etkinlestir
      painter.SetClipRect(sdl_painter::Rect{100.0f, 380.0f, 320.0f, 200.0f});

      // Kispi disina tasacak sekilde buyuk bir daire ciz
      painter.SetBrush(
          sdl_painter::Brush(sdl_painter::Color{80, 160, 255, 180}));
      painter.FillCircle(260.0f, 480.0f, 150.0f);

      // Kispi icinde donan dikdortgen
      painter.Save();
      painter.Translate(260.0f, 480.0f);
      painter.Rotate(angle);
      painter.SetBrush(
          sdl_painter::Brush(sdl_painter::Color{255, 180, 60, 200}));
      painter.FillRect(-80.0f, -25.0f, 160.0f, 50.0f);
      painter.SetPen(
          sdl_painter::Pen(sdl_painter::Color{255, 230, 140, 255}, 2.0f));
      painter.DrawRect(-80.0f, -25.0f, 160.0f, 50.0f);
      painter.Restore();

      // Kispiyi kaldir
      painter.ClearClip();

      // ----------------------------------------------------------------
      // Bolum 6: ResetTransform testi — mutlak koordinatlarda cizim
      // ----------------------------------------------------------------
      // Onceki transform durumu ne olursa olsun, reset sonrasi
      // ekranin ortasinda sabit bir daire ciziyoruz.
      painter.Save();
      painter.Translate(9999.0f, 9999.0f);  // ← yanlis transform
      painter.ResetTransform();             // ← sifirla
      painter.SetBrush(
          sdl_painter::Brush(sdl_painter::Color{200, 50, 100, 200}));
      painter.SetPen(
          sdl_painter::Pen(sdl_painter::Color{255, 100, 150, 255}, 2.0f));
      painter.FillCircle(540.0f, 480.0f, 50.0f);
      painter.DrawCircle(540.0f, 480.0f, 50.0f);
      painter.Restore();

      // ----------------------------------------------------------------
      // Bolum 7: Scale animasyonu — X ve Y bagimsiz
      // ----------------------------------------------------------------
      {
        constexpr float kPi = 3.14159265358979323846f;
        float sx = 1.0f + 0.5f * std::cos(angle * kPi / 180.0f);
        float sy = 1.0f + 0.5f * std::sin(angle * kPi / 180.0f);
        painter.Save();
        painter.Translate(760.0f, 480.0f);
        painter.Scale(sx, sy);
        painter.SetBrush(
            sdl_painter::Brush(sdl_painter::Color{100, 220, 180, 200}));
        painter.SetPen(
            sdl_painter::Pen(sdl_painter::Color{180, 255, 230, 255}, 1.5f));
        painter.FillEllipse(0.0f, 0.0f, 60.0f, 40.0f);
        painter.DrawEllipse(0.0f, 0.0f, 60.0f, 40.0f);
        painter.Restore();
      }

      painter.End();
    }
  }  // Painter burada yok edilir; GL kaynaklari context gecerliyken silinir.

  spdlog::info("Demo kapatiliyor.");
  SDL_DestroyWindow(window);
  SDL_Quit();
  return 0;
}
