#!/usr/bin/env python3
"""SH3 Quest VR 0.1.4 (owner 90: "someone also said it keeps saying they have the wrong version installed. then asked if
it works with the european version"; owner 91: "if anything, try to extend compatibility however you can"): what the
port needs of a player's sh3.exe, taken from the 2003-09-03 build it was recompiled from.

The port runs its own recompiled copy of the game's code, so a player's sh3.exe only has to give the game's DATA at the
same addresses: the code section may be anything (a disc's copy protection, a no-CD/no-DVD patch, a fix). This writes
  - the build's layout (image base, the data sections), the ranges a protection rewrites (the import address table and
    the import directory), and a CRC-32 of every 4 KB page of its data sections with those ranges as zeros: hashes,
    not the game's bytes;
  - its import slots by address with the Windows function each holds (the loader fills them from this list, whatever
    import table a protected exe has).
Outputs: src/runtime/exe_ref.h (the loader's log and import slots) and the "base/section/skip/page" lines that
tools/game_manifest.py puts in android/app/src/main/assets/game_files.txt (the launcher's check, ExeCheck.java); and,
beside the recompiled code (src/recomp/gen, not in git), exe_code_data.c: the switch tables the recompiled code reads
from the code section, which the loader puts back over a protected exe's encrypted code (tools/regen.sh runs this).

    python tools/exe_ref.py [path/to/sh3.exe]      (default C:/SH3Quest/gamedir/sh3.exe)
"""
import glob
import hashlib
import os
import re
import struct
import sys
import zlib

PAGE = 4096
DATA_SECTIONS = (".rdata", ".data", ".data1")


def u16(d, o):
    return struct.unpack_from("<H", d, o)[0]


def u32(d, o):
    return struct.unpack_from("<I", d, o)[0]


class Exe:
    def __init__(self, path):
        self.path = path
        self.d = open(path, "rb").read()
        d = self.d
        pe = u32(d, 0x3C)
        assert d[pe:pe + 4] == b"PE\0\0", "not a PE file"
        self.nsec = u16(d, pe + 6)
        self.timestamp = u32(d, pe + 8)
        opt = pe + 24
        self.base = u32(d, opt + 28)
        self.size_of_image = u32(d, opt + 56)
        self.dirs = [(u32(d, opt + 96 + 8 * i), u32(d, opt + 96 + 8 * i + 4)) for i in range(16)]
        self.sections = []
        at = opt + u16(d, pe + 20)
        for i in range(self.nsec):
            name = d[at:at + 8].rstrip(b"\0").decode("latin-1")
            vs, va, rs, ro = struct.unpack_from("<IIII", d, at + 8)
            self.sections.append((name, va, vs, rs, ro))
            at += 40

    def byte_range(self, rva, n):
        """The image's bytes [rva, rva + n) as the loader maps them (past a section's raw data: zeros)."""
        out = bytearray(n)
        for name, va, vs, rs, ro in self.sections:
            lo, hi = max(rva, va), min(rva + n, va + min(rs, vs))
            if lo < hi:
                out[lo - rva:hi - rva] = self.d[ro + lo - va:ro + hi - va]
        return out

    def imports(self):
        """[(slot rva, name)], the name as src/runtime/rt_core.c's bridges have it, and the import area's extent."""
        out = []
        d = self.d
        rva_dir, size_dir = self.dirs[1]
        lo, hi = rva_dir, rva_dir + size_dir

        def at(rva):
            for name, va, vs, rs, ro in self.sections:
                if va <= rva < va + max(vs, rs):
                    return ro + rva - va
            raise ValueError(hex(rva))

        desc = rva_dir
        while True:
            ilt, _, _, name_rva, iat = struct.unpack_from("<IIIII", d, at(desc))
            if name_rva == 0:
                hi = max(hi, desc + 20)
                break
            dll = d[at(name_rva):d.index(b"\0", at(name_rva))].decode()
            lo, hi = min(lo, name_rva), max(hi, name_rva + len(dll) + 1)
            table = ilt or iat
            i = 0
            while True:
                e = u32(d, at(table + 4 * i))
                if ilt:
                    lo, hi = min(lo, table), max(hi, table + 4 * i + 4)
                if e == 0:
                    break
                if e & 0x80000000:
                    name = "%s#%d" % (dll.split(".")[0], e & 0xFFFF)
                else:
                    name = d[at(e + 2):d.index(b"\0", at(e + 2))].decode()
                    lo, hi = min(lo, e), max(hi, e + 2 + len(name) + 1)
                out.append((iat + 4 * i, name))
                i += 1
            desc += 20
        return out, (lo, hi)


