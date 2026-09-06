#include "vulkan_renderer.h"

#include "sdl_painter/color.h"
#include "sdl_painter/vertex.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>

#include <algorithm>
#include <array>
#include <cstring>
#include <spdlog/spdlog.h>

#include "vk_check.h"

namespace sdl_painter {

VulkanRenderer::~VulkanRenderer() {
  Shutdown();
}

bool VulkanRenderer::Initialize(SDL_Window* window) {
  mWindow = window;

  mContext = std::make_unique<VkContext>();
  if (!mContext->Initialize(window)) {
    return false;
  }

  uint32_t width = 0;
  uint32_t height = 0;
  QueryWindowDrawableSize(width, height);

  mSwapchain = std::make_unique<VkSwapchain>();
  if (!mSwapchain->Initialize(mContext.get(), width, height)) {
    return false;
  }

  mFrameSync = std::make_unique<VkFrameSync>();
  if (!mFrameSync->Initialize(mContext.get(), mSwapchain->GetImageCount())) {
    return false;
  }

  mClearValue.color = {{0.0F, 0.0F, 0.0F, 1.0F}};
  mViewportW = static_cast<int32_t>(width);
  mViewportH = static_cast<int32_t>(height);

  // (CPU/GPU paralelliği için RAW hazard'ı önler; bkz. K1).
  constexpr VkDeviceSize kPerSlotSize = 4 * 1024 * 1024;  // 4 MB / slot
  constexpr VkDeviceSize kRingSize =
      kPerSlotSize * VkFrameSync::kMaxFramesInFlight;
  mVertexRing = std::make_unique<VulkanBuffer>();
  if (!mVertexRing->Init(mContext->GetDevice(), mContext->GetPhysicalDevice(),
                         kRingSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                         VkFrameSync::kMaxFramesInFlight)) {
    spdlog::error("VulkanRenderer: VulkanBuffer init failed.");
    return false;
  }

  // Pipeline kurulumu başarısız olursa sert hata veriyoruz. Eskiden burada
  // uyarı loglanıp devam ediliyordu; gerekçe, .spv dosyalarının çalışma
  // zamanında eksik olabilmesiydi. Shader'lar artık binary'ye gömülü olduğu
  // için o senaryo imkânsız (bkz. ADR-009) ve sessizce devam etmek kullanıcıya
  // sebebi log'a gömülü siyah bir pencere bırakıyordu.
  mPipeline = std::make_unique<VulkanPipeline>();
  if (!mPipeline->Init(mContext->GetDevice(), mSwapchain->GetRenderPass())) {
    spdlog::error("VulkanRenderer: untextured pipeline init failed.");
    return false;
  }

  // Textured vertex ring buffer — aynı slot mantığı.
  mTexturedVertexRing = std::make_unique<VulkanBuffer>();
  if (!mTexturedVertexRing->Init(
          mContext->GetDevice(), mContext->GetPhysicalDevice(), kRingSize,
          VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VkFrameSync::kMaxFramesInFlight)) {
    spdlog::error("VulkanRenderer: textured VulkanBuffer init failed.");
    return false;
  }

  mTexturedPipeline = std::make_unique<VulkanTexturedPipeline>();
  if (!mTexturedPipeline->Init(mContext->GetDevice(),
                               mSwapchain->GetRenderPass())) {
    spdlog::error("VulkanRenderer: textured pipeline init failed.");
    return false;
  }

  mPushConstants = PushConstants{};

  spdlog::info("VulkanRenderer initialized.");
  return true;
}

void VulkanRenderer::Shutdown() {
  if (mContext != nullptr && mContext->GetDevice() != VK_NULL_HANDLE) {
    vkDeviceWaitIdle(mContext->GetDevice());
    // Silinmeyi bekleyenleri zorla temizle (device idle, güvenli).
    ProcessPendingTextureDeletes(/*force=*/true);
    // Texture'ları önce sil (descriptor set pool mTexturedPipeline'da)
    for (auto& [handle, tex] : mTextures) {
      tex->Destroy(mContext->GetDevice());
    }
    mTextures.clear();
    // Hedefler de descriptor set'lerini aynı havuzdan almıştı.
    for (auto& [handle, entry] : mRenderTargets) {
      entry.target->Destroy(mContext->GetDevice());
    }
    mRenderTargets.clear();
    mCurrentTarget = kInvalidRenderTarget;
    if (mOffscreenTexturedPipeline) {
      mOffscreenTexturedPipeline->Destroy(mContext->GetDevice());
      mOffscreenTexturedPipeline.reset();
    }
    if (mOffscreenPipeline) {
      mOffscreenPipeline->Destroy(mContext->GetDevice());
      mOffscreenPipeline.reset();
    }
    if (mOffscreenRenderPass != VK_NULL_HANDLE) {
      vkDestroyRenderPass(mContext->GetDevice(), mOffscreenRenderPass, nullptr);
      mOffscreenRenderPass = VK_NULL_HANDLE;
    }
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

void VulkanRenderer::RecreateSwapchainOrDefer() {
  // Yüzey çizilemez durumdaysa (simge durumu) swapchain'i yeniden inşa etme;
  // 0x0 extent Vulkan tarafından reddedilir. Bayrağı kaldır, pencere geri
  // geldiğinde BeginFrame yeniden dener.
  if (mSwapchain == nullptr || !mSwapchain->IsSurfaceRenderable()) {
    mSwapchainNeedsRecreate = true;
    return;
  }
  uint32_t w = 0;
  uint32_t h = 0;
  QueryWindowDrawableSize(w, h);
  if (!mSwapchain->Recreate(w, h)) {
    mSwapchainNeedsRecreate = true;
    return;
  }
  mViewportW = static_cast<int32_t>(mSwapchain->GetExtent().width);
  mViewportH = static_cast<int32_t>(mSwapchain->GetExtent().height);
  mSwapchainNeedsRecreate = false;
}

bool VulkanRenderer::AcquireNextImage() {
  VkDevice device = mContext->GetDevice();
  VkFence fence = mFrameSync->GetInFlightFence(mCurrentFrame);
  vkWaitForFences(device, 1, &fence, VK_TRUE, UINT64_MAX);

  // Acquire semaphore'u frame-in-flight slotu ile indekslenir. Hemen yukarıda
  // beklenen fence, bu slotu kullanan önceki submit'in tamamlandığını garanti
  // eder; dolayısıyla semaphore unsignaled ve yeniden kullanılabilir.
  // (Signal semaphore'u ise image_index ile indekslenir — presentation engine
  // onu image'a bağlar; bkz. SubmitAndPresent.)
  VkSemaphore acquire_sem =
      mFrameSync->GetImageAvailableSemaphore(mCurrentFrame);

  VkResult res =
      vkAcquireNextImageKHR(device, mSwapchain->GetSwapchain(), UINT64_MAX,
                            acquire_sem, VK_NULL_HANDLE, &mCurrentImageIndex);
  if (res == VK_ERROR_OUT_OF_DATE_KHR) {
    mSwapchainOutOfDate = true;
    RecreateSwapchainOrDefer();
    return false;
  }
  if (res != VK_SUCCESS && res != VK_SUBOPTIMAL_KHR) {
    spdlog::error("vkAcquireNextImageKHR failed: {}",
                  vk_detail::VkResultToString(res));
    return false;
  }

  vkResetFences(device, 1, &fence);
  return true;
}

void VulkanRenderer::BeginFrame() {
  mSwapchainOutOfDate = false;
  mFrameActive = false;
  // Hedef secimi kare sinirini asmaz: her kare ekranda baslar.
  mCurrentTarget = kInvalidRenderTarget;

  // Pencere simge durumuna küçültüldüğünde yüzey 0x0 olur. Bu durumda
  // swapchain/framebuffer oluşturmak ve render pass başlatmak Vulkan
  // geçerlilik kurallarını ihlal eder (VUID-VkSwapchainCreateInfoKHR-
  // imageExtent-01689 vb.). Kareyi tamamen atla; pencere geri geldiğinde
  // mSwapchainNeedsRecreate ile swapchain yeniden inşa edilir.
  if (mSwapchain == nullptr || !mSwapchain->IsSurfaceRenderable()) {
    mSwapchainNeedsRecreate = true;
    return;
  }

  if (mSwapchainNeedsRecreate) {
    uint32_t w = 0;
    uint32_t h = 0;
    QueryWindowDrawableSize(w, h);
    if (!mSwapchain->Recreate(w, h)) {
      return;
    }
    mViewportW = static_cast<int32_t>(mSwapchain->GetExtent().width);
    mViewportH = static_cast<int32_t>(mSwapchain->GetExtent().height);
    mSwapchainNeedsRecreate = false;
  }

  if (!AcquireNextImage()) {
    return;
  }
  mFrameActive = true;

  VkCommandBuffer cmd = mFrameSync->GetCommandBuffer(mCurrentFrame);
  vkResetCommandBuffer(cmd, 0);

  // Sadece bu frame'in slot'unu sıfırla — diğer slot hâlâ GPU'da kullanılabilir.
  if (mVertexRing != nullptr) {
    mVertexRing->ResetRing(mCurrentFrame);
  }
  if (mTexturedVertexRing != nullptr) {
    mTexturedVertexRing->ResetRing(mCurrentFrame);
  }

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
  if (!mFrameActive) {
    return;
  }

  // Kullanici hedefte biraktiysa ekrana don: aksi halde bu karede swapchain
  // image'ina hic yazilmaz ve resume pass'in initialLayout beklentisi de
  // karsilanmaz.
  if (mCurrentTarget != kInvalidRenderTarget) {
    SetRenderTarget(kInvalidRenderTarget);
  }

  VkCommandBuffer cmd = mFrameSync->GetCommandBuffer(mCurrentFrame);
  vkCmdEndRenderPass(cmd);
  VK_CHECK_RETURN(vkEndCommandBuffer(cmd));

  SubmitAndPresent();

  mCurrentFrame = (mCurrentFrame + 1) % VkFrameSync::kMaxFramesInFlight;
  ++mFrameCounter;
  mFrameActive = false;

  // Silinmeyi bekleyen texture'lardan süresi dolanları serbest bırak.
  ProcessPendingTextureDeletes(/*force=*/false);
}

void VulkanRenderer::SubmitAndPresent() {
  VkCommandBuffer cmd = mFrameSync->GetCommandBuffer(mCurrentFrame);
  VkSemaphore image_avail =
      mFrameSync->GetImageAvailableSemaphore(mCurrentFrame);
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
    mSwapchainOutOfDate = true;
    RecreateSwapchainOrDefer();
  } else if (res != VK_SUCCESS) {
    spdlog::error("vkQueuePresentKHR failed: {}",
                  vk_detail::VkResultToString(res));
  }
}

void VulkanRenderer::ApplyDynamicViewportScissor(VkCommandBuffer cmd) const {
  ApplyDynamicViewport(cmd);
  ApplyDynamicScissor(cmd);
}

void VulkanRenderer::ApplyDynamicViewport(VkCommandBuffer cmd) const {
  const VkExtent2D kExtent = CurrentExtent();

  VkViewport vp{};
  vp.x = static_cast<float>(mViewportX);
  vp.y = static_cast<float>(mViewportY);
  vp.width = mViewportW > 0 ? static_cast<float>(mViewportW)
                            : static_cast<float>(kExtent.width);
  vp.height = mViewportH > 0 ? static_cast<float>(mViewportH)
                             : static_cast<float>(kExtent.height);
  vp.minDepth = 0.0F;
  vp.maxDepth = 1.0F;
  vkCmdSetViewport(cmd, 0, 1, &vp);
}

void VulkanRenderer::ApplyDynamicScissor(VkCommandBuffer cmd) const {
  const VkExtent2D kExtent = CurrentExtent();

  VkRect2D scissor{};
  if (mScissorEnabled) {
    // Scissor, yürürlükteki hedefin sınırlarını aşamaz (Vulkan geçerlilik
    // kuralı):
    // negatif offset ve taşan genişlik kırpılır.
    const int32_t kX0 = std::max(0, mScissorX);
    const int32_t kY0 = std::max(0, mScissorY);
    const int32_t kX1 = std::min(static_cast<int32_t>(kExtent.width),
                                 mScissorX + std::max(0, mScissorW));
    const int32_t kY1 = std::min(static_cast<int32_t>(kExtent.height),
                                 mScissorY + std::max(0, mScissorH));
    scissor.offset = {kX0, kY0};
    scissor.extent = {static_cast<uint32_t>(std::max(0, kX1 - kX0)),
                      static_cast<uint32_t>(std::max(0, kY1 - kY0))};
  } else {
    scissor.offset = {0, 0};
    scissor.extent = kExtent;
  }
  vkCmdSetScissor(cmd, 0, 1, &scissor);
}

void VulkanRenderer::SetViewport(int32_t x, int32_t y, int32_t width,
                                 int32_t height) {
  mViewportX = x;
  mViewportY = y;
  mViewportW = width;
  mViewportH = height;
  // Kare ortasında çağrıldıysa dinamik state'i hemen komut buffer'ına yaz;
  // aksi halde değişiklik bir sonraki BeginFrame'e kadar etkisiz kalır.
  if (mFrameActive) {
    ApplyDynamicViewport(mFrameSync->GetCommandBuffer(mCurrentFrame));
  }
}

void VulkanRenderer::SetScissor(int32_t x, int32_t y, int32_t width,
                                int32_t height) {
  mScissorEnabled = true;
  mScissorX = x;
  mScissorY = y;
  mScissorW = width;
  mScissorH = height;
  if (mFrameActive) {
    ApplyDynamicScissor(mFrameSync->GetCommandBuffer(mCurrentFrame));
  }
}

void VulkanRenderer::ClearScissor() {
  mScissorEnabled = false;
  if (mFrameActive) {
    ApplyDynamicScissor(mFrameSync->GetCommandBuffer(mCurrentFrame));
  }
}

void VulkanRenderer::Clear(const Color& color) {
  // BeginFrame'deki render pass load_op=CLEAR olduğundan ilk temizleme orada
  // uygulanır (mClearValue bir sonraki frame'in başlangıç değeri olur).
  // Aktif frame için mevcut render pass içinde vkCmdClearAttachments kullanılır.
  mClearValue.color.float32[0] = color.RedF();
  mClearValue.color.float32[1] = color.GreenF();
  mClearValue.color.float32[2] = color.BlueF();
  mClearValue.color.float32[3] = color.AlphaF();

  if (!mFrameActive) {
    return;
  }

  VkCommandBuffer cmd = mFrameSync->GetCommandBuffer(mCurrentFrame);
  VkClearAttachment clear{};
  clear.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  clear.colorAttachment = 0;
  clear.clearValue = mClearValue;

  VkExtent2D extent = CurrentExtent();
  VkClearRect rect{};
  rect.rect.offset = {0, 0};
  rect.rect.extent = extent;
  rect.baseArrayLayer = 0;
  rect.layerCount = 1;

  vkCmdClearAttachments(cmd, 1, &clear, 1, &rect);
}

void VulkanRenderer::SetOpacity(float alpha) {
  mOpacity = alpha;
}

void VulkanRenderer::DrawTriangles(const std::vector<Vertex>& vertices) {
  if (!mFrameActive || vertices.empty()) {
    return;
  }
  if (vertices.size() % 3 != 0) {
    spdlog::error(
        "VulkanRenderer::DrawTriangles: vertex sayisi ({}) 3'un kati degil; "
        "cizim atlandi.",
        vertices.size());
    return;
  }
  const VulkanPipeline* pipeline = ActivePipeline();
  if (pipeline == nullptr || mVertexRing == nullptr) {
    return;
  }

  const auto kByteSize =
      static_cast<VkDeviceSize>(vertices.size() * sizeof(Vertex));
  constexpr VkDeviceSize kAlignment = 4;
  VkDeviceSize offset_bytes = 0;

  if (!mVertexRing->Write(vertices.data(), kByteSize, kAlignment, mCurrentFrame,
                          offset_bytes)) {
    return;
  }

  // Renk vertex'te taşındığı için tint her zaman 1.0.
  mPushConstants.tint_color[0] = 1.0F;
  mPushConstants.tint_color[1] = 1.0F;
  mPushConstants.tint_color[2] = 1.0F;
  mPushConstants.tint_color[3] = 1.0F;
  mPushConstants.opacity = mOpacity;

  VkCommandBuffer cmd = mFrameSync->GetCommandBuffer(mCurrentFrame);

  vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                    pipeline->GetPipeline(mBlendMode));

  VkBuffer buf = mVertexRing->GetBuffer();
  vkCmdBindVertexBuffers(cmd, 0, 1, &buf, &offset_bytes);

  vkCmdPushConstants(cmd, pipeline->GetLayout(), VK_SHADER_STAGE_VERTEX_BIT, 0,
                     static_cast<uint32_t>(sizeof(PushConstants)),
                     &mPushConstants);

  vkCmdDraw(cmd, static_cast<uint32_t>(vertices.size()), 1, 0, 0);
}

void VulkanRenderer::SetBlendMode(BlendMode mode) {
  // Vulkan'da blend pipeline durumu: burada yalnizca kaydedilir, cizim aninda
  // dogru pipeline varyanti baglanir.
  mBlendMode = mode;
}

TextureHandle VulkanRenderer::CreateTexture(const uint8_t* data, int32_t width,
                                            int32_t height, int32_t channels) {
  return CreateTexture(data, width, height, channels, TextureFilter::kLinear);
}

TextureHandle VulkanRenderer::CreateTexture(const uint8_t* data, int32_t width,
                                            int32_t height, int32_t channels,
                                            TextureFilter filter) {
  if (mTexturedPipeline == nullptr || data == nullptr || width <= 0 ||
      height <= 0) {
    return kInvalidTexture;
  }

  VkDescriptorSet desc_set =
      mTexturedPipeline->AllocateDescriptorSet(mContext->GetDevice());
  if (desc_set == VK_NULL_HANDLE) {
    return kInvalidTexture;
  }

  auto tex = std::make_unique<VulkanTexture>();
  if (!tex->Upload(mContext.get(), mFrameSync->GetCommandPool(), data, width,
                   height, channels, desc_set,
                   mTexturedPipeline->GetDescriptorSetLayout(), filter)) {
    mTexturedPipeline->FreeDescriptorSet(mContext->GetDevice(), desc_set);
    return kInvalidTexture;
  }

  const TextureHandle kHandle = mNextTextureHandle++;
  mTextures[kHandle] = std::move(tex);
  return kHandle;
}

void VulkanRenderer::UpdateTexture(TextureHandle handle, int32_t x, int32_t y,
                                   int32_t width, int32_t height,
                                   const uint8_t* data) {
  auto it = mTextures.find(handle);
  if (it == mTextures.end() || data == nullptr) {
    return;
  }
  // Not: kare ortasında çağrılabilir. Kendi tek seferlik komut buffer'ını
  // gönderip bekler; hâlâ kaydedilmekte olan frame komut buffer'ı henüz
  // submit edilmediğinden çakışma olmaz. Aynı karede daha önce çizilmiş
  // glyph'lerin bölgeleri asla üzerine yazılmaz (atlas yalnızca kullanılmamış
  // alana ekler), dolayısıyla eski UV'ler geçerli kalır.
  it->second->UpdateRegion(mContext.get(), mFrameSync->GetCommandPool(), x, y,
                           width, height, data);
}

void VulkanRenderer::DestroyTexture(TextureHandle handle) {
  auto it = mTextures.find(handle);
  if (it == mTextures.end()) {
    return;
  }

  // Texture, hâlâ uçuşta olan karelerin komut buffer'larından referans
  // ediliyor olabilir. Eskiden burada `vkDeviceWaitIdle` çağrılıyordu — bu,
  // her texture yıkımında GPU'yu tamamen durduruyordu (bir font kapatılırken
  // glyph sayısı kadar tam stall).
  //
  // Bunun yerine gecikmeli silme: texture, kMaxFramesInFlight kare boyunca
  // bekletilir. O süre dolduğunda onu kullanmış olabilecek tüm submit'ler
  // tamamlanmıştır (in-flight fence bekleme döngüsü bunu garanti eder).
  mPendingTextureDeletes.push_back(
      {std::move(it->second), mFrameCounter + VkFrameSync::kMaxFramesInFlight});
  mTextures.erase(it);
}

void VulkanRenderer::ProcessPendingTextureDeletes(bool force) {
  if (mContext == nullptr || mContext->GetDevice() == VK_NULL_HANDLE) {
    mPendingTextureDeletes.clear();
    return;
  }
  VkDevice device = mContext->GetDevice();

  auto ready = [&](const PendingTextureDelete& p) {
    return force || mFrameCounter >= p.delete_after_frame;
  };

  for (auto& pending : mPendingTextureDeletes) {
    if (!ready(pending) || pending.texture == nullptr) {
      continue;
    }
    if (mTexturedPipeline != nullptr) {
      mTexturedPipeline->FreeDescriptorSet(device,
                                           pending.texture->GetDescriptorSet());
    }
    pending.texture->Destroy(device);
    pending.texture.reset();
  }
  mPendingTextureDeletes.erase(
      std::remove_if(
          mPendingTextureDeletes.begin(), mPendingTextureDeletes.end(),
          [](const PendingTextureDelete& p) { return p.texture == nullptr; }),
      mPendingTextureDeletes.end());
}

void VulkanRenderer::DrawTextured(const std::vector<TexturedVertex>& vertices,
                                  TextureHandle texture) {
  if (!mFrameActive || vertices.empty()) {
    return;
  }
  const VulkanTexturedPipeline* pipeline = ActiveTexturedPipeline();
  if (pipeline == nullptr || mTexturedVertexRing == nullptr) {
    return;
  }

  // Handle normal bir texture'a da, bir hedefin renk image'ina da ait olabilir.
  VkDescriptorSet desc_set = LookupDescriptorSet(texture);
  if (desc_set == VK_NULL_HANDLE) {
    return;
  }

  const auto kByteSize =
      static_cast<VkDeviceSize>(vertices.size() * sizeof(TexturedVertex));
  constexpr VkDeviceSize kAlignment = 4;
  VkDeviceSize offset_bytes = 0;

  if (!mTexturedVertexRing->Write(vertices.data(), kByteSize, kAlignment,
                                  mCurrentFrame, offset_bytes)) {
    return;
  }

  // Renk vertex'te taşındığı için tint her zaman 1.0.
  PushConstants pc = mPushConstants;
  pc.tint_color[0] = 1.0F;
  pc.tint_color[1] = 1.0F;
  pc.tint_color[2] = 1.0F;
  pc.tint_color[3] = 1.0F;
  pc.opacity = mOpacity;

  VkCommandBuffer cmd = mFrameSync->GetCommandBuffer(mCurrentFrame);

  vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                    pipeline->GetPipeline(mBlendMode));

