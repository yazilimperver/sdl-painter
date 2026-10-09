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
#include "sdl_painter/vertex.h"

#include <algorithm>
#include <cstdint>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
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
using sdl_painter::IRenderer;
using sdl_painter::LineCap;
using sdl_painter::Painter;
using sdl_painter::Pen;
using sdl_painter::Rect;
using sdl_painter::RendererBackend;
using sdl_painter::RenderTarget;
using sdl_painter::TextureFilter;
using sdl_painter::Vertex;
using sdl_painter::testing::AvailableBackends;
using sdl_painter::testing::ColorNear;
using sdl_painter::testing::DumpRgba;
using sdl_painter::testing::HiddenWindow;
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

  /// @brief Hedefi okur ve dokumunu yazar; okuma basarisizsa testi keser.
  ///
  /// `ASSERT_*` yalnizca `void` donen islevde kullanilabildiginden okunan veri
  /// parametreyle doner. Cagri `ASSERT_NO_FATAL_FAILURE` ile sarilmali.
  void ReadAndDump(Painter& painter, const RenderTarget& target,
                   const std::string& name, std::vector<uint8_t>& rgba) const {
    ASSERT_TRUE(painter.ReadRenderTarget(target, rgba));
    // Cagiranlar piksel indeksler; kisa bir tampon sinir disi okuma olurdu.
    ASSERT_EQ(rgba.size(), static_cast<std::size_t>(target.Width()) *
                               static_cast<std::size_t>(target.Height()) * 4U);
    DumpRgba(name + Tag(), rgba, target.Width(), target.Height());
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
  ASSERT_NO_FATAL_FAILURE(ReadAndDump(p, a, "rebind_", px));
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
  ASSERT_NO_FATAL_FAILURE(ReadAndDump(p, t, "update_", px));
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
  ASSERT_NO_FATAL_FAILURE(ReadAndDump(p, t, "control_noupdate_", px));
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
  ASSERT_NO_FATAL_FAILURE(ReadAndDump(p, t, "control_updatefirst_", px));
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
  ASSERT_NO_FATAL_FAILURE(ReadAndDump(p, t, "fresh_target_", px));
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

  // Eski tavan 4 MB idi. Bir dikdortgen 6 vertex'tir; tavana siglan dikdortgen
  // sayisi Vertex boyutundan turetilir (12 bayt icin 58 254). Bu bir bayt
  // hesabidir: batcher geometriyi 8192 vertex'lik batch'lerle yazar,
  // dolayisiyla eski hatada dusen aralik tam batch'lere yuvarlanir (ilk
  // dusen dikdortgen 57 330) ve sondaki kucuk batch kalan bosluga sigip
  // cizilebilir. Bu yuzden ana isaret sona degil, dusen bolgenin ortasina
  // konur; sondaki kuyruk ayri bir isaretle sinanir.
  constexpr int32_t kRectCount = 70000;
  constexpr int32_t kRectsUnderOldCeiling =
      static_cast<int32_t>((4U * 1024U * 1024U) / (6U * sizeof(Vertex)));
  constexpr int32_t kMarkerAt =
      kRectsUnderOldCeiling + ((kRectCount - kRectsUnderOldCeiling) / 2);
  constexpr int32_t kTailMarkerAt = kRectCount - 1;
  static_assert(
      kMarkerAt > kRectsUnderOldCeiling,
      "Isaret eski tavanin altinda kalirsa test regresyonu yakalamaz");
  static_assert(kMarkerAt < kTailMarkerAt,
                "Ana isaret sondaki kuyruktan once olmali");
  // Mavi (32, 8, 16, 16), yesil (8, 8, 16, 16), kirmizi (0, 0, 4, 4): x
  // araliklari 32..48, 8..24 ve 0..4; uc bolge ayrik, hedef 64x32 icinde.
  const Color kTailGreen{0, 255, 0, 255};

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
    if (i == kTailMarkerAt) {
      p.SetBrush(Brush(kTailGreen));
      p.FillRect(8.0F, 8.0F, 16.0F, 16.0F);
      continue;
    }
    p.SetBrush(Brush(kRed));
    p.FillRect(0.0F, 0.0F, 4.0F, 4.0F);
  }
  p.ResetRenderTarget();
  p.End();

  std::vector<uint8_t> px;
  ASSERT_NO_FATAL_FAILURE(ReadAndDump(p, t, "large_frame_", px));
  EXPECT_TRUE(ColorNear(PixelAt(px, kW, 40, 16), kBlue, kTolerance));
  EXPECT_TRUE(ColorNear(PixelAt(px, kW, 16, 16), kTailGreen, kTolerance));
  // Kirmizi dikdortgenler (0, 0, 4, 4) bolgesine, tavandan once, ustuste
  // cizilir.
  EXPECT_TRUE(ColorNear(PixelAt(px, kW, 2, 2), kRed, kTolerance));
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
  ASSERT_NO_FATAL_FAILURE(ReadAndDump(p, t, "clear_ignores_clip_", px));
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
  ASSERT_NO_FATAL_FAILURE(ReadAndDump(p, t, "concave_notch_", px));
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
  ASSERT_NO_FATAL_FAILURE(ReadAndDump(p, out, "translucent_target_", px));
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
  ASSERT_NO_FATAL_FAILURE(
      ReadAndDump(p, out, "translucent_target_opacity_", px));
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
  ASSERT_NO_FATAL_FAILURE(ReadAndDump(p, out, "opaque_target_tint_", px));
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
  ASSERT_NO_FATAL_FAILURE(
      ReadAndDump(p, out, "translucent_target_additive_", px));
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
  ASSERT_NO_FATAL_FAILURE(ReadAndDump(p, out, "translucent_clear_target_", px));
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
  ASSERT_NO_FATAL_FAILURE(ReadAndDump(p, out, "straight_after_target_", px));
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
  ASSERT_NO_FATAL_FAILURE(ReadAndDump(p, t, "small_radial_", px));
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
  ASSERT_NO_FATAL_FAILURE(ReadAndDump(p, t, "round_cap_", px));
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

