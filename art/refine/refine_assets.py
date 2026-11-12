"""Refines every Meshy model of the game in Blender, one at a time, each with its own settings.

    blender -b -P art/refine/refine_assets.py -- <repo> [inspect] [Name ...]

For each model in ASSETS it reads the raw Meshy GLB, and
  1. welds vertices that lie on top of each other (Meshy leaves seams open),
  2. deletes loose fragments: islands of the mesh smaller than the model's 'fragment' share of its faces
     that float free of the body (specks, stray leaves, inside-out bubbles),
  3. dissolves degenerate faces and makes all normals point outward,
  4. reduces the model to its triangle budget, keeping its outline (only where the model has more),
  5. gives it smooth normals with hard edges kept above the model's 'sharp' angle,
  6. for scenery that stands on the ground, puts its foot on y = 0 and its middle on the origin,
  7. writes art/refine/out/<Name>.glb (same texture coordinates, so the Meshy colour map still fits) and a
     picture art/refine/out/<Name>.png (before on the left, after on the right) for looking at.
With 'inspect' it changes nothing and only prints what each model is like.

art/monsters/prepare_models.py and art/mountain/prepare_models.py take art/refine/out/<Name>.glb instead of the raw
model when it exists. Run rig_models.py after recompiling creatures, as always.

The origin of a creature is not moved: the game and the rig tool measure creatures from their own bounds.
"""
import bpy, bmesh, sys, os, math
from mathutils import Vector

args = sys.argv[sys.argv.index("--") + 1:]
REPO = args[0]
INSPECT = "inspect" in args
ONLY = [a for a in args[1:] if a != "inspect"]
OUT = os.path.join(REPO, "art", "refine", "out")

M, P, L = "art/monsters/meshy/out", "art/props/meshy/out", "art/mountain/meshy/out"
# name: folder, triangle budget, sharp angle (degrees), fragment share, ground (stand it on y = 0), note
ASSETS = {
    # creatures: skin and cloth, soft normals; small ones need fewer triangles than giants seen close
    "Chinka":     (M + "/chinka_3d",     22000, 50, .004, False, "small imp, seen in packs"),
    "Ali":        (M + "/ali_3d",        30000, 55, .004, False, "robed spirit: long hair and cloth, keep strands"),
    "Kudiani":    (M + "/kudiani_3d",    30000, 50, .004, False, "witch with a staff: thin staff must survive"),
    "Ochokochi":  (M + "/ochokochi_3d",  32000, 45, .004, False, "thorns on the chest: sharper edges"),
    "Almasty":    (M + "/almasty_3d",    30000, 60, .004, False, "fur: soft"),
    "Kaji":       (M + "/kaji_3d",       30000, 45, .004, False, "iron smith: hard armour edges"),
    "Uaig":       (M + "/uaig_3d",       36000, 50, .003, False, "one-eyed giant, fills the screen"),
    "Devi":       (M + "/devi_3d",       36000, 50, .003, False, "many-headed giant, fills the screen"),
    "Rokapi":     (M + "/rokapi_3d",     34000, 50, .003, False, "boss"),
    "Gveleshapi": (M + "/gveleshapi_3d", 34000, 40, .003, False, "dragon boss: scales, keep them crisp"),
    # things
    "Chest":      (M + "/chest_3d",      12000, 35, .006, True,  "hard edges: wood and iron"),
    "DeadTree":   (P + "/deadtree_3d",   16000, 40, .002, True,  "thin twigs: tiny islands are real, keep most"),
    "Fir":        (P + "/fir_3d",        14000, 60, .002, True,  "needles as cards: keep islands"),
    "Rocks":      (P + "/rocks_3d",      9000,  30, .010, True,  "faceted stone: keep it faceted"),
    "Tower":      (P + "/tower_3d",      18000, 30, .006, True,  "masonry: hard edges"),
}


