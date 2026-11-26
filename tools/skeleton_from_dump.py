#!/usr/bin/env python3
"""skeleton_from_dump.py sh3.log x y z yaw_deg: Heather's skeleton from an SH3_VSC_DUMP frame (LIST 19).

Her skinned draws (stride 48) each carry 16 bones from c48; a bone shared by several draws has the same rows in each.
This merges them into one list of distinct bones and prints each in her own frame (metres: right, up, forward from
the given position, the game's yaw turning her), with the draws and palette slots that use it.
"""
import math
import sys

sys.path.insert(0, __file__.rsplit('\\', 1)[0].rsplit('/', 1)[0])
from bones_from_dump import bone, draws  # noqa: E402

UNITS = 360.0  # game units a metre (sh3vr_internal.h kUnitsPerMetre)


def main():
    path = sys.argv[1]
    x0, y0, z0, yaw = (float(v) for v in sys.argv[2:6])
    s, c = math.sin(math.radians(yaw)), math.cos(math.radians(yaw))
    found = {}
    for d in draws(path):
        if d['stride'] != 48:
            continue
        for b in range(16):
            r = bone(d['c'], b)
            if not r:
                continue
            p = (r[0][3], r[1][3], r[2][3])
            if math.hypot(p[0] - x0, p[2] - z0) > 1.2 * UNITS:
                continue
            key = tuple(round(v, 2) for row in r for v in row)
            found.setdefault(key, {'p': p, 'r': r, 'uses': []})['uses'].append((d['id'], b))
    rows = []
    for key, f in found.items():
        dx, dy, dz = f['p'][0] - x0, f['p'][1] - y0, f['p'][2] - z0
        # her frame: y down in the game, so up = -dy; yaw turns the ground plane
        right = (dx * c - dz * s) / UNITS
        fwd = (dx * s + dz * c) / UNITS
        up = -dy / UNITS
        rows.append((up, right, fwd, f))
    rows.sort(key=lambda t: -t[0])
    for i, (up, right, fwd, f) in enumerate(rows):
        uses = ' '.join(f'{d}:{b}' for d, b in f['uses'])
        print(f"{i:2d} up {up:6.3f} right {right:6.3f} fwd {fwd:6.3f}  uses {uses}")


if __name__ == '__main__':
    main()
