#include "sdl_painter/font.h"

#include "sdl_painter/renderer.h"

#include <SDL3_ttf/SDL_ttf.h>

#include <spdlog/spdlog.h>
#include <utility>

#include "glyph_atlas.h"

namespace sdl_painter {

// TTF subsystem referans sayacı — birden fazla Font nesnesi güvenli paylaşım.
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables, readability-identifier-naming)
static int32_t gTTFRefCount = 0;

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

Font::Font() = default;

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
  mGlyphCache.clear();
  mAtlas.reset();
  if (mHandle != nullptr) {
    TTF_CloseFont(static_cast<TTF_Font*>(mHandle));
    mHandle = nullptr;
    ReleaseTTF();
  }
}

Font::Font(Font&& other) noexcept
    : mHandle(other.mHandle),
      mPointSize(other.mPointSize),
      mGlyphCache(std::move(other.mGlyphCache)),
      mAtlas(std::move(other.mAtlas)) {
  other.mHandle = nullptr;
  other.mPointSize = 0;
  other.mGlyphCache.clear();
}

Font& Font::operator=(Font&& other) noexcept {
  if (this != &other) {
    mGlyphCache.clear();
    mAtlas.reset();
    if (mHandle != nullptr) {
      TTF_CloseFont(static_cast<TTF_Font*>(mHandle));
      ReleaseTTF();
    }
    mHandle = other.mHandle;
    mPointSize = other.mPointSize;
    mGlyphCache = std::move(other.mGlyphCache);
    mAtlas = std::move(other.mAtlas);
    other.mHandle = nullptr;
    other.mPointSize = 0;
    other.mGlyphCache.clear();
  }
  return *this;
}

int32_t Font::Ascent() const {
  if (mHandle == nullptr) {
    return 0;
  }
  return TTF_GetFontAscent(static_cast<TTF_Font*>(mHandle));
}

int32_t Font::LineHeight() const {
  if (mHandle == nullptr) {
    return 0;
  }
  return TTF_GetFontHeight(static_cast<TTF_Font*>(mHandle));
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

  // Beyaz render edilir: renk vertex'te tasindigi icin glyph notr bir
  // taban olmali, aksi halde Pen rengiyle carpim yanlis sonuc verir.
  SDL_Color white = {255, 255, 255, 255};
  // TTF_RenderGlyph_Blended, karakterin sıkıca kırpılmış (tightly cropped)
  // bir yüzeyini verir.
  SDL_Surface* surface = TTF_RenderGlyph_Blended(font, codepoint, white);
  if (surface == nullptr) {
    return nullptr;
  }

  SDL_Surface* rgba_surface =
      SDL_ConvertSurface(surface, SDL_PIXELFORMAT_RGBA32);
  SDL_DestroySurface(surface);
  if (rgba_surface == nullptr) {
    return nullptr;
  }

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

  // Gorunmez glyph (bosluk, kontrol karakteri): atlasa yer ayirma, yalnizca
  // metrikleri onbellege al. Cagiran `IsValid()` false gorup imleci `advance`
  // kadar ilerletir.
  if (rgba_surface->w <= 0 || rgba_surface->h <= 0) {
    Glyph blank;
    blank.advance = advance;
    blank.bearing_x = minx;
    blank.bearing_y = TTF_GetFontAscent(font);
    SDL_DestroySurface(rgba_surface);
    return &mGlyphCache.emplace(codepoint, std::move(blank)).first->second;
  }

  // Glyph'i ortak atlasa yerlestir. Ayri texture yerine atlas kullanmak,
  // ayni fonttan cizilen tum karakterlerin tek draw call'da toplanmasini
  // saglar (bkz. GlyphAtlas).
  if (mAtlas == nullptr) {
    mAtlas = std::make_unique<GlyphAtlas>();
  }

  Glyph glyph;
  GlyphAtlas::Region region;
  if (!mAtlas->Add(renderer, static_cast<const uint8_t*>(rgba_surface->pixels),
                   rgba_surface->w, rgba_surface->h, region)) {
    spdlog::warn("Font: U+{:04X} atlasa yerlestirilemedi ({}x{}).",
                 static_cast<uint32_t>(codepoint), rgba_surface->w,
                 rgba_surface->h);
    SDL_DestroySurface(rgba_surface);
    return nullptr;
  }
  glyph.texture = region.texture;
  glyph.u0 = region.u0;
  glyph.v0 = region.v0;
  glyph.u1 = region.u1;
  glyph.v1 = region.v1;

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

  // Tek ekleme + tek arama: emplace, eklenen ogeye iterator dondurur.
  return &mGlyphCache.emplace(codepoint, std::move(glyph)).first->second;
}

std::size_t Font::AtlasPageCount() const {
  return mAtlas ? mAtlas->PageCount() : 0;
}

}  // namespace sdl_painter
