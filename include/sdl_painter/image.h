#pragma once

#include <cstdint>
#include <memory>
#include <string>

#include "sdl_painter/renderer.h"
#include "sdl_painter/texture.h"

namespace sdl_painter {

class IRenderer;

/// @brief Yüklenmiş görüntü — texture sarmalayıcı.
///
/// stb_image üzerinden dosya yükler. Texture, renderer üzerinden oluşturulur.
class Image {
 public:
  Image() = default;

  /// @brief Dosyadan görüntü yükle. Başarısız olursa IsValid() false döner.
  explicit Image(const std::string& file_path);

  ~Image();

  // Non-copyable, movable
  Image(const Image&) = delete;
  Image& operator=(const Image&) = delete;
  Image(Image&&) noexcept;
  Image& operator=(Image&&) noexcept;

  /// @brief Görüntü başarıyla yüklendi mi?
  bool IsValid() const { return mRawData != nullptr; }

  /// @brief Görüntü genişliği (piksel).
  int32_t Width() const { return mWidth; }

  /// @brief Görüntü yüksekliği (piksel).
  int32_t Height() const { return mHeight; }

  /// @brief Kanal sayısı (3 = RGB, 4 = RGBA).
  int32_t Channels() const { return mChannels; }

  /// @brief Ham piksel verisi (stb_image tarafından yüklendi).
  const uint8_t* RawData() const { return mRawData.get(); }

  /// @brief Ham piksel verisinden görüntü oluştur (veriler kopyalanır).
  ///
  /// Prosedürel dokular ve bellekten yükleme için kullanılır.
  static Image CreateFromData(const uint8_t* data,
                              int32_t width, int32_t height, int32_t channels);

  /// @brief Texture'ı renderer'a yükle; ikinci çağrıda önbelleği döner.
  TextureHandle Upload(IRenderer& renderer) const;

  /// @brief Önceden yüklenmiş texture tanımlayıcısını döner (kInvalidTexture → henüz yüklenmedi).
  TextureHandle GetHandle() const { return mHandle.Handle(); }

 private:
  struct StbDeleter {
    void operator()(uint8_t* ptr) const;
  };

  std::unique_ptr<uint8_t, StbDeleter> mRawData;
  int32_t mWidth{0};
  int32_t mHeight{0};
  int32_t mChannels{0};
  mutable Texture mHandle;  ///< Önbelleklenmiş RAII texture; sahip renderer mHandle.Owner()'da
};

}  // namespace sdl_painter
