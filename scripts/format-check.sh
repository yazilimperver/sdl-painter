#!/usr/bin/env bash
# clang-format-18 ile format kontrolü yapar (CI'daki ile aynı).
# Kullanım: ./scripts/format_check.sh [--fix]

set -euo pipefail

FIX_MODE=false
if [[ "${1:-}" == "--fix" ]]; then
    FIX_MODE=true
fi

FILES=$(find include/ src/ -name '*.h' -o -name '*.cpp' | sort)

if $FIX_MODE; then
    echo ">>> Format düzeltiliyor..."
    echo "${FILES}" | xargs clang-format-18 -i
    echo ">>> Format düzeltmesi tamamlandı."
else
    echo ">>> Format kontrol ediliyor..."
    echo "${FILES}" | xargs clang-format-18 --dry-run --Werror
    echo ">>> Format kontrolü başarılı."
fi
