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

  // Baslangic karistirma durumu SetBlendMode uzerinden kurulur. Eskiden burada
  // ayri bir glBlendFunc cagrisi vardi; SetBlendMode duzeltilince o satir
  // sessizce eskidi ve mod hic degistirilmeyen sahnelerde ESKI faktorler
  // yururlukte kaldi. Tek kaynak: SetBlendMode.
  SetBlendMode(BlendMode::kAlpha);

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
  SetupTimerQueries();
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

  if (mTimerQueries[0] != 0U) {
    if (mTimerQueryActive) {
      glEndQuery(GL_TIME_ELAPSED);
      mTimerQueryActive = false;
    }
    glDeleteQueries(kTimerQueryCount, mTimerQueries);
    mTimerQueries[0] = 0;
  }
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

  // Kullanicinin serbest birakmadigi hedefler: context yikilmadan once sil ki
  // ayikla derlemede sizinti araclari temiz rapor versin.
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  mCurrentTarget = kInvalidRenderTarget;
  for (auto& entry : mRenderTargets) {
    glDeleteFramebuffers(1, &entry.second.fbo);
    glDeleteTextures(1, &entry.second.texture);
  }
  mRenderTargets.clear();

  SDL_GL_DestroyContext(static_cast<SDL_GLContext>(mGLContext));
  mGLContext = nullptr;
}

void OpenGLRenderer::SetupTimerQueries() {
  glGenQueries(kTimerQueryCount, mTimerQueries);
  for (bool& pending : mTimerQueryPending) {
    pending = false;
  }
  mTimerQueryIndex = 0;
  mTimerQueryActive = false;
  mLastGpuFrameMs = 0.0;
}

void OpenGLRenderer::CollectGpuTime() {
  // Bu karenin sorgusunu DEGIL, siradaki (yani en eski) sorguyu yokla.
  // Cift tamponda "siradaki" bir onceki karenin sorgusudur ve sonucu
  // muhtemelen hazirdir; hazir degilse beklemeden geciyoruz.
  const int32_t oldest = (mTimerQueryIndex + 1) % kTimerQueryCount;
  if (!mTimerQueryPending[oldest]) {
    return;
  }
  GLint available = 0;
  glGetQueryObjectiv(mTimerQueries[oldest], GL_QUERY_RESULT_AVAILABLE,
                     &available);
  if (available == GL_FALSE) {
    return;
  }
  GLuint64 elapsed_ns = 0;
  glGetQueryObjectui64v(mTimerQueries[oldest], GL_QUERY_RESULT, &elapsed_ns);
  mLastGpuFrameMs = static_cast<double>(elapsed_ns) / 1.0e6;
  mTimerQueryPending[oldest] = false;
}

void OpenGLRenderer::BeginFrame() {
  if (mTimerQueries[0] == 0) {
    return;
  }
  CollectGpuTime();

  // Bu slotun onceki sonucu hala toplanmadiysa uzerine yazmak sorguyu
  // kaybettirir; o kareyi olcmeden gec.
  if (mTimerQueryPending[mTimerQueryIndex]) {
    return;
  }
  glBeginQuery(GL_TIME_ELAPSED, mTimerQueries[mTimerQueryIndex]);
  mTimerQueryActive = true;
}

