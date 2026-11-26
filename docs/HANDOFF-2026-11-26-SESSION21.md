# SH3 Quest VR: session 21 handoff (2026-11-26)

Read this first, then `docs/HANDOFF-2026-11-26-SESSION20.md` for the 0.1.4 report's background. The owner's messages
are in `C:/SH3Quest/notes/owner-messages-sessions1-7.txt` (96 to 105 are this session's).

## State

- **0.1.5 in progress, nothing committed.** The repo is at 147792d (session 19's handoff; 0.1.4 published at 50047a1,
  tag v0.1.4). Every change below is uncommitted (`git status`). Version still 0.1.4: `android/app/build.gradle`
  needs versionCode 6 / versionName "0.1.5".
- **PC build 217 is OK** (`C:/SH3Quest/build_pc_217.log`) and has everything below. No regen is needed unless a new
  manual wrapper is added (regen 215 has sub_0049FAC0).
- **The Quest APK has not been built this session.** `tools/build_quest.py`'s wrapper check list may need
  sub_0049FAC0_gen, sub_0041E200_gen and sub_0041FF80_gen.
- **Next sim run: vr270.** Shots go to `C:/SH3Quest/shots/vrN`, and that folder must exist first (`mkdir -p`):
  the sim saves nothing into a missing folder (vr267's first try lost its shots this way).
- Release notes are drafted:
  - `docs/RELEASE-NOTES-0.1.5.draft.md`: the top and What's new. The rest still has to be taken from 0.1.4's notes;
    see "Release notes" below.
  - `docs/whats_new-0.1.5.draft.txt`: the launcher's What's new entry.
  - Both hold the placeholders LAMP_GLOW_LINE / LAMP_GLOW_SHORT for the glow item.

## The owner's messages this session (96-105)

- 96: pick up from the handoff.
- 97: resolution back to 100%.
- 98: 120 Hz by default, performance defaults as I see fit, ghost hands off.
- 99: remove the thin red line at the pointer's circle.
- 100: Heather's (IK body's) shadow off by default.
- 101: no screen-placed effects such as lens flares; "be sure visuals all look good".
- 102: a player's report: black screen after ~5 s, strange light reflections (D:/Downloads/sh3.log, Test.1.mp4).
- 103: hide the head in Dynamic cutscenes ONLY in first person.
- Then:
  - performance "as good as it can be on quest";
  - the new app icon from the cover image;
  - "everything worked into this release";
  - adjust the performance defaults if confident;
  - the coloured orbs "should follow whatever theyre coming from in view, then not be there when out of view ...
    double check";
  - "cmon man, open your eyes. you should be catching shit like this" (check every picture whole).
- 104: "check pcvr build for visual fixes that have already been done. i assume this is one of them. proceed if not."
- 105: "wrap up and do a handoff".

## Done in 0.1.5 (coded; checked on the rig as noted)

- **The cutscene time step** (session 20): the game's step is the real time each frame stands for, up to 3/60 s
  (`sh3vr_frame_seconds`).
- **The defaults, folded once** (`kDefaultsVersion` 5 in sh3vr.cpp; Options.java `DEFAULTS_VERSION` 5):
  - Smoother frames (AppSW) off;
  - Render resolution 1.0x on every headset (a Quest 3/3S still at 0.1.4's 1.5x goes to 1.0x);
  - ghost hands off;
  - Heather's shadow off;
  - 120 Hz kept.
  - The fold's log line was checked.
- **The pointer's red line** removed (xr_openxr.cpp, the dot's square).
- **Black screen and 2 s freezes** (the player's report): `fill_chain` writes every image only the first time, then
  never waits on the image the headset is showing. Frames over 250 ms are told with their parts (`frame_breakdown`).
- **Depth of field and the shaking double picture** left out in VR (the screen-effect pass: 0x420370, 0x41E200,
  0x41FF80). Told in the log.
- **Lens flare** (0x4690A0 → 0x468DF0 / 0x468F6E rings and glow, 0x468B40 / 0x468CDA streaks): now left out
  **entirely** in VR, glow included, as the PC VR mod does (build 217). `sh3vr_glow_map` was removed.
- **The lamp test without reading the GPU back**: the sub_0049FAC0 wrapper (sh3.c) and `sh3vr_point_in_view`.
  - The original drew into a 1x1 target and LockRect-read it: a GPU sync per lamp per frame.
  - Checked: "the lamps' test" lines.
- **Dynamic cutscenes**:
  - Facing from her head bone (`scene_facing`; checked: 1.44 vs her place's 0.00).
  - Her head and hair hidden from her eyes in first person only (body_cut's scene branch).
  - The newest rule (a draw all at her head: fade 1) logs "a draw all at her head ... hidden from her eyes" in
    vr267/vr269. **Its pictures (s05200, s05700 in vr267 and vr269) have not been looked at yet.**
- **Head held in a wall 2 s**: the view is brought back to her (plan_move fail-safe).
- **Both sticks clicked together recentre** (`stick_chord`): kSoundMenu variant 2 and a buzz. Checked once on the
  rig. Added to docs/CONTROLS.md and assets/controls.txt this session.
- **Quit crash**: `rt_leave(0)` on every way out (the entry point returning, SDL_QUIT).
- **Performance**:
  - D3DX natives (0 differences over 19.2 million calls);
  - hidden TLS and -Bsymbolic-functions;
  - getenv cached;
  - the framebuffer status cache (d3d8_res.c);
  - manifest: `trade_cpu_for_gpu_amount` -1 (Quest 3/3S) and `com.oculus.dualcorecpuset` true (Quest 2/Pro);
  - BOOST and the renderer thread hint kept.
  - The game thread's busy share fell from ~75% (vr264) to ~17% (vr266) of the profiler's samples.
- **The app icon**: `mipmap-*/ic_launcher_sh3.png` from `tools/make_icon.py` (the source in C:/SH3Quest/icon_work);
  the manifest points to it.
- The log is quieter.

## OPEN: the lamps' glows (the owner's orbs) — finish this first

What vr267/vr268 showed (build 216):
- Glows beside or far from their lamps, and a blue one where no lamp is, at **the same pixels after an 80° head turn**
  (c07410 vs c07440).
- They are not the flare. SH3_ROUTE_DUMP at raw frame 7355 (the play's script frame 7410 is raw ~7356; play starts at
  raw 7258 = script 7312) shows them all from the game's sprite helper. All are vs 0x144, z 0.95-0.998, blended, and
  routed through the world-effects path (`sh3vr_effect_map`):
  - 0x5F69F8 (128x128 texture, 5 draws, 50-215 px);
  - 0x5F6937 (128x128, 2 draws; the blue one at x 835-891);
  - 0x5F68CC (512x512, tiny 6 px sparkles, 10 at the blue light).
- So the game projected them with a camera that does not turn with the head, and the eyes took them as the frame
  camera's.

Build 217's attempt:
- `sh3vr_effect_map(ndc, tex_w, game_view, out)` takes every effect back through `inv(from_d3d(game_view)) *
  inv(from_d3d(p_main))`, then `vp_m` (the head). `game_view` is sh3.c's `game_cam_view` via the new `sh3_game_view()`.
- The weapon effects are still moved with the gun (`combat_effect_rel` now gets that world point).
- vr269 (build 217): the misplaced orbs are gone from c07410 and c07440, **but the lamps have no halos either**.
- The log's "an effect of the game's ... put at its place in the world" lines throw them far out of view. For example,
  frame 1927: the game's camera at ndc -0.109 -0.974 becomes the head's 0.220 3.911.
- So `game_cam_view × p_main` is **not** the camera the effects were projected with (the projection or the view is
  wrong).
- **Not acceptable as it stands**: the muzzle flash, smoke, sparks and blood go through the same path and may now be
  misplaced or gone. That has not been looked at.

Next, in order:
1. Find the matrix the effects are projected with:
   - `sub_00678C20` (recomp_0045.c:11592) loads `eax = 0x72C460` (the camera block's view×projection, D3D row-vector),
     likely the project-a-point helper. Check that the sprite helper's callers project through it.
   - Wrap sub_00678C20 (a manual wrapper: regen) to keep the 16 floats at 0x72C460 at its call.
   - Use those as G: `game_c2w = inv(from_d3d(G))`, `map = vp_m * R * game_c2w`. Log G against vp_m once.
   - The flat projection is p_main with [0] and [5] divided by proj_k (sh3vr_proj_scale widens it for both eyes). If
     the effects use the unscaled one, that alone moves them.
2. Check the pictures whole (vr270, script tools/sim/check267.txt):
   - each halo on its lamp in both eyes, gone when the lamp is out of view;
   - nothing floating.
   - Also fire a gun (add a shot with `right_trigger`) to see the muzzle flash at the muzzle.
3. If it cannot be made exact quickly, the owner's 104 says take the PC VR mod's fix:
   - The PC VR mod (C:/SH3Quest/refs/Silent-Hill-3-VR-Mod, sh3vr/sh3vr_proxy/d3d9_hook.cpp `ShouldReplayStereoDraw`,
     positionMask 4 → false; `IsCutsceneHeatherDrawForStereo`'s note: "subtitle/UI/effect draws use Draw*UP and
     remain excluded") puts **no** pre-transformed sprite in its eyes. The lamps keep their glowing 3D bulbs.
   - That means leaving out the sprite helper's glow draws (0x5F69F8, 0x5F6937, 0x5F68CC) in the eyes when they are not
     the weapon's (`combat_effect_rel` false), and putting `sh3vr_effect_map` back to its 0.1.4 form for the weapon's
     effects.
4. Then fill LAMP_GLOW_LINE / LAMP_GLOW_SHORT in the drafts. Say "as the PC VR mod" where it applies.

## Release notes (after the glows)

`docs/RELEASE-NOTES-0.1.5.draft.md` holds the title, the intro and What's new. Append 0.1.4's sections from
"Updating from an earlier version" to the end, with:
- 0.1.4 → 0.1.5 throughout. "Already on 0.1.0 to 0.1.4". The options line: "except Smoother frames, the ghost hands
  and Heather's shadow, turned off once, and a Quest 3/3S's Render resolution still at 1.5x, set to 1.0x once".
- Everything in 0.1.5:
  - the ghost hand "can show" (off to start);
  - Heather's shadow off to start;
  - the picture line (no lens flare; the lamps' glows as decided);
  - Performance: 120 Hz, an even 60 frames a second; Smoother frames if you want it, off to start; foveated;
    Dynamic resolution; drop "the top resolution on a Quest 3 or 3S";
  - "Every VR feature is on from the start except Smoother frames, the ghost hands and Heather's shadow".
- Controls table: add "Recentre the view | Both sticks clicked together".
- Known for now: Smoother frames is off to start; the rest as 0.1.4.
- Files: the new APK's size, SHA-256, versionCode 6; the same release key (certificate SHA-256 38811d96...).
  0.1.0 to 0.1.4 on their own pages.

Then:
- android/app/src/main/assets/whats_new.txt: put the drafted 0.1.5 entry above 0.1.4's.
- features.txt: Heather's shadow and the ghost hands off, both sticks recentre, no lens flare.
- README's feature list.
- docs/HEADSET_CHECKLIST.md (~line 50: Heather's shadow "on" → off; add both sticks, the scene facing, the head
  hidden).
- docs/GAME_FACTS.md:
  - the time step;
  - the screen-effect pass 0x420370 / 0x41E200 / 0x41FF80;
  - the flare 0x469E70 → 0x468840 → 0x4687C0, record 0x898420 (+0x40 view point, stride 0x50), drawn by 0x4690A0
    → 0x468DF0 / 0x468B40;
  - the lamp test 0x49FAC0 (1x1 target, LockRect);
  - the sprite helper 0x5F69F8 / 0x5F6937 / 0x5F68CC;
  - what the effects are projected with (once found).
- The AUDIT rows.

## Then the release (owner rules)

1. versionCode 6 / "0.1.5".
2. Build the APK with tools/build_quest.py, then apkcheck. Hand over the path and SHA-256.
3. Re-check every 0.1.5 feature on the release build itself (older runs do not count). Keep it short: ~30 min of sim
   in all, check every picture whole.
4. **Ask the owner before publishing.**
5. A reply for the player's report (black screen, light reflections) without naming them in public notes.

## Files changed this session (uncommitted, besides session 20's)

- **sh3vr.cpp**:
  - `scene_facing`, `stick_chord`, the plan_move fail-safe;
  - `frame_clip` / `sh3vr_point_in_view`;
  - `sh3vr_effect_map` with `game_view` (build 217; see OPEN).
- **sh3vr.h**: `sh3vr_effect_map`'s new parameter and `sh3_game_view`; `sh3vr_glow_map` removed.
- **sh3vr_ik.cpp**: game_head_fwd, scene_drawn, the body_cut scene branch, the all-at-head rule.
- **sh3vr_internal.h**, **sh3vr_sounds.cpp** (kSoundMenu variant 2).
- **d3d8.c**: the whole flare left out; the effects path passes `sh3_game_view`.
- **d3d8_res.c**: the framebuffer status cache, `res_report_framebuffer`.
- **sh3.c**: the sub_0049FAC0 wrapper and lookup; `sh3_game_view`.
- **rt_core.c**, **launcher.c**: rt_leave.
- **AndroidManifest.xml**: dualcorecpuset, the icon.
- **Android resources and tools**: mipmap-*/ic_launcher_sh3.png, tools/make_icon.py.
- **Docs**: docs/CONTROLS.md, assets/controls.txt.
- **Sim scripts**: tools/sim/check265-268.txt.
- **Drafts**: docs/RELEASE-NOTES-0.1.5.draft.md, docs/whats_new-0.1.5.draft.txt.

## Logs and pictures

- vr265 and vr266: the checks on build 214 (vr266's scene shots showed the hair from inside, now handled; its profile
  is in C:/SH3Quest/prof266.txt).
- vr267: build 216, check267, the glows misplaced and fixed on screen.
- vr268: the route dump, raw frames 7355-7357.
- vr269: build 217, check267.
- Shots in C:/SH3Quest/shots/vr26N.
