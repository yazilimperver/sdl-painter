#include "vulkan_buffer.h"

#include <spdlog/spdlog.h>

#include <cstring>

#include "vk_check.h"
#include "vk_memory.h"

namespace sdl_painter {

bool VulkanBuffer::Init(VkDevice device, VkPhysicalDevice phys_device,
                        VkDeviceSize capacity, VkBufferUsageFlags usage) {
  mCapacity = capacity;
  mHead = 0;

  // Buffer oluştur.
  VkBufferCreateInfo buf_info{};
  buf_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  buf_info.size = capacity;
  buf_info.usage = usage;
  buf_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

  VK_CHECK(vkCreateBuffer(device, &buf_info, nullptr, &mBuffer));

  // Bellek gereksinimleri sorgula.
  VkMemoryRequirements mem_req{};
  vkGetBufferMemoryRequirements(device, mBuffer, &mem_req);

  // Host-visible + coherent bellek seç (staging gerekmez).
  const VkMemoryPropertyFlags mem_props =
      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
  uint32_t mem_type =
      vk_detail::FindMemoryType(phys_device, mem_req.memoryTypeBits, mem_props);
  if (mem_type == UINT32_MAX) {
    vkDestroyBuffer(device, mBuffer, nullptr);
    mBuffer = VK_NULL_HANDLE;
    return false;
  }

  VkMemoryAllocateInfo alloc_info{};
  alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  alloc_info.allocationSize = mem_req.size;
  alloc_info.memoryTypeIndex = mem_type;

  VK_CHECK(vkAllocateMemory(device, &alloc_info, nullptr, &mMemory));
  VK_CHECK(vkBindBufferMemory(device, mBuffer, mMemory, 0));

  // Kalıcı map — HOST_COHERENT olduğundan flush gerekmez.
  VK_CHECK(vkMapMemory(device, mMemory, 0, capacity, 0, &mMapped));

  spdlog::debug("VulkanBuffer initialized: {} bytes", capacity);
  return true;
}

void VulkanBuffer::Destroy(VkDevice device) {
  if (mMapped) {
    vkUnmapMemory(device, mMemory);
    mMapped = nullptr;
  }
  if (mBuffer != VK_NULL_HANDLE) {
    vkDestroyBuffer(device, mBuffer, nullptr);
    mBuffer = VK_NULL_HANDLE;
  }
  if (mMemory != VK_NULL_HANDLE) {
    vkFreeMemory(device, mMemory, nullptr);
    mMemory = VK_NULL_HANDLE;
  }
  mCapacity = 0;
  mHead = 0;
}

bool VulkanBuffer::Write(const void* data, VkDeviceSize byte_size,
                         VkDeviceSize alignment,
                         VkDeviceSize& out_offset_bytes) {
  // Hizala: mHead'i alignment'ın katına yuvarlat.
  VkDeviceSize aligned_head = (mHead + alignment - 1) & ~(alignment - 1);

  if (aligned_head + byte_size > mCapacity) {
    spdlog::warn(
        "VulkanBuffer ring overflow: requested {} bytes, available {} bytes. "
        "Draw call skipped.",
        byte_size, mCapacity - aligned_head);
    return false;
  }

  std::memcpy(static_cast<char*>(mMapped) + aligned_head, data,
              static_cast<std::size_t>(byte_size));
  out_offset_bytes = aligned_head;
  mHead = aligned_head + byte_size;
  return true;
}

void VulkanBuffer::ResetRing() { mHead = 0; }

}  // namespace sdl_painter
