#pragma once

#include <vulkan/vulkan.h>

#include <cstdint>

namespace sdl_painter::vk_detail {

/// @brief Verilen özelliklere uygun bellek tipi indeksini döndürür.
/// @return Bellek tipi indeksi; bulunamazsa UINT32_MAX.
uint32_t FindMemoryType(VkPhysicalDevice phys_device, uint32_t type_filter,
                        VkMemoryPropertyFlags props);

}  // namespace sdl_painter::vk_detail
