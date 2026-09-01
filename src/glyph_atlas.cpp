#include "glyph_atlas.h"

#include <spdlog/spdlog.h>
#include <vector>

namespace sdl_painter {

std::size_t GlyphAtlas::CreatePage(IRenderer& renderer) {
  // Sayfa şeffaf siyahla başlatilir: kullanilmayan alanlar örneklenirse
  // görünmez olur (glyph kenarlarindaki dolgu piksellerinde de bu gecerli).
  const std::vector<uint8_t> kBlank(
      static_cast<std::size_t>(kPageSize) * kPageSize * 4, 0);

  TextureHandle handle =
      renderer.CreateTexture(kBlank.data(), kPageSize, kPageSize, 4);
  if (handle == kInvalidTexture) {
    spdlog::error("GlyphAtlas: sayfa texture'i oluşturulamadi.");
    return static_cast<std::size_t>(-1);
  }

  auto page = std::make_unique<Page>();
  page->texture = Texture(&renderer, handle);
  mPages.push_back(std::move(page));
  spdlog::debug("GlyphAtlas: yeni sayfa acildi (toplam {}).", mPages.size());
  return mPages.size() - 1;
}

bool GlyphAtlas::Allocate(Page& page, int32_t width, int32_t height,
                          int32_t& out_x, int32_t& out_y) {
  const int32_t kAdvanceX = width + kPadding;
  const int32_t kAdvanceY = height + kPadding;

  // Aktif rafta yatay olarak siğiyor mu?
  if (page.cursor_x + kAdvanceX > kPageSize) {
    page.shelf_y += page.shelf_height + kPadding;
    page.cursor_x = kPadding;
    page.shelf_height = 0;
  }
  // Yeni raf sayfaya siğiyor mu?
  if (page.shelf_y + kAdvanceY > kPageSize) {
    return false;
  }

  out_x = page.cursor_x;
  out_y = page.shelf_y;
  page.cursor_x += kAdvanceX;
  if (height > page.shelf_height) {
    page.shelf_height = height;
  }
  return true;
}

bool GlyphAtlas::Add(IRenderer& renderer, const uint8_t* pixels, int32_t width,
                     int32_t height, Region& out_region) {
  if (pixels == nullptr || width <= 0 || height <= 0) {
    return false;
  }
  // Dolgu payiyla birlikte tek sayfaya siğmayan glyph atlasa alinamaz
  // (cok büyük punto). cağiran bu durumda kendi texture'ini kullanmali.
  if (width + 2 * kPadding > kPageSize || height + 2 * kPadding > kPageSize) {
    return false;
  }

  int32_t x = 0;
  int32_t y = 0;
  Page* target = nullptr;

  // Mevcut sayfalarda yer ara; yoksa yeni sayfa ac.
  for (auto& page : mPages) {
    if (Allocate(*page, width, height, x, y)) {
      target = page.get();
      break;
    }
  }
  if (target == nullptr) {
    const std::size_t kIndex = CreatePage(renderer);
    if (kIndex == static_cast<std::size_t>(-1)) {
      return false;
    }
    target = mPages[kIndex].get();
    if (!Allocate(*target, width, height, x, y)) {
      return false;  // Boş sayfaya bile siğmadi — yukaridaki kontrol kacirdi.
    }
  }

  renderer.UpdateTexture(target->texture.Handle(), x, y, width, height, pixels);

  constexpr float kInvSize = 1.0F / static_cast<float>(kPageSize);
  out_region.texture = target->texture.Handle();
  out_region.u0 = static_cast<float>(x) * kInvSize;
  out_region.v0 = static_cast<float>(y) * kInvSize;
  out_region.u1 = static_cast<float>(x + width) * kInvSize;
  out_region.v1 = static_cast<float>(y + height) * kInvSize;
  return true;
}

}  // namespace sdl_painter
