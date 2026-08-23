/// @file batching_benchmark.cpp
/// @brief RenderBatcher verimliliği ölçümü — hangi çizim deseni kaç draw
///        call üretiyor?
///
/// Amaç, "batch'lemenin faydası ne zaman var, ne zaman yok" sorusunu
/// tahminle değil sayıyla cevaplamak. Ölçülen üç şey:
///   * kare başına draw call sayısı (batch verimliliğinin doğrudan ölçüsü),
///   * kare başına uniform yükleme sayısı (SetModelMatrix / SetOpacity),
///   * kare süresi (ms).
///
/// Kütüphaneye sayaç eklenmedi; ölçüm @ref bench::CountingRenderer
/// sarmalayıcısıyla dışarıdan yapılır (bkz. counting_renderer.h).
///
/// Kullanım:
///   batching_benchmark [--shapes=N] [--frames=N] [--null] [--csv=DOSYA]
///                      [--screenshot=DIZIN]
///
///   --null        Pencere/OpenGL açma; yalnızca CPU yolunu ve çağrı
///                 sayılarını ölç. Draw call sayıları her iki modda da aynıdır.
///   --csv         Sonuçları CSV olarak da yaz (repoya eklemek için).
///   --screenshot  Her senaryonun çıktısını `<senaryo>.png` olarak kaydet
///                 (yalnızca OpenGL modunda; `--null` ile yok sayılır).

#include <SDL3/SDL.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "counting_renderer.h"
#include "screenshot.h"
#include "sdl_painter/brush.h"
#include "sdl_painter/color.h"
#include "sdl_painter/font.h"
#include "sdl_painter/geometry.h"
#include "sdl_painter/painter.h"
#include "sdl_painter/pen.h"

namespace {

using sdl_painter::Brush;
using sdl_painter::Color;
using sdl_painter::Painter;
using sdl_painter::Pen;

constexpr int32_t kViewportWidth = 1280;
constexpr int32_t kViewportHeight = 720;
constexpr int32_t kWarmupFrames = 5;

/// @brief Tekrarlanabilir sonuç için basit LCG — std::rand implementasyona
///        göre değiştiğinden ölçüm makineler arasında karşılaştırılamaz olur.
class Lcg {
 public:
  explicit Lcg(uint32_t seed) : mState(seed) {}

  uint32_t Next() noexcept {
    mState = (mState * 1664525U) + 1013904223U;
    return mState;
  }

  /// @brief [0, n) aralığında değer.
  uint32_t Below(uint32_t n) noexcept { return Next() % n; }

  /// @brief [0, 1] aralığında değer.
  float Unit() noexcept {
    return static_cast<float>(Next() >> 8U) / 16777215.0F;
  }