void OpenGLRenderer::EndFrame() {
  if (mTimerQueryActive) {
    glEndQuery(GL_TIME_ELAPSED);
    mTimerQueryPending[mTimerQueryIndex] = true;
    mTimerQueryActive = false;
    mTimerQueryIndex = (mTimerQueryIndex + 1) % kTimerQueryCount;
  }
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

void OpenGLRenderer::SetBlendMode(BlendMode mode) {
  // Neden glBlendFunc DEGIL de glBlendFuncSeparate:
  //
  // glBlendFunc alfa kanalina da RENK faktorlerini uygular. Vulkan tarafinda
  // ise alfa faktorleri ayri alanlardir (srcAlphaBlendFactor /
  // dstAlphaBlendFactor) ve orada bilincli olarak farkli degerler secilmisti.
  // Sonuc: iki backend ayni cizimde ayni RGB'yi ama FARKLI alfayi uretiyordu.
  //
  // Ekranda gorunmuyordu — swapchain'in alfasi sunumda yok sayilir. Bir cizim
  // hedefine cizilince ortaya cikti ve test_backend_parity.cpp yakaladi.
  //
  // Secilen alfa formulu her iki backend'de de ayni (bkz. vk_blend.h):
  // aOut = aSrc + aDst * (1 - aSrc) — standart "over" bilesimi. Eski GL
  // davranisi (aSrc^2 + aDst(1-aSrc)) alfayi eksik biriktiriyordu; yari
  // saydam bir sekil cizilen hedef, olmasi gerekenden saydam kaliyordu.
  switch (mode) {
    case BlendMode::kNone:
      glDisable(GL_BLEND);
      return;
    case BlendMode::kAdditive:
      glEnable(GL_BLEND);
      // Kaynak alfasiyla olceklenip eklenir: ust uste binen parlak nesneler
      // birbirini soner degil, parlatir. Alfa da birikir.
      glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE, GL_ONE, GL_ONE);
      return;
    case BlendMode::kMultiply:
      glEnable(GL_BLEND);
      glBlendFuncSeparate(GL_DST_COLOR, GL_ZERO, GL_DST_ALPHA, GL_ZERO);
      return;
    case BlendMode::kAlpha:
    default:
      glEnable(GL_BLEND);
      glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE,
                          GL_ONE_MINUS_SRC_ALPHA);
      return;
  }
}

TextureHandle OpenGLRenderer::CreateTexture(const uint8_t* data, int32_t width,
                                            int32_t height, int32_t channels) {
  return CreateTexture(data, width, height, channels, TextureFilter::kLinear);
}

TextureHandle OpenGLRenderer::CreateTexture(const uint8_t* data, int32_t width,
                                            int32_t height, int32_t channels,
                                            TextureFilter filter) {
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

  // Buyutmede (MAG) fark gorunur; kucultmede (MIN) de ayni filtre kullanilir
  // ki piksel sanati olceklendikce tutarli kalsin.
  const GLint kFilter =
      (filter == TextureFilter::kNearest) ? GL_NEAREST : GL_LINEAR;
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, kFilter);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, kFilter);
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

// --- Çizim hedefleri (FBO) -------------------------------------------------

RenderTargetHandle OpenGLRenderer::CreateRenderTarget(int32_t width,
                                                      int32_t height,
                                                      TextureFilter filter) {
  if (mGLContext == nullptr || width <= 0 || height <= 0) {
    return kInvalidRenderTarget;
  }

  RenderTargetGL target;
  target.width = width;
  target.height = height;

  glGenTextures(1, &target.texture);
  glBindTexture(GL_TEXTURE_2D, target.texture);
  // Format bilincli olarak GL_RGBA8: hedeflerin icerigi geri okunabiliyor ve
  // okuma sozlesmesi backend'e gore degismemeli (bkz. renderer.h).
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA,
               GL_UNSIGNED_BYTE, nullptr);
  const GLint kFilter =
      (filter == TextureFilter::kNearest) ? GL_NEAREST : GL_LINEAR;
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, kFilter);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, kFilter);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glBindTexture(GL_TEXTURE_2D, 0);

  glGenFramebuffers(1, &target.fbo);
  glBindFramebuffer(GL_FRAMEBUFFER, target.fbo);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                         target.texture, 0);

  const GLenum kStatus = glCheckFramebufferStatus(GL_FRAMEBUFFER);
  // Yururlukteki hedefe geri don; kurulum arada gecici olarak baglamisti.
  BindTargetFramebuffer(mCurrentTarget);

  if (kStatus != GL_FRAMEBUFFER_COMPLETE) {
    spdlog::error("[OpenGLRenderer] Framebuffer eksik: 0x{:X}", kStatus);
    glDeleteFramebuffers(1, &target.fbo);
    glDeleteTextures(1, &target.texture);
    return kInvalidRenderTarget;
  }

  const RenderTargetHandle handle = mNextRenderTarget++;
  mRenderTargets.emplace(handle, target);
  return handle;
}

