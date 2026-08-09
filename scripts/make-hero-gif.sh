#!/usr/bin/env bash
#
# hero demosunun PPM karelerinden README tanitim GIF'ini uretir.
#
# Once kareleri uret:
#   ./build/linux-debug/examples/hero --dump-frames build/hero_frames
# Sonra:
#   ./scripts/make-hero-gif.sh
#
# Iki gecisli palet kullanilir: once sahneye ozel 256 renklik palet cikarilir
# (palettegen), sonra kareler o palete eslenir (paletteuse). Tek gecisli
# donusum bu sahnedeki degradelerde gorunur bantlanma yapiyor.
#
# Kullanim:
#   ./scripts/make-hero-gif.sh [--frames DIR] [--out FILE] [--fps N]
#                              [--width N] [--mp4]

set -euo pipefail

FRAMES_DIR="build/hero_frames"
OUTPUT="doc/hero.gif"
FPS=15
WIDTH=720
MAKE_MP4=0

while [[ $# -gt 0 ]]; do
    case "$1" in
        --frames) FRAMES_DIR="$2"; shift 2 ;;
        --out)    OUTPUT="$2";     shift 2 ;;
        --fps)    FPS="$2";        shift 2 ;;
        --width)  WIDTH="$2";      shift 2 ;;
        --mp4)    MAKE_MP4=1;      shift ;;
        -h|--help) sed -n '2,20p' "$0"; exit 0 ;;
        *) echo "Bilinmeyen secenek: $1" >&2; exit 1 ;;
    esac
done

if ! command -v ffmpeg >/dev/null 2>&1; then
    echo "ffmpeg bulunamadi. Kurulum:" >&2
    echo "    sudo apt install ffmpeg      # Debian/Ubuntu" >&2
    echo "    brew install ffmpeg          # macOS" >&2
    exit 1
fi

if [ ! -d "$FRAMES_DIR" ]; then
    echo "Kare dizini yok: $FRAMES_DIR" >&2
    echo "Once: hero --dump-frames $FRAMES_DIR" >&2
    exit 1
fi

count=$(find "$FRAMES_DIR" -name 'frame_*.ppm' | wc -l)
if [ "$count" -eq 0 ]; then
    echo "$FRAMES_DIR icinde frame_*.ppm yok." >&2
    echo "Once: hero --dump-frames $FRAMES_DIR" >&2
    exit 1
fi
echo "$count kare bulundu."

mkdir -p "$(dirname "$OUTPUT")"

PALETTE="$(mktemp -t sdlpainter_hero_palette.XXXXXX.png)"
trap 'rm -f "$PALETTE"' EXIT

INPUT="$FRAMES_DIR/frame_%04d.ppm"
FILTERS="fps=$FPS,scale=${WIDTH}:-1:flags=lanczos"

echo "1/2  palet cikariliyor..."
ffmpeg -y -loglevel error -framerate 30 -i "$INPUT" \
    -vf "$FILTERS,palettegen=stats_mode=diff" "$PALETTE"

echo "2/2  GIF uretiliyor..."
ffmpeg -y -loglevel error -framerate 30 -i "$INPUT" -i "$PALETTE" \
    -lavfi "$FILTERS[x];[x][1:v]paletteuse=dither=bayer:bayer_scale=3" \
    -loop 0 "$OUTPUT"

size_mb=$(awk "BEGIN {printf \"%.2f\", $(stat -c%s "$OUTPUT" 2>/dev/null || stat -f%z "$OUTPUT") / 1048576}")
echo "$OUTPUT  ->  ${size_mb} MB  (${WIDTH}px, ${FPS} fps)"
awk "BEGIN {exit !($size_mb > 3)}" && \
    echo "UYARI: 3 MB hedefinin uzerinde. --fps 12 veya --width 640 deneyin, ya da --mp4." >&2

if [ "$MAKE_MP4" -eq 1 ]; then
    MP4_OUT="${OUTPUT%.*}.mp4"
    echo "mp4 uretiliyor..."
    # yuv420p + cift sayiya yuvarlama (-2): eski oynaticilar ve GitHub icin gerekli.
    ffmpeg -y -loglevel error -framerate 30 -i "$INPUT" \
        -vf "scale=${WIDTH}:-2:flags=lanczos" \
        -c:v libx264 -pix_fmt yuv420p -crf 20 -movflags +faststart "$MP4_OUT"
    mp4_mb=$(awk "BEGIN {printf \"%.2f\", $(stat -c%s "$MP4_OUT" 2>/dev/null || stat -f%z "$MP4_OUT") / 1048576}")
    echo "$MP4_OUT  ->  ${mp4_mb} MB"
fi
