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
