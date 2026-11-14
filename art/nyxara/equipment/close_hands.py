"""Closes Nyxara's hands into fists, in the mesh itself: her rig has no finger joints, and she always holds something.

    python close_hands.py <in.glb> <out.glb> [curl=1.0] [report.json]

Only vertex positions and normals of the skinned mesh change, in place in the GLB's binary buffer: the skeleton, the
weights and every animation stay byte for byte what they were, so the compiled rig signature and clips do not change.

For each hand (the vertices that follow Hand_L or Hand_R): the fingers' direction is found from the mesh (from the
joint to the farthest vertices), the palm's normal is the thin axis of the hand, turned to the side the hand closes to.
The hand is then bent as a strip along the fingers' direction: nothing up to the knuckles, then three bends (knuckle,
middle joint, last joint), each spread over a short length so the surface does not tear. A vertex keeps its distance
from the strip's middle line, across and through the hand.
"""
import json, struct, sys
import numpy as np

SRC, OUT = sys.argv[1], sys.argv[2]
CURL = float(sys.argv[3]) if len(sys.argv) > 3 else 1.0
REPORT = sys.argv[4] if len(sys.argv) > 4 else None
HANDS = ("Hand_L", "Hand_R")
MIN_WEIGHT = 0.35                      # a vertex belongs to a hand when the hand joint carries at least this much of it
# where along the hand (0 = the wrist joint, 1 = the fingertips) each bend is, how far it turns (degrees), and the
# length (as a part of the hand) it is spread over
BENDS = ((0.50, 80.0, 0.10), (0.70, 95.0, 0.09), (0.86, 60.0, 0.08))

data = open(SRC, "rb").read()
assert data[:4] == b"glTF"
jlen = struct.unpack_from("<I", data, 12)[0]
doc = json.loads(data[20:20 + jlen].decode("utf-8"))
bstart = 20 + jlen + 8
blen = struct.unpack_from("<I", data, 20 + jlen)[0]
binary = bytearray(data[bstart:bstart + blen])
COMP = {5120: ("b", 1), 5121: ("B", 1), 5122: ("h", 2), 5123: ("H", 2), 5125: ("I", 4), 5126: ("f", 4)}
WIDTH = {"SCALAR": 1, "VEC2": 2, "VEC3": 3, "VEC4": 4, "MAT4": 16}


def view(index):
    """(array copy, write-back function) of an accessor."""
    a = doc["accessors"][index]; bv = doc["bufferViews"][a["bufferView"]]
    code, size = COMP[a["componentType"]]; n = WIDTH[a["type"]]
    start = bv.get("byteOffset", 0) + a.get("byteOffset", 0); stride = bv.get("byteStride", size * n)
    dt = np.dtype("<" + code)
    out = np.empty((a["count"], n), dtype=dt)
    for i in range(n):
        out[:, i] = np.frombuffer(binary, dtype=dt, count=a["count"], offset=start + i * size) if stride == size * n and n == 1 else \
            np.ndarray((a["count"],), dtype=dt, buffer=binary, offset=start + i * size, strides=(stride,))

    def write(values):
        for i in range(n):
            np.ndarray((a["count"],), dtype=dt, buffer=binary, offset=start + i * size, strides=(stride,))[:] = values[:, i]
    return out, write


skin = doc["skins"][0]
names = [doc["nodes"][j].get("name") for j in skin["joints"]]
ibm, _ = view(skin["inverseBindMatrices"])
bind = [np.linalg.inv(m.reshape(4, 4).T.astype(np.float64)) for m in ibm]      # column-vector matrices, mesh space


def smooth(x):
    x = np.clip(x, 0.0, 1.0); return x * x * (3 - 2 * x)


