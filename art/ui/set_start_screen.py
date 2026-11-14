"""The start screen's picture from a finished painting (not painted by build_start_screen.py any more).

    python art/ui/set_start_screen.py [picture]

Default picture: art/map/MapA_MapLab_Package/references/background.png (Map A, the eagle shrine above the coast).
It is scaled to cover 1920 x 1080, cut to that size about its centre, and written to game/assets/ui/Menu/StartScreen.png.
"""
import os, sys
from PIL import Image

root = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
source = sys.argv[1] if len(sys.argv) > 1 else os.path.join(root, "art/map/MapA_MapLab_Package/references/background.png")
W, H = 1920, 1080
picture = Image.open(source).convert("RGB")
scale = max(W / picture.width, H / picture.height)
picture = picture.resize((round(picture.width * scale), round(picture.height * scale)), Image.LANCZOS)
left, top = (picture.width - W) // 2, (picture.height - H) // 2
picture.crop((left, top, left + W, top + H)).save(os.path.join(root, "game/assets/ui/Menu/StartScreen.png"))
print("StartScreen.png from", source, "scaled", round(scale, 3))
