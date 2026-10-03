/// @file test_pixel_support.cpp
///
/// @brief pixel_test_support.h yardımcılarının (ColorNear, PixelAt) kendi
///        testleri. Gerçek backend'de piksel doğrulayan testlerin tüm iddiaları
///        bu iki yardımcıya dayanır; yardımcı yanlışsa onlar da yanlış sonuç
///        verir. Pencere veya backend kurulmaz.

#include "sdl_painter/color.h"

#include <cstddef>
#include <cstdint>
#include <gtest/gtest-spi.h>
#include <gtest/gtest.h>
#include <ostream>
#include <string>
#include <vector>

#include "pixel_test_support.h"

using sdl_painter::Color;
using sdl_painter::testing::ColorNear;
using sdl_painter::testing::PixelAt;

namespace {

constexpr int32_t kTolerance = 5;
constexpr int32_t kBufferWidth = 3;
constexpr int32_t kBufferHeight = 2;

/// @brief Her pikseli birbirinden ve kanalları birbirinden farklı olan tampon.
///
/// Satır öncelikli, yukarıdan aşağı: k. piksel (k = y * width + x)
/// `(10k, 10k + 1, 10k + 2, 10k + 3)` olur.
std::vector<uint8_t> MakeBuffer(int32_t width, int32_t height) {
  std::vector<uint8_t> rgba;
  const int32_t pixels = width * height;
  for (int32_t k = 0; k < pixels; ++k) {
    for (int32_t channel = 0; channel < 4; ++channel) {
      rgba.push_back(static_cast<uint8_t>((10 * k) + channel));
    }
  }
  return rgba;
}

Color ExpectedPixel(int32_t k) {
  return Color{static_cast<uint8_t>(10 * k), static_cast<uint8_t>((10 * k) + 1),
               static_cast<uint8_t>((10 * k) + 2),
               static_cast<uint8_t>((10 * k) + 3)};
}

/// @brief `base`'in bir kanalını (0 = r, 1 = g, 2 = b, 3 = a) `value` yapar.
Color WithChannel(const Color& base, int32_t channel, int32_t value) {
  Color c = base;
  const auto v = static_cast<uint8_t>(value);
  switch (channel) {
    case 0:
      c.r = v;
      break;
    case 1:
      c.g = v;
      break;
    case 2:
      c.b = v;
      break;
    default:
      c.a = v;
      break;
  }
  return c;
}

// --- ColorNear ---

/// @brief Bir kanal `base` değerinden `delta` kadar farklı; kabul edilmeli mi?
struct ToleranceCase {
  std::string name;
  int32_t channel;
  int32_t delta;
  bool expect_near;
};

/// @brief Her kanal için toleransta ve tolerans + 1'de, iki yönde.
std::vector<ToleranceCase> ToleranceCases() {
  const char* const kChannelNames[] = {"Red", "Green", "Blue", "Alpha"};
  std::vector<ToleranceCase> cases;
  for (int32_t channel = 0; channel < 4; ++channel) {
    const std::string name = kChannelNames[channel];
    cases.push_back({name + "PlusToleranceIsNear", channel, kTolerance, true});
    cases.push_back(
        {name + "MinusToleranceIsNear", channel, -kTolerance, true});
    cases.push_back(
        {name + "PlusToleranceAndOneIsFar", channel, kTolerance + 1, false});
    cases.push_back(
        {name + "MinusToleranceAndOneIsFar", channel, -kTolerance - 1, false});
  }
  return cases;
}

/// @brief Başarısız örneğin ham bayt dökümü yerine adı yazılsın.
void PrintTo(const ToleranceCase& param, std::ostream* os) {
  *os << param.name;
}

std::string ToleranceCaseName(
    const ::testing::TestParamInfo<ToleranceCase>& info) {
  return info.param.name;
}

class ColorNearToleranceTest : public ::testing::TestWithParam<ToleranceCase> {
};

TEST_P(ColorNearToleranceTest, ShouldAcceptDifferenceUpToToleranceOnly) {
  const ToleranceCase& param = GetParam();
  const Color base(100, 100, 100, 100);
  const Color actual = WithChannel(base, param.channel, 100 + param.delta);

  const ::testing::AssertionResult result = ColorNear(actual, base, kTolerance);

  EXPECT_EQ(static_cast<bool>(result), param.expect_near);
}

INSTANTIATE_TEST_SUITE_P(EveryChannel, ColorNearToleranceTest,
                         ::testing::ValuesIn(ToleranceCases()),
                         ToleranceCaseName);

// --- PixelAt, geçersiz istekler ---

/// @brief Geçersiz bir PixelAt isteği ve raporlanması beklenen ileti parçası.
struct BadRequest {
  const char* name;
  int32_t width;
  int32_t x;
  int32_t y;
  const char* message;
};

void PrintTo(const BadRequest& param, std::ostream* os) {
  *os << param.name;
}

std::string BadRequestName(const ::testing::TestParamInfo<BadRequest>& info) {
  return info.param.name;
}

class PixelAtBadRequestTest : public ::testing::TestWithParam<BadRequest> {};

TEST_P(PixelAtBadRequestTest, ShouldReportFailureAndReturnDefaultColor) {
  const BadRequest& param = GetParam();
  const std::vector<uint8_t> rgba = MakeBuffer(kBufferWidth, kBufferHeight);
  Color actual(1, 2, 3, 4);

  EXPECT_NONFATAL_FAILURE(actual = PixelAt(rgba, param.width, param.x, param.y),
                          param.message);

  EXPECT_EQ(actual, Color{});
}

// `x == width` satırı: indeks tampon içinde kalır (3. satırın değil, bir
// sonraki satırın ilk pikseli), bu yüzden yalnızca toplam indeks sınırı
// yetmez. `y == height` ise indeksin tampon dışına düştüğü durumdur.
INSTANTIATE_TEST_SUITE_P(
    OutOfRange, PixelAtBadRequestTest,
    ::testing::Values(
        BadRequest{"NegativeX", kBufferWidth, -1, 0, "gecersiz istek"},
        BadRequest{"NegativeY", kBufferWidth, 0, -1, "gecersiz istek"},
        BadRequest{"XEqualToWidthStaysInsideTheBuffer", kBufferWidth,
                   kBufferWidth, 0, "gecersiz istek"},
        BadRequest{"XBeyondWidth", kBufferWidth, kBufferWidth + 4, 0,
                   "gecersiz istek"},
        BadRequest{"ZeroWidth", 0, 0, 0, "gecersiz istek"},
        BadRequest{"NegativeWidth", -kBufferWidth, 0, 0, "gecersiz istek"},
        BadRequest{"YEqualToHeight", kBufferWidth, 0, kBufferHeight,
                   "tamponun disinda"},
        BadRequest{"YFarBeyondHeight", kBufferWidth, 0, 1000,
                   "tamponun disinda"}),
    BadRequestName);

}  // namespace

