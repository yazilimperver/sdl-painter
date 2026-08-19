/// @file test_vk_check.cpp
/// @brief `VkResultToString` — Vulkan hata kodu → okunabilir metin.
///
/// Saf fonksiyon, GPU gerektirmez. Yalnızca hata yollarından çağrıldığı için
/// kapsama ölçümünde %0 görünüyordu; oysa bir hata anında log'da doğru ismin
/// çıkması teşhisin tamamı olabiliyor.

#ifdef SDLPAINTER_HAS_VULKAN

#include <cstring>
#include <gtest/gtest.h>
#include <string>
#include <vulkan/vulkan.h>

#include "vulkan/vk_check.h"

namespace {

using sdl_painter::vk_detail::VkResultToString;

/// @brief Bilinen bir kod, kendi enum adına birebir çevrilmeli.
TEST(VkResultToString, MapsSuccessAndCommonCodes) {
  EXPECT_STREQ(VkResultToString(VK_SUCCESS), "VK_SUCCESS");
  EXPECT_STREQ(VkResultToString(VK_NOT_READY), "VK_NOT_READY");
  EXPECT_STREQ(VkResultToString(VK_TIMEOUT), "VK_TIMEOUT");
  EXPECT_STREQ(VkResultToString(VK_INCOMPLETE), "VK_INCOMPLETE");
}

TEST(VkResultToString, MapsMemoryErrors) {
  EXPECT_STREQ(VkResultToString(VK_ERROR_OUT_OF_HOST_MEMORY),
               "VK_ERROR_OUT_OF_HOST_MEMORY");
  EXPECT_STREQ(VkResultToString(VK_ERROR_OUT_OF_DEVICE_MEMORY),
               "VK_ERROR_OUT_OF_DEVICE_MEMORY");
}

/// Swapchain kodları en sık karşılaşılanlar: resize ve simge durumu
/// yollarında bunların doğru raporlanması teşhis için kritik.
TEST(VkResultToString, MapsSwapchainCodes) {
  EXPECT_STREQ(VkResultToString(VK_ERROR_OUT_OF_DATE_KHR),
               "VK_ERROR_OUT_OF_DATE_KHR");
  EXPECT_STREQ(VkResultToString(VK_SUBOPTIMAL_KHR), "VK_SUBOPTIMAL_KHR");
  EXPECT_STREQ(VkResultToString(VK_ERROR_SURFACE_LOST_KHR),
               "VK_ERROR_SURFACE_LOST_KHR");
}

TEST(VkResultToString, MapsDeviceAndDriverErrors) {
  EXPECT_STREQ(VkResultToString(VK_ERROR_DEVICE_LOST), "VK_ERROR_DEVICE_LOST");
  EXPECT_STREQ(VkResultToString(VK_ERROR_INITIALIZATION_FAILED),
               "VK_ERROR_INITIALIZATION_FAILED");
  EXPECT_STREQ(VkResultToString(VK_ERROR_EXTENSION_NOT_PRESENT),
               "VK_ERROR_EXTENSION_NOT_PRESENT");
  EXPECT_STREQ(VkResultToString(VK_ERROR_LAYER_NOT_PRESENT),
               "VK_ERROR_LAYER_NOT_PRESENT");
}

/// Tabloda olmayan bir kod, çökme yerine belirli bir yedek metin dönmeli.
TEST(VkResultToString, UnknownCodeFallsBack) {
  const auto kBogus = static_cast<VkResult>(-999999);
  EXPECT_STREQ(VkResultToString(kBogus), "VK_UNKNOWN_RESULT");
}

/// Hiçbir kod için `nullptr` veya boş metin dönmemeli — log'da boş satır
/// teşhisi imkânsız kılar.
TEST(VkResultToString, NeverReturnsNullOrEmpty) {
  const VkResult kCodes[] = {
      VK_SUCCESS,
      VK_NOT_READY,
      VK_TIMEOUT,
      VK_EVENT_SET,
      VK_EVENT_RESET,
      VK_INCOMPLETE,
      VK_ERROR_OUT_OF_HOST_MEMORY,
      VK_ERROR_OUT_OF_DEVICE_MEMORY,
      VK_ERROR_INITIALIZATION_FAILED,
      VK_ERROR_DEVICE_LOST,
      VK_ERROR_MEMORY_MAP_FAILED,
      VK_ERROR_LAYER_NOT_PRESENT,
      VK_ERROR_EXTENSION_NOT_PRESENT,
      VK_ERROR_FEATURE_NOT_PRESENT,
      VK_ERROR_INCOMPATIBLE_DRIVER,
      VK_ERROR_TOO_MANY_OBJECTS,
      VK_ERROR_FORMAT_NOT_SUPPORTED,
      VK_ERROR_FRAGMENTED_POOL,
      VK_ERROR_OUT_OF_DATE_KHR,
      VK_SUBOPTIMAL_KHR,
      VK_ERROR_SURFACE_LOST_KHR,
      VK_ERROR_NATIVE_WINDOW_IN_USE_KHR,
      static_cast<VkResult>(12345),
      static_cast<VkResult>(-12345),
  };
  for (VkResult code : kCodes) {
    const char* text = VkResultToString(code);
    ASSERT_NE(text, nullptr) << "kod " << static_cast<int>(code);
    EXPECT_GT(std::strlen(text), 0U) << "kod " << static_cast<int>(code);
  }
}

/// Dönen işaretçi statik ömürlü olmalı: çağıran onu log'da kullanana kadar
/// geçerli kalmalı ve aynı kod için aynı adresi vermeli.
TEST(VkResultToString, ReturnsStableStaticStrings) {
  const char* a = VkResultToString(VK_ERROR_DEVICE_LOST);
  const char* b = VkResultToString(VK_ERROR_DEVICE_LOST);
  EXPECT_EQ(a, b);
}

}  // namespace

#endif  // SDLPAINTER_HAS_VULKAN