// Vulkan'da model matrisi ayri push constant degil, projeksiyonla CPU'da
// carpiliyor. IRenderer'i dogrudan kullanan kod icin anlami iki backend'de
// ayni kalmali: model matrisi geometriyi tasir.
TEST_P(RegressionGpu, RendererModelMatrixMovesGeometry) {
  const sdl_painter::testing::ValidationGuard guard;
  const HiddenWindow window(GetParam(), 128, 96);
  if (window.Get() == nullptr) {
    SDLPAINTER_BACKEND_UNAVAILABLE(std::string("Pencere olusturulamadi: ") +
                                   window.Error());
  }
  std::unique_ptr<IRenderer> owned = sdl_painter::CreateRenderer(GetParam());
  ASSERT_NE(owned, nullptr);
  if (!owned->Initialize(window.Get())) {
    SDLPAINTER_BACKEND_UNAVAILABLE("Backend baslatilamadi.");
  }
  IRenderer* const renderer = owned.get();
  Painter p(std::move(owned), 128, 96);

  RenderTarget t = p.CreateRenderTarget(kW, kH);
  ASSERT_TRUE(t.IsValid());
  const std::vector<Vertex> kQuad = {
      {0.0F, 0.0F, 255, 0, 0, 255},   {16.0F, 0.0F, 255, 0, 0, 255},
      {16.0F, 16.0F, 255, 0, 0, 255}, {0.0F, 0.0F, 255, 0, 0, 255},
      {16.0F, 16.0F, 255, 0, 0, 255}, {0.0F, 16.0F, 255, 0, 0, 255},
  };
  glm::mat3 shift(1.0F);
  shift[2][0] = 32.0F;

  p.Begin();
  ASSERT_TRUE(p.SetRenderTarget(t));
  p.Clear(kBlack);
  renderer->SetModelMatrix(glm::value_ptr(shift));
  renderer->DrawTriangles(kQuad);
  renderer->SetModelMatrix(glm::value_ptr(glm::mat3(1.0F)));
  p.ResetRenderTarget();
  p.End();

  std::vector<uint8_t> px;
  ASSERT_NO_FATAL_FAILURE(ReadAndDump(p, t, "model_matrix_", px));
  EXPECT_TRUE(ColorNear(PixelAt(px, kW, 40, 8), kRed, kTolerance))
      << "Kaydirilmis dortgen";
  EXPECT_TRUE(ColorNear(PixelAt(px, kW, 8, 8), kBlack, kTolerance))
      << "Kaydirilmamis konum bos kalmali";
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
  ASSERT_NO_FATAL_FAILURE(
      ReadAndDump(p, out, "target_destroyed_after_draw_", px));
  EXPECT_TRUE(ColorNear(PixelAt(px, kW, 16, 16), kRed, kTolerance))
      << "Silinen hedefin cizimi kayboldu.";
}

