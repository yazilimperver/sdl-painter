#include <gtest/gtest.h>

#include <cmath>
#include <cstddef>
#include <vector>

#include "sdl_painter/geometry.h"
#include "sdl_painter/vertex.h"

// Tessellator dahili başlık (src/ altında)
#include "../src/tessellator.h"

using sdl_painter::Point;
using sdl_painter::Tessellator;
using sdl_painter::TexturedVertex;
using sdl_painter::Vertex;

namespace {

/// @brief Vertex kümesinin eksen hizalı sınır kutusu.
struct Box {
  float min_x, min_y, max_x, max_y;
};

Box BoxOf(const std::vector<Vertex>& v) {
  Box b{v.at(0).x, v.at(0).y, v.at(0).x, v.at(0).y};
  for (const auto& p : v) {
    b.min_x = std::fmin(b.min_x, p.x);
    b.min_y = std::fmin(b.min_y, p.y);
    b.max_x = std::fmax(b.max_x, p.x);
    b.max_y = std::fmax(b.max_y, p.y);
  }
  return b;
}

}  // namespace


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

// ─── Yuvarlatılmış dikdörtgen ───────────────────────────────────────────────

TEST(RoundedRect, ZeroRadiusGivesThePlainFourCorners) {
  auto pts = Tessellator::BuildRoundedRectPoints(10, 20, 100, 50, 0.0f);
  ASSERT_EQ(pts.size(), 4u);
  EXPECT_FLOAT_EQ(pts[0].x, 10.0f);
  EXPECT_FLOAT_EQ(pts[0].y, 20.0f);
  EXPECT_FLOAT_EQ(pts[2].x, 110.0f);
  EXPECT_FLOAT_EQ(pts[2].y, 70.0f);
}

TEST(RoundedRect, NegativeRadiusBehavesLikeZero) {
  auto zero = Tessellator::BuildRoundedRectPoints(0, 0, 80, 40, 0.0f);
  auto neg = Tessellator::BuildRoundedRectPoints(0, 0, 80, 40, -12.0f);
  ASSERT_EQ(neg.size(), zero.size());
  for (std::size_t i = 0; i < neg.size(); ++i) {
    EXPECT_FLOAT_EQ(neg[i].x, zero[i].x);
    EXPECT_FLOAT_EQ(neg[i].y, zero[i].y);
  }
}

TEST(RoundedRect, NonPositiveSizeReturnsEmpty) {
  EXPECT_TRUE(Tessellator::BuildRoundedRectPoints(0, 0, 0, 40, 5.0f).empty());
  EXPECT_TRUE(Tessellator::BuildRoundedRectPoints(0, 0, 80, 0, 5.0f).empty());
  EXPECT_TRUE(Tessellator::BuildRoundedRectPoints(0, 0, -10, 40, 5.0f).empty());
}

/// @brief Dış hat asla verilen dikdörtgenin dışına taşmamalı.
TEST(RoundedRect, OutlineStaysInsideTheRectangle) {
  const float x = 15.0f;
  const float y = 25.0f;
  const float w = 120.0f;
  const float h = 80.0f;
  auto pts = Tessellator::BuildRoundedRectPoints(x, y, w, h, 20.0f);
  ASSERT_GE(pts.size(), 4u);
  for (const auto& p : pts) {
    EXPECT_GE(p.x, x - 1e-3f);
    EXPECT_LE(p.x, x + w + 1e-3f);
    EXPECT_GE(p.y, y - 1e-3f);
    EXPECT_LE(p.y, y + h + 1e-3f);
  }
}

TEST(RoundedRect, OutlineTouchesAllFourEdges) {
  const float x = 0.0f;
  const float y = 0.0f;
  const float w = 100.0f;
  const float h = 60.0f;
  auto pts = Tessellator::BuildRoundedRectPoints(x, y, w, h, 15.0f);
  ASSERT_GE(pts.size(), 4u);

  float min_x = 1e9f;
  float max_x = -1e9f;
  float min_y = 1e9f;
  float max_y = -1e9f;
  for (const auto& p : pts) {
    min_x = std::fmin(min_x, p.x);
    max_x = std::fmax(max_x, p.x);
    min_y = std::fmin(min_y, p.y);
    max_y = std::fmax(max_y, p.y);
  }
  EXPECT_NEAR(min_x, x, 1e-3f);
  EXPECT_NEAR(max_x, x + w, 1e-3f);
  EXPECT_NEAR(min_y, y, 1e-3f);
  EXPECT_NEAR(max_y, y + h, 1e-3f);
}

