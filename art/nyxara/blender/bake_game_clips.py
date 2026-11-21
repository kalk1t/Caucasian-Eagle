# Nyxara's clips as the game plays them, into her Blender file (owner, 2026-11-21: "update nyxara's assets/blender project
# with latest animations which should only include what we use ingame now").
#
# The game makes many of her moves in code when it loads (her combo moves, her finishing kills, the twisted dash) and
# reshapes some of her clips (her cuts' wind-ups, her wrists). Its kit writes them all out, every joint 30 times a second:
#     set CAUCASIAN_EAGLE_NYXARA_BAKE=<folder>   and start the game or the Character Lab once
#     (tools\character-lab.ps1 -Preview Idle -Seconds 2 -Wait)
# This script opens Nyxara_mixamo.blend (her mesh and rig: the game's skeleton is this rig's, bone for bone), takes away
# every Action, and makes one Action for each clip of GAME_CLIPS from that folder, keyed on every frame. It saves the result
# under another name: Nyxara_mixamo.blend stays as it is, as the game's bundle is built from its Mixamo clips.
#
#     blender -b art/nyxara/blender/Nyxara_mixamo.blend --python art/nyxara/blender/bake_game_clips.py -- <bake folder> <out .blend>
#
# The game's space is Y up (glTF); Blender's Z up: a point (x, y, z) of the game's is (x, -z, y) here. Each bone's
# armature-space matrix in a frame is C * G * K, G the joint's place in the game (its parents' local poses times its own),
# C that turn of the axes and K the bone's own fixed difference from its joint (taken from the rest pose); its pose (the
# matrix Blender keys) is its rest relative to its parent, undone, times the same relative to its parent in the frame.

import bpy, csv, os, sys
from mathutils import Matrix, Quaternion, Vector

# what the game plays (0.5.2), with what it is
GAME_CLIPS = [
    ("Stand", "at ease, swords on her back (loops)"),
    ("Idle", "on guard, both swords up (loops)"),
    ("Walk", "her walk, in place (loops)"),
    ("Run", "her run, in place (loops)"),
    ("Jump", "the jump, in place: the game moves her up and down"),
    ("Fall", "falling (loops)"),
    ("Sword_Reach", "hands on the sword handles over her shoulders: the draw and the sheathe"),
    ("Fly_Rise", "the take-off"),
    ("Fly", "flight (loops)"),
    ("Fly_Land", "the landing out of flight"),
    ("Attack_S2_R", "LMB 1: FLASH STEP, a blink in, turning, and the cut as she arrives"),
    ("Attack_S4_L", "LMB 2 and RMB 3: PINWHEEL, one turn with both blades out"),
    ("Attack_S7_R", "LMB 3: MOUNTAIN BREAKER, a cut into both blades down into the ground"),
    ("Attack_S2_L", "RMB 1: DRAW CUT, from high over her left shoulder, down and level across at the waist"),
    ("Attack_S9_R", "RMB 2: SWALLOW DART, in and out"),
    ("Skill_Slide", "F on foot: the slide, low along the ground"),
    ("Dash_Twist", "F in flight: the twisted dash"),
    ("Finish_Wheel", "finishing kill: REAPER'S WHEEL"),
    ("Finish_Breaker", "finishing kill: MOUNTAIN BREAKER"),
    ("Finish_Flurry", "finishing kill: TEMPEST FLURRY"),
    ("Finish_Headsman", "finishing kill: HEADSMAN"),
    ("Death", "her death"),
]

C = Matrix(((1, 0, 0, 0), (0, 0, -1, 0), (0, 1, 0, 0), (0, 0, 0, 1)))


def local(t, q, s):
    return Matrix.LocRotScale(Vector(t), Quaternion((q[3], q[0], q[1], q[2])), Vector(s))


def chain(locals_, parents):
    out = []
    for i, m in enumerate(locals_):
        out.append(out[parents[i]] @ m if parents[i] >= 0 else m)
    return out


