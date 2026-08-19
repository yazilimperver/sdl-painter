#include "opengl_renderer.h"

#include "sdl_painter/color.h"
#include "sdl_painter/embedded_gl_shaders.h"
#include "sdl_painter/vertex.h"

#include <SDL3/SDL.h>

#include <array>
#include <cstdint>
#include <cstring>
#include <glad/glad.h>
#include <spdlog/spdlog.h>

// Phase 1'de tam implementasyon yapılacak.
// Şimdilik OpenGL context kurulumu ve temel altyapı hazır.

namespace sdl_painter {

// --- OpenGLRenderer implementasyonu ---

OpenGLRenderer::~OpenGLRenderer() {
  Shutdown();
}

bool OpenGLRenderer::Initialize(SDL_Window* window) {
  mWindow = window;

  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);

  mGLContext = SDL_GL_CreateContext(window);
  if (mGLContext == nullptr) {
    spdlog::error("[OpenGLRenderer] Failed to create GL context: {}",
                  SDL_GetError());
    return false;
  }

  if (gladLoadGLLoader(reinterpret_cast<GLADloadproc>(SDL_GL_GetProcAddress)) ==
      0) {
    spdlog::error("[OpenGLRenderer] Failed to load GLAD");
    return false;
  }

  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  // Shader kaynakları binary'ye gömülüdür (bkz. cmake/EmbedShaders.cmake);
  // çalışma zamanında hiçbir dosya aranmaz.
  if (!mBasicShader.Build(detail::kBasicVert, detail::kBasicFrag)) {
    spdlog::error("[OpenGLRenderer] Failed to build basic shader");
    return false;
  }

  if (!mTexturedShader.Build(detail::kTexturedVert, detail::kTexturedFrag)) {
    spdlog::error("[OpenGLRenderer] Failed to build textured shader");
    return false;
  }

  SetupBuffers();
  return true;
}

void OpenGLRenderer::SetupBuffers() {
  // Temel VAO/VBO
  glGenVertexArrays(1, &mVao);
  glGenBuffers(1, &mVbo);
  glBindVertexArray(mVao);
  glBindBuffer(GL_ARRAY_BUFFER, mVbo);

  // Position (Location 0)
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                        reinterpret_cast<void*>(static_cast<uintptr_t>(0)));
  // Color (Location 1) - Normalized uint8 -> float [0, 1]
  glEnableVertexAttribArray(1);
  glVertexAttribPointer(
      1, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(Vertex),
      reinterpret_cast<void*>(static_cast<uintptr_t>(2 * sizeof(float))));

  glBindVertexArray(0);

  // Textured VAO/VBO
  glGenVertexArrays(1, &mTexturedVao);
  glGenBuffers(1, &mTexturedVbo);
  glBindVertexArray(mTexturedVao);
  glBindBuffer(GL_ARRAY_BUFFER, mTexturedVbo);

  // Position (Location 0)
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(TexturedVertex),
                        reinterpret_cast<void*>(static_cast<uintptr_t>(0)));
  // TexCoord (Location 1)
  glEnableVertexAttribArray(1);
  glVertexAttribPointer(
      1, 2, GL_FLOAT, GL_FALSE, sizeof(TexturedVertex),
      reinterpret_cast<void*>(static_cast<uintptr_t>(2 * sizeof(float))));
  // Color (Location 2)
  glEnableVertexAttribArray(2);
  glVertexAttribPointer(
      2, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(TexturedVertex),
      reinterpret_cast<void*>(static_cast<uintptr_t>(4 * sizeof(float))));

  glBindVertexArray(0);
}

