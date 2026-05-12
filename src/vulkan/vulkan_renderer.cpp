#include "vulkan_renderer.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include <spdlog/spdlog.h>

#include <cstring>

#include "sdl_painter/color.h"
#include "sdl_painter/vertex.h"
#include "vk_check.h"

namespace sdl_painter {

VulkanRenderer::~VulkanRenderer() { Shutdown(); }

bool VulkanRenderer::Initialize(SDL_Window* window) {
  mWindow = window;

  mContext = std::make_unique<VkContext>();
  if (!mContext->Initialize(window)) return false;

  uint32_t width = 0;
  uint32_t height = 0;
  QueryWindowDrawableSize(width, height);

  mSwapchain = std::make_unique<VkSwapchain>();
  if (!mSwapchain->Initialize(mContext.get(), width, height)) return false;

  mFrameSync = std::make_unique<VkFrameSync>();
  if (!mFrameSync->Initialize(mContext.get(), mSwapchain->GetImageCount()))
    return false;

  // Default clear: siyah.
  mClearValue.color = {{0.0f, 0.0f, 0.0f, 1.0f}};
  mViewportW = static_cast<int32_t>(width);
  mViewportH = static_cast<int32_t>(height);

  // Phase 5b: vertex ring buffer
  constexpr VkDeviceSize kRingSize = 4 * 1024 * 1024;  // 4 MB
  mVertexRing = std::make_unique<VulkanBuffer>();
  if (!mVertexRing->Init(mContext->GetDevice(), mContext->GetPhysicalDevice(),
                         kRingSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT)) {
    spdlog::error("VulkanRenderer: VulkanBuffer init failed.");
    return false;
  }

  // Phase 5b: untextured graphics pipeline
  mShaderDir = ResolveShaderDir();
  mPipeline = std::make_unique<VulkanPipeline>();
  if (!mPipeline->Init(mContext->GetDevice(), mSwapchain->GetRenderPass(),
                       mShaderDir)) {
    spdlog::warn(
        "VulkanRenderer: pipeline init failed (shader files missing?). "
        "DrawTriangles will be a no-op.");
    mPipeline.reset();
  }

  // Phase 5c: textured vertex ring buffer
  mTexturedVertexRing = std::make_unique<VulkanBuffer>();
  if (!mTexturedVertexRing->Init(mContext->GetDevice(),
                                  mContext->GetPhysicalDevice(),
                                  kRingSize,
                                  VK_BUFFER_USAGE_VERTEX_BUFFER_BIT)) {
    spdlog::error("VulkanRenderer: textured VulkanBuffer init failed.");
    return false;
  }

  // Phase 5c: textured pipeline
  mTexturedPipeline = std::make_unique<VulkanTexturedPipeline>();
  if (!mTexturedPipeline->Init(mContext->GetDevice(),
                                mSwapchain->GetRenderPass(), mShaderDir)) {
    spdlog::warn(
        "VulkanRenderer: textured pipeline init failed (shader files "
        "missing?). DrawTextured will be a no-op.");
    mTexturedPipeline.reset();
  }

  // Identity projection başlangıç değeri (Painter SetProjectionMatrix çağırır)
  mPushConstants = PushConstants{};

  spdlog::info("VulkanRenderer initialized.");
  return true;
}

void VulkanRenderer::Shutdown() {
  if (mContext && mContext->GetDevice() != VK_NULL_HANDLE) {
    vkDeviceWaitIdle(mContext->GetDevice());
    // Texture'ları önce sil (descriptor set pool mTexturedPipeline'da)
    for (auto& [handle, tex] : mTextures) {
      tex->Destroy(mContext->GetDevice());
    }
    mTextures.clear();
    if (mTexturedPipeline) {
      mTexturedPipeline->Destroy(mContext->GetDevice());
      mTexturedPipeline.reset();
    }
    if (mTexturedVertexRing) {
      mTexturedVertexRing->Destroy(mContext->GetDevice());
      mTexturedVertexRing.reset();
    }
    if (mPipeline) {
      mPipeline->Destroy(mContext->GetDevice());
      mPipeline.reset();
    }
    if (mVertexRing) {
      mVertexRing->Destroy(mContext->GetDevice());
      mVertexRing.reset();
    }
  }
  mFrameSync.reset();
  mSwapchain.reset();
  mContext.reset();
  mWindow = nullptr;
}

void VulkanRenderer::QueryWindowDrawableSize(uint32_t& width,
                                             uint32_t& height) const {
  int w = 0;
  int h = 0;
  SDL_GetWindowSizeInPixels(mWindow, &w, &h);
  width = static_cast<uint32_t>(w > 0 ? w : 1);
  height = static_cast<uint32_t>(h > 0 ? h : 1);
}

bool VulkanRenderer::AcquireNextImage() {
  VkDevice device = mContext->GetDevice();
  VkFence fence = mFrameSync->GetInFlightFence(mCurrentFrame);
  vkWaitForFences(device, 1, &fence, VK_TRUE, UINT64_MAX);

  // Acquire için per-frame semaphore kullan (image index henüz bilinmiyor).
  // Submit aşamasında mCurrentImageIndex ile per-image semaphore'a geçilir.
  // Acquire için döngüsel semaphore seçimi: image_count kadar semaphore
  // döngüsel olarak seçilir. fence beklendikten sonra mCurrentFrame slotu
  // güvenli — ama swapchain 3 image, frames-in-flight 2 olunca yeterli değil.
  // Bu yüzden mImageAvailable swapchain image count kadar oluşturuldu;
  // acquire'a mCurrentFrame % image_count ile seçilir.
  // Acquire semaphore'u: swapchain image sayısı kadar semaphore arasından
  // döngüsel seçim yapılır. mAcquireSlot her frame'de ilerler ve submit'te
  // wait için saklanır. Fence beklendikten sonra slot güvenle yeniden
  // kullanılabilir çünkü bir önceki submit tamamlandı.
  const uint32_t image_count = mSwapchain->GetImageCount();
  mAcquireSlot = (mAcquireSlot + 1) % image_count;
  VkSemaphore acquire_sem =
      mFrameSync->GetImageAvailableSemaphore(mAcquireSlot);

  VkResult res = vkAcquireNextImageKHR(device, mSwapchain->GetSwapchain(),
                                       UINT64_MAX, acquire_sem, VK_NULL_HANDLE,
                                       &mCurrentImageIndex);
  if (res == VK_ERROR_OUT_OF_DATE_KHR) {
    uint32_t w = 0;
    uint32_t h = 0;
    QueryWindowDrawableSize(w, h);
    mSwapchain->Recreate(w, h);
    mViewportW = static_cast<int32_t>(mSwapchain->GetExtent().width);
    mViewportH = static_cast<int32_t>(mSwapchain->GetExtent().height);
    mSwapchainOutOfDate = true;
    return false;
  } else if (res != VK_SUCCESS && res != VK_SUBOPTIMAL_KHR) {
    spdlog::error("vkAcquireNextImageKHR failed: {}",
                  vk_detail::VkResultToString(res));
    return false;
  }

  vkResetFences(device, 1, &fence);
  return true;
}

void VulkanRenderer::BeginFrame() {
  mSwapchainOutOfDate = false;
  if (!AcquireNextImage()) {
    mFrameActive = false;
    return;
  }
  mFrameActive = true;

  VkCommandBuffer cmd = mFrameSync->GetCommandBuffer(mCurrentFrame);
  vkResetCommandBuffer(cmd, 0);

  // Her frame başında ring buffer'ları sıfırla.
  if (mVertexRing) mVertexRing->ResetRing();
  if (mTexturedVertexRing) mTexturedVertexRing->ResetRing();

  VkCommandBufferBeginInfo bi{};
  bi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
  VK_CHECK_RETURN(vkBeginCommandBuffer(cmd, &bi));

  VkRenderPassBeginInfo rp{};
  rp.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
  rp.renderPass = mSwapchain->GetRenderPass();
  rp.framebuffer = mSwapchain->GetFramebuffer(mCurrentImageIndex);
  rp.renderArea.offset = {0, 0};
  rp.renderArea.extent = mSwapchain->GetExtent();
  rp.clearValueCount = 1;
  rp.pClearValues = &mClearValue;
  vkCmdBeginRenderPass(cmd, &rp, VK_SUBPASS_CONTENTS_INLINE);

  // Dynamic viewport + scissor — pipeline'dan önce set edilmeli.
  ApplyDynamicViewportScissor(cmd);
}

void VulkanRenderer::EndFrame() {
  if (!mFrameActive) return;

  VkCommandBuffer cmd = mFrameSync->GetCommandBuffer(mCurrentFrame);
  vkCmdEndRenderPass(cmd);
  VK_CHECK_RETURN(vkEndCommandBuffer(cmd));

  SubmitAndPresent();

  mCurrentFrame = (mCurrentFrame + 1) % VkFrameSync::kMaxFramesInFlight;
  mFrameActive = false;
}

void VulkanRenderer::SubmitAndPresent() {
  VkCommandBuffer cmd = mFrameSync->GetCommandBuffer(mCurrentFrame);
  // Acquire sırasında kaydedilen slot'tan semaphore'u al.
  VkSemaphore image_avail =
      mFrameSync->GetImageAvailableSemaphore(mAcquireSlot);
  // renderFinished image_index ile indekslenir — presentation engine
  // semaphore'u image'a bağlar, frame_index ile çakışma yaratır.
  VkSemaphore render_done =
      mFrameSync->GetRenderFinishedSemaphore(mCurrentImageIndex);
  VkFence fence = mFrameSync->GetInFlightFence(mCurrentFrame);

  VkPipelineStageFlags wait_stage =
      VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

  VkSubmitInfo si{};
  si.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  si.waitSemaphoreCount = 1;
  si.pWaitSemaphores = &image_avail;
  si.pWaitDstStageMask = &wait_stage;
  si.commandBufferCount = 1;
  si.pCommandBuffers = &cmd;
  si.signalSemaphoreCount = 1;
  si.pSignalSemaphores = &render_done;

  VK_CHECK_RETURN(vkQueueSubmit(mContext->GetGraphicsQueue(), 1, &si, fence));

  VkSwapchainKHR swap = mSwapchain->GetSwapchain();
  VkPresentInfoKHR pi{};
  pi.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
  pi.waitSemaphoreCount = 1;
  pi.pWaitSemaphores = &render_done;
  pi.swapchainCount = 1;
  pi.pSwapchains = &swap;
  pi.pImageIndices = &mCurrentImageIndex;

  VkResult res = vkQueuePresentKHR(mContext->GetPresentQueue(), &pi);
  if (res == VK_ERROR_OUT_OF_DATE_KHR || res == VK_SUBOPTIMAL_KHR) {
    uint32_t w = 0;
    uint32_t h = 0;
    QueryWindowDrawableSize(w, h);
    mSwapchain->Recreate(w, h);
    mViewportW = static_cast<int32_t>(mSwapchain->GetExtent().width);
    mViewportH = static_cast<int32_t>(mSwapchain->GetExtent().height);
    mSwapchainOutOfDate = true;
  } else if (res != VK_SUCCESS) {
    spdlog::error("vkQueuePresentKHR failed: {}",
                  vk_detail::VkResultToString(res));
  }
}

std::string VulkanRenderer::ResolveShaderDir() const {
  // Build sistemi tarafından tanımlanan makroyu önce dene.
#ifdef SDLPAINTER_VULKAN_SHADER_DIR
  return SDLPAINTER_VULKAN_SHADER_DIR;
#else
  // Fallback: executable yanında vulkan_shaders/ dizini.
  const char* base = SDL_GetBasePath();
  if (base) {
    return std::string(base) + "vulkan_shaders";
  }
  return "vulkan_shaders";
#endif
}

void VulkanRenderer::ApplyDynamicViewportScissor(VkCommandBuffer cmd) const {
  const VkExtent2D extent = mSwapchain->GetExtent();

  VkViewport vp{};
  vp.x = static_cast<float>(mViewportX);
  vp.y = static_cast<float>(mViewportY);
  vp.width = mViewportW > 0 ? static_cast<float>(mViewportW)
                             : static_cast<float>(extent.width);
  vp.height = mViewportH > 0 ? static_cast<float>(mViewportH)
                              : static_cast<float>(extent.height);
  vp.minDepth = 0.0f;
  vp.maxDepth = 1.0f;
  vkCmdSetViewport(cmd, 0, 1, &vp);

  VkRect2D scissor{};
  if (mScissorEnabled) {
    scissor.offset = {mScissorX, mScissorY};
    scissor.extent = {static_cast<uint32_t>(mScissorW),
                      static_cast<uint32_t>(mScissorH)};
  } else {
    scissor.offset = {0, 0};
    scissor.extent = extent;
  }
  vkCmdSetScissor(cmd, 0, 1, &scissor);
}

void VulkanRenderer::SetViewport(int32_t x, int32_t y, int32_t width,
                                 int32_t height) {
  mViewportX = x;
  mViewportY = y;
  mViewportW = width;
  mViewportH = height;
}

void VulkanRenderer::SetScissor(int32_t x, int32_t y, int32_t width,
                                int32_t height) {
  mScissorEnabled = true;
  mScissorX = x;
  mScissorY = y;
  mScissorW = width;
  mScissorH = height;
}

void VulkanRenderer::ClearScissor() { mScissorEnabled = false; }

void VulkanRenderer::Clear(const Color& color) {
  // BeginFrame'deki render pass load_op=CLEAR olduğundan ilk temizleme orada
  // uygulanır (mClearValue bir sonraki frame'in başlangıç değeri olur).
  // Aktif frame için mevcut render pass içinde vkCmdClearAttachments kullanılır.
  mClearValue.color.float32[0] = color.RedF();
  mClearValue.color.float32[1] = color.GreenF();
  mClearValue.color.float32[2] = color.BlueF();
  mClearValue.color.float32[3] = color.AlphaF();

  if (!mFrameActive) return;

  VkCommandBuffer cmd = mFrameSync->GetCommandBuffer(mCurrentFrame);
  VkClearAttachment clear{};
  clear.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  clear.colorAttachment = 0;
  clear.clearValue = mClearValue;

  VkExtent2D extent = mSwapchain->GetExtent();
  VkClearRect rect{};
  rect.rect.offset = {0, 0};
  rect.rect.extent = extent;
  rect.baseArrayLayer = 0;
  rect.layerCount = 1;

  vkCmdClearAttachments(cmd, 1, &clear, 1, &rect);
}

void VulkanRenderer::SetOpacity(float alpha) { mOpacity = alpha; }

void VulkanRenderer::DrawTriangles(const std::vector<Vertex>& vertices) {
  if (!mFrameActive || vertices.empty()) return;
  if (!mPipeline || !mVertexRing) return;

  const VkDeviceSize byte_size =
      static_cast<VkDeviceSize>(vertices.size() * sizeof(Vertex));
  constexpr VkDeviceSize kAlignment = 4;
  VkDeviceSize offset_bytes = 0;

  if (!mVertexRing->Write(vertices.data(), byte_size, kAlignment, offset_bytes))
    return;

  // Renk vertex'te taşındığı için tint her zaman 1.0.
  mPushConstants.tint_color[0] = 1.0f;
  mPushConstants.tint_color[1] = 1.0f;
  mPushConstants.tint_color[2] = 1.0f;
  mPushConstants.tint_color[3] = 1.0f;
  mPushConstants.opacity = mOpacity;

  VkCommandBuffer cmd = mFrameSync->GetCommandBuffer(mCurrentFrame);

  vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                    mPipeline->GetPipeline());

