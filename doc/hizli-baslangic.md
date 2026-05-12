# SDLPainter — Hızlı Başlangıç

Bu rehber SDLPainter'ı **ilk defa kullananlar** içindir. Beş dakikada
çalışan bir pencere ve birkaç şekil çizdirmek hedef. Kütüphanenin
mimari arka planı için [Mimari Genel Bakış](mimari-genel-bakis.md).

---

## 1. Önkoşullar

| Araç | Minimum Sürüm | Notlar |
|------|---------------|--------|
| C++ derleyicisi | C++17 destekleyen GCC 11+, Clang 14+, MSVC 2022 | |
| CMake | 3.20 | Presets desteği için |
| Conan | 2.x | `pip install conan` |
| Git | — | repoyu klonlamak için |
| OpenGL sürücüsü | 3.3 Core | Çoğu sistemde mevcut |
| Vulkan loader | 1.1+ runtime (opsiyonel) | Hedef API 1.1; Conan paketi `vulkan-loader/1.3.290` (geri uyumlu) |

---

## 2. Kurulum

```bash
# 1) Klonla
git clone https://example.com/sdl-painter.git
cd sdl-painter

# 2) Bağımlılıkları yükle (debug build için)
conan install . --build=missing -s build_type=Debug

# 3) Configure + build
cmake --preset conan-debug
cmake --build --preset conan-debug

# 4) Demo'yu çalıştır
./build/Debug/examples/phase1_demo
```

> Windows / MSVC için preset adları `windows-debug` ve `windows-release`
> şeklindedir. Cross-compile için Dockerfile içindeki `windows-cross`
> stage'i kullanılabilir.

---

## 3. İlk Uygulama — "Hello, Rectangle"

Yeni bir CMake projesi varsayalım. Aşağıdaki dosyayı `main.cpp` olarak
kaydedin:

```cpp
#include <SDL3/SDL.h>
#include "sdl_painter/painter.h"
#include "sdl_painter/pen.h"
#include "sdl_painter/brush.h"

int main() {
  SDL_Init(SDL_INIT_VIDEO);

  // OpenGL 3.3 Core context için ipuçları
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,
                      SDL_GL_CONTEXT_PROFILE_CORE);

  SDL_Window* window = SDL_CreateWindow(
      "SDLPainter Hello", 800, 600,
      SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);

  // Painter'ı bir scope'a al → window'dan ÖNCE yıkılsın
  {
    sdl_painter::Painter painter(window,
                                  sdl_painter::RendererBackend::kOpenGL);
    if (!painter.IsValid()) {
      SDL_DestroyWindow(window);
      SDL_Quit();
      return 1;
    }

    bool running = true;
    while (running) {
      SDL_Event event;
      while (SDL_PollEvent(&event)) {
        if (event.type == SDL_EVENT_QUIT) running = false;
      }

      painter.Begin();
      painter.Clear({30, 30, 40, 255});

      painter.SetBrush(sdl_painter::Brush({80, 160, 220, 255}));
      painter.FillRect(100.0f, 100.0f, 200.0f, 150.0f);

      painter.SetPen(sdl_painter::Pen({255, 200, 50, 255}, 3.0f));
      painter.DrawCircle(500.0f, 300.0f, 80.0f);

      painter.End();
    }
  }

  SDL_DestroyWindow(window);
  SDL_Quit();
  return 0;
}
```

### Önemli Detaylar

| Satır | Neden | Atlanırsa |
|-------|-------|-----------|
| `SDL_GL_SetAttribute(...CORE)` | OpenGL 3.3 Core profile şart | Compatibility profile'da çalışabilir ama undefined |
| `Painter` scope içinde | GL context window'a bağlı | `glDelete*` çağrısı 1282 hatası verir |
| `painter.IsValid()` | Constructor sessizce başarısız olabilir | Sonraki çağrılarda crash |
| `painter.Begin()` / `painter.End()` | Frame sınırı | Çizimler ekrana yansımaz |

---

## 4. Temel API Özeti

### 4.1 Stil Ayarları

```cpp
painter.SetPen(sdl_painter::Pen(color, width));   // çerçeve / çizgi
painter.SetBrush(sdl_painter::Brush(color));      // dolgu
painter.SetOpacity(0.5f);                         // global alpha [0,1]
```

`Pen::NoPen()` ve `Brush::NoBrush()` görünmez varyantlardır; o anki
çizimler atlanır (GPU yüküne girmez).

### 4.2 Çizim Primitifleri

| Stroke (çerçeve) | Fill (dolgu) |
|------------------|--------------|
| `DrawLine(x1, y1, x2, y2)` | — |
| `DrawRect(x, y, w, h)` | `FillRect(x, y, w, h)` |
| `DrawCircle(cx, cy, r)` | `FillCircle(cx, cy, r)` |
| `DrawEllipse(cx, cy, rx, ry)` | `FillEllipse(cx, cy, rx, ry)` |
| `DrawPolygon(points)` | `FillPolygon(points)` |
| `DrawPolyline(points)` | — |

### 4.3 Transform Stack

```cpp
painter.Save();
painter.Translate(400.0f, 300.0f);
painter.Rotate(45.0f);                   // derece
painter.Scale(1.5f, 1.5f);
painter.FillRect(-50.0f, -50.0f, 100.0f, 100.0f);
painter.Restore();                       // önceki state geri yüklenir
```

`Save`/`Restore` sayısı dengeli olmalı. QPainter ile birebir aynı semantik.

