/// @brief images — Image / texture cizimi.
///
/// Gelistirme Fazi 3 demosu (eski ad: phase3_demo).
///
/// Gosterilen ozellikler:
///   - CreateFromData ile prosedürel dokular (checkerboard, degrade, alfa daire)
///   - DrawImage(image, x, y) — orijinal boyut
///   - DrawImage(image, dest_rect) — olceklendirilmis cizim
///   - DrawImage(image, src_rect, dest_rect) — texture atlas dilimleme
///   - Transform + DrawImage birlestirme (donme animasyonu)
///   - Alfa karistirma (transparan doku uzerine cizim)
///
/// ESC veya pencere kapat cikmak icin.

#include "sdl_painter/brush.h"
#include "sdl_painter/color.h"
#include "sdl_painter/geometry.h"
#include "sdl_painter/image.h"
#include "sdl_painter/painter.h"
#include "sdl_painter/pen.h"

#include <SDL3/SDL.h>

#include <cmath>
#include <cstdint>
#include <spdlog/sinks/ansicolor_sink.h>
#include <spdlog/spdlog.h>
#include <vector>

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

// --- Prosedürel doku üreticileri ---

/// @brief 128x128 RGBA damalı desen.
static sdl_painter::Image MakeCheckerboard(int32_t size, int32_t cell) {
  std::vector<uint8_t> pixels(static_cast<std::size_t>(size * size * 4));
  for (int32_t y = 0; y < size; ++y) {
    for (int32_t x = 0; x < size; ++x) {
      bool light = ((x / cell) + (y / cell)) % 2 == 0;
      int32_t idx = (y * size + x) * 4;
      pixels[static_cast<std::size_t>(idx + 0)] = light ? 220 : 40;
      pixels[static_cast<std::size_t>(idx + 1)] = light ? 220 : 60;
      pixels[static_cast<std::size_t>(idx + 2)] = light ? 220 : 180;
      pixels[static_cast<std::size_t>(idx + 3)] = 255;
    }
  }
  return sdl_painter::Image::CreateFromData(pixels.data(), size, size, 4);
}

/// @brief 256x64 RGB yatay renk degradesi.
static sdl_painter::Image MakeGradient(int32_t w, int32_t h) {
  std::vector<uint8_t> pixels(static_cast<std::size_t>(w * h * 3));
  for (int32_t y = 0; y < h; ++y) {
    for (int32_t x = 0; x < w; ++x) {
      int32_t idx = (y * w + x) * 3;
      pixels[static_cast<std::size_t>(idx + 0)] =
          static_cast<uint8_t>(255 * x / (w - 1));
      pixels[static_cast<std::size_t>(idx + 1)] =
          static_cast<uint8_t>(180 * y / (h - 1));
      pixels[static_cast<std::size_t>(idx + 2)] =
          static_cast<uint8_t>(255 - 255 * x / (w - 1));
    }
  }
  return sdl_painter::Image::CreateFromData(pixels.data(), w, h, 3);
}

/// @brief 128x128 RGBA — beyaz dolgulu daire, kenar soluklasiyor (alfa).
static sdl_painter::Image MakeAlphaCircle(int32_t size) {
  std::vector<uint8_t> pixels(static_cast<std::size_t>(size * size * 4), 0);
  const float cx = static_cast<float>(size) * 0.5f;
  const float cy = static_cast<float>(size) * 0.5f;
  const float r = cx - 2.0f;
  for (int32_t y = 0; y < size; ++y) {
    for (int32_t x = 0; x < size; ++x) {
      float dx = static_cast<float>(x) - cx;
      float dy = static_cast<float>(y) - cy;
      float dist = std::sqrt(dx * dx + dy * dy);
      if (dist < r) {
        float t = dist / r;  // 0 = merkez, 1 = kenar
        auto alpha = static_cast<uint8_t>(255.0f * (1.0f - t * t));
        int32_t idx = (y * size + x) * 4;
        pixels[static_cast<std::size_t>(idx + 0)] = 255;
        pixels[static_cast<std::size_t>(idx + 1)] = 200;
        pixels[static_cast<std::size_t>(idx + 2)] = 80;
        pixels[static_cast<std::size_t>(idx + 3)] = alpha;
      }
    }
  }
  return sdl_painter::Image::CreateFromData(pixels.data(), size, size, 4);
}

