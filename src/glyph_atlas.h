#pragma once

#include "sdl_painter/renderer.h"
#include "sdl_painter/texture.h"

#include <cstdint>
#include <memory>
#include <vector>

namespace sdl_painter {

/// @brief Glyph görüntülerini ortak texture sayfalarında toplayan atlas.
///
/// Amaç: her glyph için ayrı texture oluşturmak, @ref RenderBatcher'ı her
/// karakterde flush'a zorluyor ve `DrawText("Merhaba")` yedi ayrı draw call
/// üretiyordu. Aynı sayfaya paketlenen glyph'ler tek çağrıda çizilir.
///
/// **Paketleme:** basit *shelf* (raf) algoritması. Glyph'ler soldan sağa
/// yerleştirilir; satır dolunca bir alt rafa geçilir. Serbest bırakma yoktur —
/// bir bölge yazıldıktan sonra asla değişmez. Bu değişmezlik önemlidir:
/// @ref IRenderer::UpdateTexture kare ortasında çağrılabildiği için, aynı
/// karede daha önce çizilmiş glyph'lerin piksellerinin bozulmaması gerekir.
///
/// Sayfa dolduğunda yeni bir sayfa açılır; farklı sayfadaki glyph'ler ayrı
/// draw call'a düşer (nadir).
class GlyphAtlas {
 public:
  /// @brief Bir sayfanın kenar uzunluğu (piksel). RGBA8 → 1 MB / sayfa.
  static constexpr int32_t kPageSize = 512;

  /// @brief Glyph'ler arası dolgu — bilinear örneklemede komşu glyph'in
  /// pikselinin sızmasını (bleeding) önler.
  static constexpr int32_t kPadding = 1;

  /// @brief Atlasa yerleştirilmiş bir bölgenin konumu.
  struct Region {
    TextureHandle texture{kInvalidTexture};  ///< Sayfanın texture'ı
    float u0{0.0F};
    float v0{0.0F};
    float u1{0.0F};
    float v1{0.0F};
  };

  GlyphAtlas() = default;
  ~GlyphAtlas() = default;

  GlyphAtlas(const GlyphAtlas&) = delete;
  GlyphAtlas& operator=(const GlyphAtlas&) = delete;
  GlyphAtlas(GlyphAtlas&&) = default;
  GlyphAtlas& operator=(GlyphAtlas&&) = default;

  /// @brief RGBA8 bir görüntüyü atlasa ekle.
  ///
  /// @param renderer Texture'ları oluşturacak/güncelleyecek renderer.
  /// @param pixels Sıkı paketlenmiş RGBA8 veri (`width * height * 4` bayt).
  /// @param width Görüntü genişliği (piksel).
  /// @param height Görüntü yüksekliği (piksel).
  /// @param out_region [out] Başarıda yerleştirilen bölge.
  /// @return Yerleştirme başarılıysa true. Görüntü tek sayfaya sığmıyorsa
  ///         (kenarı @ref kPageSize'ı aşıyorsa) false döner.
  bool Add(IRenderer& renderer, const uint8_t* pixels, int32_t width,
           int32_t height, Region& out_region);

  /// @brief Açılmış sayfa sayısı (test/teşhis).
  [[nodiscard]] std::size_t PageCount() const { return mPages.size(); }

  /// @brief Tüm sayfa texture'larını serbest bırak.
  void Clear() { mPages.clear(); }

 private:
  /// @brief Tek bir atlas sayfası — GPU texture'ı + raf paketleme durumu.
  struct Page {
    Texture texture;
    int32_t cursor_x{kPadding};  ///< Aktif rafta bir sonraki boş sütun
    int32_t shelf_y{kPadding};   ///< Aktif rafın üst kenarı
    int32_t shelf_height{0};     ///< Aktif raftaki en yüksek glyph
  };

  /// @brief Yeni bir boş sayfa oluştur (şeffaf siyahla doldurulur).
  /// @return Başarıda sayfa indeksi, aksi halde SIZE_MAX.
  std::size_t CreatePage(IRenderer& renderer);

  /// @brief Sayfada verilen boyuta yer aç; sığmazsa false.
  static bool Allocate(Page& page, int32_t width, int32_t height,
                       int32_t& out_x, int32_t& out_y);

  std::vector<std::unique_ptr<Page>> mPages;
};

}  // namespace sdl_painter
