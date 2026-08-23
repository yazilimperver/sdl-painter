// Public ABI / paylasimli kutuphane (DLL) testleri.
//
// NEDEN AYRI BIR HEDEF:
// `sdl_painter_tests` beyaz kutu bir takim — `src/` altindaki DAHILI basliklari
// (tessellator.h, render_batcher.h, vk_check.h ...) include eder ve o
// sembolleri dogrudan cagirir. Bu semboller bilincli olarak export EDILMEZ,
// dolayisiyla o takim `BUILD_SHARED_LIBS=ON` ile link edilemez (ve edilmemeli;
// dahili detaylar ABI yuzeyine cikmamali).
//
// Bu dosya ise YALNIZCA `include/sdl_painter/` altindaki public basliklari
// kullanir ve hedefi `src/` include yoluna sahip DEGILDIR. Boylece:
//   * static derlemede normal bir birim testi gibi kosar,
//   * shared derlemede kutuphaneye SADECE import library uzerinden baglanir.
//
// Yakaladigi hata sinifi: yeni bir public sinif/fonksiyon eklenip
// SDLPAINTER_API ile isaretlenmeyi unutulursa, shared derlemede bu hedef
// LNK2019 ile DUSER. Windows'ta export isaretlemesi olmadan DLL uretilir ama
// import library uretilmez; bu sessiz bir paketleme hatasiydi.

#include "sdl_painter/app/application.h"
#include "sdl_painter/brush.h"
#include "sdl_painter/color.h"
#include "sdl_painter/font.h"
#include "sdl_painter/geometry.h"
#include "sdl_painter/image.h"
#include "sdl_painter/painter.h"
#include "sdl_painter/pen.h"
#include "sdl_painter/renderer.h"
#include "sdl_painter/version.h"

#include <cstdint>
#include <gtest/gtest.h>
#include <memory>
#include <string>
#include <utility>

#include "mock_renderer.h"

