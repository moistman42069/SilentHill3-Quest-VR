# SH3 Quest VR: handoff, session 13 (2026-11-25)

Read docs/HANDOFF-2026-11-25-SESSION12.md first (the state before, its "Open, in order"); this one carries on from it.
PC test builds 165-172 (build_pc_172.log, BUILD OK); runs vr190-vr197 (about 30 run-minutes: the session's budget).
Nothing committed, no APK.

## The owner's messages this session (C:/SH3Quest/notes/owner-messages-sessions1-7.txt, 56-58)

- 56: "was working in another session. docs/HANDOFF-2026-11-25-SESSION12.md . pick up from where it left off.
  miss/forget nothing. proceed with list."
- 57: "this looks off. shotgun looked good, but yea, just be sure two hand grip works/looks smoothly on all
  guns/weapons. dont fuck that up. proceed with list" (after the submachine gun's two-hand picture).
- 58: "BE SURE THE FUCKIN SMG LOOKS GOOD WHEN TWO HANDED. most recent image of it still looks off. proceed with list"
  (after vr195).

## Done in session 13

- The audit (owner messages 53-54): docs/AUDIT-2026-11-25.md, one row per request (1-85), its gaps coded.
- LIST 24, the extras (builds 165-170, vr192-vr196): their bones carried from her animated hand to her IK hand
  (ik_hand_carry, combat_weapon_bones), then each given its own hold: weapon 17 (the unlimited submachine gun) as the
  submachine gun (the same model), the gold and silver pipes as the steel pipe (170/290 is its second signature),
  the beam saber (profile 8) and the flamethrower (profile 9) measured from their draws (SH3_MESH_DUMP=W); the game's
  own right hands read for both (groups 34, 37). The game's fist centre is (24.4, -1.1, -14.3) in weapon bone 0 for
  every weapon. All five held at the controller in vr196's pictures.
- LIST 12 (builds 168-170): the click zones' wrapper (0x601A40) is now in sh3.c's dispatch-override switch (generated
  code calls through the dispatch table: a wrapper that is not in the switch is never called; vr194 showed "zone 0 of
  0"). vr195: on OPTIONS the game itself highlights the row under the moving pointer and one pull chooses it (our own
  hover code from build 168 removed). vr196: the inventory's 11 zones read, a pull "over click zone 3 of 11".
- LIST 52, two hands on every two-hand weapon (owner 57): taking and letting go eased (two_w: ~0.1 s on, ~0.15 s
  off), the weapon's turn toward the other hand eased with it (q_mix in build_frame), the rig's records ("second grip
  taken/let go, eased W", "second grip held: the palm toward ..."). vr195: all ten take their grip at 9 cm; shotgun,
  handgun, pipes, katana, maul, beam saber, flamethrower right in pictures.
- The submachine guns (owner 58), two causes found:
  1. vr195: the rig held the right hand 35 cm out and ahead: "the left controller is past her arm's reach", her hand
     left at the arm's end beside the receiver. Build 171: where a held second grip lies past her other arm's reach
     (ik_arm_reach: her shoulder and reach as the arm was last solved), the weapon and both hands come back toward
     that shoulder by what the arm lacks (at most 20 cm), eased ("held back toward her"); only while the grip is held
     (build 172). The rig now holds the guns as a player does (the right hand before her chest).
  2. vr196 (reachable pose): once held, both submachine guns swung 90 degrees to the left ("forward -0.99 ...").
     Two frames after each take: "the left hand is held at a surface (the arm's line met it)". The left arm's line
     from her shoulder across to the short gun's front passes behind the gun and her right arm as the eyes see them;
     the depth probes took that for a wall and held the hand by her chest; the two-hand line followed the held hand.
     Build 172 (sh3vr_body.cpp): a hand on a weapon's second grip has no surface hold of its own (no arm's-line hold,
     no palm planes): its place is the weapon's, which the weapon's own probes hold back from walls.
- LIST 45: vr196 stood her at event 8's place facing 315 and swept the free left hand all round: nothing met. Build
  172 logs each event a hand's check meets but leaves to the game ("trigger N, not the button's"): vr197.

## vr197 (build 172, tools/sim/combined197.txt)

(checked in session 14) All ten two-hand weapons took their second grip at 9 cm ("the left hand took ...'s second
grip (0.09 m from it)"); the owner's message 60 followed from its sheet (the palm-up holds' wrists twisted: session
14). The door part: the walk reached event 8's place ("point 1 (-17386 21386) reached at -17317 21317") and the left
hand's sweep met nothing (no "can use" and no "not the button's" line): event 8's region lies past the hand's own
ray, within the button's 1.02 m (session 14, build 175: the game's reach toward the hand).

