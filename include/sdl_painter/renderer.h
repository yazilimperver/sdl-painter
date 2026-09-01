#pragma once

#include "sdl_painter/export.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

// SDL forward declaration
struct SDL_Window;

namespace sdl_painter {

struct Color;
struct Vertex;
struct TexturedVertex;

/// @brief Texture tanımlayıcısı — backend-agnostic opak tip.
using TextureHandle = uint32_t;

/// @brief Geçersiz/boş texture tanımlayıcısı.
constexpr TextureHandle kInvalidTexture = 0;

/// @brief Çizim hedefi (offscreen render target) tanımlayıcısı — opak tip.
using RenderTargetHandle = uint32_t;

/// @brief Geçersiz hedef; @ref IRenderer::SetRenderTarget'a verildiğinde
///        çizim ekrana (varsayılan framebuffer'a) döner.
constexpr RenderTargetHandle kInvalidRenderTarget = 0;

// Çizim hedeflerinin piksel formatı her backend'de doğrusal RGBA8'dir ve
// ekran yüzeyinin formatından (Vulkan'da tipik olarak B8G8R8A8_UNORM)
// kasıtlı olarak ayrıştırılmıştır — gerekçe ReadRenderTarget'ta.

/// @brief Doku örnekleme filtresi.
enum class TextureFilter : uint8_t {
  /// @brief Doğrusal enterpolasyon — büyütmede yumuşak. **Varsayılan.**
  kLinear,
  /// @brief En yakın komşu — büyütmede keskin piksel kenarları. Piksel sanatı
  ///        bununla çizilmelidir; `kLinear` ile pikseller bulanıklaşır.
  kNearest,
};

/// @brief Renk karıştırma (blend) modu.
///
/// Kaynak (çizilen) ile hedef (ekranda olan) rengin nasıl birleştirileceğini
/// belirler.
enum class BlendMode : uint8_t {
  /// @brief Standart alfa karıştırma. **Varsayılan.**
  kAlpha,
  /// @brief Toplamalı — renkler üst üste biriktikçe parlar. Işık, kıvılcım,
  ///        parlama efektleri için; koyu zeminde etkilidir.
  kAdditive,
  /// @brief Çarpımsal — hedefi koyulaştırır. Gölge ve renk süzgeci için.
  kMultiply,
  /// @brief Karıştırma kapalı — kaynak rengi olduğu gibi yazılır, alfa
  ///        dikkate alınmaz.
  kNone,
};

/// @brief @ref BlendMode değer sayısı (pipeline varyantı dizileri için).
inline constexpr std::size_t kBlendModeCount = 4;

/// @brief Render backend seçeneği.
enum class RendererBackend : uint8_t {
  kOpenGL,
  kVulkan,
};

/// @brief Backend-agnostik soyut renderer arayüzü.
///
/// OpenGL ve Vulkan implementasyonları bu arayüzü uygular.
/// Painter, IRenderer üzerinden çalışır — backend detaylarını bilmez.
class IRenderer {
 public:
  IRenderer() = default;
  virtual ~IRenderer() = default;

  // Soyut arayüz: kopyalama/taşıma kapalı (object slicing önlenir).
  // Sahiplik daima std::unique_ptr<IRenderer> üzerinden taşınır.
  IRenderer(const IRenderer&) = delete;
  IRenderer& operator=(const IRenderer&) = delete;
  IRenderer(IRenderer&&) = delete;
  IRenderer& operator=(IRenderer&&) = delete;

  // --- Yaşam döngüsü ---

  /// @brief Renderer'ı verilen pencere için başlat.
  virtual bool Initialize(SDL_Window* window) = 0;

  /// @brief Kaynakları serbest bırak.
  virtual void Shutdown() = 0;

  /// @brief Yeni frame'e başla.
  virtual void BeginFrame() = 0;

  /// @brief Frame'i tamamla ve ekrana sun.
  virtual void EndFrame() = 0;

  // --- Durum ---

  /// @brief Viewport'u ayarla (piksel koordinatları).
  virtual void SetViewport(int32_t x, int32_t y, int32_t width,
                           int32_t height) = 0;

  /// @brief Scissor (kırpma) dikdörtgeni ayarla.
  virtual void SetScissor(int32_t x, int32_t y, int32_t width,
                          int32_t height) = 0;

  /// @brief Scissor kırpmasını kaldır.
  virtual void ClearScissor() = 0;

  /// @brief Ekranı belirtilen renkle temizle.
  virtual void Clear(const Color& color) = 0;

  /// @brief Global opaklığı ayarla [0.0, 1.0].
  virtual void SetOpacity(float alpha) = 0;

