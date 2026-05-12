#include <gtest/gtest.h>

#include <cstdint>
#include <vector>

#include "sdl_painter/image.h"
#include "sdl_painter/renderer.h"

using sdl_painter::Image;
using sdl_painter::kInvalidTexture;

// ─── Varsayilan yapici ─────────────────────────────────────────────────────

TEST(ImageDefault, IsNotValid) {
  Image img;
  EXPECT_FALSE(img.IsValid());
}

TEST(ImageDefault, DimensionsAreZero) {
  Image img;
  EXPECT_EQ(img.Width(), 0);
  EXPECT_EQ(img.Height(), 0);
  EXPECT_EQ(img.Channels(), 0);
}

TEST(ImageDefault, HandleIsInvalid) {
  Image img;
  EXPECT_EQ(img.GetHandle(), kInvalidTexture);
}

// ─── CreateFromData ────────────────────────────────────────────────────────

TEST(ImageCreateFromData, ValidRGBA) {
  constexpr int32_t w = 4, h = 4, ch = 4;
  std::vector<uint8_t> pixels(static_cast<std::size_t>(w * h * ch), 128);
  Image img = Image::CreateFromData(pixels.data(), w, h, ch);

  EXPECT_TRUE(img.IsValid());
  EXPECT_EQ(img.Width(), w);
  EXPECT_EQ(img.Height(), h);
  EXPECT_EQ(img.Channels(), ch);
}

TEST(ImageCreateFromData, ValidRGB) {
  constexpr int32_t w = 8, h = 2, ch = 3;
  std::vector<uint8_t> pixels(static_cast<std::size_t>(w * h * ch), 255);
  Image img = Image::CreateFromData(pixels.data(), w, h, ch);

  EXPECT_TRUE(img.IsValid());
  EXPECT_EQ(img.Width(), w);
  EXPECT_EQ(img.Height(), h);
  EXPECT_EQ(img.Channels(), ch);
}

TEST(ImageCreateFromData, NullDataReturnsInvalid) {
  Image img = Image::CreateFromData(nullptr, 4, 4, 4);
  EXPECT_FALSE(img.IsValid());
}

TEST(ImageCreateFromData, ZeroWidthReturnsInvalid) {
  uint8_t dummy = 0;
  Image img = Image::CreateFromData(&dummy, 0, 4, 4);
  EXPECT_FALSE(img.IsValid());
}

TEST(ImageCreateFromData, ZeroHeightReturnsInvalid) {
  uint8_t dummy = 0;
  Image img = Image::CreateFromData(&dummy, 4, 0, 4);
  EXPECT_FALSE(img.IsValid());
}

TEST(ImageCreateFromData, ZeroChannelsReturnsInvalid) {
  uint8_t dummy = 0;
  Image img = Image::CreateFromData(&dummy, 4, 4, 0);
  EXPECT_FALSE(img.IsValid());
}

TEST(ImageCreateFromData, HandleIsInvalidBeforeUpload) {
  std::vector<uint8_t> pixels(4 * 4 * 4, 0);
  Image img = Image::CreateFromData(pixels.data(), 4, 4, 4);
  EXPECT_EQ(img.GetHandle(), kInvalidTexture);
}

TEST(ImageCreateFromData, DataIsCopied) {
  constexpr int32_t w = 2, h = 2, ch = 4;
  std::vector<uint8_t> pixels = {
      255, 0, 0, 255,   0, 255, 0, 255,
      0, 0, 255, 255,   255, 255, 0, 255};
  Image img = Image::CreateFromData(pixels.data(), w, h, ch);

  // Kaynak degistirme imajı etkilememeli
  pixels[0] = 0;
  EXPECT_EQ(img.RawData()[0], 255);
}

// ─── Dosyadan yukleme ─────────────────────────────────────────────────────

TEST(ImageLoadFile, NonExistentFileIsInvalid) {
  Image img("non_existent_file_xyz.png");
  EXPECT_FALSE(img.IsValid());
  EXPECT_EQ(img.Width(), 0);
  EXPECT_EQ(img.Height(), 0);
}

// ─── Move semantigi ───────────────────────────────────────────────────────

TEST(ImageMove, MoveConstructorTransfersOwnership) {
  std::vector<uint8_t> pixels(4 * 4 * 4, 200);
  Image src = Image::CreateFromData(pixels.data(), 4, 4, 4);
  ASSERT_TRUE(src.IsValid());

  Image dst = std::move(src);
  EXPECT_TRUE(dst.IsValid());
  EXPECT_FALSE(src.IsValid());  // NOLINT(bugprone-use-after-move)
}

TEST(ImageMove, MoveAssignmentTransfersOwnership) {
  std::vector<uint8_t> pixels(4 * 4 * 4, 100);
  Image src = Image::CreateFromData(pixels.data(), 4, 4, 4);
  ASSERT_TRUE(src.IsValid());

  Image dst;
  dst = std::move(src);
  EXPECT_TRUE(dst.IsValid());
  EXPECT_FALSE(src.IsValid());  // NOLINT(bugprone-use-after-move)
}

// ─── RawData ──────────────────────────────────────────────────────────────

TEST(ImageRawData, NullForDefaultImage) {
  Image img;
  EXPECT_EQ(img.RawData(), nullptr);
}

TEST(ImageRawData, NonNullForValidImage) {
  std::vector<uint8_t> pixels(8 * 8 * 3, 0);
  Image img = Image::CreateFromData(pixels.data(), 8, 8, 3);
  EXPECT_NE(img.RawData(), nullptr);
}

TEST(ImageRawData, FirstPixelMatchesInput) {
  std::vector<uint8_t> pixels = {10, 20, 30, 40,  0, 0, 0, 0};
  Image img = Image::CreateFromData(pixels.data(), 2, 1, 4);
  ASSERT_NE(img.RawData(), nullptr);
  EXPECT_EQ(img.RawData()[0], 10);
  EXPECT_EQ(img.RawData()[1], 20);
  EXPECT_EQ(img.RawData()[2], 30);
  EXPECT_EQ(img.RawData()[3], 40);
}
