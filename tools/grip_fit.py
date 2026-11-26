"""grip_fit.py: measure a weapon mesh dump's grips, and fit Heather's hand on them (session 5: "i need proper grip poses
for every gun/grip point").

    python -I grip_fit.py slice MESH --frame B[:m00,..,m23] --axis x|y|z --at A0,A1,... [--bone-pose b:12 numbers]
        cross-sections of the faces at each value of one axis: the other two axes' extents (the grip's depth and width).
    python -I grip_fit.py points MESH --box x0,x1,y0,y1,z0,z1 [--bone-pose ...]
        the vertices inside a box (a trigger, a pump's ends).

Meshes are d3d8.c's SH3_MESH_DUMP files (mesh_N.txt: game units, each vertex on one model bone). --bone-pose b:m puts
bone b's vertices through the 3x4 m (rows: the game's idle pose of that bone against bone 0, from the log's "against
slot 0" lines) so the whole weapon is in one frame; --frame-inverse b:m instead brings everything into bone b's frame
(bone 0's vertices through m's inverse).
"""
import argparse
import math
import struct
import sys


def f32(w):
    return struct.unpack('<f', struct.pack('<I', w))[0]


def load(path):
    verts, idx, prim = [], [], 5
    with open(path) as fh:
        for line in fh:
            if line.startswith('#') and 'primitive type' in line:
                prim = int(line.split('primitive type')[1].split(',')[0])
            elif line.startswith('v '):
                ws = [int(x, 16) for x in line.split()[1:]]
                verts.append(([f32(w) for w in ws[0:3]], ws[6] & 0xFF))
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


def inverse(m):
    R = [m[0:3], m[4:7], m[8:11]]
    t = [m[3], m[7], m[11]]
    out = []
    for i in range(3):
        row = [R[j][i] for j in range(3)]
        out += row + [-sum(row[k] * t[k] for k in range(3))]
    return out


def placed(args):
    verts, tris = load(args.mesh)
    poses = {}
    for s in args.bone_pose or []:
        b, nums = s.split(':')
        poses[int(b)] = [float(x) for x in nums.split(',')]
    if args.frame_inverse:
        b, nums = args.frame_inverse.split(':')
        m = [float(x) for x in nums.split(',')]
        poses = {int(b): None, 0: inverse(m)}
    pts = []
    for p, b in verts:
        m = poses.get(b)
        pts.append(apply(m, p) if m else list(p))
    return pts, [v[1] for v in verts], tris


def cmd_slice(args):
    pts, bones, tris = placed(args)
    ax = 'xyz'.index(args.axis)
    o1, o2 = [k for k in range(3) if k != ax]
    for at in [float(v) for v in args.at.split(',')]:
        c1, c2 = [], []
        for t in tris:
            for a, b in ((t[0], t[1]), (t[1], t[2]), (t[2], t[0])):
                pa, pb = pts[a], pts[b]
                if (pa[ax] - at) * (pb[ax] - at) < 0:
                    s = (at - pa[ax]) / (pb[ax] - pa[ax])
                    q = [pa[k] + (pb[k] - pa[k]) * s for k in range(3)]
                    if args.box:
                        bx = [float(v) for v in args.box.split(',')]
                        if not all(bx[2 * k] <= q[k] <= bx[2 * k + 1] for k in range(3)):
                            continue
                    c1.append(q[o1])
                    c2.append(q[o2])
        if c1:
            print('  %s %7.1f: %s %7.1f .. %7.1f (%5.1f)   %s %7.1f .. %7.1f (%5.1f)' % (
                args.axis, at, 'xyz'[o1], min(c1), max(c1), max(c1) - min(c1), 'xyz'[o2], min(c2), max(c2),
                max(c2) - min(c2)))
        else:
            print('  %s %7.1f: nothing' % (args.axis, at))


def cmd_points(args):
    pts, bones, tris = placed(args)
    bx = [float(v) for v in args.box.split(',')]
    sel = [(i, p) for i, p in enumerate(pts) if all(bx[2 * k] <= p[k] <= bx[2 * k + 1] for k in range(3))]
    print(len(sel), 'vertices')
    seen = set()
    for i, p in sorted(sel, key=lambda t: (t[1][0], t[1][1], t[1][2])):
        key = tuple(round(v, 1) for v in p)
        if key in seen:
            continue
        seen.add(key)
        print('  %4d bone %d  %7.1f %7.1f %7.1f' % (i, bones[i], p[0], p[1], p[2]))


def project_views(spec):
    """'x,-y' -> ((0, 1), (1, -1), depth axis): the page's right and up as model axes with signs."""
    a, b = spec.split(',')

    def one(s):
        return ('xyz'.index(s[-1]), -1 if s.startswith('-') else 1)
    h, v = one(a), one(b)
    d = [k for k in range(3) if k not in (h[0], v[0])][0]
    # the viewer looks along the axis that makes (right, up, towards the viewer) right-handed
    e = [[0] * 3 for _ in range(3)]
    e[0][h[0]] = h[1]
    e[1][v[0]] = v[1]
    towards = [e[0][1] * e[1][2] - e[0][2] * e[1][1], e[0][2] * e[1][0] - e[0][0] * e[1][2],
               e[0][0] * e[1][1] - e[0][1] * e[1][0]]
    return h, v, d, towards[d]


