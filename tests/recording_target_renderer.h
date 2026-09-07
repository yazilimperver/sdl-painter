#pragma once

/// @file recording_target_renderer.h
/// @brief Render hedeflerini uygulayan ve *çizim anındaki* GPU durumunu
///        kaydeden test renderer'ı.
///
/// Mevcut @ref sdl_painter::MockRenderer hedef metotlarını uygulamıyor;
/// @ref sdl_painter::IRenderer içindeki varsayılanlar `kInvalidRenderTarget` /
/// `false` döndüğü için hedefe dayanan senaryolar onunla kurulamıyor
/// (hedef oluşturma başarısız olur ve asıl hata hiç sınanmaz).
///
/// Buradaki üç tasarım kararı testlerin değerini belirliyor:
///
/// 1. **Çizim anı anlık görüntüsü.** Her çizim çağrısında o andaki hedef,
///    viewport, scissor ve projeksiyon kopyalanır. "SetScissor kaç kez
///    çağrıldı" sorusu bir kırpma hatasını yakalayamaz — çağrı sayısı
///    doğruyken değer yanlış olabilir. "Çizim hangi scissor ile gitti"
///    sorusu yakalar.
/// 2. **Kare politikası.** `BeginFrame`'de aktif hedefi koruyan (OpenGL
///    benzeri) ve ekrana dönen (Vulkan benzeri) iki mod. Painter'ın kare
///    sınırı sözleşmesini backend davranışına bırakması ancak iki davranış
///    yan yana konunca görünür olur.
/// 3. **Yaşam süresi günlüğü.** @ref LifetimeJournal renderer'dan uzun yaşar
///    (test kapsamında durur, renderer yalnızca ham işaretçi tutar). Renderer
///    yok edildikten sonra gelen `DestroyTexture` çağrıları ayrıca sayılır;
///    yok edilmiş renderer'a test tarafından erişilmez.
///
/// Mevcut `mock_renderer.h` değiştirilmedi: oradaki davranışa dayanan çok
/// sayıda test var ve bu dosya yalnızca ekleme yapıyor.

#include "sdl_painter/color.h"
#include "sdl_painter/renderer.h"
#include "sdl_painter/vertex.h"

#include <array>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "mock_renderer.h"

namespace sdl_painter::testing {

/// @brief Renderer'dan uzun yaşayan kaynak ömrü kaydı.
///
/// Testin kapsamında (stack'te) durur; renderer buna yalnızca ham işaretçiyle
/// bakar. Böylece renderer yok edildikten sonra günlüğe yazmak güvenlidir —
/// günlük hâlâ yaşıyordur.
struct LifetimeJournal {
  /// @brief `CreateTexture` çağrı sayısı.
  int32_t textures_created{0};

  /// @brief Renderer yaşarken gelen `DestroyTexture` çağrı sayısı.
  int32_t textures_destroyed{0};

  /// @brief Renderer'ın yıkıcısı çalıştı mı?
  bool renderer_destroyed{false};

  /// @brief `Shutdown` çağrıldı mı?
  bool shutdown_called{false};

  /// @brief Renderer yıkılırken hâlâ serbest bırakılmamış texture sayısı.
  ///
  /// Sıfırdan büyük olması, renderer'a bağlı kaynakların sahibinden önce
  /// yok edildiği anlamına gelir.
  int32_t live_textures_at_death{0};

  /// @brief Renderer yok edildikten sonr gelen `DestroyTexture` sayısı.
  ///
  /// Sıfırdan büyük olması doğrudan use-after-free kanıtıdır.
  int32_t destroy_after_death{0};
};

/// @brief `BeginFrame`'in aktif hedefe ne yaptığı.
enum class FramePolicy : uint8_t {
  /// @brief Aktif hedef kare sınırında korunur (OpenGL'de FBO bağlı kalır).
  kKeepTarget,
  /// @brief Kare başında ekrana dönülür (Vulkan'da swapchain pass'i başlar).
  kResetToScreen,
};

/// @brief Bir çizim çağrısı ve o an yürürlükte olan GPU durumu.
struct DrawSnapshot {
  /// @brief "DrawTriangles" veya "DrawTextured".
  std::string kind;

