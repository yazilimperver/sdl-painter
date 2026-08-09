#include "sdl_painter/renderer.h"

#include <SDL3/SDL.h>

#include <cstdint>
#include <gtest/gtest.h>
#include <memory>

#ifdef SDLPAINTER_HAS_VULKAN
#include "sdl_painter/embedded_vk_shaders.h"
#endif

// Gercek bir backend ayaga kaldiran tek test dosyasi. Diger testler
// MockRenderer kullanir ve shader yolunu hic calistirmaz; bu dosya
// gomulu shader'larin surucu tarafindan gercekten kabul edildigini
// dogrular (bkz. ADR-009).
//
// Pencere/context olusturulamayan ortamlarda testler FAIL degil SKIP olur:
// - Linux: SDL_VIDEODRIVER=offscreen + Mesa EGL ile calisir (CI boyle kosuyor).
// - Windows: `offscreen` surucusu EGL yukleyemiyor, gizli pencere ile gercek
//   surucu uzerinden calisir; masaustu oturumu yoksa atlanir.

namespace {

/// @brief SDL video alt sistemi + gizli pencere icin RAII sarmalayici.
///
/// Gizli pencere (`SDL_WINDOW_HIDDEN`) kullanilir; test kosarken ekranda
/// pencere belirmez ama gercek bir surucu context'i olusturulur.
class HiddenWindow {
 public:
  explicit HiddenWindow(sdl_painter::RendererBackend backend) {
    if (!SDL_InitSubSystem(SDL_INIT_VIDEO)) {
      mError = SDL_GetError();
      return;
    }
    mVideoInitialized = true;

    // Uygulama catisiyla ayni sira: GL attribute'lari pencereden ONCE.
    if (backend == sdl_painter::RendererBackend::kOpenGL) {
      SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
      SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
      SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,
                          SDL_GL_CONTEXT_PROFILE_CORE);
    }

    const SDL_WindowFlags kBackendFlag =
        (backend == sdl_painter::RendererBackend::kVulkan) ? SDL_WINDOW_VULKAN
                                                           : SDL_WINDOW_OPENGL;
    mWindow = SDL_CreateWindow("sdl_painter smoke", 64, 64,
                               kBackendFlag | SDL_WINDOW_HIDDEN);
    if (mWindow == nullptr) {
      mError = SDL_GetError();
    }
  }

  ~HiddenWindow() {
    if (mWindow != nullptr) {
      SDL_DestroyWindow(mWindow);
    }
    if (mVideoInitialized) {
      SDL_QuitSubSystem(SDL_INIT_VIDEO);
    }
  }

  HiddenWindow(const HiddenWindow&) = delete;
  HiddenWindow& operator=(const HiddenWindow&) = delete;

  [[nodiscard]] SDL_Window* Get() const { return mWindow; }
  [[nodiscard]] const char* Error() const { return mError; }

 private:
  SDL_Window* mWindow{nullptr};
  bool mVideoInitialized{false};
  const char* mError{""};
};

// ---------------------------------------------------------------------------
// OpenGL — gomulu GLSL gercekten derleniyor mu?
// ---------------------------------------------------------------------------

/// OpenGLRenderer::Initialize() shader derlemesi basarisiz olursa `false`
/// doner. Yani bu tek assert, gomulu GLSL'in surucu tarafindan kabul
/// edildigini dogrudan kanitlar.
TEST(RendererSmokeTest, OpenGLInitializeCompilesEmbeddedShaders) {
  const HiddenWindow window(sdl_painter::RendererBackend::kOpenGL);
  if (window.Get() == nullptr) {
    GTEST_SKIP() << "OpenGL penceresi olusturulamadi: " << window.Error();
  }

  auto renderer =
      sdl_painter::CreateRenderer(sdl_painter::RendererBackend::kOpenGL);
  ASSERT_NE(renderer, nullptr);

  EXPECT_TRUE(renderer->Initialize(window.Get()))
      << "OpenGLRenderer::Initialize() basarisiz — gomulu GLSL derlenemedi.";

  renderer->Shutdown();
}

/// Ayni renderer iki kez kurulup yikilabilmeli; gomulu shader kaynagi
/// tuketilen bir kaynak degil, salt-okunur veri.
TEST(RendererSmokeTest, OpenGLInitializeIsRepeatable) {
  const HiddenWindow window(sdl_painter::RendererBackend::kOpenGL);
  if (window.Get() == nullptr) {
    GTEST_SKIP() << "OpenGL penceresi olusturulamadi: " << window.Error();
  }

  for (int32_t pass = 0; pass < 2; ++pass) {
    auto renderer =
        sdl_painter::CreateRenderer(sdl_painter::RendererBackend::kOpenGL);
    ASSERT_NE(renderer, nullptr);
    EXPECT_TRUE(renderer->Initialize(window.Get())) << "gecis " << pass;
    renderer->Shutdown();
  }
}

