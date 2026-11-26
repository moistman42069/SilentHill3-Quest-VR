#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""SH3 Quest VR: the Quest APK, in one pass (LIST 3, 40; after Ratchet Quest VR's tools/build_quest.py).

  1. libmain.so (the recompiled game, its runtime and VR: CMakeLists.txt's ANDROID branch) and libSDL2.so, with the
     NDK's CMake toolchain, arm64-v8a, android-32;
  2. stages them, the OpenXR loader, SDL2's Java (org.libsdl.app) and the texts in android/build/stage, which
     android/app/build.gradle takes;
  3. Gradle: the APK, then checks what is in it (libraries, the headset's manifest entries) and, for --release, that
     it is signed with our key.

The toolchain: ANDROID_NDK_HOME (NDK 27.2), ANDROID_HOME (SDK 34), JAVA_HOME (17), cmake and ninja on PATH; Ratchet's
C:/RACQuest/env.sh sets them. src/recomp/gen must be regenerated first (tools/regen.sh). Nothing is downloaded: the
OpenXR loader is taken from an unpacked openxr_loader_for_android AAR (--openxr, SH3_OPENXR_DIR, or Ratchet's copy).
"""
import argparse
import hashlib
import os
import re
import shutil
import subprocess
import sys
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
BUILD = Path(os.environ.get("SH3_QUEST_BUILD", "C:/SH3Quest/build-quest"))
STAGE = ROOT / "android/build/stage"
SDL2_SRC = Path(os.environ.get("SH3_SDL2_SRC", "C:/SH3Quest/deps/SDL2-2.32.10"))
OPENXR_CANDIDATES = [os.environ.get("SH3_OPENXR_DIR", ""), "C:/RACQuest/b/openxr"]


def run(command, cwd=None):
    print("+", " ".join(str(c) for c in command), flush=True)
    subprocess.run([str(c) for c in command], cwd=cwd, check=True)


def ndk() -> Path:
    home = os.environ.get("ANDROID_NDK_HOME")
    if not home or not Path(home).is_dir():
        sys.exit("ANDROID_NDK_HOME is not set (source C:/RACQuest/env.sh first)")
    return Path(home)


def openxr_dir(given: str) -> Path:
    for candidate in [given, *OPENXR_CANDIDATES]:
        if candidate and (Path(candidate) / "prefab/modules/openxr_loader/libs/android.arm64-v8a/libopenxr_loader.so").is_file():
            return Path(candidate)
    sys.exit("no unpacked OpenXR loader AAR (prefab/modules/openxr_loader/...): give --openxr or SH3_OPENXR_DIR")


def check_sources() -> None:
    gen = ROOT / "src/recomp/gen/recomp_dispatch.c"
    if not gen.is_file():
        sys.exit("src/recomp/gen is empty: run tools/regen.sh first")
    for wrapped in ("sub_004ACE10_gen", "sub_004B33E0_gen", "sub_004162A0_gen", "sub_004D83B0_gen", "sub_00495360_gen",
                    "sub_004D44E0_gen", "sub_005A5E40_gen"):
        if wrapped not in gen.read_text(encoding="utf8", errors="replace"):
            sys.exit(f"src/recomp/gen lacks {wrapped}: run tools/regen.sh --recomp-only after changing sh3.c")
    if not (ROOT / "src/recomp/gen/exe_code_data.c").is_file():  # 0.1.4: the code section's switch tables (rt_core.c)
        sys.exit("src/recomp/gen lacks exe_code_data.c: run tools/exe_ref.py game/sh3.exe (tools/regen.sh does)")


def build_native(openxr: Path) -> Path:
    out = BUILD / "native"
    run(["cmake", "-S", ROOT, "-B", out, "-G", "Ninja",
         f"-DCMAKE_TOOLCHAIN_FILE={(ndk() / 'build/cmake/android.toolchain.cmake').as_posix()}",
         "-DANDROID_ABI=arm64-v8a", "-DANDROID_PLATFORM=android-32", "-DCMAKE_BUILD_TYPE=RelWithDebInfo",
         f"-DCMAKE_MAKE_PROGRAM={shutil.which('ninja')}", f"-DSH3_SDL2_SRC={SDL2_SRC.as_posix()}",
         f"-DSH3_OPENXR_DIR={openxr.as_posix()}", "-Wno-dev"])
    run(["cmake", "--build", out, "--target", "sh3", "SDL2"])
    return out


def strip_tool() -> Path:
    found = sorted(ndk().glob("toolchains/llvm/prebuilt/*/bin/llvm-strip*"))
    if not found:
        sys.exit("the NDK has no llvm-strip")
    return found[0]


def stage_libraries(native: Path, openxr: Path) -> None:
    dest = STAGE / "jniLibs/arm64-v8a"
    shutil.rmtree(dest, ignore_errors=True)
    dest.mkdir(parents=True)
    main = native / "libmain.so"  # CMakeLists: OUTPUT_NAME main (SDLActivity loads libmain.so)
    sdl = sorted(native.rglob("libSDL2.so"))
    if not main.is_file() or not sdl:
        sys.exit("the native build has no libmain.so or libSDL2.so")
    # The debug-info copy stays in the build folder for the crash reports' addresses (host.c prints build-id + offset).
    shutil.copy2(main, dest / "libmain.so")
    shutil.copy2(sdl[0], dest / "libSDL2.so")
    shutil.copy2(openxr / "prefab/modules/openxr_loader/libs/android.arm64-v8a/libopenxr_loader.so",
                 dest / "libopenxr_loader.so")
    for lib in dest.iterdir():
        run([strip_tool(), "--strip-debug", lib])


def stage_java() -> None:
    java = STAGE / "java"
    shutil.rmtree(java, ignore_errors=True)
    sdl_java = SDL2_SRC / "android-project/app/src/main/java/org/libsdl"
    if not sdl_java.is_dir():
        sys.exit(f"no SDL2 Java sources in {sdl_java}")
    shutil.copytree(sdl_java, java / "org/libsdl")


def stage_texts() -> None:
    assets = STAGE / "assets"
    assets.mkdir(parents=True, exist_ok=True)
    shutil.copy2(ROOT / "NOTICE", assets / "third_party_notices.txt")


def build_apk(release: bool) -> Path:
    gradle = ROOT / ("android/gradlew.bat" if os.name == "nt" else "android/gradlew")
    task = "assembleRelease" if release else "assembleDebug"
    run([gradle, "--no-daemon", "--offline", task], cwd=ROOT / "android")  # nothing downloaded: Ratchet's cache (env.sh)
    kind = "release" if release else "debug"
    apk = ROOT / f"android/app/build/outputs/apk/{kind}/app-{kind}.apk"
    version = re.search(r'versionName "([^"]+)"', (ROOT / "android/app/build.gradle").read_text(encoding="utf8")).group(1)
    out = ROOT / f"android/build/SH3QuestVR-{version}.apk"
    shutil.copy2(apk, out)
    digest = hashlib.sha256(out.read_bytes()).hexdigest()
    print(f"{out}: {out.stat().st_size / 1e6:.0f} MB, SHA-256 {digest}", flush=True)
    return out


def verify_apk(apk: Path) -> None:
    with zipfile.ZipFile(apk) as z:
        names = set(z.namelist())
    wanted = [f"lib/arm64-v8a/lib{n}.so" for n in ("main", "SDL2", "openxr_loader")]
    wanted += [f"assets/{n}" for n in ("game_files.txt", "controls.txt", "credits.txt", "whats_new.txt",
                                       "third_party_notices.txt")]
    wanted += ["classes.dex"]
    missing = [n for n in wanted if n not in names]
    if missing:
        sys.exit(f"{apk.name} lacks: {missing}")
    build_tools = Path(os.environ["ANDROID_HOME"]) / "build-tools/34.0.0"
    aapt = next(iter(sorted(build_tools.glob("aapt2*"))))
    tree = subprocess.run([aapt, "dump", "xmltree", apk, "--file", "AndroidManifest.xml"],
                          capture_output=True, text=True).stdout
    for needle in ("com.oculus.intent.category.VR", "org.khronos.openxr.intent.category.IMMERSIVE_HMD",
                   "com.oculus.vr.focusaware", "android.hardware.vr.headtracking",
                   "org.khronos.openxr.permission.OPENXR", ".QuestGameActivity", ".ImportService", "extractNativeLibs"):
        if needle not in tree:
            sys.exit(f"{apk.name}: the manifest lacks {needle}")
    print(f"verified {apk.name}: {len(wanted)} files, manifest checked", flush=True)


def verify_signer(apk: Path) -> None:
    cert = ROOT / "tools/release-cert.sha256"
    build_tools = Path(os.environ["ANDROID_HOME"]) / "build-tools/34.0.0"
    signer = next(iter(sorted(build_tools.glob("apksigner*"))))
    out = subprocess.run([signer, "verify", "--print-certs", apk], capture_output=True, text=True)
    found = re.search(r"certificate SHA-256 digest: ([0-9a-f]{64})", out.stdout)
    if not cert.is_file():
        print(f"signed by {found.group(1) if found else '?'}; tools/release-cert.sha256 not written yet: record it",
              flush=True)
        return
    expected = cert.read_text(encoding="utf8").strip()
    if out.returncode != 0 or not found or found.group(1) != expected:
        sys.exit(f"{apk.name} is not signed with the release key (SH3QUEST_SIGNING names its properties file)")
    print(f"signed with the release key {expected[:16]}...", flush=True)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--openxr", default="", help="an unpacked openxr_loader_for_android AAR")
    parser.add_argument("--no-native", action="store_true", help="stage the last native build")
    parser.add_argument("--no-apk", action="store_true", help="stop before Gradle")
    parser.add_argument("--release", action="store_true")
    args = parser.parse_args()
    check_sources()
    openxr = openxr_dir(args.openxr)
    native = BUILD / "native" if args.no_native else build_native(openxr)
    stage_libraries(native, openxr)
    stage_java()
    stage_texts()
    if not args.no_apk:
        apk = build_apk(args.release)
        verify_apk(apk)
        if args.release:
            verify_signer(apk)
    return 0


if __name__ == "__main__":
    sys.exit(main())
