# SH3 Quest VR handoff, session 24 (2026-11-26): 0.1.6

Repo C:/SH3Quest/SH3QuestVR. 0.1.5 = 4761a88 (tag v0.1.5). This session made and published 0.1.6 (see the end for the
commit, tag and the APK's digest). Read HANDOFF-2026-11-26-SESSION23.md for where 0.1.6 started.

## The owner's messages (C:/SH3Quest/notes/owner-messages-sessions1-7.txt, 114-122; backups .bak-s24b)

- 111 (session 23): sprint toggle (on by default), a toggle for the wrist's bars, smooth the arms twisting two-handed
  while walking back, refine what the video shows.
- 115-118: a player's report on 0.1.5, "Enemies only become visible when they're right on top of you", with a video
  (D:/Downloads/f.mp4) and log (D:/Downloads/sh3 (1).log). The many weapons at the start are the nightmare's own
  (owner 117: normal). "pcvr port already acknowledged this somehow" (the Camera Mod: the game's camera at her head).
- 119: "make no mistakes, make sure this next build is perfect as can be".
- 120: "dont ask for my permission, just publish when done".
- 121: "set defaults to the best performance and force switch it in this next update ... BEST defaults for BEST
  performance" (120 Hz). Session 24 had tried a 72 Hz start; taken back.
- 122: holsters swapped (gun on the right hip, melee over the right shoulder) with a toggle; radius options for the
  weapon holsters and the ammo pouch; the ammo pouch's radius a bit larger.

## What 0.1.6 changed (code)

- src/game/sh3.c, wrapper of 0x43A0F0 (the models' culling test, against the game's own camera): a model it leaves
  out is tested again from the head (VR_SCRATCH's view): drawn within 80 degrees of the gaze and the far distance
  [0x743BD0]; the game's hidden list (0x742A64/0x742A68) respected. Testing: SH3_CULL_TEST=1 tests every model from
  the head (counts every 120 frames). GAME_FACTS.md, Camera, has the addresses. tools/build_quest.py checks the
  wrapper (sub_0043A0F0_gen).
- sh3vr.cpp:
  - load_options: defaults 6 puts every Performance row at its default once (owner 121), logged with what changed;
    the launcher's Config.newDefaults the same (and Game threads at three cores).
  - update_sprint: the Sprint toggle (sprint_toggle, Comfort, on): a click runs until clicked again or the stick
    rests 0.5 s (real time, game_seconds); a click while standing leaves time to push.
  - plan_move: in first person her facing follows the head while she walks (backing up, strafing); two-handed she
    turns after the head from 20 degrees (50 otherwise).
  - The reload hint: UiKind::kHint, a level label over the gaze (ui_place_hint), its backing only round the text.
  - Rows: wrist_health "Health and stamina on your wrist" (View), sprint_toggle, gun_on_hip "Gun on the hip, melee on
    the shoulder", holster_radius_cm "Weapon holster radius" (18), ammo_pouch_radius_cm "Ammo pouch radius" (14).
- sh3vr_combat.cpp: HolsterId kGunHolster / kMeleeHolster (were kShoulder / kHip); holster_at_hip/at_shoulder by
  gun_on_hip; holster_radius from the options; sim_holster_spot for the rig.
- xr_sim.cpp: "lh_at_holster N" / "rh_at_holster N" (1 gun, 2 melee, 3 belt pouch, 4 chest pocket, 5 ammo pouch).
- Launcher: Options.java (REFRESH_DEFAULT "120", the new rows), Config.java (the defaults-6 fold).

## Rig checks (PC build; pictures in C:/SH3Quest/shots/vrNNN, logs C:/SH3Quest/vrNNN.log)

- vr284 (tools/sim/check284.txt, an old sh3vr.ini with 72 Hz, 1.2x, foveation High): the fold set 120 Hz, 1.0x,
  Medium ("changed: Refresh rate, Render resolution, Foveated rendering"); the hint level over the gaze, gliding and
  settling; sprint on/off/let go; the shotgun two-handed walking back and strafing ("her body keeps facing the head",
  arms never through her). No culling line: nothing near the park's start is left out by the game's camera.
- vr285 (wrist.txt, its own setup): the watch's bars shown (4094 px), faded turned away, back, less, more: as
  before. (vr284's wrist pose came after the shotgun and never faced the watch to the eyes: not a fault.)
- vr286 (tools/sim/check286.txt, SH3_CULL_TEST=1): holsters swapped and named "hip (guns)" / "shoulder (melee)";
  the handgun put away at the hip, the knife drawn over the shoulder, the handgun drawn 17 cm off the hip holster
  (sounds at each); no magazine from 20 cm in front of the ammo pouch, one taken at 12 cm; sprint off 31 frames after
  the stick let go. FOUND: no culling line at all: **0x454670 calls 0x43A0F0 by address (RECOMP_ICALL_SAFE), which
  goes through recomp_lookup_manual (sh3.c) and then the dispatch table's sub_0043A0F0_gen: the wrapper was never
  reached.** Fixed: its case added; tools/build_quest.py now refuses any `void sub_X(void)` wrapper in sh3.c without
  a case. Also: the VR menu walk failed (the sim's pointer took a View row; the rig's body_fade was toggled, put back):
  check286.txt now points the controllers down while the sticks walk the menu.
- vr287 (check284.txt again, final build, the old ini again): the fold again ("changed: Refresh rate, Render
  resolution, Foveated rendering"; the ini saved with defaults 6 and the new rows); every picture as vr284's (the head
  turned, the hint level, gliding and settled, sprint, the shotgun two-handed backing up and strafing); sprint on/off by
  click and by the stick let go; no crash. No culling line: at the park's start the game's camera leaves nothing out
  that the head sees (vr288's SH3_CULL_TEST shows the head's test itself).
- vr288 (check286.txt again, final build, SH3_CULL_TEST=1): the wrapper reached now ("SH3_CULL_TEST: 240 model tests,
  240 drawn from the head's view" every 120 frames from the park's start; at the start Heather and her weapon are the
  only models tested, both by the head); every head angle's picture as vr286's (nothing in view missing). The holsters
  as vr286 (the handgun put away at the hip, "hip (guns)"; the knife drawn over the shoulder; the handgun drawn 17 cm
  off the hip; sounds at each); no magazine 20 cm out, one taken at 12 cm; sprint on by the click, off about half a
  second after the stick let go (as vr286); the VR menu's Hands and weapons page shows "Gun on the hip, melee on the
  shoulder On", "Weapon holster radius 18 cm", "Ammo pouch radius 14 cm", stepped to 15 cm and back ("VR menu:
  ammo_pouch_radius_cm = 15 cm", then 14); the rig's sh3vr.ini unchanged after. No crash.
- vr289 (tools/sim/check289.txt, final build, as players have it: no SH3_CULL_TEST): door252's walk through the park's
  gate into the hall, where its dog comes at her; the head turned left at it. "a model the game's camera leaves out
  (0200 015D, 8.8 m from the head, 53 degrees off the gaze, radius 1.12 m) drawn: the head sees it": 0200 015D is the
  hall's dog (the attacker's +0x80 in the steer test's dump). It bit her from behind (its place 0.67 m behind her
  facing), so the left-looking pictures never had it. No crash.
- vr290 (tools/sim/check290.txt, as vr289; the head to both sides at the gate, then behind her in the hall): the dog
  left out by the game's camera **8.7 m from the head, 7 degrees off the gaze** (squarely in view: 0.1.5 drew nothing
  there), and a second monster (0202 015E) 14.8 m off at 57 degrees, both drawn; "1035 model draws the game's camera
  left out were drawn from the head's view" in the next 1200 frames. The hall's picture (j08200) shows the dog several
  metres off by the far pillar, whole. No crash. The owner's player report (115) is answered on the rig.

## Release

- The release build: `source /c/RACQuest/env.sh; C:/SH3Quest/venv/Scripts/python.exe -I tools/build_quest.py --release`
  (log C:/SH3Quest/build-quest-016.log): android/build/SH3QuestVR-0.1.6.apk, versionCode 7, **9,522,642 bytes,
  SHA-256 7b16d82a503b1faade43a09b8e6f62acc8cd6495af21bdff3bb0bdab52832206**, signed with the release key (certificate
  38811d96...). A copy kept at C:/SH3Quest/SH3QuestVR-0.1.6.apk (the next release's apkcheck compares with it).
- tools/apkcheck.py against 0.1.5: ALL CHECKS PASS (22 files each; changed: the manifest's versionCode and versionName
  only, controls/features/whats_new, classes.dex, libmain.so; libSDL2 and the OpenXR loader byte-identical; no text
  relocations; the same certificate). The new rows' names, the fold's and the culling's log lines are in libmain.so,
  the launcher's rows in classes.dex.
- Published per owner 120 ("dont ask for my permission, just publish when done"): main **ba72cb1** (pushed), annotated
  tag **v0.1.6**, release page https://github.com/moistman42069/SilentHill3-Quest-VR/releases/tag/v0.1.6 with
  SH3QuestVR-0.1.6.apk and docs/RELEASE-NOTES-0.1.6.md as its text. releases/latest is v0.1.6; its asset 9,522,642
  bytes, GitHub digest sha256:7b16d82a...2206, the downloaded copy hashed the same (what the launcher's update checks).

## Open

- The players' replies wait for the owner (not posted unasked): C:/SH3Quest/notes/reply-player-report-0.1.5.txt (the
  0.1.4 report, answered by 0.1.5) and reply-player-report-0.1.6.txt (the 0.1.5 report on the monsters, answered by
  0.1.6).
- Headset checks: docs/HEADSET_CHECKLIST.md's 0.1.6 rows (the monsters at a distance in the park and the dogs' run,
  the holsters, the pouch radius by feel, 120 Hz).
- Cleanup for the owner to decide (listed, not deleted): C:/SH3Quest/shots/video114 and video116 (the videos' frames),
  docs/RELEASE-NOTES-0.1.5.draft.md and docs/whats_new-0.1.5.draft.txt (untracked drafts); C:/SH3Quest/shots holds
  28 GB in 276 rig picture folders (this session's vr284-vr290 about 1.5 GB).