/// @brief 256x256 RGBA — 2x2 renk atlas (4 farkli renk blogu).
///
/// Sol-ust: kirmizi  |  Sag-ust: yesil
/// Sol-alt: mavi     |  Sag-alt: sari
static sdl_painter::Image MakeColorAtlas(int32_t size) {
  std::vector<uint8_t> pixels(static_cast<std::size_t>(size * size * 4));
  int32_t half = size / 2;
  for (int32_t y = 0; y < size; ++y) {
    for (int32_t x = 0; x < size; ++x) {
      bool right = x >= half;
      bool bottom = y >= half;
      uint8_t r = 0, g = 0, b = 0;
      if (!right && !bottom) {
        r = 220;
        g = 60;
        b = 60;
      }  // kirmizi
      if (right && !bottom) {
        r = 60;
        g = 200;
        b = 80;
      }  // yesil
      if (!right && bottom) {
        r = 60;
        g = 100;
        b = 220;
      }  // mavi
      if (right && bottom) {
        r = 240;
        g = 200;
        b = 50;
      }  // sari
      int32_t idx = (y * size + x) * 4;
      pixels[static_cast<std::size_t>(idx + 0)] = r;
      pixels[static_cast<std::size_t>(idx + 1)] = g;
      pixels[static_cast<std::size_t>(idx + 2)] = b;
      pixels[static_cast<std::size_t>(idx + 3)] = 220;
    }
  }
  return sdl_painter::Image::CreateFromData(pixels.data(), size, size, 4);
}

