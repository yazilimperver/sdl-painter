// Kenar yumuşatma (Painter::SetAntialiasing, Tessellator::AntialiasTriangles).
//
// Üç katman:
//  - Tessellator: şerit geometrisi saf fonksiyon olarak sınanır (köşe sayısı,
//    içeri çekme, kapsama alanı, kaynaştırma, sınır durumları ve parçaların
//    birleştiği yerde yarık olmaması).
//  - Painter (mock renderer): durum yönetimi, dönüşüm ölçeğinde 1 piksellik
//    şerit, ince kalem ve gradient.
//  - GPU: gerçek rasterleştirmede tam sayı koordinatlı dikdörtgen keskin
//    kalıyor, eğri kenarda ara tonlar oluşuyor.

#include "sdl_painter/brush.h"
#include "sdl_painter/color.h"
#include "sdl_painter/geometry.h"
#include "sdl_painter/painter.h"
#include "sdl_painter/pen.h"
#include "sdl_painter/render_target.h"
#include "sdl_painter/vertex.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <functional>
#include <gtest/gtest.h>
#include <limits>
#include <memory>
#include <vector>

#include "../src/tessellator.h"
#include "mock_renderer.h"
#include "pixel_test_support.h"
#include "test_support.h"

using sdl_painter::Brush;
using sdl_painter::Color;
using sdl_painter::LineCap;
using sdl_painter::LineJoin;
using sdl_painter::MockRenderer;
using sdl_painter::Painter;
using sdl_painter::Pen;
using sdl_painter::Point;
using sdl_painter::RendererBackend;
using sdl_painter::RenderTarget;
using sdl_painter::Tessellator;
using sdl_painter::Vertex;
using sdl_painter::testing::AvailableBackends;
using sdl_painter::testing::PixelAt;

namespace {

constexpr std::uint8_t kFull = 255;

/// @brief Kapsamayla ağırlıklı alan: alfa üçgende doğrusal olduğu için
///        üçgen alanı * köşe alfalarının ortalaması tam integraldir.
double CoverageArea(const std::vector<Vertex>& tris) {
  double sum = 0.0;
  for (std::size_t i = 0; i + 2 < tris.size(); i += 3) {
    const Vertex& a = tris[i];
    const Vertex& b = tris[i + 1];
    const Vertex& c = tris[i + 2];
    const double area =
        std::fabs(((b.x - a.x) * (c.y - a.y)) - ((c.x - a.x) * (b.y - a.y))) /
        2.0;
    sum += area * (a.a + b.a + c.a) / (3.0 * 255.0);
  }
  return sum;
}

/// @brief Noktanın [p, q] doğru parçasına uzaklığı.
float DistanceToSegment(const Point& v, const Point& p, const Point& q) {
  const float dx = q.x - p.x;
  const float dy = q.y - p.y;
  const float len2 = (dx * dx) + (dy * dy);
  float t =
      len2 > 0.0F ? (((v.x - p.x) * dx) + ((v.y - p.y) * dy)) / len2 : 0.0F;
  t = std::clamp(t, 0.0F, 1.0F);
  const float ex = v.x - (p.x + (t * dx));
  const float ey = v.y - (p.y + (t * dy));
  return std::sqrt((ex * ex) + (ey * ey));
}

/// @brief Kapalı yolun kenarlarına en yakın uzaklık.
float DistanceToClosedPath(const Point& v, const std::vector<Point>& path) {
  float best = std::numeric_limits<float>::max();
  for (std::size_t i = 0; i < path.size(); ++i) {
    best = std::min(best,
                    DistanceToSegment(v, path[i], path[(i + 1) % path.size()]));
  }
  return best;
}

std::size_t CountAlpha(const std::vector<Vertex>& verts, std::uint8_t alpha) {
  return static_cast<std::size_t>(
      std::count_if(verts.begin(), verts.end(),
                    [alpha](const Vertex& v) { return v.a == alpha; }));
}

}  // namespace

// --- Tessellator::AntialiasTriangles ---