  /// @brief Renk karıştırma modunu ayarla.
  ///
  /// Saf sanal değildir: varsayılan gövde çağrıyı yok sayar, yani bu
  /// arayüzü dışarıda implemente etmiş kod derlenmeye devam eder ve standart
  /// alfa karıştırmayla çalışır. Aynı yaklaşım @ref GetLastGpuFrameMs ve
  /// filtreli @ref CreateTexture için de kullanılıyor.
  virtual void SetBlendMode(BlendMode mode) { (void)mode; }

  // --- Çizim primitifleri (tessellated vertex'ler) ---

  /// @brief Üçgenler çiz. Vertex listesi 3'ün katı olmalı. Renk vertex'te taşınır.
  virtual void DrawTriangles(const std::vector<Vertex>& vertices) = 0;

  // --- Texture işlemleri ---

  /// @brief Ham piksel verisinden texture oluştur. Başarısızlıkta kInvalidTexture döner.
  virtual TextureHandle CreateTexture(const uint8_t* data, int32_t width,
                                      int32_t height, int32_t channels) = 0;

  /// @brief Örnekleme filtresi belirterek texture oluştur.
  ///
  /// Saf sanal değildir: varsayılan gövde filtreyi yok sayıp yukarıdaki
  /// aşırı yüklemeye düşer. Böylece bu arayüzü dışarıda implemente etmiş
  /// kod, filtre desteği gelmeden önce yazılmış olsa bile derlenmeye devam
  /// eder ve doğrusal filtreyle çalışır. Aynı yaklaşım
  /// @ref GetLastGpuFrameMs için de kullanılıyor.
  virtual TextureHandle CreateTexture(const uint8_t* data, int32_t width,
                                      int32_t height, int32_t channels,
                                      TextureFilter filter) {
    (void)filter;
    return CreateTexture(data, width, height, channels);
  }

  /// @brief Var olan bir texture'ın alt bölgesini güncelle (sub-image yükleme).
  ///
  /// Glyph atlası gibi artımlı doldurulan texture'lar için gereklidir:
  /// her yeni glyph'te tüm sayfayı yeniden yaratmak yerine yalnızca ilgili
  /// dikdörtgen yüklenir.
  ///
  /// @param handle Güncellenecek texture (@ref CreateTexture ile üretilmiş).
  /// @param x Hedef bölgenin sol kenarı (piksel).
  /// @param y Hedef bölgenin üst kenarı (piksel).
  /// @param width Bölge genişliği (piksel, > 0).
  /// @param height Bölge yüksekliği (piksel, > 0).
  /// @param data Sıkı paketlenmiş RGBA8 piksel verisi
  ///        (`width * height * 4` bayt).
  ///
  /// @warning Kare ortasında çağrılabilir; ancak o kare içinde daha önce
  ///          çizim komutu verilmiş bir bölgenin üzerine yazmak tanımsızdır.
  ///          Yalnızca henüz kullanılmamış bölgeleri doldurun (glyph atlası
  ///          bunu şerf paketlemesiyle garanti eder).
  virtual void UpdateTexture(TextureHandle handle, int32_t x, int32_t y,
                             int32_t width, int32_t height,
                             const uint8_t* data) = 0;

  /// @brief Texture'ı sil.
  virtual void DestroyTexture(TextureHandle handle) = 0;

  /// @brief Texture'lı vertex'leri çiz. Tint rengi vertex'te taşınır.
  virtual void DrawTextured(const std::vector<TexturedVertex>& vertices,
                            TextureHandle texture) = 0;

  // --- Çizim hedefleri (offscreen render target) ---
  //
  // Aşağıdakilerin hiçbiri saf sanal DEĞİLDİR; varsayılan gövdeler "hedef
  // desteklenmiyor" anlamına gelir. Böylece bu arayüzü dışarıda implemente
  // etmiş kod derlenmeye devam eder (aynı yaklaşım SetBlendMode, filtreli
  // CreateTexture ve GetLastGpuFrameMs için de kullanılıyor).

  /// @brief Çizilebilir bir offscreen hedef oluştur.
  ///
  /// Hedefin içeriği @ref GetRenderTargetTexture ile bir texture olarak
  /// örneklenebilir; yani "önce dokuya çiz, sonra o dokuyu ekrana bas"
  /// akışı mümkün olur (mini harita, son işlem efektleri, iz efekti).
  ///
  /// @param width  Genişlik (piksel, > 0).
  /// @param height Yükseklik (piksel, > 0).
  /// @param filter Sonucu örneklerken kullanılacak filtre.
  /// @return Hedef tanımlayıcısı; desteklenmiyorsa veya başarısızsa
  ///         @ref kInvalidRenderTarget.
  virtual RenderTargetHandle CreateRenderTarget(int32_t width, int32_t height,
                                                TextureFilter filter) {
    (void)width;
    (void)height;
    (void)filter;
    return kInvalidRenderTarget;
  }

