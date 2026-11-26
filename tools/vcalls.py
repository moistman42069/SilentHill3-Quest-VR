#!/usr/bin/env python3
"""Finds indirect calls through a vtable slot (call [reg+disp]) in sh3.exe's .text, e.g. IDirect3DDevice8 methods:
SetTransform is slot 37 (disp 0x94), SetVertexShaderConstant slot 79 (0x13C), SetRenderTarget 31 (0x7C).

Usage: vcalls.py 0x94 [0x13C ...] [--exe=game/sh3.exe] [--ctx=N]   (prints each call with N instructions before it)
"""
import struct, sys
import pefile
from capstone import Cs, CS_ARCH_X86, CS_MODE_32

args = [a for a in sys.argv[1:] if not a.startswith('--')]
exe = next((a.split('=', 1)[1] for a in sys.argv[1:] if a.startswith('--exe=')), 'game/sh3.exe')
ctx = int(next((a.split('=', 1)[1] for a in sys.argv[1:] if a.startswith('--ctx=')), '0'))
pe = pefile.PE(exe, fast_load=True)
base = pe.OPTIONAL_HEADER.ImageBase
text = next(s for s in pe.sections if s.Name.startswith(b'.text'))
tva = base + text.VirtualAddress
code = text.get_data()
md = Cs(CS_ARCH_X86, CS_MODE_32)

for a in args:
    disp = int(a, 16)
    hits = []
    for o in range(len(code) - 6):
        if code[o] != 0xFF:
            continue
        modrm = code[o + 1]
        mod, reg, rm = modrm >> 6, (modrm >> 3) & 7, modrm & 7
        if reg != 2 or rm == 4:  # /2 = call; rm 4 needs a SIB byte
            continue
        if mod == 2 and struct.unpack_from('<i', code, o + 2)[0] == disp:
            hits.append(o)
        elif mod == 1 and disp < 0x80 and code[o + 2] == disp:
            hits.append(o)
    print(f'== call [reg+{disp:#x}]: {len(hits)}')
    for o in hits:
        print(f'   {tva + o:#010x}')
        if ctx:
            start = max(0, o - ctx * 7)
            lines = list(md.disasm(code[start:o + 6], tva + start))
            for ins in lines[-(ctx + 1):]:
                print(f'      {ins.address:08X}  {ins.mnemonic} {ins.op_str}')