namespace {

using sdl_painter::MockRenderer;

constexpr int32_t kViewportWidth = 320;
constexpr int32_t kViewportHeight = 240;

// --- Kutuphane sinirini gecen semboller -------------------------------------

// CreateRenderer derlenmis bir semboldur (renderer.cpp). Basliktan gelmez;
// cozulmesi kutuphanenin gercekten link edildigini kanitlar.
TEST(PublicAbi, CreateRendererResolvesAcrossLibraryBoundary) {
  auto renderer =
      sdl_painter::CreateRenderer(sdl_painter::RendererBackend::kOpenGL);
  EXPECT_NE(renderer, nullptr);
}

#ifdef SDLPAINTER_HAS_VULKAN
TEST(PublicAbi, CreateRendererVulkanResolvesAcrossLibraryBoundary) {
  auto renderer =
      sdl_painter::CreateRenderer(sdl_painter::RendererBackend::kVulkan);
  EXPECT_NE(renderer, nullptr);
}
#endif

// version.h saf constexpr; derleme zamaninda cozulur, link gerektirmez.
// Yine de paketlenmis basligin tutarli oldugunu dogrular.
TEST(PublicAbi, VersionHeaderIsSelfConsistent) {
  const std::string expected = std::to_string(sdl_painter::kVersionMajor) +
                               "." +
                               std::to_string(sdl_painter::kVersionMinor) +
                               "." + std::to_string(sdl_painter::kVersionPatch);
  EXPECT_EQ(expected, std::string(sdl_painter::kVersionString));
}

// --- Disa acik siniflarin yasam dongusu -------------------------------------
//
// Kurucu ve yikicilarin .cpp'de govdesi var; sinif export edilmezse bu testler
// link asamasinda duser.

TEST(PublicAbi, PainterConstructsAndDestructsAcrossBoundary) {
  auto renderer = std::make_unique<MockRenderer>();
  sdl_painter::Painter painter(std::move(renderer), kViewportWidth,
                               kViewportHeight);
  EXPECT_TRUE(painter.IsValid());
}

TEST(PublicAbi, PainterWithNullRendererIsInvalid) {
  sdl_painter::Painter painter(nullptr, kViewportWidth, kViewportHeight);
  EXPECT_FALSE(painter.IsValid());
}

TEST(PublicAbi, PainterIsMovableAcrossBoundary) {
  sdl_painter::Painter first(std::make_unique<MockRenderer>(), kViewportWidth,
                             kViewportHeight);
  sdl_painter::Painter second(std::move(first));
  EXPECT_TRUE(second.IsValid());
}

// Image'in varsayilan kurucusu satir ici, yikicisi .cpp'de.
TEST(PublicAbi, DefaultImageIsInvalidAndDestructsCleanly) {
  const sdl_painter::Image image;
  EXPECT_FALSE(image.IsValid());
}

TEST(PublicAbi, ImageFromMissingFileFailsGracefully) {
  const sdl_painter::Image image("bu-dosya-yok-abi-testi.png");
  EXPECT_FALSE(image.IsValid());
}

// Font'un varsayilan kurucusu bilincli olarak .cpp'ye tasindi: sinif dllexport
// edildiginde satir ici govde, eksik tip olan GlyphAtlas icin unique_ptr
// yikicisini ornekletiyordu. Bu test o kurucuyu sinirin otesinden cagirir.
TEST(PublicAbi, DefaultFontIsInvalidAndDestructsCleanly) {
  const sdl_painter::Font font;
  EXPECT_FALSE(font.IsValid());
}

TEST(PublicAbi, FontFromMissingFileFailsGracefully) {
  const sdl_painter::Font font("bu-font-yok-abi-testi.ttf", 16);
  EXPECT_FALSE(font.IsValid());
}

// --- Uzatma noktasi: IRenderer ----------------------------------------------
//
// README "yeni backend eklemek = yalnizca IRenderer implemente etmek" diyor.
// MockRenderer bunu tuketici tarafinda yapiyor: asagidaki test, sinirin
// otesinde tanimlanmis bir alt sinifin vtable'inin kutuphane icinden dogru
// cagrildigini gosterir.
TEST(PublicAbi, ConsumerDefinedRendererReceivesDrawCalls) {
  auto renderer = std::make_unique<MockRenderer>();
  MockRenderer* observer = renderer.get();

  sdl_painter::Painter painter(std::move(renderer), kViewportWidth,
                               kViewportHeight);
  ASSERT_TRUE(painter.IsValid());

  painter.Begin();
  painter.SetBrush(sdl_painter::Brush(sdl_painter::Color::Red()));
  painter.FillRect(10.0F, 10.0F, 50.0F, 25.0F);
  painter.End();

  EXPECT_FALSE(observer->calls.empty())
      << "Kutuphane, tuketici tarafinda tanimlanmis IRenderer'i hic cagirmadi.";
}

TEST(PublicAbi, ConsumerDefinedRendererReceivesClear) {
  auto renderer = std::make_unique<MockRenderer>();
  MockRenderer* observer = renderer.get();

  sdl_painter::Painter painter(std::move(renderer), kViewportWidth,
                               kViewportHeight);
  ASSERT_TRUE(painter.IsValid());

  painter.Begin();
  painter.Clear(sdl_painter::Color::Black());
  painter.End();

  EXPECT_FALSE(observer->calls.empty());
}

// --- sdl_painter_app: ayri kutuphane, ayri export makrosu -------------------
//
// Application AYRI bir hedeftir (shared derlemede ayri bir DLL) ve kendi
// SDLPAINTER_APP_API makrosunu kullanir. Uye fonksiyonun ADRESINI almak,
// hicbir sey calistirmadan link-zamani cozunurlugu zorlar — Application'i
// gercekten kurmak SDL init + pencere gerektirirdi.
TEST(PublicAbi, ApplicationSymbolsResolveAcrossBoundary) {
  void (sdl_painter::Application::*quit)() noexcept =
      &sdl_painter::Application::Quit;
  EXPECT_NE(quit, nullptr);
}

// --- Satir ici tipler --------------------------------------------------------
//
// Pen, Brush, Color ve geometri tipleri tamamen basliktadir; export
// isaretlemesi GEREKMEZ. Bu test, onlarin oyle kalmasini (yani sessizce .cpp'ye
// tasinip export edilmeden kalmamalarini) tuketici tarafindan dogrular.

TEST(PublicAbi, HeaderOnlyTypesUsableWithoutLinking) {
  const sdl_painter::Color color = sdl_painter::Color::Red();
  const sdl_painter::Pen pen(color, 2.0F);
  const sdl_painter::Brush brush(color);
  const sdl_painter::Rect rect{0.0F, 0.0F, 100.0F, 50.0F};

  EXPECT_EQ(color.r, 255);
  EXPECT_FLOAT_EQ(pen.GetWidth(), 2.0F);
  EXPECT_EQ(brush.GetColor().a, 255);
  EXPECT_FLOAT_EQ(rect.w, 100.0F);
}

}  // namespace
