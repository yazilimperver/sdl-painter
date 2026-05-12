#include "sdl_painter/renderer.h"

#include "opengl/opengl_renderer.h"

#ifdef SDLPAINTER_HAS_VULKAN
#include "vulkan/vulkan_renderer.h"
#endif

namespace sdl_painter {

std::unique_ptr<IRenderer> CreateRenderer(RendererBackend backend) {
  switch (backend) {
    case RendererBackend::kOpenGL:
      return std::make_unique<OpenGLRenderer>();

#ifdef SDLPAINTER_HAS_VULKAN
    case RendererBackend::kVulkan:
      return std::make_unique<VulkanRenderer>();
#endif

    default:
      return nullptr;
  }
}

}  // namespace sdl_painter
