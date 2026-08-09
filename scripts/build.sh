#!/usr/bin/env bash
# Projeyi derler. Toolchain yoksa Conan install otomatik çalışır.
# Kullanım:
#   ./scripts/build.sh                                       # Debug, varsayılan
#   ./scripts/build.sh Release                               # Release
#   ./scripts/build.sh ASan                                  # Debug + ASan/UBSan
#   ./scripts/build.sh Debug --vulkan                        # Vulkan backend
#   ./scripts/build.sh Release --vulkan --no-examples        # Birleşik
#   ./scripts/build.sh Debug --target clipping               # Tek hedef
#   ./scripts/build.sh Debug --clean                         # build/<preset> sil, baştan
#   ./scripts/build.sh Debug --jobs 8                        # Paralel iş sayısı
#   ./scripts/build.sh Debug --skip-conan                    # Sadece cmake
#   ./scripts/build.sh Debug --docs                          # Build + Doxygen dokümantasyonu
#
# Pozisyonel: [Debug|Release|ASan]   (varsayılan: Debug)
# Bayraklar :
#   --vulkan         Vulkan backend etkin
#   --no-examples    Örnekleri build etme
#   --no-tests       Testleri build etme
#   --target <name>  Sadece belirli bir hedef
#   --clean          Build dizinini temizle
#   --jobs <N>       Paralel iş sayısı (cmake -j)
#   --skip-conan     Conan install'ı atla
#   --docs           Build sonrası Doxygen HTML dokümantasyonu üret

set -euo pipefail

BUILD_TYPE="Debug"
WITH_VULKAN=0
NO_EXAMPLES=0
NO_TESTS=0
SKIP_CONAN=0
CLEAN=0
TARGET=""
JOBS=0
WITH_DOCS=0

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
        --skip-conan)  SKIP_CONAN=1  ; shift ;;
        --clean)       CLEAN=1       ; shift ;;
        --target)
            [[ $# -ge 2 ]] || { echo "Hata: --target için değer gerekli." >&2 ; exit 1; }
            TARGET="$2" ; shift 2 ;;
        --jobs)
            [[ $# -ge 2 ]] || { echo "Hata: --jobs için değer gerekli." >&2 ; exit 1; }
            JOBS="$2" ; shift 2 ;;
        --docs)    WITH_DOCS=1   ; shift ;;
        *) echo "Hata: Bilinmeyen bayrak '$1'." >&2 ; exit 1 ;;
    esac
done

case "${BUILD_TYPE}" in
    Debug)   PRESET="linux-debug" ;;
    Release) PRESET="linux-release" ;;
    ASan)    PRESET="linux-debug-asan" ;;
    *) echo "Hata: Bilinmeyen build type '${BUILD_TYPE}'. Debug|Release|ASan kullanın." >&2 ; exit 1 ;;
esac

# Linux preset'lerinde toolchain yolu Conan layout'una göre build_type altında.
case "${BUILD_TYPE}" in
    Debug|ASan) CONAN_BUILD_DIR="Debug" ;;
    Release)    CONAN_BUILD_DIR="Release" ;;
esac

BUILD_DIR="build/${PRESET}"
TOOLCHAIN_FILE="${BUILD_DIR}/generators/build/${CONAN_BUILD_DIR}/generators/conan_toolchain.cmake"
STAMP_FILE="${BUILD_DIR}/.sdlpainter-options.stamp"

# Stale toolchain tespiti için opsiyon imzası.
OPTION_STAMP=""
[[ ${WITH_VULKAN} -eq 1 ]] && OPTION_STAMP+="vulkan,"
[[ ${NO_EXAMPLES} -eq 1 ]] && OPTION_STAMP+="no-examples,"
[[ ${NO_TESTS}    -eq 1 ]] && OPTION_STAMP+="no-tests,"
OPTION_STAMP="${OPTION_STAMP%,}"  # son virgülü at

if [[ ${CLEAN} -eq 1 ]]; then
    if [[ -d "${BUILD_DIR}" ]]; then
        echo ">>> Clean: ${BUILD_DIR} siliniyor"
        rm -rf "${BUILD_DIR}"
    else
        echo ">>> Clean: ${BUILD_DIR} zaten yok"
    fi
fi

# Conan install kararı
NEED_CONAN=0
if [[ ${SKIP_CONAN} -eq 0 ]]; then
    if [[ ! -f "${TOOLCHAIN_FILE}" ]]; then
        NEED_CONAN=1
        echo ">>> Toolchain bulunamadı, Conan install gerekli."
    elif [[ -f "${STAMP_FILE}" ]]; then
        PREVIOUS_OPTIONS="$(cat "${STAMP_FILE}" 2>/dev/null || true)"
        if [[ "${PREVIOUS_OPTIONS}" != "${OPTION_STAMP}" ]]; then
            NEED_CONAN=1
            echo ">>> Build opsiyonları değişti ('${PREVIOUS_OPTIONS}' -> '${OPTION_STAMP}'); Conan yeniden kuruluyor."
        fi
    else
        # Toolchain var, stamp yok -> bilinmeyen onceki opsiyonlar; guvenli olarak yeniden kur.
        NEED_CONAN=1
        echo ">>> Toolchain'e ait opsiyon damgası yok; Conan yeniden kuruluyor."
    fi
fi

if [[ ${NEED_CONAN} -eq 1 ]]; then
    CONAN_FWD=("${BUILD_TYPE}")
    [[ ${WITH_VULKAN} -eq 1 ]] && CONAN_FWD+=(--vulkan)
    [[ ${NO_EXAMPLES} -eq 1 ]] && CONAN_FWD+=(--no-examples)
    [[ ${NO_TESTS}    -eq 1 ]] && CONAN_FWD+=(--no-tests)

    "$(dirname "$0")/conan-install.sh" "${CONAN_FWD[@]}"

    mkdir -p "${BUILD_DIR}"
    printf '%s' "${OPTION_STAMP}" > "${STAMP_FILE}"
fi

echo ">>> Configure: preset=${PRESET}"
cmake --preset "${PRESET}"

# Build komutu
BUILD_ARGS=(--build --preset "${PRESET}")
if [[ -n "${TARGET}" ]]; then
    BUILD_ARGS+=(--target "${TARGET}")
    echo ">>> Build: preset=${PRESET} target=${TARGET}"
else
    echo ">>> Build: preset=${PRESET} (tüm hedefler)"
fi
[[ ${JOBS} -gt 0 ]] && BUILD_ARGS+=(-j "${JOBS}")

cmake "${BUILD_ARGS[@]}"

echo ">>> Build tamamlandı."

if [[ ${WITH_DOCS} -eq 1 ]]; then
    if ! command -v doxygen &>/dev/null; then
        echo "Hata: doxygen bulunamadı. Lütfen kurun (ör. apt install doxygen)." >&2
        exit 1
    fi
    echo ">>> Doxygen dokümantasyonu oluşturuluyor..."
    doxygen Doxyfile
    echo ">>> Dokümantasyon hazır: build/docs/index.html"
fi
