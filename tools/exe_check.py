#!/usr/bin/env python3
"""Checks an sh3.exe against what sh3-nx expects: PE header, sections, and the original bytes at every
patch site sh3-nx's tools/patch_exe.py touches.

Usage: exe_check.py path/to/sh3.exe path/to/sh3-nx
"""
import hashlib, importlib.util, os, sys, time
import pefile

exe, nx = sys.argv[1], sys.argv[2]
data = open(exe, 'rb').read()
print('size', len(data), 'sha1', hashlib.sha1(data).hexdigest())
pe = pefile.PE(data=data)
fh, oh = pe.FILE_HEADER, pe.OPTIONAL_HEADER
print('timestamp', hex(fh.TimeDateStamp), time.strftime('%Y-%m-%d %H:%M:%S', time.gmtime(fh.TimeDateStamp)))
print('entry', hex(oh.ImageBase + oh.AddressOfEntryPoint), 'base', hex(oh.ImageBase), 'checksum', hex(oh.CheckSum),
      'linker', oh.MajorLinkerVersion, oh.MinorLinkerVersion)
for s in pe.sections:
    print('section', s.Name.rstrip(b'\0').decode(errors='replace'), 'va', hex(oh.ImageBase + s.VirtualAddress),
          'vsize', hex(s.Misc_VirtualSize), 'raw', hex(s.PointerToRawData), 'rawsize', hex(s.SizeOfRawData),
          'chars', hex(s.Characteristics))
for e in pe.DIRECTORY_ENTRY_IMPORT:
    print('import', e.dll.decode(), len(e.imports))

spec = importlib.util.spec_from_file_location('patch_exe', os.path.join(nx, 'tools', 'patch_exe.py'))
m = importlib.util.module_from_spec(spec)
spec.loader.exec_module(m)
bad = 0
for va, old, new, what in m.PATCHES:
    o = pe.get_offset_from_rva(va - oh.ImageBase)
    got = data[o:o + len(old)]
    ok = got == old
    bad += not ok
    print('patch site', hex(va), 'OK' if ok else f'DIFFERS: expected {old.hex()} found {got.hex()}', what)
for name, va in (('_setjmp3', 0x662954), ('longjmp', 0x6628D8)):
    o = pe.get_offset_from_rva(va - oh.ImageBase)
    print(name, hex(va), data[o:o + 16].hex())
print('patch sites differing:', bad)