  /// @brief Çizimin gittiği hedef (`kInvalidRenderTarget` = ekran).
  RenderTargetHandle target{kInvalidRenderTarget};

  /// @brief Çizim anındaki viewport.
  MockRenderer::RectCall viewport{};

  /// @brief Çizim anında scissor etkin miydi?
  bool has_scissor{false};

  /// @brief Çizim anındaki scissor kutusu (yalnızca @ref has_scissor ise).
  MockRenderer::RectCall scissor{};

  /// @brief Çizim anındaki projeksiyon matrisi (sütun-major).
  std::array<float, 16> projection{};

  /// @brief Gönderilen vertex'ler (renkleriyle birlikte).
  std::vector<Vertex> vertices;

  /// @brief Dokulu çizimde gönderilen vertex'ler.
  std::vector<TexturedVertex> textured_vertices;

  /// @brief Dokulu çizimde kullanılan texture.
  TextureHandle texture{kInvalidTexture};
};

/// @brief Hedefleri uygulayan, çizim anını kaydeden test renderer'ı.
class RecordingTargetRenderer : public MockRenderer {
 public:
  RecordingTargetRenderer() = default;

  ~RecordingTargetRenderer() override {
    if (mJournal != nullptr) {
      mJournal->renderer_destroyed = true;
      mJournal->live_textures_at_death =
          mJournal->textures_created - mJournal->textures_destroyed;
    }
  }

  RecordingTargetRenderer(const RecordingTargetRenderer&) = delete;
  RecordingTargetRenderer& operator=(const RecordingTargetRenderer&) = delete;

  /// @brief `BeginFrame`'in aktif hedefe ne yapacağı.
  FramePolicy frame_policy{FramePolicy::kKeepTarget};

  /// @brief Kaynak ömrü günlüğü; renderer'dan uzun yaşamalıdır.
  void SetJournal(LifetimeJournal* journal) { mJournal = journal; }

  /// @brief Çizimler, o andaki GPU durumuyla birlikte.
  std::vector<DrawSnapshot> draws;

  /// @brief Backend'e gerçekten iletilen hedef geçişleri (sırayla).
  std::vector<RenderTargetHandle> set_target_calls;

  /// @brief Backend'den gerçekten okunan hedefler (sırayla).
  std::vector<RenderTargetHandle> read_target_calls;

  /// @brief Yürürlükteki hedef — testin doğrudan sorabilmesi için.
  [[nodiscard]] RenderTargetHandle ActiveTarget() const noexcept {
    return mActiveTarget;
  }

  void BeginFrame() override {
    if (frame_policy == FramePolicy::kResetToScreen) {
      mActiveTarget = kInvalidRenderTarget;
    }
    MockRenderer::BeginFrame();
  }

  void Shutdown() override {
    if (mJournal != nullptr) {
      mJournal->shutdown_called = true;
    }
    MockRenderer::Shutdown();
  }

  void SetViewport(int32_t x, int32_t y, int32_t w, int32_t h) override {
    mViewport = {x, y, w, h};
    MockRenderer::SetViewport(x, y, w, h);
  }

  void SetScissor(int32_t x, int32_t y, int32_t w, int32_t h) override {
    mScissor = {x, y, w, h};
    mHasScissor = true;
    MockRenderer::SetScissor(x, y, w, h);
  }

  void ClearScissor() override {
    mHasScissor = false;
    MockRenderer::ClearScissor();
  }

  void DrawTriangles(const std::vector<Vertex>& vertices) override {
    DrawSnapshot snap = Snapshot("DrawTriangles");
    snap.vertices = vertices;
    draws.push_back(std::move(snap));
    MockRenderer::DrawTriangles(vertices);
  }

  void DrawTextured(const std::vector<TexturedVertex>& vertices,
                    TextureHandle texture) override {
    DrawSnapshot snap = Snapshot("DrawTextured");
    snap.textured_vertices = vertices;
    snap.texture = texture;
    draws.push_back(std::move(snap));
    MockRenderer::DrawTextured(vertices, texture);
  }

  using MockRenderer::CreateTexture;

  TextureHandle CreateTexture(const uint8_t* data, int32_t w, int32_t h,
                              int32_t channels) override {
    if (mJournal != nullptr) {
      ++mJournal->textures_created;
    }
    return MockRenderer::CreateTexture(data, w, h, channels);
  }

