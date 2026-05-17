# Docker Aşamaları — Kullanım Kılavuzu

## İmaj Hiyerarşisi

```mermaid
graph TD
    BASE["debian:13-slim"]

    BASE --> BUILDER

    subgraph BUILDER["builder"]
        B1["gcc-13 / clang-18"]
        B2["SDL3 sistem bağımlılıkları"]
        B3["Conan 2 + profil"]
        B4["Conan cache\nLinux Debug + Release"]
    end

    BUILDER --> CI
    BUILDER --> WIN

    subgraph CI["ci"]
        C1["lcov / gcovr"]
        C2["SDL_VIDEODRIVER=offscreen"]
        C3["SDL_AUDIODRIVER=dummy"]
    end

    subgraph WIN["windows-cross"]
        W1["mingw-w64 / wine64"]
        W2["windres wrapper"]
        W3["Conan profil: windows-mingw"]
        W4["Conan cache\nWindows Debug + Release"]
    end
```

---

## Yerel Geliştirme Akışları

### builder — Linux geliştirme ortamı

```mermaid
flowchart LR
    A([Geliştirici]) --> B["docker build\n--target builder\n-t sdl-painter:dev ."]
    B --> C["docker run --rm -it\n-v \$(pwd):/workspace\nsdl-painter:dev bash"]
    C --> D["cmake --preset linux-debug"]
    D --> E["cmake --build\n--preset linux-debug"]
    E --> F["ctest --preset linux-debug\n--output-on-failure"]
```

```powershell
docker build --target builder -t sdl-painter:dev .
docker run --rm -it -v "${PWD}:/workspace" sdl-painter:dev bash
```

---

### ci — Headless test ve kod kapsama

```mermaid
flowchart LR
    A([CI / Geliştirici]) --> B["docker build\n--target ci\n-t sdl-painter:ci ."]
    B --> C["docker run --rm\n-v \$(pwd):/workspace\nsdl-painter:ci bash -c ..."]

    C --> D["cmake --preset linux-debug"]
    D --> E["cmake --build\n--preset linux-debug"]
    E --> F{"Test türü"}

    F -->|Birim test| G["ctest --preset linux-debug\n--output-on-failure"]
    F -->|Kapsama| H["ctest ... &&\ngcovr -r . build/"]
```

```powershell
docker build --target ci -t sdl-painter:ci .

# Debug test
docker run --rm -v "${PWD}:/workspace" sdl-painter:ci bash -c `
  "cmake --preset linux-debug && cmake --build --preset linux-debug && ctest --preset linux-debug --output-on-failure"

# Release test
docker run --rm -v "${PWD}:/workspace" sdl-painter:ci bash -c `
  "cmake --preset linux-release && cmake --build --preset linux-release && ctest --preset linux-release --output-on-failure"
```

---

### windows-cross — Windows cross-compile (Linux host)

```mermaid
flowchart LR
    A([Geliştirici]) --> B["docker build\n--target windows-cross\n-t sdl-painter:windows-cross ."]
    B --> C["docker run --rm\n-v \$(pwd):/workspace\nsdl-painter:windows-cross bash -c ..."]

    C --> D["conan install .\n--profile:host=windows-mingw"]
    D --> E["cmake --preset\nwindows-mingw-debug"]
    E --> F["cmake --build\n--preset windows-mingw-debug"]
    F --> G[("Windows .exe / .dll")]

    G --> H{"Çalıştırma"}
    H -->|Konteyner içi| I["wine64 build/MyApp.exe"]
    H -->|Windows host'a kopyala| J["scp / mount"]
```

```powershell
docker build --target windows-cross -t sdl-painter:windows-cross .

docker run --rm -v "${PWD}:/workspace" sdl-painter:windows-cross bash -c `
  "conan install . --output-folder=build/windows-mingw-debug/generators --build=missing -s build_type=Debug --profile:build=default --profile:host=windows-mingw && cmake --preset windows-mingw-debug && cmake --build --preset windows-mingw-debug"