def main():
    args = sys.argv[sys.argv.index("--") + 1:]
    bake, out_path = args[0], args[1]
    rig = bpy.data.objects["Nyxara_rig"]
    scene = bpy.context.scene
    scene.render.fps, scene.render.fps_base = 30, 1

    rows = list(csv.DictReader(open(os.path.join(bake, "skeleton.tsv")), delimiter="\t"))
    names = [r["name"] for r in rows]
    parents = [int(r["parent"]) for r in rows]
    g_rest = chain([local([float(r[k]) for k in ("tx", "ty", "tz")], [float(r[k]) for k in ("rx", "ry", "rz", "rw")],
                          [float(r[k]) for k in ("sx", "sy", "sz")]) for r in rows], parents)
    bones = [rig.data.bones[n] for n in names]   # (every joint is a bone of hers: a missing one stops it here)
    b_rest = [b.matrix_local.copy() for b in bones]
    k = [(C @ g).inverted() @ br for g, br in zip(g_rest, b_rest)]
    rel_rest_inv = [((b_rest[parents[i]].inverted() @ b_rest[i]) if parents[i] >= 0 else b_rest[i]).inverted() for i in range(len(names))]

    seconds = {r["clip"]: (int(r["frames"]), r["loops"] == "1") for r in csv.DictReader(open(os.path.join(bake, "clips.tsv")), delimiter="\t")}
    missing = [c for c, _ in GAME_CLIPS if c not in seconds]
    if missing:
        raise SystemExit("not in the bake: " + ", ".join(missing))

    for a in list(bpy.data.actions):
        bpy.data.actions.remove(a)
    if rig.animation_data is None:
        rig.animation_data_create()
    for pb in rig.pose.bones:
        pb.rotation_mode = "QUATERNION"

    worst = 0.0
    for clip, what in GAME_CLIPS:
        frames, loops = seconds[clip]
        poses = []   # per frame: each bone's armature-space matrix
        with open(os.path.join(bake, clip + ".tsv")) as f:
            for line in f:
                v = line.rstrip("\n").split("\t")
                if len(v) < 1 + 10 * len(names):
                    continue
                n = [float(x) for x in v[1:]]
                ls = [local(n[i * 10:i * 10 + 3], n[i * 10 + 3:i * 10 + 7], n[i * 10 + 7:i * 10 + 10]) for i in range(len(names))]
                poses.append([C @ g @ kk for g, kk in zip(chain(ls, parents), k)])
        action = bpy.data.actions.new(clip)
        action.use_fake_user = True
        action["what"] = what
        action.use_frame_range = True
        action.frame_start, action.frame_end = 0, len(poses) - 1
        action.use_cyclic = loops
        rig.animation_data.action = action
        for i, name in enumerate(names):
            basis = []
            last = None
            for pose in poses:
                rel = (pose[parents[i]].inverted() @ pose[i]) if parents[i] >= 0 else pose[i]
                loc, rot, sca = (rel_rest_inv[i] @ rel).decompose()
                if last is not None and last.dot(rot) < 0:
                    rot = -rot   # (the nearer of the two ways round: no flips between keys)
                last = rot
                basis.append((loc, rot, sca))
            base = 'pose.bones["%s"].' % name
            for prop, count, pick in (("location", 3, lambda b, j: b[0][j]), ("rotation_quaternion", 4, lambda b, j: b[1][j]), ("scale", 3, lambda b, j: b[2][j])):
                for j in range(count):
                    fc = action.fcurve_ensure_for_datablock(rig, base + prop, index=j, group_name=name)
                    fc.keyframe_points.add(len(basis))
                    co = []
                    for fr, b in enumerate(basis):
                        co += [fr, pick(b, j)]
                    fc.keyframe_points.foreach_set("co", co)
                    for kp in fc.keyframe_points:
                        kp.interpolation = "LINEAR"
                    fc.update()
        # check: Blender's own pose at a few frames is the game's (each bone's head)
        for fr in sorted({0, len(poses) // 2, len(poses) - 1}):
            scene.frame_set(fr)
            for i, name in enumerate(names):
                worst = max(worst, (rig.pose.bones[name].matrix.translation - poses[fr][i].translation).length)
        print("BAKED %-16s %4d frames%s  %s" % (clip, len(poses) - 1, " (loops)" if loops else "", what))

    rig.animation_data.action = bpy.data.actions["Idle"]
    scene.frame_start, scene.frame_end = 0, int(bpy.data.actions["Idle"].frame_range[1])
    scene.frame_set(0)
    print("CHECK worst bone head off the game's: %.6f m" % worst)
    if worst > 1e-3:
        raise SystemExit("the baked clips are not the game's")
    bpy.ops.wm.save_as_mainfile(filepath=out_path, compress=True)
    print("SAVED", out_path, len(bpy.data.actions), "actions")


main()
