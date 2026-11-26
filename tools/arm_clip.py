"""Heather's bones through the game's camera in an SH3_VSC_DUMP frame (the PC rig, LIST 19).

    python tools/arm_clip.py <log>

Takes the 73-bone model's bones the dump logged (after VR's arms were solved: the dump follows the solve in
sh3.c's 0x45EC00 wrapper) and the frame camera's view x projection (c2-c5 of a main draw of a skinned shader,
stride 48), and prints where each bone of her arms, chest and head lands: x/w and y/w (-1..1 of the game's wide
picture, y down) and w (the depth, game units and metres at 490 a metre).
"""
import re
import sys

NAMES = {0: "upper root", 2: "spine", 4: "chest", 6: "neck", 9: "head", 18: "head top", 11: "collar A",
         15: "collar B", 26: "shoulder A", 32: "shoulder B", 34: "elbow A", 36: "elbow B", 37: "forearm A",
         38: "forearm B", 39: "hand A", 41: "hand B", 1: "lower root", 33: "foot A", 35: "foot B"}


def main(path):
    lines = open(path, encoding="utf-8", errors="replace").read().splitlines()
    bones, cur, n = None, None, 0
    vp, in_draw, main, stride, rows = None, False, 0, 0, {}
    for ln in lines:
        m = re.search(r"bones: model \S+ skeleton \S+ world \S+ flag \S+, (\d+) bones", ln)
        if m:
            n = int(m.group(1))
            cur = {} if n == 73 else None
            continue
        m = re.match(r"\s+bone\s+(\d+): (.*)", ln)
        if m and cur is not None:
            vals = [float(v) for v in m.group(2).replace("|", " ").split()]
            cur[int(m.group(1))] = vals
            if len(cur) == 73:
                bones = cur
            continue
        m = re.search(r"vsc draw \d+: vs \S+ nv \d+ np \d+ stride (\d+) main (\d+)", ln)
        if m:
            if in_draw and vp is None and main and stride == 48 and all(k in rows for k in (2, 3, 4, 5)):
                vp = [rows[k] for k in (2, 3, 4, 5)]
            in_draw, stride, main, rows = True, int(m.group(1)), int(m.group(2)), {}
            continue
        m = re.match(r"\s+c(\d\d)\s+(\S+)\s+(\S+)\s+(\S+)\s+(\S+)", ln)
        if m and in_draw:
            rows[int(m.group(1))] = [float(m.group(i)) for i in range(2, 6)]
    if bones is None or vp is None:
        print("no 73-bone model or no skinned main draw in this log")
        return 1
    for b in sorted(NAMES):
        x, y, z = bones[b][12], bones[b][13], bones[b][14]
        c = [r[0] * x + r[1] * y + r[2] * z + r[3] for r in vp]
        w = c[3]
        sx = c[0] / w if abs(w) > 1e-6 else float("nan")
        sy = c[1] / w if abs(w) > 1e-6 else float("nan")
        print(f"{b:3d} {NAMES[b]:11s} world {x:9.1f} {y:8.1f} {z:9.1f}   screen {sx:6.2f} {sy:6.2f}   depth {w:7.1f} "
              f"({w / 490:5.2f} m)")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1]))
