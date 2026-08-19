#pragma once

#include "sdl_painter/color.h"
#include "sdl_painter/renderer.h"
#include "sdl_painter/vertex.h"

#include <cstdint>
#include <vector>

namespace sdl_painter {

class IRenderer;

/// @brief Çizim komutlarını biriktirip topluca renderer'a gönderen sınıf.
class RenderBatcher {
 public:
  enum class DrawMode { kNone, kBasic, kTextured };

  explicit RenderBatcher(IRenderer& renderer);

  /// @brief Standart (düz renk) üçgenleri ekle.
  void PushTriangles(const std::vector<Vertex>& vertices, const Color& color,
                     float opacity);

  /// @brief Textured üçgenleri ekle.
  void PushTexturedTriangles(const std::vector<TexturedVertex>& vertices,
                             TextureHandle texture, const Color& tint,
                             float opacity);

  /// @brief Birikmiş tüm verileri renderer'a gönder.
  void Flush();

 private:
  /// @brief İki opaklık değeri aynı batch'te birleştirilebilir mi?
  ///
  /// Doğrudan `!=` yerine tolerans kullanılır: opaklık kullanıcıdan gelen bir
  /// float'tır ve hesaplanmış değerlerde (örn. `1.0F / 3.0F * 3.0F`) bit
  /// düzeyinde eşitlik beklemek gereksiz flush'a yol açar. 1/512, 8-bit alfa
  /// çözünürlüğünün (1/255) altındadır — görsel fark üretmez.
  [[nodiscard]] static bool SameOpacity(float a, float b) noexcept {
    constexpr float kEpsilon = 1.0F / 512.0F;
    const float diff = a - b;
    return (diff < 0.0F ? -diff : diff) < kEpsilon;
  }

  IRenderer& mRenderer;

  DrawMode mCurrentMode{DrawMode::kNone};
  TextureHandle mCurrentTexture{kInvalidTexture};
  float mCurrentOpacity{1.0F};

  std::vector<Vertex> mVertexBuffer;
  std::vector<TexturedVertex> mTexturedBuffer;

  static constexpr std::size_t kMaxVertices = 8192;
};

}  // namespace sdl_painter