TEST(AntialiasTriangles, RectGetsCoreAndFourFringeStrips) {
  const auto rect = Tessellator::TessellateFilledRect(0.0F, 0.0F, 10.0F, 10.0F);
  const auto out = Tessellator::AntialiasTriangles(rect, 1.0F);

  // Köşegen iç kenardır: 2 çekirdek üçgen + 4 sınır kenarı * 2 üçgen.
  ASSERT_EQ(out.size(), 6U + (4U * 6U));
  EXPECT_EQ(CountAlpha(out, 0) + CountAlpha(out, kFull), out.size());
  EXPECT_EQ(CountAlpha(out, 0), 4U * 3U);
}

TEST(AntialiasTriangles, FringeIsCentredOnTheOriginalEdge) {
  const auto rect = Tessellator::TessellateFilledRect(0.0F, 0.0F, 10.0F, 10.0F);
  const auto out = Tessellator::AntialiasTriangles(rect, 1.0F);
  float inner_min = 1e9F;
  float inner_max = -1e9F;
  float outer_min = 1e9F;
  float outer_max = -1e9F;
  for (const Vertex& v : out) {
    if (v.a == kFull) {
      inner_min = std::min({inner_min, v.x, v.y});
      inner_max = std::max({inner_max, v.x, v.y});
    } else {
      outer_min = std::min({outer_min, v.x, v.y});
      outer_max = std::max({outer_max, v.x, v.y});
    }
  }
  EXPECT_FLOAT_EQ(inner_min, 0.5F);
  EXPECT_FLOAT_EQ(inner_max, 9.5F);
  EXPECT_FLOAT_EQ(outer_min, -0.5F);
  EXPECT_FLOAT_EQ(outer_max, 10.5F);
}

TEST(AntialiasTriangles, CoverageAreaMatchesTheShape) {
  // Kenarlarda kapsama tam korunur; köşelerde miter yüzünden ufak bir fazlalık
  // var (her köşe ~1/12 piksel kare).
  const auto rect = Tessellator::TessellateFilledRect(0.0F, 0.0F, 10.0F, 10.0F);
  EXPECT_NEAR(CoverageArea(Tessellator::AntialiasTriangles(rect, 1.0F)), 100.0,
              0.5);

  const auto circle = Tessellator::TessellateFilledCircle(0.0F, 0.0F, 40.0F);
  const double kPolygonArea = CoverageArea(circle);
  EXPECT_NEAR(CoverageArea(Tessellator::AntialiasTriangles(circle, 1.0F)),
              kPolygonArea, kPolygonArea * 0.005);
}

TEST(AntialiasTriangles, FanSpokesAreInteriorEdges) {
  const auto circle = Tessellator::TessellateFilledCircle(0.0F, 0.0F, 20.0F);
  const std::size_t kSegments = circle.size() / 3;
  const auto out = Tessellator::AntialiasTriangles(circle, 1.0F);
  // Yalnızca çevre kenarları şerit alır; merkezden çıkan kollar almaz.
  EXPECT_EQ(out.size(), circle.size() + (kSegments * 6U));
}

TEST(AntialiasTriangles, NearlyCoincidentVerticesAreWelded) {
  auto rect = Tessellator::TessellateFilledRect(0.0F, 0.0F, 10.0F, 10.0F);
  // İkinci üçgendeki ortak köşeyi son bitte kaydır: hâlâ aynı köşe sayılmalı.
  rect[3].x += 1e-6F;
  rect[4].y -= 1e-6F;
  EXPECT_EQ(Tessellator::AntialiasTriangles(rect, 1.0F).size(), 30U);
}

TEST(AntialiasTriangles, DegenerateTrianglesAreDropped) {
  auto rect = Tessellator::TessellateFilledRect(0.0F, 0.0F, 10.0F, 10.0F);
  rect.emplace_back(0.0F, 0.0F);
  rect.emplace_back(5.0F, 0.0F);
  rect.emplace_back(10.0F, 0.0F);  // doğrusal: alanı sıfır
  EXPECT_EQ(Tessellator::AntialiasTriangles(rect, 1.0F).size(), 30U);
}

TEST(AntialiasTriangles, TinyShapeIsNotInset) {
  const auto rect = Tessellator::TessellateFilledRect(2.0F, 2.0F, 0.5F, 0.5F);
  const auto out = Tessellator::AntialiasTriangles(rect, 1.0F);
  for (const Vertex& v : out) {
    if (v.a == kFull) {
      EXPECT_GE(v.x, 2.0F - 1e-6F);
      EXPECT_LE(v.x, 2.5F + 1e-6F);
    }
  }
  EXPECT_GT(CoverageArea(out), 0.25);
}

