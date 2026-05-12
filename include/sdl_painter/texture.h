#pragma once

#include "sdl_painter/renderer.h"

namespace sdl_painter {

/// @brief RAII sarmalayıcı — texture yaşam döngüsünü yönetir.
///
/// @warning Texture, raw `IRenderer*` pointer tutar. Texture'ı oluşturan
/// renderer (ve dolayısıyla Painter), Texture'dan **önce** yıkılmamalıdır.
/// Aksi halde `Reset()` çağrısı dangling renderer üzerinden `DestroyTexture`
/// çağırır — davranış tanımsızdır. Bu sözleşme @ref Image ve @ref Font
/// içindeki Texture'lar için de geçerlidir. v0.2.0'da `weak_ptr` ile
/// güçlendirilmesi planlanıyor.
class Texture {
 public:
  Texture(IRenderer* renderer = nullptr) : mRenderer(renderer) {}
  Texture(IRenderer* renderer, TextureHandle handle)
      : mRenderer(renderer), mHandle(handle) {}

  ~Texture() { Reset(); }

  // Non-copyable
  Texture(const Texture&) = delete;
  Texture& operator=(const Texture&) = delete;

  // Movable
  Texture(Texture&& other) noexcept
      : mRenderer(other.mRenderer), mHandle(other.mHandle) {
    other.mRenderer = nullptr;
    other.mHandle   = kInvalidTexture;
  }

  Texture& operator=(Texture&& other) noexcept {
    if (this != &other) {
      Reset();
      mRenderer       = other.mRenderer;
      mHandle         = other.mHandle;
      other.mRenderer = nullptr;
      other.mHandle   = kInvalidTexture;
    }
    return *this;
  }

  /// @brief Texture'ı serbest bırak.
  void Reset() {
    if (mRenderer && mHandle != kInvalidTexture) {
      mRenderer->DestroyTexture(mHandle);
    }
    mHandle   = kInvalidTexture;
    mRenderer = nullptr;
  }

  /// @brief Ham handle'ı al.
  [[nodiscard]] TextureHandle Handle() const noexcept { return mHandle; }

  /// @brief Texture'ı oluşturan renderer'ı döner (yüklenmemişse nullptr).
  [[nodiscard]] IRenderer* Owner() const noexcept { return mRenderer; }

  /// @brief Geçerli bir texture mı?
  [[nodiscard]] bool IsValid() const noexcept {
    return mHandle != kInvalidTexture;
  }

 private:
  IRenderer* mRenderer{nullptr};
  TextureHandle mHandle{kInvalidTexture};
};

}  // namespace sdl_painter
