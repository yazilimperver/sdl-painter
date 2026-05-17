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
  IRenderer& mRenderer;

  DrawMode mCurrentMode{DrawMode::kNone};
  TextureHandle mCurrentTexture{kInvalidTexture};
  float mCurrentOpacity{1.0f};

  std::vector<Vertex> mVertexBuffer;
  std::vector<TexturedVertex> mTexturedBuffer;

  static constexpr std::size_t kMaxVertices = 8192;
};

}  // namespace sdl_painter
