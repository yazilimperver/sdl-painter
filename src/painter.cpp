#include "sdl_painter/painter.h"

#include "sdl_painter/image.h"
#include "sdl_painter/renderer.h"
#include "sdl_painter/texture.h"
#include "sdl_painter/vertex.h"

#include <SDL3/SDL.h>

#include <array>
#include <cmath>
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

/// @brief Metni `\n` (ve `\r\n`) sinirlarindan satirlara ayir.
///
/// Bos satirlar KORUNUR: iki ardisik `\n` bir bosluk satiri demektir ve
/// yerlesimde yer kaplamalidir.
std::vector<std::string> SplitLines(const std::string& text) {
  std::vector<std::string> lines;
  std::string current;
  for (char c : text) {
    if (c == '\n') {
      lines.push_back(current);
      current.clear();
    } else if (c != '\r') {
      current.push_back(c);
    }
  }
  lines.push_back(current);
  return lines;
}

/// @brief Tek bir satiri, genisligi asmayacak parcalara bol.
///
/// Once sozcuk sinirlarindan denenir; tek basina sigmayan bir sozcuk
/// karakter sinirindan bolunur. Bolme daima UTF-8 kod noktasi sinirinda
/// yapilir — aksi halde bozuk bir dizi olusur ve cozumleyici U+FFFD uretir.
void WrapLine(const Font& font, const std::string& line, float max_width,
              std::vector<std::string>& out) {
  if (line.empty() || !(max_width > 0.0F)) {
    out.push_back(line);
    return;
  }

  auto fits = [&font, max_width](const std::string& s) {
    if (s.empty()) {
      return true;
    }
    int32_t w = 0;
    int32_t h = 0;
    if (!font.MeasureText(s, w, h)) {
      return true;  // Olculemiyorsa bolmeye calisma.
    }
    return static_cast<float>(w) <= max_width;
  };

  // Sozcugu karakter sinirindan bol (tek basina sigmiyorsa).
  auto hard_split = [&fits](const std::string& word,
                            std::vector<std::string>& dst) {
    std::string piece;
    for (std::size_t i = 0; i < word.size();) {
      std::size_t advance = 0;
      detail::DecodeUTF8(word.c_str() + i, word.size() - i, advance);
      const std::string next = piece + word.substr(i, advance);
      if (!piece.empty() && !fits(next)) {
        dst.push_back(piece);
        piece.clear();
        continue;  // Ayni karakteri yeni parcada yeniden dene.
      }
      piece = next;
      i += advance;
    }
    if (!piece.empty()) {
      dst.push_back(piece);
    }
  };

  std::string current;
  std::size_t pos = 0;
  while (pos <= line.size()) {
    const std::size_t space = line.find(' ', pos);
    const std::string word = line.substr(
        pos, space == std::string::npos ? std::string::npos : space - pos);

    const std::string candidate = current.empty() ? word : current + " " + word;
    if (fits(candidate)) {
      current = candidate;
    } else if (current.empty()) {
      // Sozcuk tek basina sigmiyor.
      hard_split(word, out);
      current.clear();
      if (!out.empty()) {
        current = out.back();
        out.pop_back();
      }
    } else {
      out.push_back(current);
      current = word;
      if (!fits(current)) {
        hard_split(current, out);
        current.clear();
        if (!out.empty()) {
          current = out.back();
          out.pop_back();
        }
      }
    }

    if (space == std::string::npos) {
      break;
    }
    pos = space + 1;
  }
  out.push_back(current);
}

/// @brief Metni, cizilecek nihai satirlara donustur.
std::vector<std::string> LayoutLines(const Font& font, const std::string& text,
                                     float max_width, TextWrap wrap) {
  const std::vector<std::string> raw = SplitLines(text);
  if (wrap == TextWrap::kNone) {
    return raw;
  }
  std::vector<std::string> out;
  out.reserve(raw.size());
  for (const auto& line : raw) {
    WrapLine(font, line, max_width, out);
  }
  return out;
}

