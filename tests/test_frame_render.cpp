/// @file test_frame_render.cpp
/// @brief Gerçek backend üzerinde uçtan uca kare çizimi (ekrana sunum).
///
/// Bu dosya çizim yolunu çalıştırır: vertex buffer yazımı, pipeline bağlama,
/// texture yükleme, scissor, push constant / uniform güncellemesi, present ve
/// frame senkronizasyonu. Kareler pencereye sunulur ve ekranın içeriği
/// okunmaz; bu yüzden testler pikselleri doğrulamaz, yalnızca "çöküyor mu,
/// sürücü/validation şikâyet ediyor mu" sorusunu cevaplar. Vulkan validation
/// layer testlerde açık olduğundan (bkz. `ValidationGuard`), senkronizasyon ve
/// image-layout hataları burada otomatik olarak yakalanır. Offscreen hedefe
/// çizip pikselleri okuyan testler `test_regression_gpu.cpp` ve
/// `test_backend_parity.cpp` içindedir.
///
/// Neden değerli: bulduğumuz kritik hataların çoğu (Vulkan'da kırpmanın hiç
/// uygulanmaması, texture yükleme yolundaki sızıntılar, simge durumunda 0x0
/// swapchain) tam olarak bu yolda yaşıyordu ve hiçbir test buraya girmiyordu.
///
/// Sınırlar: yeniden boyutlandırma testleri duman testidir (yalnızca çökme ve
/// validation hatası yokluğu). Simge durumunda 0x0 yüzey bu dosyada
/// sınanmıyor: gizli pencere 0x0 yüzey üretmiyor.

#include "sdl_painter/brush.h"
#include "sdl_painter/color.h"
#include "sdl_painter/font.h"
#include "sdl_painter/geometry.h"
#include "sdl_painter/image.h"
#include "sdl_painter/painter.h"
#include "sdl_painter/path.h"
#include "sdl_painter/pen.h"
#include "sdl_painter/render_target.h"
#include "sdl_painter/renderer.h"

#include <SDL3/SDL.h>

#include <cstdint>
#include <gtest/gtest.h>
#include <memory>
#include <vector>

#include "pixel_test_support.h"

