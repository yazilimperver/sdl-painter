#include "render_batcher.h"

#include "sdl_painter/color.h"

namespace sdl_painter {

namespace {

/// @brief 3x3 affine matrisin (column-major) 2B nokta çarpımı.
///
/// `glm::vec3` üzerinden gitmek yerine bileşenler elle çarpılır: alt satır
/// affine matriste daima [0 0 1] olduğundan üçüncü bileşenin hesabı gereksiz.
struct Affine2D {
  float m00, m10, m20;  // x' = m00*x + m10*y + m20
  float m01, m11, m21;  // y' = m01*x + m11*y + m21

  explicit Affine2D(const glm::mat3& m)
      : m00(m[0][0]),
        m10(m[1][0]),
        m20(m[2][0]),
        m01(m[0][1]),
        m11(m[1][1]),
        m21(m[2][1]) {}

  [[nodiscard]] float X(float x, float y) const noexcept {
    return (m00 * x) + (m10 * y) + m20;
  }
  [[nodiscard]] float Y(float x, float y) const noexcept {
    return (m01 * x) + (m11 * y) + m21;
  }
};

}  // namespace

RenderBatcher::RenderBatcher(IRenderer& renderer) : mRenderer(renderer) {
  mVertexBuffer.reserve(kMaxVertices);
  mTexturedBuffer.reserve(kMaxVertices);
}

void RenderBatcher::PushTriangles(const std::vector<Vertex>& vertices,
                                  const glm::mat3& transform,
                                  const Color& color, float opacity) {
  // State değişimi veya buffer taşması kontrolü.
  // Transform artık bir state değil (CPU'da vertex'e gömülüyor), bu yüzden
  // burada kontrol edilmez — batch'i kırmayan tek "değişken" odur.
  if (mCurrentMode != DrawMode::kBasic ||
      !SameOpacity(mCurrentOpacity, opacity) ||
      (mVertexBuffer.size() + vertices.size()) > kMaxVertices) {
    Flush();
  }

  mCurrentMode = DrawMode::kBasic;
  mCurrentOpacity = opacity;

  const Affine2D t(transform);
  for (auto v : vertices) {
    const float x = v.x;
    const float y = v.y;
    v.x = t.X(x, y);
    v.y = t.Y(x, y);
    v.r = color.r;
    v.g = color.g;
    v.b = color.b;
    v.a = color.a;
    mVertexBuffer.push_back(v);
  }
}

void RenderBatcher::PushTexturedTriangles(
    const std::vector<TexturedVertex>& vertices, const glm::mat3& transform,
    TextureHandle texture, const Color& tint, float opacity) {
  if (mCurrentMode != DrawMode::kTextured || mCurrentTexture != texture ||
      !SameOpacity(mCurrentOpacity, opacity) ||
      (mTexturedBuffer.size() + vertices.size()) > kMaxVertices) {
    Flush();
  }

  mCurrentMode = DrawMode::kTextured;
  mCurrentTexture = texture;
  mCurrentOpacity = opacity;

  const Affine2D t(transform);
  for (auto v : vertices) {
    const float x = v.x;
    const float y = v.y;
    v.x = t.X(x, y);
    v.y = t.Y(x, y);
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
    ++mDrawCalls;
    mVertexCount += static_cast<uint32_t>(mVertexBuffer.size());
    mVertexBuffer.clear();
  } else if (mCurrentMode == DrawMode::kTextured && !mTexturedBuffer.empty()) {
    mRenderer.SetOpacity(mCurrentOpacity);
    mRenderer.DrawTextured(mTexturedBuffer, mCurrentTexture);
    ++mDrawCalls;
    mVertexCount += static_cast<uint32_t>(mTexturedBuffer.size());
    mTexturedBuffer.clear();
  }

  // mCurrentOpacity korunuyor — aynı opacity ile gelen sonraki batch için flush tetiklenmez.
  mCurrentMode = DrawMode::kNone;
  mCurrentTexture = kInvalidTexture;
}

}  // namespace sdl_painter
