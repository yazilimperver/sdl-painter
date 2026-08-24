#pragma once

#include "sdl_painter/brush.h"
#include "sdl_painter/color.h"
#include "sdl_painter/export.h"
#include "sdl_painter/font.h"
#include "sdl_painter/frame_stats.h"
#include "sdl_painter/geometry.h"
// ImageFlip için — Image'ın kendisi ileri bildirimle yetiyordu, ancak
// DrawImage'ın varsayılan argümanı enum'un tam tanımını gerektiriyor.
#include "sdl_painter/image.h"
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

/// @brief Ana çizim sınıfı
///
/// Painter, SDL penceresi üzerinde 2B çizim yapar. Backend (OpenGL/Vulkan)
/// IRenderer arayüzü üzerinden soyutlanır.
///
/// @warning **Yaşam döngüsü sözleşmesi:** Painter'a texture yükleyen tüm
/// @ref Image ve @ref Font nesneleri, Painter yıkılmadan **önce**
/// yıkılmalıdır. Image veya Font'u global / `static` ya da daha uzun
/// yaşayan bir konuma yerleştirmek tanımsız davranışa yol açar — yıkım
/// sırasında dangling IRenderer pointer kullanılır. v0.2.0'da bu sözleşme
/// `weak_ptr<IRenderer>` veya benzeri bir mekanizma ile zorunlu kılınacaktır.
class SDLPAINTER_API Painter {
 public:
  /// @brief Belirtilen pencere ve backend ile Painter oluştur.
  Painter(SDL_Window* window, RendererBackend backend);

  /// @brief Hazır bir renderer ile pencere olmadan Painter oluştur.
  ///
  /// Renderer'ın sahipliği devralınır; `Initialize()` çağrısı **yapılmaz**
  /// (çağıranın sorumluluğu). Pencere olmadığı için @ref Begin her karede
  /// yeniden boyutlandırma yoklaması yapmaz — viewport bu ctor'da verilen
  /// değerde sabit kalır.
  ///
  /// Offscreen render ve birim testleri (sahte IRenderer enjeksiyonu) için.
  ///
  /// @param renderer Sahipliği devralınan renderer; `nullptr` ise Painter
  ///        geçersiz durumda kalır (@ref IsValid `false`).
  /// @param viewport_width  Viewport genişliği (piksel, > 0).
  /// @param viewport_height Viewport yüksekliği (piksel, > 0).
  Painter(std::unique_ptr<IRenderer> renderer, int32_t viewport_width,
          int32_t viewport_height);

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

  /// @brief Çizim yüzeyi boyutunu bildir (viewport + projeksiyon güncellenir).
  ///
  /// Boyut **framebuffer piksel** cinsindendir. @ref Application bunu
  /// `SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED` olayında çağırır.
  ///
  /// @note Bu fonksiyon bir kez çağrıldığında Painter, boyutu artık her
  ///       karede pencereden okumayı bırakır ve yalnızca bu çağrılara
  ///       güvenir. Kendi olay döngüsünü yazan uygulamalar ya bu metodu
  ///       yeniden boyutlandırmada çağırmalı ya da hiç çağırmamalıdır
  ///       (o zaman otomatik yoklama sürer).
  void SetDrawableSize(int32_t width, int32_t height);

  // --- Temizlik ---

  /// @brief Ekranı belirtilen renkle temizle.
  void Clear(const Color& color);

  // --- Profilleme ---

  /// @brief Son tamamlanan karenin çizim istatistikleri.
  ///
  /// @ref Begin sayaçları sıfırlar, @ref End değerleri tamamlar. Dolayısıyla
  /// anlamlı okuma noktası `End()` sonrası — tipik olarak bir sonraki karenin
  /// çizimi sırasında (ekran üstü FPS göstergesi bunu böyle kullanır).
  ///
  /// @see FrameStats
  [[nodiscard]] const FrameStats& GetFrameStats() const noexcept {
    return mLastStats;
  }

  // --- Stil ---

  /// @brief Aktif kalemi ayarla (çizgi rengi ve kalınlığı).
  void SetPen(const Pen& pen);

  /// @brief Aktif fırçayı ayarla (dolgu rengi).
  void SetBrush(const Brush& brush);

  /// @brief Aktif fontu ayarla
  void SetFont(std::shared_ptr<Font> font);

  /// @brief Aktif font (yoksa `nullptr`).
  ///
  /// @ref Save / @ref Restore font'u **kapsamaz** (font bir çizim durumu
  /// değil, paylaşılan bir kaynaktır). Geçici olarak başka bir fontla çizim
  /// yapan kod, önceki fontu bununla okuyup geri koyabilir.
  [[nodiscard]] const std::shared_ptr<Font>& GetFont() const noexcept {
    return mCurrentFont;
  }

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

  /// @brief Yay çiz (açık — uçları birleştirilmez).
  ///
  /// Açı birimi **derece**. 0° = +x ekseni ve açı, @ref Rotate ile aynı yönde
  /// artar. Qt'nin 1/16 derece sözleşmesi bilinçli olarak izlenmez; kütüphane
  /// içi tutarlılık tercih edildi.
  ///
  /// @param sweep_degrees Taranan açı; negatif olabilir (ters yön), mutlak
  ///        değeri 360°'ye kırpılır.
  void DrawArc(float cx, float cy, float rx, float ry, float start_degrees,
               float sweep_degrees);

