**Türkçe** | [English version](architecture.md)

# SDLPainter — Mimari Genel Bakış

Bu doküman SDLPainter'ın **katmanlı mimarisini**, **bileşenler arasındaki bağımlılıkları**
ve **veri akışını** üst seviyeden tarif eder. Sınıf detayları için
[Sınıf Diyagramı](sinif-diyagrami.md), çalışma zamanı akışları için
[Akış Diyagramları](akislar.md) belgelerine bakabilirsiniz.

---

## 1. Katmanlı Mimari

SDLPainter dört bağımsız katmandan oluşur. Her katman yalnızca bir altındaki
katmanın **arayüzüne** bağımlıdır; somut implementasyona değil.

```mermaid
flowchart TB
    subgraph user["Kullanıcı Uygulaması"]
        APP["Demo / Editor / Game"]
    end

    subgraph api["1) Public API Katmanı"]
        PAINTER["Painter<br/>"]
        STYLE["Pen / Brush / Color<br/>Image / Font"]
    end

    subgraph mid["2) Geometri Katmanı (backend-agnostik)"]
        TESS["Tessellator<br/>(şekil → vertex)"]
        STATE["RenderState<br/>(transform stack elemanı)"]
        BATCH["RenderBatcher<br/>(draw call coalescing)"]
    end

    subgraph backend["3) Backend Soyutlama"]
        IRENDER["IRenderer<br/>(saf arayüz)"]
    end

    subgraph impl["4) Backend İmplementasyonları"]
        OGL["OpenGLRenderer<br/>(GLAD + GLSL 330)"]
        VK["VulkanRenderer<br/>(Vulkan 1.1 + SPIR-V)"]
    end

    subgraph plat["Platform Katmanı"]
        SDL["SDL3<br/>(pencere, context, input)"]
        GPU["GPU Sürücüsü"]
    end

    APP --> PAINTER
    PAINTER --> STYLE
    PAINTER --> TESS
    PAINTER --> STATE
    PAINTER --> BATCH
    BATCH --> IRENDER
    TESS -.üretir.-> BATCH
    OGL -.uygular.-> IRENDER
    VK -.uygular.-> IRENDER
    OGL --> SDL
    VK --> SDL
    OGL --> GPU
    VK --> GPU

    classDef apiCls fill:#1f6feb,stroke:#0d3a8c,color:#fff
    classDef midCls fill:#2da44e,stroke:#1a6b30,color:#fff
    classDef backCls fill:#bf8700,stroke:#7a5a00,color:#fff
    classDef implCls fill:#cf222e,stroke:#7a0a14,color:#fff
    classDef platCls fill:#6e7781,stroke:#3a3f44,color:#fff

    class PAINTER,STYLE apiCls
    class TESS,STATE,BATCH midCls
    class IRENDER backCls
    class OGL,VK implCls
    class SDL,GPU platCls
```

### Katmanların Sorumlulukları

| Katman | Sorumluluk | Neyi BİLMEZ |
|--------|-----------|-------------|
| **Public API** | Kullanıcı API'sı, stil ve transform yönetimi | GPU, vertex formatı, batch detayı |
| **Geometri** | Şekilleri vertex'e çevirir, draw call'ları biriktirir | OpenGL/Vulkan komutları |
| **Backend Soyutlama** | Renderer arayüzü (sözleşme) | İmplementasyon detayı |
| **Backend İmpl.** | GPU komutları, shader, buffer, texture | Tessellation, stil, transform stack |

Bu ayrımın somut etkisi: `OpenGLRenderer`'ı `VulkanRenderer` ile değiştirmek
**sadece constructor argümanını değiştirmek**ten ibarettir. `Painter`,
`Tessellator`, `RenderBatcher`, `RenderState` dosyalarında tek satır
değişiklik yapılmaz.

---

## 2. Bileşen Bağımlılıkları

Her bileşenin başka hangi bileşenleri kullandığını gösteren bağımlılık grafiği:

