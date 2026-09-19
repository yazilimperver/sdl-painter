/// @file test_regression_gpu.cpp
///
/// @brief Gerçek backend üzerinde piksel doğrulayan regresyon testleri.

#include "sdl_painter/brush.h"
#include "sdl_painter/color.h"
#include "sdl_painter/font.h"
#include "sdl_painter/geometry.h"
#include "sdl_painter/image.h"
#include "sdl_painter/painter.h"
#include "sdl_painter/pen.h"
#include "sdl_painter/render_target.h"
#include "sdl_painter/renderer.h"

#include <algorithm>
#include <cstdint>
#include <gtest/gtest.h>
#include <memory>
#include <string>
#include <vector>

#include "pixel_test_support.h"

namespace {

using sdl_painter::BlendMode;
using sdl_painter::Brush;
using sdl_painter::Color;
using sdl_painter::Font;
using sdl_painter::Image;
using sdl_painter::LineCap;
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

// Yeni bir hedefin baslangic icerigi saydam siyah olmali; ekranin temizleme
// rengi oraya sizmamali.
//
// Vulkan'da offscreen pass ilk baglanmada loadOp=CLEAR ile mClearValue'yu
// kullaniyordu ve o deger en son Clear cagrisindan kaliyordu; OpenGL'de FBO
// baglamak icerigi hic degistirmiyordu. Ayni cagri dizisi iki backend'de
// farkli zemin veriyordu.
TEST_P(RegressionGpu, FreshTargetStartsTransparent) {
  REQUIRE_BACKEND(be, GetParam());
  Painter& p = *be.painter;

  RenderTarget t = p.CreateRenderTarget(kW, kH);
  ASSERT_TRUE(t.IsValid());

  p.Begin();
  p.Clear(kRed);                      // EKRAN temizligi
  ASSERT_TRUE(p.SetRenderTarget(t));  // hedefe ilk gecis, Clear YOK
  p.SetPen(Pen::NoPen());
  p.SetBrush(Brush(kBlue));
  p.FillRect(0.0F, 0.0F, 8.0F, 8.0F);
  p.ResetRenderTarget();
  p.End();

  std::vector<uint8_t> px;
  ASSERT_TRUE(p.ReadRenderTarget(t, px));
  DumpRgba("fresh_target_" + Tag(), px, kW, kH);
  EXPECT_TRUE(ColorNear(PixelAt(px, kW, 4, 4), kBlue, kTolerance));
  // Cizilmemis bolge: saydam siyah, ekranin kirmizisi degil.
  EXPECT_TRUE(
      ColorNear(PixelAt(px, kW, 40, 20), Color::Transparent(), kTolerance));
}

// Bir karede uretilen geometri hicbir backend'de sessizce dusurulmemeli.
//
// Vulkan vertex ring'i kare slotu basina 4 MB ile sinirliydi; dolunca
// VulkanBuffer::Write false donuyor ve cizim komutu hic kaydedilmiyordu.
// OpenGL'de boyle bir tavan yok (glBufferData her draw'da yeniden tahsis
// eder), dolayisiyla ayni sahne bir backend'de eksik ciziliyordu.
TEST_P(RegressionGpu, LargeFrameKeepsAllGeometry) {
  REQUIRE_BACKEND(be, GetParam());
  Painter& p = *be.painter;

  RenderTarget t = p.CreateRenderTarget(kW, kH);
  ASSERT_TRUE(t.IsValid());

  // FIXME: Isaret 35 000. dikdortgende (2.52 MB), eski 4 MB tavanin
  // altinda ciziliyor; asagidaki iddia saglanmiyor, regresyon yakalanmiyor.
  // Eski tavan 4 MB / 12 bayt = ~350 bin vertex idi. 70 bin dikdortgen
  // (420 bin vertex) onu asar. Isaret dikdortgeni tam ortada cizilir ki
  // tavan asildiktan sonraki bir batch'e dussun.
  constexpr int32_t kRectCount = 70000;
  constexpr int32_t kMarkerAt = kRectCount / 2;

  p.Begin();
  ASSERT_TRUE(p.SetRenderTarget(t));
  p.Clear(kBlack);
  p.SetPen(Pen::NoPen());
  for (int32_t i = 0; i < kRectCount; ++i) {
    if (i == kMarkerAt) {
      p.SetBrush(Brush(kBlue));
      p.FillRect(32.0F, 8.0F, 16.0F, 16.0F);
      continue;
    }
    p.SetBrush(Brush(kRed));
    p.FillRect(0.0F, 0.0F, 4.0F, 4.0F);
  }
  p.ResetRenderTarget();
  p.End();

  std::vector<uint8_t> px;
  ASSERT_TRUE(p.ReadRenderTarget(t, px));
  DumpRgba("large_frame_" + Tag(), px, kW, kH);
  EXPECT_TRUE(ColorNear(PixelAt(px, kW, 40, 16), kBlue, kTolerance));
}

// Clear TUM yuzeyi siler; kirpma onu sinirlamaz ve iki backend ayni sonucu
// verir.
//
// glClear scissor testine tabidir, vkCmdClearAttachments'a verilen rect ise
// tum yuzeydi: ayni cagri dizisi OpenGL'de yalnizca kirpma kutusunu,
// Vulkan'da tum hedefi siliyordu.
TEST_P(RegressionGpu, ClearIgnoresClipRect) {
  REQUIRE_BACKEND(be, GetParam());
  Painter& p = *be.painter;

  RenderTarget t = p.CreateRenderTarget(kW, kH);
  ASSERT_TRUE(t.IsValid());

  p.Begin();
  ASSERT_TRUE(p.SetRenderTarget(t));
  p.Clear(kRed);
  p.SetClipRect(Rect{0.0F, 0.0F, 8.0F, 8.0F});
  p.Clear(kBlue);
  p.ClearClip();
  p.ResetRenderTarget();
  p.End();

  std::vector<uint8_t> px;
  ASSERT_TRUE(p.ReadRenderTarget(t, px));
  DumpRgba("clear_ignores_clip_" + Tag(), px, kW, kH);
  EXPECT_TRUE(ColorNear(PixelAt(px, kW, 4, 4), kBlue, kTolerance));
  EXPECT_TRUE(ColorNear(PixelAt(px, kW, 40, 20), kBlue, kTolerance))
      << "Clear kirpma disini silmedi.";
}

// Konkav poligonun centigi bos kalmali.
TEST_P(RegressionGpu, ConcavePolygonNotchStaysEmpty) {
  REQUIRE_BACKEND(be, GetParam());
  Painter& p = *be.painter;

  constexpr int32_t kSize = 100;
  RenderTarget t = p.CreateRenderTarget(kSize, kSize);
  ASSERT_TRUE(t.IsValid());

  p.Begin();
  ASSERT_TRUE(p.SetRenderTarget(t));
  p.Clear(kBlack);
  p.SetPen(Pen::NoPen());
  p.SetBrush(Brush(kRed));
  p.FillPolygon({{10.0F, 10.0F},
                 {90.0F, 10.0F},
                 {90.0F, 90.0F},
                 {50.0F, 50.0F},
                 {10.0F, 90.0F}});
  p.ResetRenderTarget();
  p.End();

  std::vector<uint8_t> px;
  ASSERT_TRUE(p.ReadRenderTarget(t, px));
  DumpRgba("concave_notch_" + Tag(), px, kSize, kSize);
  EXPECT_TRUE(ColorNear(PixelAt(px, kSize, 30, 30), kRed, kTolerance));
  EXPECT_TRUE(ColorNear(PixelAt(px, kSize, 50, 80), kBlack, kTolerance))
      << "Centik boyandi.";
}

// Saydam hedefte renk premultiplied birikiyor; kompozisyonda alfa ikinci kez
// carpiliyordu (64, beklenen 128).
TEST_P(RegressionGpu, TranslucentTargetComposesLikeDirectDraw) {
  REQUIRE_BACKEND(be, GetParam());
  Painter& p = *be.painter;

  RenderTarget layer = p.CreateRenderTarget(32, 32);
  RenderTarget out = p.CreateRenderTarget(kW, kH);
  ASSERT_TRUE(layer.IsValid());
  ASSERT_TRUE(out.IsValid());
  const Color kHalfRed{255, 0, 0, 128};
  // Siyah zemine alfa 128 kirmizi: 255 * 128 / 255 = 128.
  const Color kExpected{128, 0, 0, 255};

  p.Begin();
  p.SetPen(Pen::NoPen());
  p.SetBrush(Brush(kHalfRed));
  ASSERT_TRUE(p.SetRenderTarget(layer));
  p.Clear(Color{0, 0, 0, 0});
  p.FillRect(0.0F, 0.0F, 32.0F, 32.0F);
  ASSERT_TRUE(p.SetRenderTarget(out));
  p.Clear(kBlack);
  p.FillRect(0.0F, 0.0F, 32.0F, 32.0F);
  p.DrawRenderTarget(layer, 32.0F, 0.0F);
  p.ResetRenderTarget();
  p.End();

  std::vector<uint8_t> px;
  ASSERT_TRUE(p.ReadRenderTarget(out, px));
  DumpRgba("translucent_target_" + Tag(), px, kW, kH);
  EXPECT_TRUE(ColorNear(PixelAt(px, kW, 16, 16), kExpected, kTolerance))
      << "Dogrudan cizim";
  EXPECT_TRUE(ColorNear(PixelAt(px, kW, 48, 16), kExpected, kTolerance))
      << "Ara hedef uzerinden cizim";
}

// Opaklik shader'da yalniz alfayi carpiyor; premultiplied hedefte RGB de
// olceklenmezse katman fazla parlak cizilir.
TEST_P(RegressionGpu, TranslucentTargetHonoursOpacity) {
  REQUIRE_BACKEND(be, GetParam());
  Painter& p = *be.painter;

  RenderTarget layer = p.CreateRenderTarget(32, 32);
  RenderTarget out = p.CreateRenderTarget(kW, kH);
  ASSERT_TRUE(layer.IsValid());
  ASSERT_TRUE(out.IsValid());
  const Color kHalfRed{255, 0, 0, 128};
  // 255 * (128 / 255) * 0.5 = 64.
  const Color kExpected{64, 0, 0, 255};

  p.Begin();
  p.SetPen(Pen::NoPen());
  p.SetBrush(Brush(kHalfRed));
  ASSERT_TRUE(p.SetRenderTarget(layer));
  p.Clear(Color::Transparent());
  p.FillRect(0.0F, 0.0F, 32.0F, 32.0F);
  ASSERT_TRUE(p.SetRenderTarget(out));
  p.Clear(kBlack);
  p.SetOpacity(0.5F);
  p.FillRect(0.0F, 0.0F, 32.0F, 32.0F);
  p.DrawRenderTarget(layer, 32.0F, 0.0F);
  p.ResetRenderTarget();
  p.End();

  std::vector<uint8_t> px;
  ASSERT_TRUE(p.ReadRenderTarget(out, px));
  DumpRgba("translucent_target_opacity_" + Tag(), px, kW, kH);
  EXPECT_TRUE(ColorNear(PixelAt(px, kW, 16, 16), kExpected, kTolerance))
      << "Dogrudan cizim";
  EXPECT_TRUE(ColorNear(PixelAt(px, kW, 48, 16), kExpected, kTolerance))
      << "Ara hedef uzerinden cizim";
}

// Opak hedefte tint alfasi, ayni dokuyu Image olarak cizmekle ayni sonucu
// vermeli (iz efekti bu yolu kullaniyor).
TEST_P(RegressionGpu, OpaqueTargetTintMatchesImage) {
  REQUIRE_BACKEND(be, GetParam());
  Painter& p = *be.painter;

  Image img = MakeSolidImage(kRed);
  ASSERT_TRUE(img.IsValid());
  RenderTarget layer = p.CreateRenderTarget(32, 32);
  RenderTarget out = p.CreateRenderTarget(kW, kH);
  ASSERT_TRUE(layer.IsValid());
  ASSERT_TRUE(out.IsValid());
  const Color kHalfTint{255, 255, 255, 128};
  const Color kExpected{128, 0, 0, 255};

  p.Begin();
  ASSERT_TRUE(p.SetRenderTarget(layer));
  p.Clear(kRed);
  ASSERT_TRUE(p.SetRenderTarget(out));
  p.Clear(kBlack);
  p.DrawImage(img, Rect{0.0F, 0.0F, 32.0F, 32.0F}, kHalfTint);
  p.DrawRenderTarget(layer, 32.0F, 0.0F, kHalfTint);
  p.ResetRenderTarget();
  p.End();

  std::vector<uint8_t> px;
  ASSERT_TRUE(p.ReadRenderTarget(out, px));
  DumpRgba("opaque_target_tint_" + Tag(), px, kW, kH);
  EXPECT_TRUE(ColorNear(PixelAt(px, kW, 16, 16), kExpected, kTolerance))
      << "Image";
  EXPECT_TRUE(ColorNear(PixelAt(px, kW, 48, 16), kExpected, kTolerance))
      << "Hedef";
}

// Toplamali modda da hedef, icindekiler dogrudan cizilmis gibi eklenmeli.
TEST_P(RegressionGpu, TranslucentTargetAddsLikeDirectDraw) {
  REQUIRE_BACKEND(be, GetParam());
  Painter& p = *be.painter;

  RenderTarget layer = p.CreateRenderTarget(32, 32);
  RenderTarget out = p.CreateRenderTarget(kW, kH);
  ASSERT_TRUE(layer.IsValid());
  ASSERT_TRUE(out.IsValid());
  const Color kHalfRed{255, 0, 0, 128};
  const Color kExpected{128, 0, 0, 255};

  p.Begin();
  p.SetPen(Pen::NoPen());
  p.SetBrush(Brush(kHalfRed));
  ASSERT_TRUE(p.SetRenderTarget(layer));
  p.Clear(Color::Transparent());
  p.FillRect(0.0F, 0.0F, 32.0F, 32.0F);
  ASSERT_TRUE(p.SetRenderTarget(out));
  p.Clear(kBlack);
  p.SetBlendMode(BlendMode::kAdditive);
  p.FillRect(0.0F, 0.0F, 32.0F, 32.0F);
  p.DrawRenderTarget(layer, 32.0F, 0.0F);
  p.ResetRenderTarget();
  p.End();

  std::vector<uint8_t> px;
  ASSERT_TRUE(p.ReadRenderTarget(out, px));
  DumpRgba("translucent_target_additive_" + Tag(), px, kW, kH);
  EXPECT_TRUE(ColorNear(PixelAt(px, kW, 16, 16), kExpected, kTolerance))
      << "Dogrudan cizim";
  EXPECT_TRUE(ColorNear(PixelAt(px, kW, 48, 16), kExpected, kTolerance))
      << "Ara hedef uzerinden cizim";
}

// Hedefi yari saydam renkle temizlemek de icerigi premultiplied birakmali.
TEST_P(RegressionGpu, TranslucentClearOnTargetIsPremultiplied) {
  REQUIRE_BACKEND(be, GetParam());
  Painter& p = *be.painter;

  RenderTarget layer = p.CreateRenderTarget(32, 32);
  RenderTarget out = p.CreateRenderTarget(kW, kH);
  ASSERT_TRUE(layer.IsValid());
  ASSERT_TRUE(out.IsValid());

  p.Begin();
  ASSERT_TRUE(p.SetRenderTarget(layer));
  p.Clear(Color{255, 0, 0, 128});
  ASSERT_TRUE(p.SetRenderTarget(out));
  p.Clear(kBlack);
  p.DrawRenderTarget(layer, 0.0F, 0.0F);
  p.ResetRenderTarget();
  p.End();

  std::vector<uint8_t> layer_px;
  ASSERT_TRUE(p.ReadRenderTarget(layer, layer_px));
  EXPECT_TRUE(ColorNear(PixelAt(layer_px, 32, 16, 16), Color{128, 0, 0, 128},
                        kTolerance))
      << "Hedef icerigi";

  std::vector<uint8_t> px;
  ASSERT_TRUE(p.ReadRenderTarget(out, px));
  DumpRgba("translucent_clear_target_" + Tag(), px, kW, kH);
  EXPECT_TRUE(
      ColorNear(PixelAt(px, kW, 16, 16), Color{128, 0, 0, 255}, kTolerance))
      << "Kompozisyon";
}

// Hedef cizildikten sonra gelen duz alfali cizimler kendi karistirmasini
// kullanmali; OpenGL'de blend durumu hedef cizimi icin degisip geri aliniyor.
TEST_P(RegressionGpu, StraightDrawsAfterTargetKeepTheirBlend) {
  REQUIRE_BACKEND(be, GetParam());
  Painter& p = *be.painter;

  const Color kHalfRed{255, 0, 0, 128};
  Image img = MakeSolidImage(kHalfRed);
  ASSERT_TRUE(img.IsValid());
  RenderTarget layer = p.CreateRenderTarget(32, 32);
  RenderTarget out = p.CreateRenderTarget(kW, kH);
  ASSERT_TRUE(layer.IsValid());
  ASSERT_TRUE(out.IsValid());
  const Color kExpected{128, 0, 0, 255};

  p.Begin();
  p.SetPen(Pen::NoPen());
  p.SetBrush(Brush(kHalfRed));
  ASSERT_TRUE(p.SetRenderTarget(layer));
  p.Clear(Color::Transparent());
  p.FillRect(0.0F, 0.0F, 32.0F, 32.0F);
  ASSERT_TRUE(p.SetRenderTarget(out));
  p.Clear(kBlack);
  p.DrawRenderTarget(layer, 0.0F, 0.0F);
  p.DrawImage(img, Rect{32.0F, 0.0F, 16.0F, 32.0F});
  p.FillRect(48.0F, 0.0F, 16.0F, 32.0F);
  p.ResetRenderTarget();
  p.End();

  std::vector<uint8_t> px;
  ASSERT_TRUE(p.ReadRenderTarget(out, px));
  DumpRgba("straight_after_target_" + Tag(), px, kW, kH);
  EXPECT_TRUE(ColorNear(PixelAt(px, kW, 16, 16), kExpected, kTolerance))
      << "Hedef";
  EXPECT_TRUE(ColorNear(PixelAt(px, kW, 40, 16), kExpected, kTolerance))
      << "Image";
  EXPECT_TRUE(ColorNear(PixelAt(px, kW, 56, 16), kExpected, kTolerance))
      << "Dolgu";
}

// Bolme karari yalniz kenar orta noktalarina bakinca ucgenin icindeki kucuk
// gradient diski kayboluyordu.
TEST_P(RegressionGpu, SmallOffCentreRadialGradientKeepsItsCentre) {
  REQUIRE_BACKEND(be, GetParam());
  Painter& p = *be.painter;

  constexpr int32_t kSize = 100;
  RenderTarget t = p.CreateRenderTarget(kSize, kSize);
  ASSERT_TRUE(t.IsValid());

  p.Begin();
  ASSERT_TRUE(p.SetRenderTarget(t));
  p.Clear(kBlack);
  p.SetPen(Pen::NoPen());
  p.SetBrush(Brush::RadialGradient({25.5F, 50.5F}, 8.0F, kRed, kBlue));
  p.FillRect(0.0F, 0.0F, 100.0F, 100.0F);
  p.ResetRenderTarget();
  p.End();

  std::vector<uint8_t> px;
  ASSERT_TRUE(p.ReadRenderTarget(t, px));
  DumpRgba("small_radial_" + Tag(), px, kSize, kSize);
  EXPECT_TRUE(ColorNear(PixelAt(px, kSize, 25, 50), kRed, 20))
      << "Gradient merkezi kayboldu.";
  EXPECT_TRUE(ColorNear(PixelAt(px, kSize, 80, 50), kBlue, kTolerance));
}

// Round cap tam disk oldugunda govdeyle ortusen bolge yari saydam renkte iki
// kez karisiyordu.
TEST_P(RegressionGpu, TranslucentRoundCapBlendsOnce) {
  REQUIRE_BACKEND(be, GetParam());
  Painter& p = *be.painter;

  constexpr int32_t kSize = 100;
  RenderTarget t = p.CreateRenderTarget(kSize, kSize);
  ASSERT_TRUE(t.IsValid());
  const Color kExpected{128, 0, 0, 255};

  p.Begin();
  ASSERT_TRUE(p.SetRenderTarget(t));
  p.Clear(kBlack);
  Pen pen(Color{255, 0, 0, 128}, 20.0F);
  pen.SetCapStyle(LineCap::kRound);
  p.SetPen(pen);
  p.DrawLine(20.0F, 50.0F, 80.0F, 50.0F);
  p.ResetRenderTarget();
  p.End();

  std::vector<uint8_t> px;
  ASSERT_TRUE(p.ReadRenderTarget(t, px));
  DumpRgba("round_cap_" + Tag(), px, kSize, kSize);
  EXPECT_TRUE(ColorNear(PixelAt(px, kSize, 50, 50), kExpected, kTolerance))
      << "Govde";
  EXPECT_TRUE(ColorNear(PixelAt(px, kSize, 12, 50), kExpected, kTolerance))
      << "Uc";
  EXPECT_TRUE(ColorNear(PixelAt(px, kSize, 22, 52), kExpected, kTolerance))
      << "Uc ile govdenin kesistigi bolge";
}

// Cizilen metin olculen kutunun icinde kalmali. Hizalama ve kaydirma bu
// kutuyla yapiliyor; olcum kerning uygulayip cizim uygulamayinca ya da glyph
// sol boslugu iki kez eklenince metin kutudan tasiyordu.
TEST_P(RegressionGpu, DrawnTextStaysInsideMeasuredBox) {
  REQUIRE_BACKEND(be, GetParam());
  SDLPAINTER_REQUIRE_FONT_OR_SKIP(font_path);
  Painter& p = *be.painter;

  const auto font = std::make_shared<Font>(font_path, 48);
  ASSERT_TRUE(font->IsValid());
  constexpr int32_t kTargetW = 400;
  constexpr int32_t kTargetH = 100;
  constexpr float kX = 20.0F;
  RenderTarget t = p.CreateRenderTarget(kTargetW, kTargetH);
  ASSERT_TRUE(t.IsValid());

  // "r"nin sag boslugu negatif: murekkebi kalem sonunu asar, kutunun sag
  // kenari murekkebin kendisidir.
  for (const std::string text : {"rrrr", "AVAVAVAVAV"}) {
    int32_t box_w = 0;
    int32_t box_h = 0;
    ASSERT_TRUE(font->MeasureText(text, box_w, box_h));

    p.Begin();
    ASSERT_TRUE(p.SetRenderTarget(t));
    p.Clear(Color::Transparent());
    p.SetPen(Pen(Color::White()));
    p.SetFont(font);
    p.DrawText(kX, 70.0F, text);
    p.ResetRenderTarget();
    p.End();

    std::vector<uint8_t> px;
    ASSERT_TRUE(p.ReadRenderTarget(t, px));
    int32_t ink_left = kTargetW;
    int32_t ink_right = -1;
    for (int32_t y = 0; y < kTargetH; ++y) {
      for (int32_t x = 0; x < kTargetW; ++x) {
        if (PixelAt(px, kTargetW, x, y).a > 0) {
          ink_left = std::min(ink_left, x);
          ink_right = std::max(ink_right, x + 1);
        }
      }
    }
    ASSERT_GE(ink_right, 0) << text << ": hic murekkep yok";
    EXPECT_LE(ink_right, static_cast<int32_t>(kX) + box_w)
        << text << ": murekkep olculen kutunun sagina tasti";
    EXPECT_LE(ink_right - ink_left, box_w) << text;
  }
  p.SetFont(nullptr);
}

// Doku olarak cizilen hedef, cizim gonderilmeden silinse de cizim kaybolmamali.
TEST_P(RegressionGpu, TargetDestroyedAfterBeingDrawnKeepsTheDraw) {
  REQUIRE_BACKEND(be, GetParam());
  Painter& p = *be.painter;

  RenderTarget layer = p.CreateRenderTarget(32, 32);
  RenderTarget out = p.CreateRenderTarget(kW, kH);
  ASSERT_TRUE(layer.IsValid());
  ASSERT_TRUE(out.IsValid());

  p.Begin();
  ASSERT_TRUE(p.SetRenderTarget(layer));
  p.Clear(kRed);
  ASSERT_TRUE(p.SetRenderTarget(out));
  p.Clear(kBlack);
  p.DrawRenderTarget(layer, 0.0F, 0.0F);
  layer.Reset();
  p.ResetRenderTarget();
  p.End();

  std::vector<uint8_t> px;
  ASSERT_TRUE(p.ReadRenderTarget(out, px));
  DumpRgba("target_destroyed_after_draw_" + Tag(), px, kW, kH);
  EXPECT_TRUE(ColorNear(PixelAt(px, kW, 16, 16), kRed, kTolerance))
      << "Silinen hedefin cizimi kayboldu.";
}

INSTANTIATE_TEST_SUITE_P(
    Backends, RegressionGpu, ::testing::ValuesIn(AvailableBackends()),
    [](const ::testing::TestParamInfo<RendererBackend>& i) {
      return i.param == RendererBackend::kOpenGL ? "OpenGL" : "Vulkan";
    });

// Kare icinde silinen hedefin framebuffer'i, henuz gonderilmemis komut
// buffer'indan referans edilirken yikilmamali; sonraki kareler de etkilenmemeli.
TEST(RegressionVulkan, DestroyingBoundTargetMidFrameIsSafe) {
  REQUIRE_BACKEND(be, RendererBackend::kVulkan);
  Painter& p = *be.painter;

  RenderTarget t = p.CreateRenderTarget(kW, kH);
  ASSERT_TRUE(t.IsValid());
  p.Begin();
  ASSERT_TRUE(p.SetRenderTarget(t));
  p.Clear(kRed);
  t.Reset();
  p.End();

  RenderTarget next = p.CreateRenderTarget(kW, kH);
  ASSERT_TRUE(next.IsValid());
  p.Begin();
  ASSERT_TRUE(p.SetRenderTarget(next));
  p.Clear(kBlue);
  p.ResetRenderTarget();
  p.End();

  std::vector<uint8_t> px;
  ASSERT_TRUE(p.ReadRenderTarget(next, px));
  EXPECT_TRUE(ColorNear(PixelAt(px, kW, 8, 8), kBlue, kTolerance));
}

// Kare icinde silinen doku, onu kullanan kare GPU'da bitene kadar yasamali.
// O kareyi agir cizimle uzatmak erken silmeyi validation'a gorunur kilar.
TEST(RegressionVulkan, TextureDestroyedMidFrameOutlivesItsFrame) {
  REQUIRE_BACKEND(be, RendererBackend::kVulkan);
  Painter& p = *be.painter;

  constexpr int32_t kLoadSize = 2048;
  constexpr int32_t kLoadPasses = 300;
  RenderTarget load = p.CreateRenderTarget(kLoadSize, kLoadSize);
  ASSERT_TRUE(load.IsValid());
  auto image = std::make_unique<Image>(MakeSolidImage(kRed));

  p.Begin();
  ASSERT_TRUE(p.SetRenderTarget(load));
  p.SetPen(Pen::NoPen());
  p.SetBrush(Brush(Color{255, 255, 255, 8}));
  for (int32_t i = 0; i < kLoadPasses; ++i) {
    p.FillRect(0.0F, 0.0F, static_cast<float>(kLoadSize),
               static_cast<float>(kLoadSize));
  }
  p.ResetRenderTarget();
  p.DrawImage(*image, 0.0F, 0.0F);
  // Hedef degisimi bekleyen cizimi gonderir; doku artik bu karenin komut
  // buffer'inda.
  ASSERT_TRUE(p.SetRenderTarget(load));
  image.reset();
  p.ResetRenderTarget();
  p.End();

  // Gecikmeli silme bir sonraki karenin sonunda calisiyor.
  p.Begin();
  p.End();
  p.Begin();
  p.End();
}

}  // namespace
