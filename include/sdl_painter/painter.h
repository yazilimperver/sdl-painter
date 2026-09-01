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
#include "sdl_painter/path.h"
#include "sdl_painter/pen.h"
#include "sdl_painter/render_target.h"
#include "sdl_painter/renderer.h"
#include "sdl_painter/vertex.h"

#include <cstddef>
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
  BlendMode blend_mode{BlendMode::kAlpha};
  Rect clip_rect;
  bool has_clip{false};
};

/// @brief Ana çizim sınıfı
///
/// Painter, SDL penceresi üzerinde 2B çizim yapar. Backend (OpenGL/Vulkan)
/// IRenderer arayüzü üzerinden soyutlanır.
///
/// @warning **Yaşam döngüsü sözleşmesi:** Painter'a texture yükleyen tüm
/// @ref Image ve @ref Font nesneleri, Painter yıkılmadan önce
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
  /// Renderer'ın sahipliği devralınır; `Initialize()` çağrısı yapılmaz
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
  /// Boyut framebuffer piksel cinsindendir. @ref Application bunu
  /// `SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED` olayında çağırır.
  ///
  /// @note Bu fonksiyon bir kez çağrıldığında Painter, boyutu artık her
  ///       karede pencereden okumayı bırakır ve yalnızca bu çağrılara
  ///       güvenir. Kendi olay döngüsünü yazan uygulamalar ya bu metodu
  ///       yeniden boyutlandırmada çağırmalı ya da hiç çağırmamalıdır
  ///       (o zaman otomatik yoklama sürer).
  void SetDrawableSize(int32_t width, int32_t height);

  // --- Görüntü alanı (viewport) ---

  /// @brief Çizimi pencerenin bir alt dikdörtgeniyle sınırla.
  ///
  /// Bölünmüş ekran, mini harita ve kenar panelleri için. Viewport
  /// ayarlandıktan sonra çizim koordinatları o alt dikdörtgene göre
  /// yereldir: `(0, 0)` viewport'un sol üst köşesidir ve projeksiyon
  /// `width` × `height`'a göre kurulur.
  ///
  /// @warning Bu, @ref SetClipRect ile aynı şey değildir. Kırpma
  ///          koordinat sistemini değiştirmeden pikselleri maskeler; viewport
  ///          ise koordinat sisteminin kendisini yeniden tanımlar.
  ///
  /// @note Viewport bir GPU durumudur ve @ref Save / @ref Restore kapsamında
  ///       değildir; değiştirmek biriken çizimleri flush eder. Tam
  ///       pencereye dönmek için @ref ResetViewport.
  ///
  /// @param x Sol kenar (framebuffer piksel, sol üst köşeden).
  /// @param y Üst kenar (framebuffer piksel, sol üst köşeden).
  /// @param width  Genişlik (> 0; değilse çağrı yok sayılır).
  /// @param height Yükseklik (> 0; değilse çağrı yok sayılır).
  void SetViewport(int32_t x, int32_t y, int32_t width, int32_t height);

  /// @brief Viewport'u tüm çizim yüzeyine geri al.
  void ResetViewport();

  // --- Çizim hedefi (dokuya çizim) ---

  /// @brief Ekran yerine çizilebilen offscreen bir yüzey oluştur.
  ///
  /// Mini harita, son işlem efektleri, iz efekti ve pahalı katmanların
  /// önbelleklenmesi için. Ayrıntı ve kullanım örneği: @ref RenderTarget.
  ///
  /// @param width  Genişlik (piksel, > 0).
  /// @param height Yükseklik (piksel, > 0).
  /// @param filter Hedef ekrana basılırken kullanılacak örnekleme filtresi.
  /// @return Geçerli bir hedef; başarısızlıkta @ref RenderTarget::IsValid
  ///         `false` döner (backend desteklemiyor olabilir).
  [[nodiscard]] RenderTarget CreateRenderTarget(
      int32_t width, int32_t height,
      TextureFilter filter = TextureFilter::kLinear);

  /// @brief Sonraki çizimleri verilen hedefe yönlendir.
  ///
  /// Çizim koordinatları hedefe yerel olur: `(0, 0)` hedefin sol üst
  /// köşesi, projeksiyon hedefin boyutuna göre. @ref SetViewport ile ayarlanmış
  /// bir viewport varsa hedef süresince askıya alınır ve
  /// @ref ResetRenderTarget ile geri gelir.
  ///
  /// @note Hedef seçimi bir GPU durumudur: @ref Save / @ref Restore kapsamında
  ///       değildir ve değiştirmek biriken çizimleri flush eder. Seçim kare
  ///       sınırını da aşmaz — her @ref Begin ekranda başlar.
  ///
  /// @return Geçiş yapıldıysa `true`.
  bool SetRenderTarget(const RenderTarget& target);

  /// @brief Çizimi ekrana geri al.
  void ResetRenderTarget();

  /// @brief Hedefin içeriğini orijinal boyutuyla çiz.
  ///
  /// @param tint @ref DrawImage ile aynı anlamda.
  void DrawRenderTarget(const RenderTarget& target, float x, float y,
                        const Color& tint = Color::White(),
                        ImageFlip flip = ImageFlip::kNone);

  /// @brief Hedefin içeriğini hedef dikdörtgene ölçekleyerek çiz.
  void DrawRenderTarget(const RenderTarget& target, const Rect& dest_rect,
                        const Color& tint = Color::White(),
                        ImageFlip flip = ImageFlip::kNone);

  /// @brief Hedefin piksellerini ana belleğe oku.
  ///
  /// Sonuç sıkı paketlenmiş, doğrusal RGBA8 ve satırlar yukarıdan aşağı —
  /// OpenGL ile Vulkan birebir aynı baytları verir. Ekran görüntüsü almak ve
  /// iki backend'in çıktısını karşılaştıran testler için.
  ///
  /// @warning **Bloklar:** GPU'nun işi bitene kadar bekler. Kare döngüsünde
  ///          kullanılmamalıdır. Ayrıca @ref End sonrasında çağrılmalıdır;
  ///          kare ortasında çağrılırsa o karede biriken çizimler henüz
  ///          gönderilmemiş olabilir.
  ///
  /// @param out_rgba Sonuç buraya yazılır; gerekiyorsa yeniden boyutlandırılır.
  /// @return Başarıda `true`.
  bool ReadRenderTarget(const RenderTarget& target,
                        std::vector<uint8_t>& out_rgba);

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
  /// @ref Save / @ref Restore font'u kapsamaz (font bir çizim durumu
  /// değil, paylaşılan bir kaynaktır). Geçici olarak başka bir fontla çizim
  /// yapan kod, önceki fontu bununla okuyup geri koyabilir.
  [[nodiscard]] const std::shared_ptr<Font>& GetFont() const noexcept {
    return mCurrentFont;
  }

  /// @brief Global opaklığı ayarla [0.0, 1.0].
  void SetOpacity(float alpha);

  /// @brief Renk karıştırma modunu ayarla.
  ///
  /// @ref Save / @ref Restore kapsamındadır. Opaklık gibi bir GPU durumu
  /// olduğu için batch'i kırar: mod değiştikçe bir draw call daha çıkar.
  /// Renk ve tint'in aksine vertex'te taşınamaz.
  ///
  /// @note Vulkan'da karıştırma pipeline'ın sabit durumudur; mod başına ayrı
  ///       bir pipeline varyantı önceden üretilir, çizim anında derleme
  ///       yapılmaz.
  void SetBlendMode(BlendMode mode);

  /// @brief Yürürlükteki karıştırma modu.
  [[nodiscard]] BlendMode GetBlendMode() const noexcept {
    return mCurrentState.blend_mode;
  }

  // --- Primitifler ---

  /// @brief İki nokta arasına çizgi çiz.
  void DrawLine(float x1, float y1, float x2, float y2);

  /// @brief Dikdörtgen çerçeve çiz (dolgu yok).
  void DrawRect(float x, float y, float w, float h);

  /// @brief Dikdörtgeni doldur.
  void FillRect(float x, float y, float w, float h);

  /// @brief Yuvarlatılmış köşeli dikdörtgen çerçevesi çiz.
  ///
  /// Arayüz çiziminin en sık kullanılan şekli; QPainter'daki
  /// `drawRoundedRect` karşılığı.
  ///
  /// @param radius Köşe yarıçapı. `<= 0` ise @ref DrawRect ile birebir aynı
  ///        sonucu verir. `min(w, h) / 2`'yi aşarsa oraya kırpılır — sonuç
  ///        stadyum (kare girdide daire) şeklidir, taşma olmaz.
  ///        `w` veya `h` pozitif değilse hiçbir şey çizilmez.
  void DrawRoundedRect(float x, float y, float w, float h, float radius);

  /// @brief Yuvarlatılmış köşeli dikdörtgeni doldur.
  ///
  /// Kenar durumları @ref DrawRoundedRect ile aynıdır.
  void FillRoundedRect(float x, float y, float w, float h, float radius);

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
  /// Açı birimi derece. 0° = +x ekseni ve açı, @ref Rotate ile aynı yönde
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

  // --- Yol (path) ---

  /// @brief Yolun çerçevesini çiz.
  ///
  /// Her alt yol ayrı çizilir: kapalı olanlar (@ref Path::Close) kapalı
  /// poligon gibi, açık olanlar polyline gibi işlenir. Dolayısıyla uç stili
  /// yalnızca açık alt yolların iki ucuna, birleşim stili tüm köşelere
  /// uygulanır.
  ///
  /// @note Kesikli kalemde desen her alt yolun başında sıfırlanır; tek bir
  ///       alt yol içinde ise yol boyunca sürekli ilerler (bir kesik köşenin
  ///       üzerinden geçebilir).
  void DrawPath(const Path& path);

  /// @brief Yolun içini doldur (ear clipping tessellation).
  ///
  /// Açık alt yollar dolgu için örtük olarak kapatılır — QPainter da
  /// böyle davranır.
  ///
  /// @warning Her alt yol bağımsız doldurulur; even-odd veya nonzero
  ///          dolgu kuralı uygulanmaz. Bir alt yolun diğerinin içinde kalması
  ///          onu delik yapmaz, üzerine ikinci bir dolgu çizer. Delikli
  ///          şekiller bu sürümün kapsamı dışındadır (bkz. @ref Path).
  void FillPath(const Path& path);

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

  /// @brief Dokuyu serbest biçimli bir ızgara üzerine çiz.
  ///
  /// `DrawImage`'ın üç aşırı yüklemesi de eksen hizalı `Rect` alır, yani
  /// dörtgenin köşeleri bağımsız hareket edemez. Dalgalanan bayrak, sayfa
  /// kıvrımı, perspektif taklidi gibi efektler bunu gerektirir; bu aşırı
  /// yükleme `IRenderer::DrawTextured`'a `Painter` seviyesinden erişim verir.
  ///
  /// Doku koordinatları ızgara konumundan düzgün türetilir: `(c, r)`
  /// köşesinin UV'si `(c / cols, r / rows)`'tur. Yani deformasyon yalnızca
  /// konumdadır, dokunun kendisi ızgaraya eşit dağılır.
  ///
  /// @param cols Yatay hücre sayısı (> 0).
  /// @param rows Dikey hücre sayısı (> 0).
  /// @param points Izgara köşeleri, satır-major ve tam
  ///        `(cols + 1) * (rows + 1)` adet. Boyut tutmuyorsa çağrı yok sayılır
  ///        ve hata loglanır — sessizce yanlış geometri çizmektense.
  /// @param tint @ref DrawImage ile aynı anlamda.
  void DrawImageMesh(const Image& image, int32_t cols, int32_t rows,
                     const std::vector<Point>& points,
                     const Color& tint = Color::White());

  /// @brief Yüklenmiş bir görüntünün doku içeriğini yerinde güncelle.
  ///
  /// Her karede değişen prosedürel dokular içindir (plazma, ısı haritası,
  /// piksel tuvali). Görüntüyü her karede yeniden yaratmaya göre farkı,
  /// doku tahsis/serbest bırakma döngüsünün hiç yaşanmamasıdır.
  ///
  /// Görüntü henüz yüklenmemişse bu çağrı onu yükler; sonraki çağrılar aynı
  /// dokuyu günceller.
  ///
  /// @param image Güncellenecek görüntü. 4 kanallı (RGBA8) olmalıdır;
  ///        değilse çağrı yok sayılır ve hata loglanır.
  /// @param rgba Sıkı paketlenmiş `Width() * Height() * 4` baytlık veri.
  ///
  /// @note Çağrı, biriken çizimleri flush eder. Doku içeriği anında
  ///       değiştiği için, o karede aynı dokudan yapılmış ve henüz
  ///       gönderilmemiş çizimler eski içerikle çizilmiş olmalı — aksi halde
  ///       geriye dönük olarak yeni içerikle çizilirlerdi. Bedeli kare başına
  ///       bir draw call'dur.
  void UpdateImage(const Image& image, const uint8_t* rgba);

  /// @brief Görüntünün kaynak dikdörtgenini hedef dikdörtgene çiz.
  ///
  /// @note Tint, vertex'te taşınır — aynı texture'ı farklı renklerle çizmek
  ///       batch'i kırmaz. (Opaklık için aynısı geçerli değildir.)
  void DrawImage(const Image& image, const Rect& src_rect,
                 const Rect& dest_rect, const Color& tint = Color::White(),
                 ImageFlip flip = ImageFlip::kNone);

  // --- Metin ---

  /// @brief Noktaya metin çiz.
  ///
  /// Satır sonu karakteri (LF) satır böler; her satır bir öncekinden
  /// @ref Font::LineHeight kadar aşağıya çizilir. `y` ilk satırın
  /// baseline'ıdır.
  void DrawText(float x, float y, const std::string& text);

  /// @brief Dikdörtgen içine hizalanmış metin çiz.
  ///
  /// Metin dikdörtgen içinde dikey olarak ortalanır; satır sonu karakteri
  /// (LF) satır böler.
  ///
  /// @param wrap @ref TextWrap::kWord ise satırlar dikdörtgen genişliğini
  ///        aşmayacak şekilde sözcük sınırlarından bölünür. Tek bir sözcük
  ///        bile sığmıyorsa karakter sınırından bölünür (UTF-8 güvenli).
  ///        Varsayılan @ref TextWrap::kNone: davranış eskisiyle birebir aynı,
  ///        uzun metin dikdörtgenden taşar.
  void DrawText(const Rect& rect, const std::string& text,
                Alignment alignment = Alignment::kLeft,
                TextWrap wrap = TextWrap::kNone);

  /// @brief Sarmalanmış metnin kaç satır tutacağını hesapla (çizmeden).
  ///
  /// Yerleşim hesabı için: kutunun yüksekliğini metne göre belirlemek
  /// isteyenin, metni önce çizip ölçmesi gerekmesin.
  ///
  /// @return Satır sayısı; font yoksa veya metin boşsa 0.
  [[nodiscard]] std::size_t CountTextLines(const std::string& text,
                                           float max_width,
                                           TextWrap wrap) const;

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
  /// viewport, scissor ve projeksiyon daima piksel boyutunu kullanır.
  /// Penceresiz Painter'da (0, 0) döner.
  void QueryDrawableSize(int32_t& out_width, int32_t& out_height) const;

  /// @brief Viewport + projeksiyonu verilen boyuta gore guncelle (degistiyse).
  void ApplyDrawableSize(int32_t width, int32_t height);

  /// @brief Projeksiyon matrisini viewport boyutuna göre güncelle.
  void UpdateProjection();

  /// @brief Painter'ın Y ekseni ile GPU'nunki aynı yönde mi?
  ///
  /// Painter'da Y aşağı doğru artar. Vulkan'ın clip uzayı da öyledir, OpenGL'in
  /// ekran framebuffer'ı ise ters yöndedir — bu yüzden GL'de projeksiyon,
  /// viewport ve scissor çevrilir.
  ///
  /// Bir çizim hedefine (FBO) çizerken GL'de bu çevirme yapılmaz: hedefin
  /// 0. satırı Painter'ın `y = 0`'ına denk gelsin isteriz. Aksi halde dokunun
  /// bellekteki satır sırası ters olur ve hem ekrana basıldığında baş aşağı
  /// görünür hem de @ref ReadRenderTarget çıktısı Vulkan'ınkiyle uyuşmazdı.
  [[nodiscard]] bool YAxisMatchesGpu() const;

  /// @brief Yürürlükteki çizim yüzeyinin yüksekliği (hedef bağlıysa onunki).
  [[nodiscard]] int32_t SurfaceHeight() const;

  /// @brief Viewport'u GPU'ya yaz (Y çevirmesi dahil) ve projeksiyonu güncelle.
  void ApplyViewport();

  /// @brief Dokulu bir dörtgeni batcher'a ver (aynalama dahil).
  ///
  /// @ref DrawImage ile @ref DrawRenderTarget'ın ortak gövdesi; ikisinin de
  /// tek farkı UV'yi nereden aldıklarıdır.
  void PushTexturedQuad(TextureHandle texture, float u0, float v0, float u1,
                        float v1, const Rect& dest_rect, const Color& tint,
                        ImageFlip flip);

  /// @brief Çizgi geometrisini tek renkle batcher'a ver (karıştırma senkronu
  ///        dahil).
  void PushStroke(const std::vector<Vertex>& verts, const Color& color);

  /// @brief Dolgu geometrisini fırçaya göre renklendirip batcher'a ver.
  ///
  /// Düz fırçada renk batcher'da tek seferde yazılır; gradient'te vertex
  /// başına hesaplanır. Vertex'ler bu yüzden `const` değildir.
  void PushFilled(std::vector<Vertex>& verts);

  /// @brief Tek bir satırı baseline'a çiz (satır sonu içermediği varsayılır).
  ///
  /// Çok satırlı çizim bunu satır başına bir kez çağırır; satır bölme ve
  /// hizalama hesabı çağıranda kalır.
  void DrawTextLine(float x, float y, const std::string& text);

  /// @brief Bir satırın hizalamaya göre başlangıç x'i.
  [[nodiscard]] float AlignedX(const Rect& rect, const std::string& line,
                               Alignment alignment) const;

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

  /// Tüm çizim yüzeyi (framebuffer) boyutu.
  int32_t mDrawableWidth{0};
  int32_t mDrawableHeight{0};

  /// Yürürlükteki viewport. Varsayılan olarak yüzeyin tamamıdır;
  /// @ref SetViewport ile daraltılabilir. Projeksiyon bunun boyutuna
  /// göre kurulur, yüzeyin tamamına göre değil.
  int32_t mViewportX{0};
  int32_t mViewportY{0};
  int32_t mViewportWidth{0};
  int32_t mViewportHeight{0};

  /// @ref SetViewport çağrıldı mı? `true` iken yeniden boyutlandırma
  /// viewport'u ezmez — kullanıcının seçimi korunur.
  bool mCustomViewport{false};

  /// Yürürlükteki çizim hedefi (kInvalidRenderTarget = ekran).
  RenderTargetHandle mActiveTarget{kInvalidRenderTarget};
  int32_t mTargetWidth{0};
  int32_t mTargetHeight{0};

  /// Hedefe geçmeden önceki viewport; @ref ResetRenderTarget geri yükler.
  int32_t mSavedViewportX{0};
  int32_t mSavedViewportY{0};
  int32_t mSavedViewportWidth{0};
  int32_t mSavedViewportHeight{0};
  bool mSavedCustomViewport{false};

  /// @brief Boyut her karede pencereden okunsun mu?
  /// @ref SetDrawableSize ilk çağrıldığında `false` olur.
  bool mAutoDrawableSize{true};
};

}  // namespace sdl_painter
