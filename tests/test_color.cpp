#include <gtest/gtest.h>

#include "sdl_painter/color.h"

namespace sdl_painter {

TEST(ColorTest, DefaultConstructorIsBlackOpaque) {
  Color c;
  EXPECT_EQ(c.r, 0);
  EXPECT_EQ(c.g, 0);
  EXPECT_EQ(c.b, 0);
  EXPECT_EQ(c.a, 255);
}

TEST(ColorTest, ParameterizedConstructor) {
  Color c(100, 150, 200, 128);
  EXPECT_EQ(c.r, 100);
  EXPECT_EQ(c.g, 150);
  EXPECT_EQ(c.b, 200);
  EXPECT_EQ(c.a, 128);
}

TEST(ColorTest, DefaultAlphaIsOpaque) {
  Color c(255, 0, 0);
  EXPECT_EQ(c.a, 255);
}

TEST(ColorTest, NormalizedFloatValues) {
  Color c(255, 0, 128, 255);
  EXPECT_FLOAT_EQ(c.RedF(), 1.0f);
  EXPECT_FLOAT_EQ(c.GreenF(), 0.0f);
  EXPECT_NEAR(c.BlueF(), 128.0f / 255.0f, 1e-5f);
  EXPECT_FLOAT_EQ(c.AlphaF(), 1.0f);
}

TEST(ColorTest, EqualityOperator) {
  Color a(10, 20, 30, 40);
  Color b(10, 20, 30, 40);
  Color c(10, 20, 30, 41);
  EXPECT_EQ(a, b);
  EXPECT_NE(a, c);
}

TEST(ColorTest, NamedColors) {
  EXPECT_EQ(Color::Black(), Color(0, 0, 0, 255));
  EXPECT_EQ(Color::White(), Color(255, 255, 255, 255));
  EXPECT_EQ(Color::Red(), Color(255, 0, 0, 255));
  EXPECT_EQ(Color::Green(), Color(0, 255, 0, 255));
  EXPECT_EQ(Color::Blue(), Color(0, 0, 255, 255));
  EXPECT_EQ(Color::Transparent().a, 0);
}

}  // namespace sdl_painter
