#pragma once

#include "sdl_painter/export.h"
#include "sdl_painter/texture.h"

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>

namespace sdl_painter {

class IRenderer;
class GlyphAtlas;

/// @brief Tek bir karakterin metrikleri ve atlas içindeki konumu.
///
/// @note v1.2.0'da değişti: glyph'ler artık ortak bir atlas texture'ında
/// tutulur. Artık `texture` atlasın sayfa tanımlayıcısıdır (sahiplik atlastadır, Glyph'te değil) ve
/// `u0/v0/u1/v1` glyph'in o sayfadaki bölgesini verir.
struct Glyph {
  /// @brief Glyph'i içeren atlas sayfasının texture'ı (sahiplik atlasta).
  TextureHandle texture{kInvalidTexture};

  // Atlas içindeki normalize doku koordinatları.
  float u0{0.0F};
  float v0{0.0F};
  float u1{0.0F};
  float v1{0.0F};

  int32_t width{0};
  int32_t height{0};
  int32_t advance{0};
  int32_t bearing_x{0};
  int32_t bearing_y{0};

  /// @brief Glyph çizilebilir bir görüntüye sahip mi?
  [[nodiscard]] bool IsValid() const noexcept {
    return texture != kInvalidTexture;
  }
};

/// @brief Metin hizalama seçeneği.
enum class Alignment : uint8_t {
  kLeft,
  kCenter,
  kRight,
};

/// @brief Dikdörtgen içine çizilen metnin sarmalama (word wrap) davranışı.
enum class TextWrap : uint8_t {
  /// @brief Sarmalama yok — uzun satır dikdörtgenden taşar, varsayılan.
  kNone,
  /// @brief Sözcük sınırlarından böl. Tek bir sözcük bile sığmıyorsa
  ///        karakter sınırından bölünür (UTF-8 güvenli).
  kWord,
};

/// @brief Font sarmalayıcı (SDL_ttf üzerinden).
///
/// @note Font, glyph texture'larını yükleyen Painter'dan uzun yaşayabilir
/// (`static`, global ya da Painter'dan sonra yıkılan bir üye): Painter yok
/// olunca glyph önbelleği geçersiz sayılır ve Font yıkılırken ölü renderer'a
/// dokunulmaz.
///
/// @warning **Bir Font aynı anda tek bir Painter'a aittir.** İlk
/// @ref GetGlyph çağrısı fontu o çağrının renderer'ına bağlar; glyph atlası o
/// renderer'ın texture'larından oluşur ve texture tanımlayıcıları renderer'a
/// yereldir. Bağlı Painter yaşarken aynı Font ikinci bir Painter ile
/// kullanılırsa @ref GetGlyph `nullptr` döner ve hata loglanır: birincinin
/// texture handle'ını ikinciye vermek sessizce yanlış (veya hiç) çizerdi. Çok
/// pencereli uygulamalar Painter başına ayrı bir Font açmalıdır. Bağlı Painter
/// yıkıldıktan sonra Font yeni bir Painter ile kullanılabilir; glyph'ler ona
/// yeniden yüklenir.
class SDLPAINTER_API Font {
 public:
  // Govde .cpp'de: sinif dllexport edildiginde derleyici bu ctor'u her
  // TU'da emit eder ve temizlik yolu icin unique_ptr<GlyphAtlas>'in
  // yikicisini ornekler — GlyphAtlas burada eksik tip.
  Font();

  /// @brief TTF dosyasından font yükle.
  /// @param file_path Font dosyasının yolu.
  /// @param point_size Punto boyutu.
  Font(const std::string& file_path, int32_t point_size);

  ~Font();

  // Non-copyable, movable
  Font(const Font&) = delete;
  Font& operator=(const Font&) = delete;
  Font(Font&& other) noexcept;
  Font& operator=(Font&& other) noexcept;

  /// @brief Font başarıyla yüklendi mi?
  [[nodiscard]] bool IsValid() const noexcept { return mHandle != nullptr; }

  /// @brief Punto boyutunu döndür.
  [[nodiscard]] int32_t PointSize() const noexcept { return mPointSize; }

