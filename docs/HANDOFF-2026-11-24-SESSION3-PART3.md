# Silent Hill 3 Quest VR: handoff, 2026-11-24 (session 3, part 3)

Read docs/HANDOFF-2026-11-24-SESSION3.md first (rules, how to build and run), then part 2's (LIST 41-45's design).
This part: 41-45 written, built and checked on the PC rig; the owner's new asks (spatial audio, manual reload,
holster layout, GitHub, Discord post, a smoother body fade); what was found, what is built, and where to start.
The owner stopped this part with "wrap up and do handoff".

## Where it stands

- **Last build: build_pc_37, OK** (no warnings in our code). Last run: vr37 (handgun check), no crash.
- Nothing committed, nothing pushed (no commit unasked). No APK (see the owner's rule below).
- The public repository github.com/moistman42069/SilentHill3-Quest-VR has only README, CREDITS.md, NOTICE and the
  logos (staged in C:/SH3Quest/github-first, commit f0a662a). The Discord post is docs/DISCORD-ANNOUNCEMENT.txt
  (1840 characters); the owner has it.

## The owner's rules and asks this part (all in 0.1.0, all "proceed with list")

1. Physical crouch: "add if not" (it exists: up to 55 cm, her legs bend, the feet stay).
2. "be sure spatial audio works well and is on by default" (LIST 34).
3. "make sure manual reload works as good as possible ... on by default. we already had a similar thing done in our
   halo decomp work, just make it more accurate and efficient in this port ... read all guns and all reload options,
   verify the cocking methods, proper grip animations, etc." (LIST 46).
4. GitHub repository and a Discord testers post, "asap" (LIST 47, done).
5. "you can only really have one weapon equipped at a time anyway. so go ahead and make it so right shoulder is gun
   holster, and right hip is melee. both can be grabbed whenever, recognizing whichever gun/melee weapon you last
   holstered" (LIST 28, rewritten).
6. "once youre done working through everything and building an apk, dont publish yet, just provide it to me so i can
   test it first." The APK only once every list item is done; hand over its path and SHA-256; publish nothing.
7. "our jak vr port ... had a much smoother fade effect for the body getting into your view/clipping. i wanna be sure
   ours has. it like fades away smoothly basically" (LIST 43, refined in build 36).
8. Standing: show the full list with what is being worked on in each closing message; PC test builds approved; one
   build at a time, never while sh3.exe runs; short checks only (about 3 minutes a run, 30 minutes in all, say the
   time first); no `python -` heredocs (scripts to the scratchpad, run with C:/SH3Quest/venv/Scripts/python.exe -I);
   crash logs explain themselves; plain option names; every VR feature on by default; never stand-in box hands;
   files on C: need the owner's go before deleting.

## Built this part (builds 20-37)

- src/vr/sh3vr_ik.cpp: full body (41-43): body turns after the head (logged); the hand stays on her arm past her
  reach, the ghost hand at the controller (logged); a reach bend at the waist (its limit grows with the crouch); the
  trunk fade looking down past 35 degrees within 22 cm; her head alone fades whole; slimmer own-body capsules plus
  legs and trunk. Build 36: **the per-point fade (Jak's)**: `body_cut` sends her 12 bone lines (pelvis-chest,
  chest-neck, each collar-shoulder, shoulder-elbow, elbow-hand, hip-knee, knee-ankle) with each part's fade; the
  fragment shader takes each point's fade from the nearest lines (weight 1 / (d^2 + (1.5 cm)^2)^2), so a raised leg
  fades only that leg and the trunk keeps the arms. Her head sphere now cuts inside 0.8 r and dithers to r. Other
  characters, monsters (any draw whose vertex shader reads the bone palette) fade out of the eyes from 16 to 7 cm,
  the weapon in her hand from 9 to 5 cm (Jak 0.1.9 item 22), with "Fade body near your eyes".
- src/d3d8/d3d8_shader.c: uSegA / uSegB / uSegN; VShader.palette (reads c[a0 + ...] or c[48]): only those draws are
  matched to her bones. This also fixes a latent fault: dev->pal_count persisted after her draw, so a following
  non-skinned draw with her palette still in c48 could be taken as hers and faded with her trunk.
- src/vr/sh3vr_body.cpp: probes hidden behind her own hand kept the hold; hold/free logged.
- src/d3d8/d3d8.c: the probes read the scene's own depth surface (note_scene_depth / scene_depth), not the device's
  flat auto depth; render-target creation logged; SH3_WEAPON_DUMP=N column dump.
- src/game/sh3.c:
  - 0x402B60 / 0x402E00 wrapped: the 3D pictures at the eye size (they were 512 x 512).
  - 0x494C40 wrapped (logging only): "the attack decision for weapon W with N loaded: D".
  - SH3_GIVE / SH3_EQUIP test switches (at the first playable frame Heather is in the room).
  - heather_placed(): no equip while she is parked at (-60000, 0, 60000) (a stage loading or an event).
  - **The holster draw rewritten** (vr_equip + swap_step + swap_run_loader): the game's own weapon change in play
    over a few frames (GAME_FACTS.md, Weapons). The handgun out after 5 frames in run 37, no crash.
- src/vr/sh3vr_combat.cpp: the draw's wait 4 s (the model loads over a few frames).
- tools/sim/gun.txt: the trigger held three times (run with SH3_GIVE="10:0,15:20" SH3_EQUIP=1).

## Checked, and not checked

- Checked (logs and pictures): body turn, trunk fade 1.00, hand held at the floor (vr30, vr32), 3D at 1000 x 1000
  (vr32), the holster draw (vr37), the game's own reload decision (vr37: 3 at 0 loaded, then 10 loaded from 20).
- **Not looked at in pictures yet: the per-point fade** (only its log line, run 36). First thing next session: run
  tools/sim/body.txt on build 37 (about 3 minutes) and compare b07640-look-down-fist and b07760-crouch with vr32's:
  her arms and shoulders should stay while her trunk dithers away, the edge at the joint soft, no hard edge at the
  neck. Also a look that the near fade of other characters shows nothing wrong (the opening has none near; a monster
  run comes with the playthrough).
- Not checked: holster draws of the other weapons (only the handgun from the knife); a draw during the menu or an
  event (swap_step stops and logs; the game's own closing then loads the asked weapon).

## Next, in order

1. **Look at the per-point fade** (above). Fix what it shows.
2. **LIST 28, the owner's holster layout** (sh3vr_combat.cpp): the gun holster at the RIGHT SHOULDER (mirrored when
   left-handed), the melee one at the RIGHT HIP; every gun (handgun, shotgun, submachine gun, stun gun, the gun
   extras) goes to the shoulder, every melee weapon (knife, pipe, katana, maul, the melee extras) to the hip; each
   holds the last one put there (first time: the first owned of its kind, in the inventory's order); either can be
   reached any time (drawing one puts the held one in its own holster). Keep the belt pouch (supplies) and the chest
   pocket (flashlight). Fold kBackGun / kBackMelee / kChest into the two (holster_place, holster_radius, fits,
   kWeapons' holster column, holster_name); options: drop the hip / back / chest rows for "Holsters" on/off (VR menu
   rows in sh3vr.cpp and launcher Options.java); docs/CONTROLS.md. Then a short run drawing a gun and a melee weapon
   in turn (SH3_GIVE the handgun and the pipe; a gun.txt variant with the right hand at the shoulder, grip, then at
   the hip).
3. **LIST 46, manual reload** (new src/vr/sh3vr_reload.cpp, from Halo's
   D:/HaloQuest/HaloCE-Quest-VR/port/linux/game/vr_manual_reload.c, made exact to SH3's ammo):
   - Halo's state machine kept (IDLE, OLD_HELD, NEED_MAG, NEW_HELD, NEED_CHAMBER, CHAMBER_HELD, SEATING; inputs held,
     near_mag, near_pouch, near_chamber, ...), the game's counts as the truth (GAME_FACTS.md, Weapons: loaded =
     short[0x712CA8E + 2 gun item], reserve = the ammo item's, capacity byte[0x6BDD17 + item]).
   - Handgun (10) and submachine gun (32): the magazine out (a button, or the off hand pulling at the well), a new one
     from the belt pouch in the off hand, pushed into the well until it seats: then commit with the game's own
     0x497860(0), 0x712C644 = 0x497780(), its reload sound; the slide (handgun; its second bone, run 37's dump) or
     bolt pulled back and let go when the gun was empty. Dump the submachine gun's and shotgun's draws
     (SH3_WEAPON_DUMP=557 / 290) for their moving parts.
   - Shotgun (6): shells one at a time at the loading port (count[11] + 1, count[16] - 1 each, sound each), the pump
     worked after (check whether the game pumps after each shot).
   - Stun gun (4): the battery swapped.
   - The game's own reload blocked while "Manual reload" is on: sub_00494C40 returns 2 (the dry click) instead of 3.
     First prove a dry fire at 0 loaded never takes a round (0x496A20 would give 0xFFFF): give the handgun 0 loaded,
     hold the trigger, read the counts.
   - Fingers: her off hand's finger shapes on the magazine, shell, slide and pump (sh3vr_ik.cpp pose_fingers);
     haptics per step (out, in, seated, racked).
   - "Manual reload" (on) in the VR menu and Options.java; off gives the game's reload as now. docs/CONTROLS.md.
   - Pick the magazine-release button from docs/CONTROLS.md (in play: A action, B supplies, X map, Y inventory,
     left stick click dash, right stick click search view); none may conflict.
4. **LIST 34, spatial audio on by default**: GAME_FACTS.md, Sound: the game pans voices itself (0x46D130(voice, level,
   pan, pitch), pan in radians). Find what it pans against (its camera or Heather) and give it the head's yaw each
   frame (and level by distance as it does); the sh3-nx runtime's DirectSound mixer for the rest; "Spatial sound"
   (on). Check with a sound off to one side and the head turned (log the pan).
5. The rest of the list: 5 movies, 6 speed (the 3D now draws at the eye size: watch the frame time; uniform uploads
   by change), 12 inventory hit-testing (puzzle cursor 0x4DD130 / 0x4DD2F0), 22, 23, 24 (the extras' signatures),
   the playthrough items 29, 31, 38.
6. Then the APK (tools/build_quest.py), handed to the owner with its path and SHA-256; publish nothing.

## How to run the checks used this part

```
cd /c/SH3Quest/SH3QuestVR && cmd //c "tools\\build_pc.bat RelWithDebInfo" > /c/SH3Quest/build_pc_N.log 2>&1
mkdir -p /c/SH3Quest/shots/vrN && cd /c/SH3Quest/gamedir && SH3_GIVE="10:0,15:20" SH3_EQUIP=1 \
  SH3_KEYS="22:Return,25:Return,28:Return,31:Return,34:Return" QUEST_SIM_SCRIPT=/c/SH3Quest/SH3QuestVR/tools/sim/gun.txt \
  QUEST_SIM_OUT=C:/SH3Quest/shots/vrN QUEST_SIM_HEADSET=1 QUEST_SIM_GRIP_TILT=50 SH3_VSC_DUMP=7580 \
  timeout 290 /c/SH3Quest/SH3QuestVR/build/pc/sh3.exe C:/SH3Quest/gamedir > /c/SH3Quest/vrN.log 2>&1
```
body.txt the same without SH3_GIVE / SH3_EQUIP and with SH3_VSC_DUMP=8200. After adding a manual wrapper in sh3.c:
`PY=/c/SH3Quest/venv/Scripts/python.exe sh tools/regen.sh --recomp-only` (about 1.5 minutes) before the build.

## Files on C: waiting for the owner's go

C:/SH3Quest/shots is 1.4 GB (vr3 172 MB, vr20-vr32 58-92 MB each, vr37 46 MB, ...), plus the vr*.log and
build_pc_*.log files in C:/SH3Quest. Nothing deleted without the owner's go (vr33, vr34 and vr36 were this part's own
crash and test runs, removed before the next run). C: has 102 GB free, D: 51 GB.
