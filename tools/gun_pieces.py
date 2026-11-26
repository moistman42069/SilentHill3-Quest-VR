"""List a gun mesh's connected pieces (faces sharing vertices) with their extents, and count the faces a moving set
of vertices would stretch (faces with some vertices in the set and some out).

    python gun_pieces.py <mesh_N.txt> [bone]
"""
import struct
import sys


def load(path):
    verts, idx = [], []
    with open(path) as f:
        for line in f:
            if line.startswith('v '):
                ws = [int(x, 16) for x in line.split()[1:]]
                pos = [struct.unpack('<f', struct.pack('<I', w))[0] for w in ws[0:3]]
                verts.append((pos, ws[6] & 0xFF))
            elif line.startswith('i'):
                idx = [int(x) for x in line.split()[1:]]
    tris = []
    for k in range(len(idx) - 2):
        a, b, c = idx[k], idx[k + 1], idx[k + 2]
        if a == b or b == c or a == c:
            continue
        pa, pb, pc = verts[a][0], verts[b][0], verts[c][0]
        u = [pb[i] - pa[i] for i in range(3)]
        v = [pc[i] - pa[i] for i in range(3)]
        cr = [u[1] * v[2] - u[2] * v[1], u[2] * v[0] - u[0] * v[2], u[0] * v[1] - u[1] * v[0]]
        if (cr[0] ** 2 + cr[1] ** 2 + cr[2] ** 2) ** 0.5 < 1e-3:
            continue
        tris.append((a, b, c))
    return verts, tris


def pieces(n, tris):
    parent = list(range(n))

    def find(x):
        while parent[x] != x:
            parent[x] = parent[parent[x]]
            x = parent[x]
        return x

    for a, b, c in tris:
        for u, v in ((a, b), (b, c)):
            ru, rv = find(u), find(v)
            if ru != rv:
                parent[ru] = rv
    groups = {}
    used = {i for t in tris for i in t}
    for i in range(n):
        if i in used:
            groups.setdefault(find(i), []).append(i)
    return list(groups.values())


verts, tris = load(sys.argv[1])
bone = int(sys.argv[2]) if len(sys.argv) > 2 else None
ps = pieces(len(verts), tris)
ps.sort(key=lambda g: -len(g))
print(len(verts), 'vertices,', len(tris), 'faces,', len(ps), 'pieces')
for k, g in enumerate(ps):
    bones = sorted(set(verts[i][1] for i in g))
    if bone is not None and bone not in bones:
        continue
    lo = [min(verts[i][0][j] for i in g) for j in range(3)]
    hi = [max(verts[i][0][j] for i in g) for j in range(3)]
    nf = sum(1 for t in tris if t[0] in set(g))
    print('piece %2d: %3d vertices %3d faces bones %s  x %6.1f..%6.1f  y %6.1f..%6.1f  z %6.1f..%6.1f  first %d' %
          (k, len(g), nf, bones, lo[0], hi[0], lo[1], hi[1], lo[2], hi[2], min(g)))