def reference(path):
    exe = Exe(path)
    imports, (alo, ahi) = exe.imports()
    iat_rva, iat_size = exe.dirs[12]
    skips = [(iat_rva, iat_size), (alo, ahi - alo)]
    sections, pages = [], []
    for name, va, vs, rs, ro in exe.sections:
        if name not in DATA_SECTIONS:
            continue
        n = min(vs, rs)
        sections.append((name, va, vs, n))
        for p in range(va, va + n, PAGE):
            ln = min(PAGE, va + n - p)
            b = exe.byte_range(p, ln)
            for s, sn in skips:
                lo, hi = max(p, s), min(p + ln, s + sn)
                if lo < hi:
                    b[lo - p:hi - p] = bytes(hi - lo)
            pages.append((p, ln, zlib.crc32(bytes(b)) & 0xFFFFFFFF))
    return exe, imports, skips, sections, pages


SWITCH = re.compile(r"MEM32\((\w+) \* 4 \+ 0x([0-9A-Fa-f]+)\); /\* switch: (\d+) entries")
TABLE32 = re.compile(r"MEM32\((\w+) \* 4 \+ 0x([0-9A-Fa-f]+)\)")
TABLE8 = re.compile(r"MEM8\((\w+) \+ 0x([0-9A-Fa-f]+)\)")
CMP = re.compile(r"/\* cmp (\w+), (0x[0-9A-Fa-f]+|[0-9]+)\b")  # the bound: hex or decimal


def code_tables(exe, gen):
    """The parts of the code section the recompiled code reads as data at run time: MSVC's switch tables (code
    addresses, 4 bytes an entry) and their byte index tables (case numbers; their length from the bounds check before
    them). A disc's copy protection encrypts the code section, so the loader puts the build's back (0.1.4).
    Returns {va: length} and the byte tables whose bound was not found (given 256 bytes)."""
    text = next(s for s in exe.sections if s[0] == ".text")
    lo, hi = exe.base + text[1], exe.base + text[1] + text[2]
    ranges, unbounded, unsized = {}, set(), set()

    def add(va, n):
        if lo <= va < hi:
            ranges[va] = max(ranges.get(va, 0), min(n, hi - va))

    for path in sorted(glob.glob(os.path.join(gen, "recomp_*.c"))):
        recent = []
        for line in open(path, encoding="utf-8", errors="replace"):
            m = SWITCH.search(line)
            if m:
                add(int(m.group(2), 16), 4 * int(m.group(3)))
            else:
                for m in TABLE32.finditer(line):
                    va = int(m.group(2), 16)
                    if lo <= va < hi:
                        unsized.add(va)
            for m in TABLE8.finditer(line):
                va = int(m.group(2), 16)
                if not lo <= va < hi:
                    continue
                bound = None
                for prev in reversed(recent):
                    c = CMP.search(prev)
                    if c and c.group(1) == m.group(1):
                        bound = int(c.group(2), 0) + 1
                        break
                if bound is None or bound > 0x1000:
                    unbounded.add(va)  # a copy of the read without its bounds check (code after a ret, say)
                else:
                    add(va, bound)
            recent = (recent + [line])[-12:]
    unbounded -= set(ranges)
    unsized -= set(ranges)
    for va in unbounded:
        add(va, 256)  # every value a byte index can take
    for va in unsized:
        add(va, 256)  # a table read without its switch comment: 64 entries' worth
    return ranges, unbounded, unsized