/// @brief Yarıçap yarım boyutu aşarsa kırpılmalı — aksi halde şekil kendi
///        üzerine kıvrılıp bozulurdu.
TEST(RoundedRect, OversizedRadiusIsClampedToHalfTheShortSide) {
  const float w = 100.0f;
  const float h = 60.0f;
  auto huge = Tessellator::BuildRoundedRectPoints(0, 0, w, h, 5000.0f);
  auto clamped = Tessellator::BuildRoundedRectPoints(0, 0, w, h, h * 0.5f);

  ASSERT_EQ(huge.size(), clamped.size());
  for (std::size_t i = 0; i < huge.size(); ++i) {
    EXPECT_NEAR(huge[i].x, clamped[i].x, 1e-3f) << "i=" << i;
    EXPECT_NEAR(huge[i].y, clamped[i].y, 1e-3f) << "i=" << i;
  }
  // Ve hala dikdortgenin icinde.
  for (const auto& p : huge) {
    EXPECT_LE(p.x, w + 1e-3f);
    EXPECT_LE(p.y, h + 1e-3f);
  }
}

/// @brief Kare girdide azami yarıçap daireye dejenere olmalı.
TEST(RoundedRect, SquareWithMaxRadiusBecomesACircle) {
  const float size = 80.0f;
  auto pts = Tessellator::BuildRoundedRectPoints(0, 0, size, size, size * 0.5f);
  ASSERT_GE(pts.size(), 4u);
  const float r = size * 0.5f;
  for (const auto& p : pts) {
    const float dx = p.x - r;
    const float dy = p.y - r;
    EXPECT_NEAR(std::sqrt(dx * dx + dy * dy), r, 1e-2f);
  }
}

TEST(RoundedRect, SegmentCountGrowsWithRadius) {
  auto small = Tessellator::BuildRoundedRectPoints(0, 0, 400, 400, 8.0f);
  auto big = Tessellator::BuildRoundedRectPoints(0, 0, 400, 400, 150.0f);
  EXPECT_LT(small.size(), big.size())
      << "Kose cozunurlugu yaricapa gore uyarlanmiyor.";
}

TEST(RoundedRect, FilledPolygonAcceptsTheOutline) {
  auto pts = Tessellator::BuildRoundedRectPoints(0, 0, 120, 80, 20.0f);
  auto verts = Tessellator::TessellateFilledPolygon(pts);
  EXPECT_FALSE(verts.empty()) << "Konveks dis hat ear clipping'i gecemedi.";
  EXPECT_EQ(verts.size() % 3, 0u);
}

// ─── Yay / dilim / kiriş ────────────────────────────────────────────────────

TEST(ArcPoints, EndpointsMatchStartAndSweepAngles) {
  // 0°'den 90°'ye çeyrek yay, yarıçap 100, merkez (0, 0).
  auto pts = Tessellator::BuildArcPoints(0, 0, 100, 100, 0.0f, 90.0f);
  ASSERT_GE(pts.size(), 2u);
  EXPECT_NEAR(pts.front().x, 100.0f, 1e-3f);
  EXPECT_NEAR(pts.front().y, 0.0f, 1e-3f);
  EXPECT_NEAR(pts.back().x, 0.0f, 1e-3f);
  EXPECT_NEAR(pts.back().y, 100.0f, 1e-3f);
}

TEST(ArcPoints, AllPointsLieOnTheEllipse) {
  auto pts = Tessellator::BuildArcPoints(50, 20, 80, 40, 30.0f, 200.0f);
  ASSERT_GE(pts.size(), 2u);
  for (const auto& p : pts) {
    const float dx = (p.x - 50.0f) / 80.0f;
    const float dy = (p.y - 20.0f) / 40.0f;
    EXPECT_NEAR(dx * dx + dy * dy, 1.0f, 1e-4f);
  }
}

/// @brief Segment sayısı taranan açıya göre uyarlanmalı: küçük bir yay,
///        tam çemberle aynı sayıda segment harcamamalı.
TEST(ArcPoints, SegmentCountScalesWithSweep) {
  auto small_arc = Tessellator::BuildArcPoints(0, 0, 100, 100, 0.0f, 10.0f);
  auto full = Tessellator::BuildArcPoints(0, 0, 100, 100, 0.0f, 360.0f);
  EXPECT_LT(small_arc.size(), full.size());
  EXPECT_GE(small_arc.size(), 3u) << "En az iki segment uretilmeli.";
}

