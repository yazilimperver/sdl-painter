# SDLPainter — Sınıf Diyagramları

Bu dokümanda SDLPainter'ın **sınıf yapısı** Mermaid `classDiagram` notasyonuyla
verilmiştir. Diyagramlar parçalara bölünmüştür çünkü tek bir grafik hızla
okunamaz hale gelir; her parça farklı bir konsepte odaklanır.

---

## 1. Çekirdek Sınıflar — Painter ve Bağlamı

`Painter` etrafındaki üç temel kompozisyon: stil tipleri (`Pen`/`Brush`),
transform yığını (`RenderState` + `Transform`), ve render hattı
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

**İlişki notları**

- `Painter *-- RenderState`: Kompozisyon. `mCurrentState` aktif durum,
  `mStateStack` ise `Save()`/`Restore()` ile büyüyüp küçülen yığın.
- `static` factory metodları (`NoPen`, `Black`, `MakeTranslate`) `$` ile işaretli.

---

## 2. Render Hattı — Tessellator, RenderBatcher, IRenderer

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

    Painter ..> Tessellator : kullanır
    Painter *-- RenderBatcher
    RenderBatcher --> IRenderer : referans
    RenderBatcher *-- DrawMode
    IRenderer ..> RendererBackend
    IRenderer <|.. OpenGLRenderer
    IRenderer <|.. VulkanRenderer
    CreateRenderer ..> IRenderer : döner
    CreateRenderer ..> OpenGLRenderer : new
    CreateRenderer ..> VulkanRenderer : new
```

**Kritik nokta:** `Painter` **doğrudan** `IRenderer` çağırmaz; tüm çizimler
`RenderBatcher` üzerinden gider. Sadece `Begin()`, `End()`, `Clear()`,
`SetViewport()`, `SetScissor()` gibi non-batch işlemler `IRenderer`'a
direkt iner.

---

## 3. Görsel ve Yazı Tipleri

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

    Image *-- Texture : önbellek
    Texture --> IRenderer : owner
    Font *-- Glyph : cache
    Glyph *-- Texture
    Font ..> IRenderer : GetGlyph(renderer, ...)
    Image ..> IRenderer : Upload(renderer)
```

**Notlar**

- `Texture` saf RAII sarmalayıcıdır; non-copyable, movable. Yıkıcısında
  sahibi olduğu renderer üzerinden `DestroyTexture` çağırır.
- `Image::Upload` idempotenttir: ikinci çağrıda mevcut handle döner
  (lazy upload + cache).
- `Font::GetGlyph` glyph cache miss'inde SDL_ttf'ten render edip
  texture'a yükler; sonraki çağrılarda cache'ten döner.

---

## 4. OpenGL Backend Sınıfları

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

OpenGL backend yalnızca üç dış parça kullanır: `ShaderProgram` (GLSL
program sarmalayıcısı), GLAD (function loader, sınıf değil), ve VAO/VBO
handle'ları.

---

## 5. Vulkan Backend Sınıfları

Vulkan backend OpenGL'e göre çok daha katmanlıdır çünkü pipeline,
descriptor, swapchain ve sync nesneleri ayrı ayrı yönetilmek zorundadır.

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
        +IRenderer arayüzü
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
    VulkanRenderer ..> PushConstants : kullanır
    VkSwapchain ..> VkContext : ihtiyaç
    VulkanPipeline ..> VkContext
```

**Tasarım kararı:** Tüm Vulkan kaynakları `unique_ptr` ile yönetilir;
yıkıcı sırası hatalı kaynak yıkımını engeller. `VkContext` en uzun yaşayan
nesnedir, en son yıkılır.

---

## 6. Veri Tipleri Özeti (POD/Value)

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

Bu tipler **value semantic** taşır: kopyalanabilir, küçük ve kendi başına
anlamlı. `Vertex` ve `TexturedVertex` GPU'ya gönderilen yapılardır; layout
kritik olduğundan dikkatli değiştirilmeli (shader attribute ofsetleri ile
eşleşmeli).

---

## 7. İlişki Türleri Sözlüğü

| Sembol | Anlam | Örnek |
|--------|-------|-------|
| `*--` | **Kompozisyon** — sahibi yıkılınca üye de yıkılır | `Painter *-- RenderBatcher` |
| `o--` | **Agregasyon** — paylaşımlı sahiplik | (kullanılmıyor) |
| `-->` | **Doğrudan kullanım** — pointer/referans | `RenderBatcher --> IRenderer` |
| `..>` | **Bağımlılık** — kısa ömürlü kullanım, döndüğünde unutur | `Painter ..> Tessellator` |
| `<\|--` | **Kalıtım** | (interface dışında kullanılmıyor) |
| `<\|..` | **Arayüz uygulaması** | `IRenderer <\|.. OpenGLRenderer` |

Mermaid'in `<<interface>>`, `<<enumeration>>`, `<<utility (static)>>`,
`<<RAII>>`, `<<facade>>`, `<<factory>>`, `<<typedef>>` stereotipleri
sınıfın **karakterini** tek bakışta belirtmek için kullanıldı.
