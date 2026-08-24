#include "sdl_painter/painter.h"

#include "sdl_painter/image.h"
#include "sdl_painter/renderer.h"
#include "sdl_painter/texture.h"
#include "sdl_painter/vertex.h"

#include <SDL3/SDL.h>

#include <array>
#include <glm/gtc/type_ptr.hpp>
#include <spdlog/spdlog.h>
#include <utility>

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

namespace {

/// @brief Iki durumun kirpma (clip) ayari ayni mi?
///
/// Scissor box, batcher'in tasiyamadigi tek GPU durumu; Restore yalnizca
/// gercekten degistiginde flush etsin diye karsilastirilir.
bool ClipEquals(const RenderState& a, const RenderState& b) noexcept {
  if (a.has_clip != b.has_clip) {
    return false;
  }
  if (!a.has_clip) {
    return true;
  }
  return a.clip_rect.x == b.clip_rect.x && a.clip_rect.y == b.clip_rect.y &&
         a.clip_rect.w == b.clip_rect.w && a.clip_rect.h == b.clip_rect.h;
}

/// @brief Kalemin kesik deseni varsa desen isaretcisi, yoksa nullptr.
///
/// Tessellator "kesik yok"u nullptr ile anliyor; her cagri yerinde ayni
/// dallanmayi tekrarlamamak icin burada bir kez yazilir.
const float* DashOf(const Pen& pen) noexcept {
  return pen.HasDash() ? pen.GetDashPattern().data() : nullptr;
}

/// @brief Acik bir yolu kalemin stiline gore tessellate et.
std::vector<Vertex> StrokeOpenPath(const std::vector<Point>& points,
                                   float width, const Pen& pen) {
  if (pen.HasDash()) {
    return Tessellator::TessellateDashedPolyline(
        points, width, DashOf(pen), pen.GetDashCount(), /*closed=*/false,
        pen.GetCapStyle(), pen.GetJoinStyle());
  }
  return Tessellator::TessellateThickPolyline(points, width, pen.GetCapStyle(),
                                              pen.GetJoinStyle());
}

/// @brief Kapali bir yolu kalemin stiline gore tessellate et.
std::vector<Vertex> StrokeClosedPath(const std::vector<Point>& points,
                                     float width, const Pen& pen) {
  if (pen.HasDash()) {
    return Tessellator::TessellateDashedPolyline(
        points, width, DashOf(pen), pen.GetDashCount(), /*closed=*/true,
        pen.GetCapStyle(), pen.GetJoinStyle());
  }
  return Tessellator::TessellateStrokedPolygon(points, width,
                                               pen.GetJoinStyle());
}

}  // namespace

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
  mStats = FrameStats{};
  if (mBatcher != nullptr) {
    mBatcher->ResetCounters();
  }
  mFrameStartNs = SDL_GetTicksNS();

  mRenderer->BeginFrame();

  // Model matrisi artik CPU'da vertex'lere gomuluyor (bkz. RenderBatcher);
  // uniform daima birim kalir. Kare basina bir kez yazmak, ozel bir IRenderer
  // implementasyonunun eski bir matrisle kalmamasini garanti eder.
  static constexpr glm::mat3 kIdentity(1.0F);
  mRenderer->SetModelMatrix(glm::value_ptr(kIdentity));
  ++mStats.state_changes;
}

void Painter::End() {
  if (mRenderer == nullptr) {
    return;
  }
  if (mBatcher != nullptr) {
    mBatcher->Flush();
    mStats.draw_calls = mBatcher->DrawCallCount();
    mStats.batches = mBatcher->DrawCallCount();
    mStats.vertices = mBatcher->VertexCount();
  }

  // CPU suresi sunumdan ONCE olculur: SDL_GL_SwapWindow vsync'te bloklar ve
  // o bekleme cizim maliyeti degildir.
  mStats.cpu_frame_ms =
      static_cast<double>(SDL_GetTicksNS() - mFrameStartNs) / 1.0e6;

  mRenderer->EndFrame();

  // Timer query sonucu bir kare gecikmeli gelir (bkz. IRenderer).
  mStats.gpu_frame_ms = mRenderer->GetLastGpuFrameMs();
  mLastStats = mStats;
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
  // Opaklik uniform'unun tek sahibi RenderBatcher'dir: her Flush oncesi
  // o batch'in opakligini yazar. Burada ayrica yazmak, degeri bir sonraki
  // flush'ta nasil olsa ezilecek gereksiz bir GPU cagrisi olurdu.
  mCurrentState.opacity = alpha;
}

