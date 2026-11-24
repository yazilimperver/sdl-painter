// Kendi stb_image implementasyonunu derleyen bir tüketicinin SDLPainter'ın
// statik kütüphanesiyle birlikte bağlanabildiğini doğrular.
//
// Bu dosya, tüketici gibi, STB_IMAGE_IMPLEMENTATION ile stbi_* fonksiyonlarını
// dışarı açık tanımlar. src/image.cpp stb'yi STB_IMAGE_STATIC ile derlemezse
// aynı semboller kütüphanede de dışarı açık olur ve bu test ikilisi
// "multiple definition" / LNK2005 ile bağlanmaz. Yani asıl kontrol bağlama
// adımıdır; aşağıdaki test iki kopyanın aynı dosyayı aynı çözdüğüne bakar.
//
// sdl_painter_tests yalnızca statik derlemede kurulur (bkz. CMakeLists.txt),
// sorunun görüldüğü yer de orasıdır.

#include "sdl_painter/image.h"

#include <cstddef>
#include <cstring>
#include <filesystem>
#include <gtest/gtest.h>
#include <memory>

// clang-format off
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
// clang-format on

TEST(StbCoexistence, ConsumerAndLibraryDecodeSameFile) {
  const auto path = std::filesystem::path(__FILE__).parent_path() / "fixtures" /
                    "rgba_32x24.png";

  int width = 0;
  int height = 0;
  int channels = 0;
  const std::unique_ptr<stbi_uc, decltype(&stbi_image_free)> own(
      stbi_load(path.string().c_str(), &width, &height, &channels, 0),
      &stbi_image_free);
  ASSERT_NE(own, nullptr) << stbi_failure_reason();

  const sdl_painter::Image image(path.string());
  ASSERT_TRUE(image.IsValid());
  EXPECT_EQ(image.Width(), width);
  EXPECT_EQ(image.Height(), height);
  EXPECT_EQ(image.Channels(), channels);

  const auto size = static_cast<std::size_t>(width) *
                    static_cast<std::size_t>(height) *
                    static_cast<std::size_t>(channels);
  EXPECT_EQ(std::memcmp(own.get(), image.RawData(), size), 0);
}
