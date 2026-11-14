"""Turns the Meshy scenery library into game models: the places and things that stand beside the road.

    blender -b -P art/places/prepare_places.py -- <asset builder exe> [Name ...]

Reads   art/props/meshy/out/library/<source>.glb      the downloaded Meshy models (not in git: see art/README.md)
Writes  art/places/out/<Name>.glb, <Name>.png         the reduced model and a picture of it (not in git)
        game/assets/Compiled/Places/<Name>.dmesh      the game model
        game/assets/Compiled/Places/<Name>BaseColor.png   its colour map

For each model in MODELS: joins it, welds the seams Meshy leaves open, reduces it to its triangle budget, gives it
smooth normals with hard edges, stands it on y = 0 with its middle on the origin, and compiles it with the engine's
asset builder (static-pipeline). Where the models stand is art/places/build_places.py; run that after this.
With names given, only those models are made.
"""
import bpy, sys, os, math, shutil, subprocess, tempfile, pathlib
from mathutils import Vector

args = sys.argv[sys.argv.index("--") + 1:]
BUILDER, ONLY = args[0], args[1:]
REPO = pathlib.Path(__file__).resolve().parents[2]
LIBRARY = REPO / "art" / "props" / "meshy" / "out" / "library"
OUT = REPO / "art" / "places" / "out"
GAME = REPO / "game" / "assets" / "Compiled" / "Places"

# game name: (Meshy file, triangle budget, colour map pixels, hard-edge angle in degrees)
MODELS = {
    # ---- buildings and ruins
    "Chapel":       ("Church",                14000, 1024, 35),
    "Abbey":        ("Crimson_Spires_Abbey",  22000, 1024, 35),
    "Inn":          ("Cozy_Cottage",          14000, 1024, 35),
    "Farmhouse":    ("Sunlit_Cottage",         9000, 1024, 35),
    "WitchHouse":   ("Enchanted_Moss_Cottag", 14000, 1024, 40),
    "RuinedHouse":  ("Desolate_House_in_Rui", 14000, 1024, 35),
    "RuinedFort":   ("Stone_Fortress",        18000, 1024, 35),
    "Skullspire":   ("Skullspire_Citadel",    16000, 1024, 35),
    "FaceRock":     ("SkullRock_and_temple",  20000, 1024, 40),
    "Watchtower":   ("Overgrown_Watchtower",  14000, 1024, 35),
    "BrokenStair":  ("Staircase_Through_the", 14000, 1024, 35),
    "EmeraldGate":  ("Emerald_Gate_of_the_N", 16000, 1024, 35),
    "Archway":      ("Stone_Archway",          26000, 1024, 35),
    "OldArch":      ("Overgrown_Stone_Arch",   7000, 1024, 35),
    "Pavilion":     ("Ethereal_Arbor_Pavili", 12000, 1024, 40),
    "Dock":         ("Old_Dock",               7000, 1024, 35),
    "Plateau":      ("Stone_Plateau",          6000, 1024, 30),
    # ---- carved stone
    "Throne":       ("Throne_of_the_Stone_L",  9000, 1024, 35),
    "KingFountain": ("Moss_Crowned_King_Fou", 10000, 1024, 40),
    "Runestone":    ("Geometric_Circle_Mono",  5000,  512, 35),
    "GoldColumn":   ("Gilded_Corinthian_Col",  8000,  512, 40),
    "StoneWall":    ("Mossy_Stone_Block_Wal",  3000,  512, 30),
    "CobbleCircle": ("Mossy_Cobblestone_Cir",  3000,  512, 30),
    "Crag":         ("Kaya3",                  5000,  512, 30),
    "Golem":        ("Blue_Runic_Stone_Gole", 12000, 1024, 35),
    "Priestess":    ("Winged_Flame_Priestes", 12000, 1024, 50),
    # ---- things people left
    "Palisade":     ("Wooden_Fence",           14000,  512, 35),
    "Barrel":       ("Rustic_Barrel",          1400,  512, 40),
    "Crate":        ("Rustic_Wooden_Crate",    3000,  512, 30),
    "Stall":        ("Rustic_Market_Stall",    3500,  512, 35),
    "WeaponRack":   ("Midevil_weapon_rack_g",  16000,  512, 35),
    "Well":         ("water_well",             5000,  512, 35),
    "LampPost":     ("Rustic_Lantern_Glow",    1800,  512, 40),
    "Brazier":      ("Ornate_Bowl_02_Remast",  3000,  512, 45),
    "Rug":          ("Oriental_Rug_Old",        400,  512, 60),
    "Quiver":       ("Archer_s_Quiver",        8000,  512, 40),
    "ObsidianHeap": ("Emerald_Obsidian_Heap",  3000,  512, 30),
    "Hoard":        ("A_big_heaping_pile_of",  6000,  512, 40),
    "Rope":         ("Coiled_Rope",            2500,  512, 45),
    "Beam":         ("Weathered_Timber_Beam",   700,  512, 35),
    "Key":          ("Mysterious_Key",         3000,  512, 40),
    "Shackles":     ("Wall_Mounted_Shackles",  4000,  512, 40),
    "MapChest":     ("The_Cartographer_s_Ch",  6000,  512, 35),
    # ---- growing things
    "MossLog":      ("Mossy_Log_with_Bracke",  5000,  512, 45),
    "Stump":        ("Tree_Stump",             12000,  512, 45),
    "Pine":         ("A_towering_pine_tree",  14000, 1024, 60),
    "Shrub":        ("A_dense_forest_shrub",   4000,  512, 60),
    "GlowPlant":    ("Blue_Alien_Plant_04",    6000,  512, 50),
    "OldTree":      ("Eternal_Elegance",      12000,  512, 50),
    "TreeHouse":    ("The_Tree_of_Shinobi_T", 14000, 1024, 45),
    "Grove":        ("Clockwork_Grove",       16000, 1024, 45),
}


