# SH3 Quest VR handoff, session 33 (2026-11-27): 0.1.8 checked on the release code and released

Repo C:/SH3Quest/SH3QuestVR. 0.1.7 = 703f432 (tag v0.1.7, versionCode 8). Session 33 picked up session 32's Next list
(docs/HANDOFF-2026-11-27-SESSION32.md) on owner message 158: "was working in another session. pick up from where it
left off. miss/forget nothing. proceed with list". Read sessions 30-32's handoffs for what 0.1.8 is; this one says
how it was checked and released.

## The checks on build 267 (the release's game code), the rig's way

C:/SH3Quest/checks267.sh (vr401-406) and checks267b.sh (vr407-409); the logs are C:/SH3Quest/vrN.log, the pictures
C:/SH3Quest/shots/vrN.

- **vr401 pics383, the release's own way (no SH3_GRIP_FIT): every picture checked whole, no fault.** The handgun: the
  pad on the trigger through the pull, the blade seen turning from the left, the other fingers wrapped, the thumb along
  the frame; two-handed the support hand cups the main fist. The shotgun: the trigger finger in the guard, the pump hand
  from below. The submachine gun: the finger on the trigger, the other hand cupping the receiver's front. The stun gun:
  her fist on the box's rear half (vr400's slide to the prongs gone). The knife: every finger on the handle. The pipe
  and katana: the other hand mirrored below the main hand on the handle. The maul: the other hand on the shaft (vr400's
  hand on air gone). The beam saber: both hands on the hilt. The flamethrower: her fingers in the gap before the tank,
  the other hand on the body's front. Log: no crash or error; all ten "own shape ready"; the trigger lines "its pad on
  the blade the whole way" (handgun curl 0.47 spread -16, shotgun 0.16 / -4, submachine gun 0.37 / -8).
