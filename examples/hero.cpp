/// @brief hero — README'deki tanıtım GIF'ini üreten koreografili sahne.
///
/// Diğer demolardan farkı: burada amaç bir yeteneği izole etmek değil,
/// kütüphanenin yaptıklarını tek karede birlikte göstermek. İçerik
/// **döngüye uygun** tasarlandı: tüm animasyonlar kLoopFrames karede tam
/// bir periyot tamamlar, yani son kare ilk kareyle örtüşür ve GIF'te
/// görünür bir atlama olmaz.
///
/// İki modu var:
///   hero                          → pencerede oynat (ESC ile çık)
///   hero --dump-frames <dizin>    → kLoopFrames kareyi PPM olarak yaz ve çık
///
/// Kare dökümü ekran kaydına göre tercih edilir: imleç ve pencere çerçevesi
/// karışmaz, kare atlaması olmaz, döngü tam kapanır. Üretim akışı:
///   ./hero --dump-frames build/hero_frames
///   ./scripts/make-hero-gif.sh        (veya .\scripts\Make-HeroGif.ps1)

#include <SDL3/SDL.h>
#include <spdlog/sinks/ansicolor_sink.h>
#include <spdlog/spdlog.h>

#ifdef _WIN32
#include <windows.h>
#endif

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

#include "sdl_painter/brush.h"
#include "sdl_painter/color.h"
#include "sdl_painter/font.h"
#include "sdl_painter/geometry.h"
#include "sdl_painter/image.h"
#include "sdl_painter/painter.h"
#include "sdl_painter/pen.h"

// windows.h (spdlog üzerinden dolaylı gelebilir) DrawText'i DrawTextA/W
// makrosuna çevirir ve Painter::DrawText çağrısını bozar.
#ifdef DrawText
#undef DrawText
#endif

namespace sp = sdl_painter;

namespace {

constexpr int32_t kWidth = 800;
constexpr int32_t kHeight = 450;

/// Döngü uzunluğu: 30 fps × 240 kare = 8 saniye.
constexpr int32_t kLoopFrames = 240;
constexpr float kPi = 3.14159265358979F;

const sp::Color kBackground{18, 20, 30, 255};
const sp::Color kAccentRed{235, 90, 90, 255};
const sp::Color kAccentGreen{140, 220, 160, 255};
const sp::Color kAccentBlue{120, 175, 250, 255};
const sp::Color kAccentYellow{245, 205, 120, 255};
const sp::Color kAccentPurple{190, 150, 245, 255};
const sp::Color kInk{225, 232, 245, 255};
const sp::Color kMuted{130, 145, 175, 255};

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
  logger->set_level(spdlog::level::info);
  spdlog::set_default_logger(logger);
}

/// @brief Sistemde mevcut bir TTF fontu ara; bulunamazsa boş string.
std::string FindSystemFont() {
  const char* candidates[] = {
#ifdef _WIN32
      "C:/Windows/Fonts/segoeui.ttf",
      "C:/Windows/Fonts/arial.ttf",
      "C:/Windows/Fonts/calibri.ttf",
#else
      "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
      "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf",
      "/usr/share/fonts/truetype/freefont/FreeSans.ttf",
#endif
  };
  for (const char* path : candidates) {
    if (SDL_IOFromFile(path, "rb") != nullptr) {
      return path;
    }
  }
  return {};
}

/// @brief [0,1) döngü konumundan yumuşak, periyodik bir 0→1→0 zarfı.
float PulseEnvelope(float phase) {
  return 0.5F - 0.5F * std::cos(phase * 2.0F * kPi);
}

/// @brief Prosedürel dama tahtası + degrade doku (texture yolunu göstermek için).
sp::Image MakeCheckerImage() {
  constexpr int32_t kSize = 128;
  std::vector<uint8_t> pixels(static_cast<std::size_t>(kSize) * kSize * 4);
  for (int32_t y = 0; y < kSize; ++y) {
    for (int32_t x = 0; x < kSize; ++x) {
      const bool dark = ((x / 16) + (y / 16)) % 2 == 0;
      const auto ramp = static_cast<uint8_t>(x * 255 / (kSize - 1));
      const auto idx = static_cast<std::size_t>(y * kSize + x) * 4;
      pixels[idx + 0] = dark ? 60 : ramp;
      pixels[idx + 1] = dark ? 90 : static_cast<uint8_t>(140 + ramp / 4);
      pixels[idx + 2] = dark ? 150 : 235;
      pixels[idx + 3] = 255;
    }
  }
  return sp::Image::CreateFromData(pixels.data(), kSize, kSize, 4);
}

