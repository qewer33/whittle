#!/usr/bin/env bash
# Rebuilds the toolbar icon PNGs from Lucide (lucide.dev, ISC licensed).
#
# For each icon: grab the SVG, paint the stroke white (we tint it at runtime),
# rasterize it big, then hand the whole batch to normalize_icons.py to crop and
# rescale so they all end up the same visual size. Lucide's icons fill their
# canvas by pretty different amounts, so without that step some look chunky and
# others tiny.
#
# Needs: curl, inkscape, python3 + Pillow.
#
# Usage: ./gen_icons.sh [size] [fill]
#   size  final square PNG size in px           (default 16)
#   fill  content size as a fraction of `size`  (default 0.85)
set -euo pipefail

SIZE="${1:-16}"
FILL="${2:-0.85}"
HIRES=$((SIZE * 8))
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
OUT="$HERE/icons"
BASE="https://raw.githubusercontent.com/lucide-icons/lucide/main/icons"

mkdir -p "$OUT"

# index_name -> lucide icon name. The order here is the atlas index, so keep it
# lined up with the Icon enum in src/ui/icons.h.
icons=(
  "00_menu:menu"          "01_plus:plus"        "02_trash:trash-2"
  "03_undo:undo-2"        "04_redo:redo-2"      "05_vertex:circle-dot"
  "06_move:move"          "07_rotate:rotate-cw" "08_scale:scaling"
  "09_box:box"            "10_eye:eye"          "11_flip:flip-horizontal-2"
  "12_circle:circle"      "13_pyramid:pyramid"  "14_cylinder:cylinder"
  "15_square:square"      "16_save:save"        "17_load:folder-open"
  "18_exit:log-out"     "19_paint:paintbrush"  "20_image:image"
  "21_bucket:paint-bucket"  "22_pipette:pipette"
  "23_frame:frame"          "24_fit:maximize"
  "25_texture:layers"
  "26_layout:layout-grid"
  "27_edge:minus"
  "28_pencil:pencil"
  "29_extrude:square-arrow-up"
  "30_subdivide:grid-2x2"
  "31_split:git-commit-horizontal"
  "32_shade:sun"
  "33_minimize:minimize"
  "34_eraser:eraser"
  "35_more:ellipsis-vertical"
  "36_back:arrow-left"
  "37_export:upload"
)

tmp="$(mktemp -d)"
trap 'rm -rf "$tmp"' EXIT

# fetch + whiten + rasterize each icon into the temp dir
for entry in "${icons[@]}"; do
  out="${entry%%:*}"
  name="${entry##*:}"
  echo "  $name -> $out.png"
  curl -fsSL --max-time 20 "$BASE/$name.svg" -o "$tmp/$name.svg"
  sed 's/currentColor/#ffffff/g' "$tmp/$name.svg" > "$tmp/$name.w.svg"
  inkscape "$tmp/$name.w.svg" --export-type=png \
    --export-filename="$tmp/$out.big.png" -w "$HIRES" -h "$HIRES" >/dev/null 2>&1
done

# crop, rescale and recenter everything to a uniform size
python3 "$HERE/normalize_icons.py" "$tmp" "$OUT" "$SIZE" "$FILL"

echo "Done: $OUT (${#icons[@]} icons, ${SIZE}px, fill ${FILL})"
