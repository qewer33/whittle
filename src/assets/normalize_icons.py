#!/usr/bin/env python3
# Optically normalize the rasterized icons so they all read at the same size.
# Lucide icons fill their 24px canvas by very different amounts, so we crop each
# one to its real content, then rescale that content to a common target size and
# recenter it on a fresh transparent canvas.
#
# Usage: normalize_icons.py <tmp_dir> <out_dir> <size> <fill>
#   tmp_dir  holds the *.big.png hi-res rasters
#   out_dir  where the final <size>x<size> PNGs are written
#   size     final square size in px
#   fill     content's max dimension as a fraction of size

import sys, glob, os
from PIL import Image

tmp, out, size, fill = sys.argv[1], sys.argv[2], int(sys.argv[3]), float(sys.argv[4])
target = size * fill

for f in sorted(glob.glob(os.path.join(tmp, "*.big.png"))):
    base = os.path.basename(f).replace(".big.png", "")
    im = Image.open(f).convert("RGBA")
    # Force RGB to pure white everywhere (keep alpha) so downscaling can't bleed
    # black from transparent pixels into the edges, and tinting stays clean.
    a = im.getchannel("A")
    w = Image.new("L", im.size, 255)
    im = Image.merge("RGBA", (w, w, w, a))
    # Crop to real content, then rescale so max dimension == target, centered.
    bbox = a.getbbox()
    if bbox:
        im = im.crop(bbox)
    cw, ch = im.size
    s = target / max(cw, ch)
    nw, nh = max(1, round(cw * s)), max(1, round(ch * s))
    im = im.resize((nw, nh), Image.LANCZOS)
    canvas = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    canvas.paste(im, ((size - nw) // 2, (size - nh) // 2), im)
    canvas.save(os.path.join(out, base + ".png"))
