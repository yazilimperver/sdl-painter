[Türkçe sürüm](../akislar.md) | **English**

# SDLPainter — Flow Diagrams

In this document I have tried to describe SDLPainter's **runtime behavior** with
sequence and flow diagrams. For the static structure see the
[Class Diagram](class-diagram.md); for the layer context see the
[Architecture Overview](architecture.md).

---

## 1. Frame Lifecycle

The flow Painter and the renderer follow in a typical game loop iteration:

```mermaid
sequenceDiagram
    autonumber
    participant App as User Application
    participant P as Painter
    participant B as RenderBatcher
    participant R as IRenderer
    participant GPU

    App->>P: Begin()
    P->>P: Did the window size change?
    alt size changed
        P->>R: SetViewport(0, 0, w, h)
        P->>P: UpdateProjection()
        P->>R: SetProjectionMatrix(mat4)
    end
    P->>R: BeginFrame()
    R-->>GPU: (Vulkan: AcquireNextImage)
    P->>P: FlushTransform()
    P->>R: SetModelMatrix(transform)

    Note over App,GPU: Draw calls (DrawRect, FillCircle, ...)
    loop every draw call
        App->>P: DrawXxx(...) / FillXxx(...)
        P->>P: Tessellator → vertex list
        P->>B: PushTriangles(verts, color, opacity)
        opt mode/opacity/texture changed or buffer full
            B->>R: DrawTriangles(buffer)
            R-->>GPU: vertex upload + draw
        end
    end

    App->>P: End()
    P->>B: Flush() (drain remaining vertices)
    B->>R: DrawTriangles(buffer)
    R-->>GPU: final draw
    P->>R: EndFrame()
    R-->>GPU: SwapBuffers / Present
```

**Important contracts**

- Draw calls must be **between Begin/End**; otherwise the tessellator produces
  vertices but they pile up in the batcher and the frame boundary is undefined.
- `End()` calls a **guaranteed `Flush()`**; no vertices are lost when the
  application shuts down.
- A window size change is checked at the start of every `Begin()`; no manual
  callback is needed.

---

## 2. A Single `DrawRect` Call in Detail

```mermaid
sequenceDiagram
    autonumber
    participant App
    participant P as Painter
    participant T as Tessellator
    participant B as RenderBatcher
    participant R as IRenderer

    App->>P: DrawRect(x, y, w, h)
    P->>P: CanDrawPen() ?
    alt pen invisible (alpha=0 or width=0)
        P-->>App: early return
    else pen visible
        P->>P: FlushTransform()
        Note right of P: SetModelMatrix is called only<br/>if the transform changed
        P->>T: TessellateStrokedRect(x,y,w,h, line_width)
        T-->>P: vector~Vertex~ (4 quads / 8 triangles)
        P->>B: PushTriangles(verts, pen.color, opacity)
        B->>B: mode == kBasic ? same opacity? size+verts ≤ 8192?
        alt yes (append to batch)
            B->>B: write colors and append to mVertexBuffer
        else no (state changed / buffer full)
            B->>R: SetOpacity(prev_opacity)
            B->>R: DrawTriangles(previous buffer)
            B->>B: clear buffer, mode = kBasic
            B->>B: append new vertices to buffer
        end
    end
```

**Key point:** Tessellator calls are **stateless**, so they are always
deterministic. This is why measuring and testing need no GPU (with a mock
IRenderer).

---

## 3. Render Batcher Flow

`RenderBatcher`'s internal state machine and the conditions that trigger
`Flush()`:

```mermaid
stateDiagram-v2
    [*] --> Bos: Constructor

    Bos: kNone (empty)
    Bos --> Basic: PushTriangles
    Bos --> Textured: PushTexturedTriangles

    Basic: kBasic
    Basic --> Basic: same opacity & buffer ≤ 8192
    Basic --> Basic: color / transform CHANGED (no break)
    Basic --> FlushBasic: opacity CHANGED
    Basic --> FlushBasic: buffer full (>8192)
    Basic --> FlushBasic_Tex: PushTexturedTriangles arrived

    Textured: kTextured + texture handle
    Textured --> Textured: same tex & opacity & buffer ≤ 8192
    Textured --> FlushTextured: texture CHANGED
    Textured --> FlushTextured: opacity CHANGED
    Textured --> FlushTextured: buffer full
    Textured --> FlushTex_Basic: PushTriangles arrived

    FlushBasic: renderer.DrawTriangles
    FlushBasic --> Basic
    FlushBasic_Tex --> Textured
    FlushTextured: renderer.DrawTextured
    FlushTextured --> Textured
    FlushTex_Basic --> Basic

    Basic --> [*]: Painter.End() → Flush
    Textured --> [*]: Painter.End() → Flush
```

