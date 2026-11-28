# Handoff, 2026-11-27, session 37: owner 172, the gun through her hand (NOT fixed yet)

Read docs/HANDOFF-2026-11-27-SESSION36.md first. This session took its "Next, first" list.

**0.1.10 is not published, and nothing is committed.** The tree is the session 36 tree plus this session's changes
(builds 284-290). The owner stopped the session with "wrap up and do handoff" (176).

## Owner messages this session (in C:/SH3Quest/notes/owner-messages-sessions1-7.txt)

- 173: "pick up from where it left off. miss/forget nothing. make no mistakes. proceed with list"
- 174: "there are dozens if not hundreds of other vr bases that cracked the code on gripping a ... pistol man, lets
  get this going please. make no mistakes. proceed with list"
- 175: "HOOOOLY ... GET THIS WORKKING NOOOOOOW. proceed with list"
- 176: "wrap up and do handoff"

The owner is very angry that the pistol grip still clips. The next session must get it fixed with no more
open-ended searching (see "Next").

## What is now known (the cause)

- **0.1.8's handgun place has the grip through the middle of her palm**, not just its heel.
  - Seen in the rig's grip slices (rigtools/slices.py vr507.log out.png 0 3; SH3_GRIP_SLICES=1): in the hand's grip
    space the grip's section spans x 0 to 50 mm. Her palm as drawn spans x 0.0 to 5.2 cm (the right hand; y -5.3 to
    3.6 cm, knuckles at y -4.7).
  - The hand model's palm check (palm_points) samples only her knuckle row and a heel row 7.5 cm back, so a grip
    between them passed. The 0.1.8 fit (vr400) was searched against that check.
  - So the fitted places of the guns' main hands (fit_main) are wrong, by a lot. At the handgun's place her palm as
    drawn is 18.3 mm into the grip, and the weapon's distance field reads no deeper than about -19 mm. Moving the
    hand along her palm's back, it clears only after 44.9 mm (vr515).
- **Her hand as drawn vs the hand model.** Her drawn palm is 5.2 cm thick, the model's 2.6. Her heel bulges 1.8 cm
  past the model's palm face; near the knuckles they match. The finger bones match the model's to 0.00 mm.
- **build 277's settle_heel** (moving the hand out along the shape's gradient) failed for the same reason: the deep
  points are inside the distance field's floor, and the grip goes through the palm's middle.

## What was built this session (all uncommitted)

**Her whole hand as drawn, taken and posed exactly (builds 284-287). Used in play and by the fit:**
- combat_hand_mesh (sh3vr_combat.cpp) takes every vertex of her hand with its bones and weights (HandMesh it/iw/iq),
  and her hand's triangles.
- body_hand_skin (sh3vr_body.cpp) keeps them in the grip's space and on each finger segment (HandSkin, hand_skin[2]).
- hand_skin_pose poses them with any curls, as the game skins them. hand_skin_clear measures them against the weapon:
  every vertex, and triangles sampled 2 mm apart. Her wrist's skin on her forearm (kSkinWrist, 12 vertices a hand,
  3.2 mm off) is left out.
- ik_hand_of_bone matches a bone by its axes as well as its origin (2 degrees). Her forearm's twist bone shares her
  wrist's origin and was taken for her hand's own (10 mm off). With that fixed, the right palm poses within 0.35 mm
  and the fingers within 0.01 mm (vr511).
- **In play:** finger_touches, joint_gaps (finger_wrap) and thumb_rest use the segments' own drawn skin. So her
  fingers and thumb now stop on the weapon by her drawn skin, not the model's capsules. **This changes how the
  fingers look in play.** Not yet checked on pictures (pics383).
- A rig picture line: "her hand as this model poses it ... off her hand as drawn"; and the "... its vertex N most off"
  diagnostic.

**The fit (SH3_GRIP_FIT=1, rig only; none of this runs in play) (builds 287-290):**
- body_grip_mesh: a place the fit would take must have her whole hand as drawn 0.2 mm clear (kSkinCheck), with her
  fingers posed as body_fingers poses them (grip_drawn_curls; the trigger finger both on and off the trigger). Her
  palm's nearest past 2 mm weighs against the place.
