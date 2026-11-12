/// @file renderer_comparison.cpp
/// @brief Aynı sahneyi SDLPainter ve SDL_Renderer ile çizip kare süresini
///        karşılaştırır.
///
/// Senaryolar `batching_benchmark` ile aynı yerleşimi kullanır (kenarı 6-16
/// piksel, şekil başına rastgele renk). Kenar yumuşatma kapalıdır, çünkü
/// SDL_Renderer'da yoktur.
///
///   fill_rects    FillRect               / SDL_RenderFillRect
///   fill_circles  FillCircle             / 16 dilimli fan + SDL_RenderGeometry
///   sprites       DrawImage (16x16 doku) / SDL_RenderTexture
///
/// SDL_Renderer'da daire yoktur; kullanıcı kendisi üçgen fanı kurar. Fan,
/// SDLPainter'ın bu yarıçaplarda ürettiği kadar (16 dilim) vertex içerir ve
/// her karede yeniden hesaplanır.
///
/// Süre sunum dahil tam karedir: `Painter::End` swap/present yapar, SDL'de
/// karşılığı `SDL_RenderPresent`. Vsync kapalıdır; GPU kuyruğu yüzünden tek
/// kare dalgalansa da uzun koşunun ortalaması gerçek verimi verir.
///
/// Tahsis sayımı: bu programın global `operator new`'u sayar. Kütüphane
/// varsayılan olarak statik bağlandığı için onun tahsisleri de sayılır;
/// paylaşımlı derlemede Windows'ta sayılmaz. SDL kendi `SDL_malloc`'unu
/// kullandığı için SDL_Renderer satırında bu sütun boştur.
///
/// Kullanım:
///   renderer_comparison --impl=painter-gl|painter-vk|sdl [--sdl-driver=AD]
///                       [--shapes=N] [--frames=N] [--csv=DOSYA]
///                       [--screenshot=DIZIN]
///
///   --sdl-driver  SDL_Renderer arka ucu (opengl, direct3d11, vulkan...);
///                 boşsa SDL'in varsayılanı.
///   --csv         Sonuçları CSV dosyasına ekler (yoksa başlıkla oluşturur);
///                 farklı --impl koşuları tek dosyada birleşir.
///   --screenshot  Her senaryonun karesini `<impl>-<senaryo>.png` olarak
///                 kaydet (painter-gl ve sdl).

// clang-format off
#include <SDL3/SDL.h>
#include <spdlog/spdlog.h>
#include <stb_image_write.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <memory>
#include <new>
#include <string>
#include <utility>
#include <vector>

#include "bench_scene.h"
#include "counting_renderer.h"
#include "screenshot.h"
#include "sdl_painter/brush.h"
#include "sdl_painter/color.h"
#include "sdl_painter/geometry.h"
#include "sdl_painter/image.h"
#include "sdl_painter/painter.h"
// clang-format on

// Tahsis sayacı
//
// Yalnızca sayım geçişinde açılır; süre ölçülürken atomik artış maliyeti
// sonuca girmez.

namespace {
std::atomic<bool> g_count_allocations{false};
std::atomic<uint64_t> g_allocations{0};
}  // namespace

void* operator new(std::size_t size) {
  if (g_count_allocations.load(std::memory_order_relaxed)) {
    g_allocations.fetch_add(1, std::memory_order_relaxed);
  }
  void* p = std::malloc(size == 0 ? 1 : size);
  if (p == nullptr) {
    throw std::bad_alloc();
  }
  return p;
}

void operator delete(void* p) noexcept {
  std::free(p);
}

void operator delete(void* p, std::size_t /*size*/) noexcept {
  std::free(p);
}