 private:
  uint32_t mState;
};

/// @brief Şeklin ekrandaki konumu — tüm senaryolarda aynı dağılım kullanılır.
struct Placement {
  float x{0.0F};
  float y{0.0F};
  float size{0.0F};
  Color color;
};

/// @brief Senaryolar arasında karşılaştırma yapılabilmesi için ortak,
///        deterministik yerleşim üret.
std::vector<Placement> MakePlacements(int32_t count) {
  Lcg rng(12345U);
  std::vector<Placement> out;
  out.reserve(static_cast<size_t>(count));
  for (int32_t i = 0; i < count; ++i) {
    Placement p;
    p.size = 6.0F + (rng.Unit() * 10.0F);
    p.x = rng.Unit() * (static_cast<float>(kViewportWidth) - p.size);
    p.y = rng.Unit() * (static_cast<float>(kViewportHeight) - p.size);
    p.color = Color{static_cast<uint8_t>(rng.Below(256)),
                    static_cast<uint8_t>(rng.Below(256)),
                    static_cast<uint8_t>(rng.Below(256)), 255};
    out.push_back(p);
  }
  return out;
}

// --- Senaryolar -------------------------------------------------------------
//
// Her senaryo aynı sayıda şekil çizer; tek fark çizim desenidir. Böylece draw
// call farkı doğrudan desenin batch'e etkisini gösterir.

using ScenarioFn = void (*)(Painter&, const std::vector<Placement>&);

/// Referans: tek renk, transform yok. Ulaşılabilir en iyi durum.
void FillSameColor(Painter& p, const std::vector<Placement>& items) {
  p.SetBrush(Brush(Color{200, 80, 60, 255}));
  for (const auto& it : items) {
    p.FillRect(it.x, it.y, it.size, it.size);
  }
}

/// Asıl soru: her şekil farklı renkte olunca batch bozuluyor mu?
void FillManyColors(Painter& p, const std::vector<Placement>& items) {
  for (const auto& it : items) {
    p.SetBrush(Brush(it.color));
    p.FillRect(it.x, it.y, it.size, it.size);
  }
}

/// Kalem ile çerçeve — şekil başına dört kenar quad'ı üretir.
void StrokeManyColors(Painter& p, const std::vector<Placement>& items) {
  for (const auto& it : items) {
    p.SetPen(Pen(it.color, 2.0F));
    p.DrawRect(it.x, it.y, it.size, it.size);
  }
}

/// Adaptif segmentli daire — vertex sayısı dikdörtgenin çok üstünde.
void FillCirclesManyColors(Painter& p, const std::vector<Placement>& items) {
  for (const auto& it : items) {
    p.SetBrush(Brush(it.color));
    p.FillCircle(it.x, it.y, it.size * 0.5F);
  }
}

/// Şekil başına Save/Translate/Restore — transform bir uniform olduğu için
/// her değişim batch'i kırar.
void TranslatePerShape(Painter& p, const std::vector<Placement>& items) {
  for (const auto& it : items) {
    p.SetBrush(Brush(it.color));
    p.Save();
    p.Translate(it.x, it.y);
    p.FillRect(0.0F, 0.0F, it.size, it.size);
    p.Restore();
  }
}

/// Tipik "nesne başına yerel uzay" deseni: taşı + döndür.
void TranslateRotatePerShape(Painter& p, const std::vector<Placement>& items) {
  float angle = 0.0F;
  for (const auto& it : items) {
    p.SetBrush(Brush(it.color));
    p.Save();
    p.Translate(it.x, it.y);
    p.Rotate(angle);
    p.FillRect(-it.size * 0.5F, -it.size * 0.5F, it.size, it.size);
    p.Restore();
    angle += 7.0F;
  }
}

/// Opaklık da uniform — her değişim batch'i kırar.
void OpacityAlternating(Painter& p, const std::vector<Placement>& items) {
  bool toggle = false;
  for (const auto& it : items) {
    p.SetOpacity(toggle ? 0.5F : 1.0F);
    toggle = !toggle;
    p.SetBrush(Brush(it.color));
    p.FillRect(it.x, it.y, it.size, it.size);
  }
  p.SetOpacity(1.0F);
}

/// Her şekle ayrı clip — scissor bir GPU durumu, batch'i kırar.
void ClipPerShape(Painter& p, const std::vector<Placement>& items) {
  for (const auto& it : items) {
    p.SetBrush(Brush(it.color));
    p.SetClipRect(sdl_painter::Rect{it.x, it.y, it.size, it.size});
    p.FillRect(it.x, it.y, it.size, it.size);
  }
  p.ClearClip();
}

/// Yalnızca metin — glyph atlası sayesinde tek texture, tek batch beklenir.
void TextOnly(Painter& p, const std::vector<Placement>& items) {
  p.SetPen(Pen(Color{240, 240, 240, 255}, 1.0F));
  for (const auto& it : items) {
    p.DrawText(it.x, it.y, "Ab");
  }
}

/// Şekil ile metin dönüşümlü: batch modu değiştiği için her geçiş bir flush.
void ShapesAndTextInterleaved(Painter& p, const std::vector<Placement>& items) {
  for (const auto& it : items) {
    p.SetBrush(Brush(it.color));
    p.FillRect(it.x, it.y, it.size, it.size);
    p.SetPen(Pen(it.color, 1.0F));
    p.DrawText(it.x, it.y, "Ab");
  }
}

struct Scenario {
  const char* name;
  const char* note;
  ScenarioFn fn;
  bool needs_font;
};

const Scenario kScenarios[] = {
    {"fill_same_color", "referans: tek renk, transform yok", FillSameColor,
     false},
    {"fill_many_colors", "sekil basina farkli renk", FillManyColors, false},
    {"stroke_many_colors", "kalem cercevesi, farkli renk", StrokeManyColors,
     false},
    {"circles_many_colors", "adaptif segmentli daire", FillCirclesManyColors,
     false},
    {"translate_per_shape", "Save + Translate + Restore", TranslatePerShape,
     false},
    {"translate_rotate_per_shape", "Save + Translate + Rotate + Restore",
     TranslateRotatePerShape, false},
    {"opacity_alternating", "sekil basina opaklik degisimi", OpacityAlternating,
     false},
    {"clip_per_shape", "sekil basina SetClipRect", ClipPerShape, false},
    {"text_only", "yalnizca metin (glyph atlasi)", TextOnly, true},
    {"shapes_and_text", "sekil ile metin donusumlu", ShapesAndTextInterleaved,
     true},
};

// --- Sonuç ------------------------------------------------------------------

struct Result {
  std::string name;
  std::string note;
  double draw_calls{0.0};
  double vertices{0.0};
  double model_uploads{0.0};
  double opacity_uploads{0.0};
  double scissor_changes{0.0};
  double ms_avg{0.0};
  double ms_min{0.0};
};

/// @brief Sistemde mevcut bir TTF fontu bul (examples/text.cpp ile aynı yol).
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
      "/usr/share/fonts/TTF/DejaVuSans.ttf",
      "/System/Library/Fonts/Helvetica.ttc",
#endif
  };
  for (const char* path : candidates) {
    SDL_IOStream* io = SDL_IOFromFile(path, "rb");
    if (io != nullptr) {
      SDL_CloseIO(io);
      return path;
    }
  }
  return {};
}

