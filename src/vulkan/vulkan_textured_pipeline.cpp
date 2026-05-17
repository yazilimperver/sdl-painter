#include "vulkan_textured_pipeline.h"

#include <cstddef>
#include <fstream>
#include <spdlog/spdlog.h>
#include <vector>

#include "vk_check.h"

namespace sdl_painter {

VulkanTexturedPipeline::~VulkanTexturedPipeline() {
  if (mDevice != VK_NULL_HANDLE) {
    Destroy(mDevice);
  }
}

// ---------------------------------------------------------------------------
// Yardımcı: .spv yükle
// ---------------------------------------------------------------------------
VkShaderModule VulkanTexturedPipeline::LoadSpv(VkDevice device,
                                               const std::string& path) {
  std::ifstream file(path, std::ios::ate | std::ios::binary);
  if (!file.is_open()) {
    spdlog::error("VulkanTexturedPipeline: shader açılamadı: {}", path);
    return VK_NULL_HANDLE;
  }
  const std::size_t file_size = static_cast<std::size_t>(file.tellg());
  std::vector<char> code(file_size);
  file.seekg(0);
  file.read(code.data(), static_cast<std::streamsize>(file_size));

  VkShaderModuleCreateInfo ci{};
  ci.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
  ci.codeSize = file_size;
  ci.pCode = reinterpret_cast<const uint32_t*>(code.data());  // NOLINT

  VkShaderModule mod = VK_NULL_HANDLE;
  VK_CHECK_HANDLE(vkCreateShaderModule(device, &ci, nullptr, &mod));
  return mod;
}

// ---------------------------------------------------------------------------
// Init
// ---------------------------------------------------------------------------
bool VulkanTexturedPipeline::Init(VkDevice device, VkRenderPass render_pass,
                                  const std::string& shader_dir) {
  // --- Descriptor set layout: set=0, binding=0 → combined image sampler ---
  VkDescriptorSetLayoutBinding sampler_binding{};
  sampler_binding.binding = 0;
  sampler_binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  sampler_binding.descriptorCount = 1;
  sampler_binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
  sampler_binding.pImmutableSamplers = nullptr;

  VkDescriptorSetLayoutCreateInfo layout_ci{};
  layout_ci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
  layout_ci.bindingCount = 1;
  layout_ci.pBindings = &sampler_binding;
  if (vkCreateDescriptorSetLayout(device, &layout_ci, nullptr,
                                  &mDescriptorSetLayout) != VK_SUCCESS) {
    spdlog::error(
        "VulkanTexturedPipeline: vkCreateDescriptorSetLayout başarısız.");
    return false;
  }

  // --- Descriptor pool ---
  VkDescriptorPoolSize pool_size{};
  pool_size.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  pool_size.descriptorCount = kMaxTextureDescriptors;

  VkDescriptorPoolCreateInfo pool_ci{};
  pool_ci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  pool_ci.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
  pool_ci.maxSets = kMaxTextureDescriptors;
  pool_ci.poolSizeCount = 1;
  pool_ci.pPoolSizes = &pool_size;
  if (vkCreateDescriptorPool(device, &pool_ci, nullptr, &mDescriptorPool) !=
      VK_SUCCESS) {
    spdlog::error("VulkanTexturedPipeline: vkCreateDescriptorPool başarısız.");
    vkDestroyDescriptorSetLayout(device, mDescriptorSetLayout, nullptr);
    mDescriptorSetLayout = VK_NULL_HANDLE;
    return false;
  }

  // --- Shader modülleri ---
  const std::string vert_path = shader_dir + "/textured.vert.spv";
  const std::string frag_path = shader_dir + "/textured.frag.spv";

  VkShaderModule vert_mod = LoadSpv(device, vert_path);
  VkShaderModule frag_mod = LoadSpv(device, frag_path);

  if (vert_mod == VK_NULL_HANDLE || frag_mod == VK_NULL_HANDLE) {
    if (vert_mod != VK_NULL_HANDLE)
      vkDestroyShaderModule(device, vert_mod, nullptr);
    if (frag_mod != VK_NULL_HANDLE)
      vkDestroyShaderModule(device, frag_mod, nullptr);
    // Önceki kaynaklar (descriptor set layout + pool) için cleanup.
    vkDestroyDescriptorPool(device, mDescriptorPool, nullptr);
    vkDestroyDescriptorSetLayout(device, mDescriptorSetLayout, nullptr);
    mDescriptorPool = VK_NULL_HANDLE;
    mDescriptorSetLayout = VK_NULL_HANDLE;
    return false;
  }

  VkPipelineShaderStageCreateInfo shader_stages[2]{};
  shader_stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  shader_stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
  shader_stages[0].module = vert_mod;
  shader_stages[0].pName = "main";

  shader_stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  shader_stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
  shader_stages[1].module = frag_mod;
  shader_stages[1].pName = "main";

  // --- Vertex input — TexturedVertex struct layout ---
  VkVertexInputBindingDescription binding{};
  binding.binding = 0;
  binding.stride = static_cast<uint32_t>(sizeof(TexturedVertex));
  binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

  VkVertexInputAttributeDescription attrs[3]{};
  // location=0: position (vec2)
  attrs[0].binding = 0;
  attrs[0].location = 0;
  attrs[0].format = VK_FORMAT_R32G32_SFLOAT;
  attrs[0].offset = static_cast<uint32_t>(offsetof(TexturedVertex, x));
  // location=1: tex coord (vec2)
  attrs[1].binding = 0;
  attrs[1].location = 1;
  attrs[1].format = VK_FORMAT_R32G32_SFLOAT;
  attrs[1].offset = static_cast<uint32_t>(offsetof(TexturedVertex, u));
  // location=2: color (RGBA8 → normalize [0,1])
  attrs[2].binding = 0;
  attrs[2].location = 2;
  attrs[2].format = VK_FORMAT_R8G8B8A8_UNORM;
  attrs[2].offset = static_cast<uint32_t>(offsetof(TexturedVertex, r));

  VkPipelineVertexInputStateCreateInfo vertex_input{};
  vertex_input.sType =
      VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
  vertex_input.vertexBindingDescriptionCount = 1;
  vertex_input.pVertexBindingDescriptions = &binding;
  vertex_input.vertexAttributeDescriptionCount = 3;
  vertex_input.pVertexAttributeDescriptions = attrs;

  // --- Input assembly ---
  VkPipelineInputAssemblyStateCreateInfo input_assembly{};
  input_assembly.sType =
      VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
  input_assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
  input_assembly.primitiveRestartEnable = VK_FALSE;

  // --- Dynamic viewport + scissor ---
  VkDynamicState dyn_states[] = {VK_DYNAMIC_STATE_VIEWPORT,
                                 VK_DYNAMIC_STATE_SCISSOR};
  VkPipelineDynamicStateCreateInfo dynamic_state{};
  dynamic_state.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
  dynamic_state.dynamicStateCount = 2;
  dynamic_state.pDynamicStates = dyn_states;

  VkPipelineViewportStateCreateInfo viewport_state{};
  viewport_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
  viewport_state.viewportCount = 1;
  viewport_state.scissorCount = 1;

  // --- Rasterization ---
  VkPipelineRasterizationStateCreateInfo rasterization{};
  rasterization.sType =
      VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
  rasterization.depthClampEnable = VK_FALSE;
  rasterization.rasterizerDiscardEnable = VK_FALSE;
  rasterization.polygonMode = VK_POLYGON_MODE_FILL;
  rasterization.cullMode = VK_CULL_MODE_NONE;
  rasterization.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
  rasterization.depthBiasEnable = VK_FALSE;
  rasterization.lineWidth = 1.0F;

  // --- Multisampling ---
  VkPipelineMultisampleStateCreateInfo multisampling{};
  multisampling.sType =
      VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
  multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
  multisampling.sampleShadingEnable = VK_FALSE;

  // --- Alpha blending ---
  VkPipelineColorBlendAttachmentState blend_attachment{};
  blend_attachment.blendEnable = VK_TRUE;
  blend_attachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
  blend_attachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
  blend_attachment.colorBlendOp = VK_BLEND_OP_ADD;
  blend_attachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
  blend_attachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
  blend_attachment.alphaBlendOp = VK_BLEND_OP_ADD;
  blend_attachment.colorWriteMask =
      VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
      VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

  VkPipelineColorBlendStateCreateInfo color_blend{};
  color_blend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
  color_blend.logicOpEnable = VK_FALSE;
  color_blend.attachmentCount = 1;
  color_blend.pAttachments = &blend_attachment;

  // --- Pipeline layout: descriptor set layout + push constants ---
  VkPushConstantRange push_range{};
  push_range.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
  push_range.offset = 0;
  push_range.size = static_cast<uint32_t>(sizeof(PushConstants));

  VkPipelineLayoutCreateInfo pl_ci{};
  pl_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  pl_ci.setLayoutCount = 1;
  pl_ci.pSetLayouts = &mDescriptorSetLayout;
  pl_ci.pushConstantRangeCount = 1;
  pl_ci.pPushConstantRanges = &push_range;
  if (vkCreatePipelineLayout(device, &pl_ci, nullptr, &mLayout) != VK_SUCCESS) {
    spdlog::error("VulkanTexturedPipeline: vkCreatePipelineLayout başarısız.");
    vkDestroyShaderModule(device, vert_mod, nullptr);
    vkDestroyShaderModule(device, frag_mod, nullptr);
    vkDestroyDescriptorPool(device, mDescriptorPool, nullptr);
    vkDestroyDescriptorSetLayout(device, mDescriptorSetLayout, nullptr);
    mDescriptorPool = VK_NULL_HANDLE;
    mDescriptorSetLayout = VK_NULL_HANDLE;
    return false;
  }

  // --- Graphics pipeline ---
  VkGraphicsPipelineCreateInfo pipeline_ci{};
  pipeline_ci.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
  pipeline_ci.stageCount = 2;
  pipeline_ci.pStages = shader_stages;
  pipeline_ci.pVertexInputState = &vertex_input;
  pipeline_ci.pInputAssemblyState = &input_assembly;
  pipeline_ci.pViewportState = &viewport_state;
  pipeline_ci.pRasterizationState = &rasterization;
  pipeline_ci.pMultisampleState = &multisampling;
  pipeline_ci.pDepthStencilState = nullptr;
  pipeline_ci.pColorBlendState = &color_blend;
  pipeline_ci.pDynamicState = &dynamic_state;
  pipeline_ci.layout = mLayout;
  pipeline_ci.renderPass = render_pass;
  pipeline_ci.subpass = 0;

  VkResult res = vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1,
                                           &pipeline_ci, nullptr, &mPipeline);

