#pragma once

#include <array>

namespace sdl_painter {

/// @brief 3x3 affine dönüşüm matrisi. GLM kullanılmaz, kendi implementasyonumuz.
///
/// Matris sütun-satır düzeninde saklanır:
/// | m[0][0]  m[0][1]  m[0][2] |   | sx*cos  -sy*sin   tx |
/// | m[1][0]  m[1][1]  m[1][2] | = | sx*sin   sy*cos   ty |
/// | m[2][0]  m[2][1]  m[2][2] |   |   0        0       1  |
class Transform {
 public:
  /// @brief Birim matris oluştur.
  Transform();

  /// @brief Ham 3x3 matris değerleriyle oluştur (satır-sütun düzeni).
  Transform(float m00, float m01, float m02,
            float m10, float m11, float m12,
            float m20, float m21, float m22);

  /// @brief Birim matrise sıfırla.
  void SetIdentity();

  /// @brief Bu matrisin birim matris olup olmadığını döndür.
  bool IsIdentity() const;

  /// @brief İki matrisi çarp: result = this * other
  Transform operator*(const Transform& other) const;

  /// @brief Mevcut matrise sağdan çarp: this = this * other
  Transform& operator*=(const Transform& other);

  /// @brief Öteleme dönüşümü uygula.
  Transform& Translate(float dx, float dy);

  /// @brief Döndürme dönüşümü uygula (derece cinsinden).
  Transform& Rotate(float angle_degrees);

  /// @brief Ölçekleme dönüşümü uygula.
  Transform& Scale(float sx, float sy);

  /// @brief 2D noktayı dönüştür.
  void Map(float x, float y, float* out_x, float* out_y) const;

  /// @brief Shader'a göndermek için 3x3 matrisin float dizisini döndür (satır-major).
  const float* Data() const { return &mData[0][0]; }

  /// @brief Öteleme dönüşümü matrisini üret.
  static Transform MakeTranslate(float dx, float dy);

  /// @brief Döndürme dönüşümü matrisini üret.
  static Transform MakeRotate(float angle_degrees);

  /// @brief Ölçekleme dönüşümü matrisini üret.
  static Transform MakeScale(float sx, float sy);

  bool operator==(const Transform& other) const;
  bool operator!=(const Transform& other) const { return !(*this == other); }

 private:
  // mData[satır][sütun]
  float mData[3][3]{};
};

}  // namespace sdl_painter