## Open, in order

1. The APK pass once vr197 is clean (tools/build_quest.py: the Android build has never been compiled: its first
   pass); its path and SHA-256 to the owner for the headset test (HEADSET_CHECKLIST.md); publish nothing.
2. Headset or playthrough only (HEADSET_CHECKLIST.md): LIST 6 speed, 22 fingers on a world object's own shape, 23 the
   radio near a monster, 29 every combat mechanic, 31 every puzzle, 38 the playthrough, 45 a door and a floor item by
   hand, 12 the save screen, puzzles and sliders, 50 and 57 with a monster, 56 a room with the game's own character
   shadows, every vibration's feel.

## Commands

- Build: `cd /c/SH3Quest/SH3QuestVR && timeout 900 cmd //c "tools\\build_pc.bat" > /c/SH3Quest/build_pc_N.log 2>&1`,
  then grep "BUILD OK". One at a time, never while sh3.exe runs. A new wrapper in sh3.c: run
  `PY=/c/SH3Quest/venv/Scripts/python.exe sh tools/regen.sh --recomp-only` first AND add it to sh3.c's dispatch
  override switch (`case 0x00XXXXXXu: return sub_00XXXXXX;`), or it is never called.
- Run: as SESSION12.md, with the script header's switches; the script runs about 33 frames ahead of the game in play
  (vr196). `lh_at_second 1` (xr_sim.cpp) puts the left controller at the held weapon's second grip (lh_x/y/z then
  metres from it).
- Scratchpad tools: sheet.py (`out cell cols imgs...`), crop.py, meshinfo.py, meshplot.py, slices.py, mk196.py,
  mk197.py (they write the combined scripts).

## Files changed in session 13

- src/vr/sh3vr_combat.cpp: kProfiles 10 (the beam saber 8, the flamethrower 9), their rows, holds, shapes and native
  groups; signatures up to 4 per weapon (the pipes' 170/290); profile_of 9/10/13/14; weapon_name by slot; the SMG's
  game left hand only within 4 cm; two_w and q_mix (the eased take and let-go, build_frame's eased turn); the rig's
  take/held/draw logs; sim_second_spot; reach_pull (171, only while held 172); combat_weapon_bones (165).
- src/vr/sh3vr_ik.cpp: ik_hand_carry (165); the shoulder and reach as solved, ik_arm_reach (171).
- src/vr/sh3vr_body.cpp: no surface hold for a hand on a second grip (172).
- src/vr/sh3vr_internal.h: the declarations. src/vr/xr_sim.cpp: lh_at_second.
- src/game/sh3.c: the extras' bones carried in the 0x45EC00 wrapper (165); 0x601A40's zones (zones_seen) and its
  dispatch case (170); the hand check's left-to-the-game event log (172).
- tools/sim: extras.txt, combined193-197.txt.
- docs: LIST-0.1.0.md (12, 24, 52, the Written notes), GAME_FACTS.md (the zones, OPTIONS), HEADSET_CHECKLIST.md (OPTIONS
  answered, two-hand smoothness and reach), AUDIT-2026-11-25.md (rows 84-85), this handoff; the notes file (56-58).
- Backups beside the project (C:/SH3Quest/*.before-*): sh3vr_combat.cpp, sh3vr_ik.cpp, sh3vr_internal.h,
  sh3vr_body.cpp, HEADSET_CHECKLIST.md; the notes file's .bak-s13.

Waiting for the owner's go (files on C:, never deleted without it): the shots folders C:/SH3Quest/shots/vr* and the
vr*.log, build_pc_*.log and regen_*.log files from sessions 1-13; C:/SH3Quest/dl_plmpeg; the backups above.

## The owner's rules

As in SESSION12.md's last section (unchanged).
