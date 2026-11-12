# Generates the mouse pointer of Caucasian Eagle: a dark blade-shaped arrow with a gold rim, in the
# ink and gold of the HUD (art/ui/build_hud.py).
#
#   game/assets/ui/Cursor/Pointer.cur   32, 48 and 64 pixel images, hot spot at the tip
#
# The engine sets it as the window's pointer (DoorwaysGameSession::Cursor) and picks the size for the screen.
# Drawn here from shapes: no outside images.
#
# Usage: python art/ui/build_cursor.py            (needs Pillow)
#        python art/ui/build_cursor.py --sheet X  also writes an enlarged preview to X (for review)
import os
import struct
import sys

from PIL import Image, ImageDraw, ImageFilter

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
OUT = os.path.join(REPO, "game", "assets", "ui", "Cursor", "Pointer.cur")
SIZES = (32, 48, 64)
SS = 8                                  # supersampling

INK = (10, 22, 24, 255)                 # the inside of the HUD's wells
GOLD = (222, 176, 90, 255)
GOLD_LIGHT = (255, 232, 164, 255)

# the pointer in a 32 x 32 box, tip at (1.5, 1.5): a long leaf with a notch, like the tip of her blade
OUTLINE = [(1.5, 1.5), (4.0, 22.5), (9.0, 17.2), (14.0, 27.5), (18.2, 25.4), (13.2, 15.4), (20.8, 14.6)]
RIDGE = [(3.6, 4.6), (9.6, 15.2)]       # the bright line down the blade


def draw(size):
    k = size / 32.0 * SS
    big = size * SS
    pts = [(x * k, y * k) for x, y in OUTLINE]
    shadow = Image.new("L", (big, big), 0)
    ImageDraw.Draw(shadow).polygon([(x + 1.2 * k, y + 1.6 * k) for x, y in pts], fill=150)
    shadow = shadow.filter(ImageFilter.GaussianBlur(1.1 * k))
    image = Image.new("RGBA", (big, big), (0, 0, 0, 0))
    image.putalpha(shadow)
    d = ImageDraw.Draw(image)
    d.polygon(pts, fill=GOLD)
    d.line(pts + [pts[0]], fill=GOLD, width=max(1, int(1.5 * k)), joint="curve")
    # the ink inside: the same shape drawn smaller toward its middle
    cx = sum(x for x, _ in pts) / len(pts)
    cy = sum(y for _, y in pts) / len(pts)
    inner = [(cx + (x - cx) * .74 - .25 * k, cy + (y - cy) * .74 - .1 * k) for x, y in pts]
    d.polygon(inner, fill=INK)
    d.line([(x * k, y * k) for x, y in RIDGE], fill=GOLD_LIGHT, width=max(1, int(.9 * k)))
    return image.resize((size, size), Image.LANCZOS)


def entry(image):
    """One cursor image as a 32-bit DIB with its (empty) 1-bit mask."""
    w, h = image.size
    pixels = image.load()
    xor = bytearray()
    for y in range(h - 1, -1, -1):
        for x in range(w):
            r, g, b, a = pixels[x, y]
            xor += bytes((b, g, r, a))
    mask = bytes(((w + 31) // 32) * 4) * h
    header = struct.pack("<IiiHHIIiiII", 40, w, h * 2, 1, 32, 0, len(xor) + len(mask), 0, 0, 0, 0)
    return header + bytes(xor) + mask


def main():
    images = [draw(size) for size in SIZES]
    blobs = [entry(image) for image in images]
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    offset = 6 + 16 * len(blobs)
    out = struct.pack("<HHH", 0, 2, len(blobs))
    for size, blob in zip(SIZES, blobs):
        hot = max(1, round(1.5 * size / 32))
        out += struct.pack("<BBBBHHII", size, size, 0, 0, hot, hot, len(blob), offset)
        offset += len(blob)
    with open(OUT, "wb") as f:
        f.write(out + b"".join(blobs))
    print("wrote", OUT, len(out) + sum(len(b) for b in blobs), "bytes")
    if "--sheet" in sys.argv:
        sheet = Image.new("RGBA", (64 * 4 * 3 + 40, 64 * 4 + 20), (28, 44, 48, 255))
        x = 10
        for image in images:
            big = image.resize((image.width * 4, image.height * 4), Image.NEAREST)
            sheet.alpha_composite(big, (x, 10))
            x += 64 * 4 + 10
        sheet.save(sys.argv[sys.argv.index("--sheet") + 1])


if __name__ == "__main__":
    main()
