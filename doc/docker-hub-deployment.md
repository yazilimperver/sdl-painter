# Docker Hub'a Docker İmaj Yükleme

**[Yazılımperver'in Dünyası](www.yazilimperver.net)** sayfamda olabildiğince teknolojilerine kullanımına ilişkin örnek bilgileri sizlerle paylaşıyorum. Bu sayfada aslında o minvalde hazırlandı. Buradaki adımları SDLPainter için izlemenize gerek yok, bununla birlikte kendi imajlarınızı oluşturup, yüklemenize faydalı olacağını düşünüyorum.

## Mevcut Aşamalar

`Dockerfile` (Linux, çok aşamalı):

| Hedef | Etiket | İçerik |
|---|---|---|
| `builder` | `sdl-painter:dev` | gcc-13, clang-18, SDL3 bağımlılıkları, Conan cache (Linux Debug+Release, Vulkan dahil) |
| `ci` | `sdl-painter:ci` | builder + lcov/gcovr + lavapipe Vulkan ICD, headless ortam |
| `windows-cross` | `sdl-painter:windows-cross` | builder + MinGW-w64/wine64, Conan cache (Windows MinGW, Vulkan'sız) |

`Dockerfile.windows` (ayrı, tek-OS — native Windows/MSVC):

| Hedef | Etiket | İçerik |
|---|---|---|
| `windows-msvc` | `sdl-painter-windows:latest` | VS Build Tools 17, standalone CMake, Python, Conan cache (MSVC, Vulkan dahil) |

---

## Adım 1: Docker Hub'da Hesap Oluşturma
1. [Docker Hub](https://hub.docker.com/) adresine gidin
2. Ücretsiz bir hesap oluşturun
3. "Create Repository" ile yeni bir repo oluşturun (örn: `sdl-painter-ci`)

## Adım 2: Docker Hub'a Giriş

```powershell
docker login
# Username: dockerhub_kullanici_adiniz
# Password: dockerhub_sifreniz (veya Access Token)
```

## Adım 3: İmajları Oluşturma ve Etiketleme

```powershell
# builder — Linux geliştirme ortamı
docker build --target builder -t dockerhub_kullanici_adiniz/sdl-painter:dev .
docker build --target builder -t dockerhub_kullanici_adiniz/sdl-painter:dev-v0.1.0 .

# ci — headless test + coverage
docker build --target ci -t dockerhub_kullanici_adiniz/sdl-painter:ci .
docker build --target ci -t dockerhub_kullanici_adiniz/sdl-painter:ci-v0.1.0 .

# windows-cross — MinGW cross-compile
docker build --target windows-cross -t dockerhub_kullanici_adiniz/sdl-painter:windows-cross .
docker build --target windows-cross -t dockerhub_kullanici_adiniz/sdl-painter:windows-cross-v0.1.0 .
```

## Adım 4: Docker Hub'a Gönderme

```powershell
docker push dockerhub_kullanici_adiniz/sdl-painter:dev
docker push dockerhub_kullanici_adiniz/sdl-painter:dev-v0.1.0

docker push dockerhub_kullanici_adiniz/sdl-painter:ci
docker push dockerhub_kullanici_adiniz/sdl-painter:ci-v0.1.0

docker push dockerhub_kullanici_adiniz/sdl-painter:windows-cross
docker push dockerhub_kullanici_adiniz/sdl-painter:windows-cross-v0.1.0
```

## Adım 5: İmajları Test Etme

```powershell
# builder — interaktif shell
docker run --rm -it -v "${PWD}:/workspace" dockerhub_kullanici_adiniz/sdl-painter:dev bash

# ci — headless test
docker run --rm -v "${PWD}:/workspace" dockerhub_kullanici_adiniz/sdl-painter:ci bash -c `
  "cmake --preset linux-debug && cmake --build --preset linux-debug && ctest --preset linux-debug --output-on-failure"

# windows-cross — Windows binary üretme
docker run --rm -v "${PWD}:/workspace" dockerhub_kullanici_adiniz/sdl-painter:windows-cross bash -c `
  "conan install . --output-folder=build/windows-mingw-debug/generators --build=missing -s build_type=Debug --profile:build=default --profile:host=windows-mingw && cmake --preset windows-mingw-debug && cmake --build --preset windows-mingw-debug"
```

---

## GitLab CI/CD ile Otomatik Yükleme 

`.gitlab-ci.yml` dosyasına ekleyin:

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

GitLab değişkenleri (**Settings > CI/CD > Variables**):
- `DOCKER_USERNAME` — Docker Hub kullanıcı adı
- `DOCKER_PASSWORD` — Docker Hub şifresi veya Access Token

---

## GitLab Container Registry ile Kullanım

Mevcut `.gitlab-ci.yml` job'ları imajları **şu adlarla** tüketir — push
ederken bu adlara birebir uyun, aksi halde pipeline güncellenmiş imajı görmez:

| Pipeline'ın kullandığı image | Kaynak hedef |
|---|---|
| `$CI_REGISTRY_IMAGE` (tag yok → `:latest`) | `Dockerfile` `ci` hedefi |
| `$CI_REGISTRY_IMAGE/sdl-painter-win-cross` | `Dockerfile` `windows-cross` hedefi |

```powershell
# Linux ci imajı — build:linux:* / test:* / quality:* job'ları kullanır
docker build --target ci -t registry.gitlab.com/yazilimperver/sdl-painter .
docker push registry.gitlab.com/yazilimperver/sdl-painter

# Windows MinGW cross imajı — build:windows:* job'ları kullanır
docker build --target windows-cross -t registry.gitlab.com/yazilimperver/sdl-painter/sdl-painter-win-cross .
docker push registry.gitlab.com/yazilimperver/sdl-painter/sdl-painter-win-cross
```

`.gitlab-ci.yml` ile otomatik yükleme:

```yaml
docker:gitlab:
  stage: release
  image: docker:24
  services:
    - docker:24-dind
  before_script:
    - docker login -u $CI_REGISTRY_USER -p $CI_REGISTRY_PASSWORD $CI_REGISTRY
  script:
    # Pipeline'ın okuduğu adlarla birebir aynı:
    - docker build --target ci            -t $CI_REGISTRY_IMAGE .
    - docker build --target windows-cross  -t $CI_REGISTRY_IMAGE/sdl-painter-win-cross .
    - docker push $CI_REGISTRY_IMAGE
    - docker push $CI_REGISTRY_IMAGE/sdl-painter-win-cross
  rules:
    - if: $CI_COMMIT_BRANCH == $CI_DEFAULT_BRANCH
    - if: $CI_COMMIT_TAG =~ /^v\d+\.\d+\.\d+$/
```

> Native MSVC imajı (`Dockerfile.windows`) buraya **dahil edilmez**:
> `sdl-painter-win-cross` etiketine MSVC push'lanırsa `build:windows:*`
> job'ları (Linux runner) kırılır. MSVC imajını gerekiyorsa ayrı bir etiketle
> (örn. `sdl-painter-win-msvc`) ve yalnız self-hosted Windows Docker runner için
> push'layın.

---

## Platform Notları

### Windows Host (WSL2 + Docker Desktop)

Linux container'lar WSL2 üzerinde çalışır — `builder` ve `ci` imajları doğrudan kullanılabilir:

```powershell
docker run --rm -it -v "${PWD}:/workspace" sdl-painter:ci bash
```

### Çoklu Mimari (linux/amd64 + linux/arm64)

```powershell
docker buildx build --platform linux/amd64,linux/arm64 `
  --target ci -t dockerhub_kullanici_adiniz/sdl-painter:ci --push .
```

### Windows Native Binary

Linux host'ta Windows `.exe` üretmek için `Dockerfile`'ın `windows-cross`
hedefi kullanılır (MinGW, Vulkan'sız) — CI'daki `build:windows:*` job'ları
budur. Native MSVC build (Vulkan dahil) için ayrı `Dockerfile.windows` imajı
kullanılır; bu imaj CI'da değil, yerel veya self-hosted Windows Docker
runner'da çalıştırılır (bkz. `Dockerfile.windows` içindeki "CI durumu" notu).
