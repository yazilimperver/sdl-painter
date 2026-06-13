#include "sdl_painter/font.h"

#include "sdl_painter/renderer.h"

#include <SDL3_ttf/SDL_ttf.h>

#include <spdlog/spdlog.h>

namespace sdl_painter {

// TTF subsystem referans sayacı — birden fazla Font nesnesi güvenli paylaşım.
static int32_t gTTFRefCount = 0;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables, readability-identifier-naming)

static bool EnsureTTFInit() {
  if (gTTFRefCount == 0) {
    if (!TTF_Init()) {
      spdlog::error("TTF_Init basarisiz");
      return false;
    }
    spdlog::debug("SDL_ttf baslatildi");
  }
  ++gTTFRefCount;
  return true;
}

static void ReleaseTTF() {
  if (gTTFRefCount <= 0) {
    return;
  }
  --gTTFRefCount;
  if (gTTFRefCount == 0) {
    TTF_Quit();
    spdlog::debug("SDL_ttf kapatildi");
  }
}

Font::Font(const std::string& file_path, int32_t point_size)
    : mPointSize(point_size) {
  if (!EnsureTTFInit()) {
    return;
  }
  mHandle = TTF_OpenFont(file_path.c_str(), static_cast<float>(point_size));
  if (mHandle == nullptr) {
    spdlog::error("Font yuklenemedi: {} ({})", file_path, SDL_GetError());
    ReleaseTTF();
  }
}

Font::~Font() {
  if (mHandle != nullptr) {
    TTF_CloseFont(static_cast<TTF_Font*>(mHandle));
    mHandle = nullptr;
    ReleaseTTF();
  }
}

Font::Font(Font&& other) noexcept
    : mHandle(other.mHandle), mPointSize(other.mPointSize) {
  other.mHandle = nullptr;
  other.mPointSize = 0;
}

Font& Font::operator=(Font&& other) noexcept {
  if (this != &other) {
    if (mHandle != nullptr) {
      TTF_CloseFont(static_cast<TTF_Font*>(mHandle));
      ReleaseTTF();
    }
    mHandle = other.mHandle;
    mPointSize = other.mPointSize;
    other.mHandle = nullptr;
    other.mPointSize = 0;
  }
  return *this;
}

int32_t Font::Ascent() const {
  if (mHandle == nullptr) {
    return 0;
  }
  return TTF_GetFontAscent(static_cast<TTF_Font*>(mHandle));
}

bool Font::MeasureText(const std::string& text, int32_t& out_width,
                       int32_t& out_height) const {
  if (mHandle == nullptr || text.empty()) {
    out_width = out_height = 0;
    return false;
  }
  int w = 0;
  int h = 0;
  bool ok = TTF_GetStringSize(static_cast<TTF_Font*>(mHandle), text.c_str(),
                              text.size(), &w, &h);
  out_width = w;
  out_height = h;
  return ok;
}

const Glyph* Font::GetGlyph(IRenderer& renderer, char32_t codepoint) const {
  auto it = mGlyphCache.find(codepoint);
  if (it != mGlyphCache.end()) {
    return &it->second;
  }

  if (mHandle == nullptr) {
    return nullptr;
  }

  auto* font = static_cast<TTF_Font*>(mHandle);

  // Karakteri render et (Beyaz renkte)
  SDL_Color white = {255, 255, 255, 255};
  // TTF_RenderGlyph_Blended, karakterin sıkıca kırpılmış (tightly cropped)
  // bir yüzeyini verir.
  SDL_Surface* surface = TTF_RenderGlyph_Blended(font, codepoint, white);
  if (surface == nullptr) {
    return nullptr;
  }

  // RGBA32 formatina donustur
  SDL_Surface* rgba_surface =
      SDL_ConvertSurface(surface, SDL_PIXELFORMAT_RGBA32);
  SDL_DestroySurface(surface);
  if (rgba_surface == nullptr) {
    return nullptr;
  }

  // Texture olustur
  TextureHandle handle =
      renderer.CreateTexture(static_cast<const uint8_t*>(rgba_surface->pixels),
                             rgba_surface->w, rgba_surface->h, 4);

  // Metrikleri al
  int minx = 0;
  int maxx = 0;
  int miny = 0;
  int maxy = 0;
  int advance = 0;
  if (!TTF_GetGlyphMetrics(font, codepoint, &minx, &maxx, &miny, &maxy,
                           &advance)) {
    SDL_DestroySurface(rgba_surface);
    return nullptr;
  }

  // Önbelleğe ekle
  Glyph glyph;
  glyph.texture = Texture(&renderer, handle);
  glyph.width = rgba_surface->w;
  glyph.height = rgba_surface->h;
  glyph.advance = advance;
  glyph.bearing_x = minx;
  // SDL_ttf 3.x'te TTF_RenderGlyph_Blended, `TTF_RenderText_Blended` ile
  // ayni yolu izler: tek glyph yuzeyinde baseline `font->ascent` satirindadir
  // ve yuzey yuksekligi glyph'ten bagimsiz olarak `font->height` kadardir.
  // Bu yuzden bearing_y tum glyph'ler icin SABIT `font->ascent` degeridir.
  glyph.bearing_y = TTF_GetFontAscent(font);
  (void)maxy;
  (void)miny;

  SDL_DestroySurface(rgba_surface);

  mGlyphCache[codepoint] = std::move(glyph);
  return &mGlyphCache[codepoint];
}

}  // namespace sdl_painter
