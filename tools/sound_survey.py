#!/usr/bin/env python3
"""SH3 Quest VR: a survey of the game's sounds (data/sound/sd.afs: 378 plain PCM WAVs, entry n = the game's sound id n,
as the guns' fire / reload / empty-click triples 0x27-0x29, 0x3B-0x3D, 0x51-0x53 show). For each: the format, the
length, the peak, how many separate hits (onsets) it has, where its energy sits (spectral centroid, Hz) and how fast it
dies away, so that sounds can be picked for the VR hand actions without hearing them.

Usage: sound_survey.py <sd.afs> [first] [count] [--wav DIR]   (--wav writes the listed entries as DIR/<id hex>.wav)
"""
import os, struct, sys
import array, math

path = sys.argv[1]
args = [a for a in sys.argv[2:] if not a.startswith('--') and not os.path.isdir(a)]
first = int(args[0], 0) if args else 0
count = int(args[1], 0) if len(args) > 1 else 400
wav_dir = sys.argv[sys.argv.index('--wav') + 1] if '--wav' in sys.argv else None
if wav_dir:
    os.makedirs(wav_dir, exist_ok=True)


def parse_wav(b):
    if b[:4] != b'RIFF' or b[8:12] != b'WAVE':
        return None
    pos, fmt, data = 12, None, None
    while pos + 8 <= len(b):
        cid, size = b[pos:pos + 4], struct.unpack('<I', b[pos + 4:pos + 8])[0]
        body = b[pos + 8:pos + 8 + size]
        if cid == b'fmt ':
            fmt = struct.unpack('<HHIIHH', body[:16])
        elif cid == b'data':
            data = body
        pos += 8 + size + (size & 1)
    return fmt, data


def features(fmt, data):
    tag, ch, rate, _, _, bits = fmt
    if tag != 1 or bits != 16:
        return None
    a = array.array('h')
    a.frombytes(data[:len(data) // 2 * 2])
    if sys.byteorder != 'little':
        a.byteswap()
    x = [v / 32768.0 for v in a]
    if ch > 1:
        x = [sum(x[i:i + ch]) / ch for i in range(0, len(x) - ch + 1, ch)]
    n = len(x)
    if n < 64:
        return dict(rate=rate, ch=ch, dur=n / rate, peak=0, onsets=0, centroid=0, decay=0, tail=0)
    win = max(64, rate // 100)  # 10 ms frames
    frames = n // win
    db = []
    for k in range(frames):
        s = x[k * win:(k + 1) * win]
        db.append(20 * math.log10(math.sqrt(sum(v * v for v in s) / win + 1e-12) + 1e-9))
    top = max(db)
    # onsets: a frame 9 dB over the frame two before and within 30 dB of the loudest
    on = 0
    last = -10
    for i in range(2, frames):
        if db[i] > top - 30 and db[i] - db[i - 2] > 9 and i - last > 4:
            on += 1
            last = i
    # brightness without an FFT: the zero-crossing rate of the loud frames (half the crossings a second ~ the
    # dominant frequency; noise and clicks read high, thuds low)
    zc = cnt = 0
    for k in range(frames):
        if db[k] < top - 25:
            continue
        s = x[k * win:(k + 1) * win]
        zc += sum(1 for j in range(1, len(s)) if (s[j - 1] < 0) != (s[j] < 0))
        cnt += len(s)
    centroid = zc / max(cnt, 1) * rate / 2
    loud = db.index(top)
    decay = (frames - loud) * win / rate
    for j in range(loud, frames):
        if db[j] < top - 20:
            decay = (j - loud) * win / rate
            break
    tail = sum(db[-3:]) / 3 - top  # a loop ends loud (close to 0 dB below the top)
    return dict(rate=rate, ch=ch, dur=n / rate, peak=max(abs(v) for v in x), onsets=on, centroid=centroid,
                decay=decay, tail=tail)


if '--files' in sys.argv:
    # a folder of WAVs (the hand sounds SH3_SOUND_DUMP writes): each one measured the same way
    folder = sys.argv[sys.argv.index('--files') + 1]
    print('file             rate  ch   len s  peak  hits  bright Hz  -20dB s  end dB')
    for name in sorted(os.listdir(folder)):
        if not name.lower().endswith('.wav'):
            continue
        b = open(os.path.join(folder, name), 'rb').read()
        p = parse_wav(b)
        ft = features(*p) if p and p[0] and p[1] is not None else None
        if ft is None:
            print(f'{name:14s} not read')
            continue
        print(f'{name:14s} {ft["rate"]:6d} {ft["ch"]:2d} {ft["dur"]:7.3f} {ft["peak"]:5.2f} {ft["onsets"]:5d} '
              f'{ft["centroid"]:10.0f} {ft["decay"]:9.3f} {ft["tail"]:7.1f}')
    sys.exit(0)

with open(path, 'rb') as f:
    head = f.read(8)
    n = struct.unpack('<I', head[4:8])[0]
    table = f.read(8 * n)
    print('id     rate  ch   len s  peak  hits  bright Hz  -20dB s  end dB')
    for i in range(first, min(n, first + count)):
        off, size = struct.unpack('<II', table[8 * i:8 * i + 8])
        f.seek(off)
        b = f.read(size)
        p = parse_wav(b)
        if not p or not p[0] or p[1] is None:
            print(f'0x{i:03X}  not a WAV ({b[:4]!r})')
            continue
        ft = features(*p)
        if ft is None:
            print(f'0x{i:03X}  format {p[0]}')
            continue
        print(f'0x{i:03X} {ft["rate"]:6d} {ft["ch"]:2d} {ft["dur"]:7.3f} {ft["peak"]:5.2f} {ft["onsets"]:5d} '
              f'{ft["centroid"]:10.0f} {ft["decay"]:9.3f} {ft["tail"]:7.1f}')
        if wav_dir:
            open(os.path.join(wav_dir, f'{i:03x}.wav'), 'wb').write(b)