TEST(ArcPoints, NegativeSweepGoesTheOtherWay) {
  auto forward = Tessellator::BuildArcPoints(0, 0, 100, 100, 0.0f, 90.0f);
  auto backward = Tessellator::BuildArcPoints(0, 0, 100, 100, 0.0f, -90.0f);
  ASSERT_GE(forward.size(), 2u);
  ASSERT_GE(backward.size(), 2u);
  EXPECT_NEAR(forward.back().y, 100.0f, 1e-3f);
  EXPECT_NEAR(backward.back().y, -100.0f, 1e-3f);
}

TEST(ArcPoints, SweepBeyondFullTurnIsClamped) {
  auto full = Tessellator::BuildArcPoints(0, 0, 100, 100, 0.0f, 360.0f);
  auto over = Tessellator::BuildArcPoints(0, 0, 100, 100, 0.0f, 5000.0f);
  EXPECT_EQ(over.size(), full.size())
      << "Tam turdan fazlasi kirpilmadi; segment sayisi patlar.";
}

TEST(ArcPoints, DegenerateInputReturnsEmpty) {
  EXPECT_TRUE(Tessellator::BuildArcPoints(0, 0, 0, 100, 0.0f, 90.0f).empty());
  EXPECT_TRUE(Tessellator::BuildArcPoints(0, 0, 100, 100, 0.0f, 0.0f).empty());
}

TEST(FilledPie, TrianglesAllStartAtTheCentre) {
  auto v = Tessellator::TessellateFilledPie(10, 20, 50, 50, 0.0f, 90.0f);
  ASSERT_GT(v.size(), 0u);
  ASSERT_EQ(v.size() % 3, 0u);
  for (std::size_t i = 0; i < v.size(); i += 3) {
    EXPECT_FLOAT_EQ(v[i].x, 10.0f);
    EXPECT_FLOAT_EQ(v[i].y, 20.0f);
  }
}

/// @brief Tam tur dilim, dolu daire ile aynı alanı kaplamalı.
TEST(FilledPie, FullSweepCoversTheWholeEllipse) {
  auto pie = Tessellator::TessellateFilledPie(0, 0, 100, 100, 0.0f, 360.0f);
  ASSERT_FALSE(pie.empty());
  const Box b = BoxOf(pie);
  EXPECT_NEAR(b.min_x, -100.0f, 0.5f);
  EXPECT_NEAR(b.max_x, 100.0f, 0.5f);
  EXPECT_NEAR(b.min_y, -100.0f, 0.5f);
  EXPECT_NEAR(b.max_y, 100.0f, 0.5f);
}

/// @brief Kiriş merkezi İÇERMEZ — dilimden farkı budur.
TEST(FilledChord, DoesNotIncludeTheCentre) {
  // 0°..90° kirisi: merkez (0,0), yay birinci ceyrekte. Kirisin kapladigi
  // bolge, merkezden uzaktaki ince dilimdir.
  auto chord = Tessellator::TessellateFilledChord(0, 0, 100, 100, 0.0f, 90.0f);
  ASSERT_FALSE(chord.empty());
  for (const auto& p : chord) {
    // Hicbir vertex merkezde olmamali.
    const bool at_centre =
        std::fabs(p.x) < 1e-3f && std::fabs(p.y) < 1e-3f;
    EXPECT_FALSE(at_centre) << "Kiris merkezi iceriyor; bu dilim davranisi.";
  }
}

TEST(FilledChord, ProducesFanFromFirstArcPoint) {
  auto v = Tessellator::TessellateFilledChord(0, 0, 100, 50, 0.0f, 180.0f);
  ASSERT_GT(v.size(), 0u);
  ASSERT_EQ(v.size() % 3, 0u);
  // Fan tepe noktasi yayin ilk noktasi: (100, 0).
  for (std::size_t i = 0; i < v.size(); i += 3) {
    EXPECT_NEAR(v[i].x, 100.0f, 1e-3f);
    EXPECT_NEAR(v[i].y, 0.0f, 1e-3f);
  }
}

