# SH3 Quest VR handoff, session 31 (2026-11-27): 0.1.8 in progress (the trigger pulled, the fits)

Repo C:/SH3Quest/SH3QuestVR. 0.1.7 = 703f432 (tag v0.1.7, versionCode 8). Nothing of 0.1.8 is committed: the tree is
uncommitted work (no commit unasked; the release commit is the only one). Read docs/HANDOFF-2026-11-26-SESSION30.md
first for 0.1.8's base (the options, the weapons' own shapes, the fit search); this one says what session 31 added.

## The owner's messages (C:/SH3Quest/notes/owner-messages-sessions1-7.txt)

- 150: "pick up from where it left off. miss/forget nothing. make no mistakes. check everything you need to be sure this
  build is perfect. proceed with list."
- 151: "not looking for "acceptable" it needs to be PERFECT as can be. make sure the fingers can actually be seen
  properly pulling the triggers n shi. proceed with list".
- Standing (owner 123): when 0.1.8 is done, publish it without asking.

## Done in session 31 (uncommitted; PC build 259 is this code)

- The fit search turns the hand in round 0 (roll +-32 by 8, pitch +-16 by 8, moves 11^3; round 1 +-8 by 4, moves +-5;
  round 2 0.5-unit moves; 1200 places a frame). The turn turns only the hand on the gun (main_hand_pose): the gun keeps
  0.1.7's way in the hand, so aim is unchanged.
- The trigger finger may spread sideways at its knuckle (finger_points' `spread`, degrees; drawn by pose_fingers with
  the same axis; index_spread[h] eased in body_fingers): body_trigger_curl tries 0, +-4 ... +-16 and takes the first
  whose pad meets the blade (within 10 mm where the sweep meets it, or the nearest pass within 3 mm), curl 0.12-0.85,
  the finger no more than 3 mm into the gun (index_clear). body_trigger_rest: off the trigger, 0.06, 0, -0.1, -0.2 (the
  first clear), unspread then at the on-trigger spread.
