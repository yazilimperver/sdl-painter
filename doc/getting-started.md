[Türkçe sürüm](hizli-baslangic.md) | **English**

# SDLPainter — Getting Started

This guide is for people using SDLPainter **for the first time**. The goal is to provide a
working window with a few shapes on it in five minutes. For the reasoning behind
the design, see the [Architecture Overview](architecture.md) please.

---

## 1. Prerequisites

| Tool | Minimum version | Notes |
|------|-----------------|-------|
| C++ compiler | GCC 11+, Clang 14+ or MSVC 2022 with C++17 | |
| CMake | 3.21 | `PROJECT_IS_TOP_LEVEL`, `$<TARGET_RUNTIME_DLLS>`, presets v3 |
| Conan | 2.x | `pip install conan` |
| Git | — | to clone the repository |
| OpenGL driver | 3.3 Core | present on most systems |
| Vulkan loader | 1.1+ runtime (optional) | target API is 1.1; the Conan package is `vulkan-loader/1.3.290` (backward compatible) |

---

## 2. Setup

```bash
# 1) Clone
git clone https://github.com/yazilimperver/sdl-painter.git
cd sdl-painter

# 2) Install dependencies (for a debug build)
conan install . --output-folder=build/linux-debug/generators \
    --build=missing -s build_type=Debug

# 3) Configure + build
cmake --preset linux-debug
cmake --build --preset linux-debug

# 4) Run a demo
./build/linux-debug/examples/primitives
```

> The preset name **must** match `--output-folder`: `CMakePresets.json` looks
> for each preset's toolchain under `build/<preset>/generators/...`.
>
> On Windows/MSVC the presets are `windows-debug` and `windows-release`
> (`--output-folder=build/windows-debug/generators`). For cross-compilation use
> the `windows-cross` stage in the Dockerfile together with the
> `windows-mingw-debug` / `windows-mingw-release` presets.

### 2.1 Windows (Visual Studio 2022) — manual

```powershell
# 0. Load the VS 2022 environment
$vsInstallPath = & "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" -latest -property installationPath
Import-Module "$vsInstallPath\Common7\Tools\Microsoft.VisualStudio.DevShell.dll"
Enter-VsDevShell -VsInstallPath $vsInstallPath -DevCmdArguments "-arch=x64"

# 1. Create a Conan profile (first time only)
conan profile detect

# 2. Install dependencies
conan install . --output-folder=build/windows-debug/generators --build=missing -s build_type=Debug

# 3. Build
cmake --preset windows-debug
cmake --build --preset windows-debug
```

### 2.2 Building with the scripts

Run every script from the project root. Full flag list:
[Script Reference](scripts.md).

```bash
chmod +x scripts/*.sh

./scripts/build.sh              # Build (Debug, default)
./scripts/build.sh Release      # Release build
./scripts/build.sh --docs       # Build + API documentation
./scripts/run-tests.sh          # Run the tests
./scripts/format-check.sh       # Format check
```

```powershell
.\scripts\Build.ps1             # Build (Debug, default)
.\scripts\Build.ps1 Release     # Release build
.\scripts\Build.ps1 -Docs       # Build + API documentation
.\scripts\Run-Tests.ps1         # Run the tests
.\scripts\Format-Check.ps1      # Format check
```

### 2.3 CMake preset reference

| Preset | Platform | Build type | Notes |
|--------|----------|------------|-------|
| `linux-debug` | Linux | Debug | used also in CI |
| `linux-release` | Linux | Release | used also in CI |
| `linux-debug-asan` | Linux | Debug | ASan + UBSan enabled |
| `windows-debug` | Windows | Debug | MSVC, Visual Studio 17 2022 |
| `windows-release` | Windows | Release | MSVC, Visual Studio 17 2022 |
| `windows-mingw-debug` | Windows (cross) | Debug | MinGW-w64 from a Linux host, no Vulkan |
| `windows-mingw-release` | Windows (cross) | Release | MinGW-w64 from a Linux host, no Vulkan |

