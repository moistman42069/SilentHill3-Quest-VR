# Silent Hill 3 Quest VR: handoff for session 10 (2026-11-25)

Session 10 picked up session 9, which went on past its handoff (builds 122-125, runs vr145-vr151) and stopped on a
usage limit in the middle of an edit. Session 10 opened on the owner's "was working in another session. pick up from
where it left off. miss/forget nothing. proceed with list.". Read docs/HANDOFF-2026-11-24-SESSION3.md (rules, build,
run), HANDOFF-2026-11-25-SESSION8.md, HANDOFF-2026-11-25-SESSION9.md, then this. Every owner message is in
C:/SH3Quest/notes/owner-messages-sessions1-7.txt (messages 36-38 added this session).

## Where it stands

- **Last build: build_pc_137, OK. Last run: vr164** (build 137, tools/sim/scenes_near.txt; see "Done later in session
  10"). Session 10 built 126-137 and ran vr152-vr164 (about 29 of the 30 minutes of runs: the budget is spent; the
  next session starts a fresh one). Nothing committed, no APK.
- Shots C:/SH3Quest/shots/vr145-vr164, logs C:/SH3Quest/vr145-164.log, build_pc_122-137.log, regen_127.log: all on
  C:, waiting for the owner's go before deleting (with session 9's sounds133/135, first_shell.txt and the clang crash
  files).

## The owner's messages since session 9's handoff

- 36 (to session 9): "work through everything and dont stop. proceed with list".
- 37 (to session 9): "silent hill 3 has a health and stamina bar bru. proceed with list. verify this kinda shit".
  Session 9 had said "one indicator, no stamina": wrong. The lesson: verify a claim about the game (its code and a
  picture) before stating it.
- 38 (session 10): "not sure how it works when enemies grab you, but see about adding an option to shake hands or
  something to break out of holds. on by default like all other vr features of course. proceed with list."