/// @brief Iki rengi t oraninda karistir (t [0,1]'e kirpilir).
Color LerpColor(const Color& a, const Color& b, float t) {
  const float k = t < 0.0F ? 0.0F : (t > 1.0F ? 1.0F : t);
  auto mix = [k](uint8_t lo, uint8_t hi) {
    return static_cast<uint8_t>(
        static_cast<float>(lo) +
        (static_cast<float>(hi) - static_cast<float>(lo)) * k);
  };
  return Color{mix(a.r, b.r), mix(a.g, b.g), mix(a.b, b.b), mix(a.a, b.a)};
}

/// @brief Vertex renklerini fircaya gore yaz.
///
/// Gradient burada, tessellation SONRASI ama transform ONCESI uygulanir:
/// vertex konumlari hala sekil-yerel oldugu icin gradient koordinatlari da
/// cizim koordinatlariyla ayni uzayda kalir.
void ApplyBrushColors(std::vector<Vertex>& verts, const Brush& brush) {
  if (brush.GetType() == BrushType::kLinear) {
    const Point s = brush.GetStart();
    const Point e = brush.GetEnd();
    const float dx = e.x - s.x;
    const float dy = e.y - s.y;
    const float len_sq = dx * dx + dy * dy;
    if (len_sq < 1e-12F) {
      // Sifir uzunluklu gradient: duz baslangic rengi (sifira bolme yok).
      for (auto& v : verts) {
        const Color c = brush.GetColor();
        v.r = c.r;
        v.g = c.g;
        v.b = c.b;
        v.a = c.a;
      }
      return;
    }
    for (auto& v : verts) {
      // Noktanin gradient ekseni uzerindeki izdusumu.
      const float t = ((v.x - s.x) * dx + (v.y - s.y) * dy) / len_sq;
      const Color c = LerpColor(brush.GetColor(), brush.GetColor2(), t);
      v.r = c.r;
      v.g = c.g;
      v.b = c.b;
      v.a = c.a;
    }
    return;
  }

  if (brush.GetType() == BrushType::kRadial) {
    const Point c0 = brush.GetStart();
    const float r = brush.GetRadius();
    for (auto& v : verts) {
      const float dx = v.x - c0.x;
      const float dy = v.y - c0.y;
      const float t = (r > 0.0F) ? (std::sqrt(dx * dx + dy * dy) / r) : 0.0F;
      const Color c = LerpColor(brush.GetColor(), brush.GetColor2(), t);
      v.r = c.r;
      v.g = c.g;
      v.b = c.b;
      v.a = c.a;
    }
    return;
  }

  const Color c = brush.GetColor();
  for (auto& v : verts) {
    v.r = c.r;
    v.g = c.g;
    v.b = c.b;
    v.a = c.a;
  }
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
  QueryDrawableSize(mDrawableWidth, mDrawableHeight);
  mViewportWidth = mDrawableWidth;
  mViewportHeight = mDrawableHeight;
  mRenderer->SetViewport(0, 0, mViewportWidth, mViewportHeight);
  UpdateProjection();
}