// Kirpma dikdortgeni cizimi dogru yerde sinirlamali.
//
// Hedef (64x32) pencereden (128x96) kucuk: Y cevirmesi hedef yerine pencere
// yuksekligine gore yapilirsa kirpma (16, 24) civarina duser ve iki backend
// ayni yanlisi yapsa parite testi bunu gormez.
TEST_P(RegressionGpu, ClipRectLimitsDrawsToItsBox) {
  REQUIRE_BACKEND(be, GetParam());
  Painter& p = *be.painter;

  RenderTarget t = p.CreateRenderTarget(kW, kH);
  ASSERT_TRUE(t.IsValid());

  p.Begin();
  ASSERT_TRUE(p.SetRenderTarget(t));
  p.Clear(kBlack);
  p.SetPen(Pen::NoPen());
  p.SetBrush(Brush(kBlue));
  p.SetClipRect(Rect{8.0F, 4.0F, 16.0F, 8.0F});
  p.FillRect(0.0F, 0.0F, static_cast<float>(kW), static_cast<float>(kH));
  p.ClearClip();
  p.ResetRenderTarget();
  p.End();

  std::vector<uint8_t> px;
  ASSERT_NO_FATAL_FAILURE(ReadAndDump(p, t, "clip_rect_", px));
  EXPECT_TRUE(ColorNear(PixelAt(px, kW, 16, 8), kBlue, kTolerance))
      << "Kirpma kutusunun icinde";
  EXPECT_TRUE(ColorNear(PixelAt(px, kW, 16, 2), kBlack, kTolerance))
      << "Kutunun ustu";
  EXPECT_TRUE(ColorNear(PixelAt(px, kW, 16, 14), kBlack, kTolerance))
      << "Kutunun alti";
  EXPECT_TRUE(ColorNear(PixelAt(px, kW, 4, 8), kBlack, kTolerance))
      << "Kutunun solu";
  EXPECT_TRUE(ColorNear(PixelAt(px, kW, 28, 8), kBlack, kTolerance))
      << "Kutunun sagi";
  EXPECT_TRUE(ColorNear(PixelAt(px, kW, 16, 24), kBlack, kTolerance))
      << "Hedef yuksekligine gore ters cevrilmis kutu";
}

// kMultiply hedefi kaynakla kanal kanal carpmali: hedef * kaynak.
//
// Faktorler iki backend'de de (DST_COLOR, ZERO): 128/255 * 255/255 = 128,
// 128/255 * 128/255 = 64, 128/255 * 0 = 0; alfa 1 * 1 = 1.
TEST_P(RegressionGpu, MultiplyBlendScalesTheDestination) {
  REQUIRE_BACKEND(be, GetParam());
  Painter& p = *be.painter;

  RenderTarget t = p.CreateRenderTarget(kW, kH);
  ASSERT_TRUE(t.IsValid());
  const Color kExpected{128, 64, 0, 255};

  p.Begin();
  ASSERT_TRUE(p.SetRenderTarget(t));
  p.Clear(Color{128, 128, 128, 255});
  p.SetPen(Pen::NoPen());
  p.SetBrush(Brush(Color{255, 128, 0, 255}));
  p.SetBlendMode(BlendMode::kMultiply);
  p.FillRect(0.0F, 0.0F, 32.0F, 32.0F);
  p.ResetRenderTarget();
  p.End();

  std::vector<uint8_t> px;
  ASSERT_NO_FATAL_FAILURE(ReadAndDump(p, t, "multiply_", px));
  EXPECT_TRUE(ColorNear(PixelAt(px, kW, 16, 16), kExpected, kTolerance))
      << "Carpilan bolge";
  EXPECT_TRUE(
      ColorNear(PixelAt(px, kW, 48, 16), Color{128, 128, 128, 255}, kTolerance))
      << "Cizilmeyen bolge degismemeli";
}