namespace {

using bench::kViewportHeight;
using bench::kViewportWidth;
using bench::MakePlacements;
using bench::Placement;
using sdl_painter::Brush;
using sdl_painter::Color;
using sdl_painter::Image;
using sdl_painter::Painter;
using sdl_painter::Rect;

constexpr int32_t kWarmupFrames = 30;
constexpr int32_t kAllocationFrames = 10;
constexpr int32_t kSpriteSize = 16;
// Tessellator::AdaptiveSegments, AA kapalıyken yarıçap 32'nin altında 16 verir;
// bu sahnedeki daireler 3-8 piksel.
constexpr int32_t kCircleSegments = 16;
constexpr float kTwoPi = 6.28318530718F;
const Color kBackground{20, 22, 28, 255};

#ifdef _WIN32
#define BENCH_GLAPI __stdcall
#else
#define BENCH_GLAPI
#endif

/// @brief Ölçümün hangi sürücü ve GPU'da yapıldığı.
struct RunInfo {
  std::string driver;
  std::string device;  ///< Yalnızca OpenGL'de bilinir (`GL_RENDERER`).
};

/// @brief Geçerli OpenGL context'inin `GL_RENDERER` metni; birden çok GPU'lu
///        makinede ölçümün hangi kartta yapıldığını kaydetmek için.
std::string GlRendererName() {
  using GlGetStringFn = const unsigned char*(BENCH_GLAPI*)(uint32_t);
  constexpr uint32_t kGlRenderer = 0x1F01;
  auto get_string =
      reinterpret_cast<GlGetStringFn>(SDL_GL_GetProcAddress("glGetString"));
  const unsigned char* name =
      get_string != nullptr ? get_string(kGlRenderer) : nullptr;
  return name != nullptr ? reinterpret_cast<const char*>(name) : "";
}

enum class Impl : uint8_t { kPainterGl, kPainterVk, kSdl };

enum class Scenario : uint8_t { kFillRects, kFillCircles, kSprites };

struct ScenarioInfo {
  Scenario id;
  const char* name;
};

constexpr ScenarioInfo kScenarios[] = {
    {Scenario::kFillRects, "fill_rects"},
    {Scenario::kFillCircles, "fill_circles"},
    {Scenario::kSprites, "sprites"},
};

struct Options {
  Impl impl{Impl::kPainterGl};
  std::string impl_name{"painter-gl"};
  std::string sdl_driver;
  int32_t shapes{2000};
  int32_t frames{300};
  std::string csv_path;
  std::string screenshot_dir;
};

/// @brief Bir senaryonun ölçümü. Bilinmeyen sayaçlar negatiftir.
struct Result {
  std::string scenario;
  double ms_avg{0.0};
  double ms_median{0.0};
  double ms_min{0.0};
  double draw_calls{-1.0};
  double allocations{-1.0};
};

/// @brief 16x16 RGBA sprite: kenarı yumuşak, köşeden köşeye renk geçişli
///        bir disk. İki kütüphane aynı pikselleri yükler.
std::vector<uint8_t> MakeSpritePixels() {
  std::vector<uint8_t> px(static_cast<std::size_t>(kSpriteSize) * kSpriteSize *
                          4U);
  const float kCentre = (static_cast<float>(kSpriteSize) - 1.0F) * 0.5F;
  const float kRadius = static_cast<float>(kSpriteSize) * 0.5F;
  for (int32_t y = 0; y < kSpriteSize; ++y) {
    for (int32_t x = 0; x < kSpriteSize; ++x) {
      const float dx = static_cast<float>(x) - kCentre;
      const float dy = static_cast<float>(y) - kCentre;
      const float edge = kRadius - std::sqrt((dx * dx) + (dy * dy));
      const float alpha = std::clamp(edge, 0.0F, 1.0F);
      const std::size_t i = (static_cast<std::size_t>(y) * kSpriteSize +
                             static_cast<std::size_t>(x)) *
                            4U;
      px[i + 0] = static_cast<uint8_t>(255 * x / (kSpriteSize - 1));
      px[i + 1] = static_cast<uint8_t>(255 * y / (kSpriteSize - 1));
      px[i + 2] = 200;
      px[i + 3] = static_cast<uint8_t>(std::lround(alpha * 255.0F));
    }
  }
  return px;
}

/// @brief Kareleri ölçer: ısınma, süre geçişi ve (istenirse) tahsis geçişi.
Result Measure(const char* scenario, int32_t frames,
               const std::function<void()>& frame, bool count_allocations) {
  using Clock = std::chrono::steady_clock;
  for (int32_t i = 0; i < kWarmupFrames; ++i) {
    frame();
  }

  std::vector<double> ms(static_cast<std::size_t>(frames));
  for (double& m : ms) {
    const auto t0 = Clock::now();
    frame();
    m = std::chrono::duration<double, std::milli>(Clock::now() - t0).count();
  }

  Result r;
  r.scenario = scenario;
  double total = 0.0;
  for (const double m : ms) {
    total += m;
  }
  r.ms_avg = total / static_cast<double>(frames);
  std::sort(ms.begin(), ms.end());
  r.ms_median = ms[ms.size() / 2];
  r.ms_min = ms.front();

  if (count_allocations) {
    g_allocations.store(0, std::memory_order_relaxed);
    g_count_allocations.store(true, std::memory_order_relaxed);
    for (int32_t i = 0; i < kAllocationFrames; ++i) {
      frame();
    }
    g_count_allocations.store(false, std::memory_order_relaxed);
    r.allocations =
        static_cast<double>(g_allocations.load(std::memory_order_relaxed)) /
        kAllocationFrames;
  }
  return r;
}

// SDLPainter

void DrawPainterScenario(Painter& p, Scenario scenario,
                         const std::vector<Placement>& items,
                         const Image& sprite) {
  switch (scenario) {
    case Scenario::kFillRects:
      for (const auto& it : items) {
        p.SetBrush(Brush(it.color));
        p.FillRect(it.x, it.y, it.size, it.size);
      }
      break;
    case Scenario::kFillCircles:
      for (const auto& it : items) {
        const float r = it.size * 0.5F;
        p.SetBrush(Brush(it.color));
        p.FillCircle(it.x + r, it.y + r, r);
      }
      break;
    case Scenario::kSprites:
      for (const auto& it : items) {
        p.DrawImage(sprite, Rect{it.x, it.y, it.size, it.size});
      }
      break;
  }
}

bool RunPainter(const Options& opt, const std::vector<Placement>& items,
                std::vector<Result>& results, RunInfo& info) {
  const bool vulkan = opt.impl == Impl::kPainterVk;
  const auto backend = vulkan ? sdl_painter::RendererBackend::kVulkan
                              : sdl_painter::RendererBackend::kOpenGL;
  SDL_Window* window =
      SDL_CreateWindow("renderer_comparison", kViewportWidth, kViewportHeight,
                       vulkan ? SDL_WINDOW_VULKAN : SDL_WINDOW_OPENGL);
  if (window == nullptr) {
    std::printf("Pencere olusturulamadi: %s\n", SDL_GetError());
    return false;
  }
  std::unique_ptr<sdl_painter::IRenderer> inner =
      sdl_painter::CreateRenderer(backend);
  if (inner == nullptr || !inner->Initialize(window)) {
    std::printf("%s renderer baslatilamadi.\n", opt.impl_name.c_str());
    SDL_DestroyWindow(window);
    return false;
  }
  if (!vulkan) {
    // Vsync kapali olmali; aksi halde olculen sey ekranin tazeleme hizi olur.
    // Vulkan'da sunum modu swapchain'de secilir (destekleniyorsa MAILBOX).
    SDL_GL_SetSwapInterval(0);
    info.device = GlRendererName();
  }
  info.driver = vulkan ? "vulkan" : "opengl";

  auto counting =
      std::make_unique<bench::CountingRenderer>(std::move(inner), backend);
  bench::CountingRenderer& counter = *counting;
  bool ok = true;
  {
    Painter painter(std::move(counting), kViewportWidth, kViewportHeight);
    if (!painter.IsValid()) {
      std::printf("Painter gecersiz.\n");
      ok = false;
    }
    const std::vector<uint8_t> pixels = MakeSpritePixels();
    const Image sprite =
        Image::CreateFromData(pixels.data(), kSpriteSize, kSpriteSize, 4);

    for (const auto& sc : kScenarios) {
      if (!ok) {
        break;
      }
      auto frame = [&] {
        SDL_PumpEvents();
        painter.Begin();
        painter.Clear(kBackground);
        DrawPainterScenario(painter, sc.id, items, sprite);
        painter.End();
      };
      counter.ResetStats();
      Result r =
          Measure(sc.name, opt.frames, frame, /*count_allocations=*/true);
      // Isinma ve tahsis kareleri de sayaca girdi; kare basina ortalama ayni.
      const auto counted_frames =
          static_cast<double>(kWarmupFrames + opt.frames + kAllocationFrames);
      r.draw_calls =
          static_cast<double>(counter.Stats().draw_calls) / counted_frames;
      results.push_back(r);

      if (!opt.screenshot_dir.empty()) {
        if (vulkan) {
          std::printf("  --screenshot Vulkan'da desteklenmiyor, atlandi.\n");
        } else {
          const std::string path =
              opt.screenshot_dir + "/" + opt.impl_name + "-" + sc.name + ".png";
          bool saved = false;
          counter.CaptureBeforeNextPresent([&] {
            saved =
                bench::SaveBackBufferPng(path, kViewportWidth, kViewportHeight);
          });
          frame();
          std::printf("  ekran goruntusu %s: %s\n",
                      saved ? "kaydedildi" : "ALINAMADI", path.c_str());
        }
      }
    }
  }
  SDL_DestroyWindow(window);
  return ok;
}

// SDL_Renderer

void SetDrawColor(SDL_Renderer* renderer, const Color& c) {
  SDL_SetRenderDrawColor(renderer, c.r, c.g, c.b, c.a);
}

/// @brief SDLPainter'ın dolu dairesiyle aynı fan: merkez + 16 çevre noktası.
void BuildCircleFan(std::vector<SDL_Vertex>& out, float cx, float cy, float r,
                    const Color& c) {
  const SDL_FColor kColor{
      static_cast<float>(c.r) / 255.0F, static_cast<float>(c.g) / 255.0F,
      static_cast<float>(c.b) / 255.0F, static_cast<float>(c.a) / 255.0F};
  SDL_FPoint rim[kCircleSegments];
  for (int32_t i = 0; i < kCircleSegments; ++i) {
    const float a =
        kTwoPi * static_cast<float>(i) / static_cast<float>(kCircleSegments);
    rim[i] = SDL_FPoint{cx + (std::cos(a) * r), cy + (std::sin(a) * r)};
  }
  out.clear();
  for (int32_t i = 0; i < kCircleSegments; ++i) {
    out.push_back(SDL_Vertex{SDL_FPoint{cx, cy}, kColor, SDL_FPoint{0, 0}});
    out.push_back(SDL_Vertex{rim[i], kColor, SDL_FPoint{0, 0}});
    out.push_back(
        SDL_Vertex{rim[(i + 1) % kCircleSegments], kColor, SDL_FPoint{0, 0}});
  }
}

void DrawSdlScenario(SDL_Renderer* renderer, Scenario scenario,
                     const std::vector<Placement>& items, SDL_Texture* sprite,
                     std::vector<SDL_Vertex>& fan) {
  switch (scenario) {
    case Scenario::kFillRects:
      for (const auto& it : items) {
        SetDrawColor(renderer, it.color);
        const SDL_FRect rect{it.x, it.y, it.size, it.size};
        SDL_RenderFillRect(renderer, &rect);
      }
      break;
    case Scenario::kFillCircles:
      for (const auto& it : items) {
        const float r = it.size * 0.5F;
        BuildCircleFan(fan, it.x + r, it.y + r, r, it.color);
        SDL_RenderGeometry(renderer, nullptr, fan.data(),
                           static_cast<int>(fan.size()), nullptr, 0);
      }
      break;
    case Scenario::kSprites:
      for (const auto& it : items) {
        const SDL_FRect dst{it.x, it.y, it.size, it.size};
        SDL_RenderTexture(renderer, sprite, nullptr, &dst);
      }
      break;
  }
}

bool SaveRendererPng(SDL_Renderer* renderer, const std::string& path) {
  SDL_Surface* raw = SDL_RenderReadPixels(renderer, nullptr);
  if (raw == nullptr) {
    return false;
  }
  SDL_Surface* rgba = SDL_ConvertSurface(raw, SDL_PIXELFORMAT_RGBA32);
  SDL_DestroySurface(raw);
  if (rgba == nullptr) {
    return false;
  }
  const int written = stbi_write_png(path.c_str(), rgba->w, rgba->h, 4,
                                     rgba->pixels, rgba->pitch);
  SDL_DestroySurface(rgba);
  return written != 0;
}

void PrintRenderDrivers() {
  std::printf("Kullanilabilir SDL_Renderer arka uclari:");
  for (int i = 0; i < SDL_GetNumRenderDrivers(); ++i) {
    std::printf(" %s", SDL_GetRenderDriver(i));
  }
  std::printf("\n");
}

bool RunSdl(const Options& opt, const std::vector<Placement>& items,
            std::vector<Result>& results, RunInfo& info) {
  SDL_Window* window = SDL_CreateWindow("renderer_comparison", kViewportWidth,
                                        kViewportHeight, 0);
  if (window == nullptr) {
    std::printf("Pencere olusturulamadi: %s\n", SDL_GetError());
    return false;
  }
  SDL_Renderer* renderer = SDL_CreateRenderer(
      window, opt.sdl_driver.empty() ? nullptr : opt.sdl_driver.c_str());
  if (renderer == nullptr) {
    std::printf("SDL_Renderer olusturulamadi: %s\n", SDL_GetError());
    PrintRenderDrivers();
    SDL_DestroyWindow(window);
    return false;
  }
  SDL_SetRenderVSync(renderer, 0);
  info.driver = SDL_GetRendererName(renderer);
  if (info.driver == "opengl") {
    info.device = GlRendererName();
  }

  const std::vector<uint8_t> pixels = MakeSpritePixels();
  SDL_Texture* sprite =
      SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32,
                        SDL_TEXTUREACCESS_STATIC, kSpriteSize, kSpriteSize);
  bool ok =
      sprite != nullptr &&
      SDL_UpdateTexture(sprite, nullptr, pixels.data(), kSpriteSize * 4) &&
      SDL_SetTextureBlendMode(sprite, SDL_BLENDMODE_BLEND);
  if (!ok) {
    std::printf("Sprite dokusu olusturulamadi: %s\n", SDL_GetError());
  }

