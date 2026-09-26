[Türkçe sürüm](../ornekler.md) | **English**

# SDLPainter — Examples Guide

Each demo builds on the previous one. The examples were developed phase by phase,
each one adding something on top of another. This document explains **what each
example verifies**, **which API capabilities it uses** and **why it matters from
an engineering point of view**.

> **Scope:** This guide covers the core demos that came with the phases, in order
> (`hello_window` → `tictactoe`). The later `graphics/`, `games/`,
> `benchmarks/` demos and the `hero` showcase are not covered here yet; for the
> up-to-date list of all of them with one-line descriptions see
> [examples/README.md](../../examples/README.md).

---

## hello_window — Infrastructure Check

**Purpose:** Verifies that the build, SDL3 initialization and the spdlog
integration work. It draws nothing.

### Capabilities used

| Capability | Detail |
|---------|-------|
| SDL3 initialization | `SDL_Init(SDL_INIT_VIDEO)` |
| OpenGL context | 3.3 Core Profile attribute settings |
| spdlog | ANSI colored output, `[HH:MM:SS][Level]` format |
| Event loop | `SDL_PollEvent` + ESC / `SDL_EVENT_QUIT` |
| Windows compatibility | Enabling ANSI escapes with `ENABLE_VIRTUAL_TERMINAL_PROCESSING` |

### Engineering note

That the Phase 0 demo draws nothing is deliberate: moving on to the later phases
makes no sense until the infrastructure (CMake, Conan, CI/CD pipeline,
`IRenderer` interface) works. The demo acts as an integration test of that
infrastructure.

---

## primitives — Basic Primitives

**Purpose:** Verifies all basic drawing primitives of the OpenGL backend.
Stroke/fill pairs, thick lines, a polyline and a concave polygon are shown
together in a single window.

### Capabilities used

| Capability | Shown as |
|---------|---------|
| `FillRect` / `DrawRect` | Filled and outlined rectangles |
| `FillCircle` / `DrawCircle` | Different colors and widths |
| `FillEllipse` / `DrawEllipse` | Asymmetric ellipse with rx ≠ ry |
| `DrawLine` | Two crossing thick lines (5px) |
| `DrawPolyline` | 5-point broken line |
| `FillPolygon` | L-shaped concave polygon — ear clipping |
| `DrawPolygon` | Outline of the same polygon |
| `Save` / `Rotate` / `Scale` / `Restore` | Rotated and scaled rectangle/circle |
| Regular polygon generation | Pentagon vertices computed in a loop |
| MSAA | `SDL_GL_MULTISAMPLESAMPLES = 4` |

### Engineering note

The L-shaped concave polygon is a direct test of the ear clipping algorithm; a
triangle fan would render this geometry incorrectly. The pentagon vertices are
computed trigonometrically — not a fixed coordinate list. The demo is fully
redrawn every frame; no stateful render state accumulates.

---

## transforms — Transform Stack

**Purpose:** Verifies every operation of the transform stack with animation.
Seven independent sections isolate each operation.

### Capabilities used

| Section | Capability tested |
|-------|---------------------|
| 1 | Chained `Translate` — nested offsets |
| 2 | `Rotate` animation — spinning rectangle |
| 3 | `Scale` animation — breathing effect with `sin` |
| 4 | 3-level nested `Save` / `Restore` — independent speed and direction |
| 5 | `SetClipRect` / `ClearClip` — circle and spinning rectangle clipped to a scissor |
| 6 | `ResetTransform` — reset after a deliberately wrong transform |
| 7 | Independent X/Y `Scale` animation — on an ellipse |

### Engineering note

In section 6, after moving off-screen with `Translate(9999, 9999)`,
`ResetTransform()` is called and the circle appears at its expected position.
This is a regression test verifying that resetting the matrix does not corrupt
the upper stack levels. The 3-level nested stack in section 4 shows that the
push/pop order is correct: the innermost `Rotate` moves independently of the
outer ones.

---

## clipping — Center Rotation Check

**Purpose:** Verifies in isolation that the `Translate → Rotate` order is applied
correctly. Even when the window is resized, the rectangle always rotates around
its own center in the exact middle of the screen.

### Capabilities used

| Capability | Detail |
|---------|-------|
| Dynamic window size | `SDL_GetWindowSize` is called every frame |
| `Translate` + `Rotate` | Center rotation with a `(-w/2, -h/2)` offset |
| Reference crosshair | The screen center is marked with thin lines |

### Engineering note