### 2.4 CMake options

| Option | Default | Effect |
|--------|---------|--------|
| `SDLPAINTER_WITH_VULKAN` | `OFF` | Vulkan backend |
| `SDLPAINTER_BUILD_EXAMPLES` | `ON` | Example applications |
| `SDLPAINTER_BUILD_TESTS` | `ON` | GTest unit tests |
| `ENABLE_SANITIZERS` | `OFF` | ASan + UBSan (GCC/Clang) |

Building with everything, Vulkan included:

```bash
conan install . --output-folder=build/linux-debug/generators --build=missing \
    -s build_type=Debug -o "&:with_vulkan=True"
cmake --preset linux-debug
cmake --build --preset linux-debug
```

---

## 3. Your first application — "Hello, Rectangle"

To develop a minimal application, please save the following as `main.cpp`:

```cpp
#include <SDL3/SDL.h>
#include "sdl_painter/painter.h"
#include "sdl_painter/pen.h"
#include "sdl_painter/brush.h"

int main() {
  SDL_Init(SDL_INIT_VIDEO);

  // Hints for an OpenGL 3.3 Core context
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,
                      SDL_GL_CONTEXT_PROFILE_CORE);

  SDL_Window* window = SDL_CreateWindow(
      "SDLPainter Hello", 800, 600,
      SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);

  // Scope the Painter → it must be destroyed BEFORE the window
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

### The details that matter

| Line | Why | If you skip it |
|------|-----|----------------|
| `SDL_GL_SetAttribute(...CORE)` | OpenGL 3.3 Core profile is required | may work on a compatibility profile, but it is undefined |
| `Painter` inside a scope | the GL context belongs to the window | `glDelete*` fails with error 1282 |
| `painter.IsValid()` | the constructor can fail silently | later calls crash |
| `painter.Begin()` / `painter.End()` | frame boundary | nothing reaches the screen |

---

## 4. API in brief

### 4.1 Style

```cpp
painter.SetPen(sdl_painter::Pen(color, width));   // outline / line
painter.SetBrush(sdl_painter::Brush(color));      // fill
painter.SetOpacity(0.5f);                         // global alpha [0,1]
```

`Pen::NoPen()` and `Brush::NoBrush()` are the invisible variants; the affected
draws are skipped entirely, so they cost no GPU work.

### 4.2 Primitives

| Stroke | Fill |
|--------|------|
| `DrawLine(x1, y1, x2, y2)` | — |
| `DrawRect(x, y, w, h)` | `FillRect(x, y, w, h)` |
| `DrawCircle(cx, cy, r)` | `FillCircle(cx, cy, r)` |
| `DrawEllipse(cx, cy, rx, ry)` | `FillEllipse(cx, cy, rx, ry)` |
| `DrawPolygon(points)` | `FillPolygon(points)` |
| `DrawPolyline(points)` | — |

### 4.3 Transform stack

```cpp
painter.Save();
painter.Translate(400.0f, 300.0f);
painter.Rotate(45.0f);                   // degrees
painter.Scale(1.5f, 1.5f);
painter.FillRect(-50.0f, -50.0f, 100.0f, 100.0f);
painter.Restore();                       // previous state is restored
```

`Save` and `Restore` must be balanced.

### 4.4 Images

```cpp
sdl_painter::Image img("assets/sprite.png");
if (img.IsValid()) {
  painter.DrawImage(img, 100.0f, 50.0f);                       // original size
  painter.DrawImage(img, sdl_painter::Rect{200, 50, 64, 64});  // scaled
}
```

The image is uploaded to the GPU on first draw; later draws reuse the cache.

### 4.5 Text

```cpp
auto font = std::make_shared<sdl_painter::Font>("assets/font.ttf", 24);
painter.SetFont(font);
painter.SetPen(sdl_painter::Pen({255, 255, 255, 255}));  // the pen colour tints the text
painter.DrawText(50.0f, 100.0f, "Hello, world!");

