#pragma once

#include <cstdint>
#include <vector>
#include <vulkan/vulkan.h>

namespace sdl_painter {

class VkContext;

/// @brief Swapchain, render pass ve framebuffer sahipliği.
///
/// Pencere resize edildiğinde `Recreate(w, h)` ile yeniden inşa edilir.
/// Phase 5a için tek bir render pass (renk attachment, load=CLEAR, store=STORE)
/// kullanılır. Phase 5b'den itibaren pipeline bu render pass'a bağlanır.
class VkSwapchain {
 public:
  VkSwapchain() = default;
  ~VkSwapchain();

  VkSwapchain(const VkSwapchain&) = delete;
  VkSwapchain& operator=(const VkSwapchain&) = delete;

  /// @brief İlk kez swapchain'i oluştur.
  bool Initialize(VkContext* context, uint32_t width, uint32_t height);

  /// @brief Tüm swapchain kaynaklarını serbest bırak.
  void Shutdown();

  /// @brief Pencere boyutu değiştiğinde swapchain'i yeniden inşa et.
  bool Recreate(uint32_t width, uint32_t height);

  VkSwapchainKHR GetSwapchain() const { return mSwapchain; }
  VkRenderPass GetRenderPass() const { return mRenderPass; }
  VkFormat GetImageFormat() const { return mImageFormat; }
  VkExtent2D GetExtent() const { return mExtent; }
  uint32_t GetImageCount() const {
    return static_cast<uint32_t>(mImages.size());
  }
  VkFramebuffer GetFramebuffer(uint32_t index) const {
    return mFramebuffers[index];
  }

 private:
  bool CreateSwapchain(uint32_t width, uint32_t height);
  bool CreateSwapchainWithOld(uint32_t width, uint32_t height,
                              VkSwapchainKHR old_swapchain);
  bool CreateImageViews();
  bool CreateRenderPass();
  bool CreateFramebuffers();
  void DestroySwapchainResources();

  VkContext* mContext{nullptr};
  VkSwapchainKHR mSwapchain{VK_NULL_HANDLE};
  VkFormat mImageFormat{VK_FORMAT_UNDEFINED};
  VkExtent2D mExtent{0, 0};

  std::vector<VkImage> mImages;
  std::vector<VkImageView> mImageViews;
  std::vector<VkFramebuffer> mFramebuffers;

  VkRenderPass mRenderPass{VK_NULL_HANDLE};
};

}  // namespace sdl_painter
