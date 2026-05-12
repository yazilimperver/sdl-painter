#pragma once

#include <vulkan/vulkan.h>

#include <spdlog/spdlog.h>

namespace sdl_painter::vk_detail {

/// @brief VkResult enum değerini okunabilir bir string'e çevirir.
const char* VkResultToString(VkResult result);

}  // namespace sdl_painter::vk_detail

/// @brief Vulkan çağrılarını kontrol eder; başarısızlıkta log atıp abort eder.
/// Init zamanı Vulkan çağrıları için kullanın.
#define VK_CHECK(expr)                                                      \
  do {                                                                      \
    VkResult _vk_result = (expr);                                           \
    if (_vk_result != VK_SUCCESS) {                                         \
      spdlog::error("Vulkan call failed: {} -> {} ({}:{})", #expr,          \
                    ::sdl_painter::vk_detail::VkResultToString(_vk_result), \
                    __FILE__, __LINE__);                                    \
      std::abort();                                                         \
    }                                                                       \
  } while (0)

/// @brief Frame içi Vulkan çağrıları için hata kontrolü — abort etmez,
/// fonksiyondan erken döner (void dönen fonksiyonlarda kullanın).
#define VK_CHECK_RETURN(expr)                                               \
  do {                                                                      \
    VkResult _vk_result = (expr);                                           \
    if (_vk_result != VK_SUCCESS) {                                         \
      spdlog::error("Vulkan hatası: {} -> {} ({}:{})", #expr,               \
                    ::sdl_painter::vk_detail::VkResultToString(_vk_result), \
                    __FILE__, __LINE__);                                    \
      return;                                                               \
    }                                                                       \
  } while (0)
