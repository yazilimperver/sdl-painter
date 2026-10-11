// app_detail::StatsOverlay (dahili) için deterministik testler.
//
// FPS hesabı Sample(frame_ns) ile süreyi parametre olarak aldığı için gerçek
// uyku gerekmez. Çizim testleri MockRenderer üzerinde Painter'ı sürer; font
// gerektirenler sistem fontunu kullanır (SDLPAINTER_REQUIRE_FONT_OR_SKIP).
// Font tek bir renderer'a bağlandığı için her test kendi Painter'ını kurar.

#include "app/stats_overlay.h"

#include "sdl_painter/brush.h"
#include "sdl_painter/color.h"
#include "sdl_painter/font.h"
#include "sdl_painter/frame_stats.h"
#include "sdl_painter/geometry.h"
#include "sdl_painter/painter.h"
#include "sdl_painter/pen.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <gtest/gtest.h>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "mock_renderer.h"
#include "test_support.h"

using sdl_painter::BlendMode;
using sdl_painter::Brush;
using sdl_painter::Color;
using sdl_painter::Font;
using sdl_painter::FrameStats;
using sdl_painter::MockRenderer;
using sdl_painter::Painter;
using sdl_painter::Pen;
using sdl_painter::Rect;
using sdl_painter::StatsOverlayMode;
using sdl_painter::Vertex;
using sdl_painter::app_detail::StatsOverlay;

namespace {

constexpr uint64_t kMs = 1'000'000ULL;

/// @brief Hiçbir sistemde bulunmayan font yolu: göstergeyi fontsuz kurar.
constexpr const char* kMissingFont = "sdlpainter-test-missing-font.ttf";

constexpr int32_t kViewportW = 800;
constexpr int32_t kViewportH = 600;

struct Harness {
  MockRenderer* mock{nullptr};
  Painter painter;

  Harness() : Harness(std::make_unique<MockRenderer>()) {}

  explicit Harness(std::unique_ptr<MockRenderer> m)
      : mock(m.get()), painter(std::move(m), kViewportW, kViewportH) {}
};

int32_t TexturedVertexTotal(const MockRenderer& mock) {
  int32_t total = 0;
  for (const auto& call : mock.calls) {
    if (call.name == "DrawTextured") {
      total += static_cast<int32_t>(call.vertex_count);
    }
  }
  return total;
}

bool HasColor(const Vertex& v, const Color& c) {
  return v.r == c.r && v.g == c.g && v.b == c.b && v.a == c.a;
}

/// @brief Her çizim çağrısında yürürlükte olan opaklık.
///
/// Mock çizim çağrısına opaklık yazmaz; batcher her batch'ten önce
/// SetOpacity gönderir, yürürlükteki değer ondan önceki son SetOpacity'dir.
/// `from` öncesindeki SetOpacity'ler başlangıç değerini belirler.
std::vector<std::pair<std::string, float>> DrawOpacities(
    const MockRenderer& mock, std::size_t from = 0) {
  std::vector<std::pair<std::string, float>> result;
  float current = 1.0F;
  for (std::size_t i = 0; i < mock.calls.size(); ++i) {
    const auto& call = mock.calls[i];
    if (call.name == "SetOpacity") {
      current = call.opacity;
    } else if (i >= from &&
               (call.name == "DrawTriangles" || call.name == "DrawTextured")) {
      result.emplace_back(call.name, current);
    }
  }
  return result;
}

/// @brief Her çizim çağrısında yürürlükte olan karışım modu.
///
/// Mock'ta k'inci "SetBlendMode" çağrısının modu `blend_calls[k]`'dir.
/// Renderer `BlendMode::kAlpha` ile başlar.
std::vector<BlendMode> DrawBlendModes(const MockRenderer& mock,
                                      std::size_t from) {
  std::vector<BlendMode> result;
  BlendMode current = BlendMode::kAlpha;
  std::size_t next_blend = 0;
  for (std::size_t i = 0; i < mock.calls.size(); ++i) {
    const auto& call = mock.calls[i];
    if (call.name == "SetBlendMode" && next_blend < mock.blend_calls.size()) {
      current = mock.blend_calls[next_blend++];
    } else if (i >= from &&
               (call.name == "DrawTriangles" || call.name == "DrawTextured")) {
      result.push_back(current);
    }
  }
  return result;
}

}  // namespace