/// @brief Senaryoyu tek kare çizip PNG olarak kaydet.
///
/// Sunumdan önceki ana @ref bench::CountingRenderer::CaptureBeforeNextPresent
/// ile girilir; o noktada batch boşaltılmış ama arka tampon henüz
/// takas edilmemiştir.
bool CaptureScenario(const Scenario& scenario, Painter& painter,
                     bench::CountingRenderer& counter,
                     const std::vector<Placement>& items,
                     const std::string& path) {
  bool ok = false;
  counter.CaptureBeforeNextPresent([&] {
    ok = bench::SaveBackBufferPng(path, kViewportWidth, kViewportHeight);
  });

  painter.Begin();
  painter.Clear(Color{20, 22, 28, 255});
  scenario.fn(painter, items);
  painter.End();
  return ok;
}

Result RunScenario(const Scenario& scenario, Painter& painter,
                   bench::CountingRenderer& counter,
                   const std::vector<Placement>& items, int32_t frames) {
  using Clock = std::chrono::steady_clock;

  // Isınma: ilk karelerde glyph atlası doldurulur, GPU buffer'ları büyür.
  for (int32_t i = 0; i < kWarmupFrames; ++i) {
    painter.Begin();
    painter.Clear(Color{20, 22, 28, 255});
    scenario.fn(painter, items);
    painter.End();
  }

  counter.ResetStats();
  double total_ms = 0.0;
  double min_ms = 1e30;

  for (int32_t i = 0; i < frames; ++i) {
    const auto t0 = Clock::now();
    painter.Begin();
    painter.Clear(Color{20, 22, 28, 255});
    scenario.fn(painter, items);
    painter.End();
    const auto t1 = Clock::now();
    const double ms =
        std::chrono::duration<double, std::milli>(t1 - t0).count();
    total_ms += ms;
    min_ms = std::min(min_ms, ms);
  }

  const auto& s = counter.Stats();
  const auto f = static_cast<double>(frames);
  Result r;
  r.name = scenario.name;
  r.note = scenario.note;
  r.draw_calls = static_cast<double>(s.draw_calls) / f;
  r.vertices = static_cast<double>(s.vertices) / f;
  r.model_uploads = static_cast<double>(s.model_uploads) / f;
  r.opacity_uploads = static_cast<double>(s.opacity_uploads) / f;
  r.scissor_changes = static_cast<double>(s.scissor_changes) / f;
  r.ms_avg = total_ms / f;
  r.ms_min = min_ms;
  return r;
}

