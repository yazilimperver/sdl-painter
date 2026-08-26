/// @file test_frame_render.cpp
/// @brief Gerçek backend üzerinde uçtan uca kare çizimi.
///
/// Diğer testler ya `MockRenderer` kullanır (backend'e hiç girmez) ya da
/// yalnızca `Initialize`/`Shutdown` çağırır. Bu dosya **çizim yolunu**
/// çalıştırır: vertex buffer yazımı, pipeline bağlama, texture yükleme,
/// scissor, push constant / uniform güncellemesi, present ve frame
/// senkronizasyonu.
///
/// Neden değerli: bulduğumuz kritik hataların çoğu (Vulkan'da kırpmanın hiç
/// uygulanmaması, texture yükleme yolundaki sızıntılar, simge durumunda 0x0
/// swapchain) tam olarak bu yolda yaşıyordu ve hiçbir test buraya girmiyordu.
/// Vulkan validation layer testlerde açık olduğundan, senkronizasyon ve
/// image-layout hataları burada otomatik olarak yakalanır.
///
/// @note Bu testler pikselleri doğrulamaz — "çöküyor mu, sürücü/validation
///       şikâyet ediyor mu" sorusunu cevaplar. Piksel doğrulaması ayrı bir
///       readback altyapısı ister.

#include "sdl_painter/brush.h"
#include "sdl_painter/color.h"
#include "sdl_painter/font.h"
#include "sdl_painter/geometry.h"
#include "sdl_painter/image.h"
#include "sdl_painter/painter.h"
#include "sdl_painter/path.h"
#include "sdl_painter/pen.h"

#include <cstdint>
#include <gtest/gtest.h>
#include <memory>
#include <vector>

#include "test_support.h"