def code_data_between(exe, functions_json):
    """The rest of the data in the code section (0.1.4, run vr260: DirectInput's joystick format, which sh3.exe links
    into its code section, read through the pointer the game hands SetDataFormat): every byte outside the functions the
    disassembly found (tools/regen.sh's build/disasm/functions.json) that is not padding (int 3 or nop), and the
    "functions" it decoded from data (they call outside the program). [(va, length)]; none without the file."""
    if not os.path.isfile(functions_json):
        print("no %s: only the switch tables (run tools/regen.sh)" % os.path.normpath(functions_json))
        return []
    import json
    text = next(s for s in exe.sections if s[0] == ".text")
    lo, hi = exe.base + text[1], exe.base + text[1] + text[2]
    funcs = json.load(open(functions_json))
    covered, data = [], []
    for f in sorted(funcs, key=lambda f: int(f["start"], 16)):
        s, e = int(f["start"], 16), int(f["end"], 16)
        if f.get("section", ".text") != ".text" or not lo <= s < hi:
            continue
        if any(not lo <= int(c, 16) < hi for c in f.get("calls_to", [])):
            data.append((s, e - s))  # data decoded as code: its "calls" leave the program
            continue
        if covered and s <= covered[-1][1]:
            covered[-1][1] = max(covered[-1][1], e)
        else:
            covered.append([s, e])
    at = lo
    for s, e in covered + [[hi, hi]]:
        if s > at:
            b = exe.byte_range(at - exe.base, s - at)
            if any(x not in (0xCC, 0x90) for x in b):
                data.append((at, s - at))
        at = max(at, e)
    return data


def write_code_data(path, gen):
    """src/recomp/gen/exe_code_data.c: the build's switch tables from its code section, for the loader (rt_core.c) to
    put in place whatever the player's exe has there. Generated beside the recompiled code (not in git), from the same
    exe (tools/regen.sh runs this)."""
    exe = Exe(path)
    ranges, unbounded, unsized = code_tables(exe, gen)
    data_ranges = code_data_between(exe, os.path.join(os.path.dirname(gen), "..", "..", "build", "disasm",
                                                      "functions.json"))
    switch_count = len(ranges)
    for va, n in data_ranges:
        ranges[va] = max(ranges.get(va, 0), n)
    merged = []
    for va in sorted(ranges):
        end = va + ranges[va]
        if merged and va <= merged[-1][1]:
            merged[-1][1] = max(merged[-1][1], end)
        else:
            merged.append([va, end])
    blob = bytearray()
    w = ["/* Generated by tools/exe_ref.py from %s (SHA-1 %s): the parts of the code section the recompiled code"
         % (os.path.basename(path), hashlib.sha1(exe.d).hexdigest()),
         " * reads as data (switch tables and their byte index tables), which rt_core.c puts in place after loading a",
         " * player's exe: a disc's copy protection encrypts that section (SH3 Quest VR 0.1.4, owners 90-91). */",
         "#include <stdint.h>", "", "const uint32_t exe_code_ranges[][3] = {  /* va, length, offset in exe_code_bytes */"]
    for va, end in merged:
        w.append("    {0x%08Xu, %d, %d}," % (va, end - va, len(blob)))
        blob += exe.byte_range(va - exe.base, end - va)
    w += ["};", "const unsigned exe_code_range_count = %d;" % len(merged), "",
          "const uint8_t exe_code_bytes[%d] = {" % len(blob)]
    for i in range(0, len(blob), 24):
        w.append("    " + ",".join("%d" % b for b in blob[i:i + 24]) + ",")
    w += ["};", ""]
    out = os.path.join(gen, "exe_code_data.c")
    with open(out, "w", newline="\n") as f:
        f.write("\n".join(w))
    print("%d code-section tables and %d stretches of other data (%d ranges, %d bytes; %d byte tables without a bound "
          "found, %d tables without a size)"
          " -> %s" % (switch_count, len(data_ranges), len(merged), len(blob), len(unbounded), len(unsized),
                      os.path.normpath(out)))


