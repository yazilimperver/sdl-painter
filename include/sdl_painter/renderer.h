#pragma once

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

  // --- Çizim primitifleri (tessellated vertex'ler) ---

  /// @brief Üçgenler çiz. Vertex listesi 3'ün katı olmalı. Renk vertex'te taşınır.
  virtual void DrawTriangles(const std::vector<Vertex>& vertices) = 0;

  // --- Texture işlemleri ---

  /// @brief Ham piksel verisinden texture oluştur. Başarısızlıkta kInvalidTexture döner.
  virtual TextureHandle CreateTexture(const uint8_t* data, int32_t width,
                                      int32_t height, int32_t channels) = 0;

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
  /// @param data Sıkı paketlenmiş **RGBA8** piksel verisi
  ///        (`width * height * 4` bayt).
  ///
  /// @warning Kare ortasında çağrılabilir; ancak o kare içinde **daha önce**
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

  // --- Transform ---

  /// @brief Backend tipini döndür — projeksiyon yönü için kullanılır.
  [[nodiscard]] virtual RendererBackend GetBackend() const = 0;

  /// @brief Ortografik projeksiyon matrisini ayarla (4x4, sütun-major).
  virtual void SetProjectionMatrix(const float* mat4) = 0;

  /// @brief Model dönüşüm matrisini ayarla
  /// (3x3, sütun-major — glm::mat3 / glm::value_ptr düzeni).
  virtual void SetModelMatrix(const float* mat3) = 0;
};

/// @brief Seçilen backend için IRenderer örneği oluştur.
std::unique_ptr<IRenderer> CreateRenderer(RendererBackend backend);

}  // namespace sdl_painter
