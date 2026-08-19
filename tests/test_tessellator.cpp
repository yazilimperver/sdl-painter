#include <gtest/gtest.h>

#include <cmath>

#include "sdl_painter/geometry.h"
#include "sdl_painter/vertex.h"

// Tessellator dahili başlık (src/ altında)
#include "../src/tessellator.h"

using sdl_painter::Point;
using sdl_painter::Tessellator;
using sdl_painter::TexturedVertex;
using sdl_painter::Vertex;

// ─── TessellateFilledRect ───────────────────────────────────────────────────

TEST(TessellateFilledRect, ProducesExactlySixVertices) {
  auto v = Tessellator::TessellateFilledRect(0, 0, 100, 50);
  EXPECT_EQ(v.size(), 6u);
}

TEST(TessellateFilledRect, VerticesCoverCorners) {
  auto v = Tessellator::TessellateFilledRect(10, 20, 100, 50);
  // Beklenen köşeler (x, y) çiftleri olarak kontrol
  bool has_tl = false, has_tr = false, has_bl = false, has_br = false;
  for (const auto& vert : v) {
    if (vert.x == 10.0f  && vert.y == 20.0f)  has_tl = true;
    if (vert.x == 110.0f && vert.y == 20.0f)  has_tr = true;
    if (vert.x == 10.0f  && vert.y == 70.0f)  has_bl = true;
    if (vert.x == 110.0f && vert.y == 70.0f)  has_br = true;
  }
  EXPECT_TRUE(has_tl) << "Sol-üst köşe eksik";
  EXPECT_TRUE(has_tr) << "Sağ-üst köşe eksik";
  EXPECT_TRUE(has_bl) << "Sol-alt köşe eksik";
  EXPECT_TRUE(has_br) << "Sağ-alt köşe eksik";
}

TEST(TessellateFilledRect, ZeroSizeProducesSixDegenerateVertices) {
  auto v = Tessellator::TessellateFilledRect(5, 5, 0, 0);
  EXPECT_EQ(v.size(), 6u);
  for (const auto& vert : v) {
    EXPECT_FLOAT_EQ(vert.x, 5.0f);
    EXPECT_FLOAT_EQ(vert.y, 5.0f);
  }
}

// ─── TessellateFilledCircle ─────────────────────────────────────────────────

TEST(TessellateFilledCircle, VertexCountIsMultipleOfThree) {
  auto v = Tessellator::TessellateFilledCircle(0, 0, 50.0f);
  EXPECT_GT(v.size(), 0u);
  EXPECT_EQ(v.size() % 3, 0u);
}

TEST(TessellateFilledCircle, AdaptiveSegmentsSmallRadius) {
  // radius=10 → segments = max(16, 5) = 16 → 48 vertex
  auto v = Tessellator::TessellateFilledCircle(0, 0, 10.0f);
  EXPECT_EQ(v.size(), 48u);
}

TEST(TessellateFilledCircle, AdaptiveSegmentsLargeRadius) {
  // radius=100 → segments = max(16, 50) = 50 → 150 vertex
  auto v = Tessellator::TessellateFilledCircle(0, 0, 100.0f);
  EXPECT_EQ(v.size(), 150u);
}

TEST(TessellateFilledCircle, CenterVertexMatchesInput) {
  float cx = 30.0f, cy = 40.0f;
  auto v = Tessellator::TessellateFilledCircle(cx, cy, 25.0f);
  // Her üçgende ilk vertex merkez olmalı
  for (std::size_t i = 0; i < v.size(); i += 3) {
    EXPECT_FLOAT_EQ(v[i].x, cx);
    EXPECT_FLOAT_EQ(v[i].y, cy);
  }
}

TEST(TessellateFilledCircle, PerimeterVerticesAtCorrectRadius) {
  float cx = 0.0f, cy = 0.0f, r = 40.0f;
  auto v = Tessellator::TessellateFilledCircle(cx, cy, r);
  for (std::size_t i = 1; i < v.size(); ++i) {
    float dx = v[i].x - cx;
    float dy = v[i].y - cy;
    float dist = std::sqrt(dx * dx + dy * dy);
    // Merkez vertex'leri (i % 3 == 0) atla
    if (i % 3 != 0) {
      EXPECT_NEAR(dist, r, 1e-4f) << "vertex " << i;
    }
  }
}

// ─── TessellateFilledEllipse ────────────────────────────────────────────────

