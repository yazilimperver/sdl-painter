#pragma once

#include "sdl_painter/renderer.h"

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include <vulkan/vulkan.h>

#include "vk_context.h"
#include "vk_frame_sync.h"
#include "vk_swapchain.h"
#include "vulkan_buffer.h"
#include "vulkan_pipeline.h"
#include "vulkan_texture.h"
#include "vulkan_textured_pipeline.h"

namespace sdl_painter {

/// @brief Vulkan 1.1 IRenderer implementasyonu — Phase 5c.
///
/// Phase 5a: Initialize/Shutdown, BeginFrame/EndFrame, Clear(color).
/// Phase 5b: DrawTriangles, SetProjectionMatrix, SetModelMatrix (untextured).
/// Phase 5c: CreateTexture, DestroyTexture, DrawTextured (textured pipeline).
class VulkanRenderer final : public IRenderer {
 public:
  VulkanRenderer() = default;
  ~VulkanRenderer() override;

  VulkanRenderer(const VulkanRenderer&) = delete;
  VulkanRenderer& operator=(const VulkanRenderer&) = delete;

  // --- IRenderer arayüzü ---

  bool Initialize(SDL_Window* window) override;
  void Shutdown() override;
  void BeginFrame() override;
  void EndFrame() override;

  void SetViewport(int32_t x, int32_t y, int32_t width,
                   int32_t height) override;
  void SetScissor(int32_t x, int32_t y, int32_t width, int32_t height) override;
  void ClearScissor() override;
  void Clear(const Color& color) override;
  void SetOpacity(float alpha) override;

  void DrawTriangles(const std::vector<Vertex>& vertices) override;

  TextureHandle CreateTexture(const uint8_t* data, int32_t width,
                              int32_t height, int32_t channels) override;
  void DestroyTexture(TextureHandle handle) override;
  void DrawTextured(const std::vector<TexturedVertex>& vertices,
                    TextureHandle texture) override;

  RendererBackend GetBackend() const override {
    return RendererBackend::kVulkan;
  }
  void SetProjectionMatrix(const float* mat4) override;
  void SetModelMatrix(const float* mat3) override;

 private:
  /// @brief Pencerenin anlık drawable boyutunu döndürür.
  void QueryWindowDrawableSize(uint32_t& width, uint32_t& height) const;

  /// @brief Frame başında swapchain image alımı; out-of-date ise recreate.
  bool AcquireNextImage();

  /// @brief Frame sonunda komutları submit edip present yap.
  void SubmitAndPresent();

  SDL_Window* mWindow{nullptr};

  std::unique_ptr<VkContext> mContext;
  std::unique_ptr<VkSwapchain> mSwapchain;
  std::unique_ptr<VkFrameSync> mFrameSync;

  uint32_t mCurrentFrame{0};          // 0..kMaxFramesInFlight-1
  uint32_t mCurrentImageIndex{0};     // vkAcquireNextImageKHR'dan dönen index
  uint32_t mAcquireSlot{UINT32_MAX};  // İlk frame'de 0'a wrap-around yapar
  bool mFrameActive{false};
  bool mSwapchainOutOfDate{false};

  // Aktif frame clear değeri — Clear() çağrısından EndFrame'e taşınır.
  VkClearValue mClearValue{};

  // Viewport/scissor (Phase 5b'de pipeline dinamik state olarak kullanacak)
  int32_t mViewportX{0};
  int32_t mViewportY{0};
  int32_t mViewportW{0};
  int32_t mViewportH{0};
  bool mScissorEnabled{false};
  int32_t mScissorX{0};
  int32_t mScissorY{0};
  int32_t mScissorW{0};
  int32_t mScissorH{0};

  float mOpacity{1.0f};

  // Phase 5b: untextured pipeline + vertex ring buffer
  std::unique_ptr<VulkanPipeline> mPipeline;
  std::unique_ptr<VulkanBuffer> mVertexRing;
  PushConstants mPushConstants{};

  // Phase 5c: textured pipeline + texture registry
  std::unique_ptr<VulkanTexturedPipeline> mTexturedPipeline;
  std::unique_ptr<VulkanBuffer> mTexturedVertexRing;
  std::unordered_map<TextureHandle, std::unique_ptr<VulkanTexture>> mTextures;
  TextureHandle mNextTextureHandle{1};  // 0 = kInvalidTexture

  /// @brief Ring buffer'daki mevcut frame için viewport + scissor dinamik state'ini ayarla.
  void ApplyDynamicViewportScissor(VkCommandBuffer cmd) const;
};

}  // namespace sdl_painter
