# SDLPainter — Backend İç Yapısı

Bu dokümanda, `IRenderer` arayüzünün arkasındaki **iki somut implementasyonu**
(OpenGL 3.3 ve Vulkan 1.1) detaylı anlatmaya çalışacağım. Üst katman bağlamı için
[Mimari Genel Bakış](mimari-genel-bakis.md), arayüz tanımları için
[Sınıf Diyagramı](sinif-diyagrami.md) dokümanlarına başvurabilirsiniz.

---

## 1. Backend Karşılaştırması

| Konu | OpenGL 3.3 Backend | Vulkan 1.1 Backend |
|------|-------------------|-------------------|
| Initialization karmaşıklığı | Düşük (context + GLAD) | Yüksek (instance/device/swapchain/sync) |
| Frame yapısı | Begin/End trivial | Begin/End: acquire + submit + present |
| Komut gönderimi | Anında (immediate) | Command buffer'a kaydedilir |
| Vertex upload | `glBufferData` her draw | Ring buffer + persistent map |
| Shader | GLSL 330 kaynağı binary'ye gömülü, runtime derlenir | SPIR-V binary'ye gömülü (offline `glslc` ile önceden üretilmiş) |
| Uniform | `glUniform*` | Push constants |
| Pencere boyutu | Otomatik | Swapchain recreate gerekir |
| Sync | Sürücüye bırakılır | Manuel (semaphore + fence) |
| Texture | `glGenTextures` + bind | `VkImage` + `VkImageView` + sampler + descriptor |
| Sahip olunan SDL flag | `SDL_WINDOW_OPENGL` | `SDL_WINDOW_VULKAN` |

**Özet:** OpenGL backend basit ve hızlı bir başlangıç sağlar. Vulkan
backend modern GPU mimarileriyle daha verimli çalışır ve daha fazla
kontrol verir; karşılığında çok daha fazla kod ve dikkat gerektirir.
İkisi de aynı `IRenderer` arayüzünü kullandığı için Painter ikisi
arasında fark görmez.

---

## 2. OpenGL 3.3 Backend

### 2.1 Bileşen Yapısı

