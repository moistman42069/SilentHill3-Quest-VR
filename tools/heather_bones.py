#!/usr/bin/env python3
"""heather_bones.py sh3.log: Heather's bones from an SH3_VSC_DUMP frame's "bones:" lines (src/game/sh3.c, 0x45EC00).

Prints each bone's place in her own frame (game units: right, up, forward from her position, her yaw turning it) and
the nearest bone above it in the list that could be its parent (the nearest earlier bone), for naming the arm chains.
"""
import math
import re
import sys


def main():
    pos = yaw = None
    bones = []
    for line in open(sys.argv[1], encoding='utf-8', errors='replace'):
        m = re.match(r'bones: Heather \(\w+\) at (\S+) (\S+) (\S+) yaw (\S+)', line)
        if m:
            pos = [float(m.group(i)) for i in (1, 2, 3)]
            yaw = float(m.group(4))
            bones = []
            continue
        if line.startswith('  bone ') and pos is not None:
            nums = [float(v) for v in re.findall(r'-?\d+\.\d+', line.split(':', 1)[1])]
            if len(nums) == 16:
                bones.append(nums)
        elif line.startswith('bones: model') and bones:
            break
    if not bones:
        sys.exit('no bones in the log')
    s, c = math.sin(yaw), math.cos(yaw)
    print(f"Heather at {pos}, yaw {yaw:.4f}; {len(bones)} bones (right, up, forward; units)")
    places = []
    for i, b in enumerate(bones):
        dx, dy, dz = b[12] - pos[0], b[13] - pos[1], b[14] - pos[2]
        # the game's yaw 0 faces +z; her right is then -x? Both printed: a = along x turned, f = along z turned
        a = dx * c - dz * s
        f = dx * s + dz * c
        places.append((a, -dy, f))
    for i, (a, u, f) in enumerate(places):
        near = min(range(i), key=lambda j: math.dist(places[j], places[i]), default=None) if i else None
        nd = math.dist(places[near], places[i]) if near is not None else 0
        print(f"{i:3d}: a {a:7.1f} up {u:6.1f} f {f:7.1f}   nearest earlier {near} at {nd:6.1f}")


if __name__ == '__main__':
    main()
