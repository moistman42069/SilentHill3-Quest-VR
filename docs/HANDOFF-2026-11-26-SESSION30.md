# SH3 Quest VR handoff, session 30 (2026-11-26): 0.1.8 in progress (grips on every weapon)

Repo C:/SH3Quest/SH3QuestVR. 0.1.7 = 703f432 (tag v0.1.7, versionCode 8), handoff 3bf881a. Nothing of 0.1.8 is
committed: the whole tree below is uncommitted work (no commit unasked; the release commit is the only one).
Session 30 stopped on owner message 149 ("wrap up and do a handoff") in the middle of the gun main-hand fits.

## The owner's messages (C:/SH3Quest/notes/owner-messages-sessions1-7.txt, 146-149)

- 146 (with a headset video and a 0.1.7 report): the trigger finger is not on the trigger, the two-hand grip is still a
  bit off; "grip anywhere" should be its own toggle; "by default though, make ALL guns and weapons/melee etc, grip
  properly and accurately. make sure the melee isnt too senstiive off the bat either ... make no fucking mistakes".
- 147: "review online data on similar ports/games and their two hand/weapon grip implementations ... regress nothing,
  only refine and improve."
- 148: "we already have proper world collision with finger reactions and such, and i recall asking for that collision to
  also be applicable to the guns. it wasnt ... use all the information at your disposal to get this working perfectly",
  then "*guns/weapons/melee weapons, etc. proceed with list".
- 149: "wrap up and do a handoff".
- Standing (owner 123): when 0.1.8 is done, publish it without asking.

Research done for 147 (cite in the release's final answer): HurricaneVR (grab points with a posed hand, the trigger
finger blended between an off and a pulled pose), Auto Hand (fingers swept closed until each meets the object's
collider, its grab solver), Meta Interaction SDK hand grabs (authored hand poses per grab point), Half-Life: Alyx (hands
snapped to the weapon's pose), SDFGen-style signed distance fields (the weapon's own shape for the fingers).

## Done (uncommitted; PC build 255 is this code)

- Options: "Hold weapons anywhere" (grip_anywhere, Hands and weapons, OFF by default) and "Swing strength needed"
  (swing_needed, Light / Normal / Hard, Normal default = 0.1.7's threshold x1.2; Light = 0.1.7's). In sh3vr.cpp's
  option table, sh3vr_internal.h, Options.java. The calibration file's v3 migration (two-hand spot reset once; backup of
  the rig's at /c/SH3Quest/sh3quest_weapons.ini.before-s30).
