#pragma once

#include "sdl_painter/color.h"
#include "sdl_painter/renderer.h"
#include "sdl_painter/vertex.h"

#include <cstdint>
#include <glm/glm.hpp>
#include <vector>

namespace sdl_painter {

class IRenderer;

/// @brief Çizim komutlarını biriktirip topluca renderer'a gönderen sınıf.
///
/// @note Transform **burada** uygulanır. Önceden model matrisi
/// `IRenderer::SetModelMatrix` ile bir uniform olarak gönderiliyordu; bu,
/// her `Translate/Rotate/Scale` çağrısının batch'i kırması demekti — ölçüm
/// (bkz. `examples/benchmarks/`) aynı 2000 şeklin transform'suz 2, şekil
/// başına transform ile 2000 draw call ürettiğini gösterdi. Artık vertex'ler
/// CPU'da dönüştürülür ve model matrisi daima birimdir; transform değişimi
/// batch'i kırmaz. Dönüşüm, rengin yazıldığı kopyalama döngüsünde yapılır —
/// ek geçiş veya tahsis yoktur.
class RenderBatcher {
 public:
  enum class DrawMode { kNone, kBasic, kTextured };

  explicit RenderBatcher(IRenderer& renderer);

  /// @brief Standart (düz renk) üçgenleri ekle.
  /// @param transform Vertex pozisyonlarına uygulanacak 3x3 affine matris.
  void PushTriangles(const std::vector<Vertex>& vertices,
                     const glm::mat3& transform, const Color& color,
                     float opacity);

  /// @brief Textured üçgenleri ekle.
  /// @param transform Vertex pozisyonlarına uygulanacak 3x3 affine matris.
  void PushTexturedTriangles(const std::vector<TexturedVertex>& vertices,
                             const glm::mat3& transform, TextureHandle texture,
                             const Color& tint, float opacity);

  /// @brief Birikmiş tüm verileri renderer'a gönder.
  void Flush();

  /// @brief Bu batcher'ın ürettiği draw call sayısı.
  [[nodiscard]] uint32_t DrawCallCount() const noexcept { return mDrawCalls; }

  /// @brief Gönderilen toplam vertex sayısı.
  [[nodiscard]] uint32_t VertexCount() const noexcept { return mVertexCount; }

  /// @brief Sayaçları sıfırla (kare başında).
  void ResetCounters() noexcept {
    mDrawCalls = 0;
    mVertexCount = 0;
  }

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

  uint32_t mDrawCalls{0};
  uint32_t mVertexCount{0};

  static constexpr std::size_t kMaxVertices = 8192;
};

}  // namespace sdl_painter