TEST(FilledChord, DegenerateSweepReturnsEmpty) {
  EXPECT_TRUE(
      Tessellator::TessellateFilledChord(0, 0, 100, 100, 0.0f, 0.0f).empty());
  EXPECT_TRUE(
      Tessellator::TessellateFilledPie(0, 0, 100, 100, 0.0f, 0.0f).empty());
}

// ─── Uç stili (LineCap) ─────────────────────────────────────────────────────

TEST(LineCap, ButtIsTheDefaultAndAddsNothing) {
  auto implicit = Tessellator::TessellateThickLine(0, 0, 100, 0, 8.0f);
  auto explicit_butt = Tessellator::TessellateThickLine(
      0, 0, 100, 0, 8.0f, sdl_painter::LineCap::kButt);
  EXPECT_EQ(implicit.size(), 6u);
  EXPECT_EQ(implicit.size(), explicit_butt.size());
}

TEST(LineCap, SquareExtendsBothEndsByHalfWidth) {
  constexpr float kWidth = 8.0f;
  auto butt = Tessellator::TessellateThickLine(0, 0, 100, 0, kWidth);
  auto square = Tessellator::TessellateThickLine(
      0, 0, 100, 0, kWidth, sdl_painter::LineCap::kSquare);

  const Box b = BoxOf(butt);
  const Box s = BoxOf(square);

  EXPECT_FLOAT_EQ(s.min_x, b.min_x - kWidth * 0.5f)
      << "Bas uc yarim kalinlik kadar uzatilmadi.";
  EXPECT_FLOAT_EQ(s.max_x, b.max_x + kWidth * 0.5f)
      << "Son uc yarim kalinlik kadar uzatilmadi.";
  // Kalinlik yonunde buyume olmamali.
  EXPECT_FLOAT_EQ(s.min_y, b.min_y);
  EXPECT_FLOAT_EQ(s.max_y, b.max_y);
  // Iki uc quad'i: 2 * 6 vertex.
  EXPECT_EQ(square.size(), butt.size() + 12u);
}

TEST(LineCap, RoundExtendsBothEndsByHalfWidth) {
  constexpr float kWidth = 8.0f;
  auto butt = Tessellator::TessellateThickLine(0, 0, 100, 0, kWidth);
  auto round = Tessellator::TessellateThickLine(
      0, 0, 100, 0, kWidth, sdl_painter::LineCap::kRound);

  const Box b = BoxOf(butt);
  const Box r = BoxOf(round);

  EXPECT_NEAR(r.min_x, b.min_x - kWidth * 0.5f, 1e-4f);
  EXPECT_NEAR(r.max_x, b.max_x + kWidth * 0.5f, 1e-4f);
  EXPECT_GT(round.size(), butt.size());
  // Disk ucgen fani: eklenen vertex sayisi 3'un kati.
  EXPECT_EQ((round.size() - butt.size()) % 3, 0u);
}

TEST(LineCap, AppliedOnlyToPolylineEndsNotInteriorVertices) {
  const std::vector<Point> pts = {{0, 0}, {100, 0}, {100, 100}};
  auto butt = Tessellator::TessellateThickPolyline(pts, 8.0f);
  auto square = Tessellator::TessellateThickPolyline(
      pts, 8.0f, sdl_painter::LineCap::kSquare);
  // Yalnizca 2 uc -> 2 quad -> 12 vertex. Ic kose uc almaz.
  EXPECT_EQ(square.size(), butt.size() + 12u);
}

TEST(LineCap, IgnoredOnClosedGeometry) {
  const std::vector<Point> pts = {{0, 0}, {100, 0}, {100, 100}};
  auto a = Tessellator::TessellateStrokedPolygon(pts, 8.0f);
  auto b = Tessellator::TessellateStrokedRect(0, 0, 50, 50, 8.0f);
  // Kapali geometride uc yoktur; sadece derlenip makul cikti uretmeli.
  EXPECT_FALSE(a.empty());
  EXPECT_FALSE(b.empty());
}

TEST(LineCap, ThinLineSkipsCapsBecauseTheyWouldBeInvisible) {
  // 1.5 piksel esigi altinda uc geometrisi eklenmez (bkz. birlesim esigi).
  const std::vector<Point> pts = {{0, 0}, {100, 0}, {100, 100}};
  auto v = Tessellator::TessellateThickPolyline(
      pts, 1.0f, sdl_painter::LineCap::kRound);
  EXPECT_EQ(v.size(), 12u) << "Ince cizgide uc/birlesim eklenmemeli.";
}