  VkBuffer buf = mVertexRing->GetBuffer();
  vkCmdBindVertexBuffers(cmd, 0, 1, &buf, &offset_bytes);

  vkCmdPushConstants(cmd, mPipeline->GetLayout(), VK_SHADER_STAGE_VERTEX_BIT,
                     0, static_cast<uint32_t>(sizeof(PushConstants)),
                     &mPushConstants);

  vkCmdDraw(cmd, static_cast<uint32_t>(vertices.size()), 1, 0, 0);
}

TextureHandle VulkanRenderer::CreateTexture(const uint8_t* data,
                                            int32_t width, int32_t height,
                                            int32_t channels) {
  if (!mTexturedPipeline || !data || width <= 0 || height <= 0) {
    return kInvalidTexture;
  }

  VkDescriptorSet desc_set =
      mTexturedPipeline->AllocateDescriptorSet(mContext->GetDevice());
  if (desc_set == VK_NULL_HANDLE) return kInvalidTexture;

  auto tex = std::make_unique<VulkanTexture>();
  if (!tex->Upload(mContext.get(), mFrameSync->GetCommandPool(), data, width,
                   height, channels, desc_set,
                   mTexturedPipeline->GetDescriptorSetLayout())) {
    mTexturedPipeline->FreeDescriptorSet(mContext->GetDevice(), desc_set);
    return kInvalidTexture;
  }

  const TextureHandle handle = mNextTextureHandle++;
  mTextures[handle] = std::move(tex);
  return handle;
}

