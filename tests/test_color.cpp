#include "sdl_painter/color.h"

#include <gtest/gtest.h>

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

TEST(ColorTest, TransparentIsAllChannelsZero) {
  EXPECT_EQ(Color::Transparent(), Color(0, 0, 0, 0));
}

TEST(ColorTest, EqualityDetectsRedDifference) {
  const Color a(10, 20, 30, 40);
  const Color b(11, 20, 30, 40);
  EXPECT_NE(a, b);
}

TEST(ColorTest, EqualityDetectsGreenDifference) {
  const Color a(10, 20, 30, 40);
  const Color b(10, 21, 30, 40);
  EXPECT_NE(a, b);
}

TEST(ColorTest, EqualityDetectsBlueDifference) {
  const Color a(10, 20, 30, 40);
  const Color b(10, 20, 31, 40);
  EXPECT_NE(a, b);
}

/// @brief Dört kanal da farklı ara değerde: iki kanalın karışması (örneğin
///        RedF ile AlphaF) bu testte görünür olur.
TEST(ColorTest, NormalizedFloatsKeepEveryChannelApart) {
  const Color c(51, 102, 153, 204);
  EXPECT_NEAR(c.RedF(), 0.2F, 1e-5F);
  EXPECT_NEAR(c.GreenF(), 0.4F, 1e-5F);
  EXPECT_NEAR(c.BlueF(), 0.6F, 1e-5F);
  EXPECT_NEAR(c.AlphaF(), 0.8F, 1e-5F);
}

/// @brief Kurucular ve adlandırılmış renkler sabit ifadede kullanılabilmeli.
TEST(ColorTest, ConstructorsAndNamedColorsUsableInStaticAssert) {
  constexpr Color kDefault{};
  constexpr Color kFull(1, 2, 3, 4);
  constexpr Color kOpaque(5, 6, 7);
  constexpr Color kBlack = Color::Black();
  constexpr Color kWhite = Color::White();
  constexpr Color kRed = Color::Red();
  constexpr Color kGreen = Color::Green();
  constexpr Color kBlue = Color::Blue();
  constexpr Color kTransparent = Color::Transparent();

  static_assert(kDefault.r == 0 && kDefault.g == 0 && kDefault.b == 0 &&
                    kDefault.a == 255,
                "Varsayilan renk siyah ve opak olmali");
  static_assert(kFull.r == 1 && kFull.g == 2 && kFull.b == 3 && kFull.a == 4,
                "Dort kanalli kurucu degerleri sirasiyla saklamali");
  static_assert(
      kOpaque.r == 5 && kOpaque.g == 6 && kOpaque.b == 7 && kOpaque.a == 255,
      "Alfa verilmezse opak olmali");
  static_assert(
      kBlack.r == 0 && kBlack.g == 0 && kBlack.b == 0 && kBlack.a == 255,
      "Black");
  static_assert(
      kWhite.r == 255 && kWhite.g == 255 && kWhite.b == 255 && kWhite.a == 255,
      "White");
  static_assert(kRed.r == 255 && kRed.g == 0 && kRed.b == 0 && kRed.a == 255,
                "Red");
  static_assert(
      kGreen.r == 0 && kGreen.g == 255 && kGreen.b == 0 && kGreen.a == 255,
      "Green");
  static_assert(
      kBlue.r == 0 && kBlue.g == 0 && kBlue.b == 255 && kBlue.a == 255, "Blue");
  static_assert(kTransparent.r == 0 && kTransparent.g == 0 &&
                    kTransparent.b == 0 && kTransparent.a == 0,
                "Transparent");
  SUCCEED();
}

}  // namespace sdl_painter
