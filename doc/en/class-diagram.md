[Türkçe sürüm](../sinif-diyagrami.md) | **English**

# SDLPainter — Class Diagrams

This document gives SDLPainter's **class structure** in Mermaid `classDiagram`
notation. The diagrams are split into parts because a single graph quickly
becomes unreadable; each part focuses on a different concept.

---

## 1. Core Classes — Painter and Its Context

The three basic compositions around `Painter`: the style types (`Pen`/`Brush`),
the transform stack (`RenderState` + `Transform`), and the render pipeline
(`Tessellator` + `RenderBatcher` + `IRenderer`).

```mermaid
classDiagram
    direction TB

    class Painter {
        -SDL_Window& mWindow
        -unique_ptr~IRenderer~ mRenderer
        -unique_ptr~RenderBatcher~ mBatcher
        -vector~RenderState~ mStateStack
        -RenderState mCurrentState
        -shared_ptr~Font~ mCurrentFont
        -int32_t mViewportWidth
        -int32_t mViewportHeight
        +Painter(SDL_Window*, RendererBackend)
        +Begin() void
        +End() void
        +Clear(Color) void
        +SetPen(Pen) void
        +SetBrush(Brush) void
        +SetOpacity(float) void
        +DrawLine/Rect/Circle/Ellipse/Polygon ...
        +FillRect/Circle/Ellipse/Polygon ...
        +DrawImage(Image, ...) void
        +DrawText(...) void
        +Save() void
        +Restore() void
        +Translate/Rotate/Scale/ResetTransform ...
        +SetClipRect(Rect) void
        +ClearClip() void
        -UpdateProjection() void
        -FlushTransform() void
        -ApplyScissor(Rect) void
    }

    class RenderState {
        +Transform transform
        +Pen pen
        +Brush brush
        +float opacity
        +Rect clip_rect
        +bool has_clip
    }

    class Transform {
        -float mData[3][3]
        +SetIdentity() void
        +IsIdentity() bool
        +Translate(dx, dy) Transform&
        +Rotate(degrees) Transform&
        +Scale(sx, sy) Transform&
        +Map(x, y, out_x, out_y) void
        +operator*(Transform) Transform
        +Data() const float*
        +MakeTranslate/Rotate/Scale Transform$
    }

    class Pen {
        -Color mColor
        -float mWidth
        +GetColor() Color
        +GetWidth() float
        +IsVisible() bool
        +NoPen() Pen$
    }

    class Brush {
        -Color mColor
        +GetColor() Color
        +IsVisible() bool
        +NoBrush() Brush$
    }

    class Color {
        +uint8_t r, g, b, a
        +RedF/GreenF/BlueF/AlphaF() float
        +Black/White/Red/Green/Blue/Transparent() Color$
    }

    Painter "1" *-- "1" RenderState : current
    Painter "1" *-- "0..*" RenderState : stack
    RenderState *-- Transform
    RenderState *-- Pen
    RenderState *-- Brush
    Pen --> Color
    Brush --> Color
```

**Relationship notes**

- `Painter *-- RenderState`: Composition. `mCurrentState` is the active state,
  `mStateStack` is the stack that grows and shrinks with `Save()`/`Restore()`.
- `static` factory methods (`NoPen`, `Black`, `MakeTranslate`) are marked with `$`.

---

## 2. Render Pipeline — Tessellator, RenderBatcher, IRenderer