// kNone karistirmayi kapatir: yari saydam kaynak, hedefe alfasi uygulanmadan
// oldugu gibi yazilir (renderer.h, BlendMode::kNone).
TEST_P(RegressionGpu, NoBlendWritesSourceWithItsAlpha) {
  REQUIRE_BACKEND(be, GetParam());
  Painter& p = *be.painter;

  RenderTarget t = p.CreateRenderTarget(kW, kH);
  ASSERT_TRUE(t.IsValid());
  const Color kExpected{255, 0, 0, 128};

  p.Begin();
  ASSERT_TRUE(p.SetRenderTarget(t));
  p.Clear(kBlue);
  p.SetPen(Pen::NoPen());
  p.SetBrush(Brush(Color{255, 0, 0, 128}));
  p.SetBlendMode(BlendMode::kNone);
  p.FillRect(0.0F, 0.0F, 32.0F, 32.0F);
  p.ResetRenderTarget();
  p.End();

  std::vector<uint8_t> px;
  ASSERT_NO_FATAL_FAILURE(ReadAndDump(p, t, "no_blend_", px));
  EXPECT_TRUE(ColorNear(PixelAt(px, kW, 16, 16), kExpected, kTolerance))
      << "Kaynak alfasiyla birlikte yazilmali";
  EXPECT_TRUE(ColorNear(PixelAt(px, kW, 48, 16), kBlue, kTolerance))
      << "Cizilmeyen bolge degismemeli";
}

// 1 kanalli veri gri tonlama: (v, v, v, 255). OpenGL swizzle ile, Vulkan
// CPU'da RGBA'ya genisleterek yapiyor; sonuc ayni olmali.
TEST_P(RegressionGpu, OneChannelImageDrawsAsOpaqueGray) {
  REQUIRE_BACKEND(be, GetParam());
  Painter& p = *be.painter;

  const std::vector<uint8_t> gray(2U * 2U, 128U);
  Image img = Image::CreateFromData(gray.data(), 2, 2, 1);
  ASSERT_TRUE(img.IsValid());
  RenderTarget t = p.CreateRenderTarget(kW, kH);
  ASSERT_TRUE(t.IsValid());

  p.Begin();
  ASSERT_TRUE(p.SetRenderTarget(t));
  p.Clear(kBlack);
  p.DrawImage(img, Rect{0.0F, 0.0F, 32.0F, 32.0F});
  p.ResetRenderTarget();
  p.End();

  std::vector<uint8_t> px;
  ASSERT_NO_FATAL_FAILURE(ReadAndDump(p, t, "one_channel_", px));
  EXPECT_TRUE(ColorNear(PixelAt(px, kW, 16, 16), Color{128, 128, 128, 255},
                        kTolerance));
}

// 2 kanalli veri gri + alfa: (v, v, v, a). Zemin mavi: v = 255, a = 128 icin
// r, g = 128 ve b = 128 + 127 = 255; siyah zeminde v ile a takasi gorunmezdi
// (v * a simetrik), mavi zeminde takasta b = 128 olur.
TEST_P(RegressionGpu, TwoChannelImageDrawsAsGrayWithAlpha) {
  REQUIRE_BACKEND(be, GetParam());
  Painter& p = *be.painter;

  std::vector<uint8_t> gray_alpha;
  for (int32_t i = 0; i < 4; ++i) {
    gray_alpha.push_back(255U);
    gray_alpha.push_back(128U);
  }
  Image img = Image::CreateFromData(gray_alpha.data(), 2, 2, 2);
  ASSERT_TRUE(img.IsValid());
  RenderTarget t = p.CreateRenderTarget(kW, kH);
  ASSERT_TRUE(t.IsValid());

  p.Begin();
  ASSERT_TRUE(p.SetRenderTarget(t));
  p.Clear(kBlue);
  p.DrawImage(img, Rect{0.0F, 0.0F, 32.0F, 32.0F});
  p.ResetRenderTarget();
  p.End();

  std::vector<uint8_t> px;
  ASSERT_NO_FATAL_FAILURE(ReadAndDump(p, t, "two_channel_", px));
  EXPECT_TRUE(ColorNear(PixelAt(px, kW, 16, 16), Color{128, 128, 255, 255},
                        kTolerance));
}

/// @brief 2x1 (kirmizi, mavi) goruntu; filtre ilk cizimden once ayarlanir.
Image MakeRedBlueImage(TextureFilter filter) {
  const std::vector<uint8_t> pixels = {255, 0, 0, 255, 0, 0, 255, 255};
  Image img = Image::CreateFromData(pixels.data(), 2, 1, 4);
  img.SetFilter(filter);
  return img;
}