  vkDestroyShaderModule(device, vert_mod, nullptr);
  vkDestroyShaderModule(device, frag_mod, nullptr);

  if (res != VK_SUCCESS) {
    spdlog::error(
        "VulkanTexturedPipeline: vkCreateGraphicsPipelines hatası: {}",
        vk_detail::VkResultToString(res));
    // Tüm önceki kaynakları temizle.
    vkDestroyPipelineLayout(device, mLayout, nullptr);
    vkDestroyDescriptorPool(device, mDescriptorPool, nullptr);
    vkDestroyDescriptorSetLayout(device, mDescriptorSetLayout, nullptr);
    mLayout = VK_NULL_HANDLE;
    mDescriptorPool = VK_NULL_HANDLE;
    mDescriptorSetLayout = VK_NULL_HANDLE;
    return false;
  }

  spdlog::info("VulkanTexturedPipeline initialized.");
  // RAII için device'i sakla.
  mDevice = device;
  return true;
}

// ---------------------------------------------------------------------------
// Descriptor set tahsis/iade
// ---------------------------------------------------------------------------
VkDescriptorSet VulkanTexturedPipeline::AllocateDescriptorSet(VkDevice device) {
  VkDescriptorSetAllocateInfo alloc_info{};
  alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  alloc_info.descriptorPool = mDescriptorPool;
  alloc_info.descriptorSetCount = 1;
  alloc_info.pSetLayouts = &mDescriptorSetLayout;

  VkDescriptorSet set = VK_NULL_HANDLE;
  if (vkAllocateDescriptorSets(device, &alloc_info, &set) != VK_SUCCESS) {
    spdlog::error("VulkanTexturedPipeline: descriptor set tahsisi başarısız.");
    return VK_NULL_HANDLE;
  }
  return set;
}

