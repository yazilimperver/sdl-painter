/// @file test_path.cpp
/// @brief Path yapımı, Bézier düzleştirme ve Painter entegrasyonu.
///
/// Düzleştirmenin doğruluğu **sayısal olarak** sınanır: üretilen kırık çizgi
/// ile gerçek eğri arasındaki azami sapma ölçülüp toleransla karşılaştırılır.
/// Segment sayısını sabit bir beklenen değerle karşılaştırmak, formülü
/// yeniden yazmaktan başka bir şey doğrulamazdı.

#include "sdl_painter/brush.h"
#include "sdl_painter/painter.h"
#include "sdl_painter/path.h"
#include "sdl_painter/pen.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <gtest/gtest.h>
#include <memory>
#include <vector>

#include "mock_renderer.h"

using sdl_painter::Brush;
using sdl_painter::Color;
using sdl_painter::MockRenderer;
using sdl_painter::Painter;
using sdl_painter::Path;
using sdl_painter::Pen;
using sdl_painter::Point;
using sdl_painter::SubPath;

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

/// @brief Referans cubic Bézier değerlendirmesi (test tarafı, bağımsız).
Point CubicAt(const Point& p0, const Point& p1, const Point& p2,
              const Point& p3, float t) {
  const float u = 1.0F - t;
  const float w0 = u * u * u;
  const float w1 = 3.0F * u * u * t;
  const float w2 = 3.0F * u * t * t;
  const float w3 = t * t * t;
  return {(w0 * p0.x) + (w1 * p1.x) + (w2 * p2.x) + (w3 * p3.x),
          (w0 * p0.y) + (w1 * p1.y) + (w2 * p2.y) + (w3 * p3.y)};
}

float Distance(const Point& a, const Point& b) {
  const float dx = a.x - b.x;
  const float dy = a.y - b.y;
  return std::sqrt((dx * dx) + (dy * dy));
}

/// @brief Düzleştirilmiş eğrinin gerçek eğriden azami sapması.
///
/// Her segmentin orta noktası, aynı parametre aralığının ortasındaki gerçek
/// eğri noktasıyla karşılaştırılır — düzgün örneklemede sapmanın en büyük
/// olduğu yer burasıdır.
float MaxDeviation(const std::vector<Point>& flat, const Point& p0,
                   const Point& p1, const Point& p2, const Point& p3) {
  const auto count = static_cast<int32_t>(flat.size()) - 1;
  float worst = 0.0F;
  for (int32_t i = 0; i < count; ++i) {
    const Point chord_mid{(flat[static_cast<std::size_t>(i)].x +
                           flat[static_cast<std::size_t>(i) + 1].x) *
                              0.5F,
                          (flat[static_cast<std::size_t>(i)].y +
                           flat[static_cast<std::size_t>(i) + 1].y) *
                              0.5F};
    const float t = (static_cast<float>(i) + 0.5F) / static_cast<float>(count);
    worst = std::max(worst, Distance(chord_mid, CubicAt(p0, p1, p2, p3, t)));
  }
  return worst;
}

/// @brief Renderer'a giden toplam vertex sayısı.
std::size_t TotalVertices(const MockRenderer& mock) {
  std::size_t total = 0;
  for (const auto& call : mock.calls) {
    total += call.vertex_count;
  }
  return total;
}

// ---------------------------------------------------------------------------
// Yol kurulumu
// ---------------------------------------------------------------------------

TEST(PathTest, DefaultPathIsEmpty) {
  const Path path;
  EXPECT_TRUE(path.IsEmpty());
  EXPECT_EQ(path.SubPaths().size(), 0U);
  EXPECT_EQ(path.PointCount(), 0U);
  EXPECT_FLOAT_EQ(path.Flatness(), sdl_painter::kDefaultFlatness);
}

TEST(PathTest, NonPositiveFlatnessFallsBackToDefault) {
  EXPECT_FLOAT_EQ(Path(0.0F).Flatness(), sdl_painter::kDefaultFlatness);
  EXPECT_FLOAT_EQ(Path(-1.0F).Flatness(), sdl_painter::kDefaultFlatness);
  EXPECT_FLOAT_EQ(Path(2.0F).Flatness(), 2.0F);
}

