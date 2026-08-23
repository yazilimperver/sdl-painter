#include "screenshot.h"

#include <spdlog/spdlog.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

namespace bench {

namespace {

// GL sabitleri — glad'a bağlanmamak için elle tanımlı (glad, sdl_painter'a
// PRIVATE bağlıdır ve örneklerin include yolunda değildir).
constexpr uint32_t kGlRgba = 0x1908;
constexpr uint32_t kGlUnsignedByte = 0x1401;
constexpr uint32_t kGlPackAlignment = 0x0D05;
constexpr uint32_t kGlFront = 0x0404;
constexpr uint32_t kGlBack = 0x0405;

#ifdef _WIN32
#define BENCH_GLAPI __stdcall
#else
#define BENCH_GLAPI
#endif

using GlReadPixelsFn = void(BENCH_GLAPI*)(int32_t, int32_t, int32_t, int32_t,
                                          uint32_t, uint32_t, void*);
using GlPixelStoreiFn = void(BENCH_GLAPI*)(uint32_t, int32_t);
using GlFinishFn = void(BENCH_GLAPI*)();
using GlReadBufferFn = void(BENCH_GLAPI*)(uint32_t);

}  // namespace

bool SaveBackBufferPng(const std::string& path, int32_t width, int32_t height,
                       bool front_buffer) {
  if (width <= 0 || height <= 0) {
    return false;
  }

  auto read_pixels =
      reinterpret_cast<GlReadPixelsFn>(SDL_GL_GetProcAddress("glReadPixels"));
  auto pixel_storei =
      reinterpret_cast<GlPixelStoreiFn>(SDL_GL_GetProcAddress("glPixelStorei"));
  auto finish = reinterpret_cast<GlFinishFn>(SDL_GL_GetProcAddress("glFinish"));
  if (read_pixels == nullptr || pixel_storei == nullptr || finish == nullptr) {
    spdlog::error("[screenshot] GL fonksiyonlari cozulemedi: {}",
                  SDL_GetError());
    return false;
  }

  finish();
  pixel_storei(kGlPackAlignment, 1);
  auto read_buffer =
      reinterpret_cast<GlReadBufferFn>(SDL_GL_GetProcAddress("glReadBuffer"));
  if (read_buffer != nullptr) {
    read_buffer(front_buffer ? kGlFront : kGlBack);
  }

  const std::size_t row_bytes = static_cast<std::size_t>(width) * 4U;
  std::vector<uint8_t> pixels(row_bytes * static_cast<std::size_t>(height));
  read_pixels(0, 0, width, height, kGlRgba, kGlUnsignedByte, pixels.data());

  // GL'de (0,0) sol ALT kösedir; PNG satirlari yukaridan asagi gider.
  std::vector<uint8_t> flipped(pixels.size());
  for (int32_t y = 0; y < height; ++y) {
    const uint8_t* src = pixels.data() + (static_cast<std::size_t>(height - 1 - y) * row_bytes);
    uint8_t* dst = flipped.data() + (static_cast<std::size_t>(y) * row_bytes);
    for (std::size_t i = 0; i < row_bytes; ++i) {
      dst[i] = src[i];
    }
  }

  if (read_buffer != nullptr) {
    read_buffer(kGlBack);
  }

  const int written =
      stbi_write_png(path.c_str(), width, height, 4, flipped.data(),
                     static_cast<int>(row_bytes));
  if (written == 0) {
    spdlog::error("[screenshot] PNG yazilamadi: {}", path);
    return false;
  }
  return true;
}

}  // namespace bench