Painter::Painter(std::unique_ptr<IRenderer> renderer, int32_t viewport_width,
                 int32_t viewport_height)
    : mRenderer(std::move(renderer)),
      mDrawableWidth(viewport_width),
      mDrawableHeight(viewport_height),
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
      mDrawableWidth(other.mDrawableWidth),
      mDrawableHeight(other.mDrawableHeight),
      mViewportX(other.mViewportX),
      mViewportY(other.mViewportY),
      mViewportWidth(other.mViewportWidth),
      mViewportHeight(other.mViewportHeight),
      mCustomViewport(other.mCustomViewport) {
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
    mDrawableWidth = other.mDrawableWidth;
    mDrawableHeight = other.mDrawableHeight;
    mViewportX = other.mViewportX;
    mViewportY = other.mViewportY;
    mViewportWidth = other.mViewportWidth;
    mViewportHeight = other.mViewportHeight;
    mCustomViewport = other.mCustomViewport;
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

void Painter::SetViewport(int32_t x, int32_t y, int32_t width, int32_t height) {
  if (mRenderer == nullptr || width <= 0 || height <= 0) {
    return;
  }
  if (mCustomViewport && x == mViewportX && y == mViewportY &&
      width == mViewportWidth && height == mViewportHeight) {
    return;
  }
  // Viewport bir GPU durumu: biriken cizimler eski viewport'a aitti.
  if (mBatcher != nullptr) {
    mBatcher->Flush();
  }
  mCustomViewport = true;
  mViewportX = x;
  mViewportY = y;
  mViewportWidth = width;
  mViewportHeight = height;

  // OpenGL viewport'unun orijini sol ALT kosededir; Vulkan'da sol ust.
  const bool kIsVulkan = (mRenderer->GetBackend() == RendererBackend::kVulkan);
  const int32_t kGpuY = kIsVulkan ? y : (mDrawableHeight - y - height);
  mRenderer->SetViewport(x, kGpuY, width, height);
  ++mStats.state_changes;
  UpdateProjection();
}

void Painter::ResetViewport() {
  if (mRenderer == nullptr || !mCustomViewport) {
    return;
  }
  if (mBatcher != nullptr) {
    mBatcher->Flush();
  }
  mCustomViewport = false;
  mViewportX = 0;
  mViewportY = 0;
  mViewportWidth = mDrawableWidth;
  mViewportHeight = mDrawableHeight;
  mRenderer->SetViewport(0, 0, mViewportWidth, mViewportHeight);
  ++mStats.state_changes;
  UpdateProjection();
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
void Painter::SetBlendMode(BlendMode mode) {
  mCurrentState.blend_mode = mode;
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
    PushStroke(outline_verts, pen.GetOutlineColor());
  }
  auto verts = StrokeOpenPath({{x1, y1}, {x2, y2}}, pen.GetWidth(), pen);
  PushStroke(verts, pen.GetColor());
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
    PushStroke(outline_verts, pen.GetOutlineColor());
  }
  auto verts = Tessellator::TessellateStrokedRect(
      x, y, w, h, pen.GetWidth(), pen.GetJoinStyle(), DashOf(pen),
      pen.GetDashCount(), pen.GetCapStyle());
  PushStroke(verts, pen.GetColor());
}

void Painter::FillRect(float x, float y, float w, float h) {
  if (!CanDrawBrush()) {
    return;
  }
  auto verts = Tessellator::TessellateFilledRect(x, y, w, h);
  PushFilled(verts);
}

void Painter::DrawRoundedRect(float x, float y, float w, float h,
                              float radius) {
  if (!CanDrawPen()) {
    return;
  }
  const std::vector<Point> pts =
      Tessellator::BuildRoundedRectPoints(x, y, w, h, radius);
  if (pts.size() < 3) {
    return;
  }
  const Pen& pen = mCurrentState.pen;
  if (pen.HasOutline()) {
    auto outline_verts = StrokeClosedPath(
        pts, pen.GetWidth() + 2.0F * pen.GetOutlineWidth(), pen);
    PushStroke(outline_verts, pen.GetOutlineColor());
  }
  auto verts = StrokeClosedPath(pts, pen.GetWidth(), pen);
  PushStroke(verts, pen.GetColor());
}

void Painter::FillRoundedRect(float x, float y, float w, float h,
                              float radius) {
  if (!CanDrawBrush()) {
    return;
  }
  const std::vector<Point> pts =
      Tessellator::BuildRoundedRectPoints(x, y, w, h, radius);
  if (pts.size() < 3) {
    return;
  }
  // Dis hat konveks oldugu icin ear clipping ek mantik gerektirmez.
  auto verts = Tessellator::TessellateFilledPolygon(pts);
  PushFilled(verts);
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
    PushStroke(outline_verts, pen.GetOutlineColor());
  }
  auto verts = Tessellator::TessellateStrokedCircle(
      cx, cy, radius, pen.GetWidth(), pen.GetJoinStyle(), DashOf(pen),
      pen.GetDashCount(), pen.GetCapStyle());
  PushStroke(verts, pen.GetColor());
}

