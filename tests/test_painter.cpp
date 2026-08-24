/// @file test_painter.cpp
/// @brief Painter davranış testleri — sahte IRenderer enjeksiyonu ile.
///
/// Painter, pencere gerektirmeyen ctor'u sayesinde MockRenderer ile
/// sürülebilir. Böylece transform yayılımı, kırpma koordinat dönüşümü ve
/// opaklık gibi backend'den bağımsız mantık, GPU olmadan doğrulanabilir.

#include "sdl_painter/brush.h"
#include "sdl_painter/font.h"
#include "sdl_painter/image.h"
#include "sdl_painter/painter.h"
#include "sdl_painter/pen.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <gtest/gtest.h>
#include <memory>
#include <string>
#include <vector>

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
using sdl_painter::TextWrap;

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

/// @brief Bir vertex aralığının eksen hizalı sınır kutusu.
///
/// v1.3.0'dan itibaren transform CPU'da vertex'lere gömülüyor (model matrisi
/// daima birim). Dolayısıyla "transform uygulandı mı?" sorusu artık matrise
/// değil, renderer'a giden **gerçek koordinatlara** bakılarak sınanır.
struct Bounds {
  float min_x{0.0F};
  float min_y{0.0F};
  float max_x{0.0F};
  float max_y{0.0F};
};

/// @param first Aralığın ilk vertex indeksi.
/// @param count Vertex sayısı; 0 ise `first`'ten sona kadar.
template <typename V>
Bounds BoundsOf(const std::vector<V>& verts, std::size_t first = 0,
                std::size_t count = 0) {
  const std::size_t last = (count == 0) ? verts.size() : (first + count);
  Bounds b{verts.at(first).x, verts.at(first).y, verts.at(first).x,
           verts.at(first).y};
  for (std::size_t i = first; i < last; ++i) {
    b.min_x = std::min(b.min_x, verts[i].x);
    b.min_y = std::min(b.min_y, verts[i].y);
    b.max_x = std::max(b.max_x, verts[i].x);
    b.max_y = std::max(b.max_y, verts[i].y);
  }
  return b;
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

  ASSERT_EQ(h.mock->last_vertices.size(), 6u) << "Hiç çizim komutu üretilmedi.";
  const Bounds b = BoundsOf(h.mock->last_vertices);
  EXPECT_FLOAT_EQ(b.min_x, 100.0F);
  EXPECT_FLOAT_EQ(b.min_y, 50.0F);
  EXPECT_FLOAT_EQ(b.max_x, 110.0F);
  EXPECT_FLOAT_EQ(b.max_y, 60.0F);
}

/// @brief Yeni sözleşme: transform vertex'e gömüldüğü için model matrisi
///        çizim anında daima birimdir.
TEST(PainterTransform, ModelMatrixStaysIdentity) {
  Harness h;
  h.painter.Begin();
  h.painter.SetBrush(Brush(Color::Red()));
  h.painter.Translate(100.0F, 50.0F);
  h.painter.Rotate(30.0F);
  h.painter.Scale(2.0F, 2.0F);
  h.painter.FillRect(0.0F, 0.0F, 10.0F, 10.0F);
  h.painter.End();

  ASSERT_FALSE(h.mock->model_at_draw.empty());
  EXPECT_TRUE(IsIdentity(h.mock->model_at_draw.front()))
      << "Model matrisi artık kullanılmıyor; birim kalmalı.";
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

  // Transform artık batch'i kırmıyor: iki dikdörtgen tek draw call'a girer.
  ASSERT_EQ(h.mock->CountCalls("DrawTriangles"), 1);
  ASSERT_EQ(h.mock->last_vertices.size(), 12u);
  const Bounds inside = BoundsOf(h.mock->last_vertices, 0, 6);
  const Bounds outside = BoundsOf(h.mock->last_vertices, 6, 6);
  EXPECT_FLOAT_EQ(inside.min_x, 200.0F);
  EXPECT_FLOAT_EQ(inside.min_y, 300.0F);
  EXPECT_FLOAT_EQ(outside.min_x, 0.0F)
      << "Restore sonrası transform birime dönmeliydi.";
  EXPECT_FLOAT_EQ(outside.min_y, 0.0F);
}

/// @brief REGRESYON (K5): DrawText transform stack'ini yok sayıyordu.
///
/// `Painter::DrawText` `FlushTransform()` çağırmadığı için, Translate/Rotate
/// sonrası çizilen metin renderer'a **eski** model matrisiyle gidiyordu.
/// Görünür etkisi: `examples/text.cpp` ve `examples/vulkan_text.cpp`
/// içindeki "Dönen metin!" bölümü ekranda hiç görünmüyordu (metin ekran
/// dışına, ham koordinatlara çiziliyordu).
TEST(PainterTransform, TranslateIsAppliedToTextDrawCall) {
  SDLPAINTER_REQUIRE_FONT_OR_SKIP(font_path);

  Harness h;
  auto font = std::make_shared<Font>(font_path, 16);
  ASSERT_TRUE(font->IsValid()) << "Font yüklenemedi: " << font_path;

  h.painter.Begin();
  h.painter.SetFont(font);
  h.painter.SetPen(Pen(Color::White(), 1.0F));
  h.painter.Translate(100.0F, 50.0F);
  h.painter.DrawText(0.0F, 0.0F, "A");
  h.painter.End();

  ASSERT_FALSE(h.mock->last_textured_vertices.empty())
      << "DrawText hiç çizim komutu üretmedi.";
  const Bounds moved = BoundsOf(h.mock->last_textured_vertices);

  // Referans: aynı metin, transform'suz.
  Harness ref;
  auto ref_font = std::make_shared<Font>(font_path, 16);
  ref.painter.Begin();
  ref.painter.SetFont(ref_font);
  ref.painter.SetPen(Pen(Color::White(), 1.0F));
  ref.painter.DrawText(0.0F, 0.0F, "A");
  ref.painter.End();
  ASSERT_FALSE(ref.mock->last_textured_vertices.empty());
  const Bounds base = BoundsOf(ref.mock->last_textured_vertices);

  EXPECT_FLOAT_EQ(moved.min_x - base.min_x, 100.0F)
      << "DrawText, güncel transform'u vertex'lere uygulamıyor.";
  EXPECT_FLOAT_EQ(moved.min_y - base.min_y, 50.0F);
}

