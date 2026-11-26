# SH3 Quest VR: the game files the launcher checks (LIST 4): every file under data/ with its size, and sh3.exe's
# SHA-1s we accept. Written to android/app/src/main/assets/game_files.txt from a PC game folder:
#   python tools/game_manifest.py C:/SH3Quest/gamedir
# Lines: "exe <sha1> <what>" and "file <relative path> <size>", then tools/exe_ref.py's base/section/skip/page lines
# (the 2003-09-03 build's data, 0.1.4). The launcher (GameFiles.java, ExeCheck.java) takes an exe whose SHA-1 is
# listed, or any exe whose data matches the build's (its code may be anything: the port runs its own recompiled
# code); it needs every data file listed, and a size that differs is reported (another edition's languages, a mod)
# but does not block (0.1.4, owners 90-91: "it keeps saying they have the wrong version installed", "try to extend
# compatibility however you can").
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import exe_ref  # noqa: E402

EXES = [
    ("d851669ef609b3164e3581aa8dcfbfaec866cb79",
     "the 2003-09-03 build, unprotected (the North American copy this port was recompiled from)"),
    ("aa2aca6dba19bf510c9c2e92eefdc15e98266581", "the 2003-09-03 build, unprotected (sh3-nx's)"),
]


def main():
    root = sys.argv[1] if len(sys.argv) > 1 else r"C:/SH3Quest/gamedir"
    here = os.path.dirname(os.path.abspath(__file__))
    out = os.path.join(here, "..", "android", "app", "src", "main", "assets", "game_files.txt")
    lines = ["# The PC game folder's files (tools/game_manifest.py). exe: sh3.exe SHA-1s accepted. file: path size."]
    for sha, what in EXES:
        lines.append("exe %s %s" % (sha, what))
    data = os.path.join(root, "data")
    total = 0
    for folder, dirs, files in os.walk(data):
        dirs.sort()
        for name in sorted(files):
            path = os.path.join(folder, name)
            rel = os.path.relpath(path, root).replace("\\", "/")
            if " " in rel:
                raise SystemExit("a space in " + rel + ": the manifest's format has none")
            size = os.path.getsize(path)
            total += size
            lines.append("file %s %d" % (rel, size))
    lines += exe_ref.manifest_lines(os.path.join(root, "sh3.exe"))
    with open(out, "w", newline="\n") as f:
        f.write("\n".join(lines) + "\n")
    files = sum(1 for line in lines if line.startswith("file "))
    pages = sum(1 for line in lines if line.startswith("page "))
    print("%d files, %.2f GB, %d data pages of sh3.exe -> %s" % (files, total / 1e9, pages, os.path.normpath(out)))


if __name__ == "__main__":
    main()
