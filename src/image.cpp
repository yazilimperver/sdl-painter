#include "sdl_painter/image.h"

#include "sdl_painter/renderer.h"

#include <cstdlib>
#include <cstring>
#include <spdlog/spdlog.h>

// stb_image tek-başlıklı bir kütüphanedir: implementasyon yalnızca
// STB_IMAGE_IMPLEMENTATION tanımlı olan çeviri biriminde üretilir. Tanım ile
// include ARALARINA başka bir şey girmemelidir; clang-format/IWYU geçişi
// include'ları yeniden sıralarsa derleme sessizce link hatasına döner.
// Bu yüzden ikisi bitişik tutulur ve blok formatlamaya kapatılır.
//
// STBI_MAX_DIMENSIONS: güvenilmeyen bir PNG/JPG'nin devasa boyut bildirerek
// belleği tüketmesini (decompression bomb) önler. stb bunu kendi içinde,
// piksel verisi ayrılmadan ÖNCE kontrol eder.
// clang-format off
#define STBI_MAX_DIMENSIONS 32768
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
// clang-format on

namespace sdl_painter {

namespace {
/// @brief Dosyadan yüklenen görüntü için azami toplam piksel sayısı.
///
/// STBI_MAX_DIMENSIONS tek kenarı sınırlar; 32768x32768 hâlâ ~4 GB eder.
/// Toplam piksel sınırı asıl bellek korumasıdır (256 M piksel ≈ 1 GB RGBA).
constexpr std::size_t kMaxTotalPixels = 256U * 1024U * 1024U;
}  // namespace

void Image::StbDeleter::operator()(uint8_t* ptr) const {
  // stbi_image_free, stbi_load tarafindan malloc ile tahsis edilmis bellegi
  // serbest birakir. CreateFromData da malloc kullandigi icin ayni deleter
  // her iki kaynak tipi icin de gecerlidir.
  stbi_image_free(ptr);
}

Image::Image(const std::string& file_path) {
  // Piksel verisini ayırmadan ÖNCE başlığı okuyup boyutu doğrula: güvenilmeyen
  // bir dosya devasa boyut bildirip belleği tüketebilir (decompression bomb).
  int probe_w = 0;
  int probe_h = 0;
  int probe_ch = 0;
  if (stbi_info(file_path.c_str(), &probe_w, &probe_h, &probe_ch) == 0) {
    spdlog::error("Image: '{}' okunamadı ({}).", file_path,
                  stbi_failure_reason() != nullptr ? stbi_failure_reason()
                                                   : "bilinmeyen hata");
    return;
  }
  if (probe_w <= 0 || probe_h <= 0) {
    spdlog::error("Image: '{}' geçersiz boyut ({}x{}).", file_path, probe_w,
                  probe_h);
    return;
  }
  const std::size_t kTotalPixels =
      static_cast<std::size_t>(probe_w) * static_cast<std::size_t>(probe_h);
  if (kTotalPixels > kMaxTotalPixels) {
    spdlog::error(
        "Image: '{}' çok büyük ({}x{} = {} piksel, sınır {}). Yüklenmedi.",
        file_path, probe_w, probe_h, kTotalPixels, kMaxTotalPixels);
    return;
  }

  int32_t w = 0;
  int32_t h = 0;
  int32_t ch = 0;
  uint8_t* data = stbi_load(file_path.c_str(), &w, &h, &ch, 0);
  if (data != nullptr) {
    mRawData.reset(data);
    mWidth = w;
    mHeight = h;
    mChannels = ch;
  } else {
    spdlog::error("Image: '{}' yüklenemedi ({}).", file_path,
                  stbi_failure_reason() != nullptr ? stbi_failure_reason()
                                                   : "bilinmeyen hata");
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
  // NOLINTNEXTLINE(cppcoreguidelines-no-malloc, cppcoreguidelines-owning-memory)
  auto* buf = static_cast<uint8_t*>(std::malloc(kSize));
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
