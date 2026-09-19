#include "sdl_painter/font.h"

#include "sdl_painter/renderer.h"

#include <SDL3_ttf/SDL_ttf.h>

#include <algorithm>
#include <spdlog/spdlog.h>
#include <utility>

#include "glyph_atlas.h"
#include "text_utf8.h"

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
      mAtlas(std::move(other.mAtlas)),
      mOwner(other.mOwner) {
  other.mHandle = nullptr;
  other.mPointSize = 0;
  other.mGlyphCache.clear();
  // Onbellek ve atlas tasindi; kaynagin artik bir sahibi yok.
  other.mOwner = nullptr;
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
    mOwner = other.mOwner;
    other.mHandle = nullptr;
    other.mPointSize = 0;
    other.mGlyphCache.clear();
    other.mOwner = nullptr;
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

int32_t Font::Kerning(char32_t previous, char32_t current) const {
  if (mHandle == nullptr) {
    return 0;
  }
  int kerning = 0;
  if (!TTF_GetGlyphKerning(static_cast<TTF_Font*>(mHandle), previous, current,
                           &kerning)) {
    return 0;
  }
  return kerning;
}

bool Font::MeasureText(const std::string& text, int32_t& out_width,
                       int32_t& out_height) const {
  out_width = out_height = 0;
  if (mHandle == nullptr || text.empty()) {
    return false;
  }
  // TTF_GetStringSize kullanilmiyor: HarfBuzz ile sekillendirip GPOS
  // kerning'i uyguluyor, cizim ise glyph'leri tek tek yerlestiriyor. Kalem
  // konumlari Painter::DrawTextLine ile ayni (advance + Kerning); kutu,
  // TTF_GetStringSize'daki gibi murekkep ile kalem sonunun birlesimi.
  auto* font = static_cast<TTF_Font*>(mHandle);
  const int32_t kAscent = TTF_GetFontAscent(font);
  int32_t pen = 0;
  int32_t left = 0;
  int32_t right = 0;
  int32_t top = 0;
  int32_t bottom = TTF_GetFontHeight(font);
  char32_t previous = 0;
  for (std::size_t i = 0; i < text.size();) {
    std::size_t step = 0;
    const char32_t kCodepoint =
        detail::DecodeUTF8(text.c_str() + i, text.size() - i, step);
    i += step;

    int minx = 0;
    int maxx = 0;
    int miny = 0;
    int maxy = 0;
    int advance = 0;
    if (!TTF_GetGlyphMetrics(font, kCodepoint, &minx, &maxx, &miny, &maxy,
                             &advance)) {
      return false;
    }
    if (previous != 0) {
      pen += Kerning(previous, kCodepoint);
    }
    left = std::min(left, pen + minx);
    right = std::max(right, pen + maxx);
    top = std::min(top, kAscent - maxy);
    bottom = std::max(bottom, kAscent - miny);
    pen += advance;
    previous = kCodepoint;
  }
  out_width = std::max(right, pen) - left;
  out_height = bottom - top;
  return true;
}

const Glyph* Font::GetGlyph(IRenderer& renderer, char32_t codepoint) const {
  if (mHandle == nullptr) {
    return nullptr;
  }

  // Font ilk cagrida bir renderer'a baglanir. Onbellekteki texture
  // tanimlayicilari o renderer'a yereldir; sahibi karsilastirmadan onbellek
  // isabetini dondurmek, ikinci bir renderer'a bir handle vermek anlamına geliyor.
  if (mOwner == nullptr) {
    mOwner = &renderer;
  } else if (mOwner != &renderer) {
    spdlog::error(
        "Font: bu font baska bir renderer'a bagli; her Painter icin ayri bir "
        "Font acin (bkz. font.h).");
    return nullptr;
  }

  auto it = mGlyphCache.find(codepoint);
  if (it != mGlyphCache.end()) {
    return &it->second;
  }

  auto* font = static_cast<TTF_Font*>(mHandle);

  // Beyaz render edilir: renk vertex'te tasindigi icin glyph notr bir
  // taban olmali, aksi halde Pen rengiyle carpim yanlis sonuc verir.
  SDL_Color white = {255, 255, 255, 255};
  // TTF_RenderGlyph_Blended tek karakterlik TTF_RenderText_Blended'dir; yuzey
  // sikica kirpilmis degildir. Sol kenari kalem konumudur (bearing negatifse
  // onun solu), murekkep yuzeyin icinde bearing kadar saga yerlesir.
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
