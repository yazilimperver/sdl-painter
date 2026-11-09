// Kaynak ömrü: Texture, Image ve Font onları yükleyen renderer'dan (yani
// Painter'dan) uzun yaşayabilir.
//
// Eskiden üçü de ham `IRenderer*` tutuyordu; renderer önce yıkılınca
// yıkıcıları ölü nesne üzerinden DestroyTexture çağırıyordu (tanımsız
// davranış). Şimdi IRenderer::LifetimeToken ile renderer'ın ölümünü görürler.
//
// "Aynı adres" testleri ikinci tehlikeyi sınar: ölen renderer'ın adresinde
// yenisi kurulursa yalnızca işaretçiye bakan kod eski texture tanımlayıcısını
// yeni renderer'a aitmiş gibi verirdi. Yerleştirme `new` ile adres
// sabitlenir. Yıkılmış nesneye hiçbir test erişmez; ASan derlemesi bu yolu
// denetler.

#include "sdl_painter/font.h"
#include "sdl_painter/image.h"
#include "sdl_painter/painter.h"
#include "sdl_painter/renderer.h"
#include "sdl_painter/texture.h"

#include <array>
#include <cstdint>
#include <gtest/gtest.h>
#include <memory>
#include <new>
#include <string>
#include <utility>

#include "mock_renderer.h"
#include "test_support.h"

using sdl_painter::Font;
using sdl_painter::Image;
using sdl_painter::kInvalidTexture;
using sdl_painter::MockRenderer;
using sdl_painter::Painter;
using sdl_painter::Texture;

namespace {

constexpr int32_t kViewport = 64;

/// @brief 2x2 RGBA görüntü.
Image SmallImage() {
  const std::array<uint8_t, 16> kPixels{};
  return Image::CreateFromData(kPixels.data(), 2, 2, 4);
}

/// @brief Aynı bellek adresinde art arda MockRenderer kurmak için yer.
struct RendererSlot {
  alignas(MockRenderer) unsigned char storage[sizeof(MockRenderer)];

  MockRenderer* Construct() { return new (storage) MockRenderer(); }
  static void Destroy(MockRenderer* renderer) { renderer->~MockRenderer(); }
};

}  // namespace

// --- Texture ---

TEST(ResourceLifetime, TextureReleasesThroughLiveRenderer) {
  MockRenderer renderer;
  {
    const Texture texture(&renderer, renderer.CreateTexture(nullptr, 2, 2, 4));
    EXPECT_TRUE(texture.IsValid());
    EXPECT_EQ(texture.Owner(), &renderer);
  }
  EXPECT_EQ(renderer.destroy_texture_count, 1);
}

TEST(ResourceLifetime, TextureOutlivingRendererIsInvalidAndSilent) {
  auto renderer = std::make_unique<MockRenderer>();
  Texture texture(renderer.get(), renderer->CreateTexture(nullptr, 2, 2, 4));
  ASSERT_TRUE(texture.IsValid());

  renderer.reset();

  EXPECT_FALSE(texture.IsValid());
  EXPECT_EQ(texture.Owner(), nullptr);
  EXPECT_EQ(texture.Handle(), kInvalidTexture);
  texture.Reset();  // Ölü renderer'a dokunmamalı.
  EXPECT_FALSE(texture.IsValid());
}

TEST(ResourceLifetime, MovedTextureKeepsItsOwnersLifetime) {
  auto renderer = std::make_unique<MockRenderer>();
  Texture source(renderer.get(), renderer->CreateTexture(nullptr, 2, 2, 4));
  Texture moved(std::move(source));
  EXPECT_TRUE(moved.IsValid());

  renderer.reset();
  EXPECT_FALSE(moved.IsValid());
}

TEST(ResourceLifetime, TextureIsNotFooledByRendererAtSameAddress) {
  RendererSlot slot;
  MockRenderer* first = slot.Construct();
  Texture texture(first, first->CreateTexture(nullptr, 2, 2, 4));
  RendererSlot::Destroy(first);

  MockRenderer* second = slot.Construct();
  ASSERT_EQ(static_cast<void*>(first), static_cast<void*>(second));
  EXPECT_FALSE(texture.IsValid());
  EXPECT_EQ(texture.Owner(), nullptr);

  texture.Reset();
  EXPECT_EQ(second->destroy_texture_count, 0)
      << "olu renderer'in tanimlayicisi yenisine silindi";
  RendererSlot::Destroy(second);
}

