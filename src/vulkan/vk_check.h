#pragma once

#include <spdlog/spdlog.h>
#include <vulkan/vulkan.h>

namespace sdl_painter::vk_detail {

/// @brief VkResult enum değerini okunabilir bir string'e çevirir.
const char* VkResultToString(VkResult result);

}  // namespace sdl_painter::vk_detail

/// @brief Vulkan init çağrılarını kontrol eder; başarısızlıkta log atıp
/// içinde bulunduğu fonksiyondan `return false;` ile çıkar.
///
/// Yalnızca `bool` dönen init/setup fonksiyonlarında kullanılmalıdır.
/// Kütüphane kodu init başarısızlığında `std::abort` çağırmaz; çağıran
/// uygulamaya hatayı bildirir, böylece uygulama kendi sahip olduğu kaynakları
/// graceful biçimde yıkma şansı bulur.
#define VK_CHECK(expr)                                                      \
  do {                                                                      \
    VkResult _vk_result = (expr);                                           \
    if (_vk_result != VK_SUCCESS) {                                         \
      spdlog::error("Vulkan call failed: {} -> {} ({}:{})", #expr,          \
                    ::sdl_painter::vk_detail::VkResultToString(_vk_result), \
                    __FILE__, __LINE__);                                    \
      return false;                                                         \
    }                                                                       \
  } while (0)

/// @brief Vulkan handle dönen fonksiyonlarda (VkShaderModule vb.) hata
/// kontrolü — başarısızlıkta `return VK_NULL_HANDLE;` ile çıkar.
#define VK_CHECK_HANDLE(expr)                                               \
  do {                                                                      \
    VkResult _vk_result = (expr);                                           \
    if (_vk_result != VK_SUCCESS) {                                         \
      spdlog::error("Vulkan call failed: {} -> {} ({}:{})", #expr,          \
                    ::sdl_painter::vk_detail::VkResultToString(_vk_result), \
                    __FILE__, __LINE__);                                    \
      return VK_NULL_HANDLE;                                                \
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