void Painter::DrawLine(float x1, float y1, float x2, float y2) {
  if (!CanDrawPen()) {
    return;
  }
  const Pen& pen = mCurrentState.pen;
  if (pen.HasOutline()) {
    auto outline_verts =
        StrokeOpenPath({{x1, y1}, {x2, y2}},
                       pen.GetWidth() + 2.0F * pen.GetOutlineWidth(), pen);
    mBatcher->PushTriangles(outline_verts, mCurrentState.transform,
                            pen.GetOutlineColor(), mCurrentState.opacity);
  }
  auto verts = StrokeOpenPath({{x1, y1}, {x2, y2}}, pen.GetWidth(), pen);
  mBatcher->PushTriangles(verts, mCurrentState.transform, pen.GetColor(),
                          mCurrentState.opacity);
}

void Painter::DrawRect(float x, float y, float w, float h) {
  if (!CanDrawPen()) {
    return;
  }
  const Pen& pen = mCurrentState.pen;
  if (pen.HasOutline()) {
    auto outline_verts = Tessellator::TessellateStrokedRect(
        x, y, w, h, pen.GetWidth() + 2.0F * pen.GetOutlineWidth(),
        pen.GetJoinStyle(), DashOf(pen), pen.GetDashCount(), pen.GetCapStyle());
    mBatcher->PushTriangles(outline_verts, mCurrentState.transform,
                            pen.GetOutlineColor(), mCurrentState.opacity);
  }
  auto verts = Tessellator::TessellateStrokedRect(
      x, y, w, h, pen.GetWidth(), pen.GetJoinStyle(), DashOf(pen),
      pen.GetDashCount(), pen.GetCapStyle());
  mBatcher->PushTriangles(verts, mCurrentState.transform, pen.GetColor(),
                          mCurrentState.opacity);
}

void Painter::FillRect(float x, float y, float w, float h) {
  if (!CanDrawBrush()) {
    return;
  }
  auto verts = Tessellator::TessellateFilledRect(x, y, w, h);
  mBatcher->PushTriangles(verts, mCurrentState.transform,
                          mCurrentState.brush.GetColor(),
                          mCurrentState.opacity);
}

void Painter::DrawCircle(float cx, float cy, float radius) {
  if (!CanDrawPen()) {
    return;
  }
  const Pen& pen = mCurrentState.pen;
  if (pen.HasOutline()) {
    auto outline_verts = Tessellator::TessellateStrokedCircle(
        cx, cy, radius, pen.GetWidth() + 2.0F * pen.GetOutlineWidth(),
        pen.GetJoinStyle(), DashOf(pen), pen.GetDashCount(), pen.GetCapStyle());
    mBatcher->PushTriangles(outline_verts, mCurrentState.transform,
                            pen.GetOutlineColor(), mCurrentState.opacity);
  }
  auto verts = Tessellator::TessellateStrokedCircle(
      cx, cy, radius, pen.GetWidth(), pen.GetJoinStyle(), DashOf(pen),
      pen.GetDashCount(), pen.GetCapStyle());
  mBatcher->PushTriangles(verts, mCurrentState.transform, pen.GetColor(),
                          mCurrentState.opacity);
}

void Painter::FillCircle(float cx, float cy, float radius) {
  if (!CanDrawBrush()) {
    return;
  }
  auto verts = Tessellator::TessellateFilledCircle(cx, cy, radius);
  mBatcher->PushTriangles(verts, mCurrentState.transform,
                          mCurrentState.brush.GetColor(),
                          mCurrentState.opacity);
}

