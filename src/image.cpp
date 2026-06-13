#include "sdl_painter/image.h"

#define STB_IMAGE_IMPLEMENTATION
#include "sdl_painter/renderer.h"

#include <cstdlib>
#include <cstring>
#include <spdlog/spdlog.h>
#include <stb_image.h>

namespace sdl_painter {

void Image::StbDeleter::operator()(uint8_t* ptr) const {
  // stbi_image_free, stbi_load tarafindan malloc ile tahsis edilmis bellegi
  // serbest birakir. CreateFromData da malloc kullandigi icin ayni deleter
  // her iki kaynak tipi icin de gecerlidir.
  stbi_image_free(ptr);
}

Image::Image(const std::string& file_path) {
  int32_t w = 0;
  int32_t h = 0;
  int32_t ch = 0;
  uint8_t* data = stbi_load(file_path.c_str(), &w, &h, &ch, 0);
  if (data != nullptr) {
    mRawData.reset(data);
    mWidth = w;
    mHeight = h;
    mChannels = ch;
  }
}

Image::~Image() = default;

Image::Image(Image&&) noexcept = default;
Image& Image::operator=(Image&&) noexcept = default;

// static
Image Image::CreateFromData(const uint8_t* data, int32_t width, int32_t height,
                            int32_t channels) {
  Image img;
  if (data == nullptr || width <= 0 || height <= 0 || channels <= 0) {
    return img;
  }

  constexpr int32_t kMaxDimension = 32767;
  if (width > kMaxDimension || height > kMaxDimension) {
    spdlog::warn("Image::CreateFromData: çok büyük boyut ({}x{})", width,
                 height);
    return img;
  }
  const std::size_t kSize = static_cast<std::size_t>(width) *
                            static_cast<std::size_t>(height) *
                            static_cast<std::size_t>(channels);
  // malloc kullaniyoruz — StbDeleter, stbi_image_free -> free() cagirir.
  auto* buf = static_cast<uint8_t*>(std::malloc(kSize));  // NOLINT(cppcoreguidelines-no-malloc, cppcoreguidelines-owning-memory)
  if (buf == nullptr) {
    return img;
  }

  std::memcpy(buf, data, kSize);
  img.mRawData.reset(buf);
  img.mWidth = width;
  img.mHeight = height;
  img.mChannels = channels;
  return img;
}

TextureHandle Image::Upload(IRenderer& renderer) const {
  if (!IsValid()) {
    return kInvalidTexture;
  }

  // Farklı renderer ile çağrıldıysa (örn. renderer yeniden oluşturuldu)
  // eski handle'ı serbest bırak ve yeniden yükle.
  if (mHandle.IsValid() && mHandle.Owner() != &renderer) {
    mHandle.Reset();
  }

  if (!mHandle.IsValid()) {
    mHandle = Texture(&renderer, renderer.CreateTexture(mRawData.get(), mWidth,
                                                        mHeight, mChannels));
  }

  return mHandle.Handle();
}

}  // namespace sdl_painter
