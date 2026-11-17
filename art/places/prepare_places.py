"""Turns the Meshy scenery library into game models: the places and things that stand beside the road.

    blender -b -P art/places/prepare_places.py -- <asset builder exe> [Name ...]

Reads   art/props/meshy/out/library/<source>.glb      the downloaded Meshy models (not in git: see art/README.md)
Writes  art/places/out/<Name>.glb, <Name>.png         the game model and a picture of it (not in git)
        art/places/out/<Name>BaseColor.png, <Name>Normal.png   its baked colour and normal maps
        game/assets/Compiled/Places/<Name>.dmesh, <Name>BaseColor.png, <Name>Normal.png
        game/assets/Compiled/Places/<Name>Far.dmesh   the far version (make_far.py; models of 4,000 triangles or more)
        art/places/qa_report.txt                       the QA gate's verdict on every model (in git)

For each model in MODELS: joins it, welds the seams Meshy leaves open and stands it on y = 0 with its middle on the
origin. That is the source (the high model). The game model is a new closed surface wrapped round it (voxel remesh),
reduced to the triangle budget, its crumbs dropped, unwrapped afresh (reduce_copy). Where the remesh falls apart (open
sheets thinner than a voxel: Barrel) and for REDUCE_ONLY (leaves: Shrub), the source itself is reduced instead, its
own UVs kept. Either way the source's colour and detail are baked onto the game model in Cycles (bake_onto), the
model goes through the QA gate (model_qa), and the engine's asset builder (static-pipeline) compiles it.
Where the models stand is art/places/build_places.py; run that after this. With names given, only those are made.
The 4th column of MODELS (the hard-edge angle) is no longer used: the baked normal map carries the hard edges.
"""
import bpy, bmesh, sys, os, math, shutil, subprocess, tempfile, pathlib
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


sys.path.insert(0, str(REPO / "art" / "tools" / "blender_skills"))
import qa_check, bake_maps        # from the blender-agent-skills plugin (MIT, see art/tools/blender_skills/README.md)
sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
from make_far import make_far


def apply_modifiers(obj):
    with bpy.context.temp_override(object=obj, active_object=obj, selected_objects=[obj], selected_editable_objects=[obj]):
        for m in list(obj.modifiers): bpy.ops.object.modifier_apply(modifier=m.name)


VOXELS = 500          # the remesh's voxel: the model's diagonal / VOXELS (about 5 mm on a 2.5 m model)
CRUMB = 60            # loose parts smaller than the model's diagonal / CRUMB are dropped (voxel crumbs; by size, not
                      # by faces: a barrel's staves are separate parts of few faces each at a 1,400-triangle budget)
# Models made the old way (the source itself reduced, its own UVs kept), and baked like the rest: thin leaves by the
# thousand do not survive a remesh (Shrub came out as dark crumbs).
REDUCE_ONLY = {"Shrub"}


def reduce_copy(high, budget, name):
    """The game model: a new closed surface around the source (voxel remesh), reduced to the triangle budget, its
    crumbs dropped, unwrapped afresh. Reducing the source itself (2026-11-12 to 11-17) folded its thin layers
    (shingles, planks, needles) over each other: sheets hanging over the walls, z-fighting (35 of 56 models failed the
    QA gate), hidden faces, and Pine stuck at 111,000 triangles because its 1,000s of needles could not be reduced.
    The remesh keeps the true shape; bake_onto() brings back the source's colour and detail."""
    low = high.copy(); low.data = high.data.copy(); low.name = "Doorways_" + name
    bpy.context.scene.collection.objects.link(low)
    if name in REDUCE_ONLY or not remesh(low, budget, name):
        if name not in REDUCE_ONLY:          # the remesh fell apart: start again from the source
            bpy.data.objects.remove(low, do_unlink=True)
            low = high.copy(); low.data = high.data.copy(); low.name = "Doorways_" + name
            bpy.context.scene.collection.objects.link(low)
        METHOD[name] = "reduced"
        tris = sum(len(p.vertices) - 2 for p in low.data.polygons)
        if tris > budget:
            mod = low.modifiers.new("Budget", 'DECIMATE'); mod.ratio = budget / tris; mod.use_collapse_triangulate = True
            apply_modifiers(low)
        with bpy.context.temp_override(object=low, active_object=low, selected_objects=[low], selected_editable_objects=[low]):
            bpy.ops.object.shade_smooth()
        return low
    METHOD[name] = "remeshed"
    return low


METHOD = {}


