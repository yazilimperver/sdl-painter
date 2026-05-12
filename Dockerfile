# ─── Builder stage ───────────────────────────────────────────────────────────
FROM ubuntu:24.04 AS builder

ENV DEBIAN_FRONTEND=noninteractive \
    CONAN_HOME=/root/.conan2

RUN apt-get update && apt-get install -y --no-install-recommends \
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
    libgl1-mesa-dev libegl1-mesa-dev \
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

# ─── CI stage ────────────────────────────────────────────────────────────────
# Builder üzerine ekstra CI araçları (lcov, gcovr) ekler.
FROM builder AS ci

RUN apt-get update && apt-get install -y --no-install-recommends \
    lcov \
    gcovr \
    && rm -rf /var/lib/apt/lists/*

# Headless OpenGL için offscreen driver
ENV SDL_VIDEODRIVER=offscreen \
    SDL_AUDIODRIVER=dummy

# ─── Windows cross-compile stage ─────────────────────────────────────────────
# Linux container içinde MinGW-w64 ile Windows .exe / .dll üretir.
# Üretilen binary'ler Windows'ta çalışır; container Windows host gerektirmez.
FROM builder AS windows-cross

RUN apt-get update && apt-get install -y --no-install-recommends \
    mingw-w64 \
    wine64 \
    && rm -rf /var/lib/apt/lists/*

# Conan'a MinGW Windows cross-compile profili ekle
RUN conan profile detect && \
    cp ~/.conan2/profiles/default ~/.conan2/profiles/windows-mingw && \
    sed -i 's/^os=.*/os=Windows/'          ~/.conan2/profiles/windows-mingw && \
    sed -i 's/^arch=.*/arch=x86_64/'       ~/.conan2/profiles/windows-mingw && \
    sed -i 's/^compiler=.*/compiler=gcc/'  ~/.conan2/profiles/windows-mingw && \
    sed -i 's/^compiler.version=.*/compiler.version=13/' \
    ~/.conan2/profiles/windows-mingw && \
    sed -i 's/^compiler.libcxx=.*/compiler.libcxx=libstdc++11/' \
    ~/.conan2/profiles/windows-mingw

# CMake toolchain dosyası repo'dan kopyalanır (cmake/MinGwToolchain.cmake)
COPY cmake/MinGwToolchain.cmake /usr/local/share/MinGwToolchain.cmake

WORKDIR /workspace

# ─── Kullanım ─────────────────────────────────────────────────────────────────
# Linux builder image:
#   docker build --target builder -t sdl-painter:dev .
#
# CI (headless test):
#   docker build --target ci -t sdl-painter:ci .
#
# Windows cross-compile image:
#   docker build --target windows-cross -t sdl-painter:windows-cross .
#
# Geliştirme shell'i (Linux):
#   docker run --rm -it -v $(pwd):/workspace sdl-painter:dev bash
#
# Linux testleri headless çalıştır:
#   docker run --rm -v $(pwd):/workspace sdl-painter:ci bash -c \
#     "conan install . --output-folder=build/linux-debug/generators \
#        --build=missing -s build_type=Debug && \
#      cmake --preset linux-debug && \
#      cmake --build --preset linux-debug && \
#      ctest --preset linux-debug --output-on-failure"
#
# Windows cross-compile (Linux host'ta Windows .exe üretir):
#   docker run --rm -v $(pwd):/workspace sdl-painter:windows-cross bash -c \
#     "conan install . --output-folder=build/linux-debug/generators \
#        --build=missing -s build_type=Debug \
#        --profile:build=default \
#        --profile:host=windows-mingw && \
#      cmake --preset linux-debug \
#        -DCMAKE_TOOLCHAIN_FILE=/usr/local/share/MinGwToolchain.cmake && \
#      cmake --build --preset linux-debug"