- Every weapon in her finger hand (kFingerHandOn all true, sh3vr_combat.cpp).
- NEW src/vr/sh3vr_shape.cpp (in CMakeLists.txt's VR list; Android builds the same list): each weapon's OWN SHAPE from
  its mesh as drawn in the hand. combat_weapon_mesh (called first in sh3vr_shadow_part for a held draw, kind 1) skins
  each vertex with the uploaded constants (stride 48, weights f[3..5], slots +24..27), maps it into the held bone's
  model units by the held slot's inverse, winds faces outward by the normals, shape_add dedupes by draw key. Once every
  draw signature is taken (placed_frames >= 36, no reload, no shot within 0.3 s), shape_frame builds a signed distance
  field on a detached thread (exact distance within 12 units, cell 1 unit coarsened to <= 2.5M points, int16 x64,
  inside/outside by 6 ray votes). weapon_clearance uses shape_segment (sampled every 0.75 cell + 5 ternary steps) once
  ready; the capsules (kShapes) before that, and with SH3_WEAPON_MESH=0. Fields build in 95-332 ms on the PC.
  SH3_SHAPE_SLICES=1 writes shape_<profile>_<x|y|z>.pgm slices to the game folder; tools/pgm_to_png.py converts them.
  Slices checked (vr379): the handgun's trigger guard is open, its trigger inside; the flamethrower's tank stands about
  2.4 cm in front of its pistol grip. The katana's field lacks its far blade (bounds y to 322) and the saber's beam is
  not solid: both fine for the hands.
- The fitted places: GripPlace fit_main[10], fit_second[10] (all zero now, NOT baked yet); main_fit, second_fit,
  main_native (left-handed: the right hand's game grip mirrored, mirrored_pose / mirrored_point), second_nudge,
  second_natural, resolve_moved_hold all take them. place_turn: pitch about the weapon's right, then roll about the
  grip's rod, about the game's fist centre.
- The fit search (SH3_GRIP_FIT=1, fit_step): main hand then other hand per weapon, body_grip_fit scoring (sh3vr_body.cpp):
  why codes 1 knuckle in (<4 mm), 2 palm in (>2 mm), 3 a finger buried at every curl or out of it only past 0.5, 4 a
  needed finger's curl outside 0.3-0.95, 6 the trigger (pad not on it, or the finger >3 mm in the gun on it or straight
  off it), 7 too little palm on it. The record (`grip fit record:` lines, 9 x 4 places round the start) gives per finger
  k(knuckle mm) c(curl meeting it) o(curl out of it) w(first clear window) [segments' skin mm at curl 0 | curl 0.6], the
  thumb rest, the palm, and for guns the trigger pad's nearest pass (mm, its curl, pad-minus-blade in model units), the
  index root and straight tip (model units).
- thumb_rest searches curls -0.4..1 (-1 = none; checks are `> -0.5f`).
- body_hand_capsules: the main hand's fingers as capsules, so the other hand wraps them (handgun two-handed).
- xr_sim.cpp: `rh_at_second 1` (the right controller at the second grip, for left-handed rig checks).
- Rig scripts: tools/sim/grips370.txt (all 10 weapons: SH3_EQUIP="1,2,3,8,4,5,6,7,9,10" SH3_EQUIP_EVERY=400, timeout
  ~1000), tools/sim/guns373.txt (the 5 guns: SH3_EQUIP="1,2,3,8,10" SH3_EQUIP_EVERY=400, timeout 600). Runner
  C:/SH3Quest/checks245.sh (edit its `run` line; output /c/SH3Quest/vrN.log, pictures C:/SH3Quest/shots/vrN).
- docs/RELEASE-NOTES-0.1.8.md exists but is still 0.1.7's text: rewrite it.

## Where it stands: the gun main hands (the open problem)

Other-hand fits found (vr376/377, older thumb rule: re-run, do not bake as they are):
handgun {-1.0,-13.0,6.5},16,0; shotgun {23.0,-20.0,4.0},-8,-16; submachine gun {17.0,17.5,0.5},0,-16; flamethrower
{1.0,23.0,8.0},8,8; katana {-14.2,-13.8,2.2},8,-8; maul {-11.4,14.2,3.1},16,16; saber {-18.6,-21.0,-2.5},0,16.
Main-hand fits found: knife {1.7,-17.4,-3.1},-8,-8; katana {-3.1,-0.8,-5.2},-8,16; maul {-2.5,-8.4,-3.5},0,0; saber
{-3.7,-5.2,-7.3},-8,8; stun gun {25.5,6.0,-2.9},-16,0. NO main-hand fit for the handgun, shotgun, submachine gun,
flamethrower (every place turned away).

vr378/vr380's records (build 253/255) show why:
- At the game's place her fist is 3-4 cm too far back (pictures vr309/vr313/vr379 g0-handgun-1close: the fist low and
  behind the trigger guard). 15-20 units forward and 10-20 down, the middle, ring and little fingers wrap well (curls
  0.45-0.57) and the thumb rests along the frame (-0.3); those places fail only on the trigger (why 6).
- The cause (vr380's trigger lines): her straight index points about 27 degrees ACROSS the gun (handgun: root at model
  z 1.4, straight tip at z -15.9, the gun's middle is z -13), so the hand is turned on the grip about the grip's line.
  Where the middle fingers wrap, the index root sits at y -5..5 and its pad passes 8-18 units below the trigger (blade y
  13.5-21.5) and, curled, 15 units past its left side. At the height where the pad could reach it (up 0, root y 14.6)
  the middle finger is in the trigger guard's bottom bar (y 7.5-10.5) and the palm in the grip. The search turns the
  hand only in round 1 (pitch/roll +-16 in 8s) and only round its round-0 best: with no round-0 place it never tries
  the turn that is needed.
- The flamethrower: every place has the index and middle buried (its tank in front of the grip): same turn problem plus
  a tight gap; the shotgun's best region is back (-10..-5 forward, -20 up), the submachine gun's +10..+20 forward,
  -20 up (index c0.56-0.69 clear at 0): both fail on the trigger the same way.

## Next (in order)

1. In sh3vr_combat.cpp fit_round_size / fit_candidate: round 0 also turns the hand: roll -32..32 in 8s (9) x pitch
   -16..16 in 8s (5) x moves x,y +-25 in 5s (11 x 11), z +-12.5 in 2.5s (11) = 59,895 places; round 1 round the best
   pitch/roll +-8 in 4s (25) x moves +-5 in 2.5s (125); round 2 as now. Raise the places tried a frame in fit_step
   (`k < 400`) to about 1200 so a weapon's main and other fits still finish inside SH3_EQUIP_EVERY=400 frames (check the
   log's fit lines land before the next equip). Update the comment block above FitRun (it still says 2.5-unit moves
   first, turns in round 1 only).
2. In sh3vr_body.cpp body_trigger_curl: keep the sweep onto the blade (free at curl 0, the pad within 10 mm when it
   meets it); when the sweep never meets the blade, take the pad's nearest pass if its skin passes within 3 mm (err =
   that gap). body_fingers uses the same function, so drawing follows. Consider the off-trigger curl: if 0.06 puts the
   straight index >3 mm into the gun, try 0, -0.1, -0.2 (lifted off the guard) and use the first clear one in both the
   fit's check and body_fingers (target when the trigger is not touched).