// ─── Birleşim stili (LineJoin) ──────────────────────────────────────────────

TEST(LineJoin, BevelAddsExactlyOneTrianglePerInteriorCorner) {
  const std::vector<Point> pts = {{0, 0}, {100, 0}, {100, 100}};
  auto thin = Tessellator::TessellateThickPolyline(pts, 1.0f);  // birlesimsiz
  auto bevel = Tessellator::TessellateThickPolyline(
      pts, 8.0f, sdl_painter::LineCap::kButt, sdl_painter::LineJoin::kBevel);
  EXPECT_EQ(bevel.size(), thin.size() + 3u)
      << "Bevel, kose basina tam bir ucgen olmali.";
}

TEST(LineJoin, MiterAddsTwoTrianglesAndReachesFurtherThanBevel) {
  const std::vector<Point> pts = {{0, 0}, {100, 0}, {100, 100}};
  auto bevel = Tessellator::TessellateThickPolyline(
      pts, 8.0f, sdl_painter::LineCap::kButt, sdl_painter::LineJoin::kBevel);
  auto miter = Tessellator::TessellateThickPolyline(
      pts, 8.0f, sdl_painter::LineCap::kButt, sdl_painter::LineJoin::kMiter);

  EXPECT_EQ(miter.size(), bevel.size() + 3u)
      << "Miter iki ucgen (6 vertex), bevel bir ucgen (3 vertex).";

  // 90 derecelik donuste kose (100, 0), yarim kalinlik 4. Dis taraf (+x, -y)
  // caprazi; miter noktasi tam olarak (104, -4) olmali.
  //
  // Sinir kutusuna bakmak bu farki GOSTERMEZ: segment quad'lari zaten
  // x=104 ve y=-4'e ulasiyor. Bu yuzden noktanin kendisi aranir.
  auto contains = [](const std::vector<Vertex>& v, float x, float y) {
    for (const auto& p : v) {
      if (std::fabs(p.x - x) < 1e-4f && std::fabs(p.y - y) < 1e-4f) {
        return true;
      }
    }
    return false;
  };

  EXPECT_TRUE(contains(miter, 104.0f, -4.0f))
      << "Miter noktasi (104, -4) uretilmedi.";
  EXPECT_FALSE(contains(bevel, 104.0f, -4.0f))
      << "Bevel, miter noktasini uretmemeli — kose kesik kalmali.";
}

TEST(LineJoin, MiterFallsBackToBevelOnVerySharpAngle) {
  // Geri donen (neredeyse 180 derece) kose: miter noktasi cok uzaga kacar,
  // miter siniri devreye girip bevel uretilmeli.
  const std::vector<Point> pts = {{0, 0}, {100, 0}, {2, 1}};
  auto bevel = Tessellator::TessellateThickPolyline(
      pts, 8.0f, sdl_painter::LineCap::kButt, sdl_painter::LineJoin::kBevel);
  auto miter = Tessellator::TessellateThickPolyline(
      pts, 8.0f, sdl_painter::LineCap::kButt, sdl_painter::LineJoin::kMiter);
  EXPECT_EQ(miter.size(), bevel.size())
      << "Miter siniri asilmasina ragmen sivri uc (spike) uretildi.";
}

TEST(LineJoin, RoundIsTheDefaultSoExistingOutputIsUnchanged) {
  const std::vector<Point> pts = {{0, 0}, {100, 0}, {100, 100}, {200, 100}};
  auto implicit = Tessellator::TessellateThickPolyline(pts, 8.0f);
  auto explicit_round = Tessellator::TessellateThickPolyline(
      pts, 8.0f, sdl_painter::LineCap::kButt, sdl_painter::LineJoin::kRound);
  ASSERT_EQ(implicit.size(), explicit_round.size());
  for (std::size_t i = 0; i < implicit.size(); ++i) {
    EXPECT_FLOAT_EQ(implicit[i].x, explicit_round[i].x) << "i=" << i;
    EXPECT_FLOAT_EQ(implicit[i].y, explicit_round[i].y) << "i=" << i;
  }
}

