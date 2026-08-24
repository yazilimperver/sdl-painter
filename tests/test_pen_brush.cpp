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

// --- Uç ve birleşim stili ---

/// @brief Varsayılanlar, stil seçeneği eklenmeden önceki davranışı korur:
///        uç yoktu (düz kesik), birleşim daima yuvarlaktı.
TEST(PenTest, DefaultCapIsButtAndDefaultJoinIsRound) {
  const Pen pen;
  EXPECT_EQ(pen.GetCapStyle(), LineCap::kButt);
  EXPECT_EQ(pen.GetJoinStyle(), LineJoin::kRound);
}

TEST(PenTest, CapAndJoinAreSettable) {
  Pen pen(Color::Red(), 4.0F);
  pen.SetCapStyle(LineCap::kRound);
  pen.SetJoinStyle(LineJoin::kMiter);
  EXPECT_EQ(pen.GetCapStyle(), LineCap::kRound);
  EXPECT_EQ(pen.GetJoinStyle(), LineJoin::kMiter);
}

/// @brief Eşitlik stilleri de kapsamalı; aksi halde `RenderState`
///        karşılaştırmaları stil değişimini kaçırır.
TEST(PenTest, EqualityCoversCapAndJoin) {
  Pen a(Color::Red(), 4.0F);
  Pen b(Color::Red(), 4.0F);
  ASSERT_EQ(a, b);

  b.SetCapStyle(LineCap::kSquare);
  EXPECT_NE(a, b);

  b.SetCapStyle(LineCap::kButt);
  ASSERT_EQ(a, b);
  b.SetJoinStyle(LineJoin::kBevel);
  EXPECT_NE(a, b);
}

// --- Kesikli çizgi deseni ---

TEST(PenTest, DefaultPenHasNoDash) {
  const Pen pen;
  EXPECT_FALSE(pen.HasDash());
  EXPECT_EQ(pen.GetDashCount(), 0u);
}

TEST(PenTest, DashPatternIsStoredInOrder) {
  Pen pen;
  pen.SetDashPattern({10.0F, 5.0F, 2.0F});
  ASSERT_EQ(pen.GetDashCount(), 3u);
  EXPECT_TRUE(pen.HasDash());
  EXPECT_FLOAT_EQ(pen.GetDashPattern()[0], 10.0F);
  EXPECT_FLOAT_EQ(pen.GetDashPattern()[1], 5.0F);
  EXPECT_FLOAT_EQ(pen.GetDashPattern()[2], 2.0F);
}

TEST(PenTest, ClearDashPatternDisablesDashing) {
  Pen pen;
  pen.SetDashPattern({10.0F, 5.0F});
  ASSERT_TRUE(pen.HasDash());
  pen.ClearDashPattern();
  EXPECT_FALSE(pen.HasDash());
}

/// @brief Pozitif olmayan uzunluk desene girmemeli: tessellator tarafında
///        sıfır uzunluk, desen yürüyüşünü hiç ilerletmez.
TEST(PenTest, NonPositiveDashLengthsAreRejected) {
  Pen pen;
  pen.SetDashPattern({10.0F, 0.0F, -3.0F, 5.0F});
  ASSERT_EQ(pen.GetDashCount(), 2u);
  EXPECT_FLOAT_EQ(pen.GetDashPattern()[0], 10.0F);
  EXPECT_FLOAT_EQ(pen.GetDashPattern()[1], 5.0F);
}

TEST(PenTest, DashPatternIsCappedAtMaxSegments) {
  Pen pen;
  pen.SetDashPattern({1, 2, 3, 4, 5, 6, 7, 8, 9, 10});
  EXPECT_EQ(pen.GetDashCount(), sdl_painter::kMaxDashSegments);
}

TEST(PenTest, SettingDashPatternReplacesThePreviousOne) {
  Pen pen;
  pen.SetDashPattern({1.0F, 2.0F, 3.0F});
  pen.SetDashPattern({9.0F});
  ASSERT_EQ(pen.GetDashCount(), 1u);
  EXPECT_FLOAT_EQ(pen.GetDashPattern()[0], 9.0F);
}

TEST(PenTest, EqualityCoversDashPattern) {
  Pen a(Color::Red(), 2.0F);
  Pen b(Color::Red(), 2.0F);
  ASSERT_EQ(a, b);

  b.SetDashPattern({4.0F, 4.0F});
  EXPECT_NE(a, b);

  a.SetDashPattern({4.0F, 4.0F});
  EXPECT_EQ(a, b);

  b.SetDashPattern({4.0F, 8.0F});
  EXPECT_NE(a, b) << "Ayni uzunluktaki farkli desenler esit sayildi.";
}

}  // namespace sdl_painter
