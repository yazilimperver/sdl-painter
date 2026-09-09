#pragma once

#include "sdl_painter/export.h"
#include "sdl_painter/renderer.h"

#include <cstdint>
#include <memory>

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
/// @par Başlangıç içeriği
/// Yeni bir hedef `(0, 0, 0, 0)` olarak ilklenir ve bu iki backend'de de
/// aynıdır. Ekranın veya başka bir hedefin temizleme rengi buraya sızmaz;
/// hedefe geçmeden önce @ref Painter::Clear çağırmak zorunlu değildir.
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
  ///
  /// @param painter Üreten Painter'ın "kendini gösteren kutusu". Painter
  ///        taşınabilir olduğu için doğrudan `Painter*` tutmak yetmez: kutu
  ///        taşımada güncellenir, Painter yıkılınca `nullptr` olur.
  RenderTarget(IRenderer* owner, RenderTargetHandle handle, int32_t width,
               int32_t height, std::shared_ptr<Painter*> painter) noexcept
      : mOwner(owner),
        mHandle(handle),
        mWidth(width),
        mHeight(height),
        mPainter(std::move(painter)) {}

  IRenderer* mOwner{nullptr};
  RenderTargetHandle mHandle{kInvalidRenderTarget};
  int32_t mWidth{0};
  int32_t mHeight{0};

  /// @brief Üreten Painter'a dolaylı işaretçi (bkz. private ctor).
  ///
  /// @ref Reset bunu iki şey için kullanır: hedef o an bağlıysa Painter'ın
  /// ekrana dönmesini sağlamak (aksi halde Painter ölü bir hedefe çizdiğini
  /// sanmayı sürdürürdü) ve Painter zaten yıkılmışsa `mOwner` üzerinden
  /// yıkım çağrısı yapmamak.
  std::shared_ptr<Painter*> mPainter;
};

}  // namespace sdl_painter
