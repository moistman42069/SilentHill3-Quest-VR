"""pgm_to_png.py: turn the rig's greyscale PGM pictures (P5, 8-bit: sh3vr_shape.cpp's SH3_SHAPE_SLICES) into PNGs,
with the standard library only.

    python -I pgm_to_png.py IN.pgm [IN2.pgm ...]   (each written beside it as .png)
"""
import struct
import sys
import zlib


def read_pgm(path):
    with open(path, 'rb') as fh:
        data = fh.read()
    parts, pos = [], 0
    while len(parts) < 4:
        while data[pos:pos + 1].isspace():
            pos += 1
        start = pos
        while not data[pos:pos + 1].isspace():
            pos += 1
        parts.append(data[start:pos])
    pos += 1
    if parts[0] != b'P5' or int(parts[3]) != 255:
        raise ValueError(path + ': not an 8-bit P5 PGM')
    w, h = int(parts[1]), int(parts[2])
    return w, h, data[pos:pos + w * h]


def write_png(path, w, h, pixels):
    def chunk(kind, body):
        return struct.pack('>I', len(body)) + kind + body + struct.pack('>I', zlib.crc32(kind + body) & 0xffffffff)
    raw = b''.join(b'\x00' + pixels[y * w:(y + 1) * w] for y in range(h))
    with open(path, 'wb') as fh:
        fh.write(b'\x89PNG\r\n\x1a\n')
        fh.write(chunk(b'IHDR', struct.pack('>IIBBBBB', w, h, 8, 0, 0, 0, 0)))
        fh.write(chunk(b'IDAT', zlib.compress(raw, 9)))
        fh.write(chunk(b'IEND', b''))


def main():
    for path in sys.argv[1:]:
        w, h, px = read_pgm(path)
        out = path[:-4] + '.png' if path.endswith('.pgm') else path + '.png'
        write_png(out, w, h, px)
        print(out, w, h)


if __name__ == '__main__':
    main()
