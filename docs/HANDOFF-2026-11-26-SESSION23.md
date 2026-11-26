# SH3 Quest VR handoff, session 23 (2026-11-26): 0.1.6 started from the owner's feedback on 0.1.5

Repo C:/SH3Quest/SH3QuestVR at 1bd89d8 (0.1.5 = 4761a88, tag v0.1.5, published). **No code changed this session**:
only this handoff (uncommitted) and the owner's messages saved. Read HANDOFF-2026-11-26-SESSION22.md for 0.1.5.

## The owner's messages (saved in C:/SH3Quest/notes/owner-messages-sessions1-7.txt as 111-113; backup .bak-s23)

- 111: praise ("feels native", loves the reload tutorial pop-ups), then for the next release:
  - "an option for a sprint toggle (on by default)";
  - the health and stamina bars show by default though not unlocked in the game: fine, "just make sure theres a
    toggle there so people can switch it on and off";
  - "the arms twisting a bit when two handing a gun, i assume due to how the body position works when walking back
    while looking forward ... see if theres anyway to smoothen that out";
  - "feel free to refine and smoothen out anything else you see fit based on the findings in the video";
  - "proceed with list".
- 112: the first log was the wrong one; the right ones are D:/Downloads/run_2026-11-26_10-07-03.txt and
  D:/Downloads/report_1795705626543.txt (the same 0.1.5 run; the second only adds 19 logcat lines).
- 113: "wrap up and do a handoff".

## Files

- Video: D:/Downloads/dev.sh3quest.vr-20261126-095755-0.mp4 (61 MB). **Not watched yet.** Decoders that work:
  Python cv2 5.0.0, and ffmpeg at
  C:/Users/Shadow/AppData/Roaming/Python/Python314/site-packages/imageio_ffmpeg/binaries/ffmpeg-win-x86_64-v7.1.exe
  (no ffmpeg on PATH). Put frames on C: (D: is nearly full); run Python that imports cv2/PIL without `-I`.
- D:/Downloads/run_2026-11-26_09-53-22.txt: a **0.1.4** report (old options file), short run: ignore.
- D:/Downloads/run_2026-11-26_10-07-03.txt: the **0.1.5** report, Quest 3, NA exe, 120 Hz, Smoother frames off,
  1.0x, dynamic resolution on. sh3.log is lines 91-1929, the run before 1932-2350, logcat 2350 on.

## Found in the 0.1.5 log so far

1. **Frame rate at the defaults (120 Hz, no AppSW): about 70-90 frames/s with 300-500 displays missed each 10 s in
   play**, "worked" 10-14 ms a frame against an 8.33 ms period (CPU side; the GPU lines say it has room); dynamic
   resolution logs "the smaller pictures did not make the frames quicker". Single long frames: 2479/545 ms at the
   start (loading), 156/151 ms at frame 2788, 185/166 at 5779, 863 at 7536 and 468 at 9247 (xrBeginFrame 84-89 ms in
   the last two; likely loads). Owner 106 asked for no choppiness at the defaults: decide whether the default
   refresh should drop (90 or 72 Hz) or where the CPU time goes (tools/prof_sample.c exists). Check the video for
   judder before choosing.
2. **Walking back while looking forward turns her body round**: "her body turns after the head" at -146, -168,
   -178, 174, 177 degrees off. Code: src/vr/sh3vr.cpp ~2271-2305. While the stick steers, `move.face_yaw` turns her
   facing toward `move.heading` (the stick's way, up to 10 rad/s): stick back = she turns 180 degrees and walks
   toward the head, so her shoulders face behind while the hands (two-handing the gun) are in front: the arms twist
   through her. Likely fix: when the walk's heading is more than about 100 degrees off the view heading (backing
   up), or a gun is raised / two-handed (move.aim), keep her facing on the head (or the gun's aim) and let her back
   step; check how the game draws a backward walk (does SH3's walk have a back-step animation? GAME_FACTS has
   nothing on it) and what the PC VR mod does (owner rule: read the PC VR mod's source first).
3. **The two-hand grip**: taken and let go 9 times; while held, "handgun held back toward her: the left hand's
   second grip N cm past her arm's reach" 19 times (sh3vr_combat.cpp ~865 and ~1160), and the left controller "past
   her arm's reach" around the same frames (e.g. 18495-18581, right after her body turned 177 degrees). So item 2
   is likely the cause of the twist; recheck after fixing it.
4. **The wrist bar's toggle**: it exists, "Health bar on your wrist" (`wrist_health`; sh3vr.cpp:138 the VR menu's
   row, Options.java:238 the launcher's, default on in both the options struct and the launcher). The name does
   not say stamina: rename to something like "Health and stamina on your wrist" (plain-names rule) in the VR menu,
   the launcher, CONTROLS/README/notes. Check the VR menu row's last argument (`false` in the SW(...) at
   sh3vr.cpp:138) is not its default.
5. **Sprint**: today run = the left stick held in (`move.run = c.left_click`, sh3vr.cpp:2239). Add a "Sprint
   toggle" option (plain name, e.g. "Click the stick to run (toggle)"), on by default: a click turns running on
   until clicked again or the stick is let go (decide; say which in the notes); in both the VR menu and the
   launcher's Options; CONTROLS.md, assets/controls.txt, the notes' Controls table.

## 0.1.6 list (in order)

1. Watch the video (frames every 0.5-1 s, denser at the two-hand moments) and note everything else to smooth.
2. Sprint toggle option, on by default (item 5 above).
3. The wrist bar's toggle named for health and stamina, easy to find (item 4).
4. Her body kept facing the head/aim when backing up or two-handing; the arm twist gone (items 2-3); PC VR mod first.
5. The frame rate at the defaults (item 1), and whatever else the video shows.
6. Docs: versionCode 7 / "0.1.6" (android/app/build.gradle), docs/RELEASE-NOTES-0.1.6.md, whats_new.txt,
   features.txt, CONTROLS.md, assets/controls.txt, README, HEADSET_CHECKLIST, GAME_FACTS, AUDIT, handoff, memory.
7. PC build (one at a time, never while sh3.exe runs), short targeted rig runs (~30 min max: walk back with the gun
   two-handed, the sprint toggle, the wrist bar off/on), pictures checked whole; then
   `source /c/RACQuest/env.sh; C:/SH3Quest/venv/Scripts/python.exe -I tools/build_quest.py --release`, apkcheck plus
   the scratchpad manifest_diff.py; **ask the owner before publishing**.

Still open from 0.1.5: the player's reply (C:/SH3Quest/notes/reply-player-report-0.1.5.txt) waits for the owner.
