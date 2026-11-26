#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""SH3 Quest VR: a weapon mesh's parts (LIST 46), from a dump SH3_MESH_DUMP wrote (d3d8.c: mesh_N.txt, stride 48:
+0 position, +12 weight, +24 the bone index bytes, +28 normal, +40 texture coordinates). For each bone the mesh is skinned
to: its vertices, their box and centre (model units, 490 a metre), so a moving part (a slide, a bolt, a pump, a
magazine) shows as a bone of its own, and the whole gun's long axis (the barrel's line).

    python tools/mesh_parts.py mesh_368.txt [mesh_290.txt ...]
"""
import struct
import sys


def words_to_floats(ws):
    return [struct.unpack('<f', struct.pack('<I', w))[0] for w in ws]


def parts(path):
    verts = []
    header = ''
    with open(path) as f:
        for line in f:
            if line.startswith('#'):
                header = line.strip()
            elif line.startswith('v '):
                ws = [int(x, 16) for x in line.split()[1:]]
                pos = words_to_floats(ws[0:3])
                weight = words_to_floats(ws[3:4])[0]
                bones = ws[6].to_bytes(4, 'little')
                verts.append((pos, weight, bones))
    print(f'== {path}: {header}')
    lo = [min(v[0][i] for v in verts) for i in range(3)]
    hi = [max(v[0][i] for v in verts) for i in range(3)]
    ext = [hi[i] - lo[i] for i in range(3)]
    axis = 'xyz'[ext.index(max(ext))]
    print(f'   whole: {len(verts)} vertices, box {fmt(lo)} .. {fmt(hi)}, size {fmt(ext)} '
          f'({ext[0] / 490 * 100:.1f} x {ext[1] / 490 * 100:.1f} x {ext[2] / 490 * 100:.1f} cm), long axis {axis}')
    weights = sorted({round(v[1], 3) for v in verts})
    print(f'   weights: {weights[:8]}{" ..." if len(weights) > 8 else ""}')
    by_bone = {}
    for pos, w, b in verts:
        by_bone.setdefault(b[0], []).append(pos)
    for bone in sorted(by_bone):
        ps = by_bone[bone]
        blo = [min(p[i] for p in ps) for i in range(3)]
        bhi = [max(p[i] for p in ps) for i in range(3)]
        c = [sum(p[i] for p in ps) / len(ps) for i in range(3)]
        print(f'   bone {bone}: {len(ps):4d} vertices, box {fmt(blo)} .. {fmt(bhi)}, centre {fmt(c)}, '
              f'size {fmt([bhi[i] - blo[i] for i in range(3)])}')
    others = {b[1:] for _, _, b in verts}
    print(f'   the other index bytes seen: {sorted(others)[:6]}')


def fmt(v):
    return '(' + ' '.join(f'{x:7.2f}' for x in v) + ')'


for p in sys.argv[1:]:
    parts(p)