3. Build 256 (never while sh3.exe runs), run guns373 as vr381, read `grip fit:` lines (main/other per gun) and the
   records; the trigger pad must be on the blade (its pad <=3 mm off), index root near the trigger's height.
4. When the guns fit, run grips370 (all 10 weapons) with SH3_GRIP_FIT=1 and bake every main and other line into
   fit_main[] / fit_second[] (the log prints each as its table line). Fix the comments that say the fits are against
   kShapes (they are against the weapon's own shape now).
5. Pictures, every one checked whole (owner rule): each weapon one- and two-handed, trigger finger on/off, from the rig
   run WITHOUT SH3_GRIP_FIT (the baked places). Then left-handed: left_handed=1 in C:/SH3Quest/gamedir/sh3vr.ini (put
   it back after), a script with rh_at_second / left_first_touch. Then Hold weapons anywhere on, the moved two-hand spot
   and its resets, melee swings at Normal (not too sensitive), the ini's v3 migration.
6. Release 0.1.8: android/app/build.gradle versionCode 9 / versionName 0.1.8; whats_new.txt, RELEASE-NOTES-0.1.8.md
   (rewrite; public notes name no player), README, HEADSET_CHECKLIST, GAME_FACTS, AUDIT; re-check every feature on the
   release build itself (owner rule); `python tools/build_quest.py --release`, apkcheck against 0.1.7, commit, annotated
   tag v0.1.8, gh release (latest), the asset digest against a download. Publish without asking (owner 123). Update this
   handoff and the memory file.

## Rig notes

- Build: `cd /c/SH3Quest/SH3QuestVR && timeout 900 cmd //c "tools\\build_pc.bat" > /c/SH3Quest/build_pc_N.log 2>&1`.
  Next PC build 256, next run vr381. One build at a time; never while sh3.exe runs.
- A run is about 3.5 minutes; the game boots about 7300 frames before the first weapon. Short runs only (owner rule:
  no multi-hour sweeps).
- The rig's sh3vr.ini and sh3quest_weapons.ini are the rig's own; the backup above is from before session 30.
- Slices: run with SH3_SHAPE_SLICES=1, then `python -I tools/pgm_to_png.py C:/SH3Quest/gamedir/shape_0_z.pgm`.
