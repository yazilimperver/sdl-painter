# ─── Builder Stage ────────────────────────────────────────────────────────────
# Tüm araçları, SDL3 bağımlılıklarını ve Conan cache'i hazırlar.
# cmake configure/build çalışma zamanında yapılır.
FROM debian:13-slim AS builder

ENV DEBIAN_FRONTEND=noninteractive \
    CONAN_HOME=/root/.conan2

RUN apt-get update && apt-get install -y --no-install-recommends \
    ca-certificates \
    build-essential \
    gcc-13 g++-13 \
    clang-18 clang-format-18 clang-tidy-18 \
    cmake \
    ninja-build \
    python3 python3-pip \
    pkg-config \
    git \
    # SDL3 system bağımlılıkları (X11/Wayland/audio)
    libx11-dev libxext-dev libxrandr-dev libxcursor-dev libxi-dev \
    libxss-dev libxkbcommon-dev \
    libasound2-dev libpulse-dev \
    libwayland-dev \
    libgl-dev libegl-dev \
    # Headless test için
    xvfb \
    && rm -rf /var/lib/apt/lists/*

# GCC 13'ü varsayılan yap
RUN update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-13 100 \
    && update-alternatives --install /usr/bin/g++ g++ /usr/bin/g++-13 100

# Conan 2
RUN pip3 install --break-system-packages "conan>=2.0,<3.0"

# Conan varsayılan profil oluştur
RUN conan profile detect

WORKDIR /workspace

# ─── Conan Cache Ön Isıtma ───────────────────────────────────────────────────
# conanfile.py kopyalanır ve bağımlılıklar Conan cache'e indirilir.
# Proje build'i (cmake configure/build) çalışma zamanında yapılır.

COPY conanfile.py /workspace/

RUN conan install . \
    --output-folder=build/linux-debug/generators \
    --build=missing \
    -s build_type=Debug \
    -s compiler=gcc \
    -s compiler.version=13 \
    -s compiler.libcxx=libstdc++11 \
    -c tools.system.package_manager:mode=install \
    -c tools.system.package_manager:sudo=False

RUN conan install . \
    --output-folder=build/linux-release/generators \
    --build=missing \
    -s build_type=Release \
    -s compiler=gcc \
    -s compiler.version=13 \
    -s compiler.libcxx=libstdc++11 \
    -c tools.system.package_manager:mode=install \
    -c tools.system.package_manager:sudo=False

# Vulkan variantı için Conan cache pre-warm (with_vulkan=True).
# vulkan-loader, vulkan-headers, shaderc + transitive deps (glslang, spirv-tools)
# burada derlenip ~/.conan2 cache'ine alınır. Output-folder /tmp altında ve
# son adımda silinir — sadece cache (~/.conan2/p) kalır, image gereksiz şişmez.
# Shaderc'in tool_requires olduğunu unutma: build profile (Linux) için derlenir.
RUN conan install . \
    --output-folder=/tmp/vk-precache \
    --build=missing \
    -s build_type=Debug \
    -s compiler=gcc \
    -s compiler.version=13 \
    -s compiler.libcxx=libstdc++11 \
    -o "&:with_vulkan=True" \
    -c tools.system.package_manager:mode=install \
    -c tools.system.package_manager:sudo=False \
    && rm -rf /tmp/vk-precache

RUN conan install . \
    --output-folder=/tmp/vk-precache \
    --build=missing \
    -s build_type=Release \
    -s compiler=gcc \
    -s compiler.version=13 \
    -s compiler.libcxx=libstdc++11 \
    -o "&:with_vulkan=True" \
    -c tools.system.package_manager:mode=install \
    -c tools.system.package_manager:sudo=False \
    && rm -rf /tmp/vk-precache

# ─── CI Stage ────────────────────────────────────────────────────────────────
# Builder üzerine coverage araçları + Vulkan runtime (lavapipe) ekler, headless
# ortam değişkenlerini ayarlar.
FROM builder AS ci

RUN apt-get update && apt-get install -y --no-install-recommends \
    lcov \
    gcovr \
    # Vulkan ICD: Mesa lavapipe (CPU software renderer, headless test için).
    # Fiziksel GPU olmadan Vulkan API'sini sunar — CI runner'larında zorunlu.
    mesa-vulkan-drivers \
    libvulkan1 \
    vulkan-tools \
    && rm -rf /var/lib/apt/lists/*

# Vulkan ICD'sini lavapipe'a sabitle — birden fazla ICD varsa belirsizliği
# önler. Ancak mesa paketinin verdiği dosya adı sürüme göre değişiyor
# (lvp_icd.json / lvp_icd.x86_64.json). ENV'e sabit ad yazmak, ad değiştiğinde
# loader'ın HİÇBİR sürücü yüklememesine ve Vulkan testlerinin sessizce
# atlanmasına yol açıyordu. Bu yüzden imaj derlenirken gerçek dosyaya sabit
# adlı bir symlink kuruyoruz; lavapipe yoksa imaj derlemesi burada durur.
RUN mkdir -p /etc/vulkan/icd.d \
    && ICD="$(ls /usr/share/vulkan/icd.d/lvp_icd*.json 2>/dev/null | head -1)" \
    && if [ -z "$ICD" ]; then \
         echo "HATA: lavapipe ICD bulunamadi (mesa-vulkan-drivers eksik?)" >&2; \
         ls -la /usr/share/vulkan/icd.d/ >&2 || true; \
         exit 1; \
       fi \
    && ln -sf "$ICD" /etc/vulkan/icd.d/lvp_icd.json \
    && echo "lavapipe ICD -> $ICD"

ENV SDL_VIDEODRIVER=offscreen \
    SDL_AUDIODRIVER=dummy \
    VK_ICD_FILENAMES=/etc/vulkan/icd.d/lvp_icd.json \
    # Mesa software renderer override (OpenGL kod yolları için).
    MESA_LOADER_DRIVER_OVERRIDE=llvmpipe

# ─── Windows Cross-Compile Stage ─────────────────────────────────────────────
# Linux container içinde MinGW-w64 ile Windows .exe / .dll üretir.
FROM builder AS windows-cross

RUN apt-get update && apt-get install -y --no-install-recommends \
    mingw-w64 \
    wine64 \
    && rm -rf /var/lib/apt/lists/* \
    && printf '#!/bin/sh\nexec /usr/bin/x86_64-w64-mingw32-gcc -E -xc-header -DRC_INVOKED "$@"\n' \
    > /usr/local/bin/mingw-rc-cpp \
    && chmod +x /usr/local/bin/mingw-rc-cpp \
    && printf '#!/bin/sh\nexec /usr/bin/x86_64-w64-mingw32-windres --preprocessor=/usr/local/bin/mingw-rc-cpp "$@"\n' \
    > /usr/local/bin/windres \
    && chmod +x /usr/local/bin/windres

RUN MINGW_VER=$(x86_64-w64-mingw32-gcc -dumpfullversion | sed 's/^\([0-9][0-9]*\).*/\1/') && \
    cp ~/.conan2/profiles/default ~/.conan2/profiles/windows-mingw && \
    sed -i 's/^os=.*/os=Windows/'                                    ~/.conan2/profiles/windows-mingw && \
    sed -i 's/^arch=.*/arch=x86_64/'                                 ~/.conan2/profiles/windows-mingw && \
    sed -i 's/^compiler=.*/compiler=gcc/'                            ~/.conan2/profiles/windows-mingw && \
    sed -i "s/^compiler.version=.*/compiler.version=$MINGW_VER/"    ~/.conan2/profiles/windows-mingw && \
    sed -i 's/^compiler.libcxx=.*/compiler.libcxx=libstdc++11/'     ~/.conan2/profiles/windows-mingw && \
    printf '\n[buildenv]\nCC=x86_64-w64-mingw32-gcc\nCXX=x86_64-w64-mingw32-g++\nAR=x86_64-w64-mingw32-ar\nRC=x86_64-w64-mingw32-windres\nSTRIP=x86_64-w64-mingw32-strip\n' \
    >> ~/.conan2/profiles/windows-mingw && \
    printf '\n[conf]\ntools.build:compiler_executables={"c": "x86_64-w64-mingw32-gcc", "cpp": "x86_64-w64-mingw32-g++", "windres": "x86_64-w64-mingw32-windres", "ar": "x86_64-w64-mingw32-ar", "strip": "x86_64-w64-mingw32-strip"}\n' \
    >> ~/.conan2/profiles/windows-mingw

