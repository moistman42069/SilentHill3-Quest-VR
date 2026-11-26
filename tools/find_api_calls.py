#!/usr/bin/env python3
"""Lists every call to the named imports and disassembles the code around each one.

Usage: find_api_calls.py sh3.exe GetDriveTypeA GetVolumeInformationA ...
"""
import struct, sys
import capstone, pefile

exe, names = sys.argv[1], set(sys.argv[2:])
data = open(exe, 'rb').read()
pe = pefile.PE(data=data)
base = pe.OPTIONAL_HEADER.ImageBase
iat = {}
for e in pe.DIRECTORY_ENTRY_IMPORT:
    for i in e.imports:
        if i.name and i.name.decode() in names:
            iat[i.address] = i.name.decode()
text = next(s for s in pe.sections if s.Name.startswith(b'.text'))
tva, traw, tsz = base + text.VirtualAddress, text.PointerToRawData, text.SizeOfRawData
code = data[traw:traw + tsz]
md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
for addr, name in iat.items():
    pat = b'\xff\x15' + struct.pack('<I', addr)
    o = code.find(pat)
    while o >= 0:
        va = tva + o
        print(f'== {name} called at {va:#x}')
        start = max(0, o - 0x60)
        for ins in md.disasm(code[start:o + 0x80], tva + start):
            mark = '>>' if ins.address == va else '  '
            print(f'{mark} {ins.address:#x}: {ins.bytes.hex():<16} {ins.mnemonic} {ins.op_str}')
        o = code.find(pat, o + 1)
