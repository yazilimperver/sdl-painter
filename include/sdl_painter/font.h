#pragma once

#include "sdl_painter/texture.h"

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>

namespace sdl_painter {

class IRenderer;

/// @brief Tek bir karakterin metrikleri ve dokusu.
struct Glyph {
  Texture texture;
  int32_t width{0};
  int32_t height{0};
  int32_t advance{0};
  int32_t bearing_x{0};
  int32_t bearing_y{0};
};

/// @brief Metin hizalama seçeneği.
enum class Alignment {
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
  Font(Font&&) noexcept;
  Font& operator=(Font&&) noexcept;

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

 private:
  void* mHandle{nullptr};
  int32_t mPointSize{0};

  // Karakter önbelleği (mutable çünkü mantıksal const'u bozmuyoruz)
  mutable std::unordered_map<char32_t, Glyph> mGlyphCache;
};

}  // namespace sdl_painter
