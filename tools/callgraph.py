#!/usr/bin/env python3
"""Which game functions reach a target through direct calls in the recompiled game (src/recomp/gen, made by
tools/regen.sh from your own sh3.exe): RECOMP_ABI_CALL / RECOMP_ICALL_SAFE(0x...) calls and tail jumps; indirect calls
(jump tables, function pointers) are counted, not followed. Session 33 (owner 159, "i dont want anything similar
happening anywhere else"): every game function SH3 Quest VR calls, against every function that writes the inventory.

Usage: python -I tools/callgraph.py src/recomp/gen TARGET ROOT [ROOT...]   (addresses in hex, without 0x)
"""
import os
import re
import sys
from collections import deque

gen_dir, target, roots = sys.argv[1], int(sys.argv[2], 16), [int(r, 16) for r in sys.argv[3:]]
head = re.compile(r"^void sub_([0-9A-F]{8})(?:_gen)?\(void\)")
call = re.compile(r"(?:RECOMP_ABI_CALL|RECOMP_ICALL_SAFE(?:_AT)?)\(0x([0-9A-Fa-f]+)u|sub_([0-9A-F]{8})\(\); return; /\* tail")
indirect = re.compile(r"RECOMP_ITAIL\(|RECOMP_ICALL_SAFE\((?!0x)")
calls, indirects = {}, {}
for name in sorted(os.listdir(gen_dir)):
    if not name.endswith(".c") or name == "recomp_dispatch.c":
        continue
    cur = None
    with open(os.path.join(gen_dir, name), encoding="utf-8", errors="replace") as f:
        for line in f:
            m = head.match(line)
            if m:
                cur = int(m.group(1), 16)
                calls.setdefault(cur, set())
                indirects.setdefault(cur, 0)
                continue
            if cur is None:
                continue
            for a, b in call.findall(line):
                calls[cur].add(int(a or b, 16))
            if indirect.search(line):
                indirects[cur] += 1
print("functions read: %d" % len(calls))
for root in roots:
    prev = {root: None}
    q = deque([root])
    found = False
    reached = 0
    ind = 0
    while q:
        fn = q.popleft()
        reached += 1
        ind += indirects.get(fn, 0)
        if fn == target:
            found = True
            break
        for c in calls.get(fn, ()):
            if c not in prev:
                prev[c] = fn
                q.append(c)
    if found:
        path = []
        n = target
        while n is not None:
            path.append("%06X" % n)
            n = prev[n]
        print("%06X: REACHES %06X: %s" % (root, target, " <- ".join(path)))
    else:
        print("%06X: does not reach %06X (%d functions under it, %d indirect calls among them)" % (root, target, reached, ind))
