#include "vk_frame_sync.h"

#include <spdlog/spdlog.h>

#include "vk_check.h"
#include "vk_context.h"

namespace sdl_painter {

VkFrameSync::~VkFrameSync() { Shutdown(); }

bool VkFrameSync::Initialize(VkContext* context, uint32_t swapchain_image_count) {
  mContext = context;
  mSwapchainImageCount = swapchain_image_count;
  if (!CreateCommandPool()) return false;
  if (!AllocateCommandBuffers()) return false;
  if (!CreateSyncObjects()) return false;
  spdlog::info(
      "Frame sync initialized ({} frames in flight, {} image semaphores).",
      kMaxFramesInFlight, mSwapchainImageCount);
  return true;
}

void VkFrameSync::Shutdown() {
  if (!mContext) return;
  VkDevice device = mContext->GetDevice();
  if (device == VK_NULL_HANDLE) {
    mContext = nullptr;
    return;
  }

  // Defansif önlem — fence/semaphore'ları yıkmadan önce GPU'nun tüm işlerini
  // bitirmesini bekle. VulkanRenderer::Shutdown zaten vkDeviceWaitIdle çağırır
  // ama VkFrameSync tek başına da güvenli olmalı.
  vkDeviceWaitIdle(device);

  for (auto f : mInFlight) {
    if (f != VK_NULL_HANDLE) vkDestroyFence(device, f, nullptr);
  }
  mInFlight.clear();

  for (auto s : mRenderFinished) {
    if (s != VK_NULL_HANDLE) vkDestroySemaphore(device, s, nullptr);
  }
  mRenderFinished.clear();

  for (auto s : mImageAvailable) {
    if (s != VK_NULL_HANDLE) vkDestroySemaphore(device, s, nullptr);
  }
  mImageAvailable.clear();

  mCommandBuffers.clear();  // pool ile birlikte temizlenir
  if (mCommandPool != VK_NULL_HANDLE) {
    vkDestroyCommandPool(device, mCommandPool, nullptr);
    mCommandPool = VK_NULL_HANDLE;
  }
  mContext = nullptr;
}

bool VkFrameSync::CreateCommandPool() {
  VkDevice device = mContext->GetDevice();
  VkCommandPoolCreateInfo ci{};
  ci.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
  ci.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
  ci.queueFamilyIndex = mContext->GetGraphicsQueueFamily();
  VkResult res = vkCreateCommandPool(device, &ci, nullptr, &mCommandPool);
  if (res != VK_SUCCESS) {
    spdlog::error("vkCreateCommandPool failed: {}",
                  vk_detail::VkResultToString(res));
    return false;
  }
  return true;
}

bool VkFrameSync::AllocateCommandBuffers() {
  VkDevice device = mContext->GetDevice();
  mCommandBuffers.resize(kMaxFramesInFlight);

  VkCommandBufferAllocateInfo ai{};
  ai.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  ai.commandPool = mCommandPool;
  ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  ai.commandBufferCount = kMaxFramesInFlight;
  VkResult res = vkAllocateCommandBuffers(device, &ai, mCommandBuffers.data());
  if (res != VK_SUCCESS) {
    spdlog::error("vkAllocateCommandBuffers failed: {}",
                  vk_detail::VkResultToString(res));
    return false;
  }
  return true;
}

bool VkFrameSync::CreateSyncObjects() {
  VkDevice device = mContext->GetDevice();

  // imageAvailable: swapchain image sayısı kadar — acquire slot ile indekslenir.
  // Her image için ayrı semaphore tutarak presentation engine çakışması önlenir.
  mImageAvailable.resize(mSwapchainImageCount, VK_NULL_HANDLE);
  // renderFinished: swapchain image sayısı kadar — image_index ile indekslenir.
  // Presentation engine signal semaphore'u image'a bağlar; image başına ayrı
  // semaphore olmadan validation "semaphore may still be in use" uyarısı verir.
  mRenderFinished.resize(mSwapchainImageCount, VK_NULL_HANDLE);
  // inFlight fence: frames-in-flight sayısı kadar — frame_index ile (CPU-GPU pacing).
  mInFlight.resize(kMaxFramesInFlight, VK_NULL_HANDLE);

  VkSemaphoreCreateInfo sem_ci{};
  sem_ci.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

  VkFenceCreateInfo fence_ci{};
  fence_ci.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
  fence_ci.flags = VK_FENCE_CREATE_SIGNALED_BIT;

  for (uint32_t i = 0; i < mSwapchainImageCount; ++i) {
    if (vkCreateSemaphore(device, &sem_ci, nullptr, &mImageAvailable[i]) !=
        VK_SUCCESS) {
      spdlog::error("imageAvailable semaphore creation failed at image {}.", i);
      return false;
    }
    if (vkCreateSemaphore(device, &sem_ci, nullptr, &mRenderFinished[i]) !=
        VK_SUCCESS) {
      spdlog::error("renderFinished semaphore creation failed at image {}.", i);
      return false;
    }
  }
  for (uint32_t i = 0; i < kMaxFramesInFlight; ++i) {
    if (vkCreateFence(device, &fence_ci, nullptr, &mInFlight[i]) !=
        VK_SUCCESS) {
      spdlog::error("inFlight fence creation failed at frame {}.", i);
      return false;
    }
  }
  return true;
}

}  // namespace sdl_painter