```mermaid
classDiagram
    direction LR

    class Painter {
        <<facade>>
    }

    class Tessellator {
        <<utility (static)>>
        +TessellateFilledRect(...) vector~Vertex~$
        +TessellateFilledCircle(...) vector~Vertex~$
        +TessellateFilledEllipse(...) vector~Vertex~$
        +TessellateFilledPolygon(...) vector~Vertex~$
        +TessellateStrokedRect/Circle/Ellipse(...) vector~Vertex~$
        +TessellateThickLine(...) vector~Vertex~$
        +TessellateThickPolyline(...) vector~Vertex~$
        +TessellateStrokedPolygon(...) vector~Vertex~$
        +TessellateTexturedRect(...) vector~TexturedVertex~$
        -AdaptiveSegments(radius) int32_t$
        -EarClipping(points) vector~Vertex~$
        -IsClockwise/PointInTriangle bool$
    }

    class RenderBatcher {
        -IRenderer& mRenderer
        -DrawMode mCurrentMode
        -TextureHandle mCurrentTexture
        -float mCurrentOpacity
        -vector~Vertex~ mVertexBuffer
        -vector~TexturedVertex~ mTexturedBuffer
        -kMaxVertices: 8192$
        +PushTriangles(verts, color, opacity) void
        +PushTexturedTriangles(verts, tex, tint, opacity) void
        +Flush() void
    }

    class DrawMode {
        <<enumeration>>
        kNone
        kBasic
        kTextured
    }

    class IRenderer {
        <<interface>>
        +Initialize(SDL_Window*) bool
        +Shutdown() void
        +BeginFrame() void
        +EndFrame() void
        +SetViewport(x, y, w, h) void
        +SetScissor(x, y, w, h) void
        +ClearScissor() void
        +Clear(Color) void
        +SetOpacity(float) void
        +DrawTriangles(verts) void
        +CreateTexture(data, w, h, ch) TextureHandle
        +DestroyTexture(handle) void
        +DrawTextured(verts, tex) void
        +GetBackend() RendererBackend
        +SetProjectionMatrix(mat4) void
        +SetModelMatrix(mat3) void
    }

    class RendererBackend {
        <<enumeration>>
        kOpenGL
        kVulkan
    }

    class CreateRenderer {
        <<factory>>
        CreateRenderer(backend) unique_ptr~IRenderer~$
    }

    class OpenGLRenderer
    class VulkanRenderer

    Painter ..> Tessellator : uses
    Painter *-- RenderBatcher
    RenderBatcher --> IRenderer : reference
    RenderBatcher *-- DrawMode
    IRenderer ..> RendererBackend
    IRenderer <|.. OpenGLRenderer
    IRenderer <|.. VulkanRenderer
    CreateRenderer ..> IRenderer : returns
    CreateRenderer ..> OpenGLRenderer : new
    CreateRenderer ..> VulkanRenderer : new
```

**Key point:** `Painter` does not call `IRenderer` **directly**; all drawing goes
through `RenderBatcher`. Only non-batch operations such as `Begin()`, `End()`,
`Clear()`, `SetViewport()`, `SetScissor()` go straight to `IRenderer`.

---

## 3. Image and Font Types

```mermaid
classDiagram
    direction TB

    class Image {
        -unique_ptr~uint8_t, StbDeleter~ mRawData
        -int32_t mWidth, mHeight, mChannels
        -mutable Texture mHandle
        +Image(file_path)
        +CreateFromData(data, w, h, ch) Image$
        +IsValid() bool
        +Width/Height/Channels() int32_t
        +RawData() const uint8_t*
        +Upload(IRenderer&) TextureHandle
        +GetHandle() TextureHandle
    }

    class Texture {
        <<RAII>>
        -IRenderer* mRenderer
        -TextureHandle mHandle
        +Texture(IRenderer*)
        +Texture(IRenderer*, handle)
        +~Texture() : DestroyTexture
        +Reset() void
        +Handle() TextureHandle
        +Owner() IRenderer*
        +IsValid() bool
    }

    class Font {
        -void* mHandle
        -int32_t mPointSize
        -mutable unordered_map~char32_t, Glyph~ mGlyphCache
        +Font(file_path, point_size)
        +IsValid() bool
        +PointSize() int32_t
        +Ascent() int32_t
        +MeasureText(text, w, h) bool
        +GetGlyph(IRenderer&, codepoint) const Glyph*
    }

    class Glyph {
        +Texture texture
        +int32_t width, height
        +int32_t advance
        +int32_t bearing_x, bearing_y
    }

    class Alignment {
        <<enumeration>>
        kLeft
        kCenter
        kRight
    }

    class IRenderer {
        <<interface>>
    }

    Image *-- Texture : cache
    Texture --> IRenderer : owner
    Font *-- Glyph : cache
    Glyph *-- Texture
    Font ..> IRenderer : GetGlyph(renderer, ...)
    Image ..> IRenderer : Upload(renderer)
```

