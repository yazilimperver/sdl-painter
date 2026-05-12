#include <gtest/gtest.h>

#include <cstdint>
#include <vector>

#include "mock_renderer.h"
#include "sdl_painter/image.h"
#include "sdl_painter/renderer.h"
#include "sdl_painter/texture.h"

using sdl_painter::Image;
using sdl_painter::MockRenderer;
using sdl_painter::Texture;
using sdl_painter::kInvalidTexture;

// 2x2 RGBA piksel verisi üreten yardımcı.
static std::vector<uint8_t> MakePixels(int32_t w = 2, int32_t h = 2,
                                        int32_t ch = 4) {
  return std::vector<uint8_t>(
      static_cast<std::size_t>(w * h * ch), 128);
}

// ════════════════════════════════════════════════════════════════
//  Texture RAII
// ════════════════════════════════════════════════════════════════

// ─── Yapıcı / IsValid ───────────────────────────────────────────

TEST(Texture, DefaultConstructionIsInvalid) {
  Texture t;
  EXPECT_FALSE(t.IsValid());
  EXPECT_EQ(t.Handle(), kInvalidTexture);
}

TEST(Texture, NullRendererIsInvalid) {
  Texture t(nullptr, kInvalidTexture);
  EXPECT_FALSE(t.IsValid());
}

TEST(Texture, ValidHandleIsValid) {
  MockRenderer r;
  Texture t(&r, 42);
  EXPECT_TRUE(t.IsValid());
  EXPECT_EQ(t.Handle(), 42u);
}

// ─── Destructor → DestroyTexture ────────────────────────────────

TEST(Texture, DestructorCallsDestroyTexture) {
  MockRenderer r;
  {
    Texture t(&r, 7);
    EXPECT_EQ(r.destroy_texture_count, 0);
  }  // destructor
  EXPECT_EQ(r.destroy_texture_count, 1);
}

TEST(Texture, DestructorOnInvalidTextureDoesNotCallDestroy) {
  MockRenderer r;
  {
    Texture t;  // kInvalidTexture
  }
  EXPECT_EQ(r.destroy_texture_count, 0);
}

// ─── Reset ──────────────────────────────────────────────────────

TEST(Texture, ResetMakesInvalid) {
  MockRenderer r;
  Texture t(&r, 3);
  ASSERT_TRUE(t.IsValid());
  t.Reset();
  EXPECT_FALSE(t.IsValid());
  EXPECT_EQ(t.Handle(), kInvalidTexture);
}

TEST(Texture, ResetCallsDestroyTexture) {
  MockRenderer r;
  Texture t(&r, 5);
  t.Reset();
  EXPECT_EQ(r.destroy_texture_count, 1);
}

TEST(Texture, DoubleResetCallsDestroyOnlyOnce) {
  MockRenderer r;
  Texture t(&r, 9);
  t.Reset();
  t.Reset();  // ikinci reset: handle zaten kInvalidTexture
  EXPECT_EQ(r.destroy_texture_count, 1);
}

// ─── Move constructor ────────────────────────────────────────────

TEST(Texture, MoveConstructorTransfersOwnership) {
  MockRenderer r;
  Texture src(&r, 11);
  Texture dst(std::move(src));

  EXPECT_TRUE(dst.IsValid());
  EXPECT_EQ(dst.Handle(), 11u);
  EXPECT_FALSE(src.IsValid());  // NOLINT(bugprone-use-after-move)
}

TEST(Texture, MoveConstructorSourceDoesNotDestroyOnDestruct) {
  MockRenderer r;
  {
    Texture src(&r, 13);
    Texture dst(std::move(src));
    // src yok edildiğinde DestroyTexture çağrılmamalı (handle null)
  }  // dst yok edilir → DestroyTexture bir kez
  EXPECT_EQ(r.destroy_texture_count, 1);
}

// ─── Move assignment ─────────────────────────────────────────────

TEST(Texture, MoveAssignmentTransfersOwnership) {
  MockRenderer r;
  Texture src(&r, 17);
  Texture dst;
  dst = std::move(src);

  EXPECT_TRUE(dst.IsValid());
  EXPECT_EQ(dst.Handle(), 17u);
  EXPECT_FALSE(src.IsValid());  // NOLINT(bugprone-use-after-move)
}

TEST(Texture, MoveAssignmentDestroysOldHandle) {
  MockRenderer r;
  Texture dst(&r, 19);  // eski handle
  Texture src(&r, 23);
  dst = std::move(src);  // dst'nin eski handle'ı yok edilmeli

  EXPECT_EQ(r.destroy_texture_count, 1);  // sadece eski handle
  EXPECT_EQ(dst.Handle(), 23u);
}

