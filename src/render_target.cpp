#include "sdl_painter/render_target.h"

#include <utility>

namespace sdl_painter {

RenderTarget::~RenderTarget() {
  Reset();
}

RenderTarget::RenderTarget(RenderTarget&& other) noexcept
    : mOwner(other.mOwner),
      mHandle(other.mHandle),
      mWidth(other.mWidth),
      mHeight(other.mHeight) {
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
    other.mOwner = nullptr;
    other.mHandle = kInvalidRenderTarget;
    other.mWidth = 0;
    other.mHeight = 0;
  }
  return *this;
}

void RenderTarget::Reset() noexcept {
  if (mOwner != nullptr && mHandle != kInvalidRenderTarget) {
    mOwner->DestroyRenderTarget(mHandle);
  }
  mOwner = nullptr;
  mHandle = kInvalidRenderTarget;
  mWidth = 0;
  mHeight = 0;
}

}  // namespace sdl_painter
