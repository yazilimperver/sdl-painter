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

/// @brief Vulkan 1.1 IRenderer implementasyonu
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
  void UpdateTexture(TextureHandle handle, int32_t x, int32_t y, int32_t width,
                     int32_t height, const uint8_t* data) override;
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

  /// @brief Swapchain'i yeniden inşa et; yüzey 0x0 ise işlemi ertele.
  ///
  /// Simge durumuna küçültülmüş pencerede swapchain oluşturulamaz. Böyle bir
  /// durumda yalnızca @ref mSwapchainNeedsRecreate işaretlenir ve pencere geri
  /// geldiğinde @ref BeginFrame yeniden dener.
  void RecreateSwapchainOrDefer();

  /// @brief Frame sonunda komutları submit edip present yap.
  void SubmitAndPresent();

  SDL_Window* mWindow{nullptr};

  std::unique_ptr<VkContext> mContext;
  std::unique_ptr<VkSwapchain> mSwapchain;
  std::unique_ptr<VkFrameSync> mFrameSync;

  uint32_t mCurrentFrame{0};       // 0..kMaxFramesInFlight-1
  uint32_t mCurrentImageIndex{0};  // vkAcquireNextImageKHR'dan dönen index
  bool mFrameActive{false};
  bool mSwapchainOutOfDate{false};
  /// @brief Swapchain yeniden inşa edilmeyi bekliyor (pencere 0x0 idi).
  bool mSwapchainNeedsRecreate{false};

  // Aktif frame clear değeri — Clear() çağrısından EndFrame'e taşınır.
  VkClearValue mClearValue{};

  // Viewport/scissor
  int32_t mViewportX{0};
  int32_t mViewportY{0};
  int32_t mViewportW{0};
  int32_t mViewportH{0};
  bool mScissorEnabled{false};
  int32_t mScissorX{0};
  int32_t mScissorY{0};
  int32_t mScissorW{0};
  int32_t mScissorH{0};

  float mOpacity{1.0F};

  // Phase 5b: untextured pipeline + vertex ring buffer
  std::unique_ptr<VulkanPipeline> mPipeline;
  std::unique_ptr<VulkanBuffer> mVertexRing;
  PushConstants mPushConstants{};

  // Phase 5c: textured pipeline + texture registry
  std::unique_ptr<VulkanTexturedPipeline> mTexturedPipeline;
  std::unique_ptr<VulkanBuffer> mTexturedVertexRing;
  std::unordered_map<TextureHandle, std::unique_ptr<VulkanTexture>> mTextures;
  TextureHandle mNextTextureHandle{1};  // 0 = kInvalidTexture

  /// @brief Silinmeyi bekleyen texture — uçuştaki kareler bitince yıkılır.
  struct PendingTextureDelete {
    std::unique_ptr<VulkanTexture> texture;
    uint64_t delete_after_frame{0};
  };

  /// @brief Gecikmeli silme kuyruğu.
  ///
  /// `DestroyTexture` her çağrısında `vkDeviceWaitIdle` yapmak yerine texture
  /// burada bekletilir; @ref EndFrame sayacı ilerlettikçe süresi dolanlar
  /// serbest bırakılır. Bir font kapatılırken glyph atlası sayfaları bu
  /// yoldan gider ve GPU durdurulmaz.
  std::vector<PendingTextureDelete> mPendingTextureDeletes;

  /// @brief Monoton artan kare sayacı (gecikmeli silme zamanlaması için).
  uint64_t mFrameCounter{0};

  /// @brief Süresi dolan gecikmeli silmeleri işle.
  /// @param force `true` ise süre gözetmeksizin hepsi yıkılır (Shutdown).
  void ProcessPendingTextureDeletes(bool force);

  /// @brief Mevcut frame için viewport + scissor dinamik state'ini ayarla.
  void ApplyDynamicViewportScissor(VkCommandBuffer cmd) const;

  /// @brief Yalnızca viewport dinamik state'ini komut buffer'ına yaz.
  void ApplyDynamicViewport(VkCommandBuffer cmd) const;

  /// @brief Yalnızca scissor dinamik state'ini komut buffer'ına yaz.
  ///
  /// Scissor swapchain sınırlarına kelepçelenir; `mScissorEnabled` false ise
  /// tüm swapchain alanı kullanılır.
  void ApplyDynamicScissor(VkCommandBuffer cmd) const;
};

}  // namespace sdl_painter
