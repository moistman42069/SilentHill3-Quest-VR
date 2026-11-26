# SH3 Quest VR: session 19 handoff (2026-11-26)

Started from the task "SH3 Quest VR: make Smoother frames (AppSW) work". It asked for:
- XR_FB_space_warp working, with motion-vector and depth swapchains;
- the option back in the VR menu and the launcher, on by default once shown working;
- short rig checks, about 30 minutes;
- the docs.

Owner messages 90-93 came in during the session (C:/SH3Quest/notes/owner-messages-sessions1-7.txt):
- 90: players see "wrong version"; does the European version work? Say which copies work.
- 91: "if anything, try to extend compatibility however you can."
- 92: "be sure to update the git entirely as you see fit after this release."
- 93: the existing Android port already takes other exes. "be sure we did good there ... make sure this build is
  compatible and works in all regards there."

docs/AUDIT-2026-11-25.md rows 112-117 trace each item.

PC builds 210-212 (C:/SH3Quest/build_pc_212.log: BUILD OK, no warnings). Runs vr260-vr263. The release APK came from
tools/build_quest.py --release (tools/build_quest_9.log, then build_quest_10.log for two texts).

## Smoother frames (AppSW)

Each frame the game draws both eyes' motion and depth into XR_FB_space_warp's swapchains (sh3vr.cpp motion_frame,
kMotionVs/kMotionFs, motion_draw):
- **The motion** is the world's against the room. The previous position is the last frame's camera moved by the
  tracked head delta: X = D · V_prev · V_cur^-1, and clip · X · clip^-1 with clip = eye_corr_m[e] · pg. The head's own
  movement stays the compositor's to show.