def remesh(low, budget, name):
    """The remesh, in place. False when it falls apart: a source made of open sheets thinner than a voxel (a barrel's
    staves) comes out as fragments; more than half of it in crumbs (Barrel 100%, Stall 95%, Dock 79%; Pine loses 30% in needle tufts and is
    fine) and the model is made the other way."""
    for u in list(low.data.uv_layers): low.data.uv_layers.remove(u)
    diag = low.dimensions.length
    rm = low.modifiers.new("Remesh", 'REMESH'); rm.mode = 'VOXEL'; rm.voxel_size = diag / VOXELS; apply_modifiers(low)
    tris = 2 * len(low.data.polygons)
    if tris > budget:
        mod = low.modifiers.new("Budget", 'DECIMATE'); mod.ratio = budget / tris; mod.use_collapse_triangulate = True
        apply_modifiers(low)
    bm = bmesh.new(); bm.from_mesh(low.data)
    bmesh.ops.dissolve_degenerate(bm, dist=1e-5, edges=bm.edges[:])
    seen, crumbs = set(), []
    for f in bm.faces:
        if f.index in seen: continue
        part, stack = [], [f]; seen.add(f.index)
        while stack:
            g = stack.pop(); part.append(g)
            for e in g.edges:
                for h in e.link_faces:
                    if h.index not in seen: seen.add(h.index); stack.append(h)
        pts = [v.co for g in part for v in g.verts]
        span = (Vector([max(p[i] for p in pts) for i in range(3)]) - Vector([min(p[i] for p in pts) for i in range(3)])).length
        if span < diag / CRUMB: crumbs += part
    print("  %s: %d faces after the remesh, %d in crumbs" % (name, len(bm.faces), len(crumbs)))
    if len(crumbs) > 0.5 * len(bm.faces): bm.free(); return False
    bmesh.ops.delete(bm, geom=crumbs, context='FACES'); bm.to_mesh(low.data); bm.free()
    low.data.uv_layers.new(name="UVMap")
    for o in bpy.context.scene.objects: o.select_set(False)
    low.select_set(True); bpy.context.view_layer.objects.active = low     # only the game model: the source must not be unwrapped with it
    with bpy.context.temp_override(object=low, active_object=low, selected_objects=[low], selected_editable_objects=[low]):
        bpy.ops.object.mode_set(mode='EDIT'); bpy.ops.mesh.select_all(action='SELECT')
        bpy.ops.uv.smart_project(angle_limit=math.radians(60), island_margin=0.004, area_weight=0.0, scale_to_bounds=True)
        bpy.ops.object.mode_set(mode='OBJECT')
        bpy.ops.object.shade_smooth()     # smooth only: the baked normal map carries the hard edges (it is baked against these normals)
    return True


def bake_onto(low, high, name, pixels, png, normal_png):
    """Bakes the source's colour and normals onto the reduced model, and gives it one material with both maps."""
    mat = bpy.data.materials.new("Doorways_" + name)
    low.data.materials.clear(); low.data.materials.append(mat)
    for p in low.data.polygons: p.material_index = 0
    bake_dir = work / "bake"
    try:
        r = bake_maps.run(low=low.name, high=[high.name], maps=["basecolor", "normal"], res=pixels, samples=16,
                          margin_px=8, out=str(bake_dir), name=name, device="GPU")
    except Exception as error:
        print("BAKE FAILED", name, error); return None
    shutil.move(r["maps"]["basecolor"], png); shutil.move(r["maps"]["normal"], normal_png)
    nt = mat.node_tree; bsdf = next(n for n in nt.nodes if n.type == 'BSDF_PRINCIPLED')
    col = nt.nodes.new("ShaderNodeTexImage"); col.image = bpy.data.images.load(str(png))
    nrm = nt.nodes.new("ShaderNodeTexImage"); nrm.image = bpy.data.images.load(str(normal_png)); nrm.image.colorspace_settings.name = "Non-Color"
    nmap = nt.nodes.new("ShaderNodeNormalMap")
    nt.links.new(col.outputs["Color"], bsdf.inputs["Base Color"])
    nt.links.new(nrm.outputs["Color"], nmap.inputs["Color"]); nt.links.new(nmap.outputs["Normal"], bsdf.inputs["Normal"])
    nt.nodes.active = col                                   # the workbench picture shows the active one
    return "%s ext %.3f" % (r["device"], r["extrusion"])


