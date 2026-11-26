#!/usr/bin/env python3
"""SH3 Quest VR: every call to the game's sound starters with the sound id it passes, when it is a constant, and the
function each call sits in. The starters: 0x004A33B0(sound, volume, position, flags) (3D, cdecl), 0x0046CDF0 (3D, the
one 0x4A33B0 calls), 0x004A82B0(sound, volume, pan, pitch) (2D), plus any VA given on the command line.

Usage: sound_calls.py [exe] [extra target VAs...]
"""
import struct, sys
from capstone import Cs, CS_ARCH_X86, CS_MODE_32
import pefile

exe = sys.argv[1] if len(sys.argv) > 1 else 'game/sh3.exe'
targets = [0x4A33B0, 0x46CDF0, 0x4A82B0] + [int(a, 16) for a in sys.argv[2:]]
pe = pefile.PE(exe, fast_load=True)
base = pe.OPTIONAL_HEADER.ImageBase
text = next(s for s in pe.sections if s.Name.startswith(b'.text'))
tva = base + text.VirtualAddress
code = text.get_data()
md = Cs(CS_ARCH_X86, CS_MODE_32)


def func_start(off):
    o = off
    while o > 2:
        if code[o - 1] in (0x90, 0xCC) and code[o - 2] in (0x90, 0xCC):
            return tva + o
        o -= 1
    return tva


for t in targets:
    print(f'== calls to 0x{t:06X}')
    for off in range(len(code) - 5):
        if code[off] != 0xE8:
            continue
        rel = struct.unpack('<i', code[off + 1:off + 5])[0]
        if tva + off + 5 + rel != t:
            continue
        va = tva + off
        # the instructions from 40 bytes back up to the call (decoded from a few starts; the one ending on the call)
        best = []
        for back in range(48, 0, -1):
            ins = list(md.disasm(code[off - back:off + 5], va - back))
            if ins and ins[-1].address == va:
                best = ins
                break
        # the first argument is the last push before the call (args pushed right to left)
        pushes = [i for i in best[:-1] if i.mnemonic == 'push']
        first = pushes[-1].op_str if pushes else '?'
        movs = {}
        for i in best[:-1]:
            if i.mnemonic == 'mov' and ', 0x' in i.op_str:
                reg, val = i.op_str.split(', ', 1)
                movs[reg] = val
        sound = movs.get(first, first)
        print(f'  0x{va:06X} in 0x{func_start(off):06X}: sound {sound}   | ' +
              '; '.join(f'{i.mnemonic} {i.op_str}' for i in best[-7:-1]))