void PrintTable(const std::vector<Result>& results, int32_t shapes) {
  std::printf("\n%-28s %10s %10s %8s %8s %9s %9s\n", "senaryo", "draw/kare",
              "vertex", "model", "scissor", "ms(ort)", "ms(min)");
  std::printf("%s\n", std::string(87, '-').c_str());
  for (const auto& r : results) {
    std::printf("%-28s %10.1f %10.0f %8.1f %8.1f %9.3f %9.3f\n", r.name.c_str(),
                r.draw_calls, r.vertices, r.model_uploads, r.scissor_changes,
                r.ms_avg, r.ms_min);
  }
  std::printf(
      "\n%d sekil/kare. draw/kare degeri 1'e ne kadar yakinsa batch o kadar "
      "verimli.\n",
      shapes);
}

bool WriteCsv(const std::string& path, const std::vector<Result>& results,
              int32_t shapes, int32_t frames, const char* mode) {
  std::FILE* f = std::fopen(path.c_str(), "w");
  if (f == nullptr) {
    return false;
  }
  std::fprintf(f,
               "scenario,mode,shapes,frames,draw_calls_per_frame,"
               "vertices_per_frame,model_uploads_per_frame,"
               "opacity_uploads_per_frame,scissor_changes_per_frame,"
               "ms_avg,ms_min,note\n");
  for (const auto& r : results) {
    // note alani virgul icerebilir — CSV'de tirnaklanmali.
    std::fprintf(f, "%s,%s,%d,%d,%.2f,%.0f,%.2f,%.2f,%.2f,%.4f,%.4f,\"%s\"\n",
                 r.name.c_str(), mode, shapes, frames, r.draw_calls, r.vertices,
                 r.model_uploads, r.opacity_uploads, r.scissor_changes,
                 r.ms_avg, r.ms_min, r.note.c_str());
  }
  std::fclose(f);
  return true;
}

/// @brief `--anahtar=deger` biçimli argümandan tamsayı oku.
bool ParseIntArg(const char* arg, const char* key, int32_t& out) {
  const size_t len = std::strlen(key);
  if (std::strncmp(arg, key, len) != 0) {
    return false;
  }
  out = static_cast<int32_t>(std::atoi(arg + len));
  return true;
}

}  // namespace