This example shows why a scenario that could have been shown in passing inside
transforms had to be **pulled out into a separate demo**: getting the rotation
pivot at the right point is extremely sensitive to the order of the transform
matrix. Writing `Translate → Rotate` instead of `Rotate → Translate` shifts the
pivot, and the circle's center pointing at the corner exposes the bug
immediately. The demo is a concrete example of why regression coverage should be
kept broad.

---

## images — Image / Texture

**Purpose:** Verifies the image loading, scaling, atlas slicing and alpha
blending pipeline. All textures are generated procedurally so as not to depend
on external files.

### Procedural texture generators

| Function | Size | Channels | Content |
|-----------|-------|-------|--------|
| `MakeCheckerboard` | 128×128 | RGBA | Checkerboard pattern |
| `MakeGradient` | 256×64 | RGB | Horizontal color gradient |
| `MakeAlphaCircle` | 128×128 | RGBA | Alpha rising towards the center |
| `MakeColorAtlas` | 256×256 | RGBA | 2×2 color atlas |

### Capabilities used

| Section | Capability tested |
|-------|---------------------|
| 1 | `DrawImage(image, x, y)` — original pixel size |
| 2 | `DrawImage(image, dest_rect)` — scaled drawing |
| 3 | `DrawImage(image, src_rect, dest_rect)` — src crop + dst scale |
| 4 | Texture atlas slicing — drawing 4 tiles with separate `src_rect`s |
| 5 | Alpha blending — overlapping semi-transparent textures |
| 6 | `Transform + DrawImage` — spinning texture animation |
| 7 | `Scale` animation — texture growing and shrinking around its own center |

### Engineering note

The `Image::CreateFromData` API makes test scenarios independent of external
files — the texture pipeline can be tested even in a CI environment without
assets. Atlas slicing (section 4) simulates the sprite sheet use case. The alpha
circle texture (section 5) verifies the blending order on the GPU side: the
background rectangle is drawn first, the transparent texture after it.

---

## text — Text Rendering

**Purpose:** Verifies font loading via SDL_ttf, measurement, alignment and
transform integration.

### Capabilities used

| Section | Capability tested |
|-------|---------------------|
| 1 | `DrawText(rect, text, kCenter)` — 56pt title, centered |
| 2 | `DrawText(rect, text, kRight)` — right-aligned subtitle |
| 3 | Three alignments side by side — kLeft / kCenter / kRight comparison |
| 4 | 4 different point sizes — 14 / 22 / 36pt |
| 5 | Text color from the pen color — 5 different colors |
| 6 | `SetOpacity` — 4 different transparency levels (100% → 15%) |
| 7 | `Transform + DrawText` — pivot computed with `MeasureText`, rotating text |
| 8 | `DrawText(x, y, text)` — position-based drawing |

### Platform-independent font discovery

```
Windows: arial.ttf → calibri.ttf → segoeui.ttf → tahoma.ttf
Linux  : DejaVuSans.ttf → LiberationSans → FreeSans
```

The `FindSystemFont` function tries the candidate paths in order; if none is
found, the demo exits with an error. This is an example of handling an external
asset dependency explicitly.

### CMakeLists.txt note

`text` picks one of two `target_link_libraries` branches depending on whether
SDL_ttf is linked statically or as a shared library. It shows optional
dependencies being handled at the build-system level.

### Engineering note

The rotating text in section 7 moves the pivot to the center of the text by
applying a negative offset of half the `MeasureText` result. This verifies that
the transform stack works correctly not only with geometry but with the text
rendering pipeline too.

---

## vulkan_* — Vulkan Backend

These five demos verify step by step that the Vulkan backend behaves the same as
OpenGL. All of them are built with `SDLPAINTER_WITH_VULKAN=ON`.

| Demo | Capability verified |
|------|--------------------|
| `vulkan_clear` | Vulkan context initialization, swapchain, `Clear` |
| `vulkan_triangles` | Untextured primitives (all shape types) |
| `vulkan_textured` | `DrawImage` — texture upload, sampling |
| `vulkan_demo` | Swapchain recreate (window resize), all primitives, opacity, validation layer output |
| `vulkan_text` | SDL_ttf + Vulkan backend integration |

### Engineering note

`vulkan_demo` specifically tests the **swapchain recreate** scenario: when the
window size changes the Vulkan swapchain must be recreated, and drawing must not
break in the process. Because the `IRenderer` interface abstracts this scenario,
the `Painter` layer needs no extra code.

The `'R'` key triggers the projection update manually — this allows the same code
path to be tested in a headless environment without `SDL_WINDOWEVENT_RESIZED`.

---

## app_basics — Application Framework