# Windows bağımlılıklarını Conan cache'e önceden indir (Debug + Release).
# Output folder'lar windows-mingw-{debug,release} preset'leriyle uyumludur.
# Not: Conan profile'ında os=Windows + [conf] compiler_executables=mingw set edildiği
# için üretilen conan_toolchain.cmake CMAKE_SYSTEM_NAME=Windows ve MinGW derleyicilerini
# otomatik ayarlar — ayrıca bir CMake toolchain dosyasına ihtiyaç yoktur.
RUN conan install . \
    --output-folder=build/windows-mingw-debug/generators \
    --build=missing \
    -s build_type=Debug \
    --profile:build=default \
    --profile:host=windows-mingw \
    -c tools.system.package_manager:mode=install \
    -c tools.system.package_manager:sudo=False

RUN conan install . \
    --output-folder=build/windows-mingw-release/generators \
    --build=missing \
    -s build_type=Release \
    --profile:build=default \
    --profile:host=windows-mingw \
    -c tools.system.package_manager:mode=install \
    -c tools.system.package_manager:sudo=False

# NOT: Windows MinGW cross-compile'da Vulkan DESTEKLENMİYOR. vulkan-loader
# Conan recipe'ı Windows'ta USE_MASM=True'yu hardcoded set ediyor ve MinGW
# gcc bunu derleyemiyor (asm_offset.c'ye MSVC /Fa /FA /Od flag'leri geçiyor).
# conanfile.py configure() bu hedefte with_vulkan'ı otomatik False'a çeker.
# Windows'ta Vulkan gerekiyorsa native MSVC build (windows-release preset)
# kullanın. Bu yüzden burada Vulkan pre-cache adımı YOK.