  // Not: descriptor set birincil pipeline'in havuzundan gelmis olabilir. Iki
  // pipeline layout'u AYNI SEKILDE tanimlandigi icin Vulkan onlari set 0 icin
  // uyumlu sayar; baglama gecerlidir.
  vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                          pipeline->GetLayout(), 0, 1, &desc_set, 0, nullptr);

  VkBuffer buf = mTexturedVertexRing->GetBuffer();
  vkCmdBindVertexBuffers(cmd, 0, 1, &buf, &offset_bytes);

  vkCmdPushConstants(cmd, pipeline->GetLayout(), VK_SHADER_STAGE_VERTEX_BIT, 0,
                     static_cast<uint32_t>(sizeof(PushConstants)), &pc);

  vkCmdDraw(cmd, static_cast<uint32_t>(vertices.size()), 1, 0, 0);
}

VkExtent2D VulkanRenderer::CurrentExtent() const {
  const auto it = mRenderTargets.find(mCurrentTarget);
  if (it != mRenderTargets.end()) {
    return it->second.target->GetExtent();
  }
  return mSwapchain != nullptr ? mSwapchain->GetExtent() : VkExtent2D{0, 0};
}

const VulkanPipeline* VulkanRenderer::ActivePipeline() const {
  return mCurrentTarget == kInvalidRenderTarget ? mPipeline.get()
                                                : mOffscreenPipeline.get();
}

