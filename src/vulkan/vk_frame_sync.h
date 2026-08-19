#pragma once

#include <cstdint>
#include <vector>
#include <vulkan/vulkan.h>

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

  /// @brief Acquire semaphore'u — **frame_index** ile indekslenir.
  ///
  /// Yeniden kullanım güvenliği in-flight fence beklemesinden gelir: frame N
  /// başında `mInFlight[N % kMaxFramesInFlight]` beklendiğinde, aynı slotu
  /// kullanan önceki submit tamamlanmış ve semaphore beklemesi gerçekleşmiş
  /// olur; semaphore unsignaled duruma dönmüştür.
  VkSemaphore GetImageAvailableSemaphore(uint32_t frame_index) const {
    return mImageAvailable[frame_index];
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
  std::vector<VkSemaphore>
      mImageAvailable;  ///< frame_index ile indekslenir (kMaxFramesInFlight kadar)
  std::vector<VkSemaphore>
      mRenderFinished;  ///< image_index ile indekslenir (swapchain image count kadar)
  std::vector<VkFence> mInFlight;  ///< frame_index ile indekslenir
};

}  // namespace sdl_painter