**Notes**

- `Texture` is a pure RAII wrapper; non-copyable, movable. Its destructor calls
  `DestroyTexture` through the renderer that owns it.
- `Image::Upload` is idempotent: the second call returns the existing handle
  (lazy upload + cache).
- On a glyph cache miss, `Font::GetGlyph` renders the glyph with SDL_ttf and
  uploads it into a texture; later calls return it from the cache.

---

## 4. OpenGL Backend Classes

```mermaid
classDiagram
    direction TB

    class IRenderer {
        <<interface>>
    }

    class OpenGLRenderer {
        -SDL_Window* mWindow
        -void* mGLContext
        -ShaderProgram mBasicShader
        -ShaderProgram mTexturedShader
        -uint32_t mVao, mVbo
        -uint32_t mTexturedVao, mTexturedVbo
        -float mProjection[16]
        -float mModel[9]
        -float mOpacity
        +Initialize(window) bool
        +Shutdown() void
        +BeginFrame/EndFrame() void
        +SetViewport/Scissor/ClearScissor ...
        +Clear(Color) void
        +DrawTriangles(verts) void
        +CreateTexture/DestroyTexture/DrawTextured ...
        +SetProjectionMatrix/SetModelMatrix ...
        -SetupBuffers() void
    }

    class ShaderProgram {
        +Compile(vert_src, frag_src) bool
        +Use() void
        +SetUniformMat3/Mat4/Vec4/Float/Int ...
        +Handle() uint32_t
    }

    IRenderer <|.. OpenGLRenderer
    OpenGLRenderer *-- ShaderProgram : basic
    OpenGLRenderer *-- ShaderProgram : textured
```

The OpenGL backend uses only three external pieces: `ShaderProgram` (GLSL
program wrapper), GLAD (function loader, not a class), and the VAO/VBO handles.

---

## 5. Vulkan Backend Classes

The Vulkan backend is far more layered than OpenGL because pipelines,
descriptors, the swapchain and sync objects all have to be managed separately.

