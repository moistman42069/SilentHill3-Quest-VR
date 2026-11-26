"""Which of a model's bones each skinned draw uploads, in an SH3_VSC_DUMP frame (the PC rig).

    python tools/palette_bones.py <log> [bone count, default 73]

0x45EC00 fills the bone array at [0x6B4540] (the `bones:` line's "at" address), 64 bytes a bone; 0x45EC90 builds the
blended matrices 0x2000 after it. The parts' upload loops (0x45F403: a bone of the array, 0x45F465: a blended one)
are logged by d3d8.c as "vsc upload cN x3 from CALLER, data at PTR"; this turns each PTR back into a bone number
(b) or a blend number (B) and groups them by the draw that follows.
"""
import re
import sys


def main(path, count):
    base = None
    group = []
    draws = []
    for ln in open(path, encoding="utf-8", errors="replace"):
        m = re.search(r"bones: model \S+ skeleton \S+ world \S+ flag \S+, (\d+) bones at ([0-9A-Fa-f]+)", ln)
        if m:
            base = int(m.group(2), 16) if int(m.group(1)) == count else None
            continue
        m = re.search(r"vsc upload c(\d+) x3 from ([0-9A-Fa-f]+), data at ([0-9A-Fa-f]+)", ln)
        if m and base is not None:
            reg, caller, ptr = int(m.group(1)), int(m.group(2), 16), int(m.group(3), 16)
            if caller in (0x45F403, 0x45F465):
                off = ptr - base
                if 0 <= off < count * 64 and off % 64 == 0:
                    group.append((reg, "b%d" % (off // 64)))
                elif 0 <= off - 0x2000 < 0x4000 and (off - 0x2000) % 64 == 0:
                    group.append((reg, "B%d" % ((off - 0x2000) // 64)))
                else:
                    group.append((reg, "?%X" % ptr))
            continue
        m = re.search(r"vsc draw \d+: vs (\S+) nv (\d+) np (\d+) stride (\d+)", ln)
        if m and group:
            draws.append((m.group(1), int(m.group(2)), int(m.group(3)), int(m.group(4)), sorted(group)))
            group = []
    for vs, nv, np_, stride, g in draws:
        print("%s nv %4d np %4d stride %d: %s" % (vs, nv, np_, stride, " ".join("c%d=%s" % (r, b) for r, b in g)))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1], int(sys.argv[2]) if len(sys.argv) > 2 else 73))