- The fit's trigger rule also asks the finger to rise from its knuckle to its pad no more than 25 degrees up the gun and
  no more than 35 down (vr383: the submachine gun's fit had the hand low on the magazine, fingers pointing up).
- Owner message 151, the trigger seen pulled:
  - Each trigger is its own draw pass (sh3vr_parts.cpp kTriggerParts: the dumps' four whole pieces per trigger; handgun
    24 vertices, shotgun 30, submachine gun 20; pass kind 3), turned about its pin (sh3vr_combat.cpp kTriggers: pin just
    above the blade's front top corner, so the top turns up into the frame; 24, 22 and 26 degrees all the way, about
    6-7 mm at the pad). weapon_trigger_at(pull) is the blade turned; weapon_trigger_place builds the pass's matrix.
  - The finger drives the blade: body_trigger_pull_curls finds the finger's curl with its pad on the blade at each
    quarter of the pull (pad_on_blade, the spread fixed); the finger's target is that curve at the controller's
    trigger value; the blade is drawn as far as the finger, drawn (eased), has pushed it (trigger_shown, the curve
    inverted), so they never part or pass through each other, and a finger still on its way does not move it. The log
    says once per touch: "the right hand's trigger finger on the trigger (curl ..., spread ...); pulled a quarter at a
    time it closes to ...".
  - The shape capture (combat_weapon_mesh) takes a frame only with every pass at rest: the trigger turned no more than
    2 degrees and every moving part at rest (before, a frame with the slide back took the body without the slide).
- Found and fixed while doing it (0.1.7 bugs): the shotgun's pump moved +x (towards the muzzle) when pumped back
  (kMovingParts' back was +1, from session 4's wrong "muzzle along -x"; GAME_FACTS corrected); the handgun's two front
  top trigger corners were in the slide's height cut, so a trigger face went back with the slide.
- Rig scripts: tools/sim/make_pics.awk makes tools/sim/pics383.txt from grips370.txt (600-frame cycle; off-trigger
  pictures; the guns' pull pictures 1pull30/1pull58/1pull100/1rightpull100/1eyespull100, and from build 260 close from
  the left, where the blade is not behind the finger: 1leftpull0/1leftpull58/1leftpull100). tools/sim/mirror.awk makes
  tools/sim/lefty384.txt (left-handed, pictures L...). tools/sim/parts384.txt: the empty handgun's slide back (trigger
  in place, pulled dry) and the shotgun's pump back and forward. Remake both: `{ head -8 tools/sim/pics383.txt; awk -f
  tools/sim/make_pics.awk tools/sim/grips370.txt; }` (the header kept) and `{ head -2 tools/sim/lefty384.txt; awk -f
  tools/sim/mirror.awk tools/sim/pics383.txt; }`.
- vr384 (build 259, the fit with the pull): the pad stays on the blade the whole pull (handgun 0.36 -> 0.39 0.41 0.44
  0.47). Two faults found and fixed for build 260:
  - The handgun's thumb, from the eyes, met the frame with its root at curl 0.65 and stood out to the left at about 45
    degrees, its tip 1.5 cm off the gun. A thumb only curled cannot lie along a frame. Now it also swings sideways at its
    root (finger_points' spread for the thumb, up to 45 each way; ik's pose_fingers turns its root with it, thumb_spread
    eased like index_spread; body_hand_capsules too, so the other hand wraps the thumb as drawn). thumb_rest tries every
    curl (-0.4..1 by 0.1) at every swing (0, +-15, +-30, +-45), then the best one's neighbours (0.05, 7.5 degrees), and
    takes the least swung, nearest 0.65, of those with the pad's segment ON the weapon (its skin within 4 mm, the middle
    segment's within 8) and every segment clear by 5.5 mm at the axis; the fit (body_grip_fit, body_grip_report: "thumb
    rest .. swung ..") and the drawing (body_fingers through thumb_resting: kept while the hand stays within 1 mm on the
    weapon, again every 30 frames) use the same rule. The fit's line says "thumb swung".
  - The steel pipe got no fit: its own shape waited 600 frames for its second signature (170/290), which is never drawn
    in first person. sh3vr_shape.cpp now builds after 30 frames with what it has.

## Next (in order)

1. vr386 (build 260, pics383 with SH3_GRIP_FIT=1, all 10 weapons, EVERY=600; runner C:/SH3Quest/checks260.sh): read
   every `grip fit:` line (main and other per weapon; the thumb's swing; the steel pipe now fitted; a roll at the
   search's edge, +-40, means round 0's roll range wants widening), check the pull pictures (left side: the blade seen
   turning, the pad on it) and the thumb from the eyes; bake fit_main[] / fit_second[] in sh3vr_combat.cpp from the
   lines.
2. vr387 (parts384, same runner): the slide, trigger and pump pictures.
3. Build 261 with the baked places; pictures WITHOUT SH3_GRIP_FIT (pics383, EVERY=600, timeout 1300), every picture
   checked whole and judged as perfect (the handgun's thumb from the eyes: it stood out left in vr383); then left-handed
   (lefty384 with left_handed = 1 in C:/SH3Quest/gamedir/sh3vr.ini, put back after); Hold weapons anywhere, the moved
   two-hand spot and its resets, melee at Normal, the ini's v3 migration.
4. Release 0.1.8 (see session 30's step 6). The draft notes: scratchpad notes-018-draft.md (copy kept in the handoff
   below once final).

## Rig notes

- Build: `cd /c/SH3Quest/SH3QuestVR && timeout 900 cmd //c "tools\\build_pc.bat" > /c/SH3Quest/build_pc_N.log 2>&1`.
  Next PC build 261, next run vr388 (vr386, vr387 above). One build at a time; never while sh3.exe runs.
- Mesh dumps of the guns: C:/SH3Quest/gamedir/mesh_368.txt (handgun), mesh_290.txt (shotgun), mesh_557.txt (submachine
  gun).
