/// @file test_collision_logic.cpp
/// @brief `breakout` örneğinin saf çarpışma matematiğinin testleri.
///
/// `test_tictactoe_logic.cpp` ile aynı gerekçe: oyun mantığı çizimden ayrık
/// tutulduğu için pencere, GL context veya renderer olmadan sınanabiliyor.
/// Başlık `examples/games/` altında; include yolu tests/CMakeLists.txt'te.

#include <gtest/gtest.h>

#include "collision_logic.h"

namespace bo = breakout;

// ─── AAKK kesişimi ──────────────────────────────────────────────────────────

TEST(Aabb, OverlappingRectanglesIntersect) {
  const bo::Aabb a{0, 0, 10, 10};
  const bo::Aabb b{5, 5, 10, 10};
  EXPECT_TRUE(bo::Intersects(a, b));
}

TEST(Aabb, SeparatedRectanglesDoNotIntersect) {
  const bo::Aabb a{0, 0, 10, 10};
  const bo::Aabb b{20, 0, 10, 10};
  EXPECT_FALSE(bo::Intersects(a, b));
}

/// @brief Kenarları tam değen dikdörtgenler kesişmiş SAYILMAZ.
///
/// Aksi halde yan yana dizilmiş tuğlalar birbirine değdiği için top iki
/// tuğlayı aynı anda kırardı.
TEST(Aabb, TouchingEdgesDoNotCount) {
  const bo::Aabb a{0, 0, 10, 10};
  const bo::Aabb b{10, 0, 10, 10};
  EXPECT_FALSE(bo::Intersects(a, b));
}

// ─── Daire ↔ AAKK ───────────────────────────────────────────────────────────

TEST(CircleAabb, CentreInsideIntersects) {
  const bo::Circle c{5, 5, 2};
  const bo::Aabb r{0, 0, 10, 10};
  EXPECT_TRUE(bo::Intersects(c, r));
}

TEST(CircleAabb, FarAwayDoesNotIntersect) {
  const bo::Circle c{100, 100, 5};
  const bo::Aabb r{0, 0, 10, 10};
  EXPECT_FALSE(bo::Intersects(c, r));
}

TEST(CircleAabb, EdgeContactIntersects) {
  // Daire sağdan tam olarak yarıçapı kadar uzakta: değme anı.
  const bo::Circle c{15, 5, 5};
  const bo::Aabb r{0, 0, 10, 10};
  EXPECT_TRUE(bo::Intersects(c, r));
}

TEST(CircleAabb, JustBeyondEdgeDoesNotIntersect) {
  const bo::Circle c{15.1F, 5, 5};
  const bo::Aabb r{0, 0, 10, 10};
  EXPECT_FALSE(bo::Intersects(c, r));
}

/// @brief Köşe durumu: dikdörtgenin köşesine yakın ama uzak olan daire.
///
/// Yalnızca eksen bazlı bir test bunu yanlışlıkla "çarpışma" sayardı; en
/// yakın nokta yöntemi doğru cevaplar.
TEST(CircleAabb, CornerCaseIsHandledCorrectly) {
  // (10,10) köşesinden çapraz uzaklık ~7.07; yarıçap 5 → değmemeli.
  const bo::Circle c{15, 15, 5};
  const bo::Aabb r{0, 0, 10, 10};
  EXPECT_FALSE(bo::Intersects(c, r));

  // Yarıçap 8 → değmeli.
  const bo::Circle big{15, 15, 8};
  EXPECT_TRUE(bo::Intersects(big, r));
}

// ─── Çarpışma ekseni ────────────────────────────────────────────────────────

TEST(ResolveAxis, NoIntersectionReturnsNone) {
  const bo::Circle c{100, 100, 5};
  const bo::Aabb r{0, 0, 10, 10};
  EXPECT_EQ(bo::ResolveAxis(c, r), bo::Axis::kNone);
}

