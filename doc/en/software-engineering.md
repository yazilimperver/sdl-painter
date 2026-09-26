[Türkçe sürüm](../yazilim-muhendisligi.md) | **English**

# SDLPainter — Software Engineering Perspective

This document is not SDLPainter's technical feature list; it covers **why the
library is designed the way it is**. Architectural decisions, design patterns,
performance choices and scope management are examined from a software
engineering perspective.

---

## Architecture: Layered Separation of Responsibilities

The library consists of four layers and each layer has a single responsibility:

```
Painter          → User API, style and transform management
  Tessellator    → Shape → vertex conversion (knows no backend)
    IRenderer    → Backend contract (knows no GPU, only takes triangles)
      SDL3       → Window, context, platform
```

In practice this separation means: when replacing `OpenGLRenderer` with
`VulkanRenderer`, not a single line changes in the `Painter`, `Tessellator` or
`RenderState` files. The boundary of change is the `IRenderer` interface.

---

## Design Patterns

### Strategy — IRenderer

The backend choice is resolved in the constructor, not at compile time.
Writing `Painter(window, RendererBackend::kVulkan)` is enough; the upper layers
are completely unaware of the backend.

```cpp
// The factory function returns an IRenderer implementation
std::unique_ptr<IRenderer> CreateRenderer(RendererBackend backend);
```

Adding a new backend comes down to writing a class that implements `IRenderer`
— no existing code needs to change (Open/Closed Principle).

### Memento — Transform Stack

The `Save()` / `Restore()` pair works with push/pop semantics on a
`std::vector<RenderState>`. `RenderState` carries the transform matrix, pen,
brush, opacity and clip rect together; this guarantees state consistency in
nested transform blocks.

### Command Buffer — RenderBatcher

A `DrawCircle` call does not go to the GPU immediately. `RenderBatcher`
accumulates vertices in a buffer; `Flush()` is triggered only when:

- The draw mode changes (solid color ↔ textured)
- The active texture changes
- The opacity changes
- The 8 192-vertex limit is reached

This makes the number of draw calls independent of the number of shapes in the
scene; performance scales with the frequency of state changes, not with
individual GPU commands.

### Stateless Tessellator

All methods of the `Tessellator` class are static and hold no state.
`TessellateFilledCircle` called with the same parameters always produces the same
vertex list. This makes it possible to write unit tests without a GPU: input
coordinates → the expected vertex count and values are verified.

---

## Dependency Design

### Minimizing External Dependencies

| Decision | Choice | Why |
|-------|--------|-------|
| Image loading | stb_image | Header-only, zero transitive dependencies; SDL_image needs extra installation |
| OpenGL loading | GLAD | Only the 3.3 Core functions are included; no full GLEW load needed |
| Font | SDL_ttf | Optional; not part of the build until Phase 4 |
| Vulkan | Optional Conan package | With `with_vulkan=False` the Vulkan loader is never downloaded |

### Required and Optional Dependencies

In `conanfile.py` the dependencies are split into required and optional ones.
The Vulkan loader, SDL_ttf and GTest are pulled in only when the relevant option
is enabled. This split keeps CI time and image size small.

---

## Performance Decisions

### Not Using glLineWidth

In the OpenGL 3.3 Core Profile `glLineWidth(x > 1.0f)` is deprecated, and its
maximum value, which varies from driver to driver, cannot be guaranteed. Thick
lines are instead drawn with `TessellateThickLine`, which turns every segment
into two triangles (a quad):

```
A1 ─────────────── B1   (+half_width · normal)
A2 ─────────────── B2   (-half_width · normal)
Triangles: A1-A2-B1  and  A2-B2-B1
```

This keeps the pixel width consistent across Linux, Windows and a headless
Docker environment. The Tessellator performs this conversion; the renderer only
receives a list of triangles.

### Adaptive Tessellation

Circles and ellipses are drawn with a segment count that depends on the radius:

```cpp
int segments = std::max(16, static_cast<int>(radius * 0.5f));
```

Small circles use few segments (efficiency), large circles many (quality).
A fixed segment count cannot strike this balance.

### Convex Optimization

