# Conan Center recipe'i

Bu dizin, `conan-io/conan-center-index` (CCI) deposuna gönderilen
`recipes/sdl_painter/` recipe'inin **asıl kopyasıdır**. Değişiklik önce burada
yapılır, PR anında CCI fork'una kopyalanır. İnceleme sırasında CCI'da yapılan
düzeltmeler de buraya geri işlenir.

Kök `conanfile.py` ile karıştırma: o, geliştirme ve testler için bağımlılık
çeker, kaynağı yerel dosyalardan alır. Bu recipe ise kaynağı GitHub etiket
arşivinden indirir, test ve örnek derlemez, bağımlılık seçeneklerini zorlamaz.

| | Kök `conanfile.py` | Bu recipe |
|---|---|---|
| Kaynak | `exports_sources` (yerel) | `conandata.yml` (etiket arşivi) |
| `build_tests` / `build_examples` | Var | Yok (CCI'da yasak) |
| `zlib`, `sdl`, `harfbuzz`… shared zorlaması | Var (MinGW için) | Yok |
| `spdlog` | `header_only=True` | Tüketicinin seçimi |
| OpenGL | Sistem `find_package(OpenGL)` | `opengl/system` |

Bağımlılık sürümleri CCI index'inde **listelenen** sürümler olmalı
(`recipes/<paket>/config.yml`). Version range yalnızca CCI'ın izin verdiği
paketlerde kullanılabilir (cmake, zlib, OpenSSL…); `sdl` bu listede değil.

## Yerel deneme

```bash
cd packaging/conan-center/all

# Temel (MSVC'de profil cppstd'si 14 ise 17 verilmeli)
conan create . --version=1.4.0 --build=missing -s compiler.cppstd=17

# Diğer konfigürasyonlar
conan create . --version=1.4.0 --build=missing -s compiler.cppstd=17 -o "&:shared=True"
conan create . --version=1.4.0 --build=missing -s compiler.cppstd=17 -o "&:with_vulkan=True"
conan create . --version=1.4.0 --build=missing -s compiler.cppstd=17 -s build_type=Debug
conan create . --version=1.4.0 --build=missing -s compiler.cppstd=17 -o "spdlog/*:header_only=True"

# macOS açık bir hatayla reddedilmeli (ConanInvalidConfiguration)
conan graph info . --version=1.4.0 -s os=Macos -s compiler=apple-clang -s compiler.version=15 -s compiler.libcxx=libc++
```

## Etiketlenmemiş HEAD'i sınamak

Recipe kaynağı etiketten indirdiği için yukarıdaki komutlar son etiketi derler.
Etiket atmadan önce HEAD'i sınamak için yerel arşiv ve geçici bir
`conandata.yml` kullan; Conan `file:///` URL'lerini destekliyor:

```bash
VER=$(grep -oP 'kVersionString = "\K[^"]+' include/sdl_painter/version.h)
TMP=$(mktemp -d)
cp -r packaging/conan-center/all/. "$TMP"
# Commit'lenmemiş değişiklikler de girsin diye geçici bir stash commit'i kullanılır
REF=$(git stash create); REF=${REF:-HEAD}
git archive --format=tar.gz --prefix="sdl-painter-$VER/" -o "$TMP/src.tar.gz" "$REF"
SHA=$(sha256sum "$TMP/src.tar.gz" | cut -d' ' -f1)
cat > "$TMP/conandata.yml" <<EOF
sources:
  "$VER":
    url: "file:///$(cygpath -m "$TMP/src.tar.gz" 2>/dev/null || echo "$TMP/src.tar.gz")"
    sha256: "$SHA"
EOF
conan create "$TMP" --version="$VER" --user=local --channel=head --build=missing -s compiler.cppstd=17
```

`--user=local --channel=head`, önbellekteki etiketten derlenmiş paketle
karışmasını önler.

## Yeni sürüm

1. Etiket ve GitHub Release yayımlandıktan sonra kaynak arşivini üret, Release'e
   asset olarak yükle ve sha256'sını al. GitHub'ın otomatik arşivi yerine
   yüklenen asset kullanılır; sha256'sı değişmez:
   ```bash
   git archive --format=tar.gz --prefix=sdl-painter-X.Y.Z/ -o sdl-painter-X.Y.Z.tar.gz vX.Y.Z
   curl -sL https://github.com/yazilimperver/sdl-painter/releases/download/vX.Y.Z/sdl-painter-X.Y.Z.tar.gz | sha256sum
   ```
2. `all/conandata.yml`'a yeni sürümü ekle. `config.yml`'a satır ekle; ilk PR'da
   yalnızca en son sürüm bulunmalı.
3. CCI fork'unda (`D:\home-repos\conan-center-index`), `master`'dan dal aç ve
   bu dizini `recipes/sdl_painter/` altına kopyala (repo kökünden, yani
   `D:\home-repos\sdl-painter\code`'dan):
   ```bash
   git -C ../../conan-center-index switch -c package/sdl_painter upstream/master
   mkdir -p ../../conan-center-index/recipes/sdl_painter
   cp -r packaging/conan-center/config.yml packaging/conan-center/all \
         ../../conan-center-index/recipes/sdl_painter/
   ```
   `README.md` CCI'a kopyalanmaz.
4. PR başlığı: ilk sürümde `sdl_painter/X.Y.Z: add recipe`, sonrakilerde
   `sdl_painter/X.Y.Z: new version`.
