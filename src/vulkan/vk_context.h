#pragma once

#include <vulkan/vulkan.h>

#include <cstdint>

struct SDL_Window;

namespace sdl_painter {

/// @brief Vulkan instance, surface, physical/logical device ve queue sahipliği.
///
/// VulkanRenderer'ın alt bileşenidir — tek bir pencereye bağlıdır.
/// Initialize/Shutdown çağrıları Vulkan ömür döngüsünü yönetir.
class VkContext {
 public:
  VkContext() = default;
  ~VkContext();

  VkContext(const VkContext&) = delete;
  VkContext& operator=(const VkContext&) = delete;

  /// @brief Instance + debug messenger + surface + device + queue hazırla.
  bool Initialize(SDL_Window* window);

  /// @brief Tüm Vulkan kaynaklarını serbest bırak.
  void Shutdown();

  VkInstance GetInstance() const { return mInstance; }
  VkPhysicalDevice GetPhysicalDevice() const { return mPhysicalDevice; }
  VkDevice GetDevice() const { return mDevice; }
  VkSurfaceKHR GetSurface() const { return mSurface; }
  VkQueue GetGraphicsQueue() const { return mGraphicsQueue; }
  VkQueue GetPresentQueue() const { return mPresentQueue; }
  uint32_t GetGraphicsQueueFamily() const { return mGraphicsQueueFamily; }
  uint32_t GetPresentQueueFamily() const { return mPresentQueueFamily; }

 private:
  bool CreateInstance();
  bool CreateDebugMessenger();
  bool CreateSurface(SDL_Window* window);
  bool PickPhysicalDevice();
  bool CreateLogicalDevice();

  SDL_Window* mWindow{nullptr};

  VkInstance mInstance{VK_NULL_HANDLE};
  VkDebugUtilsMessengerEXT mDebugMessenger{VK_NULL_HANDLE};
  VkSurfaceKHR mSurface{VK_NULL_HANDLE};
  VkPhysicalDevice mPhysicalDevice{VK_NULL_HANDLE};
  VkDevice mDevice{VK_NULL_HANDLE};

  uint32_t mGraphicsQueueFamily{0};
  uint32_t mPresentQueueFamily{0};
  VkQueue mGraphicsQueue{VK_NULL_HANDLE};
  VkQueue mPresentQueue{VK_NULL_HANDLE};

  bool mValidationEnabled{false};
};

}  // namespace sdl_painter
