#include <gtest/gtest.h>

#include "sdl_painter/geometry.h"

namespace sdl_painter {

// --- Point ---

TEST(PointTest, DefaultConstructor) {
  Point p;
  EXPECT_FLOAT_EQ(p.x, 0.0f);
  EXPECT_FLOAT_EQ(p.y, 0.0f);
}

TEST(PointTest, ParameterizedConstructor) {
  Point p(3.0f, 4.0f);
  EXPECT_FLOAT_EQ(p.x, 3.0f);
  EXPECT_FLOAT_EQ(p.y, 4.0f);
}

TEST(PointTest, Addition) {
  Point a(1.0f, 2.0f);
  Point b(3.0f, 4.0f);
  Point c = a + b;
  EXPECT_FLOAT_EQ(c.x, 4.0f);
  EXPECT_FLOAT_EQ(c.y, 6.0f);
}

TEST(PointTest, Subtraction) {
  Point a(5.0f, 7.0f);
  Point b(2.0f, 3.0f);
  Point c = a - b;
  EXPECT_FLOAT_EQ(c.x, 3.0f);
  EXPECT_FLOAT_EQ(c.y, 4.0f);
}

TEST(PointTest, ScalarMultiplication) {
  Point p(2.0f, 3.0f);
  Point scaled = p * 2.5f;
  EXPECT_FLOAT_EQ(scaled.x, 5.0f);
  EXPECT_FLOAT_EQ(scaled.y, 7.5f);
}

TEST(PointTest, Equality) {
  Point a(1.0f, 2.0f);
  Point b(1.0f, 2.0f);
  Point c(1.0f, 3.0f);
  EXPECT_EQ(a, b);
  EXPECT_NE(a, c);
}

// --- Rect ---

TEST(RectTest, DefaultConstructor) {
  Rect r;
  EXPECT_FLOAT_EQ(r.x, 0.0f);
  EXPECT_FLOAT_EQ(r.y, 0.0f);
  EXPECT_FLOAT_EQ(r.w, 0.0f);
  EXPECT_FLOAT_EQ(r.h, 0.0f);
}

TEST(RectTest, ParameterizedConstructor) {
  Rect r(10.0f, 20.0f, 100.0f, 50.0f);
  EXPECT_FLOAT_EQ(r.x, 10.0f);
  EXPECT_FLOAT_EQ(r.y, 20.0f);
  EXPECT_FLOAT_EQ(r.w, 100.0f);
  EXPECT_FLOAT_EQ(r.h, 50.0f);
}

TEST(RectTest, RightAndBottom) {
  Rect r(10.0f, 20.0f, 100.0f, 50.0f);
  EXPECT_FLOAT_EQ(r.Right(), 110.0f);
  EXPECT_FLOAT_EQ(r.Bottom(), 70.0f);
}

TEST(RectTest, Center) {
  Rect r(0.0f, 0.0f, 100.0f, 80.0f);
  Point c = r.Center();
  EXPECT_FLOAT_EQ(c.x, 50.0f);
  EXPECT_FLOAT_EQ(c.y, 40.0f);
}

TEST(RectTest, Contains) {
  Rect r(10.0f, 10.0f, 80.0f, 60.0f);
  EXPECT_TRUE(r.Contains(50.0f, 40.0f));   // merkez
  EXPECT_TRUE(r.Contains(10.0f, 10.0f));   // sol-üst köşe
  EXPECT_FALSE(r.Contains(9.9f, 10.0f));   // sol dışında
  EXPECT_FALSE(r.Contains(91.0f, 10.0f));  // sağ dışında
}

TEST(RectTest, Equality) {
  Rect a(1.0f, 2.0f, 3.0f, 4.0f);
  Rect b(1.0f, 2.0f, 3.0f, 4.0f);
  Rect c(1.0f, 2.0f, 3.0f, 5.0f);
  EXPECT_EQ(a, b);
  EXPECT_NE(a, c);
}

// --- Size ---

TEST(SizeTest, DefaultConstructor) {
  Size s;
  EXPECT_FLOAT_EQ(s.w, 0.0f);
  EXPECT_FLOAT_EQ(s.h, 0.0f);
}

TEST(SizeTest, ParameterizedConstructor) {
  Size s(640.0f, 480.0f);
  EXPECT_FLOAT_EQ(s.w, 640.0f);
  EXPECT_FLOAT_EQ(s.h, 480.0f);
}

}  // namespace sdl_painter
