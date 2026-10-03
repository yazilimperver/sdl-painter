#pragma once

/// @file pixel_test_support.h
/// @brief Gerçek backend üzerinde piksel doğrulayan testler için ortak
///        yardımcılar.

#include "sdl_painter/color.h"
#include "sdl_painter/image.h"
#include "sdl_painter/painter.h"
#include "sdl_painter/renderer.h"

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <gtest/gtest.h>
#include <memory>
#include <string>
#include <vector>

#include "test_support.h"

namespace sdl_painter::testing {

/// @brief Bu derlemede sınanabilecek backend'ler.
///
/// Vulkan yalnızca `SDLPAINTER_WITH_VULKAN` ile derlendiyse listeye girer;
/// önişlemci koşulu makro argümanı içinde yazılamadığı için burada toplanır.
inline std::vector<RendererBackend> AvailableBackends() {
  std::vector<RendererBackend> backends{RendererBackend::kOpenGL};
#ifdef SDLPAINTER_HAS_VULKAN
  backends.push_back(RendererBackend::kVulkan);
#endif
  return backends;
}

/// @brief Dosya gerektirmeyen prosedürel doku (RGBA dama tahtası).
inline Image MakeCheckerImage(int32_t size = 16) {
  std::vector<uint8_t> pixels(static_cast<std::size_t>(size) * size * 4);
  for (int32_t y = 0; y < size; ++y) {
    for (int32_t x = 0; x < size; ++x) {
      const bool on = ((x / 4) + (y / 4)) % 2 == 0;
      auto* p = pixels.data() + ((static_cast<std::size_t>(y) * size) + x) * 4;
      p[0] = on ? 220 : 40;
      p[1] = on ? 90 : 40;
      p[2] = 60;
      p[3] = 255;
    }
  }
  return Image::CreateFromData(pixels.data(), size, size, 4);
}

/// @brief Geri okunan tamponda bir pikselin rengi.
///
/// Satır öncelikli, yukarıdan aşağı, piksel başına 4 bayt (RGBA). Koordinat
/// veya tampon uygun değilse `ADD_FAILURE()` raporlar ve varsayılan renk
/// döner: `x == width` tampon içinde kalıp sessizce bir sonraki satırın ilk
/// pikselini okurdu, bu yüzden `x` ayrıca sınırlanır.
inline Color PixelAt(const std::vector<uint8_t>& rgba, int32_t width, int32_t x,
                     int32_t y) {
  const bool kShapeOk =
      width > 0 && x >= 0 && x < width && y >= 0 &&
      rgba.size() % (static_cast<std::size_t>(width) * 4U) == 0U;
  if (!kShapeOk) {
    ADD_FAILURE() << "PixelAt: gecersiz istek (genislik " << width << ", x "
                  << x << ", y " << y << ", tampon " << rgba.size()
                  << " bayt).";
    return Color{};
  }
  const std::size_t i =
      ((static_cast<std::size_t>(y) * static_cast<std::size_t>(width)) +
       static_cast<std::size_t>(x)) *
      4U;
  if (i + 4U > rgba.size()) {
    ADD_FAILURE() << "PixelAt: piksel tamponun disinda (genislik " << width
                  << ", x " << x << ", y " << y << ", tampon " << rgba.size()
                  << " bayt).";
    return Color{};
  }
  return Color{rgba[i], rgba[i + 1], rgba[i + 2], rgba[i + 3]};
}

/// @brief İki renk kanal başına verilen toleransta eşit mi?
inline ::testing::AssertionResult ColorNear(const Color& actual,
                                            const Color& expected,
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
         << static_cast<int32_t>(expected.a) << ") - gelen ("
         << static_cast<int32_t>(actual.r) << ","
         << static_cast<int32_t>(actual.g) << ","
         << static_cast<int32_t>(actual.b) << ","
         << static_cast<int32_t>(actual.a) << ")";
}

/// @brief Geri okunan tamponu `<ad>.bmp` olarak diske yazar.
///
/// `SDLPAINTER_PIXEL_DUMP_DIR` ayarlı değilse hiçbir şey yapmaz. Bir piksel
/// testi düştüğünde sayı yerine görüntüye bakabilmek için. BMP seçildi çünkü
/// ek bağımlılık gerektirmiyor ve dosya doğrudan açılabiliyor.
inline void DumpRgba(const std::string& name, const std::vector<uint8_t>& rgba,
                     int32_t width, int32_t height) {
  const char* dir = std::getenv("SDLPAINTER_PIXEL_DUMP_DIR");
  if (dir == nullptr || width <= 0 || height <= 0) {
    return;
  }
  const std::size_t kPixels =
      static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
  if (rgba.size() < kPixels * 4U) {
    return;
  }

  constexpr uint32_t kHeaderSize = 54U;
  const auto kDataSize = static_cast<uint32_t>(kPixels * 4U);

  std::vector<uint8_t> out;
  out.reserve(kHeaderSize + kDataSize);
  const auto put16 = [&out](uint16_t v) {
    out.push_back(static_cast<uint8_t>(v & 0xFFU));
    out.push_back(static_cast<uint8_t>((v >> 8U) & 0xFFU));
  };
  const auto put32 = [&out](uint32_t v) {
    out.push_back(static_cast<uint8_t>(v & 0xFFU));
    out.push_back(static_cast<uint8_t>((v >> 8U) & 0xFFU));
    out.push_back(static_cast<uint8_t>((v >> 16U) & 0xFFU));
    out.push_back(static_cast<uint8_t>((v >> 24U) & 0xFFU));
  };

  out.push_back(static_cast<uint8_t>('B'));
  out.push_back(static_cast<uint8_t>('M'));
  put32(kHeaderSize + kDataSize);
  put32(0U);
  put32(kHeaderSize);

  put32(40U);
  put32(static_cast<uint32_t>(width));
  put32(static_cast<uint32_t>(height));
  put16(1U);
  put16(32U);
  put32(0U);
  put32(kDataSize);
  put32(2835U);
  put32(2835U);
  put32(0U);
  put32(0U);

  // BMP satirlari asagidan yukari sirali; geri okunan tampon yukaridan asagi.
  for (int32_t y = height - 1; y >= 0; --y) {
    const std::size_t row =
        static_cast<std::size_t>(y) * static_cast<std::size_t>(width) * 4U;
    for (int32_t x = 0; x < width; ++x) {
      const std::size_t i = row + (static_cast<std::size_t>(x) * 4U);
      out.push_back(rgba[i + 2U]);
      out.push_back(rgba[i + 1U]);
      out.push_back(rgba[i]);
      out.push_back(rgba[i + 3U]);
    }
  }

  const std::string path = std::string(dir) + "/" + name + ".bmp";
  std::ofstream file(path, std::ios::binary);
  if (!file) {
    std::fprintf(stderr, "[ UYARI ] DumpRgba: dosya acilamadi: %s\n",
                 path.c_str());
    return;
  }
  file.write(reinterpret_cast<const char*>(out.data()),
             static_cast<std::streamsize>(out.size()));
}

/// @brief Gerçek bir backend + Painter ayağa kaldırır; olmazsa testi atlar.
struct Backend {
  HiddenWindow window;
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

}  // namespace sdl_painter::testing

/// @brief Backend'i ayaga kaldirir, yoksa testi atlar; ayrica test boyunca
///        yeni bir Vulkan validation hatasi cikmadigini garanti eder.
///
/// Backend yoksa `SDLPAINTER_REQUIRE_BACKEND` ayarliysa test duser, degilse
/// atlanir (bkz. `SDLPAINTER_BACKEND_UNAVAILABLE`).
#define REQUIRE_BACKEND(var, backend)                      \
  const sdl_painter::testing::ValidationGuard var##_guard; \
  sdl_painter::testing::Backend var(backend);              \
  if (!(var).Ready()) {                                    \
    SDLPAINTER_BACKEND_UNAVAILABLE((var).skip_reason);     \
  }                                                        \
  static_assert(true, "")
