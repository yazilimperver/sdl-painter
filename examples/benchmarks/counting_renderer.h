#pragma once

/// @file counting_renderer.h
/// @brief Ölçüm amaçlı IRenderer sarmalayıcısı (decorator).
///
/// Kütüphaneye hiçbir sayaç eklemeden batch verimliliğini ölçmeyi sağlar:
/// `Painter(std::unique_ptr<IRenderer>, w, h)` ctor'u hazır bir renderer'ı
/// kabul ettiği için, gerçek renderer'ı bu sınıfla sarmalayıp Painter'a
/// vermek yeterlidir. Böylece `RenderBatcher::Flush()` her tetiklendiğinde
/// üretilen draw call sayısı doğrudan görünür olur.
///
/// @note `inner` nullptr olabilir — o zaman hiçbir GPU işi yapılmaz ve
///       yalnızca Painter + Tessellator + RenderBatcher CPU maliyeti ile
///       çağrı sayıları ölçülür.

#include "sdl_painter/renderer.h"
#include "sdl_painter/vertex.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <utility>
#include <vector>

namespace sdl_painter {
struct Color;
}

namespace bench {

/// @brief Bir kare boyunca biriken renderer çağrı sayaçları.
struct RendererStats {
  uint64_t draw_calls{0};        ///< DrawTriangles + DrawTextured toplamı.
  uint64_t basic_calls{0};       ///< DrawTriangles çağrısı sayısı.
  uint64_t textured_calls{0};    ///< DrawTextured çağrısı sayısı.
  uint64_t vertices{0};          ///< Gönderilen toplam vertex sayısı.
  uint64_t model_uploads{0};     ///< SetModelMatrix (uniform yükleme) sayısı.
  uint64_t opacity_uploads{0};   ///< SetOpacity çağrısı sayısı.
  uint64_t scissor_changes{0};   ///< SetScissor + ClearScissor sayısı.

  void Reset() noexcept { *this = RendererStats{}; }
};

/// @brief Çağrıları sayan ve isteğe bağlı olarak gerçek renderer'a ileten
///        IRenderer sarmalayıcısı.
class CountingRenderer final : public sdl_painter::IRenderer {
 public:
  /// @param inner Sarmalanan gerçek renderer; `nullptr` ise yalnızca sayım
  ///        yapılır (GPU işi yok).
  /// @param backend `inner` yokken raporlanacak backend tipi.
  explicit CountingRenderer(
      std::unique_ptr<IRenderer> inner,
      sdl_painter::RendererBackend backend = sdl_painter::RendererBackend::kOpenGL)
      : mInner(std::move(inner)), mBackend(backend) {}

  [[nodiscard]] const RendererStats& Stats() const noexcept { return mStats; }
  void ResetStats() noexcept { mStats.Reset(); }

  /// @brief Bir sonraki karede, sunumdan (swap) önce çağrılacak işlev.
  ///
  /// Ekran görüntüsü almanın doğru anı burasıdır: `Painter::End()` önce
  /// batch'i boşaltır, sonra `EndFrame()` çağırır; asıl `SDL_GL_SwapWindow`
  /// ise sarmalanan renderer'ın `EndFrame`'indedir. Bu kanca ikisinin
  /// arasına girer, yani arka tampon hâlâ okunabilir durumdadır.
  /// Tek seferliktir; çağrıldıktan sonra temizlenir.
  void CaptureBeforeNextPresent(std::function<void()> fn) {
    mBeforePresent = std::move(fn);
  }

  // --- Yaşam döngüsü ---

  bool Initialize(SDL_Window* window) override {
    return mInner ? mInner->Initialize(window) : true;
  }
  void Shutdown() override {
    if (mInner) {
      mInner->Shutdown();
    }
  }
  void BeginFrame() override {
    if (mInner) {
      mInner->BeginFrame();
    }
  }
  void EndFrame() override {
    if (mBeforePresent) {
      auto fn = std::move(mBeforePresent);
      mBeforePresent = nullptr;
      fn();
    }
    if (mInner) {
      mInner->EndFrame();
    }
  }

  // --- Durum ---

  void SetViewport(int32_t x, int32_t y, int32_t width,
                   int32_t height) override {
    if (mInner) {
      mInner->SetViewport(x, y, width, height);
    }
  }
  void SetScissor(int32_t x, int32_t y, int32_t width, int32_t height) override {
    ++mStats.scissor_changes;
    if (mInner) {
      mInner->SetScissor(x, y, width, height);
    }
  }
  void ClearScissor() override {
    ++mStats.scissor_changes;
    if (mInner) {
      mInner->ClearScissor();
    }
  }
  void Clear(const sdl_painter::Color& color) override {
    if (mInner) {
      mInner->Clear(color);
    }
  }
  void SetOpacity(float alpha) override {
    ++mStats.opacity_uploads;
    if (mInner) {
      mInner->SetOpacity(alpha);
    }
  }

  // --- Çizim ---

  void DrawTriangles(const std::vector<sdl_painter::Vertex>& vertices) override {
    ++mStats.draw_calls;
    ++mStats.basic_calls;
    mStats.vertices += vertices.size();
    if (mInner) {
      mInner->DrawTriangles(vertices);
    }
  }
  void DrawTextured(const std::vector<sdl_painter::TexturedVertex>& vertices,
                    sdl_painter::TextureHandle texture) override {
    ++mStats.draw_calls;
    ++mStats.textured_calls;
    mStats.vertices += vertices.size();
    if (mInner) {
      mInner->DrawTextured(vertices, texture);
    }
  }

  // --- Texture ---

  sdl_painter::TextureHandle CreateTexture(const uint8_t* data, int32_t width,
                                           int32_t height,
                                           int32_t channels) override {
    if (mInner) {
      return mInner->CreateTexture(data, width, height, channels);
    }
    // Sahte fakat geçerli handle — GlyphAtlas/Image kInvalidTexture'ı hata
    // sayar, o yüzden 0 dönmemek gerekir.
    return ++mFakeTexture;
  }
  void UpdateTexture(sdl_painter::TextureHandle handle, int32_t x, int32_t y,
                     int32_t width, int32_t height,
                     const uint8_t* data) override {
    if (mInner) {
      mInner->UpdateTexture(handle, x, y, width, height, data);
    }
  }
  void DestroyTexture(sdl_painter::TextureHandle handle) override {
    if (mInner) {
      mInner->DestroyTexture(handle);
    }
  }

  // --- Transform ---

  [[nodiscard]] sdl_painter::RendererBackend GetBackend() const override {
    return mInner ? mInner->GetBackend() : mBackend;
  }
  void SetProjectionMatrix(const float* mat4) override {
    if (mInner) {
      mInner->SetProjectionMatrix(mat4);
    }
  }
  void SetModelMatrix(const float* mat3) override {
    ++mStats.model_uploads;
    if (mInner) {
      mInner->SetModelMatrix(mat3);
    }
  }

 private:
  std::unique_ptr<IRenderer> mInner;
  sdl_painter::RendererBackend mBackend;
  RendererStats mStats;
  sdl_painter::TextureHandle mFakeTexture{0};
  std::function<void()> mBeforePresent;
};

}  // namespace bench