### Flush Trigger Conditions (Table)

| Transition | Cause | Result |
|-------|-------|-------|
| kBasic → kBasic | Same opacity, buffer < 8192 | **NO flush**, vertices are appended |
| kBasic → kBasic | Opacity changed | **Flush** + a new batch starts |
| kBasic → kBasic | Buffer would exceed 8192 vertices | **Flush** + new batch |
| kBasic → kTextured | Draw mode changed | **Flush** + a textured batch starts |
| kTextured → kTextured | Texture changed | **Flush** + new texture batch |
| Painter.SetClipRect / ClearClip | Scissor GPU state changed | **Flush** + new scissor |
| Painter.Restore() | If the clip state **actually** changed | **Flush** + scissor restored |
| Painter.End() | End of frame | **Flush** (whatever is left) |
| Painter.Clear() | Screen clear | **Flush** first, then Clear |

**Things that DO NOT trigger a flush** (often confused):

| Operation | Why it does not break the batch |
|-------|----------------------|
| `SetPen` / `SetBrush` | Color is carried in the `Vertex`, not a uniform |
| `Translate` / `Rotate` / `Scale` / `ResetTransform` | The transform is applied to the vertices on the CPU inside `RenderBatcher`; the model matrix is always identity |
| `Save()` | Nothing that is saved is GPU state |
| `Restore()` — if the clip is the same | Only a clip change requires a flush |
| `SetOpacity` — to the same value | `SameOpacity()` tolerance (1/512) |

**Practical effect:** 2000 `FillRect`s in different colors, each with its own
`Translate`/`Rotate` → **2 draw calls** (12 000 vertices, split in two only
because of the 8192 ceiling).

This table is a measurement, not a guess: for per-scenario draw call and timing
data see **[`examples/benchmarks/README.md`](../../examples/benchmarks/README.md)**.
The same counters can be read at runtime with `Painter::GetFrameStats()`, and in
`Application`-based programs in the on-screen overlay toggled with **F1**.

![Statistics overlay](../images/stats-overlay.png)

---

## 4. Transform Stack — `Save` / `Restore` Logic

```mermaid
sequenceDiagram
    autonumber
    participant App
    participant P as Painter
    participant S as mStateStack
    participant T as Transform (current)
    participant R as IRenderer

    App->>P: Save()
    P->>S: push_back(mCurrentState)

    App->>P: Translate(100, 50)
    P->>T: Translate(100, 50) — matrix multiplication
    Note over P: SetModelMatrix not<br/>called yet (lazy flush)

    App->>P: Rotate(45°)
    P->>T: Rotate(45)

    App->>P: DrawRect(...)
    P->>P: FlushTransform()
    P->>R: SetModelMatrix(transform.Data())
    Note right of R: Matrix sent to the GPU once
    P->>R: (Tessellator + Batcher)

    App->>P: DrawCircle(...)
    P->>P: FlushTransform() — skipped if unchanged
    P->>R: (Batcher push)

    App->>P: Restore()
    P->>S: back() → mCurrentState
    P->>S: pop_back()
    Note over P: The next draw triggers<br/>FlushTransform again
```

**What does RenderState carry?** → `transform`, `pen`, `brush`, `opacity`,
`clip_rect`, `has_clip`. `Save()` takes a **copy of all of them**;
`Restore()` restores them all.

### Nested Save/Restore

```mermaid
flowchart TB
    A["state_0 (identity)"] -->|Save| B["stack: [s0]"]
    B -->|Translate 100,0| C["state_1 (T100)"]
    C -->|Save| D["stack: [s0, s1]"]
    D -->|Rotate 30| E["state_2 (T100·R30)"]
    E -->|DrawRect| F["draw"]
    F -->|Restore| G["state_1 (T100)"]
    G -->|Restore| H["state_0 (identity)"]

    style A fill:#1f6feb,color:#fff
    style C fill:#2da44e,color:#fff
    style E fill:#bf8700,color:#fff
    style G fill:#2da44e,color:#fff
    style H fill:#1f6feb,color:#fff
```

---

## 5. Texture Upload (Image → GPU)

`Image` uses a lazy upload + cache strategy. It uploads on the first call and
returns the existing handle afterwards.

