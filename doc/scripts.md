# Script Reference

Every script has a `.sh` (Linux/macOS, bash) and a `.ps1` (Windows PowerShell)
twin with the same behaviour. **Run them from the project root.**

The scripts are a convenience layer over Conan and CMake presets (for build details see [Building from source](building.md)).

## build.sh / Build.ps1 — Build

```bash
# Usage: ./scripts/build.sh [Debug|Release|ASan] [flags]
./scripts/build.sh                              # Debug (default)
./scripts/build.sh Release                      # Release
./scripts/build.sh ASan                         # Debug + ASan/UBSan
./scripts/build.sh Debug --vulkan               # Vulkan backend
./scripts/build.sh Release --no-examples        # Build without examples
./scripts/build.sh Debug --target transforms    # Single target
./scripts/build.sh --clean                      # Remove build dir, rebuild from scratch
./scripts/build.sh --jobs 8                     # 8 parallel jobs
./scripts/build.sh --skip-conan                 # CMake only (skip Conan)
./scripts/build.sh --docs                       # Build + Doxygen HTML docs
```

```powershell
# Usage: .\scripts\Build.ps1 [Debug|Release] [flags]
.\scripts\Build.ps1                             # Debug (default)
.\scripts\Build.ps1 Release                     # Release
.\scripts\Build.ps1 -Vulkan                     # Vulkan backend
.\scripts\Build.ps1 Release -NoExamples         # Build without examples
.\scripts\Build.ps1 -Target transforms          # Single target
.\scripts\Build.ps1 -Clean                      # Clean build
.\scripts\Build.ps1 -Jobs 8                     # 8 parallel jobs
.\scripts\Build.ps1 -SkipConan                  # CMake only
.\scripts\Build.ps1 -Docs                       # Build + Doxygen HTML docs
```

> If the toolchain is missing or the flags changed, the `build` scripts invoke
> the `conan-install` script automatically.

## conan-install.sh / Conan-Install.ps1 — Dependencies

```bash
# Usage: ./scripts/conan-install.sh [Debug|Release|ASan] [flags]
./scripts/conan-install.sh                       # Debug (default)
./scripts/conan-install.sh Release               # Release
./scripts/conan-install.sh Debug --vulkan        # Include Vulkan dependencies
./scripts/conan-install.sh Release --no-tests    # Exclude test dependencies
./scripts/conan-install.sh Debug --no-examples   # Exclude example dependencies
```

```powershell
.\scripts\Conan-Install.ps1                      # Debug (default)
.\scripts\Conan-Install.ps1 Release              # Release
.\scripts\Conan-Install.ps1 -Vulkan              # Include Vulkan dependencies
.\scripts\Conan-Install.ps1 -NoTests             # Exclude test dependencies
.\scripts\Conan-Install.ps1 -NoExamples          # Exclude example dependencies
```

## run-tests.sh / Run-Tests.ps1 — Tests

```bash
# Usage: ./scripts/run-tests.sh [Debug|Release|ASan] [flags]
./scripts/run-tests.sh                           # All tests, Debug
./scripts/run-tests.sh Release                   # All tests, Release
./scripts/run-tests.sh ASan                      # Tests with sanitizers
./scripts/run-tests.sh --filter Transform        # Only Transform tests
```

```powershell
.\scripts\Run-Tests.ps1                          # All tests, Debug
.\scripts\Run-Tests.ps1 Release                  # All tests, Release
.\scripts\Run-Tests.ps1 -Filter Transform        # Only Transform tests
```

## format-check.sh / Format-Check.ps1 — Format Check

```bash
./scripts/format-check.sh           # Check formatting (exits non-zero on error)
./scripts/format-check.sh --fix     # Fix automatically
```

```powershell
.\scripts\Format-Check.ps1                               # Check formatting
.\scripts\Format-Check.ps1 -FixMode                      # Fix automatically
.\scripts\Format-Check.ps1 -ClangFormat clang-format-18  # Custom binary
```

Formatting is enforced in CI — a violation fails the pipeline. See
[Development](development.md#quality-checks).

## make-hero-gif.sh / Make-HeroGif.ps1 — README banner

Assembles the frames dumped by the [`hero`](../examples/README.md#the-hero-animation)
demo into `doc/hero.gif`. Requires `ffmpeg` (`sudo apt install ffmpeg` /
`winget install --id Gyan.FFmpeg -e`); the script says so if it is missing.

```bash
./build/linux-debug/examples/hero --dump-frames build/hero_frames
./scripts/make-hero-gif.sh                      # defaults: 15 fps, 720 px wide
./scripts/make-hero-gif.sh --fps 12 --width 640 # smaller file
./scripts/make-hero-gif.sh --mp4                # also emit doc/hero.mp4
```

```powershell
.\build\windows-debug\examples\Debug\hero.exe --dump-frames build\hero_frames
.\scripts\Make-HeroGif.ps1
.\scripts\Make-HeroGif.ps1 -Fps 12 -Width 640
.\scripts\Make-HeroGif.ps1 -Mp4
```

A two-pass palette (`palettegen` → `paletteuse`) is used because single-pass
conversion bands visibly on this scene's gradients. The script prints the
resulting size and warns above 3 MB.

## changelog-section.sh

Extracts a single version's section out of `CHANGELOG.md`; used by the release
job to build the GitHub Release body.

```bash
bash scripts/changelog-section.sh v1.1.0
```

Exits non-zero if the version has no section, so a release never ships with
empty notes.