```mermaid
flowchart TB
    subgraph user["Painter / RenderBatcher"]
        BATCH["DrawTriangles / DrawTextured çağrısı"]
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
        DRIVER["Sürücü"]
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

### 2.2 Çizim Akışı (`DrawTriangles`)

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

### 2.3 Shader Programları

İki ayrı program var:

- **basic** — `basic.vert` + `basic.frag`: pozisyon + per-vertex renk
- **textured** — `textured.vert` + `textured.frag`: pozisyon + UV + per-vertex tint

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

> Renk vertex'te taşındığı için aynı pen/brush dışında bile **tek bir
> draw call** içinde farklı renkli üçgenler çizilebilir. Bu RenderBatcher
> verimliliğinin temel noktalarından biridir.
>
> Aynı husus transform için de geçerlidir. `u_model` shader'da duruyor ama
> `Painter` onu daima **birim** gönderiyor; dönüşüm
> `RenderBatcher::PushTriangles` içinde, rengin yazıldığı kopyalama
> döngüsünde CPU'da uygulanıyor. Böylece `Translate`/`Rotate`/`Scale` de
> batch'i kırmıyor. Ölçüm: 2000 şekil, şekil başına transform ile
> **2000 → 2 draw call**, 9.44 ms → 0.30 ms'e düştüğünü gördük.
> Ayrıntılı sonuçlar ve örnekler için: [`examples/benchmarks/README.md`](../examples/benchmarks/README.md).

### 2.4 Texture İşlemleri

```cpp
// CreateTexture ana hatları
glGenTextures(1, &id);
glBindTexture(GL_TEXTURE_2D, id);
glTexImage2D(GL_TEXTURE_2D, 0, format, w, h, 0, format, GL_UNSIGNED_BYTE, data);
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
return static_cast<TextureHandle>(id);   // GL ID = TextureHandle
```

`TextureHandle` OpenGL backend'de doğrudan GL texture ID'sidir
(`uint32_t`). Vulkan backend'de ise iç haritada çözülen opak bir
sayıdır.

### 2.5 Y Ekseni Dönüşümü

OpenGL pencere koordinatlarında Y=0 **alttadır**; SDLPainter API'sinde
ise Y=0 **üstte**. Painter projeksiyon matrisini buna göre kurar
(`UpdateProjection`):

```
ortho(0, width, height, 0, -1, 1)   // top-left origin
```

Scissor kullanırken ise **OpenGL doğal Y'de** (alttan) ister; bu yüzden
`Painter::ApplyScissor` flip uygular:
```cpp
gl_y = viewport_height - rect.y - rect.h;
glScissor(rect.x, gl_y, rect.w, rect.h);
```

---

## 3. Vulkan 1.1 Backend

### 3.1 Bileşen Hiyerarşisi

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

### 3.2 Initialize Akışı

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
    Ctx->>Ctx: vkCreateInstance (validation layers Debug'da)
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

### 3.3 Frame Akışı

Detaylar için [Akış Diyagramları → Vulkan Frame](akislar.md#7-vulkan-frame-akışı-phase-5).
Üç önemli detay:

1. **2 frame in flight** — `kMaxFramesInFlight = 2`. CPU bir frame'i
   hazırlarken GPU önceki frame'i çiziyor.
2. **Vertex ring buffer** — `VulkanBuffer` persistent mapped; her
   `DrawTriangles` çağrısında ring içinde ofset ilerletilir, böylece
   memcpy + draw atomik olur.
3. **Push constants** — Projeksiyon, model matrisi ve opacity push
   constants ile gönderilir; descriptor set veya UBO yok.

### 3.4 Push Constants Layout

```cpp
struct PushConstants {
  float projection[16];   // mat4
  float model[9];         // mat3 (gerçekte 12 byte std140 alignment)
  float opacity;
};
```

Toplam ≤ 128 byte (Vulkan minimum garanti). Her draw call'da değişebilir,
descriptor set güncellemesi gerektirmez.

### 3.5 Swapchain Recreate

Pencere yeniden boyutlandığında:

```mermaid
flowchart LR
    A["AcquireNextImage<br/>VK_ERROR_OUT_OF_DATE_KHR"] --> B["mSwapchainOutOfDate = true"]
    B --> C["Recreate akışı:"]
    C --> D["vkDeviceWaitIdle"]
    D --> E["VkSwapchain.Destroy"]
    E --> F["VkSwapchain.Create(new w, new h)"]
    F --> G["Yeni framebuffer'lar"]
    G --> H["mSwapchainOutOfDate = false"]
    H --> I["Frame'i tekrar dene"]

    style A fill:#cf222e,color:#fff
    style I fill:#2da44e,color:#fff
