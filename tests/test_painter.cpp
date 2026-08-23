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
