# SH3 Quest VR handoff, session 32 (2026-11-27): 0.1.8 (players' reports: keys, walking on, the item seen double)

Repo C:/SH3Quest/SH3QuestVR. 0.1.7 = 703f432 (tag v0.1.7, versionCode 8). Read docs/HANDOFF-2026-11-27-SESSION31.md
and docs/HANDOFF-2026-11-26-SESSION30.md first for 0.1.8's base (the fitted hands, the trigger pulled); this one says
what session 32 added. docs/GAME_FACTS.md's "Session 32" section has the game's side of it.

## The owner's messages (C:/SH3Quest/notes/owner-messages-sessions1-7.txt)

- 152 (a player's video and report, D:/Downloads/dev.sh3quest.vr-20261127-122916-0.mp4 and sh3quest-report.txt, and
  two players' words): "I took the key for the bookstore. But when I went back to the book store, the key is not in my
  inventory", "When I took the key, I saw it double on my headset", "the thing where she keeps walking after
  interacting is still there if you are moving at all when you interact". "make sure the game works properly in all
  ways and can be completed as a result. shit like this should NOT be happening. make no mistakes."
- Then: "i assume it has something to do with the sprint toggle we have lol".
- 153: a toggle for the grab bar too, on by default: it exists ("Break-free bar when grabbed", View page and the
  launcher, on), nothing to add.
- Standing (owner 123): when 0.1.8 is done, publish it without asking.

## Done in session 32 (uncommitted)

- **Keys spent by VR's looks (the bookstore's key).** 0x4B9940 uses up the item an event takes (a key at its door) as
  it finds the event, when its kind argument is 0 (GAME_FACTS). VR's own checks that only look (the hands' reach each
  frame, the hints', the button's pre-checks along her facing and where the player looks, a closing hand's) ran with
  kind 0: near a key's door they spent the key, the door unopened, and the game's own check then found the locked
  event. sh3.c action_check_at now passes kind 1 and puts the inventory (0x712CA80-0x712CABF) back if anything in a
  look changed it (logged). Rig proof: SH3_SPEND_TEST="7700:8:69" (the costume made a key's door for item 69): vr396
  (fixed) item 69 stays until A, then the game's own check uses it and the event runs; vr398 (SH3_LOOK_SPENDS=1,
  0.1.7's way) the pre-check spent it and the game used event 12 instead, event 8 never ran.
- **Walking on through events.** The dispatcher returns mode 5 through event scripts too; VR walked her through
  messages. sh3.c event_hold (VR's walk and turn wait while the dispatcher's step is not 0 and the game does not walk
  her itself), her_gesture (the same while her gesture 0x28 plays), the Sprint toggle's run ends (sh3vr.cpp
  update_sprint), the room walk does not follow the head during a hold. Rig: vr396 (fixed) she stood through all three
  pages of the costume's message with the stick held, walked at once when it closed; vr397 (SH3_EVENT_HOLD=0
  SH3_NO_GESTURE_HOLD=1, 0.1.7's way) she walked 3.2 m with the message up; vr399 (event396: the Sprint toggle on, A
  mid-run) "sprint off (an event of the game's holds her)", she stood, walked (not ran) after.
- **The item seen double.** The item display (0x599A60, drawn by 0x599EA0 from inside the dispatcher only) was 3D of
  another camera drawn before the message's 2D: to the eyes at no depth. d3d8.c vr_route_pick sends another camera's
  3D drawn inside the dispatcher (sh3.c sh3_in_dispatch) to the panel. NOT seen on the rig: the display draws only in
  a room script's give, and the rig's rooms have none (SH3_ITEM_SHOW_TEST opens it but it draws nothing). The route
  dump (now with "dispatch" and "main camera") showed no other draw inside the dispatcher in play. On a headset the log
  says "the item an event shows ... on the panel" the first time.
- **A stalled floor pick-up** is asked again after 90 steps (sh3.c pickup_watch). The inventory logs every item
  gained or lost (sh3.log "item N now in the inventory"), so a report tells where an item went.
- **Grips (owner 147):** the fit keeps the guns' other hands within 2 cm along their grip (the submachine gun's left
  its front block, vr395); the stun gun's fist is free of the line limit (none fitted under it, vr395); the steel
  pipe's, katana's, maul's and beam saber's other hands are the main hand's grip mirrored across the handle's plane
  (sh3vr_combat.cpp second_hand_pose; their fitted other hands curled in the air, vr395). The shotgun's other hand keeps
  0.1.7's place (no fit under the cup limits; its pictures right).
- **Build 266's fit, vr400 (SH3_GRIP_FIT=1, pictures in C:/SH3Quest/shots/vr400):** every fit line read
  (scratchpad fitparse.sh; the lines are in C:/SH3Quest/vr400.log). Pictures checked: the pipe's and katana's mirrored
  other hands close round the handle as the main hand does (the log's left-hand curls and pads match the right's);
  the submachine gun's other hand now cups the receiver's front, fingers curled up its side (vr395's stood straight
  up); the flamethrower's wraps the body's front. Two faults: **the maul's other hand closed on air past its shaft's
  end** (g7-maul-2close), and **the stun gun's fit slid her hand 6.7 cm along the box up to its prongs** (moving the
  prongs back by as much). Both came from the fit's place ALONG a handle, which is chance on a plain handle (the maul's
  and beam saber's main hands 4.5 / 4.9 cm toward the pommel, the pipe's and katana's 4.8 cm toward the head).
