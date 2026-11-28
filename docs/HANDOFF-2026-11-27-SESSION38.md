# Handoff, 2026-11-27 and 28, session 38: owner 172 (the handgun held high) and 178 (natural holds); 0.1.10 released

Read docs/HANDOFF-2026-11-27-SESSION37.md first. This session took its "Next" list.

**0.1.10 is released from build 312: see "State at the end".** It is the session 37 tree plus this session's changes
(builds 291-312). Backups of the files as session 37 left them: C:/SH3Quest/notes/sh3vr_combat.cpp.bak-s38 and
sh3vr_body.cpp.bak-s38.

## Owner messages this session (in C:/SH3Quest/notes/owner-messages-sessions1-7.txt)

- 177: "was working in another session. pick up from where it left off. miss/forget nothing. proceed with list."
- 178 (on vr530's pictures, mid-session): "make sure the weapon grips look natural. who the FUCVK holds a smg like that
  man??? cmon. proceed with list" (the submachine gun's front from vr528: her palm on the receiver's side, her
  fingers over its top).
- 179: "wrap up and do handoff" (while vr536, build 300's every-weapon check, ran).
- 180 (2026-11-28): "proceed with list. make no mistakes".
- 181: "didnt realize there was a left handed mode. i assume its a toggle in the menu or somthing? cool. proceed with
  list" (answered: Left-handed, in the launcher's Options, Hands and weapons; off by default).
- 182: "proceed with list" (after a pause mid-edit).

## What was found

- **Why 0.1.8's search found no handgun place with her drawn palm clear:** its turn about the grip was held to ±32
  degrees round 0.1.8's own (-52), and the place needed is about 52 degrees round from that.
- **The wide search (build 291)** turns a gun's main hand the whole way round its grip (45 turns of 8 degrees,
  fit_wide_turn) and, at each turn, first finds where her drawn palm meets the weapon (fit_palm_meet). The handgun
  then fitted at {{17.7, -16.1, 10.5}, 20, 0}: her whole hand as drawn 0.3 mm clear, no clipping (vr516, vr517).
- **But that hold is too low (vr517 pictures):** the hand holds the foot of the grip. The gun sits on her fist and
  the wooden grip panel shows above her knuckles. Her index rises 15 degrees to the trigger. Not a shooter's hold.
- **The trigger finger held near level (build 293, SH3_FIT_RISE):** at 5 degrees up at most, no handgun place at any
  turn (vr519), nor at 0 (vr520). The submachine gun got a better place, {{22.4, -0.7, 9.4}, -8, -4} (score 5.91, its
  index 1 degree up, whole hand 0.2 mm clear), now baked; its Uzi-style hold looks right in the pictures.