TEST(AntialiasTriangles, InvalidInputIsReturnedUnchanged) {
  const auto rect = Tessellator::TessellateFilledRect(0.0F, 0.0F, 4.0F, 4.0F);
  EXPECT_EQ(Tessellator::AntialiasTriangles(rect, 0.0F).size(), rect.size());
  EXPECT_EQ(Tessellator::AntialiasTriangles(rect, -1.0F).size(), rect.size());
  auto bad = rect;
  bad[1].x = std::numeric_limits<float>::quiet_NaN();
  EXPECT_EQ(Tessellator::AntialiasTriangles(bad, 1.0F).size(), rect.size());
  EXPECT_TRUE(Tessellator::AntialiasTriangles({}, 1.0F).empty());
}

// Yuvarlak uç eskiden uç noktasından fanlanıyordu: gövdenin uç kenarı iki
// parçaya bölünüyor (T-birleşim), şerit onu sınır sanıp gövde ile uç arasına
// yarık açıyordu. Dış şeridin hiçbir noktası çizginin içinde kalmamalı.
TEST(AntialiasTriangles, RoundCapJoinsTheBodyWithoutAGap) {
  constexpr float kWidth = 6.0F;
  const Point kStart{10.0F, 10.0F};
  const Point kEnd{40.0F, 10.0F};
  const auto line = Tessellator::TessellateThickLine(
      kStart.x, kStart.y, kEnd.x, kEnd.y, kWidth, LineCap::kRound);
  const auto out = Tessellator::AntialiasTriangles(line, 1.0F);
  for (const Vertex& v : out) {
    if (v.a == 0) {
      EXPECT_GE(DistanceToSegment({v.x, v.y}, kStart, kEnd),
                (kWidth * 0.5F) - 1e-3F)
          << "dis serit cizginin icinde: (" << v.x << ", " << v.y << ")";
    }
  }
}

TEST(AntialiasTriangles, TrimmedJoinsKeepTheFringeOutside) {
  constexpr float kWidth = 6.0F;
  const std::vector<Point> kPath = {
      {10.0F, 10.0F}, {60.0F, 10.0F}, {60.0F, 50.0F}, {10.0F, 50.0F}};
  for (const LineJoin join :
       {LineJoin::kRound, LineJoin::kMiter, LineJoin::kBevel}) {
    const auto stroke =
        Tessellator::TessellateStrokedPolygon(kPath, kWidth, join);
    const auto out = Tessellator::AntialiasTriangles(stroke, 1.0F);
    for (const Vertex& v : out) {
      if (v.a == 0) {
        EXPECT_GE(DistanceToClosedPath({v.x, v.y}, kPath),
                  (kWidth * 0.5F) - 1e-3F)
            << "birlesim " << static_cast<int>(join) << ": (" << v.x << ", "
            << v.y << ")";
      }
    }
  }
}

// --- Painter ---

namespace {

constexpr int32_t kViewportW = 200;
constexpr int32_t kViewportH = 150;

struct Harness {
  MockRenderer* mock{nullptr};
  Painter painter;

  Harness() : Harness(std::make_unique<MockRenderer>()) {}
  explicit Harness(std::unique_ptr<MockRenderer> m)
      : mock(m.get()), painter(std::move(m), kViewportW, kViewportH) {}
};

}  // namespace

TEST(PainterAntialiasing, OffByDefaultAndFollowsSaveRestore) {
  Harness h;
  EXPECT_FALSE(h.painter.IsAntialiasing());
  h.painter.Save();
  h.painter.SetAntialiasing(true);
  EXPECT_TRUE(h.painter.IsAntialiasing());
  h.painter.Restore();
  EXPECT_FALSE(h.painter.IsAntialiasing());
}

TEST(PainterAntialiasing, FillWithoutItKeepsUniformAlpha) {
  Harness h;
  h.painter.Begin();
  h.painter.SetBrush(Brush(Color{10, 20, 30, 200}));
  h.painter.FillRect(10.0F, 10.0F, 20.0F, 20.0F);
  h.painter.End();
  ASSERT_FALSE(h.mock->last_vertices.empty());
  EXPECT_EQ(CountAlpha(h.mock->last_vertices, 200),
            h.mock->last_vertices.size());
}