// --- Image ---

TEST(ResourceLifetime, ImageOutlivingPainterIsSafeAndUploadsToNextPainter) {
  const Image image = SmallImage();
  ASSERT_TRUE(image.IsValid());
  {
    auto renderer = std::make_unique<MockRenderer>();
    MockRenderer* raw = renderer.get();
    Painter painter(std::move(renderer), kViewport, kViewport);
    painter.Begin();
    painter.DrawImage(image, 0.0F, 0.0F);
    painter.End();
    EXPECT_EQ(raw->create_texture_count, 1);
    EXPECT_NE(image.GetHandle(), kInvalidTexture);
  }
  // Painter yok: texture geçersiz, piksel verisi yerinde.
  EXPECT_EQ(image.GetHandle(), kInvalidTexture);
  EXPECT_TRUE(image.IsValid());

  auto renderer = std::make_unique<MockRenderer>();
  MockRenderer* raw = renderer.get();
  Painter painter(std::move(renderer), kViewport, kViewport);
  painter.Begin();
  painter.DrawImage(image, 0.0F, 0.0F);
  painter.End();
  EXPECT_EQ(raw->create_texture_count, 1) << "yeni Painter'a yuklenmedi";
}

TEST(ResourceLifetime, ImageReuploadsToRendererAtSameAddress) {
  const Image image = SmallImage();
  RendererSlot slot;
  MockRenderer* first = slot.Construct();
  const auto kFirstHandle = image.Upload(*first);
  ASSERT_NE(kFirstHandle, kInvalidTexture);
  RendererSlot::Destroy(first);

  MockRenderer* second = slot.Construct();
  ASSERT_EQ(static_cast<void*>(first), static_cast<void*>(second));
  const auto kSecondHandle = image.Upload(*second);
  EXPECT_NE(kSecondHandle, kInvalidTexture);
  EXPECT_EQ(second->create_texture_count, 1)
      << "eski tanimlayici yeni renderer'a verildi";
  EXPECT_EQ(second->destroy_texture_count, 0);
  RendererSlot::Destroy(second);
}

// --- Font ---

TEST(ResourceLifetime, FontOutlivingRendererRebindsToNextRenderer) {
  SDLPAINTER_REQUIRE_FONT_OR_SKIP(path);
  Font font(path, 24);
  ASSERT_TRUE(font.IsValid());

  auto first = std::make_unique<MockRenderer>();
  ASSERT_NE(font.GetGlyph(*first, U'A'), nullptr);
  EXPECT_GT(first->create_texture_count, 0);
  first.reset();

  // Bağlı renderer öldü: yeni renderer reddedilmez, glyph ona yüklenir.
  MockRenderer second;
  EXPECT_NE(font.GetGlyph(second, U'A'), nullptr);
  EXPECT_GT(second.create_texture_count, 0);
  // `second` Font'tan önce yıkılır; ~Font yine ölü renderer'a dokunmamalı.
}

TEST(ResourceLifetime, FontRebindsToRendererAtSameAddress) {
  SDLPAINTER_REQUIRE_FONT_OR_SKIP(path);
  Font font(path, 24);
  ASSERT_TRUE(font.IsValid());

  RendererSlot slot;
  MockRenderer* first = slot.Construct();
  ASSERT_NE(font.GetGlyph(*first, U'A'), nullptr);
  RendererSlot::Destroy(first);

  MockRenderer* second = slot.Construct();
  ASSERT_EQ(static_cast<void*>(first), static_cast<void*>(second));
  // Yalnızca işaretçiye bakılsaydı önbellek isabeti eski glyph'i dönerdi.
  EXPECT_NE(font.GetGlyph(*second, U'A'), nullptr);
  EXPECT_GT(second->create_texture_count, 0)
      << "olu renderer'in glyph'i yeni renderer'a verildi";
  EXPECT_EQ(second->destroy_texture_count, 0);
  RendererSlot::Destroy(second);
}

TEST(ResourceLifetime, FontStillRejectsSecondLiveRenderer) {
  SDLPAINTER_REQUIRE_FONT_OR_SKIP(path);
  MockRenderer first;
  MockRenderer second;
  Font font(path, 24);
  ASSERT_NE(font.GetGlyph(first, U'A'), nullptr);
  EXPECT_EQ(font.GetGlyph(second, U'A'), nullptr);
}