### 4.4 Image (Phase 3)

```cpp
sdl_painter::Image img("assets/sprite.png");
if (img.IsValid()) {
  painter.DrawImage(img, 100.0f, 50.0f);            // orijinal boyut
  painter.DrawImage(img, sdl_painter::Rect{200, 50, 64, 64});  // ölçekli
}
```

İlk çizimde GPU'ya yüklenir, sonraki çizimlerde cache kullanılır.

### 4.5 Metin (Phase 4)

```cpp
auto font = std::make_shared<sdl_painter::Font>("assets/font.ttf", 24);
painter.SetFont(font);
painter.SetPen(sdl_painter::Pen({255, 255, 255, 255}));  // pen rengi tint
painter.DrawText(50.0f, 100.0f, "Merhaba Dünya!");

painter.DrawText(sdl_painter::Rect{0, 200, 800, 50},
                 "Ortalı metin",
                 sdl_painter::Alignment::kCenter);
```

### 4.6 Clipping

```cpp
painter.SetClipRect({100, 100, 400, 300});
// Sadece bu dikdörtgen içine çizim görünür
painter.FillCircle(500, 250, 200);  // bir kısmı kırpılır
painter.ClearClip();
```

Scissor tabanlı, eksen-hizalı dikdörtgen. Path-based clip yok (v1 dışı).

---

## 5. Vulkan Backend'e Geçiş

```cpp
// OpenGL window flag'i yerine Vulkan flag'i:
SDL_Window* window = SDL_CreateWindow(
    "Vulkan Demo", 800, 600,
    SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);

sdl_painter::Painter painter(window,
                              sdl_painter::RendererBackend::kVulkan);
```

> Vulkan backend için kütüphane `with_vulkan=True` ile build edilmiş
> olmalı: `conan install . -o sdl_painter/*:with_vulkan=True`.

Diğer hiçbir kullanıcı kodu değişmez. Aynı `painter.FillRect`,
`painter.DrawText`, `painter.Save` çağrıları çalışır. Bu **IRenderer
soyutlamasının somut karşılığıdır**.

---

## 6. Faz Bazlı Demo Uygulamaları

`examples/` dizini her özellik için bağımsız bir demo içerir. Yeni
başlayanlar için önerilen okuma sırası:

| Demo | Konu | İlgili API |
|------|------|------------|
| `phase1_demo.cpp` | Tüm temel primitifler | `DrawRect`, `FillCircle`, `DrawPolyline` |
| `phase2_demo.cpp` | Transform stack | `Save`/`Restore`, `Translate`/`Rotate`/`Scale` |
| `phase2b_demo.cpp` | Clipping | `SetClipRect` / `ClearClip` |
| `phase3_demo.cpp` | Image / texture | `Image`, `DrawImage` |
| `phase4_demo.cpp` | Metin | `Font`, `DrawText`, `Alignment` |
| `phase5a_vulkan_clear.cpp` | Vulkan: pencereyi temizle | `RendererBackend::kVulkan` |
| `phase5b_vulkan_triangles.cpp` | Vulkan: ilk üçgenler | — |
| `phase5c_vulkan_textured.cpp` | Vulkan: image | — |
| `phase5d_vulkan_demo.cpp` | Vulkan: tüm primitifler | — |
| `phase5e_vulkan_text.cpp` | Vulkan: metin | — |

---

## 7. Sık Karşılaşılan Sorunlar

### 7.1 `painter.IsValid()` false dönüyor

- OpenGL: window flag'inde `SDL_WINDOW_OPENGL` var mı?
- OpenGL: 3.3 Core context attribute'leri set edildi mi?
- Vulkan: window flag'inde `SDL_WINDOW_VULKAN` var mı? Kütüphane
  `with_vulkan=True` ile build edildi mi?

### 7.2 Çizimler ekranda görünmüyor

- `painter.Begin()` ve `painter.End()` arasında mı?
- `Pen` veya `Brush` `Transparent` mi (alpha=0)?
- Y koordinatı ekran dışında mı? (Y=0 üstte, aşağı pozitif)
- Transform içinde miyiz? `Save`/`Restore` dengesi bozulmuş olabilir.

### 7.3 OpenGL 1282 hatası uygulama kapanırken

- `Painter` instance, `SDL_DestroyWindow`'dan **önce** yok edilmeli.
  En basit yöntem: Painter'ı bir scope (kıvrımlı blok) içine alın.

### 7.4 Performans düşük

- Her draw call'dan önce `SetPen`/`SetBrush` değiştirmeyin (renk değişimi
  flush tetiklemez ama opacity değişimi tetikler).
- Metin için `Font::GetGlyph` ilk çağrıda maliyetlidir; sonra cache
  parasız.
- Aynı texture'lı sprite'ları arka arkaya çizmek tek draw call'da çıkar.

---

## 8. Sonraki Adımlar

- 📐 [Sınıf Diyagramı](sinif-diyagrami.md) — API'nin tam yapısı
- 🔄 [Akış Diyagramları](akislar.md) — Frame yaşam döngüsü, batch flush, transform stack
- 📜 [Özellik Listesi](sdl-painter-ozellikler.md) — Desteklenen tüm primitifler
- 💡 [Örnekler](sdl-painter-ornekler.md) — Daha fazla kullanım örneği
- 🏗️ [Yazılım Mühendisliği Perspektifi](sdl-painter-yazilim-muhendisligi.md) — Tasarım kararlarının gerekçeleri
