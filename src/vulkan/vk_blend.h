#pragma once

/// @file vk_blend.h
/// @brief @ref sdl_painter::BlendMode → Vulkan blend attachment eşlemesi.
///
/// OpenGL'de karıştırma modu `glBlendFunc` ile çalışma zamanında
/// değiştirilebilir. Vulkan 1.1'de ise blend, grafik pipeline'ın ın sabit
/// durumunun parçasıdır ve dinamik olarak değiştirilemez.
///
/// Bu yüzden Vulkan tarafında mod başına ayrı bir pipeline üretilir ve
/// çizim anında doğru varyant bağlanır. Pipeline nesneleri küçük olduğundan
/// dördünü birden başlangıçta yaratmak, çizim sırasında pipeline derlemekten
/// çok daha iyi.

#include "sdl_painter/renderer.h"

#include <vulkan/vulkan.h>

namespace sdl_painter::vk_detail {

/// @brief Verilen karıştırma modu için renk eki (attachment) durumu.
inline VkPipelineColorBlendAttachmentState BlendAttachmentFor(BlendMode mode) {
  VkPipelineColorBlendAttachmentState a{};
  a.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                     VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
  a.colorBlendOp = VK_BLEND_OP_ADD;
  a.alphaBlendOp = VK_BLEND_OP_ADD;

  switch (mode) {
    case BlendMode::kNone:
      a.blendEnable = VK_FALSE;
      return a;

    case BlendMode::kAdditive:
      // GL karsiligi: glBlendFunc(GL_SRC_ALPHA, GL_ONE)
      a.blendEnable = VK_TRUE;
      a.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
      a.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
      a.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
      a.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
      return a;

    case BlendMode::kMultiply:
      // GL karsiligi: glBlendFunc(GL_DST_COLOR, GL_ZERO)
      a.blendEnable = VK_TRUE;
      a.srcColorBlendFactor = VK_BLEND_FACTOR_DST_COLOR;
      a.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
      a.srcAlphaBlendFactor = VK_BLEND_FACTOR_DST_ALPHA;
      a.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
      return a;

    case BlendMode::kAlpha:
    default:
      // GL karsiligi:
      // glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA,
      //                     GL_ONE, GL_ONE_MINUS_SRC_ALPHA)
      //
      // Alfa dst faktoru eskiden ZERO idi (aOut = aSrc): yari saydam bir
      // sekil cizmek hedefin alfasini o bolgede DUSURUYORDU. Dogru "over"
      // bilesimi aOut = aSrc + aDst(1 - aSrc); asagidaki bunu verir.
      a.blendEnable = VK_TRUE;
      a.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
      a.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
      a.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
      a.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
      return a;
  }
}

/// @brief Modu pipeline varyantı dizisindeki indekse çevir.
inline std::size_t BlendIndex(BlendMode mode) {
  const auto i = static_cast<std::size_t>(mode);
  return i < kBlendModeCount ? i : 0;
}

}  // namespace sdl_painter::vk_detail
