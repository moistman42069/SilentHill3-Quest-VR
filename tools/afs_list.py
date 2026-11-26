#!/usr/bin/env python3
"""SH3 Quest VR: lists an AFS archive's entries (offset, size, first bytes) - data/sound/sd.afs holds the game's sounds.

Usage: afs_list.py <file.afs> [first] [count] [--dump DIR]  (--dump writes each listed entry as DIR/<index>.bin)
"""
import os, struct, sys

path = sys.argv[1]
args = [a for a in sys.argv[2:] if not a.startswith('--')]
first = int(args[0], 0) if args else 0
count = int(args[1], 0) if len(args) > 1 else 40
dump = None
if '--dump' in sys.argv:
    dump = sys.argv[sys.argv.index('--dump') + 1]
    os.makedirs(dump, exist_ok=True)
with open(path, 'rb') as f:
    head = f.read(8)
    if head[:3] != b'AFS':
        sys.exit(f'not AFS: {head!r}')
    n = struct.unpack('<I', head[4:8])[0]
    table = f.read(8 * n)
    print(f'{n} entries')
    for i in range(first, min(n, first + count)):
        off, size = struct.unpack('<II', table[8 * i:8 * i + 8])
        f.seek(off)
        data = f.read(min(size, 64)) if not dump else f.read(size)
        print(f'{i:5d} 0x{i:03X}: offset 0x{off:08X} size {size:8d}  {data[:32].hex()}  {bytes(c if 32 <= c < 127 else 46 for c in data[:32]).decode()}')
        if dump:
            open(os.path.join(dump, f'{i:04d}.bin'), 'wb').write(data)
