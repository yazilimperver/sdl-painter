#pragma once

#include "sdl_painter/vertex.h"

#include <cstdint>
#include <string>
#include <vector>
#include <vulkan/vulkan.h>

namespace sdl_painter {

/// @brief VulkanRenderer'ın DrawTriangles çağrıları için push constant bloğu.
///
/// Vertex shader'daki PushConstants layout'u ile byte-for-byte eşleşmeli.
/// Toplam boyut: 64 + 64 + 16 + 4 = 148 byte (Vulkan garantili 256 byte limiti içinde).
struct alignas(4) PushConstants {
  float projection[16]{};  ///< mat4 ortografik projeksiyon (column-major)
  float model
      [16]{};  ///< mat4 2D affine transform (3x3 → 4x4 padded, column-major)
  float tint_color[4]{};  ///< vec4 tint rengi [0,1]
  float opacity{1.0f};    ///< Global opaklık [0,1]
};

/// @brief Untextured (düz renk) çizim için Vulkan graphics pipeline yönetimi.
///
/// VkPipelineLayout + VkPipeline sahipliği.
/// Init() bir kez çağrılır; Destroy() Shutdown sırasında.
class VulkanPipeline {
 public:
  VulkanPipeline() = default;
  /// @brief RAII destructor — Init başarılı olduysa kaynakları otomatik yıkar.
  ~VulkanPipeline();

  VulkanPipeline(const VulkanPipeline&) = delete;
  VulkanPipeline& operator=(const VulkanPipeline&) = delete;

  /// @brief Pipeline ve layout'u oluştur.
  /// @param device Logical device.
  /// @param render_pass Pipeline'ın bağlanacağı render pass.
  /// @param shader_dir SPIR-V .spv dosyalarının dizini.
  /// @return Başarı durumunda true.
  bool Init(VkDevice device, VkRenderPass render_pass,
            const std::string& shader_dir);

  /// @brief Pipeline kaynaklarını serbest bırak. Idempotent; destructor da çağırır.
  void Destroy(VkDevice device);

  VkPipeline GetPipeline() const { return mPipeline; }
  VkPipelineLayout GetLayout() const { return mLayout; }

 private:
  /// @brief .spv dosyasını yükle ve VkShaderModule oluştur.
  static VkShaderModule LoadSpv(VkDevice device, const std::string& path);

  VkDevice mDevice{VK_NULL_HANDLE};  // RAII için Init'te saklanır
  VkPipeline mPipeline{VK_NULL_HANDLE};
  VkPipelineLayout mLayout{VK_NULL_HANDLE};
};

}  // namespace sdl_painter
