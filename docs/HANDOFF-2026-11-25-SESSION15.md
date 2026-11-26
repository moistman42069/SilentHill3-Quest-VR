# SH3 Quest VR: handoff, session 15 (2026-11-25)

Read docs/HANDOFF-2026-11-25-SESSION14.md first (its commands and rules); this one carries on from it.
PC test builds 177-195 (build_pc_195.log, BUILD OK); runs vr206-vr229; the first APK (below). Nothing committed or
published.

## The owner's messages this session (C:/SH3Quest/notes/owner-messages-sessions1-7.txt, 67-71)

- 67: pick up where session 14 stopped, forget/miss nothing, proceed with the list.
- 68: "use some form of online reference if needed like ive advised you before".
- 69: "if there were any mods or anything that already touched the mesh": look for existing work on the beam saber's
  mesh before spending more runs. Searched (below): none touched it.
- 70: "we confirmed that doors, drawers, etc, work wll right": confirm the world's interactions by hand in the sim.
- 71: "be sure everything on the list is coded in, works, and is good": every item checked working where the sim can
  show it (a floor item by hand, melee on monsters).

## Interactions by hand, checked in the sim (owner 70-71; builds 191-195, runs vr220-vr229)

- A real door, both ways, by the left hand (vr222, tools/sim/door223.txt): the opening's entrance door is EVENT 5
  (region at -13157 22453; the tall door, event 18, is behind the stone rail at z 22118 and cannot be reached: vr220,
  vr221 stuck there). SH3_STEER_TO="-14500,21000;-13700,22500;-13406,23244" stands her at -13406 23244 facing +x; the
  left hand held ahead (lh 0.2 0.1 -0.3) and closed: "the left hand used event 5 at -13070 -626 23224", the game's
  door and the next area (the circular hall, room 1, at 15536 20000). There the hand used EVENT 4, the same door
  back ("the left hand used event 4 at 15360 -624 19887"), and she stood again at -13457 23200 in the first area.
  vr223 did the door again from the script alone.
- Floor items: the nightmare stage has NONE (sh3.c's new room log: "floor items: 0 in the room, 0 not taken" in both
  areas). The first ones are in the mall (silenthillmemories.net): the Ladies' Room of Happy Burger (the save point
  on the mirror, out by the window), the alley (its far door), the office hallway (double doors at its end), then the
  first-floor shop under the half-lowered shutter: the handgun on its floor, two packs of bullets on its green bench.
- The circular hall (area 2): vr224 walked straight on (+x) and slid along the hall's curved railing to 17139 19235
  (the steer rig's stuck tries could not leave it). Its events decoded (sh3.c's table log, the region ops: 0x02 and
  0x04 are x float, s16, z float, s16; 0x11-0x14 x, z, 2 bytes; 0x15 x, z, 4 bytes): doors are type 0x0B / 0x06 in
  the event's second word, a pair per door (the same third word): event 4/5 (BE38, the entrance), 46 at 22443 23959
  (north wall) with 45 at -21000 -23750, and 48 at 24809 17840 with 47 at 16563 -19640 (the next area, events 58-78).
  The examine spots 49-57 line the hall's walls. A walkthrough (hxchector.com): in the circular hall turn right and
  follow the corridor round to a yellow door, then past a Closer through a gate, left up the stairs, the low gate onto
  the coaster's tracks, the cutscene, the mall. vr225 (tools/sim/door225.txt) steers round by the south.
- How a door's region works (the game's own test, read from sh3.exe: 0x4B9840's byte table at 0x4B98D8 sends the
  button's regions by their op: 0x01 / 0x10 a point, 0x4B7600; 0x02-0x0C and 0x11-0x1B 0x4B8780; 0x0D-0x0F and
  0x1C-0x1E 0x4B78B0): op 0x12 is a corner (x, z) and one 2-byte step (decoded by 0x4B6C30): event 5 is a line at
  x -13157 from z 22453 to about 23950, event 48 a line at x 24809 from z 17840 toward +z. The door's face stands
  about 0.8 m before the line, so the hand (as the button) finds it from about 1 m away.