- **What holds the handgun hand down (build 294's record, "grip fit, up the weapon", vr521):** from the fitted place,
  moved up the weapon 2 mm, the skin between her knuckles (blended between bones) is 1.5 mm in the frame; from 6 mm
  up her middle finger is in the trigger guard (why 3), and the skin 7.7 to 14 mm in. So the hand cannot slide up; it
  has to turn to rise (its web up onto the back strap, its knuckles kept at the guard).
- **The wide search's turns about the weapon's right** were ±16 degrees. Build 295 lets the rig ask up to ±40
  (SH3_FIT_PITCHES=11). vr523: no place at any turn in round 0, not even at the fitted place's own turn: round 0's
  grid (5 units, 10 mm, along and up the weapon) is too coarse for the few millimetres a hand has round a grip; the
  fitted place itself is found by the fine rounds round the start. So the wide table says nothing about other turns.
- **The climb (build 296, SH3_FIT_CLIMB=1, vr525):** from the fitted place, the hand moved up the grip's own line 2 mm
  at a time (to 24 mm), a fine search at each height (turns 6 degrees apart to 24 each way about the weapon's right, 4
  to 16 about the grip's line, her palm brought to meet the weapon at each turn, moves of 2 units along and 1.5
  across). Each height's best is logged ("up the grip"); the highest height with a place is taken for the pictures.
  **Result (vr525):** places at 0, 2, 4, 6, 8, 12, 18 and 20 mm up (none at 10, 14, 16, 22, 24). The highest, 20 mm up:
  {{16.3, -6.3, 12.5}, -2, -8}, score 4.19, her whole hand 0.4 mm clear, her index 8 degrees up to the trigger (16 at
  the foot). The pictures taken after it was taken (vr525's pulls, from both sides): the hand high on the grip, the web
  under the slide's tang, the knuckles under the frame, the index into the guard on the trigger, the fingers round the
  front strap, the thumb over the middle finger; by bone all clear once settled (the ring finger's tip 0.5 mm the
  nearest). Baked into fit_main (build 297). (vr525's first pictures, 1close to 1pull30, were taken before the climb
  ended: the old place.)
- **The pull pictures' "as the fit reads it" for the trigger finger (vr517: handgun -0.4 to -1.4, shotgun -0.8 to
  -1.6) is measured against the gun's shape at rest, its trigger before the pull; the drawn trigger turns back with the
  finger.** Build 297 adds the picture line "its trigger finger as drawn against the trigger as drawn (pulled N)"
  (hand_skin_trigger: the index's skin against the turned blade, and against the rest of the gun with the trigger at
  rest left out).
- **vr517's by-bone report (every weapon, right-handed) found skin inside a weapon on four:** the submachine gun's
  front hand 15 mm (0.1.8's place kept: no place found since), the beam saber's other hand 5.8 mm, the katana's main
  hand 1.3 mm (its triangles 8.3: fitted on build 287, before the in-play thumb rested on her drawn skin, which moves
  the skin between thumb and palm), the steel pipe's other hand 0.2 mm. Left-handed (vr518, all ten weapons): none.
- **Build 296 for those:** the two-handed melee weapons' other hands (the main hand's mirror, never fitted) are now
  fitted as a move and turn from their mirrored place (second_hand_pose; fit_second; left-handed mirrored); an other
  hand whose palm meets the weapon nowhere along its back (the submachine gun's front) is searched at every turn,
  free of its grip's line. vr526 fits every weapon but the handgun again on this build.
- **vr526 (build 296), baked in build 297:** every main hand and other hand found with her whole hand as drawn 0.2 to
  0.7 mm clear: shotgun {{4.4, 1.3, 2.5}, -16, -16} (5.98), its pump the same as before; submachine gun {{21.8, -0.7,
  9.4}, -8, -4} (5.79, vr519's 0.6 units back); stun gun the same; knife {{18.3, -5.9, 2.5}, -20, 32} (its place
  before failed: palm off); steel pipe {{10.6, -3.5, 4.6}, -24, 40} and its other hand {{11.0, 17.1, 3.2}, 8, -8};
  katana {{2.2, -0.5, -2.1}, -8, 48} and {{-1.8, -7.9, -2.2}, 0, 32}; maul {{6.1, -2.0, 4.0}, 4, -28} and {{-2.0,
  -3.1, 0.0}, 12, -28} (its other hand 5.0 mm in before); beam saber {{5.0, -1.8, 0.5}, -20, 32} and {{0.7, 26.6,
  -4.0}, -16, 20}; flamethrower {{12.5, -15.7, 2.7}, 20, -152} and {{-1.5, -4.2, 6.0}, -8, 12} (6.6 mm in before,
  the main hand's place having changed). The submachine gun's front was not fitted: its search at every turn was cut
  off by the next weapon (SH3_EQUIP_EVERY). Build 297 adds SH3_FIT_SECOND (the rig: the other hand alone, the main
  hand kept): vr527 the handgun's other hand round the main hand's new place, vr528 the submachine gun's front with
  that gun alone (SH3_EQUIP=3).
- **vr527:** the handgun's other hand {{-12.1, -14.1, -5.3}, -32, -24} (5.65; her whole hand 0.5 mm off the gun and
  off her main hand; at its place before, with the main hand 20 mm higher, her palm was 3.0 mm in the grip).
  **vr528:** the submachine gun's front {{17.8, 10.2, 14.0}, -32, -84} (5.91; 1.1 mm; before -14.9): her palm on the
  receiver's left side, her fingers over its top, her forearm under and beside her right one; from her eyes a compact
  two-handed hold (0.1.8's laid flat on top of the receiver, the palm 15 mm in). Both baked, build 298.
- **Final checks, build 298 (checks529.sh):** vr529 door275, vr530 pics383 (every weapon), vr531 lefty384 (every
  weapon, left-handed), vr532 reload. The pictures' log read with a small awk (C:/SH3Quest/rigtools/picsum.awk: per picture each
  hand's nearest by bone, as the fit reads it, against the trigger as drawn, and the other hand off her main hand).

- **vr529 (door, build 298): passed** (the handle taken, let go without opening it, the hint shown, pulled 15 cm, the
  door used, the next area loaded). **vr530 (every weapon, build 298), read as played** (picsum.awk) against vr517:
  the handgun, katana and beam saber now clear (the katana's and beam saber's 4.0 to 8.5 mm in before); the
  submachine gun's front still 12 mm in as played; the pipe's and maul's other hands 1.2 and 1.1 mm in; the
  flamethrower's new main place (vr526) 6.8 mm in as played, though 0.2 mm off in the fit. vr531/vr532 were stopped
  for build 299 (owner 178).
- **The fit's reading and the pictures' can differ:** the fit poses the hand with its own curls; as played her thumb
  rests closed on her drawn skin and each finger's wrap is solved again, so a place clear in the fit can come into the
  weapon as played (the flamethrower's main hand: its thumb curled 1.00 in play, 6.8 mm in, her wrist bent 147
  degrees). From this session the places are chosen by the pictures as played; the fit only searches.
- **Owner 178, build 299:** (a) the cup rule (fit_step): an other hand whose hold's palm is up (the submachine gun's
  front, the shotgun's pump, the flamethrower's front) is searched only round a palm-up place; when its place before
  has her drawn palm more than 37 degrees off up (body_palm_skin_clear's back way), the search starts from the hold's
  own cup ("the search from the hold's own cup" in the log); the cup's clamps (roll 12 degrees each way of it, sink,
  side) hold in every stage. (b) A melee weapon's other hand closes every finger round the handle, its index too
  (body_grip_fit; least curl 0.3: vr530's maul's index lay straight). (c) The wrist: body_wrist_swing (the drawn hand's
  wrist-to-knuckles line against the controller's) in the fit's log and the pictures, with ik_wrist_bend (her wrist
  as drawn against her forearm); the fit can turn away an other hand bent past SH3_FIT_WRIST (why 8). FitRun's why
  arrays now 9.
- **vr533 (build 299, SH3_FIT_SECOND=1, every weapon's other hand, no wrist limit):** submachine gun {{5.0, -14.0,
  5.0}, -8, 8} (5.81; its place before 85 degrees off palm-up, so from the hold's own cup: palm under the receiver's
  front, her fingers up its side; 0.5 mm off as played); steel pipe {{4.0, -8.7, 3.0}, 8, -32} (5.68, every finger
  round it; 0.4 as played, bone 1.2); maul {{-2.0, -3.1, 0.0}, 8, -32} (6.04, its index round the shaft; 0.3 as
  played); katana {{1.6, 3.7, -1.0}, 0, 40} and beam saber {{3.8, 7.8, -4.9}, -4, 32} (clear, but crowded under her
  main hand, the beam saber's wrist bent 127 degrees as drawn: not taken); the handgun's and shotgun's the same as
  before. vr534 (the same with a 40-degree wrist limit) stopped: the rig's controllers both point ahead, so every
  cupped hold reads 86 to 90 degrees there and the limit turned them all away; off by default from build 300.
- **Build 300 (baked):** fit_second's submachine gun, steel pipe and maul from vr533; the flamethrower's main hand and
  other hand back to build 292's ({{16.0, -5.2, 0.7}, 12, -168} and {{-7.4, -10.6, 0.1}, 8, 24}: 0.2 mm off as played,
  vr517); the katana's and beam saber's kept (vr530: clear as played, natural). The shotgun's trigger finger: vr530
  read its drawn index 0.7 mm into the drawn trigger at rest and 1.4 mm all the way pulled (as far into the guard),
  the handgun's 0.9 to 1.9 mm off and the submachine gun's 0.5 to 1.8; kTriggerPad (sh3vr_combat.cpp): the shotgun's
  pad stops 2 mm sooner where the finger closes on the blade (GripTrigger.pad, pad_on_blade; the blade drawn and read
  the same).
- **The shotgun pump's by-bone -1.2 in its 2close picture** (vr517 and vr530 alike): in all six of its pictures her
  left palm as drawn is 17 mm off her palm as this model poses it (one group of the palm's vertices skinned
  elsewhere by the game); one camera's picture reads that group 1.2 mm in. The other five read 0.5 off, the fit 0.3.
  Not from this session's changes.
## Other list items done this session

- **Costume change (open since session 37):** her hand skin is now taken again when her drawn model changes
  (sh3vr_combat.cpp skin_watch: the draws' vertex and triangle counts, looked at every 3 seconds once taken; other
  counts for 2 seconds or more, at most 8 times a session). Tested with SH3_SKIN_FORGET_AT=7300 (vr522): both hands let
  go and taken again the same frame; her posed hand then 0.03 mm off her drawn hand (wrist 0.18); every picture the
  same as vr517's but the muzzle flashes.
- **The second hand against her main hand as drawn (open since session 37):** body_grip_mesh now also measures the
  other hand's skin against her main hand's skin as drawn (hand_skin_between, part 7 "her main hand as drawn"), and in
  the fit the other hand closes onto her main hand's drawn skin (body_hand_skin_points in weapon_clearance, the fit
  alone: in play it would cost the headset's frame). The pictures log each other hand's distance from her main hand
  as drawn. vr522: the handgun's baked other hand is 2.0 mm off her main hand in play (no clipping).
- vr517 (every weapon on build 292): the shotgun, stun gun, knife, pipe, katana, maul, beam saber and flamethrower
  holds look right (the flamethrower's roll -168 included). vr518 (left-handed): clear, 11 trigger lines, the same low
  handgun hold mirrored. **Wrong (found 2026-11-28):** vr518, like session 37's vr500, ran with SH3_EQUIP="1", the
  handgun alone, so every left-handed picture showed the handgun; no other weapon was checked left-handed until vr537.

## Continued 2026-11-28 (owner 180: "proceed with list. make no mistakes")

- **vr535 (door, build 300): passed.** Its log and all its pictures as vr529's and vr498's.
- **vr536 (every weapon right-handed, build 300), read as played** (picsum.awk, every picture looked at whole):
  every hold clear and natural but one: **the maul's other hand 3.3 mm into the shaft as played** (vr533's fit read
  0.3). The cause: in the fit alone (grip_fit_mode() == 1) weapon_clearance adds her main hand's drawn skin for the
  other hand (body_hand_skin_points), so her thumb rested on her main hand in the fit and each finger's wrap was
  solved against it; in play it is not there. **Build 301: SH3_FIT_PLAY=1** (rig only) leaves it out, so the fit reads
  as played (hand_skin_between, body_grip_mesh's part 7, still turns away an other hand in her main hand). **vr539**
  (SH3_FIT_SECOND=1 SH3_FIT_PLAY=1, the maul alone): {{-5.6, -15.2, 1.8}, 12, -16} (5.99; 0.4 mm by bone, 2.1 off her
  main hand; the fingers round the shaft from below, the thumb along it). Baked, build 302.
  - Explained, not changed: the shotgun's deep pull (58-100%) reads her crease 0.9 to 1.2 mm into the grip inside the
    closed hand (unseen in every picture; 0.1.9's 1.6), its trigger finger now 0.1 to 1.2 mm off the trigger
    (kTriggerPad works); the shotgun pump's 2close by-bone -1.2 (above); the handgun's and submachine gun's full pull
    -0.1 to -0.2 at the index as the fit reads it (the fit's mesh keeps the trigger at rest; against the trigger as
    drawn and the rest of the gun, 1.6 to 1.9 and 0.1 to 1.1 mm off). 4 trigger lines, as vr530.
  - "The main hand's place" lines (settle_heel, against the weapon's own shape, its 2 mm signed-distance grid) read
    the stun gun's palm -4.1 and thumb -10.3, the maul's palm -6.2, the knife's -2.2, the same since vr517/vr530: that
    grid's reading, not her hand as drawn against the mesh (body_grip_mesh, what the fit and the pictures read: the
    stun gun 0.7 mm). Its pictures: a fist round it, the thumb along its top, nothing in it.
- **vr538 (reload, build 300): passed** (the handgun's magazine and slide, the shotgun's shells and pump, the
  submachine gun's magazine and knob, the stun gun's battery, all by hand). Its battery and knob pictures read the off
  hand against the gun while it pushes the part in (the sim's path), as before.
- **vr537 (left-handed, build 300): the first left-handed pictures of every weapon.** The right-handed places
  mirrored went in: the katana's main hand 6.7 mm, the beam saber's 15 (its other hand 10), the flamethrower's 9.3,
  the stun gun's 1.6, the submachine gun's front 8.3 (8.4 into her main hand), the maul's other hand 3.1, the
  handgun's other hand 1.1. Clear: the knife, pipe, shotgun (at rest, the pump), the handgun's main hand, the
  flamethrower's and katana's other hands. Her hands as drawn are not each other's mirror, nor the weapons.
- **Build 302: fit_main_left / fit_second_left** (sh3vr_combat.cpp): a left-handed player's own places (the same
  mirrored convention), chosen by main_fit and second_fit when opt.left_handed; each started as the right-handed one.
- **vr540 (build 302, left_handed = 1, SH3_GRIP_FIT=1 SH3_FIT_PLAY=1; the submachine gun, stun gun, katana, beam
  saber and flamethrower) and vr541 (SH3_FIT_SECOND=1; the handgun and maul):** main hands: submachine gun {{21.2,
  -1.7, 10.3}, -4, -8} (6.34; 0.3 mm; its trigger pad 0.5 mm off the whole pull), stun gun {{26.0, -12.9, 2.5}, -4, 24}
  (6.60; 0.3), katana {{8.0, -2.3, -1.8}, -4, 24} (6.48; 0.3), beam saber {{10.1, -4.2, 5.6}, -28, 44} (6.38; 0.3),
  flamethrower {{16.6, -1.5, -4.3}, 16, -160} (6.59; 0.6); other hands: beam saber {{2.6, 17.8, 0.2}, 0, 36} (7.00;
  0.5, 0.2 off her main hand), flamethrower {{-2.8, -7.9, 4.9}, 0, 28} (5.78; 0.5), maul {{-5.3, -14.2, 1.8}, 12, -16}
  (5.99; 0.4; nearly the right-handed place). Each looked at as played: natural, as the right-handed hold. **Not
  natural, not taken:** the submachine gun's front {{7.0, -11.5, 11.0}, -20, 8} (its fingers spread in the air beside
  the receiver), the katana's other hand {{-7.7, -10.6, -4.1}, -20, 16} (under the end of the handle, the index
  straight), the handgun's {{-3.0, -18.6, -1.2}, -64, -4} (her wrist bent 117 degrees as drawn, the fingers hanging
  under the grip). The left-handed handgun's full pull reads -0.8 at the index as the fit reads it, the trigger at rest
  (against the trigger as drawn 1.6 mm, the rest of the gun 0.1): the same artifact.
- **Build 303:** those baked, and **SH3_FIT_LOCAL=1** (rig only): the search only round the place before (rounds 1
  and 2, never round 0, never at every turn, never climbing off): the right-handed hold's place kept, moved only as
  far as her left hand needs. **vr542** (left-handed, SH3_FIT_SECOND=1 SH3_FIT_PLAY=1 SH3_FIT_LOCAL=1): the handgun's
  other hand {{-7.1, -12.2, -3.5}, -32, -28} (5.00; 0.2 mm, 1.7 off her main hand, wrist 56 as drawn: round the front
  of her main hand as the right-handed hold, natural); the submachine gun's front went back to the spread fingers
  ({{6.0, -11.0, 11.0}, -16, 4}); the katana's found nothing round the right-handed place (its index never meets the
  handle with the main hand turned 24 degrees off the right-handed one).
