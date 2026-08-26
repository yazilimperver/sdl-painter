#pragma once

#include "sdl_painter/renderer.h"

#include <cstddef>
#include <cstdint>
#include <vulkan/vulkan.h>

namespace sdl_painter {

class VkContext;

/// @brief Ekran dışı (offscreen) bir çizim hedefinin Vulkan kaynakları.
///
/// Sahip olduğu şeyler: renklenebilir bir `VkImage` + belleği, `VkImageView`,
/// örnekleme için `VkSampler` ve hedefe çizim yapmayı sağlayan
/// `VkFramebuffer`. Descriptor set'in sahibi @ref VulkanTexturedPipeline'dır
/// (@ref VulkanTexture ile aynı sözleşme): burada yalnızca saklanır.
///
/// @par Format neden ekranınkinden bağımsız?
/// Image daima @ref kColorFormat ile yaratılır. Swapchain formatını
/// devralsaydı içeriği geri okumanın sözleşmesi sürücüye göre değişirdi
/// (BGRA/RGBA, sRGB/doğrusal). Bedeli, hedeflere çizen pipeline'ların ayrı
/// bir render pass ile üretilmek zorunda olmasıdır — pipeline yalnızca
/// **uyumlu** render pass ile kullanılabilir ve uyumluluk attachment
/// formatını kapsar.
class VulkanRenderTarget {
 public:
  /// @brief Hedeflerin renk formatı — doğrusal RGBA8, her platformda aynı.
  static constexpr VkFormat kColorFormat = VK_FORMAT_R8G8B8A8_UNORM;

  VulkanRenderTarget() = default;
  /// @brief RAII destructor — Create başarılı olduysa kaynakları yıkar.
  ~VulkanRenderTarget();

  VulkanRenderTarget(const VulkanRenderTarget&) = delete;
  VulkanRenderTarget& operator=(const VulkanRenderTarget&) = delete;

  /// @brief Hedefi oluştur.
  ///
  /// Image, oluşturulduktan hemen sonra `SHADER_READ_ONLY_OPTIMAL` layout'una
  /// geçirilir. Sebebi: hedefe hiç çizilmeden örneklenmesi geçerli bir
  /// kullanım (boş bir mini harita) ve `UNDEFINED` layout'undan örneklemek
  /// tanımsız olurdu.
  ///
  /// @param context Vulkan context.
  /// @param render_pass Hedefe çizim yapacak **offscreen** render pass.
  /// @param cmd_pool Layout geçişi için kullanılacak command pool.
  /// @param width  Genişlik (piksel, > 0).
  /// @param height Yükseklik (piksel, > 0).
  /// @param descriptor_set Hazır descriptor set (sahibi pipeline).
  /// @param descriptor_set_layout Descriptor set'in layout'u.
  /// @param filter Örnekleme filtresi.
  bool Create(VkContext* context, VkRenderPass render_pass,
              VkCommandPool cmd_pool, int32_t width, int32_t height,
              VkDescriptorSet descriptor_set,
              VkDescriptorSetLayout descriptor_set_layout,
              TextureFilter filter);

  /// @brief Tüm kaynakları serbest bırak. Idempotent; destructor da çağırır.
  void Destroy(VkDevice device);

  /// @brief Hedefin piksellerini ana belleğe oku (sıkı paketli RGBA8).
  ///
  /// GPU'nun işini bitirmesini **bekler**; kare döngüsünde kullanılmamalıdır.
  /// Satırlar yukarıdan aşağı sıralanır.
  ///
  /// @param byte_capacity `out_rgba`'nın kapasitesi; yetersizse `false`.
  bool ReadPixels(VkContext* context, VkCommandPool cmd_pool, uint8_t* out_rgba,
                  std::size_t byte_capacity) const;

  [[nodiscard]] VkFramebuffer GetFramebuffer() const { return mFramebuffer; }
  [[nodiscard]] VkDescriptorSet GetDescriptorSet() const {
    return mDescriptorSet;
  }
  [[nodiscard]] VkExtent2D GetExtent() const {
    return {static_cast<uint32_t>(mWidth), static_cast<uint32_t>(mHeight)};
  }
  [[nodiscard]] int32_t GetWidth() const { return mWidth; }
  [[nodiscard]] int32_t GetHeight() const { return mHeight; }
  [[nodiscard]] bool IsValid() const { return mImage != VK_NULL_HANDLE; }

 private:
  /// @brief Renk attachment'ı + örneklenebilir + transfer kaynağı image.
  bool CreateImage(VkDevice device, VkPhysicalDevice phys_device);

  /// @brief Image view + sampler.
  bool CreateViewAndSampler(VkDevice device, TextureFilter filter);

  /// @brief Descriptor set'e image + sampler bağla.
  void UpdateDescriptorSet(VkDevice device) const;

  /// @brief Tek seferlik komut buffer'ı başlat.
  static VkCommandBuffer BeginOneShot(VkDevice device, VkCommandPool pool);

  /// @brief Tek seferlik komut buffer'ını gönder ve bitmesini bekle.
  static bool EndOneShot(VkDevice device, VkQueue queue, VkCommandPool pool,
                         VkCommandBuffer cmd);

  /// @brief Renk image'ı için layout geçişi (barrier).
  static void Transition(VkCommandBuffer cmd, VkImage image,
                         VkImageLayout old_layout, VkImageLayout new_layout);

  VkDevice mDevice{VK_NULL_HANDLE};  // RAII için Create'te saklanır
  VkImage mImage{VK_NULL_HANDLE};
  VkDeviceMemory mMemory{VK_NULL_HANDLE};
  VkImageView mImageView{VK_NULL_HANDLE};
  VkSampler mSampler{VK_NULL_HANDLE};
  VkFramebuffer mFramebuffer{VK_NULL_HANDLE};
  VkDescriptorSet mDescriptorSet{VK_NULL_HANDLE};

  int32_t mWidth{0};
  int32_t mHeight{0};
};

}  // namespace sdl_painter
