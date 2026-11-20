"""All review sheets' first two shots in one grid: python -I art/trees/grid.py <out.jpg> <stem>... (stem: build/trees/review/<species>_<seed>_<lod>)."""
import sys
from PIL import Image
out, stems = sys.argv[1], sys.argv[2:]
cells = []
for st in stems:
    for k in ("0", "2"):
        im = Image.open("%s_%s.png" % (st, k)).convert("RGB"); cells.append(im.resize((int(im.width * 330 / im.height), 330)))
cols = 8; w = cells[0].width
g = Image.new("RGB", (w * cols, 330 * ((len(cells) + cols - 1) // cols)))
for i, c in enumerate(cells): g.paste(c, ((i % cols) * w, (i // cols) * 330))
g.save(out, quality=88)
