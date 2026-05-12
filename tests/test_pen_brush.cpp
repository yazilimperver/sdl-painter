#include <gtest/gtest.h>

#include "sdl_painter/brush.h"
#include "sdl_painter/pen.h"

namespace sdl_painter {

// --- Pen ---

TEST(PenTest, DefaultConstructor) {
  Pen pen;
  EXPECT_EQ(pen.GetColor(), Color::Black());
  EXPECT_FLOAT_EQ(pen.GetWidth(), 1.0f);
  EXPECT_TRUE(pen.IsVisible());
}

TEST(PenTest, ParameterizedConstructor) {
  Color red(255, 0, 0);
  Pen pen(red, 3.5f);
  EXPECT_EQ(pen.GetColor(), red);
  EXPECT_FLOAT_EQ(pen.GetWidth(), 3.5f);
}

TEST(PenTest, SetColor) {
  Pen pen;
  Color blue = Color::Blue();
  pen.SetColor(blue);
  EXPECT_EQ(pen.GetColor(), blue);
}

TEST(PenTest, SetWidth) {
  Pen pen;
  pen.SetWidth(5.0f);
  EXPECT_FLOAT_EQ(pen.GetWidth(), 5.0f);
}

TEST(PenTest, NoPenIsInvisible) {
  Pen pen = Pen::NoPen();
  EXPECT_FALSE(pen.IsVisible());
}

TEST(PenTest, ZeroWidthIsInvisible) {
  Pen pen(Color::Red(), 0.0f);
  EXPECT_FALSE(pen.IsVisible());
}

TEST(PenTest, TransparentColorIsInvisible) {
  Pen pen(Color::Transparent(), 2.0f);
  EXPECT_FALSE(pen.IsVisible());
}

TEST(PenTest, Equality) {
  Pen a(Color::Red(), 2.0f);
  Pen b(Color::Red(), 2.0f);
  Pen c(Color::Blue(), 2.0f);
  EXPECT_EQ(a, b);
  EXPECT_NE(a, c);
}

// --- Brush ---

TEST(BrushTest, DefaultConstructor) {
  Brush brush;
  EXPECT_EQ(brush.GetColor(), Color::Black());
  EXPECT_TRUE(brush.IsVisible());
}

TEST(BrushTest, ParameterizedConstructor) {
  Color green = Color::Green();
  Brush brush(green);
  EXPECT_EQ(brush.GetColor(), green);
}

TEST(BrushTest, SetColor) {
  Brush brush;
  Color white = Color::White();
  brush.SetColor(white);
  EXPECT_EQ(brush.GetColor(), white);
}

TEST(BrushTest, NoBrushIsInvisible) {
  Brush brush = Brush::NoBrush();
  EXPECT_FALSE(brush.IsVisible());
}

TEST(BrushTest, TransparentIsInvisible) {
  Brush brush(Color::Transparent());
  EXPECT_FALSE(brush.IsVisible());
}

TEST(BrushTest, Equality) {
  Brush a(Color::Red());
  Brush b(Color::Red());
  Brush c(Color::Blue());
  EXPECT_EQ(a, b);
  EXPECT_NE(a, c);
}

}  // namespace sdl_painter