  std::vector<SDL_Vertex> fan;
  fan.reserve(static_cast<std::size_t>(kCircleSegments) * 3U);
  for (const auto& sc : kScenarios) {
    if (!ok) {
      break;
    }
    auto draw = [&] {
      SetDrawColor(renderer, kBackground);
      SDL_RenderClear(renderer);
      DrawSdlScenario(renderer, sc.id, items, sprite, fan);
    };
    auto frame = [&] {
      SDL_PumpEvents();
      draw();
      SDL_RenderPresent(renderer);
    };
    results.push_back(
        Measure(sc.name, opt.frames, frame, /*count_allocations=*/false));

    if (!opt.screenshot_dir.empty()) {
      const std::string path =
          opt.screenshot_dir + "/sdl-" + info.driver + "-" + sc.name + ".png";
      draw();
      const bool saved = SaveRendererPng(renderer, path);
      SDL_RenderPresent(renderer);
      std::printf("  ekran goruntusu %s: %s\n",
                  saved ? "kaydedildi" : "ALINAMADI", path.c_str());
    }
  }

  if (sprite != nullptr) {
    SDL_DestroyTexture(sprite);
  }
  SDL_DestroyRenderer(renderer);
  SDL_DestroyWindow(window);
  return ok;
}

// Çıktı

