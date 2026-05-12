#pragma once

#include <vulkan/vulkan.h>

#include <cstdint>

namespace sdl_painter {

/// @brief Host-visible ring buffer — CPU'dan doğrudan yazılabilen vertex buffer.
///
/// Her frame başında ResetRing() çağrılır; frame boyunca Write() ile veri eklenir.
/// Staging buffer kullanmaz — VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
/// VK_MEMORY_PROPERTY_HOST_COHERENT_BIT belleğine kalıcı olarak map'lenir.
class VulkanBuffer {
 public:
  VulkanBuffer() = default;
  ~VulkanBuffer() = default;

  VulkanBuffer(const VulkanBuffer&) = delete;
  VulkanBuffer& operator=(const VulkanBuffer&) = delete;

  /// @brief Buffer'ı oluştur ve kalıcı olarak map'le.
  /// @param device Logical Vulkan device.
  /// @param phys_device Physical device (bellek tipi sorgusu için).
  /// @param capacity Toplam buffer boyutu (byte).
  /// @param usage VkBufferUsageFlagBits kombinasyonu (örn. VERTEX_BUFFER_BIT).
  /// @return Başarı durumunda true.
  bool Init(VkDevice device, VkPhysicalDevice phys_device, VkDeviceSize capacity,
            VkBufferUsageFlags usage);

  /// @brief Buffer ve belleği serbest bırak.
  void Destroy(VkDevice device);

  /// @brief Ring buffer'a veri yaz.
  ///
  /// İstenen veriyi bir sonraki hizalanmış konuma kopyalar.
  /// Ring doluysa false döner (bu frame'de drop edilir ve uyarı loglanır).
  /// @param data Kopyalanacak veri.
  /// @param byte_size Veri boyutu (byte).
  /// @param alignment Başlangıç adres hizalaması (genellikle 4).
  /// @param out_offset_bytes [out] Buffer içindeki başlangıç ofseti (byte).
  /// @return Yazma başarılıysa true.
  bool Write(const void* data, VkDeviceSize byte_size, VkDeviceSize alignment,
             VkDeviceSize& out_offset_bytes);

  /// @brief Her frame başında ring pozisyonunu sıfırla.
  void ResetRing();

  VkBuffer GetBuffer() const { return mBuffer; }
  VkDeviceSize GetCapacity() const { return mCapacity; }

 private:
  VkBuffer mBuffer{VK_NULL_HANDLE};
  VkDeviceMemory mMemory{VK_NULL_HANDLE};
  void* mMapped{nullptr};
  VkDeviceSize mCapacity{0};
  VkDeviceSize mHead{0};  // Sonraki yazma pozisyonu (byte offset)
};

}  // namespace sdl_painter
