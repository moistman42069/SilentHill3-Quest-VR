"""Checks a release APK against the last release (tools/apkcheck.py old.apk new.apk; ANDROID_HOME and ANDROID_NDK_HOME set,
as /c/RACQuest/env.sh does): see the comment below."""
import hashlib
import os
import re
import subprocess
import sys
import tempfile
import zipfile
from pathlib import Path

# apkcheck.py old.apk new.apk: the new release APK against the last one (session 16's checks): the file list, the
# manifest (only the version may differ), SDL2 and the OpenXR loader byte-identical, libmain's needed libraries the
# same, no text relocations, SDL_main exported, the same signing certificate.
old, new = Path(sys.argv[1]), Path(sys.argv[2])
home = Path(os.environ["ANDROID_HOME"]) / "build-tools/34.0.0"
aapt = next(iter(sorted(home.glob("aapt2*"))))
signer = next(iter(sorted(home.glob("apksigner*"))))
ndk = Path(os.environ["ANDROID_NDK_HOME"])
readelf = next(iter(sorted(ndk.glob("toolchains/llvm/prebuilt/*/bin/llvm-readelf*"))))
ok = True


def names(apk):
    with zipfile.ZipFile(apk) as z:
        return {i.filename: i for i in z.infolist()}


def sha(apk, name):
    with zipfile.ZipFile(apk) as z:
        return hashlib.sha256(z.read(name)).hexdigest()


a, b = names(old), names(new)
print(f"files: old {len(a)}, new {len(b)}; only in old {sorted(set(a) - set(b))}; only in new {sorted(set(b) - set(a))}")
changed = sorted(n for n in set(a) & set(b) if sha(old, n) != sha(new, n))
print(f"changed: {changed}")
for lib in ("lib/arm64-v8a/libSDL2.so", "lib/arm64-v8a/libopenxr_loader.so"):
    same = sha(old, lib) == sha(new, lib)
    ok &= same
    print(f"{lib}: {'byte-identical' if same else 'DIFFERENT'}")


def manifest(apk):
    return subprocess.run([aapt, "dump", "xmltree", apk, "--file", "AndroidManifest.xml"], capture_output=True,
                          text=True).stdout.splitlines()


ma, mb = manifest(old), manifest(new)
diff = [(x, y) for x, y in zip(ma, mb) if x != y]
print(f"manifest: {len(ma)} and {len(mb)} lines; differing lines:")
for x, y in diff:
    print(f"   old: {x.strip()}\n   new: {y.strip()}")
    if "version" not in x.lower():
        ok = False
if len(ma) != len(mb):
    ok = False

with tempfile.TemporaryDirectory() as t:
    for apk, tag in ((old, "old"), (new, "new")):
        with zipfile.ZipFile(apk) as z:
            p = Path(t) / f"{tag}-libmain.so"
            p.write_bytes(z.read("lib/arm64-v8a/libmain.so"))
    need = {}
    for tag in ("old", "new"):
        p = Path(t) / f"{tag}-libmain.so"
        dyn = subprocess.run([readelf, "-d", p], capture_output=True, text=True).stdout
        need[tag] = re.findall(r"\(NEEDED\)\s+Shared library: \[([^\]]+)\]", dyn)
        textrel = "TEXTREL" in dyn
        syms = subprocess.run([readelf, "--dyn-syms", "-W", p], capture_output=True, text=True).stdout
        sdl_main = any(line.split()[-1] == "SDL_main" and " UND " not in line for line in syms.splitlines()
                       if line.strip().endswith("SDL_main"))
        print(f"{tag} libmain.so: {p.stat().st_size} bytes, needs {need[tag]}, text relocations {textrel}, "
              f"SDL_main exported {sdl_main}")
        if tag == "new":
            ok &= not textrel and sdl_main
    ok &= need["old"] == need["new"]

for apk in (old, new):
    out = subprocess.run([signer, "verify", "--print-certs", apk], capture_output=True, text=True)
    cert = re.search(r"certificate SHA-256 digest: ([0-9a-f]{64})", out.stdout)
    print(f"{apk.name}: verify {'OK' if out.returncode == 0 else 'FAILED'}, certificate {cert.group(1) if cert else '?'}")
    if apk == new:
        ok &= out.returncode == 0 and cert is not None and \
            cert.group(1) == (Path("C:/SH3Quest/SH3QuestVR/tools/release-cert.sha256").read_text().strip())
data = new.read_bytes()
print(f"{new.name}: {len(data)} bytes, SHA-256 {hashlib.sha256(data).hexdigest()}")
print("ALL CHECKS PASS" if ok else "A CHECK FAILED")