void Painter::FillCircle(float cx, float cy, float radius) {
  if (!CanDrawBrush()) {
    return;
  }
  auto verts = Tessellator::TessellateFilledCircle(cx, cy, radius);
  PushFilled(verts);
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
    PushStroke(outline_verts, pen.GetOutlineColor());
  }
  auto verts = Tessellator::TessellateStrokedEllipse(
      cx, cy, rx, ry, pen.GetWidth(), pen.GetJoinStyle(), DashOf(pen),
      pen.GetDashCount(), pen.GetCapStyle());
  PushStroke(verts, pen.GetColor());
}

void Painter::FillEllipse(float cx, float cy, float rx, float ry) {
  if (!CanDrawBrush()) {
    return;
  }
  auto verts = Tessellator::TessellateFilledEllipse(cx, cy, rx, ry);
  PushFilled(verts);
}

void Painter::DrawPolygon(const std::vector<Point>& points) {
  if (!CanDrawPen()) {
    return;
  }
  const Pen& pen = mCurrentState.pen;
  if (pen.HasOutline()) {
    auto outline_verts = StrokeClosedPath(
        points, pen.GetWidth() + 2.0F * pen.GetOutlineWidth(), pen);
    PushStroke(outline_verts, pen.GetOutlineColor());
  }
  auto verts = StrokeClosedPath(points, pen.GetWidth(), pen);
  PushStroke(verts, pen.GetColor());
}

void Painter::FillPolygon(const std::vector<Point>& points) {
  if (!CanDrawBrush()) {
    return;
  }
  auto verts = Tessellator::TessellateFilledPolygon(points);
  PushFilled(verts);
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
    PushStroke(outline_verts, pen.GetOutlineColor());
  }
  auto verts = StrokeOpenPath(arc, pen.GetWidth(), pen);
  PushStroke(verts, pen.GetColor());
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
    PushStroke(outline_verts, pen.GetOutlineColor());
  }
  auto verts = StrokeClosedPath(outline, pen.GetWidth(), pen);
  PushStroke(verts, pen.GetColor());
}

void Painter::FillPie(float cx, float cy, float rx, float ry,
                      float start_degrees, float sweep_degrees) {
  if (!CanDrawBrush()) {
    return;
  }
  auto verts = Tessellator::TessellateFilledPie(cx, cy, rx, ry, start_degrees,
                                                sweep_degrees);
  PushFilled(verts);
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
    PushStroke(outline_verts, pen.GetOutlineColor());
  }
  auto verts = StrokeClosedPath(arc, pen.GetWidth(), pen);
  PushStroke(verts, pen.GetColor());
}

void Painter::FillChord(float cx, float cy, float rx, float ry,
                        float start_degrees, float sweep_degrees) {
  if (!CanDrawBrush()) {
    return;
  }
  auto verts = Tessellator::TessellateFilledChord(cx, cy, rx, ry, start_degrees,
                                                  sweep_degrees);
  PushFilled(verts);
}