def draw_view(pts, bones, tris, spec, box, size, grid, marks, capsules=(), title=''):
    from PIL import Image, ImageDraw
    (ha, hs), (va, vs), da, ds = project_views(spec)
    x0, x1, y0, y1 = box
    scale = size / max(x1 - x0, y1 - y0)
    W, H = int((x1 - x0) * scale), int((y1 - y0) * scale)
    img = Image.new('RGB', (W, H + 18), (20, 20, 24))
    d = ImageDraw.Draw(img)

    def to2(p):
        return ((p[ha] * hs - x0) * scale, 18 + (y1 - p[va] * vs) * scale)
    if grid:
        g = grid
        k = math.floor(x0 / g)
        while k * g <= x1:
            X = (k * g - x0) * scale
            d.line([(X, 18), (X, H + 18)], fill=(45, 45, 55) if k % 5 else (70, 70, 85))
            d.text((X + 2, 18), '%g' % (k * g), fill=(90, 90, 110))
            k += 1
        k = math.floor(y0 / g)
        while k * g <= y1:
            Y = 18 + (y1 - k * g) * scale
            d.line([(0, Y), (W, Y)], fill=(45, 45, 55) if k % 5 else (70, 70, 85))
            d.text((2, Y + 1), '%g' % (k * g), fill=(90, 90, 110))
            k += 1
    items = []
    light = [0.3, 0.8, 0.5]
    cols = [(170, 170, 170), (235, 140, 40), (80, 140, 235), (90, 200, 110)]
    for t in tris:
        p0, p1, p2 = (pts[i] for i in t)
        u = [p1[k] - p0[k] for k in range(3)]
        v = [p2[k] - p0[k] for k in range(3)]
        n = [u[1] * v[2] - u[2] * v[1], u[2] * v[0] - u[0] * v[2], u[0] * v[1] - u[1] * v[0]]
        ln = sum(c * c for c in n) ** 0.5
        if ln < 1e-6:
            continue
        sh = abs(sum(n[k] * light[k] for k in range(3)) / ln) * 0.6 + 0.35
        bs = [bones[i] for i in t]
        col = (230, 30, 200) if len(set(bs)) > 1 else cols[bs[0] % 4]
        depth = sum(p[da] for p in (p0, p1, p2)) / 3 * ds
        items.append((depth, 'tri', (to2(p0), to2(p1), to2(p2)), tuple(int(c * sh) for c in col)))
    for a, b, r, col in capsules:
        steps = max(2, int(math.dist(a, b) / max(r * 0.5, 0.5)))
        for s in range(steps + 1):
            q = [a[k] + (b[k] - a[k]) * s / steps for k in range(3)]
            items.append((q[da] * ds + 1e-3, 'disc', (to2(q), r * scale), col))
    items.sort(key=lambda it: it[0])
    for depth, kind, geo, col in items:
        if kind == 'tri':
            d.polygon(list(geo), fill=col)
        else:
            (cx, cy), rr = geo
            d.ellipse([cx - rr, cy - rr, cx + rr, cy + rr], fill=col)
    for name, p in marks:
        x, y = to2(p)
        d.line([(x - 5, y - 5), (x + 5, y + 5)], fill=(255, 40, 40), width=2)
        d.line([(x - 5, y + 5), (x + 5, y - 5)], fill=(255, 40, 40), width=2)
        d.text((x + 7, y - 6), name, fill=(255, 220, 220))
    d.text((4, 2), title or ('page right %s, up %s' % (spec.split(',')[0], spec.split(',')[1])), fill=(230, 230, 230))
    return img


def cmd_view(args):
    pts, bones, tris = placed(args)
    marks = []
    for s in args.point or []:
        name, xyz = s.split(':')
        marks.append((name, [float(x) for x in xyz.split(',')]))
    box = [float(v) for v in args.crop.split(',')]
    img = draw_view(pts, bones, tris, args.view, box, args.size, args.grid, marks)
    img.save(args.out)
    print('wrote', args.out)


def main():
    ap = argparse.ArgumentParser()
    sub = ap.add_subparsers(dest='cmd', required=True)
    for name in ('slice', 'points', 'view'):
        s = sub.add_parser(name)
        s.add_argument('mesh')
        s.add_argument('--bone-pose', action='append')
        s.add_argument('--frame-inverse')
        s.add_argument('--box')
        if name == 'slice':
            s.add_argument('--axis', required=True)
            s.add_argument('--at', required=True)
        if name == 'view':
            s.add_argument('out')
            s.add_argument('--view', default='x,y', help="the page's right and up as model axes, e.g. x,-y")
            s.add_argument('--crop', required=True, help='page left,right,bottom,top in those axes (signs applied)')
            s.add_argument('--size', type=int, default=900)
            s.add_argument('--grid', type=float, default=10)
            s.add_argument('--point', action='append')
    args = ap.parse_args()
    {'slice': cmd_slice, 'points': cmd_points, 'view': cmd_view}[args.cmd](args)


if __name__ == '__main__':
    sys.exit(main())
