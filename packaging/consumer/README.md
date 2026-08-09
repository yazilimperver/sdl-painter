# Tüketici Doğrulama Projesi

SDLPainter'ı **dışarıdan tüketen** minimal bir CMake projesi. Reponun kendi
CMake yapısına hiçbir şekilde bağlı değildir — bir kullanıcının yapacağının
aynısını yapar.

## Ne kanıtlıyor

Kütüphanenin derlenmesi, *kullanılabilir olduğu* anlamına gelmiyor. Bu proje
aradaki farkı kapatır:

1. Public header'lar kurulum ağacından bulunabiliyor
2. Kütüphane link ediliyor ve **derlenmiş sembol** çözülüyor (`CreateRenderer()`)
3. `find_package` ve `add_subdirectory` yolları **aynı hedef ismini** veriyor
4. Windows'ta README'de önerilen runtime-DLL deseni gerçekten çalışıyor

## Çalıştırma

```bash
# 1) Kütüphaneyi kur
cmake --install <build-dir> --prefix /tmp/sp-prefix

# 2) find_package yolu
cmake -S packaging/consumer -B /tmp/consumer \
      -DCMAKE_TOOLCHAIN_FILE=<conan_toolchain.cmake> \
      -DCMAKE_BUILD_TYPE=Debug \
      -DCMAKE_PREFIX_PATH=/tmp/sp-prefix \
      -DCONSUMER_MODE=find_package
cmake --build /tmp/consumer && /tmp/consumer/consumer

# 3) add_subdirectory yolu (FetchContent esdegeri)
cmake -S packaging/consumer -B /tmp/consumer-sub \
      -DCMAKE_TOOLCHAIN_FILE=<conan_toolchain.cmake> \
      -DCMAKE_BUILD_TYPE=Debug \
      -DCONSUMER_MODE=subdirectory \
      -DSDLPAINTER_SOURCE_DIR=<repo-kok-dizini>
```

CI'da `build:standalone-cmake` job'ı 1. ve 2. adımı otomatik koşar.

## Neden `packaging/` altında

Bu proje CI tarafından kullanıldığı için **repoda takip edilmek zorunda**.
Önce geçici bir çalışma dizinine konmuştu; orası `.gitignore`'da olduğu için
CI "source directory does not exist" ile düştü.
