# SH3 Quest VR: session 18 handoff (2026-11-25/26)

Picked up from docs/HANDOFF-2026-11-25-SESSION17.md "Part 2" (owner message 86: "was working in another session. pick
up from where it left off. miss/forget nothing. proceed with list", with four headset reports from D:/Downloads:
run_2026-11-25_21-51-48.txt and run_2026-11-25_23-03-50.txt on 0.1.1, run_2026-11-25_23-08-34.txt and
report_1795666121011.txt on 0.1.2, the same run). Owner messages 86-89 are in
C:/SH3Quest/notes/owner-messages-sessions1-7.txt; docs/AUDIT-2026-11-25.md rows 102-111 trace each.

PC builds 208-209 (C:/SH3Quest/build_pc_208.log, build_pc_209.log: BUILD OK); runs vr253-vr259; the release APK
from tools/build_quest.py --release (tools/build_quest_8.log).

## The owner's list (message 84) and what 0.1.3 does

- **Physical melee too sensitive** (84), **for everything, not just the knife** (89). The cause, from the 0.1.2
  reports: the hands' speeds were taken per headset frame (120 Hz) while the game made 50-60 frames a second, so a
  gentle move read twice as fast. Now (sh3vr_body.cpp, sh3vr_combat.cpp `thresholds()`, `update_swings()`):
  - speeds over the real time between displayed frames (`motion_dt()`: the display times, clamped 0.5 period..0.1 s);
  - a swing's travel only above 60% of its speed, the speed held 25 ms, a sweep from 80% of the speed and half the
    travel;
  - thresholds: knife 2.4 m/s over 0.20 m (net 0.14), stun gun 2.0 / 0.14 / 0.10, heavy (pipe, katana, maul, beam
    saber) 3.0 / 0.26 / 0.20, a gun as a club 2.8 / 0.24 / 0.18, a punch 2.8 / 0.22 / 0.15. The heavy and gun paths
    were 0.32 and 0.28 in build 208: vr258's 40 cm heavy swings at 5.8 m/s missed by a hair, so build 209 shortened
    them (the speed, never reached by a gentle or moderate move, does the rejecting).
  - Checked: vr257 (build 208) the knife 6 of 6 real swings, none of the gentle (0.86 m/s), moderate (1.8 m/s) or
    late-frame moves; vr258 (build 208) fists 2/2, handgun 2/2, pipe/katana/maul 1/2 (the 48 cm swing only);
    vr259 (build 209, the release's code): the steel pipe, katana, maul, stun gun (item 9) and handgun 2 of 2 each
    (5.0-5.6 m/s), none from the other moves; its log also has the grain's average, the shadow at 50%, dynamic
    resolution's step back and "leaving (code 0)"; every picture whole.
  - Reading the rig's log: its frames are about 29 behind the script's (a shot named m08175 is log frame ~8146).
- **Drifting fog off, smooth turning on by default** (84): option defaults; older sh3vr.ini files (defaults < 3) take
  0.1.3's defaults once (drifting fog off, smooth turning, no sharpening, film grain, smooth edges or vignette, the
  picture at 100%), saves kept; the launcher (Config.java) and the game (load_options) both do it. vr257: "sh3vr.ini
  from before 0.1.3 (defaults 0)" and the file rewritten (defaults = 3, turn = 1, drifting_fog = 0, ...).
