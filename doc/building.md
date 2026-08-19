# Building from Source

How to build **the repository itself**. 

To consume SDLPainter from your own
project instead, see the Installation section of the
[README](../README.md#installation).

A deeper, step-by-step walkthrough (Windows/Visual Studio, preset tables,
troubleshooting) is in [Getting Started](getting-started.md).

## Prerequisites

| Tool | Minimum version |
|------|-----------------|
| CMake | 3.21 |
| Conan | 2.x |
| GCC / Clang | C++17 support |
| MSVC | VS 2022 (v143) |
| Ninja | any version (Linux) |

**Optional:**

- **Vulkan SDK** (for `glslc`) — only if you *modify* the Vulkan shader sources.
  Building and using the Vulkan backend does not need it: the compiled SPIR-V is
  checked into the repository and embedded into the library
  ([ADR-009](../adr/ADR-009-embedded-shaders.md)).
- `clang-format-18`, `clang-tidy-18` — for the quality checks.
- `doxygen` — to generate the API reference.

## Build

```bash
# First time only
conan profile detect

# 1) Dependencies
conan install . --output-folder=build/linux-debug/generators \
    --build=missing -s build_type=Debug

# 2) Configure + build
cmake --preset linux-debug
cmake --build --preset linux-debug

# 3) Tests
ctest --preset linux-debug --output-on-failure
```

Enable the Vulkan backend by adding `-o "&:with_vulkan=True"` to the
`conan install` line.

The [helper scripts](scripts.md) wrap all of the above:
`./scripts/build.sh Debug --vulkan`.

## CMake options

| Option | Default | Effect |
|--------|---------|--------|
| `SDLPAINTER_WITH_VULKAN` | `OFF` | Build the Vulkan backend |
| `SDLPAINTER_BUILD_EXAMPLES` | `ON` | Build the demos in `examples/` |
| `SDLPAINTER_BUILD_TESTS` | `ON` | Build the GTest suite |
| `SDLPAINTER_REGENERATE_SHADERS` | `OFF` | Expose the `regenerate_shaders` target (needs `glslc`) |

Presets, and the full option reference, are documented in
[Getting Started §2.3–2.4](getting-started.md#23-cmake-preset-reference).

## Building without presets

The presets hardcode the Conan toolchain path, so a consumer that does not use
them needs the plain CMake invocation. This path is covered by the
`build:standalone-cmake` CI job:

```bash
cmake -S . -B build/out \
  -DCMAKE_TOOLCHAIN_FILE="$PWD/build/generators/conan_toolchain.cmake" \
  -DCMAKE_BUILD_TYPE=Release \
  -DSDLPAINTER_WITH_VULKAN=ON \
  -DSDLPAINTER_BUILD_EXAMPLES=OFF \
  -DSDLPAINTER_BUILD_TESTS=OFF
cmake --build build/out
cmake --install build/out --prefix /your/prefix
```

## Deploying runtime DLLs (Windows)

SDL3 and its dependencies are shared libraries and must sit next to your
executable, or the program exits with `0xC0000135` (DLL not found). SDLPainter
does not manage your deployment layout; the standard CMake one-liner is:

```cmake
if(WIN32)
    add_custom_command(TARGET my_app POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_RUNTIME_DLLS:my_app> $<TARGET_FILE_DIR:my_app>
        COMMAND_EXPAND_LISTS)
endif()
```

> This covers imported targets that report their DLL location — in practice
> SDL3, SDL3_ttf and the Vulkan loader. It does **not** reliably catch
> *transitive* DLLs: SDL_ttf pulls in freetype, harfbuzz, glib, plutosvg and
> zlib, and several Conan recipes leave `IMPORTED_LOCATION` unset for those. If
> you render text, deploy that chain too — the least error-prone route is
> Conan's `VirtualRunEnv` generator or a deployer.

The repository's own build uses an internal helper
(`sdlpainter_copy_runtime_dlls`) for this. It is deliberately **not** exported:
CMake functions are global, so a consumer using `add_subdirectory` would see it
while a `find_package` consumer would not — a helper that works on one route and
not the other is worse than none. `packaging/consumer/` verifies both routes.

## Docker

The `Dockerfile` contains three stages:

| Stage | Example tag | Contents |
|-------|-------------|----------|
| `builder` | `sdl-painter:dev` | GCC 13, Clang 18, CMake, Ninja, Conan 2, SDL3 system dependencies |
| `ci` | `sdl-painter:ci` | `builder` + lcov/gcovr + headless OpenGL (`SDL_VIDEODRIVER=offscreen`) |
| `windows-cross` | `sdl-painter:windows-cross` | `builder` + MinGW-w64 + Wine + Conan `windows-mingw` profile |

```bash
docker build --target builder       -t sdl-painter:dev .
docker build --target ci            -t sdl-painter:ci .
docker build --target windows-cross -t sdl-painter:windows-cross .
```

### Bind mount syntax depends on your shell

The commands below mount the project directory to `/workspace` inside the
container. **How you write the mount depends on the shell you use**; copied into
the wrong shell, the command silently mounts the wrong directory or fails:

| Shell | Correct form | What goes wrong otherwise |
|-------|--------------|---------------------------|
| Linux / macOS | `-v $(pwd):/workspace` | — |
| Windows PowerShell | `-v "${PWD}:/workspace"` (quotes are **mandatory**) | Unquoted, `$(pwd):/workspace` splits into two arguments and `:/workspace` is taken as the image name → `docker: invalid reference format` |
| Windows Git Bash | `MSYS_NO_PATHCONV=1 docker run ... -v "$(pwd):/workspace"` | MSYS rewrites the in-container path `/workspace` to `C:/Program Files/Git/workspace`; the mount points at the wrong directory and the container fails with `CMakePresets.json not found` |

The examples below use Linux/macOS syntax.

### Development shell

```bash
docker run --rm -it -v $(pwd):/workspace sdl-painter:dev bash
```

```powershell
docker run --rm -it -v "${PWD}:/workspace" sdl-painter:dev bash
```

### Headless Linux tests

```bash
docker run --rm -v $(pwd):/workspace sdl-painter:ci bash -c \
  "conan install . --output-folder=build/linux-debug/generators \
     --build=missing -s build_type=Debug && \
   cmake --preset linux-debug && \
   cmake --build --preset linux-debug && \
   ctest --preset linux-debug --output-on-failure"
```

### Windows cross-compile on a Linux host

The MinGW target has its own output folder and preset — the Linux preset is not
used. Because the Conan profile defines `os=Windows` and the MinGW compilers,
the generated `conan_toolchain.cmake` is sufficient; you do not need to pass an
extra `-DCMAKE_TOOLCHAIN_FILE`.

```bash
docker run --rm -v $(pwd):/workspace sdl-painter:windows-cross bash -c \
  "conan install . --output-folder=build/windows-mingw-debug/generators \
     --build=missing -s build_type=Debug \
     --profile:build=default \
     --profile:host=windows-mingw && \
   cmake --preset windows-mingw-debug && \
   cmake --build --preset windows-mingw-debug"
```

> Vulkan is not supported on this target — the `vulkan-loader` recipe forces
> `USE_MASM` on Windows and MinGW gcc cannot compile it. `configure()` in
> `conanfile.py` turns `with_vulkan` off automatically here. For Vulkan on
> Windows use a native MSVC build (`windows-release` preset).

Full workflows and publishing steps: [Docker Guide](docker.md) ·
[Publishing to Docker Hub](docker-hub-deployment.md) *(both in Turkish)*.