  void DestroyTexture(TextureHandle handle) override {
    if (mJournal != nullptr) {
      // Yıkıcı çalıştıysa bu çağrı yok edilmiş bir renderer üzerinden
      // geliyordur; ayrı sayılır çünkü doğrudan use-after-free kanıtıdır.
      if (mJournal->renderer_destroyed) {
        ++mJournal->destroy_after_death;
      } else {
        ++mJournal->textures_destroyed;
      }
    }
    MockRenderer::DestroyTexture(handle);
  }

  /// @brief Renderer başına 1'den başlayan handle üretir.
  ///
  /// Numaralandırmanın renderer'a yerel olması bilinçlidir: iki bağımsız
  /// renderer'ın ilk hedefinin aynı sayısal handle'ı alması, sahiplik
  /// testinin (bulgu 9) önkoşuludur.
  RenderTargetHandle CreateRenderTarget(int32_t width, int32_t height,
                                        TextureFilter filter) override {
    (void)filter;
    if (width <= 0 || height <= 0) {
      return kInvalidRenderTarget;
    }
    const RenderTargetHandle kHandle = ++mNextTargetHandle;
    mTargets[kHandle] =
        TargetInfo{width, height,
                   static_cast<TextureHandle>(kTargetTextureBase + kHandle)};
    return kHandle;
  }

  void DestroyRenderTarget(RenderTargetHandle handle) override {
    mTargets.erase(handle);
    if (mActiveTarget == handle) {
      mActiveTarget = kInvalidRenderTarget;
    }
  }

  [[nodiscard]] TextureHandle GetRenderTargetTexture(
      RenderTargetHandle handle) const override {
    const auto kIt = mTargets.find(handle);
    return kIt == mTargets.end() ? kInvalidTexture : kIt->second.texture;
  }

  /// @brief Hedefi seç.
  ///
  /// Yabancı bir renderer'ın handle'ı burada AYIRT EDİLEMEZ — sayısal handle
  /// bu renderer'ın kendi tablosunda da geçerli olabilir. Sahiplik kontrolü
  /// Painter katmanına aittir; testin sınadığı da tam olarak budur.
  bool SetRenderTarget(RenderTargetHandle handle) override {
    if (handle != kInvalidRenderTarget &&
        mTargets.find(handle) == mTargets.end()) {
      return false;
    }
    mActiveTarget = handle;
    set_target_calls.push_back(handle);
    return true;
  }

  bool ReadRenderTarget(RenderTargetHandle handle, uint8_t* out_rgba,
                        std::size_t byte_capacity) override {
    const auto kIt = mTargets.find(handle);
    if (kIt == mTargets.end() || out_rgba == nullptr) {
      return false;
    }
    const std::size_t kNeeded = static_cast<std::size_t>(kIt->second.width) *
                                static_cast<std::size_t>(kIt->second.height) *
                                4U;
    if (byte_capacity < kNeeded) {
      return false;
    }
    for (std::size_t i = 0; i < kNeeded; ++i) {
      out_rgba[i] = 0U;
    }
    read_target_calls.push_back(handle);
    return true;
  }

 private:
  struct TargetInfo {
    int32_t width{0};
    int32_t height{0};
    TextureHandle texture{kInvalidTexture};
  };

  /// @brief Hedef texture'ları normal texture handle'larıyla çakışmasın.
  static constexpr uint32_t kTargetTextureBase = 1000U;

  [[nodiscard]] DrawSnapshot Snapshot(const std::string& kind) const {
    DrawSnapshot snap;
    snap.kind = kind;
    snap.target = mActiveTarget;
    snap.viewport = mViewport;
    snap.has_scissor = mHasScissor;
    snap.scissor = mScissor;
    snap.projection = last_projection;
    return snap;
  }

  LifetimeJournal* mJournal{nullptr};

  std::map<RenderTargetHandle, TargetInfo> mTargets;
  RenderTargetHandle mNextTargetHandle{kInvalidRenderTarget};
  RenderTargetHandle mActiveTarget{kInvalidRenderTarget};

  RectCall mViewport{};
  RectCall mScissor{};
  bool mHasScissor{false};
};

}  // namespace sdl_painter::testing
