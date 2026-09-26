[Türkçe sürüm](../docker-hub-deployment.md) | **English**

# Publishing Docker Images to Docker Hub

On my **[Yazılımperver'in Dünyası](https://www.yazilimperver.net)** site I share
practical examples of using various technologies as much as I can, and this page
was written in the same spirit. You do not need to follow these steps to use
SDLPainter, but I think they will be useful for building and publishing your own
images.

## Existing Stages

`Dockerfile` (Linux, multi-stage):

| Target | Tag | Contents |
|---|---|---|
| `builder` | `sdl-painter:dev` | gcc-13, clang-18, SDL3 dependencies, Conan cache (Linux Debug+Release, Vulkan included) |
| `ci` | `sdl-painter:ci` | builder + lcov/gcovr + lavapipe Vulkan ICD, headless environment |
| `windows-cross` | `sdl-painter:windows-cross` | builder + MinGW-w64/wine64, Conan cache (Windows MinGW, no Vulkan) |

`Dockerfile.windows` (separate, single-OS — native Windows/MSVC):

| Target | Tag | Contents |
|---|---|---|
| `windows-msvc` | `sdl-painter-windows:latest` | VS Build Tools 17, standalone CMake, Python, Conan cache (MSVC, Vulkan included) |

---

## Step 1: Create a Docker Hub Account
1. Go to [Docker Hub](https://hub.docker.com/)
2. Create a free account
3. Create a new repo with "Create Repository" (e.g. `sdl-painter-ci`)

## Step 2: Log In to Docker Hub

```powershell
docker login
# Username: your_dockerhub_username
# Password: your_dockerhub_password (or an Access Token)
```

## Step 3: Build and Tag the Images

```powershell
# builder — Linux development environment
docker build --target builder -t your_dockerhub_username/sdl-painter:dev .
docker build --target builder -t your_dockerhub_username/sdl-painter:dev-v0.1.0 .

# ci — headless test + coverage
docker build --target ci -t your_dockerhub_username/sdl-painter:ci .
docker build --target ci -t your_dockerhub_username/sdl-painter:ci-v0.1.0 .

# windows-cross — MinGW cross-compile
docker build --target windows-cross -t your_dockerhub_username/sdl-painter:windows-cross .
docker build --target windows-cross -t your_dockerhub_username/sdl-painter:windows-cross-v0.1.0 .
```

## Step 4: Push to Docker Hub

```powershell
docker push your_dockerhub_username/sdl-painter:dev
docker push your_dockerhub_username/sdl-painter:dev-v0.1.0

docker push your_dockerhub_username/sdl-painter:ci
docker push your_dockerhub_username/sdl-painter:ci-v0.1.0

docker push your_dockerhub_username/sdl-painter:windows-cross
docker push your_dockerhub_username/sdl-painter:windows-cross-v0.1.0
```

## Step 5: Test the Images

```powershell
# builder — interactive shell
docker run --rm -it -v "${PWD}:/workspace" your_dockerhub_username/sdl-painter:dev bash

# ci — headless test
docker run --rm -v "${PWD}:/workspace" your_dockerhub_username/sdl-painter:ci bash -c `
  "cmake --preset linux-debug && cmake --build --preset linux-debug && ctest --preset linux-debug --output-on-failure"

# windows-cross — produce a Windows binary
docker run --rm -v "${PWD}:/workspace" your_dockerhub_username/sdl-painter:windows-cross bash -c `
  "conan install . --output-folder=build/windows-mingw-debug/generators --build=missing -s build_type=Debug --profile:build=default --profile:host=windows-mingw && cmake --preset windows-mingw-debug && cmake --build --preset windows-mingw-debug"
```

---

## Automatic Publishing with GitLab CI/CD

Add to the `.gitlab-ci.yml` file:

```yaml
docker:push:
  stage: release
  image: docker:24
  services:
    - docker:24-dind
  before_script:
    - docker login -u $DOCKER_USERNAME -p $DOCKER_PASSWORD
  script:
    - docker build --target builder      -t $DOCKER_USERNAME/$CI_PROJECT_NAME:dev .
    - docker build --target ci           -t $DOCKER_USERNAME/$CI_PROJECT_NAME:ci .
    - docker build --target windows-cross -t $DOCKER_USERNAME/$CI_PROJECT_NAME:windows-cross .
    - docker push $DOCKER_USERNAME/$CI_PROJECT_NAME:dev
    - docker push $DOCKER_USERNAME/$CI_PROJECT_NAME:ci
    - docker push $DOCKER_USERNAME/$CI_PROJECT_NAME:windows-cross
  rules:
    - if: $CI_COMMIT_BRANCH == $CI_DEFAULT_BRANCH
    - if: $CI_COMMIT_TAG =~ /^v\d+\.\d+\.\d+$/
```

GitLab variables (**Settings > CI/CD > Variables**):
- `DOCKER_USERNAME` — Docker Hub username
- `DOCKER_PASSWORD` — Docker Hub password or Access Token

---

## Using the GitLab Container Registry

The existing `.gitlab-ci.yml` jobs consume the images **under these names** —
match them exactly when pushing, otherwise the pipeline will not see the updated
image:

| Image used by the pipeline | Source target |
|---|---|
| `$CI_REGISTRY_IMAGE` (no tag → `:latest`) | `Dockerfile` `ci` target |
| `$CI_REGISTRY_IMAGE/sdl-painter-win-cross` | `Dockerfile` `windows-cross` target |

```powershell
# Linux ci image — used by the build:linux:* / test:* / quality:* jobs
docker build --target ci -t registry.gitlab.com/yazilimperver/sdl-painter .
docker push registry.gitlab.com/yazilimperver/sdl-painter

# Windows MinGW cross image — used by the build:windows:* jobs
docker build --target windows-cross -t registry.gitlab.com/yazilimperver/sdl-painter/sdl-painter-win-cross .
docker push registry.gitlab.com/yazilimperver/sdl-painter/sdl-painter-win-cross
```

Automatic publishing with `.gitlab-ci.yml`:

```yaml
docker:gitlab:
  stage: release
  image: docker:24
  services:
    - docker:24-dind
  before_script:
    - docker login -u $CI_REGISTRY_USER -p $CI_REGISTRY_PASSWORD $CI_REGISTRY
  script:
    # Exactly the same names the pipeline reads:
    - docker build --target ci            -t $CI_REGISTRY_IMAGE .
    - docker build --target windows-cross  -t $CI_REGISTRY_IMAGE/sdl-painter-win-cross .
    - docker push $CI_REGISTRY_IMAGE
    - docker push $CI_REGISTRY_IMAGE/sdl-painter-win-cross
  rules:
    - if: $CI_COMMIT_BRANCH == $CI_DEFAULT_BRANCH
    - if: $CI_COMMIT_TAG =~ /^v\d+\.\d+\.\d+$/
```

> The native MSVC image (`Dockerfile.windows`) is **not included** here: if MSVC
> is pushed to the `sdl-painter-win-cross` tag, the `build:windows:*` jobs (Linux
> runner) break. If you need the MSVC image, push it under a separate tag
> (e.g. `sdl-painter-win-msvc`) and only for a self-hosted Windows Docker runner.

---

## Platform Notes

### Windows Host (WSL2 + Docker Desktop)

Linux containers run on WSL2 — the `builder` and `ci` images can be used directly:

```powershell
docker run --rm -it -v "${PWD}:/workspace" sdl-painter:ci bash
```

### Multi-Architecture (linux/amd64 + linux/arm64)

```powershell
docker buildx build --platform linux/amd64,linux/arm64 `
  --target ci -t your_dockerhub_username/sdl-painter:ci --push .
```

### Windows Native Binary

To produce a Windows `.exe` on a Linux host, the `windows-cross` target of the
`Dockerfile` is used (MinGW, no Vulkan) — that is what the `build:windows:*` jobs
in CI are. For a native MSVC build (Vulkan included) the separate
`Dockerfile.windows` image is used; this image runs not in CI but locally or on a
self-hosted Windows Docker runner (see the "CI status" note inside
`Dockerfile.windows`).
