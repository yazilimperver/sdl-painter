#pragma once

#include "sdl_painter/export.h"
#include "sdl_painter/renderer.h"
#include "sdl_painter/texture.h"

#include <cstdint>
#include <memory>
#include <string>

namespace sdl_painter {

class IRenderer;

/// @brief Görüntü çizilirken uygulanacak aynalama.
///
/// Aynalama doku koordinatları takas edilerek yapılır; ek vertex, ek draw call
/// veya negatif ölçek içeren bir transform gerektirmez. Bu yüzden
/// @ref Painter::DrawImage çağrısını `Save`/`Scale(-1,1)`/`Restore` üçlüsüyle
/// sarmalamaya göre hem daha ucuz hem de hedef dikdörtgeni yerinde bırakır.
enum class ImageFlip : uint8_t {
  kNone,        ///< Aynalama yok.
  kHorizontal,  ///< Yatay (sol ↔ sağ).
  kVertical,    ///< Dikey (üst ↔ alt).
  kBoth,        ///< Her iki eksende — 180° döndürmeye denktir.
};

/// @brief Yüklenmiş görüntü — texture sarmalayıcı.
///
/// stb_image üzerinden dosya yükler. Texture, renderer üzerinden oluşturulur.
///
/// @warning  Bir Image, ona ait texture'ı yükleyen
/// Painter (ve dolayısıyla IRenderer) yaşıyorken yıkılmalıdır. Painter yok
/// olduktan sonra Image yıkılırsa, sahip pointer (raw IRenderer*) dangling
/// olur ve `~Image()` davranışı tanımsızdır. Pratikte: Image'ı Painter
/// scope'una göre dar tutun, asla `static` veya `Painter`'dan daha uzun
/// yaşayan bir konuma koymayın. v0.2.0'da `weak_ptr<IRenderer>` veya
/// `Painter::EvictImage` ile bu sözleşme zorunlu kılınacaktır.
class SDLPAINTER_API Image {
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
  [[nodiscard]] bool IsValid() const noexcept { return mRawData != nullptr; }

  /// @brief Görüntü genişliği (piksel).
  [[nodiscard]] int32_t Width() const noexcept { return mWidth; }

  /// @brief Görüntü yüksekliği (piksel).
  [[nodiscard]] int32_t Height() const noexcept { return mHeight; }

  /// @brief Kanal sayısı (3 = RGB, 4 = RGBA).
  [[nodiscard]] int32_t Channels() const noexcept { return mChannels; }

  /// @brief Ham piksel verisi (stb_image tarafından yüklendi).
  [[nodiscard]] const uint8_t* RawData() const noexcept {
    return mRawData.get();
  }

  /// @brief Ham piksel verisinden görüntü oluştur (veriler kopyalanır).
  ///
  /// Prosedürel dokular ve bellekten yükleme için kullanılır.
  [[nodiscard]] static Image CreateFromData(const uint8_t* data, int32_t width,
                                            int32_t height, int32_t channels);

  /// @brief Örnekleme filtresini ayarla.
  ///
  /// Piksel sanatı `kNearest` ister; varsayılan `kLinear` ile pikseller
  /// büyütüldüğünde bulanıklaşır.
  ///
  /// @warning Filtre doku oluşturulurken uygulanır, yani ilk çizimden
  ///          (ilk @ref Upload) önce ayarlanmalıdır. Sonradan çağırmak
  ///          önbelleklenmiş dokuyu etkilemez.
  void SetFilter(TextureFilter filter) noexcept { mFilter = filter; }

  /// @brief Yürürlükteki örnekleme filtresi.
  [[nodiscard]] TextureFilter GetFilter() const noexcept { return mFilter; }

  /// @brief Texture'ı renderer'a yükle; ikinci çağrıda önbelleği döner.
  TextureHandle Upload(IRenderer& renderer) const;

  /// @brief Önceden yüklenmiş texture tanımlayıcısını döner (kInvalidTexture → henüz yüklenmedi).
  [[nodiscard]] TextureHandle GetHandle() const noexcept {
    return mHandle.Handle();
  }

 private:
  struct StbDeleter {
    void operator()(uint8_t* ptr) const;
  };

  std::unique_ptr<uint8_t, StbDeleter> mRawData;
  int32_t mWidth{0};
  int32_t mHeight{0};
  int32_t mChannels{0};
  TextureFilter mFilter{TextureFilter::kLinear};
  mutable Texture
      mHandle;  ///< Önbelleklenmiş RAII texture; sahip renderer mHandle.Owner()'da
};

}  // namespace sdl_painter
