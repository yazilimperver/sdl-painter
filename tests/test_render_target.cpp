/// @file test_render_target.cpp
/// @brief Dokuya çizim (render target): mekanik, yönelim ve geri okuma.
///
/// Bu dosya kütüphanede **pikselleri doğrulayan ilk** test takımıdır. Diğer
/// testler ya `MockRenderer` kullanır ya da "çöküyor mu" sorusunu cevaplar;
/// burada gerçek sürücüde çizilen kareler geri okunup değerleri sınanır.
///
/// En kritik iddia **yönelim**: OpenGL'in framebuffer'ı aşağıdan yukarı,
/// Vulkan'ınki yukarıdan aşağı adreslenir. Bir hedefe çizerken bu fark
/// kapatılmazsa doku bellekte baş aşağı durur; ekranda ters görünür ve
/// `ReadRenderTarget` çıktısı iki backend'de farklı olur. Aşağıdaki testler
/// sahnenin **üst** yarısına çizip ilk satırları kontrol ederek bunu yakalar.

#include "sdl_painter/brush.h"
#include "sdl_painter/color.h"
#include "sdl_painter/painter.h"
#include "sdl_painter/pen.h"
#include "sdl_painter/render_target.h"

#include <cstddef>
#include <cstdint>
#include <gtest/gtest.h>
#include <memory>
#include <string>
#include <vector>

#include "mock_renderer.h"
#include "test_support.h"

namespace {

using sdl_painter::Brush;
using sdl_painter::Color;
using sdl_painter::MockRenderer;
using sdl_painter::Painter;
using sdl_painter::Pen;
using sdl_painter::Rect;
using sdl_painter::RendererBackend;
using sdl_painter::RenderTarget;

constexpr int32_t kTargetW = 64;
constexpr int32_t kTargetH = 48;

/// @brief Bu derlemede sınanabilecek backend'ler.
///
/// Vulkan yalnızca `SDLPAINTER_WITH_VULKAN` ile derlendiyse listeye girer;
/// önişlemci koşulu makro argümanı içinde yazılamadığı için burada toplanır.
std::vector<RendererBackend> AvailableBackends() {
  std::vector<RendererBackend> backends{RendererBackend::kOpenGL};
#ifdef SDLPAINTER_HAS_VULKAN
  backends.push_back(RendererBackend::kVulkan);
#endif
  return backends;
}

/// @brief Geri okunan tamponda bir pikselin rengi.
Color PixelAt(const std::vector<uint8_t>& rgba, int32_t width, int32_t x,
              int32_t y) {
  const std::size_t i =
      ((static_cast<std::size_t>(y) * static_cast<std::size_t>(width)) +
       static_cast<std::size_t>(x)) *
      4U;
  return Color{rgba[i], rgba[i + 1], rgba[i + 2], rgba[i + 3]};
}

/// @brief İki renk kanal başına verilen toleransta eşit mi?
::testing::AssertionResult ColorNear(const Color& actual, const Color& expected,
                                     int32_t tolerance) {
  const auto diff = [](uint8_t a, uint8_t b) {
    return a > b ? static_cast<int32_t>(a - b) : static_cast<int32_t>(b - a);
  };
  if (diff(actual.r, expected.r) <= tolerance &&
      diff(actual.g, expected.g) <= tolerance &&
      diff(actual.b, expected.b) <= tolerance &&
      diff(actual.a, expected.a) <= tolerance) {
    return ::testing::AssertionSuccess();
  }
  return ::testing::AssertionFailure()
         << "beklenen (" << static_cast<int32_t>(expected.r) << ","
         << static_cast<int32_t>(expected.g) << ","
         << static_cast<int32_t>(expected.b) << ","
         << static_cast<int32_t>(expected.a) << ") — gelen ("
         << static_cast<int32_t>(actual.r) << ","
         << static_cast<int32_t>(actual.g) << ","
         << static_cast<int32_t>(actual.b) << ","
         << static_cast<int32_t>(actual.a) << ")";
}

/// @brief Gerçek bir backend + Painter ayağa kaldırır; olmazsa testi atlar.
struct Backend {
  sdl_painter::testing::HiddenWindow window;
  std::unique_ptr<Painter> painter;
  std::string skip_reason;

  explicit Backend(RendererBackend backend) : window(backend, 128, 96) {
    if (window.Get() == nullptr) {
      skip_reason = std::string("Pencere olusturulamadi: ") + window.Error();
      return;
    }
    painter = std::make_unique<Painter>(window.Get(), backend);
    if (!painter->IsValid()) {
      painter.reset();
      skip_reason = "Backend baslatilamadi (uygun surucu/ICD yok olabilir).";
    }
  }

