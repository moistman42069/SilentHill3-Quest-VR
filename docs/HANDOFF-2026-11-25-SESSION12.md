# SH3 Quest VR: handoff, session 12 (2026-11-25)

Read docs/HANDOFF-2026-11-25-SESSION11.md first (the state before, the owner's rules); this one carries on from it.
PC test builds up to 164 (build_pc_164.log, BUILD OK); runs up to vr189 (the session's run budget spent). Next build
165, next runs vr190 and vr191 (on build 164). Nothing committed, no APK.

## START HERE: the owner's last three messages (53-55) set the next session's first job

- 53: "so youve almost worked through and coded EVERYTHING on the list? just be sure you have before building an apk
  for me to test." 54: "ive added lots of requests and worked thtrough lots of different sessions so be sure that
  nothing is missed here." 55: "wrap up and do a handoff."
- So, before any APK: a full audit, item 0 of "Open, in order" below. Answer given to 53: not yet (builds 160-164
  unrun; LIST 24's extra weapons still in her animated hand; the OPTIONS rows and the door need vr190/vr191's data).

## The owner's messages this session (C:/SH3Quest/notes/owner-messages-sessions1-7.txt, 48-55)

- 48: "pick up from where it left off. proceed with list. miss/forget nothing. show everything in the on screen list".
- 49: "see about making sure heathers full body ik is also taken into account for the games stencil shadows. could be
  a cool addition anyways."
- 50: "just be sure all objects like weapons, melee weapons, etc, also are taken into account for shadows."
- 51: "pretty sure theres body shadows for her in the normal game at SOME points, maybe that can be used as a base
  somehow? ... you can research online and see if theres any existing mods or adjustments or simlar vr/game
  implementations that can help you out ... be powerful".
- 52: "be sure "body ik shadow" is an option that can be toggled on and off or something in the in game menu."
  Checked in the code: "Heather's shadow" (sh3vr.cpp's option table: View page, a switch, not next_start, so it
  acts at once; sh3vr_her_shadow returns nothing while it is off), the launcher's Options (Options.java, View, on),
  sh3vr.ini key her_shadow, on by default (sh3vr_internal.h). whats_new.txt and HEADSET_CHECKLIST.md now name the
  VR menu's View page.

## Done in session 12

- Build 153, vr181-vr182: the movies in the game (LIST 5): they play on the cinema screen with sound; A, B and the
  triggers skip one (map_pad pulses the game's back button while a movie plays).
- **The game's shadows, read** (builds 154-155, vr183, vr185, vr186; GAME_FACTS.md "Shadows", session 12 part): stencil
  volumes counted from 16 (clear 0x48B2B0), darkened at 17 and over (0x48D474, LESSEQUAL 17 mask 0x1F) through a mask
  target and 256x256 blurs. 0x685C10 draws two lists: the characters' (0x4D1020: only rooms whose lights are shadow
  lights, 0x7F4C18/0x7F4C7C; kg1 shadow models, Heather's data/chr/pl/chhdd.kg1 and chhcc.kg1, weapons wp_*.kg1) and
  the map's (0x687F00). The start room has no shadow light (the characters' list empty; forcing 0x821DDF does not make
  one, SH3_CHAR_SHADOWS). Her draws use the stencil themselves (0x461462: ALWAYS, REPLACE 64). The game's character
  shadow keeps the animation's pose (its walkers never call 0x45EC00, where VR's IK writes her bones).
  Research (message 51): GameSpot's PC preview (her animated shadows "lengthen and deform" in the light), Nexus SH3
  model mods 73 and 77 (their shadows wrong unless the shadow files are redone: the shadow is the kg1, not her drawn
  parts), zealottormunds' sh3cammod (README: realtime shadows off in first person), TCRF (the kg1 loads). No mod draws
  her shadow from a VR pose: ours is new.
- **Her shadow from VR** (builds 156-158, vr184, vr185, vr187; LIST 56): sh3vr_ik.cpp her_shadow_tris builds rounded
  volumes (a 10-point stadium across the light per capsule, the palm a quad) for her IK body (collar, arms, palms,
  each finger's three bones, legs, feet, pelvis, chest, neck, head), the weapon held (weapon_capsules without the
  flash, every gun and melee weapon) and a reload part in the hand, from the flashlight's lamp (combat_beam), 8 m long,
  left out when the lamp is at or in one, over 3 m away, outside the 50-degree cone, or the eyes inside it. d3d8.c
  vr_her_shadow draws them just before the darkening's stencil-on (0x48D474), with the volume pass's camera, INCR front
  / DECR back, skipping pixels marked 0x20 (hers: sh3.c's 0x45ED40 wrapper marks her draws through d3d8_her_mark; the
  held weapon through shader_held_draw; build 158 ORs 0x20 into the game's own REPLACE 64). Option "Heather's shadow"
  (View, on), VR menu and launcher. vr187: 1428 triangles (41 casters, 8 left out), her arms lit as before, her legs'
  shadows rounded at her feet, her raised hand shading the beam's middle on the floor ahead. A room with the game's
  own character shadows logs "the game's own character shadows are on here" (HEADSET_CHECKLIST: look for doubles).
- **LIST 12** (vr187, tools/sim/shadow_screens.txt): the inventory's top tabs take the pointer: on Maps the first pull
  moves the selection, the second chooses (the game's "I don't have a map of this area."); on Memos the second pull
  enters the tab (no memos yet at the start). The game's own two-click mouse model, kept (CONTROLS.md). B in the
  inventory closes it (the script's B after Memos closed it before Options: removed; Options moved to weapons.txt).
- Docs: GAME_FACTS (Shadows), LIST 56 and 12, HEADSET_CHECKLIST (her shadow; the game's character-shadow rooms),
  CONTROLS (the two pulls; build 163: the dot and tick), whats_new.txt (her shadow; the stale "movies are skipped"
  line removed; the pointer), README.md (the features since session 4: her body and shadow, the flashlight, the watch,
  blocking, shake to break free, the movies, dynamic resolution; the latest handoff).

## Open, in order (next session: build 164 is built and not yet run)

0. **The full audit (owner messages 53-54), before anything else and before any APK.** Read every owner message in
   C:/SH3Quest/notes/owner-messages-sessions1-7.txt (its USER lines; about 85 KB, raw session logs with task
   notifications between them: skip those) and the session summaries beside it (session5/6/7 files), plus the
   handoffs' owner-message sections (sessions 8-12). For each request: its LIST-0.1.0.md item, whether the code does
   it (name the function), whether a run or a picture showed it, what is left (a rig run, the headset, or code). Write
   it as docs/AUDIT-2026-11-25.md (one row per request), then code every gap in one batch and one build.
   Found so far (session 12, before the stop):
   - LIST 24, the extra weapons: slots 9, 10, 13, 14, 17 (items 5, 6, 7, 8 and 13 by 0x6BDD4C's table; which is the
     beam saber, flamethrower, unlimited submachine gun... is not known) have no mesh signature, so
     combat_weapon_draw (sh3vr_combat.cpp) leaves them in her ANIMATED hand, not at the controller (whats_new.txt says
     so). Plan: one rig run with SH3_GIVE="5:0,6:0,7:0,8:0,13:0" SH3_EQUIP="9,10,13,14,17" SH3_EQUIP_EVERY=150
     SH3_WEAPON_BONES=1 (diag_draw logs each skinned draw's signature and palette) and a picture of the right hand per
     extra; then each gets a WeaponInfo/hold (or, where no hold is measured, a generic carry: her animated hand bone to
     her IK hand bone, B' = IK * inverse(anim) * B, as ik_chest_carry does for the chest lamp).
   - Stale notes in LIST-0.1.0.md's "Written" section from session 3 (28: the holster layout "not written"; 34:
     spatial sound "still to do"; 46: the reload module "NOT written"; 43: "not yet looked at in pictures"): each was
     done later (README, CONTROLS, HEADSET_CHECKLIST say so); correct them in the audit. 22's was corrected.
   - The game's own mouse arrow may now show at the pointer's dot (item 1 below).
1. One combined run first: tools/sim/combined190.txt (written, about 3.4 min; its header has the switches, with
   SH3_MEM_SNAP="7990,8020,8045,8075") on build 164: its pictures against vr187/vr188's like ones show build 160's
   uniform caching (LIST 6) draws the same; sh3.log's "the game's click zones" and "a pull at the game's cursor"
   lines show which OPTIONS rows and arrows a pull can choose and what each pull was over (builds 161-162 below); the
   "memory snapshots" lines with steps 1, 1 are the OPTIONS screen's highlighted row (for any row without a zone:
   our hit-test then sets it); and
   - the shotgun's and the submachine gun's two-hand holds where the pump hand can be seen (vr188 and vr189 missed
     it; head_yaw -20 looks right);
   - the handgun's slide on a shot (pictures every frame round script 7798);
   - an Options entry chosen by hovering it ("Exit", one pull: back to the inventory, or the click confirms the
     highlighted "Button Config"); the page arrows if the game's mouse never takes them (LIST 12);
   - the game's own mouse arrow: build 162 keeps its cursor shown while VR points, so the game may now draw its arrow
     at the dot (0x600BF0 tail-jumps to 0x4DD300, which stores the place for 0x4DD710; its only caller). If the panel
     pictures show it beside the dot, skip 0x4DD300 while VR's pointer is on (VR's dot is the cursor); vr189's panel
     (v08030) shows none.
2. World items (LIST 45): a hand on event 18 with tools/sim/door191.txt (about 2.5 min, build 164's SH3_STEER_TO walks
   her round what is in the way to -16400 22600; its header says why event 18). vr189's straight walk to event 20
   stopped at the striped railing (z 19129). A room-changing door and a floor item: the playthrough if 18 is not one.
3. LIST 12, the rest: the save screen, puzzles, a memo once she has one (the playthrough, HEADSET_CHECKLIST).
4. LIST 6 (speed on the Quest: measure), 22 (fingers on a world object's own shape), 23 (the radio's crackle with a
   monster near), 24 (the extras' signatures), 29, 31, 38: the playthrough on the headset (HEADSET_CHECKLIST.md); her
   shadow in a room with the game's own character shadows (could VR also cast her IK volumes from the game's shadow
   lights and hide the game's kg1 shadow of her? 0x7F4C7C's light data first; needs a save in such a room).
5. The APK, last: only once the audit (item 0) shows everything coded and the rig runs (items 1, 2 and the extras)
   pass; then the one build pass (tools/build_quest.py), its path and SHA-256 to the owner for the headset test
   (HEADSET_CHECKLIST.md); publish nothing.

## vr189 (build 159, tools/sim/events_door.txt)

- The chest lamp fix: "the chest lamp rides her chest as drawn: 0.02 m from the game's place" (standing); her
  shadow's nearest caster now her head, 0.24-0.25 m from the lamp (its shadow falls up and back); the knife's
  look-down picture (v07622) shows no shade of her own body in the beam's middle. 38 body casters, 7 of the shotgun.
- Options opens with the pointer (v07950-v08200): pull 1 selects the tab (arrows), pull 2 opens OPTIONS, the pointer
  on it; pull 3 on the page arrow opened Button Config (the highlighted entry): the game's click confirms the
  selection; its arrows are not hover targets.
- The door walk stopped at z 19129 (the striped railing before Robbie): the door not reached (item 2 above).
- The shotgun's two-hand picture (v07356): only the left arm crossing the view. The sim's yaw is positive to the
  LEFT (shadow_screens.txt: hand_yaw 16 = Maps, the left tab), so head_yaw 25 looked away from the gun.
  tools/sim/combined190.txt (written for the next run, item 1) looks 20 degrees right with head_yaw -20.

## Builds 161-162 (after the run budget; LIST 12, owner message 52)

- Build 161: sh3.c SH3_MEM_SNAP=A,B,C,D (game frames; SH3_MEM_SNAP_RANGE=lo:hi hex, the exe's .data 0x6B0000 to
  0x717A900 by default, 112 MB): copies at A, marks what changed by B as noise, keeps what changed by C that is not
  noise (up to 4096), logs at D those that changed again with their B, C, D values and steps. combined190.txt part 5:
  two down presses on OPTIONS (left_y -1 at script 8050 and 8080) between the snapshots.
- The game's click zones (GAME_FACTS.md "Click zones", "The cursor's shown flag"): screens register zones each frame
  (0x601970), a click acts on the zone under the cursor (0x601A40); hovering moves nothing. The game hides its cursor
  3 s after it last moved and a hidden cursor's click finds no zone.
- Build 162: sh3.c keeps the game's cursor shown while VR's pointer is on a screen (0x70E6418 = 1, deadline
  0xFFFFFFF0; hidden when the pointer goes), reads the zone list each frame (heap nodes checked against
  rt_heap_used) into Sh3VrGame click_zones / click_zone, logs each new set of zones (up to 40) and each pull's cursor,
  beam and zone (up to 60). update_pointer ticks the hand (0.18, 14 ms) on entering a zone where a screen has zones,
  the old 16 x 24 grid tick elsewhere. Both BUILD OK, not run.
- Build 163: the pointer's dot grows by a third over a click zone, easing over about 0.1 s (xr.h PointerLayers
  dot_scale; xr_openxr.cpp sizes the dot's quad, its beam/dot test now explicit; the sim logs "the pointer's dot grown
  over a click zone"). BUILD OK, not run.
- Build 164 (LIST 45): sh3.c steer_rig, SH3_STEER_TO=x,z[;x,z...]: while the stick is held her walk heads for each
  point, 70 then 90 degrees off the line for 45 frames when stuck (under 25 units in 40 frames), she stands at the
  last; "steer test" lines. tools/sim/door191.txt (new). BUILD OK, not run.
- Owner message 52: "Heather's shadow" was already a VR menu switch (View page, acts at once) and in the launcher;
  whats_new.txt and HEADSET_CHECKLIST.md now say where.

## Build 160 (after the run budget)

- LIST 6: uniform uploads by change only (d3d8_shader.c uniform_changed; per program: c[96], pc[8], uTexM[8], uEye[2],
  the material, power, ambient and lights as one block); uPower / uLtSpecular / uLtAmbient looked up once. BUILD OK;
  not run yet (item 1).

## vr188 (build 158, tools/sim/weapons.txt with the session-12 additions)

- The script's frames run about 24-26 ahead of the game's here (the set-up at game 7288 = script 7312; the pouch grab
  at script 7362 logged at game 7336): the trigger at script 7316 fired at game 7328 (script about 7354), after the
  slide pictures w07332-w07338, so the slide's cycle was not caught. To catch it: pictures at script 7352-7360.
- The weapons cast: "Heather's shadow ... 4 of the weapon" (the handgun, frame 7286), "3 of the weapon" (the katana,
  8540, the dump frame). The pipe's and the maul's shadows show on the floor beside her (wB188 sheet).
- Grips (LIST 52): the handgun in both hands (the left wraps the right) and the katana (both hands stacked on its
  hilt) look right. The shotgun's and the submachine gun's two-hand pictures have the gun at the picture's right edge
  (the pump hand not judgeable): vr189 turns the eyes 25 degrees right. The log's native grips: the shotgun's left
  hand 15.8 cm from the measured pump hold, the submachine gun's left 12.6 cm (the game's own left hands, read).
- LIST 53: the maul walked into the brick wall (9156: "the right hand is held at a surface (the arm's line met it)"),
  pushed 35 cm on: the hand stays at the wall, its ghost shows the controller (36 cm past, 9243), the handle along
  the wall; her arms' and the maul's shadows on the bricks. "steel pipe held at a surface" (8284), "maul ..." (8686).
- FOUND: a U-shaped shade in the beam's middle on the floor ahead with both hands out to the sides (w07338 with the
  handgun, w08140 with the knife): her trunk's capsule (13 cm round the spine) stood ahead of the chest lamp in the
  game's weapon stances: the lamp was the game's own place (0x712C1D0, its animation's chest), not her chest as VR
  poses it. FIXED in build 159: ik_chest_carry (sh3vr_ik.cpp) carries the game's lamp place from the game's chest bone
  to VR's (her lean and crouch move the lamp, its light and her shadow's lamp together; logged "the chest lamp rides
  her chest as drawn"), and with a chest lamp her trunk, upper chest and collars cast nothing (a held flashlight keeps
  them); sh3.log names the caster nearest the lamp ("her shadow's caster nearest the chest lamp: ..."). Checked in
  vr189 (v07622).
- Big shades when the left arm crosses before the chest lamp (the shotgun's and submachine gun's two-hand holds) are
  right: an arm before a chest light.
- Options (LIST 12): the first pull came before the inventory had opened (about 70 frames after Y at the wall), so one
  pull counted: the selection on Options, its arrows; the screen did not open. vr189 gives it 90 frames and 3 pulls.

## Commands

- Build: `cd /c/SH3Quest/SH3QuestVR && timeout 900 cmd //c "tools\\build_pc.bat" > /c/SH3Quest/build_pc_N.log 2>&1`,
  then grep "BUILD OK". One at a time, never while sh3.exe runs.
- Run: `mkdir -p /c/SH3Quest/shots/vrN`, then from /c/SH3Quest/gamedir with QUEST_SIM_HEADSET=1 QUEST_SIM_ALIGN=7312
  QUEST_SIM_GRIP_TILT=50 SH3_KEYS="22:Return,25:Return,28:Return,31:Return,34:Return" plus the script's own switches
  (its header), QUEST_SIM_SCRIPT=C:/SH3Quest/SH3QuestVR/tools/sim/X.txt QUEST_SIM_OUT=C:/SH3Quest/shots/vrN:
  `timeout 330 /c/SH3Quest/SH3QuestVR/build/pc/sh3.exe C:/SH3Quest/gamedir > /c/SH3Quest/vrN.log 2>&1`.
- While a screen is up the eye pictures are black: look at the "-headset-left" pictures (the panel).
- Scratchpad tools (this session's): darkdiff.py (where one picture is darker than another), sheet.py, crop.py,
  strfind.py.

## Files changed in session 12

- src/d3d8/d3d8.c: the stencil-on log and the shadow-list dump (SH3_VSC_DUMP's frame), the character-shadow notice,
  the volume pass's camera kept (draw_setup), the her-mark (draw_setup's end), vr_her_shadow, d3d8_stencil_on,
  d3d8_her_mark. d3d8.h, d3d8_shader.c: shader_held_draw; d3d8_shader.c (build 160): uniform_changed, the cached
  uniform locations (LIST 6).
- src/vr/sh3vr_ik.cpp (her_shadow_tris; build 159: ik_chest_carry, her trunk left out for a chest lamp, the nearest
  caster's log), sh3vr_internal.h (option, weapon_capsules' with_flash, declarations),
  sh3vr_combat.cpp (weapon_capsules with_flash; build 159: the chest lamp carried, combat_light_held), sh3vr.cpp
  (the option, sh3vr_her_shadow, the movie skip in
  map_pad), sh3vr.h.
- src/game/sh3.c: the her-draw log and d3d8_her_mark in the 0x45ED40 wrapper; SH3_CHAR_SHADOWS (rig test); build
  161: SH3_MEM_SNAP; build 162: the cursor kept shown while pointing, the click zones read and logged. sh3vr.h:
  Sh3VrGame click_zones / click_zone; sh3vr.cpp update_pointer: the zone tick.
- android Options.java ("Heather's shadow"), assets/whats_new.txt.
- tools/sim/shadow_screens.txt (rewritten), weapons.txt (the slide shot, the wall, Options), events_door.txt (new:
  the shotgun two-handed, the knife look-down, the door, Options), combined190.txt (new: two-hand holds, the slide,
  Options with the snapshot presses), door191.txt (new: event 18 by hand).
- docs: GAME_FACTS.md, LIST-0.1.0.md (6, 12, 23, 45, 52, 53, 56), HEADSET_CHECKLIST.md (her shadow, the game's
  character-shadow rooms, the chest lamp, the radio's crackle, the click zones), CONTROLS.md, this handoff;
  C:/SH3Quest/notes/owner-messages-sessions1-7.txt (52).

Waiting for the owner's go (files on C:, never deleted without it): the shots folders C:/SH3Quest/shots/vr* and the
vr*.log, build_pc_*.log and regen_*.log files from sessions 1-12; C:/SH3Quest/dl_plmpeg.

## The owner's rules

As in SESSION11.md's last section (unchanged): never stop on my own; speed (code in batches, one build, short runs);
runs about 30 minutes a session (session 12 used about 30: vr181-vr189); no `python -`; Edit or sed, LF; no commit,
push or publish unasked; downloads only with the owner's permission; nothing on C: deleted without the owner's go;
plain option names, every VR option on (film grain off); never box hands; the ghost hand only in a surface; HUD per
view; a sound for every VR action; crash logs that explain themselves; check every picture whole; verify game claims;
the APK only once everything is tested (path + SHA-256, never published unasked); credit every author; each closing
message shows the whole list with states.
