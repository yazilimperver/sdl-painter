#!/usr/bin/env bash
# CHANGELOG.md içinden tek bir sürümün bölümünü çıkarır.
#
# Kullanım:
#   ./scripts/changelog-section.sh v1.1.0
#   ./scripts/changelog-section.sh 1.1.0 > RELEASE_NOTES.md
#
# Neden: GitHub Release notu daha önce CHANGELOG.md'nin TAMAMIYLA
# dolduruluyordu; 1.1.0 sürümünün notunda 1.0.0 ve boş "Yayınlanmadı"
# başlıkları da görünüyordu. Bu script yalnızca ilgili bölümü verir.
#
# Sürüm bulunamazsa hata verir — sessizce boş release notu üretmektense
# pipeline'ın durması yeğdir.

set -euo pipefail

if [ $# -ne 1 ]; then
    echo "Kullanim: $0 <surum>   (or: v1.1.0 / 1.1.0)" >&2
    exit 2
fi

# Baştaki 'v' varsa at: v1.1.0 -> 1.1.0
version="${1#v}"

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
changelog="${repo_root}/CHANGELOG.md"

if [ ! -f "$changelog" ]; then
    echo "HATA: CHANGELOG.md bulunamadi: $changelog" >&2
    exit 1
fi

# '## [1.1.0]' basligindan bir sonraki '## [' basligina kadar olan govdeyi al.
# Baslik satirinin kendisi cikarilir; GitHub Release basligi zaten surumu yaziyor.
section="$(awk -v ver="$version" '
    $0 ~ "^## \\[" ver "\\]" { found = 1; next }
    found && /^## \[/        { exit }
    found                    { print }
' "$changelog")"

# Bastaki/sondaki bos satirlari kirp.
section="$(printf '%s\n' "$section" | sed -e '/./,$!d' | sed -e :a -e '/^\n*$/{$d;N;ba' -e '}')"

if [ -z "$section" ]; then
    echo "HATA: CHANGELOG.md icinde '## [${version}]' bolumu bulunamadi veya bos." >&2
    echo "Mevcut basliklar:" >&2
    grep -n '^## \[' "$changelog" >&2 || true
    exit 1
fi

printf '%s\n' "$section"
printf '\n---\n\nTum degisiklikler: [CHANGELOG.md](CHANGELOG.md)\n'
