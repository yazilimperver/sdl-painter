#include "vulkan_pipeline.h"

#include "sdl_painter/embedded_vk_shaders.h"

#include <array>
#include <cstddef>
#include <spdlog/spdlog.h>
#include <stdexcept>

#include "vk_check.h"

namespace sdl_painter {

VulkanPipeline::~VulkanPipeline() {
  if (mDevice != VK_NULL_HANDLE) {
    Destroy(mDevice);
  }
}

// Yardımcı: gömülü SPIR-V'den shader modülü oluştur
VkShaderModule VulkanPipeline::CreateShaderModule(VkDevice device,
                                                  const uint32_t* code,
                                                  std::size_t byte_size) {
  VkShaderModuleCreateInfo ci{};
  ci.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
  ci.codeSize = byte_size;
  ci.pCode = code;

  VkShaderModule mod = VK_NULL_HANDLE;
  VK_CHECK_HANDLE(vkCreateShaderModule(device, &ci, nullptr, &mod));
  return mod;
}

// Init
bool VulkanPipeline::Init(VkDevice device, VkRenderPass render_pass) {
  // --- Shader modülleri (binary'ye gömülü SPIR-V) ---
  VkShaderModule vert_mod = CreateShaderModule(device, detail::kUntexturedVert,
                                               sizeof(detail::kUntexturedVert));
  VkShaderModule frag_mod = CreateShaderModule(device, detail::kUntexturedFrag,
                                               sizeof(detail::kUntexturedFrag));

  if (vert_mod == VK_NULL_HANDLE || frag_mod == VK_NULL_HANDLE) {
    if (vert_mod != VK_NULL_HANDLE) {
      vkDestroyShaderModule(device, vert_mod, nullptr);
    }
    if (frag_mod != VK_NULL_HANDLE) {
      vkDestroyShaderModule(device, frag_mod, nullptr);
    }
    return false;
  }

  std::array<VkPipelineShaderStageCreateInfo, 2> shader_stages{};
  shader_stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  shader_stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
  shader_stages[0].module = vert_mod;
  shader_stages[0].pName = "main";

  shader_stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  shader_stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
  shader_stages[1].module = frag_mod;
  shader_stages[1].pName = "main";

  // --- Vertex input — Vertex struct layout ---
  VkVertexInputBindingDescription binding{};
  binding.binding = 0;
  binding.stride = static_cast<uint32_t>(sizeof(Vertex));
  binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

  std::array<VkVertexInputAttributeDescription, 2> attrs{};
  // location=0: position (vec2 float)
  attrs[0].binding = 0;
  attrs[0].location = 0;
  attrs[0].format = VK_FORMAT_R32G32_SFLOAT;
  attrs[0].offset = static_cast<uint32_t>(offsetof(Vertex, x));
  // location=1: color (uint8 RGBA → normalize [0,1])
  attrs[1].binding = 0;
  attrs[1].location = 1;
  attrs[1].format = VK_FORMAT_R8G8B8A8_UNORM;
  attrs[1].offset = static_cast<uint32_t>(offsetof(Vertex, r));

  VkPipelineVertexInputStateCreateInfo vertex_input{};
  vertex_input.sType =
      VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
  vertex_input.vertexBindingDescriptionCount = 1;
  vertex_input.pVertexBindingDescriptions = &binding;
  vertex_input.vertexAttributeDescriptionCount = 2;
  vertex_input.pVertexAttributeDescriptions = attrs.data();

  // --- Input assembly ---
  VkPipelineInputAssemblyStateCreateInfo input_assembly{};
  input_assembly.sType =
      VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
  input_assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
  input_assembly.primitiveRestartEnable = VK_FALSE;

  // --- Dynamic viewport + scissor ---
  std::array<VkDynamicState, 2> dyn_states = {VK_DYNAMIC_STATE_VIEWPORT,
                                              VK_DYNAMIC_STATE_SCISSOR};
  VkPipelineDynamicStateCreateInfo dynamic_state{};
  dynamic_state.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
  dynamic_state.dynamicStateCount = 2;
  dynamic_state.pDynamicStates = dyn_states.data();

  VkPipelineViewportStateCreateInfo viewport_state{};
  viewport_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
  viewport_state.viewportCount = 1;
  viewport_state.scissorCount = 1;
  // pViewports/pScissors null — dinamik state ile set edilir.

  // --- Rasterization ---
  VkPipelineRasterizationStateCreateInfo rasterization{};
  rasterization.sType =
      VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
  rasterization.depthClampEnable = VK_FALSE;
  rasterization.rasterizerDiscardEnable = VK_FALSE;
  rasterization.polygonMode = VK_POLYGON_MODE_FILL;
  rasterization.cullMode = VK_CULL_MODE_NONE;  // 2D: culling kapalı
  rasterization.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
  rasterization.depthBiasEnable = VK_FALSE;
  rasterization.lineWidth = 1.0F;

  // --- Multisampling ---
  VkPipelineMultisampleStateCreateInfo multisampling{};
  multisampling.sType =
      VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
  multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
  multisampling.sampleShadingEnable = VK_FALSE;

  // --- Karistirma: mod basina bir pipeline varyanti ---
  // Vulkan 1.1'de blend pipeline'in sabit durumu; calisma zamaninda
  // degistirilemez (bkz. vk_blend.h). Dizilerin omru
  // vkCreateGraphicsPipelines cagrisini kapsamali.
  std::array<VkPipelineColorBlendAttachmentState, kBlendModeCount>
      attachments{};
  std::array<VkPipelineColorBlendStateCreateInfo, kBlendModeCount> blends{};
  for (std::size_t i = 0; i < kBlendModeCount; ++i) {
    attachments[i] = vk_detail::BlendAttachmentFor(static_cast<BlendMode>(i));
    blends[i].sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    blends[i].logicOpEnable = VK_FALSE;
    blends[i].attachmentCount = 1;
    blends[i].pAttachments = &attachments[i];
  }

  // --- Pipeline layout (push constants) ---
  VkPushConstantRange push_range{};
  push_range.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
  push_range.offset = 0;
  push_range.size = static_cast<uint32_t>(sizeof(PushConstants));

  VkPipelineLayoutCreateInfo layout_ci{};
  layout_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  layout_ci.pushConstantRangeCount = 1;
  layout_ci.pPushConstantRanges = &push_range;

  if (vkCreatePipelineLayout(device, &layout_ci, nullptr, &mLayout) !=
      VK_SUCCESS) {
    spdlog::error("VulkanPipeline: vkCreatePipelineLayout başarısız.");
    // Shader modülleri henüz silinmedi — leak'i önle.
    vkDestroyShaderModule(device, vert_mod, nullptr);
    vkDestroyShaderModule(device, frag_mod, nullptr);
    return false;
  }

  // --- Graphics pipeline ---
  VkGraphicsPipelineCreateInfo pipeline_ci{};
  pipeline_ci.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
  pipeline_ci.stageCount = 2;
  pipeline_ci.pStages = shader_stages.data();
  pipeline_ci.pVertexInputState = &vertex_input;
  pipeline_ci.pInputAssemblyState = &input_assembly;
  pipeline_ci.pViewportState = &viewport_state;
  pipeline_ci.pRasterizationState = &rasterization;
  pipeline_ci.pMultisampleState = &multisampling;
  pipeline_ci.pDepthStencilState = nullptr;
  // pColorBlendState varyant basina asagida atanir.
  pipeline_ci.pDynamicState = &dynamic_state;
  pipeline_ci.layout = mLayout;
  pipeline_ci.renderPass = render_pass;
  pipeline_ci.subpass = 0;

  // Tek cagrida dort varyant: aralarindaki TEK fark blend durumu.
  std::array<VkGraphicsPipelineCreateInfo, kBlendModeCount> infos{};
  for (std::size_t i = 0; i < kBlendModeCount; ++i) {
    infos[i] = pipeline_ci;
    infos[i].pColorBlendState = &blends[i];
  }

  VkResult res = vkCreateGraphicsPipelines(
      device, VK_NULL_HANDLE, static_cast<uint32_t>(kBlendModeCount),
      infos.data(), nullptr, mPipelines.data());

  // Shader modülleri artık gerekmez — pipeline oluşturulduktan hemen sonra sil.
  vkDestroyShaderModule(device, vert_mod, nullptr);
  vkDestroyShaderModule(device, frag_mod, nullptr);

  if (res != VK_SUCCESS) {
    spdlog::error("vkCreateGraphicsPipelines failed: {}",
                  vk_detail::VkResultToString(res));
    // Pipeline layout artık sahipsiz — temizle.
    vkDestroyPipelineLayout(device, mLayout, nullptr);
    mLayout = VK_NULL_HANDLE;
    return false;
  }

  spdlog::info("VulkanPipeline initialized (untextured).");
  // RAII için device'i sakla.
  mDevice = device;
  return true;
}

void VulkanPipeline::Destroy(VkDevice device) {
  VkDevice d = (mDevice != VK_NULL_HANDLE) ? mDevice : device;
  if (d == VK_NULL_HANDLE) {
    return;
  }
  for (auto& pipeline : mPipelines) {
    if (pipeline != VK_NULL_HANDLE) {
      vkDestroyPipeline(d, pipeline, nullptr);
      pipeline = VK_NULL_HANDLE;
    }
  }
  if (mLayout != VK_NULL_HANDLE) {
    vkDestroyPipelineLayout(d, mLayout, nullptr);
    mLayout = VK_NULL_HANDLE;
  }
  mDevice = VK_NULL_HANDLE;
}

}  // namespace sdl_painter
