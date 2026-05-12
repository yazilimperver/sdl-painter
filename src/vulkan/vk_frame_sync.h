#pragma once

#include <vulkan/vulkan.h>

#include <cstdint>
#include <vector>

namespace sdl_painter {

class VkContext;

/// @brief Frame başına command pool + command buffer + senkronizasyon objeleri.
///
/// Çift buffer mantığıyla (`kMaxFramesInFlight = 2`) CPU frame N+1'i
/// hazırlarken GPU frame N'yi işleyebilir.
class VkFrameSync {
 public:
  static constexpr uint32_t kMaxFramesInFlight = 2;

  VkFrameSync() = default;
  ~VkFrameSync();

  VkFrameSync(const VkFrameSync&) = delete;
  VkFrameSync& operator=(const VkFrameSync&) = delete;

  /// @brief Sync objelerini başlat.
  /// @param swapchain_image_count Acquire semaphore sayısını belirler.
  bool Initialize(VkContext* context, uint32_t swapchain_image_count);
  void Shutdown();

  VkCommandBuffer GetCommandBuffer(uint32_t frame_index) const {
    return mCommandBuffers[frame_index];
  }
  VkCommandPool GetCommandPool() const { return mCommandPool; }

  /// @brief Acquire semaphore'u — image_index ile indekslenir.
  ///
  /// Her swapchain image'ı için ayrı bir semaphore tutulur. Bu sayede
  /// presentation engine'in hâlâ kullandığı bir semaphore yeniden
  /// kullanılmaz ve validation hatası oluşmaz.
  VkSemaphore GetImageAvailableSemaphore(uint32_t image_index) const {
    return mImageAvailable[image_index];
  }
  /// @brief Render finished semaphore'u — image_index ile indekslenir.
  ///
  /// Presentation engine semaphore'u swapchain image'a bağlar; bu yüzden
  /// image başına ayrı semaphore tutulur. Aynı image tekrar acquire edilene
  /// kadar semaphore güvenle yeniden kullanılamaz.
  VkSemaphore GetRenderFinishedSemaphore(uint32_t image_index) const {
    return mRenderFinished[image_index];
  }
  VkFence GetInFlightFence(uint32_t frame_index) const {
    return mInFlight[frame_index];
  }

 private:
  bool CreateCommandPool();
  bool AllocateCommandBuffers();
  bool CreateSyncObjects();

  VkContext* mContext{nullptr};
  uint32_t mSwapchainImageCount{0};
  VkCommandPool mCommandPool{VK_NULL_HANDLE};
  std::vector<VkCommandBuffer> mCommandBuffers;
  std::vector<VkSemaphore> mImageAvailable;  ///< acquire slot ile indekslenir (swapchain image count kadar)
  std::vector<VkSemaphore> mRenderFinished;  ///< image_index ile indekslenir (swapchain image count kadar)
  std::vector<VkFence> mInFlight;            ///< frame_index ile indekslenir
};

}  // namespace sdl_painter
