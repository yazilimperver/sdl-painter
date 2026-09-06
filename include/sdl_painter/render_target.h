#pragma once

#include "sdl_painter/export.h"
#include "sdl_painter/renderer.h"

#include <cstdint>

namespace sdl_painter {

class IRenderer;
class Painter;

/// @brief Ekran yerine çizilebilen offscreen bir yüzey (dokuya çizim).
///
/// QPainter'da `QImage`'e çizmenin karşılığı. Tipik akış: sahneyi hedefe çiz,
/// sonra hedefi tek bir görüntü gibi ekrana bas. Mini harita, son işlem
/// efektleri (bulanıklık, renk süzgeci), iz/hayalet efekti ve pahalı ama nadir
/// değişen katmanların önbelleklenmesi bu yolla yapılır.
///
/// Nesne @ref Painter::CreateRenderTarget ile üretilir; doğrudan kurulamaz.
///
/// @code
/// auto target = painter.CreateRenderTarget(256, 256);
///
/// painter.SetRenderTarget(target);      // artık çizimler hedefe gider
/// painter.Clear(sdl_painter::Color::Transparent());
/// painter.FillCircle(128.0F, 128.0F, 100.0F);
/// painter.ResetRenderTarget();          // ekrana dön
///
/// painter.DrawRenderTarget(target, sdl_painter::Rect{10, 10, 128, 128});
/// @endcode
///
/// @par Koordinatlar
/// Hedef bağlıyken çizim koordinatları hedefe yereldir: `(0, 0)` hedefin
/// sol üst köşesidir ve projeksiyon hedefin boyutuna göre kurulur. İki
/// backend'de de aynı: OpenGL'in aşağıdan yukarı framebuffer'ı ile Vulkan'ın
/// yukarıdan aşağı olanı arasındaki fark kütüphane içinde kapatılır, sonuç
/// piksel piksel aynıdır.
///
/// @par Piksel formatı
/// Hedefler daima doğrusal RGBA8 tutar — ekran yüzeyinin formatı ne olursa
/// olsun. @ref Painter::ReadRenderTarget bu yüzden her platformda aynı baytları
/// döndürür.
///
/// @warning  Bir RenderTarget, onu üreten
/// @ref Painter yaşıyorken yıkılmalıdır — @ref Image ve @ref Font ile aynı
/// kural. Painter yok olduktan sonra yıkılırsa sahip pointer dangling olur.
///
/// @warning Bir hedefe çizerken o hedefin kendi içeriğini örneklemek
/// tanımsızdır. Önce @ref Painter::ResetRenderTarget çağırın.
class SDLPAINTER_API RenderTarget {
 public:
  /// @brief Geçersiz (boş) hedef.
  RenderTarget() = default;

  ~RenderTarget();

  // Non-copyable, movable — GPU kaynağı sahipliği tekil.
  RenderTarget(const RenderTarget&) = delete;
  RenderTarget& operator=(const RenderTarget&) = delete;
  RenderTarget(RenderTarget&& other) noexcept;
  RenderTarget& operator=(RenderTarget&& other) noexcept;

  /// @brief Hedef başarıyla oluşturuldu mu?
  [[nodiscard]] bool IsValid() const noexcept {
    return mHandle != kInvalidRenderTarget;
  }

  /// @brief Genişlik (piksel).
  [[nodiscard]] int32_t Width() const noexcept { return mWidth; }

  /// @brief Yükseklik (piksel).
  [[nodiscard]] int32_t Height() const noexcept { return mHeight; }

  /// @brief Backend tanımlayıcısı — ileri düzey kullanım için.
  [[nodiscard]] RenderTargetHandle Handle() const noexcept { return mHandle; }

  /// @brief Hedefi kaynaklarıyla birlikte serbest bırak.
  ///
  /// Yıkıcı da bunu çağırır; erken serbest bırakmak isteyen kod için ayrıca
  /// açıktır. İkinci çağrı etkisizdir.
  void Reset() noexcept;

 private:
  friend class Painter;

  /// @brief Painter tarafından çağrılır; kullanıcıya kapalı.
  RenderTarget(IRenderer* owner, RenderTargetHandle handle, int32_t width,
               int32_t height) noexcept
      : mOwner(owner), mHandle(handle), mWidth(width), mHeight(height) {}

  IRenderer* mOwner{nullptr};
  RenderTargetHandle mHandle{kInvalidRenderTarget};
  int32_t mWidth{0};
  int32_t mHeight{0};
};

}  // namespace sdl_painter
