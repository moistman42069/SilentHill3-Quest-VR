# SH3 Quest VR handoff, session 34 (2026-11-27): 0.1.9's mirror, scene shadows and pointer

Repo C:/SH3Quest/SH3QuestVR. 0.1.8 = 5465fc8 (tag v0.1.8, versionCode 9). Session 34 picked up session 33's work on
0.1.9 (the key give-back and the flashlight hint, docs/HANDOFF-2026-11-27-SESSION33.md) on owner message 164, and
added owners 164-166 to 0.1.9 before releasing it. GAME_FACTS "Session 34" has the game's side of everything here.

## Owner 164: the mirror ("didnt show the vr body moving accurately"; "regress nothing, only refine")

- The player's video read again densely. The restroom's mirror (9:09-9:15) is a second draw of her 73-bone model,
  reflected about the glass (its root's determinant -1), with the game's own animation: VR posed only her own model,
  so the reflection stood still and had no hands (the game never poses bones 43-72 in play).
- sh3.c 0x45EC00 / 0x45ED40: a reflected draw is never taken for hers (it used to be, as "her model away from her
  place"); ik_mirror_bones checks its bones against her animation's under one reflection (20 bones within 15 cm) and
  ik_mirror_map / ik_mirror_carry give it her VR pose through that reflection, her finger hands and the flashlight
  with it. Any mirror drawn this way works the same. The game draws no weapon in its mirrors (session 35, vr472's
  draw dump with the handgun drawn), so the reflection has none; ik_mirror_carry's weapon path stays for a mirror that
  does draw one.
- The log: her shown parts were listed every frame while the two models alternated (126,009 of vr451's 145,831
  lines); listed only when they change now (127 in vr455).
- Rig: tools/sim/mirror451.txt (door252's death skip wakes her in the restroom; steer point 5 to the first mirror;
  the head turned, each hand raised and held out, looking down, 60 and 110 degrees), mirror457.txt (the same with the
  handgun, given by SH3_GIVE_AT=9600 after the dream takes her items; sh3.c, build 272 on).

## Owner 165: storage

- C: had 13 GB free; the pictures of vr287-vr412 were deleted (logs kept): 50 GB free. Each run's pictures are deleted
  once checked (memory feedback-clear-as-you-go). Kept: vr413-422 (0.1.8's release set, the baseline) and
  vr436-440 (0.1.9's first build) until 0.1.9 is out. Not this session's, so not touched: shots/an146, video123,
  video127, video146 (show the owner first).

## Owner 166: her shadow at Happy Burger's table from her eyes

- Cause (vr454, SH3_VSC_DUMP): in the wake-up scene with the eyes in her head, two of the game's z-pass volumes of her
  were drawn: a type-3 window light (record A8 = 0, its builder 0x490810 drawn through 0x48BEC0) and her own type-0
  record (lamp -100995 -429 -20942). The eye inside a z-pass volume turns its shadow inside out. her_game_volume
  rejected A8 == 0, the "on her" test used her place (the scene draws her 1 m off it), and VR had no capture of her in
  scenes.
- 0.1.9: ik_scene_eyes (first person in a scene or demo, the scene's bones fresh); sh3.c marks her draws in such a
  scene as in play (bit 0x20, caught for her shadow); sh3vr_her_game_volume tests a quarter of the volume's corners
  within 30 cm of her bones as the scene draws them (ik_scene_bones); d3d8.c takes a type-3 lamp for 0x48BEC0's draws
  (the nearest of the first 64 corners within 30 m of [0x8E43AC]); her_room_shadow_ready holds in such a scene. Her
  shadow is then cast from her body as drawn, z-fail, as in play since 0.1.7. The view probes (kView) still take her
  body before the eyes as the world (sh3vr_body.cpp), so Dynamic's switch to the game's camera is as on 0.1.8.
- It reaches the opening dream's scenes at her eyes too (vr456's room-lamp lines from frame 4173).

## Owner 167 (session 35): the pistol in a scene seen from her eyes

- "during a cutscene, a pistol didnt render in the 1st person view of her shooting, but it rendered in the 3rd person
  one" (the video at 11:21, the store's back room, "Stay back!"). A scene that draws her with the eyes in her head
  poses her hands and the weapon itself; VR still moved the weapon to the controller (combat_weapon_bones) or, with
  it holstered, drew nothing. 0.1.9 (sh3vr_combat.cpp): when ik_scene_eyes, combat_weapon_bones leaves the bones as
  the scene has them and combat_weapon_draw marks the draw as hers (return 1: her shadow, the near-eye fade; the beam
  saber's blade return 2), logged "a scene draws her with the eyes in her head: ... left in the scene's hands".
- Rig: no scene the rig reaches draws a weapon. vr476 (dream459, build 273): the opening dream's scenes at her eyes
  draw her hands empty though the knife is equipped (no "left in the scene's hands" line; "0 of the weapon" in her
  shadow's lines), and nothing in them changed (her shadow lines are vr469's to the triangle); in play the knife is in
  her hand as before. The scene path itself is not yet seen: a save at the store's back room scene (the video's) or a
  player's log with the line settles it.

## Owner 168 (session 35): a hint for puzzles

- "we already have hints that apply to puzzles, right?": no. 0.1.9 (sh3vr_tips.cpp puzzle_frame, kTipPuzzle "puzzle"):
  the first time a screen to choose on (sh3vr.cpp game_screen, worked out before the pointer's own checks, so with the
  pointer off too) is up in a mode an event's script handed over to (sh3.c event_mode, GAME_FACTS "Session 35"), two
  lines at the screen's top (y0 = panel_h / 24; play's label stays at the foot): "Puzzles and screens: point with
  either hand and pull the trigger to choose" (the pointer's hand when set), "Or the left stick and A, as on a pad. B
  backs out" (with the pointer off, "Puzzles and screens: the left stick and A, ..." alone). Its own state, apart from
  play's queue; it ends with the screen. Never modes 14 (the inventory's Options), 19/20/25 (the save screens); the
  inventory (6) opens from play. hint_label_up() counts the label on a screen for it. The Hints page lists the lines
  after the item's.
- The rig reaches no puzzle: SH3_PUZZLE_TEST=1 takes the save screen (19) for a puzzle's (testing only).
- sh3.c logs each change of the game's mode ("the game's mode A -> B (an event's script opened it / not from an
  event; ...)", 400 at most).
- Build 273: vr474 (save474: the save screen, "the game's mode 5 -> 19 (an event's script opened it", no puzzle hint,
  its slots whole), vr475 (save474 with SH3_PUZZLE_TEST=1: "a screen an event's script opened is up to choose on (the
  game's mode 19)", line 1 at frame 11290, line 2 at 11718, at the screen's top over its header's band, readable, gone
  by 12140; "puzzle = 1" written), vr477 (shadow_screens: the inventory 5 -> 6, its Options 6 -> 14, "not from an
  event", no hint on the inventory, the map's message, memos or Options), vr479 (menu361: the Hints page lists both
  lines after the item's, the list scrolling to its end as before). Not yet seen on a real puzzle.

## The video, read again: the pointer, and two things not found

- The pointer's beam and dot over the black screens between scenes: d3d8.c counts the full screen's 2D draws and the
  textured ones (frame_screen_tex); sh3vr.cpp update_pointer: untextured, or one or two draws with no click zones, is
  a fade or black (no pointer); a screen fading out keeps its state (ui.drawn_last; build 272: vr455 had the beam
  flash up for ~16 frames after each fade).
- Not found: walls flat grey in two of Douglas's scenes (7:03-7:09, 8:45; culling already follows the head in
  scenes), and a squeezed strip as scenes start (10:52, 11:40). d3d8.c dev_SetViewport now logs a game viewport on
  the eyes' picture that does not cover it ("the game's viewport on the eyes' picture is ...", 40 at most); none in
  this session's runs. The release notes' Known for now asks for a report.

## Builds and checks

- Build 271 (C:/SH3Quest/build_pc_271.log): checks455.sh: vr455 (mirror451: the reflection with her VR pose, 41
  bones within 0.0 cm; pictures checked whole), vr456 (table455, Dynamic: the table scene's first person was the
  fade from black, 9085-9183, then the game's camera for the world 17 cm before her eyes, as vr454's at 9044).
- Build 272 (C:/SH3Quest/build_pc_272.log; the pointer's fade-out hold, SH3_GIVE_AT), the release build:
  checks458.sh and checks468.sh, every picture checked whole, then deleted (session 35):
  - Keys (event391, the store's door with its key taken away as 0.1.7 did): vr463 (given: "key 69 not held though
    given ... lent", then "given back: the game's check found its door", event 8, the door open), vr464 (flag 828 not
    set, never given: event 12, the locked message, nothing lent), vr465 (the key held: as 0.1.8, event 8).
  - The hint (hint270) and the Hints page (menu361): vr461-462, the two lines on the label and on the page.
  - The mirror: vr459 (mirror451: 41 bones within 0.0 cm; the head turned, each hand raised and held out, looking
    down, 60 and 110 degrees: the reflection does each, mirrored, its hands and fingers, the flashlight on her chest).
    vr460 (mirror457, the handgun drawn): the reflection as vr459 with no gun in its hand. Session 35's vr472 (save472,
    SH3_VSC_DUMP=10790 before the mirror) explains it: the frame draws her model with the weapon on her hand, then the
    mirror's model and no second weapon. The game draws no weapon in its mirrors; session 34's notes ("the weapon in
    the reflection's hand") were corrected everywhere.
  - From her eyes: vr468 (table460, Happy Burger's table: her arm's shadow on the table under her faded arm, nothing
    inside out or dark), vr469 (dream459, the opening dream: her body and hands right; "Heather's shadow from a room
    lamp ... the game's own volumes of her left out").
  - 0.1.8's weapons again: vr466 (beam401: the beam on the hilt's line held, ready, in a swing, forward and up, across
    and in both hands, as 0.1.8's vr416), vr467 (melee259: as vr417), vr470 (every gun's manual reload: as vr419, the
    log's reload lines the same).
- With Cutscenes: First person, the rig's QUEST_SIM_ALIGN mark (first person in play out of a scene) comes in the
  opening (frame 1909), not at 7260: table458 ran early (deleted); table460 moves table455's frames by 5351.
- Session 35: vr471-473 on build 272 (the mirror with the handgun again, the save point's lines and screen: its mode
  19, vr473; vr472's draw dump: no weapon in the mirror). Build 273 (C:/SH3Quest/build_pc_273.log; owners 167 and
  168), the release build: checks474.sh (vr474-479, owner 168 above; vr476 owner 167 above; vr478 hint270: the
  flashlight's two lines on the label in play as vr461) and checks480.sh / checks489.sh: vr480 (mirror451: 41 bones
  within 0.0 cm, the reflection doing each pose as vr459), vr481 (mirror457: the handgun whole in her hand, but the
  rig's walk missed point 4 with the monster's timing, so she faced the wall beside the mirror; run again as vr489:
  the reflection as vr480's with no gun in its hand, as vr460), vr482 (table460: as vr468, her shadow's lines the same
  to the triangle but one; her arm's shadow on the table under her faded arm), vr483-485 (the keys as vr463-465: lent
  and given back, event 8; flag 828 not set, event 12, nothing lent; the key held, event 8), vr486 (beam401: 46 of 56
  pictures identical to 0.1.8's vr416, the rest a few pixels of the hilt), vr487 (melee259: 42 of 48 identical to
  vr417), vr488 (reload: the reload lines as vr470's; the pictures as vr419's but for the reload hint vr419 still
  showed once and the smoke). No warning, error or crash in any log; sh3vr.ini and the hints file put back. Every
  picture checked whole, then deleted.

## 0.1.9 released

- The APK: `source /c/RACQuest/env.sh; python -I tools/build_quest.py --release` (log C:/SH3Quest/build_quest_019.log:
  sh3.c, d3d8.c, d3d8_shader.c and every changed VR file rebuilt, libmain.so linked), then tools/apkcheck.py against
  0.1.8's: 22 files each, changed only the manifest (versionCode 9 -> 10, "0.1.8" -> "0.1.9"), whats_new.txt,
  classes.dex and libmain.so; libSDL2.so and libopenxr_loader.so byte-identical; both signed with certificate
  38811d96...cd416e6a; ALL CHECKS PASS. libmain.so carries the puzzles' hint, "left in the scene's hands", the
  mirror's line, the key's "lent to the action check", the flashlight's second line, the mode log and the pointer's
  "a fade or black: no pointer".
- **SH3QuestVR-0.1.9.apk: 9,630,950 bytes, SHA-256 ee3e10b52609ebfe16cff1a319bfb5e447e7a6f21ba25fd8ace6a1aa29fc05c2**
  (C:/SH3Quest/SH3QuestVR-0.1.9.apk).
- Commit on main (message C:/SH3Quest/commit_019.txt), annotated tag v0.1.9, both pushed; the release "Silent Hill 3
  Quest VR 0.1.9" with docs/RELEASE-NOTES-0.1.9.md and the APK, marked latest. The commit's hash and the published
  asset's digest check are in the handoff commit after it.

## Next

1. Players' reports on 0.1.9 and their headset logs: the mirror ("her reflection (a mirror) drawn with her VR pose"),
   the table and other scenes at her eyes, a lost key's door ("lent" / "given back").
2. Douglas's grey walls and the squeezed strip at scene starts: a player's log with "the game's viewport on the eyes'
   picture" lines, or a save at those scenes, settles the strip; the walls need the scene on the rig.
3. docs/HEADSET_CHECKLIST.md's 0.1.8 and 0.1.9 items on a headset.
4. Storage: shots/an146, video123, video127, video146 are older than this session: list them for the owner.
