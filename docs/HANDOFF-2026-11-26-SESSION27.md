# SH3 Quest VR handoff, session 27 (2026-11-26): 0.1.7

Repo C:/SH3Quest/SH3QuestVR. 0.1.6 = ba72cb1 (tag v0.1.6, versionCode 7).
- Session 25 (docs/HANDOFF-2026-11-26-SESSION25.md) and session 26 (docs/HANDOFF-2026-11-26-SESSION26.md) made 0.1.7's
  changes.
- Session 26 stopped on its usage limit. Its build 235 failed because a run's sh3.exe still held the file, and its
  last checks never ran.
- Session 27 stopped on the owner's "wrap up and do handoff" (message 138) BEFORE THE RELEASE. Session 28 carried on:
  see docs/HANDOFF-2026-11-26-SESSION28.md.
- Where it stands:
  - PC build 236 OK (C:/SH3Quest/build_pc_236.log): every 0.1.7 change plus session 27's two.
  - The tree is still uncommitted on 62432d6. versionCode is 8 / 0.1.7.
  - The notes and What's new are written. No APK has been made. Nothing is published.

## The owner's message (C:/SH3Quest/notes/owner-messages-sessions1-7.txt, 137)

- 137: pick up from the other session, miss nothing, proceed with the list.
- The message came with D:/Downloads/sh3 (2).log: a Quest 3 run of 0.1.6, from a disc's protected exe, frames to
  99384. What the log shows:
  - **The far door.** At the circular hall's far door (event 48, the hand at 24758 -660 18186) the hands "could use"
    it 40+ times, and no use was ever logged. A along her facing missed the door beside her line; 0.1.7's A by gaze
    answers that.
  - **Melee damage.** 19 knife hits of 30 went into one dog of 579.5 health. 0.1.7 sets Melee damage to 200% and the
    knife to x1.5.
  - **Performance.** 87 fps at 120 Hz, the game's own work about 11 ms a frame. This is known, and the notes say so.
  - **The stall.** One 1.9 s stall was the headset's own sleep (session stopped, then begun).

## What session 27 changed