TEST(PainterAntialiasing, FillGetsTransparentFringeWithBrushColor) {
  Harness h;
  h.painter.Begin();
  h.painter.SetAntialiasing(true);
  h.painter.SetBrush(Brush(Color{10, 20, 30, 200}));
  h.painter.FillRect(10.0F, 10.0F, 20.0F, 20.0F);
  h.painter.End();
  const auto& verts = h.mock->last_vertices;
  ASSERT_FALSE(verts.empty());
  EXPECT_GT(CountAlpha(verts, 0), 0U);
  EXPECT_EQ(CountAlpha(verts, 0) + CountAlpha(verts, 200), verts.size());
  for (const Vertex& v : verts) {
    EXPECT_EQ(v.r, 10);
    EXPECT_EQ(v.g, 20);
    EXPECT_EQ(v.b, 30);
  }
}

TEST(PainterAntialiasing, FringeStaysOnePixelUnderScale) {
  Harness h;
  h.painter.Begin();
  h.painter.SetAntialiasing(true);
  h.painter.SetBrush(Brush(Color::Red()));
  h.painter.Scale(2.0F, 2.0F);
  h.painter.FillRect(10.0F, 10.0F, 20.0F, 20.0F);
  h.painter.End();
  float min_x = 1e9F;
  float max_x = -1e9F;
  for (const Vertex& v : h.mock->last_vertices) {
    min_x = std::min(min_x, v.x);
    max_x = std::max(max_x, v.x);
  }
  // Ekranda 20..60 piksel; şerit kenarın iki yanına yarım ekran pikseli.
  EXPECT_FLOAT_EQ(min_x, 19.5F);
  EXPECT_FLOAT_EQ(max_x, 60.5F);
}

TEST(PainterAntialiasing, ThinPenIsWidenedToOnePixelAndFaded) {
  Harness h;
  h.painter.Begin();
  h.painter.SetAntialiasing(true);
  Pen pen(Color{255, 255, 255, 255}, 0.25F);
  pen.SetCapStyle(LineCap::kButt);
  h.painter.SetPen(pen);
  h.painter.DrawLine(10.0F, 20.0F, 50.0F, 20.0F);
  h.painter.End();
  const auto& verts = h.mock->last_vertices;
  ASSERT_FALSE(verts.empty());
  std::uint8_t max_alpha = 0;
  float max_y = -1e9F;
  for (const Vertex& v : verts) {
    max_alpha = std::max(max_alpha, v.a);
    max_y = std::max(max_y, v.y);
  }
  // Kapsama korunur: 0.25 piksellik çizgi, 1 piksel genişlikte %25 alfa.
  EXPECT_EQ(max_alpha, 64);
  EXPECT_FLOAT_EQ(max_y, 21.0F);
}

TEST(PainterAntialiasing, GradientFillKeepsTheFringe) {
  Harness h;
  h.painter.Begin();
  h.painter.SetAntialiasing(true);
  h.painter.SetBrush(Brush::LinearGradient(
      Point{0.0F, 0.0F}, Point{100.0F, 0.0F}, Color::Red(), Color::Blue()));
  h.painter.FillRect(10.0F, 10.0F, 60.0F, 20.0F);
  h.painter.End();
  const auto& verts = h.mock->last_vertices;
  ASSERT_FALSE(verts.empty());
  EXPECT_GT(CountAlpha(verts, 0), 0U);
  const auto [lo, hi] = std::minmax_element(
      verts.begin(), verts.end(),
      [](const Vertex& a, const Vertex& b) { return a.r < b.r; });
  EXPECT_LT(lo->r, hi->r) << "gradient renkleri kayboldu";
}