/// @brief Merkezi (cx,cy) olan beş köşeli yıldız — konkav poligon örneği.
std::vector<sp::Point> MakeStar(float cx, float cy, float outer, float inner) {
  std::vector<sp::Point> pts;
  pts.reserve(10);
  for (int32_t i = 0; i < 10; ++i) {
    const float r = (i % 2 == 0) ? outer : inner;
    const float a = static_cast<float>(i) * kPi / 5.0F - kPi / 2.0F;
    pts.push_back({cx + r * std::cos(a), cy + r * std::sin(a)});
  }
  return pts;
}

// ---------------------------------------------------------------------------
// Sahne
// ---------------------------------------------------------------------------

/// @brief Tek bir kareyi çizer. `frame` [0, kLoopFrames) aralığında.
void DrawScene(sp::Painter& painter, const sp::Image& texture,
               const std::shared_ptr<sp::Font>& font_lg,
               const std::shared_ptr<sp::Font>& font_sm, int32_t frame) {
  const float phase = static_cast<float>(frame) / kLoopFrames;  // [0,1)
  const float spin = phase * 360.0F;  // tam tur → döngü kapanır
  const float pulse = PulseEnvelope(phase);

  painter.Clear(kBackground);

  // --- Sol üst: temel primitifler (dolu + çerçeve) ------------------------
  painter.SetBrush(sp::Brush(kAccentRed));
  painter.SetPen(sp::Pen(kAccentRed, 0.0F));
  painter.FillRect(40.0F, 40.0F, 90.0F, 60.0F);

  painter.SetBrush(sp::Brush(sp::Color{0, 0, 0, 0}));
  painter.SetPen(sp::Pen(kAccentYellow, 3.0F));
  painter.DrawRect(40.0F, 115.0F, 90.0F, 60.0F);

  // Nabız gibi ölçeklenen daire — yarıçapa göre adaptif tessellation.
  painter.SetBrush(sp::Brush(kAccentGreen));
  painter.SetPen(sp::Pen(kAccentGreen, 0.0F));
  painter.FillCircle(200.0F, 75.0F, 28.0F + 10.0F * pulse);

  painter.SetBrush(sp::Brush(sp::Color{0, 0, 0, 0}));
  painter.SetPen(sp::Pen(kAccentBlue, 3.0F));
  painter.DrawEllipse(200.0F, 145.0F, 40.0F, 26.0F);

  // --- Kalın polyline: glLineWidth değil, quad geometrisi -----------------
  painter.SetPen(sp::Pen(kAccentPurple, 7.0F));
  {
    // Segment sayısı bilinçli olarak yüksek: kalın çizgiler quad tabanlı ve
    // köşe birleşimi (join) yok, bu yüzden az segmentte kıvrımlarda çentik
    // oluşuyor. Küçük açılarda çentik görünmez hâle geliyor.
    constexpr int32_t kWaveSegments = 56;
    std::vector<sp::Point> path;
    path.reserve(kWaveSegments);
    for (int32_t i = 0; i < kWaveSegments; ++i) {
      const float t = static_cast<float>(i) / (kWaveSegments - 1);
      const float x = 40.0F + t * 240.0F;
      // Dalga bir tam periyot kayar → döngü kapanır.
      const float y = 230.0F + 26.0F * std::sin(t * 3.0F * kPi + phase * 2.0F * kPi);
      path.push_back({x, y});
    }
    painter.DrawPolyline(path);
  }

  // --- Konkav poligon (ear clipping), kendi merkezinde döner --------------
  painter.Save();
  painter.Translate(105.0F, 355.0F);
  painter.Rotate(-spin);
  painter.SetBrush(sp::Brush(kAccentYellow));
  painter.SetPen(sp::Pen(kAccentYellow, 0.0F));
  painter.FillPolygon(MakeStar(0.0F, 0.0F, 58.0F, 24.0F));
  painter.Restore();

  // --- Orta: iç içe transform stack ---------------------------------------
  painter.Save();
  painter.Translate(400.0F, 225.0F);
  painter.Rotate(spin);
  painter.SetBrush(sp::Brush(sp::Color{0, 0, 0, 0}));
  painter.SetPen(sp::Pen(kAccentBlue, 4.0F));
  painter.DrawRect(-70.0F, -70.0F, 140.0F, 140.0F);

  painter.Save();
  painter.Rotate(spin * 2.0F);
  painter.Scale(0.55F + 0.15F * pulse, 0.55F + 0.15F * pulse);
  painter.SetBrush(sp::Brush(kAccentRed));
  painter.SetPen(sp::Pen(kAccentRed, 0.0F));
  painter.FillRect(-70.0F, -70.0F, 140.0F, 140.0F);
  painter.Restore();
  painter.Restore();

  // --- Sağ üst: texture (ölçekleme + döndürme + alpha) --------------------
  painter.Save();
  painter.Translate(645.0F, 120.0F);
  painter.Rotate(-spin);
  painter.SetOpacity(0.9F);
  painter.DrawImage(texture, sp::Rect{-70.0F, -70.0F, 140.0F, 140.0F});
  painter.SetOpacity(1.0F);
  painter.Restore();

  // --- Sağ alt: opaklık merdiveni -----------------------------------------
  for (int32_t i = 0; i < 4; ++i) {
    painter.SetOpacity(0.25F + 0.25F * static_cast<float>(i));
    painter.SetBrush(sp::Brush(kAccentGreen));
    painter.SetPen(sp::Pen(kAccentGreen, 0.0F));
    painter.FillCircle(560.0F + static_cast<float>(i) * 62.0F, 330.0F, 24.0F);
  }
  painter.SetOpacity(1.0F);

  // --- Metin ---------------------------------------------------------------
  if (font_lg && font_sm) {
    painter.SetFont(font_lg);
    painter.SetPen(sp::Pen(kInk, 0.0F));
    painter.DrawText(sp::Rect{280.0F, 372.0F, 480.0F, 40.0F}, "SDLPainter",
                     sp::Alignment::kRight);

    painter.SetFont(font_sm);
    painter.SetPen(sp::Pen(kMuted, 0.0F));
    painter.DrawText(sp::Rect{280.0F, 410.0F, 480.0F, 24.0F},
                     "2B drawing for SDL3  ·  OpenGL + Vulkan",
                     sp::Alignment::kRight);
  }
}