// --- Sample: FPS hesabı (fonttan bağımsız) ---

TEST(StatsOverlaySample, StartsAtZeroWithoutFont) {
  const StatsOverlay overlay(kMissingFont, 14);
  EXPECT_FALSE(overlay.HasFont());
  EXPECT_DOUBLE_EQ(overlay.Fps(), 0.0);
}

TEST(StatsOverlaySample, FirstValueAfterFiveFramesWithoutWaitingForWindow) {
  StatsOverlay overlay(kMissingFont, 14);
  for (int32_t i = 0; i < 4; ++i) {
    overlay.Sample(10 * kMs);
    EXPECT_DOUBLE_EQ(overlay.Fps(), 0.0) << "kare " << (i + 1);
  }
  // Beşinci kare: 5 kare / 50 ms = 100 FPS (250 ms pencere beklenmez).
  overlay.Sample(10 * kMs);
  EXPECT_DOUBLE_EQ(overlay.Fps(), 100.0);
}

TEST(StatsOverlaySample, AfterBootstrapUpdatesOnlyWhenWindowFills) {
  StatsOverlay overlay(kMissingFont, 14);
  for (int32_t i = 0; i < 5; ++i) {
    overlay.Sample(10 * kMs);
  }
  ASSERT_DOUBLE_EQ(overlay.Fps(), 100.0);

  // Bootstrap sonrası sayaçlar sıfırlandı; 9 x 25 ms = 225 ms, pencere dolmadı.
  for (int32_t i = 0; i < 9; ++i) {
    overlay.Sample(25 * kMs);
    EXPECT_DOUBLE_EQ(overlay.Fps(), 100.0) << "kare " << (i + 1);
  }
  // Onuncu kare pencereyi tam 250 ms'de doldurur: 10 / 0.25 s = 40 FPS.
  overlay.Sample(25 * kMs);
  EXPECT_DOUBLE_EQ(overlay.Fps(), 40.0);

  // Sayaçlar yeniden sıfırlandı: bir sonraki tek kare değeri değiştirmez.
  overlay.Sample(25 * kMs);
  EXPECT_DOUBLE_EQ(overlay.Fps(), 40.0);
}

TEST(StatsOverlaySample, ZeroDurationFramesNeverDivideByZero) {
  StatsOverlay overlay(kMissingFont, 14);
  for (int32_t i = 0; i < 6; ++i) {
    overlay.Sample(0);
    EXPECT_TRUE(std::isfinite(overlay.Fps()));
    EXPECT_DOUBLE_EQ(overlay.Fps(), 0.0);
  }
  overlay.Sample(10 * kMs);
  EXPECT_TRUE(std::isfinite(overlay.Fps()));
  EXPECT_GT(overlay.Fps(), 0.0);
}

// --- Draw ---

TEST(StatsOverlayDraw, WithoutFontDrawsNothing) {
  Harness h;
  StatsOverlay overlay(kMissingFont, 14);
  ASSERT_FALSE(overlay.HasFont());

  h.painter.Begin();
  h.mock->Reset();
  overlay.Draw(h.painter, StatsOverlayMode::kDetailed, FrameStats{});
  h.painter.End();

  EXPECT_EQ(h.mock->CountCalls("DrawTriangles"), 0);
  EXPECT_EQ(h.mock->CountCalls("DrawTextured"), 0);
}