void Painter::DrawEllipse(float cx, float cy, float rx, float ry) {
  if (!CanDrawPen()) {
    return;
  }
  const Pen& pen = mCurrentState.pen;
  if (pen.HasOutline()) {
    auto outline_verts = Tessellator::TessellateStrokedEllipse(
        cx, cy, rx, ry, pen.GetWidth() + 2.0F * pen.GetOutlineWidth(),
        pen.GetJoinStyle(), DashOf(pen), pen.GetDashCount(), pen.GetCapStyle());
    mBatcher->PushTriangles(outline_verts, mCurrentState.transform,
                            pen.GetOutlineColor(), mCurrentState.opacity);
  }
  auto verts = Tessellator::TessellateStrokedEllipse(
      cx, cy, rx, ry, pen.GetWidth(), pen.GetJoinStyle(), DashOf(pen),
      pen.GetDashCount(), pen.GetCapStyle());
  mBatcher->PushTriangles(verts, mCurrentState.transform, pen.GetColor(),
                          mCurrentState.opacity);
}

void Painter::FillEllipse(float cx, float cy, float rx, float ry) {
  if (!CanDrawBrush()) {
    return;
  }
  auto verts = Tessellator::TessellateFilledEllipse(cx, cy, rx, ry);
  mBatcher->PushTriangles(verts, mCurrentState.transform,
                          mCurrentState.brush.GetColor(),
                          mCurrentState.opacity);
}

void Painter::DrawPolygon(const std::vector<Point>& points) {
  if (!CanDrawPen()) {
    return;
  }
  const Pen& pen = mCurrentState.pen;
  if (pen.HasOutline()) {
    auto outline_verts = StrokeClosedPath(
        points, pen.GetWidth() + 2.0F * pen.GetOutlineWidth(), pen);
    mBatcher->PushTriangles(outline_verts, mCurrentState.transform,
                            pen.GetOutlineColor(), mCurrentState.opacity);
  }
  auto verts = StrokeClosedPath(points, pen.GetWidth(), pen);
  mBatcher->PushTriangles(verts, mCurrentState.transform, pen.GetColor(),
                          mCurrentState.opacity);
}

void Painter::FillPolygon(const std::vector<Point>& points) {
  if (!CanDrawBrush()) {
    return;
  }
  auto verts = Tessellator::TessellateFilledPolygon(points);
  mBatcher->PushTriangles(verts, mCurrentState.transform,
                          mCurrentState.brush.GetColor(),
                          mCurrentState.opacity);
}

void Painter::DrawArc(float cx, float cy, float rx, float ry,
                      float start_degrees, float sweep_degrees) {
  if (!CanDrawPen()) {
    return;
  }
  const std::vector<Point> arc =
      Tessellator::BuildArcPoints(cx, cy, rx, ry, start_degrees, sweep_degrees);
  if (arc.size() < 2) {
    return;
  }
  const Pen& pen = mCurrentState.pen;
  if (pen.HasOutline()) {
    auto outline_verts =
        StrokeOpenPath(arc, pen.GetWidth() + 2.0F * pen.GetOutlineWidth(), pen);
    mBatcher->PushTriangles(outline_verts, mCurrentState.transform,
                            pen.GetOutlineColor(), mCurrentState.opacity);
  }
  auto verts = StrokeOpenPath(arc, pen.GetWidth(), pen);
  mBatcher->PushTriangles(verts, mCurrentState.transform, pen.GetColor(),
                          mCurrentState.opacity);
}

void Painter::DrawPie(float cx, float cy, float rx, float ry,
                      float start_degrees, float sweep_degrees) {
  if (!CanDrawPen()) {
    return;
  }
  std::vector<Point> outline =
      Tessellator::BuildArcPoints(cx, cy, rx, ry, start_degrees, sweep_degrees);
  if (outline.size() < 2) {
    return;
  }
  // Dilimin cercevesi: yay + merkez. Kapali bir yol olarak cizilir ki iki
  // yaricap ile yay arasindaki koseler birlesim alsin.
  outline.emplace_back(cx, cy);

  const Pen& pen = mCurrentState.pen;
  if (pen.HasOutline()) {
    auto outline_verts = StrokeClosedPath(
        outline, pen.GetWidth() + 2.0F * pen.GetOutlineWidth(), pen);
    mBatcher->PushTriangles(outline_verts, mCurrentState.transform,
                            pen.GetOutlineColor(), mCurrentState.opacity);
  }
  auto verts = StrokeClosedPath(outline, pen.GetWidth(), pen);
  mBatcher->PushTriangles(verts, mCurrentState.transform, pen.GetColor(),
                          mCurrentState.opacity);
}