**Purpose:** Produces the same visual content as `transforms` (spinning rectangle
+ pulsing circle) without the SDL boilerplate.

### Capabilities used

| Capability | Detail |
|---------|-------|
| Deriving from `Application` | `OnInit` / `OnUpdate` / `OnRender` / `OnKeyDown` overrides |
| Automatic lifecycle | SDL init, window, GL context, event loop and teardown live in the framework |
| `TimingMode::kVariable` | One `OnUpdate` per frame, `dt` is the real elapsed time |

### Engineering note

The point of comparison is `transforms`: same output, ~250 fewer lines of code.
This is where the value of the framework becomes measurable. Because the
`Application` teardown order (Painter → window → SDL_Quit) is designed to cover
the members of the derived class too, `Image`/`Font` members can be kept as plain
members (see [ADR-008](../../adr/ADR-008-application-framework-layer.md)).

---

## game_loop — Fixed-Step Game Loop

**Purpose:** Verifies deterministic simulation and render interpolation with
`TimingMode::kFixed`. A ball falls under gravity, hits the ground and bounces.

### Capabilities used

| Capability | Detail |
|---------|-------|
| `TimingMode::kFixed` | Physics in fixed steps; `OnUpdate` runs 0..N times per frame |
| `OnRender(Painter&, alpha)` | Interpolation between the previous and current position |
| `fixed_update_hz = 30` | Deliberately low sim rate — the effect of interpolation is visible |

### Engineering note

Although the sim rate is dropped to 30 Hz, the motion looks smooth, because
drawing interpolates between the last two states with `alpha` (Game Programming
Patterns, "play catch up"). If `alpha` is ignored, the same demo looks stuttery —
a contrast that shows directly why interpolation is needed.

---

## tictactoe — A Complete Application: Input + State + Layout

**Purpose:** Shows the **input** side of the framework and the flow of a real
application. While app_basics shows the basics of the framework and game_loop
the timing, this demo covers capabilities found in none of the other examples.

### Capabilities used

| Capability | Detail |
|---------|-------|
| `OnMouseButtonDown` | Click coordinate → board cell (hit testing) |
| `OnMouseMove` | Semi-transparent highlight of the empty cell under the cursor |
| `OnResize` | Board geometry is recomputed from the window size |
| State machine | playing → winner / draw → restart |
| `DrawText(Rect, ..., kCenter)` | Centered text in the bottom status bar |
| `SetTitle` | The window title reflects the turn/result |
| Thick lines + circle | The grid and X are drawn with diagonal lines, O with a circle |

**Controls:** Left click to move · `R` to restart · `ESC` to quit.
Once the game is over, any click starts a new game.

### Engineering note

The game logic (`examples/games/tictactoe_logic.h`) consists of pure functions
**completely independent** of drawing and SDL: winner detection, move validity
and hit testing. Thanks to this separation the logic can be verified headless
with unit tests (`tests/test_tictactoe_logic.cpp`) — the demo file stays
responsible only for input and drawing. This is the first piece of logic from the
example applications to be tested.

Since it is a turn-based game it uses `TimingMode::kVariable`; there is no need
for a fixed step. Read side by side with game_loop, it makes concrete when to
choose which timing mode: continuous simulation → `kFixed`, event-driven
interaction → `kVariable`.

The responsive layout is solved with a single computation in `OnResize`; the
board is always centered in the window and scaled to its shorter side. Since hit
testing uses the same geometry, it works correctly regardless of the window size.

---

## The Common Engineering Pattern of the Examples

The pattern below applies to the `hello_window`–`vulkan_text` demos; `app_basics`,
`game_loop` and `tictactoe` hand this boilerplate over to the `Application`
framework:

```
1. InitLogger()          → start colored spdlog
2. SDL_Init              → exit early on error
3. GL/Vulkan attributes  → choose the context type
4. SDL_CreateWindow      → exit early on error
5. { Painter scope }     → RAII: Painter is destroyed before the window
6. Event loop            → SDL_PollEvent, ESC / QUIT
7. painter.Begin() ... painter.End()
8. } ← Painter destroyed; GL resources deleted while the context is valid
9. SDL_DestroyWindow / SDL_Quit
```

**Why the Painter scope is kept separate:** OpenGL objects (`glDeleteBuffers`,
`glDeleteTextures` etc.) must be deleted while the context is valid.
`SDL_DestroyWindow` destroys the context; if the Painter is not destroyed before
that point, the `glDelete*` calls produce a `GL_INVALID_OPERATION` (1282) error.
The explicit scope guarantees this order.