def base_colour_image(obj):
    """The picture wired to the material's base colour (Meshy files carry three or four pictures each)."""
    for slot in obj.material_slots:
        mat = slot.material
        if not mat or not mat.use_nodes: continue
        for node in mat.node_tree.nodes:
            if node.type != 'BSDF_PRINCIPLED': continue
            links = node.inputs["Base Color"].links
            if links and links[0].from_node.type == 'TEX_IMAGE' and links[0].from_node.image:
                mat.node_tree.nodes.active = links[0].from_node     # the workbench picture shows the active one
                return links[0].from_node.image
    return None


def picture(obj, path):
    sc = bpy.context.scene
    sc.render.engine = 'BLENDER_WORKBENCH'; sc.display.shading.light = 'STUDIO'; sc.display.shading.color_type = 'TEXTURE'
    sc.render.resolution_x = 420; sc.render.resolution_y = 420
    if sc.world is None: sc.world = bpy.data.worlds.new("w")
    sc.world.color = (.32, .34, .38)
    cos = [obj.matrix_world @ v.co for v in obj.data.vertices]
    lo = Vector((min(c[i] for c in cos) for i in range(3))); hi = Vector((max(c[i] for c in cos) for i in range(3)))
    mid, r = (lo + hi) / 2, max((hi - lo).length / 2, 1e-3)
    cam = bpy.data.objects.new("cam", bpy.data.cameras.new("cam")); sc.collection.objects.link(cam); sc.camera = cam
    cam.data.type = 'ORTHO'; cam.data.ortho_scale = r * 2.1; cam.data.clip_start = r * .01; cam.data.clip_end = r * 20
    a, e = math.radians(35), math.radians(22)
    cam.location = mid + Vector((math.sin(a) * math.cos(e), -math.cos(a) * math.cos(e), math.sin(e))) * r * 6
    cam.rotation_euler = (mid - cam.location).to_track_quat('-Z', 'Y').to_euler()
    sc.render.filepath = path; bpy.ops.render.render(write_still=True)
    bpy.data.objects.remove(cam)