TEST(PathTest, MoveToThenLineToMakesOneOpenSubPath) {
  Path path;
  path.MoveTo(10.0F, 20.0F);
  path.LineTo(30.0F, 40.0F);

  ASSERT_EQ(path.SubPaths().size(), 1U);
  const SubPath& sub = path.SubPaths().front();
  ASSERT_EQ(sub.points.size(), 2U);
  EXPECT_FALSE(sub.closed);
  EXPECT_FLOAT_EQ(sub.points[0].x, 10.0F);
  EXPECT_FLOAT_EQ(sub.points[1].y, 40.0F);
  EXPECT_FALSE(path.IsEmpty());
}

TEST(PathTest, LoneMoveToDrawsNothing) {
  Path path;
  path.MoveTo(10.0F, 20.0F);
  EXPECT_TRUE(path.IsEmpty());
}

TEST(PathTest, ConsecutiveMoveToLeavesNoDegenerateSubPath) {
  Path path;
  path.MoveTo(0.0F, 0.0F);
  path.MoveTo(10.0F, 10.0F);
  path.MoveTo(20.0F, 20.0F);
  path.LineTo(30.0F, 30.0F);

  ASSERT_EQ(path.SubPaths().size(), 1U);
  EXPECT_FLOAT_EQ(path.SubPaths().front().points[0].x, 20.0F);
}

TEST(PathTest, LineToOnEmptyPathActsAsMoveTo) {
  Path path;
  path.LineTo(5.0F, 6.0F);
  EXPECT_TRUE(path.IsEmpty());
  EXPECT_FLOAT_EQ(path.CurrentPoint().x, 5.0F);
  EXPECT_FLOAT_EQ(path.CurrentPoint().y, 6.0F);
}

TEST(PathTest, CloseMarksSubPathAndRewindsCursor) {
  Path path;
  path.MoveTo(0.0F, 0.0F);
  path.LineTo(10.0F, 0.0F);
  path.LineTo(10.0F, 10.0F);
  path.Close();

  ASSERT_EQ(path.SubPaths().size(), 1U);
  EXPECT_TRUE(path.SubPaths().front().closed);
  // Kapanis dogrusu NOKTA olarak eklenmez.
  EXPECT_EQ(path.SubPaths().front().points.size(), 3U);
  EXPECT_FLOAT_EQ(path.CurrentPoint().x, 0.0F);
  EXPECT_FLOAT_EQ(path.CurrentPoint().y, 0.0F);
}

TEST(PathTest, DrawingAfterCloseStartsNewSubPath) {
  Path path;
  path.MoveTo(0.0F, 0.0F);
  path.LineTo(10.0F, 0.0F);
  path.Close();
  path.LineTo(20.0F, 20.0F);

  ASSERT_EQ(path.SubPaths().size(), 2U);
  EXPECT_TRUE(path.SubPaths()[0].closed);
  EXPECT_FALSE(path.SubPaths()[1].closed);
  // Yeni parca kapanis noktasindan (alt yolun basi) baslar.
  EXPECT_FLOAT_EQ(path.SubPaths()[1].points[0].x, 0.0F);
}

TEST(PathTest, CloseOnSinglePointSubPathIsNoOp) {
  Path path;
  path.MoveTo(4.0F, 4.0F);
  path.Close();
  EXPECT_TRUE(path.IsEmpty());
}

TEST(PathTest, ClearResetsEverythingButFlatness) {
  Path path(1.0F);
  path.MoveTo(0.0F, 0.0F);
  path.LineTo(10.0F, 10.0F);
  path.Clear();

  EXPECT_TRUE(path.IsEmpty());
  EXPECT_EQ(path.SubPaths().size(), 0U);
  EXPECT_FLOAT_EQ(path.CurrentPoint().x, 0.0F);
  EXPECT_FLOAT_EQ(path.Flatness(), 1.0F);
}

// ---------------------------------------------------------------------------
// Bézier düzleştirme
// ---------------------------------------------------------------------------

