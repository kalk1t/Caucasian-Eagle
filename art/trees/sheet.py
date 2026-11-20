"""Puts a review's pictures side by side: python -I art/trees/sheet.py <out.jpg> <picture>... (each scaled to 450 high)."""
import sys
from PIL import Image
out, files = sys.argv[1], sys.argv[2:]
ims = [Image.open(f).convert("RGB") for f in files]
ims = [im.resize((int(im.width * 450 / im.height), 450)) for im in ims]
sheet = Image.new("RGB", (sum(im.width for im in ims), 450))
x = 0
for im in ims: sheet.paste(im, (x, 0)); x += im.width
sheet.save(out, quality=90)
