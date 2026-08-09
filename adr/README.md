# Architecture Decision Records

Every decision that shapes the public API, the backend layer or the build was
recorded here at the time it was taken, with the alternatives that were
rejected and why.

ADRs are **historical records**: they are not edited when a decision is later
revised. A new ADR supersedes the old one instead — for example ADR-009 changed
how shaders are delivered, and ADR-002's build-time `glslc` wording was left
untouched on purpose.

The records themselves are written in Turkish.

| ADR | Decision |
|-----|----------|
| [ADR-001](ADR-001-opengl-33-core-profile.md) | Choosing OpenGL 3.3 Core Profile |
| [ADR-002](ADR-002-opengl-vulkan-dual-backend.md) | OpenGL + Vulkan dual backend |
| [ADR-003](ADR-003-geometry-quad-line-thickness.md) | Geometry-quad approach for thick lines |
| [ADR-004](ADR-004-tessellator-backend-agnostic.md) | Backend-agnostic tessellator design |
| [ADR-005](ADR-005-stb-image-vs-sdl-image.md) | Image loading — stb_image vs SDL_image |
| [ADR-006](ADR-006-ear-clipping-triangulation.md) | Polygon triangulation — ear clipping |
| [ADR-007](ADR-007-glm-transform-matrix.md) | Using GLM for the transform matrix |
| [ADR-008](ADR-008-application-framework-layer.md) | Application framework layer |
| [ADR-009](ADR-009-embedded-shaders.md) | Embedding shaders into the binary |

New ADRs are required for changes that affect a backend or the public API — see
[CONTRIBUTING.md](../CONTRIBUTING.md).