TEST(PathTest, CubicWithoutMoveToStartsAtOrigin) {
  Path path;
  path.CubicTo(10.0F, 0.0F, 20.0F, 0.0F, 30.0F, 0.0F);

  ASSERT_EQ(path.SubPaths().size(), 1U);
  EXPECT_FLOAT_EQ(path.SubPaths().front().points.front().x, 0.0F);
  EXPECT_FLOAT_EQ(path.SubPaths().front().points.front().y, 0.0F);
}

TEST(PathTest, CollinearCubicNeedsOnlyOneSegment) {
  // Kontrol noktalari dogru uzerinde esit araliklarla: egrilik sifir.
  Path path;
  path.MoveTo(0.0F, 0.0F);
  path.CubicTo(10.0F, 0.0F, 20.0F, 0.0F, 30.0F, 0.0F);

  ASSERT_EQ(path.SubPaths().size(), 1U);
  EXPECT_EQ(path.SubPaths().front().points.size(), 2U);
}

TEST(PathTest, CollinearQuadNeedsOnlyOneSegment) {
  Path path;
  path.MoveTo(0.0F, 0.0F);
  path.QuadTo(5.0F, 0.0F, 10.0F, 0.0F);

  ASSERT_EQ(path.SubPaths().size(), 1U);
  EXPECT_EQ(path.SubPaths().front().points.size(), 2U);
}

TEST(PathTest, CubicEndsExactlyAtEndpoint) {
  Path path;
  path.MoveTo(0.0F, 100.0F);
  path.CubicTo(40.0F, 0.0F, 160.0F, 200.0F, 200.0F, 100.0F);

  const Point last = path.SubPaths().front().points.back();
  EXPECT_FLOAT_EQ(last.x, 200.0F);
  EXPECT_FLOAT_EQ(last.y, 100.0F);
  EXPECT_FLOAT_EQ(path.CurrentPoint().x, 200.0F);
}

TEST(PathTest, FlattenedCubicStaysWithinFlatness) {
  const Point p0{0.0F, 100.0F};
  const Point p1{40.0F, 0.0F};
  const Point p2{160.0F, 200.0F};
  const Point p3{200.0F, 100.0F};

  for (const float flatness : {0.25F, 0.5F, 1.0F, 2.0F}) {
    Path path(flatness);
    path.MoveTo(p0.x, p0.y);
    path.CubicTo(p1.x, p1.y, p2.x, p2.y, p3.x, p3.y);

    ASSERT_EQ(path.SubPaths().size(), 1U);
    const float deviation =
        MaxDeviation(path.SubPaths().front().points, p0, p1, p2, p3);
    EXPECT_LE(deviation, flatness)
        << "flatness=" << flatness << " sapma=" << deviation;
  }
}

TEST(PathTest, SmallerFlatnessProducesMorePoints) {
  const auto build = [](float flatness) {
    Path path(flatness);
    path.MoveTo(0.0F, 100.0F);
    path.CubicTo(40.0F, 0.0F, 160.0F, 200.0F, 200.0F, 100.0F);
    return path.PointCount();
  };
  EXPECT_GT(build(0.05F), build(1.0F));
}

TEST(PathTest, ExtremeCurvatureIsCappedNotUnbounded) {
  // Cok buyuk kontrol poligonu + cok kucuk tolerans: sinirsiz olsaydi
  // yuz binlerce nokta uretirdi.
  Path path(0.001F);
  path.MoveTo(0.0F, 0.0F);
  path.CubicTo(100000.0F, 100000.0F, -100000.0F, 100000.0F, 0.0F, 0.0F);

  ASSERT_EQ(path.SubPaths().size(), 1U);
  // Sinir 256 segment → en fazla 257 nokta.
  EXPECT_LE(path.SubPaths().front().points.size(), 257U);
}

// ---------------------------------------------------------------------------
// Painter entegrasyonu
// ---------------------------------------------------------------------------

TEST(PainterPathTest, DrawPathEmitsGeometry) {
  Harness h;
  h.painter.SetPen(Pen(Color::White(), 3.0F));

  Path path;
  path.MoveTo(10.0F, 10.0F);
  path.LineTo(100.0F, 10.0F);
  path.LineTo(100.0F, 100.0F);

  h.painter.Begin();
  h.painter.DrawPath(path);
  h.painter.End();

  EXPECT_GT(TotalVertices(*h.mock), 0U);
}