- **vr402 beam401 (owner 155/156, the beam saber's beam):** lit, the beam leaves the emitter along the hilt's line
  held, ready, in the swing, forward and up, across, from the left, and two-handed from the right and the left;
  "beam saber's second grip held". (w07760's view from the right framed the saber off the picture: the camera, not the
  saber.) The rig's swings there are one-frame jumps, so they log "0.00 m from its start": not a swing test (vr409 is).
- **vr403 beam401 with SH3_GRIP_FIT=0 (0.1.7's places):** the beam on the very same line as vr402 in every pair
  (across, lit from the left, forward and up, two-handed): the hilt sits along the controller where it did on 0.1.7.
- **vr404 parts384:** the empty handgun's slide held back, from the eyes, close from both sides: the trigger stays with
  the frame (0.1.7's two faces no longer go back with the slide), pulled dry her finger closes with the blade and
  comes back released. **Its shotgun part tests nothing of the pump:** the shotgun there is loaded, so no pump is due
  (sh3vr_reload.cpp: the pump moves only while needs_action, after shells go into an empty gun), and its 1 cm moves are
  far under the 6 cm travel; rest, "back" and forward show the pump hand at rest alike. parts384's header now says so;
  the pump is worked for real in the new tools/sim/pump410.txt (vr410, below).
- **vr405 event391 (the release code, SH3_SPEND_TEST=7700:8:69):** item 69 given at 7700 and still in the inventory
  through the walk, the hands' reach and the hints' checks to the counter; A at 7839 found event 8 where the player
  looks, the game's own check used item 69 ("no longer in the inventory", event step 2, event 8) and the event ran. She
  stood through the costume's three pages with the stick held (no "her walk" line from 7839 to 8231: her place
  -17334 21335 kept), the hold over at 8231 and walking at once (8235: 50 units a step again). The pictures: the
  message in place by the costume, the view unchanged through the pages, walking again after. The item display: opened
  by the test, nothing drawn (the rig's rooms have no give), the route dump at 8540: 1038 draws, none inside the
  dispatch (as vr396). No crash or error line.
- **vr406 lefty384 (left_handed = 1, put back to 0 after):** all 164 pictures read, weapon by weapon (C:/SH3Quest/
  m406-L0..L9.png): every weapon in the left hand, the trigger lines as the right hand's (handgun curl 0.46 spread 16,
  shotgun 0.16 / 4, submachine gun 0.36 / 8, "its pad on the blade the whole way"), the right hand on each second grip
  (the pipe's, katana's, maul's and beam saber's mirrored on the handle, the shotgun's pump from below, the handgun's
  round the main fist). All ten "own shape ready". The submachine gun's other hand seen from its fingers' side looks
  like straight fingers standing over the receiver (L2-smg-2close, and vr401's g2-smg-2close the same from the right):
  the fingers' bend (curls 0.53-0.69 logged) cannot show from the back of the hand, so vr411 looks at it side-on.
- **vr407 hands407 (grip_anywhere = 1, the submachine gun's spot 4 cm forward; both files put back):** the shotgun's
  other hand took hold 14 cm ahead of the pump ("took hold of shotgun away from its second grip", wrapped round the
  barrel and magazine tube from below); the submachine gun's moved spot: "the left hand wraps its part there", round
  the barrel shroud's front. The handgun took nothing: hands407 put that hand 5 cm under the slide's front, out of the
  5 cm reach (the hand stayed an empty fist): the test's place, so vr412 retakes it at the slide's height.
- **vr408 ini408 (a 0.1.7 Version=2 file: Handgun MoveForwardCm=2, SecondGripForwardCm=3, Katana MoveUpCm=1; grip_spot
  = 3):** "was 0.1.7's ... put back", "every weapon's two-hand grip is back at its natural spot", grip_spot back to 0,
  Version=3 written, both files put back. **FAULT: the katana's MoveUpCm came back 0.** The migration reset the moves of
  every weapon with kFingerHandOn, which 0.1.8 turned on for all ten, so the melee weapons' moves went too, against
  its own log line, the notes and the checklist. **Fixed (build 268):** sh3vr_combat.cpp load_calibration resets only
  the guns' and the stun gun's (0.1.7's finger hand: kGun, kStun); the log line, the notes, the in-app what's new and
  the checklist now say "the guns and the stun gun".
- **vr409 melee259 (Swing strength needed at Normal, the default):** two counted swings each for the steel pipe,
  katana, maul (3.60 m/s asked), the stun gun (2.40) and the handgun as a club (3.36), from the real 5.6 m/s swings;
  none from the gentle waves or the moderate moves (early or late frames); the stun gun's two hand jumps "ended without
  the game's attack".
- **vr410 pump410 (new):** the shotgun empty, two shells pushed in by hand ("the pump next"), the pump taken: drawn
  back it slides along the barrel to the receiver (the 6 cm gap between them closed, s2 against s1), the hand with it,
  sound "pump back"; pushed forward it is home again, "the shotgun's pump worked: it fires". 0.1.7's pump went toward
  the muzzle: fixed and now seen.
- **vr411 smg411 (new: the submachine gun's and the flamethrower's other hand side-on; checks267e.sh):** from the eyes,
  in front, both front sides, above and below (C:/SH3Quest/m411*.png). The submachine gun's other hand wraps over the
  receiver's top, the knuckles about 2 cm above it, the fingers bent down its far side and the thumb forward along the
  near side: vr406's "straight fingers" were the back of a bent hand. The flamethrower's other hand wraps the body's
  front the same way. Left-handed (vr406's L2/L9) is the mirror (one eye against the other eye).
- **vr412 hands412 (new: the handgun held anywhere at three places by the slide, grip_anywhere = 1):** all three took
  hold ("the left hand took hold of handgun away from its second grip", its palm up under the frame), the hand closing
  under the dust cover round the front of the trigger guard, the main hand's finger on the trigger untouched
  (C:/SH3Quest/m412*.png). With vr407 that is every gun held away from its spot: the shotgun's barrel, a moved
  submachine gun spot, the handgun's frame.

## Build 268 (the release's game code: build 267 + the migration fix) checked again

C:/SH3Quest/checks268.sh (vr413-422, about 30 minutes, both ini files backed up to *.before-413 and put back):

- **vr413 pics383:** all 663 pictures against vr401's (build 267, every one read whole) with C:/SH3Quest/rigtools/
  dirdiff.py: 241 identical, 372 under 20 pixels apart, the rest the muzzle flashes, smoke and ejected shells of the
  pull-100 shots and the floor's shimmer (pairs seen side by side: m413a.png, m413b.png): the same hands. All ten "own
  shape ready"; the trigger lines as vr401's. No crash or error.
- **vr414 ini408 (the fix):** "the guns' and stun gun's moves and turns (1 set) and the two-hand grip spots (1 set)
  put back ...; sizes, aims, guide dots and the melee weapons' kept"; sh3quest_weapons.ini.after-414: Version=3, the
  katana's MoveUpCm=1 KEPT, the handgun's MoveForwardCm and SecondGripForwardCm back to 0; grip_spot = 0; both files
  put back.
- **vr415 event391:** item 69 given at 7700, kept to A at 7845 (where the player looks, event 8), then "no longer in
  the inventory" at event step 2 (the game's own check used it); she stood at -17334 21335 from 7847 to 8237 and walked
  at once after; the route dump at 8540: 368 draws, none inside the dispatch. Pictures as vr405's (m415.png).
- **vr416 beam401:** "beam saber's second grip held"; the 56 pictures as vr402's (the beam's own flicker only; m416.png:
  held, ready, lit from the left, two-handed: the beam on the hilt's line).
- **vr417 melee259:** the very counts of vr409: two counted swings each (pipe, katana, maul 5.57/5.58 m/s against 3.60;
  stun gun 5.00/5.04 against 2.40; the handgun as a club 5.38/5.40 against 3.36), none for the waves; the pictures the
  same weapons in hand (m417.png).
- **vr418 pump410:** two shells in, "pump back", "pump forward", "the shotgun's pump worked: it fires"; the pictures as
  vr410's (differences the flash's glow only).
- **vr419 reload (SH3_TEST_EMPTY_FIRE=1):** all four by hand: the handgun 20 -> 10 + 10 and "the handgun's slide
  worked: it fires", the shotgun two shells (12 -> 2 + 10) and "the shotgun's pump worked: it fires", the submachine
  gun 60 -> 32 + 28 and "the submachine gun's cocking knob worked: it fires", the stun gun's battery 3 -> 3 + 0: as
  vr340-vr366.
- **vr420 hands407, vr421 hands412 (grip_anywhere = 1):** every picture identical to vr407's and vr412's to the pixel;
  the same holds logged (the shotgun's barrel, the moved submachine gun spot, the handgun's frame at three places).

## Owner 159 and 160 (mid-session)

- **159**, a second player on 0.1.7: "I have the key for the store, but when I stand in front of the door, it's gone
  from my inventory"; "i dont want anything similar happening anywhere else in the game. proceed with list." 0.1.8's
  kind-1 looks are the fix (vr396, vr405, vr415). Checked further: every game function VR calls or wraps against every
  function that writes the inventory (the new tools/callgraph.py over src/recomp/gen; docs/GAME_FACTS.md "Session 33"):
  only the action check 0x004B9940 takes items away, and VR reaches it only through action_check_at (kind 1, the
  inventory kept and put back); the equip writes the equipped slot only; the reload and a shell pushed in move rounds,
  none lost (vr419's counts add up); a shot takes a round only when the game fires. AUDIT row 189.
- **160**, a player's YouTube video of 0.1.7 (18:27, Quest 3; chapters "VR gameplay", "VR settings"): read in the
  built-in browser, a frame every 30 s, the doubtful moments second by second, and the settings pages whole. Fine: the
  hints and messages ride their own layer over the world (2:08-2:15, the doll close: white text over the pale doll reads
  poorly, as in the flat game, but nothing covers it); the phone call, Douglas, the mirror and the store's cutscenes from
  the game's camera; picking up by hand with the wrist bar; the two-handed handgun; the options page only tilted (each
  value on its row). Their settings: 120 Hz, dynamic resolution on, AppSW off, foveated Medium, CPU and GPU boost on;
  melee damage 200%. **Not settled: 11:40-11:45**, as the store's cutscene starts after the fight, the picture shows
  through a small window at the top (a trapezoid, its bottom edge curved) that grows for about 4 s, then the cutscene
  with its bars as normal. Whether the game's own opening or ours cannot be told from the video; the rig has no save
  there. Next: a save at the clothes store's fight (or the PC game flat at that spot) to see it.
- **vr422 lefty384 (left_handed = 1, put back to 0):** the 656 pictures both runs share against vr406's (every one
  read): 232 identical, the rest the flashes and smoke, the floor's shimmer (20-48 scattered pixels) and a reload hint
  vr406 still showed (the hints are once ever; vr406 has 7 more panel pictures for it): m422.png, m422b.png. The trigger
  lines as vr406's (curl 0.46 / 16, 0.16 / 4, 0.36 / 8); all ten "own shape ready"; no crash or error. "all done 268";
  sh3vr.ini back (left_handed = 0, grip_spot = 0, no grip_anywhere) and sh3quest_weapons.ini back.

## 0.1.8 released

- The APK: `source /c/RACQuest/env.sh; python -I tools/build_quest.py --release` (log C:/SH3Quest/build_quest_018.log:
  sh3vr_combat.cpp rebuilt, libmain.so linked), then tools/apkcheck.py against 0.1.7's: 22 files each, changed only
  the manifest (versionCode 8 -> 9, "0.1.7" -> "0.1.8"), whats_new.txt, classes.dex and libmain.so; libSDL2.so and
  libopenxr_loader.so byte-identical; both signed with certificate 38811d96...cd416e6a; ALL CHECKS PASS. libmain.so
  carries the migration's new log line and "Hold weapons anywhere".
- **SH3QuestVR-0.1.8.apk: 9,613,170 bytes, SHA-256 71dd5e643e86e6a84191f23a7eef7499673466e73740fbc0d510525ba1d52682**
  (C:/SH3Quest/SH3QuestVR-0.1.8.apk).
- Commit 5465fc8 on main (message C:/SH3Quest/commit_018.txt), annotated tag v0.1.8 (7297e76), both pushed; the
  release "Silent Hill 3 Quest VR 0.1.8" with docs/RELEASE-NOTES-0.1.8.md and the APK, marked latest. Checked after:
  releases/latest is v0.1.8, its asset's digest sha256:71dd5e64...1d52682 and 9,613,170 bytes, and a copy downloaded
  with gh (C:/SH3Quest/dl018) hashes the same.

## 0.1.9: the key give-back (owner 161: "Yes, add it", "just make this a quick patch release", "regress nothing")

- The nine doors that use up a held key (eight keys) and each key's giver found in the stage tables and room scripts
  (docs/GAME_FACTS.md "Session 33, part 2"; rigtools event_gives.py, event_show.py). The game takes these keys
  nowhere but at their doors.
- sh3.c keys_lend / keys_settle (before and after the game's own 0x4B9940 in the sub_004B9940 wrapper): a key not
  held, its giver's flag set, its door's event still on, in that door's stage, is lent to the game's own action check
  (the button or a closing hand); the game finds the door, spends the key, opens it. Any other result clears the bit
  at once. The rig's switch SH3_KEY_BACK_TEST="frame:event:item:flag".
- Owner 162, a player's log (D:/Downloads/sh3 (3).log): 0.1.7, the store door (stage 2, event 50 "locked", key 38)
  pressed 24 times; the key lost to 0.1.7's looks. AUDIT row 192.
- Build 269 (C:/SH3Quest/build_pc_269.log). checks269a.sh: vr423 (key taken away as 0.1.7 did: lent at frame 7839,
  the door's event 8 found and run as in vr415, the key used), vr424 (giver flag not set: nothing lent), vr425 (key
  held: nothing lent). checks269b.sh (0.1.8's checks again on build 269, vr426-435) was written but never run, nor
  checks270.sh's second half (stopped in vr441); session 34 ran the release build's weapon checks (its handoff).

## Next

1. Players' reports on 0.1.8 and their headset logs (sh3.log says what VR did; every item gained or lost is in it).
2. The video's 11:40-11:45 (owner 160): the picture through a small window at the top as the clothes store's cutscene
   starts. A save at that fight (or the flat PC game there) settles whether it is the game's own opening or ours.
3. Keys lost to 0.1.7's looks: given back at their doors since 0.1.9 (keys_lend). A player's report from a
   headset on 0.1.9 with such a save ("lent" / "given back" in sh3.log) confirms it on the real doors; the rig has
   none of them (no saves, a new game only).
4. docs/HEADSET_CHECKLIST.md's 0.1.8 and 0.1.9 items on a headset.
