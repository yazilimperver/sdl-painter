#include "sdl_painter/brush.h"
#include "sdl_painter/pen.h"

#include <array>
#include <gtest/gtest.h>
#include <type_traits>

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

// --- Gradient fırça ---

TEST(BrushTest, DefaultBrushIsSolid) {
  const Brush brush;
  EXPECT_EQ(brush.GetType(), BrushType::kSolid);
  EXPECT_FALSE(brush.IsGradient());
}

TEST(BrushTest, NoBrushIsNotAGradient) {
  const Brush brush = Brush::NoBrush();
  EXPECT_EQ(brush.GetType(), BrushType::kSolid);
  EXPECT_FALSE(brush.IsGradient());
}

TEST(BrushTest, LinearGradientStoresItsParameters) {
  const Brush brush = Brush::LinearGradient({1.0F, 2.0F}, {30.0F, 40.0F},
                                            Color::Red(), Color::Blue());
  EXPECT_EQ(brush.GetType(), BrushType::kLinear);
  EXPECT_TRUE(brush.IsGradient());
  EXPECT_EQ(brush.GetColor(), Color::Red());
  EXPECT_EQ(brush.GetColor2(), Color::Blue());
  EXPECT_EQ(brush.GetStart(), Point(1.0F, 2.0F));
  EXPECT_EQ(brush.GetEnd(), Point(30.0F, 40.0F));
  EXPECT_FLOAT_EQ(brush.GetRadius(), 0.0F);
}

TEST(BrushTest, RadialGradientStoresItsParameters) {
  const Brush brush = Brush::RadialGradient({5.0F, 6.0F}, 25.0F, Color::Green(),
                                            Color::White());
  EXPECT_EQ(brush.GetType(), BrushType::kRadial);
  EXPECT_TRUE(brush.IsGradient());
  EXPECT_EQ(brush.GetColor(), Color::Green());
  EXPECT_EQ(brush.GetColor2(), Color::White());
  EXPECT_EQ(brush.GetStart(), Point(5.0F, 6.0F));
  EXPECT_FLOAT_EQ(brush.GetRadius(), 25.0F);
}

TEST(BrushTest, GradientEqualityCoversSecondColor) {
  const Brush a = Brush::LinearGradient({0.0F, 0.0F}, {10.0F, 0.0F},
                                        Color::Red(), Color::Blue());
  const Brush b = Brush::LinearGradient({0.0F, 0.0F}, {10.0F, 0.0F},
                                        Color::Red(), Color::Green());
  EXPECT_NE(a, b);
}

TEST(BrushTest, GradientEqualityCoversStartPoint) {
  const Brush a = Brush::LinearGradient({0.0F, 0.0F}, {10.0F, 0.0F},
                                        Color::Red(), Color::Blue());
  const Brush b = Brush::LinearGradient({1.0F, 0.0F}, {10.0F, 0.0F},
                                        Color::Red(), Color::Blue());
  const Brush c = Brush::LinearGradient({0.0F, 1.0F}, {10.0F, 0.0F},
                                        Color::Red(), Color::Blue());
  EXPECT_NE(a, b);
  EXPECT_NE(a, c);
}

TEST(BrushTest, GradientEqualityCoversEndPoint) {
  const Brush a = Brush::LinearGradient({0.0F, 0.0F}, {10.0F, 0.0F},
                                        Color::Red(), Color::Blue());
  const Brush b = Brush::LinearGradient({0.0F, 0.0F}, {11.0F, 0.0F},
                                        Color::Red(), Color::Blue());
  const Brush c = Brush::LinearGradient({0.0F, 0.0F}, {10.0F, 1.0F},
                                        Color::Red(), Color::Blue());
  EXPECT_NE(a, b);
  EXPECT_NE(a, c);
}

TEST(BrushTest, GradientEqualityCoversRadius) {
  const Brush a =
      Brush::RadialGradient({0.0F, 0.0F}, 10.0F, Color::Red(), Color::Blue());
  const Brush b =
      Brush::RadialGradient({0.0F, 0.0F}, 20.0F, Color::Red(), Color::Blue());
  EXPECT_NE(a, b);
}

/// @brief Başlangıç noktası, bitiş noktası ve yarıçap aynı; yalnızca tür
///        farklı. Eşitlik türü karşılaştırmazsa bu iki fırça eşit sayılırdı.
TEST(BrushTest, GradientEqualityCoversType) {
  const Brush linear = Brush::LinearGradient({0.0F, 0.0F}, {0.0F, 0.0F},
                                             Color::Red(), Color::Blue());
  const Brush radial =
      Brush::RadialGradient({0.0F, 0.0F}, 0.0F, Color::Red(), Color::Blue());
  EXPECT_NE(linear, radial);
}

