#!/usr/bin/env python3
"""Lists the direct calls (E8 rel32) and jumps (E9) to each target VA, with the function each sits in (the nearest
preceding 'push ebp; mov ebp, esp' or the start after a run of int3/nop padding), and loads/stores of an absolute
address (for globals) when the target is given as 'data:VA'.

Usage: xrefs.py 0x600B60 0x600B70 data:0x70E6408 [--exe game/sh3.exe]
"""
import struct, sys
import pefile

args = [a for a in sys.argv[1:] if not a.startswith('--exe')]
exe = next((a.split('=', 1)[1] for a in sys.argv[1:] if a.startswith('--exe=')), 'game/sh3.exe')
pe = pefile.PE(exe, fast_load=True)
base = pe.OPTIONAL_HEADER.ImageBase
text = next(s for s in pe.sections if s.Name.startswith(b'.text'))
tva = base + text.VirtualAddress
code = text.get_data()


def func_start(off):
    # walk back to the padding (0x90/0xCC run of 2+) that ends before this function
    o = off
    while o > 2:
        if code[o - 1] in (0x90, 0xCC) and code[o - 2] in (0x90, 0xCC):
            return tva + o
        if code[o - 1] == 0xC3 and code[o] in (0x55, 0x83, 0x8B, 0x56, 0x53, 0x51, 0x6A, 0x81, 0xA1, 0x57, 0x0F, 0xD9):
            # a ret right before a likely prologue (functions packed without padding)
            pass
        o -= 1
    return tva


for a in args:
    if a.startswith('data:'):
        target = int(a[5:], 16)
        pat = struct.pack('<I', target)
        o = code.find(pat)
        hits = []
        while o >= 0:
            hits.append(o)
            o = code.find(pat, o + 1)
        print(f'== data {target:#x}: {len(hits)} references')
        for o in hits:
            print(f'   at {tva + o - 2:#x} (operand at {tva + o:#x}) in function {func_start(o):#x}')
        continue
    target = int(a, 16)
    print(f'== calls to {target:#x}')
    for o in range(len(code) - 5):
        if code[o] in (0xE8, 0xE9):
            rel = struct.unpack_from('<i', code, o + 1)[0]
            if tva + o + 5 + rel == target:
                kind = 'call' if code[o] == 0xE8 else 'jmp '
                print(f'   {kind} at {tva + o:#x} in function {func_start(o):#x}')