report = {"curl": CURL, "hands": {}}
done = set()
for mesh in doc["meshes"]:
    for prim in mesh["primitives"]:
        at = prim["attributes"]
        if "JOINTS_0" not in at or at["POSITION"] in done: continue
        done.add(at["POSITION"])
        P, writeP = view(at["POSITION"]); P = P.astype(np.float64)
        Nn, writeN = view(at["NORMAL"]); Nn = Nn.astype(np.float64)
        J, _ = view(at["JOINTS_0"]); W, _ = view(at["WEIGHTS_0"]); W = W.astype(np.float64)
        if doc["accessors"][at["WEIGHTS_0"]]["componentType"] != 5126: W = W / np.iinfo(W.dtype).max
        for hand in HANDS:
            j = names.index(hand)
            w = (W * (J == j)).sum(1)
            mine = np.nonzero(w >= MIN_WEIGHT)[0]
            if len(mine) < 50: continue
            o = bind[j][:3, 3]
            Q = P[mine] - o
            dist = np.linalg.norm(Q, axis=1)
            far = Q[dist >= np.quantile(dist, 0.95)]
            u = far.mean(0); u /= np.linalg.norm(u)                               # along the fingers
            length = float(np.quantile(Q @ u, 0.995))
            flat = Q - np.outer(Q @ u, u)
            _, _, vt = np.linalg.svd(flat - flat.mean(0), full_matrices=False)
            n = vt[1] - (vt[1] @ u) * u; n /= np.linalg.norm(n)                   # through the hand (its thin axis)
            # the hand closes toward the body's middle plane (arms down, the palms face the thighs) and a little forward
            inward = np.array([-np.sign(o[0]), 0.0, 0.0])
            if n @ inward < 0: n = -n
            a = np.cross(n, u)                                                    # across the palm (the knuckles' line)
            t = Q @ u; h = Q @ n; s = Q @ a
            mid = float(np.median(h[(t > 0.35 * length) & (t < 0.6 * length)]))   # the middle line runs through the palm's thickness
            # the angle the strip has turned by at each distance, and the middle line integrated along it
            grid = np.linspace(0.0, max(length * 1.05, t.max() + 1e-4), 400)
            phi = np.zeros_like(grid)
            for where, degrees, spread in BENDS:
                phi += np.radians(degrees * CURL) * smooth((grid - (where - spread / 2) * length) / (spread * length))
            step = np.diff(grid)
            cu = np.concatenate([[0.0], np.cumsum(np.cos((phi[:-1] + phi[1:]) / 2) * step)])
            cn = np.concatenate([[0.0], np.cumsum(np.sin((phi[:-1] + phi[1:]) / 2) * step)])
            tt = np.clip(t, 0.0, grid[-1])
            f = np.interp(tt, grid, phi); pu = np.interp(tt, grid, cu); pn = np.interp(tt, grid, cn)
            hh = h - mid
            newu = pu - hh * np.sin(f) + np.minimum(t, 0.0)
            newn = pn + hh * np.cos(f) + mid
            # (a vertex only partly the hand's moves only that part of the way: the wrist stays whole)
            k = smooth((w[mine] - MIN_WEIGHT) / (0.8 - MIN_WEIGHT))[:, None]
            moved = o + np.outer(newu, u) + np.outer(newn, n) + np.outer(s, a)
            P[mine] = P[mine] * (1 - k) + moved * k
            nu = Nn[mine] @ u; nn = Nn[mine] @ n; na = Nn[mine] @ a
            fk = f * k[:, 0]
            turned = np.outer(nu * np.cos(fk) - nn * np.sin(fk), u) + np.outer(nu * np.sin(fk) + nn * np.cos(fk), n) + np.outer(na, a)
            Nn[mine] = turned / np.maximum(np.linalg.norm(turned, axis=1, keepdims=True), 1e-9)
            report["hands"].setdefault(hand, []).append({"vertices": int(len(mine)), "length_m": round(length, 4), "joint": [round(float(x), 4) for x in o],
                "fingers": [round(float(x), 3) for x in u], "closes_toward": [round(float(x), 3) for x in n], "moved": int((f > 0.01).sum())})
        writeP(P.astype(np.float32)); writeN(Nn.astype(np.float32))
        acc = doc["accessors"][at["POSITION"]]
        acc["min"] = [float(x) for x in P.astype(np.float32).min(0)]; acc["max"] = [float(x) for x in P.astype(np.float32).max(0)]

text = json.dumps(doc, separators=(",", ":")).encode("utf-8")
text += b" " * (-len(text) % 4)
rest = data[bstart + blen:]
out = bytearray(b"glTF" + struct.pack("<II", 2, 0))
out += struct.pack("<I", len(text)) + b"JSON" + text
out += struct.pack("<I", len(binary)) + b"BIN\x00" + bytes(binary) + rest
struct.pack_into("<I", out, 8, len(out))
open(OUT, "wb").write(out)
if REPORT: json.dump(report, open(REPORT, "w"), indent=1)
print(json.dumps(report))
