#include "vk_memory.h"

#include <spdlog/spdlog.h>

namespace sdl_painter::vk_detail {

uint32_t FindMemoryType(VkPhysicalDevice phys_device, uint32_t type_filter,
                        VkMemoryPropertyFlags props) {
  VkPhysicalDeviceMemoryProperties mem_props{};
  vkGetPhysicalDeviceMemoryProperties(phys_device, &mem_props);

  for (uint32_t i = 0; i < mem_props.memoryTypeCount; ++i) {
    if ((type_filter & (1U << i)) &&
        (mem_props.memoryTypes[i].propertyFlags & props) == props) {
      return i;
    }
  }

  spdlog::error("vk_detail: uygun bellek tipi bulunamadı!");
  return UINT32_MAX;
}

}  // namespace sdl_painter::vk_detail
