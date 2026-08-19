#include <cmath>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <gtest/gtest.h>

// Transform sınıfı kaldırıldı; artık doğrudan glm::mat3 (column-major) kullanılır.
// Bu testler, Painter'ın transform metodlarının dayandığı iki sözleşmeyi kilitler:
//   1) affine matrislerin column-major kuruluşu (mat[col][row]),
//   2) post-multiply (sağdan çarp) M = M * op.
// Ayrıca glm::value_ptr'ın GPU'ya giden column-major byte düzenini doğrular.

namespace sdl_painter {
namespace {

constexpr float kEpsilon = 1e-5f;

// --- Painter ile birebir aynı affine matris kurucuları (column-major) ---

glm::mat3 MakeTranslate(float dx, float dy) {
  glm::mat3 t(1.0f);
  t[2][0] = dx;
  t[2][1] = dy;
  return t;
}

glm::mat3 MakeRotate(float angle_degrees) {
  const float r = glm::radians(angle_degrees);
  const float c = std::cos(r);
  const float s = std::sin(r);
  glm::mat3 rot(1.0f);
  rot[0][0] = c;
  rot[0][1] = s;
  rot[1][0] = -s;
  rot[1][1] = c;
  return rot;
}

glm::mat3 MakeScale(float sx, float sy) {
  glm::mat3 sc(1.0f);
  sc[0][0] = sx;
  sc[1][1] = sy;
  return sc;
}

// 2B noktayı homojen koordinatta dönüştür: p' = M * (x, y, 1).
void Map(const glm::mat3& m, float x, float y, float* out_x, float* out_y) {
  const glm::vec3 p = m * glm::vec3(x, y, 1.0f);
  *out_x = p.x;
  *out_y = p.y;
}

}  // namespace

TEST(TransformTest, DefaultIsIdentity) {
  glm::mat3 t(1.0f);
  EXPECT_EQ(t, glm::mat3(1.0f));
}

TEST(TransformTest, IdentityMapIsNoOp) {
  glm::mat3 t(1.0f);
  float ox = 0.0f;
  float oy = 0.0f;
  Map(t, 3.0f, 4.0f, &ox, &oy);
  EXPECT_NEAR(ox, 3.0f, kEpsilon);
  EXPECT_NEAR(oy, 4.0f, kEpsilon);
}

TEST(TransformTest, Translate) {
  glm::mat3 t = MakeTranslate(5.0f, -3.0f);
  float ox = 0.0f;
  float oy = 0.0f;
  Map(t, 1.0f, 1.0f, &ox, &oy);
  EXPECT_NEAR(ox, 6.0f, kEpsilon);
  EXPECT_NEAR(oy, -2.0f, kEpsilon);
}

TEST(TransformTest, Scale) {
  glm::mat3 t = MakeScale(2.0f, 3.0f);
  float ox = 0.0f;
  float oy = 0.0f;
  Map(t, 4.0f, 5.0f, &ox, &oy);
  EXPECT_NEAR(ox, 8.0f, kEpsilon);
  EXPECT_NEAR(oy, 15.0f, kEpsilon);
}

TEST(TransformTest, Rotate90Degrees) {
  glm::mat3 t = MakeRotate(90.0f);
  float ox = 0.0f;
  float oy = 0.0f;
  // (1, 0) → (0, 1) (saat yönü tersi, Y aşağı düzende)
  Map(t, 1.0f, 0.0f, &ox, &oy);
  EXPECT_NEAR(ox, 0.0f, kEpsilon);
  EXPECT_NEAR(oy, 1.0f, kEpsilon);
}

TEST(TransformTest, Rotate180Degrees) {
  glm::mat3 t = MakeRotate(180.0f);
  float ox = 0.0f;
  float oy = 0.0f;
  Map(t, 1.0f, 0.0f, &ox, &oy);
  EXPECT_NEAR(ox, -1.0f, kEpsilon);
  EXPECT_NEAR(oy, 0.0f, kEpsilon);
}

TEST(TransformTest, MatrixMultiplication) {
  // operator* standart matris çarpımı: (A*B)*v = A*(B*v) → B önce uygulanır.
  glm::mat3 translate = MakeTranslate(2.0f, 0.0f);
  glm::mat3 scale = MakeScale(3.0f, 1.0f);

  // translate * scale: önce scale, sonra translate.
  // (1,0) → scale(3x) → (3,0) → translate(+2) → (5,0)
  glm::mat3 combined_ts = translate * scale;
  float ox = 0.0f;
  float oy = 0.0f;
  Map(combined_ts, 1.0f, 0.0f, &ox, &oy);
  EXPECT_NEAR(ox, 5.0f, kEpsilon);
  EXPECT_NEAR(oy, 0.0f, kEpsilon);

  // scale * translate: önce translate, sonra scale.
  // (1,0) → translate(+2) → (3,0) → scale(3x) → (9,0)
  glm::mat3 combined_st = scale * translate;
  Map(combined_st, 1.0f, 0.0f, &ox, &oy);
  EXPECT_NEAR(ox, 9.0f, kEpsilon);
  EXPECT_NEAR(oy, 0.0f, kEpsilon);
}

TEST(TransformTest, ChainedOperations) {
  // Sağdan-çarp (post-multiply) semantiği: Translate(10,0) sonra Scale(2,2)
  // → vertex önce ölçeklenir, sonra ötelenir.
  // M = T(10,0) * S(2,2); v=(0,0) için: S·v=(0,0), T·(0,0)=(10,0).
  glm::mat3 t(1.0f);
  t = t * MakeTranslate(10.0f, 0.0f);
  t = t * MakeScale(2.0f, 2.0f);

  float ox = 0.0f;
  float oy = 0.0f;
  Map(t, 0.0f, 0.0f, &ox, &oy);
  EXPECT_NEAR(ox, 10.0f, kEpsilon);
  EXPECT_NEAR(oy, 0.0f, kEpsilon);

  // Ölçeklenen bir vertex için fark görünür: v=(1,0) için
  // S·v=(2,0), T·(2,0)=(12,0).
  Map(t, 1.0f, 0.0f, &ox, &oy);
  EXPECT_NEAR(ox, 12.0f, kEpsilon);
  EXPECT_NEAR(oy, 0.0f, kEpsilon);
}

TEST(TransformTest, TranslateThenRotateRotatesAroundLocalOrigin) {
  //  Translate(cx,cy) sonra Rotate(90), FillRect(-1,-1,2,2)
  // → dikdörtgenin merkezi (cx,cy)'de kalır, kendi etrafında döner.
  glm::mat3 t(1.0f);
  t = t * MakeTranslate(100.0f, 50.0f);
  t = t * MakeRotate(90.0f);

  float ox = 0.0f;
  float oy = 0.0f;
  // Lokal (1,0) noktası: önce 90° döner → (0,1), sonra (100,50)'ye ötelenir.
  Map(t, 1.0f, 0.0f, &ox, &oy);
  EXPECT_NEAR(ox, 100.0f, kEpsilon);
  EXPECT_NEAR(oy, 51.0f, kEpsilon);

  // Lokal merkez (0,0) → daima (100,50)'ye düşmeli.
  Map(t, 0.0f, 0.0f, &ox, &oy);
  EXPECT_NEAR(ox, 100.0f, kEpsilon);
  EXPECT_NEAR(oy, 50.0f, kEpsilon);
}

TEST(TransformTest, Equality) {
  glm::mat3 a = MakeTranslate(1.0f, 2.0f);
  glm::mat3 b = MakeTranslate(1.0f, 2.0f);
  glm::mat3 c = MakeTranslate(1.0f, 3.0f);
  EXPECT_EQ(a, b);
  EXPECT_NE(a, c);
}

TEST(TransformTest, ValuePtrIsColumnMajor) {
  // glm::value_ptr, GPU'ya (SetModelMatrix) giden column-major byte düzenini
  // verir. Bu test, OpenGL GL_FALSE upload'u ve Vulkan repack indekslerinin
  // dayandığı düzeni kilitler: element = data[col*3 + row], tx=[6], ty=[7].
  glm::mat3 t = MakeTranslate(7.0f, 9.0f);
  const float* data = glm::value_ptr(t);
  ASSERT_NE(data, nullptr);
  EXPECT_FLOAT_EQ(data[0], 1.0f);  // m00
  EXPECT_FLOAT_EQ(data[4], 1.0f);  // m11
  EXPECT_FLOAT_EQ(data[8], 1.0f);  // m22
  EXPECT_FLOAT_EQ(data[6], 7.0f);  // tx
  EXPECT_FLOAT_EQ(data[7], 9.0f);  // ty
}

}  // namespace sdl_painter
