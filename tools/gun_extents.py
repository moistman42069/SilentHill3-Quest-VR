"""Per-bone extents of a gun mesh dump (d3d8.c SH3_MESH_DUMP): where each bone's vertices lie, in model units."""
import struct
import sys

for path in sys.argv[1:]:
    verts = []
    with open(path) as f:
        for line in f:
            if line.startswith('v '):
                ws = [int(x, 16) for x in line.split()[1:]]
                pos = [struct.unpack('<f', struct.pack('<I', w))[0] for w in ws[0:3]]
                verts.append((pos, ws[6] & 0xFF, ws[3:12]))
    print('==', path, len(verts), 'vertices; words per vertex', len(verts[0][2]) + 3)
    bones = sorted(set(b for _, b, _ in verts))
    for b in bones:
        ps = [p for p, bb, _ in verts if bb == b]
        lo = [min(p[i] for p in ps) for i in range(3)]
        hi = [max(p[i] for p in ps) for i in range(3)]
        c = [sum(p[i] for p in ps) / len(ps) for i in range(3)]
        print(' bone %d: %d vertices, x %.1f..%.1f  y %.1f..%.1f  z %.1f..%.1f  centre %.1f %.1f %.1f' %
              (b, len(ps), lo[0], hi[0], lo[1], hi[1], lo[2], hi[2], c[0], c[1], c[2]))
        # the lowest tenth of the bone's vertices (by y): where its grip hangs, if it has one
        ys = sorted(ps, key=lambda p: p[1])
        low = ys[:max(1, len(ys) // 10)]
        print('   lowest tenth: x %.1f..%.1f, y %.1f..%.1f' % (min(p[0] for p in low), max(p[0] for p in low),
                                                             min(p[1] for p in low), max(p[1] for p in low)))
        hi_ = ys[-max(1, len(ys) // 10):]
        print('   highest tenth: x %.1f..%.1f, y %.1f..%.1f' % (min(p[0] for p in hi_), max(p[0] for p in hi_),
                                                              min(p[1] for p in hi_), max(p[1] for p in hi_)))
    # x histogram over all vertices, 10 bins, with the mean y of each bin
    xs = [p[0] for p, _, _ in verts]
    x0, x1 = min(xs), max(xs)
    for k in range(10):
        a = x0 + (x1 - x0) * k / 10
        bb = x0 + (x1 - x0) * (k + 1) / 10
        sel = [p for p, _, _ in verts if a <= p[0] <= bb]
        if sel:
            print('   x %7.1f..%7.1f: %3d vertices, y %.1f..%.1f' % (a, bb, len(sel), min(p[1] for p in sel),
                                                                     max(p[1] for p in sel)))