```mermaid
graph LR
    Painter --> RenderState
    Painter --> Tessellator
    Painter --> RenderBatcher
    Painter --> IRenderer
    Painter --> Image
    Painter --> Font

    RenderState --> Mat3["glm::mat3 (transform)"]
    RenderState --> Pen
    RenderState --> Brush
    RenderState --> Rect

    Tessellator --> Vertex
    Tessellator --> TexturedVertex
    Tessellator --> Geometry["Point / Rect"]

    RenderBatcher --> IRenderer
    RenderBatcher --> Vertex
    RenderBatcher --> TexturedVertex

    Image --> stbImage["stb_image"]
    Image --> Texture["Texture (RAII)"]
    Texture --> IRenderer

    Font --> SDLttf["SDL_ttf"]
    Font --> Glyph
    Glyph --> Texture

    OpenGLRenderer -.implements.-> IRenderer
    VulkanRenderer -.implements.-> IRenderer
    OpenGLRenderer --> ShaderProgram
    OpenGLRenderer --> SDL3
    VulkanRenderer --> VkContext
    VulkanRenderer --> VkSwapchain
    VulkanRenderer --> VkFrameSync
    VulkanRenderer --> VulkanPipeline
    VulkanRenderer --> VulkanTexturedPipeline
    VulkanRenderer --> SDL3

    style IRenderer fill:#bf8700,color:#fff
    style Painter fill:#1f6feb,color:#fff
    style OpenGLRenderer fill:#cf222e,color:#fff
    style VulkanRenderer fill:#cf222e,color:#fff
```

> Kesintisiz oklar **doğrudan kullanım**, kesik oklar **arayüz uygulaması**
> (`implements`) ilişkisini gösterir.

---

## 3. Veri Akışı: Tek Bir `DrawRect` Çağrısı

`painter.DrawRect(x, y, w, h)` çağrısının kütüphane içinde izlediği yol:

```mermaid
flowchart LR
    A["Kullanıcı:<br/>painter.DrawRect(...)"]
    B["RenderState'i oku<br/>(pen, opacity, transform)"]
    C["Tessellator::<br/>TessellateStrokedRect"]
    D["std::vector&lt;Vertex&gt;<br/>(4 quad / 8 üçgen)"]
    E["RenderBatcher::<br/>PushTriangles"]
    F{"Mode/opacity<br/>değişti mi?<br/>VEYA buffer<br/>doldu mu?"}
    G["Flush():<br/>renderer.DrawTriangles"]
    H["Buffer'a ekle<br/>(GPU çağrısı YOK)"]
    I["OpenGL/Vulkan<br/>vertex upload + draw"]

    A --> B --> C --> D --> E --> F
    F -- "Hayır" --> H
    F -- "Evet" --> G --> I
    H -.batch sonra.-> G

    style A fill:#1f6feb,color:#fff
    style I fill:#cf222e,color:#fff
    style F fill:#bf8700,color:#fff
```

