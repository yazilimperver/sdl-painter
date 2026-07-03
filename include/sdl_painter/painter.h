#pragma once

#include "sdl_painter/brush.h"
#include "sdl_painter/color.h"
#include "sdl_painter/font.h"
#include "sdl_painter/geometry.h"
#include "sdl_painter/pen.h"
#include "sdl_painter/renderer.h"

#include <cstdint>
#include <glm/glm.hpp>
#include <memory>
#include <string>
#include <vector>

// SDL forward declaration
struct SDL_Window;

// Windows.h DrawText makrosu (DrawTextA/DrawTextW) ile ad çakışmasını önle.
#ifdef DrawText
#undef DrawText
#endif

namespace sdl_painter {

class Image;
class IRenderer;
class RenderBatcher;

/// @brief Aktif render durumu — transform stack elemanı.
struct RenderState {
  glm::mat3 transform{1.0F};  ///< 3x3 affine dönüşüm (column-major, birim).
  Pen pen;
  Brush brush;
  float opacity{1.0F};
  Rect clip_rect;
  bool has_clip{false};
};

/// @brief Ana çizim sınıfı — QPainter'a benzer API.
///
/// Painter, SDL penceresi üzerinde 2D çizim yapar. Backend (OpenGL/Vulkan)
/// IRenderer arayüzü üzerinden soyutlanır.
///
/// @warning **Yaşam döngüsü sözleşmesi:** Painter'a texture yükleyen tüm
/// @ref Image ve @ref Font nesneleri, Painter yıkılmadan **önce**
/// yıkılmalıdır. Image veya Font'u global / `static` ya da daha uzun
/// yaşayan bir konuma yerleştirmek tanımsız davranışa yol açar — yıkım
/// sırasında dangling IRenderer pointer kullanılır. v0.2.0'da bu sözleşme
/// `weak_ptr<IRenderer>` veya benzeri bir mekanizma ile zorunlu kılınacaktır.
class Painter {
 public:
  /// @brief Belirtilen pencere ve backend ile Painter oluştur.
  Painter(SDL_Window* window, RendererBackend backend);
  ~Painter();

  // Non-copyable, movable
  Painter(const Painter&) = delete;
  Painter& operator=(const Painter&) = delete;
  Painter(Painter&& other) noexcept;
  Painter& operator=(Painter&& other) noexcept;

  /// @brief Renderer başarıyla başlatıldı mı?
  [[nodiscard]] bool IsValid() const noexcept { return mRenderer != nullptr; }

  // --- Yaşam döngüsü ---

  /// @brief Frame başlangıcı — her frame başında çağrılmalı.
  void Begin();

  /// @brief Frame sonu — her frame sonunda çağrılmalı, ekrana sunar.
  void End();

  // --- Temizlik ---

  /// @brief Ekranı belirtilen renkle temizle.
  void Clear(const Color& color);

  // --- Stil ---

  /// @brief Aktif kalemi ayarla (çizgi rengi ve kalınlığı).
  void SetPen(const Pen& pen);

  /// @brief Aktif fırçayı ayarla (dolgu rengi).
  void SetBrush(const Brush& brush);

  /// @brief Aktif fontu ayarla (Phase 4).
  void SetFont(std::shared_ptr<Font> font);

  /// @brief Global opaklığı ayarla [0.0, 1.0].
  void SetOpacity(float alpha);

  // --- Primitifler ---

  /// @brief İki nokta arasına çizgi çiz.
  void DrawLine(float x1, float y1, float x2, float y2);

  /// @brief Dikdörtgen çerçeve çiz (dolgu yok).
  void DrawRect(float x, float y, float w, float h);

  /// @brief Dikdörtgeni doldur.
  void FillRect(float x, float y, float w, float h);

  /// @brief Daire çerçeve çiz.
  void DrawCircle(float cx, float cy, float radius);

  /// @brief Daireyi doldur.
  void FillCircle(float cx, float cy, float radius);

  /// @brief Elips çerçeve çiz.
  void DrawEllipse(float cx, float cy, float rx, float ry);

  /// @brief Elipsi doldur.
  void FillEllipse(float cx, float cy, float rx, float ry);

  /// @brief Çok kenarlı kapalı şeklin çerçevesini çiz.
  void DrawPolygon(const std::vector<Point>& points);

  /// @brief Çok kenarlıyı doldur (ear clipping tessellation).
  void FillPolygon(const std::vector<Point>& points);

  /// @brief Açık çizgi dizisi çiz.
  void DrawPolyline(const std::vector<Point>& points);

  // --- Görüntü ---

  /// @brief Görüntüyü orijinal boyutuyla çiz.
  void DrawImage(const Image& image, float x, float y);

  /// @brief Görüntüyü hedef dikdörtgene ölçekleyerek çiz.
  void DrawImage(const Image& image, const Rect& dest_rect);

  /// @brief Görüntünün kaynak dikdörtgenini hedef dikdörtgene çiz.
  void DrawImage(const Image& image, const Rect& src_rect,
                 const Rect& dest_rect);

  // --- Metin (Phase 4) ---

  /// @brief Noktaya metin çiz.
  void DrawText(float x, float y, const std::string& text);

  /// @brief Dikdörtgen içine hizalanmış metin çiz.
  void DrawText(const Rect& rect, const std::string& text,
                Alignment alignment = Alignment::kLeft);

  // --- Transform stack ---

  /// @brief Mevcut render durumunu stack'e kaydet.
  void Save();

  /// @brief Son kaydedilen durumu geri yükle.
  void Restore();

  /// @brief Öteleme uygula.
  void Translate(float dx, float dy);

  /// @brief Döndürme uygula (derece).
  void Rotate(float angle_degrees);

  /// @brief Ölçekleme uygula.
  void Scale(float sx, float sy);

  /// @brief Transform'u birim matrise sıfırla.
  void ResetTransform();

  // --- Kırpma ---

  /// @brief Scissor kırpma dikdörtgeni ayarla.
  void SetClipRect(const Rect& rect);

  /// @brief Kırpma dikdörtgenini kaldır.
  void ClearClip();

 private:
  /// @brief Projeksiyon matrisini viewport boyutuna göre güncelle.
  void UpdateProjection();

  /// @brief Aktif durumun transform matrisini renderer'a gönder.
  void FlushTransform();

  /// @brief Scissor box'ı koordinat sistemi dönüsümü yaparak renderer'a gönder.
  ///
  /// OpenGL scissor Y=0 altta; Painter Y=0 ustte. Bu fonksiyon flip'i uygular.
  void ApplyScissor(const Rect& rect);

  [[nodiscard]] bool CanDrawPen() const noexcept {
    return mRenderer && mBatcher && mCurrentState.pen.IsVisible();
  }
  [[nodiscard]] bool CanDrawBrush() const noexcept {
    return mRenderer && mBatcher && mCurrentState.brush.IsVisible();
  }

  SDL_Window* mWindow{nullptr};

  std::unique_ptr<IRenderer> mRenderer;
  std::unique_ptr<RenderBatcher> mBatcher;
  std::vector<RenderState> mStateStack;
  RenderState mCurrentState;
  std::shared_ptr<Font> mCurrentFont;

  int32_t mViewportWidth{0};
  int32_t mViewportHeight{0};
};

}  // namespace sdl_painter
