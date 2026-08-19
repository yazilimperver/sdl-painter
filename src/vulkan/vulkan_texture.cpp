#include "vulkan_texture.h"

#include <cstring>
#include <spdlog/spdlog.h>
#include <vector>

#include "vk_check.h"
#include "vk_context.h"
#include "vk_memory.h"

namespace sdl_painter {

VulkanTexture::~VulkanTexture() {
  if (mDevice != VK_NULL_HANDLE) {
    Destroy(mDevice);
  }
}

// ---------------------------------------------------------------------------
// Upload — ana giriş noktası
// ---------------------------------------------------------------------------
bool VulkanTexture::Upload(VkContext* context, VkCommandPool cmd_pool,
                           const uint8_t* data, int32_t width, int32_t height,
                           int32_t channels, VkDescriptorSet descriptor_set,
                           VkDescriptorSetLayout descriptor_set_layout) {
  VkDevice device = context->GetDevice();
  VkPhysicalDevice phys_device = context->GetPhysicalDevice();

  mWidth = width;
  mHeight = height;
  mWidth_u = static_cast<uint32_t>(width);
  mHeight_u = static_cast<uint32_t>(height);
  mDescriptorSet = descriptor_set;

  // Vulkan tarafında tek format kullanılır (R8G8B8A8); 1/2/3 kanallı veri
  // RGBA'ya genişletilir. Çarpımlar size_t üzerinde yapılır: int32 aritmetiği
  // büyük görüntülerde (örn. 16384x16384) taşıyordu.
  const std::size_t kPixelCount =
      static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
  std::vector<uint8_t> rgba_storage;
  const uint8_t* rgba_data = data;
  if (channels != 4) {
    if (channels < 1 || channels > 4) {
      spdlog::error("VulkanTexture: desteklenmeyen kanal sayısı: {}", channels);
      return false;
    }
    const auto kCh = static_cast<std::size_t>(channels);
    rgba_storage.resize(kPixelCount * 4);
    for (std::size_t i = 0; i < kPixelCount; ++i) {
      const uint8_t* src = data + i * kCh;
      uint8_t* dst = rgba_storage.data() + i * 4;
      // 1 kanal: gri tonlama -> (v,v,v,255)
      // 2 kanal: gri + alfa  -> (v,v,v,a)
      // 3 kanal: RGB         -> (r,g,b,255)
      dst[0] = src[0];
      dst[1] = (kCh >= 3) ? src[1] : src[0];
      dst[2] = (kCh >= 3) ? src[2] : src[0];
      dst[3] = (kCh == 2) ? src[1] : ((kCh == 4) ? src[3] : 255);
    }
    rgba_data = rgba_storage.data();
  }

  const auto kImageSize = static_cast<VkDeviceSize>(kPixelCount * 4);

  // 1. Staging buffer
  VkBuffer staging_buf = VK_NULL_HANDLE;
  VkDeviceMemory staging_mem = VK_NULL_HANDLE;
  if (!CreateStagingBuffer(device, phys_device, rgba_data, kImageSize,
                           staging_buf, staging_mem)) {
    return false;
  }

  // 2. Device-local image + view
  if (!CreateImage(device, phys_device, mWidth_u, mHeight_u)) {
    vkDestroyBuffer(device, staging_buf, nullptr);
    vkFreeMemory(device, staging_mem, nullptr);
    return false;
  }

  // Bu noktadan itibaren nesne Vulkan kaynagi sahipleniyor. mDevice'i HEMEN
  // ata ki bundan sonraki her hata yolunda destructor Destroy() cagirsin;
  // aksi halde (mDevice fonksiyon sonunda atanirsa) CreateSampler veya
  // sonraki bir adim basarisiz oldugunda image + memory + view sizardi.
  mDevice = device;

  // 3. Upload: layout geçişi + copy + son layout
  VkBufferImageCopy full_region{};
  full_region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  full_region.imageSubresource.layerCount = 1;
  full_region.imageOffset = {0, 0, 0};
  full_region.imageExtent = {mWidth_u, mHeight_u, 1};
  if (!RecordAndSubmitCopy(device, context->GetGraphicsQueue(), cmd_pool,
                           staging_buf, full_region,
                           /*from_undefined=*/true)) {
    vkDestroyBuffer(device, staging_buf, nullptr);
    vkFreeMemory(device, staging_mem, nullptr);
    return false;
  }

  vkDestroyBuffer(device, staging_buf, nullptr);
  vkFreeMemory(device, staging_mem, nullptr);

  // 4. Sampler
  if (!CreateSampler(device)) {
    return false;
  }

  // 5. Descriptor set güncelle
  UpdateDescriptorSet(device, descriptor_set_layout);

  spdlog::debug("VulkanTexture: {}x{} yuklendi.", width, height);
  return true;
}

// ---------------------------------------------------------------------------
// UpdateRegion — var olan image'ın bir dikdörtgenini yeniden yükle
// ---------------------------------------------------------------------------
bool VulkanTexture::UpdateRegion(VkContext* context, VkCommandPool cmd_pool,
                                 int32_t x, int32_t y, int32_t width,
                                 int32_t height, const uint8_t* data) {
  if (!IsValid() || data == nullptr || width <= 0 || height <= 0) {
    return false;
  }
  if (x < 0 || y < 0 || x + width > mWidth || y + height > mHeight) {
    spdlog::error("VulkanTexture::UpdateRegion: bölge sınır dışı.");
    return false;
  }

  VkDevice device = context->GetDevice();
  const auto kSize = static_cast<VkDeviceSize>(
      static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4);

  VkBuffer staging_buf = VK_NULL_HANDLE;
  VkDeviceMemory staging_mem = VK_NULL_HANDLE;
  if (!CreateStagingBuffer(device, context->GetPhysicalDevice(), data, kSize,
                           staging_buf, staging_mem)) {
    return false;
  }

  VkBufferImageCopy region{};
  region.bufferOffset = 0;
  // Staging sıkı paketli olduğundan satır uzunluğu bölge genişliğidir.
  region.bufferRowLength = 0;
  region.bufferImageHeight = 0;
  region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  region.imageSubresource.mipLevel = 0;
  region.imageSubresource.baseArrayLayer = 0;
  region.imageSubresource.layerCount = 1;
  region.imageOffset = {x, y, 0};
  region.imageExtent = {static_cast<uint32_t>(width),
                        static_cast<uint32_t>(height), 1};

  const bool kOk =
      RecordAndSubmitCopy(device, context->GetGraphicsQueue(), cmd_pool,
                          staging_buf, region, /*from_undefined=*/false);

  vkDestroyBuffer(device, staging_buf, nullptr);
  vkFreeMemory(device, staging_mem, nullptr);
  return kOk;
}

// ---------------------------------------------------------------------------
// Staging buffer
// ---------------------------------------------------------------------------
bool VulkanTexture::CreateStagingBuffer(VkDevice device,
                                        VkPhysicalDevice phys_device,
                                        const uint8_t* rgba_data,
                                        VkDeviceSize size, VkBuffer& out_buf,
                                        VkDeviceMemory& out_mem) {
  VkBufferCreateInfo buf_ci{};
  buf_ci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  buf_ci.size = size;
  buf_ci.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
  buf_ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  if (vkCreateBuffer(device, &buf_ci, nullptr, &out_buf) != VK_SUCCESS) {
    spdlog::error("VulkanTexture: staging vkCreateBuffer başarısız.");
    out_buf = VK_NULL_HANDLE;
    return false;
  }

  VkMemoryRequirements mem_req{};
  vkGetBufferMemoryRequirements(device, out_buf, &mem_req);

  VkMemoryAllocateInfo alloc_info{};
  alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  alloc_info.allocationSize = mem_req.size;
  alloc_info.memoryTypeIndex =
      vk_detail::FindMemoryType(phys_device, mem_req.memoryTypeBits,
                                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                    VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
  if (alloc_info.memoryTypeIndex == UINT32_MAX) {
    spdlog::error("VulkanTexture: staging için uygun bellek tipi bulunamadı.");
    vkDestroyBuffer(device, out_buf, nullptr);
    out_buf = VK_NULL_HANDLE;
    return false;
  }
  if (vkAllocateMemory(device, &alloc_info, nullptr, &out_mem) != VK_SUCCESS) {
    spdlog::error("VulkanTexture: staging vkAllocateMemory başarısız.");
    vkDestroyBuffer(device, out_buf, nullptr);
    out_buf = VK_NULL_HANDLE;
    return false;
  }
  if (vkBindBufferMemory(device, out_buf, out_mem, 0) != VK_SUCCESS) {
    spdlog::error("VulkanTexture: staging vkBindBufferMemory başarısız.");
    vkFreeMemory(device, out_mem, nullptr);
    vkDestroyBuffer(device, out_buf, nullptr);
    out_mem = VK_NULL_HANDLE;
    out_buf = VK_NULL_HANDLE;
    return false;
  }

  void* mapped = nullptr;
  if (vkMapMemory(device, out_mem, 0, size, 0, &mapped) != VK_SUCCESS) {
    spdlog::error("VulkanTexture: staging vkMapMemory başarısız.");
    vkFreeMemory(device, out_mem, nullptr);
    vkDestroyBuffer(device, out_buf, nullptr);
    out_mem = VK_NULL_HANDLE;
    out_buf = VK_NULL_HANDLE;
    return false;
  }
  std::memcpy(mapped, rgba_data, static_cast<size_t>(size));
  vkUnmapMemory(device, out_mem);
  return true;
}

// ---------------------------------------------------------------------------
// Device-local image
// ---------------------------------------------------------------------------
bool VulkanTexture::CreateImage(VkDevice device, VkPhysicalDevice phys_device,
                                uint32_t width, uint32_t height) {
  VkImageCreateInfo img_ci{};
  img_ci.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
  img_ci.imageType = VK_IMAGE_TYPE_2D;
  img_ci.format = VK_FORMAT_R8G8B8A8_SRGB;
  img_ci.extent = {width, height, 1};
  img_ci.mipLevels = 1;
  img_ci.arrayLayers = 1;
  img_ci.samples = VK_SAMPLE_COUNT_1_BIT;
  img_ci.tiling = VK_IMAGE_TILING_OPTIMAL;
  img_ci.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
  img_ci.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  if (vkCreateImage(device, &img_ci, nullptr, &mImage) != VK_SUCCESS) {
    spdlog::error("VulkanTexture: vkCreateImage başarısız.");
    mImage = VK_NULL_HANDLE;
    return false;
  }

  VkMemoryRequirements mem_req{};
  vkGetImageMemoryRequirements(device, mImage, &mem_req);

  VkMemoryAllocateInfo alloc_info{};
  alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  alloc_info.allocationSize = mem_req.size;
  alloc_info.memoryTypeIndex = vk_detail::FindMemoryType(
      phys_device, mem_req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
  if (alloc_info.memoryTypeIndex == UINT32_MAX) {
    spdlog::error("VulkanTexture: image için uygun bellek tipi bulunamadı.");
    vkDestroyImage(device, mImage, nullptr);
    mImage = VK_NULL_HANDLE;
    return false;
  }
  if (vkAllocateMemory(device, &alloc_info, nullptr, &mImageMemory) !=
      VK_SUCCESS) {
    spdlog::error("VulkanTexture: image vkAllocateMemory başarısız.");
    vkDestroyImage(device, mImage, nullptr);
    mImage = VK_NULL_HANDLE;
    return false;
  }
  if (vkBindImageMemory(device, mImage, mImageMemory, 0) != VK_SUCCESS) {
    spdlog::error("VulkanTexture: vkBindImageMemory başarısız.");
    vkFreeMemory(device, mImageMemory, nullptr);
    vkDestroyImage(device, mImage, nullptr);
    mImageMemory = VK_NULL_HANDLE;
    mImage = VK_NULL_HANDLE;
    return false;
  }

  VkImageViewCreateInfo view_ci{};
  view_ci.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
  view_ci.image = mImage;
  view_ci.viewType = VK_IMAGE_VIEW_TYPE_2D;
  view_ci.format = VK_FORMAT_R8G8B8A8_SRGB;
  view_ci.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  view_ci.subresourceRange.baseMipLevel = 0;
  view_ci.subresourceRange.levelCount = 1;
  view_ci.subresourceRange.baseArrayLayer = 0;
  view_ci.subresourceRange.layerCount = 1;
  if (vkCreateImageView(device, &view_ci, nullptr, &mImageView) != VK_SUCCESS) {
    spdlog::error("VulkanTexture: vkCreateImageView başarısız.");
    vkFreeMemory(device, mImageMemory, nullptr);
    vkDestroyImage(device, mImage, nullptr);
    mImageMemory = VK_NULL_HANDLE;
    mImage = VK_NULL_HANDLE;
    return false;
  }
  return true;
}

// ---------------------------------------------------------------------------
// Sampler
// ---------------------------------------------------------------------------
bool VulkanTexture::CreateSampler(VkDevice device) {
  VkSamplerCreateInfo sampler_ci{};
  sampler_ci.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
  sampler_ci.magFilter = VK_FILTER_LINEAR;
  sampler_ci.minFilter = VK_FILTER_LINEAR;
  sampler_ci.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
  sampler_ci.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  sampler_ci.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  sampler_ci.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  sampler_ci.anisotropyEnable = VK_FALSE;
  sampler_ci.compareEnable = VK_FALSE;
  sampler_ci.borderColor = VK_BORDER_COLOR_INT_TRANSPARENT_BLACK;
  sampler_ci.unnormalizedCoordinates = VK_FALSE;
  VK_CHECK(vkCreateSampler(device, &sampler_ci, nullptr, &mSampler));
  return true;
}

// ---------------------------------------------------------------------------
// Upload komut kaydı
// ---------------------------------------------------------------------------
void VulkanTexture::TransitionImageLayout(VkCommandBuffer cmd, VkImage image,
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
  barrier.subresourceRange.baseMipLevel = 0;
  barrier.subresourceRange.levelCount = 1;
  barrier.subresourceRange.baseArrayLayer = 0;
  barrier.subresourceRange.layerCount = 1;

  VkPipelineStageFlags src_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
  VkPipelineStageFlags dst_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;

  if (old_layout == VK_IMAGE_LAYOUT_UNDEFINED &&
      new_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
    barrier.srcAccessMask = 0;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    src_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    dst_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
  } else if (old_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL &&
             new_layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    src_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    dst_stage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
  }

  vkCmdPipelineBarrier(cmd, src_stage, dst_stage, 0, 0, nullptr, 0, nullptr, 1,
                       &barrier);
}

bool VulkanTexture::RecordAndSubmitCopy(VkDevice device, VkQueue queue,
                                        VkCommandPool cmd_pool,
                                        VkBuffer staging_buf,
                                        const VkBufferImageCopy& region,
                                        bool from_undefined) {
  VkCommandBufferAllocateInfo alloc_info{};
  alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  alloc_info.commandPool = cmd_pool;
  alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  alloc_info.commandBufferCount = 1;

  VkCommandBuffer cmd = VK_NULL_HANDLE;
  VK_CHECK(vkAllocateCommandBuffers(device, &alloc_info, &cmd));

  // Tek cikisli temizlik: bu noktadan sonraki her hata yolunda cmd (ve
  // olusturulmussa fence) mutlaka serbest birakilir.
  VkFence fence = VK_NULL_HANDLE;
  auto cleanup = [&](bool ok) {
    if (fence != VK_NULL_HANDLE) {
      vkDestroyFence(device, fence, nullptr);
    }
    vkFreeCommandBuffers(device, cmd_pool, 1, &cmd);
    return ok;
  };

  VkCommandBufferBeginInfo begin_info{};
  begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
  if (vkBeginCommandBuffer(cmd, &begin_info) != VK_SUCCESS) {
    spdlog::error("VulkanTexture: vkBeginCommandBuffer başarısız.");
    return cleanup(false);
  }

  // Ilk yuklemede eski layout UNDEFINED; bolge guncellemesinde image zaten
  // SHADER_READ_ONLY durumundadir ve icerigi korunmalidir.
  TransitionImageLayout(cmd, mImage,
                        from_undefined
                            ? VK_IMAGE_LAYOUT_UNDEFINED
                            : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

  vkCmdCopyBufferToImage(cmd, staging_buf, mImage,
                         VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

  // TRANSFER_DST → SHADER_READ_ONLY
  TransitionImageLayout(cmd, mImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

  if (vkEndCommandBuffer(cmd) != VK_SUCCESS) {
    spdlog::error("VulkanTexture: vkEndCommandBuffer başarısız.");
    return cleanup(false);
  }

  VkSubmitInfo submit_info{};
  submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submit_info.commandBufferCount = 1;
  submit_info.pCommandBuffers = &cmd;

  VkFenceCreateInfo fence_ci{};
  fence_ci.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
  if (vkCreateFence(device, &fence_ci, nullptr, &fence) != VK_SUCCESS) {
    spdlog::error("VulkanTexture: vkCreateFence başarısız.");
    return cleanup(false);
  }

  if (vkQueueSubmit(queue, 1, &submit_info, fence) != VK_SUCCESS) {
    spdlog::error("VulkanTexture: vkQueueSubmit başarısız.");
    return cleanup(false);
  }
  if (vkWaitForFences(device, 1, &fence, VK_TRUE, UINT64_MAX) != VK_SUCCESS) {
    spdlog::error("VulkanTexture: vkWaitForFences başarısız.");
    return cleanup(false);
  }

  return cleanup(true);
}

// ---------------------------------------------------------------------------
// Descriptor set güncelle
// ---------------------------------------------------------------------------
void VulkanTexture::UpdateDescriptorSet(VkDevice device,
                                        VkDescriptorSetLayout /*layout*/) {
  VkDescriptorImageInfo image_info{};
  image_info.sampler = mSampler;
  image_info.imageView = mImageView;
  image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

  VkWriteDescriptorSet write{};
  write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  write.dstSet = mDescriptorSet;
  write.dstBinding = 0;
  write.dstArrayElement = 0;
  write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  write.descriptorCount = 1;
  write.pImageInfo = &image_info;

  vkUpdateDescriptorSets(device, 1, &write, 0, nullptr);
}

// ---------------------------------------------------------------------------
// Destroy
// ---------------------------------------------------------------------------
void VulkanTexture::Destroy(VkDevice device) {
  VkDevice d = (mDevice != VK_NULL_HANDLE) ? mDevice : device;
  if (d == VK_NULL_HANDLE) {
    return;
  }
  if (mSampler != VK_NULL_HANDLE) {
    vkDestroySampler(d, mSampler, nullptr);
    mSampler = VK_NULL_HANDLE;
  }
  if (mImageView != VK_NULL_HANDLE) {
    vkDestroyImageView(d, mImageView, nullptr);
    mImageView = VK_NULL_HANDLE;
  }
  if (mImage != VK_NULL_HANDLE) {
    vkDestroyImage(d, mImage, nullptr);
    mImage = VK_NULL_HANDLE;
  }
  if (mImageMemory != VK_NULL_HANDLE) {
    vkFreeMemory(d, mImageMemory, nullptr);
    mImageMemory = VK_NULL_HANDLE;
  }
  mDescriptorSet = VK_NULL_HANDLE;
  mDevice = VK_NULL_HANDLE;
}

}  // namespace sdl_painter