- **Build 304, vr543 (as played, no fit):** the submachine gun's front at the right-handed place itself, {{5.0,
  -14.0, 5.0}, -8, 8}: 0.5 mm, 3.4 off her main hand, cupped under the front with the fingers together up its side,
  as the right-handed hold: taken. (Its one-hand pull pictures there read the trigger finger 0.7 mm into the gun: they
  came in the first 80 frames after the gun was drawn mid-series, no by-bone reading yet; vr540's of the same hold,
  later, read 0.6 off. The final run takes them at their usual time.) **vr544** (SH3_GRIP_FIT=1 SH3_FIT_PLAY=1
  SH3_FIT_LOCAL=1, the katana from the right-handed main place): its main hand {{4.7, -1.5, 0.0}, 0, 40} (6.20; 0.3
  mm), its other hand at the right-handed place 3.5 mm in as played (round the end of the handle, natural), nothing
  clear round it (the ring finger never meeting). **Build 305, vr545:** 2 mm out, still 1.7 in. **vr546**
  (SH3_FIT_SECOND=1 SH3_FIT_PLAY=1 from there): {{-15.0, -15.5, -1.0}, -8, -12} (6.11; 0.3 mm, every finger closed
  round the handle's end behind the main hand, wrist 20 as drawn): natural, taken.
- **Build 306:** every left-handed place baked; the release checks checks547.sh (vr547 door, vr548 every weapon
  right-handed, vr549 left-handed, vr550 reload).
- **checks547 read:** vr547 (door) passed; vr548 the same as vr536 but the maul's other hand, now clear; vr550 (reload)
  its events line for line vr538's. vr549 (left-handed): every hold clear and natural by its readings (the handgun's
  and submachine gun's pulls -0.1 to -0.8 at the index as the fit reads it, the trigger at rest, 1.3 to 1.8 mm off the
  trigger as drawn; the shotgun's deep-pull crease -0.8 to -1.2 inside the closed hand, as right-handed; the submachine
  gun's pulls now 0.6 mm, vr543's -0.7 was the transitional frame; 4 trigger lines, the handgun twice, the shotgun, the
  submachine gun, which vr537 lacked) **but the maul: its handle's end, a metal cap, out through her left forearm 4.5
  cm behind the wrist** in its "1down" pictures from her eyes (her wrist bent 102 degrees as drawn, right-handed 60).
  No reading saw it: her hand's skin as taken stops at the wrist.