void Painter::FillPie(float cx, float cy, float rx, float ry,
                      float start_degrees, float sweep_degrees) {
  if (!CanDrawBrush()) {
    return;
  }
  auto verts = Tessellator::TessellateFilledPie(cx, cy, rx, ry, start_degrees,
                                                sweep_degrees);
  mBatcher->PushTriangles(verts, mCurrentState.transform,
                          mCurrentState.brush.GetColor(),
                          mCurrentState.opacity);
}

void Painter::DrawChord(float cx, float cy, float rx, float ry,
                        float start_degrees, float sweep_degrees) {
  if (!CanDrawPen()) {
    return;
  }
  const std::vector<Point> arc =
      Tessellator::BuildArcPoints(cx, cy, rx, ry, start_degrees, sweep_degrees);
  if (arc.size() < 2) {
    return;
  }
  // Kiris: yayin kendisi kapali bir yol olarak cizilir; kapanis dogrusu iki
  // uc noktayi birlestirir.
  const Pen& pen = mCurrentState.pen;
  if (pen.HasOutline()) {
    auto outline_verts = StrokeClosedPath(
        arc, pen.GetWidth() + 2.0F * pen.GetOutlineWidth(), pen);
    mBatcher->PushTriangles(outline_verts, mCurrentState.transform,
                            pen.GetOutlineColor(), mCurrentState.opacity);
  }
  auto verts = StrokeClosedPath(arc, pen.GetWidth(), pen);
  mBatcher->PushTriangles(verts, mCurrentState.transform, pen.GetColor(),
                          mCurrentState.opacity);
}

void Painter::FillChord(float cx, float cy, float rx, float ry,
                        float start_degrees, float sweep_degrees) {
  if (!CanDrawBrush()) {
    return;
  }
  auto verts = Tessellator::TessellateFilledChord(cx, cy, rx, ry, start_degrees,
                                                  sweep_degrees);
  mBatcher->PushTriangles(verts, mCurrentState.transform,
                          mCurrentState.brush.GetColor(),
                          mCurrentState.opacity);
}

void Painter::DrawPolyline(const std::vector<Point>& points) {
  if (!CanDrawPen()) {
    return;
  }
  const Pen& pen = mCurrentState.pen;
  if (pen.HasOutline()) {
    auto outline_verts = StrokeOpenPath(
        points, pen.GetWidth() + 2.0F * pen.GetOutlineWidth(), pen);
    mBatcher->PushTriangles(outline_verts, mCurrentState.transform,
                            pen.GetOutlineColor(), mCurrentState.opacity);
  }
  auto verts = StrokeOpenPath(points, pen.GetWidth(), pen);
  mBatcher->PushTriangles(verts, mCurrentState.transform, pen.GetColor(),
                          mCurrentState.opacity);
}

void Painter::DrawImage(const Image& image, float x, float y, const Color& tint,
                        ImageFlip flip) {
  DrawImage(image,
            Rect{0.0F, 0.0F, static_cast<float>(image.Width()),
                 static_cast<float>(image.Height())},
            Rect{x, y, static_cast<float>(image.Width()),
                 static_cast<float>(image.Height())},
            tint, flip);
}

void Painter::DrawImage(const Image& image, const Rect& dest_rect,
                        const Color& tint, ImageFlip flip) {
  DrawImage(image,
            Rect{0.0F, 0.0F, static_cast<float>(image.Width()),
                 static_cast<float>(image.Height())},
            dest_rect, tint, flip);
}

void Painter::UpdateImage(const Image& image, const uint8_t* rgba) {
  if (mRenderer == nullptr || !image.IsValid() || rgba == nullptr) {
    return;
  }
  if (image.Channels() != 4) {
    spdlog::error(
        "Painter::UpdateImage: yalnizca 4 kanalli (RGBA8) goruntu "
        "guncellenebilir, bu goruntu {} kanalli.",
        image.Channels());
    return;
  }

  const TextureHandle handle = image.Upload(*mRenderer);
  if (handle == kInvalidTexture) {
    return;
  }

  // Doku ANINDA degisir ama cizimler biriktiriliyor. Flush edilmezse, bu
  // karede daha once bu dokudan yapilmis ve henuz gonderilmemis cizimler
  // geriye donuk olarak YENI icerikle cizilirdi.
  if (mBatcher != nullptr) {
    mBatcher->Flush();
  }
  mRenderer->UpdateTexture(handle, 0, 0, image.Width(), image.Height(), rgba);
}