- **sh3vr_combat.cpp: a failed hand use is logged.** When a closing hand's use finds nothing, sh3.log now says so with
  what the hand could use as it closed: "the left hand closed at X Y Z and the game used nothing there (the hand could
  use event 48 as it closed)". The pieces:
  - hand_action.hover;
  - hover_event[2], hand_hover's state, now shared.
  - The simulator uses that door by hand from the same spot (vr242 and vr305: "the left hand used event 48 at
    24770 -626 18209").
- **sh3vr.cpp plan_move: the head offset is taken up while the stick walks her.**
  - **Before:** room walk takes up the head's offset from her only with the stick at rest.
  - **What vr326 showed:** the head leaned 30 cm ahead through the entrance gate, then came back, and the stick pushed
    at once. The eyes stayed 30 cm behind her head as she walked: her collar and hair in view, and the knife's line
    probes took her hair for a wall. Her hair is drawn outside the stencil-marked pass; with the eyes at her head it is
    cut away.
  - **Now:** while the stick walks her, any offset is taken up at 0.5 m a real second. rig.neck0 and rig.origin move
    together, as the head-in-a-wall reset does. The first 6 times are logged: "walking with the stick, the head N m
    from her: taken up as she walks".
- **Docs:** RELEASE-NOTES-0.1.7.md and whats_new.txt have both changes.

## Checks on the rig

Logs are C:/SH3Quest/vrNNN.log; pictures are in C:/SH3Quest/shots/vrNNN.

**Build 234 (session 26's chain):**
- vr314 walk292: the submachine gun at the hip and in both hands, walking, strafing and backing up. No wall hit or ding.
  The probes met her 38 times, and all 38 were known by her stencil mark.
- vr315 shadow291: her room-lamp shadow is cast from her drawn mesh (9056 triangles). The pictures are whole.
- vr316 check298: she was held and the bar showed. The old direct-damage test never reached the dog (see GAME_FACTS).
- vr317 gaze309: no door. The dog kept her from the far door.
- vr318 flat314: stalled at the script's set-up mark. Flat sends none, so QUEST_SIM_ALIGN=-1 is needed.
- vr319 gate305: event 5 found by gaze, 45 degrees from her facing, with the tick and buzz. Hands lit in the dark hall
  at rest, walking and looking down. The knife grip is whole.
- vr324 flat314 (QUEST_SIM_ALIGN=-1): the opening scene, play, walking and a head turn, all on the cinema screen (the
  panel), never black.

**Build 235:**
- vr325 walk292: as vr314. No wall hit or ding, and the two-handed pictures are clean.
- vr326 gate321 (SH3_GAZE_TEST=2, the head leaned 30 cm): event 5 found from the head's place 42 cm from hers. "Her
  own place put back"; the gate opened. FOUND: the eyes behind her head after it (fixed in build 236, above).
- vr327 check298 (SH3_HOLD_HIT=30): she was held at 8388 and the bar came in. A real knife sweep through the game's
  battle step went across the holder at 8417: "her holder 00898AC0 hit while it holds her (health 705.5): its hold
  broken". She got free at 8419, the bar emptied and rose away (gone at 8447), and she walked on.
- vr328 gaze309 (SH3_SWEEP_ATTACKER=20): the hall dog took hits of 90 and 135 (Melee damage 200%, the knife x1.5, the
  swing's speed) but its health never fell. Neither did vr317's on build 234.
  - **The cause is the rig.** SH3_HP=100 holds her health, so the dog bit her 72-75 times in a row, and a dog biting
    nonstop never applied the damage.
  - **The hit path is unchanged.** It hasn't changed since vr306 (build 230), where the dog bit her once, its health
    fell by each hit and six hits killed it.
  - vr330 repeats vr306's setup on the final build (below).

**Build 236:**
- vr329 gate321 (as vr326):
  - event 5 found from the head's place (42 cm), her own place put back, the gate used;
  - "walking with the stick, the head 0.94 m from her: taken up as she walks" from frame 8129;
  - h08240 is whole: the eyes at her head, the hands and the knife clean, no collar or hair.
  - OPEN: the offset was 0.94 m, more than the 30 cm lean explains (the door's recentre at frame 8081 came while the
    head was leaned and turned 45 degrees). Find where the rest comes from. At 0.5 m a second, such an offset takes
    about 2 s of walking to go: consider a faster take-up for large offsets.
- vr330 gate306 (vr306's setup: SH3_STEER_AT_ATTACKER=1 SH3_HP=100, the far door by A and the dog's health falling):
  STOPPED unfinished at the wrap-up (sh3.exe killed, the game folder's sh3vr.ini left at vr = 1). Run it first.

**Earlier in 0.1.7 (sessions 25 and 26):**
- vr300: the break-free bar.
- vr302, vr303 and vr309: the grip spot. Adjust ("maul's two-hand grip set where the left hand took hold"), "the
  weapon in the hand ... back at its natural spot", and "every weapon's ...".
- vr306: A at the far door, and six knife hits killing the dog.
- vr307: hands in the dark.
- vr311: A by gaze alone.
- vr312 and vr313: every weapon's grip, from the eyes and both sides, one- and two-handed. The sheets were checked
  again this session: the fists close round each gun's grip with the trigger finger on the trigger, and the support
  hands are placed.

## Release

Not made. Next, in order:

1. **Run the remaining checks on build 236** (C:/SH3Quest/checks236.sh has the run helper and the hall points; the next number is vr331):
   - gate306 as vr306: the far door by A and the dog's health falling;
   - then find the 0.94 m offset (above).
2. **If code changes:** one more PC build and the checks it reaches.
3. **The release build:** `source /c/RACQuest/env.sh; C:/SH3Quest/venv/Scripts/python.exe -I tools/build_quest.py --release`.
4. **apkcheck:** `C:/SH3Quest/venv/Scripts/python.exe -I tools/apkcheck.py C:/SH3Quest/SH3QuestVR-0.1.6.apk
   android/build/SH3QuestVR-0.1.7.apk`. Copy the APK to C:/SH3Quest/SH3QuestVR-0.1.7.apk.
5. **Fill RELEASE-NOTES-0.1.7.md's @SIZE@ and @SHA@.**
6. **Commit, tag and push:** commit (the message drafted in C:/SH3Quest/commit_017.txt, plus session 27's
   two changes), annotated tag v0.1.7, push main and the tag.
7. **The release page:** `gh release create v0.1.7 --notes-file docs/RELEASE-NOTES-0.1.7.md --verify-tag` with the
   APK. Publish without asking (owner 123).
8. **Check the release:** releases/latest's asset digest, and a downloaded copy's SHA-256.
9. **Afterwards:** the handoff commit, memory, and the APK path and SHA-256 to the owner.
- Player replies stay drafts in C:/SH3Quest/notes (reply-player-report-0.1.6-gate.txt and others); never post them
  unasked.

## Rig notes

- **PC build:** `cd /c/SH3Quest/SH3QuestVR && timeout 900 cmd //c "tools\\build_pc.bat" > /c/SH3Quest/build_pc_N.log 2>&1`.
  Never while an sh3.exe runs: build 235 failed that way. The last build is 236.
- **Sim:** run from /c/SH3Quest/gamedir with:
  - QUEST_SIM_HEADSET=1 QUEST_SIM_ALIGN=7312 QUEST_SIM_GRIP_TILT=50
  - SH3_KEYS="22:Return,25:Return,28:Return,31:Return,34:Return"
  - QUEST_SIM_SCRIPT and QUEST_SIM_OUT (mkdir the shots folder first).
  - Flat needs QUEST_SIM_ALIGN=-1 and vr = 0 in the game folder's sh3vr.ini for the run (put back after).
- **The hall:**
  - **To reach it:** `SH3_STEER_TO` with the 14 points in C:/SH3Quest/checks236.sh (HALL=...). They
    are also in gate306's header.
  - **Hold the dog to see a hold:** SH3_STEER_AT_ATTACKER=1.
  - **For the dog's health falling:** keep the run short of the dog biting nonstop. SH3_HP holds her health and makes
    that loop.
- **Test switches added in sessions 26-27:**
  - SH3_GAZE_TEST=1/2;
  - SH3_STEER_AT_ATTACKER=N (stops after game frame N when N > 1);
  - SH3_HOLD_HIT=N (a knife sweep across the holder on the N-th struggle step);
  - SH3_SWEEP_ATTACKER=N (a knife sweep across the attacker every N frames within 1.5 m);
  - SH3_SECOND_TEST.
- **The next run number:** vr331.