```mermaid
sequenceDiagram
    autonumber
    participant App
    participant Img as Image
    participant Tex as Texture (RAII)
    participant R as IRenderer
    participant GPU

    App->>Img: Image("sprite.png") (constructor)
    Img->>Img: stbi_load → mRawData (CPU memory)

    App->>P: painter.DrawImage(img, x, y)
    P->>Img: Upload(*mRenderer)

    alt First time (mHandle.Handle() == kInvalidTexture)
        Img->>R: CreateTexture(data, w, h, channels)
        R->>GPU: glTexImage2D / vkCreateImage + vkAllocateMemory + upload
        R-->>Img: TextureHandle (new)
        Img->>Tex: mHandle = Texture(renderer, handle)
    else Already cached
        Img-->>P: existing handle
    end

    P->>P: src_rect → UV coordinate conversion
    P->>P: dest_rect → screen coordinate conversion
    P->>B: PushTexturedTriangles(verts, handle, white tint, opacity)
    B->>R: DrawTextured(buffer, handle) (on Flush)
    R->>GPU: bind texture + draw
```

### Image / Texture Lifecycle

```mermaid
flowchart LR
    Created["Image created<br/>(CPU memory filled)"] -->|First Upload| Uploaded["Image + Texture<br/>(GPU memory filled)"]
    Uploaded -->|Later Upload| Uploaded
    Uploaded -->|Image destroyed| Destroyed["Texture::~Texture<br/>→ DestroyTexture"]
    Destroyed --> Done["GPU memory freed"]

    style Created fill:#1f6feb,color:#fff
    style Uploaded fill:#2da44e,color:#fff
    style Destroyed fill:#bf8700,color:#fff
    style Done fill:#6e7781,color:#fff
```

**RAII guarantee:** The `Texture` class is non-copyable and movable. Its
destructor calls `DestroyTexture` → a texture leak is impossible.

---

## 6. Text Rendering (Phase 4) — Glyph Cache

```mermaid
sequenceDiagram
    autonumber
    participant App
    participant P as Painter
    participant F as Font
    participant SDL as SDL_ttf
    participant R as IRenderer

    App->>P: DrawText(x, y, "Hello")
    loop every UTF-8 character (codepoint)
        P->>F: GetGlyph(renderer, codepoint)
        alt cache hit
            F-->>P: const Glyph* (existing texture)
        else cache miss
            F->>SDL: TTF_RenderGlyph_Blended(font, codepoint)
            SDL-->>F: SDL_Surface (RGBA pixel buffer)
            F->>R: CreateTexture(surface.pixels, w, h, 4)
            R-->>F: TextureHandle
            F->>F: mGlyphCache[codepoint] = Glyph{texture, metrics...}
            F-->>P: new Glyph*
        end
        P->>P: pen color as tint, advance current_x by glyph.advance
        P->>B: PushTexturedTriangles(quad verts, glyph.texture.handle, tint, opacity)
    end
```

**Optimization:** The glyph cache works per UTF-8 codepoint. Turkish characters
and emoji cost something on their first render and are free afterwards.

---

## 7. Vulkan Frame Flow (Phase 5)

In OpenGL `BeginFrame`/`EndFrame` are almost trivial; in Vulkan there is
swapchain management, sync objects and out-of-date handling.

```mermaid
sequenceDiagram
    autonumber
    participant P as Painter
    participant VR as VulkanRenderer
    participant SC as VkSwapchain
    participant FS as VkFrameSync
    participant Q as VkQueue
    participant Pres as Presentation Engine

    P->>VR: BeginFrame()
    VR->>FS: vkWaitForFences(inFlight[currentFrame])
    VR->>FS: vkResetFences(inFlight[currentFrame])
    VR->>SC: vkAcquireNextImageKHR(swapchain, imageAvailable[currentFrame])

    alt VK_ERROR_OUT_OF_DATE_KHR
        VR->>SC: Recreate(new_width, new_height)
        VR-->>P: retry
    else VK_SUCCESS
        VR->>VR: mCurrentImageIndex = acquired
        VR->>FS: vkBeginCommandBuffer(commandBuffer)
        VR->>VR: vkCmdBeginRenderPass(framebuffers[imageIndex])
    end

    Note over P,Q: Draw commands (DrawTriangles → vkCmdDraw)
    P->>VR: DrawTriangles(verts) (via the Batcher)
    VR->>VR: VertexRing.Upload(verts)
    VR->>VR: vkCmdBindPipeline / BindVertexBuffer / PushConstants
    VR->>VR: ApplyDynamicViewportScissor
    VR->>VR: vkCmdDraw(vertexCount, ...)

    P->>VR: EndFrame()
    VR->>VR: vkCmdEndRenderPass + vkEndCommandBuffer
    VR->>Q: vkQueueSubmit(commandBuffer, wait=imageAvailable, signal=renderFinished)
    VR->>Q: vkQueuePresentKHR(swapchain, wait=renderFinished)
    Q->>Pres: Present
    VR->>VR: currentFrame = (currentFrame + 1) % kMaxFramesInFlight (=2)
```