  /// @brief Hedefi ve ona ait tüm kaynakları serbest bırak.
  virtual void DestroyRenderTarget(RenderTargetHandle handle) { (void)handle; }

  /// @brief Hedefin içeriğini örneklemek için kullanılacak texture.
  ///
  /// @return @ref DrawTextured'a verilebilecek tanımlayıcı; hedef geçersizse
  ///         @ref kInvalidTexture.
  [[nodiscard]] virtual TextureHandle GetRenderTargetTexture(
      RenderTargetHandle handle) const {
    (void)handle;
    return kInvalidTexture;
  }

  /// @brief Sonraki çizimlerin gideceği hedefi seç.
  ///
  /// @param handle Hedef; @ref kInvalidRenderTarget verilirse çizim ekrana
  ///        döner.
  /// @return Geçiş yapıldıysa `true`.
  ///
  /// @warning Bir hedefe çizerken o hedefin kendi texture'ından okumak
  ///          tanımsızdır. Önce hedeften çıkın, sonra örnekleyin.
  virtual bool SetRenderTarget(RenderTargetHandle handle) {
    (void)handle;
    return false;
  }

  /// @brief Hedefin piksellerini ana belleğe oku.
  ///
  /// Formatı sıkı paketlenmiş, doğrusal RGBA8'dir ve satırlar
  /// yukarıdan aşağı sıralanır — iki backend'de de birebir aynı. Bu,
  /// hedeflerin ekran yüzeyinin formatını devralmamasının sebebidir: yüzey
  /// formatı sürücüye göre BGRA veya sRGB olabilir ve o zaman "iki backend
  /// aynı çizimde farklı bayt üretti" bulgusu gerçek bir hatayı değil,
  /// yalnızca format farkını gösterirdi.
  ///
  /// Çağrı bloklar: GPU'nun işi bitene kadar bekler. Kare döngüsünde
  /// değil, ekran görüntüsü alma ve test gibi yerlerde kullanılmalıdır.
  ///
  /// @param handle Okunacak hedef.
  /// @param out_rgba `width * height * 4` bayt kapasiteli tampon.
  /// @param byte_capacity `out_rgba`'nın bayt kapasitesi; yetersizse çağrı
  ///        başarısız olur (taşma yerine hata).
  /// @return Başarıda `true`.
  virtual bool ReadRenderTarget(RenderTargetHandle handle, uint8_t* out_rgba,
                                std::size_t byte_capacity) {
    (void)handle;
    (void)out_rgba;
    (void)byte_capacity;
    return false;
  }

  // --- Transform ---

  /// @brief Backend tipini döndür — projeksiyon yönü için kullanılır.
  [[nodiscard]] virtual RendererBackend GetBackend() const = 0;

  /// @brief Ortografik projeksiyon matrisini ayarla (4x4, sütun-major).
  virtual void SetProjectionMatrix(const float* mat4) = 0;

  /// @brief Model dönüşüm matrisini ayarla
  /// (3x3, sütun-major — glm::mat3 / glm::value_ptr düzeni).
  ///
  /// @note @ref Painter bu matrisi kare başına bir kez ve daima birim
  ///       olarak yazar: transform CPU'da vertex'lere gömülüyor
  ///       (bkz. `examples/benchmarks/README.md`). Arayüzde
  ///       kalmasının sebebi, kendi backend'ini yazanların matris yolunu
  ///       hâlâ kullanabilmesi ve mevcut shader'ların değişmemesidir.
  virtual void SetModelMatrix(const float* mat3) = 0;

  // --- Profilleme (isteğe bağlı) ---

  /// @brief Son tamamlanan karenin GPU süresi (milisaniye).
  ///
  /// Saf sanal değildir: ölçüm desteklemeyen backend'ler ve tüketici
  /// tarafındaki basit implementasyonlar bunu görmezden gelebilir.
  /// Varsayılan 0.0 = "ölçülmüyor".
  ///
  /// Sonuç bir kare gecikmeli olabilir; implementasyon, sorgu sonucunu
  /// beklemek yerine hazır olan en son değeri döndürmelidir (bekleme,
  /// ölçtüğü şeyi bozan bir CPU–GPU senkronizasyonu yaratır).
  [[nodiscard]] virtual double GetLastGpuFrameMs() const { return 0.0; }
};

/// @brief Seçilen backend için IRenderer örneği oluştur.
SDLPAINTER_API std::unique_ptr<IRenderer> CreateRenderer(
    RendererBackend backend);

}  // namespace sdl_painter