// Iki filtre testi: 2x1 goruntu 32 piksel genisle cizilir; renk siniri x = 16.
// Sinira en yakin iki piksel (15 ve 16) iki filtreyi ayirt eder.
//
// Piksel merkezi (x + 0.5) / 32, doku merkezleri 0.25 ve 0.75: x = 15 icin
// mavi agirligi (0.484375 * 2 - 0.5) = 0.46875, yani (135, 0, 120); x = 16
// icin (120, 0, 135). kNearest'te iki piksel saf kirmizi ve saf mavi.
TEST_P(RegressionGpu, NearestFilterKeepsAHardEdge) {
  REQUIRE_BACKEND(be, GetParam());
  Painter& p = *be.painter;

  Image img = MakeRedBlueImage(TextureFilter::kNearest);
  ASSERT_TRUE(img.IsValid());
  RenderTarget t = p.CreateRenderTarget(kW, kH);
  ASSERT_TRUE(t.IsValid());

  p.Begin();
  ASSERT_TRUE(p.SetRenderTarget(t));
  p.Clear(kBlack);
  p.DrawImage(img, Rect{0.0F, 0.0F, 32.0F, 16.0F});
  p.ResetRenderTarget();
  p.End();

  std::vector<uint8_t> px;
  ASSERT_NO_FATAL_FAILURE(ReadAndDump(p, t, "filter_nearest_", px));
  EXPECT_TRUE(ColorNear(PixelAt(px, kW, 15, 8), kRed, kTolerance))
      << "Sinirin sol pikseli";
  EXPECT_TRUE(ColorNear(PixelAt(px, kW, 16, 8), kBlue, kTolerance))
      << "Sinirin sag pikseli";
}

TEST_P(RegressionGpu, LinearFilterBlendsAcrossTheEdge) {
  REQUIRE_BACKEND(be, GetParam());
  Painter& p = *be.painter;

  Image img = MakeRedBlueImage(TextureFilter::kLinear);
  ASSERT_TRUE(img.IsValid());
  RenderTarget t = p.CreateRenderTarget(kW, kH);
  ASSERT_TRUE(t.IsValid());

  p.Begin();
  ASSERT_TRUE(p.SetRenderTarget(t));
  p.Clear(kBlack);
  p.DrawImage(img, Rect{0.0F, 0.0F, 32.0F, 16.0F});
  p.ResetRenderTarget();
  p.End();

  std::vector<uint8_t> px;
  ASSERT_NO_FATAL_FAILURE(ReadAndDump(p, t, "filter_linear_", px));
  EXPECT_TRUE(ColorNear(PixelAt(px, kW, 15, 8), Color{135, 0, 120, 255}, 4))
      << "Sinirin sol pikseli kirmizi ve mavi karisimi olmali";
  EXPECT_TRUE(ColorNear(PixelAt(px, kW, 16, 8), Color{120, 0, 135, 255}, 4))
      << "Sinirin sag pikseli kirmizi ve mavi karisimi olmali";
}

