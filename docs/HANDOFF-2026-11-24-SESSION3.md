# Silent Hill 3 Quest VR: handoff, 2026-11-24 (session 3)

Read docs/HANDOFF-2026-11-24.md (session 1: layout, toolchains, design) and docs/HANDOFF-2026-11-24-SESSION2.md, then
docs/LIST-0.1.0.md: its last section, "The PC test build", says what the PC rig showed item by item.

**Then read docs/HANDOFF-2026-11-24-SESSION3-PART2.md: the owner's later asks (items 41-45), the design for each and
where to start. It supersedes "Still to do" below for items 19-23 and 41-45.**

## Owner's rules in force
- The owner approved PC test builds ("Yes, PC build"). No APK build until the owner says (item 40). Never two builds
  at once; never build while sh3.exe runs (the exe is locked). No commit, push or publish unless asked.
- No `python -` heredocs in Git Bash: scripts go in files (Write tool), run with C:/SH3Quest/venv/Scripts/python.exe.
  Edit files with the Edit tool or plain sed (sed cannot match C strings holding `\n`: use Edit there).
- Short targeted checks only (about 30 min at most). Crash logs explain themselves. Plain option names; VR features on
  by default. APKs: path and SHA-256, the owner emails them; no adb steps.
- Do not raise the license question again (sh3-nx, the PC VR mod, the Android port: built on directly, credited).

## How to build and run on the PC
- Build: `cd /c/SH3Quest/SH3QuestVR && cmd //c "tools\\build_pc.bat RelWithDebInfo" > /c/SH3Quest/build_pc_N.log 2>&1`
  (clang/clang++, Ninja, MSVC libraries; build/pc/sh3.exe). Last build: build_pc_19 (OK; the "parts:" logging).
- Regen (only when a new wrapper is added to src/game/sh3.c): `PY=/c/SH3Quest/venv/Scripts/python.exe sh tools/regen.sh
  --recomp-only` (about 1.5 min), plus the address in recomp_lookup_manual.
- Run from C:\SH3Quest\gamedir, scripted New Game:
  `SH3_KEYS="22:Return,25:Return,28:Return,31:Return,34:Return" timeout N /c/SH3Quest/SH3QuestVR/build/pc/sh3.exe C:/SH3Quest/gamedir > log 2>&1`
  - Flat: `vr = 0` in gamedir/sh3vr.ini. VR: no sh3vr.ini, plus `QUEST_SIM_SCRIPT=<tools/sim/x.txt> QUEST_SIM_OUT=<dir>
    QUEST_SIM_HEADSET=1` and `QUEST_SIM_GRIP_TILT=50` (the grip a real controller has; 0 points our forearms up).
  - Play starts about frame 7315 of the opening. tools/sim/arms.txt (2.5 min) checks the arms; long.txt (4 min) the
    whole opening; capture.txt with SH3_VSC_DUMP=8000 the bones.
  - Switches: SH3_STATS=1, SH3_SHOT=dir (+ SH3_SHOT_EVERY=N), SH3_NO_S3TC=1, SH3_VSC_DUMP=N (frame N: every shader
    draw's constants, each c48+ upload's caller, every character model's bones, each arm's solve).
  - Tools: tools/ppm2png.py in out; tools/bones_from_dump.py, skeleton_from_dump.py, heather_bones.py; tools/arm_clip.py
    (her bones through the game's camera: screen place and depth).

## Done this session
- PC build fixes: clang++ for C++ (build_pc.bat), opengl32 on the link (CMakeLists.txt).
- 2, 3: flat and the Quest's texture path on the PC: right pictures, 55-60 fps, guest heap 202 MB.
- 5: the movie's COM request logged in full (CLSID_FilterGraph for IID_IGraphBuilder): GAME_FACTS.md, Movies.
- 7-10: VR through the simulated headset: title on the panel, scenes at the game's camera, first person in play,
  stick walking, head turning, knife at the hand, our hands. No crash in about ten runs.
- 13/35 fixed: first person waits for half a second of play after a scene (the opening's 15-frame gaps no longer
  flick the view); each view change is logged with its reasons.
- 30 fixed: the flashlight rewrite only for shaders that read c80-c82 (d3d8_shader.c marks them "lit").
- 19 done: Heather's arms reach the controllers on her own 73 bones (src/vr/sh3vr_ik.cpp, the 0x45EC00 wrapper in
  sh3.c, option "Heather's arms follow your hands", on); the first-person cut is now a 17 cm sphere about her head
  bone plus a 12 cm flat cut (her arms were inside the old 25 cm cut); our forearm stand-in is drawn only when her
  arms are not following. Checked: wrists 0 units off, bone lengths kept, pictures at rest, reaching, apart, raised.
- Scale: kUnitsPerMetre 360 -> 490 (her own measurements; GAME_FACTS.md, Camera and Bones).
- GAME_FACTS.md: a Bones section (the bone pipeline, the model header, Heather's parent table and arm chains).

## Seen on the PC rig, not yet changed
- Our hands are crude boxes (palm box and finger segments): item 20/21's look needs a real hand mesh.
- The game's blur / depth of field is applied in VR.
- Looking straight down, grey bands of her body cut by the 12 cm flat cut show near her feet.
- The sim does not pace frames (about 88 fps in play).

## Still to do on the list
- 5 Movies: a stand-in FilterGraph / IGraphBuilder, the .000 files' decryption, FFmpeg, the big screen in VR.
- 6 Speed: uniform uploads by change only; measure on the headset.
- 12 Hit-testing for the inventory ring; tremor smoothing.
- 22 Fingers wrapping a world object's own shape.
- 23 The radio's static as haptics.
- 24 The extras' mesh signatures (needs the extras drawn: a save with them).
- 34 Sound following the head (positional).
- 29, 31, 38: the playthrough. 40: the APK, when the owner says.

## Files left on C: (the owner's go before deleting)
- C:\SH3Quest\shots\vr3 (172 MB), vr11 (58 MB), vr7 (23 MB), run2 (17 MB), run6, vr1, run7; logs C:\SH3Quest\vr*.log,
  run*.log, build_pc_*.log (under 1 MB each).