```

`QueuePresentKHR` çağrısı da `OUT_OF_DATE_KHR` veya `SUBOPTIMAL_KHR`
dönebilir; aynı recreate yolu tetiklenir.

### 3.6 Texture Yaşam Döngüsü (Vulkan)

Tek bir `VulkanTexture` aşağıdaki Vulkan nesnelerini kapsar:

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

Yaratım adımları:

1. `vkCreateImage` (TILING_OPTIMAL)
2. `vkAllocateMemory` + `vkBindImageMemory`
3. **Staging buffer** ile pixel verisini kopyala
4. `vkCmdCopyBufferToImage` (transfer komutu)
5. Layout transition: `UNDEFINED → TRANSFER_DST → SHADER_READ_ONLY`
6. `vkCreateImageView` (RGBA format)
7. `vkCreateSampler` (LINEAR + CLAMP_TO_EDGE)
8. Descriptor set'e bind: `combined image sampler`

Yıkım: yukarıdaki nesnelerin tersi sırada `vkDestroy*`. RAII helper
sınıfları (`vk_raii.h`) bu sırayı garanti eder.

---

## 4. İki Backend'in Aynı Painter Çağrısına Verdiği Yanıt

Aynı `painter.FillCircle(400, 300, 50)` çağrısının iki backend'de
aldığı somut yol:

```mermaid
sequenceDiagram
    participant P as Painter
    participant T as Tessellator
    participant B as RenderBatcher
    participant OGL as OpenGLRenderer
    participant VK as VulkanRenderer
    participant GPU

    P->>T: TessellateFilledCircle(400, 300, 50)
    T-->>P: 75 vertex (25 segment, triangle fan)
    P->>B: PushTriangles(verts, brush.color, opacity)

    Note over B,GPU: Flush tetiklendi (state değişti veya End)

    par OpenGL yolu
        B->>OGL: DrawTriangles(buffer)
        OGL->>GPU: glUseProgram(basicShader)
        OGL->>GPU: glBufferData(verts)
        OGL->>GPU: glUniform*(projection, model, opacity)
        OGL->>GPU: glDrawArrays(GL_TRIANGLES, 0, 75)
    and Vulkan yolu
        B->>VK: DrawTriangles(buffer)
        VK->>VK: vertex ring → memcpy(buffer)
        VK->>GPU: vkCmdBindPipeline(untextured)
        VK->>GPU: vkCmdBindVertexBuffers(ring, offset)
        VK->>GPU: vkCmdPushConstants(projection+model+opacity)
        VK->>GPU: ApplyDynamicViewportScissor
        VK->>GPU: vkCmdDraw(75, 1, 0, 0)
    end
```

**Sonuç piksel düzeyinde aynıdır** — tessellation aynı vertex'leri
üretir, projeksiyon matrisleri aynıdır, blend state aynıdır. Tek olası
fark: anti-aliasing yapılandırması (MSAA örnekleme) backend'in pipeline
veya FBO ayarına bağlı olabilir.

---

## 5. Backend Eklemek

Yeni bir backend eklemek (örn. Metal, DirectX 12, Software) için:

```mermaid
flowchart TB
    A["1) include/sdl_painter/renderer.h<br/>RendererBackend enum'una kMetal ekle"] --> B
    B["2) src/metal/metal_renderer.{h,cpp}<br/>IRenderer'ı implement et"] --> C
    C["3) src/renderer.cpp<br/>CreateRenderer factory'sine case ekle"] --> D
    D["4) CMakeLists.txt<br/>Opsiyonel SDLPAINTER_WITH_METAL flag'i ekle"] --> E
    E["5) examples/<br/>Test demo yaz"] --> F
    F["Painter, Tessellator, RenderBatcher,<br/>RenderState DEĞİŞMEZ"]

    style F fill:#2da44e,color:#fff
```

Bu **Açık/Kapalı Prensibi**'nin somut karşılığıdır. Bu sayede IRenderer arayüzü
genişlemez ve mevcut backend'ler bu değişiklikten etkilenmez.

---

## 6. İlgili Mimari Karar Kayıtları (ADR)

| ADR | Karar | Bu dokümandaki ilgili bölüm |
|-----|-------|---------------------------|
| ADR-001 | OpenGL 3.3 Core (4.5/ES değil) | §2 |
| ADR-003 | `glLineWidth` yerine geometry quad | Tessellator (akislar.md) |
| ADR-004 | Tessellator backend-agnostic | §4 |
| ADR-005 | stb_image (SDL_image değil) | §2.4 |
| ADR-006 | Ear clipping triangulation | Tessellator (akislar.md) |

---

## 7. Sonraki Adımlar

- [Sınıf Diyagramı → Vulkan Bölümü](sinif-diyagrami.md#5-vulkan-backend-sınıfları)
- [Akış Diyagramları → Vulkan Frame](akislar.md#7-vulkan-frame-akışı-phase-5)
- [Yazılım Mühendisliği Perspektifi](sdl-painter-yazilim-muhendisligi.md)