- 39 (session 10, with vr152's picture: the game's bars at the top left beside the watch's): "be sure the bars in the
  top left arent visible when the wrist watch bars feature is enabled (which it should be by default/ when feature
  unlocked of course)".
- 40: "when they switch to third person, all hud features should switch to normal lol. but ofc if they switch back to
  1st person in game then it would go back to wrist ... these kinds of things should be obvious man. vr QOL in mind."
  The lesson (memory feedback-hud-per-view): decide every replaced HUD element per view mode, unasked.

## Done after session 9's handoff (session 9, builds 122-125, runs vr145-vr151)

- vr145 (build 121, wrist.txt): the wrist's health bar on the watch's face, 4132 pixels in the left eye at 40%, shown
  0.99 while the back of the wrist faced the eyes (0.94) and was looked at (0.97); faded when turned away.
- vr146 (scenes.txt, dynamic cutscenes): her eyes while the scene showed her (frame 1935, 1.7 m), the camera
  elsewhere. Still open: at 5280 she is drawn but her head's place was 240 frames old, so it kept the camera.
- Builds 122-124, vr147 and vr149 (SH3_HIT_TEST=frame:damage, a hit 1 m ahead at chest height; its kind word set to 4
  as the game's own direct-damage call does, or the damage step skips it): blocking checked: health 100 -> 85 guarded
  by the gun, 85 -> 55 unguarded, 55 -> 37 and 37 -> 19 guarded by the forearm (the guard keeps 0.5 and 0.6). The
  across-the-face forearm guard added to tools/sim/block.txt.
- vr148 (build 123, SH3_LIFE_DISPLAY=1, health 40%): the game's own red "+" cross drew in the lower part of the eye
  pictures (red pixels at 195-968 x 851-992): to the panel, as the 0x5A5E40 bars now are, or left off in first person
  where the wrist shows health (open, below).
- vr150 (build 125, SH3_PLAYER_DIFF): the per-frame changes in her work struct; +0x188 is her health in percent, not
  stamina.
- vr151 (weapons.txt on build 125): 17 hand sounds played (holsters, the pouch, magazines, the slide); grips plausible.
- Session 9's last, unfinished edit: the stamina in Sh3VrGame (sh3vr.h), WristLayer (xr.h), xr_sim.cpp and
  xr_openxr.cpp's key. Session 10 finished it.

## Done in session 10 (builds 126-128, runs vr152-vr154)

- **The game's Life Gauge and stamina, verified** (GAME_FACTS "Combat"): 0x5A5E40 draws a yellow health bar and a
  green stamina bar at the screen's top left (stamina = 1 - tiredness 0x712C620 / its most 0x712C624; she breathes
  hard past 30% tiredness). Shown while byte 0x70E6724 is 1 (from the options menu's copy 0xBCC1B2; which Extra option
  name it is, not yet pinned). vr152 (SH3_LIFE_BARS=1 SH3_HP=40 SH3_STAMINA=60): the game's bars at the top left of
  both eyes (1287 yellow and 1287 green pixels in each eye's top-left 500 x 120).
- **The wrist has both bars** (LIST 49; src/vr/wrist.h): health on top (yellow, red under a quarter, the ticks and the
  pulse), stamina under it (green). vr152: both readable, 40% and 60% as set.
- **The game's Life Gauge out of the eyes** (sh3.c wraps 0x5A5E40): skipped in first person while the wrist shows
  health, sent to the HUD panel otherwise (sh3_hud_2d, d3d8.c vr_route). vr153: 0 yellow and 0 green pixels in the
  top-left 500 x 120 of every eye picture with the gauge forced on (it was 1287 and 1287 in vr152). The panel route in
  third person is not yet seen.
- **Shake to break free** (owner message 38; LIST 57; "Shake to break free", Hands and weapons, on): the game's
  struggle step 0x4D44E0 (wrapped in sh3.c) counts newly pressed buttons and directions while she is knocked down or
  held; each counted press takes a frame's time off the time to get free 0x712C608, a fifth of a frame goes by itself,
  she is free at 0 (GAME_FACTS "Combat"). sh3vr_combat.cpp struggle_frame counts each sharp turn of a controller's
  motion (0.75 m/s one way, then 0.3 m/s back, at most one per 0.06 s a hand) as one press, at most 4 a step; a buzz
  and a rustle sound ("struggle shake") for each, a buzz in both hands as a hold starts and as she gets free; the logs
  "VR: ... she is knocked down or held", "held N s ...", "she got free after ...", the crash log's line.
  vr153 (build 127, SH3_STRUGGLE_TEST=7340:1.0): 1.0 s still took it to 0.80 (the game's 0.2 a second), then shaking
  freed her at 2.4 s in all (40 shakes; 5 s with no input); the buzzes and sounds as written.
  vr153 found [0x712C108] = 1/72 (the rig's 72 Hz): a press is worth a frame's time, so at the headset's 120 Hz half
  what it is at the PC game's 60. Build 128: in VR each press (a shake, or a button the step counted, read back from
  what it took off) is worth 1/60 s; the logs count the buttons too. SH3_STRUGGLE_TEST takes a list of holds.
  vr154 (build 128, tools/sim/struggle2.txt, SH3_STRUGGLE_TEST=7340:1.0,7600:1.0): hold 1, A pressed 12 times a
  second: 0.80 -> 0.40 -> 0.00, 0.40 a second = 0.2 + 12/60 exactly (the game alone at 72 Hz: 0.37); free at 3.0 s,
  24 buttons logged. Hold 2, shaking: 28 shakes in a second took 0.67 (0.2 + 28/60); free at 2.2 s, 34 shakes. The
  rig's script frames run 12 behind the game's ("the set-up done at frame 7324: frame 7312 of the script"), so every
  stroke counted (the first only looked late).
- Docs: GAME_FACTS (the gauge, stamina, the struggle, the damage x 1.5; session 9's wrong lines marked), LIST 49, 50,
  57, CONTROLS (the health and stamina row, the get-free row), HEADSET_CHECKLIST (both bars, shake to break free),
  the launcher's whats_new.txt, features.txt, controls.txt.

## Done later in session 10 (builds 129-135, runs vr155-vr160)

- **Stamina and the HUD** (vr155, build 128, stamina2.txt): stamina runs down about 5% a second running (most 2000.0,
  not 1000) and comes back about 7.5% a second at rest. The game's Life Gauge stayed in the eyes in third person even
  with the panel flag: its rectangles are packets (0x474D70 / 0x474EA0 into 0x678900 / 0x678930) drawn later, so it
  cannot be routed at the draw. sh3_hud_2d removed; the gauge is skipped wherever VR shows the world in the eyes
  (sh3vr_life_gauge_skip) and VR draws the same bars itself (show_wrist_bar's `gauge` branch).
- **Dynamic cutscenes** (LIST 48, build 129): scenes pose her model (0xC0E060, 73 bones) away from the player's hidden
  place (-60000, 0, 60000); sh3.c now takes her head from it during a scene (sh3vr_scene_head) and logs each character
  model a scene draws ("scene models:"). vr156: the park scene in her eyes 4195-6709 (1.6-8.9 m off the camera), the
  camera when her face comes up close (1.2 m). vr156 stalled at 10294: the rig's sim_ready was never set when the
  scene ended in first person; build 131 checks it every frame and places the rig anew when first person crosses a
  scene/play boundary ("first person from a scene into play: the rig anew").
- **Third-person bars** (vr157, builds 130-132): VR's bars at the view's top left, following it gently (1892 pixels);
  moved to 22 degrees left, 16 up, 0.34 m wide at 1.3 m (kGaugeAhead/Left/Up/Wide) as the first place sat near the
  middle and small.
- **Dynamic resolution** (LIST 51, builds 131-133): "Dynamic resolution" (Performance, on); Jak's rules
  (dynamic_resolution_frame: a tenth down while displays are missed, to 60%, a tenth up after 5 s calm, 10 s hold);
  the game's own resize 0x402E00 called after each present (sh3_vr_after_present) with game_picture_size following
  pic_w/pic_h; Quest 3/3S start at Render resolution 1.5x (device_defaults, Options.java STRONG_QUEST);
  xr.h displays_missed() (OpenXR counts missed displays, the sim's "late"). vr158 (dynres.txt): 1000 -> 896 -> 800 ->
  696 -> 600 while late (7334-7380), back a tenth each 5 s to 1000 at 8583; the eyes whole at every size. Build 133:
  0x402E00 answers 0 when made (1 only when even the old size failed): the log reads the size back.
- **HUD per view** (owner messages 39-40, builds 134-135): first person with "Health bar on your wrist" on (default):
  the watch only, nothing at the top left, the Life Display cross left out; third person: the game's normal HUD
  (VR's quad of its bars at the top left, the cross on the panel), no watch; back to first person: the watch. The
  option off: the normal HUD in first person too (sh3vr_wrist_health_on is first person && the option). vr159
  (bars39.txt): 0 bar pixels ahead in first person before and after third person, the watch's bars at the wrist, the
  top left's bars and the panel's cross (1929 red) in third person. Build 135: bars only after 0.5 s of play (vr159
  showed the top left's for one frame at 4061 and 4155, single play frames between the opening scene's shots).
- **Scenes whole** (vr160, build 135, scenes.txt with the gauge forced on): no bars in the scene's play frames (the
  0.5 s wait works); the pictures whole, the dark ones as the game's own (vr146: the same). The bridge walk in her
  eyes stood them right behind two fence posts (fixed in the picture at 2880 and 3350; vr161 the game's camera there,
  dark; vr162 SH3_VSC_DUMP=2880: her own draws cut and faded as meant, so the posts are the world).
- **The world right before her eyes in a scene** (builds 136-137): sh3vr_body.cpp kView probes (11 ways, 12 and 25
  cm, only while a scene is in her eyes; her column not counted as her, only her drawn arms and hands),
  view_blocked(); sh3vr.cpp scene_first_person: under 20 cm for 6 frames running -> the game's camera until her head
  moves 35 cm; logs. vr163 (build 136: five ways, two needed) missed the posts; vr164 (build 137): "the world 6 cm
  before her eyes (6 of 21 probes)" at 2328, the camera through the fence stretch, the park scene still in her eyes
  at 4200 (its picture whole).
- Docs: LIST 48, 49, 51; HEADSET_CHECKLIST (dynamic cutscenes 1.5-20 m into play and never with the world right
  before her eyes, performance with dynamic resolution and 1.5x, the HUD per view); CONTROLS; GAME_FACTS (gauge
  packets, stamina most and rates); the launcher's whats_new.txt, features.txt, controls.txt and the wrist option's
  description; notes messages 39-40.

## Open, in order

1. (vr160-vr164 done, above.) A scene check for the next session: tools/sim/scenes.txt past 5400 to the end of the
   opening (into play), with the near probes, the pictures whole.
2. World items: drawers, handles, doors (ask 28): the curl probes close fingers on any surface; pictures at a door's
   handle and a drawer still to take.
3. LIST 56: her head and gestures in her shadow. GAME_FACTS "Bones": the bone array changed between 0x45EC00 and
   0x45EC90 shows in her shadow too, so the IK's head and arms should already be there: to see in a picture where a
   room light throws her shadow. Her hand in her own flashlight's beam casts none (her draws are not in its pass).
4. Grips (LIST 52): the pipe's and maul's left, the submachine gun's left, the shotgun's pump hand; thumb and fingers.
5. Weapons against the world; the Alyx finger feel on weapons (LIST 53).
6. The handgun slide drawn as its own faces; the forearm twist.
7. tools/sim/weapons.txt again, every picture whole.
8. LIST 5 movies, 6 speed, 12 inventory hit-testing, 22-24, 29, 31, 38.
9. The APK, last: only once everything is tested and refined; path and SHA-256 to the owner; publish nothing.

## Files changed in session 10

- src/game/sh3.c: the 0x4D44E0 wrapper (shakes in, each press 1/60 s in VR, the buttons counted), the 0x5A5E40
  wrapper (sh3_hud_2d), recomp_lookup_manual's two cases; SH3_LIFE_BARS, SH3_STAMINA, SH3_STRUGGLE_TEST (a list);
  g.stamina.
- tools/build_quest.py: sub_004D44E0_gen, sub_005A5E40_gen in the wrapped tuple (regen_127: 11977/11981, 0 failed).
- src/d3d8/d3d8.c: sh3_hud_2d to the panel in vr_route.
- src/vr/wrist.h: rewritten (two bars, wrist_bar).
- src/vr/sh3vr.cpp: the wrist's stamina, its log, the "Shake to break free" row, sh3vr_struggle_presses /
  sh3vr_struggle_done(freed, buttons), struggle_crash_info. src/vr/sh3vr.h: their declarations.
- src/vr/sh3vr_internal.h: opt.shake_free, kSoundStruggle, the struggle declarations.
- src/vr/sh3vr_sounds.cpp: "struggle shake". src/vr/sh3vr_combat.cpp: the shake-to-break-free section.
- src/vr/xr_openxr.cpp: m_wrist_key a long long.
- android/.../Options.java: the "Shake to break free" row.
- tools/sim/lifebars.txt, struggle.txt, struggle2.txt (new).
- docs as above; this handoff.
- Later (builds 129-135): src/game/sh3.c (the scene head, "scene models:", pic_w/pic_h, sh3_vr_after_present, the
  0x5A5E40 skip, g.life_gauge; sh3_hud_2d gone); src/d3d8/d3d8.c (sh3_vr_after_present after the present; the
  sh3_hud_2d route gone); src/vr/sh3vr.cpp (show_wrist_bar's HUD per view and the 0.5 s wait, the gauge quad,
  sh3vr_wrist_health_on, sh3vr_life_gauge_skip, sh3vr_picture_scale, device_defaults, the "Dynamic resolution" row,
  dynamic_resolution_frame, the scene/play rig reset, sim_ready each frame); src/vr/sh3vr.h, sh3vr_internal.h,
  sh3vr_ik.cpp (ik_scene_head), sh3vr_combat.cpp (struggle_buttons); src/vr/xr.h, xr_openxr.cpp, xr_sim.cpp
  (displays_missed); android/.../Options.java (STRONG_QUEST, the dynamic resolution row, the wrist option's text);
  tools/sim/gauge3p.txt, dynres.txt, bars39.txt, bridgecam.txt, bridge.txt, scenes_near.txt (new), stamina2.txt;
  src/vr/sh3vr_body.cpp (kView probes, view_blocked) and sh3vr.cpp scene_first_person (builds 136-137).

## The owner's rules (unchanged)

As SESSION9.md: the full list with states in each closing message; PC test builds approved, one at a time, never while
sh3.exe runs; short checks (2-3.5 minutes a run, about 30 minutes in all; say the time first); no `python -` heredocs
or stdin python; Edit tool or plain sed; no commit or publish unasked; files on C: wait for the owner's go; plain option
names; every VR option on by default (film grain off); never box hands; check every picture whole with numbers; a sound
for every VR action; crash logs explain themselves; downloads need permission; the APK only when everything is tested
and refined; verify claims about the game before stating them (message 37); never stop on my own, keep working down
the list until usage runs out (message 41; memory feedback-never-stop).