const VulkanTexturedPipeline* VulkanRenderer::ActiveTexturedPipeline() const {
  return mCurrentTarget == kInvalidRenderTarget
             ? mTexturedPipeline.get()
             : mOffscreenTexturedPipeline.get();
}

VkDescriptorSet VulkanRenderer::LookupDescriptorSet(
    TextureHandle handle) const {
  const auto tex = mTextures.find(handle);
  if (tex != mTextures.end()) {
    return tex->second->GetDescriptorSet();
  }
  // Hedeflerin renk image'lari da ayni handle uzayindan bir handle alir.
  for (const auto& entry : mRenderTargets) {
    if (entry.second.texture == handle) {
      return entry.second.target->GetDescriptorSet();
    }
  }
  return VK_NULL_HANDLE;
}

bool VulkanRenderer::EnsureOffscreenResources() {
  if (mOffscreenRenderPass != VK_NULL_HANDLE) {
    return true;
  }
  if (mContext == nullptr) {
    return false;
  }
  VkDevice device = mContext->GetDevice();

  VkAttachmentDescription color{};
  color.format = VulkanRenderTarget::kColorFormat;
  color.samples = VK_SAMPLE_COUNT_1_BIT;
  color.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  color.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  color.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  color.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  // Hedef, pass disinda daima orneklenebilir durumda bulunur; boylece ayni
  // komut buffer'inda "hedefe ciz, sonra ekrana bas" mumkun olur.
  color.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  color.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

  VkAttachmentReference color_ref{};
  color_ref.attachment = 0;
  color_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

  VkSubpassDescription subpass{};
  subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
  subpass.colorAttachmentCount = 1;
  subpass.pColorAttachments = &color_ref;

  // Iki bagimlilik: girerken onceki ornekleme bitmis olmali, cikarken renk
  // yazimlari fragment shader okumasindan ONCE gorunur olmali. Ikincisi,
  // hedefin ayni karede ekrana basilabilmesinin sarti.
  std::array<VkSubpassDependency, 2> deps{};
  deps[0].srcSubpass = VK_SUBPASS_EXTERNAL;
  deps[0].dstSubpass = 0;
  deps[0].srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
  deps[0].srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
  deps[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  deps[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
  deps[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

  deps[1].srcSubpass = 0;
  deps[1].dstSubpass = VK_SUBPASS_EXTERNAL;
  deps[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  deps[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
  deps[1].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
  deps[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
  deps[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

  VkRenderPassCreateInfo ci{};
  ci.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
  ci.attachmentCount = 1;
  ci.pAttachments = &color;
  ci.subpassCount = 1;
  ci.pSubpasses = &subpass;
  ci.dependencyCount = static_cast<uint32_t>(deps.size());
  ci.pDependencies = deps.data();
  VK_CHECK(vkCreateRenderPass(device, &ci, nullptr, &mOffscreenRenderPass));

  // Pipeline yalnizca UYUMLU render pass ile kullanilabilir ve uyumluluk
  // attachment formatini kapsar; hedeflerin formati ekranınkinden farkli
  // oldugu icin ikinci bir takim sart.
  auto pipeline = std::make_unique<VulkanPipeline>();
  if (!pipeline->Init(device, mOffscreenRenderPass)) {
    spdlog::error("VulkanRenderer: offscreen pipeline olusturulamadi.");
    return false;
  }
  auto textured = std::make_unique<VulkanTexturedPipeline>();
  if (!textured->Init(device, mOffscreenRenderPass)) {
    spdlog::error(
        "VulkanRenderer: offscreen textured pipeline olusturulamadi.");
    return false;
  }
  mOffscreenPipeline = std::move(pipeline);
  mOffscreenTexturedPipeline = std::move(textured);
  return true;
}

RenderTargetHandle VulkanRenderer::CreateRenderTarget(int32_t width,
                                                      int32_t height,
                                                      TextureFilter filter) {
  if (mContext == nullptr || mTexturedPipeline == nullptr || width <= 0 ||
      height <= 0) {
    return kInvalidRenderTarget;
  }
  if (!EnsureOffscreenResources()) {
    return kInvalidRenderTarget;
  }

  VkDevice device = mContext->GetDevice();

  // Descriptor set DAIMA birincil pipeline'in havuzundan alinir; hedefin
  // texture'i ekrana basilirken de o pipeline ile baglaniyor.
  VkDescriptorSet desc_set = mTexturedPipeline->AllocateDescriptorSet(device);
  if (desc_set == VK_NULL_HANDLE) {
    return kInvalidRenderTarget;
  }

  auto target = std::make_unique<VulkanRenderTarget>();
  if (!target->Create(mContext.get(), mOffscreenRenderPass,
                      mFrameSync->GetCommandPool(), width, height, desc_set,
                      mTexturedPipeline->GetDescriptorSetLayout(), filter)) {
    mTexturedPipeline->FreeDescriptorSet(device, desc_set);
    return kInvalidRenderTarget;
  }

  RenderTargetEntry entry;
  entry.texture = mNextTextureHandle++;
  entry.target = std::move(target);

  const RenderTargetHandle kHandle = mNextRenderTarget++;
  mRenderTargets.emplace(kHandle, std::move(entry));
  return kHandle;
}

void VulkanRenderer::DestroyRenderTarget(RenderTargetHandle handle) {
  auto it = mRenderTargets.find(handle);
  if (it == mRenderTargets.end() || mContext == nullptr) {
    return;
  }
  if (mCurrentTarget == handle) {
    SetRenderTarget(kInvalidRenderTarget);
  }
  // Texture'lardaki gecikmeli silme burada uygulanamaz: hedefin framebuffer'i
  // da yikiliyor ve o, ucustaki komut buffer'larindan referans ediliyor.
  // Hedef yaratma/yikma kare dongusunde degil, kurulum sirasinda yapilir;
  // burada tam bekleme kabul edilebilir bir bedel.
  vkDeviceWaitIdle(mContext->GetDevice());
  mTexturedPipeline->FreeDescriptorSet(mContext->GetDevice(),
                                       it->second.target->GetDescriptorSet());
  it->second.target->Destroy(mContext->GetDevice());
  mRenderTargets.erase(it);
}

TextureHandle VulkanRenderer::GetRenderTargetTexture(
    RenderTargetHandle handle) const {
  const auto it = mRenderTargets.find(handle);
  return it == mRenderTargets.end() ? kInvalidTexture : it->second.texture;
}

bool VulkanRenderer::SetRenderTarget(RenderTargetHandle handle) {
  if (handle != kInvalidRenderTarget &&
      mRenderTargets.find(handle) == mRenderTargets.end()) {
    return false;
  }
  if (handle == mCurrentTarget) {
    return true;
  }
  // Kare aktif degilse yalnizca secimi kaydet; render pass zaten yok.
  if (!mFrameActive) {
    mCurrentTarget = handle;
    return true;
  }
  BeginTargetRenderPass(handle);
  return true;
}

void VulkanRenderer::BeginTargetRenderPass(RenderTargetHandle handle) {
  VkCommandBuffer cmd = mFrameSync->GetCommandBuffer(mCurrentFrame);

  // Bir komut buffer'inda birden fazla render pass ORNEGI olabilir, ama ic
  // ice olamaz: once yururlukteki bitirilir.
  vkCmdEndRenderPass(cmd);
  mCurrentTarget = handle;

  VkRenderPassBeginInfo rp{};
  rp.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
  rp.renderArea.offset = {0, 0};

  const auto it = mRenderTargets.find(handle);
  if (it != mRenderTargets.end()) {
    rp.renderPass = mOffscreenRenderPass;
    rp.framebuffer = it->second.target->GetFramebuffer();
    rp.renderArea.extent = it->second.target->GetExtent();
    rp.clearValueCount = 1;
    rp.pClearValues = &mClearValue;
  } else {
    // Ekrana donus: CLEAR yerine LOAD yapan ikiz pass kullanilir, yoksa bu
    // karede o ana kadar cizilen her sey silinirdi (bkz. GetResumeRenderPass).
    rp.renderPass = mSwapchain->GetResumeRenderPass();
    rp.framebuffer = mSwapchain->GetFramebuffer(mCurrentImageIndex);
    rp.renderArea.extent = mSwapchain->GetExtent();
    rp.clearValueCount = 0;
    rp.pClearValues = nullptr;
  }

  vkCmdBeginRenderPass(cmd, &rp, VK_SUBPASS_CONTENTS_INLINE);
  // Dinamik state render pass ornegine bagli degildir ama viewport/scissor
  // artik farkli bir yuzeye gore hesaplanmali.
  ApplyDynamicViewportScissor(cmd);
}

bool VulkanRenderer::ReadRenderTarget(RenderTargetHandle handle,
                                      uint8_t* out_rgba,
                                      std::size_t byte_capacity) {
  const auto it = mRenderTargets.find(handle);
  if (it == mRenderTargets.end() || mContext == nullptr) {
    return false;
  }
  return it->second.target->ReadPixels(
      mContext.get(), mFrameSync->GetCommandPool(), out_rgba, byte_capacity);
}

void VulkanRenderer::SetProjectionMatrix(const float* mat4) {
  std::memcpy(mPushConstants.projection, mat4, 16 * sizeof(float));
}

void VulkanRenderer::SetModelMatrix(const float* mat3) {
  // 3x3 column-major (glm::mat3) affine → 4x4 column-major dönüşümü.
  // mat3 layout (column-major): [m00 m10 0 | m01 m11 0 | tx ty 1]
  // İndeksler:                    [0]  [1] [2] [3]  [4] [5] [6][7][8]
  std::array<float, 16> m = {
      mat3[0], mat3[1], 0.0F, 0.0F,  // column 0 (m00, m10)
      mat3[3], mat3[4], 0.0F, 0.0F,  // column 1 (m01, m11)
      0.0F,    0.0F,    1.0F, 0.0F,  // column 2
      mat3[6], mat3[7], 0.0F, 1.0F,  // column 3 (tx, ty)
  };
  std::memcpy(mPushConstants.model, m.data(), 16 * sizeof(float));
}

}  // namespace sdl_painter