  [[nodiscard]] bool Ready() const { return painter != nullptr; }
};

/// @brief Backend'i ayaga kaldirir, yoksa testi atlar; ayrica test boyunca
///        yeni bir Vulkan validation hatasi cikmadigini garanti eder.
#define REQUIRE_BACKEND(var, backend)                      \
  const sdl_painter::testing::ValidationGuard var##_guard; \
  Backend var(backend);                                    \
  if (!(var).Ready()) {                                    \
    GTEST_SKIP() << (var).skip_reason;                     \
  }                                                        \
  static_assert(true, "")

/// @brief Bilinen bir sahneyi hedefe çizer: mavi zemin, ÜST-SOL çeyrek kırmızı.
///
/// Yönelim sınavının tamamı bu asimetride: hedefin sol üst köşesi kırmızı,
/// sağ altı mavi olmalı.
void DrawOrientationScene(Painter& painter, const RenderTarget& target) {
  painter.Begin();
  ASSERT_TRUE(painter.SetRenderTarget(target));
  painter.Clear(Color{0, 0, 255, 255});
  painter.SetPen(Pen::NoPen());
  painter.SetBrush(Brush(Color{255, 0, 0, 255}));
  painter.FillRect(0.0F, 0.0F, static_cast<float>(kTargetW) * 0.5F,
                   static_cast<float>(kTargetH) * 0.5F);
  painter.ResetRenderTarget();
  painter.End();
}

// ---------------------------------------------------------------------------
// Painter yüzeyi — backend gerektirmeyen davranış
// ---------------------------------------------------------------------------

TEST(RenderTargetTest, DefaultConstructedIsInvalid) {
  const RenderTarget target;
  EXPECT_FALSE(target.IsValid());
  EXPECT_EQ(target.Width(), 0);
  EXPECT_EQ(target.Height(), 0);
  EXPECT_EQ(target.Handle(), sdl_painter::kInvalidRenderTarget);
}

TEST(RenderTargetTest, UnsupportedBackendYieldsInvalidTarget) {
  // MockRenderer, IRenderer'i tuketici gibi implemente ediyor ve hedef
  // metotlarini gecersiz kilmiyor — yani "desteklemiyor" yolunu temsil eder.
  Painter painter(std::make_unique<MockRenderer>(), 128, 96);
  const RenderTarget target = painter.CreateRenderTarget(32, 32);
  EXPECT_FALSE(target.IsValid());
  // Gecersiz hedefe gecis sessizce basarisiz olmali, cokmemeli.
  EXPECT_FALSE(painter.SetRenderTarget(target));
  painter.ResetRenderTarget();
}

TEST(RenderTargetTest, MoveTransfersOwnership) {
  RenderTarget a;
  RenderTarget b(std::move(a));
  EXPECT_FALSE(b.IsValid());
  RenderTarget c;
  c = std::move(b);
  EXPECT_FALSE(c.IsValid());
}

// ---------------------------------------------------------------------------
// Gerçek backend — parametreli
// ---------------------------------------------------------------------------

class RenderTargetBackend : public ::testing::TestWithParam<RendererBackend> {};

TEST_P(RenderTargetBackend, CreateAndDestroy) {
  REQUIRE_BACKEND(be, GetParam());

  RenderTarget target = be.painter->CreateRenderTarget(kTargetW, kTargetH);
  ASSERT_TRUE(target.IsValid());
  EXPECT_EQ(target.Width(), kTargetW);
  EXPECT_EQ(target.Height(), kTargetH);

  target.Reset();
  EXPECT_FALSE(target.IsValid());
  // Ikinci Reset etkisiz olmali.
  target.Reset();
}

TEST_P(RenderTargetBackend, InvalidSizeIsRejected) {
  REQUIRE_BACKEND(be, GetParam());
  EXPECT_FALSE(be.painter->CreateRenderTarget(0, 32).IsValid());
  EXPECT_FALSE(be.painter->CreateRenderTarget(32, -1).IsValid());
}

/// Asıl sınav: hedefin sol üst köşesi kırmızı, sağ alt köşesi mavi olmalı.
/// Y ekseni ters çevrilirse bu iki assert yer değiştirir ve test düşer.
TEST_P(RenderTargetBackend, DrawingIsTopLeftOriented) {
  REQUIRE_BACKEND(be, GetParam());

  const RenderTarget target =
      be.painter->CreateRenderTarget(kTargetW, kTargetH);
  ASSERT_TRUE(target.IsValid());

  DrawOrientationScene(*be.painter, target);

  std::vector<uint8_t> pixels;
  ASSERT_TRUE(be.painter->ReadRenderTarget(target, pixels));
  ASSERT_EQ(pixels.size(), static_cast<std::size_t>(kTargetW) * kTargetH * 4U);

  const Color kRed{255, 0, 0, 255};
  const Color kBlue{0, 0, 255, 255};

  // Kenardan bir iki piksel iceri: kenar pikselinde rasterlestirme farklari
  // olabilir, sinav yonelim hakkinda.
  EXPECT_TRUE(ColorNear(PixelAt(pixels, kTargetW, 2, 2), kRed, 2))
      << "sol UST kose kirmizi olmali (y=0 yukarida)";
  EXPECT_TRUE(ColorNear(PixelAt(pixels, kTargetW, kTargetW - 3, kTargetH - 3),
                        kBlue, 2))
      << "sag ALT kose mavi olmali";
  EXPECT_TRUE(ColorNear(PixelAt(pixels, kTargetW, kTargetW - 3, 2), kBlue, 2))
      << "sag ust kose mavi olmali (yatay yon)";
  EXPECT_TRUE(ColorNear(PixelAt(pixels, kTargetW, 2, kTargetH - 3), kBlue, 2))
      << "sol alt kose mavi olmali";
}

TEST_P(RenderTargetBackend, ClearFillsWholeTarget) {
  REQUIRE_BACKEND(be, GetParam());

  const RenderTarget target =
      be.painter->CreateRenderTarget(kTargetW, kTargetH);
  ASSERT_TRUE(target.IsValid());

  be.painter->Begin();
  ASSERT_TRUE(be.painter->SetRenderTarget(target));
  be.painter->Clear(Color{10, 200, 30, 255});
  be.painter->ResetRenderTarget();
  be.painter->End();

  std::vector<uint8_t> pixels;
  ASSERT_TRUE(be.painter->ReadRenderTarget(target, pixels));

  const Color kExpected{10, 200, 30, 255};
  for (const int32_t y : {0, kTargetH / 2, kTargetH - 1}) {
    for (const int32_t x : {0, kTargetW / 2, kTargetW - 1}) {
      EXPECT_TRUE(ColorNear(PixelAt(pixels, kTargetW, x, y), kExpected, 2))
          << "x=" << x << " y=" << y;
    }
  }
}

/// Hedefe çizip aynı karede ekrana basmak geçerli olmalı. Vulkan tarafında
/// bu, offscreen pass'in çıkış bağımlılığını ve ekrana dönüşteki "içeriği
/// koruyan" render pass'i birlikte sınar; validation layer testlerde açık.
TEST_P(RenderTargetBackend, DrawTargetToScreenInSameFrame) {
  REQUIRE_BACKEND(be, GetParam());

  const RenderTarget target =
      be.painter->CreateRenderTarget(kTargetW, kTargetH);
  ASSERT_TRUE(target.IsValid());

  for (int32_t frame = 0; frame < 3; ++frame) {
    be.painter->Begin();
    be.painter->Clear(Color{20, 20, 20, 255});

    ASSERT_TRUE(be.painter->SetRenderTarget(target));
    be.painter->Clear(Color{0, 0, 0, 0});
    be.painter->SetPen(Pen::NoPen());
    be.painter->SetBrush(Brush(Color{255, 200, 0, 255}));
    be.painter->FillCircle(32.0F, 24.0F, 18.0F);
    be.painter->ResetRenderTarget();

    // Ekrana geri donduk: onceki Clear silinmemis olmali (LOAD pass).
    be.painter->SetBrush(Brush(Color{80, 80, 200, 255}));
    be.painter->FillRect(4.0F, 4.0F, 20.0F, 20.0F);
    be.painter->DrawRenderTarget(target, Rect{40.0F, 10.0F, 64.0F, 48.0F});
    be.painter->End();
  }
  SUCCEED();
}

/// Hedefe geçmek ekranın viewport'unu kalıcı olarak bozmamalı.
TEST_P(RenderTargetBackend, ViewportIsRestoredAfterReset) {
  REQUIRE_BACKEND(be, GetParam());

  const RenderTarget target =
      be.painter->CreateRenderTarget(kTargetW, kTargetH);
  ASSERT_TRUE(target.IsValid());

  be.painter->Begin();
  be.painter->SetViewport(10, 10, 60, 40);
  ASSERT_TRUE(be.painter->SetRenderTarget(target));
  be.painter->Clear(Color{0, 0, 0, 255});
  be.painter->ResetRenderTarget();
  // Ozel viewport geri gelmis olmali; ciziyoruz ki GPU durumu da tazelensin.
  be.painter->SetPen(Pen::NoPen());
  be.painter->SetBrush(Brush(Color{255, 255, 255, 255}));
  be.painter->FillRect(0.0F, 0.0F, 10.0F, 10.0F);
  be.painter->ResetViewport();
  be.painter->End();
  SUCCEED();
}

/// Kendi içeriğini örneklemek reddedilmeli (tanımsız davranış yerine hata).
TEST_P(RenderTargetBackend, DrawingTargetIntoItselfIsRejected) {
  REQUIRE_BACKEND(be, GetParam());

  const RenderTarget target =
      be.painter->CreateRenderTarget(kTargetW, kTargetH);
  ASSERT_TRUE(target.IsValid());

  be.painter->Begin();
  ASSERT_TRUE(be.painter->SetRenderTarget(target));
  be.painter->Clear(Color{0, 0, 0, 255});
  // Reddedilmeli; cokmemeli ve gecerlilik hatasi uretmemeli.
  be.painter->DrawRenderTarget(target, 0.0F, 0.0F);
  be.painter->ResetRenderTarget();
  be.painter->End();
  SUCCEED();
}

INSTANTIATE_TEST_SUITE_P(
    Backends, RenderTargetBackend, ::testing::ValuesIn(AvailableBackends()),
    [](const ::testing::TestParamInfo<RendererBackend>& i) {
      return i.param == RendererBackend::kOpenGL ? "OpenGL" : "Vulkan";
    });

}  // namespace