def manifest_lines(path):
    """The launcher's lines (game_files.txt): base, section, skip and page, all numbers hex, addresses as RVAs."""
    exe, imports, skips, sections, pages = reference(path)
    lines = ["# The 2003-09-03 build's data (tools/exe_ref.py): base <image base> <size of image>; section <name> <rva>"
             " <virtual size> <bytes checked>; skip <rva> <size> (the import tables a protection rewrites, checked as"
             " zeros); page <rva> <length> <crc32>.",
             "base %x %x" % (exe.base, exe.size_of_image)]
    lines += ["section %s %x %x %x" % s for s in sections]
    lines += ["skip %x %x" % s for s in skips]
    lines += ["page %x %x %08x" % p for p in pages]
    return lines


def write_header(path, out):
    exe, imports, skips, sections, pages = reference(path)
    sha = hashlib.sha1(exe.d).hexdigest()
    b = exe.base
    text = next(s for s in exe.sections if s[0] == ".text")
    w = ["/* Generated by tools/exe_ref.py from the 2003-09-03 build's sh3.exe (%d bytes, SHA-1 %s): what the port"
         % (len(exe.d), sha),
         " * needs of a player's exe (SH3 Quest VR 0.1.4, owners 90-91). The port runs its own recompiled code, so the",
         " * exe gives the game's data: its import slots are filled from exe_ref_imports, whatever import table the file",
         " * has (a disc's copy protection rewrites it), and rt_core.c logs how its data pages compare with these CRCs",
         " * (the skip ranges as zeros). Hashes and Windows function names only, none of the game's bytes. */",
         "#pragma once", "#include <stdint.h>", "",
         "#define EXE_REF_BASE 0x%08Xu" % b,
         "#define EXE_REF_SIZE_OF_IMAGE 0x%08Xu" % exe.size_of_image,
         "#define EXE_REF_TIMESTAMP 0x%08Xu /* 2003-09-03 05:28:44 UTC */" % exe.timestamp,
         "#define EXE_REF_CODE_VA 0x%08Xu  /* .text: the code the port recompiled */" % (b + text[1]),
         "#define EXE_REF_CODE_SIZE 0x%08Xu" % text[2], "",
         "static const struct { uint32_t slot; const char *name; } exe_ref_imports[] = {"]
    w += ['    {0x%08Xu, "%s"},' % (b + slot, name) for slot, name in imports]
    w += ["};", "", "static const struct { const char *name; uint32_t va, vsize, checked; } exe_ref_sections[] = {"]
    w += ['    {"%s", 0x%08Xu, 0x%08Xu, 0x%08Xu},' % (n, b + va, vs, c) for n, va, vs, c in sections]
    w += ["};", "", "static const struct { uint32_t va, size; } exe_ref_skips[] = {"]
    w += ["    {0x%08Xu, 0x%Xu}," % (b + s, n) for s, n in skips]
    w += ["};", "", "static const struct { uint32_t va, len, crc; } exe_ref_pages[] = {"]
    w += ["    {0x%08Xu, 0x%Xu, 0x%08Xu}," % (b + p, ln, c) for p, ln, c in pages]
    w += ["};", ""]
    with open(out, "w", newline="\n") as f:
        f.write("\n".join(w))
    print("%s: %d imports, %d sections, %d skips, %d data pages -> %s" % (path, len(imports), len(sections), len(skips),
                                                                          len(pages), os.path.normpath(out)))


if __name__ == "__main__":
    src = sys.argv[1] if len(sys.argv) > 1 else r"C:/SH3Quest/gamedir/sh3.exe"
    here = os.path.dirname(os.path.abspath(__file__))
    write_header(src, os.path.join(here, "..", "src", "runtime", "exe_ref.h"))
    gen = os.path.join(here, "..", "src", "recomp", "gen")
    if glob.glob(os.path.join(gen, "recomp_*.c")):
        write_code_data(src, gen)
    else:
        print("src/recomp/gen has no recompiled code yet: exe_code_data.c not written (tools/regen.sh runs this again)")