TEST(TessellateFilledEllipse, VertexCountIsMultipleOfThree) {
  auto v = Tessellator::TessellateFilledEllipse(0, 0, 60.0f, 30.0f);
  EXPECT_GT(v.size(), 0u);
  EXPECT_EQ(v.size() % 3, 0u);
}

TEST(TessellateFilledEllipse, PerimeterVerticesOnEllipse) {
  float cx = 50.0f, cy = 50.0f, rx = 80.0f, ry = 40.0f;
  auto v = Tessellator::TessellateFilledEllipse(cx, cy, rx, ry);
  for (std::size_t i = 1; i < v.size(); ++i) {
    if (i % 3 == 0) continue;  // merkez vertex
    float dx = (v[i].x - cx) / rx;
    float dy = (v[i].y - cy) / ry;
    EXPECT_NEAR(dx * dx + dy * dy, 1.0f, 1e-4f) << "vertex " << i;
  }
}

TEST(TessellateFilledEllipse, CenterVertexMatchesInput) {
  auto v = Tessellator::TessellateFilledEllipse(10.0f, 20.0f, 50.0f, 30.0f);
  for (std::size_t i = 0; i < v.size(); i += 3) {
    EXPECT_FLOAT_EQ(v[i].x, 10.0f);
    EXPECT_FLOAT_EQ(v[i].y, 20.0f);
  }
}

// ─── TessellateThickLine ────────────────────────────────────────────────────

TEST(TessellateThickLine, ProducesExactlySixVertices) {
  auto v = Tessellator::TessellateThickLine(0, 0, 100, 0, 4.0f);
  EXPECT_EQ(v.size(), 6u);
}

TEST(TessellateThickLine, ZeroLengthReturnsEmpty) {
  auto v = Tessellator::TessellateThickLine(5, 5, 5, 5, 4.0f);
  EXPECT_TRUE(v.empty());
}

TEST(TessellateThickLine, HorizontalLineQuadWidth) {
  // Yatay çizgi: normal vektör dikey (0, 1)
  float width = 6.0f;
  auto v = Tessellator::TessellateThickLine(0.0f, 0.0f, 100.0f, 0.0f, width);
  ASSERT_EQ(v.size(), 6u);
  // Y koordinatları ±3 olmalı
  float min_y = v[0].y, max_y = v[0].y;
  for (const auto& vert : v) {
    min_y = std::min(min_y, vert.y);
    max_y = std::max(max_y, vert.y);
  }
  EXPECT_FLOAT_EQ(min_y, -3.0f);
  EXPECT_FLOAT_EQ(max_y,  3.0f);
}

TEST(TessellateThickLine, VerticalLineQuadWidth) {
  // Dikey çizgi: normal vektör yatay (-1, 0)
  float width = 8.0f;
  auto v = Tessellator::TessellateThickLine(0.0f, 0.0f, 0.0f, 100.0f, width);
  ASSERT_EQ(v.size(), 6u);
  float min_x = v[0].x, max_x = v[0].x;
  for (const auto& vert : v) {
    min_x = std::min(min_x, vert.x);
    max_x = std::max(max_x, vert.x);
  }
  EXPECT_FLOAT_EQ(min_x, -4.0f);
  EXPECT_FLOAT_EQ(max_x,  4.0f);
}

// ─── TessellateThickPolyline ────────────────────────────────────────────────

TEST(TessellateThickPolyline, TwoPointsEqualsOneLine) {
  auto line = Tessellator::TessellateThickLine(0, 0, 100, 0, 2.0f);
  auto poly = Tessellator::TessellateThickPolyline(
      {{0.0f, 0.0f}, {100.0f, 0.0f}}, 2.0f);
  ASSERT_EQ(line.size(), poly.size());
  for (std::size_t i = 0; i < line.size(); ++i) {
    EXPECT_FLOAT_EQ(line[i].x, poly[i].x);
    EXPECT_FLOAT_EQ(line[i].y, poly[i].y);
  }
}

TEST(TessellateThickPolyline, ThinLineHasNoJoinsAndIsNMinus1Times6) {
  std::vector<Point> pts = {
      {0, 0}, {100, 0}, {100, 100}, {200, 100}};
  // Kalinlik 1.5 pikselin altinda -> birlesim eklenmez.
  auto v = Tessellator::TessellateThickPolyline(pts, 1.0f);
  EXPECT_EQ(v.size(), 18u);  // 3 segment * 6 vertex
}