int main() {
  InitLogger();
  spdlog::info("SDLPainter images demo baslatiliyor...");

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
      SDL_CreateWindow("SDLPainter — images: Image & Texture (Phase 3)", 800, 600,
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

    // Dokular bir kez olusturulur; Upload() lazy cagrilir (ilk DrawImage'da).
    sdl_painter::Image checker = MakeCheckerboard(128, 16);
    sdl_painter::Image gradient = MakeGradient(256, 64);
    sdl_painter::Image alpha_circ = MakeAlphaCircle(128);
    sdl_painter::Image atlas = MakeColorAtlas(256);

    spdlog::info("Dokular hazir. ESC veya pencere kapat sonra cikis.");

    float angle = 0.0f;
    constexpr float kPi = 3.14159265358979323846f;

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

      angle += 0.4f;
      if (angle >= 360.0f)
        angle -= 360.0f;

      painter.Begin();
      painter.Clear(sdl_painter::Color{20, 20, 30, 255});
      painter.SetPen(sdl_painter::Pen(sdl_painter::Color{0, 0, 0, 0}, 0.0f));

      // ----------------------------------------------------------------
      // Bolum 1: Damalı doku — orijinal boyut (128x128)
      // ----------------------------------------------------------------
      painter.DrawImage(checker, 50.0f, 80.0f);

      // ----------------------------------------------------------------
      // Bolum 2: Damalı doku — olceklendirilmis (dest_rect)
      // ----------------------------------------------------------------
      painter.DrawImage(checker,
                        sdl_painter::Rect{210.0f, 80.0f, 180.0f, 180.0f});

      // ----------------------------------------------------------------
      // Bolum 3: Degrade doku — buyutulmus (360x100)
      // ----------------------------------------------------------------
      painter.DrawImage(gradient, sdl_painter::Rect{0.0f, 0.0f, 256.0f, 64.0f},
                        sdl_painter::Rect{420.0f, 110.0f, 360.0f, 100.0f});

      // ----------------------------------------------------------------
      // Bolum 4: Texture atlas dilimleme — her kareyi ayri ayri ciz
      // ----------------------------------------------------------------
      // Atlas 256x256, dordü de 128x128
      const float half = 128.0f;
      const float tx = 710.0f, ty = 80.0f, tw = 100.0f, th = 100.0f;

      painter.DrawImage(atlas, sdl_painter::Rect{0.0f, 0.0f, half, half},
                        sdl_painter::Rect{tx, ty, tw, th});
      painter.DrawImage(atlas, sdl_painter::Rect{half, 0.0f, half, half},
                        sdl_painter::Rect{tx + tw, ty, tw, th});
      painter.DrawImage(atlas, sdl_painter::Rect{0.0f, half, half, half},
                        sdl_painter::Rect{tx, ty + th, tw, th});
      painter.DrawImage(atlas, sdl_painter::Rect{half, half, half, half},
                        sdl_painter::Rect{tx + tw, ty + th, tw, th});

      // Atlas karti: cerceve
      painter.SetPen(
          sdl_painter::Pen(sdl_painter::Color{200, 200, 200, 160}, 1.0f));
      painter.DrawRect(tx, ty, tw * 2.0f, th * 2.0f);

      // ----------------------------------------------------------------
      // Bolum 5: Alfa karistirmali daire dokusu
      // ----------------------------------------------------------------
      // Arkaplan olarak renkli dikdortgen ciz, sonra yarı saydam doku.
      painter.SetPen(sdl_painter::Pen(sdl_painter::Color{0, 0, 0, 0}, 0.0f));
      painter.SetBrush(
          sdl_painter::Brush(sdl_painter::Color{60, 120, 200, 200}));
      painter.FillRect(50.0f, 310.0f, 200.0f, 200.0f);
      painter.DrawImage(alpha_circ, 50.0f, 310.0f);   // 128x128, uzerine
      painter.DrawImage(alpha_circ, 122.0f, 382.0f);  // kismen ustuste

      // ----------------------------------------------------------------
      // Bolum 6: Transform + DrawImage — donme animasyonu
      // ----------------------------------------------------------------
      painter.Save();
      painter.Translate(520.0f, 430.0f);
      painter.Rotate(angle);
      // Merkez etrafinda cizmek icin: sol-ust = (-w/2, -h/2)
      // 192x192 boyut: tam doku 128x128 kaynak, buyutulmus hedef
      painter.DrawImage(checker, sdl_painter::Rect{0.0f, 0.0f, 128.0f, 128.0f},
                        sdl_painter::Rect{-96.0f, -96.0f, 192.0f, 192.0f});
      painter.Restore();

      // ----------------------------------------------------------------
      // Bolum 7: Scale animasyonu + degrade doku
      // Ekran merkezinde, kendi merkezi etrafinda buyuyup kuculen gradient.
      // ----------------------------------------------------------------
      {
        static float scaleCoeff = -0.9f;
        static float scaleIncrement = 0.001f;
        float sx = 1.0f + scaleCoeff;
        float sy = 1.0f + scaleCoeff;

        scaleCoeff += scaleIncrement;

        if (scaleCoeff > 0.9f || scaleCoeff < -0.9f)
          scaleIncrement *= -1.0f;

        constexpr float kCenterX = 400.0f;
        constexpr float kCenterY = 300.0f;
        constexpr float kImgW = 320.0f;
        constexpr float kImgH = 100.0f;

        painter.Save();
        // Once merkeze tasi, sonra orada olcekle. Scale lokal orijin
        // etrafinda calisir; vertex'leri (-w/2, -h/2) vererek image'in
        // kendi merkezinden olceklenmesini saglariz.
        painter.Translate(kCenterX, kCenterY);
        painter.Scale(sx, sy);
        painter.DrawImage(
            gradient, sdl_painter::Rect{0.0f, 0.0f, 256.0f, 64.0f},
            sdl_painter::Rect{-kImgW * 0.5f, -kImgH * 0.5f, kImgW, kImgH});
        painter.Restore();
      }

      // ----------------------------------------------------------------
      // Bolum 8: Pen outline — damali doku uzerinde kontrast
      // Ayni renk/kalinliktaki iki kalem yan yana: solda outline yok,
      // sagda siyah dis kontur. Kontur, karmasik arka plan uzerinde
      // cizginin belirginligini artirir.
      // ----------------------------------------------------------------
      painter.SetPen(sdl_painter::Pen(sdl_painter::Color{0, 0, 0, 0}, 0.0f));
      painter.DrawImage(checker,
                        sdl_painter::Rect{560.0f, 350.0f, 220.0f, 200.0f});

      painter.SetPen(
          sdl_painter::Pen(sdl_painter::Color{255, 220, 60, 255}, 3.0f));
      painter.DrawCircle(610.0f, 410.0f, 35.0f);

      painter.SetPen(sdl_painter::Pen(sdl_painter::Color{255, 220, 60, 255},
                                      3.0f, sdl_painter::Color::Black(), 2.0f));
      painter.DrawCircle(710.0f, 410.0f, 35.0f);
      painter.DrawRect(600.0f, 465.0f, 140.0f, 60.0f);

      painter.End();
    }
  }  // Painter burada yok edilir; GL kaynaklari context gecerliyken silinir.

  spdlog::info("Demo kapatiliyor.");
  SDL_DestroyWindow(window);
  SDL_Quit();
  return 0;
}
