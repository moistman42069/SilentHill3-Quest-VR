# SH3 Quest VR handoff, session 28 (2026-11-26): 0.1.7

Repo C:/SH3Quest/SH3QuestVR. 0.1.6 = ba72cb1 (tag v0.1.6, versionCode 7). Sessions 25-27 made 0.1.7's changes
(docs/HANDOFF-2026-11-26-SESSION25.md, -SESSION26.md, -SESSION27.md); session 27 stopped before the release with PC
build 236 and two checks left (vr330, the 0.94 m head offset).

## STOPPED HERE (owner 141, "wrap up and do handoff"): start with "Owner 140" and "Next" below

Uncommitted tree on 62432d6 (0.1.6 = ba72cb1), PC build 242 made (BUILD OK, C:/SH3Quest/build_pc_242.log) and NOT run.
0.1.7 is not published; no APK made.

## The owner's messages (C:/SH3Quest/notes/owner-messages-sessions1-7.txt, 139-141)

- 139: "was working in another session for SH3. pick up from where it left off. miss/forget nothing. make no mistakes.
  check everything you need to be sure this build is perfect. proceed with list."
- 140 (mid-session): the PC VR mod's maker tried this port: "I found out an issue with animation block like it was in my
  mod before / Like if you spam motion controls like knife stabs, game animations would break and i can't do anything
  besides only walking and even enemies won't attack me": "proceed with list" (so: fixed in 0.1.7 before the release).
- 141: "wrap up and do handoff."

## Owner 140: found, fixed in build 242, not yet checked

**Reproduced in vr347** (build 241, tools/sim/spam347.txt: gate335's way to the hall, then knife stabs 45 cm out and
back over 10 frames for 2000 frames at the dog with the stick held all the while; SH3_NO_KILL=1 so the dog lives,
SH3_STATE_LOG=1). At frame 8339, mid-stab (action 37), the dog grabbed her: action and movement 35, reaction 9
(0x712C665), animation 206 on both channels, and it stayed so to the run's end (10840): no "she is knocked down or
held" line, no bar, no struggle step, the dog biting (kind 120) once a second, her slashes after it giving nothing.
vr342's hold (stick at rest) went 9 -> struggle in 13 frames.

**The cause (disassembly; tools/disx.py and a whole-code listing):**
- Her reaction is movement state 34/35's step 0x4D9E60 (the movement table at 0x6D8ED4; action 35's own step is a bare
  `ret`). Step 0 (0x4DAAF5) starts it and sets her walk factor +0x118/+0x11C to 0 (0x4DAB8B). Step 1 switches on the
  reaction: 9 (a grab's start) sets the time to get free (0x4DAE70) and sub-step 1, which waits for event 0x10 in
  0x712C1C4 (0x4D9F59). 0x4C6EE0 raises 0x10 when 0x4746C0 finds the legs' channel (+0x238) at its animation's last
  frame. Then the animation picker 0x4C77D9 (movement 35/36 -> 0x4C7190) asks 0x4B47E0 for the next reaction: 9 -> 10
  (5/7/12 -> 6, 6 -> 7/8, 10 -> 11), and 10's sub-step 2 runs the struggle step 0x4D44E0.
- VR's walk wrote +0x11C every frame it steered (1.8 walking, 3.6 running), with her velocity and facing. While +0x11C
  is not 0, the movement step 0x4D83B0 re-times her legs' animation from her walk speed (0x474580 on channel 2, and on
  channel 1 too when the action equals the movement, as in a reaction). The game itself keeps +0x11C at 0 through a
  reaction; VR's walk did not, so animation 206 never reached its end and the grab never moved on. The same would hold
  a knockdown or a flinch (only VR's walk still moving her: the maker's "only walking", monsters waiting).
- The PC VR mod's fix for the same thing (d3d9_hook.cpp D3D9Hook_NotifyHeatherDamage): no heading or walking writes for
  1200 ms after damage during a knockdown or getting up.

**The fix (build 242):**
- sh3.c `her_reacting()`: her movement state 0x712C190 is 34, 35 or 36 (34/35 run 0x4D9E60; 35/36 take a reaction
  animation from 0x4C7190); read live in sub_004D83B0. While it is set VR writes neither her walk (+0xF0/+0xF8, +0x11C)
  nor her facing (sub_004D83B0 and the standing turn at the frame hook); the "walking stopped" line says "her hit
  reaction: the game moves her" and leaves her velocity to the game (0x4EAE19 sets it from +0x11C each frame).