- **"This weird fuzz over the screen", completely off; no screen filters by default** (84). The fuzz was the game's
  SECOND grain, not the Noise Effect (0x41C090, already off): 0x429A60 mode 2 draws every frame a grey quad
  (DESTCOLOR/ONE: the picture x (1+L)) and then a 128x128 DXT5 noise tile, UV 0..4, at a random offset, subtracted
  (BLENDOP REVSUBTRACT; the DrawPrimitiveUP at 0x429E38, return 0x429E3E). With "Film grain" off, d3d8.c's
  `grain_tile_draw()` binds the tile's average instead (`res_flat_texture()` in d3d8_res.c: 12 12 12 8), so the
  picture keeps its brightness with no noise. vr257 vs vr256 at frames 7400-7402: the residual where lit 4.38 ->
  0.32, pixels changing 4+ levels frame to frame 23.0% -> 0.0%; bright parts the same (85.6 / 85.1), the darkest
  about a level darker (the grain's specks lifted them). GAME_FACTS.md has the facts.
- **Her body shadow less overbearing** (84): her shadow's mask drawn again at 50% after the room's shadows ("her shadow
  counted after the room's, its mask drawn again at 50%"), none from a hand within 15 cm of the lamp. vr258's A/B
  (hand in the beam / lowered, the head still): a faint, soft hand shadow on the floor ahead (4.7% of the floor 3+
  levels darker, 1% 8+).
- **A new release**: 0.1.3, versionCode 4 (below).

## Also from the reports (message 86) and messages 87-88

- **Quitting crashed** ("FORTIFY: pthread_mutex_lock called on a destroyed mutex" on the sound thread after
  ExitProcess): leaving now flushes the open files and ends the process without the exit-time teardown
  (rt_core.c, kernel32.c). vr257-vr259: "leaving (code 0): open files flushed ...".
- **Dynamic resolution at 60% for nothing**: it steps back when drawing smaller did not make the frames quicker
  (vr257: "did not make the frames quicker (18.44 ms ... 18.03 after) ... back to 100%, no step down for 30 s").
- **AppSW on 0% of frames**: SH3 never calls the motion target / space warp, so "Smoother frames (AppSW)" is out of
  both menus (opt.space_warp false). A real AppSW (motion vectors and depth to the runtime) is the follow-up below.
  The game makes at most 60 frames a second (GAME_FACTS Timing): 120 Hz shows each twice and stays the default.
- **"Dynamic cutscenes" not named in the VR menu** (87) and **all of it belongs in the VR menu** (88, answered: yes,
  the VR menu (hold the left menu button ~1 s) and the launcher's Options tab, never the game's own menus): the
  "Cutscenes" row now reads Dynamic (her eyes or the camera) [default] / First person / Cinema screen / At the camera
  (3, new, last so 0-2 keep their meaning); 0.1.2's separate dynamic_cutscenes switch is folded in once (off ->
  At the camera) in load_options and Config.foldDynamic(). vr257's VR-menu picture shows the row. Not run: a 0.1.2
  file with dynamic_cutscenes = 0 (the gamedir's had 1).

## Release 0.1.3

- build_quest_8 (`tools/build_quest.py --release`, every native file compiled from this code: the same sources as PC
  build 209) gave android/build/SH3QuestVR-0.1.3.apk: versionCode 4, "0.1.3", 9,214,086 bytes, SHA-256
  87e8d31550f5c1a150a991decba3ed776da5161f5c8d473ebeb7240e75b61c47, the release key (certificate 38811d96...).
- tools/apkcheck.py against 0.1.2: ALL CHECKS PASS. The same 17 files; changed only AndroidManifest.xml (the
  versionCode/versionName lines alone), assets controls.txt, features.txt, whats_new.txt, classes.dex and libmain.so
  (29,752,424 bytes, the same NEEDED, no text relocations, SDL_main exported); libSDL2 and the OpenXR loader
  byte-identical; both verify with the same certificate. The APK's libmain holds "the game's second grain", "swing
  counts", "leaving (code %d)", "sh3vr.ini from before 0.1.3", the Cutscenes row's names (classes.dex too); "Smoother
  frames" is in neither.
- **0.1.3 PUBLISHED** (2026-11-26, a new release as the owner asked: "do a new release"): main 96f7288 (the 0.1.3
  commit), annotated tag v0.1.3 (664a279) on it, release "Silent Hill 3 Quest VR 0.1.3" with
  docs/RELEASE-NOTES-0.1.3.md and the APK: not a draft, not a prerelease. releases/latest = v0.1.3, GitHub's digest
  sha256:87e8d315... matches, 9,214,086 bytes; v0.1.0, v0.1.1 and v0.1.2 untouched. The launcher's updater asks
  releases/latest for "SH3QuestVR-" + the tag + ".apk" = SH3QuestVR-0.1.3.apk: the asset's name. The APK's three
  assets are byte-identical to the commit's.
- Verified on this code before publishing (build 209 = the APK's sources): vr259 (every weapon kind's swings, the
  grain's average, the shadow, dynamic resolution, a clean quit); the rest on build 208, whose code differs only in
  the heavy and gun swing paths: vr257 (no grain, the one-time defaults, the Cutscenes row, the knife, smooth
  turning), vr258 (the fists, the shadow A/B).

## Open, in order

1. The players' and the owner's reports on 0.1.3 (GitHub issues; the launcher's report: sh3-crash.log, sh3.log):
   melee on the headset (each kind; "swing counts" lines show the speeds a player's swings reach, the first 20), the
   picture with no fuzz, the defaults taken once, quitting with no crash.
2. AppSW for real (the follow-up): SH3 draws no motion vectors; a depth-reprojection AppSW needs the depth and the
   motion of each frame given to XR_FB_space_warp. Its option comes back only once it works.
3. The headset-only items: docs/HEADSET_CHECKLIST.md (0.1.3's grain, defaults, Cutscenes row, smooth turning added).
4. LIST 45's floor items by hand: as session 17 left it (the mall, a save or the headset).

## Files changed in session 18 (all in the 0.1.3 commit)

- src/d3d8/d3d8.c (grain_tile_draw), d3d8.h, d3d8_res.c (res_flat_texture); src/runtime/rt.h, rt_core.c and
  src/win32/kernel32.c (leaving without the teardown); src/vr/sh3vr.cpp (defaults 3 and its one-time move, the
  Cutscenes row, dynamic resolution's step back, real-time smooth turning), sh3vr_body.cpp (hand speeds over
  motion_dt), sh3vr_combat.cpp (the swing rules), sh3vr_ik.cpp and sh3vr_internal.h (her shadow at 50%, the
  options), xr.h, xr_openxr.cpp, xr_sim.cpp (display times; the rig's "late").
- android: build.gradle (4, "0.1.3"), Config.java (foldDynamic, the defaults), Options.java (the Cutscenes row, the
  AppSW row out, help texts), assets controls.txt, features.txt, whats_new.txt.
- tools/sim: fuzz253.txt, check256.txt + make_check256.py, melee258.txt + make_melee258.py, melee259.txt +
  make_melee259.py, door252.txt (session 17's); tools/apkcheck.py (the release APK against the last).
- docs: RELEASE-NOTES-0.1.3.md (new), README.md, CONTROLS.md, GAME_FACTS.md, HEADSET_CHECKLIST.md, LIST-0.1.0.md
  (item 27; session 17's vr252 lines in 45), AUDIT-2026-11-25.md, HANDOFF-2026-11-25-SESSION17.md (its part 2 and
  vr252), this handoff.