void Painter::DrawImage(const Image& image, const Rect& src_rect,
                        const Rect& dest_rect, const Color& tint,
                        ImageFlip flip) {
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
  float u0 = src_rect.x / kImgW;
  float v0 = src_rect.y / kImgH;
  float u1 = (src_rect.x + src_rect.w) / kImgW;
  float v1 = (src_rect.y + src_rect.h) / kImgH;

  // Aynalama: hedef dikdortgen yerinde kalir, yalnizca UV ucalari takas edilir.
  if (flip == ImageFlip::kHorizontal || flip == ImageFlip::kBoth) {
    std::swap(u0, u1);
  }
  if (flip == ImageFlip::kVertical || flip == ImageFlip::kBoth) {
    std::swap(v0, v1);
  }

  // dest_rect → ekran koordinatlari
  const float kX0 = dest_rect.x;
  const float kY0 = dest_rect.y;
  const float kX1 = dest_rect.x + dest_rect.w;
  const float kY1 = dest_rect.y + dest_rect.h;

  // Iki ucgenden olusan quad (CCW)
  const std::vector<TexturedVertex> kVerts = {
      {kX0, kY0, u0, v0}, {kX1, kY0, u1, v0}, {kX1, kY1, u1, v1},
      {kX0, kY0, u0, v0}, {kX1, kY1, u1, v1}, {kX0, kY1, u0, v1},
  };

  mBatcher->PushTexturedTriangles(kVerts, mCurrentState.transform, handle, tint,
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

    mBatcher->PushTexturedTriangles(kVerts, mCurrentState.transform,
                                    glyph->texture, tint,
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
  // Flush yok: kaydedilen hicbir sey GPU durumu degil. Transform vertex'e
  // gomuluyor, opaklik ve renk batcher'da tasiniyor; clip zaten degismiyor.
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
  const RenderState previous = mCurrentState;
  mCurrentState = mStateStack.back();
  mStateStack.pop_back();

  // Yalnizca clip GPU durumudur (scissor box) ve yalnizca gercekten
  // degistiyse flush + yeniden uygulama gerektirir. Opaklik ve transform
  // batcher tarafindan tasiniyor, burada bir sey yapmaya gerek yok.
  if (!ClipEquals(previous, mCurrentState)) {
    if (mBatcher != nullptr) {
      mBatcher->Flush();
    }
    if (mCurrentState.has_clip) {
      ApplyScissor(mCurrentState.clip_rect);
    } else {
      ClearScissorCounted();
    }
  }
}

void Painter::Translate(float dx, float dy) {
  // Sağdan çarp (post-multiply)
  // glm::mat3 column-major → çeviri sütun 2'de: t[2][0]=dx, t[2][1]=dy.
  glm::mat3 t(1.0F);
  t[2][0] = dx;
  t[2][1] = dy;
  mCurrentState.transform = mCurrentState.transform * t;
}

void Painter::Rotate(float angle_degrees) {
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
  glm::mat3 sc(1.0F);
  sc[0][0] = sx;
  sc[1][1] = sy;
  mCurrentState.transform = mCurrentState.transform * sc;
}

void Painter::ResetTransform() {
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
  ClearScissorCounted();
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
  ++mStats.state_changes;
}

void Painter::ClearScissorCounted() {
  if (mRenderer == nullptr) {
    return;
  }
  mRenderer->ClearScissor();
  ++mStats.state_changes;
}

void Painter::ApplyScissor(const Rect& rect) {
  // OpenGL scissor Y=0 altta; Vulkan Y=0 üstte.
  const bool kIsVulkan = (mRenderer->GetBackend() == RendererBackend::kVulkan);
  const int32_t kScissorY =
      kIsVulkan ? static_cast<int32_t>(rect.y)
                : (mViewportHeight - static_cast<int32_t>(rect.y) -
                   static_cast<int32_t>(rect.h));
  ++mStats.state_changes;
  mRenderer->SetScissor(static_cast<int32_t>(rect.x), kScissorY,
                        static_cast<int32_t>(rect.w),
                        static_cast<int32_t>(rect.h));
}

}  // namespace sdl_painter