// Bir hedefi baska hedefe basinca yonelim korunmali: sol ust kirmizi, sag alt
// mavi kalir. Tek renkli bilesim testleri ters donmus dokuyu gormez.
TEST_P(RegressionGpu, TargetDrawnOntoTargetKeepsItsOrientation) {
  REQUIRE_BACKEND(be, GetParam());
  Painter& p = *be.painter;

  RenderTarget source = p.CreateRenderTarget(kW, kH);
  RenderTarget out = p.CreateRenderTarget(kW, kH);
  ASSERT_TRUE(source.IsValid());
  ASSERT_TRUE(out.IsValid());

  p.Begin();
  p.SetPen(Pen::NoPen());
  ASSERT_TRUE(p.SetRenderTarget(source));
  p.Clear(kBlack);
  p.SetBrush(Brush(kRed));
  p.FillRect(0.0F, 0.0F, 16.0F, 8.0F);
  p.SetBrush(Brush(kBlue));
  p.FillRect(48.0F, 24.0F, 16.0F, 8.0F);
  ASSERT_TRUE(p.SetRenderTarget(out));
  p.Clear(kBlack);
  p.DrawRenderTarget(source, 0.0F, 0.0F);
  p.ResetRenderTarget();
  p.End();

  std::vector<uint8_t> source_px;
  ASSERT_NO_FATAL_FAILURE(
      ReadAndDump(p, source, "orientation_source_", source_px));
  EXPECT_TRUE(ColorNear(PixelAt(source_px, kW, 8, 4), kRed, kTolerance))
      << "Kaynak hedef, sol ust";
  EXPECT_TRUE(ColorNear(PixelAt(source_px, kW, 56, 28), kBlue, kTolerance))
      << "Kaynak hedef, sag alt";

  std::vector<uint8_t> px;
  ASSERT_NO_FATAL_FAILURE(ReadAndDump(p, out, "orientation_out_", px));
  EXPECT_TRUE(ColorNear(PixelAt(px, kW, 8, 4), kRed, kTolerance))
      << "Basilan hedef, sol ust";
  EXPECT_TRUE(ColorNear(PixelAt(px, kW, 56, 28), kBlue, kTolerance))
      << "Basilan hedef, sag alt";
  EXPECT_TRUE(ColorNear(PixelAt(px, kW, 8, 28), kBlack, kTolerance))
      << "Sol alt bos kalmali";
  EXPECT_TRUE(ColorNear(PixelAt(px, kW, 56, 4), kBlack, kTolerance))
      << "Sag ust bos kalmali";
}

// Painter fontun son sahibiyken yikilirsa glyph atlasinin dokulari renderer
// kapandiktan sonra serbest kalir (Painter yikicisi once Shutdown'i cagirir,
// mCurrentFont uye yikiminda birakilir). SetFont(nullptr) CAGRILMAZ: yikim
// yolu ozellikle bu. Painter fontun son sahibiyken yikim cokme ya da
// validation hatasi uretmemeli.
TEST_P(RegressionGpu, PainterAsLastFontOwnerDestroysCleanly) {
  REQUIRE_BACKEND(be, GetParam());
  SDLPAINTER_REQUIRE_FONT_OR_SKIP(font_path);
  Painter& p = *be.painter;

  p.SetFont(std::make_shared<Font>(font_path, 24));
  ASSERT_NE(p.GetFont(), nullptr);
  ASSERT_TRUE(p.GetFont()->IsValid());
  ASSERT_EQ(p.GetFont().use_count(), 1) << "Painter tek sahip olmali";

  p.Begin();
  p.Clear(kBlack);
  p.SetPen(Pen(Color::White()));
  p.DrawText(4.0F, 30.0F, "Ag");
  p.End();
  ASSERT_GT(p.GetFont()->AtlasPageCount(), 0U)
      << "Yikim aninda atlas dokusu olmali";

  // Yikim burada, koruyucu hala yasiyorken.
  be.painter.reset();
}

INSTANTIATE_TEST_SUITE_P(
    Backends, RegressionGpu, ::testing::ValuesIn(AvailableBackends()),
    [](const ::testing::TestParamInfo<RendererBackend>& i) {
      return i.param == RendererBackend::kOpenGL ? "OpenGL" : "Vulkan";
    });

// Asagidaki iki test Vulkan'a ozgu. Vulkan derlenmemis bir yapilandirmada
// (ornegin Vulkan'siz ASan isi) derlenmezler: derlenmemis backend
// SDLPAINTER_REQUIRE_BACKEND ayarliyken bile zorunlu degildir, aksi halde
// REQUIRE_BACKEND "backend kurulamadi" diye FAIL verirdi (BackendIsRequired
// yalnizca derlenmis backend'ler icin gecerlidir).
#ifdef SDLPAINTER_HAS_VULKAN

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
//
// Sinirlama: test zamanlamaya bagli. Yeterince hizli bir GPU'da kare, doku
// silinmeden once zaten bitmis olabilir; o zaman erken silme hatasi
// kacirilir (yanlis kirmizi uretmez, yalnizca hatayi gormeyebilir). Yuk
// 2048x2048 hedefe 300 tam ekran gecis, yaklasik 1,26 milyar piksel
// karistirma: yazilim surucusunde (lavapipe) agir olabilir. NVIDIA ile
// Windows Debug'da test yaklasik 170 ms (tam takim icinde) ile 360 ms (tek
// basina, pencere ve Vulkan kurulumu dahil) suruyor; lavapipe'ta olculmedi.
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

#endif  // SDLPAINTER_HAS_VULKAN

}  // namespace