- sh3vr.h Sh3VrGame.reacting (sh3.c fills it); sh3vr.cpp plan_move returns at once in a reaction (no walk, room walk or
  body turn; logged "her hit reaction (the game's movement state N): VR's walk and turn wait, the stick goes to the
  game" and "her hit reaction over after N frames", 30 at most); the pad gives the game the real left stick in a
  reaction in first person (its struggle counts directions newly pushed, as on the PC).
- sh3vr_combat.cpp combat_melee_landed: a contact hit's press of the game's attack keeps the same limits as a counted
  swing (the weapon's cooldown, the maul 0.56 s; a heavy weapon not while its sequence or hit is pending), as the PC
  mod's cooldowns; before, contact hits could press it with no cooldown.
- Test switches kept (rig only): SH3_STATE_LOG=1 (her states as they change, now also every 30 frames in a reaction with
  its sub-step, the time to get free, the walk factor, the event flags and the legs' time and end), SH3_NO_KILL=1 (her
  hits do no damage). A capped log line for a fast knife/heavy/stun swing that ended without the game's attack (20 at
  most; in vr346 these were the cooldown: 0.08-0.18 s since the last, it asks 0.22).

**Builds:** 239 = 238 plus the elbow lines logged per stretch; 240-241 = the test switches and the swing line above;
242 = the fix. Backups of the files before session 28's edits: C:/SH3Quest/*.bak-s28*.

**Other spam runs (builds 240-241):** vr345 (spam345: teleport stabs every 4/8 frames at the start): her states cycled
37/8/9 normally; the slashes after gave nothing because a single scripted jump has no travel from its start (a rig
artefact; the idle fidget actions 1/41 after 10 s standing are the game's). vr346 (spam346: continuous stabs): no
stuck state; the slashes, walk, A and inventory after it all worked. So the spam alone does not break her; a hit
reaction under VR's walk does.

## What session 28 found and changed

- **vr331 (build 236, gate306 as vr306; vr330 was session 27's unfinished try):** six knife hits of 135 (Melee damage
  200%, the knife x1.5, the swing's speed x1.5) put the hall's dog (718.7 health) down at frame 9379, as vr306. The far
  door was not reached by A this time: a second monster of the hall (00898DB0, 1351 health) came at her at the door at
  frame 11059 and SH3_STEER_AT_ATTACKER=1 drew her away before the script's A at 11890 (runs are not frame-exact).
- **vr329's 0.94 m head offset is the rig's, not a fault.** sh3.c's SH3_STEER_TO replaces VR's heading whenever VR steers,
  room walk's catch-up included. After the door the script brought the leaned head back 0.3 m (room walk then catches
  her up toward the head, the head kept still in the world while she moves), but the steer test walked her on toward
  its next point instead, so the offset grew to 0.94 m. On a headset the catch-up walks her toward the head. (The black
  h08175 of vr329 and vr333 is the head-in-a-wall fade from that same offset.)
- **The whole uncommitted diff was read again** (sh3.c, sh3vr.cpp, sh3vr_combat.cpp, sh3vr_body.cpp, sh3vr_ik.cpp,
  d3d8.c, d3d8_shader.c, xr_openxr.cpp, wrist.h, Options.java, the assets). One bug, fixed in build 237:
  - **sh3.c, the melee factor's limit.** sub_004B6A20 and sub_004B6810 took VR's factor only up to 8. At Melee damage
    400% a fast knife slash is 4 x 1.5 x 1.5 = 9, which fell back to the game's 1x. Now up to 16 (9 is the most).
- **Build 237, sh3vr.cpp plan_move:** a large head offset is taken up faster while the stick walks her: clamp(1.5 x the
  offset, 0.5, 1.0) m a second (0.5 up to a third of a metre, as session 27's; a 0.9 m offset in about 1.4 s, not 1.8).
- **Build 237, sh3vr_combat.cpp:** the "hand closed and the game used nothing" line is written every time the hand
  could use something, and for a hand that could use nothing only the first 20 times (the rig's scripts close the left
  hand every 30 frames: 33 lines in vr331).
- **Build 238, sh3vr_ik.cpp arm_to: the elbow kept out of the eyes.** vr332's last picture showed her left forearm
  running from the raised left hand up into the eyes; elbow336 (vr336) reproduced it with the head straight over her
  body and both hands raised before the face, the controllers pointing ahead (the left hand on the middle line, 10 cm
  under the eyes, 40 cm out): SH3_BODY_LOG put her left elbow 10 cm before her eyes, 5 cm under them, 12 cm from them.
  The cause: the elbow hint is the straight-wrist elbow (a forearm's length behind the hand along the controller),
  trusted fully when it lies clear of the shoulder-hand line, and for a hand that close before the face that point is
  before her nose. Now:
  - an elbow that would come within 18 cm of the eyes has its hint turned toward down and out (around the
    shoulder-hand line, where an elbow can go) until clear, or as far as it gets (joint_for gives solve_two's elbow for
    a hint); the poses that looked right had their elbows 15-33 cm off (15: an arm all but straight under the eyes);
  - the hand-twist swing (up to 60 degrees about the shoulder-wrist line; vr335's arm log had it at 60 with the hands
    raised) is halved, up to five times, while it would take the elbow within 18 cm of the eyes and nearer than it was.
  - Logged "her left elbow kept clear of the eyes: it would have come N cm from them, now M" and "her left elbow's
    swing for the hand's turn held to N degrees (it asked M)"; build 239 logs each as a stretch of it starts, 12 times
    an arm (build 238's 8 lines a site were used up by one hold).
  - The player's "arms go crazy sometimes" (125.4) may be this; session 25 put it on phantom contacts and the dead
    dog's hold, both fixed.
- **Build 238, sh3.c: a press of A that finds nothing is logged**: "the action button (pass 0) found nothing along her
  facing (N degrees) nor where the player looks (M degrees and 15/30/45 either side, 1.02 and 1.33 m) from her place X Z
  (the head's X Z)", 20 times at most, once a second.
  - Why: in vr335 (build 237) A at the far door found nothing and said nothing, though the head looked at the door
    squarely (its picture is vr338's), her place was within 3 cm of vr338's, and the gaze fan's 105-degree ray crosses
    event 48's line. The cause was not found (a hand's action that frame, which skips the gaze, or a game state that
    ignores A, are what is left). vr338 on build 238 opened the door by the gaze alone.
- **Rig artefact, not a fault:** SH3_SWEEP_ATTACKER's sweeps (queue_extra, not a VR swing) get no Melee damage factor
  unless a scripted swing's game attack is pending, so two of vr335's sweeps did the game's 30. Real swings always
  carry their factor (vr335 and vr338: 270 a fast slash at 400%).
- New rig scripts: tools/sim/gate332.txt (gate306 with the far door's A at 10900), gate335.txt (gate332 with
  SH3_SWEEP_ATTACKER=20, no SH3_HP, outside views at the end, the arm and body logs), elbow336.txt (the raised hands,
  the head straight and turned 45 degrees each way, from the eyes and both sides), anull344.txt (A where nothing is).

## Checks (logs C:/SH3Quest/vrNNN.log, pictures C:/SH3Quest/shots/vrNNN)

**Build 237 (C:/SH3Quest/checks237.sh, checks335.sh, checks336.sh):**
- vr332 gate332: no check. The dog came from behind (-133 degrees) and SH3_STEER_AT_ATTACKER backed her away from it;
  SH3_HP let it bite 69 times; no hit landed; she never reached the door. Its k10995 showed the left elbow fault.
- vr333 gate321: event 5 found from the head's place (42 cm), her own place put back, the gate used; the take-up at
  1.0 m a second from 0.94 m (0.94 -> 0.87 m in 5 frames); h08240 whole.
- vr334 check298 (SH3_HOLD_HIT=30): held at 8341, the bar came, the holder hit at 8370 ("its hold broken"), free at
  8372, the bar gone at 8400, walked on; the ini put back.
- vr335 gate335: 400%: "took 270.0 damage from her (melee: 900% ...)" and 180s, the dog dead at 8837; the far door's A
  found nothing (above); the 20-line cap on the hand lines worked.
- vr336 elbow336: the left elbow fault reproduced (head straight); fine with the head turned 45 degrees either way.

**Build 238 (C:/SH3Quest/checks238.sh, checks344.sh):**
- vr337 elbow336: "her left elbow kept clear of the eyes: it would have come 12-14 cm from them, now 23-24"; the swing
  held to about half once (-19 of -38). The pictures: the left forearm now rises from below to the hand, solid, the
  elbow under the hand from both sides; the turned-head poses as before.
- vr338 gate335: three real knife slashes of 270 (900%) put the dog down (710.8 -> 440.8 -> 170.8 -> -1.0); "the action
  button (pass 0) found event 48 where the player looks: 0 degrees from her facing, 1.02 m out" and "used event 48"
  with SH3_GAZE_TEST=1 (the far door by the gaze alone), through to the next area; after it an elbow kept clear
  (11 -> 33 cm).
- vr339 grips313: every weapon's hold one- and two-handed from the eyes and both sides. The handgun's and the pipe's
  are as vr313's (build 233). The katana's two-hand hold from her right is better: build 233 showed the right forearm
  dithered away near the eyes, now it is a solid arm back to the elbow. Shotgun, submachine gun, maul, flamethrower
  two-handed whole.
- vr340 reload: the handgun (magazine dropped, a new one lined up and seated, the slide), the shotgun (two shells, the
  pump), the submachine gun (magazine, the cocking knob), the stun gun (battery): "the hands reloaded item N" for each,
  "it fires" after the slide, pump and knob; the pictures whole.
  Its k08690 (the submachine gun's knob pulled back) has the left forearm big across the bottom of the view: the
  hand at the knob, just over the right fist and 5 cm back toward her, the elbow out to the left, the forearm about
  17 cm under the eyes (no earlier copy of that picture kept; the pose's own geometry, not the elbow rule's).
- vr341 walk292: picture for picture as vr325 (build 236): the submachine gun at the hip and in both hands, walking,
  strafing, backing up, looking down (the left arm across to the front grip, as before); no wall or ding (38 probes
  met her or the gun, all by the stencil mark); her shadow from her body as drawn.
- vr342 check298 (SH3_HOLD_HIT=30): held at 8355, the bar came, the holder hit at 8384 ("its hold broken"), free at
  8386, the bar gone at 8414, walked on to the hall; the pictures as vr334's (the skin shape low in h08362/h08402 is
  her arm in the sim's shake, there in vr334 too).
- vr343 flat314 (vr = 0, QUEST_SIM_ALIGN=-1): the opening scene, play, walking and a head turn on the cinema screen,
  as vr324; the ini put back ("ini as before").
- vr344 anull344: "the action button (pass 0) found nothing along her facing (90 degrees) nor where the player looks
  (90 degrees and 15/30/45 either side, 1.02 and 1.33 m) from her place -16000 20000 (the head's -15926 20000)", and
  again at 130 degrees with the head turned 40 degrees right.

## Next (in order)

1. Run C:/SH3Quest/checks242.sh on build 242 (written, not run; about 75 min in all, so in parts if the owner's
   time rule needs it): vr348 spam347 must now show reaction 9 -> 10, "she is knocked down or held", the bar, free,
   the dog biting again and the slashes at 10500-10700 giving "the game's attack"; vr349 spam346; then every 0.1.7
   feature again (vr350 elbow336 ... vr358 flat314, the ini put back by the script). Read every picture whole.
   If vr348 still sticks, read its 30-frame reaction lines (the walk factor must be 0, the legs' time must reach the
   end) before changing anything.
2. Docs for owner 140: RELEASE-NOTES-0.1.7.md and assets whats_new.txt (a bullet: a monster's grab, a knockdown or a
   flinch while walking could leave her stuck, walking only, monsters waiting; VR's walk and turn now wait for the
   game's reaction, and the stick works the struggle), AUDIT-2026-11-25.md (a new row; rows 166/167's states),
   HEADSET_CHECKLIST.md (walk into a monster stick held, get grabbed and knocked down: it must go on to the struggle
   and the get-up), C:/SH3Quest/commit_017.txt (the fix, tools/sim/spam345-347, the two test switches). No player
   named in public notes.
3. `source /c/RACQuest/env.sh; C:/SH3Quest/venv/Scripts/python.exe -I tools/build_quest.py --release`.
4. `C:/SH3Quest/venv/Scripts/python.exe -I tools/apkcheck.py C:/SH3Quest/SH3QuestVR-0.1.6.apk android/build/SH3QuestVR-0.1.7.apk`;
   copy the APK to C:/SH3Quest/SH3QuestVR-0.1.7.apk.
5. Fill RELEASE-NOTES-0.1.7.md's @SIZE@ and @SHA@.
6. Commit (C:/SH3Quest/commit_017.txt), annotated tag v0.1.7, push main and the tag.
7. `gh release create v0.1.7 --notes-file docs/RELEASE-NOTES-0.1.7.md --verify-tag` with the APK, without asking
   (owner 123).
8. releases/latest's asset digest and a downloaded copy's SHA-256.
9. The handoff commit, memory, the APK path and SHA-256 to the owner.

## Rig notes

- PC build: `cd /c/SH3Quest/SH3QuestVR && timeout 900 cmd //c "tools\\build_pc.bat" > /c/SH3Quest/build_pc_N.log 2>&1`;
  never while an sh3.exe runs.
- The run helper and the hall's points: C:/SH3Quest/checks242.sh. The next run number is vr348 (vr345-347 were the
  spam runs; checks239.sh was never run: its numbers clash).
- The game folder's sh3vr.ini: backed up as C:/SH3Quest/sh3vr.ini.before-s28 (Melee damage 200%, vr = 1); each run
  that changes it puts it back and compares.
- SH3_ARM_LOG=N and SH3_BODY_LOG=N log from about frame 7290 (play), 400 and 80 lines at most: N=5 and N=10 cover the
  opening's first 800-1000 frames; N=45 and N=110 a whole hall run.
- The sim's lh_/rh_ offsets are in the room's axes (only the rest place turns with head_yaw): elbow336 turns them with
  the head's yaw by hand.