// ---------------------------------------------------------------------------
// Kare dökümü
// ---------------------------------------------------------------------------

// GL fonksiyonları Windows'ta __stdcall (APIENTRY) kullanır; SDLCALL (__cdecl)
// değil. x64'te tek çağırma kuralı olduğu için fark görünmez, 32-bit derlemede
// stack'i bozar — bu yüzden doğru kural açıkça yazılıyor.
#ifdef APIENTRY
#define SDLPAINTER_GLCALL APIENTRY
#else
#define SDLPAINTER_GLCALL
#endif

using ReadPixelsFn = void(SDLPAINTER_GLCALL*)(int32_t, int32_t, int32_t, int32_t,
                                              uint32_t, uint32_t, void*);

constexpr uint32_t kGlRgb = 0x1907U;
constexpr uint32_t kGlUnsignedByte = 0x1401U;

/// @brief Geçerli back buffer'ı binary PPM (P6) olarak yazar.
///
/// glReadPixels'e glad üzerinden erişemiyoruz — glad kütüphaneye PRIVATE
/// linklenmiş durumda. SDL zaten public bağımlılık olduğu için fonksiyon
/// işaretçisini ondan alıyoruz; ek bağımlılık gerekmiyor.
///
/// PPM seçildi çünkü kayıpsız, tek başlıklı ve hiçbir kütüphane istemiyor;
/// ffmpeg diziyi doğrudan okuyor.
bool WriteFramePpm(ReadPixelsFn read_pixels, const std::string& path,
                   int32_t width, int32_t height, std::vector<uint8_t>& scratch) {
  const auto row = static_cast<std::size_t>(width) * 3;
  scratch.resize(row * static_cast<std::size_t>(height));
  read_pixels(0, 0, width, height, kGlRgb, kGlUnsignedByte, scratch.data());

  FILE* f = std::fopen(path.c_str(), "wb");
  if (f == nullptr) {
    return false;
  }
  std::fprintf(f, "P6\n%d %d\n255\n", width, height);
  // OpenGL'in orijini sol ALT köşede; PPM sol üstten başlar → satırları ters yaz.
  for (int32_t y = height - 1; y >= 0; --y) {
    std::fwrite(scratch.data() + static_cast<std::size_t>(y) * row, 1, row, f);
  }
  std::fclose(f);
  return true;
}

}  // namespace

