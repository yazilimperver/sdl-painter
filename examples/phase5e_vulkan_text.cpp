/// @brief Phase 5e demo — Vulkan backend metin çizimi.
///
/// Phase 4 (OpenGL metin) ile aynı özellikleri Vulkan üzerinde doğrular:
///   - DrawText(x, y, text)          — konuma metin
///   - DrawText(rect, text, align)   — hizalı metin
///   - Farklı punto boyutları
///   - Renkli metinler (SetPen ile)
///   - SetOpacity ile yarı saydam metin
///   - Transform stack + DrawText (dönen metin)
///   - Türkçe UTF-8 karakter desteği
///
/// Derleme gereksinimi: SDLPAINTER_WITH_VULKAN=ON
///
/// ESC veya pencere kapat → çıkış.

#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>

#include <spdlog/sinks/ansicolor_sink.h>
#include <spdlog/spdlog.h>

#ifdef _WIN32
#include <windows.h>
#endif

#include <cmath>
#include <cstdint>
#include <string>

#include "sdl_painter/brush.h"
#include "sdl_painter/color.h"
#include "sdl_painter/geometry.h"
#include "sdl_painter/font.h"
#include "sdl_painter/painter.h"
#include "sdl_painter/pen.h"

namespace {

/// @brief Windows konsolunda ANSI renk kodlarını etkinleştirir.
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

/// @brief Sistemde mevcut bir TTF fontunu bulmaya çalışır.
std::string FindSystemFont() {
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

/// @brief Arka planlı metin kutusu çizer — hizalama testi için.
void DrawLabelBox(sdl_painter::Painter& p, const sdl_painter::Rect& box,
                  const std::string& text, sdl_painter::Alignment align,
                  sdl_painter::Color bg, sdl_painter::Color text_color) {
  p.SetPen(sdl_painter::Pen({0, 0, 0, 0}, 0.0f));
  p.SetBrush(sdl_painter::Brush(bg));
  p.FillRect(box.x, box.y, box.w, box.h);

  p.SetPen(sdl_painter::Pen({180, 180, 180, 100}, 1.0f));
  p.SetBrush(sdl_painter::Brush({0, 0, 0, 0}));
  p.DrawRect(box.x, box.y, box.w, box.h);

  p.SetPen(sdl_painter::Pen(text_color, 0.0f));
  p.DrawText(box, text, align);
}

}  // namespace

int main() {
  InitLogger();

  spdlog::info("SDLPainter Phase 5e (Vulkan + Metin) başlıyor...");
  spdlog::info("ESC veya pencereyi kapat ile çıkış.");

  if (!SDL_Init(SDL_INIT_VIDEO)) {
    spdlog::error("SDL_Init hata: {}", SDL_GetError());
    return 1;
  }

  if (!TTF_Init()) {
    spdlog::error("TTF_Init hata: {}", SDL_GetError());
    SDL_Quit();
    return 1;
  }

  const std::string font_path = FindSystemFont();
  if (font_path.empty()) {
    spdlog::error("Sistem fontu bulunamadı. Demo çalıştırılamıyor.");
    TTF_Quit();
    SDL_Quit();
    return 1;
  }

  SDL_Window* window =
      SDL_CreateWindow("SDLPainter — Phase 5e (Vulkan + Metin)", 900, 650,
                       SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);
  if (!window) {
    spdlog::error("SDL_CreateWindow hata: {}", SDL_GetError());
    TTF_Quit();
    SDL_Quit();
    return 1;
  }

  {  // Painter scope: window'dan önce destroy edilmeli.
    sdl_painter::Painter painter(window, sdl_painter::RendererBackend::kVulkan);
    if (!painter.IsValid()) {
      spdlog::error("Vulkan Painter başlatılamadı.");
      SDL_DestroyWindow(window);
      TTF_Quit();
      SDL_Quit();
      return 1;
    }

    // Farklı punto boyutlarında fontlar.
    auto font_sm = std::make_shared<sdl_painter::Font>(font_path, 14);
    auto font_md = std::make_shared<sdl_painter::Font>(font_path, 22);
    auto font_lg = std::make_shared<sdl_painter::Font>(font_path, 36);
    auto font_xl = std::make_shared<sdl_painter::Font>(font_path, 56);

    if (!font_sm->IsValid() || !font_md->IsValid() || !font_lg->IsValid() ||
        !font_xl->IsValid()) {
      spdlog::error("En az bir font yüklenemedi.");
      SDL_DestroyWindow(window);
      TTF_Quit();
      SDL_Quit();
      return 1;
    }

    // MeasureText kontrolü
    int32_t mw = 0, mh = 0;
    font_md->MeasureText("SDLPainter Phase 5e", mw, mh);
    spdlog::info("MeasureText: \"SDLPainter Phase 5e\" → {}×{} piksel", mw, mh);

    float angle     = 0.0f;
    uint32_t frames = 0;

    bool running = true;
    while (running) {
      SDL_Event event;
      while (SDL_PollEvent(&event)) {
        if (event.type == SDL_EVENT_QUIT) running = false;
        if (event.type == SDL_EVENT_KEY_DOWN &&
            event.key.key == SDLK_ESCAPE)
          running = false;
      }

      angle += 0.5f;
      if (angle >= 360.0f) angle -= 360.0f;
      ++frames;

      painter.Begin();
      painter.Clear(sdl_painter::Color{18, 18, 28, 255});

      // -----------------------------------------------------------------------
      // 1. Büyük başlık — ortada (Vulkan backend üzerinde)
      // -----------------------------------------------------------------------
      painter.SetFont(font_xl);
      painter.SetPen(sdl_painter::Pen({249, 226, 175, 255}, 0.0f));  // sarı
      painter.DrawText(sdl_painter::Rect{0.0f, 10.0f, 900.0f, 70.0f},
                       "SDLPainter", sdl_painter::Alignment::kCenter);

      // -----------------------------------------------------------------------
      // 2. Alt başlık — sağa hizalı
      // -----------------------------------------------------------------------
      painter.SetFont(font_md);
      painter.SetPen(sdl_painter::Pen({137, 180, 250, 255}, 0.0f));  // mavi
      painter.DrawText(sdl_painter::Rect{0.0f, 75.0f, 890.0f, 30.0f},
                       "Phase 5e: Vulkan + Metin (UTF-8)",
                       sdl_painter::Alignment::kRight);

      // -----------------------------------------------------------------------
      // 3. Hizalama karşılaştırması — sol / orta / sağ
      // -----------------------------------------------------------------------
      constexpr float kBoxY   = 120.0f;
      constexpr float kBoxW   = 280.0f;
      constexpr float kBoxH   = 40.0f;
      constexpr float kGap    = 10.0f;
      constexpr float kStartX = 15.0f;

      painter.SetFont(font_sm);

      DrawLabelBox(painter, {kStartX, kBoxY, kBoxW, kBoxH},
                   "Sol hizalı metin", sdl_painter::Alignment::kLeft,
                   {40, 60, 80, 200}, {200, 220, 255, 255});

      DrawLabelBox(painter, {kStartX + kBoxW + kGap, kBoxY, kBoxW, kBoxH},
                   "Orta hizalı metin", sdl_painter::Alignment::kCenter,
                   {40, 80, 60, 200}, {200, 255, 220, 255});

      DrawLabelBox(painter,
                   {kStartX + (kBoxW + kGap) * 2.0f, kBoxY, kBoxW, kBoxH},
                   "Sağ hizalı metin", sdl_painter::Alignment::kRight,
                   {80, 40, 60, 200}, {255, 200, 220, 255});

      // -----------------------------------------------------------------------
      // 4. Farklı punto boyutları
      // -----------------------------------------------------------------------
      painter.SetPen(sdl_painter::Pen({220, 220, 220, 255}, 0.0f));
      const float sz_y = 178.0f;

      painter.SetFont(font_sm);
      painter.DrawText(20.0f, sz_y, "14pt - Küçük punto");

      painter.SetFont(font_md);
      painter.DrawText(20.0f, sz_y + 24.0f, "22pt - Orta punto");

      painter.SetFont(font_lg);
      painter.DrawText(20.0f, sz_y + 56.0f, "36pt - Büyük punto");

      // -----------------------------------------------------------------------
      // 5. Renkli metinler
      // -----------------------------------------------------------------------
      painter.SetFont(font_md);
      const float col_y = 308.0f;
      const sdl_painter::Color kColors[] = {
          {235, 66,  66,  255},  // kırmızı
          {166, 227, 161, 255},  // yeşil
          {137, 180, 250, 255},  // mavi
          {249, 226, 175, 255},  // sarı
          {203, 166, 247, 255},  // mor
      };
      const char* kColorTexts[] = {
          "Kırmızı", "Yeşil", "Mavi", "Sarı", "Mor",
      };
      for (int32_t i = 0; i < 5; ++i) {
        painter.SetPen(sdl_painter::Pen(kColors[i], 0.0f));
        painter.DrawText(20.0f + static_cast<float>(i) * 172.0f, col_y,
                         kColorTexts[i]);
      }

      // -----------------------------------------------------------------------
      // 6. UTF-8 / Türkçe karakter testi
      // -----------------------------------------------------------------------
      painter.SetFont(font_md);
      painter.SetPen(sdl_painter::Pen({148, 226, 213, 255}, 0.0f));  // teal
      painter.DrawText(20.0f, 348.0f,
                       "UTF-8: Ğğ Üü Şş İı Öö Çç — Merhaba Dünya!");

      // -----------------------------------------------------------------------
      // 7. SetOpacity ile yarı saydam metin
      // -----------------------------------------------------------------------
      painter.SetFont(font_lg);
      painter.SetPen(sdl_painter::Pen({255, 255, 255, 255}, 0.0f));
      const float op_base    = 390.0f;
      const float kOpacities[] = {1.0f, 0.7f, 0.4f, 0.15f};
      const char* kOpLabels[]  = {"100%", "70%", "40%", "15%"};
      for (int32_t i = 0; i < 4; ++i) {
        painter.SetOpacity(kOpacities[i]);
        painter.DrawText(20.0f + static_cast<float>(i) * 210.0f, op_base,
                         kOpLabels[i]);
      }
      painter.SetOpacity(1.0f);

      // -----------------------------------------------------------------------
      // 8. Transform + DrawText — dönen metin
      // -----------------------------------------------------------------------
      painter.Save();
      painter.Translate(750.0f, 490.0f);
      painter.Rotate(angle);
      painter.SetFont(font_md);
      painter.SetPen(sdl_painter::Pen({250, 179, 135, 255}, 0.0f));  // şeftali
      int32_t rot_w = 0, rot_h = 0;
      font_md->MeasureText("Dönen metin!", rot_w, rot_h);
      painter.DrawText(static_cast<float>(-rot_w) * 0.5f,
                       static_cast<float>(-rot_h) * 0.5f, "Dönen metin!");
      painter.Restore();

      // -----------------------------------------------------------------------
      // 9. Alt bilgi şeridi
      // -----------------------------------------------------------------------
      painter.SetOpacity(0.35f);
      painter.SetBrush(sdl_painter::Brush({49, 50, 68, 255}));
      painter.SetPen(sdl_painter::Pen({49, 50, 68, 255}, 0.0f));
      painter.FillRect(0.0f, 618.0f, 900.0f, 32.0f);
      painter.SetOpacity(1.0f);

      painter.SetFont(font_sm);
      painter.SetPen(sdl_painter::Pen({120, 120, 140, 255}, 0.0f));
      painter.DrawText(
          sdl_painter::Rect{10.0f, 619.0f, 880.0f, 28.0f},
          "DrawText  |  Hizalama  |  Punto  |  Renk  |  UTF-8  |  Opacity  "
          "|  Transform",
          sdl_painter::Alignment::kCenter);

      painter.End();
    }

    spdlog::info("Kapatiliyor. Toplam frame: {}", frames);
  }  // painter destructor — VkSurface henüz geçerli.

  SDL_DestroyWindow(window);
  TTF_Quit();
  SDL_Quit();
  return 0;
}