TEST(StatsOverlayDraw, NoneModeDrawsNothingEvenWithFont) {
  SDLPAINTER_REQUIRE_FONT_OR_SKIP(font_path);
  Harness h;
  StatsOverlay overlay(font_path, 14);
  ASSERT_TRUE(overlay.HasFont());

  h.painter.Begin();
  h.mock->Reset();
  overlay.Draw(h.painter, StatsOverlayMode::kNone, FrameStats{});
  h.painter.End();

  EXPECT_EQ(h.mock->CountCalls("DrawTriangles"), 0);
  EXPECT_EQ(h.mock->CountCalls("DrawTextured"), 0);
}

TEST(StatsOverlayDraw, FpsAndDetailedModesDrawPanelAndText) {
  SDLPAINTER_REQUIRE_FONT_OR_SKIP(font_path);
  Harness h;
  StatsOverlay overlay(font_path, 14);
  ASSERT_TRUE(overlay.HasFont());

  h.painter.Begin();
  h.mock->Reset();
  overlay.Draw(h.painter, StatsOverlayMode::kFps, FrameStats{});
  h.painter.End();
  EXPECT_GE(h.mock->CountCalls("DrawTriangles"), 1) << "panel cizilmedi";
  const int32_t fps_text_vertices = TexturedVertexTotal(*h.mock);
  EXPECT_GT(fps_text_vertices, 0) << "metin cizilmedi";

  FrameStats stats;
  stats.cpu_frame_ms = 1.25;
  stats.draw_calls = 12;
  stats.batches = 3;
  stats.vertices = 456;
  stats.state_changes = 7;
  h.painter.Begin();
  h.mock->Reset();
  overlay.Draw(h.painter, StatsOverlayMode::kDetailed, stats);
  h.painter.End();
  // Ayrıntılı mod FPS satırına ek olarak altı satır daha yazar.
  EXPECT_GT(TexturedVertexTotal(*h.mock), fps_text_vertices);
}

