#include "render_batcher.h"

#include "sdl_painter/color.h"

namespace sdl_painter {

RenderBatcher::RenderBatcher(IRenderer& renderer) : mRenderer(renderer) {
  mVertexBuffer.reserve(kMaxVertices);
  mTexturedBuffer.reserve(kMaxVertices);
}

void RenderBatcher::PushTriangles(const std::vector<Vertex>& vertices,
                                  const Color& color, float opacity) {
  // State değişimi veya buffer taşması kontrolü
  if (mCurrentMode != DrawMode::kBasic ||
      !SameOpacity(mCurrentOpacity, opacity) ||
      (mVertexBuffer.size() + vertices.size()) > kMaxVertices) {
    Flush();
  }

  mCurrentMode = DrawMode::kBasic;
  mCurrentOpacity = opacity;

  for (auto v : vertices) {
    v.r = color.r;
    v.g = color.g;
    v.b = color.b;
    v.a = color.a;
    mVertexBuffer.push_back(v);
  }
}

void RenderBatcher::PushTexturedTriangles(
    const std::vector<TexturedVertex>& vertices, TextureHandle texture,
    const Color& tint, float opacity) {
  if (mCurrentMode != DrawMode::kTextured || mCurrentTexture != texture ||
      !SameOpacity(mCurrentOpacity, opacity) ||
      (mTexturedBuffer.size() + vertices.size()) > kMaxVertices) {
    Flush();
  }

  mCurrentMode = DrawMode::kTextured;
  mCurrentTexture = texture;
  mCurrentOpacity = opacity;

  for (auto v : vertices) {
    v.r = tint.r;
    v.g = tint.g;
    v.b = tint.b;
    v.a = tint.a;
    mTexturedBuffer.push_back(v);
  }
}

void RenderBatcher::Flush() {
  if (mCurrentMode == DrawMode::kBasic && !mVertexBuffer.empty()) {
    mRenderer.SetOpacity(mCurrentOpacity);
    mRenderer.DrawTriangles(mVertexBuffer);
    mVertexBuffer.clear();
  } else if (mCurrentMode == DrawMode::kTextured && !mTexturedBuffer.empty()) {
    mRenderer.SetOpacity(mCurrentOpacity);
    mRenderer.DrawTextured(mTexturedBuffer, mCurrentTexture);
    mTexturedBuffer.clear();
  }

  // mCurrentOpacity korunuyor — aynı opacity ile gelen sonraki batch için flush tetiklenmez.
  mCurrentMode = DrawMode::kNone;
  mCurrentTexture = kInvalidTexture;
}

}  // namespace sdl_painter