void OpenGLRenderer::Shutdown() {
  if (mGLContext == nullptr) {
    return;
  }

  // GL kaynakları silinmeden önce context'i aktif et.
  SDL_GL_MakeCurrent(mWindow, static_cast<SDL_GLContext>(mGLContext));

  // Shader programlarını context geçerliyken sil; destructor'lar
  // SDL_GL_DestroyContext'ten sonra çalışıyor olurdu → 1282 hatası.
  mBasicShader = ShaderProgram{};
  mTexturedShader = ShaderProgram{};

  if (mVao != 0U) {
    glDeleteVertexArrays(1, &mVao);
    mVao = 0;
  }
  if (mVbo != 0U) {
    glDeleteBuffers(1, &mVbo);
    mVbo = 0;
  }
  if (mTexturedVao != 0U) {
    glDeleteVertexArrays(1, &mTexturedVao);
    mTexturedVao = 0;
  }
  if (mTexturedVbo != 0U) {
    glDeleteBuffers(1, &mTexturedVbo);
    mTexturedVbo = 0;
  }

  SDL_GL_DestroyContext(static_cast<SDL_GLContext>(mGLContext));
  mGLContext = nullptr;
}

void OpenGLRenderer::BeginFrame() {}

void OpenGLRenderer::EndFrame() {
  SDL_GL_SwapWindow(mWindow);
}

void OpenGLRenderer::SetViewport(int32_t x, int32_t y, int32_t width,
                                 int32_t height) {
  glViewport(x, y, width, height);
}

void OpenGLRenderer::SetScissor(int32_t x, int32_t y, int32_t width,
                                int32_t height) {
  glEnable(GL_SCISSOR_TEST);
  glScissor(x, y, width, height);
}

void OpenGLRenderer::ClearScissor() {
  glDisable(GL_SCISSOR_TEST);
}

void OpenGLRenderer::Clear(const Color& color) {
  glClearColor(color.RedF(), color.GreenF(), color.BlueF(), color.AlphaF());
  glClear(GL_COLOR_BUFFER_BIT);
}

void OpenGLRenderer::SetOpacity(float alpha) {
  mOpacity = alpha;
}

void OpenGLRenderer::DrawTriangles(const std::vector<Vertex>& vertices) {
  if (vertices.empty()) {
    return;
  }
  // Arayuz sozlesmesi: ucgen listesi 3'un kati olmali. Eksik bir ucgen
  // sessizce atlanirdi; bunu gorunur kil.
  if (vertices.size() % 3 != 0) {
    spdlog::error(
        "[OpenGLRenderer] DrawTriangles: vertex sayisi ({}) 3'un "
        "kati degil; cizim atlandi.",
        vertices.size());
    return;
  }

  mBasicShader.Use();
  mBasicShader.SetUniformMat4("u_projection", mProjection);
  mBasicShader.SetUniformMat3("u_model", mModel);
  mBasicShader.SetUniformFloat("u_opacity", mOpacity);

  glBindVertexArray(mVao);
  glBindBuffer(GL_ARRAY_BUFFER, mVbo);
  glBufferData(GL_ARRAY_BUFFER,
               static_cast<GLsizeiptr>(vertices.size() * sizeof(Vertex)),
               vertices.data(), GL_STREAM_DRAW);
  glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(vertices.size()));
  glBindVertexArray(0);
}

