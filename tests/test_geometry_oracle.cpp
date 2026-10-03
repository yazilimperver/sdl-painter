/// @file test_geometry_oracle.cpp
///
/// @brief geometry_oracle.h ölçütlerinin kendi testleri. Ölçüt yanlışsa ona
///        dayanan regresyon testleri de yanlış sonuç verir.

#include "sdl_painter/geometry.h"
#include "sdl_painter/vertex.h"

#include <gtest/gtest.h>
#include <vector>

#include "geometry_oracle.h"

using sdl_painter::Point;
using sdl_painter::Vertex;
using sdl_painter::testing::CoverageCount;
using sdl_painter::testing::ShoelaceArea;
using sdl_painter::testing::TriangleListArea;

namespace {

// 2x2 kare, (0,0)-(2,2) köşegeniyle iki üçgene bölünmüş.
const std::vector<Vertex> kSquare = {
    {0.0F, 0.0F}, {2.0F, 0.0F}, {2.0F, 2.0F},
    {0.0F, 0.0F}, {2.0F, 2.0F}, {0.0F, 2.0F},
};

// Karenin alt üçgeni ikinci kez eklenmiş hâli.
std::vector<Vertex> SquareWithRepeatedTriangle() {
  std::vector<Vertex> v = kSquare;
  v.insert(v.end(), kSquare.begin(), kSquare.begin() + 3);
  return v;
}

}  // namespace

TEST(GeometryOracle, TriangleListAreaSumsTriangles) {
  EXPECT_DOUBLE_EQ(TriangleListArea(kSquare), 4.0);
}

TEST(GeometryOracle, OverlappingTrianglesInflateArea) {
  EXPECT_DOUBLE_EQ(TriangleListArea(SquareWithRepeatedTriangle()), 6.0);
}

TEST(GeometryOracle, ShoelaceAreaIgnoresWinding) {
  const std::vector<Point> l_shape = {
      {0.0F, 0.0F}, {2.0F, 0.0F}, {2.0F, 1.0F},
      {1.0F, 1.0F}, {1.0F, 2.0F}, {0.0F, 2.0F},
  };
  const std::vector<Point> reversed(l_shape.rbegin(), l_shape.rend());
  EXPECT_DOUBLE_EQ(ShoelaceArea(l_shape), 3.0);
  EXPECT_DOUBLE_EQ(ShoelaceArea(reversed), 3.0);
}

TEST(GeometryOracle, CoverageCountsContainingTriangles) {
  EXPECT_EQ(CoverageCount(kSquare, 1.5F, 0.5F), 1);
  EXPECT_EQ(CoverageCount(kSquare, 0.5F, 1.5F), 1);
  EXPECT_EQ(CoverageCount(kSquare, 3.0F, 1.0F), 0);
  EXPECT_EQ(CoverageCount(SquareWithRepeatedTriangle(), 1.5F, 0.5F), 2);
}

TEST(GeometryOracle, CoverageDoesNotDependOnWinding) {
  const std::vector<Vertex> ccw = {{0.0F, 0.0F}, {2.0F, 0.0F}, {0.0F, 2.0F}};
  const std::vector<Vertex> cw = {{0.0F, 0.0F}, {0.0F, 2.0F}, {2.0F, 0.0F}};
  EXPECT_EQ(CoverageCount(ccw, 0.5F, 0.5F), 1);
  EXPECT_EQ(CoverageCount(cw, 0.5F, 0.5F), 1);
}

TEST(GeometryOracle, CoverageIgnoresDegenerateTriangles) {
  const std::vector<Vertex> sliver = {{0.0F, 0.0F}, {1.0F, 1.0F}, {2.0F, 2.0F}};
  EXPECT_EQ(CoverageCount(sliver, 1.0F, 1.0F), 0);
}

// Kenar semantiği: geometry_oracle.h belgesine göre kenardaki nokta içeride,
// iki üçgenin ortak kenarındaki nokta iki kez sayılır. Koordinatlar ikinin
// kuvvetleri ve tamsayı olduğundan Orientation tam sıfır döner.

TEST(GeometryOracle, CoverageCountsPointOnEdgeAsInside) {
  const std::vector<Vertex> triangle = {
      {0.0F, 0.0F}, {2.0F, 0.0F}, {0.0F, 2.0F}};
  EXPECT_EQ(CoverageCount(triangle, 1.0F, 0.0F), 1);  // alt kenar
  EXPECT_EQ(CoverageCount(triangle, 1.0F, 1.0F), 1);  // hipotenüs
  EXPECT_EQ(CoverageCount(triangle, 0.0F, 0.0F), 1);  // köşe
}