`TessellateFilledPolygon` checks convexity first. Convex polygons are handled
with an O(n) triangle fan; concave ones are routed to O(n²) ear clipping. For
typical polygon sizes in 2D UI drawing (n < 200) the difference is negligible.

---

## Testability

### Unit Tests Without a GPU

Since `Tessellator` is stateless and independent of `IRenderer`, its unit tests
run purely on the CPU. The vertex count generated for a circle, the coordinates
of the thick line quad corners and the triangle count from ear clipping can be
verified by direct numeric comparison.

### Mockability of IRenderer

Since `RenderBatcher` and `Painter` work purely through the interface, tests can
use a mock `IRenderer` instead of real OpenGL/Vulkan. The number of draw calls
and their parameters become testable.

### Sanitizer Integration

The `linux-debug-asan` CMake preset enables AddressSanitizer and UBSan. In the CI
pipeline a separate `test:unit:asan` job runs through this preset. Memory errors
and undefined behavior are caught at this stage even when the normal tests pass.

---

## Scope Management

Features deliberately left out of scope are documented. The list describes
**today's** limits; Path/Bézier curves and gradient fills, which were once listed
here, were added in this release (`Path` + `DrawPath`/`FillPath`,
`Brush::LinearGradient`/`RadialGradient`) and removed from the list.

| Feature | Status | Rationale |
|---------|-------|---------|
| Fills with holes (even-odd / nonzero) | Out of scope | Ear clipping does not support holes; it would require Delaunay CDT. `FillPath` fills every subpath independently |
| Multi-stop and conical gradients | Out of scope | A two-stop transition comes for free from vertex colors; anything more means shader and batch cost |
| Path-based clipping | Out of scope | Clipping is done with scissor; a path clip would require a stencil buffer |
| Full anti-aliasing | Partial (MSAA) | Analytic AA requires a path-based rasterizer |
| Print / PDF output | Out of scope | The library is designed for drawing to the screen |

---

## Architecture Decision Records (ADR)

Our important decisions in the project are each tied to an ADR file:

| ADR | Decision |
|-----|-------|
| ADR-001 | 3.3 Core Profile instead of OpenGL 4.5 or ES |
| ADR-003 | Geometry quads instead of `glLineWidth` |
| ADR-004 | Backend-agnostic design of the Tessellator |
| ADR-005 | stb_image instead of SDL_image |
| ADR-006 | Ear clipping for triangulation |

ADR format: context → alternatives considered → decision → rationale →
consequences. It answers "why was it done", not "what was done".

---

## Infrastructure Design

### Reproducibility

The `CMakePresets.json` + Conan 2 combination ensures that exactly the same
toolchain and dependency versions are used when a developer runs the
`linux-debug` preset, when the CI pipeline runs and when the Docker container
runs. It structurally eliminates the "works on my machine" problem.

### Docker Multi-Stage and Headless OpenGL

The `ci` Docker stage is started with the `SDL_VIDEODRIVER=offscreen`
environment variable. In this mode SDL3 does not look for a real display server;
the OpenGL context is created offscreen. Since GitLab runners run headless,
without this the renderer tests would crash at context creation.

The second advantage of the multi-stage structure is cache efficiency: the `ci`
and `windows-cross` stages are based on `builder`; the GCC, Clang and Conan
installation layers are not downloaded again on every build.

### Quality Gate

The `quality:clang-format` job runs with `--Werror` and breaks the pipeline; no
code to be merged can carry a format violation. `clang-tidy`, on the other hand,
is marked `allow_failure: true` in the early phases — silencing every warning in
the early stages slows development down. This setup is a deliberate trade-off.

---

## Design Principles Summary

| Principle | Its counterpart in SDLPainter |
|---------|--------------------------|
| Single Responsibility | The Tessellator knows geometry, the renderer knows the API, Painter knows style |
| Open/Closed | New backend → just implement `IRenderer`, existing code does not change |
| Liskov Substitution | `OpenGLRenderer` and `VulkanRenderer` are interchangeable for `Painter` |
| Dependency Inversion | `Painter` depends on the `IRenderer` abstraction, not on a concrete renderer |
| Scope Control | Documented limits; no speculative features |
