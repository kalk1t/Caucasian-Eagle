"""The looks' parts' textures in a lab bundle made small: 512 x 512 (a sword, a pauldron or a crown is small on the
screen), and their separate roughness and metallic maps 8 x 8, since a packed map is read before them (Color.hlsl:
DoorwaysSampleRoughness, DoorwaysSampleMetallic) and every look part has one. Her own textures are left as they are.

    python slim_textures.py <bundle>
"""
import os, re, sys
from PIL import Image

tex = os.path.join(sys.argv[1], "Textures")
names = sorted(os.listdir(tex))
packed = {m.group(1) for n in names for m in [re.match(r"(Material_\d+_Look_.+)_PackedMaterial\.png$", n)] if m}
before = after = 0
for n in names:
    m = re.match(r"(Material_\d+_Look_.+)_(BaseColor|Normal|PackedMaterial|Roughness|Metallic)\.png$", n)
    if not m:
        continue
    part, use = m.groups()
    size = 8 if use in ("Roughness", "Metallic") and part in packed else 512
    p = os.path.join(tex, n)
    before += os.path.getsize(p)
    im = Image.open(p)
    if im.width > size:
        im.resize((size, size), Image.LANCZOS).save(p, optimize=True)
    after += os.path.getsize(p)
print("look textures %.1f MB -> %.1f MB; parts with a packed map: %d" % (before / 2**20, after / 2**20, len(packed)))
