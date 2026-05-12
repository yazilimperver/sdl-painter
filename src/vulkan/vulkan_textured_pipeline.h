#pragma once

#include <vulkan/vulkan.h>

#include <cstdint>
#include <string>
#include <vector>

#include "sdl_painter/vertex.h"
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
  /// @param device Logical device.
  /// @param render_pass Pipeline'ın bağlanacağı render pass.
  /// @param shader_dir SPIR-V .spv dosyalarının dizini.
  /// @return Başarı durumunda true.
  bool Init(VkDevice device, VkRenderPass render_pass,
            const std::string& shader_dir);

  /// @brief Tüm kaynakları serbest bırak. Idempotent; destructor da çağırır.
  void Destroy(VkDevice device);

  /// @brief Descriptor pool'dan yeni bir set tahsis et.
  /// @return Başarısız olursa VK_NULL_HANDLE.
  VkDescriptorSet AllocateDescriptorSet(VkDevice device);

  /// @brief Tahsis edilmiş descriptor set'i pool'a iade et.
  void FreeDescriptorSet(VkDevice device, VkDescriptorSet set);

  VkPipeline GetPipeline() const { return mPipeline; }
  VkPipelineLayout GetLayout() const { return mLayout; }
  VkDescriptorSetLayout GetDescriptorSetLayout() const {
    return mDescriptorSetLayout;
  }

 private:
  static VkShaderModule LoadSpv(VkDevice device, const std::string& path);

  VkDevice mDevice{VK_NULL_HANDLE};  // RAII için Init'te saklanır
  VkPipeline mPipeline{VK_NULL_HANDLE};
  VkPipelineLayout mLayout{VK_NULL_HANDLE};
  VkDescriptorSetLayout mDescriptorSetLayout{VK_NULL_HANDLE};
  VkDescriptorPool mDescriptorPool{VK_NULL_HANDLE};
};

}  // namespace sdl_painter
