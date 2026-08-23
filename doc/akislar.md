# SDLPainter — Akış Diyagramları

Bu dokümanda SDLPainter'ın **çalışma zamanı davranışını** sequence ve flow
diyagramlarıyla anlatmaya çalıştım. Statik yapı için
[Sınıf Diyagramı](sinif-diyagrami.md), katman bağlamı için
[Mimari Genel Bakış](mimari-genel-bakis.md) dokümanlarına başvurabilirsiniz.

---

## 1. Frame Yaşam Döngüsü

Tipik bir oyun döngüsü iterasyonunda Painter ve renderer'ın izlediği akış:

```mermaid
sequenceDiagram
    autonumber
    participant App as Kullanıcı Uygulaması
    participant P as Painter
    participant B as RenderBatcher
    participant R as IRenderer
    participant GPU

    App->>P: Begin()
    P->>P: Pencere boyutu değişti mi?
    alt boyut değişti
        P->>R: SetViewport(0, 0, w, h)
        P->>P: UpdateProjection()
        P->>R: SetProjectionMatrix(mat4)
    end
    P->>R: BeginFrame()
    R-->>GPU: (Vulkan: AcquireNextImage)
    P->>P: FlushTransform()
    P->>R: SetModelMatrix(transform)

    Note over App,GPU: Çizim çağrıları (DrawRect, FillCircle, ...)
    loop her çizim çağrısı
        App->>P: DrawXxx(...) / FillXxx(...)
        P->>P: Tessellator → vertex listesi
        P->>B: PushTriangles(verts, color, opacity)
        opt mode/opacity/texture değişti veya buffer doldu
            B->>R: DrawTriangles(buffer)
            R-->>GPU: vertex upload + draw
        end
    end

    App->>P: End()
    P->>B: Flush() (kalan vertex'leri boşalt)
    B->>R: DrawTriangles(buffer)
    R-->>GPU: son draw
    P->>R: EndFrame()
    R-->>GPU: SwapBuffers / Present
```

**Önemli sözleşmeler**

- Çizim çağrıları **Begin/End arasında** olmalı; aksi halde tessellator
  vertex üretir ama batcher'da birikir, frame sınırı belirsiz kalır.
- `End()` **garantili `Flush()`** çağırır; uygulama kapanırken kayıp
  vertex olmaz.
- Pencere boyutu değişimi her `Begin()` başında kontrol edilir; manuel
  callback gerekmez.

---