int main(int argc, char** argv) {
  spdlog::set_level(spdlog::level::warn);

  int32_t shapes = 2000;
  int32_t frames = 200;
  bool null_mode = false;
  std::string csv_path;
  std::string screenshot_dir;

  for (int i = 1; i < argc; ++i) {
    const char* a = argv[i];
    if (ParseIntArg(a, "--shapes=", shapes)) {
      continue;
    }
    if (ParseIntArg(a, "--frames=", frames)) {
      continue;
    }
    if (std::strcmp(a, "--null") == 0) {
      null_mode = true;
      continue;
    }
    if (std::strncmp(a, "--csv=", 6) == 0) {
      csv_path = a + 6;
      continue;
    }
    if (std::strncmp(a, "--screenshot=", 13) == 0) {
      screenshot_dir = a + 13;
      continue;
    }
    std::printf("Bilinmeyen arguman: %s\n", a);
    return 1;
  }

  if (shapes <= 0 || frames <= 0) {
    std::printf("--shapes ve --frames pozitif olmali.\n");
    return 1;
  }

  if (!screenshot_dir.empty() && null_mode) {
    std::printf("--screenshot yalnizca OpenGL modunda calisir; yok sayiliyor.\n");
    screenshot_dir.clear();
  }

  if (!SDL_Init(SDL_INIT_VIDEO)) {
    std::printf("SDL_Init basarisiz: %s\n", SDL_GetError());
    return 1;
  }

  if (!screenshot_dir.empty()) {
    SDL_CreateDirectory(screenshot_dir.c_str());
  }

  SDL_Window* window = nullptr;
  std::unique_ptr<sdl_painter::IRenderer> inner;

  if (!null_mode) {
    window = SDL_CreateWindow("sdl_painter batching benchmark", kViewportWidth,
                              kViewportHeight, SDL_WINDOW_OPENGL);
    if (window == nullptr) {
      std::printf("Pencere olusturulamadi: %s\n", SDL_GetError());
      SDL_Quit();
      return 1;
    }
    inner = sdl_painter::CreateRenderer(sdl_painter::RendererBackend::kOpenGL);
    if (inner == nullptr || !inner->Initialize(window)) {
      std::printf("OpenGL renderer baslatilamadi.\n");
      SDL_DestroyWindow(window);
      SDL_Quit();
      return 1;
    }
    // Vsync kapali olmali; aksi halde olculen sey ekranin tazeleme hizi olur.
    SDL_GL_SetSwapInterval(0);
  }

  auto counting = std::make_unique<bench::CountingRenderer>(std::move(inner));
  bench::CountingRenderer& counter = *counting;

  const char* mode = null_mode ? "null" : "opengl";
  std::printf("mod=%s  sekil=%d  kare=%d  viewport=%dx%d\n", mode, shapes,
              frames, kViewportWidth, kViewportHeight);

  std::vector<Result> results;
  {
    Painter painter(std::move(counting), kViewportWidth, kViewportHeight);
    if (!painter.IsValid()) {
      std::printf("Painter gecersiz.\n");
      SDL_Quit();
      return 1;
    }

    std::shared_ptr<sdl_painter::Font> font;
    const std::string font_path = FindSystemFont();
    if (!font_path.empty()) {
      font = std::make_shared<sdl_painter::Font>(font_path, 14);
      if (!font->IsValid()) {
        font.reset();
      }
    }
    if (font) {
      painter.SetFont(font);
    } else {
      std::printf("UYARI: TTF font bulunamadi, metin senaryolari atlaniyor.\n");
    }

    const std::vector<Placement> items = MakePlacements(shapes);

    for (const auto& scenario : kScenarios) {
      if (scenario.needs_font && !font) {
        continue;
      }
      results.push_back(RunScenario(scenario, painter, counter, items, frames));
      if (!screenshot_dir.empty()) {
        const std::string path =
            screenshot_dir + "/" + scenario.name + ".png";
        if (CaptureScenario(scenario, painter, counter, items, path)) {
          std::printf("  ekran goruntusu: %s\n", path.c_str());
        } else {
          std::printf("  ekran goruntusu ALINAMADI: %s\n", path.c_str());
        }
      }
    }

    // Yasam dongusu sozlesmesi: Font, Painter'dan ONCE yikilmali. Painter
    // kendi shared_ptr kopyasini tuttugu icin burada ayrica temizliyoruz.
    painter.SetFont(nullptr);
    font.reset();
  }

  PrintTable(results, shapes);

  if (!csv_path.empty()) {
    if (WriteCsv(csv_path, results, shapes, frames, mode)) {
      std::printf("CSV yazildi: %s\n", csv_path.c_str());
    } else {
      std::printf("CSV yazilamadi: %s\n", csv_path.c_str());
    }
  }

  if (window != nullptr) {
    SDL_DestroyWindow(window);
  }
  SDL_Quit();
  return 0;
}
