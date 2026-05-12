#include <gtest/gtest.h>

#include <cmath>

#include "sdl_painter/transform.h"

namespace sdl_painter {

constexpr float kEpsilon = 1e-5f;

TEST(TransformTest, DefaultIsIdentity) {
  Transform t;
  EXPECT_TRUE(t.IsIdentity());
}

TEST(TransformTest, SetIdentity) {
  Transform t = Transform::MakeTranslate(10.0f, 20.0f);
  EXPECT_FALSE(t.IsIdentity());
  t.SetIdentity();
  EXPECT_TRUE(t.IsIdentity());
}

TEST(TransformTest, IdentityMapIsNoOp) {
  Transform t;
  float ox = 0.0f;
  float oy = 0.0f;
  t.Map(3.0f, 4.0f, &ox, &oy);
  EXPECT_NEAR(ox, 3.0f, kEpsilon);
  EXPECT_NEAR(oy, 4.0f, kEpsilon);
}

TEST(TransformTest, Translate) {
  Transform t = Transform::MakeTranslate(5.0f, -3.0f);
  float ox = 0.0f;
  float oy = 0.0f;
  t.Map(1.0f, 1.0f, &ox, &oy);
  EXPECT_NEAR(ox, 6.0f, kEpsilon);
  EXPECT_NEAR(oy, -2.0f, kEpsilon);
}

TEST(TransformTest, Scale) {
  Transform t = Transform::MakeScale(2.0f, 3.0f);
  float ox = 0.0f;
  float oy = 0.0f;
  t.Map(4.0f, 5.0f, &ox, &oy);
  EXPECT_NEAR(ox, 8.0f, kEpsilon);
  EXPECT_NEAR(oy, 15.0f, kEpsilon);
}

TEST(TransformTest, Rotate90Degrees) {
  Transform t = Transform::MakeRotate(90.0f);
  float ox = 0.0f;
  float oy = 0.0f;
  // (1, 0) → (0, 1) (saat yönü tersi, Y aşağı düzende)
  t.Map(1.0f, 0.0f, &ox, &oy);
  EXPECT_NEAR(ox, 0.0f, kEpsilon);
  EXPECT_NEAR(oy, 1.0f, kEpsilon);
}

TEST(TransformTest, Rotate180Degrees) {
  Transform t = Transform::MakeRotate(180.0f);
  float ox = 0.0f;
  float oy = 0.0f;
  t.Map(1.0f, 0.0f, &ox, &oy);
  EXPECT_NEAR(ox, -1.0f, kEpsilon);
  EXPECT_NEAR(oy, 0.0f, kEpsilon);
}

TEST(TransformTest, MatrixMultiplication) {
  // operator* standart matris çarpımı: (A*B)*v = A*(B*v) → B önce uygulanır.
  Transform translate = Transform::MakeTranslate(2.0f, 0.0f);
  Transform scale = Transform::MakeScale(3.0f, 1.0f);

  // translate * scale: önce scale, sonra translate.
  // (1,0) → scale(3x) → (3,0) → translate(+2) → (5,0)
  Transform combined_ts = translate * scale;
  float ox = 0.0f;
  float oy = 0.0f;
  combined_ts.Map(1.0f, 0.0f, &ox, &oy);
  EXPECT_NEAR(ox, 5.0f, kEpsilon);
  EXPECT_NEAR(oy, 0.0f, kEpsilon);

  // scale * translate: önce translate, sonra scale.
  // (1,0) → translate(+2) → (3,0) → scale(3x) → (9,0)
  Transform combined_st = scale * translate;
  combined_st.Map(1.0f, 0.0f, &ox, &oy);
  EXPECT_NEAR(ox, 9.0f, kEpsilon);
  EXPECT_NEAR(oy, 0.0f, kEpsilon);
}

TEST(TransformTest, ChainedOperations) {
  // Sağdan-çarp (post-multiply) semantiği: Translate(10,0).Scale(2,2)
  // → vertex önce ölçeklenir, sonra ötelenir.
  // M = T(10,0) * S(2,2); v=(0,0) için: S·v=(0,0), T·(0,0)=(10,0).
  Transform t;
  t.Translate(10.0f, 0.0f).Scale(2.0f, 2.0f);

  float ox = 0.0f;
  float oy = 0.0f;
  t.Map(0.0f, 0.0f, &ox, &oy);
  EXPECT_NEAR(ox, 10.0f, kEpsilon);
  EXPECT_NEAR(oy, 0.0f, kEpsilon);

  // Ölçeklenen bir vertex için fark görünür: v=(1,0) için
  // S·v=(2,0), T·(2,0)=(12,0).
  t.Map(1.0f, 0.0f, &ox, &oy);
  EXPECT_NEAR(ox, 12.0f, kEpsilon);
  EXPECT_NEAR(oy, 0.0f, kEpsilon);
}

TEST(TransformTest, TranslateThenRotateRotatesAroundLocalOrigin) {
  // QPainter senaryosu: Translate(cx,cy).Rotate(90).FillRect(-1,-1,2,2)
  // → dikdörtgenin merkezi (cx,cy)'de kalır, kendi etrafında döner.
  Transform t;
  t.Translate(100.0f, 50.0f).Rotate(90.0f);

  float ox = 0.0f;
  float oy = 0.0f;
  // Lokal (1,0) noktası: önce 90° döner → (0,1), sonra (100,50)'ye ötelenir.
  t.Map(1.0f, 0.0f, &ox, &oy);
  EXPECT_NEAR(ox, 100.0f, kEpsilon);
  EXPECT_NEAR(oy, 51.0f, kEpsilon);

  // Lokal merkez (0,0) → daima (100,50)'ye düşmeli.
  t.Map(0.0f, 0.0f, &ox, &oy);
  EXPECT_NEAR(ox, 100.0f, kEpsilon);
  EXPECT_NEAR(oy, 50.0f, kEpsilon);
}

TEST(TransformTest, Equality) {
  Transform a = Transform::MakeTranslate(1.0f, 2.0f);
  Transform b = Transform::MakeTranslate(1.0f, 2.0f);
  Transform c = Transform::MakeTranslate(1.0f, 3.0f);
  EXPECT_EQ(a, b);
  EXPECT_NE(a, c);
}

TEST(TransformTest, DataPointerAccessible) {
  Transform t;
  const float* data = t.Data();
  EXPECT_NE(data, nullptr);
  // Birim matris: köşegen 1, diğerleri 0
  EXPECT_FLOAT_EQ(data[0], 1.0f);  // [0][0]
  EXPECT_FLOAT_EQ(data[1], 0.0f);  // [0][1]
  EXPECT_FLOAT_EQ(data[4], 1.0f);  // [1][1]
  EXPECT_FLOAT_EQ(data[8], 1.0f);  // [2][2]
}

}  // namespace sdl_painter
