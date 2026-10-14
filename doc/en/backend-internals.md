[Türkçe sürüm](../backend-ic-yapisi.md) | **English**

# SDLPainter — Backend Internals

In this document I will try to describe in detail the **two concrete
implementations** behind the `IRenderer` interface (OpenGL 3.3 and Vulkan 1.1).
For the upper-layer context see the [Architecture Overview](architecture.md);
for the interface definitions see the [Class Diagram](class-diagram.md).

---

## 1. Backend Comparison

| Topic | OpenGL 3.3 Backend | Vulkan 1.1 Backend |
|------|-------------------|-------------------|
| Initialization complexity | Low (context + GLAD) | High (instance/device/swapchain/sync) |
| Frame structure | Begin/End trivial | Begin/End: acquire + submit + present |
| Command submission | Immediate | Recorded into a command buffer |
| Vertex upload | `glBufferData` on every draw | Ring buffer + persistent map |
| Shader | GLSL 330 source embedded in the binary, compiled at runtime | SPIR-V embedded in the binary (pre-built offline with `glslc`) |
| Uniform | `glUniform*` | Push constants |
| Window size | Automatic | Requires swapchain recreate |
| Sync | Left to the driver | Manual (semaphore + fence) |
| Texture | `glGenTextures` + bind | `VkImage` + `VkImageView` + sampler + descriptor |
| Required SDL flag | `SDL_WINDOW_OPENGL` | `SDL_WINDOW_VULKAN` |

**Summary:** The OpenGL backend gives a simple, quick start. The Vulkan backend
works more efficiently with modern GPU architectures and gives more control; in
return it requires far more code and care. Since both use the same `IRenderer`
interface, Painter sees no difference between them.

---

## 2. OpenGL 3.3 Backend

### 2.1 Component Structure

```mermaid
flowchart TB
    subgraph user["Painter / RenderBatcher"]
        BATCH["DrawTriangles / DrawTextured call"]
    end

    subgraph ogl["OpenGLRenderer"]
        INIT["Initialize:<br/>SDL_GL_CreateContext +<br/>gladLoadGL"]
        SHADERS["ShaderProgram x2:<br/>basic + textured"]
        VAO["VAO + VBO x2:<br/>untextured + textured"]
        UNIFORMS["Uniform state:<br/>mProjection, mModel, mOpacity"]
        DRAW["DrawTriangles:<br/>BindVAO + glBufferData +<br/>SetUniforms + glDrawArrays"]
        TEX["CreateTexture:<br/>glGenTextures +<br/>glTexImage2D +<br/>glTexParameteri"]
    end

    subgraph gpu["GPU"]
        DRIVER["Driver"]
    end

    BATCH --> DRAW
    INIT --> SHADERS
    INIT --> VAO
    DRAW --> SHADERS
    DRAW --> VAO
    DRAW --> UNIFORMS
    DRAW --> DRIVER
    TEX --> DRIVER

    style ogl fill:#cf222e20,stroke:#cf222e
    style gpu fill:#6e778120,stroke:#6e7781
```

### 2.2 Draw Flow (`DrawTriangles`)

```mermaid
sequenceDiagram
    participant B as RenderBatcher
    participant R as OpenGLRenderer
    participant SP as ShaderProgram (basic)
    participant GL

    B->>R: DrawTriangles(vector~Vertex~)
    R->>SP: Use() (glUseProgram)
    R->>SP: SetUniformMat4("u_projection", mProjection)
    R->>SP: SetUniformMat3("u_model", mModel)
    R->>SP: SetUniformFloat("u_opacity", mOpacity)
    R->>GL: glBindVertexArray(mVao)
    R->>GL: glBindBuffer(GL_ARRAY_BUFFER, mVbo)
    R->>GL: glBufferData(verts.size, verts.data, GL_DYNAMIC_DRAW)
    R->>GL: glEnable(GL_BLEND)
    R->>GL: glBlendFunc(SRC_ALPHA, ONE_MINUS_SRC_ALPHA)
    R->>GL: glDrawArrays(GL_TRIANGLES, 0, count)
```

### 2.3 Shader Programs

There are two separate programs:

- **basic** — `basic.vert` + `basic.frag`: position + per-vertex color
- **textured** — `textured.vert` + `textured.frag`: position + UV + per-vertex tint

```mermaid
flowchart LR
    subgraph vs["Vertex Shader (basic.vert)"]
        VIN["in vec2 a_position<br/>in vec4 a_color"]
        VTRANS["mat3 u_model · vec3(a_position, 1.0)"]
        VPROJ["mat4 u_projection · vec4(...)"]
        VOUT["gl_Position +<br/>out vec4 v_color"]
    end

    subgraph fs["Fragment Shader (basic.frag)"]
        FIN["in vec4 v_color"]
        FALPHA["v_color * vec4(1, 1, 1, u_opacity)"]
        FOUT["out vec4 frag_color"]
    end

    VIN --> VTRANS --> VPROJ --> VOUT
    VOUT --> FIN --> FALPHA --> FOUT
```

