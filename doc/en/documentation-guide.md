[Türkçe sürüm](../dokumantasyon-rehberi.md) | **English**

# SDLPainter — Documentation Guide

This document explains the **Doxygen comment standard**, the local build steps
and the GitLab Pages integration for contributors to the library.

---

## 1. Toolchain

```
include/sdl_painter/*.h   →  Doxygen  →  HTML (public/)
doc/*.md                  →  Doxygen  →  HTML (public/)
                                           ↓
                                    GitLab Pages
                              https://<namespace>.gitlab.io/<repo>/
```

Doxygen merges two sources: **header comments** (API reference) and the
**doc/ markdown files** (guides, diagrams). The result is a single HTML site.

Mermaid diagrams are **rendered natively in Doxygen's HTML output with recent
Doxygen versions (≈1.10+)**: ` ```mermaid ` blocks are wrapped in
`<pre class="mermaid">` and mermaid.js is injected into the page from a CDN. That
is why the `docs` job in CI downloads Doxygen's official Linux binary instead of
the (frozen, old) package from Ubuntu's apt repository. With old/frozen Doxygen
versions (like the one from apt) the diagrams show up as plain source code blocks
— in that case the `doc/*.md` files remain readable in the GitLab/GitHub markdown
viewer.

---

## 2. Setup (first time)

### Add the doxygen-awesome-css submodule

```bash
git submodule add \
  https://github.com/jothepro/doxygen-awesome-css.git \
  third_party/doxygen-awesome-css
git commit -m "chore: add doxygen-awesome-css submodule"
```

To pull the submodule while cloning:

```bash
git clone --recurse-submodules <repo-url>
# Or in an existing clone:
git submodule update --init
```

`GIT_SUBMODULE_STRATEGY: recursive` is already set in GitLab CI; no extra
configuration is needed.

---

## 3. Local Build

```bash
# CMake configure (once is enough — same command as the normal build)
cmake --preset linux-debug

# Run Doxygen
cmake --build --preset linux-debug --target sdlpainter_docs

# Open in the browser
xdg-open build/linux-debug/docs/public/index.html   # Linux
start build\linux-debug\docs\public\index.html       # Windows
```

If Doxygen is not installed, CMake does not define the `sdlpainter_docs` target
(it does not block the project build). Installation:

```bash
# Ubuntu / Debian
sudo apt install doxygen graphviz

# macOS
brew install doxygen graphviz

# Windows (winget)
winget install doxygen.doxygen
winget install graphviz.graphviz
```

---

## 4. What to Document and What Not To

| Scope | Rule |
|--------|-------|
| `include/sdl_painter/*.h` — all `public` members | `@brief` is **mandatory**; parameters and the return value must be documented |
| `include/sdl_painter/*.h` — class/struct definition | A class-level block (`/// @brief` + a description of the general behavior) is **mandatory** |
| `src/tessellator.h`, `src/render_batcher.h` (internal) | `@brief` may be written; it does not end up in the Doxygen output (covered by `EXCLUDE_PATTERNS`) |
| `private` methods | Do not write comments unless needed — well-named code is enough |
| Implementation (`*.cpp`) | Inline comments only for the **why**; they do not end up in the Doxygen output |

**Rule:** "If I deleted this comment, would another developer suffer from the
loss?" If the answer is no, do not write the comment.

---

## 5. Comment Format

All comments are written with the `///` triple slash. The `/** */` block format
is not used (for consistency with Google Style).

The project's own source comments are written in Turkish; the examples below are
translated.

### 5.1 Basic Comment Block

```cpp
/// @brief One-sentence summary. Starts with a capital letter, ends with a period.
///
/// Second paragraph: why this method exists, what it guarantees, how it
/// should be called. If it is longer than one line, add another paragraph.
///
/// @param cx   Center X coordinate (pixels, Y=0 at the top).
/// @param cy   Center Y coordinate (pixels).
/// @param radius  Radius (pixels). 0 or negative → returns an empty list.
/// @return The tessellated vertex list.
///         The segment count is adaptive: `max(16, int(radius * 0.5f))`.
/// @note Stateless — the same input always gives the same output.
static std::vector<Vertex> TessellateFilledCircle(float cx, float cy,
                                                  float radius);