def stats(obj):
    bm = bmesh.new(); bm.from_mesh(obj.data)
    bm.verts.ensure_lookup_table(); bm.faces.ensure_lookup_table()
    seen = set(); islands = []
    for f in bm.faces:
        if f.index in seen:
            continue
        stack = [f]; seen.add(f.index); n = 0
        while stack:
            g = stack.pop(); n += 1
            for e in g.edges:
                for h in e.link_faces:
                    if h.index not in seen:
                        seen.add(h.index); stack.append(h)
        islands.append(n)
    open_edges = sum(1 for e in bm.edges if len(e.link_faces) == 1)
    tris = sum(len(f.verts) - 2 for f in bm.faces)
    lo = Vector((min(v.co[i] for v in bm.verts) for i in range(3))); hi = Vector((max(v.co[i] for v in bm.verts) for i in range(3)))
    bm.free()
    islands.sort(reverse=True)
    return dict(verts=len(obj.data.vertices), tris=tris, islands=islands, open=open_edges, lo=lo, hi=hi)


def describe(tag, s):
    small = [n for n in s["islands"][1:] if n < 0.01 * s["tris"]]
    print("  %-7s verts %6d  tris %6d  islands %4d (largest %d, tiny %d)  open edges %5d  size %.2f x %.2f x %.2f  foot %.3f" % (
        tag, s["verts"], s["tris"], len(s["islands"]), s["islands"][0], len(small), s["open"],
        s["hi"].x - s["lo"].x, s["hi"].y - s["lo"].y, s["hi"].z - s["lo"].z, s["lo"].z))


def load(path):
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.gltf(filepath=path)
    meshes = [o for o in bpy.context.scene.objects if o.type == "MESH"]
    bpy.ops.object.select_all(action="DESELECT")
    for o in meshes:
        o.select_set(True)
    bpy.context.view_layer.objects.active = meshes[0]
    if len(meshes) > 1:
        bpy.ops.object.join()
    obj = bpy.context.view_layer.objects.active
    bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
    return obj


def remove_fragments(obj, share):
    """Deletes islands smaller than 'share' of the faces whose bounds do not touch the body (the largest island)."""
    bm = bmesh.new(); bm.from_mesh(obj.data); bm.faces.ensure_lookup_table()
    seen = set(); islands = []
    for f in bm.faces:
        if f.index in seen:
            continue
        stack = [f]; seen.add(f.index); faces = []
        while stack:
            g = stack.pop(); faces.append(g)
            for e in g.edges:
                for h in e.link_faces:
                    if h.index not in seen:
                        seen.add(h.index); stack.append(h)
        islands.append(faces)
    islands.sort(key=len, reverse=True)
    total = len(bm.faces)
    def box(faces):
        cos = [v.co for f in faces for v in f.verts]
        return Vector((min(c[i] for c in cos) for i in range(3))), Vector((max(c[i] for c in cos) for i in range(3)))
    body_lo, body_hi = box(islands[0])
    reach = 0.02 * max(body_hi - body_lo)
    doomed = []; removed = 0
    for faces in islands[1:]:
        if len(faces) >= share * total:
            continue
        lo, hi = box(faces)
        apart = any(lo[i] > body_hi[i] + reach or hi[i] < body_lo[i] - reach for i in range(3))
        speck = len(faces) < 12
        if apart or speck:
            doomed.extend(faces); removed += 1
    if doomed:
        bmesh.ops.delete(bm, geom=doomed, context="FACES")
    bm.to_mesh(obj.data); bm.free()
    return removed


