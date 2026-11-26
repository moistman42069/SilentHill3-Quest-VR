#!/usr/bin/env python3
"""Reads bytes of sh3.exe at a virtual address, as the loaded image holds them.

Usage: peek.py VA COUNT [fmt] [exe]   fmt: x8 (default), u16, u32, f32, s (string)
  peek.py 0x7214E4 25 u16
"""
import struct, sys
import pefile

va, count = int(sys.argv[1], 16), int(sys.argv[2], 0)
fmt = sys.argv[3] if len(sys.argv) > 3 else 'x8'
exe = sys.argv[4] if len(sys.argv) > 4 else 'game/sh3.exe'
pe = pefile.PE(exe, fast_load=True)
rva = va - pe.OPTIONAL_HEADER.ImageBase
size = {'x8': 1, 'u16': 2, 'u32': 4, 'f32': 4, 's': 1}[fmt]
data = pe.get_data(rva, count * size)
if fmt == 's':
    print(data.split(b'\0')[0].decode('latin-1'))
elif fmt == 'x8':
    for o in range(0, len(data), 16):
        print(f'{va + o:08X}  ' + ' '.join(f'{b:02x}' for b in data[o:o + 16]))
else:
    code = {'u16': '<H', 'u32': '<I', 'f32': '<f'}[fmt]
    for i in range(count):
        v = struct.unpack_from(code, data, i * size)[0]
        print(f'{va + i * size:08X}  {v:#06x}' if fmt != 'f32' else f'{va + i * size:08X}  {v}')