- **The value** is current NDC − previous NDC, with z = 2 · (before.z − d).
- **The depth** is reversed (1 − the game's D3D depth). depth_zero is the far plane and depth_one the near, in metres
  from depth_abcd. The layer gets nearZ = far and farZ = near (xr_openxr.cpp).
- **First person**: nothing within 1 m of the eyes gets motion (her hands, the weapon).
- **Cuts**: a jump over 2 m, a turn over 25 degrees, or a mode change is marked as a cut (the skip flag).
- **Frames with no world** (a screen, a load, a movie) keep the last motion, marked to skip, once motion has been
  given.

The option and pacing:
- "Smoother frames (AppSW)" is in the VR menu (kRows, Performance) and in the launcher (Options.java, Performance).
- It is on by default: opt.space_warp and Options.SPACE_WARP_ON.
- Options files from before 0.1.4 get it on once. The defaults version is now 4: load_options in the game,
  Config.newDefaults in the launcher. A version-3 file keeps its 0.1.3 choices.
- xr_openxr.cpp counts missed displays and the frame budget over the display's own period (1 / refresh). The runtime
  halves predictedDisplayPeriod under AppSW, as Jak 0.1.6's headset logs show.

Checked on the rig:
- **vr261** (build 211) and **vr262** (build 212, the release's code), with tools/sim/appsw.txt:

  | Shot | Motion |
  |---|---|
  | Still | 0 |
  | Walk | mean ≈ 0, 57% of the picture moving, the most 0.028 NDC a frame |
  | Turn right | mean across −0.037 (the world going left), 97% moving |
  | Head alone | 0 |

- Motion on 300 of 300 frames in play.
- vr262's log: "sh3vr.ini from before 0.1.4 (defaults 3): Smoother frames (AppSW) on once"; the file was rewritten
  with defaults = 4 and space_warp = 1.
- The rig cannot run the OpenXR submit (the sim headset has no XR_FB_space_warp). That path is the one Jak's headset
  builds use; HEADSET_CHECKLIST has the items to look for.

## Which copies of the game work (owners 90, 91, 93)

The old check compared the exe's SHA-1 with two values. It refused every exe but the unprotected one, which is where
"wrong version" came from. The port never runs the exe's code: it uses its own recompiled copy. So now:

- **The launcher** (ExeCheck.java, GameFiles.java):
  - It compares the exe's data with the build's: 163 pages of 4 KB, from game_files.txt, which
    tools/game_manifest.py writes from tools/exe_ref.py.
  - The import tables are left out of the comparison: a disc's protection rewrites them.
  - Up to 16 changed pages still pass.
  - A data file of another size is only noted; only a missing one counts.
  - sh3.exe is found whatever its case.
  - A game copied one or two folders too deep is moved up (settle, moveInto: folders merged, no file replaced).
  - "Try it anyway" (Play in VR anyway / Play flat anyway) is offered whenever there is an exe.
  - WHICH_GAME spells out the copies that work.
- **The loader** (rt_core.c load_exe, exe_data_check, exe_code_tables):
  - It logs the PE timestamp, the sections and the data check.
  - It puts the build's code-section data in place when the exe's differs: src/recomp/gen/exe_code_data.c, which
    tools/exe_ref.py writes (2977 stretches, 332,804 bytes). Those are switch tables, byte tables, and the bytes outside
    the functions, among them DirectInput's c_dfDIJoystick at 0x6254B0/0x625520. vr260 crashed in the pad's
    SetDataFormat on a protected-like exe without it.
  - It fills all 242 import slots from the build's list (src/runtime/exe_ref.h), whatever the exe's own table says.
  - regen.sh runs exe_ref.py, and build_quest.py refuses to build without exe_code_data.c.
- **SH3AndroidEvolved and sh3-nx** load any sh3.exe from its own import table, with no check. This port takes the same
  range and more: a wiped import table, and encrypted data in the code section.
- **Checked**:
  - ExeCheckTest (the scratchpad's jtest) against C:/SH3Quest/exevariants v1-v7:

    | Variant | Result |
    |---|---|
    | Code patched | accepted |
    | Protected-like | accepted |
    | 3 pages changed | accepted |
    | 40 pages changed | refused |
    | Another base address | refused |
    | Not a program | refused |
    | Unchanged | accepted |

    The settle test also passed.
  - vr261 ran the protected-like exe from C:/SH3Quest/gamedir_v2 (whose data folder is a junction to the installed
    game's). Its log says:
    - "all 163 pages the same";
    - "2977 of the 2977 stretches ... differ ... the build's are used";
    - "its own import table has 0 of the build's 242 functions ... filled from the build's own list".

    It played and quit cleanly.
  - vr262 ran the unprotected exe. Its log says:
    - "the data in its code section ... is the build's";
    - "its import table is the build's (242 functions)".
- **Not tried: a real European or Asian exe.** None is at hand. As far as known, all three editions carry the same
  2003-09-03 build. The notes say so and ask for reports.

## Docs

- docs/RELEASE-NOTES-0.1.4.md (new; its "Which copies of Silent Hill 3 work" table).
- README: v0.1.4, a "Which copies of Silent Hill 3 work" section, the Smoother frames feature, and what regen needs.
- docs/RELEASE-NOTES-0.1.3.md: its Files section named 0.1.2's APK and hash, now 0.1.3's. The v0.1.3 release page's
  text is corrected the same way; its APK and tag are untouched.
- CREDITS, NOTICE and credits.txt: SH3AndroidEvolved's lead on taking any exe.
- GAME_FACTS: "The exe itself".
- HEADSET_CHECKLIST: two 0.1.4 items.
- The audit rows.
- Launcher assets: whats_new.txt and features.txt.

## Release 0.1.4

- **The APK**:
  - tools/build_quest.py --release (build_quest_9: every native file compiled from this code, the same sources as PC
    build 212).
  - Then --release --no-native (build_quest_10), after What's new and Features were hedged ("as far as we know" the
    same build in every edition). Its libmain.so is byte-identical to build_quest_9's.
  - Result: android/build/SH3QuestVR-0.1.4.apk, versionCode 5, "0.1.4", 9,381,206 bytes, SHA-256
    9d0fee94ccfb5f635149a59238069fc534ab7dc899b7017d43714e6b4edbf70d, the release key (certificate 38811d96...).
  - The first pass is kept at C:/SH3Quest/SH3QuestVR-0.1.4-pass1.apk (9,381,190 bytes, fe667985...).
- **tools/apkcheck.py against 0.1.3**: ALL CHECKS PASS.
  - The same 17 files.
  - Changed: AndroidManifest.xml (only the versionCode/versionName lines), assets credits.txt, features.txt,
    game_files.txt, third_party_notices.txt (= NOTICE), whats_new.txt, classes.dex and libmain.so.
  - libmain.so is 30,147,432 bytes, with the same NEEDED, no text relocations and SDL_main exported.
  - libSDL2 and the OpenXR loader are byte-identical.
  - Both APKs verify with the same certificate.
  - libmain holds "smoother frames: motion given", "stretches of data in its code section", "sh3vr.ini from before
    0.1.4", "filled from the build's own list" and "Smoother frames (AppSW)".
  - classes.dex holds "Try it anyway", "Play in VR anyway", "moved up" and "Smoother frames (AppSW)".
  - The APK's assets are byte-identical to the commit's.
- **Verified on this code before publishing** (build 212 = the APK's sources):
  - vr262: the unprotected exe with a 0.1.3 options file (folded once), the motion, the pictures, a clean quit;
  - vr263: the protected-like exe, the same;
  - ExeCheckTest on ExeCheck.java and GameFiles.java as committed.
- **0.1.4 PUBLISHED** (2026-11-26; owner 92: "be sure to update the git entirely as you see fit after this release"):
  - main 50047a1 (the 0.1.4 commit), annotated tag v0.1.4 (8f9e90c) on it, pushed;
  - the release "Silent Hill 3 Quest VR 0.1.4", with docs/RELEASE-NOTES-0.1.4.md and the APK: not a draft, not a
    prerelease;
  - releases/latest = v0.1.4, and GitHub's digest sha256:9d0fee94... matches, 9,381,206 bytes;
  - the updater asks for "SH3QuestVR-" + "0.1.4" + ".apk", which is the asset's name;
  - v0.1.0 to v0.1.3 untouched, except the v0.1.3 page's Files text: it named 0.1.2's APK, and now names its own,
    9,214,086 bytes, 87e8d315... Its APK and tag are unchanged.

## Open, in order

1. Reports on 0.1.4 from players and the owner (GitHub issues; the launcher's report). Look for:
   - a European or Asian exe: sh3.log's first lines;
   - Smoother frames on the headset: "[vr-perf] ... smoother frames on N%", and any wobble or smear at edges.
2. The headset-only items: docs/HEADSET_CHECKLIST.md (0.1.4's two added).
3. LIST 45's floor items by hand, as session 17 left them (the mall, a save or the headset).
