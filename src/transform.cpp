#include "sdl_painter/transform.h"

#include <cmath>
#include <cstdint>
#include <cstring>
namespace sdl_painter {

Transform::Transform() {
  SetIdentity();
}

Transform::Transform(float m00, float m01, float m02, float m10, float m11,
                     float m12, float m20, float m21, float m22) {
  // clang-format off
  mData[0][0] = m00; mData[0][1] = m01; mData[0][2] = m02;
  mData[1][0] = m10; mData[1][1] = m11; mData[1][2] = m12;
  mData[2][0] = m20; mData[2][1] = m21; mData[2][2] = m22;
  // clang-format on
}

void Transform::SetIdentity() {
  // clang-format off
  mData[0][0] = 1.0f; mData[0][1] = 0.0f; mData[0][2] = 0.0f;
  mData[1][0] = 0.0f; mData[1][1] = 1.0f; mData[1][2] = 0.0f;
  mData[2][0] = 0.0f; mData[2][1] = 0.0f; mData[2][2] = 1.0f;
  // clang-format on
}

bool Transform::IsIdentity() const {
  // Doğrudan element kontrol — magic-statics ve karşılaştırma maliyetini
  // ortadan kaldırır.
  // clang-format off
  return mData[0][0] == 1.0f && mData[0][1] == 0.0f && mData[0][2] == 0.0f &&
         mData[1][0] == 0.0f && mData[1][1] == 1.0f && mData[1][2] == 0.0f &&
         mData[2][0] == 0.0f && mData[2][1] == 0.0f && mData[2][2] == 1.0f;
  // clang-format on
}

Transform Transform::operator*(const Transform& other) const {
  Transform result;
  for (int32_t r = 0; r < 3; ++r) {
    for (int32_t c = 0; c < 3; ++c) {
      result.mData[r][c] = 0.0f;
      for (int32_t k = 0; k < 3; ++k) {
        result.mData[r][c] += mData[r][k] * other.mData[k][c];
      }
    }
  }
  return result;
}

Transform& Transform::operator*=(const Transform& other) {
  *this = *this * other;
  return *this;
}

Transform& Transform::Translate(float dx, float dy) {
  // Sağdan çarp (post-multiply): QPainter / HTML Canvas / OpenGL fixed
  // pipeline semantiği. Translate().Rotate().FillRect(-w/2,-h/2,w,h)
  // → dikdörtgen önce kendi merkezi etrafında döner, sonra (dx,dy)'ye taşınır.
  // Vertex çarpımı: M·v = T·R·v → R önce, T sonra uygulanır.
  *this = *this * MakeTranslate(dx, dy);
  return *this;
}

Transform& Transform::Rotate(float angle_degrees) {
  *this = *this * MakeRotate(angle_degrees);
  return *this;
}

Transform& Transform::Scale(float sx, float sy) {
  *this = *this * MakeScale(sx, sy);
  return *this;
}

void Transform::Map(float x, float y, float* out_x, float* out_y) const {
  *out_x = mData[0][0] * x + mData[0][1] * y + mData[0][2];
  *out_y = mData[1][0] * x + mData[1][1] * y + mData[1][2];
}

Transform Transform::MakeTranslate(float dx, float dy) {
  // clang-format off
  return Transform(1.0f, 0.0f, dx,
                   0.0f, 1.0f, dy,
                   0.0f, 0.0f, 1.0f);
  // clang-format on
}

Transform Transform::MakeRotate(float angle_degrees) {
  constexpr float kPi = 3.14159265358979323846f;
  float rad = angle_degrees * kPi / 180.0f;
  float c = std::cos(rad);
  float s = std::sin(rad);
  // clang-format off
  return Transform(c,    -s,   0.0f,
                   s,     c,   0.0f,
                   0.0f,  0.0f, 1.0f);
  // clang-format on
}

Transform Transform::MakeScale(float sx, float sy) {
  // clang-format off
  return Transform(sx,   0.0f, 0.0f,
                   0.0f, sy,   0.0f,
                   0.0f, 0.0f, 1.0f);
  // clang-format on
}

bool Transform::operator==(const Transform& other) const {
  // memcmp bit-bit karşılaştırma yapar. Bu, IEEE 754'te NaN != NaN
  // semantiğinden sapmadır: iki NaN matris bit-bit eşitse true döner.
  // Pratikte transform matrisleri NaN içermez; bu kabul edilen bir
  // sapmadır ve transform stack içindeki "değişti mi" testleri için
  // yeterlidir.
  return std::memcmp(mData, other.mData, sizeof(mData)) == 0;
}

}  // namespace sdl_painter
