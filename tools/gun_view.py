"""Draw a weapon mesh dump (d3d8.c SH3_MESH_DUMP, mesh_N.txt) from the side, the top and the front, so its shape and
which way it points can be looked at, not only read from numbers. Each bone's faces in their own colour (bone 0 grey,
bone 1 orange, 2 blue, 3 green), flat-shaded, drawn far to near; the model's axes marked; optional points drawn as
red crosses with their names.

    python -I gun_view.py mesh_368.txt out.png [--bone-pose 1:m00,m01,...,m23] [--point name:x,y,z ...]

--bone-pose b:12 numbers puts bone b's vertices through that 3x4 (rows; a relative pose into bone 0's frame, as the
game posed it) before drawing; without it every bone is drawn in its own local frame.
"""
import argparse
import struct

from PIL import Image, ImageDraw

COLOURS = [(170, 170, 170), (235, 140, 40), (80, 140, 235), (90, 200, 110)]


def load(path):
    verts, idx, prim = [], [], 5
    with open(path) as f:
        for line in f:
            if line.startswith('#') and 'primitive type' in line:
                prim = int(line.split('primitive type')[1].split(',')[0])
            elif line.startswith('v '):
                ws = [int(x, 16) for x in line.split()[1:]]
                pos = [struct.unpack('<f', struct.pack('<I', w))[0] for w in ws[0:3]]
                verts.append((pos, ws[6] & 0xFF))
            elif line.startswith('i'):
                idx = [int(x) for x in line.split()[1:]]
    tris = []
    if prim == 4:
        for k in range(0, len(idx) - 2, 3):
            tris.append((idx[k], idx[k + 1], idx[k + 2]))
    else:
        for k in range(len(idx) - 2):
            a, b, c = idx[k], idx[k + 1], idx[k + 2]
            if a == b or b == c or a == c:
                continue
            tris.append((a, b, c) if k % 2 == 0 else (b, a, c))
    return verts, tris


def apply(m, p):
    return [m[r * 4] * p[0] + m[r * 4 + 1] * p[1] + m[r * 4 + 2] * p[2] + m[r * 4 + 3] for r in range(3)]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('mesh')
    ap.add_argument('out')
    ap.add_argument('--bone-pose', action='append', default=[])
    ap.add_argument('--point', action='append', default=[])
    ap.add_argument('--size', type=int, default=520)
    a = ap.parse_args()
    verts, tris = load(a.mesh)
    poses = {}
    for s in a.bone_pose:
        b, nums = s.split(':')
        poses[int(b)] = [float(x) for x in nums.split(',')]
    pts = [apply(poses[b], p) if b in poses else p for p, b in verts]
    marks = []
    for s in a.point:
        name, xyz = s.split(':')
        marks.append((name, [float(x) for x in xyz.split(',')]))
    allp = pts + [m[1] for m in marks]
    lo = [min(p[i] for p in allp) for i in range(3)]
    hi = [max(p[i] for p in allp) for i in range(3)]
    span = max(hi[i] - lo[i] for i in range(3)) * 1.1 + 1e-3
    # views: (horizontal axis, sign), (vertical axis, sign: up on the page), depth axis (towards the viewer), name
    views = [((0, 1), (1, 1), 2, 'side: x right, y up (seen from +z)'),
             ((0, 1), (2, -1), 1, 'top: x right, z down (seen from +y)'),
             ((2, -1), (1, 1), 0, 'front: -z right, y up (seen from +x)')]
    S = a.size
    img = Image.new('RGB', (S * 3, S + 40), (20, 20, 24))
    d = ImageDraw.Draw(img)
    light = [0.3, 0.8, 0.5]
    for vi, ((ha, hs), (va, vs), da, title) in enumerate(views):
        ox = vi * S
        cx = (lo[ha] + hi[ha]) / 2
        cy = (lo[va] + hi[va]) / 2

        def to2(p):
            return (ox + S / 2 + hs * (p[ha] - cx) / span * S, 40 + S / 2 - vs * (p[va] - cy) / span * S)
        order = sorted(tris, key=lambda t: sum(pts[i][da] for i in t) * (1 if da != 1 else 1))
        for t in order:
            p0, p1, p2 = (pts[i] for i in t)
            u = [p1[k] - p0[k] for k in range(3)]
            v = [p2[k] - p0[k] for k in range(3)]
            n = [u[1] * v[2] - u[2] * v[1], u[2] * v[0] - u[0] * v[2], u[0] * v[1] - u[1] * v[0]]
            ln = sum(c * c for c in n) ** 0.5
            if ln < 1e-6:
                continue
            sh = abs(sum(n[k] * light[k] for k in range(3)) / ln) * 0.7 + 0.3
            bones = [verts[i][1] for i in t]
            mixed = len(set(bones)) > 1
            col = (230, 30, 200) if mixed else COLOURS[bones[0] % 4]
            col = tuple(int(c * sh) for c in col)
            d.polygon([to2(p0), to2(p1), to2(p2)], fill=col)
        # the model's origin and axes (10 units)
        o = to2([0, 0, 0])
        for ax, colr in ((0, (255, 60, 60)), (1, (60, 255, 60)), (2, (60, 120, 255))):
            e = [0, 0, 0]
            e[ax] = span * 0.08
            d.line([o, to2(e)], fill=colr, width=2)
        for name, p in marks:
            x, y = to2(p)
            d.line([(x - 6, y - 6), (x + 6, y + 6)], fill=(255, 0, 0), width=2)
            d.line([(x - 6, y + 6), (x + 6, y - 6)], fill=(255, 0, 0), width=2)
            d.text((x + 8, y - 6), name, fill=(255, 220, 220))
        d.text((ox + 6, 6), title, fill=(230, 230, 230))
        d.text((ox + 6, 20), 'magenta: faces joining two bones; axes x red, y green, z blue', fill=(200, 200, 200))
    img.save(a.out)
    print('wrote', a.out, 'extents', [round(v, 1) for v in lo], [round(v, 1) for v in hi])


if __name__ == '__main__':
    main()