**Out-of-date handling:** When the window is resized, the swapchain becomes
invalid. `AcquireNextImage` or `QueuePresent` returns `OUT_OF_DATE`;
`VulkanRenderer` rebuilds the swapchain with `Recreate`. Painter is not aware of
this detail.

---

## 8. Backend Selection (Factory)

```mermaid
flowchart TB
    A["Painter constructor:<br/>Painter(window, RendererBackend)"] --> B["CreateRenderer(backend)"]
    B --> C{"backend?"}
    C -->|kOpenGL| D["new OpenGLRenderer"]
    C -->|kVulkan| E{"SDLPAINTER_HAS_VULKAN<br/>compile flag?"}
    E -->|set| F["new VulkanRenderer"]
    E -->|not set| G["nullptr → Painter.IsValid() false"]
    D --> H["unique_ptr~IRenderer~"]
    F --> H
    G --> I["Error log + early return"]
    H --> J["renderer->Initialize(window)"]
    J --> K{"succeeded?"}
    K -->|yes| L["Painter ready"]
    K -->|no| M["renderer.reset() — IsValid() false"]

    style D fill:#cf222e,color:#fff
    style F fill:#cf222e,color:#fff
    style L fill:#2da44e,color:#fff
    style I fill:#bf8700,color:#fff
    style M fill:#bf8700,color:#fff
```

**Contract:** The Painter constructor can fail; **the user must check with
`painter.IsValid()`**. No exception is thrown (for deterministic errors on the
C++ side).

---

## 9. Tessellator Internal Flows

### 9.1 Adaptive Circle Segment Count

```mermaid
flowchart LR
    A["TessellateFilledCircle(cx, cy, r)"] --> B["segments = max(16, int(r * 0.5))"]
    B --> C["Build a triangle fan:<br/>center + N+1 perimeter points"]
    C --> D["3 vertices × N triangles"]

    style A fill:#1f6feb,color:#fff
    style D fill:#2da44e,color:#fff
```

| Radius | Segments | Triangles | Vertices |
|---------|---------|-------|--------|
| 8 px    | 16      | 16    | 48     |
| 50 px   | 25      | 25    | 75     |
| 200 px  | 100     | 100   | 300    |
| 1000 px | 500     | 500   | 1500   |

### 9.2 Concave Polygon — Ear Clipping

```mermaid
flowchart TB
    A["TessellateFilledPolygon(points)"] --> B{"Convex?"}
    B -->|yes| C["Triangle fan O(n)"]
    B -->|no| D["EarClipping O(n²)"]
    D --> E["1. Keep as a linked list"]
    E --> F["2. Find an 'ear':<br/>a triangle with no other point inside"]
    F --> G["3. Remove the ear, store the triangle"]
    G --> H{"Points left?"}
    H -->|yes| F
    H -->|no| I["Triangulation complete"]
    C --> J["vector~Vertex~"]
    I --> J

    style B fill:#bf8700,color:#fff
    style D fill:#cf222e,color:#fff
    style J fill:#2da44e,color:#fff
```

### 9.3 Thick Line (no `glLineWidth`)

```mermaid
flowchart LR
    A["TessellateThickLine(A, B, w)"] --> B["dir = normalize(B-A)"]
    B --> C["normal = (-dir.y, dir.x)"]
    C --> D["half = w/2 · normal"]
    D --> E["A1 = A + half<br/>A2 = A − half<br/>B1 = B + half<br/>B2 = B − half"]
    E --> F["2 triangles:<br/>(A1, A2, B1) + (A2, B2, B1)"]
    F --> G["6 vertices"]

    style A fill:#1f6feb,color:#fff
    style G fill:#2da44e,color:#fff
```

Under an orthographic projection this approach gives pixel-consistent results on
every platform; `glLineWidth`, on the other hand, has an upper limit that varies
by driver.

---

## 10. Misuse Scenarios

| Mistake | Effect | Safeguard |
|------|------|-------|
| `DrawRect` without calling `Begin()` | Piles up in the buffer, frame boundary breaks | Assertion in the API (development) |
| Shutting down without calling `End()` | The last batch is not flushed, the last frame is broken | Painter destructor → Shutdown |
| `Pen({0,0,0,0}, 5.0f)` | `IsVisible()` false → drawing is skipped | Early return, no GPU load |
| Not checking `painter.IsValid()` | Null deref if renderer init failed | The demos check it mandatorily |
| `Upload`ing the same `Image` in multiple Painters | A separate texture per renderer | An Image should be bound to one renderer |
| Forgetting `Restore` after `Save` | `mStateStack` leaks | An RAII guard helper is suggested (v2) |
