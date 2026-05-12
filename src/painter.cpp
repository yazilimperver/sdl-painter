#include "sdl_painter/painter.h"

#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <spdlog/spdlog.h>

#include "render_batcher.h"
#include "sdl_painter/image.h"
#include "sdl_painter/renderer.h"
#include "sdl_painter/texture.h"
#include "sdl_painter/vertex.h"
#include "tessellator.h"

// spdlog (Windows'ta) Windows.h çeker ve `DrawText → DrawTextA` makrosunu
// geri tanımlar. Painter::DrawText üye tanımı doğru çözünsün diye burada
// da bir kez daha kaldır.
#ifdef DrawText
#  undef DrawText
#endif

namespace sdl_painter {

namespace {
/// @brief UTF-8 dizisinden tek bir Unicode kod noktasını okur.
///
/// UTF-8, Unicode kod noktalarını 1-4 bayta kodlayan değişken uzunluklu bir
/// karakter kodlamasıdır. İlk baytın yüksek bitleri sequence uzunluğunu
/// belirler, sonraki continuation byte'lar `10xxxxxx` formatındadır.
///
/// Kodlama tablosu:
/// | Uzunluk | İlk bayt    | Continuation    | Kod noktası aralığı |
/// |---------|-------------|-----------------|---------------------|
/// | 1 bayt  | `0xxxxxxx`  | —               | U+0000..U+007F      |
/// | 2 bayt  | `110xxxxx`  | `10xxxxxx`      | U+0080..U+07FF      |
/// | 3 bayt  | `1110xxxx`  | `10xxxxxx` x2   | U+0800..U+FFFF      |
/// | 4 bayt  | `11110xxx`  | `10xxxxxx` x3   | U+10000..U+10FFFF   |
///
/// Kod noktası; ilk bayttan kalan payload bitleri ile her continuation
/// byte'ın alt 6 bitinin sola kaydırmalı birleşimi sonucu elde edilir.
///
/// @param str Okunacak baytların başlangıç adresi.
/// @param remaining Okunabilecek maksimum bayt sayısı (string'in kalan boyutu).
/// @param advance [out] Tüketilen bayt sayısı (geçerli → 1..4; geçersiz → 1).
/// @return Unicode kod noktası; geçersiz sequence → U+FFFD (replacement character).
char32_t DecodeUTF8(const char* str, std::size_t remaining,
                    std::size_t& advance) {
  // Continuation byte kontrolü: `10xxxxxx` pattern'ı.
  auto IsCont = [](uint8_t b) { return (b & 0xC0) == 0x80; };

  const auto b0 = static_cast<uint8_t>(str[0]);

  // 1-bayt sequence: 0xxxxxxx → ASCII (U+0000..U+007F)
  if (b0 < 0x80) {
    advance = 1;
    return static_cast<char32_t>(b0);
  }

  // 2-bayt sequence: 110xxxxx 10xxxxxx → U+0080..U+07FF
  if ((b0 & 0xE0) == 0xC0 && remaining >= 2) {
    const auto b1 = static_cast<uint8_t>(str[1]);
    if (IsCont(b1)) {
      advance = 2;
      return static_cast<char32_t>(((b0 & 0x1F) << 6) | (b1 & 0x3F));
    }
  }

  // 3-bayt sequence: 1110xxxx 10xxxxxx 10xxxxxx → U+0800..U+FFFF (BMP)
  if ((b0 & 0xF0) == 0xE0 && remaining >= 3) {
    const auto b1 = static_cast<uint8_t>(str[1]);
    const auto b2 = static_cast<uint8_t>(str[2]);
    if (IsCont(b1) && IsCont(b2)) {
      advance = 3;
      return static_cast<char32_t>(((b0 & 0x0F) << 12) | ((b1 & 0x3F) << 6) |
                                   (b2 & 0x3F));
    }
  }

  // 4-bayt sequence: 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx → U+10000..U+10FFFF
  // (supplementary planes — emoji, nadir CJK karakterleri, vs.)
  if ((b0 & 0xF8) == 0xF0 && remaining >= 4) {
    const auto b1 = static_cast<uint8_t>(str[1]);
    const auto b2 = static_cast<uint8_t>(str[2]);
    const auto b3 = static_cast<uint8_t>(str[3]);
    if (IsCont(b1) && IsCont(b2) && IsCont(b3)) {
      advance = 4;
      return static_cast<char32_t>(((b0 & 0x07) << 18) | ((b1 & 0x3F) << 12) |
                                   ((b2 & 0x3F) << 6) | (b3 & 0x3F));
    }
  }

  // Geçersiz sequence (malformed, kesik veya overlong) — tek bayt atla,
  // replacement character döndür. Çağıran döngü sonraki byte'a geçer.
  advance = 1;
  return U'\uFFFD';
}

}  // namespace

Painter::Painter(SDL_Window* window, RendererBackend backend)
    : mWindow(window), mRenderer(CreateRenderer(backend)) {
  if (!window) {
    spdlog::error("Painter: window pointer null, Painter geçersiz durumda.");
    mRenderer.reset();
    return;
  }
  if (!mRenderer) {
    spdlog::error(
        "Painter: renderer oluşturulamadı (Vulkan derlenmemiş olabilir?).");
    return;
  }
  if (!mRenderer->Initialize(window)) {
    spdlog::error("Painter: renderer initialize başarısız.");
    mRenderer.reset();
    return;
  }
  mBatcher = std::make_unique<RenderBatcher>(*mRenderer);
  int w = 0, h = 0;
  SDL_GetWindowSize(window, &w, &h);
  mViewportWidth  = w;
  mViewportHeight = h;
  UpdateProjection();
}

Painter::~Painter() {
  if (mRenderer) mRenderer->Shutdown();
}

Painter::Painter(Painter&& other) noexcept
    : mWindow(other.mWindow),
      mRenderer(std::move(other.mRenderer)),
      mBatcher(std::move(other.mBatcher)),
      mStateStack(std::move(other.mStateStack)),
      mCurrentState(std::move(other.mCurrentState)),
      mCurrentFont(std::move(other.mCurrentFont)),
      mViewportWidth(other.mViewportWidth),
      mViewportHeight(other.mViewportHeight) {
  other.mWindow = nullptr;
}

Painter& Painter::operator=(Painter&& other) noexcept {
  if (this != &other) {
    if (mRenderer) mRenderer->Shutdown();
    mWindow         = other.mWindow;
    mRenderer       = std::move(other.mRenderer);
    mBatcher        = std::move(other.mBatcher);
    mStateStack     = std::move(other.mStateStack);
    mCurrentState   = std::move(other.mCurrentState);
    mCurrentFont    = std::move(other.mCurrentFont);
    mViewportWidth  = other.mViewportWidth;
    mViewportHeight = other.mViewportHeight;
    other.mWindow   = nullptr;
  }
  return *this;
}

void Painter::Begin() {
  if (!mRenderer || !mWindow) return;
  // Pencere yeniden boyutlandirildiysa viewport ve projeksiyon matrisini guncelle.
  int w = 0, h = 0;
  SDL_GetWindowSize(mWindow, &w, &h);
  if (w != mViewportWidth || h != mViewportHeight) {
    mViewportWidth  = w;
    mViewportHeight = h;
    mRenderer->SetViewport(0, 0, w, h);
    UpdateProjection();
  }
  mRenderer->BeginFrame();
  FlushTransform();
}

void Painter::End() {
  if (!mRenderer) return;
  if (mBatcher) mBatcher->Flush();
  mRenderer->EndFrame();
}

void Painter::Clear(const Color& color) {
  if (!mRenderer) return;
  if (mBatcher) mBatcher->Flush();
  mRenderer->Clear(color);
}

void Painter::SetPen(const Pen& pen) { mCurrentState.pen = pen; }
void Painter::SetBrush(const Brush& brush) { mCurrentState.brush = brush; }
void Painter::SetFont(std::shared_ptr<Font> font) { mCurrentFont = std::move(font); }
void Painter::SetOpacity(float alpha) {
  mCurrentState.opacity = alpha;
  if (mRenderer) mRenderer->SetOpacity(alpha);
}

void Painter::DrawLine(float x1, float y1, float x2, float y2) {
  if (!CanDrawPen()) return;
  FlushTransform();
  auto verts = Tessellator::TessellateThickLine(
      x1, y1, x2, y2, mCurrentState.pen.GetWidth());
  mBatcher->PushTriangles(verts, mCurrentState.pen.GetColor(), mCurrentState.opacity);
}

void Painter::DrawRect(float x, float y, float w, float h) {
  if (!CanDrawPen()) return;
  FlushTransform();
  auto verts = Tessellator::TessellateStrokedRect(
      x, y, w, h, mCurrentState.pen.GetWidth());
  mBatcher->PushTriangles(verts, mCurrentState.pen.GetColor(), mCurrentState.opacity);
}

void Painter::FillRect(float x, float y, float w, float h) {
  if (!CanDrawBrush()) return;
  FlushTransform();
  auto verts = Tessellator::TessellateFilledRect(x, y, w, h);
  mBatcher->PushTriangles(verts, mCurrentState.brush.GetColor(), mCurrentState.opacity);
}

void Painter::DrawCircle(float cx, float cy, float radius) {
  if (!CanDrawPen()) return;
  FlushTransform();
  auto verts = Tessellator::TessellateStrokedCircle(
      cx, cy, radius, mCurrentState.pen.GetWidth());
  mBatcher->PushTriangles(verts, mCurrentState.pen.GetColor(), mCurrentState.opacity);
}

void Painter::FillCircle(float cx, float cy, float radius) {
  if (!CanDrawBrush()) return;
  FlushTransform();
  auto verts = Tessellator::TessellateFilledCircle(cx, cy, radius);
  mBatcher->PushTriangles(verts, mCurrentState.brush.GetColor(), mCurrentState.opacity);
}

void Painter::DrawEllipse(float cx, float cy, float rx, float ry) {
  if (!CanDrawPen()) return;
  FlushTransform();
  auto verts = Tessellator::TessellateStrokedEllipse(
      cx, cy, rx, ry, mCurrentState.pen.GetWidth());
  mBatcher->PushTriangles(verts, mCurrentState.pen.GetColor(), mCurrentState.opacity);
}

void Painter::FillEllipse(float cx, float cy, float rx, float ry) {
  if (!CanDrawBrush()) return;
  FlushTransform();
  auto verts = Tessellator::TessellateFilledEllipse(cx, cy, rx, ry);
  mBatcher->PushTriangles(verts, mCurrentState.brush.GetColor(), mCurrentState.opacity);
}

void Painter::DrawPolygon(const std::vector<Point>& points) {
  if (!CanDrawPen()) return;
  FlushTransform();
  auto verts = Tessellator::TessellateStrokedPolygon(
      points, mCurrentState.pen.GetWidth());
  mBatcher->PushTriangles(verts, mCurrentState.pen.GetColor(), mCurrentState.opacity);
}

void Painter::FillPolygon(const std::vector<Point>& points) {
  if (!CanDrawBrush()) return;
  FlushTransform();
  auto verts = Tessellator::TessellateFilledPolygon(points);
  mBatcher->PushTriangles(verts, mCurrentState.brush.GetColor(), mCurrentState.opacity);
}

void Painter::DrawPolyline(const std::vector<Point>& points) {
  if (!CanDrawPen()) return;
  FlushTransform();
  auto verts = Tessellator::TessellateThickPolyline(
      points, mCurrentState.pen.GetWidth());
  mBatcher->PushTriangles(verts, mCurrentState.pen.GetColor(), mCurrentState.opacity);
}

void Painter::DrawImage(const Image& image, float x, float y) {
  DrawImage(image,
            Rect{0.0f, 0.0f,
                 static_cast<float>(image.Width()),
                 static_cast<float>(image.Height())},
            Rect{x, y,
                 static_cast<float>(image.Width()),
                 static_cast<float>(image.Height())});
}

void Painter::DrawImage(const Image& image, const Rect& dest_rect) {
  DrawImage(image,
            Rect{0.0f, 0.0f,
                 static_cast<float>(image.Width()),
                 static_cast<float>(image.Height())},
            dest_rect);
}

void Painter::DrawImage(const Image& image, const Rect& src_rect,
                        const Rect& dest_rect) {
  if (!mRenderer || !image.IsValid()) return;

  TextureHandle handle = image.Upload(*mRenderer);
  if (handle == kInvalidTexture) return;

  // src_rect → UV koordinatlarina donustur [0, 1]
  const float img_w = static_cast<float>(image.Width());
  const float img_h = static_cast<float>(image.Height());
  const float u0 = src_rect.x / img_w;
  const float v0 = src_rect.y / img_h;
  const float u1 = (src_rect.x + src_rect.w) / img_w;
  const float v1 = (src_rect.y + src_rect.h) / img_h;

  // dest_rect → ekran koordinatlari
  const float x0 = dest_rect.x;
  const float y0 = dest_rect.y;
  const float x1 = dest_rect.x + dest_rect.w;
  const float y1 = dest_rect.y + dest_rect.h;

  // Iki ucgenden olusan quad (CCW)
  const std::vector<TexturedVertex> verts = {
      {x0, y0, u0, v0}, {x1, y0, u1, v0}, {x1, y1, u1, v1},
      {x0, y0, u0, v0}, {x1, y1, u1, v1}, {x0, y1, u0, v1},
  };

  FlushTransform();
  mBatcher->PushTexturedTriangles(verts, handle, Color{255, 255, 255, 255}, mCurrentState.opacity);
}

void Painter::DrawText(float x, float y, const std::string& text) {
  if (!mRenderer || !mBatcher || !mCurrentFont || !mCurrentFont->IsValid()) return;
  if (text.empty()) return;

  const Color& tint = mCurrentState.pen.GetColor();
  float current_x   = x;

  auto* font = static_cast<TTF_Font*>(mCurrentFont->Handle());
  const float baseline_y = y;

  for (size_t i = 0; i < text.size(); ) {
    std::size_t advance = 0;
    char32_t c = DecodeUTF8(text.c_str() + i, text.size() - i, advance);
    i += advance;

    const Glyph* glyph = mCurrentFont->GetGlyph(*mRenderer, c);
    if (!glyph || !glyph->texture.IsValid()) {
      if (c == ' ') {
        int w = 0, h = 0;
        TTF_GetStringSize(font, " ", 1, &w, &h);
        current_x += static_cast<float>(w);
      }
      continue;
    }

    // Baseline tabanlı çizim:
    // y: baseline - glyph'in baseline'dan yukarı olan yüksekliği
    const float gx0 = current_x + static_cast<float>(glyph->bearing_x);
    const float gy0 = baseline_y - static_cast<float>(glyph->bearing_y);
    const float gx1 = gx0 + static_cast<float>(glyph->width);
    const float gy1 = gy0 + static_cast<float>(glyph->height);

    const std::vector<TexturedVertex> verts = {
        {gx0, gy0, 0.0f, 0.0f}, {gx1, gy0, 1.0f, 0.0f}, {gx1, gy1, 1.0f, 1.0f},
        {gx0, gy0, 0.0f, 0.0f}, {gx1, gy1, 1.0f, 1.0f}, {gx0, gy1, 0.0f, 1.0f},
    };

    mBatcher->PushTexturedTriangles(verts, glyph->texture.Handle(), tint, mCurrentState.opacity);

    current_x += static_cast<float>(glyph->advance);
  }
}

void Painter::DrawText(const Rect& rect, const std::string& text,
                       Alignment alignment) {
  if (!mRenderer || !mCurrentFont || !mCurrentFont->IsValid()) return;
  if (text.empty()) return;

  // Metin boyutunu ölç, hizalama ofseti hesapla.
  int32_t text_w = 0, text_h = 0;
  mCurrentFont->MeasureText(text, text_w, text_h);

  float x = rect.x;
  switch (alignment) {
    case Alignment::kLeft:
      x = rect.x;
      break;
    case Alignment::kCenter:
      x = rect.x + (rect.w - static_cast<float>(text_w)) * 0.5f;
      break;
    case Alignment::kRight:
      x = rect.x + rect.w - static_cast<float>(text_w);
      break;
  }
  // Dikdörtgen içinde dikey ortala. text_h = font->height + max_ascent
  // bilesenlerini icerdiginden, yazi kutusunun ust kenari top_y olur ve
  // baseline top_y + font_ascent konumundadir.
  const float top_y = rect.y + (rect.h - static_cast<float>(text_h)) * 0.5f;
  const float baseline_y = top_y + static_cast<float>(mCurrentFont->Ascent());

  DrawText(x, baseline_y, text);
}

void Painter::Save() {
  if (mBatcher) mBatcher->Flush();
  mStateStack.push_back(mCurrentState);
}

void Painter::Restore() {
  if (mStateStack.empty()) {
    // Save() çağrılmadan Restore() çağrıldı — debugging için uyarı.
    spdlog::warn("Painter::Restore() çağrıldı ancak state stack boş "
                 "(unbalanced Save/Restore).");
    return;
  }
  if (mBatcher) mBatcher->Flush();
  mCurrentState = mStateStack.back();
  mStateStack.pop_back();
  FlushTransform();

  // Opaklik durumunu renderer'a yeniden uygula.
  if (mRenderer) mRenderer->SetOpacity(mCurrentState.opacity);

  // Kaydedilen clip durumunu renderer'a yeniden uygula.
  if (mCurrentState.has_clip) {
    ApplyScissor(mCurrentState.clip_rect);
  } else {
    if (mRenderer) mRenderer->ClearScissor();
  }
}

void Painter::Translate(float dx, float dy) {
  if (mBatcher) mBatcher->Flush();
  mCurrentState.transform.Translate(dx, dy);
}

void Painter::Rotate(float angle_degrees) {
  if (mBatcher) mBatcher->Flush();
  mCurrentState.transform.Rotate(angle_degrees);
}

void Painter::Scale(float sx, float sy) {
  if (mBatcher) mBatcher->Flush();
  mCurrentState.transform.Scale(sx, sy);
}

void Painter::ResetTransform() {
  if (mBatcher) mBatcher->Flush();
  mCurrentState.transform.SetIdentity();
}

void Painter::SetClipRect(const Rect& rect) {
  if (!mRenderer) return;
  if (mBatcher) mBatcher->Flush();
  mCurrentState.clip_rect = rect;
  mCurrentState.has_clip = true;
  ApplyScissor(rect);
}

void Painter::ClearClip() {
  if (!mRenderer) return;
  if (mBatcher) mBatcher->Flush();
  mCurrentState.has_clip = false;
  mRenderer->ClearScissor();
}

void Painter::UpdateProjection() {
  if (!mRenderer) return;
  if (mBatcher) mBatcher->Flush();
  // Ortografik projeksiyon: [0, width] x [0, height] → NDC
  // OpenGL: Y ekseni clip space'de yukarı pozitif → Y'yi ters çevir (-2/h).
  // Vulkan: Y ekseni clip space'de aşağı pozitif → ters çevirme gerekmez (+2/h).
  float w = static_cast<float>(mViewportWidth);
  float h = static_cast<float>(mViewportHeight);

  const bool is_vulkan =
      (mRenderer->GetBackend() == RendererBackend::kVulkan);
  const float sy = is_vulkan ? (2.0f / h) : (-2.0f / h);
  const float ty = is_vulkan ? -1.0f : 1.0f;

  // 4x4 sütun-major ortografik matris
  // clang-format off
  float mat[16] = {
      2.0f / w,  0.0f,  0.0f, 0.0f,
      0.0f,      sy,    0.0f, 0.0f,
      0.0f,      0.0f, -1.0f, 0.0f,
     -1.0f,      ty,    0.0f, 1.0f,
  };
  // clang-format on
  mRenderer->SetProjectionMatrix(mat);
}

void Painter::FlushTransform() {
  if (!mRenderer) return;
  mRenderer->SetModelMatrix(mCurrentState.transform.Data());
}

void Painter::ApplyScissor(const Rect& rect) {
  // OpenGL scissor Y=0 altta; Vulkan Y=0 üstte.
  const bool is_vulkan =
      (mRenderer->GetBackend() == RendererBackend::kVulkan);
  const int32_t scissor_y =
      is_vulkan ? static_cast<int32_t>(rect.y)
                : (mViewportHeight - static_cast<int32_t>(rect.y) -
                   static_cast<int32_t>(rect.h));
  mRenderer->SetScissor(static_cast<int32_t>(rect.x), scissor_y,
                        static_cast<int32_t>(rect.w),
                        static_cast<int32_t>(rect.h));
}

}  // namespace sdl_painter
