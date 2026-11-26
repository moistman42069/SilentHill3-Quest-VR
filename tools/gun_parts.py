#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""SH3 Quest VR: the guns' moving parts for the cocking visual (LIST 46), from the mesh dumps (SH3_MESH_DUMP, d3d8.c).

The game's gun models have no slide, pump or cocking knob of their own: each is two bone parts (the body; a small
piece at the hand). Their faces come in small connected pieces (flat shading), so a part is the pieces lying wholly
inside a box (model units of one bone): moving whole pieces stretches no face. Prints each part's vertex list (indices
from the draw's first vertex) for src/vr/sh3vr_combat.cpp, and draws it (red) over the gun (grey) for a look.

    python tools/gun_parts.py <folder with mesh_N.txt> [out folder for pictures]
"""
import os
import struct
import sys

# gun, mesh, bone, box (x0 x1 y0 y1 z0 z1), whole pieces (True) or every vertex in the box (False), what it is. The
# handgun's slide shares its sides with the grip's piece, so it is cut by height (the frame's top line, y 22: the fewest
# faces across the cut that do not belong to the grip; those few stretch a little at full travel).
PARTS = [
    ('handgun', 368, 0, (-1e9, 1e9, 22.0, 1e9, -1e9, 1e9), False, 'the slide (over the frame, the hammer with it)'),
    ('submachine gun', 557, 0, (-1.0, 13.0, 55.0, 70.0, -12.5, 3.5), True, 'the cocking knob on top at the rear'),
    ('shotgun', 290, 1, (-129.5, -12.9, -0.2, 27.3, -8.8, 8.8), True, 'the pump'),
]


def load(path):
    verts, idx = [], []
    with open(path) as f:
        for line in f:
            if line.startswith('v '):
                ws = [int(x, 16) for x in line.split()[1:]]
                pos = [struct.unpack('<f', struct.pack('<I', w))[0] for w in ws[0:3]]
                verts.append((pos, ws[6] & 0xFF))
            elif line.startswith('i'):
                idx = [int(x) for x in line.split()[1:]]
    tris = []
    for k in range(len(idx) - 2):
        a, b, c = idx[k], idx[k + 1], idx[k + 2]
        if a == b or b == c or a == c:
            continue
        pa, pb, pc = verts[a][0], verts[b][0], verts[c][0]
        u = [pb[i] - pa[i] for i in range(3)]
        v = [pc[i] - pa[i] for i in range(3)]
        cr = [u[1] * v[2] - u[2] * v[1], u[2] * v[0] - u[0] * v[2], u[0] * v[1] - u[1] * v[0]]
        if (cr[0] ** 2 + cr[1] ** 2 + cr[2] ** 2) ** 0.5 < 1e-3:
            continue  # the strip's stitches
        tris.append((a, b, c))
    return verts, tris


def pieces(n, tris):
    parent = list(range(n))

    def find(x):
        while parent[x] != x:
            parent[x] = parent[parent[x]]
            x = parent[x]
        return x

    for a, b, c in tris:
        for u, v in ((a, b), (b, c)):
            ru, rv = find(u), find(v)
            if ru != rv:
                parent[ru] = rv
    groups = {}
    for i in range(n):
        groups.setdefault(find(i), []).append(i)
    return list(groups.values())


def draw(verts, tris, chosen, bone, path):
    from PIL import Image, ImageDraw
    bv = [i for i in range(len(verts)) if verts[i][1] == bone]
    lo = [min(verts[i][0][k] for i in bv) for k in range(3)]
    hi = [max(verts[i][0][k] for i in bv) for k in range(3)]
    W = 1000
    s = (W - 40) / (hi[0] - lo[0])
    top2 = (hi[1] - lo[1]) * s + 60
    img = Image.new('RGB', (W, int(top2 + (hi[2] - lo[2]) * s + 40)), (16, 16, 16))
    d = ImageDraw.Draw(img)
    for view in (0, 1):
        for t in tris:
            if any(verts[i][1] != bone for i in t):
                continue
            pts = [(20 + (verts[i][0][0] - lo[0]) * s,
                    (20 + (hi[1] - verts[i][0][1]) * s) if view == 0 else (top2 + (verts[i][0][2] - lo[2]) * s))
                   for i in t]
            d.polygon(pts, outline=(230, 50, 50) if all(i in chosen for i in t) else (100, 100, 100))
    img.save(path)


src = sys.argv[1]
out = sys.argv[2] if len(sys.argv) > 2 else None
def in_box(v, bone, box):
    return v[1] == bone and all(box[2 * k] <= v[0][k] <= box[2 * k + 1] for k in range(3))


for gun, nv, bone, box, whole, what in PARTS:
    path = os.path.join(src, f'mesh_{nv}.txt')
    if not os.path.exists(path):
        print(f'// {gun}: {path} missing')
        continue
    verts, tris = load(path)
    chosen = set()
    if whole:
        for g in pieces(len(verts), tris):
            if all(in_box(verts[i], bone, box) for i in g):
                chosen.update(g)
    else:
        chosen = {i for i in range(len(verts)) if in_box(verts[i], bone, box)}
    # a single vertex with no face is no part of anything
    used = {i for t in tris for i in t}
    chosen &= used
    lst = sorted(chosen)
    print(f'// {gun} ({nv} vertices, bone {bone}): {what}, {len(lst)} vertices')
    print('{' + ', '.join(str(i) for i in lst) + '}')
    if out:
        draw(verts, tris, chosen, bone, os.path.join(out, f'part_{nv}.png'))