TextureHandle OpenGLRenderer::CreateTexture(const uint8_t* data, int32_t width,
                                            int32_t height, int32_t channels) {
  if (data == nullptr || width <= 0 || height <= 0) {
    return kInvalidTexture;
  }

  // Kanal sayısı → GL formatı. Eskiden 4 olmayan her değer GL_RGB'ye
  // düşüyordu; 1 veya 2 kanallı veride GL, satır başına `width*3` bayt
  // okuyup buffer'ın dışına taşıyordu.
  GLenum format = GL_RGBA;
  switch (channels) {
    case 1:
      format = GL_RED;
      break;
    case 2:
      format = GL_RG;
      break;
    case 3:
      format = GL_RGB;
      break;
    case 4:
      format = GL_RGBA;
      break;
    default:
      spdlog::error("[OpenGLRenderer] Desteklenmeyen kanal sayısı: {}",
                    channels);
      return kInvalidTexture;
  }

  uint32_t tex_id = 0;
  glGenTextures(1, &tex_id);
  glBindTexture(GL_TEXTURE_2D, tex_id);

  // GL varsayılan unpack hizalaması 4 bayttır. RGB (3 bayt/piksel) veride
  // genişlik 4'ün katı değilse her satır 4'e hizalı okunur ve görüntü
  // kademeli olarak kayar. Piksel verisi sıkı paketli geldiği için 1 yap.
  GLint prev_alignment = 4;
  glGetIntegerv(GL_UNPACK_ALIGNMENT, &prev_alignment);
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

  // Tek/çift kanallı texture'lar shader'da .rgb olarak örneklenebilsin diye
  // swizzle uygula: R -> (R,R,R,1), RG -> (R,R,R,G) (gri tonlama + alfa).
  if (channels == 1) {
    const std::array<GLint, 4> kSwizzle = {GL_RED, GL_RED, GL_RED, GL_ONE};
    glTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_RGBA, kSwizzle.data());
  } else if (channels == 2) {
    const std::array<GLint, 4> kSwizzle = {GL_RED, GL_RED, GL_RED, GL_GREEN};
    glTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_RGBA, kSwizzle.data());
  }

  glTexImage2D(GL_TEXTURE_2D, 0, static_cast<GLint>(format), width, height, 0,
               format, GL_UNSIGNED_BYTE, data);

  glPixelStorei(GL_UNPACK_ALIGNMENT, prev_alignment);
  glBindTexture(GL_TEXTURE_2D, 0);
  return static_cast<TextureHandle>(tex_id);
}

void OpenGLRenderer::UpdateTexture(TextureHandle handle, int32_t x, int32_t y,
                                   int32_t width, int32_t height,
                                   const uint8_t* data) {
  if (handle == kInvalidTexture || data == nullptr || width <= 0 ||
      height <= 0) {
    return;
  }
  glBindTexture(GL_TEXTURE_2D, static_cast<uint32_t>(handle));

  GLint prev_alignment = 4;
  glGetIntegerv(GL_UNPACK_ALIGNMENT, &prev_alignment);
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

  glTexSubImage2D(GL_TEXTURE_2D, 0, x, y, width, height, GL_RGBA,
                  GL_UNSIGNED_BYTE, data);

  glPixelStorei(GL_UNPACK_ALIGNMENT, prev_alignment);
  glBindTexture(GL_TEXTURE_2D, 0);
}

void OpenGLRenderer::DestroyTexture(TextureHandle handle) {
  // GL context yok olmuşsa (Shutdown sonrası) çağrıya gerek yok:
  // context yıkımı zaten tüm GL kaynaklarını serbest bırakır.
  if (handle == kInvalidTexture || mGLContext == nullptr) {
    return;
  }
  auto tex_id = static_cast<uint32_t>(handle);
  glDeleteTextures(1, &tex_id);
}

void OpenGLRenderer::DrawTextured(const std::vector<TexturedVertex>& vertices,
                                  TextureHandle texture) {
  if (vertices.empty() || texture == kInvalidTexture) {
    return;
  }

  mTexturedShader.Use();
  mTexturedShader.SetUniformMat4("u_projection", mProjection);
  mTexturedShader.SetUniformMat3("u_model", mModel);
  mTexturedShader.SetUniformFloat("u_opacity", mOpacity);
  mTexturedShader.SetUniformInt("u_texture", 0);

  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, static_cast<uint32_t>(texture));

  glBindVertexArray(mTexturedVao);
  glBindBuffer(GL_ARRAY_BUFFER, mTexturedVbo);
  glBufferData(
      GL_ARRAY_BUFFER,
      static_cast<GLsizeiptr>(vertices.size() * sizeof(TexturedVertex)),
      vertices.data(), GL_STREAM_DRAW);
  glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(vertices.size()));
  glBindVertexArray(0);
  glBindTexture(GL_TEXTURE_2D, 0);
}

void OpenGLRenderer::SetProjectionMatrix(const float* mat4) {
  std::memcpy(mProjection, mat4, sizeof(mProjection));
}

void OpenGLRenderer::SetModelMatrix(const float* mat3) {
  std::memcpy(mModel, mat3, sizeof(mModel));
}

}  // namespace sdl_painter