void Painter::DrawPolyline(const std::vector<Point>& points) {
  if (!CanDrawPen()) {
    return;
  }
  const Pen& pen = mCurrentState.pen;
  if (pen.HasOutline()) {
    auto outline_verts = StrokeOpenPath(
        points, pen.GetWidth() + 2.0F * pen.GetOutlineWidth(), pen);
    PushStroke(outline_verts, pen.GetOutlineColor());
  }
  auto verts = StrokeOpenPath(points, pen.GetWidth(), pen);
  PushStroke(verts, pen.GetColor());
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

void Painter::DrawImageMesh(const Image& image, int32_t cols, int32_t rows,
                            const std::vector<Point>& points,
                            const Color& tint) {
  if (mRenderer == nullptr || mBatcher == nullptr || !image.IsValid()) {
    return;
  }
  if (cols <= 0 || rows <= 0) {
    return;
  }

  const auto kExpected =
      static_cast<std::size_t>(cols + 1) * static_cast<std::size_t>(rows + 1);
  if (points.size() != kExpected) {
    // Sessizce yanlis geometri cizmektense acik hata: bu, cagiranin izgara
    // boyutuyla nokta sayisini karistirdigi en olasi hata.
    spdlog::error(
        "Painter::DrawImageMesh: {} nokta bekleniyordu ({}x{} izgara), {} "
        "verildi.",
        kExpected, cols, rows, points.size());
    return;
  }

  const TextureHandle handle = image.Upload(*mRenderer);
  if (handle == kInvalidTexture) {
    return;
  }

  const auto stride = static_cast<std::size_t>(cols) + 1;
  const float inv_cols = 1.0F / static_cast<float>(cols);
  const float inv_rows = 1.0F / static_cast<float>(rows);

  std::vector<TexturedVertex> verts;
  verts.reserve(static_cast<std::size_t>(cols) *
                static_cast<std::size_t>(rows) * 6);

  for (int32_t r = 0; r < rows; ++r) {
    for (int32_t c = 0; c < cols; ++c) {
      const std::size_t i0 =
          static_cast<std::size_t>(r) * stride + static_cast<std::size_t>(c);
      const std::size_t i1 = i0 + 1;
      const std::size_t i2 = i0 + stride;
      const std::size_t i3 = i2 + 1;

      const float u0 = static_cast<float>(c) * inv_cols;
      const float u1 = static_cast<float>(c + 1) * inv_cols;
      const float v0 = static_cast<float>(r) * inv_rows;
      const float v1 = static_cast<float>(r + 1) * inv_rows;

      // Iki ucgen, DrawImage ile ayni sarim.
      verts.push_back({points[i0].x, points[i0].y, u0, v0});
      verts.push_back({points[i1].x, points[i1].y, u1, v0});
      verts.push_back({points[i3].x, points[i3].y, u1, v1});
      verts.push_back({points[i0].x, points[i0].y, u0, v0});
      verts.push_back({points[i3].x, points[i3].y, u1, v1});
      verts.push_back({points[i2].x, points[i2].y, u0, v1});
    }
  }

  mBatcher->SetBlendMode(mCurrentState.blend_mode);
  mBatcher->PushTexturedTriangles(verts, mCurrentState.transform, handle, tint,
                                  mCurrentState.opacity);
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

  mBatcher->SetBlendMode(mCurrentState.blend_mode);
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

  // Tek satirlik yaygin durumda arama/tahsis yapma.
  if (text.find('\n') == std::string::npos) {
    DrawTextLine(x, y, text);
    return;
  }

  const auto line_height = static_cast<float>(mCurrentFont->LineHeight());
  float baseline = y;
  for (const auto& line : SplitLines(text)) {
    if (!line.empty()) {
      DrawTextLine(x, baseline, line);
    }
    baseline += line_height;
  }
}

void Painter::DrawTextLine(float x, float y, const std::string& text) {
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

    mBatcher->SetBlendMode(mCurrentState.blend_mode);
    mBatcher->PushTexturedTriangles(kVerts, mCurrentState.transform,
                                    glyph->texture, tint,
                                    mCurrentState.opacity);

    current_x += static_cast<float>(glyph->advance);
  }
}

void Painter::DrawText(const Rect& rect, const std::string& text,
                       Alignment alignment, TextWrap wrap) {
  if (mRenderer == nullptr || mCurrentFont == nullptr ||
      !mCurrentFont->IsValid()) {
    return;
  }
  if (text.empty()) {
    return;
  }

  const std::vector<std::string> lines =
      LayoutLines(*mCurrentFont, text, rect.w, wrap);

  // Tek satir: eski davranis birebir korunur (ayni olcum, ayni ortalama).
  if (lines.size() == 1) {
    int32_t text_w = 0;
    int32_t text_h = 0;
    mCurrentFont->MeasureText(lines[0], text_w, text_h);

    const float kTopY = rect.y + (rect.h - static_cast<float>(text_h)) * 0.5F;
    DrawTextLine(AlignedX(rect, lines[0], alignment),
                 kTopY + static_cast<float>(mCurrentFont->Ascent()), lines[0]);
    return;
  }

  // Cok satir: blok yuksekligi satir sayisi * satir yuksekligi.
  const auto line_height = static_cast<float>(mCurrentFont->LineHeight());
  const float block_h = static_cast<float>(lines.size()) * line_height;
  const float top = rect.y + (rect.h - block_h) * 0.5F;

  float baseline = top + static_cast<float>(mCurrentFont->Ascent());
  for (const auto& line : lines) {
    if (!line.empty()) {
      DrawTextLine(AlignedX(rect, line, alignment), baseline, line);
    }
    baseline += line_height;
  }
}