TEST(LineJoin, ClosedPolygonAppliesJoinToEveryCorner) {
  const std::vector<Point> pts = {{0, 0}, {100, 0}, {100, 100}, {0, 100}};
  auto open_line = Tessellator::TessellateThickPolyline(
      pts, 8.0f, sdl_painter::LineCap::kButt, sdl_painter::LineJoin::kBevel);
  auto closed = Tessellator::TessellateStrokedPolygon(
      pts, 8.0f, sdl_painter::LineJoin::kBevel);
  // Kapali: 4 segment + 4 birlesim. Acik: 3 segment + 2 birlesim.
  EXPECT_EQ(closed.size(), 4u * 6u + 4u * 3u);
  EXPECT_EQ(open_line.size(), 3u * 6u + 2u * 3u);
}

TEST(LineJoin, CollinearCornerProducesNoJoinGeometry) {
  // Duz devam eden kosede bosluk yoktur; bevel/miter ucgen eklememeli.
  const std::vector<Point> pts = {{0, 0}, {50, 0}, {100, 0}};
  auto v = Tessellator::TessellateThickPolyline(
      pts, 8.0f, sdl_painter::LineCap::kButt, sdl_painter::LineJoin::kBevel);
  EXPECT_EQ(v.size(), 12u) << "Duz kosede gereksiz birlesim ucgeni uretildi.";
}

// ─── Kesikli çizgi (dash) ───────────────────────────────────────────────────

namespace {

/// @brief Kesikli çizginin toplam **çizili** uzunluğu.
///
/// Her parça kalın bir quad olarak üretildiği için doğrudan uzunluk ölçmek
/// yerine üretilen alanı kalınlığa bölmek gerekir; bunun yerine daha basit ve
/// daha kararlı bir ölçüt kullanılır: üretilen vertex sayısı parça sayısıyla
/// orantılıdır. Parça sayısını saymak için tessellator yerine desen
/// yürüyüşünün gözlenebilir sonucu olan quad sayısı kullanılır.
std::size_t QuadCount(const std::vector<Vertex>& v) {
  return v.size() / 6;
}

}  // namespace

TEST(DashedPolyline, EmptyPatternFallsBackToSolid) {
  const std::vector<Point> pts = {{0, 0}, {100, 0}};
  auto solid = Tessellator::TessellateThickPolyline(pts, 4.0f);
  auto dashed = Tessellator::TessellateDashedPolyline(pts, 4.0f, nullptr, 0,
                                                      /*closed=*/false);
  EXPECT_EQ(dashed.size(), solid.size());
}

/// @brief REGRESYON: desende sıfır uzunluk olursa yürüme döngüsü ilerlemez.
///
/// Bu test, sonsuz döngüye girmediğini kanıtlar — düşerse takılır, geçerse
/// koruma çalışıyordur.
TEST(DashedPolyline, ZeroLengthInPatternFallsBackToSolidInsteadOfHanging) {
  const std::vector<Point> pts = {{0, 0}, {100, 0}};
  const float kPattern[] = {10.0f, 0.0f};
  auto solid = Tessellator::TessellateThickPolyline(pts, 4.0f);
  auto dashed = Tessellator::TessellateDashedPolyline(pts, 4.0f, kPattern, 2,
                                                      /*closed=*/false);
  EXPECT_EQ(dashed.size(), solid.size());
}

TEST(DashedPolyline, SplitsLineIntoExpectedNumberOfPieces) {
  // 100 piksellik yatay cizgi, 10 ciz / 10 atla -> 5 tam cizili parca.
  const std::vector<Point> pts = {{0, 0}, {100, 0}};
  const float kPattern[] = {10.0f, 10.0f};
  auto v = Tessellator::TessellateDashedPolyline(pts, 4.0f, kPattern, 2,
                                                 /*closed=*/false);
  EXPECT_EQ(QuadCount(v), 5u);
}

TEST(DashedPolyline, DrawnLengthMatchesPatternDutyCycle) {
  // 10 ciz / 30 atla -> yolun dortte biri cizili.
  const std::vector<Point> pts = {{0, 0}, {400, 0}};
  const float kPattern[] = {10.0f, 30.0f};
  auto v = Tessellator::TessellateDashedPolyline(pts, 4.0f, kPattern, 2,
                                                 /*closed=*/false);
  // 400 / 40 = 10 tam donem -> 10 cizili parca.
  EXPECT_EQ(QuadCount(v), 10u);
}