TEST(PainterAntialiasing, CurveSegmentsFollowToleranceOnlyWhenEnabled) {
  // Dolu dairenin fanı n üçgen; kenar yumuşatma her çevre kenarına 2 şerit
  // üçgeni ekler (bkz. FanSpokesAreInteriorEdges): 9n vertex.
  auto fill_circle = [](bool antialias, float scale) {
    Harness h;
    h.painter.Begin();
    h.painter.SetAntialiasing(antialias);
    h.painter.SetBrush(Brush(Color::Red()));
    h.painter.Scale(scale, scale);
    h.painter.FillCircle(40.0F, 40.0F, 30.0F);
    h.painter.End();
    return h.mock->last_vertices.size();
  };
  // Kapalıyken klasik kural, ölçekten bağımsız: max(16, 30 * 0.5) = 16.
  EXPECT_EQ(fill_circle(false, 1.0F), 16U * 3U);
  EXPECT_EQ(fill_circle(false, 2.0F), 16U * 3U);
  // Açıkken ekrandaki yarıçapa göre: 30 piksel -> 25, 60 piksel -> 35.
  EXPECT_EQ(fill_circle(true, 1.0F), 25U * 9U);
  EXPECT_EQ(fill_circle(true, 2.0F), 35U * 9U);
}

// --- GPU ---

namespace {

constexpr int32_t kTargetW = 64;
constexpr int32_t kTargetH = 64;

class AntialiasingBackend : public ::testing::TestWithParam<RendererBackend> {};

/// @brief Siyah zemine beyaz şekli kenar yumuşatmayla çiz, pikselleri oku.
bool RenderWhite(sdl_painter::testing::Backend& be,
                 const std::function<void(Painter&)>& draw,
                 std::vector<uint8_t>& pixels) {
  const RenderTarget target =
      be.painter->CreateRenderTarget(kTargetW, kTargetH);
  if (!target.IsValid()) {
    return false;
  }
  be.painter->Begin();
  if (!be.painter->SetRenderTarget(target)) {
    return false;
  }
  be.painter->Clear(Color{0, 0, 0, 255});
  be.painter->SetPen(Pen::NoPen());
  be.painter->SetBrush(Brush(Color{255, 255, 255, 255}));
  be.painter->SetAntialiasing(true);
  draw(*be.painter);
  be.painter->ResetRenderTarget();
  be.painter->End();
  return be.painter->ReadRenderTarget(target, pixels);
}

int32_t CountPartialPixels(const std::vector<uint8_t>& pixels) {
  int32_t partial = 0;
  for (int32_t y = 0; y < kTargetH; ++y) {
    for (int32_t x = 0; x < kTargetW; ++x) {
      const std::uint8_t r = PixelAt(pixels, kTargetW, x, y).r;
      partial += (r > 8 && r < 247) ? 1 : 0;
    }
  }
  return partial;
}

}  // namespace

TEST_P(AntialiasingBackend, IntegerAlignedRectStaysCrisp) {
  REQUIRE_BACKEND(be, GetParam());
  std::vector<uint8_t> pixels;
  ASSERT_TRUE(RenderWhite(
      be, [](Painter& p) { p.FillRect(16.0F, 16.0F, 32.0F, 32.0F); }, pixels));
  // Şerit piksel sınırına oturur: kenar pikselleri tam ya da boş.
  EXPECT_EQ(CountPartialPixels(pixels), 0);
  EXPECT_EQ(PixelAt(pixels, kTargetW, 16, 16).r, 255);
  EXPECT_EQ(PixelAt(pixels, kTargetW, 47, 47).r, 255);
  EXPECT_EQ(PixelAt(pixels, kTargetW, 15, 16).r, 0);
  EXPECT_EQ(PixelAt(pixels, kTargetW, 48, 47).r, 0);
}

TEST_P(AntialiasingBackend, CurvedEdgeGetsIntermediateTones) {
  REQUIRE_BACKEND(be, GetParam());
  std::vector<uint8_t> pixels;
  ASSERT_TRUE(RenderWhite(
      be, [](Painter& p) { p.FillCircle(32.0F, 32.0F, 20.0F); }, pixels));
  // Çevre ~125 piksel; neredeyse her kenar pikseli ara ton almalı.
  EXPECT_GT(CountPartialPixels(pixels), 60);
  EXPECT_EQ(PixelAt(pixels, kTargetW, 32, 32).r, 255);
  EXPECT_EQ(PixelAt(pixels, kTargetW, 2, 2).r, 0);
}

INSTANTIATE_TEST_SUITE_P(
    Backends, AntialiasingBackend, ::testing::ValuesIn(AvailableBackends()),
    [](const ::testing::TestParamInfo<RendererBackend>& i) {
      return i.param == RendererBackend::kOpenGL ? "OpenGL" : "Vulkan";
    });
