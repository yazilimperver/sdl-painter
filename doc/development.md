# Development

Working *on* SDLPainter. For building it see [Building from source](building.md);
for the helper scripts see the [Script Reference](scripts.md).

Contribution rules (commit format, branch strategy, when an ADR is required) are
in [CONTRIBUTING.md](../CONTRIBUTING.md).

## Directory layout

```
sdl-painter/
├── include/sdl_painter/   # Public headers (including app/)
├── src/                   # Implementation
│   ├── app/               # Application framework (sdl_painter_app)
│   ├── opengl/            # OpenGL backend + GLSL shaders
│   └── vulkan/            # Vulkan backend + SPIR-V shaders
├── tests/                 # GTest unit tests
├── examples/              # Demo applications (see examples/README.md)
├── packaging/consumer/    # External-consumer verification project
├── doc/                   # Design documents, diagrams, screenshots
├── cmake/                 # CMake helper modules
├── scripts/               # Build/test helper scripts (.sh + .ps1)
└── adr/                   # Architecture Decision Records
```

`packaging/consumer/` is a standalone project that links SDLPainter from the
outside, in both `find_package` and `add_subdirectory` mode. It must stay
outside any gitignored directory — CI builds it.

## Quality checks

```bash
# Format check
./scripts/format-check.sh

# Auto-fix formatting
./scripts/format-check.sh --fix

# clang-tidy (after building)
find src/ -name '*.cpp' | xargs clang-tidy-18 -p build/linux-debug/
```

Formatting is **mandatory**: `quality:clang-format` fails the pipeline on any
violation. clang-tidy runs as a soft fail.

## API reference (Doxygen)

**Online:** on every push to the default branch the `docs` job publishes to
GitHub Pages — <https://yazilimperver.github.io/sdl-painter>

**Locally:**

```bash
doxygen Doxyfile              # documentation only
./scripts/build.sh --docs     # build + documentation
xdg-open build/docs/index.html
```

```powershell
doxygen Doxyfile
.\scripts\Build.ps1 -Docs
Start-Process build\docs\index.html
```

> If Doxygen is not installed: `apt install doxygen` (Linux) or
> [doxygen.nl](https://www.doxygen.nl/download.html) (Windows).

Doxygen configuration and comment conventions:
[Documentation Guide](dokumantasyon-rehberi.md) *(in Turkish)*.

## CI/CD

The primary pipeline is **GitHub Actions** (`.github/workflows/ci.yml`). Linux
jobs run in the prebuilt Docker Hub image (`yazilimperver/sdl-painter:ci-v1.0`);
Windows jobs run on a native MSVC runner.

| Job | Environment | What it does |
|-----|-------------|--------------|
| `build:linux:debug` | Linux (container) | Debug, Vulkan on |
| `build:linux:release` | Linux (container) | Release, Vulkan on |
| `build:windows:debug` | `windows-2022`, MSVC | Debug, Vulkan on, runs `ctest` |
| `build:windows:release` | `windows-2022`, MSVC | Release, Vulkan on |
| `test:unit` | Linux (container) | GTest — headless (offscreen + lavapipe ICD) |
| `test:unit:asan` | Linux (container) | ASan + UBSan, built without Vulkan |
| `build:standalone-cmake` | Linux (container) | Configure without presets, install, then build `packaging/consumer/` |
| `package:conan-create` | Linux (container) | `conan create` + `test_package` — early warning for the packaging path |
| `quality:shader-freshness` | `ubuntu-latest` | Checks the SPIR-V outputs against the GLSL sources via `sources.sha256` |
| `quality:clang-format` | Linux (container) | Google Style check — **hard fail** |
| `quality:clang-tidy` | Linux (container) | Static analysis — soft fail |
| `docs` | `ubuntu-latest` | Doxygen → GitHub Pages (default branch only) |
| `release:publish` | `ubuntu-latest` | GitHub Release on `v*.*.*` tags |

Notes worth knowing before touching CI:

- **Renderer smoke tests only really run on Linux.** Windows' `offscreen` video
  driver cannot load EGL, so the OpenGL/Vulkan smoke tests `SKIP` there. On
  Linux, Mesa EGL plus the lavapipe ICD makes them run headless for real.
- **The lavapipe ICD filename is resolved at runtime** (`lvp_icd*.json`), because
  it varies by Mesa version. A wrong hardcoded path makes the loader load *no*
  driver, and the Vulkan tests then SKIP silently.
- **SPIR-V is compared through a manifest, not by recompiling.** The CI image has
  no `glslc`; `cmake -P cmake/CheckShaderFreshness.cmake` re-hashes the GLSL
  sources and compares them against `src/vulkan/shaders/spirv/sources.sha256`.

The repository also contains a `.gitlab-ci.yml` for the GitLab mirror. Both
pipelines now run the **same 15 jobs** — including the packaging path
(`build:standalone-cmake`, `build:shared`, `package:conan-create`,
`quality:shader-freshness`), which the mirror was missing until 26 August 2026.
`clang-format` is a hard gate on both.

Three differences remain, all deliberate: Windows builds go through **MinGW
cross-compile** on a Linux runner (GitHub uses native MSVC), the Windows test
job is commented out (it needs a GitLab SaaS Windows runner), and the `pages`
job publishes to GitLab Pages instead of GitHub Pages.