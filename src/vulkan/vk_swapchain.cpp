#include "vk_swapchain.h"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <array>
#include <vector>

#include "vk_check.h"
#include "vk_context.h"

namespace sdl_painter {

namespace {

VkSurfaceFormatKHR PickSurfaceFormat(VkPhysicalDevice device,
                                     VkSurfaceKHR surface) {
  uint32_t count = 0;
  vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &count, nullptr);
  std::vector<VkSurfaceFormatKHR> formats(count);
  vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &count, formats.data());

  for (const auto& f : formats) {
    if (f.format == VK_FORMAT_B8G8R8A8_UNORM &&
        f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
      return f;
    }
  }
  return formats.empty() ? VkSurfaceFormatKHR{VK_FORMAT_B8G8R8A8_UNORM,
                                              VK_COLOR_SPACE_SRGB_NONLINEAR_KHR}
                         : formats[0];
}

VkPresentModeKHR PickPresentMode(VkPhysicalDevice device,
                                 VkSurfaceKHR surface) {
  uint32_t count = 0;
  vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &count, nullptr);
  std::vector<VkPresentModeKHR> modes(count);
  vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &count,
                                            modes.data());
  for (auto m : modes) {
    if (m == VK_PRESENT_MODE_MAILBOX_KHR) return m;
  }
  return VK_PRESENT_MODE_FIFO_KHR;  // her zaman desteklenir
}

VkExtent2D PickExtent(const VkSurfaceCapabilitiesKHR& caps, uint32_t width,
                      uint32_t height) {
  if (caps.currentExtent.width != UINT32_MAX) return caps.currentExtent;
  VkExtent2D actual{width, height};
  actual.width = std::clamp(actual.width, caps.minImageExtent.width,
                            caps.maxImageExtent.width);
  actual.height = std::clamp(actual.height, caps.minImageExtent.height,
                             caps.maxImageExtent.height);
  return actual;
}

}  // namespace

VkSwapchain::~VkSwapchain() { Shutdown(); }

bool VkSwapchain::Initialize(VkContext* context, uint32_t width,
                             uint32_t height) {
  mContext = context;
  if (!CreateSwapchain(width, height)) return false;
  if (!CreateImageViews()) return false;
  if (!CreateRenderPass()) return false;
  if (!CreateFramebuffers()) return false;
  spdlog::info("Swapchain initialized ({}x{}, {} images).", mExtent.width,
               mExtent.height, mImages.size());
  return true;
}

void VkSwapchain::Shutdown() {
  if (!mContext) return;
  VkDevice device = mContext->GetDevice();
  if (device != VK_NULL_HANDLE) {
    DestroySwapchainResources();
    if (mRenderPass != VK_NULL_HANDLE) {
      vkDestroyRenderPass(device, mRenderPass, nullptr);
      mRenderPass = VK_NULL_HANDLE;
    }
  }
  mContext = nullptr;
}

bool VkSwapchain::Recreate(uint32_t width, uint32_t height) {
  VkDevice device = mContext->GetDevice();
  vkDeviceWaitIdle(device);

  // Mevcut swapchain'i oldSwapchain olarak sakla; yeni swapchain buna bakarak
  // hâlâ sunulan image'ları devralabilir (daha hızlı geçiş).
  VkSwapchainKHR old = mSwapchain;
  mSwapchain = VK_NULL_HANDLE;

  // Framebuffer + image view'ları yok et (swapchain henüz canlı).
  for (auto fb : mFramebuffers) {
    if (fb != VK_NULL_HANDLE) vkDestroyFramebuffer(device, fb, nullptr);
  }
  mFramebuffers.clear();
  for (auto view : mImageViews) {
    if (view != VK_NULL_HANDLE) vkDestroyImageView(device, view, nullptr);
  }
  mImageViews.clear();
  mImages.clear();

  // Yeni swapchain'i eski referansıyla oluştur.
  if (!CreateSwapchainWithOld(width, height, old)) {
    if (old != VK_NULL_HANDLE) vkDestroySwapchainKHR(device, old, nullptr);
    return false;
  }
  // Eski swapchain artık kullanılmıyor — yok et.
  if (old != VK_NULL_HANDLE) vkDestroySwapchainKHR(device, old, nullptr);

  if (!CreateImageViews()) return false;
  if (!CreateFramebuffers()) return false;

  spdlog::info("Swapchain recreated ({}x{}).", mExtent.width, mExtent.height);
  return true;
}

void VkSwapchain::DestroySwapchainResources() {
  VkDevice device = mContext->GetDevice();
  for (auto fb : mFramebuffers) {
    if (fb != VK_NULL_HANDLE) vkDestroyFramebuffer(device, fb, nullptr);
  }
  mFramebuffers.clear();
  for (auto view : mImageViews) {
    if (view != VK_NULL_HANDLE) vkDestroyImageView(device, view, nullptr);
  }
  mImageViews.clear();
  mImages.clear();
  if (mSwapchain != VK_NULL_HANDLE) {
    vkDestroySwapchainKHR(device, mSwapchain, nullptr);
    mSwapchain = VK_NULL_HANDLE;
  }
}

bool VkSwapchain::CreateSwapchain(uint32_t width, uint32_t height) {
  return CreateSwapchainWithOld(width, height, VK_NULL_HANDLE);
}