TEST(TessellateThickPolyline, ThickLineAddsRoundJoinsAtInteriorVertices) {
  std::vector<Point> pts = {
      {0, 0}, {100, 0}, {100, 100}, {200, 100}};
  auto thin = Tessellator::TessellateThickPolyline(pts, 1.0f);
  auto thick = Tessellator::TessellateThickPolyline(pts, 8.0f);
  // 4 nokta -> 2 ic kose -> 2 birlesim diski eklenir.
  EXPECT_GT(thick.size(), thin.size())
      << "Ic koselere birlesim eklenmedi (kosede bosluk kalir).";
  // Her disk ucgen fani oldugundan fark 3'un kati olmali.
  EXPECT_EQ((thick.size() - thin.size()) % 3, 0u);
}

TEST(TessellateThickPolyline, TwoPointsHasNoInteriorVertexSoNoJoin) {
  std::vector<Point> pts = {{0, 0}, {100, 0}};
  auto v = Tessellator::TessellateThickPolyline(pts, 8.0f);
  EXPECT_EQ(v.size(), 6u) << "Ic kose yokken birlesim eklenmemeli.";
}

TEST(TessellateThickPolyline, SinglePointReturnsEmpty) {
  auto v = Tessellator::TessellateThickPolyline({{10.0f, 10.0f}}, 2.0f);
  EXPECT_TRUE(v.empty());
}

// ─── TessellateStrokedRect ──────────────────────────────────────────────────

TEST(TessellateStrokedRect, ProducesNonEmptyResult) {
  auto v = Tessellator::TessellateStrokedRect(0, 0, 100, 50, 2.0f);
  EXPECT_FALSE(v.empty());
}

TEST(TessellateStrokedRect, ThinStrokeIsFourQuads) {
  // Kalinlik 1.5'in altinda -> birlesim yok: 4 kenar * 6 vertex = 24.
  auto v = Tessellator::TessellateStrokedRect(0, 0, 100, 50, 1.0f);
  EXPECT_EQ(v.size(), 24u);
}

TEST(TessellateStrokedRect, ThickStrokeJoinsAllFourCorners) {
  auto thin = Tessellator::TessellateStrokedRect(0, 0, 100, 50, 1.0f);
  auto thick = Tessellator::TessellateStrokedRect(0, 0, 100, 50, 10.0f);
  // Kapali poligon: 4 kosenin HEPSINE birlesim uygulanir.
  EXPECT_GT(thick.size(), thin.size());
  EXPECT_EQ((thick.size() - thin.size()) % 3, 0u);
}

TEST(TessellateStrokedRect, VertexCountIsMultipleOfThree) {
  auto v = Tessellator::TessellateStrokedRect(0, 0, 100, 50, 2.0f);
  EXPECT_EQ(v.size() % 3, 0u) << "Ucgen listesi 3'un kati olmali.";
}

// ─── TessellateStrokedCircle ─────────────────────────────────────────────────

TEST(TessellateStrokedCircle, ProducesNonEmptyResult) {
  auto v = Tessellator::TessellateStrokedCircle(0, 0, 50.0f, 2.0f);
  EXPECT_FALSE(v.empty());
}

TEST(TessellateStrokedCircle, VertexCountIsMultipleOfThree) {
  auto v = Tessellator::TessellateStrokedCircle(0, 0, 50.0f, 2.0f);
  EXPECT_EQ(v.size() % 3, 0u);
}

// ─── TessellateStrokedEllipse ────────────────────────────────────────────────

TEST(TessellateStrokedEllipse, ProducesNonEmptyResult) {
  auto v = Tessellator::TessellateStrokedEllipse(0, 0, 80.0f, 40.0f, 2.0f);
  EXPECT_FALSE(v.empty());
}

// ─── TessellateFilledPolygon / EarClipping ───────────────────────────────────

TEST(TessellateFilledPolygon, LessThanThreePointsReturnsEmpty) {
  EXPECT_TRUE(Tessellator::TessellateFilledPolygon({}).empty());
  EXPECT_TRUE(Tessellator::TessellateFilledPolygon({{0, 0}}).empty());
  EXPECT_TRUE(
      Tessellator::TessellateFilledPolygon({{0, 0}, {1, 0}}).empty());
}