TEST(DashedPolyline, OddPatternAlternatesLikeSvg) {
  // Tek uzunluk: {10} -> 10 ciz, 10 atla (desen iki tur boyunca tersine doner).
  const std::vector<Point> pts = {{0, 0}, {100, 0}};
  const float kOdd[] = {10.0f};
  const float kEven[] = {10.0f, 10.0f};
  auto odd = Tessellator::TessellateDashedPolyline(pts, 4.0f, kOdd, 1,
                                                   /*closed=*/false);
  auto even = Tessellator::TessellateDashedPolyline(pts, 4.0f, kEven, 2,
                                                    /*closed=*/false);
  EXPECT_EQ(odd.size(), even.size())
      << "Tek uzunluklu desen, ayni uzunlugun iki kez yazilmisina denk olmali.";
}

/// @brief Desen köşede sıfırlanmamalı; yol boyunca sürekli ilerlemeli.
TEST(DashedPolyline, PatternContinuesAcrossCorners) {
  // Iki segment, her biri 100 piksel. Desen 10/10 ile toplam 200 piksel
  // uzerinde 10 cizili parca vermeli. Desen her kosede sifirlansaydi
  // segment basi 5 olmak uzere yine 10 cikardi; ayrimi gormek icin kosenin
  // ORTASINA denk gelen bir desen secilir: 30/30, segment uzunlugu 100.
  //
  // Surekli desende: 0-30 ciz, 30-60 atla, 60-90 ciz, 90-120 atla (kose 100'de,
  // yani bosluk kosenin uzerinden gecer), 120-150 ciz, 150-180 atla, 180-200 ciz
  // -> 4 cizili parca.
  // Sifirlanan desende her segment 0-30 ciz, 60-90 ciz -> segment basi 2,
  // toplam 4 ama parcalarin KONUMU farkli olurdu. Bu yuzden konum sinanir.
  const std::vector<Point> pts = {{0, 0}, {100, 0}, {200, 0}};
  const float kPattern[] = {30.0f, 30.0f};
  auto v = Tessellator::TessellateDashedPolyline(pts, 4.0f, kPattern, 2,
                                                 /*closed=*/false);
  ASSERT_FALSE(v.empty());

  // x = 100..110 araligi bosluga denk gelmeli (90-120 atlanan bolge).
  bool has_vertex_in_gap = false;
  for (const auto& p : v) {
    if (p.x > 101.0f && p.x < 119.0f) {
      has_vertex_in_gap = true;
    }
  }
  EXPECT_FALSE(has_vertex_in_gap)
      << "Desen kosede sifirlanmis: bosluga denk gelmesi gereken bolgede "
         "geometri var.";
}

TEST(DashedPolyline, ClosedShapeIsDashedAsOpenPieces) {
  // 100x100 kare -> cevre 400. 20/20 deseni -> 10 cizili parca.
  const std::vector<Point> pts = {
      {0, 0}, {100, 0}, {100, 100}, {0, 100}};
  const float kPattern[] = {20.0f, 20.0f};
  auto v = Tessellator::TessellateDashedPolyline(pts, 4.0f, kPattern, 2,
                                                 /*closed=*/true);
  EXPECT_EQ(QuadCount(v), 10u);
}

TEST(DashedPolyline, StrokedRectHonoursDash) {
  const float kPattern[] = {20.0f, 20.0f};
  auto solid = Tessellator::TessellateStrokedRect(0, 0, 100, 100, 4.0f);
  auto dashed = Tessellator::TessellateStrokedRect(
      0, 0, 100, 100, 4.0f, sdl_painter::LineJoin::kRound, kPattern, 2);
  EXPECT_NE(dashed.size(), solid.size());
  EXPECT_FALSE(dashed.empty());
}

TEST(DashedPolyline, CapAppliesToEachDashPiece) {
  const std::vector<Point> pts = {{0, 0}, {100, 0}};
  const float kPattern[] = {10.0f, 10.0f};
  auto butt = Tessellator::TessellateDashedPolyline(
      pts, 8.0f, kPattern, 2, /*closed=*/false, sdl_painter::LineCap::kButt);
  auto square = Tessellator::TessellateDashedPolyline(
      pts, 8.0f, kPattern, 2, /*closed=*/false, sdl_painter::LineCap::kSquare);
  // 5 parca * 2 uc * 6 vertex = 60 ek vertex.
  EXPECT_EQ(square.size(), butt.size() + 60u);
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
