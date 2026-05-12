/// @brief Phase 5d demo — Vulkan backend polish & doğrulama.
///
/// Test edilen özellikler:
///   - Swapchain recreate: pencereyi yeniden boyutlandır, çizim bozulmamalı.
///   - Tüm untextured primitive'ler: FillRect, DrawRect, FillCircle, DrawCircle,
///     FillEllipse, DrawEllipse, FillPolygon, DrawLine, DrawPolyline.
///   - Textured çizim: DrawImage (src+dst rect).
///   - Transform stack: Translate, Rotate, Scale, Save/Restore.
///   - Opacity korelasyonu: farklı opaklık seviyelerinde tüm tipler.
///   - Validation layer çıktısı: konsola hata/uyarı basılmamalı.
///
/// ESC veya pencere kapat ile sonlandırılır.
/// 'R' tuşuna basılarak projeksiyon güncellemesi zorlanabilir.

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

constexpr float kPi = 3.14159265f;

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

/// @brief 2D n-gen düzenli poligon noktaları üretir.
std::vector<sdl_painter::Point> RegularPolygon(float cx, float cy, float r,
                                                int32_t sides,
                                                float offset_deg = 0.0f) {
  std::vector<sdl_painter::Point> pts;
  pts.reserve(static_cast<size_t>(sides));
  for (int32_t i = 0; i < sides; ++i) {
    const float theta =
        offset_deg * (kPi / 180.0f) +
        static_cast<float>(i) * (2.0f * kPi / static_cast<float>(sides));
    pts.push_back({cx + r * std::cos(theta), cy + r * std::sin(theta)});
  }
  return pts;
}

/// @brief 256x256 gradyan doku verisi üretir (kırmızıdan sarıya yatay geçiş).
std::vector<uint8_t> MakeGradientTexture() {
  constexpr int32_t kSize = 256;
  std::vector<uint8_t> data(static_cast<size_t>(kSize * kSize * 4));
  for (int32_t y = 0; y < kSize; ++y) {
    for (int32_t x = 0; x < kSize; ++x) {
      const float t = static_cast<float>(x) / static_cast<float>(kSize - 1);
      const size_t idx = static_cast<size_t>((y * kSize + x) * 4);
      data[idx + 0] = 235;                                        // R: sabit
      data[idx + 1] = static_cast<uint8_t>(t * 226.0f);          // G: 0→226
      data[idx + 2] = static_cast<uint8_t>((1.0f - t) * 100.0f); // B: 100→0
      data[idx + 3] = 255;
    }
  }
  return data;
}

}  // namespace