/// @brief Düz fırça, aynı ilk renge sahip gradientten her iki yönde de farklı.
TEST(BrushTest, SolidIsNotEqualToGradientWithTheSameFirstColor) {
  const Brush solid(Color::Red());
  const Brush gradient =
      Brush::RadialGradient({0.0F, 0.0F}, 10.0F, Color::Red(), Color::Blue());
  EXPECT_NE(solid, gradient);
  EXPECT_NE(gradient, solid);
}

TEST(BrushTest, OpaqueToTransparentGradientIsVisible) {
  const Brush gradient = Brush::LinearGradient(
      {0.0F, 0.0F}, {10.0F, 0.0F}, Color::Red(), Color(0, 0, 255, 0));
  EXPECT_TRUE(gradient.IsVisible());
}

/// @brief Her iki ucun alfası sıfır: RGB değerleri ne olursa olsun görünmez.
TEST(BrushTest, GradientWithTransparentEndsIsInvisible) {
  const Color clear_red(255, 0, 0, 0);
  const Color clear_blue(0, 0, 255, 0);
  const Brush linear =
      Brush::LinearGradient({0.0F, 0.0F}, {10.0F, 0.0F}, clear_red, clear_blue);
  const Brush radial =
      Brush::RadialGradient({0.0F, 0.0F}, 10.0F, clear_red, clear_blue);
  EXPECT_FALSE(linear.IsVisible());
  EXPECT_FALSE(radial.IsVisible());
}

TEST(BrushTest, SetColorOnGradientChangesOnlyTheStartColor) {
  Brush brush = Brush::LinearGradient({1.0F, 2.0F}, {30.0F, 40.0F},
                                      Color::Red(), Color::Blue());
  brush.SetColor(Color::Green());
  EXPECT_EQ(brush.GetColor(), Color::Green());
  EXPECT_EQ(brush.GetColor2(), Color::Blue());
  EXPECT_EQ(brush.GetStart(), Point(1.0F, 2.0F));
  EXPECT_EQ(brush.GetEnd(), Point(30.0F, 40.0F));
}

TEST(BrushTest, SetColorKeepsTheGradientType) {
  Brush linear = Brush::LinearGradient({0.0F, 0.0F}, {10.0F, 0.0F},
                                       Color::Red(), Color::Blue());
  Brush radial =
      Brush::RadialGradient({0.0F, 0.0F}, 10.0F, Color::Red(), Color::Blue());
  linear.SetColor(Color::Green());
  radial.SetColor(Color::Green());
  EXPECT_EQ(linear.GetType(), BrushType::kLinear);
  EXPECT_EQ(radial.GetType(), BrushType::kRadial);
  EXPECT_TRUE(linear.IsGradient());
  EXPECT_TRUE(radial.IsGradient());
}

TEST(BrushGradient, TransparentToOpaqueIsStillVisible) {
  const Brush g = Brush::LinearGradient({0.0F, 0.0F}, {10.0F, 0.0F},
                                        Color::Transparent(), Color::Red());
  EXPECT_TRUE(g.IsVisible())
      << "Saydamdan opaga giden gradient gorunmez sayildi.";
}

