[Türkçe sürüm](../ozellikler.md) | **English**

# SDLPainter — Feature List

This page lists the most notable features of the SDLPainter library.

## Drawing Primitives

- Line (`DrawLine`)
- Rectangle — stroke and fill (`DrawRect` / `FillRect`)
- Rounded rectangle — stroke and fill
  (`DrawRoundedRect` / `FillRoundedRect`)
- Circle — stroke and fill (`DrawCircle` / `FillCircle`)
- Ellipse — stroke and fill (`DrawEllipse` / `FillEllipse`)
- Arc (`DrawArc`), pie (`DrawPie` / `FillPie`), chord
  (`DrawChord` / `FillChord`) — angles in degrees, 0° = +x
- Polygon — stroke and fill (`DrawPolygon` / `FillPolygon`)
- Polyline (`DrawPolyline`)

## Visual Styles

- **Pen:** color + width (geometry-based quads; `glLineWidth` is not used),
  dash pattern (`SetDashPattern`), cap style (butt/square/round), join
  style (miter/bevel/round — falls back to bevel automatically once the miter
  limit is exceeded)
- **Brush:** solid fill color or a two-stop linear/radial gradient.
  Gradients need no shader: the transition is written into the vertex colors
  and the hardware interpolates it — so it **does not break the batch**, but
  its precision is only as good as the shape's vertex density
- **Opacity:** global transparency `[0.0, 1.0]`
- **Blend mode:** `kAlpha` / `kAdditive` / `kMultiply` / `kNone`.
  Being GPU state, it breaks the batch; on Vulkan a pipeline variant per mode is
  created at startup

## Path

- `Path` — line segments, quadratic (`QuadTo`) and cubic (`CubicTo`) Bézier
  curves; every `MoveTo` starts a new subpath, `Close` closes the subpath
- Curves are flattened **as they are added to the path** (`Flatness`, default
  0.25 px): no control points are stored, the tessellator is untouched and all
  pen styles (dash, cap, join) work on curves too
- `DrawPath` draws the outline, `FillPath` the fill
- **Limitation:** `FillPath` fills every subpath independently — there is no
  even-odd / nonzero rule, so shapes with holes such as the letter "O" do not
  fill correctly

## Transform Stack

- `Translate`, `Rotate`, `Scale`
- `Save` / `Restore`
- `ResetTransform`
- 3×3 affine matrix — `glm::mat3` (column-major), see ADR-007

## Clipping

- Scissor-based rect clip (`SetClipRect` / `ClearClip`)

## Viewport

- `SetViewport` / `ResetViewport` — restricts drawing to a sub-rectangle and
  makes coordinates local to it (split screen, minimap)
- Difference from clipping: clipping masks without changing the coordinate
  system, a viewport redefines the system itself

## Image / Texture

- PNG and JPG loading (stb_image)
- Full source rect → destination rect scaling (atlas slicing)
- Color tinting and mirroring (`ImageFlip`) — both are carried in the vertex,
  so neither breaks the batch
- Sampling filter: `kLinear` (default) / `kNearest` (pixel art)
- In-place texture updates with `Painter::UpdateImage` (procedural textures
  that change every frame)
- Free-form textured grid with `Painter::DrawImageMesh` — vertices can move
  independently
- Alpha blending support

## Render Target (RenderTarget)

- Draw into a texture instead of the screen with `Painter::CreateRenderTarget`
  — minimaps, post-processing effects, trail effects, caching layers that are
  expensive but rarely change
- `SetRenderTarget` / `ResetRenderTarget`; while a target is bound, coordinates
  are **local to the target** and both backends produce pixel-identical results
- `DrawRenderTarget` draws the target like an image (tint and mirroring included)
- `ReadRenderTarget` reads the pixels back as linear RGBA8 — screenshots and the
  backend parity tests use it
- Targets are single-sampled (no MSAA)

## Text

- Font loading via SDL_ttf 3.x
- Glyph cache
- Alignment: left / center / right
- Placing text inside a rect
- Multi-line text (newline character) and `Font::LineHeight()`
- Word wrapping (`TextWrap::kWord`) — UTF-8 safe; `CountTextLines()` returns the
  line count without drawing

## Tessellator

- Backend-independent vertex generation
- Adaptive segment count for circles/ellipses: `max(16, int(radius * 0.5f))`
- Ear clipping triangulation for concave polygons

## Render Batcher

Draw calls do not go straight to the GPU; `RenderBatcher` accumulates vertices in a buffer and hands them to the renderer in one go with `Flush()` only when the mode, texture or opacity changes (or the 8192-vertex limit is reached). This greatly reduces the number of draw calls, especially in scenes where many small shapes are drawn back to back.

## Backend Support

- **OpenGL 3.3 Core** — GLAD loader, GLSL shaders
- **Vulkan 1.1** — SPIR-V pipeline (Phase 5, optional)
- `IRenderer` interface — the single implementation point for adding a new backend

## Platform Support

- Linux (GCC / Clang, Ninja)
- Windows (MSVC, Visual Studio 2022)
- Windows cross-compile inside a Linux container (MinGW-w64)

## Infrastructure

- CMake Presets (8 presets: `linux-debug`, `linux-release`, `linux-debug-coverage`, `linux-debug-asan`, `windows-debug`, `windows-release`, `windows-mingw-debug`, `windows-mingw-release`)
- Conan 2 dependency management — required packages including SDL_ttf, only Vulkan is optional (`-o "&:with_vulkan=True"`)
- Docker multi-stage: development / headless CI / cross-compile
- CI/CD: GitHub Actions (primary) + GitLab CI (mirror) — build → test → quality → docs; clang-format mandatory, clang-tidy soft
- AddressSanitizer + UBSan preset (`linux-debug-asan`)
- Google C++ Style, clang-format-18
- Unit tests (gtest)
