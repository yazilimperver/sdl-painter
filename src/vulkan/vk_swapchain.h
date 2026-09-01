#pragma once

#include <cstdint>
#include <vector>
#include <vulkan/vulkan.h>

namespace sdl_painter {

class VkContext;

/// @brief Swapchain, render pass ve framebuffer sahipliği.
///
/// Pencere resize edildiğinde `Recreate(w, h)` ile yeniden inşa edilir.
class VkSwapchain {
 public:
  VkSwapchain() = default;
  ~VkSwapchain();

  VkSwapchain(const VkSwapchain&) = delete;
  VkSwapchain& operator=(const VkSwapchain&) = delete;

  /// @brief İlk kez swapchain'i oluştur.
  bool Initialize(VkContext* context, uint32_t width, uint32_t height);

  /// @brief Tüm swapchain kaynaklarını serbest bırak.
  void Shutdown();

  /// @brief Pencere boyutu değiştiğinde swapchain'i yeniden inşa et.
  ///
  /// @note Sıfır boyutlu (simge durumuna küçültülmüş) pencerede çağrılmamalı;
  ///       Vulkan `imageExtent` bileşenlerinin sıfırdan büyük olmasını şart
  ///       koşar. Çağırmadan önce @ref IsSurfaceRenderable ile kontrol edin.
  bool Recreate(uint32_t width, uint32_t height);

  /// @brief Yüzey şu an çizilebilir bir alana sahip mi?
  ///
  /// Pencere simge durumuna küçültüldüğünde platform, yüzey yeteneklerinde
  /// `currentExtent = {0, 0}` bildirir. Bu durumda swapchain oluşturmak,
  /// framebuffer yaratmak veya render pass başlatmak Vulkan geçerlilik
  /// kurallarını ihlal eder — kare tamamen atlanmalıdır.
  [[nodiscard]] bool IsSurfaceRenderable() const;

  VkSwapchainKHR GetSwapchain() const { return mSwapchain; }
  VkRenderPass GetRenderPass() const { return mRenderPass; }

  /// @brief Ekran render pass'inin içeriği koruyan ikizi.
  ///
  /// Kare ortasında bir offscreen hedefe geçilip geri dönüldüğünde ekran
  /// render pass'i yeniden başlatılmak zorundadır (bir komut buffer'ında
  /// birden fazla render pass örneği olabilir, ama iç içe olamaz). Asıl
  /// pass `loadOp = CLEAR` olduğu için onu yeniden başlatmak o ana kadar
  /// çizilen her şeyi silerdi; bu ikiz `loadOp = LOAD` ile devam eder.
  ///
  /// Asıl pass ile @b uyumludur (aynı format, aynı örnek sayısı, aynı
  /// attachment düzeni) — bu yüzden hem aynı framebuffer'lar hem de asıl
  /// pass için üretilmiş pipeline'lar olduğu gibi kullanılabilir. Uyumluluk
  /// `loadOp`/`storeOp` ve layout'ları kapsamaz.
  VkRenderPass GetResumeRenderPass() const { return mResumeRenderPass; }
  VkFormat GetImageFormat() const { return mImageFormat; }
  VkExtent2D GetExtent() const { return mExtent; }
  uint32_t GetImageCount() const {
    return static_cast<uint32_t>(mImages.size());
  }
  VkFramebuffer GetFramebuffer(uint32_t index) const {
    return mFramebuffers[index];
  }

 private:
  bool CreateSwapchain(uint32_t width, uint32_t height);
  bool CreateSwapchainWithOld(uint32_t width, uint32_t height,
                              VkSwapchainKHR old_swapchain);
  bool CreateImageViews();
  bool CreateRenderPass();
  bool CreateResumeRenderPass();
  bool CreateFramebuffers();
  void DestroySwapchainResources();

  VkContext* mContext{nullptr};
  VkSwapchainKHR mSwapchain{VK_NULL_HANDLE};
  VkFormat mImageFormat{VK_FORMAT_UNDEFINED};
  VkExtent2D mExtent{0, 0};

  std::vector<VkImage> mImages;
  std::vector<VkImageView> mImageViews;
  std::vector<VkFramebuffer> mFramebuffers;

  VkRenderPass mRenderPass{VK_NULL_HANDLE};

  /// Icerigi koruyan ikiz (bkz. GetResumeRenderPass).
  VkRenderPass mResumeRenderPass{VK_NULL_HANDLE};
};

}  // namespace sdl_painter
