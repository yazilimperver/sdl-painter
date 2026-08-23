#pragma once

#include "sdl_painter/renderer.h"

#include <cstdint>
#include <vector>

#include "shader_program.h"

namespace sdl_painter {

/// @brief OpenGL 3.3 Core Profile IRenderer implementasyonu.
class OpenGLRenderer final : public IRenderer {
 public:
  OpenGLRenderer() = default;
  ~OpenGLRenderer() override;

  // Non-copyable, non-movable (OpenGL state'i sahipleniyor)
  OpenGLRenderer(const OpenGLRenderer&) = delete;
  OpenGLRenderer& operator=(const OpenGLRenderer&) = delete;

  // --- IRenderer arayüzü ---

  bool Initialize(SDL_Window* window) override;
  void Shutdown() override;
  void BeginFrame() override;
  void EndFrame() override;

  void SetViewport(int32_t x, int32_t y, int32_t width,
                   int32_t height) override;
  void SetScissor(int32_t x, int32_t y, int32_t width, int32_t height) override;
  void ClearScissor() override;
  void Clear(const Color& color) override;
  void SetOpacity(float alpha) override;

  void DrawTriangles(const std::vector<Vertex>& vertices) override;

  TextureHandle CreateTexture(const uint8_t* data, int32_t width,
                              int32_t height, int32_t channels) override;
  void UpdateTexture(TextureHandle handle, int32_t x, int32_t y, int32_t width,
                     int32_t height, const uint8_t* data) override;
  void DestroyTexture(TextureHandle handle) override;
  void DrawTextured(const std::vector<TexturedVertex>& vertices,
                    TextureHandle texture) override;

  RendererBackend GetBackend() const override {
    return RendererBackend::kOpenGL;
  }
  void SetProjectionMatrix(const float* mat4) override;
  void SetModelMatrix(const float* mat3) override;

  [[nodiscard]] double GetLastGpuFrameMs() const override {
    return mLastGpuFrameMs;
  }

 private:
  /// @brief OpenGL VAO, VBO oluştur.
  void SetupBuffers();

  /// @brief Timer query nesnelerini oluştur (GL 3.3 core: ARB_timer_query).
  void SetupTimerQueries();

  /// @brief Hazır olan en eski sorgu sonucunu topla (bloklamadan).
  void CollectGpuTime();

  SDL_Window* mWindow{nullptr};
  void* mGLContext{nullptr};

  // Shader programları
  ShaderProgram mBasicShader;
  ShaderProgram mTexturedShader;

  // VAO / VBO
  uint32_t mVao{0};
  uint32_t mVbo{0};

  // Textured VAO / VBO
  uint32_t mTexturedVao{0};
  uint32_t mTexturedVbo{0};

  // Aktif projeksiyon ve model matrisleri
  float mProjection[16]{};
  float mModel[9]{};
  float mOpacity{1.0F};

  // --- GPU zaman ölçümü ---
  //
  // Cift tamponlu: bu karenin sorgusu yazilirken bir onceki karenin sonucu
  // okunur. Ayni karenin sonucunu beklemek CPU'yu GPU'ya kilitler ve
  // olculmek istenen seyi bozar.
  static constexpr int32_t kTimerQueryCount = 2;
  uint32_t mTimerQueries[kTimerQueryCount]{};
  bool mTimerQueryPending[kTimerQueryCount]{};
  int32_t mTimerQueryIndex{0};
  bool mTimerQueryActive{false};
  double mLastGpuFrameMs{0.0};
};

}  // namespace sdl_painter
