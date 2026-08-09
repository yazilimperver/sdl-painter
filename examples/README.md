# SDLPainter Examples

Sixteen runnable demos, each isolating one capability. Build them with the
repository (they are on by default) and run them straight from the build tree:

```bash
cmake --preset linux-debug
cmake --build --preset linux-debug

./build/linux-debug/examples/primitives
```

On Windows the executables land under `build\windows-debug\examples\Debug\`.

To skip building the demos entirely: `-DSDLPAINTER_BUILD_EXAMPLES=OFF`.

---

## Start here

| Demo | What it shows | Needs |
|---|---|---|
| [`hello_window`](hello_window.cpp) | Opens an SDL window and shuts down cleanly — verifies the toolchain, nothing is drawn. | — |
| [`primitives`](primitives.cpp) | Every basic shape: filled and stroked rectangles, circles and ellipses, thick lines, polylines, a concave polygon. | — |
| [`transforms`](transforms.cpp) | `Translate` / `Rotate` / `Scale`, nested `Save`/`Restore`, viewport tracking on resize. | — |
| [`clipping`](clipping.cpp) | `SetClipRect` / `ClearClip`, plus a centre-rotation correctness check that stays centred at any window size. | — |
| [`images`](images.cpp) | `DrawImage` in all three overloads — original size, scaled destination rect, atlas slicing — with alpha blending and a rotating texture. | — |
| [`text`](text.cpp) | `DrawText` at a point and aligned inside a rect, `Font::MeasureText`, multiple point sizes, coloured and semi-transparent text. | SDL_ttf |

| | |
|:---:|:---:|
| ![primitives](../doc/screenshots/primitifler.png) | ![images](../doc/screenshots/texture.png) |
| **`primitives`** | **`images`** |
| ![text](../doc/screenshots/metin.png) | ![tictactoe](../doc/screenshots/uygulama.png) |
| **`text`** | **`tictactoe`** |

## Application framework

The window, event loop and timing layer is optional and lives in a separate
target (`sdl_painter::app` — see
[ADR-008](../adr/ADR-008-application-framework-layer.md)).

| Demo | What it shows | Needs |
|---|---|---|
| [`app_basics`](app_basics.cpp) | The same visual output as `transforms`, without its ~250 lines of SDL boilerplate. | — |
| [`game_loop`](game_loop.cpp) | Fixed-timestep simulation with render interpolation — a bouncing ball stays smooth even at a low `fixed_update_hz`. | — |
| [`tictactoe`](tictactoe.cpp) | A complete application: mouse hit testing, hover highlighting, responsive layout on resize, and an app state machine. | SDL_ttf |

The pure game logic of `tictactoe` sits in [`tictactoe_logic.h`](tictactoe_logic.h)
so it can be unit tested without a window (`tests/test_tictactoe_logic.cpp`).

## Vulkan backend

All of these require `-DSDLPAINTER_WITH_VULKAN=ON` (Conan:
`-o "&:with_vulkan=True"`). They verify that the Vulkan backend behaves
identically to OpenGL, one capability at a time.

| Demo | What it shows | Needs |
|---|---|---|
| [`vulkan_clear`](vulkan_clear.cpp) | Instance, device, swapchain and `Clear`. | Vulkan |
| [`vulkan_triangles`](vulkan_triangles.cpp) | Untextured primitives, transform stack, opacity. | Vulkan |
| [`vulkan_textured`](vulkan_textured.cpp) | Texture upload and sampling through `DrawTextured`. | Vulkan |
| [`vulkan_demo`](vulkan_demo.cpp) | Swapchain recreation on resize, every primitive, every opacity level — validation layers must stay silent. | Vulkan |
| [`vulkan_text`](vulkan_text.cpp) | SDL_ttf glyph atlases on the Vulkan backend, including UTF-8. | Vulkan + SDL_ttf |

## The hero animation

| Demo | What it shows | Needs |
|---|---|---|
| [`hero`](hero.cpp) | The choreographed scene behind the README banner: primitives, a thick polyline, a concave polygon, nested transforms, a procedural texture, an opacity ramp and text — all animating on an 8-second perfect loop. | SDL_ttf |

Unlike the others this one is a *composition* rather than an isolated feature,
and it can render itself to disk instead of to a window:

```bash
./build/linux-debug/examples/hero                                 # watch it
./build/linux-debug/examples/hero --dump-frames build/hero_frames # 240 PPM frames
./scripts/make-hero-gif.sh                                        # → doc/hero.gif
```

Dumping frames beats screen recording here: no cursor, no window chrome, no
dropped frames, and the loop closes exactly. It needs `ffmpeg` only for the
final assembly step. Every animation completes a whole number of periods over
the 240 frames, so frame 240 is identical to frame 0.

---

## Development phases

SDLPainter was built phase by phase, and the demos were originally named after
those phases. The names changed after v1.1.0; this table maps the old names so
that write-ups and blog posts referring to a phase still lead somewhere.

| Phase | Old name | Current name |
|---|---|---|
| 0 | `phase0_demo` | `hello_window` |
| 1 | `phase1_demo` | `primitives` |
| 2 | `phase2_demo` | `transforms` |
| 2b | `phase2b_demo` | `clipping` |
| 3 | `phase3_demo` | `images` |
| 4 | `phase4_demo` | `text` |
| 5a | `phase5a_vulkan_clear` | `vulkan_clear` |
| 5b | `phase5b_vulkan_triangles` | `vulkan_triangles` |
| 5c | `phase5c_vulkan_textured` | `vulkan_textured` |
| 5d | `phase5d_vulkan_demo` | `vulkan_demo` |
| 5e | `phase5e_vulkan_text` | `vulkan_text` |
| 6 | `phase6_app_demo` | `app_basics` |
| 7 | `phase7_game_demo` | `game_loop` |
| 8 | `phase8_tictactoe` | `tictactoe` |
| — | — | `hero` *(new, not part of the phased build-up)* |

Each source file also repeats its phase in the header comment.

---

A longer walkthrough of every demo — what it verifies and why it is written
that way — is in [`doc/sdl-painter-ornekler.md`](../doc/sdl-painter-ornekler.md)
*(in Turkish)*.
