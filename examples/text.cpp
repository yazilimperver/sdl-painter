/// @brief text — Metin çizimi (SDL_ttf).
///
/// Geliştirme Fazı 4 demosu (eski ad: phase4_demo).
///
/// Gösterilen özellikler:
///   - DrawText(x, y, text) — konuma metin çiz
///   - DrawText(rect, text, alignment) — dikdörtgen içinde hizalı metin
///   - Font::MeasureText() — metin boyutu ölçümü
///   - Farklı punto boyutları
///   - Pen rengi ile metin rengi kontrolü
///   - Transform + DrawText — dönen metin
///   - SetOpacity ile yarı saydam metin
///
/// ESC veya pencere kapat → çıkış.

#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <spdlog/sinks/ansicolor_sink.h>
#include <spdlog/spdlog.h>

#include <cmath>
#include <cstdint>
#include <string>

#include "sdl_painter/brush.h"
#include "sdl_painter/color.h"
#include "sdl_painter/font.h"
#include "sdl_painter/geometry.h"
#include "sdl_painter/painter.h"
#include "sdl_painter/pen.h"

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

/// @brief Sistemde mevcut bir TTF fontunu bulmaya çalış.
///
/// Önce yaygın Windows/Linux yollarını dener; bulamazsa boş string döner.
static std::string FindSystemFont() {
  const char* candidates[] = {
#ifdef _WIN32
      "C:/Windows/Fonts/arial.ttf",
      "C:/Windows/Fonts/calibri.ttf",
      "C:/Windows/Fonts/segoeui.ttf",
      "C:/Windows/Fonts/tahoma.ttf",
#else
      "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
      "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf",
      "/usr/share/fonts/truetype/freefont/FreeSans.ttf",
      "/usr/share/fonts/TTF/DejaVuSans.ttf",
#endif
  };
  for (const char* path : candidates) {
    if (SDL_IOFromFile(path, "rb")) {
      spdlog::info("Font bulundu: {}", path);
      return path;
    }
  }
  return {};
}

// ---------------------------------------------------------------------------
// Yardımcı: arka planlı metin kutusu çiz.
// ---------------------------------------------------------------------------
static void DrawLabelBox(sdl_painter::Painter& p,
                         const sdl_painter::Rect& box,
                         const std::string& text,
                         sdl_painter::Alignment align,
                         sdl_painter::Color bg,
                         sdl_painter::Color text_color) {
  p.SetPen(sdl_painter::Pen(sdl_painter::Color{0, 0, 0, 0}, 0.0f));
  p.SetBrush(sdl_painter::Brush(bg));
  p.FillRect(box.x, box.y, box.w, box.h);

  p.SetPen(sdl_painter::Pen(sdl_painter::Color{180, 180, 180, 100}, 1.0f));
  p.SetBrush(sdl_painter::Brush(sdl_painter::Color{0, 0, 0, 0}));
  p.DrawRect(box.x, box.y, box.w, box.h);

  p.SetPen(sdl_painter::Pen(text_color, 0.0f));
  p.DrawText(box, text, align);
}

