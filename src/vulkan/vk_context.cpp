#include "vk_context.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>

#include <algorithm>
#include <cstring>
#include <optional>
#include <set>
#include <spdlog/spdlog.h>
#include <vector>

#include "vk_check.h"

namespace sdl_painter {

namespace {

constexpr const char* kValidationLayer = "VK_LAYER_KHRONOS_validation";

#ifdef NDEBUG
constexpr bool kEnableValidationDefault = false;
#else
constexpr bool kEnableValidationDefault = true;
#endif

VKAPI_ATTR VkBool32 VKAPI_CALL DebugMessengerCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT severity,
    VkDebugUtilsMessageTypeFlagsEXT /*type*/,
    const VkDebugUtilsMessengerCallbackDataEXT* data, void* /*user_data*/) {
  if (!data || !data->pMessage)
    return VK_FALSE;
  if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
    spdlog::error("[Vulkan] {}", data->pMessage);
  } else if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
    spdlog::warn("[Vulkan] {}", data->pMessage);
  } else if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT) {
    spdlog::info("[Vulkan] {}", data->pMessage);
  } else {
    spdlog::debug("[Vulkan] {}", data->pMessage);
  }
  return VK_FALSE;
}

bool CheckValidationLayerSupport() {
  uint32_t count = 0;
  vkEnumerateInstanceLayerProperties(&count, nullptr);
  std::vector<VkLayerProperties> available(count);
  vkEnumerateInstanceLayerProperties(&count, available.data());
  for (const auto& layer : available) {
    if (std::strcmp(layer.layerName, kValidationLayer) == 0)
      return true;
  }
  return false;
}

VkDebugUtilsMessengerCreateInfoEXT MakeDebugMessengerCreateInfo() {
  VkDebugUtilsMessengerCreateInfoEXT info{};
  info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
  info.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                         VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
  info.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                     VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                     VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
  info.pfnUserCallback = DebugMessengerCallback;
  return info;
}

}  // namespace

VkContext::~VkContext() {
  Shutdown();
}

bool VkContext::Initialize(SDL_Window* window) {
  mWindow = window;
  mValidationEnabled =
      kEnableValidationDefault && CheckValidationLayerSupport();
  if constexpr (kEnableValidationDefault) {
    if (!mValidationEnabled) {
      spdlog::warn("Vulkan validation layers requested but not available.");
    }
  }

  if (!CreateInstance())
    return false;
  if (mValidationEnabled && !CreateDebugMessenger())
    return false;
  if (!CreateSurface(window))
    return false;
  if (!PickPhysicalDevice())
    return false;
  if (!CreateLogicalDevice())
    return false;

  spdlog::info("VkContext initialized (validation={}).", mValidationEnabled);
  return true;
}

void VkContext::Shutdown() {
  if (mDevice != VK_NULL_HANDLE) {
    vkDeviceWaitIdle(mDevice);
    vkDestroyDevice(mDevice, nullptr);
    mDevice = VK_NULL_HANDLE;
  }
  if (mSurface != VK_NULL_HANDLE && mInstance != VK_NULL_HANDLE) {
    SDL_Vulkan_DestroySurface(mInstance, mSurface, nullptr);
    mSurface = VK_NULL_HANDLE;
  }
  if (mDebugMessenger != VK_NULL_HANDLE && mInstance != VK_NULL_HANDLE) {
    auto destroyFn = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
        vkGetInstanceProcAddr(mInstance, "vkDestroyDebugUtilsMessengerEXT"));
    if (destroyFn)
      destroyFn(mInstance, mDebugMessenger, nullptr);
    mDebugMessenger = VK_NULL_HANDLE;
  }
  if (mInstance != VK_NULL_HANDLE) {
    vkDestroyInstance(mInstance, nullptr);
    mInstance = VK_NULL_HANDLE;
  }
  mPhysicalDevice = VK_NULL_HANDLE;
  mGraphicsQueue = VK_NULL_HANDLE;
  mPresentQueue = VK_NULL_HANDLE;
}

