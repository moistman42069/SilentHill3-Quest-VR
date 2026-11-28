# SH3 Quest VR handoff, session 36 (2026-11-27): 0.1.10 in progress (grips, doors by hand; the gun through the hand still open)

Repo C:/SH3Quest/SH3QuestVR. HEAD 9e2f7b1 (session 34's handoff); 0.1.9 = a4c33e3 (tag v0.1.9, versionCode 10).
**Nothing committed**: the whole of 0.1.10 so far is the uncommitted tree (14 files changed, plus 3 new files). See
"The tree" below. Release 0.1.10 (versionCode 11) is NOT built, committed or published yet. The owner said to publish
without asking once it is done (owner 123). Owner messages 169-172 are logged in
C:/SH3Quest/notes/owner-messages-sessions1-7.txt, with what each led to.

Session 35's work (owners 167-168) is in 0.1.9. Its game facts are in GAME_FACTS "Session 35". This session is 36.
GAME_FACTS "Session 36" has the game's side of everything here. AUDIT rows 202-205 have the owner's asks.

## Owner 169: the pistol grip ("all grips should be perfect ... no clipping, while still keeping finger on trigger")

The clarifications:
- "keep the finger trigger work you did, like keep it so the finger can still touch the triggfer". So the index is
  NOT forced onto the trigger. Its 0.1.8/0.1.9 logic is untouched: it goes on the trigger while the player touches or
  pulls the controller's trigger, and takes the off-trigger rest otherwise.
- Then: "make NO mistakes this time."

**The video (D:/Downloads/dev.sh3quest.vr-20261127-145450-0.mp4, 0.1.9) and the rig (vr413):** every joint of a finger
bent by one curl, so each finger stopped the moment any part met the weapon. On the handgun, the ring and little
fingertips touched the grip while their middle segments stood 10-16 mm off it.

**0.1.10 code (build 274 on):**
- **The wrap solver** (sh3vr_body.cpp, around lines 850-990, in the anonymous namespace):
  - `finger_points3` (per-joint curls) and `joint_gaps` (each segment's skin from the weapon, via weapon_clearance).
  - `joints_clear`, and `wrap_cost`: the middle and last segments' gaps, the first segment's at half, each counted to
    15 mm; a penalty for the knuckle bent more than 0.15 past the middle joint; 3x each joint's change from c.
  - `finger_wrap`: the knuckle from c-0.05 to c+0.35, the middle joint from c-0.25 to c+0.45, the last joint bisected
    to the most closed clear value. A lower-bound prune skips candidates that cannot win. The path from c is checked
    at quarters.
  - `wrap_of`: one cache per hand and finger. It is kept while the weapon_shape_key, role, knuckle and tip model points
    (within 0.5 units) and contact curl (within 0.02) stay the same, and the cached pose is rechecked clear every
    frame. At most ONE new solve per hand per frame.
  - Logging: each solve logs "closed joint by joint ... solved in N us" (the first 64). `wraps_solved[h]` is counted
    in the rig's picture lines.
- **Drawing:** `finger_joint_off[2][5][3]` (sh3vr_ik.cpp) holds each joint's curl over the finger's curl.
  - `pose_fingers` applies it; arm_to passes it, and passes none for ghost hands.
  - body_fingers:
    - eases it in, scaled by how settled the finger's curl is (a 0.1 band);
    - zeroes it at once for a hand on the world;
    - zeroes it when the hand is invalid.
- **The other hand:** `body_hand_capsules` uses the same per-joint pose (its cache key is now 29 floats), so the
  second hand wraps what is drawn.
- **The thumb and trigger finger** are never wrapped; for the trigger finger, `joint_target[kIndex]` stays 0 whenever
  its trigger logic runs.
- `weapon_shape_key()` is in sh3vr_combat.cpp.

**Results (vr493, build 275, every weapon):**
- The handgun's middle, ring and little fingers came out 0.68/0.58/0.56, 0.73/0.58/0.20 and 0.77/0.67/0.06. Their
  segments are 0-10 mm off the grip, none in it.
- About 9 solves per hand per weapon, 60-80 us each on the PC (estimate 0.3 ms on the Quest, once per weapon).
- The pictures show the fingers tucked tighter round the front strap than vr413, with no finger in the gun.
- Noted, not changed: the trigger finger's off-trigger rest is drawn 2.5-2.9 mm into the frame in the picture report
  (I [-2.5 0.7 -2.9]). This is 0.1.9's own work and was never wrapped. Check whether it shows in the pictures. If it
  does, the off-trigger rest (body_trigger_rest) needs a clearance margin.

## Owner 172: "WHY ... IS THE GUN STILL CLIPPING THROUGH THE HAND ... FIX THIS"

This came on build 275's pictures, which the owner saw in the transcript.
- **What the picture showed:** vr493's g0-handgun-1close, from the gun's right side. The grip's back strap runs across
  the rear of her palm by the wrist: the sleeve ends at the back strap, and the rear ~2 cm of the hand is hidden
  inside the right grip panel.
- **Cause:** 0.1.8's fitted places (fit_main, the search's palm_points) kept the palm out of the weapon only to 7.5 cm
  back from the knuckles. Her palm runs on about 2 cm more.
- **Attempted fix (build 276). It DID NOTHING: every weapon was "left as fitted". See "The results of checks495".**
  - `body_heel_points` (sh3vr_body.cpp): the palm's face and back (±1.3 cm) on each finger's line at 7.5, 8.5 and
    9.5 cm back, plus the outer edge. It returns 0 until her hands are read.
  - `settle_heel` (sh3vr_combat.cpp, called after place_weapon): runs once per weapon and handedness, at the first
    frames the fitted place applies. Where any of those points lies within 1.5 mm of the weapon's shape (or inside),
    it moves the main hand out along the shape's outward gradient at the deepest point (at most 8 steps, 3 cm in all).
  - The result is stored in `heel_extra[profile][left_handed]`, and `main_fit` adds it to the fitted shift, so the
    hand is drawn there and the weapon held so. The fit search (SH3_GRIP_FIT=1) gets the raw places.
  - Logged: "<weapon>, the main hand's place (right-handed): the rear of its palm N mm in the weapon ... the hand moved
    N mm out of it", or "left as fitted".
  - The rig's picture lines end with "the palm's rear N mm off it".
- **Verified as not working**: see "The results of checks495".
- **Risk to check:** moving the hand out can take the trigger finger out of the trigger's reach (body_trigger_curl
  -1). sh3.log would then show no "trigger finger on the trigger" line for that gun, and the index would fall back to
  0.1.7's curls. The fingers and thumb re-close each frame from the new place. The second hand on the handgun wraps
  the main hand's capsules, so check g0-handgun-2close.