  /// @brief Dilim (pie) çerçevesi — yay artı merkeze giden iki yarıçap.
  void DrawPie(float cx, float cy, float rx, float ry, float start_degrees,
               float sweep_degrees);

  /// @brief Dilimi doldur. Çizelgelerin (pasta grafik) temel yapı taşı.
  void FillPie(float cx, float cy, float rx, float ry, float start_degrees,
               float sweep_degrees);

  /// @brief Kiriş (chord) çerçevesi — yay artı uçlarını birleştiren doğru.
  void DrawChord(float cx, float cy, float rx, float ry, float start_degrees,
                 float sweep_degrees);

  /// @brief Kirişi doldur.
  void FillChord(float cx, float cy, float rx, float ry, float start_degrees,
                 float sweep_degrees);

  /// @brief Çok kenarlı kapalı şeklin çerçevesini çiz.
  void DrawPolygon(const std::vector<Point>& points);

  /// @brief Çok kenarlıyı doldur (ear clipping tessellation).
  void FillPolygon(const std::vector<Point>& points);

  /// @brief Açık çizgi dizisi çiz.
  void DrawPolyline(const std::vector<Point>& points);

  // --- Görüntü ---

  /// @brief Görüntüyü orijinal boyutuyla çiz.
  /// @param tint Doku rengiyle çarpılacak renk. Varsayılan beyaz = değişiklik
  ///        yok. Alfası, @ref SetOpacity ile *çarpışmaz*: opaklık tüm kareye
  ///        uygulanan bir uniform, tint ise yalnızca bu görüntüye aittir.
  /// @param flip Aynalama (bkz. @ref ImageFlip).
  void DrawImage(const Image& image, float x, float y,
                 const Color& tint = Color::White(),
                 ImageFlip flip = ImageFlip::kNone);

  /// @brief Görüntüyü hedef dikdörtgene ölçekleyerek çiz.
  void DrawImage(const Image& image, const Rect& dest_rect,
                 const Color& tint = Color::White(),
                 ImageFlip flip = ImageFlip::kNone);

  /// @brief Yüklenmiş bir görüntünün doku içeriğini **yerinde** güncelle.
  ///
  /// Her karede değişen prosedürel dokular içindir (plazma, ısı haritası,
  /// piksel tuvali). Görüntüyü her karede yeniden yaratmaya göre farkı,
  /// doku tahsis/serbest bırakma döngüsünün hiç yaşanmamasıdır.
  ///
  /// Görüntü henüz yüklenmemişse bu çağrı onu yükler; sonraki çağrılar aynı
  /// dokuyu günceller.
  ///
  /// @param image Güncellenecek görüntü. **4 kanallı (RGBA8)** olmalıdır;
  ///        değilse çağrı yok sayılır ve hata loglanır.
  /// @param rgba Sıkı paketlenmiş `Width() * Height() * 4` baytlık veri.
  ///
  /// @note Çağrı, biriken çizimleri **flush eder**. Doku içeriği anında
  ///       değiştiği için, o karede aynı dokudan yapılmış ve henüz
  ///       gönderilmemiş çizimler eski içerikle çizilmiş olmalı — aksi halde
  ///       geriye dönük olarak yeni içerikle çizilirlerdi. Bedeli kare başına
  ///       bir draw call'dur.
  void UpdateImage(const Image& image, const uint8_t* rgba);

  /// @brief Görüntünün kaynak dikdörtgenini hedef dikdörtgene çiz.
  ///
  /// @note Tint, vertex'te taşınır — aynı texture'ı farklı renklerle çizmek
  ///       batch'i **kırmaz**. (Opaklık için aynısı geçerli değildir.)
  void DrawImage(const Image& image, const Rect& src_rect,
                 const Rect& dest_rect, const Color& tint = Color::White(),
                 ImageFlip flip = ImageFlip::kNone);

  // --- Metin

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
  /// @brief Pencerenin framebuffer (piksel) boyutunu döndür.
  ///
  /// HiDPI ölçeklemede mantıksal pencere boyutu ile piksel boyutu ayrışır;
  /// viewport, scissor ve projeksiyon daima **piksel** boyutunu kullanır.
  /// Penceresiz Painter'da (0, 0) döner.
  void QueryDrawableSize(int32_t& out_width, int32_t& out_height) const;

  /// @brief Viewport + projeksiyonu verilen boyuta gore guncelle (degistiyse).
  void ApplyDrawableSize(int32_t width, int32_t height);

  /// @brief Projeksiyon matrisini viewport boyutuna göre güncelle.
  void UpdateProjection();

  /// @brief Scissor box'ı koordinat sistemi dönüsümü yaparak renderer'a gönder.
  ///
  /// OpenGL scissor Y=0 altta; Painter Y=0 ustte. Bu fonksiyon flip'i uygular.
  void ApplyScissor(const Rect& rect);

  /// @brief Scissor'i kaldir ve durum degisikligi sayacini artir.
  void ClearScissorCounted();

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

  /// Bu karede birikenler; End() sonunda mLastStats'a tasinir.
  FrameStats mStats;
  FrameStats mLastStats;
  uint64_t mFrameStartNs{0};

  int32_t mViewportWidth{0};
  int32_t mViewportHeight{0};

  /// @brief Boyut her karede pencereden okunsun mu?
  /// @ref SetDrawableSize ilk çağrıldığında `false` olur.
  bool mAutoDrawableSize{true};
};

}  // namespace sdl_painter