std::size_t Painter::CountTextLines(const std::string& text, float max_width,
                                    TextWrap wrap) const {
  if (mCurrentFont == nullptr || !mCurrentFont->IsValid() || text.empty()) {
    return 0;
  }
  return LayoutLines(*mCurrentFont, text, max_width, wrap).size();
}

float Painter::AlignedX(const Rect& rect, const std::string& line,
                        Alignment alignment) const {
  if (alignment == Alignment::kLeft) {
    return rect.x;
  }
  int32_t w = 0;
  int32_t h = 0;
  mCurrentFont->MeasureText(line, w, h);
  if (alignment == Alignment::kCenter) {
    return rect.x + (rect.w - static_cast<float>(w)) * 0.5F;
  }
  return rect.x + rect.w - static_cast<float>(w);
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
  if (width == mDrawableWidth && height == mDrawableHeight) {
    return;
  }
  mDrawableWidth = width;
  mDrawableHeight = height;

  // Kullanici acik bir viewport sectiyse yeniden boyutlandirma onu ezmez;
  // kendi yerlesimini yeniden hesaplayip SetViewport'u tekrar cagirmasi
  // beklenir (bolunmus ekranda dogru olan davranis budur).
  if (mCustomViewport) {
    return;
  }
  mViewportX = 0;
  mViewportY = 0;
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

void Painter::PushStroke(const std::vector<Vertex>& verts, const Color& color) {
  if (verts.empty()) {
    return;
  }
  mBatcher->SetBlendMode(mCurrentState.blend_mode);
  mBatcher->PushTriangles(verts, mCurrentState.transform, color,
                          mCurrentState.opacity);
}

void Painter::PushFilled(std::vector<Vertex>& verts) {
  if (verts.empty()) {
    return;
  }
  mBatcher->SetBlendMode(mCurrentState.blend_mode);
  const Brush& brush = mCurrentState.brush;
  if (!brush.IsGradient()) {
    // Duz dolgu: renk batcher'da tek seferde yazilir, burada dolasmaya gerek
    // yok.
    mBatcher->PushTriangles(verts, mCurrentState.transform, brush.GetColor(),
                            mCurrentState.opacity);
    return;
  }
  ApplyBrushColors(verts, brush);
  mBatcher->PushTrianglesPreColored(verts, mCurrentState.transform,
                                    mCurrentState.opacity);
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
  // Kirpma dikdortgeni cagirana VIEWPORT-YEREL gelir (cizim koordinatlariyla
  // ayni sistem), ama scissor kutusu PENCERE koordinatindadir. Viewport
  // ozellestirilmisse ofset eklenmeli; aksi halde bolunmus ekranda kirpma
  // yanlis panele duser.
  const int32_t kWindowX = mViewportX + static_cast<int32_t>(rect.x);
  const int32_t kWindowY = mViewportY + static_cast<int32_t>(rect.y);

  // OpenGL scissor Y=0 altta; Vulkan Y=0 üstte. Ters cevirme YUZEYIN
  // tamamina gore yapilir, viewport'a gore degil.
  const bool kIsVulkan = (mRenderer->GetBackend() == RendererBackend::kVulkan);
  const int32_t kScissorY =
      kIsVulkan ? kWindowY
                : (mDrawableHeight - kWindowY - static_cast<int32_t>(rect.h));
  ++mStats.state_changes;
  mRenderer->SetScissor(kWindowX, kScissorY, static_cast<int32_t>(rect.w),
                        static_cast<int32_t>(rect.h));
}

}  // namespace sdl_painter
