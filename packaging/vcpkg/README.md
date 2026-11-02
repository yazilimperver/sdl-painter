# vcpkg overlay port'u

`ports/sdl-painter/`, SDLPainter'ı vcpkg ile kurmak için bir overlay port'tur.
Kaynağı GitHub etiket arşivinden (`vcpkg_from_github`) indirir; test ve örnek
derlemez.

| Dosya | İşlev |
|---|---|
| `vcpkg.json` | Sürüm, bağımlılıklar, `vulkan` özelliği |
| `portfile.cmake` | İndirme, yama, configure, kurulum |
| `usage` | Kurulumdan sonra vcpkg'nin gösterdiği CMake tarifi |
| `vcpkg-dependencies.patch` | Yalnızca 1.4.0 için: stb ve Vulkan başlık hedefinin vcpkg'de bulunması |

## Yerel deneme

```bash
# Repo kökünden; VCPKG_ROOT bir vcpkg klonunu göstermeli
vcpkg install sdl-painter --overlay-ports=packaging/vcpkg/ports
vcpkg install "sdl-painter[vulkan]" --overlay-ports=packaging/vcpkg/ports
vcpkg install sdl-painter:x64-windows-static --overlay-ports=packaging/vcpkg/ports

# Kurulu paketi tüketici projeyle sına
cmake -S packaging/consumer -B build/vcpkg-consumer \
      -DCMAKE_TOOLCHAIN_FILE="$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake" \
      -DVCPKG_TARGET_TRIPLET=x64-windows
cmake --build build/vcpkg-consumer --config Release
build/vcpkg-consumer/Release/consumer.exe
```

Statik triplet'te (`x64-windows-static`) tüketici de statik CRT kullanmalı:
`-DCMAKE_MSVC_RUNTIME_LIBRARY="MultiThreaded$<$<CONFIG:Debug>:Debug>"`.

Kurulu bir paketi başka özellikle yeniden kurmak için (`sdl-painter` →
`sdl-painter[vulkan]`) `--recurse` gerekir. Port'ta değişiklik yaptıktan sonra
`vcpkg remove sdl-painter` ile kaldırıp yeniden kur; vcpkg aynı ABI'yi ikili
önbellekten getirir.

Shared triplet'te vcpkg toolchain'i bağımlılık DLL'lerini (SDL3_ttf zinciri
dahil) exe'nin yanına kendisi kopyalar.

## Linux

`vcpkg.json`, `sdl3`'ü `default-features: false` ve yalnızca `x11` + `wayland`
ile ister. vcpkg varsayılan özellikleri yine de seçer, çünkü onları yalnızca
**üst düzey proje** kapatabilir. Bu yüzden iki yol var:

| Yol | Ne gerekir | Paket sayısı |
|---|---|---|
| Yalın: manifest'te `{ "name": "sdl3", "default-features": false }` | Yalnızca derleme araçları ve X11/Wayland geliştirme paketleri | 20 |
| Varsayılan: klasik `vcpkg install` ya da düz manifest | Ek olarak `autoconf autoconf-archive automake libtool libltdl-dev python3-venv libibus-1.0-dev` | 32 |

Fark, `sdl3`'ün `ibus` özelliğinden geliyor: `dbus` → `libsystemd` → `libmount`,
`libcap`, `libxcrypt`, `gperf`. SDLPainter'ın buna ihtiyacı yok.

Her iki yol da `ci-v1.2` imajında (Debian 13, GCC 13.3) `x64-linux` ve
`x64-linux-dynamic` ile sınandı; ikisinde de `vcpkg` aracı için `curl zip unzip
tar` gerekiyor. `package:vcpkg` CI job'u yalın yolu sınar. Kaynak olarak HEAD'i
değil port'taki sürümü derler, sabit bir vcpkg commit'i kullanır; port
güncellenince `VCPKG_COMMIT` de gözden geçirilmeli.

## vcpkg'ye özgü notlar

- **stb:** vcpkg'nin stb port'u CMake config'i kurmaz. CMakeLists `stb::stb`
  hedefini bulamazsa `stb_image.h`'yi `find_path` ile arar. stb yalnızca
  `src/image.cpp`'de kullanıldığı için export'a girmez.
- **Vulkan:** `Vulkan::Loader` ve `Vulkan::Headers` hedef adları Conan'da ve
  vcpkg'de aynı.
- **glad:** vcpkg her zaman statik derler ve uyumluluk profiliyle en son GL
  sürümünü üretir. 3.3 core'un bütün fonksiyonları içinde.
- **sdl3-ttf:** vcpkg'de `harfbuzz` ve `svg` özellikleri varsayılan olarak
  kapalı. SDLPainter yalnızca temel SDL_ttf API'sini kullandığı için gerekmez.

## Yeni sürüm

1. `vcpkg.json`'da `version`'ı yükselt.
2. `portfile.cmake`'teki `SHA512`'yi yenile:
   ```bash
   curl -sL https://github.com/yazilimperver/sdl-painter/archive/vX.Y.Z.tar.gz | sha512sum
   ```
3. Etiket bu düzeltmeleri içeriyorsa (1.4.0'dan sonraki her sürüm)
   `vcpkg-dependencies.patch`'i ve `PATCHES` satırını sil.