std::string FormatOptional(double value, const char* format) {
  if (value < 0.0) {
    return "-";
  }
  char buf[32];
  std::snprintf(buf, sizeof(buf), format, value);
  return buf;
}

void PrintTable(const Options& opt, const RunInfo& info,
                const std::vector<Result>& results) {
  std::printf("\nimpl=%s  surucu=%s  sekil=%d  kare=%d  viewport=%dx%d\n",
              opt.impl_name.c_str(), info.driver.c_str(), opt.shapes,
              opt.frames, kViewportWidth, kViewportHeight);
  if (!info.device.empty()) {
    std::printf("GPU: %s\n", info.device.c_str());
  }
  std::printf("%-14s %9s %10s %9s %10s %12s\n", "senaryo", "ms(ort)",
              "ms(medyan)", "ms(min)", "draw/kare", "tahsis/kare");
  std::printf("%s\n", std::string(69, '-').c_str());
  for (const auto& r : results) {
    std::printf("%-14s %9.3f %10.3f %9.3f %10s %12s\n", r.scenario.c_str(),
                r.ms_avg, r.ms_median, r.ms_min,
                FormatOptional(r.draw_calls, "%.1f").c_str(),
                FormatOptional(r.allocations, "%.0f").c_str());
  }
}

bool AppendCsv(const Options& opt, const RunInfo& info,
               const std::vector<Result>& results) {
  std::FILE* f = std::fopen(opt.csv_path.c_str(), "a");
  if (f == nullptr) {
    return false;
  }
  std::fseek(f, 0, SEEK_END);
  if (std::ftell(f) == 0) {
    std::fprintf(f,
                 "impl,driver,scenario,shapes,frames,ms_avg,ms_median,ms_min,"
                 "draw_calls_per_frame,allocations_per_frame,device\n");
  }
  for (const auto& r : results) {
    const std::string draws =
        r.draw_calls < 0.0 ? "" : FormatOptional(r.draw_calls, "%.1f");
    const std::string allocs =
        r.allocations < 0.0 ? "" : FormatOptional(r.allocations, "%.0f");
    std::fprintf(f, "%s,%s,%s,%d,%d,%.4f,%.4f,%.4f,%s,%s,\"%s\"\n",
                 opt.impl_name.c_str(), info.driver.c_str(), r.scenario.c_str(),
                 opt.shapes, opt.frames, r.ms_avg, r.ms_median, r.ms_min,
                 draws.c_str(), allocs.c_str(), info.device.c_str());
  }
  std::fclose(f);
  return true;
}

