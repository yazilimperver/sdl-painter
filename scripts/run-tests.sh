#!/usr/bin/env bash
# GTest birim testlerini çalıştırır.
# Kullanım:
#   ./scripts/run-tests.sh                       # Tüm testler, Debug
#   ./scripts/run-tests.sh Release               # Tüm testler, Release
#   ./scripts/run-tests.sh ASan                  # Sanitizer'lı koşum
#   ./scripts/run-tests.sh --filter Transform    # Sadece TransformTest.* eşleşenler
#
# Pozisyonel: [Debug|Release|ASan]   (varsayılan: Debug)
# Bayraklar : --filter <regex>

set -euo pipefail

BUILD_TYPE="Debug"
FILTER=""

# İlk pozisyonel argüman varsa build type, sonrası bayrak.
if [[ $# -gt 0 && ! "$1" =~ ^-- ]]; then
    BUILD_TYPE="$1"
    shift
fi

while [[ $# -gt 0 ]]; do
    case "$1" in
        --filter)
            [[ $# -ge 2 ]] || { echo "Hata: --filter için değer gerekli." >&2 ; exit 1; }
            FILTER="$2" ; shift 2 ;;
        *) echo "Hata: Bilinmeyen bayrak '$1'." >&2 ; exit 1 ;;
    esac
done

case "${BUILD_TYPE}" in
    Debug)   PRESET="linux-debug" ;;
    Release) PRESET="linux-release" ;;
    ASan)    PRESET="linux-debug-asan" ;;
    *) echo "Hata: Bilinmeyen build type '${BUILD_TYPE}'. Debug|Release|ASan kullanın." >&2 ; exit 1 ;;
esac

CTEST_ARGS=(--preset "${PRESET}" --output-on-failure)
if [[ -n "${FILTER}" ]]; then
    CTEST_ARGS+=(-R "${FILTER}")
    echo ">>> Testler çalıştırılıyor: preset=${PRESET} filter=/${FILTER}/"
else
    echo ">>> Testler çalıştırılıyor: preset=${PRESET}"
fi

ctest "${CTEST_ARGS[@]}"

echo ">>> Testler tamamlandı."