void VulkanTexturedPipeline::FreeDescriptorSet(VkDevice device,
                                               VkDescriptorSet set) {
  if (set != VK_NULL_HANDLE && mDescriptorPool != VK_NULL_HANDLE) {
    vkFreeDescriptorSets(device, mDescriptorPool, 1, &set);
  }
}

// ---------------------------------------------------------------------------
// Destroy
// ---------------------------------------------------------------------------
void VulkanTexturedPipeline::Destroy(VkDevice device) {
  VkDevice d = (mDevice != VK_NULL_HANDLE) ? mDevice : device;
  if (d == VK_NULL_HANDLE)
    return;
  if (mPipeline != VK_NULL_HANDLE) {
    vkDestroyPipeline(d, mPipeline, nullptr);
    mPipeline = VK_NULL_HANDLE;
  }
  if (mLayout != VK_NULL_HANDLE) {
    vkDestroyPipelineLayout(d, mLayout, nullptr);
    mLayout = VK_NULL_HANDLE;
  }
  if (mDescriptorPool != VK_NULL_HANDLE) {
    vkDestroyDescriptorPool(d, mDescriptorPool, nullptr);
    mDescriptorPool = VK_NULL_HANDLE;
  }
  if (mDescriptorSetLayout != VK_NULL_HANDLE) {
    vkDestroyDescriptorSetLayout(d, mDescriptorSetLayout, nullptr);
    mDescriptorSetLayout = VK_NULL_HANDLE;
  }
  mDevice = VK_NULL_HANDLE;
}

}  // namespace sdl_painter