/// @brief `--anahtar=deger` biçimli argümandan tamsayı oku.
bool ParseIntArg(const char* arg, const char* key, int32_t& out) {
  const std::size_t len = std::strlen(key);
  if (std::strncmp(arg, key, len) != 0) {
    return false;
  }
  out = static_cast<int32_t>(std::atoi(arg + len));
  return true;
}

/// @brief `--anahtar=deger` biçimli argümandan metin oku.
bool ParseStringArg(const char* arg, const char* key, std::string& out) {
  const std::size_t len = std::strlen(key);
  if (std::strncmp(arg, key, len) != 0) {
    return false;
  }
  out = arg + len;
  return true;
}

bool ParseOptions(int argc, char** argv, Options& opt) {
  for (int i = 1; i < argc; ++i) {
    const char* a = argv[i];
    if (ParseStringArg(a, "--impl=", opt.impl_name) ||
        ParseStringArg(a, "--sdl-driver=", opt.sdl_driver) ||
        ParseIntArg(a, "--shapes=", opt.shapes) ||
        ParseIntArg(a, "--frames=", opt.frames) ||
        ParseStringArg(a, "--csv=", opt.csv_path) ||
        ParseStringArg(a, "--screenshot=", opt.screenshot_dir)) {
      continue;
    }
    std::printf("Bilinmeyen arguman: %s\n", a);
    return false;
  }
  if (opt.impl_name == "painter-gl") {
    opt.impl = Impl::kPainterGl;
  } else if (opt.impl_name == "painter-vk") {
    opt.impl = Impl::kPainterVk;
  } else if (opt.impl_name == "sdl") {
    opt.impl = Impl::kSdl;
  } else {
    std::printf("--impl painter-gl, painter-vk ya da sdl olmali.\n");
    return false;
  }
  if (opt.shapes <= 0 || opt.frames <= 0) {
    std::printf("--shapes ve --frames pozitif olmali.\n");
    return false;
  }
  return true;
}

}  // namespace

int main(int argc, char** argv) {
  spdlog::set_level(spdlog::level::warn);

  Options opt;
  if (!ParseOptions(argc, argv, opt)) {
    return 1;
  }
  if (!SDL_Init(SDL_INIT_VIDEO)) {
    std::printf("SDL_Init basarisiz: %s\n", SDL_GetError());
    return 1;
  }
  if (!opt.screenshot_dir.empty()) {
    SDL_CreateDirectory(opt.screenshot_dir.c_str());
  }

  const std::vector<Placement> items = MakePlacements(opt.shapes);
  std::vector<Result> results;
  RunInfo info;
  const bool ok = opt.impl == Impl::kSdl
                      ? RunSdl(opt, items, results, info)
                      : RunPainter(opt, items, results, info);

  if (!results.empty()) {
    PrintTable(opt, info, results);
    if (!opt.csv_path.empty()) {
      std::printf("CSV %s: %s\n",
                  AppendCsv(opt, info, results) ? "eklendi" : "YAZILAMADI",
                  opt.csv_path.c_str());
    }
  }
  SDL_Quit();
  return ok ? 0 : 1;
}
