#include "sdl_painter/render_target.h"

#include "sdl_painter/painter.h"

#include <utility>

namespace sdl_painter {

RenderTarget::~RenderTarget() {
  Reset();
}

RenderTarget::RenderTarget(RenderTarget&& other) noexcept
    : mOwner(other.mOwner),
      mHandle(other.mHandle),
      mWidth(other.mWidth),
      mHeight(other.mHeight),
      mPainter(std::move(other.mPainter)) {
  other.mOwner = nullptr;
  other.mHandle = kInvalidRenderTarget;
  other.mWidth = 0;
  other.mHeight = 0;
}

RenderTarget& RenderTarget::operator=(RenderTarget&& other) noexcept {
  if (this != &other) {
    Reset();
    mOwner = other.mOwner;
    mHandle = other.mHandle;
    mWidth = other.mWidth;
    mHeight = other.mHeight;
    mPainter = std::move(other.mPainter);
    other.mOwner = nullptr;
    other.mHandle = kInvalidRenderTarget;
    other.mWidth = 0;
    other.mHeight = 0;
  }
  return *this;
}

void RenderTarget::Reset() noexcept {
  if (mHandle != kInvalidRenderTarget) {
    // Painter hala yasiyorsa once ona haber ver: bu hedef o an bagliysa
    // biriken cizimleri hedef dururken flush edip ekrana doner. Aksi halde
    // Painter olu bir handle'i "aktif hedef" saymayi surdurur ve kare
    // sonuna kadar hedefin viewport'u/projeksiyonuyla ekrana cizer.
    Painter* painter = mPainter ? *mPainter : nullptr;
    if (painter != nullptr) {
      painter->OnRenderTargetDestroyed(mHandle);
    }
    // Painter yikildiysa renderer da yikilmistir ve kaynaklari zaten
    // birakmistir; mOwner uzerinden cagri dangling pointer kullanirdi.
    const bool kOwnerAlive = mPainter == nullptr || painter != nullptr;
    if (mOwner != nullptr && kOwnerAlive) {
      mOwner->DestroyRenderTarget(mHandle);
    }
  }
  mOwner = nullptr;
  mHandle = kInvalidRenderTarget;
  mWidth = 0;
  mHeight = 0;
  mPainter.reset();
}

}  // namespace sdl_painter