int main() {
  InitLogger();
  spdlog::info("SDLPainter Phase 5d (Vulkan polish & doğrulama) başlıyor...");
  spdlog::info("Pencereyi yeniden boyutlandırarak resize testini yapabilirsiniz.");
  spdlog::info("ESC veya pencereyi kapat ile çıkış.");

  if (!SDL_Init(SDL_INIT_VIDEO)) {
    spdlog::error("SDL_Init hata: {}", SDL_GetError());
    return 1;
  }

  SDL_Window* window = SDL_CreateWindow(
      "SDLPainter — Phase 5d (Vulkan Polish)", 900, 650,
      SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);
  if (!window) {
    spdlog::error("SDL_CreateWindow hata: {}", SDL_GetError());
    SDL_Quit();
    return 1;
  }

  {  // Painter scope: window'dan önce destroy edilmeli (surface leak önleme).
  sdl_painter::Painter painter(window, sdl_painter::RendererBackend::kVulkan);
  if (!painter.IsValid()) {
    spdlog::error("Vulkan Painter başlatılamadı.");
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 1;
  }

  // --- Gradyan texture ---
  auto grad_data = MakeGradientTexture();
  sdl_painter::Image grad_img =
      sdl_painter::Image::CreateFromData(grad_data.data(), 256, 256, 4);

  // --- Sabit renkler (Catppuccin Mocha paleti) ---
  const sdl_painter::Color kBg{30, 30, 46, 255};       // Base
  const sdl_painter::Color kRed{235, 66, 66, 255};
  const sdl_painter::Color kGreen{166, 227, 161, 255};
  const sdl_painter::Color kBlue{137, 180, 250, 255};
  const sdl_painter::Color kYellow{249, 226, 175, 255};
  const sdl_painter::Color kPeach{250, 179, 135, 200}; // yarı saydam
  const sdl_painter::Color kMauve{203, 166, 247, 255};
  const sdl_painter::Color kTeal{148, 226, 213, 255};
  const sdl_painter::Color kWhite{205, 214, 244, 255}; // Text
  const sdl_painter::Color kSurface{49, 50, 68, 255};  // Surface0

  float angle = 0.0f;  // animasyon açısı
  uint32_t frame_count = 0;

  bool running = true;
  while (running) {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
      if (event.type == SDL_EVENT_QUIT) running = false;
      if (event.type == SDL_EVENT_KEY_DOWN) {
        if (event.key.key == SDLK_ESCAPE) running = false;
      }
      // SDL_EVENT_WINDOW_RESIZED: Painter::Begin() otomatik algılar.
    }

    angle += 0.4f;
    if (angle >= 360.0f) angle -= 360.0f;
    ++frame_count;

    painter.Begin();
    painter.Clear(kBg);

    // -----------------------------------------------------------------------
    // 1. SOL SÜTUN — Dolu şekiller (FillRect, FillCircle, FillEllipse)
    // -----------------------------------------------------------------------

    // FillRect — kırmızı
    painter.SetBrush(sdl_painter::Brush(kRed));
    painter.SetPen(sdl_painter::Pen(kRed, 0.0f));
    painter.FillRect(20.0f, 20.0f, 120.0f, 80.0f);

    // FillCircle — yeşil
    painter.SetBrush(sdl_painter::Brush(kGreen));
    painter.SetPen(sdl_painter::Pen(kGreen, 0.0f));
    painter.FillCircle(80.0f, 200.0f, 55.0f);

    // FillEllipse — mavi
    painter.SetBrush(sdl_painter::Brush(kBlue));
    painter.SetPen(sdl_painter::Pen(kBlue, 0.0f));
    painter.FillEllipse(80.0f, 340.0f, 70.0f, 40.0f);

    // FillPolygon — altıgen, sarı
    painter.SetBrush(sdl_painter::Brush(kYellow));
    painter.SetPen(sdl_painter::Pen(kYellow, 0.0f));
    painter.FillPolygon(RegularPolygon(80.0f, 470.0f, 55.0f, 6, 30.0f));

    // -----------------------------------------------------------------------
    // 2. ORTA SÜTUN — Çerçeveli şekiller (DrawRect, DrawCircle, DrawEllipse)
    // -----------------------------------------------------------------------

    // DrawRect — teal çerçeve, kalın
    painter.SetPen(sdl_painter::Pen(kTeal, 3.0f));
    painter.SetBrush(sdl_painter::Brush({0, 0, 0, 0}));
    painter.DrawRect(210.0f, 20.0f, 120.0f, 80.0f);

    // DrawCircle — mauve çerçeve
    painter.SetPen(sdl_painter::Pen(kMauve, 2.5f));
    painter.DrawCircle(270.0f, 200.0f, 55.0f);

    // DrawEllipse — sarı çerçeve
    painter.SetPen(sdl_painter::Pen(kYellow, 2.0f));
    painter.DrawEllipse(270.0f, 340.0f, 70.0f, 40.0f);

    // DrawPolygon — sekizgen, beyaz çerçeve
    painter.SetPen(sdl_painter::Pen(kWhite, 2.0f));
    painter.DrawPolygon(RegularPolygon(270.0f, 470.0f, 55.0f, 8));

    // -----------------------------------------------------------------------
    // 3. TRANSFORM + ANİMASYON — dönen şekiller (orta sağ)
    // -----------------------------------------------------------------------

    // Dönen kırmızı dikdörtgen
    painter.Save();
    painter.Translate(450.0f, 100.0f);
    painter.Rotate(angle);
    painter.SetBrush(sdl_painter::Brush(kRed));
    painter.SetPen(sdl_painter::Pen(kRed, 0.0f));
    painter.FillRect(-55.0f, -35.0f, 110.0f, 70.0f);
    painter.Restore();

    // Dönen beşgen (ters yön)
    painter.Save();
    painter.Translate(450.0f, 260.0f);
    painter.Rotate(-angle * 0.7f);
    painter.SetBrush(sdl_painter::Brush(kMauve));
    painter.SetPen(sdl_painter::Pen(kMauve, 0.0f));
    painter.FillPolygon(RegularPolygon(0.0f, 0.0f, 55.0f, 5, -90.0f));
    painter.Restore();

    // Scale test — büyüyen/küçülen daire
    const float scale_factor = 0.6f + 0.4f * std::sin(angle * kPi / 180.0f);
    painter.Save();
    painter.Translate(450.0f, 430.0f);
    painter.Scale(scale_factor, scale_factor);
    painter.SetBrush(sdl_painter::Brush(kTeal));
    painter.SetPen(sdl_painter::Pen(kTeal, 0.0f));
    painter.FillCircle(0.0f, 0.0f, 50.0f);
    painter.Restore();

    // -----------------------------------------------------------------------
    // 4. TEXTURE — sağ sütun
    // -----------------------------------------------------------------------

    // Sabit gradyan texture
    painter.DrawImage(grad_img, sdl_painter::Rect{590.0f, 20.0f, 200.0f, 150.0f});

    // %50 opacity ile texture
    painter.SetOpacity(0.5f);
    painter.DrawImage(grad_img, sdl_painter::Rect{590.0f, 190.0f, 200.0f, 150.0f});
    painter.SetOpacity(1.0f);

    // Dönen texture (transform + DrawImage)
    painter.Save();
    painter.Translate(690.0f, 430.0f);
    painter.Rotate(angle * 0.5f);
    painter.DrawImage(grad_img, sdl_painter::Rect{-75.0f, -75.0f, 150.0f, 150.0f});
    painter.Restore();

    // -----------------------------------------------------------------------
    // 5. OPACITY KORELASYOnu — üst bantda şeffaf katmanlar
    // -----------------------------------------------------------------------

    // Yarı saydam peach dikdörtgen (tüm genişliğe yayılmış, alt kısım)
    painter.SetOpacity(0.35f);
    painter.SetBrush(sdl_painter::Brush(kPeach));
    painter.SetPen(sdl_painter::Pen(kPeach, 0.0f));
    painter.FillRect(0.0f, 590.0f, 900.0f, 60.0f);
    painter.SetOpacity(1.0f);

    // Surface0 arka plan dikdörtgeni alt panele
    painter.SetBrush(sdl_painter::Brush(kSurface));
    painter.SetPen(sdl_painter::Pen(kSurface, 0.0f));
    painter.FillRect(0.0f, 580.0f, 900.0f, 10.0f);

    // -----------------------------------------------------------------------
    // 6. ÇİZGİLER — DrawLine + DrawPolyline
    // -----------------------------------------------------------------------

    // Yatay çizgi (çerçeve test)
    painter.SetPen(sdl_painter::Pen(kWhite, 1.5f));
    painter.DrawLine(20.0f, 540.0f, 560.0f, 540.0f);

    // Yıldırım şekli polyline
    const std::vector<sdl_painter::Point> lightning = {
        {600.0f, 530.0f}, {640.0f, 555.0f}, {625.0f, 555.0f},
        {665.0f, 580.0f}, {630.0f, 568.0f}, {645.0f, 568.0f},
        {610.0f, 590.0f},
    };
    painter.SetPen(sdl_painter::Pen(kYellow, 2.0f));
    painter.DrawPolyline(lightning);

    painter.End();
  }

  spdlog::info("Kapatiliyor. Toplam frame: {}", frame_count);
  }  // painter destructor burada çalışır — VkSurface henüz geçerli.
  SDL_DestroyWindow(window);
  SDL_Quit();
  return 0;
}