TEST(TessellateFilledPolygon, TriangleProducesThreeVertices) {
  auto v = Tessellator::TessellateFilledPolygon(
      {{0, 0}, {100, 0}, {50, 100}});
  EXPECT_EQ(v.size(), 3u);
}

TEST(TessellateFilledPolygon, ConvexQuadProducesSixVertices) {
  // Saat yönünün tersine dikdörtgen
  auto v = Tessellator::TessellateFilledPolygon(
      {{0, 0}, {100, 0}, {100, 100}, {0, 100}});
  EXPECT_EQ(v.size(), 6u);
}

TEST(TessellateFilledPolygon, ConcaveLShapeCorrectTriangleCount) {
  // L şekli (6 köşe → 4 üçgen → 12 vertex)
  auto v = Tessellator::TessellateFilledPolygon({
      {0, 0}, {60, 0}, {60, 40}, {40, 40}, {40, 80}, {0, 80}});
  EXPECT_EQ(v.size(), 12u);
}

TEST(TessellateFilledPolygon, ClockwiseInputProducesSameCount) {
  // Aynı dikdörtgen CW sırayla
  auto v_ccw = Tessellator::TessellateFilledPolygon(
      {{0, 0}, {100, 0}, {100, 100}, {0, 100}});
  auto v_cw = Tessellator::TessellateFilledPolygon(
      {{0, 0}, {0, 100}, {100, 100}, {100, 0}});
  EXPECT_EQ(v_ccw.size(), v_cw.size());
}

TEST(TessellateFilledPolygon, TriangleVerticesMatchInput) {
  auto v = Tessellator::TessellateFilledPolygon(
      {{10, 20}, {110, 20}, {60, 120}});
  ASSERT_EQ(v.size(), 3u);
  EXPECT_FLOAT_EQ(v[0].x, 10.0f);  EXPECT_FLOAT_EQ(v[0].y, 20.0f);
  EXPECT_FLOAT_EQ(v[1].x, 110.0f); EXPECT_FLOAT_EQ(v[1].y, 20.0f);
  EXPECT_FLOAT_EQ(v[2].x, 60.0f);  EXPECT_FLOAT_EQ(v[2].y, 120.0f);
}

// ─── TessellateTexturedRect ──────────────────────────────────────────────────

TEST(TessellateTexturedRect, ProducesExactlySixVertices) {
  auto v = Tessellator::TessellateTexturedRect(0, 0, 100, 50, 0, 0, 1, 1);
  EXPECT_EQ(v.size(), 6u);
}

TEST(TessellateTexturedRect, UVCoordinatesCorrect) {
  auto v = Tessellator::TessellateTexturedRect(
      0.0f, 0.0f, 200.0f, 100.0f, 0.25f, 0.1f, 0.75f, 0.9f);
  ASSERT_EQ(v.size(), 6u);

  // İlk vertex: sol üst
  EXPECT_FLOAT_EQ(v[0].u, 0.25f);
  EXPECT_FLOAT_EQ(v[0].v, 0.1f);
  // İkinci vertex: sağ üst
  EXPECT_FLOAT_EQ(v[1].u, 0.75f);
  EXPECT_FLOAT_EQ(v[1].v, 0.1f);
  // Son vertex: sağ alt
  EXPECT_FLOAT_EQ(v[5].u, 0.75f);
  EXPECT_FLOAT_EQ(v[5].v, 0.9f);
}

TEST(TessellateTexturedRect, PositionCoordinatesCorrect) {
  auto v = Tessellator::TessellateTexturedRect(
      10.0f, 20.0f, 80.0f, 60.0f, 0, 0, 1, 1);
  ASSERT_EQ(v.size(), 6u);
  EXPECT_FLOAT_EQ(v[0].x, 10.0f);   EXPECT_FLOAT_EQ(v[0].y, 20.0f);
  EXPECT_FLOAT_EQ(v[1].x, 90.0f);   EXPECT_FLOAT_EQ(v[1].y, 20.0f);
  EXPECT_FLOAT_EQ(v[2].x, 10.0f);   EXPECT_FLOAT_EQ(v[2].y, 80.0f);
  EXPECT_FLOAT_EQ(v[5].x, 90.0f);   EXPECT_FLOAT_EQ(v[5].y, 80.0f);
}

