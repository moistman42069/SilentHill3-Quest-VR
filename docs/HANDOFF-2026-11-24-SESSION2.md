# Silent Hill 3 Quest VR: handoff, 2026-11-24 (session 2)

Read docs/HANDOFF-2026-11-24.md (session 1: layout, toolchains, design), then docs/LIST-0.1.0.md: its new last section
says where each of the 40 items stands.

## Owner's rules in force
- No exe or APK builds until every list item is done; then one build pass. Running existing tools (regen, disx,
  xrefs, peek) is fine. No commit, push or publish unless asked. Never two builds at once.
- No `python -` heredocs in Git Bash: scripts go in files (Write tool), run with the venv's python.
- Short targeted checks only (about 30 min at most). Crash logs explain themselves. Plain option names; VR features on
  by default. APKs: path and SHA-256, the owner emails them; no adb steps.
- Do not raise the license question again (sh3-nx, the PC VR mod, the Android port: built on directly, credited).

## Done this session (none of it compiled)
- src/vr/: the VR layer split into sh3vr.cpp (options, frame, view, panel, pad, movement, C API), sh3vr_body.cpp
  (hands, depth probes, ghost hands, head fade, vignette, guide dot), sh3vr_combat.cpp (weapons, holsters, melee,
  punches, clubs, two hands, flashlight, grabbing, haptics), sh3vr_menu.cpp (the VR menu), sh3vr_internal.h.
- src/game/sh3.c: weapon state for VR (drawn weapon, equipped slot, weapons owned), holster equips (vr_equip:
  0x497A70 then 0x4711D0 between frames, in play only), melee volume replacement (wrapper 0x4ACE10 with an injecting
  guard, on a copy at 0x0EC00200), VR's own hits queued at the battle step (wrapper 0x4B33E0, from 0x0EC00280,
  dropped when the queue holds 48 or more), shot haptics (0x4B3D50). regen --recomp-only rerun: both _gen bodies exist.
- Checked in the exe and written into GAME_FACTS.md: the weapon numbers (the PC mod's comment was wrong for the guns:
  0 none, 1 handgun, 2 shotgun, 3 SMG, 4-7 melee, 8 stun gun, 9+ extras), ownership and equip, the battle queue, the
  movies (DirectShow COM + encrypted .000).
- android/: LauncherActivity, QuestGameActivity (SDL2), Options, Config (sh3vr.ini / sh3.ini in the game folder),
  GameFiles, ImportService, RunLog, ReportProvider; assets controls.txt, credits.txt, whats_new.txt.
- tools/build_quest.py; README.md (sh3-nx's kept as docs/README-sh3-nx.md), CREDITS.md, NOTICE, docs/CONTROLS.md,
  docs/PUZZLES.md, docs/HEADSET_CHECKLIST.md.

## What the build pass must check (compile-unverified)
- C++: the four sh3vr_*.cpp files have never been compiled together: includes (hand.h before pointer.h), the
  anonymous-namespace helpers used across files, Options/OptionRow tables, std::clamp/round use.
- GLSL in sh3vr_body.cpp: multiview programs (#extension GL_OVR_multiview2, num_views 2), uniform arrays uploaded one
  element at a time (uUV[128]), the probe pass reading a 2-layer depth array with a NEAREST sampler.
- GL state: GlSave restores everything the renderer set per draw (program, VAO, buffers, textures, sampler, FBOs,
  viewport, enables, masks, blend); detaching multiview attachments with texture 0; PBO + fence readback a frame late.
- Weapon palette: the assumption that c48 on holds world-space 3x4 rows per bone; the grip vs aim pose for the
  calibration axes; the reference bone choice; skinned strides learned per draw.
- Flashlight: c80-c82 world -> (s, t, q) with row 2 as q; the 1 m plausibility test against Heather's chest.
- Heather's own draws found by palette and stride for the 25 cm near cut (head out of the eyes).
- Equip between frames (0x4711D0 loads the weapon model synchronously through 0x46FD40): a hitch or a wrong state if
  Heather is mid-attack; sh3.log logs each equip and the weapon after it.
- Melee: the hit volumes' segment lengthened by the radius at both ends; punches and clubs need the knife's or pipe's
  attack id learned once (sh3quest_learned.ini) before they land.
- Screen-space collision: hands only stop on what the eyes see (Heather excluded within 0.22 m).
- Auto third person: camera more than 7 m from Heather for 1.5 s; back under 4.5 m for about 1 s.
- Java: the launcher has not been through javac: Options/Config inheritance, lambdas in loops, the foreground service
  type on API 34, SDLActivity's getArguments/getLibraries overrides.

## Still to do on the list (no build needed to write them)
- 5 Movies: log the CLSIDs/IIDs the player asks for, find the .000 decryption (a source filter of the game's or a
  read path), answer the interfaces with FFmpeg (C:\RACQuest\toolchain\ffmpeg has binaries; Android libraries need a
  build), the big screen in VR, the soundtrack through dsound.c's movie path.
- 6 Speed: uniform uploads by change only; measure on the headset.
- 12 Hit-testing for screens without mouse support (the inventory ring); tremor smoothing.
- 19 Full-body IK on Heather's model (her palette from a PC frame capture first).
- 22 Fingers wrapping a world object's own shape.
- 23 The radio's static as haptics.
- 24 The extras' mesh signatures (a PC frame capture with each extra drawn).
- 34 Sound following the head (positional).
- 29, 31, 38: the playthrough after the build.
- 40: the one build pass (tools/build_pc.bat for the PC rig, then tools/build_quest.py), when the owner says.

## Mistake this session
- Once ran `python -` with a heredoc in Git Bash (it hung; stopped with TaskStop at once). The rule stands: write the
  script to a file first. The scratch string finder is in the session's scratchpad (findstr.py), not in the repo.
