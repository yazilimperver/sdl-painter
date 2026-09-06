#include "vulkan_render_target.h"

#include <array>
#include <cstring>
#include <spdlog/spdlog.h>

#include "vk_check.h"
#include "vk_context.h"
#include "vk_memory.h"

namespace sdl_painter {

VulkanRenderTarget::~VulkanRenderTarget() {
  Destroy(mDevice);
}

bool VulkanRenderTarget::Create(VkContext* context, VkRenderPass render_pass,
                                VkCommandPool cmd_pool, int32_t width,
                                int32_t height, VkDescriptorSet descriptor_set,
                                VkDescriptorSetLayout descriptor_set_layout,
                                TextureFilter filter) {
  (void)descriptor_set_layout;  // Set zaten bu layout ile tahsis edildi.

  if (context == nullptr || width <= 0 || height <= 0) {
    return false;
  }
  VkDevice device = context->GetDevice();
  mDevice = device;
  mWidth = width;
  mHeight = height;
  mDescriptorSet = descriptor_set;

  if (!CreateImage(device, context->GetPhysicalDevice())) {
    return false;
  }
  if (!CreateViewAndSampler(device, filter)) {
    return false;
  }

  VkFramebufferCreateInfo fb{};
  fb.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
  fb.renderPass = render_pass;
  fb.attachmentCount = 1;
  fb.pAttachments = &mImageView;
  fb.width = static_cast<uint32_t>(width);
  fb.height = static_cast<uint32_t>(height);
  fb.layers = 1;
  VK_CHECK(vkCreateFramebuffer(device, &fb, nullptr, &mFramebuffer));

  // Hicbir sey cizilmeden orneklenmesi gecerli bir kullanim; UNDEFINED
  // layout'undan ornekleme tanimsiz olurdu.
  VkCommandBuffer cmd = BeginOneShot(device, cmd_pool);
  if (cmd == VK_NULL_HANDLE) {
    return false;
  }
  Transition(cmd, mImage, VK_IMAGE_LAYOUT_UNDEFINED,
             VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
  if (!EndOneShot(device, context->GetGraphicsQueue(), cmd_pool, cmd)) {
    return false;
  }

  UpdateDescriptorSet(device);
  return true;
}

bool VulkanRenderTarget::CreateImage(VkDevice device,
                                     VkPhysicalDevice phys_device) {
  VkImageCreateInfo ci{};
  ci.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
  ci.imageType = VK_IMAGE_TYPE_2D;
  ci.format = kColorFormat;
  ci.extent = {static_cast<uint32_t>(mWidth), static_cast<uint32_t>(mHeight),
               1};
  ci.mipLevels = 1;
  ci.arrayLayers = 1;
  ci.samples = VK_SAMPLE_COUNT_1_BIT;
  ci.tiling = VK_IMAGE_TILING_OPTIMAL;
  // Uc kullanim da gerekli: hedefe cizmek (COLOR_ATTACHMENT), sonucu ekrana
  // basmak (SAMPLED), icerigini geri okumak (TRANSFER_SRC).
  ci.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT |
             VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
  ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  ci.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  VK_CHECK(vkCreateImage(device, &ci, nullptr, &mImage));

  VkMemoryRequirements req{};
  vkGetImageMemoryRequirements(device, mImage, &req);

  const uint32_t kType = vk_detail::FindMemoryType(
      phys_device, req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
  if (kType == UINT32_MAX) {
    spdlog::error("VulkanRenderTarget: uygun bellek tipi bulunamadi.");
    return false;
  }

  VkMemoryAllocateInfo ai{};
  ai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  ai.allocationSize = req.size;
  ai.memoryTypeIndex = kType;
  VK_CHECK(vkAllocateMemory(device, &ai, nullptr, &mMemory));
  VK_CHECK(vkBindImageMemory(device, mImage, mMemory, 0));
  return true;
}

bool VulkanRenderTarget::CreateViewAndSampler(VkDevice device,
                                              TextureFilter filter) {
  VkImageViewCreateInfo vi{};
  vi.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
  vi.image = mImage;
  vi.viewType = VK_IMAGE_VIEW_TYPE_2D;
  vi.format = kColorFormat;
  vi.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  vi.subresourceRange.levelCount = 1;
  vi.subresourceRange.layerCount = 1;
  VK_CHECK(vkCreateImageView(device, &vi, nullptr, &mImageView));

  const VkFilter kFilter = (filter == TextureFilter::kNearest)
                               ? VK_FILTER_NEAREST
                               : VK_FILTER_LINEAR;
  VkSamplerCreateInfo si{};
  si.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
  si.magFilter = kFilter;
  si.minFilter = kFilter;
  si.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  si.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  si.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  si.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
  si.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
  VK_CHECK(vkCreateSampler(device, &si, nullptr, &mSampler));
  return true;
}

void VulkanRenderTarget::UpdateDescriptorSet(VkDevice device) const {
  if (mDescriptorSet == VK_NULL_HANDLE) {
    return;
  }
  VkDescriptorImageInfo info{};
  info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  info.imageView = mImageView;
  info.sampler = mSampler;

  VkWriteDescriptorSet write{};
  write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  write.dstSet = mDescriptorSet;
  write.dstBinding = 0;
  write.descriptorCount = 1;
  write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  write.pImageInfo = &info;
  vkUpdateDescriptorSets(device, 1, &write, 0, nullptr);
}

void VulkanRenderTarget::Destroy(VkDevice device) {
  if (device == VK_NULL_HANDLE) {
    return;
  }
  if (mFramebuffer != VK_NULL_HANDLE) {
    vkDestroyFramebuffer(device, mFramebuffer, nullptr);
    mFramebuffer = VK_NULL_HANDLE;
  }
  if (mSampler != VK_NULL_HANDLE) {
    vkDestroySampler(device, mSampler, nullptr);
    mSampler = VK_NULL_HANDLE;
  }
  if (mImageView != VK_NULL_HANDLE) {
    vkDestroyImageView(device, mImageView, nullptr);
    mImageView = VK_NULL_HANDLE;
  }
  if (mImage != VK_NULL_HANDLE) {
    vkDestroyImage(device, mImage, nullptr);
    mImage = VK_NULL_HANDLE;
  }
  if (mMemory != VK_NULL_HANDLE) {
    vkFreeMemory(device, mMemory, nullptr);
    mMemory = VK_NULL_HANDLE;
  }
  // Descriptor set'in sahibi pipeline'dir (VulkanTexture ile ayni sozlesme).
  mDescriptorSet = VK_NULL_HANDLE;
}

bool VulkanRenderTarget::ReadPixels(VkContext* context, VkCommandPool cmd_pool,
                                    uint8_t* out_rgba,
                                    std::size_t byte_capacity) const {
  if (context == nullptr || out_rgba == nullptr || !IsValid()) {
    return false;
  }
  const auto kNeeded =
      static_cast<std::size_t>(mWidth) * static_cast<std::size_t>(mHeight) * 4U;
  if (byte_capacity < kNeeded) {
    spdlog::error(
        "VulkanRenderTarget::ReadPixels: tampon kucuk ({} < {} bayt).",
        byte_capacity, kNeeded);
    return false;
  }

  VkDevice device = context->GetDevice();

  // Hedefe yazan kare komut buffer'i hala ucusta olabilir. Okuma zaten bir
  // senkronizasyon noktasi oldugundan burada tam bekleme en yalin dogru yol.
  vkDeviceWaitIdle(device);

  VkBuffer buffer = VK_NULL_HANDLE;
  VkDeviceMemory memory = VK_NULL_HANDLE;

  VkBufferCreateInfo bi{};
  bi.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  bi.size = static_cast<VkDeviceSize>(kNeeded);
  bi.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
  bi.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  VK_CHECK(vkCreateBuffer(device, &bi, nullptr, &buffer));

  auto cleanup = [&](bool ok) {
    if (memory != VK_NULL_HANDLE) {
      vkFreeMemory(device, memory, nullptr);
    }
    if (buffer != VK_NULL_HANDLE) {
      vkDestroyBuffer(device, buffer, nullptr);
    }
    return ok;
  };

  VkMemoryRequirements req{};
  vkGetBufferMemoryRequirements(device, buffer, &req);
  const uint32_t kType = vk_detail::FindMemoryType(
      context->GetPhysicalDevice(), req.memoryTypeBits,
      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
          VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
  if (kType == UINT32_MAX) {
    spdlog::error("VulkanRenderTarget::ReadPixels: host-visible bellek yok.");
    return cleanup(false);
  }

  VkMemoryAllocateInfo ai{};
  ai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  ai.allocationSize = req.size;
  ai.memoryTypeIndex = kType;
  if (vkAllocateMemory(device, &ai, nullptr, &memory) != VK_SUCCESS ||
      vkBindBufferMemory(device, buffer, memory, 0) != VK_SUCCESS) {
    spdlog::error("VulkanRenderTarget::ReadPixels: bellek baglanamadi.");
    return cleanup(false);
  }

  VkCommandBuffer cmd = BeginOneShot(device, cmd_pool);
  if (cmd == VK_NULL_HANDLE) {
    return cleanup(false);
  }

  Transition(cmd, mImage, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
             VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

  VkBufferImageCopy region{};
  region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  region.imageSubresource.layerCount = 1;
  region.imageExtent = {static_cast<uint32_t>(mWidth),
                        static_cast<uint32_t>(mHeight), 1};
  vkCmdCopyImageToBuffer(cmd, mImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                         buffer, 1, &region);

  Transition(cmd, mImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
             VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

  if (!EndOneShot(device, context->GetGraphicsQueue(), cmd_pool, cmd)) {
    return cleanup(false);
  }

  void* mapped = nullptr;
  if (vkMapMemory(device, memory, 0, static_cast<VkDeviceSize>(kNeeded), 0,
                  &mapped) != VK_SUCCESS) {
    spdlog::error("VulkanRenderTarget::ReadPixels: vkMapMemory basarisiz.");
    return cleanup(false);
  }
  // Image formati zaten R8G8B8A8_UNORM ve satirlar Vulkan'da yukaridan asagi:
  // donusum veya cevirme gerekmez.
  std::memcpy(out_rgba, mapped, kNeeded);
  vkUnmapMemory(device, memory);

  return cleanup(true);
}

VkCommandBuffer VulkanRenderTarget::BeginOneShot(VkDevice device,
                                                 VkCommandPool pool) {
  VkCommandBufferAllocateInfo ai{};
  ai.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  ai.commandPool = pool;
  ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  ai.commandBufferCount = 1;

  VkCommandBuffer cmd = VK_NULL_HANDLE;
  if (vkAllocateCommandBuffers(device, &ai, &cmd) != VK_SUCCESS) {
    spdlog::error("VulkanRenderTarget: komut buffer tahsis edilemedi.");
    return VK_NULL_HANDLE;
  }

  VkCommandBufferBeginInfo bi{};
  bi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
  if (vkBeginCommandBuffer(cmd, &bi) != VK_SUCCESS) {
    vkFreeCommandBuffers(device, pool, 1, &cmd);
    spdlog::error("VulkanRenderTarget: vkBeginCommandBuffer basarisiz.");
    return VK_NULL_HANDLE;
  }
  return cmd;
}

bool VulkanRenderTarget::EndOneShot(VkDevice device, VkQueue queue,
                                    VkCommandPool pool, VkCommandBuffer cmd) {
  VkFence fence = VK_NULL_HANDLE;
  auto cleanup = [&](bool ok) {
    if (fence != VK_NULL_HANDLE) {
      vkDestroyFence(device, fence, nullptr);
    }
    vkFreeCommandBuffers(device, pool, 1, &cmd);
    return ok;
  };

  if (vkEndCommandBuffer(cmd) != VK_SUCCESS) {
    return cleanup(false);
  }

  VkFenceCreateInfo fi{};
  fi.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
  if (vkCreateFence(device, &fi, nullptr, &fence) != VK_SUCCESS) {
    return cleanup(false);
  }

  VkSubmitInfo si{};
  si.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  si.commandBufferCount = 1;
  si.pCommandBuffers = &cmd;
  if (vkQueueSubmit(queue, 1, &si, fence) != VK_SUCCESS) {
    return cleanup(false);
  }
  if (vkWaitForFences(device, 1, &fence, VK_TRUE, UINT64_MAX) != VK_SUCCESS) {
    return cleanup(false);
  }
  return cleanup(true);
}

void VulkanRenderTarget::Transition(VkCommandBuffer cmd, VkImage image,
                                    VkImageLayout old_layout,
                                    VkImageLayout new_layout) {
  VkImageMemoryBarrier barrier{};
  barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
  barrier.oldLayout = old_layout;
  barrier.newLayout = new_layout;
  barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.image = image;
  barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  barrier.subresourceRange.levelCount = 1;
  barrier.subresourceRange.layerCount = 1;

  VkPipelineStageFlags src_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
  VkPipelineStageFlags dst_stage = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;

  if (old_layout == VK_IMAGE_LAYOUT_UNDEFINED &&
      new_layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
    barrier.srcAccessMask = 0;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    src_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    dst_stage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
  } else if (old_layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL &&
             new_layout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL) {
    barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    src_stage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    dst_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
  } else if (old_layout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL &&
             new_layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    src_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    dst_stage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
  }

  vkCmdPipelineBarrier(cmd, src_stage, dst_stage, 0, 0, nullptr, 0, nullptr, 1,
                       &barrier);
}

}  // namespace sdl_painter
