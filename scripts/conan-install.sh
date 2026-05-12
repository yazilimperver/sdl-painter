#!/usr/bin/env bash
# Conan bağımlılıklarını yükler.
# Kullanım:
#   ./scripts/conan-install.sh                                  # Debug, varsayılan
#   ./scripts/conan-install.sh Release                          # Release
#   ./scripts/conan-install.sh Debug --vulkan                   # Vulkan backend
#   ./scripts/conan-install.sh Release --vulkan --no-examples   # Birleşik
#
# Pozisyonel: [Debug|Release|ASan]   (varsayılan: Debug)
# Bayraklar : --vulkan  --no-examples  --no-tests

set -euo pipefail

BUILD_TYPE="Debug"
WITH_VULKAN=0
NO_EXAMPLES=0
NO_TESTS=0

# İlk pozisyonel argüman varsa build type, sonrası bayrak.
if [[ $# -gt 0 && ! "$1" =~ ^-- ]]; then
    BUILD_TYPE="$1"
    shift
fi

while [[ $# -gt 0 ]]; do
    case "$1" in
        --vulkan)      WITH_VULKAN=1 ; shift ;;
        --no-examples) NO_EXAMPLES=1 ; shift ;;
        --no-tests)    NO_TESTS=1    ; shift ;;
        *) echo "Hata: Bilinmeyen bayrak '$1'." >&2 ; exit 1 ;;
    esac
done

# Conan'a verilecek build_type ASan'da Debug olur, preset adıysa ayrı.
CONAN_BUILD_TYPE="${BUILD_TYPE}"
case "${BUILD_TYPE}" in
    Debug)   PRESET="linux-debug" ;;
    Release) PRESET="linux-release" ;;
    ASan)    PRESET="linux-debug-asan" ; CONAN_BUILD_TYPE="Debug" ;;
    *) echo "Hata: Bilinmeyen build type '${BUILD_TYPE}'. Debug|Release|ASan kullanın." >&2 ; exit 1 ;;
esac

OUTPUT_FOLDER="build/${PRESET}/generators"

# Conan opsiyonlarını array olarak topla; hiç bayrak yoksa boş kalır.
CONAN_OPTS=()
[[ ${WITH_VULKAN} -eq 1 ]] && CONAN_OPTS+=(-o "sdl_painter/*:with_vulkan=True")
[[ ${NO_EXAMPLES} -eq 1 ]] && CONAN_OPTS+=(-o "sdl_painter/*:build_examples=False")
[[ ${NO_TESTS}    -eq 1 ]] && CONAN_OPTS+=(-o "sdl_painter/*:build_tests=False")

if [[ ${#CONAN_OPTS[@]} -gt 0 ]]; then
    OPTS_SUMMARY="${CONAN_OPTS[*]}"
else
    OPTS_SUMMARY="(varsayılan)"
fi

echo ">>> Conan install: build_type=${CONAN_BUILD_TYPE}, output=${OUTPUT_FOLDER}, opts=${OPTS_SUMMARY}"
conan install . \
    --output-folder="${OUTPUT_FOLDER}" \
    --build=missing \
    -s build_type="${CONAN_BUILD_TYPE}" \
    "${CONAN_OPTS[@]}"

echo ">>> Conan install tamamlandı."