- vr226 (build 194, door226.txt, the hall at 4000 units from its middle 20300 19200): round the hall by the south
  (points 5-10 reached; the first passed as unreachable) to the far door (event 48: the brown double door with long
  bar handles, left of the souvenir shop's window; pictures j10000, j10300, j11200). At frame 10267 her hand met it:
  "the left hand can use event 48 (closing it uses it)" at 24323 18169, but no grip fell in that 13-frame window and
  the stuck tries moved her off (the last point was 24350 17840, on the door's edge). door230.txt (ready, not run)
  puts the last point at 24330 18200, where the hand met it, so she stands there and the next grip uses it.
- vr227 (build 194, door227.txt): the hall's Closer met her at its start (game frame 8303) and held her there all run
  (2782 hits; health held): she never got round. Its knife circles (a step every 3 frames) never ended as swings (the
  hand's smoothed speed never fell), so they logged nothing and hit nothing: the Closer's body stood past the knife's
  reach (its arm hits came from 55-70 cm). LIST 50 checked against it (below).
- The dog (vr225, vr226: one bite, kind 122, 5 damage, game frames 8655-8668, from her left) and the Closer (kind
  120, 0.2 a frame; kind 121, 2.0) come at different times run to run: the runs are not frame-exact.
- Melee on a monster (LIST 27) is NOT yet seen in any run: no "took N damage from her" line has ever been logged.
  Build 195 adds SH3_STEER_AT_ATTACKER=1 (below) and door229.txt swings the knife across in front of her every 10
  frames (the pattern that registers: vr226 "the right knife's swing sent 5 sweeps (the hand's travel 0.59 m)"),
  100 frames chest-high, 100 low. vr229 was stopped at its frame 3900 for this handoff (nothing to read).
- The short way to the mall's floor items (hxchector.com: dying in the nightmare skips it): door228.txt (ready, not
  run), SH3_HP=1 so the dog's first bite ends the nightmare, then the left menu button (the game's pause, which skips
  a scene) every 200 frames, pictures between, the mall's tables logged as its rooms load.
- LIST 50, physical blocking, checked against a REAL monster (vr227): of the Closer's 2782 hits on her, 2451 were
  guarded: 2419 by the knife (a third kept, the log rounds it to "0.0 of 0.2"), 31 by her right forearm (0.6
  kept), its one 2.0 blow by the knife (0.7 kept); 331 came from off the guard's line.

## Done in session 15

- LIST 45, Robbie / event 8 by hand (vr206, SH3_STEER_TO="-17386,21386;-17560,21570"): DONE in the sim. She stood at
  -17497 21507; the left hand's ray (reach 488, then the game's 500) met event 8 at frame 8196 ("the left hand can use
  event 8", the light tick), the closing hand used it ("the left hand used event 8"), and the game's examine text
  ("It's a giant rabbit costume. Why would this be here?") showed by the hand (i08436). Event 12 the same.
- Owner 61, the beam saber's green bar: FIXED in the sim (builds 186-190, vr216-vr219).
  - The cause: the BLADE is a skinned draw of its own (24 vertices / 30 indices, one palette slot = model bone 1, its
    x row 0 while off, stretched as the beam extends), drawn just before the hilt (126/154) and with no signature of
    its own, so nothing moved it: the lit blade stayed at her animated hand by her hip and crossed the view.
    Found with SH3_SABER_BONES (vr216: every skinned draw's palette while the saber is out). Earlier runs missed it
    because the draw is there unlit too (a zero-width blade), so the lit/unlit route dumps (vr212) never showed it.
  - The fix: sh3vr_combat.cpp saber_blade_draw: that draw (weapon 9, 24/30, one slot, model bone 1, its bone within
    400 units of the game's hilt) takes the hilt's map, built from this frame's hold (hold_matrix: H x inverse(the
    held bone in the weapon) x inverse(the game's weapon)), so it does not lag the hilt; its stretch is kept;
    holstered, it collapses. Marked as the weapon's draw (faded near the eyes as the weapon is, not as her body),
    and as a glow (sh3vr_weapon_draw returns 2): its pixels kept from the game's shadows as the weapon's are, but no
    part of her own-mesh shadow (a beam of light casts none; build 191, compiled, not run).
  - vr217 (pictures C:/SH3Quest/shots/vr217, tools/green_px.py): the bar's 19,496 green pixels gone in every shot;
    the blade comes straight out of the hilt in the hand: short at ignition (w07570), held up (w07660), across
    (w07690), in the swing (w07612); off at w07720. Hands, wrists and the rest of the picture whole.
  - The saber's green point light (c17, 0.40 0.60 0.20) sat on the blade in her animated hand: sh3vr_beam_light
    moves it with the saber to the hand (d3d8_shader.c, every frame-camera draw while the saber is out: the game
    clears 0x712C66B after a swing with the blade still out, vr218). Picked by colour (green) within 0.6 m of the
    game's saber line (palm to 1.5 m past the tip) or 2 m of its hilt. vr219: moved at every logged frame except 7600
    (0.92 m off the line on the swing's frame); build 190 adds the 2 m test for that frame (built, NOT run). Her
    flashlight's white light (c16) is left alone.
  - Session 14's sprite move was wrong and is gone: the 128x128 sprites (0x468F6E / 0x468CDA, sub_004690A0) are the
    scene lamps' halos (combat_effect_rel's beam branch removed; sh3vr_beam_sprite deleted).
  - Kept: sh3vr_beam_mesh (vs F0000007 ignition glow meshes, 6 vertices, moved by the weapon's map, no depth writes).
- References searched (owner 68-69), none touched the saber's mesh: sh3-evil-effect, the Nexus Silent Hill 3 mods,
  silent-hill-museum (SH2 models only), the PC fix and RTX pages. Findings are in GAME_FACTS.md's session 15 section.
- LIST 27: the beam saber's swings sent their sweeps (4, 3, 2 a swing) and met no one (no monster in the opening).

## The APK pass (LIST 3, 40)

DONE (the first Android compile ever): `source /c/RACQuest/env.sh` then
`C:/SH3Quest/venv/Scripts/python.exe -I tools/build_quest.py` (logs C:/SH3Quest/build_quest_1.log to _4.log).
- Pass 1: every file compiled for arm64 with NO compile errors (the GLES 3.2 renderer, OpenXR, SDL2 2.32.10 from its
  source). The link failed: `__start_game_bridges` / `__stop_game_bridges` undefined (rt.h BRIDGE_REG puts the
  bridges' table in section game_bridges; lld's --gc-sections with start-stop-gc, its default, dropped it). Fixed in
  CMakeLists.txt's Android branch: `-Wl,-z,nostart-stop-gc` (backup C:/SH3Quest/CMakeLists.txt.before-s15).
- Pass 2 linked and packaged (Gradle 8.10.2 offline, 44 s); pass 3 took build 191's blade-shadow change; Gradle's
  incremental packaging left 9 MB of dead space in the APK, so it was repackaged with `gradlew assembleDebug
  --rerun-tasks` and `build_quest.py --no-native` (nothing deleted).
- The APK: C:/SH3Quest/SH3QuestVR/android/build/SH3QuestVR-0.1.0.apk, 10,493,334 bytes,
  SHA-256 3c2d90625fae25d2a721a90b6b2dbe05e9a36359b03026b07300d462a36e33c5. Zip test clean; the script's checks pass
  (libmain, SDL2, the OpenXR loader, the texts, the headset's manifest entries). libmain.so 29.7 MB stripped (88 MB
  with debug info at C:/SH3Quest/build-quest/native/libmain.so, for crash symbols): 12,325 game functions, 241 bridge
  registrations kept, SDL_main exported. Signed with the DEBUG key (no release key set up yet:
  tools/release-cert.sha256 and SH3QUEST_SIGNING); a later release-signed APK needs this one uninstalled first.
- Handed to the owner (path + SHA-256), NOT published. Never run on a headset yet. On the headset: copy the PC
  SILENT HILL 3 folder (sh3.exe and data/, about 5 GB) to the headset and pick it with the launcher's CHOOSE THE GAME
  FOLDER (or a .zip of it).

## Open, in order

Every run below: from C:/SH3Quest/gamedir, with the command line in the script's own header (QUEST_SIM_HEADSET=1
QUEST_SIM_ALIGN=7312 QUEST_SIM_GRIP_TILT=50 SH3_KEYS="22:Return,25:Return,28:Return,31:Return,34:Return", the
script's SH3_STEER_TO and switches, QUEST_SIM_SCRIPT=..., QUEST_SIM_OUT=C:/SH3Quest/shots/vrN), `mkdir -p` the
shots folder FIRST (vr225 wrote no pictures without it), timeout 660, in the background; build 195 is current (no
build needed). Next run number: vr230 (vr229 was stopped unread: run door229 again as vr230 or later).

1. The owner's first headset run of the APK: whatever the Quest shows (sh3-crash.log / the game's own log) is the
   next work; then HEADSET_CHECKLIST.md.
2. LIST 27, melee on a monster (never yet seen): tools/sim/door229.txt with SH3_STEER_AT_ATTACKER=1 (about 9 min).
   Read "steer test: ... the attacker" (its place, bearing, health; does +0x180 hold a monster's health? if it says
   "let go (health 0.0 ...)" at once, find the monster's health field first) and "took N damage from her" (the hit's
   damage, reaction and way). If no hit: put the stand spot nearer (343 units now) or the swing lower/higher by the
   attacker's height in the log, and look at the pictures (the monster ahead?).
3. LIST 45, the hall's far door by hand: tools/sim/door230.txt (about 9 min): "the left hand used event 48", then
   the next area's table (events 58-78), and on toward its gate (events 58-62: 58 at 22200 -19000 is trigger 3).
   A monster may hold her (vr227): then run it again.
4. LIST 45, floor items by hand: tools/sim/door228.txt (SH3_HP=1, the death skip, about 9 min); then route the mall
   from its tables (the room log on each load): the Ladies' Room's save point on the mirror and its window (by
   hand), the alley's far door, the office hallway's double doors, the shop under the half-lowered shutter: the
   handgun on the floor and the bullets on the green bench taken by the left hand ("floor item" in the hand logs).
5. The saber's light at frame 7600 is moved (vr220-vr222, DONE). vr222 (build 191): the saber held up (w07660) and
   across (w07690) with the flashlight on showed no blade-shaped shadow; a clear picture of her shadow with the
   hilt in it is still for the headset.
6. A release key for public builds (when the owner asks for a release): tools/release-cert.sha256, SH3QUEST_SIGNING.

## Files changed in session 15

- src/vr/sh3vr_combat.cpp: hold_matrix, saber_blade_draw (the blade), bone_finite (the rest of a weapon's palette,
  scaled bones too), combat_beam_map, combat_beam_line (the held bone's space), combat_effect_rel's beam branch
  removed, SH3_SABER_BONES.
- src/vr/sh3vr.cpp, sh3vr.h, sh3vr_internal.h: sh3vr_beam_mesh, sh3vr_beam_screen, sh3vr_beam_light;
  sh3vr_effect_map's log counter.
- src/d3d8/d3d8.c, d3d8.h, d3d8_shader.c: saber_out, beam_mesh / beam_map (the ignition glow's map before the eyes'
  corrections, no depth writes), the saber light call, the testing switches below.
- CMakeLists.txt: the Android link keeps the bridges' section (-Wl,-z,nostart-stop-gc).
- src/game/sh3.c (build 192, test logs only, run only with SH3_EVENT_SCAN; backup C:/SH3Quest/sh3.c.before-s15):
  event_table_log prints each region's place ("(at x z)") and the room's floor items (index, taken flag, bytes; "floor
  items: N in the room, M not taken"); event_scan logs the table again whenever the room's table changes. The APK
  (build 191) needs no rebuild for it.
- src/game/sh3.c (builds 193-194, the steer rig and the table log, test code only): SH3_STEER_TO takes up to 24
  points; a point she cannot stand on is passed after 4 stuck tries within 400 units of it or 8 anywhere ("passed
  (cannot reach it)"; vr225 stuck 1300 frames 254 units from one); a point over 40 m away is in another area, so she
  stands (at a door, for the hand) until a door takes her there ("is in another area: standing at"), so one run's
  points can go through several areas. The table log reads ops 0x02 / 0x04's z at +7.
- src/game/sh3.c (build 195, test code only): SH3_STEER_AT_ATTACKER=1 (steer_at_attacker, rig_attacker kept by
  sub_004B6A20 from the hits on her): once a monster has hit her, while it lives and is within 8 m she stands 0.7 m
  short of it along her facing, logged every 60 frames.
- tools/sim: door220.txt (the event 18 walk and a 16-way hand sweep), door223.txt (the entrance door, event 5, by
  the left hand), door224.txt (on into the circular hall), door225.txt (round the hall, south),
  door226.txt (the hall at 4000 units from its middle, the knife on its dog, on through event 48), door227.txt (the
  knife in circles; vr227), door228.txt (the death skip to the mall, not run), door229.txt (melee on the attacker,
  stopped), door230.txt (event 48 by hand, not run).
- tools/green_px.py (green pixels' count, box, middle and axis per picture); tools/sim: beam207.txt, combined206.txt,
  quit300.txt, quit7300.txt.
- docs: LIST-0.1.0.md (24, 27, 45, 50), GAME_FACTS.md (session 15 sections: the saber; the event table and doors),
  PUZZLES.md (how the hand finds things now, what is checked), HEADSET_CHECKLIST.md
  (the beam saber, the door by hand), this handoff; C:/SH3Quest/notes/owner-messages-sessions1-7.txt (67-71).
- Backups beside the project (C:/SH3Quest): sh3vr.cpp.before-s15b, sh3vr_combat.cpp.before-s15b,
  LIST-0.1.0.md.before-s15, GAME_FACTS.md.before-s15, HEADSET_CHECKLIST.md.before-s15, PUZZLES.md.before-s15.

Testing switches added (off unless set): SH3_BEAM_VERTS=N (+ SH3_BEAM_VERTS_FROM=F, SH3_BEAM_VERTS_TEX=W: rhw draws
while the saber is lit, with the blade's projected lines), SH3_ROUTE_DUMP2=M (a second route dump frame; each line
with its caller), SH3_HIDE_BEAM_MESH=F, SH3_HIDE_RIBBON=F, SH3_SABER_BONES=F (every skinned draw's palette at F and
F+30 while the saber is out), SH3_VS_GLSL_DIR=dir (each translated vertex shader to dir/vs_XXXXXXXX.glsl),
SH3_STEER_AT_ATTACKER=1 (above); SH3_STEER_TO now up to 24 points, passing unreachable ones, standing for far ones.

Waiting for the owner's go (files on C:, never deleted without it): as session 14 lists, plus the logs and shots of
vr206-vr229, build_pc_177-195.log, build_quest_1-4.log (and _4a), C:/SH3Quest/vsglsl (106 shader dumps from vr214),
C:/SH3Quest/build-quest (the Android build tree), and the backups above (plus CMakeLists.txt.before-s15,
sh3-quest-idea.md.before-s15 and MEMORY.md.before-s15).

## The owner's rules

As in SESSION12.md's last section (unchanged).
