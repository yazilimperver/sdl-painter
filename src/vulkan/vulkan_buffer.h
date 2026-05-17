#pragma once

#include <cstdint>
#include <vector>
#include <vulkan/vulkan.h>

namespace sdl_painter {

/// @brief Host-visible ring buffer — CPU'dan doğrudan yazılabilen vertex buffer.
///
/// Buffer, `frame_slot_count` parçaya bölünür; her parça (slot) için ayrı bir
/// `head` tutulur. Bu sayede CPU frame N+1'i hazırlarken GPU hâlâ frame N'in
/// komut buffer'ını işliyor olsa bile, aynı bölge üzerine yazım yapılmaz —
/// yani RAW (read-after-write) hazard önlenir. Toplam kapasite slot sayısına
/// eşit oranda bölünür (örn. 8 MB / 2 slot = 4 MB / slot).
///
/// Her frame başında çağrı sırası:
///   ResetRing(frame_slot)  — sadece o slot'un head'ini sıfırlar.
///   Write(..., frame_slot) — o slot içine yazar, mutlak offset döndürür.
///
/// Staging buffer kullanmaz — VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
/// VK_MEMORY_PROPERTY_HOST_COHERENT_BIT belleğine kalıcı olarak map'lenir.
class VulkanBuffer {
 public:
  VulkanBuffer() = default;
  /// @brief RAII destructor — Init başarılı olduysa kaynakları otomatik yıkar.
  ~VulkanBuffer();

  VulkanBuffer(const VulkanBuffer&) = delete;
  VulkanBuffer& operator=(const VulkanBuffer&) = delete;

  /// @brief Buffer'ı oluştur ve kalıcı olarak map'le.
  /// @param device Logical Vulkan device.
  /// @param phys_device Physical device (bellek tipi sorgusu için).
  /// @param capacity Toplam buffer boyutu (byte) — slot sayısına bölünür.
  /// @param usage VkBufferUsageFlagBits kombinasyonu (örn. VERTEX_BUFFER_BIT).
  /// @param frame_slot_count Buffer'ın bölüneceği eş zamanlı slot sayısı
  ///        (genellikle frames-in-flight kadar; default 1 = klasik tek slot).
  /// @return Başarı durumunda true.
  bool Init(VkDevice device, VkPhysicalDevice phys_device,
            VkDeviceSize capacity, VkBufferUsageFlags usage,
            uint32_t frame_slot_count = 1);

  /// @brief Buffer ve belleği serbest bırak. Idempotent; destructor da çağırır.
  /// @param device Önceki Init'te verilen device ile aynı olmalı (ileri uyum
  /// için parametre tutuluyor; ihmal edilen versiyon önerilir).
  void Destroy(VkDevice device);

  /// @brief Belirtilen slot'a veri yaz.
  ///
  /// İstenen veriyi slot içindeki bir sonraki hizalanmış konuma kopyalar.
  /// Slot doluysa false döner (bu frame'de drop edilir, uyarı loglanır).
  /// @param data Kopyalanacak veri.
  /// @param byte_size Veri boyutu (byte).
  /// @param alignment Başlangıç adres hizalaması (genellikle 4).
  /// @param frame_slot Hedef slot (`[0, frame_slot_count)`).
  /// @param out_offset_bytes [out] Buffer içindeki **mutlak** başlangıç
  ///        ofseti (byte) — `vkCmdBindVertexBuffers`'a doğrudan verilebilir.
  /// @return Yazma başarılıysa true.
  bool Write(const void* data, VkDeviceSize byte_size, VkDeviceSize alignment,
             uint32_t frame_slot, VkDeviceSize& out_offset_bytes);

  /// @brief Verilen frame slot'unun head'ini sıfırla. Diğer slotlar
  /// (hâlâ GPU tarafından kullanılıyor olabilir) etkilenmez.
  void ResetRing(uint32_t frame_slot);

  VkBuffer GetBuffer() const { return mBuffer; }
  VkDeviceSize GetCapacity() const { return mCapacity; }

 private:
  VkDevice mDevice{VK_NULL_HANDLE};  // RAII için Init'te saklanan device
  VkBuffer mBuffer{VK_NULL_HANDLE};
  VkDeviceMemory mMemory{VK_NULL_HANDLE};
  void* mMapped{nullptr};
  VkDeviceSize mCapacity{0};      // Toplam buffer kapasitesi (byte)
  VkDeviceSize mSlotCapacity{0};  // Slot başına kapasite (byte)
  uint32_t mFrameSlotCount{1};    // Eş zamanlı slot sayısı
  std::vector<VkDeviceSize> mHeads;  // Slot başına bağımsız head (byte offset)
};

}  // namespace sdl_painter
