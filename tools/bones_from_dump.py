#!/usr/bin/env python3
"""bones_from_dump.py sh3.log [x z radius]: the skinned draws of an SH3_VSC_DUMP frame (LIST 19, 24).

Each skinned draw's palette is 16 bones from c48, three world-space rows (rotation, translation in w). Prints, per
draw, its signature (shader, vertices, primitives, stride) and its bones' positions; with x z radius (game units), only
draws with a bone within radius of (x, z) on the ground plane: Heather's, when given her position.
"""
import math
import sys


def draws(path):
    cur = None
    for line in open(path, encoding='utf-8', errors='replace'):
        if line.startswith('vsc draw '):
            if cur:
                yield cur
            w = line.split()
            cur = {'id': int(w[2].rstrip(':')), 'vs': w[4], 'nv': int(w[6]), 'np': int(w[8]), 'stride': int(w[10]),
                   'c': {}}
        elif cur and line.startswith('  c'):
            w = line.split()
            cur['c'][int(w[0][1:])] = [float(v) for v in w[1:5]]
    if cur:
        yield cur


def bone(c, b):
    rows = [c.get(48 + b * 3 + r) for r in range(3)]
    if any(r is None for r in rows):
        return None
    for r in rows:
        if abs(r[0] * r[0] + r[1] * r[1] + r[2] * r[2] - 1.0) > 0.08:
            return None
    return rows


def main():
    path = sys.argv[1]
    near = None
    if len(sys.argv) >= 5:
        near = (float(sys.argv[2]), float(sys.argv[3]), float(sys.argv[4]))
    for d in draws(path):
        bones = [bone(d['c'], b) for b in range(16)]
        whole = [(b, r) for b, r in enumerate(bones) if r]
        if len(whole) < 2:
            continue
        pos = [(b, (r[0][3], r[1][3], r[2][3])) for b, r in whole]
        if near and not any(math.hypot(p[0] - near[0], p[2] - near[1]) < near[2] for _, p in pos):
            continue
        print(f"draw {d['id']}: vs {d['vs']} nv {d['nv']} np {d['np']} stride {d['stride']}, {len(whole)} bones")
        for b, p in pos:
            print(f"    bone {b:2d}: {p[0]:10.1f} {p[1]:8.1f} {p[2]:10.1f}")


if __name__ == '__main__':
    main()
