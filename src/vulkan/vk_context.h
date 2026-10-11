#pragma once

#include <cstdint>
#include <vulkan/vulkan.h>

struct SDL_Window;

namespace sdl_painter {

namespace vk_detail {

/// @brief Süreç başından beri raporlanan Vulkan validation hatası sayısı.
/// Sayaç süreç geneli ve thread safedir; validation kapalıysa
/// (release derlemesi, katman kurulu değil) daima 0 kalır.
[[nodiscard]] uint64_t ValidationErrorCount() noexcept;

/// @brief Kurulan bir Vulkan bağlamında validation'ın durumu.
///
/// Yalnızca `kEnabled`, `ValidationErrorCount()` sayacının o bağlamda
/// gerçekten beslendiği anlamına gelir: katmanın kurulu olması yetmez,
/// mesajları sayan debug messenger da kurulmuş olmalıdır.
enum class ValidationState : uint8_t {
  kEnabled,          ///< Debug derleme, katman açık, messenger kurulu.
  kDisabledInBuild,  ///< Release (`NDEBUG`) derleme; tasarım gereği kapalı.
  kLayerMissing,     ///< Debug derleme; `VK_LAYER_KHRONOS_validation` yok.
  kMessengerFailed,  ///< Katman açık, ama debug messenger kurulamadı.
  /// Durum değil, durum sayısı; sayaç dizisinin boyutu. Yeni durum bunun
  /// önüne eklenir. `ContextCount`'a verilmez.
  kCount,
};

/// @brief Süreç başından beri verilen validation durumuyla başarıyla kurulan
///        Vulkan bağlamı sayısı.
///
/// `VkContext::Initialize()` başarıyla döndüğünde ilgili durumun sayacı bir
/// artar; yarıda kalan kurulum sayılmaz. Testler bir bloğun öncesi/sonrası
/// değerleri karşılaştırarak o blokta validation'sız bağlam kurulup
/// kurulmadığını görür (`SDLPAINTER_REQUIRE_VALIDATION`). Yan etkisizdir;
/// sayaçlar süreç geneli ve thread safedir.
[[nodiscard]] uint64_t ContextCount(ValidationState state) noexcept;

}  // namespace vk_detail

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
