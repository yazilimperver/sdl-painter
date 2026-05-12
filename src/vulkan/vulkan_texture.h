#pragma once

#include <vulkan/vulkan.h>

#include <cstdint>

namespace sdl_painter {

class VkContext;

/// @brief Bir Vulkan texture'ının tüm kaynaklarını yöneten RAII sınıfı.
///
/// Ham piksel verisinden VkImage (RGBA8_SRGB), VkImageView ve VkSampler
/// oluşturur. DrawTextured çağrısı için bir VkDescriptorSet de içerir.
///
/// Upload işlemi tek seferlik bir staging buffer + transfer komut setiyle
/// yapılır: host-visible staging → device-local image → layout geçişi.
class VulkanTexture {
 public:
  VulkanTexture() = default;
  /// @brief RAII destructor — Upload başarılı olduysa kaynakları otomatik
  /// yıkar. Not: VkDescriptorSet sahibi VulkanTexturedPipeline'dır, burada
  /// free edilmez (yalnızca null'lanır).
  ~VulkanTexture();

  VulkanTexture(const VulkanTexture&) = delete;
  VulkanTexture& operator=(const VulkanTexture&) = delete;

  /// @brief Texture'ı oluştur ve GPU'ya yükle.
  ///
  /// @param context Vulkan context (device, physical device, queue bilgisi).
  /// @param cmd_pool Upload için kullanılacak command pool.
  /// @param data Ham piksel verisi (channels == 4 → RGBA8, channels == 3 → RGB8).
  /// @param width Genişlik (piksel).
  /// @param height Yükseklik (piksel).
  /// @param channels Kanal sayısı (3 = RGB, 4 = RGBA). 3 ise RGBA'ya dönüştürülür.
  /// @param descriptor_set Texture'a bağlanacak hazır descriptor set.
  /// @param descriptor_set_layout Descriptor set'in layout'u (image sampler update için).
  /// @return Başarı durumunda true.
  bool Upload(VkContext* context, VkCommandPool cmd_pool,
              const uint8_t* data, int32_t width, int32_t height,
              int32_t channels, VkDescriptorSet descriptor_set,
              VkDescriptorSetLayout descriptor_set_layout);

  /// @brief Tüm Vulkan kaynaklarını serbest bırak. Idempotent; destructor da çağırır.
  void Destroy(VkDevice device);

  VkImageView GetImageView() const { return mImageView; }
  VkSampler GetSampler() const { return mSampler; }
  VkDescriptorSet GetDescriptorSet() const { return mDescriptorSet; }
  int32_t GetWidth() const { return mWidth; }
  int32_t GetHeight() const { return mHeight; }
  bool IsValid() const { return mImage != VK_NULL_HANDLE; }

 private:
  /// @brief Host-visible staging buffer oluştur ve veriyi kopyala.
  bool CreateStagingBuffer(VkDevice device, VkPhysicalDevice phys_device,
                           const uint8_t* rgba_data, VkDeviceSize size,
                           VkBuffer& out_buf, VkDeviceMemory& out_mem);

  /// @brief Device-local VkImage + VkImageView oluştur.
  bool CreateImage(VkDevice device, VkPhysicalDevice phys_device,
                   uint32_t width, uint32_t height);

  /// @brief VkSampler oluştur (linear filter, clamp to edge).
  bool CreateSampler(VkDevice device);

  /// @brief Tek seferlik command buffer gönder (layout geçişi + copy).
  bool RecordAndSubmitUpload(VkDevice device, VkQueue queue,
                             VkCommandPool cmd_pool,
                             VkBuffer staging_buf);

  /// @brief Image layout geçişi (barrier).
  static void TransitionImageLayout(VkCommandBuffer cmd, VkImage image,
                                    VkImageLayout old_layout,
                                    VkImageLayout new_layout);

  /// @brief Descriptor set'e image + sampler bağla.
  void UpdateDescriptorSet(VkDevice device,
                           VkDescriptorSetLayout layout);

  VkDevice mDevice{VK_NULL_HANDLE};  // RAII için Upload'da saklanır
  VkImage mImage{VK_NULL_HANDLE};
  VkDeviceMemory mImageMemory{VK_NULL_HANDLE};
  VkImageView mImageView{VK_NULL_HANDLE};
  VkSampler mSampler{VK_NULL_HANDLE};
  VkDescriptorSet mDescriptorSet{VK_NULL_HANDLE};

  int32_t mWidth{0};
  int32_t mHeight{0};

  uint32_t mWidth_u{0};
  uint32_t mHeight_u{0};
};

}  // namespace sdl_painter
