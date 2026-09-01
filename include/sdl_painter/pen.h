#pragma once

#include "sdl_painter/color.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <initializer_list>

namespace sdl_painter {

/// @brief Açık uçlu çizgilerin uç (cap) stili.
///
/// Yalnızca açık geometrinin uçlarına uygulanır: `DrawLine` ve
/// `DrawPolyline`. Kapalı şekillerde (dikdörtgen, daire, poligon çerçevesi)
/// uç yoktur, yalnızca birleşim vardır.
enum class LineCap : uint8_t {
  kButt,    ///< Düz kesik — çizgi uç noktasında biter. **Varsayılan.**
  kSquare,  ///< Kare — kalınlığın yarısı kadar uzatılır.
  kRound,   ///< Yuvarlak — uca yarım kalınlık yarıçaplı disk.
};

/// @brief Köşelerde iki segmentin birleşim (join) stili.
///
/// Segmentler bağımsız quad'lar olarak üretildiği için her köşede kama biçimli
/// bir boşluk kalır; birleşim bu boşluğu doldurur.
enum class LineJoin : uint8_t {
  kRound,  ///< Yuvarlak — köşeye disk. Keskin açılarda sivrilmez. **Varsayılan.**
  kMiter,  ///< Sivri — dış kenarlar kesişene dek uzatılır; keskin açılarda
           ///< miter sınırı aşılırsa kendiliğinden bevel'a düşer.
  kBevel,  ///< Kesik köşe — iki dış nokta tek üçgenle birleştirilir.
};

/// @brief Miter birleşimin uzunluk sınırı (çizgi kalınlığının katı).
///
/// Bu oranın üstünde miter noktası köşeden aşırı uzaklaşıp sivri bir çıkıntı
/// (spike) üretir; o durumda bevel'a düşülür. SVG'nin varsayılanıyla aynı.
inline constexpr float kMiterLimit = 4.0F;

/// @brief Bir kesikli çizgi deseninin taşıyabileceği en fazla uzunluk sayısı.
///
/// Desen `Pen` içinde sabit boyutlu bir dizide tutulur, `std::vector`'da değil:
/// `Pen`, `RenderState`'in üyesidir ve her `Save`/`Restore` ile kopyalanır —
/// orada bir yığın (heap) tahsisi istemiyoruz. Sekiz, pratikte görülen tüm
/// desenleri fazlasıyla karşılar (tipik desenler 2–4 uzunluktan oluşur).
inline constexpr std::size_t kMaxDashSegments = 8;

/// @brief Çizgi stili — renk, kalınlık ve isteğe bağlı dış kontur.
class Pen {
 public:
  /// @brief Varsayılan kalem: siyah, 1 piksel kalınlık.
  Pen() = default;

  /// @brief Renk ve kalınlıkla kalem oluştur.
  /// @param color Çizgi rengi.
  /// @param width Çizgi kalınlığı (piksel, >= 0.0).
  explicit Pen(const Color& color, float width = 1.0F)
      : mColor(color), mWidth(width) {}

  /// @brief Dış konturlu (outline) kalem oluştur.
  ///
  /// Çizim sırasında önce outline_color ile daha kalın bir geometri,
  /// ardından üzerine color/width ile normal geometri çizilir. Bu sayede
  /// çizgi, resim veya karmaşık arka planlar üzerinde daha belirgin olur.
  /// @param color Çizgi rengi.
  /// @param width Çizgi kalınlığı (piksel, >= 0.0).
  /// @param outline_color Dış kontur rengi.
  /// @param outline_width Dış kontur kalınlığı (piksel, her iki tarafa
  /// eklenir; >= 0.0).
  Pen(const Color& color, float width, const Color& outline_color,
      float outline_width)
      : mColor(color),
        mWidth(width),
        mOutlineColor(outline_color),
        mOutlineWidth(outline_width) {}

  /// @brief Çizgi rengini döndür.
  [[nodiscard]] const Color& GetColor() const noexcept { return mColor; }

  /// @brief Çizgi kalınlığını döndür.
  [[nodiscard]] float GetWidth() const noexcept { return mWidth; }

  /// @brief Dış kontur rengini döndür.
  [[nodiscard]] const Color& GetOutlineColor() const noexcept {
    return mOutlineColor;
  }

  /// @brief Dış kontur kalınlığını döndür.
  [[nodiscard]] float GetOutlineWidth() const noexcept { return mOutlineWidth; }

  /// @brief Çizgi rengini ayarla.
  void SetColor(const Color& color) noexcept { mColor = color; }

  /// @brief Çizgi kalınlığını ayarla.
  void SetWidth(float width) noexcept { mWidth = width > 0.0F ? width : 0.0F; }

  /// @brief Dış kontur rengini ayarla.
  void SetOutlineColor(const Color& color) noexcept { mOutlineColor = color; }