void OpenGLRenderer::DestroyRenderTarget(RenderTargetHandle handle) {
  // Context yok olmussa GL kaynaklarini silmeye gerek yok; context yikimi
  // zaten hepsini serbest birakir (DestroyTexture ile ayni gerekce).
  const auto it = mRenderTargets.find(handle);
  if (it == mRenderTargets.end()) {
    return;
  }
  if (mCurrentTarget == handle) {
    SetRenderTarget(kInvalidRenderTarget);
  }
  if (mGLContext != nullptr) {
    glDeleteFramebuffers(1, &it->second.fbo);
    glDeleteTextures(1, &it->second.texture);
  }
  mRenderTargets.erase(it);
}

TextureHandle OpenGLRenderer::GetRenderTargetTexture(
    RenderTargetHandle handle) const {
  const auto it = mRenderTargets.find(handle);
  return it == mRenderTargets.end()
             ? kInvalidTexture
             : static_cast<TextureHandle>(it->second.texture);
}

bool OpenGLRenderer::SetRenderTarget(RenderTargetHandle handle) {
  if (handle != kInvalidRenderTarget &&
      mRenderTargets.find(handle) == mRenderTargets.end()) {
    return false;
  }
  mCurrentTarget = handle;
  BindTargetFramebuffer(handle);
  return true;
}

void OpenGLRenderer::BindTargetFramebuffer(RenderTargetHandle handle) {
  const auto it = mRenderTargets.find(handle);
  glBindFramebuffer(GL_FRAMEBUFFER,
                    it == mRenderTargets.end() ? 0U : it->second.fbo);
}

bool OpenGLRenderer::ReadRenderTarget(RenderTargetHandle handle,
                                      uint8_t* out_rgba,
                                      std::size_t byte_capacity) {
  const auto it = mRenderTargets.find(handle);
  if (it == mRenderTargets.end() || out_rgba == nullptr) {
    return false;
  }
  const RenderTargetGL& target = it->second;
  const auto kNeeded = static_cast<std::size_t>(target.width) *
                       static_cast<std::size_t>(target.height) * 4U;
  if (byte_capacity < kNeeded) {
    spdlog::error(
        "[OpenGLRenderer] ReadRenderTarget: tampon kucuk ({} < {} bayt).",
        byte_capacity, kNeeded);
    return false;
  }

  glFinish();
  glBindFramebuffer(GL_FRAMEBUFFER, target.fbo);

  GLint prev_alignment = 4;
  glGetIntegerv(GL_PACK_ALIGNMENT, &prev_alignment);
  glPixelStorei(GL_PACK_ALIGNMENT, 1);

  // Satir sirasi: hedefe cizerken projeksiyonun Y'si TERS CEVRILMEZ
  // (bkz. Painter::UpdateProjection), yani Painter'in y=0'i FBO'nun 0.
  // satirina dusuyor. Dolayisiyla glReadPixels ciktisi dogrudan yukaridan
  // asagi siralidir ve ekstra bir cevirme GEREKMEZ — ekran icin yazilmis
  // examples/benchmarks/screenshot.cpp'den farki budur.
  glReadPixels(0, 0, target.width, target.height, GL_RGBA, GL_UNSIGNED_BYTE,
               out_rgba);

  glPixelStorei(GL_PACK_ALIGNMENT, prev_alignment);
  BindTargetFramebuffer(mCurrentTarget);
  return true;
}

void OpenGLRenderer::SetProjectionMatrix(const float* mat4) {
  std::memcpy(mProjection, mat4, sizeof(mProjection));
}

void OpenGLRenderer::SetModelMatrix(const float* mat3) {
  std::memcpy(mModel, mat3, sizeof(mModel));
}

}  // namespace sdl_painter
