# Generates the program icon of Caucasian Eagle: the Eagle before the moon, cut from the rendered
# start picture (art/ui/export/CoverRender.png, made by art/ui/render_cover.py), in a gold ring.
#
#   game/CaucasianEagle.ico    16, 24, 32, 48, 64, 128 and 256 pixel images
#
# The icon is compiled into CaucasianEagle.exe by game/CaucasianEagle.rc and used by the installer.
#
# Usage: python art/ui/build_icon.py            (needs Pillow)
#        python art/ui/build_icon.py --sheet X  also writes a preview to X (for review)
import os
import sys

from PIL import Image, ImageDraw

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
SOURCE = os.path.join(REPO, "art", "ui", "export", "CoverRender.png")
FALLBACK = os.path.join(REPO, "game", "assets", "ui", "Menu", "StartScreen.png")
OUT = os.path.join(REPO, "game", "CaucasianEagle.ico")
SIZES = (16, 24, 32, 48, 64, 128, 256)

# the moon in the 1920 x 1080 picture: centre and radius, in pixels
MOON = (1213, 212, 163)
INK = (8, 20, 26, 255)
GOLD = (222, 176, 90, 255)
BIG = 1024


def main():
    path = SOURCE if os.path.exists(SOURCE) else FALLBACK
    picture = Image.open(path).convert("RGBA")
    k = picture.width / 1920.0
    cx, cy, r = (v * k for v in MOON)
    reach = r * 1.02
    moon = picture.crop((int(cx - reach), int(cy - reach), int(cx + reach), int(cy + reach)))

    icon = Image.new("RGBA", (BIG, BIG), (0, 0, 0, 0))
    d = ImageDraw.Draw(icon)
    d.ellipse((8, 8, BIG - 8, BIG - 8), fill=GOLD)                       # the ring
    d.ellipse((52, 52, BIG - 52, BIG - 52), fill=INK)
    inner = BIG - 2 * 76
    moon = moon.resize((inner, inner), Image.LANCZOS)
    mask = Image.new("L", (inner, inner), 0)
    ImageDraw.Draw(mask).ellipse((0, 0, inner, inner), fill=255)
    icon.paste(moon, (76, 76), mask)

    icon.save(OUT, format="ICO", sizes=[(s, s) for s in SIZES])
    print("wrote", OUT, os.path.getsize(OUT), "bytes, from", os.path.basename(path))
    if "--sheet" in sys.argv:
        sheet = Image.new("RGBA", (256 + 128 + 64 + 48 + 32 + 16 + 70, 276), (40, 60, 66, 255))
        x = 10
        for s in (256, 128, 64, 48, 32, 16):
            sheet.alpha_composite(icon.resize((s, s), Image.LANCZOS), (x, 10))
            x += s + 10
        sheet.save(sys.argv[sys.argv.index("--sheet") + 1])


if __name__ == "__main__":
    main()
