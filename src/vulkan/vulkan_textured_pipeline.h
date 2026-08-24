#pragma once

#include "sdl_painter/vertex.h"

#include <array>
#include <cstdint>
#include <string>
#include <vector>
#include <vulkan/vulkan.h>

#include "vk_blend.h"
#include "vulkan_pipeline.h"  // PushConstants

namespace sdl_painter {

/// @brief Maksimum eş zamanlı texture descriptor sayısı.
///
/// Bu kadar farklı texture aynı anda descriptor pool'da bulunabilir.
constexpr uint32_t kMaxTextureDescriptors = 256;

/// @brief Textured (sampler2D) çizim için Vulkan graphics pipeline yönetimi.
///
/// Untextured pipeline'a ek olarak:
///   - set=0, binding=0 → combined image sampler için descriptor set layout
///   - descriptor pool (kMaxTextureDescriptors adet set)
///   - textured.vert.spv + textured.frag.spv
class VulkanTexturedPipeline {
 public:
  VulkanTexturedPipeline() = default;
  /// @brief RAII destructor — Init başarılı olduysa kaynakları otomatik yıkar.
  ~VulkanTexturedPipeline();

  VulkanTexturedPipeline(const VulkanTexturedPipeline&) = delete;
  VulkanTexturedPipeline& operator=(const VulkanTexturedPipeline&) = delete;

  /// @brief Pipeline, layout ve descriptor pool'u oluştur.
  ///
  /// SPIR-V modülleri binary'ye gömülüdür; çalışma zamanında dosya okunmaz.
  /// @param device Logical device.
  /// @param render_pass Pipeline'ın bağlanacağı render pass.
  /// @return Başarı durumunda true.
  bool Init(VkDevice device, VkRenderPass render_pass);

  /// @brief Tüm kaynakları serbest bırak. Idempotent; destructor da çağırır.
  void Destroy(VkDevice device);

  /// @brief Descriptor pool'dan yeni bir set tahsis et.
  /// @return Başarısız olursa VK_NULL_HANDLE.
  VkDescriptorSet AllocateDescriptorSet(VkDevice device);

  /// @brief Tahsis edilmiş descriptor set'i pool'a iade et.
  void FreeDescriptorSet(VkDevice device, VkDescriptorSet set);

  /// @brief Karıştırma moduna karşılık gelen pipeline varyantı.
  ///
  /// Vulkan'da blend, pipeline'ın sabit durumudur; mod başına ayrı bir
  /// varyant üretilir (bkz. vk_blend.h).
  VkPipeline GetPipeline(BlendMode mode = BlendMode::kAlpha) const {
    return mPipelines[vk_detail::BlendIndex(mode)];
  }
  VkPipelineLayout GetLayout() const { return mLayout; }
  VkDescriptorSetLayout GetDescriptorSetLayout() const {
    return mDescriptorSetLayout;
  }

 private:
  /// @brief Gömülü SPIR-V kelime dizisinden VkShaderModule oluştur.
  /// @param code SPIR-V kelime dizisi (uint32 hizalı).
  /// @param byte_size Dizinin bayt cinsinden boyutu.
  static VkShaderModule CreateShaderModule(VkDevice device,
                                           const uint32_t* code,
                                           std::size_t byte_size);

  VkDevice mDevice{VK_NULL_HANDLE};  // RAII için Init'te saklanır
  std::array<VkPipeline, kBlendModeCount> mPipelines{};
  VkPipelineLayout mLayout{VK_NULL_HANDLE};
  VkDescriptorSetLayout mDescriptorSetLayout{VK_NULL_HANDLE};
  VkDescriptorPool mDescriptorPool{VK_NULL_HANDLE};
};

}  // namespace sdl_painter