// ─── Dejenere poligon girdileri (K4 regresyonu) ─────────────────────────────
//
// EarClipping, kulak bulamadığında `break` ile çıkıp kalan poligonu SESSİZCE
// atıyordu. Tekrarlı (duplicate) köşe içeren poligonlarda üçgenlerin çoğu
// kayboluyor; kullanıcı verisinden gelen poligonlarda görünür veri kaybı.
//
// Beklenen: n köşeli basit bir poligon (n-2) üçgen = (n-2)*3 vertex üretir.
// Tekrarlı köşeler mantıksal olarak elenir; sonuç eleme sonrası köşe
// sayısına göre değerlendirilir.

TEST(TessellateFilledPolygon, DuplicateVertexDoesNotDropTriangles) {
  // Kare + ikinci köşenin birebir tekrarı → mantıksal olarak hâlâ bir kare.
  const std::vector<Point> pts = {
      {0.0f, 0.0f}, {10.0f, 0.0f}, {10.0f, 0.0f}, {10.0f, 10.0f}, {0.0f, 10.0f},
  };
  auto v = Tessellator::TessellateFilledPolygon(pts);
  // Tekrar elendikten sonra 4 köşe kalır → 2 üçgen → 6 vertex.
  EXPECT_EQ(v.size(), 6u)
      << "Tekrarlı köşe üçgenlerin kaybolmasına yol açtı.";
}

TEST(TessellateFilledPolygon, MultipleDuplicateVerticesHandled) {
  const std::vector<Point> pts = {
      {0.0f, 0.0f},   {0.0f, 0.0f},  {10.0f, 0.0f},
      {10.0f, 10.0f}, {10.0f, 10.0f}, {0.0f, 10.0f},
  };
  auto v = Tessellator::TessellateFilledPolygon(pts);
  EXPECT_EQ(v.size(), 6u);
}

TEST(TessellateFilledPolygon, CollinearVertexDoesNotDropTriangles) {
  // Üst kenarın ortasında fazladan (kolineer) bir nokta.
  const std::vector<Point> pts = {
      {0.0f, 0.0f}, {5.0f, 0.0f}, {10.0f, 0.0f}, {10.0f, 10.0f}, {0.0f, 10.0f},
  };
  auto v = Tessellator::TessellateFilledPolygon(pts);
  EXPECT_EQ(v.size(), 9u) << "5 köşe → 3 üçgen beklenir.";
}

TEST(TessellateFilledPolygon, DegenerateInputCollapsingToLineReturnsEmpty) {
  // Tüm noktalar aynı → geçerli hiçbir üçgen yok, ama çökmemeli.
  const std::vector<Point> pts = {
      {5.0f, 5.0f}, {5.0f, 5.0f}, {5.0f, 5.0f}, {5.0f, 5.0f},
  };
  auto v = Tessellator::TessellateFilledPolygon(pts);
  EXPECT_TRUE(v.empty());
}

TEST(TessellateFilledPolygon, ConcaveWithDuplicateVertexKeepsAllTriangles) {
  // Konkav L şekli + tekrarlı köşe → eleme sonrası 6 köşe = 4 üçgen.
  const std::vector<Point> pts = {
      {0.0f, 0.0f},  {10.0f, 0.0f}, {10.0f, 10.0f},
      {5.0f, 10.0f}, {5.0f, 10.0f}, {5.0f, 5.0f}, {0.0f, 5.0f},
  };
  auto v = Tessellator::TessellateFilledPolygon(pts);
  EXPECT_EQ(v.size(), 12u);
}

// ─── Adaptif segment üst sınırı (Y5 regresyonu) ─────────────────────────────

TEST(TessellateFilledCircle, SegmentCountIsCapped) {
  // Üst sınır olmadan yarıçap 100000 → 150.000 vertex (~1.7 MB) üretiliyordu.
  auto v = Tessellator::TessellateFilledCircle(0.0f, 0.0f, 100000.0f);
  EXPECT_LE(v.size(), 512u * 3u)
      << "Segment sayısı sınırsız: " << v.size() << " vertex üretildi.";
  EXPECT_GT(v.size(), 0u);
}

TEST(TessellateStrokedCircle, SegmentCountIsCapped) {
  auto v = Tessellator::TessellateStrokedCircle(0.0f, 0.0f, 100000.0f, 2.0f);
  // 512 segment * 6 vertex (quad) + 512 birlesim diski * en fazla 24 * 3.
  EXPECT_LE(v.size(), 512u * 6u + 512u * 24u * 3u);
  EXPECT_GT(v.size(), 0u);
}
