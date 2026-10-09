/// @file test_render_target.cpp
/// @brief Dokuya çizim (render target): mekanik, yönelim ve geri okuma.
///
/// Bu dosya iki katmanı birlikte sınar. GPU testlerinde gerçek sürücüde
/// çizilen hedefler geri okunup değerleri sınanır (aynı türden piksel testleri
/// test_regression_gpu.cpp ve test_backend_parity.cpp içinde de var). Ekrana
/// çizim ise okunamaz; o yollar için GPU testleri validation destekli duman
/// testidir ve asıl sözleşme `RecordingTargetRenderer` ile mock testlerinde
/// (hedef ömrü, viewport, kendine çizimin reddi) sınanır.
///
/// En kritik iddia yönelim: OpenGL'in framebuffer'ı aşağıdan yukarı,
/// Vulkan'ınki yukarıdan aşağı adreslenir. Bir hedefe çizerken bu fark
/// kapatılmazsa doku bellekte baş aşağı durur; ekranda ters görünür ve
/// `ReadRenderTarget` çıktısı iki backend'de farklı olur. Aşağıdaki testler
/// sahnenin üst yarısına çizip ilk satırları kontrol ederek bunu yakalar.

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
#include "pixel_test_support.h"
#include "recording_target_renderer.h"

namespace {

using sdl_painter::Brush;
using sdl_painter::Color;
using sdl_painter::kInvalidRenderTarget;
using sdl_painter::kInvalidTexture;
using sdl_painter::MockRenderer;
using sdl_painter::Painter;
using sdl_painter::Pen;
using sdl_painter::Rect;
using sdl_painter::RendererBackend;
using sdl_painter::RenderTarget;
using sdl_painter::RenderTargetHandle;
using sdl_painter::testing::AvailableBackends;
using sdl_painter::testing::Backend;
using sdl_painter::testing::ColorNear;
using sdl_painter::testing::LifetimeJournal;
using sdl_painter::testing::PixelAt;
using sdl_painter::testing::RecordingTargetRenderer;

constexpr int32_t kTargetW = 64;
constexpr int32_t kTargetH = 48;

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

TEST(RenderTargetTest, DefaultConstructedIsInvalid) {
  const RenderTarget target;
  EXPECT_FALSE(target.IsValid());
  EXPECT_EQ(target.Width(), 0);
  EXPECT_EQ(target.Height(), 0);
  EXPECT_EQ(target.Handle(), sdl_painter::kInvalidRenderTarget);
}

TEST(RenderTargetTest, UnsupportedBackendYieldsInvalidTarget) {
  // MockRenderer, IRenderer'i tuketici gibi implemente ediyor ve hedef
  // metotlarini gecersiz kilmiyor; yani "desteklemiyor" yolunu temsil eder.
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

/// @brief Geçerli hedef taşıma testleri için `RecordingTargetRenderer` kurar.
///
/// Günlük Painter'dan önce bildirildiği için ondan sonra yıkılır; hedefler
/// testin yerel nesneleridir ve fixture'dan önce yok olur.
class RenderTargetLifetimeTest : public ::testing::Test {
 protected:
  void SetUp() override {
    auto renderer = std::make_unique<RecordingTargetRenderer>();
    mock = renderer.get();
    mock->SetJournal(&journal);
    painter = std::make_unique<Painter>(std::move(renderer), 128, 96);
  }

  LifetimeJournal journal;
  RecordingTargetRenderer* mock{nullptr};
  std::unique_ptr<Painter> painter;
};

TEST_F(RenderTargetLifetimeTest, ShouldKeepHandleAndSizeWhenMovingValidTarget) {
  // Arrange
  RenderTarget source = painter->CreateRenderTarget(32, 24);
  ASSERT_TRUE(source.IsValid());
  const RenderTargetHandle kHandle = source.Handle();

  // Act
  RenderTarget moved(std::move(source));

  // Assert
  EXPECT_TRUE(moved.IsValid());
  EXPECT_EQ(moved.Handle(), kHandle);
  EXPECT_EQ(moved.Width(), 32);
  EXPECT_EQ(moved.Height(), 24);
  EXPECT_FALSE(source.IsValid());  // NOLINT(bugprone-use-after-move)
  EXPECT_NE(mock->GetRenderTargetTexture(kHandle), kInvalidTexture);
  EXPECT_TRUE(journal.render_targets_destroyed.empty());
}

TEST_F(RenderTargetLifetimeTest, ShouldDestroyOldTargetWhenMoveAssigning) {
  // Arrange
  RenderTarget old_target = painter->CreateRenderTarget(32, 24);
  RenderTarget incoming = painter->CreateRenderTarget(16, 8);
  ASSERT_TRUE(old_target.IsValid());
  ASSERT_TRUE(incoming.IsValid());
  const RenderTargetHandle kOldHandle = old_target.Handle();
  const RenderTargetHandle kIncomingHandle = incoming.Handle();
  ASSERT_NE(kOldHandle, kIncomingHandle);

  // Act
  old_target = std::move(incoming);

  // Assert
  EXPECT_EQ(mock->GetRenderTargetTexture(kOldHandle), kInvalidTexture);
  EXPECT_EQ(journal.render_targets_destroyed,
            std::vector<RenderTargetHandle>{kOldHandle});
  EXPECT_EQ(old_target.Handle(), kIncomingHandle);
  EXPECT_EQ(old_target.Width(), 16);
  EXPECT_EQ(old_target.Height(), 8);
  EXPECT_NE(mock->GetRenderTargetTexture(kIncomingHandle), kInvalidTexture);
  EXPECT_FALSE(incoming.IsValid());  // NOLINT(bugprone-use-after-move)
}

TEST_F(RenderTargetLifetimeTest, ShouldDestroyMovedTargetExactlyOnce) {
  // Arrange
  RenderTargetHandle handle = kInvalidRenderTarget;
  {
    RenderTarget source = painter->CreateRenderTarget(32, 24);
    ASSERT_TRUE(source.IsValid());
    handle = source.Handle();
    {
      RenderTarget destination(std::move(source));
      ASSERT_TRUE(destination.IsValid());
    }

    // Assert: tasinan hedef kapsam sonunda silindi, kaynak henuz yasiyor.
    EXPECT_EQ(journal.render_targets_destroyed,
              std::vector<RenderTargetHandle>{handle});
  }

  // Assert: tasinmis kaynagin yikimi ikinci silme uretmedi.
  EXPECT_EQ(journal.render_targets_destroyed,
            std::vector<RenderTargetHandle>{handle});
}

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
///
/// Ekran okunamadığı için bu bir duman testidir: ekrana yapılan bileşimi ve
/// ekran içeriğinin korunmasını (LOAD pass) KANITLAMAZ. Tek sinyal validation
/// koruyucusudur (OpenGL'de o da yok). Yalnızca hedefin kendi içeriği
/// geri okunup sınanır: ekrana basmak hedefi bozmamalı.
TEST_P(RenderTargetBackend, DrawTargetToScreenSmokeKeepsTargetContent) {
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

  std::vector<uint8_t> pixels;
  ASSERT_TRUE(be.painter->ReadRenderTarget(target, pixels));
  ASSERT_EQ(pixels.size(), static_cast<std::size_t>(kTargetW) * kTargetH * 4U);
  EXPECT_TRUE(
      ColorNear(PixelAt(pixels, kTargetW, 32, 24), Color{255, 200, 0, 255}, 2))
      << "daire merkezi sari olmali";
  EXPECT_TRUE(ColorNear(PixelAt(pixels, kTargetW, 1, 1), Color{0, 0, 0, 0}, 2))
      << "kose saydam kalmali";
}

/// Hedefe geçmek ekranın viewport'unu kalıcı olarak bozmamalı.
///
/// Ekran okunamadığı için bu bir duman testidir; adı "restored" dese de
/// geri yüklenen viewport değerini doğrulamaz, yalnızca bu çağrı
/// dizisinin validation hatası üretmediğini sınar. Viewport'un gerçekten geri
/// yüklendiği `ViewportIsRestoredAfterTargetReset` mock testinde sınanır.
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
///
/// GPU tarafı duman testidir: tek renkli hedefin değişmemesi reddi kanıtlamaz
/// (hatalı self-sampling de aynı rengi üretebilir). Reddin asıl iddiası
/// `DrawingTargetIntoItselfProducesNoTexturedDraw` mock testindedir.
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

/// @brief Mock backend'i ve ekrana dönüşte beklenen GPU viewport y değeri.
struct ViewportRestoreCase {
  RendererBackend backend;
  int32_t expected_gpu_y;
};

class RenderTargetViewportMock
    : public ::testing::TestWithParam<ViewportRestoreCase> {};

/// Özel viewport kurulu iken hedefe geçip dönmek, ekrana dönüşteki çizimin
/// özel viewport ile gitmesini sağlamalı. OpenGL'de ekran viewport'unun
/// orijini sol alttır: y = 96 - 10 - 40 = 46; Vulkan'da sol üsttür: y = 10.
TEST_P(RenderTargetViewportMock, ViewportIsRestoredAfterTargetReset) {
  // Arrange
  auto renderer = std::make_unique<RecordingTargetRenderer>();
  RecordingTargetRenderer* mock = renderer.get();
  mock->backend = GetParam().backend;
  Painter painter(std::move(renderer), 128, 96);
  const RenderTarget target = painter.CreateRenderTarget(kTargetW, kTargetH);
  ASSERT_TRUE(target.IsValid());

  // Act
  painter.Begin();
  painter.SetViewport(10, 10, 60, 40);
  ASSERT_TRUE(painter.SetRenderTarget(target));
  painter.ResetRenderTarget();
  painter.SetPen(Pen::NoPen());
  painter.SetBrush(Brush(Color{255, 255, 255, 255}));
  painter.FillRect(0.0F, 0.0F, 10.0F, 10.0F);
  painter.End();

  // Assert
  ASSERT_EQ(mock->draws.size(), 1U);
  const auto& snap = mock->draws[0];
  EXPECT_EQ(snap.target, kInvalidRenderTarget);
  EXPECT_EQ(snap.viewport.x, 10);
  EXPECT_EQ(snap.viewport.y, GetParam().expected_gpu_y);
  EXPECT_EQ(snap.viewport.w, 60);
  EXPECT_EQ(snap.viewport.h, 40);
}

INSTANTIATE_TEST_SUITE_P(
    Backends, RenderTargetViewportMock,
    ::testing::Values(ViewportRestoreCase{RendererBackend::kOpenGL, 46},
                      ViewportRestoreCase{RendererBackend::kVulkan, 10}),
    [](const ::testing::TestParamInfo<ViewportRestoreCase>& i) {
      return i.param.backend == RendererBackend::kOpenGL ? "OpenGL" : "Vulkan";
    });

/// Aktif hedef kendine çizilmeye çalışıldığında backend'e hiçbir dokulu çizim
/// gitmemeli. Reddin kanıtı çağrı yokluğudur; kontrol olarak aynı hedef
/// ekrana dönüldüğünde çizilebilir (aksi halde test boş geçerdi).
TEST(RenderTargetMockTest, DrawingTargetIntoItselfProducesNoTexturedDraw) {
  // Arrange
  auto renderer = std::make_unique<RecordingTargetRenderer>();
  RecordingTargetRenderer* mock = renderer.get();
  Painter painter(std::move(renderer), 128, 96);
  const RenderTarget target = painter.CreateRenderTarget(kTargetW, kTargetH);
  ASSERT_TRUE(target.IsValid());

  // Act
  painter.Begin();
  ASSERT_TRUE(painter.SetRenderTarget(target));
  ASSERT_EQ(mock->ActiveTarget(), target.Handle());
  painter.DrawRenderTarget(target, 0.0F, 0.0F);
  painter.ResetRenderTarget();  // bekleyen cizimleri gonderir

  // Assert: reddedilen cagri hicbir sey gondermedi.
  EXPECT_EQ(mock->CountCalls("DrawTextured"), 0);
  EXPECT_TRUE(mock->draws.empty());

  // Kontrol: ayni hedef ekrana basilabiliyor.
  painter.DrawRenderTarget(target, 0.0F, 0.0F);
  painter.End();
  ASSERT_EQ(mock->draws.size(), 1U);
  EXPECT_EQ(mock->draws[0].kind, "DrawTextured");
  EXPECT_EQ(mock->draws[0].target, kInvalidRenderTarget);
  EXPECT_EQ(mock->draws[0].texture,
            mock->GetRenderTargetTexture(target.Handle()));
}

}  // namespace
