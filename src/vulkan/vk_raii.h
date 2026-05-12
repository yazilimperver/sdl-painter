#pragma once

#include <vulkan/vulkan.h>

#include <utility>

namespace sdl_painter {

/// @brief Vulkan handle'ı için ince bir RAII sarmalayıcı.
///
/// Deleter'ın iki farklı imzası olabilir:
///   - void Fn(VkDevice, T, const VkAllocationCallbacks*)
///   - void Fn(VkInstance, T, const VkAllocationCallbacks*)
/// Her iki durum için de şablon overloads seti sağlanır.
///
/// Taşınabilir (movable), kopyalanamaz (non-copyable).
template <typename Handle, typename Owner, typename Deleter>
class VkScoped {
 public:
  VkScoped() = default;

  VkScoped(Owner owner, Handle handle, Deleter deleter)
      : mOwner(owner), mHandle(handle), mDeleter(deleter) {}

  ~VkScoped() { Reset(); }

  VkScoped(const VkScoped&) = delete;
  VkScoped& operator=(const VkScoped&) = delete;

  VkScoped(VkScoped&& other) noexcept
      : mOwner(other.mOwner),
        mHandle(other.mHandle),
        mDeleter(other.mDeleter) {
    other.mHandle = VK_NULL_HANDLE;
  }

  VkScoped& operator=(VkScoped&& other) noexcept {
    if (this != &other) {
      Reset();
      mOwner = other.mOwner;
      mHandle = other.mHandle;
      mDeleter = other.mDeleter;
      other.mHandle = VK_NULL_HANDLE;
    }
    return *this;
  }

  /// @brief Sahiplenilen handle'ı serbest bırak.
  void Reset() {
    if (mHandle != VK_NULL_HANDLE && mOwner != VK_NULL_HANDLE && mDeleter) {
      mDeleter(mOwner, mHandle, nullptr);
      mHandle = VK_NULL_HANDLE;
    }
  }

  /// @brief Handle sahipliğini serbest bırakmadan bırak.
  Handle Release() {
    Handle tmp = mHandle;
    mHandle = VK_NULL_HANDLE;
    return tmp;
  }

  Handle Get() const { return mHandle; }
  operator Handle() const { return mHandle; }
  bool IsValid() const { return mHandle != VK_NULL_HANDLE; }

 private:
  Owner mOwner{VK_NULL_HANDLE};
  Handle mHandle{VK_NULL_HANDLE};
  Deleter mDeleter{nullptr};
};

}  // namespace sdl_painter