  /// @brief Font ascent (baseline'dan yukariya olan maksimum piksel) degerini dondur.
  [[nodiscard]] int32_t Ascent() const;

  /// @brief İki satır arasındaki önerilen dikey mesafe (piksel).
  ///
  /// Fontun kendi metriğidir; `PointSize()` ile aynı şey değildir ve
  /// ondan büyük olması normaldir (çıkıntılar ve satır arası boşluk dahil).
  /// Çok satırlı metinde satır başına bu kadar ilerlenir.
  [[nodiscard]] int32_t LineHeight() const;

  /// @brief SDL_ttf font handle'ı döndür (opak pointer).
  [[nodiscard]] void* Handle() const noexcept { return mHandle; }

  /// @brief Verilen metnin piksel boyutunu ölç.
  ///
  /// Ölçüm, çizimle aynı glyph konumlarını kullanır (advance +
  /// @ref Kerning); genişlik, glyph'lerin görünen kısmı ile kalem sonunun
  /// birleşimidir. Hizalama ve sözcük kaydırma bu değerle yapılır.
  ///
  /// Metin soldan sağa, glyph glyph yerleştirilir: sağdan sola yazım,
  /// ligatür ve karmaşık yazım biçimlendirmesi (shaping) desteklenmez.
  /// Tek satır içindir; `\n` satır ayırıcı olarak işlenmez.
  ///
  /// @param text Ölçülecek metin.
  /// @param out_width Çıkış: genişlik (piksel).
  /// @param out_height Çıkış: yükseklik (piksel).
  /// @return Ölçüm başarılıysa true.
  bool MeasureText(const std::string& text, int32_t& out_width,
                   int32_t& out_height) const;

  /// @brief İki ardışık karakter arasındaki kerning düzeltmesi (piksel).
  ///
  /// Fontun `kern` tablosundan okunur. Kerning'i yalnızca GPOS tablosunda
  /// tanımlayan fontlarda 0 döner; o fontlar kerning'siz çizilir ve ölçülür.
  [[nodiscard]] int32_t Kerning(char32_t previous, char32_t current) const;

  /// @brief Karakter için Glyph al; yoksa oluşturur.
  ///
  /// İlk çağrı fontu @p renderer'a bağlar. Bağlı renderer yaşarken başka bir
  /// renderer ile gelen çağrılar `nullptr` döner; bağlı renderer yıkılmışsa
  /// önbellek bırakılır ve font @p renderer'a bağlanır (bkz. sınıf belgesi).
  const Glyph* GetGlyph(IRenderer& renderer, char32_t codepoint) const;

  /// @brief Glyph atlasının açtığı sayfa sayısı (teşhis/test).
  [[nodiscard]] std::size_t AtlasPageCount() const;

 private:
  void* mHandle{nullptr};
  int32_t mPointSize{0};

  // Karakter önbelleği (mutable çünkü mantıksal const'u bozmuyoruz)
  mutable std::unordered_map<char32_t, Glyph> mGlyphCache;

  /// @brief Glyph görüntülerini toplayan ortak texture atlası.
  /// İlk @ref GetGlyph çağrısında oluşturulur.
  mutable std::unique_ptr<GlyphAtlas> mAtlas;

  /// @brief Fontu ilk @ref GetGlyph çağrısında bağlayan renderer.
  ///
  /// Önbellekteki glyph'lerin texture tanımlayıcıları buna yereldir; sahibi
  /// karşılaştırmadan önbellek isabetini döndürmek, ikinci bir renderer'a
  /// yabancı bir handle vermek demekti.
  mutable IRenderer* mOwner{nullptr};

  /// @brief @ref mOwner'ın yaşam belirteci (@ref IRenderer::LifetimeToken).
  ///
  /// Sahip renderer yıkılınca font önbelleği bırakıp bir sonraki renderer'a
  /// bağlanabilir. İşaretçi karşılaştırması tek başına yetmez: ölen
  /// renderer'ın adresinde yeni bir renderer oluşabilir.
  mutable std::weak_ptr<const void> mOwnerAlive;
};

}  // namespace sdl_painter