```mermaid
classDiagram
    direction TB

    class IRenderer {
        <<interface>>
    }

    class VulkanRenderer {
        -SDL_Window* mWindow
        -unique_ptr~VkContext~ mContext
        -unique_ptr~VkSwapchain~ mSwapchain
        -unique_ptr~VkFrameSync~ mFrameSync
        -unique_ptr~VulkanPipeline~ mPipeline
        -unique_ptr~VulkanTexturedPipeline~ mTexturedPipeline
        -unique_ptr~VulkanBuffer~ mVertexRing
        -unique_ptr~VulkanBuffer~ mTexturedVertexRing
        -unordered_map~Handle, VulkanTexture~ mTextures
        -uint32_t mCurrentFrame, mCurrentImageIndex
        -bool mFrameActive, mSwapchainOutOfDate
        -PushConstants mPushConstants
        +IRenderer interface
        -AcquireNextImage() bool
        -SubmitAndPresent() void
        -ApplyDynamicViewportScissor(cmd) void
    }

    class VkContext {
        +VkInstance instance
        +VkPhysicalDevice physicalDevice
        +VkDevice device
        +VkQueue graphicsQueue
        +VkCommandPool commandPool
        +VkSurfaceKHR surface
    }

    class VkSwapchain {
        +VkSwapchainKHR swapchain
        +vector~VkImage~ images
        +vector~VkImageView~ views
        +VkRenderPass renderPass
        +vector~VkFramebuffer~ framebuffers
        +Recreate(width, height) bool
    }

    class VkFrameSync {
        +vector~VkSemaphore~ imageAvailable
        +vector~VkSemaphore~ renderFinished
        +vector~VkFence~ inFlight
        +vector~VkCommandBuffer~ commandBuffers
        +kMaxFramesInFlight: 2$
    }

    class VulkanPipeline {
        +VkPipeline pipeline
        +VkPipelineLayout layout
        +VkShaderModule vertShader
        +VkShaderModule fragShader
    }

    class VulkanTexturedPipeline {
        +VkPipeline pipeline
        +VkPipelineLayout layout
        +VkDescriptorSetLayout dsLayout
        +VkDescriptorPool dsPool
    }

    class VulkanBuffer {
        +VkBuffer buffer
        +VkDeviceMemory memory
        +void* mapped
        +size_t size
    }

    class VulkanTexture {
        +VkImage image
        +VkImageView view
        +VkSampler sampler
        +VkDeviceMemory memory
        +VkDescriptorSet descriptorSet
    }

    class PushConstants {
        +float projection[16]
        +float model[9]
        +float opacity
    }

    IRenderer <|.. VulkanRenderer
    VulkanRenderer *-- VkContext
    VulkanRenderer *-- VkSwapchain
    VulkanRenderer *-- VkFrameSync
    VulkanRenderer *-- VulkanPipeline
    VulkanRenderer *-- VulkanTexturedPipeline
    VulkanRenderer *-- VulkanBuffer : vertex ring
    VulkanRenderer *-- VulkanBuffer : textured ring
    VulkanRenderer "1" *-- "0..*" VulkanTexture
    VulkanRenderer ..> PushConstants : uses
    VkSwapchain ..> VkContext : needs
    VulkanPipeline ..> VkContext
```

**Design decision:** All Vulkan resources are managed with `unique_ptr`; the
destructor order prevents incorrect resource destruction. `VkContext` is the
longest-lived object and is destroyed last.

---

## 6. Data Types Summary (POD/Value)

```mermaid
classDiagram
    class Vertex {
        +float x, y
        +uint8_t r, g, b, a
    }
    class TexturedVertex {
        +float x, y
        +float u, v
        +uint8_t r, g, b, a
    }
    class Point {
        +float x, y
    }
    class Rect {
        +float x, y, w, h
        +Right() float
        +Bottom() float
        +Center() Point
        +Contains(px, py) bool
    }
    class Size {
        +float w, h
    }
    class TextureHandle {
        <<typedef>>
        uint32_t (kInvalidTexture = 0)
    }
```

These types carry **value semantics**: copyable, small and meaningful on their
own. `Vertex` and `TexturedVertex` are the structures sent to the GPU; since
their layout is critical they must be changed carefully (they have to match the
shader attribute offsets).

---

## 7. Relationship Types Glossary

| Symbol | Meaning | Example |
|--------|-------|-------|
| `*--` | **Composition** — the member is destroyed when the owner is | `Painter *-- RenderBatcher` |
| `o--` | **Aggregation** — shared ownership | (not used) |
| `-->` | **Direct use** — pointer/reference | `RenderBatcher --> IRenderer` |
| `..>` | **Dependency** — short-lived use, forgotten on return | `Painter ..> Tessellator` |
| `<\|--` | **Inheritance** | (not used apart from interfaces) |
| `<\|..` | **Interface realization** | `IRenderer <\|.. OpenGLRenderer` |

Mermaid's `<<interface>>`, `<<enumeration>>`, `<<utility (static)>>`,
`<<RAII>>`, `<<facade>>`, `<<factory>>`, `<<typedef>>` stereotypes are used to
show the **character** of a class at a glance.