  /// @brief Dış kontur kalınlığını ayarla.
  void SetOutlineWidth(float width) noexcept { mOutlineWidth = width; }

  /// @brief Uç stilini ayarla (yalnızca açık çizgiler için).
  void SetCapStyle(LineCap cap) noexcept { mCap = cap; }

  /// @brief Birleşim stilini ayarla.
  void SetJoinStyle(LineJoin join) noexcept { mJoin = join; }

  [[nodiscard]] LineCap GetCapStyle() const noexcept { return mCap; }

  [[nodiscard]] LineJoin GetJoinStyle() const noexcept { return mJoin; }

  /// @brief Kesikli çizgi desenini ayarla.
  ///
  /// Uzunluklar piksel cinsinden, sırayla çizili / boş olarak okunur:
  /// `{10, 5}` → 10 piksel çiz, 5 piksel atla, tekrarla.
  ///
  /// Desen yol boyunca sürekli ilerler: bir köşede yarım kalan çizgi
  /// parçası, sonraki segmentte kaldığı yerden devam eder.
  ///
  /// Tek sayıda uzunluk verilirse desen SVG'deki gibi iki tur boyunca
  /// kendini tersine çevirerek tamamlar (`{5}` → 5 çiz, 5 atla).
  ///
  /// @param lengths Uzunluklar; en fazla @ref kMaxDashSegments tanesi alınır,
  ///        fazlası yok sayılır. Pozitif olmayan değerler eklenmez.
  ///        Boş liste veya toplamı sıfır olan desen, kesiği kapatır.
  void SetDashPattern(std::initializer_list<float> lengths) noexcept {
    mDashCount = 0;
    for (float len : lengths) {
      if (mDashCount >= kMaxDashSegments) {
        break;
      }
      if (len > 0.0F) {
        mDash[mDashCount++] = len;
      }
    }
  }

  /// @brief Kesiği kapat — çizgi kesintisiz olur.
  void ClearDashPattern() noexcept { mDashCount = 0; }

  /// @brief Desendeki geçerli uzunluk sayısı (0 ise kesik yok).
  [[nodiscard]] std::size_t GetDashCount() const noexcept { return mDashCount; }

  /// @brief Desen uzunlukları. Yalnızca ilk @ref GetDashCount tanesi anlamlı.
  [[nodiscard]] const std::array<float, kMaxDashSegments>& GetDashPattern()
      const noexcept {
    return mDash;
  }

  /// @brief Kalem kesikli mi?
  [[nodiscard]] bool HasDash() const noexcept { return mDashCount > 0; }

  /// @brief Kalem görünür mü? (alpha > 0 ve width > 0)
  [[nodiscard]] bool IsVisible() const noexcept {
    return mColor.a > 0 && mWidth > 0.0F;
  }

  /// @brief Kalemin görünür bir dış konturu var mı?
  [[nodiscard]] bool HasOutline() const noexcept {
    return mOutlineColor.a > 0 && mOutlineWidth > 0.0F;
  }

  [[nodiscard]] bool operator==(const Pen& other) const noexcept {
    return mColor == other.mColor && mWidth == other.mWidth &&
           mOutlineColor == other.mOutlineColor &&
           mOutlineWidth == other.mOutlineWidth && mCap == other.mCap &&
           mJoin == other.mJoin && DashEquals(other);
  }
  [[nodiscard]] bool operator!=(const Pen& other) const noexcept {
    return !(*this == other);
  }

  /// @brief Şeffaf (çizim yapmayan) kalem.
  [[nodiscard]] static Pen NoPen() noexcept {
    return Pen(Color::Transparent(), 0.0F);
  }

 private:
  Color mColor{Color::Black()};
  float mWidth{1.0F};
  Color mOutlineColor{Color::Transparent()};
  float mOutlineWidth{0.0F};
  // Varsayilanlar mevcut davranisi birebir korur: uc stili yoktu (duz kesik),
  // birlesim ise daima yuvarlakti. Qt'nin varsayilanlari (square/bevel) farkli
  // ama onlara gecmek sessizce her cizimin gorunumunu degistirirdi.
  LineCap mCap{LineCap::kButt};
  LineJoin mJoin{LineJoin::kRound};
  std::array<float, kMaxDashSegments> mDash{};
  std::size_t mDashCount{0};

  /// @brief Yalnızca geçerli (sayılan) desen uzunluklarını karşılaştır.
  [[nodiscard]] bool DashEquals(const Pen& other) const noexcept {
    if (mDashCount != other.mDashCount) {
      return false;
    }
    for (std::size_t i = 0; i < mDashCount; ++i) {
      if (mDash[i] != other.mDash[i]) {
        return false;
      }
    }
    return true;
  }
};

}  // namespace sdl_painter
