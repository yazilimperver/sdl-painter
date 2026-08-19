/// @file test_painter.cpp
/// @brief Painter davranış testleri — sahte IRenderer enjeksiyonu ile.
///
/// Painter, pencere gerektirmeyen ctor'u sayesinde MockRenderer ile
/// sürülebilir. Böylece transform yayılımı, kırpma koordinat dönüşümü ve
/// opaklık gibi backend'den bağımsız mantık, GPU olmadan doğrulanabilir.

#include "sdl_painter/font.h"
#include "sdl_painter/painter.h"
#include "sdl_painter/pen.h"

#include <array>
#include <cmath>
#include <gtest/gtest.h>
#include <memory>
#include <string>

#include "mock_renderer.h"
#include "test_support.h"

using sdl_painter::Alignment;
using sdl_painter::Brush;
using sdl_painter::Color;
using sdl_painter::Font;
using sdl_painter::MockRenderer;
using sdl_painter::Painter;
using sdl_painter::Pen;
using sdl_painter::Point;
using sdl_painter::Rect;
using sdl_painter::RendererBackend;

namespace {

constexpr int32_t kViewportW = 800;
constexpr int32_t kViewportH = 600;

/// @brief MockRenderer sahipliğini Painter'a devreder, ham pointer'ı saklar.
struct Harness {
  MockRenderer* mock{nullptr};
  Painter painter;

  Harness() : Harness(std::make_unique<MockRenderer>()) {}

  explicit Harness(std::unique_ptr<MockRenderer> m)
      : mock(m.get()), painter(std::move(m), kViewportW, kViewportH) {}
};

/// @brief 3x3 sütun-major matrisin öteleme bileşenleri (tx, ty).
float Tx(const std::array<float, 9>& m) {
  return m[6];
}
float Ty(const std::array<float, 9>& m) {
  return m[7];
}

/// @brief Matris birim mi?
bool IsIdentity(const std::array<float, 9>& m) {
  const std::array<float, 9> kId{{1, 0, 0, 0, 1, 0, 0, 0, 1}};
  for (std::size_t i = 0; i < 9; ++i) {
    if (std::fabs(m[i] - kId[i]) > 1e-5F) {
      return false;
    }
  }
  return true;
}

}  // namespace

// ─── Transform yayılımı ─────────────────────────────────────────────────────

TEST(PainterTransform, TranslateIsAppliedToShapeDrawCall) {
  Harness h;
  h.painter.Begin();
  h.painter.SetBrush(Brush(Color::Red()));
  h.painter.Translate(100.0F, 50.0F);
  h.painter.FillRect(0.0F, 0.0F, 10.0F, 10.0F);
  h.painter.End();

  ASSERT_FALSE(h.mock->model_at_draw.empty()) << "Hiç çizim komutu üretilmedi.";
  const auto& m = h.mock->model_at_draw.front();
  EXPECT_FLOAT_EQ(Tx(m), 100.0F);
  EXPECT_FLOAT_EQ(Ty(m), 50.0F);
}

TEST(PainterTransform, SaveRestoreBalancesTransform) {
  Harness h;
  h.painter.Begin();
  h.painter.SetBrush(Brush(Color::Red()));

  h.painter.Save();
  h.painter.Translate(200.0F, 300.0F);
  h.painter.FillRect(0.0F, 0.0F, 10.0F, 10.0F);
  h.painter.Restore();

  h.painter.FillRect(0.0F, 0.0F, 10.0F, 10.0F);
  h.painter.End();

  ASSERT_EQ(h.mock->model_at_draw.size(), 2u);
  EXPECT_FLOAT_EQ(Tx(h.mock->model_at_draw[0]), 200.0F);
  EXPECT_TRUE(IsIdentity(h.mock->model_at_draw[1]))
      << "Restore sonrası transform birime dönmeliydi.";
}

