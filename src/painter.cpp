#include "sdl_painter/painter.h"

#include "sdl_painter/image.h"
#include "sdl_painter/renderer.h"
#include "sdl_painter/texture.h"
#include "sdl_painter/vertex.h"

#include <SDL3/SDL.h>

#include <array>
#include <glm/gtc/type_ptr.hpp>
#include <spdlog/spdlog.h>

#include "render_batcher.h"
#include "tessellator.h"
#include "text_utf8.h"

// spdlog (Windows'ta) Windows.h çeker ve `DrawText → DrawTextA` makrosunu
// geri tanımlar. Painter::DrawText üye tanımı doğru çözünsün diye burada
// da bir kez daha kaldır.
#ifdef DrawText
#undef DrawText
#endif

namespace sdl_painter {

Painter::Painter(SDL_Window* window, RendererBackend backend)
    : mWindow(window), mRenderer(CreateRenderer(backend)) {
  if (window == nullptr) {
    spdlog::error("Painter: window pointer null, Painter geçersiz durumda.");
    mRenderer.reset();
    return;
  }
  if (mRenderer == nullptr) {
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
  QueryDrawableSize(mViewportWidth, mViewportHeight);
  mRenderer->SetViewport(0, 0, mViewportWidth, mViewportHeight);
  UpdateProjection();
}

Painter::Painter(std::unique_ptr<IRenderer> renderer, int32_t viewport_width,
                 int32_t viewport_height)
    : mRenderer(std::move(renderer)),
      mViewportWidth(viewport_width),
      mViewportHeight(viewport_height) {
  if (mRenderer == nullptr) {
    spdlog::error("Painter: renderer null, Painter geçersiz durumda.");
    return;
  }
  mBatcher = std::make_unique<RenderBatcher>(*mRenderer);
  mRenderer->SetViewport(0, 0, mViewportWidth, mViewportHeight);
  UpdateProjection();
}

Painter::~Painter() {
  if (mRenderer != nullptr) {
    mRenderer->Shutdown();
  }
}

Painter::Painter(Painter&& other) noexcept
    : mWindow(other.mWindow),
      mRenderer(std::move(other.mRenderer)),
      mBatcher(std::move(other.mBatcher)),
      mStateStack(std::move(other.mStateStack)),
      mCurrentState(other.mCurrentState),
      mCurrentFont(std::move(other.mCurrentFont)),
      mViewportWidth(other.mViewportWidth),
      mViewportHeight(other.mViewportHeight) {
  other.mWindow = nullptr;
}

Painter& Painter::operator=(Painter&& other) noexcept {
  if (this != &other) {
    if (mRenderer != nullptr) {
      mRenderer->Shutdown();
    }
    mWindow = other.mWindow;
    mRenderer = std::move(other.mRenderer);
    mBatcher = std::move(other.mBatcher);
    mStateStack = std::move(other.mStateStack);
    mCurrentState = other.mCurrentState;
    mCurrentFont = std::move(other.mCurrentFont);
    mViewportWidth = other.mViewportWidth;
    mViewportHeight = other.mViewportHeight;
    other.mWindow = nullptr;
  }
  return *this;
}

void Painter::Begin() {
  if (mRenderer == nullptr) {
    return;
  }
  // Boyut degisimini yoklama: yalnizca uygulama SetDrawableSize ile acik bir
  // bildirim yapmiyorsa. Olay tabanli bildirim hem daha ucuz hem de dogru
  // zamanda gelir; yoklama, kendi olay dongusunu yazan basit uygulamalar
  // icin geriye donuk uyumlu bir emniyet agi olarak duruyor.
  if (mAutoDrawableSize && mWindow != nullptr) {
    int32_t w = 0;
    int32_t h = 0;
    QueryDrawableSize(w, h);
    ApplyDrawableSize(w, h);
  }
  mRenderer->BeginFrame();
  FlushTransform();
}

void Painter::End() {
  if (mRenderer == nullptr) {
    return;
  }
  if (mBatcher != nullptr) {
    mBatcher->Flush();
  }
  mRenderer->EndFrame();
}

void Painter::Clear(const Color& color) {
  if (mRenderer == nullptr) {
    return;
  }
  if (mBatcher != nullptr) {
    mBatcher->Flush();
  }
  mRenderer->Clear(color);
}

void Painter::SetPen(const Pen& pen) {
  mCurrentState.pen = pen;
}
void Painter::SetBrush(const Brush& brush) {
  mCurrentState.brush = brush;
}
void Painter::SetFont(std::shared_ptr<Font> font) {
  mCurrentFont = std::move(font);
}
void Painter::SetOpacity(float alpha) {
  mCurrentState.opacity = alpha;
  if (mRenderer != nullptr) {
    mRenderer->SetOpacity(alpha);
  }
}

void Painter::DrawLine(float x1, float y1, float x2, float y2) {
  if (!CanDrawPen()) {
    return;
  }
  FlushTransform();
  const Pen& pen = mCurrentState.pen;
  if (pen.HasOutline()) {
    auto outline_verts = Tessellator::TessellateThickLine(
        x1, y1, x2, y2, pen.GetWidth() + 2.0F * pen.GetOutlineWidth());
    mBatcher->PushTriangles(outline_verts, pen.GetOutlineColor(),
                            mCurrentState.opacity);
  }
  auto verts = Tessellator::TessellateThickLine(x1, y1, x2, y2, pen.GetWidth());
  mBatcher->PushTriangles(verts, pen.GetColor(), mCurrentState.opacity);
}

void Painter::DrawRect(float x, float y, float w, float h) {
  if (!CanDrawPen()) {
    return;
  }
  FlushTransform();
  const Pen& pen = mCurrentState.pen;
  if (pen.HasOutline()) {
    auto outline_verts = Tessellator::TessellateStrokedRect(
        x, y, w, h, pen.GetWidth() + 2.0F * pen.GetOutlineWidth());
    mBatcher->PushTriangles(outline_verts, pen.GetOutlineColor(),
                            mCurrentState.opacity);
  }
  auto verts = Tessellator::TessellateStrokedRect(x, y, w, h, pen.GetWidth());
  mBatcher->PushTriangles(verts, pen.GetColor(), mCurrentState.opacity);
}

void Painter::FillRect(float x, float y, float w, float h) {
  if (!CanDrawBrush()) {
    return;
  }
  FlushTransform();
  auto verts = Tessellator::TessellateFilledRect(x, y, w, h);
  mBatcher->PushTriangles(verts, mCurrentState.brush.GetColor(),
                          mCurrentState.opacity);
}

void Painter::DrawCircle(float cx, float cy, float radius) {
  if (!CanDrawPen()) {
    return;
  }
  FlushTransform();
  const Pen& pen = mCurrentState.pen;
  if (pen.HasOutline()) {
    auto outline_verts = Tessellator::TessellateStrokedCircle(
        cx, cy, radius, pen.GetWidth() + 2.0F * pen.GetOutlineWidth());
    mBatcher->PushTriangles(outline_verts, pen.GetOutlineColor(),
                            mCurrentState.opacity);
  }
  auto verts =
      Tessellator::TessellateStrokedCircle(cx, cy, radius, pen.GetWidth());
  mBatcher->PushTriangles(verts, pen.GetColor(), mCurrentState.opacity);
}

void Painter::FillCircle(float cx, float cy, float radius) {
  if (!CanDrawBrush()) {
    return;
  }
  FlushTransform();
  auto verts = Tessellator::TessellateFilledCircle(cx, cy, radius);
  mBatcher->PushTriangles(verts, mCurrentState.brush.GetColor(),
                          mCurrentState.opacity);
}

void Painter::DrawEllipse(float cx, float cy, float rx, float ry) {
  if (!CanDrawPen()) {
    return;
  }
  FlushTransform();
  const Pen& pen = mCurrentState.pen;
  if (pen.HasOutline()) {
    auto outline_verts = Tessellator::TessellateStrokedEllipse(
        cx, cy, rx, ry, pen.GetWidth() + 2.0F * pen.GetOutlineWidth());
    mBatcher->PushTriangles(outline_verts, pen.GetOutlineColor(),
                            mCurrentState.opacity);
  }
  auto verts =
      Tessellator::TessellateStrokedEllipse(cx, cy, rx, ry, pen.GetWidth());
  mBatcher->PushTriangles(verts, pen.GetColor(), mCurrentState.opacity);
}

void Painter::FillEllipse(float cx, float cy, float rx, float ry) {
  if (!CanDrawBrush()) {
    return;
  }
  FlushTransform();
  auto verts = Tessellator::TessellateFilledEllipse(cx, cy, rx, ry);
  mBatcher->PushTriangles(verts, mCurrentState.brush.GetColor(),
                          mCurrentState.opacity);
}

void Painter::DrawPolygon(const std::vector<Point>& points) {
  if (!CanDrawPen()) {
    return;
  }
  FlushTransform();
  const Pen& pen = mCurrentState.pen;
  if (pen.HasOutline()) {
    auto outline_verts = Tessellator::TessellateStrokedPolygon(
        points, pen.GetWidth() + 2.0F * pen.GetOutlineWidth());
    mBatcher->PushTriangles(outline_verts, pen.GetOutlineColor(),
                            mCurrentState.opacity);
  }
  auto verts = Tessellator::TessellateStrokedPolygon(points, pen.GetWidth());
  mBatcher->PushTriangles(verts, pen.GetColor(), mCurrentState.opacity);
}

void Painter::FillPolygon(const std::vector<Point>& points) {
  if (!CanDrawBrush()) {
    return;
  }
  FlushTransform();
  auto verts = Tessellator::TessellateFilledPolygon(points);
  mBatcher->PushTriangles(verts, mCurrentState.brush.GetColor(),
                          mCurrentState.opacity);
}

void Painter::DrawPolyline(const std::vector<Point>& points) {
  if (!CanDrawPen()) {
    return;
  }
  FlushTransform();
  const Pen& pen = mCurrentState.pen;
  if (pen.HasOutline()) {
    auto outline_verts = Tessellator::TessellateThickPolyline(
        points, pen.GetWidth() + 2.0F * pen.GetOutlineWidth());
    mBatcher->PushTriangles(outline_verts, pen.GetOutlineColor(),
                            mCurrentState.opacity);
  }
  auto verts = Tessellator::TessellateThickPolyline(points, pen.GetWidth());
  mBatcher->PushTriangles(verts, pen.GetColor(), mCurrentState.opacity);
}

void Painter::DrawImage(const Image& image, float x, float y) {
  DrawImage(image,
            Rect{0.0F, 0.0F, static_cast<float>(image.Width()),
                 static_cast<float>(image.Height())},
            Rect{x, y, static_cast<float>(image.Width()),
                 static_cast<float>(image.Height())});
}

void Painter::DrawImage(const Image& image, const Rect& dest_rect) {
  DrawImage(image,
            Rect{0.0F, 0.0F, static_cast<float>(image.Width()),
                 static_cast<float>(image.Height())},
            dest_rect);
}

void Painter::DrawImage(const Image& image, const Rect& src_rect,
                        const Rect& dest_rect) {
  if (mRenderer == nullptr || !image.IsValid()) {
    return;
  }

  TextureHandle handle = image.Upload(*mRenderer);
  if (handle == kInvalidTexture) {
    return;
  }

  // src_rect → UV koordinatlarina donustur [0, 1]
  const auto kImgW = static_cast<float>(image.Width());
  const auto kImgH = static_cast<float>(image.Height());
  const float kU0 = src_rect.x / kImgW;
  const float kV0 = src_rect.y / kImgH;
  const float kU1 = (src_rect.x + src_rect.w) / kImgW;
  const float kV1 = (src_rect.y + src_rect.h) / kImgH;

  // dest_rect → ekran koordinatlari
  const float kX0 = dest_rect.x;
  const float kY0 = dest_rect.y;
  const float kX1 = dest_rect.x + dest_rect.w;
  const float kY1 = dest_rect.y + dest_rect.h;

  // Iki ucgenden olusan quad (CCW)
  const std::vector<TexturedVertex> kVerts = {
      {kX0, kY0, kU0, kV0}, {kX1, kY0, kU1, kV0}, {kX1, kY1, kU1, kV1},
      {kX0, kY0, kU0, kV0}, {kX1, kY1, kU1, kV1}, {kX0, kY1, kU0, kV1},
  };

  FlushTransform();
  mBatcher->PushTexturedTriangles(kVerts, handle, Color{255, 255, 255, 255},
                                  mCurrentState.opacity);
}

void Painter::DrawText(float x, float y, const std::string& text) {
  if (mRenderer == nullptr || mBatcher == nullptr || mCurrentFont == nullptr ||
      !mCurrentFont->IsValid()) {
    return;
  }
  if (text.empty()) {
    return;
  }

  // Metin de diğer primitifler gibi güncel transform ile çizilir; aksi halde
  // Translate/Rotate/Scale sonrası glyph'ler eski matrisle gönderilir.
  FlushTransform();

  const Color& tint = mCurrentState.pen.GetColor();
  float current_x = x;

  const float kBaselineY = y;

  for (size_t i = 0; i < text.size();) {
    std::size_t advance = 0;
    char32_t c = detail::DecodeUTF8(text.c_str() + i, text.size() - i, advance);
    i += advance;

    const Glyph* glyph = mCurrentFont->GetGlyph(*mRenderer, c);
    if (glyph == nullptr) {
      continue;
    }
    // Bosluk gibi gorunmez glyph'lerin texture'i yoktur; yalnizca imleci
    // ilerletiriz. advance metrigi zaten Glyph icinde, her karakterde
    // TTF_GetStringSize cagirmaya gerek yok.
    if (!glyph->IsValid()) {
      current_x += static_cast<float>(glyph->advance);
      continue;
    }

    // Baseline tabanlı çizim:
    // y: baseline - glyph'in baseline'dan yukarı olan yüksekliği
    const float kGx0 = current_x + static_cast<float>(glyph->bearing_x);
    const float kGy0 = kBaselineY - static_cast<float>(glyph->bearing_y);
    const float kGx1 = kGx0 + static_cast<float>(glyph->width);
    const float kGy1 = kGy0 + static_cast<float>(glyph->height);

    // UV'ler artık glyph'in atlas sayfasındaki bölgesidir (bkz. GlyphAtlas);
    // aynı fonttan çizilen tüm karakterler aynı texture'ı paylaştığı için
    // batcher metni tek draw call'a toplar.
    const float kU0 = glyph->u0;
    const float kV0 = glyph->v0;
    const float kU1 = glyph->u1;
    const float kV1 = glyph->v1;

    const std::vector<TexturedVertex> kVerts = {
        {kGx0, kGy0, kU0, kV0}, {kGx1, kGy0, kU1, kV0}, {kGx1, kGy1, kU1, kV1},
        {kGx0, kGy0, kU0, kV0}, {kGx1, kGy1, kU1, kV1}, {kGx0, kGy1, kU0, kV1},
    };

    mBatcher->PushTexturedTriangles(kVerts, glyph->texture, tint,
                                    mCurrentState.opacity);

    current_x += static_cast<float>(glyph->advance);
  }
}

void Painter::DrawText(const Rect& rect, const std::string& text,
                       Alignment alignment) {
  if (mRenderer == nullptr || mCurrentFont == nullptr ||
      !mCurrentFont->IsValid()) {
    return;
  }
  if (text.empty()) {
    return;
  }

  // Metin boyutunu ölç, hizalama ofseti hesapla.
  int32_t text_w = 0;
  int32_t text_h = 0;
  mCurrentFont->MeasureText(text, text_w, text_h);

  float x = 0.0F;
  switch (alignment) {
    case Alignment::kLeft:
      x = rect.x;
      break;
    case Alignment::kCenter:
      x = rect.x + (rect.w - static_cast<float>(text_w)) * 0.5F;
      break;
    case Alignment::kRight:
      x = rect.x + rect.w - static_cast<float>(text_w);
      break;
  }
  // Dikdörtgen içinde dikey ortala. text_h = font->height + max_ascent
  // bilesenlerini icerdiginden, yazi kutusunun ust kenari top_y olur ve
  // baseline top_y + font_ascent konumundadir.
  const float kTopY = rect.y + (rect.h - static_cast<float>(text_h)) * 0.5F;
  const float kBaselineY = kTopY + static_cast<float>(mCurrentFont->Ascent());

  DrawText(x, kBaselineY, text);
}

void Painter::Save() {
  if (mBatcher != nullptr) {
    mBatcher->Flush();
  }
  mStateStack.push_back(mCurrentState);
}

void Painter::Restore() {
  if (mStateStack.empty()) {
    // Save() çağrılmadan Restore() çağrıldı — debugging için uyarı.
    spdlog::warn(
        "Painter::Restore() çağrıldı ancak state stack boş "
        "(unbalanced Save/Restore).");
    return;
  }
  if (mBatcher != nullptr) {
    mBatcher->Flush();
  }
  mCurrentState = mStateStack.back();
  mStateStack.pop_back();
  FlushTransform();

  // Opaklik durumunu renderer'a yeniden uygula.
  if (mRenderer != nullptr) {
    mRenderer->SetOpacity(mCurrentState.opacity);
  }

  // Kaydedilen clip durumunu renderer'a yeniden uygula.
  if (mCurrentState.has_clip) {
    ApplyScissor(mCurrentState.clip_rect);
  } else {
    if (mRenderer != nullptr) {
      mRenderer->ClearScissor();
    }
  }
}

void Painter::Translate(float dx, float dy) {
  if (mBatcher != nullptr) {
    mBatcher->Flush();
  }
  // Sağdan çarp (post-multiply)
  // glm::mat3 column-major → çeviri sütun 2'de: t[2][0]=dx, t[2][1]=dy.
  glm::mat3 t(1.0F);
  t[2][0] = dx;
  t[2][1] = dy;
  mCurrentState.transform = mCurrentState.transform * t;
}

void Painter::Rotate(float angle_degrees) {
  if (mBatcher != nullptr) {
    mBatcher->Flush();
  }
  const float r = glm::radians(angle_degrees);
  const float c = std::cos(r);
  const float s = std::sin(r);
  // column-major: rot[col][row]. Row-major {c,-s; s,c} karşılığı.
  glm::mat3 rot(1.0F);
  rot[0][0] = c;
  rot[0][1] = s;
  rot[1][0] = -s;
  rot[1][1] = c;
  mCurrentState.transform = mCurrentState.transform * rot;
}

void Painter::Scale(float sx, float sy) {
  if (mBatcher != nullptr) {
    mBatcher->Flush();
  }
  glm::mat3 sc(1.0F);
  sc[0][0] = sx;
  sc[1][1] = sy;
  mCurrentState.transform = mCurrentState.transform * sc;
}

void Painter::ResetTransform() {
  if (mBatcher != nullptr) {
    mBatcher->Flush();
  }
  mCurrentState.transform = glm::mat3(1.0F);
}

void Painter::SetClipRect(const Rect& rect) {
  if (mRenderer == nullptr) {
    return;
  }
  if (mBatcher != nullptr) {
    mBatcher->Flush();
  }
  mCurrentState.clip_rect = rect;
  mCurrentState.has_clip = true;
  ApplyScissor(rect);
}

void Painter::ClearClip() {
  if (mRenderer == nullptr) {
    return;
  }
  if (mBatcher != nullptr) {
    mBatcher->Flush();
  }
  mCurrentState.has_clip = false;
  mRenderer->ClearScissor();
}

void Painter::SetDrawableSize(int32_t width, int32_t height) {
  // Ilk acik bildirimden sonra otomatik yoklamayi kapat.
  mAutoDrawableSize = false;
  ApplyDrawableSize(width, height);
}

void Painter::ApplyDrawableSize(int32_t width, int32_t height) {
  if (mRenderer == nullptr) {
    return;
  }
  if (width == mViewportWidth && height == mViewportHeight) {
    return;
  }
  mViewportWidth = width;
  mViewportHeight = height;
  mRenderer->SetViewport(0, 0, width, height);
  UpdateProjection();
}

void Painter::QueryDrawableSize(int32_t& out_width, int32_t& out_height) const {
  out_width = 0;
  out_height = 0;
  if (mWindow == nullptr) {
    return;
  }
  // Framebuffer (piksel) boyutu — mantıksal pencere boyutu DEĞİL. glViewport
  // ve vkCmdSetViewport ikisi de piksel bekler; HiDPI ölçeklemede bu ikisi
  // ayrışır ve mantıksal boyut kullanmak çizimi ekranın bir köşesine sıkıştırır.
  int w = 0;
  int h = 0;
  SDL_GetWindowSizeInPixels(mWindow, &w, &h);
  out_width = w;
  out_height = h;
}

void Painter::UpdateProjection() {
  if (mRenderer == nullptr) {
    return;
  }
  // Sıfır boyutlu viewport (simge durumu) projeksiyonu NaN yapar; bu durumda
  // matrisi olduğu gibi bırak — kare zaten çizilmeyecek.
  if (mViewportWidth <= 0 || mViewportHeight <= 0) {
    return;
  }
  if (mBatcher != nullptr) {
    mBatcher->Flush();
  }
  // Ortografik projeksiyon: [0, width] x [0, height] → NDC
  // OpenGL: Y ekseni clip space'de yukarı pozitif → Y'yi ters çevir (-2/h).
  // Vulkan: Y ekseni clip space'de aşağı pozitif → ters çevirme gerekmez (+2/h).
  auto w = static_cast<float>(mViewportWidth);
  auto h = static_cast<float>(mViewportHeight);

  const bool kIsVulkan = (mRenderer->GetBackend() == RendererBackend::kVulkan);
  const float kSy = kIsVulkan ? (2.0F / h) : (-2.0F / h);
  const float kTy = kIsVulkan ? -1.0F : 1.0F;

  // 4x4 sütun-major ortografik matris
  // clang-format off
  std::array<float, 16> mat = {
      2.0F / w,  0.0F,  0.0F, 0.0F,
      0.0F,      kSy,   0.0F, 0.0F,
      0.0F,      0.0F, -1.0F, 0.0F,
     -1.0F,      kTy,   0.0F, 1.0F,
  };
  // clang-format on
  mRenderer->SetProjectionMatrix(mat.data());
}

void Painter::FlushTransform() {
  if (mRenderer == nullptr) {
    return;
  }
  mRenderer->SetModelMatrix(glm::value_ptr(mCurrentState.transform));
}

void Painter::ApplyScissor(const Rect& rect) {
  // OpenGL scissor Y=0 altta; Vulkan Y=0 üstte.
  const bool kIsVulkan = (mRenderer->GetBackend() == RendererBackend::kVulkan);
  const int32_t kScissorY =
      kIsVulkan ? static_cast<int32_t>(rect.y)
                : (mViewportHeight - static_cast<int32_t>(rect.y) -
                   static_cast<int32_t>(rect.h));
  mRenderer->SetScissor(static_cast<int32_t>(rect.x), kScissorY,
                        static_cast<int32_t>(rect.w),
                        static_cast<int32_t>(rect.h));
}

}  // namespace sdl_painter