bool VkContext::CreateInstance() {
  VkApplicationInfo app{};
  app.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
  app.pApplicationName = "SDLPainter";
  app.applicationVersion = VK_MAKE_VERSION(0, 1, 0);
  app.pEngineName = "SDLPainter";
  app.engineVersion = VK_MAKE_VERSION(0, 1, 0);
  app.apiVersion = VK_API_VERSION_1_1;

  // SDL3 Vulkan instance extension'larını al.
  Uint32 sdl_ext_count = 0;
  const char* const* sdl_exts =
      SDL_Vulkan_GetInstanceExtensions(&sdl_ext_count);
  if (!sdl_exts) {
    spdlog::error("SDL_Vulkan_GetInstanceExtensions failed: {}",
                  SDL_GetError());
    return false;
  }

  std::vector<const char*> extensions(sdl_exts, sdl_exts + sdl_ext_count);
  if (mValidationEnabled) {
    extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
  }

  std::vector<const char*> layers;
  if (mValidationEnabled) {
    layers.push_back(kValidationLayer);
  }

  VkInstanceCreateInfo ci{};
  ci.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
  ci.pApplicationInfo = &app;
  ci.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
  ci.ppEnabledExtensionNames = extensions.data();
  ci.enabledLayerCount = static_cast<uint32_t>(layers.size());
  ci.ppEnabledLayerNames = layers.empty() ? nullptr : layers.data();

  // Instance oluşturma sırasında bile debug messenger'ı zincire takalım.
  VkDebugUtilsMessengerCreateInfoEXT dbg_ci{};
  if (mValidationEnabled) {
    dbg_ci = MakeDebugMessengerCreateInfo();
    ci.pNext = &dbg_ci;
  }

  VkResult res = vkCreateInstance(&ci, nullptr, &mInstance);
  if (res != VK_SUCCESS) {
    spdlog::error("vkCreateInstance failed: {}",
                  vk_detail::VkResultToString(res));
    return false;
  }
  return true;
}

bool VkContext::CreateDebugMessenger() {
  auto createFn = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
      vkGetInstanceProcAddr(mInstance, "vkCreateDebugUtilsMessengerEXT"));
  if (!createFn) {
    spdlog::warn("vkCreateDebugUtilsMessengerEXT not available.");
    return true;  // kritik değil
  }
  auto info = MakeDebugMessengerCreateInfo();
  VkResult res = createFn(mInstance, &info, nullptr, &mDebugMessenger);
  if (res != VK_SUCCESS) {
    spdlog::warn("Debug messenger creation failed: {}",
                 vk_detail::VkResultToString(res));
    return true;
  }
  return true;
}

bool VkContext::CreateSurface(SDL_Window* window) {
  if (!SDL_Vulkan_CreateSurface(window, mInstance, nullptr, &mSurface)) {
    spdlog::error("SDL_Vulkan_CreateSurface failed: {}", SDL_GetError());
    return false;
  }
  return true;
}

namespace {

struct QueueFamilies {
  std::optional<uint32_t> graphics;
  std::optional<uint32_t> present;
  bool IsComplete() const { return graphics && present; }
};

QueueFamilies FindQueueFamilies(VkPhysicalDevice device, VkSurfaceKHR surface) {
  QueueFamilies result;
  uint32_t count = 0;
  vkGetPhysicalDeviceQueueFamilyProperties(device, &count, nullptr);
  std::vector<VkQueueFamilyProperties> props(count);
  vkGetPhysicalDeviceQueueFamilyProperties(device, &count, props.data());

  for (uint32_t i = 0; i < count; ++i) {
    if (props[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
      result.graphics = i;
    }
    VkBool32 present_support = VK_FALSE;
    vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &present_support);
    if (present_support) {
      result.present = i;
    }
    if (result.IsComplete())
      break;
  }
  return result;
}

bool DeviceSupportsSwapchain(VkPhysicalDevice device) {
  uint32_t count = 0;
  vkEnumerateDeviceExtensionProperties(device, nullptr, &count, nullptr);
  std::vector<VkExtensionProperties> exts(count);
  vkEnumerateDeviceExtensionProperties(device, nullptr, &count, exts.data());
  for (const auto& e : exts) {
    if (std::strcmp(e.extensionName, VK_KHR_SWAPCHAIN_EXTENSION_NAME) == 0) {
      return true;
    }
  }
  return false;
}

int ScoreDevice(VkPhysicalDevice device, VkSurfaceKHR surface) {
  auto q = FindQueueFamilies(device, surface);
  if (!q.IsComplete())
    return -1;
  if (!DeviceSupportsSwapchain(device))
    return -1;

  VkPhysicalDeviceProperties props{};
  vkGetPhysicalDeviceProperties(device, &props);
  int score = 0;
  if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
    score += 1000;
  if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU)
    score += 100;
  return score;
}

}  // namespace

