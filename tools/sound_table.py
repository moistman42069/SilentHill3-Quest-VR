#!/usr/bin/env python3
"""SH3 Quest VR: the character sound events' sound ids. 0x005680F0(chr, pos, ?, index) picks a sound id (and a volume)
by a byte table (index -> case) and a jump table (case -> code); this prints each index's code's first moves.

Usage: sound_table.py [exe]
"""
import struct, sys
from capstone import Cs, CS_ARCH_X86, CS_MODE_32

exe = sys.argv[1] if len(sys.argv) > 1 else 'game/sh3.exe'
d = open(exe, 'rb').read()
md = Cs(CS_ARCH_X86, CS_MODE_32)


def at(va, n):
    return d[va - 0x400000: va - 0x400000 + n]


def dis(va, count=6):
    out = []
    for ins in md.disasm(at(va, 64), va):
        out.append(f'{ins.mnemonic} {ins.op_str}')
        if len(out) >= count or ins.mnemonic in ('jmp', 'ret'):
            break
    return '; '.join(out)


BYTES, JUMPS, MAXI = 0x5683FC, 0x5683D8, 0x4B
seen = {}
for i in range(MAXI + 1):
    case = at(BYTES + i, 1)[0]
    target = struct.unpack('<I', at(JUMPS + 4 * case, 4))[0]
    print(f'index {i:3d} (0x{i:02X}): case {case} -> 0x{target:06X}: {dis(target)}')