def refine(obj, budget, sharp, share, ground):
    bpy.context.view_layer.objects.active = obj
    bpy.ops.object.mode_set(mode="EDIT")
    bpy.ops.mesh.select_all(action="SELECT")
    bpy.ops.mesh.remove_doubles(threshold=1e-5)
    bpy.ops.mesh.dissolve_degenerate(threshold=1e-6)
    bpy.ops.mesh.delete_loose(use_verts=True, use_edges=True, use_faces=False)
    bpy.ops.object.mode_set(mode="OBJECT")
    removed = remove_fragments(obj, share)
    bpy.ops.object.mode_set(mode="EDIT")
    bpy.ops.mesh.select_all(action="SELECT")
    bpy.ops.mesh.normals_make_consistent(inside=False)
    bpy.ops.object.mode_set(mode="OBJECT")
    tris = sum(len(p.vertices) - 2 for p in obj.data.polygons)
    if tris > budget:
        mod = obj.modifiers.new("Budget", "DECIMATE")
        mod.ratio = budget / tris; mod.use_collapse_triangulate = True
        bpy.ops.object.modifier_apply(modifier=mod.name)
    bpy.ops.object.shade_smooth()
    try:
        bpy.ops.object.shade_auto_smooth(angle=math.radians(sharp))
        for m in list(obj.modifiers):
            bpy.ops.object.modifier_apply(modifier=m.name)
    except Exception as error:                      # older Blender: mesh auto smooth
        print("  (auto smooth by modifier unavailable: %s)" % error)
    if ground:
        cos = [v.co for v in obj.data.vertices]
        lo = Vector((min(c[i] for c in cos) for i in range(3))); hi = Vector((max(c[i] for c in cos) for i in range(3)))
        shift = Vector((-(lo.x + hi.x) / 2, -(lo.y + hi.y) / 2, -lo.z))
        for v in obj.data.vertices:
            v.co += shift
    return removed


def picture(obj, path):
    """A three-quarter view under plain light, so the form can be judged."""
    cos = [obj.matrix_world @ v.co for v in obj.data.vertices]
    lo = Vector((min(c[i] for c in cos) for i in range(3))); hi = Vector((max(c[i] for c in cos) for i in range(3)))
    mid, size = (lo + hi) / 2, max(hi - lo)
    scene = bpy.context.scene
    for o in [o for o in scene.objects if o.type in ("CAMERA", "LIGHT")]:
        bpy.data.objects.remove(o)
    cam = bpy.data.objects.new("Cam", bpy.data.cameras.new("Cam")); scene.collection.objects.link(cam)
    cam.location = mid + Vector((0.9, -1.55, 0.45)) * size * 1.25
    cam.rotation_euler = (mid - cam.location).to_track_quat("-Z", "Y").to_euler(); cam.data.lens = 50
    for name, energy, turn in (("Key", 3.5, (50, 0, 35)), ("Fill", 1.2, (60, 0, -120))):
        sun = bpy.data.objects.new(name, bpy.data.lights.new(name, "SUN")); scene.collection.objects.link(sun)
        sun.data.energy = energy; sun.rotation_euler = tuple(math.radians(a) for a in turn)
    if scene.world is None:
        scene.world = bpy.data.worlds.new("W")
    scene.world.use_nodes = True
    scene.world.node_tree.nodes["Background"].inputs[0].default_value = (.16, .19, .22, 1)
    scene.camera = cam; scene.render.engine = "BLENDER_EEVEE"
    scene.render.resolution_x, scene.render.resolution_y = 640, 720; scene.render.filepath = path
    bpy.ops.render.render(write_still=True)


os.makedirs(OUT, exist_ok=True)
for name, (folder, budget, sharp, share, ground, note) in ASSETS.items():
    if ONLY and name not in ONLY:
        continue
    source = os.path.join(REPO, folder, "model_url.glb")
    if not os.path.exists(source):
        print("MISSING", name, source); continue
    obj = load(source)
    print("ASSET %s: %s" % (name, note))
    before = stats(obj); describe("before", before)
    if INSPECT:
        continue
    picture(obj, os.path.join(OUT, name + "_before.png"))
    removed = refine(obj, budget, sharp, share, ground)
    after = stats(obj); describe("after", after)
    print("  fragments removed %d" % removed)
    picture(obj, os.path.join(OUT, name + "_after.png"))
    for o in [o for o in bpy.context.scene.objects if o.type in ("CAMERA", "LIGHT")]:
        bpy.data.objects.remove(o)
    bpy.ops.object.select_all(action="DESELECT"); obj.select_set(True)
    bpy.ops.export_scene.gltf(filepath=os.path.join(OUT, name + ".glb"), export_format="GLB", use_selection=True, export_apply=True,
                              export_yup=True, export_lights=False, export_cameras=False)
    print("  wrote", name + ".glb")