# ─── Kullanım ─────────────────────────────────────────────────────────────────
# Builder (geliştirme ortamı):
#   docker build --target builder -t sdl-painter:dev .
#   docker run --rm -it -v ${PWD}:/workspace sdl-painter:dev bash
#
# CI (headless test + coverage):
#   docker build --target ci -t sdl-painter:ci .
#   docker run --rm -v "${PWD}:/workspace" sdl-painter:ci bash -c "conan install . --output-folder=build/linux-release/generators --build=missing -s build_type=Release -s compiler=gcc -s  compiler.version=13 -s compiler.libcxx=libstdc++11 && cmake --preset linux-release && cmake --build --preset linux-release && ctest --preset linux-release --output-on-failure"
#   docker run --rm -v "${PWD}:/workspace" sdl-painter:ci bash -c "conan install . --output-folder=build/linux-debug/generators --build=missing -s build_type=Debug -s compiler=gcc -s  compiler.version=13 -s compiler.libcxx=libstdc++11 && cmake --preset linux-debug && cmake --build --preset linux-debug && ctest --preset linux-debug --output-on-failure"          
#   docker run --rm -v "${PWD}:/workspace" sdl-painter:ci bash -c "conan install . --output-folder=build/linux-debug-asan/generators --build=missing -s build_type=Debug -c tools.system.package_manager:mode=install && cmake --preset linux-debug-asan && cmake --build --preset linux-debug-asan && ctest --preset linux-debug-asan --output-on-failure"
# 
# Windows cross-compile (Linux host'ta Windows .exe / .dll üretir):
#   docker build --target windows-cross -t sdl-painter:windows-cross .
#
# Debug build:
#   docker run --rm -v "${PWD}:/workspace" sdl-painter:windows-cross bash -c "conan install . --output-folder=build/windows-mingw-debug/generators --build=missing -s build_type=Debug --profile:build=default --profile:host=windows-mingw && cmake --preset windows-mingw-debug && cmake --build --preset windows-mingw-debug"
#   docker run --rm -v "${PWD}:/workspace" sdl-painter:windows-cross bash -c "conan install . --output-folder=build/windows-mingw-debug/generators --build=missing -s build_type=Debug --profile:build=default --profile:host=windows-mingw && cmake --preset windows-mingw-debug && cmake --build --preset windows-mingw-debug && ctest --preset windows-mingw-debug"
#
# Release build:
#   docker run --rm -v "${PWD}:/workspace" sdl-painter:windows-cross bash -c "conan install . --output-folder=build/windows-mingw-release/generators --build=missing -s build_type=Release --profile:build=default --profile:host=windows-mingw && cmake --preset windows-mingw-release && cmake --build --preset windows-mingw-release"
#
# Çıktılar: build/windows-mingw-{debug,release}/ altında .exe ve .dll dosyaları
# docker run --rm -v "${PWD}:/workspace" sdl-painter:windows-cross x86_64-w64-mingw32-objdump -p build/windows-mingw-release/examples/phase0_demo.exe | grep "DLL Name"
#
# Vulkan destekli build — yalnızca Linux (Windows MinGW cross-compile'da
# vulkan-loader recipe'ı USE_MASM nedeniyle derlenemiyor; configure() bu
# hedefte with_vulkan'ı otomatik kapatır):
#   docker run --rm -v "${PWD}:/workspace" sdl-painter:ci bash -c "conan install . --output-folder=build/linux-release/generators --build=missing -s build_type=Release -s compiler=gcc -s compiler.version=13 -s compiler.libcxx=libstdc++11 -o '&:with_vulkan=True' && cmake --preset linux-release && cmake --build --preset linux-release && ctest --preset linux-release --output-on-failure"
#
# Windows'ta Vulkan gerekiyorsa native MSVC (Windows host'ta, cross-compile
# değil): conan install ... && cmake --preset windows-release -o "&:with_vulkan=True"