/// @brief REGRESYON (K5): Rect aşırı yüklemesi de aynı hatayı taşıyordu.
TEST(PainterTransform, TranslateIsAppliedToAlignedTextDrawCall) {
  SDLPAINTER_REQUIRE_FONT_OR_SKIP(font_path);

  Harness h;
  auto font = std::make_shared<Font>(font_path, 16);
  ASSERT_TRUE(font->IsValid());

  h.painter.Begin();
  h.painter.SetFont(font);
  h.painter.SetPen(Pen(Color::White(), 1.0F));
  h.painter.Translate(70.0F, 20.0F);
  h.painter.DrawText(Rect{0.0F, 0.0F, 200.0F, 40.0F}, "A", Alignment::kCenter);
  h.painter.End();

  ASSERT_FALSE(h.mock->last_textured_vertices.empty());
  const Bounds moved = BoundsOf(h.mock->last_textured_vertices);

  Harness ref;
  auto ref_font = std::make_shared<Font>(font_path, 16);
  ref.painter.Begin();
  ref.painter.SetFont(ref_font);
  ref.painter.SetPen(Pen(Color::White(), 1.0F));
  ref.painter.DrawText(Rect{0.0F, 0.0F, 200.0F, 40.0F}, "A",
                       Alignment::kCenter);
  ref.painter.End();
  ASSERT_FALSE(ref.mock->last_textured_vertices.empty());
  const Bounds base = BoundsOf(ref.mock->last_textured_vertices);

  EXPECT_FLOAT_EQ(moved.min_x - base.min_x, 70.0F);
  EXPECT_FLOAT_EQ(moved.min_y - base.min_y, 20.0F);
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
  SDLPAINTER_REQUIRE_FONT_OR_SKIP(font_path);

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
  SDLPAINTER_REQUIRE_FONT_OR_SKIP(font_path);

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
  SDLPAINTER_REQUIRE_FONT_OR_SKIP(font_path);

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
  SDLPAINTER_REQUIRE_FONT_OR_SKIP(font_path);

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

// ─── Primitifler: her çizim metodu renderer'a ulaşıyor mu? ──────────────────
//
// Bu metotlar ince sarmalayıcılardır (tessellate → batcher'a it), ama tam da
// bu yüzden sessizce bozulabilirler: K5'te DrawText'in FlushTransform
// çağırmadığı fark edilmemişti. Aşağıdaki testler her metodun (a) çizim
// ürettiğini ve (b) güncel transform ile ürettiğini kilitler.

namespace {

/// @brief Bir çizim çağrısını sarar; üretilen toplam vertex sayısını döndürür.
template <typename Fn>
std::size_t DrawAndCount(Harness& h, Fn&& draw) {
  h.painter.Begin();
  h.painter.SetPen(Pen(Color::White(), 2.0F));
  h.painter.SetBrush(Brush(Color::Red()));
  draw(h.painter);
  h.painter.End();
  std::size_t total = 0;
  for (const auto& c : h.mock->calls) {
    if (c.name == "DrawTriangles") {
      total += c.vertex_count;
    }
  }
  return total;
}

/// @brief Dosya gerektirmeyen 4x4 RGBA test görüntüsü.
sdl_painter::Image MakeTinyImage() {
  const std::vector<uint8_t> pixels(4 * 4 * 4, 200);
  return sdl_painter::Image::CreateFromData(pixels.data(), 4, 4, 4);
}

}  // namespace

TEST(PainterPrimitives, DrawLineProducesGeometry) {
  Harness h;
  EXPECT_GT(DrawAndCount(h, [](Painter& p) { p.DrawLine(0, 0, 100, 50); }), 0u);
}

TEST(PainterPrimitives, DrawCircleProducesGeometry) {
  Harness h;
  EXPECT_GT(DrawAndCount(h, [](Painter& p) { p.DrawCircle(50, 50, 20); }), 0u);
}

TEST(PainterPrimitives, FillCircleProducesGeometry) {
  Harness h;
  EXPECT_GT(DrawAndCount(h, [](Painter& p) { p.FillCircle(50, 50, 20); }), 0u);
}

TEST(PainterPrimitives, DrawEllipseProducesGeometry) {
  Harness h;
  EXPECT_GT(DrawAndCount(h, [](Painter& p) { p.DrawEllipse(50, 50, 30, 15); }),
            0u);
}

TEST(PainterPrimitives, FillEllipseProducesGeometry) {
  Harness h;
  EXPECT_GT(DrawAndCount(h, [](Painter& p) { p.FillEllipse(50, 50, 30, 15); }),
            0u);
}

TEST(PainterPrimitives, DrawPolygonProducesGeometry) {
  Harness h;
  const std::vector<Point> pts = {{0, 0}, {40, 0}, {40, 40}};
  EXPECT_GT(DrawAndCount(h, [&](Painter& p) { p.DrawPolygon(pts); }), 0u);
}

TEST(PainterPrimitives, FillPolygonProducesGeometry) {
  Harness h;
  const std::vector<Point> pts = {{0, 0}, {40, 0}, {40, 40}};
  EXPECT_EQ(DrawAndCount(h, [&](Painter& p) { p.FillPolygon(pts); }), 3u);
}

TEST(PainterPrimitives, DrawPolylineProducesGeometry) {
  Harness h;
  const std::vector<Point> pts = {{0, 0}, {40, 0}, {40, 40}};
  EXPECT_GT(DrawAndCount(h, [&](Painter& p) { p.DrawPolyline(pts); }), 0u);
}

TEST(PainterPrimitives, DegenerateInputProducesNoDrawCall) {
  Harness h1;
  // Sıfır uzunluklu çizgi: tessellator boş döner.
  EXPECT_EQ(DrawAndCount(h1, [](Painter& p) { p.DrawLine(10, 10, 10, 10); }),
            0u);
  Harness h2;
  EXPECT_EQ(DrawAndCount(h2, [](Painter& p) { p.FillPolygon({{0, 0}}); }), 0u);
  Harness h3;
  EXPECT_EQ(DrawAndCount(h3, [](Painter& p) { p.DrawPolyline({{0, 0}}); }), 0u);
}

// ─── Transform, tüm primitiflere uygulanıyor mu? ────────────────────────────

TEST(PainterPrimitives, TransformReachesEveryPrimitive) {
  struct Case {
    const char* name;
    void (*draw)(Painter&);
  };
  const Case kCases[] = {
      {"DrawLine", [](Painter& p) { p.DrawLine(0, 0, 10, 10); }},
      {"DrawRect", [](Painter& p) { p.DrawRect(0, 0, 10, 10); }},
      {"FillRect", [](Painter& p) { p.FillRect(0, 0, 10, 10); }},
      {"DrawCircle", [](Painter& p) { p.DrawCircle(0, 0, 10); }},
      {"FillCircle", [](Painter& p) { p.FillCircle(0, 0, 10); }},
      {"DrawEllipse", [](Painter& p) { p.DrawEllipse(0, 0, 10, 5); }},
      {"FillEllipse", [](Painter& p) { p.FillEllipse(0, 0, 10, 5); }},
      {"DrawPolygon",
       [](Painter& p) { p.DrawPolygon({{0, 0}, {10, 0}, {10, 10}}); }},
      {"FillPolygon",
       [](Painter& p) { p.FillPolygon({{0, 0}, {10, 0}, {10, 10}}); }},
      {"DrawPolyline",
       [](Painter& p) { p.DrawPolyline({{0, 0}, {10, 0}, {10, 10}}); }},
  };

  for (const Case& c : kCases) {
    SCOPED_TRACE(c.name);
    Harness h;
    h.painter.Begin();
    h.painter.SetPen(Pen(Color::White(), 2.0F));
    h.painter.SetBrush(Brush(Color::Red()));
    h.painter.Translate(123.0F, 45.0F);
    c.draw(h.painter);
    h.painter.End();

    ASSERT_FALSE(h.mock->last_vertices.empty()) << "hic cizim uretilmedi";
    const Bounds moved = BoundsOf(h.mock->last_vertices);

    // Ayni primitif, transform'suz — fark tam olarak oteleme kadar olmali.
    Harness ref;
    ref.painter.Begin();
    ref.painter.SetPen(Pen(Color::White(), 2.0F));
    ref.painter.SetBrush(Brush(Color::Red()));
    c.draw(ref.painter);
    ref.painter.End();
    ASSERT_FALSE(ref.mock->last_vertices.empty());
    const Bounds base = BoundsOf(ref.mock->last_vertices);

    EXPECT_FLOAT_EQ(moved.min_x - base.min_x, 123.0F);
    EXPECT_FLOAT_EQ(moved.min_y - base.min_y, 45.0F);
  }
}

// ─── Transform işlemleri ────────────────────────────────────────────────────

TEST(PainterTransform, RotateIsAppliedToVertices) {
  Harness h;
  h.painter.Begin();
  h.painter.SetBrush(Brush(Color::Red()));
  h.painter.Rotate(90.0F);
  h.painter.FillRect(0.0F, 0.0F, 10.0F, 10.0F);
  h.painter.End();

  // 90 derece donuste (x, y) -> (-y, x): kare sol yariya gecer.
  ASSERT_EQ(h.mock->last_vertices.size(), 6u);
  const Bounds b = BoundsOf(h.mock->last_vertices);
  EXPECT_NEAR(b.min_x, -10.0F, 1e-4F);
  EXPECT_NEAR(b.max_x, 0.0F, 1e-4F);
  EXPECT_NEAR(b.min_y, 0.0F, 1e-4F);
  EXPECT_NEAR(b.max_y, 10.0F, 1e-4F);
}

TEST(PainterTransform, ScaleIsAppliedToVertices) {
  Harness h;
  h.painter.Begin();
  h.painter.SetBrush(Brush(Color::Red()));
  h.painter.Scale(2.0F, 3.0F);
  h.painter.FillRect(0.0F, 0.0F, 10.0F, 10.0F);
  h.painter.End();

  ASSERT_EQ(h.mock->last_vertices.size(), 6u);
  const Bounds b = BoundsOf(h.mock->last_vertices);
  EXPECT_FLOAT_EQ(b.max_x, 20.0F);
  EXPECT_FLOAT_EQ(b.max_y, 30.0F);
}

TEST(PainterTransform, ResetTransformClearsAccumulatedTransform) {
  Harness h;
  h.painter.Begin();
  h.painter.SetBrush(Brush(Color::Red()));
  h.painter.Translate(50.0F, 60.0F);
  h.painter.Rotate(45.0F);
  h.painter.ResetTransform();
  h.painter.FillRect(0.0F, 0.0F, 10.0F, 10.0F);
  h.painter.End();

  ASSERT_EQ(h.mock->last_vertices.size(), 6u);
  const Bounds b = BoundsOf(h.mock->last_vertices);
  EXPECT_FLOAT_EQ(b.min_x, 0.0F);
  EXPECT_FLOAT_EQ(b.min_y, 0.0F);
  EXPECT_FLOAT_EQ(b.max_x, 10.0F);
  EXPECT_FLOAT_EQ(b.max_y, 10.0F);
}

/// Transform'lar birikmeli (post-multiply, QPainter semantigi).
TEST(PainterTransform, TranslationsAccumulate) {
  Harness h;
  h.painter.Begin();
  h.painter.SetBrush(Brush(Color::Red()));
  h.painter.Translate(10.0F, 20.0F);
  h.painter.Translate(5.0F, 7.0F);
  h.painter.FillRect(0.0F, 0.0F, 10.0F, 10.0F);
  h.painter.End();

  ASSERT_EQ(h.mock->last_vertices.size(), 6u);
  const Bounds b = BoundsOf(h.mock->last_vertices);
  EXPECT_FLOAT_EQ(b.min_x, 15.0F);
  EXPECT_FLOAT_EQ(b.min_y, 27.0F);
}

/// Ic ice Save/Restore dogru sirada geri sarmali.
TEST(PainterTransform, NestedSaveRestoreUnwindsInOrder) {
  Harness h;
  h.painter.Begin();
  h.painter.SetBrush(Brush(Color::Red()));

  h.painter.Save();
  h.painter.Translate(100.0F, 0.0F);
  h.painter.Save();
  h.painter.Translate(10.0F, 0.0F);
  h.painter.FillRect(0, 0, 1, 1);  // 110
  h.painter.Restore();
  h.painter.FillRect(0, 0, 1, 1);  // 100
  h.painter.Restore();
  h.painter.FillRect(0, 0, 1, 1);  // 0
  h.painter.End();

  // Uc dikdortgen de tek batch'e girer; sirasiyla 6'sar vertex.
  ASSERT_EQ(h.mock->CountCalls("DrawTriangles"), 1);
  ASSERT_EQ(h.mock->last_vertices.size(), 18u);
  EXPECT_FLOAT_EQ(BoundsOf(h.mock->last_vertices, 0, 6).min_x, 110.0F);
  EXPECT_FLOAT_EQ(BoundsOf(h.mock->last_vertices, 6, 6).min_x, 100.0F);
  EXPECT_FLOAT_EQ(BoundsOf(h.mock->last_vertices, 12, 6).min_x, 0.0F);
}

// ─── Kare istatistikleri ────────────────────────────────────────────────────

TEST(PainterFrameStats, CountsDrawCallsAndVertices) {
  Harness h;
  h.painter.Begin();
  h.painter.SetBrush(Brush(Color::Red()));
  for (int32_t i = 0; i < 10; ++i) {
    h.painter.FillRect(static_cast<float>(i), 0.0F, 5.0F, 5.0F);
  }
  h.painter.End();

  const auto& stats = h.painter.GetFrameStats();
  // Hepsi tek batch'e girer: renk vertex'te tasindigi icin batch kirilmaz.
  EXPECT_EQ(stats.draw_calls, 1u);
  EXPECT_EQ(stats.batches, stats.draw_calls);
  EXPECT_EQ(stats.vertices, 60u);  // 10 dikdortgen x 6 vertex
}

/// @brief Ana kazanc: transform artik batch'i kirmiyor (bkz. benchmarks/).
TEST(PainterFrameStats, TransformDoesNotBreakBatch) {
  Harness h;
  h.painter.Begin();
  h.painter.SetBrush(Brush(Color::Red()));
  for (int32_t i = 0; i < 10; ++i) {
    h.painter.Save();
    h.painter.Translate(static_cast<float>(i) * 10.0F, 0.0F);
    h.painter.Rotate(static_cast<float>(i));
    h.painter.FillRect(0.0F, 0.0F, 5.0F, 5.0F);
    h.painter.Restore();
  }
  h.painter.End();

  EXPECT_EQ(h.painter.GetFrameStats().draw_calls, 1u);
}

/// @brief Opaklik hala bir uniform: degisimi batch'i kirar (bilincli sinir).
TEST(PainterFrameStats, OpacityChangeBreaksBatch) {
  Harness h;
  h.painter.Begin();
  h.painter.SetBrush(Brush(Color::Red()));
  h.painter.SetOpacity(1.0F);
  h.painter.FillRect(0.0F, 0.0F, 5.0F, 5.0F);
  h.painter.SetOpacity(0.5F);
  h.painter.FillRect(0.0F, 0.0F, 5.0F, 5.0F);
  h.painter.End();

  EXPECT_EQ(h.painter.GetFrameStats().draw_calls, 2u);
}

TEST(PainterFrameStats, ClipCountsAsStateChange) {
  Harness h;
  h.painter.Begin();
  const uint32_t before = h.painter.GetFrameStats().state_changes;
  h.painter.SetBrush(Brush(Color::Red()));
  h.painter.SetClipRect(Rect{0.0F, 0.0F, 10.0F, 10.0F});
  h.painter.FillRect(0.0F, 0.0F, 5.0F, 5.0F);
  h.painter.ClearClip();
  h.painter.End();

  // SetClipRect + ClearClip = 2 durum degisikligi (Begin'inkine ek olarak).
  EXPECT_GE(h.painter.GetFrameStats().state_changes, before + 2u);
}

TEST(PainterFrameStats, ResetsEachFrame) {
  Harness h;
  h.painter.Begin();
  h.painter.SetBrush(Brush(Color::Red()));
  h.painter.FillRect(0.0F, 0.0F, 5.0F, 5.0F);
  h.painter.End();
  ASSERT_EQ(h.painter.GetFrameStats().draw_calls, 1u);

  h.painter.Begin();
  h.painter.End();
  EXPECT_EQ(h.painter.GetFrameStats().draw_calls, 0u);
  EXPECT_EQ(h.painter.GetFrameStats().vertices, 0u);
}

/// @brief MockRenderer GPU olcumu desteklemiyor; varsayilan 0 gelmeli.
TEST(PainterFrameStats, GpuTimeIsZeroWhenUnsupported) {
  Harness h;
  h.painter.Begin();
  h.painter.End();
  EXPECT_DOUBLE_EQ(h.painter.GetFrameStats().gpu_frame_ms, 0.0);
}

// ─── Karıştırma modu ────────────────────────────────────────────────────────

TEST(PainterBlend, DefaultIsAlphaAndNoModeIsPushedWhenUnchanged) {
  Harness h;
  EXPECT_EQ(h.painter.GetBlendMode(), sdl_painter::BlendMode::kAlpha);

  h.painter.Begin();
  h.painter.SetBrush(Brush(Color::Red()));
  h.painter.FillRect(0.0F, 0.0F, 10.0F, 10.0F);
  h.painter.End();

  // Varsayilan zaten alfa: renderer'a gereksiz durum yazilmamali.
  EXPECT_TRUE(h.mock->blend_calls.empty())
      << "Mod degismedigi halde SetBlendMode cagrildi.";
}

TEST(PainterBlend, ChangingModeReachesTheRenderer) {
  Harness h;
  h.painter.Begin();
  h.painter.SetBlendMode(sdl_painter::BlendMode::kAdditive);
  h.painter.SetBrush(Brush(Color::Red()));
  h.painter.FillRect(0.0F, 0.0F, 10.0F, 10.0F);
  h.painter.End();

  ASSERT_FALSE(h.mock->blend_calls.empty());
  EXPECT_EQ(h.mock->blend_calls.back(), sdl_painter::BlendMode::kAdditive);
}

/// @brief Karıştırma bir GPU durumu: vertex'te taşınamaz, bu yüzden batch'i
///        kırar. Renk ve tint'in aksine.
TEST(PainterBlend, ChangingModeBreaksTheBatch) {
  Harness h;
  h.painter.Begin();
  h.painter.SetBrush(Brush(Color::Red()));
  h.painter.FillRect(0.0F, 0.0F, 10.0F, 10.0F);
  h.painter.SetBlendMode(sdl_painter::BlendMode::kAdditive);
  h.painter.FillRect(20.0F, 0.0F, 10.0F, 10.0F);
  h.painter.End();

  EXPECT_EQ(h.mock->CountCalls("DrawTriangles"), 2)
      << "Mod degisimi flush etmedi; iki mod tek batch'e girdi.";
}

TEST(PainterBlend, SameModeTwiceDoesNotBreakTheBatch) {
  Harness h;
  h.painter.Begin();
  h.painter.SetBlendMode(sdl_painter::BlendMode::kAdditive);
  h.painter.SetBrush(Brush(Color::Red()));
  h.painter.FillRect(0.0F, 0.0F, 10.0F, 10.0F);
  h.painter.SetBlendMode(sdl_painter::BlendMode::kAdditive);
  h.painter.FillRect(20.0F, 0.0F, 10.0F, 10.0F);
  h.painter.End();

  EXPECT_EQ(h.mock->CountCalls("DrawTriangles"), 1)
      << "Ayni mod yeniden yazilinca gereksiz flush olustu.";
}

TEST(PainterBlend, SaveRestoreCoversBlendMode) {
  Harness h;
  h.painter.SetBlendMode(sdl_painter::BlendMode::kAlpha);
  h.painter.Save();
  h.painter.SetBlendMode(sdl_painter::BlendMode::kMultiply);
  EXPECT_EQ(h.painter.GetBlendMode(), sdl_painter::BlendMode::kMultiply);
  h.painter.Restore();
  EXPECT_EQ(h.painter.GetBlendMode(), sdl_painter::BlendMode::kAlpha);
}

TEST(PainterBlend, TexturedDrawsAlsoHonourTheMode) {
  Harness h;
  const auto image = MakeTinyImage();
  ASSERT_TRUE(image.IsValid());

  h.painter.Begin();
  h.painter.SetBlendMode(sdl_painter::BlendMode::kAdditive);
  h.painter.DrawImage(image, 0.0F, 0.0F);
  h.painter.End();

  ASSERT_FALSE(h.mock->blend_calls.empty());
  EXPECT_EQ(h.mock->blend_calls.back(), sdl_painter::BlendMode::kAdditive);
}

// ─── Doku filtresi ──────────────────────────────────────────────────────────

TEST(PainterFilter, DefaultIsLinear) {
  Harness h;
  const auto image = MakeTinyImage();
  h.painter.Begin();
  h.painter.DrawImage(image, 0.0F, 0.0F);
  h.painter.End();
  EXPECT_EQ(h.mock->last_filter, sdl_painter::TextureFilter::kLinear);
}

TEST(PainterFilter, NearestReachesTheRenderer) {
  Harness h;
  auto image = MakeTinyImage();
  ASSERT_TRUE(image.IsValid());
  image.SetFilter(sdl_painter::TextureFilter::kNearest);

  h.painter.Begin();
  h.painter.DrawImage(image, 0.0F, 0.0F);
  h.painter.End();

  EXPECT_EQ(h.mock->last_filter, sdl_painter::TextureFilter::kNearest);
}

/// @brief Filtre doku YARATILIRKEN uygulanır: sonradan değiştirmek
///        önbelleklenmiş dokuyu etkilemez. Belgelenmiş davranış.
TEST(PainterFilter, ChangingAfterUploadDoesNotRecreateTheTexture) {
  Harness h;
  auto image = MakeTinyImage();
  h.painter.Begin();
  h.painter.DrawImage(image, 0.0F, 0.0F);
  h.painter.End();
  ASSERT_EQ(h.mock->create_texture_count, 1);

  image.SetFilter(sdl_painter::TextureFilter::kNearest);
  h.painter.Begin();
  h.painter.DrawImage(image, 0.0F, 0.0F);
  h.painter.End();

  EXPECT_EQ(h.mock->create_texture_count, 1)
      << "Filtre degisimi dokuyu yeniden yaratti.";
}

// ─── Dokulu ızgara ──────────────────────────────────────────────────────────

namespace {

/// @brief `cols` x `rows` hücrelik düzgün bir ızgara üret.
std::vector<sdl_painter::Point> MakeGrid(int32_t cols, int32_t rows, float w,
                                         float h) {
  std::vector<sdl_painter::Point> pts;
  for (int32_t r = 0; r <= rows; ++r) {
    for (int32_t c = 0; c <= cols; ++c) {
      pts.push_back({w * static_cast<float>(c) / static_cast<float>(cols),
                     h * static_cast<float>(r) / static_cast<float>(rows)});
    }
  }
  return pts;
}

}  // namespace

TEST(PainterMesh, ProducesTwoTrianglesPerCell) {
  Harness h;
  const auto image = MakeTinyImage();
  ASSERT_TRUE(image.IsValid());

  h.painter.Begin();
  h.painter.DrawImageMesh(image, 4, 3, MakeGrid(4, 3, 100.0F, 60.0F));
  h.painter.End();

  EXPECT_EQ(h.mock->last_textured_vertices.size(), 4u * 3u * 6u);
}

TEST(PainterMesh, UVsSpanTheWholeTexture) {
  Harness h;
  const auto image = MakeTinyImage();
  h.painter.Begin();
  h.painter.DrawImageMesh(image, 2, 2, MakeGrid(2, 2, 100.0F, 100.0F));
  h.painter.End();

  ASSERT_FALSE(h.mock->last_textured_vertices.empty());
  float min_u = 1e9F;
  float max_u = -1e9F;
  float min_v = 1e9F;
  float max_v = -1e9F;
  for (const auto& v : h.mock->last_textured_vertices) {
    min_u = std::fmin(min_u, v.u);
    max_u = std::fmax(max_u, v.u);
    min_v = std::fmin(min_v, v.v);
    max_v = std::fmax(max_v, v.v);
  }
  EXPECT_FLOAT_EQ(min_u, 0.0F);
  EXPECT_FLOAT_EQ(max_u, 1.0F);
  EXPECT_FLOAT_EQ(min_v, 0.0F);
  EXPECT_FLOAT_EQ(max_v, 1.0F);
}

/// @brief Deforme edilmiş ızgara: köşeler bağımsız hareket edebilmeli — bu,
///        `DrawImage`'ın eksen hizalı `Rect` ile yapamadığı şey.
TEST(PainterMesh, CornersMoveIndependently) {
  Harness h;
  const auto image = MakeTinyImage();
  auto grid = MakeGrid(1, 1, 100.0F, 100.0F);
  ASSERT_EQ(grid.size(), 4u);
  grid[0] = {50.0F, -40.0F};  // sol-ust kosesi disari cekildi

  h.painter.Begin();
  h.painter.DrawImageMesh(image, 1, 1, grid);
  h.painter.End();

  const Bounds b = BoundsOf(h.mock->last_textured_vertices);
  EXPECT_FLOAT_EQ(b.min_y, -40.0F)
      << "Kose bagimsiz hareket etmedi; eksen hizali dikdortgen cizilmis.";
}

TEST(PainterMesh, WrongPointCountIsRejected) {
  Harness h;
  const auto image = MakeTinyImage();
  h.painter.Begin();
  // 3x2 izgara 12 nokta ister; 10 verildi.
  h.painter.DrawImageMesh(image, 3, 2, MakeGrid(4, 1, 100.0F, 100.0F));
  h.painter.End();
  EXPECT_EQ(h.mock->CountCalls("DrawTextured"), 0)
      << "Yanlis nokta sayisi sessizce kabul edildi.";
}

TEST(PainterMesh, DegenerateGridProducesNothing) {
  Harness h;
  const auto image = MakeTinyImage();
  h.painter.Begin();
  h.painter.DrawImageMesh(image, 0, 2, MakeGrid(1, 1, 10.0F, 10.0F));
  h.painter.DrawImageMesh(image, 2, -1, MakeGrid(1, 1, 10.0F, 10.0F));
  h.painter.End();
  EXPECT_EQ(h.mock->CountCalls("DrawTextured"), 0);
}

TEST(PainterMesh, TintReachesEveryVertex) {
  Harness h;
  const auto image = MakeTinyImage();
  h.painter.Begin();
  h.painter.DrawImageMesh(image, 2, 2, MakeGrid(2, 2, 100.0F, 100.0F),
                          Color{255, 0, 0, 128});
  h.painter.End();

  ASSERT_FALSE(h.mock->last_textured_vertices.empty());
  for (const auto& v : h.mock->last_textured_vertices) {
    EXPECT_EQ(v.r, 255);
    EXPECT_EQ(v.a, 128);
  }
}

// ─── Gradient fırça ─────────────────────────────────────────────────────────

namespace {

/// @brief Verilen konuma en yakın vertex'in rengi.
sdl_painter::Color ColorNear(const std::vector<sdl_painter::Vertex>& verts,
                             float x, float y) {
  float best = 1e18F;
  sdl_painter::Color out;
  for (const auto& v : verts) {
    const float d = (v.x - x) * (v.x - x) + (v.y - y) * (v.y - y);
    if (d < best) {
      best = d;
      out = sdl_painter::Color{v.r, v.g, v.b, v.a};
    }
  }
  return out;
}

}  // namespace

TEST(PainterGradient, SolidBrushIsUnchangedByDefault) {
  Harness h;
  h.painter.Begin();
  h.painter.SetBrush(Brush(Color::Red()));
  h.painter.FillRect(0.0F, 0.0F, 100.0F, 100.0F);
  h.painter.End();

  ASSERT_FALSE(h.mock->last_vertices.empty());
  for (const auto& v : h.mock->last_vertices) {
    EXPECT_EQ(v.r, 255);
    EXPECT_EQ(v.g, 0);
    EXPECT_EQ(v.b, 0);
  }
}

TEST(PainterGradient, LinearGradientEndpointsCarryTheEndColours) {
  Harness h;
  const Brush g = Brush::LinearGradient({0.0F, 0.0F}, {0.0F, 100.0F},
                                        Color::Red(), Color::Blue());
  h.painter.Begin();
  h.painter.SetBrush(g);
  h.painter.FillRect(0.0F, 0.0F, 100.0F, 100.0F);
  h.painter.End();

  ASSERT_FALSE(h.mock->last_vertices.empty());
  const Color top = ColorNear(h.mock->last_vertices, 50.0F, 0.0F);
  const Color bottom = ColorNear(h.mock->last_vertices, 50.0F, 100.0F);

  EXPECT_EQ(top.r, 255);
  EXPECT_EQ(top.b, 0);
  EXPECT_EQ(bottom.r, 0);
  EXPECT_EQ(bottom.b, 255);
}

/// @brief Gradient eksenine DİK yönde renk değişmemeli.
TEST(PainterGradient, ColourIsConstantPerpendicularToTheAxis) {
  Harness h;
  const Brush g = Brush::LinearGradient({0.0F, 0.0F}, {0.0F, 100.0F},
                                        Color::Red(), Color::Blue());
  h.painter.Begin();
  h.painter.SetBrush(g);
  h.painter.FillRect(0.0F, 0.0F, 100.0F, 100.0F);
  h.painter.End();

  const Color left = ColorNear(h.mock->last_vertices, 0.0F, 0.0F);
  const Color right = ColorNear(h.mock->last_vertices, 100.0F, 0.0F);
  EXPECT_EQ(left.r, right.r);
  EXPECT_EQ(left.b, right.b);
}

/// @brief Sıfır uzunluklu gradient sıfıra bölmemeli — düz renge düşmeli.
TEST(PainterGradient, ZeroLengthLinearGradientFallsBackToStartColour) {
  Harness h;
  const Brush g = Brush::LinearGradient({50.0F, 50.0F}, {50.0F, 50.0F},
                                        Color::Green(), Color::Blue());
  h.painter.Begin();
  h.painter.SetBrush(g);
  h.painter.FillRect(0.0F, 0.0F, 100.0F, 100.0F);
  h.painter.End();

  ASSERT_FALSE(h.mock->last_vertices.empty());
  for (const auto& v : h.mock->last_vertices) {
    EXPECT_EQ(v.g, 255);
    EXPECT_EQ(v.b, 0);
  }
}

TEST(PainterGradient, RadialGradientIsBrightestAtTheCentre) {
  Harness h;
  const Brush g =
      Brush::RadialGradient({50.0F, 50.0F}, 50.0F, Color::Red(), Color::Blue());
  h.painter.Begin();
  h.painter.SetBrush(g);
  h.painter.FillCircle(50.0F, 50.0F, 50.0F);
  h.painter.End();

  ASSERT_FALSE(h.mock->last_vertices.empty());
  const Color centre = ColorNear(h.mock->last_vertices, 50.0F, 50.0F);
  const Color edge = ColorNear(h.mock->last_vertices, 100.0F, 50.0F);
  EXPECT_EQ(centre.r, 255);
  EXPECT_EQ(centre.b, 0);
  EXPECT_EQ(edge.r, 0);
  EXPECT_EQ(edge.b, 255);
}

TEST(PainterGradient, RadialWithZeroRadiusFallsBackToStartColour) {
  Harness h;
  const Brush g =
      Brush::RadialGradient({50.0F, 50.0F}, 0.0F, Color::Green(), Color::Blue());
  h.painter.Begin();
  h.painter.SetBrush(g);
  h.painter.FillRect(0.0F, 0.0F, 100.0F, 100.0F);
  h.painter.End();

  for (const auto& v : h.mock->last_vertices) {
    EXPECT_EQ(v.g, 255);
  }
}

/// @brief Gradient'in varlık sebebi: renk vertex'te taşındığı için batch'i
///        kırmaz. Kırsaydı özelliğin bedeli kazancından büyük olurdu.
TEST(PainterGradient, GradientsDoNotBreakBatching) {
  Harness h;
  h.painter.Begin();
  h.painter.SetBrush(Brush::LinearGradient({0.0F, 0.0F}, {100.0F, 0.0F},
                                           Color::Red(), Color::Blue()));
  h.painter.FillRect(0.0F, 0.0F, 100.0F, 50.0F);
  h.painter.SetBrush(Brush::RadialGradient({200.0F, 50.0F}, 40.0F,
                                           Color::Green(), Color::White()));
  h.painter.FillRect(150.0F, 0.0F, 100.0F, 50.0F);
  h.painter.SetBrush(Brush(Color::Red()));
  h.painter.FillRect(300.0F, 0.0F, 100.0F, 50.0F);
  h.painter.End();

  EXPECT_EQ(h.mock->CountCalls("DrawTriangles"), 1)
      << "Gradient batch'i kirdi.";
}

TEST(PainterGradient, TransformStillAppliesToGradientFills) {
  Harness h;
  h.painter.Begin();
  h.painter.Translate(200.0F, 300.0F);
  h.painter.SetBrush(Brush::LinearGradient({0.0F, 0.0F}, {0.0F, 100.0F},
                                           Color::Red(), Color::Blue()));
  h.painter.FillRect(0.0F, 0.0F, 100.0F, 100.0F);
  h.painter.End();

  const Bounds b = BoundsOf(h.mock->last_vertices);
  EXPECT_FLOAT_EQ(b.min_x, 200.0F);
  EXPECT_FLOAT_EQ(b.min_y, 300.0F);
  // Renk sekil-yerel koordinata gore hesaplanir: ust kenar hala kirmizi.
  const Color top = ColorNear(h.mock->last_vertices, 250.0F, 300.0F);
  EXPECT_EQ(top.r, 255);
}

TEST(BrushGradient, TransparentToOpaqueIsStillVisible) {
  const Brush g = Brush::LinearGradient({0.0F, 0.0F}, {10.0F, 0.0F},
                                        Color::Transparent(), Color::Red());
  EXPECT_TRUE(g.IsVisible())
      << "Saydamdan opaga giden gradient gorunmez sayildi.";
}

TEST(BrushGradient, EqualityCoversGradientParameters) {
  const Brush a = Brush::LinearGradient({0.0F, 0.0F}, {10.0F, 0.0F},
                                        Color::Red(), Color::Blue());
  const Brush b = Brush::LinearGradient({0.0F, 0.0F}, {10.0F, 0.0F},
                                        Color::Red(), Color::Blue());
  const Brush c = Brush::LinearGradient({0.0F, 0.0F}, {20.0F, 0.0F},
                                        Color::Red(), Color::Blue());
  const Brush solid(Color::Red());
  EXPECT_EQ(a, b);
  EXPECT_NE(a, c);
  EXPECT_NE(a, solid);
}

// ─── Viewport ───────────────────────────────────────────────────────────────

TEST(PainterViewport, SetViewportForwardsRectToRenderer) {
  Harness h;
  h.mock->viewport_calls.clear();
  h.painter.SetViewport(100, 50, 300, 200);

  ASSERT_FALSE(h.mock->viewport_calls.empty());
  const auto& v = h.mock->viewport_calls.back();
  EXPECT_EQ(v.w, 300);
  EXPECT_EQ(v.h, 200);
  EXPECT_EQ(v.x, 100);
  // OpenGL viewport orijini sol ALTTA: y = yuzey yuksekligi - y - h.
  EXPECT_EQ(v.y, kViewportH - 50 - 200);
}

TEST(PainterViewport, VulkanBackendDoesNotFlipViewportY) {
  auto mock = std::make_unique<MockRenderer>();
  mock->backend = RendererBackend::kVulkan;
  Harness h(std::move(mock));

  h.mock->viewport_calls.clear();
  h.painter.SetViewport(100, 50, 300, 200);

  ASSERT_FALSE(h.mock->viewport_calls.empty());
  EXPECT_EQ(h.mock->viewport_calls.back().y, 50)
      << "Vulkan'da viewport Y'si ters cevrilmemeli.";
}

TEST(PainterViewport, InvalidSizeIsIgnored) {
  Harness h;
  h.mock->viewport_calls.clear();
  h.painter.SetViewport(0, 0, 0, 100);
  h.painter.SetViewport(0, 0, 100, -5);
  EXPECT_TRUE(h.mock->viewport_calls.empty());
}

/// @brief Viewport çizim koordinatlarını yerelleştirir: aynı koordinat,
///        viewport farklıysa ekranda farklı yere düşer.
TEST(PainterViewport, DrawingCoordinatesBecomeViewportLocal) {
  Harness h;
  h.painter.Begin();
  h.painter.SetViewport(0, 0, 200, 100);
  h.painter.SetBrush(Brush(Color::Red()));
  h.painter.FillRect(0.0F, 0.0F, 50.0F, 50.0F);
  h.painter.End();

  ASSERT_FALSE(h.mock->last_vertices.empty());
  const Bounds b = BoundsOf(h.mock->last_vertices);
  // Vertex koordinatlari degismez; degisen projeksiyondur.
  EXPECT_FLOAT_EQ(b.min_x, 0.0F);
  EXPECT_FLOAT_EQ(b.max_x, 50.0F);
}

TEST(PainterViewport, ResetRestoresTheFullSurface) {
  Harness h;
  h.painter.SetViewport(10, 10, 100, 100);
  h.mock->viewport_calls.clear();
  h.painter.ResetViewport();

  ASSERT_FALSE(h.mock->viewport_calls.empty());
  const auto& v = h.mock->viewport_calls.back();
  EXPECT_EQ(v.x, 0);
  EXPECT_EQ(v.y, 0);
  EXPECT_EQ(v.w, kViewportW);
  EXPECT_EQ(v.h, kViewportH);
}

TEST(PainterViewport, ResetWithoutCustomViewportDoesNothing) {
  Harness h;
  h.mock->viewport_calls.clear();
  h.painter.ResetViewport();
  EXPECT_TRUE(h.mock->viewport_calls.empty());
}

/// @brief Viewport bir GPU durumu: değiştirmek biriken çizimi flush etmeli,
///        yoksa önceki panelin geometrisi yeni viewport'la çizilirdi.
TEST(PainterViewport, ChangingViewportFlushesPendingGeometry) {
  Harness h;
  h.painter.Begin();
  h.painter.SetBrush(Brush(Color::Red()));
  h.painter.FillRect(0.0F, 0.0F, 10.0F, 10.0F);
  h.painter.SetViewport(0, 0, 100, 100);
  h.painter.SetBrush(Brush(Color::Blue()));
  h.painter.FillRect(0.0F, 0.0F, 10.0F, 10.0F);
  h.painter.End();

  EXPECT_EQ(h.mock->CountCalls("DrawTriangles"), 2)
      << "Viewport degisimi flush etmedi; iki panel tek batch'e girdi.";
}

/// @brief Kırpma viewport-yerel verilir ama scissor pencere koordinatındadır.
TEST(PainterViewport, ClipRectIsOffsetByTheViewportOrigin) {
  Harness h;
  h.painter.Begin();
  h.painter.SetViewport(200, 100, 300, 200);
  h.painter.SetClipRect(Rect{10.0F, 20.0F, 50.0F, 40.0F});
  h.painter.SetBrush(Brush(Color::Red()));
  h.painter.FillRect(0.0F, 0.0F, 10.0F, 10.0F);
  h.painter.End();

  ASSERT_FALSE(h.mock->scissor_calls.empty());
  const auto& s = h.mock->scissor_calls.back();
  EXPECT_EQ(s.x, 200 + 10) << "Kirpma viewport orijinine tasinmadi.";
  // OpenGL: y = yuzey yuksekligi - (viewport_y + rect_y) - rect_h
  EXPECT_EQ(s.y, kViewportH - (100 + 20) - 40);
}

// ─── Yuvarlatılmış dikdörtgen ───────────────────────────────────────────────

TEST(PainterRoundedRect, ZeroRadiusFillMatchesFillRect) {
  Harness a;
  a.painter.Begin();
  a.painter.SetBrush(Brush(Color::Red()));
  a.painter.FillRect(10.0F, 20.0F, 100.0F, 50.0F);
  a.painter.End();
  const Bounds plain = BoundsOf(a.mock->last_vertices);

  Harness b;
  b.painter.Begin();
  b.painter.SetBrush(Brush(Color::Red()));
  b.painter.FillRoundedRect(10.0F, 20.0F, 100.0F, 50.0F, 0.0F);
  b.painter.End();
  const Bounds rounded = BoundsOf(b.mock->last_vertices);

  EXPECT_FLOAT_EQ(plain.min_x, rounded.min_x);
  EXPECT_FLOAT_EQ(plain.min_y, rounded.min_y);
  EXPECT_FLOAT_EQ(plain.max_x, rounded.max_x);
  EXPECT_FLOAT_EQ(plain.max_y, rounded.max_y);
}

TEST(PainterRoundedRect, FillUsesBrushAndStaysInsideTheBox) {
  Harness h;
  h.painter.Begin();
  h.painter.SetBrush(Brush(Color::Blue()));
  h.painter.FillRoundedRect(30.0F, 40.0F, 160.0F, 90.0F, 25.0F);
  h.painter.End();

  ASSERT_FALSE(h.mock->last_vertices.empty());
  EXPECT_EQ(h.mock->last_vertices[0].b, 255);
  const Bounds b = BoundsOf(h.mock->last_vertices);
  EXPECT_GE(b.min_x, 30.0F - 1e-3F);
  EXPECT_LE(b.max_x, 190.0F + 1e-3F);
  EXPECT_GE(b.min_y, 40.0F - 1e-3F);
  EXPECT_LE(b.max_y, 130.0F + 1e-3F);
}

TEST(PainterRoundedRect, StrokeHonoursPenDashAndJoin) {
  Harness h;
  h.painter.Begin();
  h.painter.SetPen(Pen(Color::White(), 4.0F));
  h.painter.DrawRoundedRect(0.0F, 0.0F, 120.0F, 80.0F, 20.0F);
  h.painter.End();
  const std::size_t solid = h.mock->last_vertices.size();

  h.mock->Reset();
  Pen dashed(Color::White(), 4.0F);
  dashed.SetDashPattern({8.0F, 8.0F});
  h.painter.Begin();
  h.painter.SetPen(dashed);
  h.painter.DrawRoundedRect(0.0F, 0.0F, 120.0F, 80.0F, 20.0F);
  h.painter.End();

  ASSERT_GT(solid, 0u);
  EXPECT_NE(h.mock->last_vertices.size(), solid)
      << "DrawRoundedRect kalemin kesik desenini yok sayiyor.";
}

TEST(PainterRoundedRect, NoBrushOrPenMeansNoDrawCall) {
  Harness h;
  h.painter.Begin();
  h.painter.SetBrush(Brush(Color::Transparent()));
  h.painter.SetPen(Pen::NoPen());
  h.painter.FillRoundedRect(0.0F, 0.0F, 100.0F, 60.0F, 10.0F);
  h.painter.DrawRoundedRect(0.0F, 0.0F, 100.0F, 60.0F, 10.0F);
  h.painter.End();
  EXPECT_EQ(h.mock->CountCalls("DrawTriangles"), 0);
}

TEST(PainterRoundedRect, DegenerateSizeProducesNothing) {
  Harness h;
  h.painter.Begin();
  h.painter.SetBrush(Brush(Color::Red()));
  h.painter.SetPen(Pen(Color::White(), 2.0F));
  h.painter.FillRoundedRect(0.0F, 0.0F, 0.0F, 60.0F, 10.0F);
  h.painter.DrawRoundedRect(0.0F, 0.0F, 100.0F, -5.0F, 10.0F);
  h.painter.End();
  EXPECT_EQ(h.mock->CountCalls("DrawTriangles"), 0);
}

// ─── Çok satırlı metin ve sözcük kaydırma ───────────────────────────────────

namespace {

/// @brief Metin testleri için font yüklü bir Harness.
struct TextHarness {
  Harness h;
  std::shared_ptr<Font> font;

  explicit TextHarness(const std::string& path, int32_t size = 16)
      : font(std::make_shared<Font>(path, size)) {}
};

}  // namespace

TEST(PainterText, LineHeightIsPositiveAndAtLeastPointSize) {
  SDLPAINTER_REQUIRE_FONT_OR_SKIP(font_path);
  const Font font(font_path, 20);
  ASSERT_TRUE(font.IsValid());
  EXPECT_GT(font.LineHeight(), 0);
  EXPECT_GE(font.LineHeight(), font.Ascent());
}

/// @brief Satır sonu artık gerçekten satır bölmeli — eskiden tek bir görünmez
///        glyph olarak ele alınıyordu.
TEST(PainterText, NewlineStartsANewLine) {
  SDLPAINTER_REQUIRE_FONT_OR_SKIP(font_path);
  TextHarness t(font_path);
  ASSERT_TRUE(t.font->IsValid());

  t.h.painter.Begin();
  t.h.painter.SetFont(t.font);
  t.h.painter.SetPen(Pen(Color::White(), 1.0F));
  t.h.painter.DrawText(10.0F, 40.0F, "AA");
  t.h.painter.End();
  const Bounds single = BoundsOf(t.h.mock->last_textured_vertices);

  t.h.mock->Reset();
  t.h.painter.Begin();
  t.h.painter.SetFont(t.font);
  t.h.painter.SetPen(Pen(Color::White(), 1.0F));
  t.h.painter.DrawText(10.0F, 40.0F, "A\nA");
  t.h.painter.End();
  const Bounds two = BoundsOf(t.h.mock->last_textured_vertices);

  EXPECT_GT(two.max_y - two.min_y, single.max_y - single.min_y)
      << "Satir sonu dikeyde yer kaplamadi.";
  EXPECT_LT(two.max_x, single.max_x)
      << "Iki satir hala yan yana ciziliyor.";
}

TEST(PainterText, SecondLineIsOneLineHeightBelowTheFirst) {
  SDLPAINTER_REQUIRE_FONT_OR_SKIP(font_path);
  TextHarness t(font_path);
  ASSERT_TRUE(t.font->IsValid());

  t.h.painter.Begin();
  t.h.painter.SetFont(t.font);
  t.h.painter.SetPen(Pen(Color::White(), 1.0F));
  t.h.painter.DrawText(10.0F, 40.0F, "A");
  t.h.painter.End();
  const Bounds first = BoundsOf(t.h.mock->last_textured_vertices);

  t.h.mock->Reset();
  t.h.painter.Begin();
  t.h.painter.SetFont(t.font);
  t.h.painter.SetPen(Pen(Color::White(), 1.0F));
  t.h.painter.DrawText(10.0F, 40.0F, "\nA");  // yalnizca ikinci satir dolu
  t.h.painter.End();
  const Bounds second = BoundsOf(t.h.mock->last_textured_vertices);

  EXPECT_NEAR(second.min_y - first.min_y,
              static_cast<float>(t.font->LineHeight()), 1.0F);
}

TEST(PainterText, EmptyLinesStillTakeVerticalSpace) {
  SDLPAINTER_REQUIRE_FONT_OR_SKIP(font_path);
  TextHarness t(font_path);
  ASSERT_TRUE(t.font->IsValid());

  t.h.painter.Begin();
  t.h.painter.SetFont(t.font);
  t.h.painter.SetPen(Pen(Color::White(), 1.0F));
  t.h.painter.DrawText(10.0F, 40.0F, "A\n\n\nA");
  t.h.painter.End();

  const Bounds b = BoundsOf(t.h.mock->last_textured_vertices);
  // Ilk ve son satir arasi 3 satir yuksekligi olmali.
  EXPECT_GT(b.max_y - b.min_y, 2.5F * static_cast<float>(t.font->LineHeight()));
}

/// @brief Varsayılan sarmalama YOK: mevcut davranış birebir korunmalı.
TEST(PainterText, WrappingIsOffByDefault) {
  SDLPAINTER_REQUIRE_FONT_OR_SKIP(font_path);
  TextHarness t(font_path);
  ASSERT_TRUE(t.font->IsValid());

  const std::string kLong = "bu metin dar bir kutuya kesinlikle sigmaz";
  const Rect kNarrow{0.0F, 0.0F, 60.0F, 200.0F};

  t.h.painter.Begin();
  t.h.painter.SetFont(t.font);
  t.h.painter.SetPen(Pen(Color::White(), 1.0F));
  t.h.painter.DrawText(kNarrow, kLong, Alignment::kLeft);
  t.h.painter.End();

  const Bounds b = BoundsOf(t.h.mock->last_textured_vertices);
  EXPECT_GT(b.max_x, kNarrow.w)
      << "Varsayilan sarmalama acilmis; eski davranis degisti.";
}

TEST(PainterText, WordWrapKeepsTextInsideTheBox) {
  SDLPAINTER_REQUIRE_FONT_OR_SKIP(font_path);
  TextHarness t(font_path);
  ASSERT_TRUE(t.font->IsValid());

  const std::string kLong = "bu metin dar bir kutuya kesinlikle sigmaz";
  const Rect kBox{0.0F, 0.0F, 120.0F, 300.0F};

  t.h.painter.Begin();
  t.h.painter.SetFont(t.font);
  t.h.painter.SetPen(Pen(Color::White(), 1.0F));
  t.h.painter.DrawText(kBox, kLong, Alignment::kLeft, TextWrap::kWord);
  t.h.painter.End();

  const Bounds b = BoundsOf(t.h.mock->last_textured_vertices);
  // Bir miktar tolerans: son glyph'in bearing'i kutuyu birkac piksel asabilir.
  EXPECT_LE(b.max_x, kBox.w + 4.0F) << "Sarmalama kutuyu tutamadi.";
  EXPECT_GT(b.max_y - b.min_y, static_cast<float>(t.font->LineHeight()))
      << "Metin tek satirda kalmis; sarmalama calismadi.";
}

TEST(PainterText, CountTextLinesMatchesWrappedOutput) {
  SDLPAINTER_REQUIRE_FONT_OR_SKIP(font_path);
  TextHarness t(font_path);
  ASSERT_TRUE(t.font->IsValid());

  t.h.painter.SetFont(t.font);
  EXPECT_EQ(t.h.painter.CountTextLines("tek satir", 1000.0F, TextWrap::kNone),
            1u);
  EXPECT_EQ(t.h.painter.CountTextLines("a\nb\nc", 1000.0F, TextWrap::kNone),
            3u);
  EXPECT_GT(
      t.h.painter.CountTextLines("bu metin dar bir kutuya kesinlikle sigmaz",
                                 120.0F, TextWrap::kWord),
      1u);
}

TEST(PainterText, CountTextLinesIsZeroWithoutFontOrText) {
  Harness h;
  EXPECT_EQ(h.painter.CountTextLines("bir sey", 100.0F, TextWrap::kWord), 0u)
      << "Font yokken satir sayilmamali.";
}

/// @brief Tek bir sözcük bile sığmazsa karakterden bölünmeli — ve bölme
///        UTF-8 kod noktası sınırında olmalı, yoksa bozuk dizi oluşur.
TEST(PainterText, OverlongWordIsSplitAtCodepointBoundaries) {
  SDLPAINTER_REQUIRE_FONT_OR_SKIP(font_path);
  TextHarness t(font_path);
  ASSERT_TRUE(t.font->IsValid());

  // Turkce karakterler cok baytli: bolme yanlis yerden olursa U+FFFD cikar.
  const std::string kWord = "çğıöşüçğıöşüçğıöşü";
  const Rect kNarrow{0.0F, 0.0F, 40.0F, 300.0F};

  t.h.painter.Begin();
  t.h.painter.SetFont(t.font);
  t.h.painter.SetPen(Pen(Color::White(), 1.0F));
  t.h.painter.DrawText(kNarrow, kWord, Alignment::kLeft, TextWrap::kWord);
  t.h.painter.End();

  const Bounds b = BoundsOf(t.h.mock->last_textured_vertices);
  EXPECT_GT(b.max_y - b.min_y, static_cast<float>(t.font->LineHeight()))
      << "Sigmayan sozcuk hic bolunmedi.";
  EXPECT_LE(b.max_x, kNarrow.w + 8.0F);
}

TEST(PainterText, WrappedLinesHonourAlignment) {
  SDLPAINTER_REQUIRE_FONT_OR_SKIP(font_path);
  TextHarness t(font_path);
  ASSERT_TRUE(t.font->IsValid());

  const std::string kText = "kisa\nbiraz daha uzun satir";
  const Rect kBox{0.0F, 0.0F, 300.0F, 200.0F};

  t.h.painter.Begin();
  t.h.painter.SetFont(t.font);
  t.h.painter.SetPen(Pen(Color::White(), 1.0F));
  t.h.painter.DrawText(kBox, kText, Alignment::kLeft);
  t.h.painter.End();
  const Bounds left = BoundsOf(t.h.mock->last_textured_vertices);

  t.h.mock->Reset();
  t.h.painter.Begin();
  t.h.painter.SetFont(t.font);
  t.h.painter.SetPen(Pen(Color::White(), 1.0F));
  t.h.painter.DrawText(kBox, kText, Alignment::kRight);
  t.h.painter.End();
  const Bounds right = BoundsOf(t.h.mock->last_textured_vertices);

  EXPECT_GT(right.min_x, left.min_x) << "Saga hizalama satir basina uygulanmadi.";
  EXPECT_NEAR(right.max_x, kBox.w, 4.0F);
}

// ─── Yay / dilim / kiriş ────────────────────────────────────────────────────

TEST(PainterArc, FillPieUsesBrushAndDrawsFromCentre) {
  Harness h;
  h.painter.Begin();
  h.painter.SetBrush(Brush(Color::Red()));
  h.painter.FillPie(100.0F, 100.0F, 50.0F, 50.0F, 0.0F, 90.0F);
  h.painter.End();

  ASSERT_FALSE(h.mock->last_vertices.empty());
  EXPECT_EQ(h.mock->last_vertices[0].r, 255);
  EXPECT_EQ(h.mock->last_vertices[0].x, 100.0F);
  EXPECT_EQ(h.mock->last_vertices[0].y, 100.0F);
}

TEST(PainterArc, NoBrushMeansNoFillDrawCall) {
  Harness h;
  h.painter.Begin();
  h.painter.SetBrush(Brush(Color::Transparent()));
  h.painter.FillPie(100.0F, 100.0F, 50.0F, 50.0F, 0.0F, 90.0F);
  h.painter.FillChord(100.0F, 100.0F, 50.0F, 50.0F, 0.0F, 90.0F);
  h.painter.End();
  EXPECT_EQ(h.mock->CountCalls("DrawTriangles"), 0);
}

TEST(PainterArc, NoPenMeansNoStrokeDrawCall) {
  Harness h;
  h.painter.Begin();
  h.painter.SetPen(Pen::NoPen());
  h.painter.DrawArc(100.0F, 100.0F, 50.0F, 50.0F, 0.0F, 90.0F);
  h.painter.DrawPie(100.0F, 100.0F, 50.0F, 50.0F, 0.0F, 90.0F);
  h.painter.DrawChord(100.0F, 100.0F, 50.0F, 50.0F, 0.0F, 90.0F);
  h.painter.End();
  EXPECT_EQ(h.mock->CountCalls("DrawTriangles"), 0);
}

/// @brief Yay AÇIK bir yoldur: uçları birleştirilmez, dolayısıyla dilim ve
///        kirişten daha az geometri üretir.
TEST(PainterArc, ArcProducesLessGeometryThanPieAndChord) {
  Harness h;
  h.painter.Begin();
  h.painter.SetPen(Pen(Color::White(), 3.0F));
  h.painter.DrawArc(100.0F, 100.0F, 50.0F, 50.0F, 0.0F, 90.0F);
  h.painter.End();
  const std::size_t arc_size = h.mock->last_vertices.size();

  h.mock->Reset();
  h.painter.Begin();
  h.painter.SetPen(Pen(Color::White(), 3.0F));
  h.painter.DrawPie(100.0F, 100.0F, 50.0F, 50.0F, 0.0F, 90.0F);
  h.painter.End();
  const std::size_t pie_size = h.mock->last_vertices.size();

  ASSERT_GT(arc_size, 0u);
  EXPECT_LT(arc_size, pie_size)
      << "Yay, dilim kadar geometri uretti — uclari kapatiliyor olabilir.";
}

TEST(PainterArc, DegenerateSweepProducesNothing) {
  Harness h;
  h.painter.Begin();
  h.painter.SetPen(Pen(Color::White(), 3.0F));
  h.painter.SetBrush(Brush(Color::Red()));
  h.painter.DrawArc(100.0F, 100.0F, 50.0F, 50.0F, 0.0F, 0.0F);
  h.painter.FillPie(100.0F, 100.0F, 50.0F, 50.0F, 0.0F, 0.0F);
  h.painter.End();
  EXPECT_EQ(h.mock->CountCalls("DrawTriangles"), 0);
}

TEST(PainterArc, TransformIsAppliedToArcVertices) {
  Harness h;
  h.painter.Begin();
  h.painter.SetBrush(Brush(Color::Red()));
  h.painter.Translate(200.0F, 300.0F);
  h.painter.FillPie(0.0F, 0.0F, 50.0F, 50.0F, 0.0F, 360.0F);
  h.painter.End();

  ASSERT_FALSE(h.mock->last_vertices.empty());
  const Bounds b = BoundsOf(h.mock->last_vertices);
  EXPECT_NEAR(b.min_x, 150.0F, 1.0F);
  EXPECT_NEAR(b.max_x, 250.0F, 1.0F);
  EXPECT_NEAR(b.min_y, 250.0F, 1.0F);
  EXPECT_NEAR(b.max_y, 350.0F, 1.0F);
}

// ─── Kesikli kalem ──────────────────────────────────────────────────────────

TEST(PainterDash, DashedLineProducesLessGeometryThanSolid) {
  Harness h;
  h.painter.Begin();
  h.painter.SetPen(Pen(Color::White(), 4.0F));
  h.painter.DrawLine(0.0F, 0.0F, 200.0F, 0.0F);
  h.painter.End();
  const std::size_t solid = h.mock->last_vertices.size();

  h.mock->Reset();
  Pen dashed(Color::White(), 4.0F);
  dashed.SetDashPattern({10.0F, 10.0F});
  h.painter.Begin();
  h.painter.SetPen(dashed);
  h.painter.DrawLine(0.0F, 0.0F, 200.0F, 0.0F);
  h.painter.End();
  const std::size_t dashed_size = h.mock->last_vertices.size();

  ASSERT_GT(solid, 0u);
  ASSERT_GT(dashed_size, 0u);
  EXPECT_NE(dashed_size, solid) << "Kesik desen hic uygulanmadi.";
}

TEST(PainterDash, DashAppliesToClosedShapesToo) {
  Harness h;
  h.painter.Begin();
  h.painter.SetPen(Pen(Color::White(), 4.0F));
  h.painter.DrawRect(0.0F, 0.0F, 100.0F, 100.0F);
  h.painter.End();
  const std::size_t solid = h.mock->last_vertices.size();

  h.mock->Reset();
  Pen dashed(Color::White(), 4.0F);
  dashed.SetDashPattern({8.0F, 8.0F});
  h.painter.Begin();
  h.painter.SetPen(dashed);
  h.painter.DrawRect(0.0F, 0.0F, 100.0F, 100.0F);
  h.painter.End();

  EXPECT_NE(h.mock->last_vertices.size(), solid)
      << "DrawRect kalemin kesik desenini yok sayiyor.";
}

// ─── Görüntü çizimi ─────────────────────────────────────────────────────────

TEST(PainterImage, DrawImageUploadsOnceAndDraws) {
  Harness h;
  const auto image = MakeTinyImage();
  ASSERT_TRUE(image.IsValid());

  h.painter.Begin();
  h.painter.DrawImage(image, 10.0F, 20.0F);
  h.painter.End();

  EXPECT_EQ(h.mock->create_texture_count, 1);
  EXPECT_EQ(h.mock->CountCalls("DrawTextured"), 1);
  EXPECT_EQ(h.mock->last_textured_vertices.size(), 6u);
}

TEST(PainterImage, SecondDrawReusesUploadedTexture) {
  Harness h;
  const auto image = MakeTinyImage();
  ASSERT_TRUE(image.IsValid());

  h.painter.Begin();
  h.painter.DrawImage(image, 0.0F, 0.0F);
  h.painter.DrawImage(image, 20.0F, 0.0F);
  h.painter.End();

  EXPECT_EQ(h.mock->create_texture_count, 1)
      << "Ayni goruntu ikinci kez yuklendi.";
  // Ayni texture -> tek batch.
  EXPECT_EQ(h.mock->CountCalls("DrawTextured"), 1);
  EXPECT_EQ(h.mock->last_textured_vertices.size(), 12u);
}

TEST(PainterImage, DestRectOverloadScalesToTarget) {
  Harness h;
  const auto image = MakeTinyImage();
  ASSERT_TRUE(image.IsValid());

  h.painter.Begin();
  h.painter.DrawImage(image, Rect{5.0F, 6.0F, 40.0F, 30.0F});
  h.painter.End();

  ASSERT_EQ(h.mock->last_textured_vertices.size(), 6u);
  float min_x = 1e9F;
  float max_x = -1e9F;
  for (const auto& v : h.mock->last_textured_vertices) {
    min_x = std::fmin(min_x, v.x);
    max_x = std::fmax(max_x, v.x);
  }
  EXPECT_FLOAT_EQ(min_x, 5.0F);
  EXPECT_FLOAT_EQ(max_x, 45.0F);
}

TEST(PainterImage, SrcRectOverloadMapsSubRegionUVs) {
  Harness h;
  const auto image = MakeTinyImage();  // 4x4
  ASSERT_TRUE(image.IsValid());

  h.painter.Begin();
  // Sol ust 2x2 -> UV [0, 0.5]
  h.painter.DrawImage(image, Rect{0.0F, 0.0F, 2.0F, 2.0F},
                      Rect{0.0F, 0.0F, 10.0F, 10.0F});
  h.painter.End();

  ASSERT_FALSE(h.mock->last_textured_vertices.empty());
  float max_u = 0.0F;
  for (const auto& v : h.mock->last_textured_vertices) {
    max_u = std::fmax(max_u, v.u);
  }
  EXPECT_FLOAT_EQ(max_u, 0.5F);
}

// ─── Tint ve aynalama ───────────────────────────────────────────────────────

TEST(PainterImage, DefaultTintIsWhiteSoTextureIsUnchanged) {
  Harness h;
  const auto image = MakeTinyImage();
  ASSERT_TRUE(image.IsValid());

  h.painter.Begin();
  h.painter.DrawImage(image, 0.0F, 0.0F);
  h.painter.End();

  ASSERT_FALSE(h.mock->last_textured_vertices.empty());
  for (const auto& v : h.mock->last_textured_vertices) {
    EXPECT_EQ(v.r, 255);
    EXPECT_EQ(v.g, 255);
    EXPECT_EQ(v.b, 255);
    EXPECT_EQ(v.a, 255);
  }
}

TEST(PainterImage, TintReachesEveryVertex) {
  Harness h;
  const auto image = MakeTinyImage();
  ASSERT_TRUE(image.IsValid());

  const Color kTint{255, 0, 0, 128};
  h.painter.Begin();
  h.painter.DrawImage(image, 0.0F, 0.0F, kTint);
  h.painter.End();

  ASSERT_EQ(h.mock->last_textured_vertices.size(), 6u);
  for (const auto& v : h.mock->last_textured_vertices) {
    EXPECT_EQ(v.r, kTint.r);
    EXPECT_EQ(v.g, kTint.g);
    EXPECT_EQ(v.b, kTint.b);
    EXPECT_EQ(v.a, kTint.a) << "Tint alfasi vertex'e tasinmadi.";
  }
}

/// @brief Tint vertex'te tasinir; farkli renkler ayni batch'te kalmali.
///
/// Bu, ozelligin varlik sebebi: renk basina flush gerekseydi tint'i disari
/// acmanin bedeli, kazancindan buyuk olurdu.
TEST(PainterImage, DifferentTintsDoNotBreakBatching) {
  Harness h;
  const auto image = MakeTinyImage();
  ASSERT_TRUE(image.IsValid());

  h.painter.Begin();
  h.painter.DrawImage(image, 0.0F, 0.0F, Color::Red());
  h.painter.DrawImage(image, 20.0F, 0.0F, Color::Blue());
  h.painter.End();

  EXPECT_EQ(h.mock->CountCalls("DrawTextured"), 1)
      << "Farkli tint batch'i kirdi.";
  EXPECT_EQ(h.mock->last_textured_vertices.size(), 12u);
}

/// @brief Aynalama UV'leri takas eder; hedef dikdortgen yerinde kalir.
TEST(PainterImage, HorizontalFlipSwapsUOnlyAndKeepsGeometry) {
  Harness h;
  const auto image = MakeTinyImage();
  ASSERT_TRUE(image.IsValid());

  const Rect kDest{0.0F, 0.0F, 10.0F, 10.0F};

  h.painter.Begin();
  h.painter.DrawImage(image, kDest);
  h.painter.End();
  const auto plain = h.mock->last_textured_vertices;

  h.mock->Reset();
  h.painter.Begin();
  h.painter.DrawImage(image, kDest, Color::White(),
                      sdl_painter::ImageFlip::kHorizontal);
  h.painter.End();
  const auto flipped = h.mock->last_textured_vertices;

  ASSERT_EQ(plain.size(), flipped.size());
  for (std::size_t i = 0; i < plain.size(); ++i) {
    EXPECT_FLOAT_EQ(plain[i].x, flipped[i].x) << "Geometri i=" << i;
    EXPECT_FLOAT_EQ(plain[i].y, flipped[i].y) << "Geometri i=" << i;
    EXPECT_FLOAT_EQ(plain[i].v, flipped[i].v) << "Dikey UV degisti, i=" << i;
    EXPECT_FLOAT_EQ(flipped[i].u, 1.0F - plain[i].u) << "Yatay UV, i=" << i;
  }
}

TEST(PainterImage, VerticalFlipSwapsVOnly) {
  Harness h;
  const auto image = MakeTinyImage();
  ASSERT_TRUE(image.IsValid());

  const Rect kDest{0.0F, 0.0F, 10.0F, 10.0F};

  h.painter.Begin();
  h.painter.DrawImage(image, kDest);
  h.painter.End();
  const auto plain = h.mock->last_textured_vertices;

  h.mock->Reset();
  h.painter.Begin();
  h.painter.DrawImage(image, kDest, Color::White(),
                      sdl_painter::ImageFlip::kVertical);
  h.painter.End();
  const auto flipped = h.mock->last_textured_vertices;

  ASSERT_EQ(plain.size(), flipped.size());
  for (std::size_t i = 0; i < plain.size(); ++i) {
    EXPECT_FLOAT_EQ(plain[i].u, flipped[i].u) << "Yatay UV degisti, i=" << i;
    EXPECT_FLOAT_EQ(flipped[i].v, 1.0F - plain[i].v) << "Dikey UV, i=" << i;
  }
}

TEST(PainterImage, BothFlipSwapsUAndV) {
  Harness h;
  const auto image = MakeTinyImage();
  ASSERT_TRUE(image.IsValid());

  const Rect kDest{0.0F, 0.0F, 10.0F, 10.0F};

  h.painter.Begin();
  h.painter.DrawImage(image, kDest);
  h.painter.End();
  const auto plain = h.mock->last_textured_vertices;

  h.mock->Reset();
  h.painter.Begin();
  h.painter.DrawImage(image, kDest, Color::White(),
                      sdl_painter::ImageFlip::kBoth);
  h.painter.End();
  const auto flipped = h.mock->last_textured_vertices;

  ASSERT_EQ(plain.size(), flipped.size());
  for (std::size_t i = 0; i < plain.size(); ++i) {
    EXPECT_FLOAT_EQ(flipped[i].u, 1.0F - plain[i].u) << "i=" << i;
    EXPECT_FLOAT_EQ(flipped[i].v, 1.0F - plain[i].v) << "i=" << i;
  }
}

/// @brief Aynalama, src_rect ile birlikte yalnizca o alt bolgeyi cevirmeli.
TEST(PainterImage, FlipStaysWithinSourceSubRegion) {
  Harness h;
  const auto image = MakeTinyImage();  // 4x4
  ASSERT_TRUE(image.IsValid());

  h.painter.Begin();
  // Sol ust 2x2 -> UV [0, 0.5]; aynalama bu araligin disina tasmamali.
  h.painter.DrawImage(image, Rect{0.0F, 0.0F, 2.0F, 2.0F},
                      Rect{0.0F, 0.0F, 10.0F, 10.0F},
                      Color::White(),
                      sdl_painter::ImageFlip::kHorizontal);
  h.painter.End();

  ASSERT_FALSE(h.mock->last_textured_vertices.empty());
  for (const auto& v : h.mock->last_textured_vertices) {
    EXPECT_GE(v.u, 0.0F);
    EXPECT_LE(v.u, 0.5F) << "Aynalama kaynak alt bolgesinin disina tasti.";
  }
}

TEST(PainterImage, InvalidImageProducesNoDrawCall) {
  Harness h;
  const sdl_painter::Image empty;  // yuklenmemis
  h.painter.Begin();
  h.painter.DrawImage(empty, 0.0F, 0.0F);
  h.painter.End();

  EXPECT_EQ(h.mock->create_texture_count, 0);
  EXPECT_EQ(h.mock->CountCalls("DrawTextured"), 0);
}

TEST(PainterImage, TransformIsAppliedToImageDrawCall) {
  Harness h;
  const auto image = MakeTinyImage();
  ASSERT_TRUE(image.IsValid());

  h.painter.Begin();
  h.painter.Translate(64.0F, 32.0F);
  h.painter.DrawImage(image, 0.0F, 0.0F);
  h.painter.End();

  ASSERT_EQ(h.mock->last_textured_vertices.size(), 6u);
  const Bounds b = BoundsOf(h.mock->last_textured_vertices);
  EXPECT_FLOAT_EQ(b.min_x, 64.0F);
  EXPECT_FLOAT_EQ(b.min_y, 32.0F);
  EXPECT_FLOAT_EQ(b.max_x, 68.0F);  // 4x4 goruntu
  EXPECT_FLOAT_EQ(b.max_y, 36.0F);
}

// ─── Taşıma (move) semantiği ────────────────────────────────────────────────

TEST(PainterLifecycle, MoveConstructorTransfersRenderer) {
  auto mock = std::make_unique<MockRenderer>();
  MockRenderer* raw = mock.get();
  Painter src(std::move(mock), kViewportW, kViewportH);
  ASSERT_TRUE(src.IsValid());

  Painter dst(std::move(src));
  EXPECT_TRUE(dst.IsValid());
  EXPECT_FALSE(src.IsValid());  // NOLINT(bugprone-use-after-move)

  dst.Begin();
  dst.SetBrush(Brush(Color::Red()));
  dst.FillRect(0.0F, 0.0F, 10.0F, 10.0F);
  dst.End();
  EXPECT_EQ(raw->CountCalls("DrawTriangles"), 1);
}

TEST(PainterLifecycle, MoveAssignmentTransfersRenderer) {
  auto mock_a = std::make_unique<MockRenderer>();
  auto mock_b = std::make_unique<MockRenderer>();
  MockRenderer* raw_b = mock_b.get();

  Painter dst(std::move(mock_a), kViewportW, kViewportH);
  Painter src(std::move(mock_b), kViewportW, kViewportH);
  dst = std::move(src);

  ASSERT_TRUE(dst.IsValid());
  dst.Begin();
  dst.SetBrush(Brush(Color::Red()));
  dst.FillRect(0.0F, 0.0F, 10.0F, 10.0F);
  dst.End();
  EXPECT_EQ(raw_b->CountCalls("DrawTriangles"), 1);
}

// ─── Çizim yüzeyi boyutu ────────────────────────────────────────────────────

TEST(PainterViewport, SetDrawableSizeForwardsViewport) {
  Harness h;
  h.mock->viewport_calls.clear();
  h.painter.SetDrawableSize(320, 240);

  ASSERT_FALSE(h.mock->viewport_calls.empty());
  const auto& v = h.mock->viewport_calls.back();
  EXPECT_EQ(v.w, 320);
  EXPECT_EQ(v.h, 240);
}

TEST(PainterViewport, SetDrawableSizeIsIdempotent) {
  Harness h;
  h.painter.SetDrawableSize(320, 240);
  h.mock->viewport_calls.clear();
  h.painter.SetDrawableSize(320, 240);  // ayni boyut -> yeniden gonderme
  EXPECT_TRUE(h.mock->viewport_calls.empty());
}

/// Sifir boyutta projeksiyon NaN'a dusmemeli (simge durumu senaryosu).
TEST(PainterViewport, ZeroSizeDoesNotProduceNaNProjection) {
  Harness h;
  h.painter.SetDrawableSize(0, 0);
  for (float f : h.mock->last_projection) {
    EXPECT_FALSE(std::isnan(f));
    EXPECT_FALSE(std::isinf(f));
  }
}