TEST(PainterPathTest, DrawPathWithNoPenEmitsNothing) {
  Harness h;
  h.painter.SetPen(Pen::NoPen());

  Path path;
  path.MoveTo(10.0F, 10.0F);
  path.LineTo(100.0F, 10.0F);

  h.painter.Begin();
  h.painter.DrawPath(path);
  h.painter.End();

  EXPECT_EQ(TotalVertices(*h.mock), 0U);
}

TEST(PainterPathTest, EmptyPathEmitsNothing) {
  Harness h;
  h.painter.SetPen(Pen(Color::White(), 3.0F));
  h.painter.SetBrush(Brush(Color::Red()));

  const Path path;

  h.painter.Begin();
  h.painter.DrawPath(path);
  h.painter.FillPath(path);
  h.painter.End();

  EXPECT_EQ(TotalVertices(*h.mock), 0U);
}

TEST(PainterPathTest, FillPathTriangulatesEachSubPath) {
  // Ucgen sayisi kadar alt yol iceren bir yolu doldur; vertex sayisi alt yol
  // sayisiyla dogru orantili olmali (her alt yol bagimsiz ucgenlenir).
  const auto fill_vertex_count = [](int32_t triangle_count) {
    Harness h;
    h.painter.SetPen(Pen::NoPen());
    h.painter.SetBrush(Brush(Color::Red()));

    Path path;
    for (int32_t i = 0; i < triangle_count; ++i) {
      const float offset = static_cast<float>(i) * 100.0F;
      path.MoveTo(offset, 0.0F);
      path.LineTo(offset + 50.0F, 0.0F);
      path.LineTo(offset + 50.0F, 50.0F);
      path.Close();
    }

    h.painter.Begin();
    h.painter.FillPath(path);
    h.painter.End();
    return TotalVertices(*h.mock);
  };

  const std::size_t one = fill_vertex_count(1);
  EXPECT_EQ(one, 3U);
  EXPECT_EQ(fill_vertex_count(3), 3U * one);
}

TEST(PainterPathTest, FillPathIgnoresSubPathsWithFewerThanThreePoints) {
  Harness h;
  h.painter.SetPen(Pen::NoPen());
  h.painter.SetBrush(Brush(Color::Red()));

  Path path;
  path.MoveTo(0.0F, 0.0F);
  path.LineTo(50.0F, 0.0F);  // yalnizca iki nokta — alan yok

  h.painter.Begin();
  h.painter.FillPath(path);
  h.painter.End();

  EXPECT_EQ(TotalVertices(*h.mock), 0U);
}

TEST(PainterPathTest, ClosedSubPathStrokesMoreThanOpenOne) {
  // Kapali yol, kapanis kenarini da cizer; acik olandan fazla vertex uretmeli.
  const auto stroke_vertex_count = [](bool closed) {
    Harness h;
    h.painter.SetPen(Pen(Color::White(), 4.0F));

    Path path;
    path.MoveTo(0.0F, 0.0F);
    path.LineTo(60.0F, 0.0F);
    path.LineTo(60.0F, 60.0F);
    if (closed) {
      path.Close();
    }

    h.painter.Begin();
    h.painter.DrawPath(path);
    h.painter.End();
    return TotalVertices(*h.mock);
  };

  EXPECT_GT(stroke_vertex_count(true), stroke_vertex_count(false));
}

TEST(PainterPathTest, PenOutlineAddsASecondStrokePass) {
  const auto vertex_count = [](bool with_outline) {
    Harness h;
    Pen pen(Color::White(), 3.0F);
    if (with_outline) {
      pen = Pen(Color::White(), 3.0F, Color::Black(), 2.0F);
    }
    h.painter.SetPen(pen);

    Path path;
    path.MoveTo(10.0F, 10.0F);
    path.LineTo(100.0F, 10.0F);

    h.painter.Begin();
    h.painter.DrawPath(path);
    h.painter.End();
    return TotalVertices(*h.mock);
  };

  EXPECT_GT(vertex_count(true), vertex_count(false));
}

}  // namespace
