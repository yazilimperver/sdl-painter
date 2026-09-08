/// @file test_regression_gpu.cpp
///
/// @brief Gerçek backend üzerinde piksel doğrulayan regresyon testleri.

#include "sdl_painter/brush.h"
#include "sdl_painter/color.h"
#include "sdl_painter/geometry.h"
#include "sdl_painter/image.h"
#include "sdl_painter/painter.h"
#include "sdl_painter/pen.h"
#include "sdl_painter/render_target.h"
#include "sdl_painter/renderer.h"

#include <cstdint>
#include <gtest/gtest.h>
#include <string>
#include <vector>

#include "pixel_test_support.h"

namespace {

using sdl_painter::Brush;
using sdl_painter::Color;
using sdl_painter::Image;
using sdl_painter::Painter;
using sdl_painter::Pen;
using sdl_painter::Rect;
using sdl_painter::RendererBackend;
using sdl_painter::RenderTarget;
using sdl_painter::TextureFilter;
using sdl_painter::testing::AvailableBackends;
using sdl_painter::testing::ColorNear;
using sdl_painter::testing::DumpRgba;
using sdl_painter::testing::PixelAt;

constexpr int32_t kW = 64;
constexpr int32_t kH = 32;
constexpr int32_t kTolerance = 2;

const Color kBlack{0, 0, 0, 255};
const Color kRed{255, 0, 0, 255};
const Color kBlue{0, 0, 255, 255};

/// @brief 2x2 tek renk RGBA verisi.
std::vector<uint8_t> SolidPixels(const Color& c) {
  std::vector<uint8_t> data;
  data.reserve(2U * 2U * 4U);
  for (int32_t i = 0; i < 4; ++i) {
    data.push_back(c.r);
    data.push_back(c.g);
    data.push_back(c.b);
    data.push_back(c.a);
  }
  return data;
}

/// @brief Büyütüldüğünde kenar karışmasın diye nearest filtreli görüntü.
Image MakeSolidImage(const Color& c) {
  const std::vector<uint8_t> pixels = SolidPixels(c);
  Image img = Image::CreateFromData(pixels.data(), 2, 2, 4);
  img.SetFilter(TextureFilter::kNearest);
  return img;
}

class RegressionGpu : public ::testing::TestWithParam<RendererBackend> {
 protected:
  /// @brief Dokum dosyalarini backend'e gore adlandirmak icin.
  [[nodiscard]] std::string Tag() const {
    return GetParam() == RendererBackend::kOpenGL ? "opengl" : "vulkan";
  }
};

// Hedefe yeniden baglanma onceki icerigi korumali.
//
// Vulkan'da offscreen render pass her girisde loadOp=CLEAR ile aciliyor;
// OpenGL'de FBO baglamak temizleme yapmiyor. Ayni cagri dizisi iki backend'de
// farkli sonuc veriyor.
TEST_P(RegressionGpu, RebindingTargetPreservesContents) {
  REQUIRE_BACKEND(be, GetParam());
  Painter& p = *be.painter;

  RenderTarget a = p.CreateRenderTarget(kW, kH);
  ASSERT_TRUE(a.IsValid());

  p.Begin();
  ASSERT_TRUE(p.SetRenderTarget(a));
  p.Clear(kBlack);
  p.SetPen(Pen::NoPen());
  p.SetBrush(Brush(kRed));
  p.FillRect(0.0F, 0.0F, 32.0F, 32.0F);
  p.ResetRenderTarget();

  // Ikinci baglanma; bilerek Clear yok.
  ASSERT_TRUE(p.SetRenderTarget(a));
  p.SetBrush(Brush(kBlue));
  p.FillRect(32.0F, 0.0F, 32.0F, 32.0F);
  p.ResetRenderTarget();
  p.End();

  std::vector<uint8_t> px;
  ASSERT_TRUE(p.ReadRenderTarget(a, px));
  DumpRgba("rebind_" + Tag(), px, kW, kH);
  EXPECT_TRUE(ColorNear(PixelAt(px, kW, 16, 16), kRed, kTolerance))
      << "yeniden baglanma ilk cizimi silmemeliydi";
  EXPECT_TRUE(ColorNear(PixelAt(px, kW, 48, 16), kBlue, kTolerance))
      << "ikinci cizim hedefe gitmeliydi";
}

// Ayni karede UpdateImage, kendisinden önce kaydedilmis cizimleri
// etkilememeli.
//
// UpdateImage biriken cizimleri flush ettigini belgeliyor. Vulkan'da flush
// yalnizca frame komut tamponuna kayit yapiyor; doku guncellemesi ise ayri
// komutla hemen gonderiliyor. Frame sonradan gonderildiginde daha once
// kaydedilmis cizim de yeni dokuyu ornekliyor.
TEST_P(RegressionGpu, ImageUpdateDoesNotAffectEarlierDraws) {
  REQUIRE_BACKEND(be, GetParam());
  Painter& p = *be.painter;

  Image img = MakeSolidImage(kRed);
  ASSERT_TRUE(img.IsValid());
  const std::vector<uint8_t> blue = SolidPixels(kBlue);

  RenderTarget t = p.CreateRenderTarget(kW, kH);
  ASSERT_TRUE(t.IsValid());

  p.Begin();
  ASSERT_TRUE(p.SetRenderTarget(t));
  p.Clear(kBlack);
  p.DrawImage(img, Rect{0.0F, 0.0F, 32.0F, 32.0F});
  p.UpdateImage(img, blue.data());
  p.DrawImage(img, Rect{32.0F, 0.0F, 32.0F, 32.0F});
  p.ResetRenderTarget();
  p.End();

  std::vector<uint8_t> px;
  ASSERT_TRUE(p.ReadRenderTarget(t, px));
  DumpRgba("update_" + Tag(), px, kW, kH);
  EXPECT_TRUE(ColorNear(PixelAt(px, kW, 16, 16), kRed, kTolerance))
      << "guncellemeden onceki cizim eski icerikle kalmaliydi";
  EXPECT_TRUE(ColorNear(PixelAt(px, kW, 48, 16), kBlue, kTolerance))
      << "guncellemeden sonraki cizim yeni icerigi kullanmaliydi";
}

// Yukaridaki testin fixture hatasindan ayrilmasi icin iki kontrol.
TEST_P(RegressionGpu, ControlBothDrawsRedWithoutUpdate) {
  REQUIRE_BACKEND(be, GetParam());
  Painter& p = *be.painter;

  Image img = MakeSolidImage(kRed);
  ASSERT_TRUE(img.IsValid());
  RenderTarget t = p.CreateRenderTarget(kW, kH);
  ASSERT_TRUE(t.IsValid());

  p.Begin();
  ASSERT_TRUE(p.SetRenderTarget(t));
  p.Clear(kBlack);
  p.DrawImage(img, Rect{0.0F, 0.0F, 32.0F, 32.0F});
  p.DrawImage(img, Rect{32.0F, 0.0F, 32.0F, 32.0F});
  p.ResetRenderTarget();
  p.End();

  std::vector<uint8_t> px;
  ASSERT_TRUE(p.ReadRenderTarget(t, px));
  DumpRgba("control_noupdate_" + Tag(), px, kW, kH);
  EXPECT_TRUE(ColorNear(PixelAt(px, kW, 16, 16), kRed, kTolerance));
  EXPECT_TRUE(ColorNear(PixelAt(px, kW, 48, 16), kRed, kTolerance));
}

TEST_P(RegressionGpu, ControlBothDrawsBlueWhenUpdatedFirst) {
  REQUIRE_BACKEND(be, GetParam());
  Painter& p = *be.painter;

  Image img = MakeSolidImage(kRed);
  ASSERT_TRUE(img.IsValid());
  const std::vector<uint8_t> blue = SolidPixels(kBlue);
  RenderTarget t = p.CreateRenderTarget(kW, kH);
  ASSERT_TRUE(t.IsValid());

  p.Begin();
  ASSERT_TRUE(p.SetRenderTarget(t));
  p.Clear(kBlack);
  p.UpdateImage(img, blue.data());
  p.DrawImage(img, Rect{0.0F, 0.0F, 32.0F, 32.0F});
  p.DrawImage(img, Rect{32.0F, 0.0F, 32.0F, 32.0F});
  p.ResetRenderTarget();
  p.End();

  std::vector<uint8_t> px;
  ASSERT_TRUE(p.ReadRenderTarget(t, px));
  DumpRgba("control_updatefirst_" + Tag(), px, kW, kH);
  EXPECT_TRUE(ColorNear(PixelAt(px, kW, 16, 16), kBlue, kTolerance));
  EXPECT_TRUE(ColorNear(PixelAt(px, kW, 48, 16), kBlue, kTolerance));
}

INSTANTIATE_TEST_SUITE_P(
    Backends, RegressionGpu, ::testing::ValuesIn(AvailableBackends()),
    [](const ::testing::TestParamInfo<RendererBackend>& i) {
      return i.param == RendererBackend::kOpenGL ? "OpenGL" : "Vulkan";
    });

}  // namespace