**Önemli not:** Tipik bir frame'de birçok `DrawRect`/`DrawCircle` çağrısı
aynı pen ve opacity ile gelir. Bu durumda `Flush()` tetiklenmez; tüm
vertex'ler **tek bir GPU draw call**'ında gönderilir. Detaylar için
[akislar.md → Batch Flush Koşulları](akislar.md#3-render-batcher-akışı).

---

## 4. Modül Haritası — Kaynak Ağacı

```
sdl-painter/
├── include/sdl_painter/        ← Public API (kullanıcı bu header'ları görür)
│   ├── painter.h               · Ana çizim sınıfı
│   ├── pen.h, brush.h, color.h · Stil tipleri
│   ├── geometry.h              · Point, Rect, Size
│   ├── image.h, texture.h      · Görsel + RAII texture sarmalayıcı
│   ├── font.h                  · SDL_ttf wrapper, Alignment
│   ├── renderer.h              · IRenderer arayüzü + RendererBackend enum
│   └── vertex.h                · Vertex, TexturedVertex
│
├── src/                        ← İmplementasyon (kullanıcıya kapalı)
│   ├── painter.cpp             · API ↔ batcher/tessellator köprüsü
│   ├── render_batcher.{h,cpp}  · Draw call coalescing
│   ├── tessellator.{h,cpp}     · Şekil → vertex
│   ├── renderer.cpp            · CreateRenderer factory
│   ├── opengl/                 · OpenGL 3.3 backend
│   │   ├── opengl_renderer.{h,cpp}
│   │   ├── shader_program.{h,cpp}
│   │   └── shaders/*.{vert,frag}
│   └── vulkan/                 · Vulkan 1.1 backend
│       ├── vulkan_renderer.{h,cpp}     · IRenderer impl
│       ├── vk_context.{h,cpp}          · Instance, device, queue
│       ├── vk_swapchain.{h,cpp}        · Swapchain + image view
│       ├── vk_frame_sync.{h,cpp}       · Semaphore, fence
│       ├── vk_memory.{h,cpp}           · Buffer/image bellek
│       ├── vulkan_pipeline.{h,cpp}     · Untextured pipeline
│       ├── vulkan_textured_pipeline.{h,cpp}
│       ├── vulkan_buffer.{h,cpp}       · Vertex ring buffer
│       ├── vulkan_texture.{h,cpp}      · Texture + descriptor set
│       └── shaders/*.spv               · SPIR-V derlenmiş shader'lar
│
├── examples/                   ← Faz bazlı demo uygulamaları
│   ├── primitives.cpp          · Primitifler
│   ├── transforms.cpp          · Transform stack
│   ├── images.cpp              · Image
│   ├── text.cpp                · Text
│   └── vulkan_*.cpp            · Vulkan örnekleri
│
└── tests/                      ← GTest birim testleri
    ├── mock_renderer.h         · IRenderer mock'u
    ├── test_tessellator.cpp
    ├── test_render_batcher.cpp
    ├── test_transform.cpp
    └── ...
```

### Public ve Private Ayırımı

- `include/sdl_painter/*.h` → **Kullanıcıya açık**. ABI/API stabilite hedefi var.
- `src/*.h` (`tessellator.h`, `render_batcher.h`) → **İç implementasyon**.
  Kullanıcı bu header'ları include etmemeli.
- `src/opengl/*.h`, `src/vulkan/*.h` → Backend'e özel iç tipler.

---

## 5. Bağımlılık Yönetimi (Conan 2)

```mermaid
graph TB
    Painter["sdl_painter"]

    subgraph zorunlu["Zorunlu — her zaman çekilir"]
        SDL3["SDL3 (sdl/3.2.x)"]
        GLAD["GLAD (glad/0.1.x)"]
        STB["stb_image (stb/cci.*)"]
        GLM["GLM (transform matrisi)"]
        TTF["SDL_ttf (sdl_ttf/3.2.x)"]
    end

    subgraph ops["Opsiyonel — option ile etkinleştirilir"]
        VK["Vulkan loader/headers (with_vulkan)"]
        GTEST["GTest (build_tests)"]
    end

    Painter --> SDL3
    Painter --> GLAD
    Painter --> STB
    Painter --> GLM
    Painter --> TTF
    Painter -.opt.-> VK
    Painter -.opt.-> GTEST

    style zorunlu fill:#2da44e20,stroke:#2da44e
    style ops fill:#bf870020,stroke:#bf8700
```

`with_vulkan=False` olduğunda Vulkan loader hiç indirilmez; CI süresi ve
container imaj boyutu küçük kalır. Detaylar için `conanfile.py` ve
[Derleme](building.md) *(İngilizce)*.

---

## 6. Sözleşmeler ve Değişmez Kurallar (Invariants)

Mimarinin tutarlı kalması için bileşenler arasında uyulan kurallar:

| Sözleşme | Açıklama |
|----------|----------|
| **Tessellator stateless** | Aynı input → aynı output, her zaman. GPU bağımlılığı yok. |
| **IRenderer salt arayüz** | Hiçbir state (transform, opacity stack) tutmaz; sadece komut alır. |
| **Painter, IRenderer'a sahiptir** | `unique_ptr<IRenderer>`; yaşam döngüsü Painter'a bağlı. |
| **RenderBatcher Painter'a aittir** | Painter dışında erişilemez; arayüzü internal. |
| **Texture handle opak** | `uint32_t`; backend bunu kendi haritasında çözer. |
| **Y ekseni Painter'da yukarı pozitif değil** | Painter Y=0 üstte; OpenGL scissor'unda flip ApplyScissor'da uygulanır. |
| **Frame sınırı: Begin/End** | İki çağrı arasında transform/state biriktirilir, End'te flush. |

Bu değişmezler bozulduğunda mimari avantajları (test edilebilirlik,
backend değiştirilebilirliği, performans öngörülebilirliği) kaybolur.

---

## 7. Sonraki Adımlar

- [Sınıf Diyagramı](sinif-diyagrami.md) — UML class diagram, ilişki türleri
- [Akış Diyagramları](akislar.md) — Frame, draw call, transform stack, texture upload sequence'leri
- [Backend İç Yapısı](backend-ic-yapisi.md) — OpenGL ve Vulkan implementasyon detayı
- [Hızlı Başlangıç](hizli-baslangic.md) — İlk Painter uygulaması
- [Yazılım Mühendisliği Perspektifi](sdl-painter-yazilim-muhendisligi.md) — Tasarım kararlarının gerekçeleri