namespace {

using sdl_painter::Alignment;
using sdl_painter::Brush;
using sdl_painter::Color;
using sdl_painter::Font;
using sdl_painter::Image;
using sdl_painter::Painter;
using sdl_painter::Path;
using sdl_painter::Pen;
using sdl_painter::Point;
using sdl_painter::Rect;
using sdl_painter::RendererBackend;

constexpr int32_t kWidth = 128;
constexpr int32_t kHeight = 96;

/// @brief Dosya gerektirmeyen prosedürel test görüntüsü (RGBA dama tahtası).
Image MakeCheckerImage(int32_t size = 16) {
  std::vector<uint8_t> pixels(static_cast<std::size_t>(size) * size * 4);
  for (int32_t y = 0; y < size; ++y) {
    for (int32_t x = 0; x < size; ++x) {
      const bool on = ((x / 4) + (y / 4)) % 2 == 0;
      auto* p = pixels.data() + (static_cast<std::size_t>(y) * size + x) * 4;
      p[0] = on ? 220 : 40;
      p[1] = on ? 90 : 40;
      p[2] = 60;
      p[3] = 255;
    }
  }
  return Image::CreateFromData(pixels.data(), size, size, 4);
}

/// @brief Kütüphanenin çizim yüzeyinin büyük kısmına dokunan tek bir kare.
///
/// Kasıtlı olarak zengin: her primitif, texture'lı çizim, metin, kırpma,
/// transform stack ve opaklık aynı karede kullanılır — böylece backend'in
/// durum geçişleri (batch flush, pipeline değişimi, scissor) da çalışır.
void DrawBusyFrame(Painter& painter, const Image& image, Font* font) {
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

  // Texture'lı çizim — üç aşırı yükleme de
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

  // Metin (glyph atlası + texture'lı pipeline)
  if (font != nullptr) {
    painter.SetPen(Pen(Color::White(), 1.0F));
    painter.DrawText(6.0F, 30.0F, "Ag");
    painter.DrawText(Rect{0.0F, 84.0F, 128.0F, 12.0F}, "SDLPainter",
                     Alignment::kCenter);
  }

  painter.End();
}

/// @brief Verilen backend'de birkaç kare çizer; ortam uygun değilse atlar.
///
/// Birden fazla kare kasıtlı: Vulkan'da frames-in-flight, vertex ring
/// buffer'ın slot sıfırlaması, semaphore döngüsü ve gecikmeli texture silme
/// ancak kare sayacı ilerleyince çalışır.
void RunFrames(RendererBackend backend, int32_t frame_count) {
  // Validation mesajlari eskiden yalnizca log'a yaziliyordu; bu koruyucu
  // onlari testi dusuren bir sinyale cevirir (bkz. test_support.h).
  const sdl_painter::testing::ValidationGuard guard;
  const sdl_painter::testing::HiddenWindow window(backend, kWidth, kHeight);
  if (window.Get() == nullptr) {
    GTEST_SKIP() << "Pencere olusturulamadi: " << window.Error();
  }

  Painter painter(window.Get(), backend);
  if (!painter.IsValid()) {
    GTEST_SKIP() << "Backend baslatilamadi (uygun surucu/ICD yok olabilir).";
  }

  const Image image = MakeCheckerImage();
  ASSERT_TRUE(image.IsValid());

  // Font isteğe bağlı: yoksa metin bölümü atlanır ama kare yine çizilir.
  const std::string font_path = sdl_painter::testing::FindSystemFont();
  std::unique_ptr<Font> font;
  if (!font_path.empty()) {
    font = std::make_unique<Font>(font_path, 12);
    if (!font->IsValid()) {
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

// ─── OpenGL ─────────────────────────────────────────────────────────────────

TEST(FrameRender, OpenGLDrawsBusyFrame) {
  RunFrames(RendererBackend::kOpenGL, 1);
}

TEST(FrameRender, OpenGLDrawsRepeatedFrames) {
  RunFrames(RendererBackend::kOpenGL, 5);
}

TEST(FrameRender, OpenGLEmptyFrameIsSafe) {
  const sdl_painter::testing::HiddenWindow window(RendererBackend::kOpenGL,
                                                  kWidth, kHeight);
  if (window.Get() == nullptr) {
    GTEST_SKIP() << "Pencere olusturulamadi: " << window.Error();
  }
  Painter painter(window.Get(), RendererBackend::kOpenGL);
  if (!painter.IsValid()) {
    GTEST_SKIP() << "OpenGL baslatilamadi.";
  }
  // Hiç çizim komutu olmayan kare — batcher boşken flush edilebilmeli.
  painter.Begin();
  painter.End();
  SUCCEED();
}

#ifdef SDLPAINTER_HAS_VULKAN

// ─── Vulkan ─────────────────────────────────────────────────────────────────
//
// Validation layer açık (bkz. VkContext). Bu testler geçiyorsa çizim yolunda
// senkronizasyon / image-layout / kaynak ömrü ihlali yok demektir.

TEST(FrameRender, VulkanDrawsBusyFrame) {
  RunFrames(RendererBackend::kVulkan, 1);
}

/// Frames-in-flight sayısından fazla kare: ring buffer slot sıfırlaması,
/// acquire semaphore döngüsü ve gecikmeli texture silme ancak burada çalışır.
TEST(FrameRender, VulkanDrawsRepeatedFrames) {
  RunFrames(RendererBackend::kVulkan, 6);
}

TEST(FrameRender, VulkanEmptyFrameIsSafe) {
  const sdl_painter::testing::HiddenWindow window(RendererBackend::kVulkan,
                                                  kWidth, kHeight);
  if (window.Get() == nullptr) {
    GTEST_SKIP() << "Pencere olusturulamadi: " << window.Error();
  }
  Painter painter(window.Get(), RendererBackend::kVulkan);
  if (!painter.IsValid()) {
    GTEST_SKIP() << "Vulkan baslatilamadi.";
  }
  painter.Begin();
  painter.End();
  SUCCEED();
}

/// Texture oluşturup yok etmek gecikmeli silme kuyruğunu çalıştırır;
/// kuyruk boşalmadan Shutdown gelirse zorla temizlik yolu da denenir.
TEST(FrameRender, VulkanTextureCreateDestroyCycle) {
  const sdl_painter::testing::HiddenWindow window(RendererBackend::kVulkan,
                                                  kWidth, kHeight);
  if (window.Get() == nullptr) {
    GTEST_SKIP() << "Pencere olusturulamadi: " << window.Error();
  }
  Painter painter(window.Get(), RendererBackend::kVulkan);
  if (!painter.IsValid()) {
    GTEST_SKIP() << "Vulkan baslatilamadi.";
  }

  for (int32_t i = 0; i < 3; ++i) {
    // Her turda yeni Image → yeni texture; tur sonunda yıkılır.
    const Image image = MakeCheckerImage(8 + i * 4);
    ASSERT_TRUE(image.IsValid());
    painter.Begin();
    painter.Clear(Color::Black());
    painter.DrawImage(image, 8.0F, 8.0F);
    painter.End();
  }
  SUCCEED();
}

#endif  // SDLPAINTER_HAS_VULKAN

}  // namespace
