#pragma once

#include "sdl_painter/renderer.h"

#include <memory>
#include <utility>

namespace sdl_painter {

/// @brief RAII sarmalayıcı: texture yaşam döngüsünü yönetir.
///
/// Texture, onu oluşturan renderer'ın @ref IRenderer::LifetimeToken
/// belirtecini saklar. Renderer (dolayısıyla Painter) Texture'dan önce
/// yıkılırsa Texture geçersiz görünür (@ref IsValid `false`, @ref Owner
/// `nullptr`, @ref Handle `kInvalidTexture`) ve @ref Reset renderer'a
/// dokunmaz: GPU kaynakları renderer kapanırken zaten bırakılmıştır. Aynı
/// koruma @ref Image ve @ref Font içindeki Texture'lar için de geçerlidir.
class Texture {
 public:
  explicit Texture(IRenderer* renderer = nullptr)
      : mRenderer(renderer), mAlive(TokenOf(renderer)) {}
  Texture(IRenderer* renderer, TextureHandle handle)
      : mRenderer(renderer), mHandle(handle), mAlive(TokenOf(renderer)) {}

  ~Texture() { Reset(); }

  // Non-copyable
  Texture(const Texture&) = delete;
  Texture& operator=(const Texture&) = delete;

  // Movable
  Texture(Texture&& other) noexcept
      : mRenderer(other.mRenderer),
        mHandle(other.mHandle),
        mAlive(std::move(other.mAlive)) {
    other.mRenderer = nullptr;
    other.mHandle = kInvalidTexture;
  }

  Texture& operator=(Texture&& other) noexcept {
    if (this != &other) {
      Reset();
      mRenderer = other.mRenderer;
      mHandle = other.mHandle;
      mAlive = std::move(other.mAlive);
      other.mRenderer = nullptr;
      other.mHandle = kInvalidTexture;
    }
    return *this;
  }

  /// @brief Texture'ı serbest bırak.
  ///
  /// Renderer artık yoksa yalnızca tanımlayıcıyı unutur.
  void Reset() {
    if (OwnerAlive() && mHandle != kInvalidTexture) {
      mRenderer->DestroyTexture(mHandle);
    }
    mHandle = kInvalidTexture;
    mRenderer = nullptr;
    mAlive.reset();
  }

  /// @brief Ham handle'ı al; renderer yıkılmışsa `kInvalidTexture`.
  [[nodiscard]] TextureHandle Handle() const noexcept {
    return OwnerAlive() ? mHandle : kInvalidTexture;
  }

  /// @brief Texture'ı oluşturan renderer; yüklenmemişse ya da renderer
  ///        yıkılmışsa `nullptr`.
  [[nodiscard]] IRenderer* Owner() const noexcept {
    return OwnerAlive() ? mRenderer : nullptr;
  }

  /// @brief Geçerli bir texture mı? Renderer yıkılmışsa `false`.
  [[nodiscard]] bool IsValid() const noexcept {
    return mHandle != kInvalidTexture && OwnerAlive();
  }

 private:
  static std::weak_ptr<const void> TokenOf(const IRenderer* renderer) noexcept {
    return renderer != nullptr ? renderer->LifetimeToken()
                               : std::weak_ptr<const void>{};
  }

  /// @brief Sahip renderer hâlâ yaşıyor mu?
  ///
  /// Yalnızca işaretçiye bakmak yetmez: ölen renderer'ın adresinde yeni bir
  /// renderer oluşabilir ve eski tanımlayıcı ona aitmiş gibi görünür.
  [[nodiscard]] bool OwnerAlive() const noexcept {
    return mRenderer != nullptr && !mAlive.expired();
  }

  IRenderer* mRenderer{nullptr};
  TextureHandle mHandle{kInvalidTexture};
  std::weak_ptr<const void> mAlive;
};

}  // namespace sdl_painter