/// @brief REGRESYON (K5): DrawText transform stack'ini yok sayıyordu.
///
/// `Painter::DrawText` `FlushTransform()` çağırmadığı için, Translate/Rotate
/// sonrası çizilen metin renderer'a **eski** model matrisiyle gidiyordu.
/// Görünür etkisi: `examples/text.cpp` ve `examples/vulkan_text.cpp`
/// içindeki "Dönen metin!" bölümü ekranda hiç görünmüyordu (metin ekran
/// dışına, ham koordinatlara çiziliyordu).
TEST(PainterTransform, TranslateIsAppliedToTextDrawCall) {
  const std::string font_path = sdl_painter::testing::FindSystemFont();
  if (font_path.empty()) {
    GTEST_SKIP() << "Sistemde TTF font bulunamadı.";
  }

  Harness h;
  auto font = std::make_shared<Font>(font_path, 16);
  ASSERT_TRUE(font->IsValid()) << "Font yüklenemedi: " << font_path;

  h.painter.Begin();
  h.painter.SetFont(font);
  h.painter.SetPen(Pen(Color::White(), 1.0F));
  h.painter.Translate(100.0F, 50.0F);
  h.painter.DrawText(0.0F, 0.0F, "A");
  h.painter.End();

  ASSERT_FALSE(h.mock->model_at_draw.empty())
      << "DrawText hiç çizim komutu üretmedi.";
  const auto& m = h.mock->model_at_draw.front();
  EXPECT_FLOAT_EQ(Tx(m), 100.0F)
      << "DrawText, güncel transform'u renderer'a göndermiyor.";
  EXPECT_FLOAT_EQ(Ty(m), 50.0F);
}

/// @brief REGRESYON (K5): Rect aşırı yüklemesi de aynı hatayı taşıyordu.
TEST(PainterTransform, TranslateIsAppliedToAlignedTextDrawCall) {
  const std::string font_path = sdl_painter::testing::FindSystemFont();
  if (font_path.empty()) {
    GTEST_SKIP() << "Sistemde TTF font bulunamadı.";
  }

  Harness h;
  auto font = std::make_shared<Font>(font_path, 16);
  ASSERT_TRUE(font->IsValid());

  h.painter.Begin();
  h.painter.SetFont(font);
  h.painter.SetPen(Pen(Color::White(), 1.0F));
  h.painter.Translate(70.0F, 20.0F);
  h.painter.DrawText(Rect{0.0F, 0.0F, 200.0F, 40.0F}, "A", Alignment::kCenter);
  h.painter.End();

  ASSERT_FALSE(h.mock->model_at_draw.empty());
  EXPECT_FLOAT_EQ(Tx(h.mock->model_at_draw.front()), 70.0F);
  EXPECT_FLOAT_EQ(Ty(h.mock->model_at_draw.front()), 20.0F);
}

// ─── Kırpma (clip) koordinat dönüşümü ───────────────────────────────────────

TEST(PainterClip, OpenGLClipRectIsYFlipped) {
  Harness h;
  h.painter.Begin();
  h.painter.SetClipRect(Rect{10.0F, 20.0F, 100.0F, 50.0F});
  h.painter.End();

  ASSERT_EQ(h.mock->scissor_calls.size(), 1u);
  const auto& s = h.mock->scissor_calls.front();
  EXPECT_EQ(s.x, 10);
  // OpenGL scissor Y=0 altta: viewport_h - y - h = 600 - 20 - 50 = 530
  EXPECT_EQ(s.y, 530);
  EXPECT_EQ(s.w, 100);
  EXPECT_EQ(s.h, 50);
}

TEST(PainterClip, VulkanClipRectIsNotYFlipped) {
  auto mock = std::make_unique<MockRenderer>();
  mock->backend = RendererBackend::kVulkan;
  Harness h(std::move(mock));

  h.painter.Begin();
  h.painter.SetClipRect(Rect{10.0F, 20.0F, 100.0F, 50.0F});
  h.painter.End();

  ASSERT_EQ(h.mock->scissor_calls.size(), 1u);
  EXPECT_EQ(h.mock->scissor_calls.front().y, 20)
      << "Vulkan'da Y ekseni zaten üstten başlar, flip uygulanmamalı.";
}