namespace {

using sdl_painter::Alignment;
using sdl_painter::BlendMode;
using sdl_painter::Brush;
using sdl_painter::Color;
using sdl_painter::Font;
using sdl_painter::Image;
using sdl_painter::ImageFlip;
using sdl_painter::LineCap;
using sdl_painter::LineJoin;
using sdl_painter::Painter;
using sdl_painter::Path;
using sdl_painter::Pen;
using sdl_painter::Point;
using sdl_painter::Rect;
using sdl_painter::RendererBackend;
using sdl_painter::RenderTarget;
using sdl_painter::TextWrap;
using sdl_painter::testing::AvailableBackends;
using sdl_painter::testing::MakeCheckerImage;

constexpr int32_t kWidth = 128;
constexpr int32_t kHeight = 96;

/// @brief Kütüphanenin çizim yüzeyinin büyük kısmına dokunan tek bir kare.
///
/// Kasıtlı olarak zengin: her primitif, texture'lı çizim, metin, kırpma,
/// transform stack ve opaklık aynı karede kullanılır; böylece backend'in
/// durum geçişleri (batch flush, pipeline değişimi, scissor) da çalışır.
void DrawBusyFrame(Painter& painter, const Image& image, Font* font) {
  // Hedef kareden once yaratilir, kare bitince yikilir: gecikmeli silme yolu da
  // her karede calisir.
  RenderTarget layer = painter.CreateRenderTarget(32, 24);
  ASSERT_TRUE(layer.IsValid());

  painter.Begin();
  painter.Clear(Color{18, 18, 28, 255});

  // Dolgular ve çerçeveler
  painter.SetBrush(Brush(Color{60, 120, 200, 255}));
  painter.FillRect(4.0F, 4.0F, 40.0F, 24.0F);
  painter.FillCircle(70.0F, 20.0F, 12.0F);
  painter.FillEllipse(104.0F, 20.0F, 16.0F, 10.0F);

  painter.SetPen(Pen(Color{240, 200, 80, 255}, 3.0F));
  painter.DrawRect(4.0F, 34.0F, 40.0F, 20.0F);
  painter.DrawCircle(70.0F, 48.0F, 10.0F);
  painter.DrawEllipse(104.0F, 48.0F, 14.0F, 8.0F);
  painter.DrawLine(4.0F, 60.0F, 120.0F, 60.0F);

  // Poligon + polyline (ear clipping ve birleşim yolları)
  const std::vector<Point> kConcave = {{8.0F, 66.0F},  {30.0F, 66.0F},
                                       {30.0F, 88.0F}, {20.0F, 88.0F},
                                       {20.0F, 76.0F}, {8.0F, 76.0F}};
  painter.SetBrush(Brush(Color{120, 200, 120, 200}));
  painter.FillPolygon(kConcave);
  painter.DrawPolygon(kConcave);
  painter.DrawPolyline({{40.0F, 88.0F}, {56.0F, 68.0F}, {72.0F, 88.0F}});

  // Yol: Bézier düzleştirme + çok parçalı dolgu. Kapalı ve açık alt yol
  // birlikte, ki hem StrokeClosedPath hem StrokeOpenPath yolu çalışsın.
  Path path;
  path.MoveTo(4.0F, 92.0F);
  path.CubicTo(20.0F, 70.0F, 44.0F, 70.0F, 60.0F, 92.0F);
  path.MoveTo(80.0F, 92.0F);
  path.QuadTo(96.0F, 72.0F, 112.0F, 92.0F);
  path.Close();
  painter.SetBrush(Brush(Color{200, 140, 240, 160}));
  painter.FillPath(path);
  painter.SetPen(Pen(Color{240, 220, 255, 255}, 2.0F));
  painter.DrawPath(path);

  // Texture'lı çizim, üç aşırı yükleme de
  painter.DrawImage(image, 76.0F, 66.0F);
  painter.DrawImage(image, Rect{96.0F, 66.0F, 24.0F, 20.0F});
  painter.DrawImage(image, Rect{0.0F, 0.0F, 8.0F, 8.0F},
                    Rect{50.0F, 4.0F, 12.0F, 12.0F});

  // Kırpma
  painter.SetClipRect(Rect{10.0F, 10.0F, 40.0F, 30.0F});
  painter.SetBrush(Brush(Color{255, 80, 80, 180}));
  painter.FillRect(0.0F, 0.0F, 128.0F, 96.0F);
  painter.ClearClip();

  // Transform stack + opaklık
  painter.Save();
  painter.SetOpacity(0.5F);
  painter.Translate(64.0F, 48.0F);
  painter.Rotate(30.0F);
  painter.Scale(1.5F, 0.75F);
  painter.SetBrush(Brush(Color{200, 200, 255, 255}));
  painter.FillRect(-10.0F, -6.0F, 20.0F, 12.0F);
  painter.Restore();
  painter.ResetTransform();

  // Yuvarlatilmis dikdortgen ile yay, dilim ve kiris. Bu ve sonraki bloklar
  // Save/Restore icinde: kalem, firca ve karistirma modu sizmaz.
  painter.Save();
  painter.SetBrush(Brush(Color{90, 160, 220, 200}));
  painter.SetPen(Pen(Color{250, 250, 250, 255}, 1.5F));
  painter.FillRoundedRect(46.0F, 28.0F, 26.0F, 16.0F, 5.0F);
  painter.DrawRoundedRect(46.0F, 28.0F, 26.0F, 16.0F, 5.0F);
  painter.FillPie(24.0F, 24.0F, 12.0F, 8.0F, 20.0F, 250.0F);
  painter.DrawPie(24.0F, 24.0F, 12.0F, 8.0F, 20.0F, 250.0F);
  painter.FillChord(104.0F, 70.0F, 10.0F, 10.0F, 30.0F, 200.0F);
  painter.DrawChord(104.0F, 70.0F, 10.0F, 10.0F, 30.0F, 200.0F);
  painter.DrawArc(70.0F, 48.0F, 16.0F, 12.0F, -40.0F, 270.0F);
  painter.Restore();

  // Karistirma modlari: her mod hem duz hem dokulu cizimle (iki pipeline
  // varyanti). Sonunda varsayilan mod geri yuklenir.
  painter.Save();
  painter.SetPen(Pen::NoPen());
  painter.SetBrush(Brush(Color{200, 120, 60, 200}));
  float blend_x = 4.0F;
  for (const BlendMode mode :
       {BlendMode::kAdditive, BlendMode::kMultiply, BlendMode::kNone}) {
    painter.SetBlendMode(mode);
    painter.FillRect(blend_x, 56.0F, 10.0F, 8.0F);
    painter.DrawImage(image, Rect{blend_x, 66.0F, 10.0F, 8.0F});
    blend_x += 14.0F;
  }
  painter.SetBlendMode(BlendMode::kAlpha);
  painter.Restore();

  // Gradient firca: dogrusal ve isinsal, duz ve yuvarlatilmis sekillerde.
  painter.Save();
  painter.SetPen(Pen::NoPen());
  painter.SetBrush(Brush::LinearGradient({80.0F, 4.0F}, {120.0F, 28.0F},
                                         Color{255, 80, 80, 255},
                                         Color{80, 80, 255, 64}));
  painter.FillRoundedRect(80.0F, 4.0F, 40.0F, 24.0F, 6.0F);
  painter.SetBrush(Brush::RadialGradient({100.0F, 40.0F}, 12.0F,
                                         Color{255, 255, 120, 255},
                                         Color{255, 120, 0, 0}));
  painter.FillCircle(100.0F, 40.0F, 12.0F);
  painter.Restore();

  // Kesik desenli kalem; uc ve birlesim stilleri.
  painter.Save();
  Pen dashed(Color{255, 160, 0, 255}, 2.0F);
  dashed.SetDashPattern({6.0F, 3.0F});
  dashed.SetCapStyle(LineCap::kRound);
  dashed.SetJoinStyle(LineJoin::kMiter);
  painter.SetPen(dashed);
  painter.DrawLine(4.0F, 80.0F, 120.0F, 80.0F);
  painter.DrawPolyline({{8.0F, 94.0F}, {24.0F, 82.0F}, {40.0F, 94.0F}});
  dashed.SetCapStyle(LineCap::kSquare);
  dashed.SetJoinStyle(LineJoin::kBevel);
  painter.SetPen(dashed);
  painter.DrawPolyline({{48.0F, 94.0F}, {64.0F, 82.0F}, {80.0F, 94.0F}});
  painter.DrawRoundedRect(84.0F, 82.0F, 36.0F, 12.0F, 4.0F);
  painter.Restore();

  // Gorsel: tint, aynalama, serbest izgara ve yerinde guncelleme.
  painter.Save();
  painter.DrawImage(image, 2.0F, 2.0F, Color{255, 128, 128, 200});
  painter.DrawImage(image, Rect{20.0F, 2.0F, 14.0F, 14.0F}, Color::White(),
                    ImageFlip::kHorizontal);
  painter.DrawImage(image, Rect{36.0F, 2.0F, 14.0F, 14.0F}, Color::White(),
                    ImageFlip::kVertical);
  painter.DrawImage(image, Rect{0.0F, 0.0F, 8.0F, 8.0F},
                    Rect{54.0F, 18.0F, 10.0F, 10.0F}, Color::White(),
                    ImageFlip::kBoth);
  const std::vector<Point> kGrid = {
      {96.0F, 68.0F}, {108.0F, 66.0F}, {120.0F, 68.0F},
      {94.0F, 78.0F}, {108.0F, 80.0F}, {122.0F, 78.0F},
      {96.0F, 90.0F}, {108.0F, 92.0F}, {120.0F, 90.0F}};
  painter.DrawImageMesh(image, 2, 2, kGrid, Color{200, 255, 200, 255});
  // Ayni icerikle guncelleme: kareyi degistirmez, flush + UpdateTexture yolunu
  // calistirir.
  painter.UpdateImage(image, image.RawData());
  painter.DrawImage(image, 56.0F, 44.0F);
  painter.Restore();

  // Hedefe cizip ekrana basma: tint ve aynalamali ve olcekli hali de.
  painter.Save();
  painter.SetPen(Pen::NoPen());
  painter.SetBrush(Brush(Color{255, 200, 80, 160}));
  EXPECT_TRUE(painter.SetRenderTarget(layer));
  painter.Clear(Color{0, 0, 0, 0});
  painter.FillCircle(16.0F, 12.0F, 10.0F);
  painter.ResetRenderTarget();
  painter.DrawRenderTarget(layer, 90.0F, 50.0F);
  painter.DrawRenderTarget(layer, Rect{50.0F, 70.0F, 24.0F, 16.0F},
                           Color{255, 255, 255, 180}, ImageFlip::kHorizontal);
  painter.Restore();

  // Metin (glyph atlası + texture'lı pipeline)
  if (font != nullptr) {
    painter.SetPen(Pen(Color::White(), 1.0F));
    painter.DrawText(6.0F, 30.0F, "Ag");
    painter.DrawText(Rect{0.0F, 84.0F, 128.0F, 12.0F}, "SDLPainter",
                     Alignment::kCenter);
    // Cok satirli ve sozcuk kaydirmali metin.
    painter.DrawText(76.0F, 50.0F, "iki\nsatir");
    painter.DrawText(Rect{4.0F, 36.0F, 36.0F, 24.0F}, "sozcuk kaydirma ornegi",
                     Alignment::kLeft, TextWrap::kWord);
  }

  painter.End();
}

/// @brief Verilen backend'de birkaç kare çizer; ortam uygun değilse atlar.
///
/// Birden fazla kare kasıtlı: Vulkan'da frames-in-flight, vertex ring
/// buffer'ın slot sıfırlaması, semaphore döngüsü ve gecikmeli texture silme
/// ancak kare sayacı ilerleyince çalışır.
void RunFrames(RendererBackend backend, int32_t frame_count) {
  // Makro ValidationGuard'i Painter'dan once tanimlar: validation mesajlari
  // yalnizca log'a yazilir, koruyucu onlari testi dusuren bir sinyale cevirir
  // (bkz. test_support.h).
  REQUIRE_BACKEND(be, backend);
  Painter& painter = *be.painter;

  const Image image = MakeCheckerImage();
  ASSERT_TRUE(image.IsValid());

  // Font isteğe bağlı: yoksa metin bölümü atlanır ama kare yine çizilir.
  // Yalnizca SDLPAINTER_REQUIRE_FONT ayarliysa font yoklugu hatadir.
  const std::string font_path = sdl_painter::testing::FindSystemFont();
  if (font_path.empty() && sdl_painter::testing::FontIsRequired()) {
    FAIL() << "Sistemde TTF font bulunamadi, ancak SDLPAINTER_REQUIRE_FONT "
              "ayarli. Metin yolu sessizce atlanamaz.";
  }
  std::unique_ptr<Font> font;
  if (!font_path.empty()) {
    font = std::make_unique<Font>(font_path, 12);
    if (!font->IsValid()) {
      if (sdl_painter::testing::FontIsRequired()) {
        FAIL() << "Font yuklenemedi, ancak SDLPAINTER_REQUIRE_FONT ayarli: "
               << font_path;
      }
      font.reset();
    }
  }
  if (font) {
    // Painter shared_ptr istiyor; sahiplik burada, Painter'dan önce yıkılır.
    painter.SetFont(std::shared_ptr<Font>(font.get(), [](Font*) {}));
  }

  for (int32_t i = 0; i < frame_count; ++i) {
    DrawBusyFrame(painter, image, font.get());
  }

  SUCCEED();
}

/// @brief Pencereyi yeniden boyutlandirip kare cizer.
///
/// Gizli pencerede SDL_SetWindowSize calisir (SDL_SyncWindow ile beklenir);
/// Vulkan'da swapchain yeniden olusturulur, OpenGL'de viewport guncellenir.
/// Ekran okunamadigi icin yalnizca cokme ve validation hatasi yoklugu sinanir.
void RunFramesAcrossResize(RendererBackend backend) {
  REQUIRE_BACKEND(be, backend);
  Painter& painter = *be.painter;
  SDL_Window* window = be.window.Get();

  const Image image = MakeCheckerImage();
  ASSERT_TRUE(image.IsValid());

  const auto resize_to = [window](int32_t width, int32_t height) {
    SDL_SetWindowSize(window, width, height);
    SDL_SyncWindow(window);
    int32_t pixel_width = 0;
    int32_t pixel_height = 0;
    SDL_GetWindowSizeInPixels(window, &pixel_width, &pixel_height);
    return pixel_width == width && pixel_height == height;
  };

  DrawBusyFrame(painter, image, nullptr);
  ASSERT_TRUE(resize_to(192, 144)) << "Pencere buyutulemedi.";
  DrawBusyFrame(painter, image, nullptr);
  DrawBusyFrame(painter, image, nullptr);
  ASSERT_TRUE(resize_to(64, 48)) << "Pencere kucultulemedi.";
  DrawBusyFrame(painter, image, nullptr);
  DrawBusyFrame(painter, image, nullptr);

  SUCCEED();
}

class FrameRender : public ::testing::TestWithParam<RendererBackend> {};

TEST_P(FrameRender, DrawsBusyFrame) {
  RunFrames(GetParam(), 1);
}

/// Frames-in-flight sayısından fazla kare: Vulkan'da ring buffer slot
/// sıfırlaması, acquire semaphore döngüsü ve gecikmeli texture silme ancak
/// burada çalışır. OpenGL de aynı kare sayısıyla koşar.
TEST_P(FrameRender, DrawsRepeatedFrames) {
  RunFrames(GetParam(), 6);
}

/// Vulkan'da swapchain'in yeniden oluşturulması validation altında temiz
/// olmalı; OpenGL'de viewport güncellenir.
TEST_P(FrameRender, DrawsFramesAcrossWindowResize) {
  RunFramesAcrossResize(GetParam());
}

TEST_P(FrameRender, EmptyFrameIsSafe) {
  REQUIRE_BACKEND(be, GetParam());
  Painter& painter = *be.painter;
  // Hiç çizim komutu olmayan kare: batcher boşken flush edilebilmeli.
  painter.Begin();
  painter.End();
  SUCCEED();
}

/// Texture oluşturup yok etmek Vulkan'da gecikmeli silme kuyruğunu çalıştırır;
/// kuyruk boşalmadan Shutdown gelirse zorla temizlik yolu da denenir.
/// OpenGL'de silme hemen yapılır, aynı senaryo orada da anlamlıdır
/// (oluştur, çiz, yık döngüsü doku ömrünü sınar).
TEST_P(FrameRender, TextureCreateDestroyCycle) {
  REQUIRE_BACKEND(be, GetParam());
  Painter& painter = *be.painter;

  for (int32_t i = 0; i < 3; ++i) {
    // Her turda yeni Image, yeni texture; tur sonunda yıkılır.
    const Image image = MakeCheckerImage(8 + i * 4);
    ASSERT_TRUE(image.IsValid());
    painter.Begin();
    painter.Clear(Color::Black());
    painter.DrawImage(image, 8.0F, 8.0F);
    painter.End();
  }
  SUCCEED();
}

INSTANTIATE_TEST_SUITE_P(
    Backends, FrameRender, ::testing::ValuesIn(AvailableBackends()),
    [](const ::testing::TestParamInfo<RendererBackend>& i) {
      return i.param == RendererBackend::kOpenGL ? "OpenGL" : "Vulkan";
    });

}  // namespace