- body_grip_fit, with her palm as drawn (palm_skin) taken:
  - "palm in": any point more than 1 mm in;
  - "palm against it": its nearest within 4 mm, and an eighth of its face within 1 cm (a tenth for the other hand);
  - the middle, ring and little fingers must meet the weapon between curls 0.12 and 1.0 (it was 0.3 to 0.95).
  - Without palm_skin it is as 0.1.8.
- GripFit::sub, and the fit log's second line: why each place was turned away (the curls, the trigger, the palm off).
- The fit's records (the grid round its place): now also across the grip, with her drawn palm's and whole hand's
  depth at each record place (body_grip_mesh(..., any = true)).
- fit_candidate: cup hands may sink 2.5 cm (was 0.8); a fist hand may stand 2.4 cm off its handle's line (was 1.2).
- fit_step, before the search: a hand not round a handle (the guns' main hands, every second hand) moves out the way
  her palm's back faces until her drawn palm is clear (body_palm_skin_clear; up to 12 cm), and the search runs round
  there.
- fit_step waits for her hand skin too (body_hand_skin_ready).

## The runs

- **vr511** (build 287): posing check. Palm 0.08/0.35 mm, fingers 0.01. The handgun place's whole hand: -19.1 mm.
- **vr512** (build 287, the fit on every weapon):
  - Fitted with her whole drawn hand clear: the handgun's other hand {{-5.9, -12.9, -4.9}, -20, -20} 5.92; knife
    {{7.3, -3.0, 6.5}, -8, 28} 6.23; katana {{-0.8, 0.3, -0.3}, -16, 36} 5.96; maul {{-4.9, 1.4, 0.2}, -12, -12}
    5.99; beam saber {{-6.2, 2.3, -0.6}, -16, 8} 5.65; the flamethrower's main hand {{9.0, -20.5, 10.0}, -4, 32}
    4.96.
  - No place: the main hands of the handgun, shotgun, submachine gun, stun gun and steel pipe; the front hands of the
    shotgun, submachine gun and flamethrower.
  - These were NOT baked: the main hands they were fitted with will move.
- **vr513, vr514** (builds 288, 289, the handgun alone): still no place. The records showed moving across the grip
  never clears her palm (it stays 16-19 mm in).
- **vr515** (build 290; the handgun, shotgun, submachine gun, stun gun, steel pipe and flamethrower; ended 18:18,
  exit 0):
  - The handgun's main hand moved 44.9 mm out along her palm's back before its palm cleared, and then **no place**.
    Places turned away: palm not touching 22925, a finger never meeting the grip 2590, the trigger's pad off the
    trigger 2421, knuckles in 9629, palm in 25346.
  - So with her palm clear of the grip, her fingers and trigger finger no longer reach round it, in the orientations
    the search tries (0.1.8's turn, pitch -4 and roll -52, ±32/±16 degrees).
  - The handgun's other hand moved 10.2 mm out first. It fitted at {{14.0f, -14.1f, -5.1f}, -36f, 0f} (5.91), but
    against the handgun's main hand at its unfitted place, so do not bake it until the main hand is fixed.
  - **The move-out works for the shotgun and submachine gun** (both new places, ready to bake, with her whole hand as
    drawn clear and the trigger finger on the trigger the whole pull):
    - shotgun main: moved 34.7 mm out, then `{{7.4f, -3.4f, 2.0f}, -28f, -24f}` score 5.70, trigger curl 0.37, its
      pad 0.6 mm off, whole hand 0.5 mm clear (her palm);
    - shotgun pump (other): moved 20.4 mm out, then `{{0.0f, 5.0f, -0.0f}, -16f, 0f}` score 5.29, whole hand 0.3 mm
      clear (blended skin). Its index curl is -1 (it never meets the pump): check the picture;
    - submachine gun main: moved 20.4 mm out, then `{{21.9f, -0.2f, 10.4f}, -8f, 0f}` score 5.64, trigger curl 0.54,
      its pad 0.1 mm off, whole hand 0.3 mm clear (her index finger);
    - submachine gun front (other): still -8.3 mm after 60 units out; no place (why 2). Its cup may need its own way
      out (down, under the gun);
    - stun gun main (a fist, so no move-out): `{{26.1f, -12.8f, 5.0f}, -32f, 16f}` score 6.57, whole hand 0.7 mm
      clear (her palm);
    - steel pipe main (fist; the 2.4 cm off-line limit): `{{-0.9f, -0.2f, 6.0f}, -12f, 20f}` score 5.94, whole hand
      0.4 mm clear (her palm);
    - flamethrower main: moved 32.7 mm out, then `{{44.6f, -5.6f, 8.2f}, 8f, 28f}` score 5.97, whole hand 0.5 mm
      clear (blended skin). The shift is large, so check the picture;
    - flamethrower front (other): moved 42.9 mm out, then `{{-7.4f, -10.6f, 0.1f}, 8f, 24f}` score 5.29, whole hand
      0.3 mm clear (blended skin).
  - Read the rest of vr515.log ("VR: grip fit: ") for the submachine gun's front hand, stun gun, steel pipe and
    flamethrower.

## Next (the owner wants this working, fast)

1. **Read vr515's other weapons.** Check sh3.exe is not running before any build. The shotgun's and submachine gun's
   main hands are solved by build 290's way (above).
2. **Fix the handgun main hand with one decisive approach**, not more open search. Its palm cleared only 44.9 mm out
   (the shotgun's 34.7, the submachine gun's 20.4), too far for its fingers and trigger. Two candidates, best first:
   - **Turn the hand round the grip, not only move it.** The search only turns ±16/±32 degrees round 0.1.8's turn,
     which was fitted with the grip through the palm. Find the hand's turn where the grip's section (about 3 x 5 cm,
     diagonal in the slices) lies in front of her palm's face, below her knuckles' line, with the long side along the
     palm. That is a real pistol hold: back strap in the web between thumb and index, front strap in the fingers.
     Derive this turn from the slices' geometry: the grip's line in grip space against her palm (x 0 to 5.2 cm) and her
     knuckles (y -4.7 cm). Then run the fit round that turn with her palm moved out (build 290's step). Check the
     records first, which give the trigger pad's distance at each place.
   - **If no turn reaches the trigger with her palm clear:** her drawn palm is 5.2 cm thick; a real palm is about
     3-3.5 cm. Thin her palm's skin while a weapon is held, by scaling her hand bone's palm side in the palette. This
     needs the owner's OK first: it changes how her hand looks, and the owner checks hands closely.
3. Then the fit on every weapon (checks512.sh's line: `run N pics383 1300 SH3_GRIP_FIT=1 SH3_GIVE="$GIVE"
   SH3_EQUIP="1,2,3,8,4,5,6,7,9,10" SH3_EQUIP_EVERY=600`, about 9 minutes). Bake fit_main and fit_second
   (sh3vr_combat.cpp about line 489) from the "grip fit:" lines with "her whole hand as drawn" 0.2 mm or more. Update
   their comments (vr number, build).
4. Pictures (pics383, every weapon, no SH3_GRIP_FIT):
   - every picture whole;
   - the by-bone report 0 or more everywhere;
   - "trigger finger on the trigger" lines no fewer than vr496 (4) and vr497 (11);
   - compare g0-handgun-1close with vr496's (kept as the "before").
   - The in-play finger change (their drawn skin) needs this check too.
5. Then session 36's release list:
   - notes for owner 172 (only once fixed and seen; RELEASE-NOTES-0.1.10.md and whats_new.txt do not claim it now);
   - versionCode 11 is already set;
   - the APK and apkcheck against 0.1.9;
   - SIZE/SHA in the notes;
   - commit, tag v0.1.10, `gh release create --latest`, check the digest;
   - re-check the door, grips, left-handed and reload on the release build itself first (checks498.sh pattern).
6. Open, not done: the second hand against her main hand as drawn (only the main hand's capsules are checked). A
   costume change does not re-take her hand skin.

## Rig and files

- Scripts: C:/SH3Quest/checks511.sh to checks515.sh (head -16 of checks498.sh plus run lines). The next run is
  **516**, the next build **291**.
- Pictures on disk: vr413-422 (the baseline) and vr496 (the "before" of the handgun). The pictures of vr495,
  vr497-500, vr502-504 and vr511-515 were deleted (superseded, or fit runs; their logs are kept). vr515 ended at
  18:18 (exit 0, sh3vr.ini the defaults).
- Testing switches (rig only): SH3_GRIP_FIT, SH3_GRIP_SLICES, SH3_PALM_DUMP, SH3_HEEL_SETTLE. settle_heel and
  body_heel_points change nothing in play (measure only).