- **Builds 307-310: her forearm read against the weapon** (rig only, compiled out on the headset): sh3vr_combat.cpp
  ArmMesh, each forearm's own skin as drawn (half its weight or more on her forearm's bones, ik_forearm_of_bone in
  sh3vr_ik.cpp: their origins within 1.5 cm of her elbow-to-wrist line, not her hand's), and each picture's line
  "her forearm as drawn off the weapon at the nearest: N mm, M cm behind her wrist, ... (they reach A to B cm behind
  it)"; picsum.awk prints it as "arm N@Mcm" and flags ARM<0. Build 307 took only the joints' rings (her forearm's skin
  is on a bone 14% along from her elbow, 0.6 cm off the line: SH3_ARM_DIAG, vr553/554, since removed); build 310 all
  of it (188 vertices, wrist to elbow). **vr555/556 (every weapon, both hands): three in it:** the left-handed maul's
  main hand -10.0 mm at 4.5 cm (all 14 pictures), the right-handed flamethrower's main hand -6.4 at 6.7 cm (all 14),
  the left-handed beam saber's other hand -5.1 at 6.3 cm (its 6 two-hand pictures); every other one 1.0 mm or more
  off (the left-handed flamethrower's 1.0, the right-handed katana's other hand 4.6).
- **Build 311: SH3_FIT_ARM=1** (rig only): the fit turns away a place whose forearm, turned about her elbow to the
  place's wrist, is in the weapon (why 9; combat_forearm_clear; the fit's last line gives "her forearm as drawn N mm
  off it there ... places turned away for it N"). Its model is rough away from the place drawn (the maul's fit read
  0.4 mm, as played 20), so take its places by the pictures' reading. **vr557** (left-handed maul, SH3_FIT_LOCAL=1
  SH3_FIT_PLAY=1): its main hand {{6.1, -2.0, 4.0}, 12, -36} (6.00; 0.4 mm; forearm 20 mm off as played, wrist 31 as
  drawn; the shaft's end beside her forearm as on 0.1.9), its other hand from there {{-4.0, -15.2, 2.0}, 12, -8}
  (5.42; 0.4 mm, over 10 off her main hand); both hands natural in the pictures: **baked, build 312**. vr558 (the
  beam saber's other hand): kept (the fit's frame read 14 mm: the other hand not yet drawn there; its two-hand pictures
  -5.1, the hilt's end behind her right forearm, unseen from her eyes). vr559/vr560 (the right-handed flamethrower):
  nothing round build 292's place clears her forearm, and the wide search's one clear place holds the fuel canister:
  kept (its body's underside on the top of her forearm behind the wrist, unseen in every picture).
- **Build 312:** the release candidate; the release checks checks561.sh (vr561 door, vr562 right-handed, vr563
  left-handed, vr564 reload), every picture read whole:
  - vr561 (door): passed, as vr547.
  - vr562 (every weapon right-handed): as vr548 within the run-to-run 0.1-0.3 mm (the pipe's bone 0.5 to 0.7: which
    frame her vertices were sorted on); 4 trigger-on-trigger lines; the one forearm reading below 0 the flamethrower's
    (-5.4, unseen, kept above). The handgun: her hand 0.5 mm off by bone, 0.8 at the palm, the web under the slide.
  - vr563 (left-handed): the maul fixed (its main hand 0.4 by bone, the forearm 20 mm off, the wrist 31 as drawn;
    the other hand 0.5, over 10 off her main hand); the one forearm reading below 0 the beam saber's (-5.1, kept);
    the pull pictures' known index readings (handgun, shotgun, submachine gun) only. Trigger-on-trigger lines 4, as
    right-handed: the line comes once a trigger finger is fitted (the handgun twice, the second after her skin is
    taken again; the shotgun; the submachine gun, which vr537 lacked before its left-handed place; the flamethrower
    none in any run); session 37's 11 (vr497) were the handgun's one line repeated, the handgun alone drawn.
  - vr564 (reload): its reload events the same as vr550's line for line (numbers aside); pictures as before.
  - vr565 (checks565.sh, every weapon right-handed again): its readings as vr562's (none moved over 0.35 mm, the same
    31 known flags, 4 trigger lines). The handgun beside vr496: on 0.1.9 low on the grip, its wooden back strap out
    through the back of her palm; now high, the web under the slide, the grip inside her hand. Every weapon close up
    and two-handed beside vr530 (build 298) and vr536 (build 300): as build 300, the submachine gun's front cupped
    from below (build 298's over the top).

## Rig

- Testing switches added (rig only): SH3_FIT_RISE, SH3_FIT_RISE_FREE, SH3_FIT_RISE_W (body_grip_fit's trigger-rise
  limit and weight), SH3_FIT_PITCHES (the wide search's turns about the weapon's right), SH3_SKIN_FORGET_AT,
  SH3_FIT_CLIMB (the climb up a grip's line), SH3_FIT_SECOND (the other hand alone, the main hand kept), SH3_FIT_WRIST,
  SH3_FIT_WRIST_FREE, SH3_FIT_WRIST_W (the other hand's wrist limit: off by default since build 300), SH3_FIT_PLAY
  (build 301: the fit reads as played, without her main hand's drawn skin in the other hand's clearance; use it for
  every fit from now on) and SH3_FIT_LOCAL (build 303: the search only round the place before, rounds 1 and 2) and
  SH3_FIT_ARM (build 311: her forearm as drawn kept out of the weapon, why 9). Each picture's line now reads her
  forearm against the weapon (build 310; picsum.awk "arm N@Mcm", ARM<0).
- Picture labels are the script's, by frame (lefty384/pics383): they name the weapon only when SH3_EQUIP is the full
  list "1,2,3,8,4,5,6,7,9,10" with SH3_EQUIP_EVERY=600; with another list, map each picture to the weapon by the log's
  "VR: weapon N (...) in the hand" lines and the "grip fit:" lines before it (session 38's vr540-546).
- The pictures' log reader: `awk -f C:/SH3Quest/rigtools/picsum.awk vrN.log` (per picture each hand's nearest by bone,
  as the fit reads it, its wrist as drawn / against the controller, the trigger finger against the trigger as drawn,
  the other hand off her main hand; FIT<0 / BONE<0 flagged; the count of trigger-on-trigger lines). Montages:
  `python C:/SH3Quest/rigtools/montage.py out cols x0 y0 x1 y1 scale files...`.
- Scripts: C:/SH3Quest/checks516.sh to checks565.sh. **checks561.sh is the release check for build 312, 0.1.10 as
  released** (vr561 door, vr562 every weapon, vr563 left-handed, vr564 reload); checks565.sh every weapon again (one
  run, 4 min). Session 38's later ones: checks547 (build 306), checks551/555 (the forearm reading), checks553/554 (its
  diagnostics, since removed), checks557 (the left-handed refits with SH3_FIT_ARM), checks560 (the flamethrower's
  wide search). TaskStop on a checks script's shell leaves the script running (checks535 went on to vr537 and set
  left_handed = 1): kill the bash script by its pid (`ps -ef | grep checksN`), then sh3.exe, then restore
  gamedir/sh3vr.ini from C:/SH3Quest/sh3vr.ini.before-413 (it is the defaults now).
- Pictures kept: vr413-422 (baseline) and vr496 (the handgun's "before", 0.1.9's grip through her palm). Every other
  run's pictures were deleted once read (vr516-565; the last compare: vr565 beside vr530 and vr536, and the handgun
  beside vr496). Each run's readings stay in its log (C:/SH3Quest/vrN.log) and C:/SH3Quest/notes/picsumN.txt.
- Backups of the files before builds 299 and 300: C:/SH3Quest/notes/*.bak-s38b and *.bak-s38c.

## State at the end of session 38

- **0.1.10 is released from build 312** (the tree as committed; nothing in src/ changed after build 312, checked with
  find -newer). SH3QuestVR-0.1.10.apk: 9,680,214 bytes, SHA-256
  01d8919e62cd373901f11ec399503a266bbd275716463dc0ab325c98369f328a (C:/SH3Quest/SH3QuestVR-0.1.10.apk for the next
  release's apkcheck). apkcheck against 0.1.9: only the manifest (versionCode 11, "0.1.10"), whats_new.txt,
  classes.dex and libmain.so changed, the same release certificate 38811d96...cd416e6a. The Android build compiled the
  new forearm reading (rig only: arm_wanted is false on the headset) with no warnings.
- Release checks on build 312, every picture read whole: vr561 (door), vr562 and vr565 (every weapon right-handed),
  vr563 (left-handed), vr564 (reload). See "Build 312" above.
- Two forearm readings below 0 left as chosen, unseen from every view: the right-handed flamethrower's body 5.4 to
  6.4 mm into the top of her forearm under the gun, the left-handed beam saber's hilt end 5.1 mm behind her right
  forearm in the two-hand stance. HEADSET_CHECKLIST asks to look for them.

## Next

1. Players' reports and headset logs on 0.1.10 (the headset checklist's new items: every hold from her eyes, the
   handgun high under the slide, left-handed holds and the maul raised, Open doors by hand).
2. Should a player see a weapon in her forearm: the right-handed flamethrower and the left-handed beam saber are the two
   the rig reads (above); refit with SH3_FIT_ARM=1 and choose by the pictures as played (picsum.awk's "arm").