```

### 5.2 Tags in Use

| Tag | When |
|--------|----------|
| `@brief` | **Mandatory** on every public member — one-line summary |
| `@param name` | For every parameter; write its name and the expected value range |
| `@return` | On every non-void function; including the error case |
| `@note` | Unexpected behavior, an important restriction, a contract |
| `@warning` | If misuse causes a crash / undefined behavior |
| `@code` / `@endcode` | A usage example (if it is more than one line) |
| `@see` | Another related class/method (optional) |
| `@since` | If it was added to the API in a specific phase (e.g. `@since Phase 4`) |

`@todo` and `@deprecated` are not used in the repo; those belong in an ADR or the
issue tracker.

### 5.3 Class-Level Block

```cpp
/// @brief Image manager — loads files through stb_image.
///
/// When an Image object is created, the pixel data is loaded into CPU memory.
/// The GPU texture is uploaded lazily on the first `DrawImage` call and
/// cached; the second call costs nothing.
///
/// An Image must be bound to a single IRenderer: uploading to different
/// renderers is not supported (the texture handle is renderer-specific).
///
/// @see Texture, IRenderer::CreateTexture
class Image {
```

### 5.4 Enum Values

```cpp
/// @brief Render backend option.
enum class RendererBackend {
  kOpenGL,  ///< OpenGL 3.3 Core Profile — with the GLAD loader.
  kVulkan,  ///< Vulkan 1.1 — SPIR-V pipeline (requires SDLPAINTER_HAS_VULKAN).
};
```

The trailing `///<` format attaches comments to enum values in Doxygen.

### 5.5 Unit and Range Rule for `@param`

State the unit or range for every parameter that could be ambiguous:

```cpp
// Good:
/// @param angle_degrees  Rotation angle (degrees, clockwise positive).
/// @param alpha          Global opacity [0.0 = fully transparent, 1.0 = fully opaque].
/// @param width          Line width (pixels, >= 0; 0 → drawing is skipped).

// Bad (what does it fail to specify?):
/// @param angle  Angle.
/// @param alpha  Alpha value.
/// @param width  Width.
```

---

## 6. Common Mistakes

### 6.1 Repeating the `@brief`

```cpp
// Bad: the signature already says this
/// @brief The DrawRect function draws a rectangle.
void DrawRect(float x, float y, float w, float h);

// Good: add what the signature does not say
/// @brief Draw a rectangle outline; use FillRect for the fill.
/// @note If the pen is invisible (alpha=0 or width=0), drawing is skipped.
void DrawRect(float x, float y, float w, float h);
```

### 6.2 Putting Implementation Details in a Public Comment

```cpp
// Bad: the user does not need to know this
/// @brief Calls Tessellator::TessellateStrokedRect and sends it to the batcher.
void DrawRect(float x, float y, float w, float h);

// Good: the behavioral contract
/// @brief Draw a rectangle outline.
/// @note Drawing is skipped if the pen is invisible; must be called between Begin/End.
void DrawRect(float x, float y, float w, float h);
```

### 6.3 Commenting Everything

```cpp
// Bad: noise
/// @brief Returns the width.
int32_t Width() const { return mWidth; }

// Good: getters are self-explanatory, no comment needed
int32_t Width() const { return mWidth; }
```

---

## 7. WARN_IF_UNDOCUMENTED

`WARN_IF_UNDOCUMENTED = YES` is set in `Doxyfile.in`. This means Doxygen emits a
warning for every public member missing a `@brief`. The CI `pages` job writes
these warnings to stderr; for now it does not break the build (warning ≠ error)
but they should be reviewed.

To make it strict later, in `Doxyfile.in`:

```ini
WARN_AS_ERROR = YES   # Treat all warnings as errors → the CI job fails
# or more selectively:
WARN_AS_ERROR = FAIL_ON_WARNINGS
```

---

## 8. GitLab Pages

The `pages` CI job runs only on the **default branch** (`main`). The
documentation is updated automatically after every merge.

Published address:
```
https://<gitlab-namespace>.gitlab.io/<repo-name>/
```

If the project is not public yet, **"Everyone"** access may need to be granted in
the GitLab Pages settings (`Settings → General → Visibility → Pages`).

---

## 9. Document Types and Responsibilities

```
include/sdl_painter/*.h   ← API reference (Doxygen)      Who: whoever writes the code
doc/hizli-baslangic.md    ← Usage guide (Markdown)       Who: whoever writes the code
doc/mimari-genel-bakis.md ← Architecture (Markdown+Mermaid) Who: architect / senior
doc/akislar.md            ← Flows (Markdown+Mermaid)     Who: architect / senior
adr/                      ← Decision rationale (ADR)     Who: whoever makes the decision
```

English versions of the doc/ files live in `doc/en/` (e.g. `getting-started.md`,
`architecture.md`, `flows.md`).

The two kinds of document do not replace each other:
- **Doxygen comments** → "What does this method do, how is it called?"
- **doc/ guides** → "Why does this component exist, where does it sit in the big picture?"

---

## 10. Related Files

| File | Role |
|-------|-----|
| `cmake/Doxyfile.in` | Doxygen template (configured with CMake variables) |
| `cmake/Docs.cmake` | Defines the `sdlpainter_docs` CMake target |
| `CMakeLists.txt` | `SDLPAINTER_BUILD_DOCS` option |
| `.gitlab-ci.yml` | `pages` job — publishes to GitLab Pages on the main branch |
| `third_party/doxygen-awesome-css/` | Theme submodule (added with git submodule add) |