# The asset QA gate (art/tools/blender_skills/qa_check.py, game profile): z-fighting, detached parts, flipped normals,
# open or degenerate geometry, per model, into art/places/qa_report.txt. A model that fails is still made (the report
# says what to look at); duplicate vertices at UV seams are normal for a game model and left out of the report.
QA_REPORT = REPO / "art" / "places" / "qa_report.txt"
QA_LINES = {}
if QA_REPORT.exists():
    for line in QA_REPORT.read_text(encoding="utf-8").splitlines(keepends=True):
        if line.strip() and not line.startswith("#"): QA_LINES[line.split("\t")[0]] = line


def model_qa(obj, name, before, after):
    r = qa_check.run(objects=[obj.name], profile="game")
    zf = sum(int(e.split("(")[1].split()[0]) for e in r["errors"] if e.startswith("z-fighting") and "(" in e)
    errors = [e for e in r["errors"] if not e.startswith("z-fighting")]
    warnings = [w for w in r["warnings"] if "duplicate verts" not in w]
    verdict = "pass" if r["pass"] else "FAIL"
    QA_LINES[name] = "%s\t%s\tsource %d tris\tgame %d tris\tz-fight pairs %d\t%s\t%s\n" % (
        name, verdict, before, after, zf, "; ".join(errors) or "-", "; ".join(warnings) or "-")
    return "%s zf %d%s" % (verdict, zf, (" +%d other" % len(errors)) if errors else "")


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
    if base_colour_image(obj) is None: print("NO COLOUR MAP", name); failed.append(name); continue
    # the source stays as it is (the high model); the game model is a reduced copy of it
    high = obj
    cos = [v.co for v in high.data.vertices]
    lo = Vector((min(c[i] for c in cos) for i in range(3))); hi = Vector((max(c[i] for c in cos) for i in range(3)))
    shift = Vector((-(lo.x + hi.x) / 2, -(lo.y + hi.y) / 2, -lo.z))
    for v in high.data.vertices: v.co += shift
    size = hi - lo
    obj = reduce_copy(high, budget, name)
    after =sum(len(p.vertices) - 2 for p in obj.data.polygons)
    # The source's detail (shingles, planks, carving, Meshy's own normal map) baked onto the reduced model: its colour
    # and its normal map, in the reduced model's own UVs (art/tools/blender_skills/bake_maps.py, Cycles on the GPU).
    # Before (2026-11-12 to 11-17) the reduced model simply kept the source's colour map, and the detail the
    # reduction took away was gone (a close look at RuinedHouse's roof: smeared, folded shards where shingles were).
    png, normal_png = OUT / (name + "BaseColor.png"), OUT / (name + "Normal.png")
    baked = bake_onto(obj, high, name, pixels, png, normal_png)
    if baked is None: failed.append(name); continue
    bpy.data.objects.remove(high, do_unlink=True)
    picture(obj, str(OUT / (name + ".png")))
    qa = model_qa(obj, name, before, after)
    obj.name = "Doorways_" + name; obj.data.name = "Doorways_" + name
    glb = OUT / (name + ".glb")
    bpy.ops.object.select_all(action='DESELECT'); obj.select_set(True)
    bpy.ops.export_scene.gltf(filepath=str(glb), export_format='GLB', use_selection=True, export_apply=True, export_yup=True,
                              export_lights=False, export_cameras=False, export_tangents=True)   # the bake's own tangents
    out = work / name
    result = subprocess.run([BUILDER, "static-pipeline", str(glb), str(out)], capture_output=True, text=True)
    made = list(out.rglob("*.dmesh"))
    if not made: print("BUILDER FAILED", name, result.stdout[-400:], result.stderr[-200:]); failed.append(name); continue
    shutil.copy(made[0], GAME / (name + ".dmesh")); shutil.copy(png, GAME / (name + "BaseColor.png"))
    shutil.copy(normal_png, GAME / (name + "Normal.png"))
    # Blender x, y, z -> wide, deep, tall
    print("PLACE %-13s tris %8d -> %6d  wide %.3f deep %.3f tall %.3f  dmesh %d KB  %s  bake %s  qa %s" % (name, before, after, size.x, size.y, size.z, made[0].stat().st_size // 1024, METHOD.get(name), baked, qa))
    made_sizes[name] = (size.x, size.y, size.z)
    print("  far version: %d tris" % make_far(name, BUILDER))       # art/places/make_far.py (empties the scene: last)
shutil.rmtree(work, ignore_errors=True)
QA_REPORT.write_text("# model\tverdict\tsource\tgame\tz-fighting\terrors\twarnings  (prepare_places.py, art/tools/blender_skills/qa_check.py, game profile)\n"
                     + "".join(QA_LINES[n] for n in MODELS if n in QA_LINES), encoding="utf-8")
print("QA report:", QA_REPORT)
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
