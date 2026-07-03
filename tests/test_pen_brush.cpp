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

TEST(PenTest, DefaultHasNoOutline) {
  Pen pen;
  EXPECT_FALSE(pen.HasOutline());
}

TEST(PenTest, OutlineConstructor) {
  Pen pen(Color::Red(), 2.0f, Color::Black(), 1.5f);
  EXPECT_EQ(pen.GetColor(), Color::Red());
  EXPECT_FLOAT_EQ(pen.GetWidth(), 2.0f);
  EXPECT_EQ(pen.GetOutlineColor(), Color::Black());
  EXPECT_FLOAT_EQ(pen.GetOutlineWidth(), 1.5f);
  EXPECT_TRUE(pen.HasOutline());
}

TEST(PenTest, SetOutlineColorAndWidth) {
  Pen pen;
  pen.SetOutlineColor(Color::Black());
  pen.SetOutlineWidth(1.0f);
  EXPECT_EQ(pen.GetOutlineColor(), Color::Black());
  EXPECT_FLOAT_EQ(pen.GetOutlineWidth(), 1.0f);
  EXPECT_TRUE(pen.HasOutline());
}

TEST(PenTest, HasOutlineFalseWhenOutlineWidthZero) {
  Pen pen(Color::Red(), 2.0f, Color::Black(), 0.0f);
  EXPECT_FALSE(pen.HasOutline());
}

TEST(PenTest, HasOutlineFalseWhenOutlineColorTransparent) {
  Pen pen(Color::Red(), 2.0f, Color::Transparent(), 1.0f);
  EXPECT_FALSE(pen.HasOutline());
}

TEST(PenTest, EqualityWithOutline) {
  Pen a(Color::Red(), 2.0f, Color::Black(), 1.0f);
  Pen b(Color::Red(), 2.0f, Color::Black(), 1.0f);
  Pen c(Color::Red(), 2.0f, Color::Black(), 2.0f);
  Pen d(Color::Red(), 2.0f);
  EXPECT_EQ(a, b);
  EXPECT_NE(a, c);
  EXPECT_NE(a, d);
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