> Because color is carried in the vertex, triangles of different colors can be
> drawn in **a single draw call** even across different pens/brushes. This is one
> of the cornerstones of RenderBatcher's efficiency.
>
> The same holds for the transform. `u_model` is still in the shader, but
> `Painter` always sends it as **identity**; the transform is applied on the CPU
> inside `RenderBatcher::PushTriangles`, in the same copy loop that writes the
> color. So `Translate`/`Rotate`/`Scale` do not break the batch either.
> Measurement: with 2000 shapes and a transform per shape we saw
> **2000 → 2 draw calls**, 9.44 ms → 0.30 ms.
> For detailed results and examples: [`examples/benchmarks/README.md`](../../examples/benchmarks/README.md).

### 2.4 Texture Operations

```cpp
// CreateTexture in outline
glGenTextures(1, &id);
glBindTexture(GL_TEXTURE_2D, id);
glTexImage2D(GL_TEXTURE_2D, 0, format, w, h, 0, format, GL_UNSIGNED_BYTE, data);
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
return static_cast<TextureHandle>(id);   // GL ID = TextureHandle
```

In the OpenGL backend `TextureHandle` is the GL texture ID itself (`uint32_t`).
In the Vulkan backend it is an opaque number resolved through an internal map.

### 2.5 Y-Axis Conversion

In OpenGL window coordinates Y=0 is at the **bottom**; in the SDLPainter API
Y=0 is at the **top**. Painter builds the projection matrix accordingly
(`UpdateProjection`):

```
ortho(0, width, height, 0, -1, 1)   // top-left origin
```

For scissoring, however, **OpenGL wants its native Y** (from the bottom); that
is why `Painter::ApplyScissor` applies a flip:
```cpp
gl_y = viewport_height - rect.y - rect.h;
glScissor(rect.x, gl_y, rect.w, rect.h);
```

---

## 3. Vulkan 1.1 Backend

### 3.1 Component Hierarchy

```mermaid
flowchart TB
    VR["VulkanRenderer"]
    VR --> CTX["VkContext<br/>(instance, physicalDevice,<br/>device, queue, surface)"]
    VR --> SC["VkSwapchain<br/>(images, views, renderpass,<br/>framebuffers)"]
    VR --> FS["VkFrameSync<br/>(imageAvailable, renderFinished,<br/>inFlight, commandBuffers x2)"]
    VR --> P1["VulkanPipeline<br/>(untextured)"]
    VR --> P2["VulkanTexturedPipeline<br/>(descriptor set layout +<br/>combined image sampler)"]
    VR --> RING1["VulkanBuffer (vertex ring)<br/>untextured"]
    VR --> RING2["VulkanBuffer (vertex ring)<br/>textured"]
    VR --> TEXMAP["unordered_map~Handle, VulkanTexture~"]
    TEXMAP --> TEX["VulkanTexture x N<br/>(image, view, sampler,<br/>descriptor set)"]

    SC --> CTX
    FS --> CTX
    P1 --> CTX
    P2 --> CTX
    RING1 --> CTX
    RING2 --> CTX

    style VR fill:#cf222e,color:#fff
    style CTX fill:#1f6feb,color:#fff
    style SC fill:#bf8700,color:#fff
    style FS fill:#bf8700,color:#fff
```

### 3.2 Initialize Flow

```mermaid
sequenceDiagram
    participant App
    participant VR as VulkanRenderer
    participant Ctx as VkContext
    participant SC as VkSwapchain
    participant FS as VkFrameSync
    participant P as VulkanPipeline

    App->>VR: Initialize(window)
    VR->>Ctx: Create()
    Ctx->>Ctx: vkCreateInstance (validation layers in Debug)
    Ctx->>Ctx: SDL_Vulkan_CreateSurface
    Ctx->>Ctx: vkEnumeratePhysicalDevices + pick suitable
    Ctx->>Ctx: vkCreateDevice + queue family
    Ctx->>Ctx: vkCreateCommandPool
    Ctx-->>VR: ready

    VR->>SC: Create(width, height)
    SC->>SC: vkCreateSwapchainKHR
    SC->>SC: vkCreateImageView x N
    SC->>SC: vkCreateRenderPass
    SC->>SC: vkCreateFramebuffer x N
    SC-->>VR: ready

    VR->>FS: Create(kMaxFramesInFlight=2)
    FS->>FS: vkCreateSemaphore x 2 (imageAvailable)
    FS->>FS: vkCreateSemaphore x 2 (renderFinished)
    FS->>FS: vkCreateFence x 2 (inFlight, signaled)
    FS->>FS: vkAllocateCommandBuffers x 2
    FS-->>VR: ready

    VR->>P: Create (untextured pipeline)
    P->>P: vkCreateShaderModule (vert, frag from .spv)
    P->>P: vkCreatePipelineLayout (push constants)
    P->>P: vkCreateGraphicsPipelines (dynamic viewport+scissor)
    P-->>VR: ready

    VR->>VR: VulkanTexturedPipeline + ring buffers
    VR-->>App: bool true
```