```

---

## GitLab CI/CD Entegrasyonu

Pipeline, bu repodaki `Dockerfile`'dan üretilip GitLab Container Registry'ye
push'lanan imajları **doğrudan** kullanır. Ayrı bir `.conan_base` template'i
veya `before_script` ile araç kurulumu **yoktur** — araçlar imajda hazır gelir.

| Job | Image | Kaynak |
|---|---|---|
| `build:linux:*`, `test:unit*`, `quality:*` | `$CI_REGISTRY_IMAGE` | `Dockerfile` `ci` / `builder` hedefi |
| `build:windows:*` | `$CI_REGISTRY_IMAGE/sdl-painter-win-cross` | `Dockerfile` `windows-cross` hedefi (Linux + MinGW) |
| `pages` | `ubuntu:24.04` | Conan/CMake gerektirmez, yalnız Doxygen |
| `release:publish` | `release-cli` | `v*.*.*` tag'inde çalışır |

```mermaid
flowchart TD
    PUSH([git push]) --> PIPE[GitLab CI Pipeline]

    PIPE --> BUILD[build]
    PIPE --> TEST[test]
    PIPE --> QUALITY[quality]
    PIPE --> DOCS[docs]
    PIPE --> REL[release]

    BUILD --> BLD["build:linux:debug / release\n— $CI_REGISTRY_IMAGE\n-o with_vulkan=True"]
    BUILD --> BWIN["build:windows:debug / release\n— $CI_REGISTRY_IMAGE/sdl-painter-win-cross\nMinGW cross (Vulkan'sız)"]

    TEST --> TU["test:unit\n— $CI_REGISTRY_IMAGE\noffscreen + lavapipe ICD"]
    TEST --> TA["test:unit:asan\n— $CI_REGISTRY_IMAGE\nbağımsız çalışır"]
    TEST --> TWN["test:unit:windows\n— yorum satırı (devre dışı)"]

    QUALITY --> FMT["quality:clang-format\n— --Werror, allow_failure"]
    QUALITY --> TIDY["quality:clang-tidy\n— allow_failure"]

    DOCS --> DOC["pages\n— ubuntu:24.04, Doxygen\n(yalnızca main)"]

    REL --> RELP["release:publish\n— v*.*.* tag'i ile"]

    style BLD fill:#2d6a4f,color:#fff,stroke:#1b4332
    style TU fill:#2d6a4f,color:#fff,stroke:#1b4332
```

> **Not:** `build:windows:*` job'ları SaaS Windows runner / MSVC **değildir** —
> Linux runner üzerinde MinGW cross-compile imajı (`sdl-painter-win-cross`)
> kullanır ve bu hedefte Vulkan desteklenmez (`conanfile.py configure()`
> otomatik kapatır). Native MSVC build için ayrı `Dockerfile.windows` imajı
> kullanılır (aşağıya bakın).

---

## Dockerfile.windows — Native MSVC İmajı

Linux `Dockerfile`'ının Windows muadili; tek başına çalışan, tek-OS bir imaj:
VS Build Tools 17 (VS 2022) + standalone CMake + Python + Conan 2, ve MSVC ile
ön-ısıtılmış Conan cache (Vulkan dahil). Linux Conan adımları bu imajda
çalışmaz; onlar Linux `Dockerfile`'da kalır.

```powershell
# Build — Docker, Windows containers modunda olmalı. --network nat ZORUNLU:
# aksi halde conan adımları DNS çözemez ("getaddrinfo failed").
docker build --network nat -f Dockerfile.windows -t sdl-painter-windows:latest .

# İlk doğrulama için Vulkan ön-cache'siz (çok daha hızlı):
docker build --network nat -f Dockerfile.windows `
  --build-arg WITH_VULKAN_PRECACHE=false -t sdl-painter-windows:latest .

# Native MSVC + Vulkan: derleme + testler:
docker run --rm --network nat --dns 8.8.8.8 -v ${PWD}:C:\workspace --entrypoint cmd `
  sdl-painter-windows:latest /S /C "call C:\BuildTools\VC\Auxiliary\Build\vcvars64.bat && cd /d C:\workspace && conan install . --output-folder=build/windows-release/generators --build=missing -s build_type=Release -s compiler.cppstd=17 -o ^&:with_vulkan=True && cmake --preset windows-release && cmake --build --preset windows-release && ctest --preset windows-release --output-on-failure"
```

> **DİKKAT:** Bu MSVC imajını CI'daki `sdl-painter-win-cross` etiketine
> **push'lamayın** — o etiket Linux + MinGW imajıdır; MSVC ile değiştirilirse
> `build:windows:*` job'ları (Linux runner) kırılır. Yeri: yerel native MSVC
> build/test veya isteğe bağlı self-hosted Windows Docker runner (ayrı etiketle).
> Detay için `Dockerfile.windows` içindeki "CI durumu" notuna bakın.

---

## Hızlı Başvuru

| Dosya / Hedef | İmaj etiketi | Kullanım alanı |
|---|---|---|
| `Dockerfile` → `builder` | `sdl-painter:dev` | Yerel Linux geliştirme |
| `Dockerfile` → `ci` | `sdl-painter:ci` | Headless test, kod kapsama, Vulkan (lavapipe) |
| `Dockerfile` → `windows-cross` | `sdl-painter:windows-cross` | Windows cross-compile (MinGW, Vulkan'sız) |
| `Dockerfile.windows` → `windows-msvc` | `sdl-painter-windows:latest` | Yerel native MSVC build + test (Vulkan destekli) |
