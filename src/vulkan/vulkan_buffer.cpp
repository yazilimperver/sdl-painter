#include "vulkan_buffer.h"

#include <cstring>
#include <spdlog/spdlog.h>

#include "vk_check.h"
#include "vk_memory.h"

namespace sdl_painter {

VulkanBuffer::~VulkanBuffer() {
  if (mDevice != VK_NULL_HANDLE) {
    Destroy(mDevice);
  }
}

bool VulkanBuffer::Init(VkDevice device, VkPhysicalDevice phys_device,
                        VkDeviceSize capacity, VkBufferUsageFlags usage,
                        uint32_t frame_slot_count) {
  if (frame_slot_count == 0) {
    frame_slot_count = 1;
  }
  mCapacity = capacity;
  mFrameSlotCount = frame_slot_count;
  mSlotCapacity = capacity / frame_slot_count;
  mHeads.assign(frame_slot_count, 0);

  // Buffer oluştur.
  VkBufferCreateInfo buf_info{};
  buf_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  buf_info.size = capacity;
  buf_info.usage = usage;
  buf_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

  if (vkCreateBuffer(device, &buf_info, nullptr, &mBuffer) != VK_SUCCESS) {
    spdlog::error("VulkanBuffer: vkCreateBuffer başarısız.");
    return false;
  }

  // Bellek gereksinimleri sorgula.
  VkMemoryRequirements mem_req{};
  vkGetBufferMemoryRequirements(device, mBuffer, &mem_req);

  // Host-visible + coherent bellek seç (staging gerekmez).
  const VkMemoryPropertyFlags kMemProps = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                          VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
  uint32_t mem_type =
      vk_detail::FindMemoryType(phys_device, mem_req.memoryTypeBits, kMemProps);
  if (mem_type == UINT32_MAX) {
    spdlog::error("VulkanBuffer: uygun bellek tipi bulunamadı.");
    vkDestroyBuffer(device, mBuffer, nullptr);
    mBuffer = VK_NULL_HANDLE;
    return false;
  }

  VkMemoryAllocateInfo alloc_info{};
  alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  alloc_info.allocationSize = mem_req.size;
  alloc_info.memoryTypeIndex = mem_type;

  if (vkAllocateMemory(device, &alloc_info, nullptr, &mMemory) != VK_SUCCESS) {
    spdlog::error("VulkanBuffer: vkAllocateMemory başarısız.");
    vkDestroyBuffer(device, mBuffer, nullptr);
    mBuffer = VK_NULL_HANDLE;
    return false;
  }

  if (vkBindBufferMemory(device, mBuffer, mMemory, 0) != VK_SUCCESS) {
    spdlog::error("VulkanBuffer: vkBindBufferMemory başarısız.");
    vkFreeMemory(device, mMemory, nullptr);
    vkDestroyBuffer(device, mBuffer, nullptr);
    mMemory = VK_NULL_HANDLE;
    mBuffer = VK_NULL_HANDLE;
    return false;
  }

  // Kalıcı map — HOST_COHERENT olduğundan flush gerekmez.
  if (vkMapMemory(device, mMemory, 0, capacity, 0, &mMapped) != VK_SUCCESS) {
    spdlog::error("VulkanBuffer: vkMapMemory başarısız.");
    vkFreeMemory(device, mMemory, nullptr);
    vkDestroyBuffer(device, mBuffer, nullptr);
    mMemory = VK_NULL_HANDLE;
    mBuffer = VK_NULL_HANDLE;
    return false;
  }

  spdlog::debug("VulkanBuffer initialized: {} bytes", capacity);
  // RAII için device'i sakla (destructor başarılı bir Init sonrası yıkar).
  mDevice = device;
  return true;
}

void VulkanBuffer::Destroy(VkDevice device) {
  // Idempotent — birden fazla Destroy ya da Destroy + destructor güvenli.
  // mDevice geçerliyse onu kullan; aksi halde parametre device'ı kullan.
  VkDevice d = (mDevice != VK_NULL_HANDLE) ? mDevice : device;
  if (d == VK_NULL_HANDLE) {
    return;
  }

  if (mMapped != nullptr) {
    vkUnmapMemory(d, mMemory);
    mMapped = nullptr;
  }
  if (mBuffer != VK_NULL_HANDLE) {
    vkDestroyBuffer(d, mBuffer, nullptr);
    mBuffer = VK_NULL_HANDLE;
  }
  if (mMemory != VK_NULL_HANDLE) {
    vkFreeMemory(d, mMemory, nullptr);
    mMemory = VK_NULL_HANDLE;
  }
  mCapacity = 0;
  mSlotCapacity = 0;
  mFrameSlotCount = 1;
  mHeads.clear();
  mDevice = VK_NULL_HANDLE;  // tekrar yıkımı önle
}

bool VulkanBuffer::Write(const void* data, VkDeviceSize byte_size,
                         VkDeviceSize alignment, uint32_t frame_slot,
                         VkDeviceSize& out_offset_bytes) {
  if (frame_slot >= mFrameSlotCount) {
    spdlog::warn("VulkanBuffer::Write: geçersiz frame_slot {} (max {}).",
                 frame_slot, mFrameSlotCount);
    return false;
  }

  // Slot'un mutlak başlangıç ve bitiş offset'leri.
  const VkDeviceSize kSlotStart = frame_slot * mSlotCapacity;
  const VkDeviceSize kSlotEnd = kSlotStart + mSlotCapacity;
  // Slot içinde geçerli mutlak head.
  const VkDeviceSize kAbsHead = kSlotStart + mHeads[frame_slot];
  // Hizala.
  const VkDeviceSize kAlignedHead =
      (kAbsHead + alignment - 1) & ~(alignment - 1);

  if (kAlignedHead + byte_size > kSlotEnd) {
    spdlog::warn(
        "VulkanBuffer slot {} overflow: requested {} bytes, available {} "
        "bytes. "
        "Draw call skipped.",
        frame_slot, byte_size, kSlotEnd - kAlignedHead);
    return false;
  }

  std::memcpy(static_cast<char*>(mMapped) + kAlignedHead, data,
              static_cast<std::size_t>(byte_size));
  out_offset_bytes = kAlignedHead;
  mHeads[frame_slot] = (kAlignedHead + byte_size) - kSlotStart;
  return true;
}

void VulkanBuffer::ResetRing(uint32_t frame_slot) {
  if (frame_slot < mFrameSlotCount) {
    mHeads[frame_slot] = 0;
  }
}

}  // namespace sdl_painter