### 3.3 Frame Flow

For details see [Flow Diagrams → Vulkan Frame](flows.md#7-vulkan-frame-flow-phase-5).
Three important details:

1. **2 frames in flight** — `kMaxFramesInFlight = 2`. While the CPU prepares one
   frame, the GPU draws the previous one.
2. **Vertex ring buffer** — `VulkanBuffer` is persistently mapped; on every
   `DrawTriangles` call the offset inside the ring advances, so memcpy + draw
   are atomic.
3. **Push constants** — The projection × model matrix, tint and opacity are sent
   as push constants; there is no descriptor set or UBO.

### 3.4 Push Constants Layout

```cpp
struct PushConstants {
  float transform[16];    // mat4: projection × model, multiplied on the CPU
  float tint_color[4];    // vec4
  float opacity;
};
```

84 bytes in total; below the 128 bytes Vulkan guarantees (guarded by a
`static_assert`). It can change on every draw call and needs no descriptor set
update.

### 3.5 Swapchain Recreate

The swapchain is rebuilt by two triggers:

```mermaid
flowchart LR
    S["BeginFrame<br/>window pixel size ≠ mRequestedExtent"] --> N["mSwapchainNeedsRecreate = true"]
    A["AcquireNextImage<br/>VK_ERROR_OUT_OF_DATE_KHR"] --> B["mSwapchainOutOfDate = true"]
    N --> C["Recreate flow:"]
    B --> C
    C --> D["vkDeviceWaitIdle"]
    D --> E["VkSwapchain.Destroy"]
    E --> F["VkSwapchain.Create(new w, new h)"]
    F --> G["New framebuffers"]
    G --> H["mRequestedExtent = (w, h)<br/>mSwapchainNeedsRecreate = false"]
    H --> I["The next acquire uses the new swapchain"]

    style S fill:#cf222e,color:#fff
    style A fill:#cf222e,color:#fff
    style I fill:#2da44e,color:#fff
```

The first trigger is the size check in `BeginFrame`. On surfaces that report a
`currentExtent` of 0xFFFFFFFF (Wayland, the headless surface of SDL's
`offscreen` driver) the swapchain determines the surface size; when the window
changes the swapchain does not become incompatible and the driver does not have
to report `OUT_OF_DATE`. That is why `BeginFrame` polls the window's pixel size
(`SDL_GetWindowSizeInPixels`) every frame and, if it differs, rebuilds the
swapchain before acquiring an image. The comparison is made against the size
requested at the last successful build (`mRequestedExtent`), not against the
swapchain extent: the extent may have been clamped by
`minImageExtent`/`maxImageExtent` or stayed at the size the surface reports,
and the swapchain would then be rebuilt every frame. `mRequestedExtent` is
updated in `Initialize` and on every successful rebuild along either path.

A rebuild moves the viewport to the new extent only if `SetViewport` has never
been called, so in that case the viewport tracks the surface. A viewport given
through `SetViewport` (`mViewportSetByCaller`) is preserved, just as OpenGL
writes `glViewport` only in `SetViewport`; on a size change Painter itself sends
the new viewport before `BeginFrame`.

The `QueuePresentKHR` call can also return `OUT_OF_DATE_KHR` or
`SUBOPTIMAL_KHR`; the same recreate path is triggered.

If the surface is not renderable (minimized, 0x0), `BeginFrame` skips the frame
before reaching the size check and sets `mSwapchainNeedsRecreate`; the rebuild
is deferred until the window comes back. If a rebuild fails, the flag stays set
and the next frame tries again.

### 3.6 Texture Lifecycle (Vulkan)

A single `VulkanTexture` wraps the following Vulkan objects:

```mermaid
classDiagram
    class VulkanTexture {
        +VkImage image
        +VkImageView view
        +VkSampler sampler
        +VkDeviceMemory memory
        +VkDescriptorSet descriptorSet
        +VkExtent2D extent
    }
```

Creation steps:

1. `vkCreateImage` (TILING_OPTIMAL)
2. `vkAllocateMemory` + `vkBindImageMemory`
3. Copy the pixel data through a **staging buffer**
4. `vkCmdCopyBufferToImage` (transfer command)
5. Layout transition: `UNDEFINED → TRANSFER_DST → SHADER_READ_ONLY`
6. `vkCreateImageView` (RGBA format)
7. `vkCreateSampler` (LINEAR + CLAMP_TO_EDGE)
8. Bind to the descriptor set: `combined image sampler`

Destruction: `vkDestroy*` on the objects above in reverse order. The
`VulkanTexture` destructor (`Destroy()`) guarantees this order.

---

## 4. How the Two Backends Respond to the Same Painter Call

The concrete path the same `painter.FillCircle(400, 300, 50)` call takes in the
two backends:

```mermaid
sequenceDiagram
    participant P as Painter
    participant T as Tessellator
    participant B as RenderBatcher
    participant OGL as OpenGLRenderer
    participant VK as VulkanRenderer
    participant GPU

    P->>T: TessellateFilledCircle(400, 300, 50)
    T-->>P: 75 vertices (25 segments, triangle fan)
    P->>B: PushTriangles(verts, brush.color, opacity)

    Note over B,GPU: Flush triggered (state changed or End)

    par OpenGL path
        B->>OGL: DrawTriangles(buffer)
        OGL->>GPU: glUseProgram(basicShader)
        OGL->>GPU: glBufferData(verts)
        OGL->>GPU: glUniform*(projection, model, opacity)
        OGL->>GPU: glDrawArrays(GL_TRIANGLES, 0, 75)
    and Vulkan path
        B->>VK: DrawTriangles(buffer)
        VK->>VK: vertex ring → memcpy(buffer)
        VK->>GPU: vkCmdBindPipeline(untextured)
        VK->>GPU: vkCmdBindVertexBuffers(ring, offset)
        VK->>GPU: vkCmdPushConstants(projection+model+opacity)
        VK->>GPU: ApplyDynamicViewportScissor
        VK->>GPU: vkCmdDraw(75, 1, 0, 0)
    end
```

**The result is identical at the pixel level** — tessellation produces the same
vertices, the projection matrices are the same, the blend state is the same.

This is no longer an assumption but a **measurement**:
`tests/test_backend_parity.cpp` draws the same scene into a render target on both
backends and compares the pixels (measured difference: 0). When the test was
written it found three real divergences — Vulkan textures being in sRGB format,
the blend modes' alpha factors not matching, and the initial blend state on
OpenGL skipping `SetBlendMode`. None of the three was visible on screen.

**The only known difference is anti-aliasing.** MSAA is a GL window attribute
(`SDL_GL_MULTISAMPLESAMPLES`, via `AppConfig::msaa_samples`) and only affects
OpenGL's **screen** framebuffer. The Vulkan pipelines hard-code
`VK_SAMPLE_COUNT_1_BIT`; render targets are single-sampled on both backends too.
That is why the parity test draws into a target: the comparison does not trip
over the MSAA difference.

---

## 5. Adding a Backend

To add a new backend (e.g. Metal, DirectX 12, Software):

```mermaid
flowchart TB
    A["1) include/sdl_painter/renderer.h<br/>Add kMetal to the RendererBackend enum"] --> B
    B["2) src/metal/metal_renderer.{h,cpp}<br/>Implement IRenderer"] --> C
    C["3) src/renderer.cpp<br/>Add a case to the CreateRenderer factory"] --> D
    D["4) CMakeLists.txt<br/>Add an optional SDLPAINTER_WITH_METAL flag"] --> E
    E["5) examples/<br/>Write a test demo"] --> F
    F["Painter, Tessellator, RenderBatcher,<br/>RenderState DO NOT CHANGE"]

    style F fill:#2da44e,color:#fff
```

This is the concrete form of the **Open/Closed Principle**. Thanks to it the
IRenderer interface does not grow and the existing backends are not affected by
the change.

---

## 6. Related Architecture Decision Records (ADR)

| ADR | Decision | Related section in this document |
|-----|-------|---------------------------|
| ADR-001 | OpenGL 3.3 Core (not 4.5/ES) | §2 |
| ADR-003 | Geometry quads instead of `glLineWidth` | Tessellator (flows.md) |
| ADR-004 | Backend-agnostic Tessellator | §4 |
| ADR-005 | stb_image (not SDL_image) | §2.4 |
| ADR-006 | Ear clipping triangulation | Tessellator (flows.md) |

---

## 7. Next Steps

- [Class Diagram → Vulkan Section](class-diagram.md#5-vulkan-backend-classes)
- [Flow Diagrams → Vulkan Frame](flows.md#7-vulkan-frame-flow-phase-5)
- [Software Engineering Perspective](software-engineering.md)