OUT.mkdir(parents=True, exist_ok=True); GAME.mkdir(parents=True, exist_ok=True)
work = pathlib.Path(tempfile.mkdtemp(prefix="ce_places_"))
failed, made_sizes = [], {}
for name, (source, budget, pixels, sharp) in MODELS.items():
    if ONLY and name not in ONLY: continue
    path = LIBRARY / (source + ".glb")
    if not path.exists(): print("MISSING", name, path); failed.append(name); continue
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.gltf(filepath=str(path))
    meshes = [o for o in bpy.context.scene.objects if o.type == 'MESH']
    bpy.ops.object.select_all(action='DESELECT')
    for o in meshes: o.select_set(True)
    bpy.context.view_layer.objects.active = meshes[0]
    if len(meshes) > 1: bpy.ops.object.join()
    obj = bpy.context.view_layer.objects.active
    bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
    before = sum(len(p.vertices) - 2 for p in obj.data.polygons)
    bpy.ops.object.mode_set(mode='EDIT'); bpy.ops.mesh.select_all(action='SELECT')
    bpy.ops.mesh.remove_doubles(threshold=1e-4); bpy.ops.mesh.dissolve_degenerate(threshold=1e-6)
    bpy.ops.mesh.delete_loose(use_verts=True, use_edges=True, use_faces=False)
    bpy.ops.object.mode_set(mode='OBJECT')
    tris = sum(len(p.vertices) - 2 for p in obj.data.polygons)
    if tris > budget:
        mod = obj.modifiers.new("Budget", 'DECIMATE'); mod.ratio = budget / tris; mod.use_collapse_triangulate = True
        bpy.ops.object.modifier_apply(modifier=mod.name)
    bpy.ops.object.shade_smooth()
    try:
        bpy.ops.object.shade_auto_smooth(angle=math.radians(sharp))
        for m in list(obj.modifiers): bpy.ops.object.modifier_apply(modifier=m.name)
    except Exception as error:
        print("  (no auto smooth: %s)" % error)
    cos = [v.co for v in obj.data.vertices]
    lo = Vector((min(c[i] for c in cos) for i in range(3))); hi = Vector((max(c[i] for c in cos) for i in range(3)))
    shift = Vector((-(lo.x + hi.x) / 2, -(lo.y + hi.y) / 2, -lo.z))
    for v in obj.data.vertices: v.co += shift
    size = hi - lo
    after = sum(len(p.vertices) - 2 for p in obj.data.polygons)
    image = base_colour_image(obj)
    if image is None: print("NO COLOUR MAP", name); failed.append(name); continue
    image.scale(pixels, pixels)
    png = OUT / (name + "BaseColor.png")
    image.filepath_raw = str(png); image.file_format = 'PNG'; image.save()
    picture(obj, str(OUT / (name + ".png")))
    obj.name = "Doorways_" + name; obj.data.name = "Doorways_" + name
    glb = OUT / (name + ".glb")
    bpy.ops.object.select_all(action='DESELECT'); obj.select_set(True)
    bpy.ops.export_scene.gltf(filepath=str(glb), export_format='GLB', use_selection=True, export_apply=True, export_yup=True,
                              export_lights=False, export_cameras=False)
    out = work / name
    result = subprocess.run([BUILDER, "static-pipeline", str(glb), str(out)], capture_output=True, text=True)
    made = list(out.rglob("*.dmesh"))
    if not made: print("BUILDER FAILED", name, result.stdout[-400:], result.stderr[-200:]); failed.append(name); continue
    shutil.copy(made[0], GAME / (name + ".dmesh")); shutil.copy(png, GAME / (name + "BaseColor.png"))
    # Blender x, y, z -> wide, deep, tall
    print("PLACE %-13s tris %8d -> %6d  wide %.3f deep %.3f tall %.3f  dmesh %d KB" % (name, before, after, size.x, size.y, size.z, made[0].stat().st_size // 1024))
    made_sizes[name] = (size.x, size.y, size.z)
shutil.rmtree(work, ignore_errors=True)
# the proportions of every model, for build_places.py (kept in git: that script needs no Blender)
SIZES = REPO / "art" / "places" / "sizes.txt"
known = {}
if SIZES.exists():
    for line in SIZES.read_text(encoding="utf-8").splitlines():
        if line.strip() and not line.startswith("#"):
            n, w, d, t = line.split(); known[n] = (float(w), float(d), float(t))
known.update(made_sizes)
SIZES.write_text("# name wide deep tall (model units; the game scales a model by how tall it is to stand). Written by prepare_places.py.\n" +
                 "".join("%s %.3f %.3f %.3f\n" % (n, *known[n]) for n in MODELS if n in known), encoding="utf-8")
print("DONE failed:", failed)