TEST(StatsOverlayDraw, RestoresPainterStateAndFont) {
  SDLPAINTER_REQUIRE_FONT_OR_SKIP(font_path);
  Harness h;
  StatsOverlay overlay(font_path, 14);
  ASSERT_TRUE(overlay.HasFont());
  auto app_font = std::make_shared<Font>(font_path, 22);
  ASSERT_TRUE(app_font->IsValid());

  constexpr Color kPenColor{200, 10, 20, 255};
  constexpr Color kBrushColor{30, 40, 210, 255};

  h.painter.Begin();
  h.painter.SetFont(app_font);
  h.painter.SetPen(Pen(kPenColor, 3.0F));
  h.painter.SetBrush(Brush(kBrushColor));
  h.painter.Translate(5.0F, 7.0F);
  h.painter.SetClipRect(Rect{10.0F, 20.0F, 300.0F, 200.0F});
  h.painter.SetOpacity(0.5F);
  h.painter.SetBlendMode(BlendMode::kAdditive);
  // Kırpmanın renderer'a gittiği hali kaydet.
  h.painter.FillRect(0.0F, 0.0F, 1.0F, 1.0F);
  h.painter.End();
  ASSERT_FALSE(h.mock->scissor_calls.empty());
  const MockRenderer::RectCall kClip = h.mock->scissor_calls.back();

  h.painter.Begin();
  overlay.Draw(h.painter, StatsOverlayMode::kDetailed, FrameStats{});
  EXPECT_EQ(h.painter.GetFont().get(), app_font.get())
      << "gosterge uygulamanin fontunu geri koymadi";
  EXPECT_EQ(h.painter.GetBlendMode(), BlendMode::kAdditive);

  // Göstergeden sonraki uygulama çizimi eski durumla yapılmalı.
  const std::size_t first = h.mock->calls.size();
  h.painter.FillRect(0.0F, 0.0F, 10.0F, 10.0F);
  h.painter.DrawLine(0.0F, 20.0F, 10.0F, 20.0F);
  h.painter.End();

  const auto kAppDraws = DrawOpacities(*h.mock, first);
  ASSERT_EQ(kAppDraws.size(), 1U);
  EXPECT_EQ(kAppDraws[0].first, "DrawTriangles");
  EXPECT_FLOAT_EQ(kAppDraws[0].second, 0.5F) << "opaklik geri gelmedi";
  bool has_brush = false;
  bool has_pen = false;
  float min_x = 1e9F;
  float min_y = 1e9F;
  for (const auto& v : h.mock->last_vertices) {
    has_brush = has_brush || HasColor(v, kBrushColor);
    has_pen = has_pen || HasColor(v, kPenColor);
    min_x = std::min(min_x, v.x);
    min_y = std::min(min_y, v.y);
  }
  EXPECT_TRUE(has_brush) << "firca geri gelmedi";
  EXPECT_TRUE(has_pen) << "kalem geri gelmedi";
  EXPECT_FLOAT_EQ(min_x, 5.0F) << "donusum geri gelmedi";
  EXPECT_FLOAT_EQ(min_y, 7.0F) << "donusum geri gelmedi";

  // Gösterge kırpmayı kaldırdı; uygulamanın kırpması geri yüklenmiş olmalı.
  std::size_t last_set = 0;
  std::size_t last_clear = 0;
  for (std::size_t i = 0; i < h.mock->calls.size(); ++i) {
    if (h.mock->calls[i].name == "SetScissor") {
      last_set = i + 1;
    } else if (h.mock->calls[i].name == "ClearScissor") {
      last_clear = i + 1;
    }
  }
  EXPECT_GT(last_set, last_clear) << "kirpma geri yuklenmedi";
  ASSERT_FALSE(h.mock->scissor_calls.empty());
  const MockRenderer::RectCall kAfter = h.mock->scissor_calls.back();
  EXPECT_EQ(kAfter.x, kClip.x);
  EXPECT_EQ(kAfter.y, kClip.y);
  EXPECT_EQ(kAfter.w, kClip.w);
  EXPECT_EQ(kAfter.h, kClip.h);
}

TEST(StatsOverlayDraw, DrawsWithFullOpacityAndAlphaBlendRegardlessOfApp) {
  SDLPAINTER_REQUIRE_FONT_OR_SKIP(font_path);
  Harness h;
  StatsOverlay overlay(font_path, 14);
  ASSERT_TRUE(overlay.HasFont());

  // Uygulamanın opaklık ve karışım modunu renderer'a gerçekten ulaştır.
  h.painter.Begin();
  h.painter.SetOpacity(0.25F);
  h.painter.SetBlendMode(BlendMode::kMultiply);
  h.painter.FillRect(0.0F, 0.0F, 4.0F, 4.0F);
  h.painter.End();
  ASSERT_EQ(DrawBlendModes(*h.mock, 0).back(), BlendMode::kMultiply);

  h.painter.Begin();
  const std::size_t first = h.mock->calls.size();
  overlay.Draw(h.painter, StatsOverlayMode::kFps, FrameStats{});
  h.painter.End();

  const auto kOpacities = DrawOpacities(*h.mock, first);
  const auto kBlends = DrawBlendModes(*h.mock, first);
  ASSERT_FALSE(kOpacities.empty());
  ASSERT_EQ(kOpacities.size(), kBlends.size());
  for (std::size_t i = 0; i < kOpacities.size(); ++i) {
    EXPECT_FLOAT_EQ(kOpacities[i].second, 1.0F) << kOpacities[i].first;
    // Gösterge, uygulamanın karışım modundan bağımsız okunabilir kalmalı.
    EXPECT_EQ(kBlends[i], BlendMode::kAlpha) << kOpacities[i].first;
  }
  EXPECT_EQ(h.painter.GetBlendMode(), BlendMode::kMultiply);
}
