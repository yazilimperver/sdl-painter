#pragma once

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
/// @note v1.2.0'da değişti: glyph'ler artık **ortak bir atlas texture'ında**
/// tutulur. Eskiden her glyph kendi `Texture` nesnesine sahipti ve bu, metin
/// çiziminde karakter başına bir draw call'a yol açıyordu. Artık `texture`
/// atlasın sayfa tanımlayıcısıdır (sahiplik atlastadır, Glyph'te değil) ve
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

/// @brief Font sarmalayıcı — SDL_ttf üzerinden (Phase 4).
///
/// Phase 4'e kadar bu sınıf yalnızca bir stub'dır.
///
/// @warning **Yaşam döngüsü sözleşmesi:** Font, glyph önbelleğindeki
/// texture'ları yükleyen Painter (ve dolayısıyla IRenderer) yaşıyorken
/// yıkılmalıdır. Painter yok olduktan sonra Font yıkılırsa, glyph
/// önbelleğindeki her `Texture` raw IRenderer pointer'ı dangling olur ve
/// davranış tanımsızdır. Painter `std::shared_ptr<Font>` ile tuttuğu için
/// genelde sözleşme kendiliğinden sağlanır; ancak kullanıcı font'u
/// Painter'dan bağımsız olarak `static` veya global tutarsa bu garanti
/// kaybolur. v0.2.0'da bu daha sağlam bir mekanizmayla zorunlu kılınacaktır.
class Font {
 public:
  Font() = default;

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

  /// @brief SDL_ttf font handle'ı döndür (opak pointer).
  [[nodiscard]] void* Handle() const noexcept { return mHandle; }

  /// @brief Verilen metnin piksel boyutunu ölç.
  /// @param text Ölçülecek metin.
  /// @param out_width Çıkış: genişlik (piksel).
  /// @param out_height Çıkış: yükseklik (piksel).
  /// @return Ölçüm başarılıysa true.
  bool MeasureText(const std::string& text, int32_t& out_width,
                   int32_t& out_height) const;

  /// @brief Karakter için Glyph al; yoksa oluşturur.
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
};

}  // namespace sdl_painter