// --- ColorNear ---

TEST(ColorNearTest, ShouldRequireExactMatchWhenToleranceIsZero) {
  const Color base(10, 20, 30, 40);

  EXPECT_TRUE(ColorNear(base, base, 0));
  EXPECT_FALSE(ColorNear(Color(11, 20, 30, 40), base, 0));
}

TEST(ColorNearTest, ShouldHandleFullChannelRangeWithoutWraparound) {
  const Color black_clear(0, 0, 0, 0);
  const Color white_opaque(255, 255, 255, 255);

  EXPECT_FALSE(ColorNear(black_clear, white_opaque, 254));
  EXPECT_TRUE(ColorNear(black_clear, white_opaque, 255));
  EXPECT_FALSE(ColorNear(white_opaque, black_clear, 254));
  EXPECT_TRUE(ColorNear(white_opaque, black_clear, 255));
}

TEST(ColorNearTest, ShouldNameBothColorsInTheFailureMessage) {
  const ::testing::AssertionResult result =
      ColorNear(Color(11, 22, 33, 44), Color(1, 2, 3, 4), 0);

  ASSERT_FALSE(result);
  EXPECT_EQ(std::string(result.message()),
            "beklenen (1,2,3,4) - gelen (11,22,33,44)");
}

// --- PixelAt ---

TEST(PixelAtTest, ShouldIndexRowMajorFromTheTop) {
  const std::vector<uint8_t> rgba = MakeBuffer(kBufferWidth, kBufferHeight);

  EXPECT_EQ(PixelAt(rgba, kBufferWidth, 0, 0), ExpectedPixel(0));
  EXPECT_EQ(PixelAt(rgba, kBufferWidth, 1, 0), ExpectedPixel(1));
  EXPECT_EQ(PixelAt(rgba, kBufferWidth, 2, 0), ExpectedPixel(2));
  EXPECT_EQ(PixelAt(rgba, kBufferWidth, 0, 1), ExpectedPixel(3));
  EXPECT_EQ(PixelAt(rgba, kBufferWidth, 1, 1), ExpectedPixel(4));
}

TEST(PixelAtTest, ShouldReadTheLastPixelOfTheBuffer) {
  const std::vector<uint8_t> rgba = MakeBuffer(kBufferWidth, kBufferHeight);

  EXPECT_EQ(PixelAt(rgba, kBufferWidth, kBufferWidth - 1, kBufferHeight - 1),
            ExpectedPixel(5));
}

TEST(PixelAtTest, ShouldKeepChannelOrderRgba) {
  const std::vector<uint8_t> rgba = {1, 2, 3, 4};

  const Color pixel = PixelAt(rgba, 1, 0, 0);

  EXPECT_EQ(pixel.r, 1);
  EXPECT_EQ(pixel.g, 2);
  EXPECT_EQ(pixel.b, 3);
  EXPECT_EQ(pixel.a, 4);
}

/// @brief 24 baytlık tampon 4 piksel genişliğe bölünmez (satır 16 bayt). (0, 0)
///        indeksi tampon içinde olsa da genişlik yanlış, istek reddedilmeli.
TEST(PixelAtTest, ShouldRejectBufferThatIsNotWholeRows) {
  const std::vector<uint8_t> rgba = MakeBuffer(kBufferWidth, kBufferHeight);
  Color actual(1, 2, 3, 4);

  EXPECT_NONFATAL_FAILURE(actual = PixelAt(rgba, kBufferWidth + 1, 0, 0),
                          "gecersiz istek");

  EXPECT_EQ(actual, Color{});
}

TEST(PixelAtTest, ShouldRejectEmptyBuffer) {
  const std::vector<uint8_t> rgba;
  Color actual(1, 2, 3, 4);

  EXPECT_NONFATAL_FAILURE(actual = PixelAt(rgba, kBufferWidth, 0, 0),
                          "tamponun disinda");

  EXPECT_EQ(actual, Color{});
}