bool VkContext::PickPhysicalDevice() {
  uint32_t count = 0;
  vkEnumeratePhysicalDevices(mInstance, &count, nullptr);
  if (count == 0) {
    spdlog::error("No Vulkan-capable GPU found.");
    return false;
  }
  std::vector<VkPhysicalDevice> devices(count);
  vkEnumeratePhysicalDevices(mInstance, &count, devices.data());

  int best_score = -1;
  VkPhysicalDevice best = VK_NULL_HANDLE;
  for (auto* d : devices) {
    int score = ScoreDevice(d, mSurface);
    if (score > best_score) {
      best_score = score;
      best = d;
    }
  }
  if (best == VK_NULL_HANDLE || best_score < 0) {
    spdlog::error("No suitable GPU (missing graphics/present/swapchain).");
    return false;
  }

  mPhysicalDevice = best;
  VkPhysicalDeviceProperties props{};
  vkGetPhysicalDeviceProperties(best, &props);
  spdlog::info("Selected GPU: {} (API {}.{}.{})", props.deviceName,
               VK_VERSION_MAJOR(props.apiVersion),
               VK_VERSION_MINOR(props.apiVersion),
               VK_VERSION_PATCH(props.apiVersion));

  auto q = FindQueueFamilies(best, mSurface);
  mGraphicsQueueFamily = *q.graphics;
  mPresentQueueFamily = *q.present;
  return true;
}

bool VkContext::CreateLogicalDevice() {
  std::set<uint32_t> unique_families{mGraphicsQueueFamily, mPresentQueueFamily};
  std::vector<VkDeviceQueueCreateInfo> queue_cis;
  float priority = 1.0F;
  for (uint32_t family : unique_families) {
    VkDeviceQueueCreateInfo qci{};
    qci.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    qci.queueFamilyIndex = family;
    qci.queueCount = 1;
    qci.pQueuePriorities = &priority;
    queue_cis.push_back(qci);
  }

  VkPhysicalDeviceFeatures features{};  // Phase 5a için özellik gerekmez.

  std::vector<const char*> device_exts = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};

  VkDeviceCreateInfo ci{};
  ci.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
  ci.queueCreateInfoCount = static_cast<uint32_t>(queue_cis.size());
  ci.pQueueCreateInfos = queue_cis.data();
  ci.pEnabledFeatures = &features;
  ci.enabledExtensionCount = static_cast<uint32_t>(device_exts.size());
  ci.ppEnabledExtensionNames = device_exts.data();

  VkResult res = vkCreateDevice(mPhysicalDevice, &ci, nullptr, &mDevice);
  if (res != VK_SUCCESS) {
    spdlog::error("vkCreateDevice failed: {}",
                  vk_detail::VkResultToString(res));
    return false;
  }

  vkGetDeviceQueue(mDevice, mGraphicsQueueFamily, 0, &mGraphicsQueue);
  vkGetDeviceQueue(mDevice, mPresentQueueFamily, 0, &mPresentQueue);
  return true;
}

}  // namespace sdl_painter