int main() {
  InitLogger();
  spdlog::info("SDLPainter text demo baslatiliyor...");

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
      "SDLPainter — text: Metin Cizimi (Phase 4)",
      900, 650,
      SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
  if (!window) {
    spdlog::error("SDL_CreateWindow basarisiz: {}", SDL_GetError());
    SDL_Quit();
    return 1;
  }

  const std::string font_path = FindSystemFont();
  if (font_path.empty()) {
    spdlog::error("Sistem fontu bulunamadi. Demo calistirilemiyor.");
    SDL_DestroyWindow(window);
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

    // Farklı punto boyutlarında fontlar.
    auto font_sm = std::make_shared<sdl_painter::Font>(font_path, 14);
    auto font_md = std::make_shared<sdl_painter::Font>(font_path, 22);
    auto font_lg = std::make_shared<sdl_painter::Font>(font_path, 36);
    auto font_xl = std::make_shared<sdl_painter::Font>(font_path, 56);

    if (!font_sm->IsValid() || !font_md->IsValid() ||
        !font_lg->IsValid() || !font_xl->IsValid()) {
      spdlog::error("En az bir font yuklenemedi.");
      SDL_DestroyWindow(window);
      SDL_Quit();
      return 1;
    }

    // MeasureText kontrolü
    int32_t mw = 0, mh = 0;
    font_md->MeasureText("SDLPainter text demo", mw, mh);
    spdlog::info("MeasureText: \"SDLPainter text demo\" -> {}x{} piksel", mw, mh);

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
      painter.Clear(sdl_painter::Color{18, 18, 28, 255});

      // ---------------------------------------------------------------
      // Bölüm 1: Büyük başlık — ortada
      // ---------------------------------------------------------------
      painter.SetFont(font_xl);
      painter.SetPen(sdl_painter::Pen(sdl_painter::Color{255, 220, 80, 255}, 0.0f));
      painter.DrawText(sdl_painter::Rect{0.0f, 10.0f, 900.0f, 70.0f},
                       "SDLPainter", sdl_painter::Alignment::kCenter);

      // ---------------------------------------------------------------
      // Bölüm 2: Alt başlık — sağa hizalı
      // ---------------------------------------------------------------
      painter.SetFont(font_md);
      painter.SetPen(sdl_painter::Pen(sdl_painter::Color{160, 200, 255, 255}, 0.0f));
      painter.DrawText(sdl_painter::Rect{0.0f, 75.0f, 890.0f, 30.0f},
                       "Phase 4: Metin Cizimi (SDL_ttf)",
                       sdl_painter::Alignment::kRight);

      // ---------------------------------------------------------------
      // Bölüm 3: Hizalama karşılaştırması (sol / orta / sağ)
      // ---------------------------------------------------------------
      constexpr float kBoxY  = 120.0f;
      constexpr float kBoxW  = 280.0f;
      constexpr float kBoxH  = 40.0f;
      constexpr float kGap   = 10.0f;
      constexpr float kStartX = 15.0f;

      painter.SetFont(font_sm);

      DrawLabelBox(painter,
                   {kStartX, kBoxY, kBoxW, kBoxH},
                   "Sol hizali metin",
                   sdl_painter::Alignment::kLeft,
                   sdl_painter::Color{40, 60, 80, 200},
                   sdl_painter::Color{200, 220, 255, 255});

      DrawLabelBox(painter,
                   {kStartX + kBoxW + kGap, kBoxY, kBoxW, kBoxH},
                   "Orta hizali metin",
                   sdl_painter::Alignment::kCenter,
                   sdl_painter::Color{40, 80, 60, 200},
                   sdl_painter::Color{200, 255, 220, 255});

      DrawLabelBox(painter,
                   {kStartX + (kBoxW + kGap) * 2.0f, kBoxY, kBoxW, kBoxH},
                   "Sag hizali metin",
                   sdl_painter::Alignment::kRight,
                   sdl_painter::Color{80, 40, 60, 200},
                   sdl_painter::Color{255, 200, 220, 255});

      // ---------------------------------------------------------------
      // Bölüm 4: Farklı punto boyutları
      // ---------------------------------------------------------------
      const float sz_y = 180.0f;
      painter.SetPen(sdl_painter::Pen(sdl_painter::Color{220, 220, 220, 255}, 0.0f));

      painter.SetFont(font_sm);
      painter.DrawText(20.0f, sz_y, "14pt - Kucuk punto");

      painter.SetFont(font_md);
      painter.DrawText(20.0f, sz_y + 24.0f, "22pt - Orta punto");

      painter.SetFont(font_lg);
      painter.DrawText(20.0f, sz_y + 56.0f, "36pt - Buyuk punto");

      // ---------------------------------------------------------------
      // Bölüm 5: Renkli metinler
      // ---------------------------------------------------------------
      painter.SetFont(font_md);
      const float col_y = 310.0f;
      const sdl_painter::Color colors[] = {
          {255, 80,  80,  255},  // kirmizi
          {80,  255, 120, 255},  // yesil
          {80,  160, 255, 255},  // mavi
          {255, 220, 80,  255},  // sari
          {200, 80,  255, 255},  // mor
      };
      const char* color_texts[] = {
          "Kirmizi metin",
          "Yesil metin",
          "Mavi metin",
          "Sari metin",
          "Mor metin",
      };
      for (int32_t i = 0; i < 5; ++i) {
        painter.SetPen(sdl_painter::Pen(colors[i], 0.0f));
        painter.DrawText(20.0f + static_cast<float>(i) * 170.0f,
                         col_y, color_texts[i]);
      }

      // ---------------------------------------------------------------
      // Bölüm 6: SetOpacity ile yarı saydam metin
      // ---------------------------------------------------------------
      painter.SetFont(font_lg);
      painter.SetPen(sdl_painter::Pen(sdl_painter::Color{255, 255, 255, 255}, 0.0f));
      const float op_base = 350.0f;
      const float opacities[] = {1.0f, 0.7f, 0.4f, 0.15f};
      const char* op_labels[] = {"100%", "70%", "40%", "15%"};
      for (int32_t i = 0; i < 4; ++i) {
        painter.SetOpacity(opacities[i]);
        painter.DrawText(20.0f + static_cast<float>(i) * 210.0f,
                         op_base, op_labels[i]);
      }
      painter.SetOpacity(1.0f);

      // ---------------------------------------------------------------
      // Bölüm 7: Transform + DrawText — dönen metin
      // ---------------------------------------------------------------
      painter.Save();
      painter.Translate(750.0f, 450.0f);
      painter.Rotate(angle);
      painter.SetFont(font_md);
      painter.SetPen(sdl_painter::Pen(sdl_painter::Color{255, 160, 60, 255}, 0.0f));
      // Merkez etrafında döndürmek için ofset uygula.
      int32_t rot_w = 0, rot_h = 0;
      font_md->MeasureText("Donen metin!", rot_w, rot_h);
      painter.DrawText(static_cast<float>(-rot_w) * 0.5f,
                       static_cast<float>(-rot_h) * 0.5f, "Donen metin!");
      painter.Restore();

      // ---------------------------------------------------------------
      // Bölüm 8: DrawText(x, y) ile çok satırlı alt bilgi
      // ---------------------------------------------------------------
      painter.SetFont(font_sm);
      painter.SetPen(sdl_painter::Pen(sdl_painter::Color{120, 120, 120, 255}, 0.0f));
      painter.SetOpacity(1.0f);
      painter.DrawText(10.0f, 610.0f,
                       "DrawText(x,y)  |  DrawText(rect, align)  |"
                       "  MeasureText  |  Transform + metin  |  Opacity");

      painter.End();
    }
  }

  spdlog::info("Demo kapatiliyor.");
  SDL_DestroyWindow(window);
  SDL_Quit();
  return 0;
}
