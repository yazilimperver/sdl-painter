#!/usr/bin/env bash
# Surum etiketi olusturur ve (istenirse) origin'e gonderir.
#
# Kullanim:
#   ./scripts/release-tag.sh              # kontroller + yerel etiket
#   ./scripts/release-tag.sh --push       # kontroller + yerel etiket + push
#   ./scripts/release-tag.sh --check-only # yalnizca kontroller, etiket yok
#
# Surum ELLE VERILMEZ: include/sdl_painter/version.h okunur

set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$repo_root"

push_tag=false
check_only=false
case "${1:-}" in
    --push)       push_tag=true ;;
    --check-only) check_only=true ;;
    "")           ;;
    *)
        echo "Kullanim: $0 [--push|--check-only]" >&2
        exit 2
        ;;
esac

fail() { echo "HATA: $*" >&2; exit 1; }
ok()   { echo "  [ok] $*"; }

echo ">>> Surum etiketi kontrolleri"

# 1) Surum, tek kaynagindan okunur; sayisal bilesenlerle metin ayrisamaz
#    (CMake configure asamasinda ayni kontrolu yapar, burada erken yakaliyoruz).
version_header="include/sdl_painter/version.h"
[ -f "$version_header" ] || fail "$version_header bulunamadi."

read_component() {
    local component="$1" value
    value="$(sed -n "s/.*constexpr int32_t kVersion${component}[[:space:]]*=[[:space:]]*\([0-9]\+\).*/\1/p" "$version_header")"
    [ -n "$value" ] || fail "kVersion${component} okunamadi: $version_header"
    printf '%s' "$value"
}
version="$(read_component Major).$(read_component Minor).$(read_component Patch)"
version_string="$(sed -n 's/.*kVersionString[[:space:]]*=[[:space:]]*"\([^"]*\)".*/\1/p' "$version_header")"
[ -n "$version_string" ] || fail "kVersionString okunamadi: $version_header"
[ "$version_string" = "$version" ] || \
    fail "version.h tutarsiz: kVersionString=\"$version_string\", sayisal bilesenler=$version"
tag="v${version}"
ok "surum $version (version.h)"

# 2) Calisma agaci temiz olmali -- etiket, commit'lenmemis degisiklikleri kapsamaz.
[ -z "$(git status --porcelain)" ] || fail "calisma agaci kirli; once commit'le veya temizle."
ok "calisma agaci temiz"

# 3) Doxyfile surumu (Doxygen header okuyamadigi icin degeri kendi tutar).
doxy_version="$(sed -n 's/^PROJECT_NUMBER[[:space:]]*=[[:space:]]*\(.*\)/\1/p' Doxyfile | tr -d '[:space:]')"
[ "$doxy_version" = "$version" ] || \
    fail "Doxyfile PROJECT_NUMBER=$doxy_version, beklenen $version."
ok "Doxyfile PROJECT_NUMBER=$version"

# 4) README'lerdeki FetchContent GIT_TAG ornegi guncel mi?
for readme in README.md README.tr.md; do
    grep -q "GIT_TAG[[:space:]]*${tag})" "$readme" || \
        fail "$readme icindeki GIT_TAG $tag degil (kullanicilara eski surumu gosterir)."
done
ok "README GIT_TAG ornekleri $tag"

# 5) CHANGELOG bolumu -- release notunu bu uretiyor, bos olamaz.
#    (changelog-section.sh bolum yoksa zaten hata verir.)
bash scripts/changelog-section.sh "$version" > /dev/null || \
    fail "CHANGELOG.md icinde '## [$version]' bolumu yok. '## [Yayinlanmadi]' basligini muhurledin mi?"
ok "CHANGELOG bolumu var ($(bash scripts/changelog-section.sh "$version" | wc -l) satir release notu)"

# 6) CI atlama anahtar kelimeleri -- betigin varlik sebebi (yukaridaki nota bak).
head_message="$(git log -1 --format=%B)"
if printf '%s' "$head_message" | grep -qiE '\[(skip ci|ci skip|no ci|skip actions|actions skip)\]'; then
    fail "etiketlenecek commit mesaji CI atlama anahtar kelimesi iceriyor:
    $(git log -1 --format='%h %s')
  Etiket push'u da bir push olayidir; workflow -- release job'i dahil -- atlanir.
  Cozum: bos bir commit at ve etiketi ona tasi:
    git commit --allow-empty -m \"chore: ${tag} surumu yayinlaniyor\"
    git push origin main"
fi
ok "commit mesajinda CI atlama anahtar kelimesi yok"

# 7) Etiket, remote'ta bulunan bir commit'i gostermeli; aksi halde release
#    job'i checkout edemez.
git fetch --quiet --tags origin
branch="$(git rev-parse --abbrev-ref HEAD)"
[ "$branch" = "main" ] || fail "surum yalnizca main'den etiketlenir (su an: $branch)."
[ "$(git rev-parse HEAD)" = "$(git rev-parse origin/main)" ] || \
    fail "HEAD ile origin/main ayni degil; once 'git push origin main'."
ok "main origin ile ayni noktada"

# 8) Remote'ta ayni etiket varsa dur -- yayinlanmis etiket oynatilmaz.
if git ls-remote --exit-code --tags origin "refs/tags/${tag}" > /dev/null 2>&1; then
    fail "$tag zaten origin'de yayinlanmis. Yeni sürüm için version.h'yi yukselt."
fi
ok "$tag remote'ta yok"

if $check_only; then
    echo ">>> Kontroller basarili ($tag). --check-only verildi, etiket olusturulmadi."
    exit 0
fi

# Yerel etiket: ayni commit'te ise yeniden kullanilir, farkli commit'te ise
# sessizce tasinmaz -- kullanici bilerek silmeli.
if git rev-parse --verify --quiet "refs/tags/${tag}" > /dev/null; then
    if [ "$(git rev-list -n1 "$tag")" = "$(git rev-parse HEAD)" ]; then
        ok "$tag yerelde zaten var ve HEAD'i gosteriyor"
    else
        fail "$tag yerelde var ama baska bir commit'i ($(git rev-list -n1 --abbrev-commit "$tag")) gosteriyor.
  Tasimak istiyorsan once sil: git tag -d $tag"
    fi
else
    # Annotated (v1.0.0 lightweight'ti, sonrakiler annotated -- tarih ve yazar tasisin).
    git tag -a "$tag" -m "SDLPainter ${tag}"
    ok "$tag olusturuldu (annotated, $(git rev-parse --short HEAD))"
fi

if ! $push_tag; then
    echo ">>> Yerel etiket hazir. Yayinlamak icin:"
    echo "      ./scripts/release-tag.sh --push     (veya: git push origin $tag)"
    exit 0
fi

echo ">>> $tag origin'e gonderiliyor (release job'i tetiklenecek)..."
git push origin "$tag"
echo ">>> Gonderildi. Pipeline: https://github.com/yazilimperver/sdl-painter/actions"