void VulkanRenderer::DestroyTexture(TextureHandle handle) {
  auto it = mTextures.find(handle);
  if (it == mTextures.end()) return;

  vkDeviceWaitIdle(mContext->GetDevice());
  if (mTexturedPipeline) {
    mTexturedPipeline->FreeDescriptorSet(mContext->GetDevice(),
                                          it->second->GetDescriptorSet());
  }
  it->second->Destroy(mContext->GetDevice());
  mTextures.erase(it);
}

void VulkanRenderer::DrawTextured(const std::vector<TexturedVertex>& vertices,
                                   TextureHandle texture) {
  if (!mFrameActive || vertices.empty()) return;
  if (!mTexturedPipeline || !mTexturedVertexRing) return;

  auto it = mTextures.find(texture);
  if (it == mTextures.end()) return;

  const VkDeviceSize byte_size =
      static_cast<VkDeviceSize>(vertices.size() * sizeof(TexturedVertex));
  constexpr VkDeviceSize kAlignment = 4;
  VkDeviceSize offset_bytes = 0;

  if (!mTexturedVertexRing->Write(vertices.data(), byte_size, kAlignment,
                                   offset_bytes)) {
    return;
  }

  // Renk vertex'te taşındığı için tint her zaman 1.0.
  PushConstants pc = mPushConstants;
  pc.tint_color[0] = 1.0f;
  pc.tint_color[1] = 1.0f;
  pc.tint_color[2] = 1.0f;
  pc.tint_color[3] = 1.0f;
  pc.opacity = mOpacity;

  VkCommandBuffer cmd = mFrameSync->GetCommandBuffer(mCurrentFrame);

  vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                    mTexturedPipeline->GetPipeline());

  VkDescriptorSet desc_set = it->second->GetDescriptorSet();
  vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                           mTexturedPipeline->GetLayout(), 0, 1, &desc_set,
                           0, nullptr);

  VkBuffer buf = mTexturedVertexRing->GetBuffer();
  vkCmdBindVertexBuffers(cmd, 0, 1, &buf, &offset_bytes);

  vkCmdPushConstants(cmd, mTexturedPipeline->GetLayout(),
                     VK_SHADER_STAGE_VERTEX_BIT, 0,
                     static_cast<uint32_t>(sizeof(PushConstants)), &pc);

  vkCmdDraw(cmd, static_cast<uint32_t>(vertices.size()), 1, 0, 0);
}

void VulkanRenderer::SetProjectionMatrix(const float* mat4) {
  std::memcpy(mPushConstants.projection, mat4, 16 * sizeof(float));
}

void VulkanRenderer::SetModelMatrix(const float* mat3) {
  // 3x3 row-major affine → 4x4 column-major dönüşümü.
  // mat3 layout (row-major): [m00 m01 tx | m10 m11 ty | 0 0 1]
  // İndeksler:                 [0]  [1]  [2]  [3]  [4]  [5]
  float m[16] = {
      mat3[0], mat3[3], 0.0f, 0.0f,  // column 0
      mat3[1], mat3[4], 0.0f, 0.0f,  // column 1
      0.0f,    0.0f,    1.0f, 0.0f,  // column 2
      mat3[2], mat3[5], 0.0f, 1.0f,  // column 3
  };
  std::memcpy(mPushConstants.model, m, 16 * sizeof(float));
}

}  // namespace sdl_painter