- **Build 267 (built, NOT yet checked on the rig):**
  - fit_main / fit_second baked from vr400's lines (sh3vr_combat.cpp, "Baked from the search's lines"). The shotgun's
    other hand and the four two-handed melee weapons' other hands stay unfitted ({}), as above.
  - main_fit: every weapon held in the fist (knife, stun gun, steel pipe, katana, maul, beam saber) keeps 0.1.7's place
    ALONG its handle (the fit's shift with its part along kShapes[p].c[0] taken out); only the place round the handle
    and the turn are the fit's. So each weapon sits on the controller along its line as on 0.1.7: the same reach, the
    beam saber's beam where it was (owner 155/156). The search itself (SH3_GRIP_FIT=1) still works on the full place.
  - second_hand_pose (the mirrored other hand): slid along the handle's own line (c[0]), not the weapon's forward (the
    katana's is 6 degrees off it), to where the other hand takes hold (second_natural + the Two-hand grip spot), its
    centre kept 3.5 cm in from either end and 8.5 cm (a fist) from the main hand, on the main hand's other side where
    one side has no room. The mirror plane is square to the handle's line.
  - Worked out for 0.1.7's places: the other hand's centre sits 8.65 cm below the main fist on the maul (at the
    pommel), 10.2 cm on the beam saber and the pipe, 13.4 cm on the katana: all on the handle.
- **The beam saber's beam (owner 155/156)** is drawn with the hilt's own map every frame (saber_blade_draw: the same H
  as the hilt), so it starts at the hilt along its line wherever the hilt sits in the hand. vr400's pictures had the
  beam off (it lights only with the trigger held), so it has not been seen with the fits yet: Next 2.
- **Keys already lost on 0.1.7** (a save made after the loss): not given back. The game keeps no record of an item
  ever held (0x004972B0 sets the inventory bit and the score only), so telling a lost key from one never found needs
  each key's own "taken" flag: room script gives set it themselves (0x00568910: `or [0x0715D0C4], 0x02000000` before
  giving item 0x4B, flag 825; item 0x4C flag 826), floor items through the room table's taken flag. Not built. The
  release notes tell players to load a save from before they came up to the key's door.
- Release notes (docs/RELEASE-NOTES-0.1.8.md): the intro and the Fixed list now have the keys, walking on, the item
  seen double and the stalled pick-up. The testers section and the Files line are still 0.1.7's (Next 5). The in-app
  what's new draft is in the scratchpad (wn018.txt, copied below).

## Next (in order)

1. **Build 267 on the rig, the release's own way (no SH3_GRIP_FIT):** `run 401 pics383 1300 SH3_GIVE="$GIVE"
   SH3_EQUIP="1,2,3,8,4,5,6,7,9,10" SH3_EQUIP_EVERY=600` (C:/SH3Quest/checks266.sh's run function and GIVE). Check
   every picture whole, above all: the maul's other hand on its shaft (2close/2right/2eyes/2down), the beam saber's,
   pipe's and katana's both hands, the stun gun's hand on the box's rear half (1close/1right/1left), the knife's little
   finger on the handle (its 3 cm toward the pommel is gone now), the submachine gun's cup, the flamethrower's other
   hand, and the handgun as before. Compare against vr400 where it helps (delete C:/SH3Quest/shots/vr400 after).
2. **The beam saber lit (owner 156: "the beam sabers saber part, making sure thats still alligned"):**
   tools/sim/beam207.txt with SH3_EQUIP="3,9,4" SH3_EQUIP_EVERY=200 (its header's env, no SH3_GRIP_FIT), then again
   with SH3_GRIP_FIT=0 (0.1.7's places): the beam must leave the hilt's emitter along the hilt's line in both, held,
   lit, forward-up, across and in the swing. Add a two-handed lit picture (lh_at_second 1, left_grip 1 with the
   trigger held) if the script has none.
3. The rest of the checks: lefty384 (left_handed = 1 in C:/SH3Quest/gamedir/sh3vr.ini, put back after), parts384,
   Hold weapons anywhere and the moved Two-hand grip spot with its resets, a swing at Normal, the ini's v3 migration,
   and event391 with SH3_SPEND_TEST=7700:8:69 (vr396's env) once more on the release code. About 30 minutes in all.
4. Re-read all of session 32's new code once (sh3.c action_check_at, event_hold, pickup_watch, the spend test;
   d3d8.c's dispatch routing; sh3vr_combat.cpp main_fit, second_hand_pose, fit_candidate).
5. Release 0.1.8 (owner 123: publish without asking when done): versionCode 9 / versionName 0.1.8
   (android/app/build.gradle); android/app/src/main/assets/whats_new.txt gets the 0.1.8 section (wn018.txt below, plus
   the two-handed melee weapons' other hand); RELEASE-NOTES-0.1.8.md's testers section (what the rig checked; the item
   display not seen on the rig) and Files line (the new APK's name, size, SHA-256); AUDIT: a 0.1.8 section for
   sessions 29-32 (owner messages 145-157); `build_quest.py --release`, apkcheck, copy to
   C:/SH3Quest/SH3QuestVR-0.1.8.apk; the control-character scan; commit (with the untracked tools/sim/walk389.txt,
   event391.txt, event396.txt and these handoffs), tag v0.1.8, push, `gh release create`, check the digest; the
   memory's line. The final answer cites the research (HurricaneVR, Auto Hand, Meta Interaction SDK, Half-Life: Alyx,
   SDFGen), says the grab bar's toggle exists (owner 153), and lists the old picture folders vr287-vr377 under
   C:/SH3Quest/shots for the owner's go to delete.

The in-app what's new draft (wn018.txt):

```
0.1.8:

- Keys no longer used up at their doors without the door opening (the bookstore's key gone from the inventory): the
  checks VR makes only to look (a hand reaching out, the hints, A looking where you look) used a key the game's way
  the moment they found its door. They now only look; the key is used once, when the door opens.
- She no longer keeps walking after you use something with the stick held (or the Sprint toggle's run on): she stands
  while the game's event runs (a message, a door, an item), as on a PC, and walks again as soon as it is over.
- The item you take, turning in its box by its message, no longer shows double: it is on the message's panel.
- A pick-up from the floor whose bend was cut short no longer leaves the game waiting for good: the bend is asked
  again.
- Her trigger finger on the trigger, pulling it: on the handgun, shotgun and submachine gun her index finger lies on
  the trigger while your finger rests on the controller's trigger, and along the frame, clear of the guard, when it is
  off. As you pull, the gun's own trigger swings back about its pin and her finger closes with it, its pad on it the
  whole way. The flamethrower and the stun gun have no trigger in their models: the finger closes round the grip.
- Every weapon gripped properly, both hands: each hand's place on each weapon was fitted against the weapon's own
  shape (her palm against it, every finger closing round it with its pad on it). The second hand wraps the support
  grip, pump or handle; the knife, steel pipe, katana, maul and beam saber are in her own hand too, fingers closed on
  each handle.
- Her thumb swings at its root as well as curling, so it lies along a pistol's frame or round a grip or handle.
- Her fingers stop on the weapon in her hand and on the other hand's, using the weapon's own mesh as drawn.
- A gun still aims where it aimed: the hand moves on the gun, the gun keeps 0.1.7's way in your hand.
- Hold weapons anywhere (Hands and weapons, off): your other hand takes hold wherever you grip the weapon, wrapped
  round that part. Off, it takes each weapon's two-hand grip as before.
- Swing strength needed (Hands and weapons: Light, Normal, Hard; Normal): Normal asks a firm swing, so moving the
  weapon or your arm about never attacks; Light is 0.1.7's.
- Fixed: the shotgun's pump slid forward past the muzzle as you pumped it (it now goes back with your hand), and a
  piece of the handgun's trigger moved with the slide.
- A 0.1.7 weapon file's moves and turns for the guns, and every Two-hand grip spot, are put back once, as the hands now
  sit differently on the guns.
```

The owner's messages 154-157 came in this session's last part: 154 (the walking on was not only the Sprint toggle:
right, told), 155 "regress nothing. make sure beam saber is still alligned and looks right", 156 "i meant more the
beam sabers saber part, making sure thats still alligned", 157 "wrap up and do handoff".

## Rig notes

- Build: `cd /c/SH3Quest/SH3QuestVR && timeout 900 cmd //c "tools\\build_pc.bat" > /c/SH3Quest/build_pc_N.log 2>&1`.
  One build at a time; never while sh3.exe runs.
- The event test: tools/sim/event391.txt (header has the env), event396.txt (the Sprint toggle). Sim frames run about
  49 ahead of the game's (rt_frame) in these.
- head_yaw in the sim scripts: her facing becomes 90 - head_yaw degrees.
- The last build is 267 (C:/SH3Quest/build_pc_267.log); the next run is vr401. Runs: C:/SH3Quest/checksNNN.sh (copy
  checks266.sh: its run function, KEYS and GIVE). The picture and fit helpers are in C:/SH3Quest/rigtools:
  fitparse.sh LOG (the fit lines in short), `grep "VR: grip fit:" LOG | awk -f bake.awk` (the tables; the last line per
  weapon wins), montage.py out cols x0 y0 x1 y1 scale pictures..., imgdiff.py; with the 0.1.8 what's new drafts
  (wn018.txt for the app, whatsnew-018.md for the notes).
- montage.py's crop takes the picture's own size (0 0 1000 1000): a bigger box makes a giant image.