/// @brief Üstten çarpma dikey eksende çözülmeli — top yukarı sekmeli.
TEST(ResolveAxis, HitFromAboveIsVertical) {
  const bo::Aabb brick{0, 0, 60, 20};
  const bo::Circle ball{30, -6, 8};  // tuğlanın üstünde, ortasında
  EXPECT_EQ(bo::ResolveAxis(ball, brick), bo::Axis::kVertical);
}

TEST(ResolveAxis, HitFromBelowIsVertical) {
  const bo::Aabb brick{0, 0, 60, 20};
  const bo::Circle ball{30, 26, 8};
  EXPECT_EQ(bo::ResolveAxis(ball, brick), bo::Axis::kVertical);
}

/// @brief Yandan çarpma yatay eksende çözülmeli.
///
/// Bu ayrım olmadan top, yandan çarptığında da yukarı sekerdi — oyunun en
/// çok fark edilen hatası bu olurdu.
TEST(ResolveAxis, HitFromSideIsHorizontal) {
  const bo::Aabb brick{0, 0, 60, 20};
  const bo::Circle ball{-6, 10, 8};  // tuğlanın solunda, ortasında
  EXPECT_EQ(bo::ResolveAxis(ball, brick), bo::Axis::kHorizontal);
}

TEST(ResolveAxis, HitFromRightIsHorizontal) {
  const bo::Aabb brick{0, 0, 60, 20};
  const bo::Circle ball{66, 10, 8};
  EXPECT_EQ(bo::ResolveAxis(ball, brick), bo::Axis::kHorizontal);
}

// ─── Raketten sekme ─────────────────────────────────────────────────────────

TEST(PaddleBounce, CentreHitGoesStraight) {
  const bo::Aabb paddle{100, 500, 140, 16};
  EXPECT_FLOAT_EQ(bo::PaddleBounce(paddle.CenterX(), paddle), 0.0F);
}

TEST(PaddleBounce, LeftEdgeIsMinusOne) {
  const bo::Aabb paddle{100, 500, 140, 16};
  EXPECT_FLOAT_EQ(bo::PaddleBounce(paddle.Left(), paddle), -1.0F);
}

TEST(PaddleBounce, RightEdgeIsPlusOne) {
  const bo::Aabb paddle{100, 500, 140, 16};
  EXPECT_FLOAT_EQ(bo::PaddleBounce(paddle.Right(), paddle), 1.0F);
}

/// @brief Raketin dışına taşan değme değeri sınırlanmalı.
///
/// Top raketin kenarını sıyırdığında değme noktası teknik olarak raketin
/// dışında kalabilir; kırpılmazsa sekme açısı sınırı aşar ve top neredeyse
/// yatay gider — sonsuza kadar sektiği bir duruma düşer.
TEST(PaddleBounce, OutsideHitIsClamped) {
  const bo::Aabb paddle{100, 500, 140, 16};
  EXPECT_FLOAT_EQ(bo::PaddleBounce(paddle.Left() - 500.0F, paddle), -1.0F);
  EXPECT_FLOAT_EQ(bo::PaddleBounce(paddle.Right() + 500.0F, paddle), 1.0F);
}

TEST(PaddleBounce, ZeroWidthPaddleDoesNotDivideByZero) {
  const bo::Aabb paddle{100, 500, 0, 16};
  EXPECT_FLOAT_EQ(bo::PaddleBounce(123.0F, paddle), 0.0F);
}

// ─── Yardımcı ───────────────────────────────────────────────────────────────

TEST(ClampTo, KeepsValueInRange) {
  EXPECT_FLOAT_EQ(bo::ClampTo(5.0F, 0.0F, 10.0F), 5.0F);
  EXPECT_FLOAT_EQ(bo::ClampTo(-3.0F, 0.0F, 10.0F), 0.0F);
  EXPECT_FLOAT_EQ(bo::ClampTo(42.0F, 0.0F, 10.0F), 10.0F);
}