painter.DrawText(sdl_painter::Rect{0, 200, 800, 50},
                 "Centred text",
                 sdl_painter::Alignment::kCenter);
```

### 4.6 Clipping

```cpp
painter.SetClipRect({100, 100, 400, 300});
// only what falls inside this rectangle is visible
painter.FillCircle(500, 250, 200);  // partially clipped
painter.ClearClip();
```

Scissor-based, axis-aligned rectangles.

---

## 5. Switching to the Vulkan backend

```cpp
// Vulkan window flag instead of the OpenGL one:
SDL_Window* window = SDL_CreateWindow(
    "Vulkan Demo", 800, 600,
    SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);

sdl_painter::Painter painter(window,
                              sdl_painter::RendererBackend::kVulkan);
```

> The library must have been built with Vulkan support:
> `conan install . -o "&:with_vulkan=True"`.

Nothing else in your code changes. The same `painter.FillRect`,
`painter.DrawText` and `painter.Save` calls keep working. This is what the
`IRenderer` abstraction buys you, in concrete terms.

---

## 6. Demos to read, in order

`examples/` contains one self-contained demo per feature. A good reading order
for newcomers:

| Demo | Topic | Relevant API |
|------|-------|--------------|
| `primitives.cpp` | every basic primitive | `DrawRect`, `FillCircle`, `DrawPolyline` |
| `transforms.cpp` | transform stack | `Save`/`Restore`, `Translate`/`Rotate`/`Scale` |
| `clipping.cpp` | clipping | `SetClipRect` / `ClearClip` |
| `images.cpp` | images and textures | `Image`, `DrawImage` |
| `text.cpp` | text | `Font`, `DrawText`, `Alignment` |
| `vulkan_clear.cpp` | Vulkan: clear the window | `RendererBackend::kVulkan` |
| `vulkan_triangles.cpp` | Vulkan: first triangles | — |
| `vulkan_textured.cpp` | Vulkan: images | — |
| `vulkan_demo.cpp` | Vulkan: every primitive | — |
| `vulkan_text.cpp` | Vulkan: text | — |

Descriptions of all sixteen demos: [examples/README.md](../examples/README.md).

---

## 7. Troubleshooting

### 7.1 `painter.IsValid()` returns false

- OpenGL: does the window carry the `SDL_WINDOW_OPENGL` flag?
- OpenGL: were the 3.3 Core context attributes set?
- Vulkan: does the window carry `SDL_WINDOW_VULKAN`, and was the library built
  with `with_vulkan=True`?

### 7.2 Nothing is drawn

- Are the calls between `painter.Begin()` and `painter.End()`?
- Is the `Pen` or `Brush` transparent (alpha = 0)?
- Is the Y coordinate off-screen? (Y = 0 is at the top, positive goes down.)
- Are you inside a transform with unbalanced `Save`/`Restore`?

### 7.3 OpenGL error 1282 on shutdown

- The `Painter` instance must be destroyed **before** `SDL_DestroyWindow`.
  The simplest fix is to put the Painter in its own scope.

### 7.4 Poor performance

- Do not change `SetPen`/`SetBrush` before every draw call — a colour change
  does not force a flush, but an opacity change does.
- `Font::GetGlyph` is expensive on the first call for a glyph and free
  afterwards, once it is cached.
- Drawing sprites that share a texture back to back collapses into a single
  draw call.

---

## 8. Where to go next

- 🧩 [Class Diagram](sinif-diyagrami.md) — the full API structure *(in Turkish)*
- 🔄 [Flow Diagrams](akislar.md) — frame lifecycle, batch flush, transform stack *(in Turkish)*
- 📜 [Feature List](sdl-painter-ozellikler.md) — every supported primitive *(in Turkish)*
- 💡 [Examples Guide](sdl-painter-ornekler.md) — more usage examples *(in Turkish)*
- 🏗️ [Software Engineering Perspective](sdl-painter-yazilim-muhendisligi.md) — why the design decisions were made *(in Turkish)*
- 📐 [Architecture Overview](architecture.md) — layers, dependencies, data flow