## 2. Tek Bir `DrawRect` Çağrısının Detayı

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
    alt pen görünmez (alpha=0 veya width=0)
        P-->>App: erken return
    else pen görünür
        P->>P: FlushTransform()
        Note right of P: Sadece transform değiştiyse<br/>SetModelMatrix çağrılır
        P->>T: TessellateStrokedRect(x,y,w,h, line_width)
        T-->>P: vector~Vertex~ (4 quad / 8 üçgen)
        P->>B: PushTriangles(verts, pen.color, opacity)
        B->>B: mode == kBasic ? opacity aynı? size+verts ≤ 8192?
        alt evet (batch'e ekle)
            B->>B: mVertexBuffer'a renkleri yazıp ekle
        else hayır (state değişti / buffer dolu)
            B->>R: SetOpacity(prev_opacity)
            B->>R: DrawTriangles(önceki buffer)
            B->>B: buffer'ı temizle, mode = kBasic
            B->>B: yeni vertex'leri buffer'a ekle
        end
    end
```

**Kritik nokta:** Tessellator çağrıları **stateless** olduğu için her zaman
deterministiktir. Ölçüm ve test bu sayede GPU gerektirmez (mock IRenderer
ile).

---

## 3. Render Batcher Akışı

`RenderBatcher`'ın iç state machine'i ve `Flush()` tetikleme koşulları:

```mermaid
stateDiagram-v2
    [*] --> Bos: Constructor

    Bos: kNone (boş)
    Bos --> Basic: PushTriangles
    Bos --> Textured: PushTexturedTriangles

    Basic: kBasic
    Basic --> Basic: aynı opacity & buffer ≤ 8192
    Basic --> Basic: renk / transform DEĞİŞTİ (kırmaz)
    Basic --> FlushBasic: opacity DEĞİŞTİ
    Basic --> FlushBasic: buffer doldu (>8192)
    Basic --> FlushBasic_Tex: PushTexturedTriangles geldi

    Textured: kTextured + texture handle
    Textured --> Textured: aynı tex & opacity & buffer ≤ 8192
    Textured --> FlushTextured: texture DEĞİŞTİ
    Textured --> FlushTextured: opacity DEĞİŞTİ
    Textured --> FlushTextured: buffer doldu
    Textured --> FlushTex_Basic: PushTriangles geldi

    FlushBasic: renderer.DrawTriangles
    FlushBasic --> Basic
    FlushBasic_Tex --> Textured
    FlushTextured: renderer.DrawTextured
    FlushTextured --> Textured
    FlushTex_Basic --> Basic

    Basic --> [*]: Painter.End() → Flush
    Textured --> [*]: Painter.End() → Flush
```

### Flush Tetikleme Koşulları (Tablo)

| Geçiş | Sebep | Sonuç |
|-------|-------|-------|
| kBasic → kBasic | Aynı opacity, buffer < 8192 | **Flush YOK**, vertex eklenir |
| kBasic → kBasic | Opacity değişti | **Flush** + yeni batch başlar |
| kBasic → kBasic | Buffer >8192 vertex olur | **Flush** + yeni batch |
| kBasic → kTextured | Çizim modu değişti | **Flush** + textured batch başlar |
| kTextured → kTextured | Texture değişti | **Flush** + yeni texture batch |
| Painter.SetClipRect / ClearClip | Scissor GPU durumu değişti | **Flush** + yeni scissor |
| Painter.Restore() | Clip durumu **gerçekten** değiştiyse | **Flush** + scissor geri yüklenir |
| Painter.End() | Frame sonu | **Flush** (kalan ne varsa) |
| Painter.Clear() | Ekran temizleme | **Flush** önce, sonra Clear |

**Flush TETİKLEMEYENLER** (sık karıştırılır):

| İşlem | Neden batch'i kırmaz |
|-------|----------------------|
| `SetPen` / `SetBrush` | Renk `Vertex` içinde taşınır, uniform değil |
| `Translate` / `Rotate` / `Scale` / `ResetTransform` | Transform, `RenderBatcher` içinde CPU'da vertex'e uygulanır; model matrisi daima birim |
| `Save()` | Kaydedilen hiçbir şey GPU durumu değil |
| `Restore()` — clip aynıysa | Yalnızca clip değişimi flush gerektirir |
| `SetOpacity` — aynı değere | `SameOpacity()` toleransı (1/512) |

**Pratik etki:** 2000 farklı renkte, her biri kendi `Translate`/`Rotate`'i
olan `FillRect` → **2 draw call** (12 000 vertex, yalnızca 8192 tavanı
yüzünden ikiye bölünür).

Bu tablo tahmin değil, ölçüm sonucudur: senaryo bazlı draw call ve süre
verileri için bkz. **[`examples/benchmarks/README.md`](../examples/benchmarks/README.md)**.
Aynı sayaçlar çalışma zamanında `Painter::GetFrameStats()` ile, `Application`
tabanlı uygulamalarda ise **F1** ile açılan ekran üstü göstergede okunabilir.

![İstatistik göstergesi](images/stats-overlay.png)

---

## 4. Transform Stack — `Save` / `Restore` Mantığı

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
    P->>T: Translate(100, 50) — matris çarpımı
    Note over P: SetModelMatrix henüz<br/>çağrılmadı (lazy flush)

    App->>P: Rotate(45°)
    P->>T: Rotate(45)

    App->>P: DrawRect(...)
    P->>P: FlushTransform()
    P->>R: SetModelMatrix(transform.Data())
    Note right of R: Matris GPU'ya bir kez gönderildi
    P->>R: (Tessellator + Batcher)

    App->>P: DrawCircle(...)
    P->>P: FlushTransform() — fakat değişmediyse skip
    P->>R: (Batcher push)

    App->>P: Restore()
    P->>S: back() → mCurrentState
    P->>S: pop_back()
    Note over P: Sonraki çizimde<br/>FlushTransform tekrar tetikler
```

**RenderState ne taşır?** → `transform`, `pen`, `brush`, `opacity`,
`clip_rect`, `has_clip`. `Save()` bunların **tamamının kopyasını** alır;
`Restore()` hepsini geri yükler.

### İç İçe Save/Restore

```mermaid
flowchart TB
    A["state_0 (identity)"] -->|Save| B["stack: [s0]"]
    B -->|Translate 100,0| C["state_1 (T100)"]
    C -->|Save| D["stack: [s0, s1]"]
    D -->|Rotate 30| E["state_2 (T100·R30)"]
    E -->|DrawRect| F["çizim"]
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

`Image` lazy upload + cache stratejisi kullanır. İlk çağrıda yükler,
sonrakilerde mevcut handle'ı döner.

```mermaid
sequenceDiagram
    autonumber
    participant App
    participant Img as Image
    participant Tex as Texture (RAII)
    participant R as IRenderer
    participant GPU

    App->>Img: Image("sprite.png") (constructor)
    Img->>Img: stbi_load → mRawData (CPU bellek)

    App->>P: painter.DrawImage(img, x, y)
    P->>Img: Upload(*mRenderer)

    alt İlk kez (mHandle.Handle() == kInvalidTexture)
        Img->>R: CreateTexture(data, w, h, channels)
        R->>GPU: glTexImage2D / vkCreateImage + vkAllocateMemory + upload
        R-->>Img: TextureHandle (yeni)
        Img->>Tex: mHandle = Texture(renderer, handle)
    else Önbellekte mevcut
        Img-->>P: existing handle
    end

    P->>P: src_rect → UV koordinat dönüşümü
    P->>P: dest_rect → ekran koordinat dönüşümü
    P->>B: PushTexturedTriangles(verts, handle, white tint, opacity)
    B->>R: DrawTextured(buffer, handle) (Flush'ta)
    R->>GPU: bind texture + draw
```

### Image / Texture Yaşam Döngüsü

```mermaid
flowchart LR
    Created["Image oluşturuldu<br/>(CPU bellek dolu)"] -->|İlk Upload| Uploaded["Image + Texture<br/>(GPU bellek dolu)"]
    Uploaded -->|Sonraki Upload| Uploaded
    Uploaded -->|Image yok edilir| Destroyed["Texture::~Texture<br/>→ DestroyTexture"]
    Destroyed --> Done["GPU bellek serbest"]

    style Created fill:#1f6feb,color:#fff
    style Uploaded fill:#2da44e,color:#fff
    style Destroyed fill:#bf8700,color:#fff
    style Done fill:#6e7781,color:#fff
```

**RAII garantisi:** `Texture` sınıfı non-copyable, movable. Yıkıcı
`DestroyTexture` çağırır → texture sızıntısı imkansız.

---

## 6. Metin Çizimi (Phase 4) — Glyph Cache

```mermaid
sequenceDiagram
    autonumber
    participant App
    participant P as Painter
    participant F as Font
    participant SDL as SDL_ttf
    participant R as IRenderer

    App->>P: DrawText(x, y, "Merhaba")
    loop her UTF-8 karakter (codepoint)
        P->>F: GetGlyph(renderer, codepoint)
        alt cache hit
            F-->>P: const Glyph* (mevcut texture)
        else cache miss
            F->>SDL: TTF_RenderGlyph_Blended(font, codepoint)
            SDL-->>F: SDL_Surface (RGBA pixel buffer)
            F->>R: CreateTexture(surface.pixels, w, h, 4)
            R-->>F: TextureHandle
            F->>F: mGlyphCache[codepoint] = Glyph{texture, metrics...}
            F-->>P: yeni Glyph*
        end
        P->>P: pen rengi tint olarak, glyph.advance ile current_x ilerlet
        P->>B: PushTexturedTriangles(quad verts, glyph.texture.handle, tint, opacity)
    end
```

**Optimizasyon:** Glyph cache UTF-8 codepoint bazında çalışır. Türkçe ve
emoji karakterleri ilk render'da maliyet yaratır, sonra parasız döner.

---

## 7. Vulkan Frame Akışı (Phase 5)

OpenGL'de `BeginFrame`/`EndFrame` neredeyse trivialdir; Vulkan'da
swapchain yönetimi, sync nesneleri ve out-of-date kontrolü vardır.

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
        VR-->>P: tekrar dene
    else VK_SUCCESS
        VR->>VR: mCurrentImageIndex = acquired
        VR->>FS: vkBeginCommandBuffer(commandBuffer)
        VR->>VR: vkCmdBeginRenderPass(framebuffers[imageIndex])
    end

    Note over P,Q: Çizim komutları (DrawTriangles → vkCmdDraw)
    P->>VR: DrawTriangles(verts) (Batcher üzerinden)
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

**Out-of-date handling:** Pencere yeniden boyutlandığında swapchain
geçersizleşir. `AcquireNextImage` veya `QueuePresent` `OUT_OF_DATE` döner;
`VulkanRenderer` swapchain'i `Recreate` ile yeniden oluşturur. Painter
bu detaydan haberdar olmaz.

---

## 8. Backend Seçimi (Factory)

```mermaid
flowchart TB
    A["Painter constructor:<br/>Painter(window, RendererBackend)"] --> B["CreateRenderer(backend)"]
    B --> C{"backend?"}
    C -->|kOpenGL| D["new OpenGLRenderer"]
    C -->|kVulkan| E{"SDLPAINTER_HAS_VULKAN<br/>compile flag?"}
    E -->|var| F["new VulkanRenderer"]
    E -->|yok| G["nullptr → Painter.IsValid() false"]
    D --> H["unique_ptr~IRenderer~"]
    F --> H
    G --> I["Hata logu + early return"]
    H --> J["renderer->Initialize(window)"]
    J --> K{"başarılı?"}
    K -->|evet| L["Painter hazır"]
    K -->|hayır| M["renderer.reset() — IsValid() false"]

    style D fill:#cf222e,color:#fff
    style F fill:#cf222e,color:#fff
    style L fill:#2da44e,color:#fff
    style I fill:#bf8700,color:#fff
    style M fill:#bf8700,color:#fff
```

**Sözleşme:** Painter constructor başarısız olabilir; **kullanıcı
`painter.IsValid()` ile kontrol etmek zorundadır**. Exception fırlatılmaz
(C++ tarafında deterministik hata için).

---

## 9. Tessellator İç Akışları

### 9.1 Adaptif Daire Segment Sayısı

```mermaid
flowchart LR
    A["TessellateFilledCircle(cx, cy, r)"] --> B["segments = max(16, int(r * 0.5))"]
    B --> C["Triangle fan üret:<br/>merkez + N+1 çevre noktası"]
    C --> D["3 vertex × N üçgen"]

    style A fill:#1f6feb,color:#fff
    style D fill:#2da44e,color:#fff
```

| Yarıçap | Segment | Üçgen | Vertex |
|---------|---------|-------|--------|
| 8 px    | 16      | 16    | 48     |
| 50 px   | 25      | 25    | 75     |
| 200 px  | 100     | 100   | 300    |
| 1000 px | 500     | 500   | 1500   |

### 9.2 Konkav Polygon — Ear Clipping

```mermaid
flowchart TB
    A["TessellateFilledPolygon(points)"] --> B{"Konveks mi?"}
    B -->|evet| C["Triangle fan O(n)"]
    B -->|hayır| D["EarClipping O(n²)"]
    D --> E["1. Linked list olarak tut"]
    E --> F["2. Bir 'ear' bul:<br/>Üçgen, içinde başka nokta yok"]
    F --> G["3. Ear'ı çıkar, üçgen kaydet"]
    G --> H{"Nokta kaldı mı?"}
    H -->|evet| F
    H -->|hayır| I["Triangulation tamam"]
    C --> J["vector~Vertex~"]
    I --> J

    style B fill:#bf8700,color:#fff
    style D fill:#cf222e,color:#fff
    style J fill:#2da44e,color:#fff
```

### 9.3 Kalın Çizgi (`glLineWidth` yok)

```mermaid
flowchart LR
    A["TessellateThickLine(A, B, w)"] --> B["dir = normalize(B-A)"]
    B --> C["normal = (-dir.y, dir.x)"]
    C --> D["half = w/2 · normal"]
    D --> E["A1 = A + half<br/>A2 = A − half<br/>B1 = B + half<br/>B2 = B − half"]
    E --> F["2 üçgen:<br/>(A1, A2, B1) + (A2, B2, B1)"]
    F --> G["6 vertex"]

    style A fill:#1f6feb,color:#fff
    style G fill:#2da44e,color:#fff
```

Ortografik projeksiyon altında bu yaklaşım her platformda piksel-tutarlı
sonuç verir; `glLineWidth` ise sürücüye göre değişen üst sınıra sahiptir.

---

## 10. Hatalı Kullanım Senaryoları

| Hata | Etki | Önlem |
|------|------|-------|
| `Begin()` çağırmadan `DrawRect` | Buffer'da birikir, frame sınırı bozulur | API'de assertion (geliştirme) |
| `End()` çağırmadan kapatma | Son batch flush olmaz, son frame bozuk | Painter destructor → Shutdown |
| `Pen({0,0,0,0}, 5.0f)` | `IsVisible()` false → çizim atlanır | Erken return, GPU yükü yok |
| `painter.IsValid()` kontrol etmemek | Renderer init başarısız olduysa null deref | Demo'larda zorunlu kontrol var |
| Aynı `Image`'i çoklu Painter'da `Upload` | Her renderer için ayrı texture | Image bir renderer'a bağlanmalı |
| `Save` sonrası `Restore` unutulmak | `mStateStack` sızar | RAII guard helper önerilir (v2) |