- **If translation alone is not enough or loses the trigger:** turn the hand about its index knuckle instead. Or, the
  real fix: add body_heel_points to the fit search's score (body_grip_fit) and re-run the search (SH3_GRIP_FIT=1),
  which is a multi-hour job, so it needs the owner's OK.

## Owners 170, 171: open doors by hand ("grabbing the knob/handle ... pulling it open ... for all doors", "*on by default")

How it works:
- **The door test** (sh3.c `sh3_event_is_door`): a button event (trigger 1) with field 17 set (721 of the stage
  tables' 2429 button events) or one of key_doors. sh3.c passes it to VR with each hover:
  `sh3vr_hand_hover(h, ev, door)`, with the signature changed in sh3vr.h and sh3vr.cpp, and `hand_hover(h, ev, door)`.
- **Unknown doors:** a room's load (the dispatch returns mode 2) after a button event the test does not count is
  logged: "a room's load after event N ... which the doors' test does not count".
- **Taking the handle** (sh3vr_combat.cpp `DoorHold`, `update_grabbing`): a free, reaching hand on a door (with "Take
  items with your hand" on) takes the handle. It gives a touch sound (kSoundHandSurface) and a buzz, and logs "took
  door event N's handle".
- **Opening** (`update_door_hold`): moving the controller 10 cm level while holding opens the door ("pulled it toward
  her", "pushed it away" or "slid it aside"). It runs through the existing hand_action path at the handle's place: the
  game's own door, sound, fade and load.
- **No open:** letting go (grip under 0.35), holding 6 s, or the hand carried 50 cm off it (walking away) opens
  nothing ("let go of door event N's handle without opening it").
- **Guards:**
  - one door at a time;
  - a re-squeeze while holding is ignored (review fix);
  - the opening waits for the other hand's action to end (review fix).
  - hand_reach_point skips the holding hand.
- **The hint** ("door" in sh3quest_tips.ini): "A door: grip its handle and pull it toward you (or push it away) to open
  it". It shows once, as soon as the label is free while a hand is on a door (build 276 tries every frame; build 275
  only on a new hover). The Hints page lists "Doors - grip its handle and pull it toward you (or push it away) to open
  it".
- **The option:** `door_by_hand` defaults to on (sh3vr_internal.h, sh3vr.cpp SW, Options.java with its description).
  Its key is not written into sh3vr.ini at its default, so sh3vr.ini.before-413 is unchanged (ini492.diff empty).
  - Off: a closing hand uses a door at once, as on 0.1.9.
  - It needs "Take items with your hand", because the hover comes from it.
  - A still opens doors at once.
- **Honest limit:** the door cannot swing in the picture (the room's geometry has no open door). The screen fades to
  the next area as with A. This is said in the notes ("Known for now").
- **Verified on vr492 (build 275, tools/sim/door275.txt, stage 2's entrance, event 5):**
  - hover → "took door event 5's handle" → let go → nothing opened;
  - the hint shown, whole in its label;
  - took it again → "pulled it toward her 15.0 cm" → "used event 5" → mode 2 load → the next area;
  - pictures checked whole, then deleted.
- **Not tested on the rig:** a locked door by hand (stage 2's event 48 needs key 38). A locked door should give the
  game's "locked" message.

## The code review (subagent, all fixed in build 276)

1. `#include <chrono>` was inside `#if !defined(__ANDROID__)`, so the APK build would have failed. It is now with the
   other standard includes.
2. A grip eased to 0.35-0.6 and squeezed again while holding a door fell through to the old instant open, and a pull
   could then fire twice. `update_grabbing` now skips a hand that holds a door.
3. Opening a door could overwrite the other hand's running action. It now waits until `hand_action.frames == 0 &&
   grab_frames == 0`.

The review checked these as right: the C prototype order, every hover caller, kTipKeys' 10 entries, the solver's
bounds and prune, no solve starvation, the trigger finger untouched, and body_hand_capsules' key.

## Builds and checks

- **Build 274:** wrap v1. vr490 (the handgun only) showed the old cost kept the uniform pose. Pictures deleted.
- **Build 275:** wrap v2 and the door. checks492.sh:
  - vr492 door275: passed (above), pictures deleted;
  - vr493 pics383, all 10 weapons: the wrap numbers above, and owner 172's finding. **Its pictures are still in
    C:/SH3Quest/shots/vr493, kept to compare with vr496. Delete them after.**
  - vr494 lefty384: superseded, pictures deleted unchecked.
- **Build 276** (current, build_pc_276.log: BUILD OK, no warnings): the review fixes, settle_heel, the hint each
  frame. **checks495.sh** started 15:48:19 (vr495 door275, vr496 pics383 all weapons, vr497 lefty384 handgun, about
  11 minutes in all). Results at the end of this file.
- **What to check on 276:**
  1. sh3.log "the main hand's place" for every weapon: how far each was moved.
  2. Every weapon's 1close/1left/1right/1down/2close pictures whole against vr493 and vr413: no grip through the palm
     or wrist, fingers round the grip, the trigger finger on the trigger as before ("trigger finger on the trigger"
     lines, pull pictures).
  3. Left-handed (vr497) against vr422.
  4. vr495: the door hint at the first touch now.
- The rig: checks*.sh `run N script timeout env...`; pictures in C:/SH3Quest/shots/vrN; logs in C:/SH3Quest/vrN.log.
  Tools: C:/SH3Quest/rigtools/montage.py (`out cols x0 y0 x1 y1 scale in...`), dirdiff.py.
- The baseline: vr413-422. checks495 resets "door = 0" in gamedir/sh3quest_tips.ini before the door run.
- The scripts restore sh3vr.ini from sh3vr.ini.before-413 after the left-handed run.
- PC build: `cd /c/SH3Quest/SH3QuestVR && timeout 900 cmd //c "tools\\build_pc.bat" > /c/SH3Quest/build_pc_N.log 2>&1`
  (next: 277). Never build while sh3.exe runs.

## The tree (uncommitted)

- **src/game/sh3.c:** sh3_event_is_door, the room-load log, and the door flag on the hover call (forward declarations
  of event_trigger and sh3_event_is_door near pickup_watch's).
- **src/vr/sh3vr.h, sh3vr.cpp:** sh3vr_hand_hover(hand, event, door); the door_by_hand switch.
- **src/vr/sh3vr_internal.h:**
  - finger_joint_off, weapon_shape_key and body_heel_points;
  - door_by_hand = true;
  - hand_hover(h, ev, door);
  - kTipDoor.
- **src/vr/sh3vr_body.cpp:**
  - finger_points3, the wrap solver and wrap_of, body_fingers' wrap;
  - body_hand_capsules' key, body_heel_points;
  - the picture lines (joints drawn, wraps solved, the palm's rear);
  - `<chrono>`.
- **src/vr/sh3vr_ik.cpp:** finger_joint_off, and pose_fingers' per-joint offsets.
- **src/vr/sh3vr_combat.cpp:**
  - weapon_shape_key; heel_extra, main_fit's adjustment and settle_heel;
  - DoorHold, update_grabbing's door branch and update_door_hold;
  - hand_reach_point's skip, and hand_hover's door and hint.
- **src/vr/sh3vr_tips.cpp:** the "door" key, door_line, kTipDoor's case, and the Hints page line.
- **android/.../Options.java:** door_by_hand (on).
- **tools/sim/door275.txt** (new).
- **Docs:**
  - RELEASE-NOTES-0.1.10.md (new; drafted by a subagent from 0.1.9's and checked). Its Files section still has
    SIZE_TBD / SHA_TBD.
  - whats_new.txt and README.md (0.1.10 items).
  - GAME_FACTS "Session 36".
  - AUDIT rows 202-205 (205's state: to be checked on 276).
  - HEADSET_CHECKLIST: two 0.1.10 entries (the fingers, the door).
- **To add to the notes once 276 checks out:** the palm-out-of-the-gun fix (owner 172) in the grip bullet of
  RELEASE-NOTES-0.1.10.md "What's new" and in whats_new.txt (for example: "The gun's grip no longer passes through the
  rear of her palm by the wrist: each hand's place on a weapon now keeps the whole palm out of it, back to the
  wrist"). Also a checklist line for it.
- **Not bumped yet:** android/app/build.gradle versionCode 10 / versionName "0.1.9" → 11 / "0.1.10".
- **Backups** of the files as session 36 found them, in C:/SH3Quest/notes: owner-messages-sessions1-7.txt.bak-s36,
  sh3vr_body.cpp.bak-s36, sh3vr_combat.cpp.bak-s36, sh3vr_ik.cpp.bak-s36, sh3vr_internal.h.bak-s36 (git diff shows
  the rest).

## Next

1. Read checks495's results (vr495-497 logs and pictures, every picture whole). Fix whatever they show, as one more
   build and one combined short run (the owner's ~30 min rule).
2. Fill in the notes for owner 172, bump versionCode 11 / 0.1.10.
3. Run the release: the APK (`source /c/RACQuest/env.sh; C:/SH3Quest/venv/Scripts/python.exe -I tools/build_quest.py
   --release`), `python -I tools/apkcheck.py` 0.1.9's APK vs the new one, size and SHA into the notes' Files section.
4. Commit, tag v0.1.10, `gh release create --latest` with the APK, check the uploaded digest.
5. Write the handoff, update the memory file (sh3-quest-idea.md).
6. Clear as you go: delete vr493 and vr495-497 pictures once checked (keep vr413-422).

## The results of checks495 (build 276)


checks495 ran 15:48-15:58, all three exited 0, and sh3vr.ini is the defaults again (ini495.diff empty).

- **vr495 (door275):** the same as vr492. "took door event 5's handle" → let go without opening (moved 0.0 cm) → the
  hint (frame 8097, after the let-go: the label was not free before) → took it again → "pulled it toward her 15.0 cm"
  → "used event 5" → mode 5 -> 2 → the next area. Pictures not looked at (the log is the same as vr492's, whose
  pictures were checked).
- **vr496 (every weapon): THE GRIP STILL PASSES THROUGH HER HAND. Owner 172 is NOT fixed.**
  - settle_heel left every weapon as fitted: the model's palm rear is clear of each. Handgun 12.3 mm, shotgun 9.4,
    submachine gun 19.3, stun gun 35.5, knife 54.1, steel pipe 36.6, katana 48.2, maul 59.2, beam saber 50.5,
    flamethrower 43.6; left-handed handgun 12.7 (vr497).
  - rigtools/dirdiff.py vr493 vr496: 598 of 656 pictures identical, g0-handgun-1close among them. The other 58 are
    the pulls, flashes and similar (not looked at).
  - So the palm as VR models it (finger_points' knuckles, ±1.3 cm face and back, back to 9.5 cm) is NOT where her
    drawn hand mesh is. The drawn mesh near the wrist is wider, thicker or placed differently, and the grip passes
    through that.
  - The one model point within 1 mm: the submachine gun's second (left) hand, -0.8 mm, in g2-smg-2eyes and 2close.
  - "trigger finger on the trigger" lines: 4 in vr493 and 4 in vr496 (unchanged); 11 in vr497 (left-handed).
- **vr497 (left-handed, the handgun):** run, not looked at.
- **Pictures left on disk** (C: is 92% full, about 44 GB free): vr493, vr495, vr496, vr497. Check vr497 against vr422
  and vr496's 58 differing pictures, then delete all four (keep vr413-422).

## Next, first (owner 172 is still open, and it is the owner's top complaint)

1. **Measure the drawn hand itself, not the model.** At a rig picture, take her skinned hand's vertices as drawn:
   the hand and finger bones' vertices of her model, skinned with the frame's bones, in tracking space. d3d8.c draws
   her model; session 14's skinning notes are in GAME_FACTS. Test each vertex against the weapon's shape with
   weapon_clearance, and log the deepest vertices with their bone and their place in grip space. That shows exactly
   which part of her hand the grip goes through, and by how much.
2. **Then feed settle_heel those real vertices** (or the vertex set's bounds per bone) in place of body_heel_points,
   which found nothing. Or, if the mesh's palm differs from palm_points everywhere, correct palm_points and the
   capsules (body_hand_capsules) from the mesh. The fits (fit_main) were searched against palm_points, so the fit
   search may need re-running on the corrected palm (SH3_GRIP_FIT=1, multi-hour: needs the owner's OK).
3. Until then, settle_heel and body_heel_points are in the tree and change nothing (every weapon "left as fitted"):
   keep them for step 2 or remove them. **Do not claim the palm fix in the release notes:** RELEASE-NOTES-0.1.10.md and
   whats_new.txt do not mention it now, and AUDIT row 205 says it is open.
4. Then the "Next" list above from step 2 (notes, versionCode 11, APK, commit, tag, publish), every check re-run on
   the release build itself (vr495-497's door, grips and left-handed, plus whatever step 1 changes).