int main(int argc, char** argv) {
  InitLogger();

  std::string dump_dir;
  for (int32_t i = 1; i < argc; ++i) {
    const std::string arg = argv[i];
    if (arg == "--dump-frames" && i + 1 < argc) {
      dump_dir = argv[++i];
    } else if (arg == "--help" || arg == "-h") {
      spdlog::info("kullanim: hero [--dump-frames <dizin>]");
      return 0;
    }
  }
  const bool dumping = !dump_dir.empty();

  if (!SDL_Init(SDL_INIT_VIDEO)) {
    spdlog::error("SDL_Init basarisiz: {}", SDL_GetError());
    return 1;
  }

  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
  SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1);
  SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 4);

  // Döküm modunda pencere görünmez: ekranda titremesin, önüne bir şey
  // gelirse kareler bozulmasın.
  SDL_WindowFlags flags = SDL_WINDOW_OPENGL;
  if (dumping) {
    flags |= SDL_WINDOW_HIDDEN;
  }

  SDL_Window* window =
      SDL_CreateWindow("SDLPainter — hero", kWidth, kHeight, flags);
  if (window == nullptr) {
    spdlog::error("SDL_CreateWindow basarisiz: {}", SDL_GetError());
    SDL_Quit();
    return 1;
  }

  auto* read_pixels =
      reinterpret_cast<ReadPixelsFn>(SDL_GL_GetProcAddress("glReadPixels"));
  if (dumping && read_pixels == nullptr) {
    spdlog::error("glReadPixels alinamadi; kare dokumu yapilamiyor.");
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 1;
  }

  int32_t exit_code = 0;
  {
    sp::Painter painter(window, sp::RendererBackend::kOpenGL);
    if (!painter.IsValid()) {
      spdlog::error("Painter baslatilamadi.");
      SDL_DestroyWindow(window);
      SDL_Quit();
      return 1;
    }

    const sp::Image texture = MakeCheckerImage();

    std::shared_ptr<sp::Font> font_lg;
    std::shared_ptr<sp::Font> font_sm;
    const std::string font_path = FindSystemFont();
    if (!font_path.empty()) {
      font_lg = std::make_shared<sp::Font>(font_path, 34);
      font_sm = std::make_shared<sp::Font>(font_path, 16);
      if (!font_lg->IsValid() || !font_sm->IsValid()) {
        font_lg.reset();
        font_sm.reset();
      }
    }
    if (!font_lg) {
      spdlog::warn("Sistem fontu bulunamadi; sahne metinsiz cizilecek.");
    }

    std::vector<uint8_t> scratch;
    int32_t frame = 0;
    bool running = true;

    while (running) {
      SDL_Event event;
      while (SDL_PollEvent(&event)) {
        if (event.type == SDL_EVENT_QUIT) {
          running = false;
        }
        if (event.type == SDL_EVENT_KEY_DOWN &&
            event.key.key == SDLK_ESCAPE) {
          running = false;
        }
      }

      painter.Begin();
      DrawScene(painter, texture, font_lg, font_sm, frame % kLoopFrames);

      if (dumping) {
        // ClearClip() batcher'i bosaltir ama swap ETMEZ. End() ise once
        // flush edip hemen ardindan SDL_GL_SwapWindow cagirir; swap'ten
        // sonra back buffer'in icerigi tanimsizdir. Bu yuzden kareyi
        // buradan, flush edilmis ve henuz swap edilmemis back buffer'dan
        // okuyoruz.
        painter.ClearClip();
        char name[64];
        std::snprintf(name, sizeof(name), "/frame_%04d.ppm", frame);
        if (!WriteFramePpm(read_pixels, dump_dir + name, kWidth, kHeight,
                           scratch)) {
          spdlog::error("Kare yazilamadi: {}{} (dizin var mi?)", dump_dir, name);
          exit_code = 1;
          running = false;
        }
      }

      painter.End();

      ++frame;
      if (dumping && frame >= kLoopFrames) {
        running = false;
      }
    }

    if (dumping && exit_code == 0) {
      spdlog::info("{} kare yazildi ({}x{}) -> {}", kLoopFrames, kWidth, kHeight,
                   dump_dir);
      spdlog::info("Simdi: scripts/make-hero-gif.sh (veya Make-HeroGif.ps1)");
    }
  }

  SDL_DestroyWindow(window);
  SDL_Quit();
  return exit_code;
}