TEST(PainterClip, RestoreReappliesSavedClip) {
  Harness h;
  h.painter.Begin();
  h.painter.SetClipRect(Rect{0.0F, 0.0F, 100.0F, 100.0F});
  h.painter.Save();
  h.painter.SetClipRect(Rect{50.0F, 50.0F, 10.0F, 10.0F});
  h.painter.Restore();
  h.painter.End();

  // Restore, kaydedilen clip'i (0,0,100,100) yeniden uygulamalı.
  ASSERT_GE(h.mock->scissor_calls.size(), 3u);
  const auto& last = h.mock->scissor_calls.back();
  EXPECT_EQ(last.x, 0);
  EXPECT_EQ(last.w, 100);
  EXPECT_EQ(last.h, 100);
}

TEST(PainterClip, ClearClipCallsRendererClearScissor) {
  Harness h;
  h.painter.Begin();
  h.painter.SetClipRect(Rect{0.0F, 0.0F, 10.0F, 10.0F});
  h.painter.ClearClip();
  h.painter.End();

  EXPECT_GE(h.mock->clear_scissor_count, 1);
}

// ─── Opaklık ────────────────────────────────────────────────────────────────

TEST(PainterOpacity, OpacityReachesRendererOnFlush) {
  Harness h;
  h.painter.Begin();
  h.painter.SetBrush(Brush(Color::Red()));
  h.painter.SetOpacity(0.5F);
  h.painter.FillRect(0.0F, 0.0F, 10.0F, 10.0F);
  h.painter.End();

  EXPECT_FLOAT_EQ(h.mock->last_opacity, 0.5F);
}

TEST(PainterOpacity, RestoreReappliesSavedOpacity) {
  Harness h;
  h.painter.Begin();
  h.painter.SetBrush(Brush(Color::Red()));
  h.painter.SetOpacity(1.0F);
  h.painter.Save();
  h.painter.SetOpacity(0.25F);
  h.painter.FillRect(0.0F, 0.0F, 10.0F, 10.0F);
  h.painter.Restore();
  h.painter.FillRect(0.0F, 0.0F, 10.0F, 10.0F);
  h.painter.End();

  EXPECT_FLOAT_EQ(h.mock->last_opacity, 1.0F);
}

// ─── Görünmez kalem / fırça kısa devresi ────────────────────────────────────

TEST(PainterVisibility, NoPenProducesNoDrawCall) {
  Harness h;
  h.painter.Begin();
  h.painter.SetPen(Pen::NoPen());
  h.painter.DrawRect(0.0F, 0.0F, 10.0F, 10.0F);
  h.painter.End();

  EXPECT_EQ(h.mock->CountCalls("DrawTriangles"), 0);
}

TEST(PainterVisibility, TransparentBrushProducesNoDrawCall) {
  Harness h;
  h.painter.Begin();
  h.painter.SetBrush(Brush(Color::Transparent()));
  h.painter.FillRect(0.0F, 0.0F, 10.0F, 10.0F);
  h.painter.End();

  EXPECT_EQ(h.mock->CountCalls("DrawTriangles"), 0);
}

// ─── Yaşam döngüsü ──────────────────────────────────────────────────────────

TEST(PainterLifecycle, NullRendererYieldsInvalidPainter) {
  Painter p(nullptr, kViewportW, kViewportH);
  EXPECT_FALSE(p.IsValid());
  // Geçersiz Painter'da çizim çağrıları sessizce yok sayılmalı (çökme yok).
  p.Begin();
  p.FillRect(0.0F, 0.0F, 10.0F, 10.0F);
  p.DrawText(0.0F, 0.0F, "x");
  p.End();
}

TEST(PainterLifecycle, UnbalancedRestoreIsSafe) {
  Harness h;
  h.painter.Begin();
  h.painter.Restore();  // Save() olmadan — uyarı loglanır, çökmez.
  h.painter.End();
  SUCCEED();
}