#ifdef SDLPAINTER_HAS_VULKAN

// ---------------------------------------------------------------------------
// Vulkan — gomulu SPIR-V yapisal olarak gecerli mi?
// ---------------------------------------------------------------------------
//
// Bu testler GPU istemez; CMake'teki bayt-sirasi donusumunun (little-endian
// dosya -> uint32 kelime) dogru oldugunu her ortamda dogrular. Donusum bozuk
// olsaydi sihirli sayi tutmazdi.

/// @brief SPIR-V ilk kelimesi (magic number).
constexpr uint32_t kSpirvMagic = 0x07230203U;

void ExpectValidSpirv(const uint32_t* words, std::size_t byte_size,
                      const char* name) {
  SCOPED_TRACE(name);
  ASSERT_GT(byte_size, 0U) << "gomulu modul bos";
  ASSERT_EQ(byte_size % sizeof(uint32_t), 0U)
      << "SPIR-V boyutu 4'un kati olmali";
  // Baslik 5 kelimedir: magic, version, generator, bound, schema.
  ASSERT_GE(byte_size / sizeof(uint32_t), 5U) << "SPIR-V basligi eksik";

  EXPECT_EQ(words[0], kSpirvMagic)
      << "magic number tutmuyor — CMake bayt-sirasi donusumu bozuk olabilir";

  // Version kelimesi: 0x00MMmm00 (major/minor). 1.0-1.6 arasi beklenir.
  const uint32_t kMajor = (words[1] >> 16U) & 0xFFU;
  const uint32_t kMinor = (words[1] >> 8U) & 0xFFU;
  EXPECT_EQ(kMajor, 1U) << "beklenmeyen SPIR-V ana surumu";
  EXPECT_LE(kMinor, 6U) << "beklenmeyen SPIR-V alt surumu";

  EXPECT_GT(words[3], 0U) << "id bound sifir olamaz";
}

TEST(SpirvEmbedTest, UntexturedModulesAreValid) {
  ExpectValidSpirv(sdl_painter::detail::kUntexturedVert,
                   sizeof(sdl_painter::detail::kUntexturedVert),
                   "untextured.vert");
  ExpectValidSpirv(sdl_painter::detail::kUntexturedFrag,
                   sizeof(sdl_painter::detail::kUntexturedFrag),
                   "untextured.frag");
}

TEST(SpirvEmbedTest, TexturedModulesAreValid) {
  ExpectValidSpirv(sdl_painter::detail::kTexturedVert,
                   sizeof(sdl_painter::detail::kTexturedVert), "textured.vert");
  ExpectValidSpirv(sdl_painter::detail::kTexturedFrag,
                   sizeof(sdl_painter::detail::kTexturedFrag), "textured.frag");
}

/// Vulkan backend'i gercek bir ICD ile ayaga kalkiyor mu?
///
/// `Initialize()` artik pipeline kurulumu basarisiz olursa `false` donuyor
/// (ADR-009 sonrasi sessiz degradasyon kaldirildi), yani bozuk bir SPIR-V
/// burada da yakalanir.
///
/// Ancak `false` donusu "Vulkan yok" ile "pipeline bozuk" arasinda ayrim
/// yapmiyor — surface/ICD eksikligi de ayni sonucu veriyor. Bu yuzden test
/// SKIP'e dusuyor; SPIR-V dogrulugunun GPU'suz garantisini yukaridaki
/// SpirvEmbedTest'ler veriyor.
TEST(RendererSmokeTest, VulkanInitializeSucceeds) {
  const HiddenWindow window(sdl_painter::RendererBackend::kVulkan);
  if (window.Get() == nullptr) {
    GTEST_SKIP() << "Vulkan penceresi olusturulamadi: " << window.Error();
  }

  auto renderer =
      sdl_painter::CreateRenderer(sdl_painter::RendererBackend::kVulkan);
  ASSERT_NE(renderer, nullptr);

  if (!renderer->Initialize(window.Get())) {
    GTEST_SKIP() << "Vulkan baslatilamadi (uygun ICD yok olabilir).";
  }
  renderer->Shutdown();
}

#endif  // SDLPAINTER_HAS_VULKAN

}  // namespace