TEST(BrushGradient, EqualityCoversGradientParameters) {
  const Brush a = Brush::LinearGradient({0.0F, 0.0F}, {10.0F, 0.0F},
                                        Color::Red(), Color::Blue());
  const Brush b = Brush::LinearGradient({0.0F, 0.0F}, {10.0F, 0.0F},
                                        Color::Red(), Color::Blue());
  const Brush c = Brush::LinearGradient({0.0F, 0.0F}, {20.0F, 0.0F},
                                        Color::Red(), Color::Blue());
  const Brush solid(Color::Red());
  EXPECT_EQ(a, b);
  EXPECT_NE(a, c);
  EXPECT_NE(a, solid);
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

// --- Kalem sınır durumları ---

TEST(PenTest, IsTriviallyCopyable) {
  // Desen sabit dizide tutulur; Save/Restore kopyaları yığın tahsisi yapmaz.
  static_assert(std::is_trivially_copyable<Pen>::value,
                "Pen kopyasi yigin tahsisi icermemeli");
  SUCCEED();
}

// Negatif kalınlık davranışı bugün üç yolda tutarsız: SetWidth 0'a kırpar,
// kurucu ve SetOutlineWidth kırpmaz. Aşağıdaki üç test mevcut davranışı
// belgeler; tutarlı hale getirmek ayrı bir üretim değişikliğidir.

TEST(PenTest, SetWidthClampsNegativeToZero) {
  Pen pen;
  pen.SetWidth(-2.0F);
  EXPECT_FLOAT_EQ(pen.GetWidth(), 0.0F);
  EXPECT_FALSE(pen.IsVisible());
}

TEST(PenTest, ConstructorDoesNotClampNegativeWidth) {
  const Pen pen(Color::Red(), -3.0F);
  EXPECT_FLOAT_EQ(pen.GetWidth(), -3.0F);
  EXPECT_FALSE(pen.IsVisible());
}

TEST(PenTest, OutlineConstructorDoesNotClampNegativeWidths) {
  const Pen pen(Color::Red(), -3.0F, Color::Black(), -2.0F);
  EXPECT_FLOAT_EQ(pen.GetWidth(), -3.0F);
  EXPECT_FLOAT_EQ(pen.GetOutlineWidth(), -2.0F);
  EXPECT_FALSE(pen.IsVisible());
  EXPECT_FALSE(pen.HasOutline());
}

TEST(PenTest, SetOutlineWidthDoesNotClampNegative) {
  Pen pen;
  pen.SetOutlineColor(Color::Black());
  pen.SetOutlineWidth(-1.5F);
  EXPECT_FLOAT_EQ(pen.GetOutlineWidth(), -1.5F);
  EXPECT_FALSE(pen.HasOutline());
}

TEST(PenTest, EqualityCoversWidth) {
  const Pen a(Color::Red(), 2.0F);
  const Pen b(Color::Red(), 3.0F);
  EXPECT_NE(a, b);
}

TEST(PenTest, EqualityCoversOutlineColor) {
  const Pen a(Color::Red(), 2.0F, Color::Black(), 1.0F);
  const Pen b(Color::Red(), 2.0F, Color::Blue(), 1.0F);
  EXPECT_NE(a, b);
}

/// @brief Desen kısalınca dizinin sayılmayan kuyruğu eski değerde kalır:
///        `{1, 2, 3}` ardından `{9}` dizide `{9, 2, 3, ...}` bırakır. Eşitlik
///        yalnızca sayılan ilk GetDashCount() elemana bakmalı; ham diziyi
///        karşılaştıran bir eşitlik bu iki kalemi farklı bulurdu.
TEST(PenTest, EqualityIgnoresStaleDashData) {
  Pen shortened;
  shortened.SetDashPattern({1.0F, 2.0F, 3.0F});
  shortened.SetDashPattern({9.0F});
  Pen direct;
  direct.SetDashPattern({9.0F});
  EXPECT_EQ(shortened, direct);
}

/// @brief ClearDashPattern() yalnızca sayacı sıfırlar; dizide eski değerler
///        kalır. Yine de kalem hiç kesik görmemiş kalemle eşit olmalı.
TEST(PenTest, ClearedDashPatternEqualsDefaultPen) {
  Pen cleared;
  cleared.SetDashPattern({1.0F, 2.0F, 3.0F});
  cleared.ClearDashPattern();
  EXPECT_EQ(cleared, Pen());
}

TEST(PenTest, EmptyDashPatternDisablesDashing) {
  Pen pen;
  pen.SetDashPattern({10.0F, 5.0F});
  ASSERT_TRUE(pen.HasDash());
  pen.SetDashPattern({});
  EXPECT_FALSE(pen.HasDash());
  EXPECT_EQ(pen.GetDashCount(), 0u);
}

TEST(PenTest, AllNonPositiveDashPatternDisablesDashing) {
  Pen pen;
  pen.SetDashPattern({10.0F, 5.0F});
  ASSERT_TRUE(pen.HasDash());
  pen.SetDashPattern({0.0F, -1.0F, -4.0F});
  EXPECT_FALSE(pen.HasDash());
  EXPECT_EQ(pen.GetDashCount(), 0u);
}

TEST(PenTest, DashPatternAtTheLimitKeepsTheFirstValuesInOrder) {
  static_assert(sdl_painter::kMaxDashSegments == 8,
                "Beklenen dizi bu sinira gore yazildi");
  Pen pen;
  pen.SetDashPattern({1, 2, 3, 4, 5, 6, 7, 8, 9, 10});
  const std::array<float, 8> expected{{1, 2, 3, 4, 5, 6, 7, 8}};
  EXPECT_EQ(pen.GetDashPattern(), expected);
}

/// @brief Reddedilen değerler yer tüketmez: başa gelen üç geçersiz değer,
///        sonraki sekiz geçerli değerin hepsinin alınmasını engellememeli.
TEST(PenTest, RejectedDashValuesDoNotConsumeSlots) {
  static_assert(sdl_painter::kMaxDashSegments == 8,
                "Beklenen dizi bu sinira gore yazildi");
  Pen pen;
  pen.SetDashPattern({-1.0F, 0.0F, -2.0F, 1, 2, 3, 4, 5, 6, 7, 8, 9});
  EXPECT_EQ(pen.GetDashCount(), 8u);
  const std::array<float, 8> expected{{1, 2, 3, 4, 5, 6, 7, 8}};
  EXPECT_EQ(pen.GetDashPattern(), expected);
}

TEST(PenTest, NoPenHasNoOutline) {
  EXPECT_FALSE(Pen::NoPen().HasOutline());
}

}  // namespace sdl_painter
