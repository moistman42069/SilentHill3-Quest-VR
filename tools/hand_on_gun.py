"""hand_on_gun.py: draw a weapon mesh dump and one of Heather's gripping-hand mesh dumps together, in three views.

    python -I tools/hand_on_gun.py out.png GUN_MESH HAND_MESH [--gun-bone 0] [--hand-xf 12 numbers] [--size 520]

The game holds every weapon's bone 0 exactly at her right hand bone (session 5: the identity relation), so a weapon's
bone-0 vertices and a right gripping hand's vertices (both SH3_MESH_DUMP files, in their bone's own units) share one
frame: what the game itself shows in her hand. --hand-xf moves the hand first (3x4 rows: a pose to try). Faces are
filled back to front (the hand skin-coloured, the gun grey) in the side (x right, y up), top (x right, z down) and
front (z right, y up) views, with a 10-unit grid (490 units a metre).
"""
import argparse
import os
import sys

from PIL import Image, ImageDraw

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from grip_fit import load  # noqa: E402


def faces(path, bone=None, xf=None):
    verts, tris = load(path)
    out = []
    for t in tris:
        vs = [verts[i] for i in t]
        if bone is not None and any(v[1] != bone for v in vs):
            continue
        pts = [v[0] for v in vs]
        if xf:
            pts = [[xf[r * 4] * p[0] + xf[r * 4 + 1] * p[1] + xf[r * 4 + 2] * p[2] + xf[r * 4 + 3] for r in range(3)]
                   for p in pts]
        if len({tuple(p) for p in pts}) == 3:
            out.append(pts)
    return out


def view(gun, hand, right, up, depth, size, box):
    im = Image.new("RGB", (size, size), (25, 25, 30))
    d = ImageDraw.Draw(im)
    (l, r), (b, t) = box
    sx, sy = size / (r - l), size / (t - b)

    def P(p):
        return ((p[right[0]] * right[1] - l) * sx, (t - p[up[0]] * up[1]) * sy)

    for g in range(int(l // 10) * 10, int(r) + 1, 10):
        x = (g - l) * sx
        d.line([(x, 0), (x, size)], fill=(60, 60, 60) if g else (120, 60, 60))
    for g in range(int(b // 10) * 10, int(t) + 1, 10):
        y = (t - g) * sy
        d.line([(0, y), (size, y)], fill=(60, 60, 60) if g else (120, 60, 60))
    allf = [(f, (150, 150, 155)) for f in gun] + [(f, (215, 160, 125)) for f in hand]
    allf.sort(key=lambda fc: sum(p[depth[0]] * depth[1] for p in fc[0]))
    for f, c in allf:
        # shade by the face's facing (a little light from the viewer)
        a, bq, cq = f
        u = [bq[i] - a[i] for i in range(3)]
        v = [cq[i] - a[i] for i in range(3)]
        n = [u[1] * v[2] - u[2] * v[1], u[2] * v[0] - u[0] * v[2], u[0] * v[1] - u[1] * v[0]]
        ln = sum(x * x for x in n) ** 0.5 or 1
        k = 0.45 + 0.55 * abs(n[depth[0]] / ln)
        d.polygon([P(p) for p in f], fill=tuple(int(x * k) for x in c), outline=tuple(int(x * k * 0.7) for x in c))
    return im


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("out")
    ap.add_argument("gun")
    ap.add_argument("hand")
    ap.add_argument("--gun-bone", type=int, default=0)
    ap.add_argument("--hand-xf", type=float, nargs=12)
    ap.add_argument("--size", type=int, default=520)
    ap.add_argument("--box", type=float, nargs=6, default=[-60, 140, -80, 80, -80, 60],
                    help="x0 x1 y0 y1 z0 z1 of the views")
    a = ap.parse_args()
    gun = faces(a.gun, a.gun_bone)
    hand = faces(a.hand, None, a.hand_xf)
    x0, x1, y0, y1, z0, z1 = a.box
    side = view(gun, hand, (0, 1), (1, 1), (2, -1), a.size, ((x0, x1), (y0, y1)))      # looking along -z... from +z
    top = view(gun, hand, (0, 1), (2, -1), (1, 1), a.size, ((x0, x1), (-z1, -z0)))    # from +y (up)
    front = view(gun, hand, (2, 1), (1, 1), (0, 1), a.size, ((z0, z1), (y0, y1)))     # from +x (the muzzle's end)
    sheet = Image.new("RGB", (a.size * 3 + 8, a.size + 18), (0, 0, 0))
    dr = ImageDraw.Draw(sheet)
    for i, (name, im) in enumerate((("side: x right, y up (from +z)", side), ("top: x right, -z up (from +y)", top),
                                    ("front: z right, y up (from +x)", front))):
        sheet.paste(im, (i * (a.size + 4), 18))
        dr.text((i * (a.size + 4) + 4, 3), name, fill=(255, 255, 0))
    sheet.save(a.out)
    print("gun faces %d, hand faces %d" % (len(gun), len(hand)))


if __name__ == "__main__":
    main()