bool VkSwapchain::CreateSwapchainWithOld(uint32_t width, uint32_t height,
                                          VkSwapchainKHR old_swapchain) {
  VkPhysicalDevice phys = mContext->GetPhysicalDevice();
  VkSurfaceKHR surface = mContext->GetSurface();
  VkDevice device = mContext->GetDevice();

  VkSurfaceCapabilitiesKHR caps{};
  vkGetPhysicalDeviceSurfaceCapabilitiesKHR(phys, surface, &caps);

  VkSurfaceFormatKHR format = PickSurfaceFormat(phys, surface);
  VkPresentModeKHR present_mode = PickPresentMode(phys, surface);
  VkExtent2D extent = PickExtent(caps, width, height);

  uint32_t image_count = caps.minImageCount + 1;
  if (caps.maxImageCount > 0 && image_count > caps.maxImageCount) {
    image_count = caps.maxImageCount;
  }

  VkSwapchainCreateInfoKHR ci{};
  ci.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
  ci.surface = surface;
  ci.minImageCount = image_count;
  ci.imageFormat = format.format;
  ci.imageColorSpace = format.colorSpace;
  ci.imageExtent = extent;
  ci.imageArrayLayers = 1;
  ci.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

  uint32_t queues[2] = {mContext->GetGraphicsQueueFamily(),
                        mContext->GetPresentQueueFamily()};
  if (queues[0] != queues[1]) {
    ci.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
    ci.queueFamilyIndexCount = 2;
    ci.pQueueFamilyIndices = queues;
  } else {
    ci.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
  }

  ci.preTransform = caps.currentTransform;
  ci.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
  ci.presentMode = present_mode;
  ci.clipped = VK_TRUE;
  ci.oldSwapchain = old_swapchain;

  VkResult res = vkCreateSwapchainKHR(device, &ci, nullptr, &mSwapchain);
  if (res != VK_SUCCESS) {
    spdlog::error("vkCreateSwapchainKHR failed: {}",
                  vk_detail::VkResultToString(res));
    return false;
  }

  uint32_t actual = 0;
  vkGetSwapchainImagesKHR(device, mSwapchain, &actual, nullptr);
  mImages.resize(actual);
  vkGetSwapchainImagesKHR(device, mSwapchain, &actual, mImages.data());

  mImageFormat = format.format;
  mExtent = extent;
  return true;
}

bool VkSwapchain::CreateImageViews() {
  VkDevice device = mContext->GetDevice();
  mImageViews.resize(mImages.size());
  for (size_t i = 0; i < mImages.size(); ++i) {
    VkImageViewCreateInfo ci{};
    ci.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    ci.image = mImages[i];
    ci.viewType = VK_IMAGE_VIEW_TYPE_2D;
    ci.format = mImageFormat;
    ci.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
    ci.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    ci.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    ci.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
    ci.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    ci.subresourceRange.baseMipLevel = 0;
    ci.subresourceRange.levelCount = 1;
    ci.subresourceRange.baseArrayLayer = 0;
    ci.subresourceRange.layerCount = 1;
    VkResult res = vkCreateImageView(device, &ci, nullptr, &mImageViews[i]);
    if (res != VK_SUCCESS) {
      spdlog::error("vkCreateImageView[{}] failed: {}", i,
                    vk_detail::VkResultToString(res));
      return false;
    }
  }
  return true;
}

bool VkSwapchain::CreateRenderPass() {
  VkDevice device = mContext->GetDevice();

  VkAttachmentDescription color{};
  color.format = mImageFormat;
  color.samples = VK_SAMPLE_COUNT_1_BIT;
  color.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  color.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  color.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  color.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  color.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  color.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

  VkAttachmentReference color_ref{};
  color_ref.attachment = 0;
  color_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

  VkSubpassDescription subpass{};
  subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
  subpass.colorAttachmentCount = 1;
  subpass.pColorAttachments = &color_ref;

  VkSubpassDependency dep{};
  dep.srcSubpass = VK_SUBPASS_EXTERNAL;
  dep.dstSubpass = 0;
  dep.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  dep.srcAccessMask = 0;
  dep.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  dep.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

  VkRenderPassCreateInfo ci{};
  ci.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
  ci.attachmentCount = 1;
  ci.pAttachments = &color;
  ci.subpassCount = 1;
  ci.pSubpasses = &subpass;
  ci.dependencyCount = 1;
  ci.pDependencies = &dep;

  VkResult res = vkCreateRenderPass(device, &ci, nullptr, &mRenderPass);
  if (res != VK_SUCCESS) {
    spdlog::error("vkCreateRenderPass failed: {}",
                  vk_detail::VkResultToString(res));
    return false;
  }
  return true;
}

bool VkSwapchain::CreateFramebuffers() {
  VkDevice device = mContext->GetDevice();
  mFramebuffers.resize(mImageViews.size());
  for (size_t i = 0; i < mImageViews.size(); ++i) {
    std::array<VkImageView, 1> attachments{mImageViews[i]};
    VkFramebufferCreateInfo ci{};
    ci.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    ci.renderPass = mRenderPass;
    ci.attachmentCount = static_cast<uint32_t>(attachments.size());
    ci.pAttachments = attachments.data();
    ci.width = mExtent.width;
    ci.height = mExtent.height;
    ci.layers = 1;
    VkResult res = vkCreateFramebuffer(device, &ci, nullptr, &mFramebuffers[i]);
    if (res != VK_SUCCESS) {
      spdlog::error("vkCreateFramebuffer[{}] failed: {}", i,
                    vk_detail::VkResultToString(res));
      return false;
    }
  }
  return true;
}

}  // namespace sdl_painter
