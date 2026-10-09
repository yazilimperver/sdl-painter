#include "sdl_painter/renderer.h"

#include <SDL3/SDL.h>

#include <cstdint>
#include <gtest/gtest.h>
#include <memory>

#include "test_support.h"

#ifdef SDLPAINTER_HAS_VULKAN
#include "sdl_painter/embedded_vk_shaders.h"
#endif

// Gercek backend ayaga kaldiran dosyalardan biri (digerleri:
// test_frame_render, test_render_target, test_backend_parity,
// test_regression_gpu, test_regression_findings, test_public_abi). Oradaki
// testler cizim sonucunu sinar; bu dosya ise yalnizca kurulumu: gomulu
// shader'larin surucu tarafindan gercekten kabul edildigini ve renderer'in
// ayni surecte tekrar kurulabildigini dogrular (bkz. ADR-009). MockRenderer
// kullanan testler shader yolunu hic calistirmaz.
//
// Pencere/context olusturulamayan ortamlarda testler FAIL degil SKIP olur
// (SDLPAINTER_REQUIRE_BACKEND=1 ayarliysa FAIL):
// - Linux: SDL_VIDEODRIVER=offscreen + Mesa EGL ile calisir (CI boyle kosuyor).
// - Windows: `offscreen` surucusu EGL yukleyemiyor, gizli pencere ile gercek
//   surucu uzerinden calisir; masaustu oturumu yoksa atlanir.

namespace {

// ---------------------------------------------------------------------------
// OpenGL: gomulu GLSL gercekten derleniyor mu?
// ---------------------------------------------------------------------------

/// OpenGLRenderer::Initialize() shader derlemesi basarisiz olursa `false`
/// doner. Yani bu tek assert, gomulu GLSL'in surucu tarafindan kabul
/// edildigini dogrudan kanitlar.
TEST(RendererSmokeTest, OpenGLInitializeCompilesEmbeddedShaders) {
  const sdl_painter::testing::HiddenWindow window(
      sdl_painter::RendererBackend::kOpenGL);
  if (window.Get() == nullptr) {
    SDLPAINTER_BACKEND_UNAVAILABLE(
        std::string("OpenGL penceresi olusturulamadi: ") + window.Error());
  }

  auto renderer =
      sdl_painter::CreateRenderer(sdl_painter::RendererBackend::kOpenGL);
  ASSERT_NE(renderer, nullptr);

  EXPECT_TRUE(renderer->Initialize(window.Get()))
      << "OpenGLRenderer::Initialize() basarisiz: gomulu GLSL derlenemedi.";

  renderer->Shutdown();
}

/// Ayni renderer iki kez kurulup yikilabilmeli; gomulu shader kaynagi
/// tuketilen bir kaynak degil, salt-okunur veri.
TEST(RendererSmokeTest, OpenGLInitializeIsRepeatable) {
  const sdl_painter::testing::HiddenWindow window(
      sdl_painter::RendererBackend::kOpenGL);
  if (window.Get() == nullptr) {
    SDLPAINTER_BACKEND_UNAVAILABLE(
        std::string("OpenGL penceresi olusturulamadi: ") + window.Error());
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
// Vulkan: gomulu SPIR-V yapisal olarak gecerli mi?
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
      << "magic number tutmuyor; CMake bayt-sirasi donusumu bozuk olabilir";

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
/// yapmiyor; surface/ICD eksikligi de ayni sonucu veriyor. Bu yuzden test
/// SKIP'e dusuyor (SDLPAINTER_REQUIRE_BACKEND=1 ise FAIL); SPIR-V
/// dogrulugunun GPU'suz garantisini yukaridaki SpirvEmbedTest'ler veriyor.
TEST(RendererSmokeTest, VulkanInitializeSucceeds) {
  // Pencere ve renderer'dan once kurulur, onlardan sonra yikilir: kurulum ve
  // kapatma sirasindaki validation hatalari da yakalanir.
  const sdl_painter::testing::ValidationGuard guard;
  const sdl_painter::testing::HiddenWindow window(
      sdl_painter::RendererBackend::kVulkan);
  if (window.Get() == nullptr) {
    SDLPAINTER_BACKEND_UNAVAILABLE(
        std::string("Vulkan penceresi olusturulamadi: ") + window.Error());
  }

  auto renderer =
      sdl_painter::CreateRenderer(sdl_painter::RendererBackend::kVulkan);
  ASSERT_NE(renderer, nullptr);

  if (!renderer->Initialize(window.Get())) {
    SDLPAINTER_BACKEND_UNAVAILABLE(
        "Vulkan baslatilamadi (uygun ICD yok olabilir).");
  }
  renderer->Shutdown();
}

/// OpenGLInitializeIsRepeatable'in Vulkan esi: ayni surecte, ayni pencerede
/// Vulkan iki kez kurulup yikilabilmeli (instance, cihaz, swapchain ve gomulu
/// SPIR-V pipeline'i her seferinde sifirdan). ctest her testi ayri surecte
/// kosar; bu yol yalnizca ikili dogrudan calistirilinca ikinci kurulumu
/// gorur. Validation koruyucusu iki turun da temiz oldugunu sinar.
TEST(RendererSmokeTest, VulkanInitializeIsRepeatable) {
  const sdl_painter::testing::ValidationGuard guard;
  const sdl_painter::testing::HiddenWindow window(
      sdl_painter::RendererBackend::kVulkan);
  if (window.Get() == nullptr) {
    SDLPAINTER_BACKEND_UNAVAILABLE(
        std::string("Vulkan penceresi olusturulamadi: ") + window.Error());
  }

  // Ilk kurulum ortamin Vulkan'i destekleyip desteklemedigini ayirt eder.
  auto first =
      sdl_painter::CreateRenderer(sdl_painter::RendererBackend::kVulkan);
  ASSERT_NE(first, nullptr);
  if (!first->Initialize(window.Get())) {
    SDLPAINTER_BACKEND_UNAVAILABLE(
        "Vulkan baslatilamadi (uygun ICD yok olabilir).");
  }
  first->Shutdown();
  first.reset();

  // Ilk kurulum basardiysa ikincinin basarisizligi gercek bir hatadir.
  auto second =
      sdl_painter::CreateRenderer(sdl_painter::RendererBackend::kVulkan);
  ASSERT_NE(second, nullptr);
  EXPECT_TRUE(second->Initialize(window.Get()))
      << "ikinci Vulkan kurulumu basarisiz (ilki basarili olmustu).";
  second->Shutdown();
}

#endif  // SDLPAINTER_HAS_VULKAN

}  // namespace