TEST(Texture, SelfMoveAssignmentIsSafe) {
  MockRenderer r;
  Texture t(&r, 29);
  // Kendi üzerine move — tanımsız davranış olmamalı
  Texture& ref = t;
  t = std::move(ref);  // NOLINT(bugprone-use-after-move)
  // Bu çağrı sonrası t geçersiz olabilir; önemli olan crash olmaması.
  (void)t;
}

// ════════════════════════════════════════════════════════════════
//  Image::Upload — renderer takibi
// ════════════════════════════════════════════════════════════════

// ─── Temel Upload ────────────────────────────────────────────────

TEST(ImageUpload, InvalidImageReturnsInvalidHandle) {
  MockRenderer r;
  Image img;
  EXPECT_EQ(img.Upload(r), kInvalidTexture);
  EXPECT_EQ(r.create_texture_count, 0);
}

TEST(ImageUpload, FirstUploadCallsCreateTexture) {
  MockRenderer r;
  auto pixels = MakePixels();
  Image img = Image::CreateFromData(pixels.data(), 2, 2, 4);

  auto handle = img.Upload(r);
  EXPECT_NE(handle, kInvalidTexture);
  EXPECT_EQ(r.create_texture_count, 1);
}

TEST(ImageUpload, SecondUploadSameRendererReturnsCached) {
  MockRenderer r;
  auto pixels = MakePixels();
  Image img = Image::CreateFromData(pixels.data(), 2, 2, 4);

  auto h1 = img.Upload(r);
  auto h2 = img.Upload(r);

  EXPECT_EQ(h1, h2);
  // CreateTexture yalnızca bir kez çağrılmış olmalı
  EXPECT_EQ(r.create_texture_count, 1);
}

TEST(ImageUpload, GetHandleMatchesUploadedHandle) {
  MockRenderer r;
  auto pixels = MakePixels();
  Image img = Image::CreateFromData(pixels.data(), 2, 2, 4);

  auto uploaded = img.Upload(r);
  EXPECT_EQ(img.GetHandle(), uploaded);
}

// ─── Renderer değişimi → yeniden yükleme ────────────────────────

TEST(ImageUpload, DifferentRendererTriggersReupload) {
  MockRenderer r1, r2;
  auto pixels = MakePixels();
  Image img = Image::CreateFromData(pixels.data(), 2, 2, 4);

  img.Upload(r1);                  // r1'e yükle
  EXPECT_EQ(r1.create_texture_count, 1);

  img.Upload(r2);                  // r2'ye yükle → yeniden oluşturulmalı
  EXPECT_EQ(r2.create_texture_count, 1);
}

TEST(ImageUpload, DifferentRendererDestroysOldHandle) {
  MockRenderer r1, r2;
  auto pixels = MakePixels();
  Image img = Image::CreateFromData(pixels.data(), 2, 2, 4);

  img.Upload(r1);
  // r2 ile çağrıldığında r1'deki eski handle DestroyTexture ile silinmeli
  img.Upload(r2);
  EXPECT_EQ(r1.destroy_texture_count, 1);
}

TEST(ImageUpload, AfterRendererSwitchNewHandleIsValid) {
  MockRenderer r1, r2;
  auto pixels = MakePixels();
  Image img = Image::CreateFromData(pixels.data(), 2, 2, 4);

  img.Upload(r1);
  auto h2 = img.Upload(r2);
  EXPECT_NE(h2, kInvalidTexture);
}

TEST(ImageUpload, SameRendererSecondTimeNoReuploadAfterSwitch) {
  MockRenderer r1, r2;
  auto pixels = MakePixels();
  Image img = Image::CreateFromData(pixels.data(), 2, 2, 4);

  img.Upload(r1);   // r1'e yükle
  img.Upload(r2);   // r2'ye geç
  img.Upload(r2);   // aynı r2 → önbellekten dönmeli

  EXPECT_EQ(r2.create_texture_count, 1);
}

// ─── Move sonrası Upload ─────────────────────────────────────────

TEST(ImageUpload, MovedFromImageCannotUpload) {
  MockRenderer r;
  auto pixels = MakePixels();
  Image src = Image::CreateFromData(pixels.data(), 2, 2, 4);
  Image dst = std::move(src);

  // moved-from nesne artık geçersiz → Upload kInvalidTexture dönmeli
  auto handle = src.Upload(r);  // NOLINT(bugprone-use-after-move)
  EXPECT_EQ(handle, kInvalidTexture);
  EXPECT_EQ(r.create_texture_count, 0);
}

TEST(ImageUpload, MovedToImageUploadsNormally) {
  MockRenderer r;
  auto pixels = MakePixels();
  Image src = Image::CreateFromData(pixels.data(), 2, 2, 4);
  Image dst = std::move(src);

  auto handle = dst.Upload(r);
  EXPECT_NE(handle, kInvalidTexture);
  EXPECT_EQ(r.create_texture_count, 1);
}