// ─── Metin batch'leme (Y4 regresyonu) ───────────────────────────────────────
//
// Eskiden her glyph ayrı bir texture'a sahipti; RenderBatcher texture
// değişiminde flush ettiği için "Merhaba" yedi ayrı draw call üretiyordu.
// Glyph atlası ile aynı fontun tüm karakterleri tek sayfayı paylaşır.

TEST(PainterText, MultiCharacterTextIsASingleDrawCall) {
  const std::string font_path = sdl_painter::testing::FindSystemFont();
  if (font_path.empty()) {
    GTEST_SKIP() << "Sistemde TTF font bulunamadı.";
  }

  Harness h;
  auto font = std::make_shared<Font>(font_path, 20);
  ASSERT_TRUE(font->IsValid());

  h.painter.Begin();
  h.painter.SetFont(font);
  h.painter.SetPen(Pen(Color::White(), 1.0F));
  h.painter.DrawText(10.0F, 40.0F, "Merhaba");
  h.painter.End();

  EXPECT_EQ(h.mock->CountCalls("DrawTextured"), 1)
      << "Metin tek draw call'a toplanmadı (glyph atlası devrede değil?).";
}

TEST(PainterText, TextVertexCountMatchesGlyphCount) {
  const std::string font_path = sdl_painter::testing::FindSystemFont();
  if (font_path.empty()) {
    GTEST_SKIP() << "Sistemde TTF font bulunamadı.";
  }

  Harness h;
  auto font = std::make_shared<Font>(font_path, 20);
  ASSERT_TRUE(font->IsValid());

  h.painter.Begin();
  h.painter.SetFont(font);
  h.painter.SetPen(Pen(Color::White(), 1.0F));
  h.painter.DrawText(10.0F, 40.0F, "ABC");
  h.painter.End();

  // 3 glyph * 6 vertex (iki üçgen) = 18
  EXPECT_EQ(h.mock->last_textured_vertices.size(), 18u);
}

TEST(PainterText, TurkishTextRendersAllGlyphs) {
  const std::string font_path = sdl_painter::testing::FindSystemFont();
  if (font_path.empty()) {
    GTEST_SKIP() << "Sistemde TTF font bulunamadı.";
  }

  Harness h;
  auto font = std::make_shared<Font>(font_path, 20);
  ASSERT_TRUE(font->IsValid());

  h.painter.Begin();
  h.painter.SetFont(font);
  h.painter.SetPen(Pen(Color::White(), 1.0F));
  // "Ğüşİöç" — 6 karakter, hepsi çok baytlı UTF-8.
  h.painter.DrawText(10.0F, 40.0F,
                     "\xC4\x9E\xC3\xBC\xC5\x9F\xC4\xB0\xC3\xB6\xC3\xA7");
  h.painter.End();

  EXPECT_EQ(h.mock->CountCalls("DrawTextured"), 1);
  EXPECT_EQ(h.mock->last_textured_vertices.size(), 36u)
      << "6 glyph * 6 vertex bekleniyordu — UTF-8 çözümlemesi eksik olabilir.";
}

TEST(PainterText, GlyphUVsStayWithinAtlasRange) {
  const std::string font_path = sdl_painter::testing::FindSystemFont();
  if (font_path.empty()) {
    GTEST_SKIP() << "Sistemde TTF font bulunamadı.";
  }

  Harness h;
  auto font = std::make_shared<Font>(font_path, 20);
  ASSERT_TRUE(font->IsValid());

  h.painter.Begin();
  h.painter.SetFont(font);
  h.painter.SetPen(Pen(Color::White(), 1.0F));
  h.painter.DrawText(10.0F, 40.0F, "AVW");
  h.painter.End();

  ASSERT_FALSE(h.mock->last_textured_vertices.empty());
  for (const auto& v : h.mock->last_textured_vertices) {
    EXPECT_GE(v.u, 0.0F);
    EXPECT_LE(v.u, 1.0F);
    EXPECT_GE(v.v, 0.0F);
    EXPECT_LE(v.v, 1.0F);
  }
}