TEST(GeometryOracle, CoverageCountsPointOnSharedEdgeTwice) {
  // kSquare'in iki üçgeni (0,0)-(2,2) köşegenini paylaşır.
  EXPECT_EQ(CoverageCount(kSquare, 1.0F, 1.0F), 2);
}

// Eksik sondaki üçgen: vertex sayısı 3'ün katı değilse yok sayılır.

TEST(GeometryOracle, TriangleListAreaIgnoresTrailingIncompleteTriangle) {
  std::vector<Vertex> one_extra = kSquare;
  one_extra.push_back({10.0F, 10.0F});
  std::vector<Vertex> two_extra = kSquare;
  two_extra.push_back({10.0F, 10.0F});
  two_extra.push_back({20.0F, 0.0F});

  EXPECT_DOUBLE_EQ(TriangleListArea(one_extra), 4.0);
  EXPECT_DOUBLE_EQ(TriangleListArea(two_extra), 4.0);
}

// Vertex'ler üçerli gruplanır, kayan pencere kullanılmaz: (b, c, d) üçlüsü
// P'yi kapsardı, ama d eksik grubun parçası.
TEST(GeometryOracle, CoverageCountGroupsVerticesInThreesNotSlidingWindows) {
  const std::vector<Vertex> list = {
      {0.0F, 0.0F}, {1.0F, 0.0F}, {0.0F, 1.0F}, {4.0F, 0.0F}};
  EXPECT_EQ(CoverageCount(list, 1.5F, 0.2F), 0);
  EXPECT_EQ(CoverageCount(list, 0.2F, 0.2F), 1);
  EXPECT_DOUBLE_EQ(TriangleListArea(list), 0.5);
}

TEST(GeometryOracle, EmptyInputHasNoAreaAndNoCoverage) {
  const std::vector<Vertex> no_vertices;
  const std::vector<Point> no_points;
  EXPECT_DOUBLE_EQ(TriangleListArea(no_vertices), 0.0);
  EXPECT_EQ(CoverageCount(no_vertices, 0.0F, 0.0F), 0);
  EXPECT_DOUBLE_EQ(ShoelaceArea(no_points), 0.0);
}

TEST(GeometryOracle, FewerThanThreeVerticesHaveNoAreaAndNoCoverage) {
  const std::vector<Vertex> two = {{0.0F, 0.0F}, {2.0F, 0.0F}};
  EXPECT_DOUBLE_EQ(TriangleListArea(two), 0.0);
  EXPECT_EQ(CoverageCount(two, 1.0F, 0.0F), 0);
}

TEST(GeometryOracle, ShoelaceAreaIsZeroForFewerThanThreePoints) {
  const std::vector<Point> one = {{1.0F, 1.0F}};
  const std::vector<Point> two = {{0.0F, 0.0F}, {2.0F, 3.0F}};
  EXPECT_DOUBLE_EQ(ShoelaceArea(one), 0.0);
  EXPECT_DOUBLE_EQ(ShoelaceArea(two), 0.0);
}

// Basit poligon ön koşulu: kelebek biçimli poligonun iki bölgesi zıt yönlü
// olduğundan birbirini götürür; gerçek kaplanan alan 2'dir, ölçüt 0 verir.
TEST(GeometryOracle, ShoelaceAreaCancelsOutForSelfIntersectingPolygon) {
  const std::vector<Point> bow_tie = {
      {0.0F, 0.0F}, {2.0F, 2.0F}, {2.0F, 0.0F}, {0.0F, 2.0F}};
  EXPECT_DOUBLE_EQ(ShoelaceArea(bow_tie), 0.0);
}

TEST(GeometryOracle, TriangleListAreaIgnoresWindingOfEachTriangle) {
  // Karışık sarım: ilk üçgen saat yönünde, ikincisi ters yönde. İşaretli
  // toplam 0 olduğundan, mutlak değer toplamın dışına alınırsa test düşer.
  const std::vector<Vertex> mixed = {
      {0.0F, 0.0F}, {2.0F, 2.0F}, {2.0F, 0.0F},
      {0.0F, 0.0F}, {2.0F, 2.0F}, {0.0F, 2.0F},
  };
  EXPECT_DOUBLE_EQ(TriangleListArea(mixed), 4.0);
}
