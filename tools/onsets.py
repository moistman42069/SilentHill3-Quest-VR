#!/usr/bin/env python3
"""SH3 Quest VR: the separate hits in one of the game's sounds (data/sound/sd.afs entry), each one's start, peak and
length, to pick one hit of a many-hit sound (the shotgun's reload 0x3C) for a hand action.

Usage: onsets.py <sd.afs> <id>
"""
import array, math, struct, sys

path, sid = sys.argv[1], int(sys.argv[2], 0)
with open(path, 'rb') as f:
    f.seek(8 + 8 * sid)
    off, size = struct.unpack('<II', f.read(8))
    f.seek(off)
    b = f.read(size)
pos, fmt, data = 12, None, None
while pos + 8 <= len(b):
    cid, n = b[pos:pos + 4], struct.unpack('<I', b[pos + 4:pos + 8])[0]
    if cid == b'fmt ':
        fmt = struct.unpack('<HHIIHH', b[pos + 8:pos + 24])
    elif cid == b'data':
        data = b[pos + 8:pos + 8 + n]
    pos += 8 + n + (n & 1)
rate, ch = fmt[2], fmt[1]
a = array.array('h')
a.frombytes(data[:len(data) // 2 * 2])
x = [v / 32768.0 for v in a][::ch]
win = rate // 200  # 5 ms
db = []
for k in range(len(x) // win):
    s = x[k * win:(k + 1) * win]
    db.append(20 * math.log10(math.sqrt(sum(v * v for v in s) / win) + 1e-9))
top = max(db)
starts = [i for i in range(2, len(db)) if db[i] > top - 30 and db[i] - db[i - 2] > 8]
hits = []
for i in starts:
    if hits and i - hits[-1] < 8:
        continue
    hits.append(i)
print(f'0x{sid:X}: {len(x) / rate:.3f} s at {rate} Hz, loudest frame {top:.1f} dB')
for j, i in enumerate(hits):
    end = hits[j + 1] if j + 1 < len(hits) else len(db)
    seg = x[i * win:end * win]
    pk = max(abs(v) for v in seg) if seg else 0
    print(f'  hit {j}: {i * win / rate * 1000:6.0f} ms to {end * win / rate * 1000:6.0f} ms, peak {pk:.3f}, '
          f'loudest {max(db[i:end]):.1f} dB')
