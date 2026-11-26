# SH3 Quest VR: the game files the launcher checks (LIST 4): every file under data/ with its size, and sh3.exe's
# SHA-1s we accept. Written to android/app/src/main/assets/game_files.txt from a PC game folder:
#   python tools/game_manifest.py C:/SH3Quest/gamedir
# Lines: "exe <sha1> <what>" and "file <relative path> <size>". The launcher (GameFiles.java) needs every file
# listed; a size that differs is reported (another language's release) but does not block.
import os
import sys

EXES = [
    ("d851669ef609b3164e3581aa8dcfbfaec866cb79", "the 2003-09-03 PC release (the owner's, recompiled for this port)"),
    ("aa2aca6dba19bf510c9c2e92eefdc15e98266581", "the 2003-09-03 PC release (sh3-nx's)"),
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
    with open(out, "w", newline="\n") as f:
        f.write("\n".join(lines) + "\n")
    print("%d files, %.2f GB -> %s" % (len(lines) - 1 - len(EXES), total / 1e9, os.path.normpath(out)))


if __name__ == "__main__":
    main()
