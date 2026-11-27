# SH3 Quest VR handoff, session 25 (2026-11-26): 0.1.7 in progress, NOT published

Repo C:/SH3Quest/SH3QuestVR. 0.1.6 = ba72cb1 (tag v0.1.6, versionCode 7; the handoff commit after it is 62432d6).
0.1.7 is uncommitted in the working tree (14 files, about 850 lines) plus new sim scripts. Every code change below is in
PC build 227 (build_pc_227.log, BUILD OK 12:58); no edit came after it. The session stopped on the owner's "wrap up and
do handoff" (message 131) partway through the grip fit. Release, docs and the 0.1.7 commit are still to do. The owner
said to publish 0.1.7 without asking once everything is done and checked (message 123).

## The owner's messages (C:/SH3Quest/notes/owner-messages-sessions1-7.txt, 123-131)

- 123: hand grips on weapons are off ("as perfect as possible"). Weapons act as if they collide mid-walk or in the
  air, with a ding (124/125 follow-up: a regression). Heather's normal shadow renders oddly. "dont ask me anything jusst
  do whats best and publish when done without asking me."
- 124: two-hand grip calibration (adjust where the second hand locks).
- 125: a player's feedback on 0.1.6:
  1. pressing A while moving keeps her walking;
  2. the intro dog's grab sticks her to the dead dog;
  3. absurd knifing needed;
  4. arms go crazy sometimes;
  5. no feedback on a successful interaction.
- 126: the player: the Flat version launches to a black screen (sound plays).
- 127: the player's video (D:/Downloads/63838334d4fc9ddcaf157f531cc0f4bd.mp4): with the dog dead she stays rubberbanded
  to its last position.
- 128: put the game's purple hold bar on screen, smooth and natural, with a toggle (on by default). Melee damage on by
  default and tuned for VR.
- 129: grips should look natural and fitted to the guns; "some fingers are a bit off, or the hand position", "the
  pistol and smg for example".
- 130: "make two hand grip calibration PER GUN/weapon". It already is (see below); one choice is left open.
- 131: wrap up and do handoff.

## What 0.1.7 changed so far (all in build 227)

### The ding regression (123, 125.4), sh3vr_body.cpp

- **Her pixels are now known exactly by stencil.** The weapon-line probes' pass reads her pixels' stencil mark (bit
  0x20, set by d3d8.c for every draw of Heather's and the weapon in her hand) into the alpha. Before, the capsules round
  her bones missed her skirt, chest, shoulders, feet and faded parts, and the probes took those for walls.
- **Fallback:** if the stencil read is refused, the capsules and her column are used, as before. stencil_reads is -1
  until tried, 0 if refused, 1 if working, logged every ~10 s.
- **A contact needs two answers in a row** (the hand probes too: arm_runs), so one stray sample never holds the weapon
  back or knocks it on nothing.
- weapon_wall_line() logs what the line saw when a contact started.
- "Arms go crazy" (125.4) was not reproduced on its own. These phantom contacts pulling the hands, and the dead dog's
  hold, are the likely causes; both are fixed.

### Heather's shadow (123)

- **The cause:** in a lit place the game casts her shadow itself. Its map list 0x687F00 has a type-0 record for her
  (object 0x0712D260), and 0x490810 draws her kg1 shadow model's volumes z-pass, in her ANIMATION's pose. That never
  matched the IK body: legs walked under the view, and the eyes sat inside her head's volume.
- **d3d8.c:** her_game_volume() leaves those volume draws out.
- **sh3vr_ik.cpp** (her_room_shadow_tris): her drawn mesh and the weapon are counted z-fail from the lamp, into the
  game's own count, at full strength. The volumes run to 16 m, kept inside the game camera's far plane.
- **Fallback:** if her mesh is not caught (-1), the game's own volumes are kept.
- Test: SH3_NO_ROOM_SHADOW=1 turns this off. The flat-plane contacts in the alley (vr295/296) are a real wall; vr297
  ruled out the room shadow.

### Walking on after A (125.1), sh3.c

- **The fix:** the frame VR's steering stops, her walk velocity (+0xF0/+0xF8) is cleared once.
- **Why:** an examine stops the playable update, and the last step's velocity carried her through the whole message.

### The dead dog's hold (125.2, 127), sh3.c, struggle step

- **While held, a hit on her holder** (+0x17C) breaks the hold: the time to get free 0x712C608 is set to 0. The holder
  is left at least 1 HP, so the game's own release runs; the next hit kills it.
- **A holder found dead** frees her.
- **Test:** SH3_KILL_HOLDER=N gives the holder a deadly hit on the N-th struggle step.
- **Rig:** the intro dog's hold was not reached on the rig (vr295/296/299, even with check290's SH3_STEER_TO). The
  logic was checked with SH3_STRUGGLE_TEST.

### Interaction feedback (125.5)

- sh3.c: the action button's check finding something the game uses calls sh3vr_button_action_found. The result is a
  tick (kSoundUse, sh3vr_sounds.cpp) and a buzz in the hand, once per press (30 frames apart).

### Flat black screen (126), sh3vr.cpp

- **The fix:** on Android, vr=0 now starts the headset and keeps the view on the cinema screen (ViewMode::kScreen,
  flat_screen). The game's camera, HUD and menus are on the screen, the controllers act as the game's pad, and the VR
  menu works as ever.
- **Why:** the immersive activity shows only what OpenXR is given.
- **The PC** keeps its flat window. SH3_FLAT_SCREEN=1 with vr=0 tries the headset Flat on the rig.

### Melee damage (125.3, 128)

- **"Melee damage"** (melee_damage_pct): on by default at 200%, range 50-400. Rows are in the VR menu and the launcher
  (Options.java).
- **The scaling:** sh3.c scales the game's own damage (+0x15C) by the factor for a swing or punch hit.
- **sh3vr_combat.cpp:** melee_class_factor gives the knife and fists 1.5x, others 1.0. It multiplies the swing-speed
  factor; a pending swing's own game attack uses the class factor.
- At 200% a dog takes about 6 knife slashes or 3 pipe swings (attack table 0x6CE170: knife 8/9 = 40, 10-14 = 30; pipe
  15/16 = 130; the hall dog about 712 HP).
- Not yet rig-logged on a real hit.

### The break-free bar (128)

- **The picture:** wrist.h render_hold (purple, 256x40). It reaches the headset as xr.h HoldLayer / show_hold, an
  XrCompositionLayerQuad after the wrist in xr_openxr.cpp, and is stamped in the sim by xr_sim.cpp stamp_quad.
- **sh3vr.cpp show_hold_bar:**
  - Placement: 1 m ahead, 0.16 m down, 0.24 m wide.
  - Fade in and out over 0.25 s, rising in and growing.
  - It follows the gaze lazily and empties smoothly as she struggles.
  - Each counted press lights it over a slow throb. When freed it empties and rises away.
  - Shown in first and third person; the cinema screen keeps the game's HUD.
- **The source:** sh3vr_combat.cpp struggle_bar().
- **Option:** "Break-free bar when grabbed" (hold_bar, View page, on by default, in the launcher too).
- **Verified:** vr300 (tools/sim/bar300.txt, SH3_STRUGGLE_TEST): the pictures were good.

### Two-hand grip calibration (124, 130), sh3vr_combat.cpp update_two_hand

- **The option:** "Two-hand grip spot" (grip_spot, Hands and weapons; VR menu only, not the launcher). It has three
  settings: "As saved", "Adjust: take hold where you want it", "Back to the natural spots".
- **Adjust:** the off hand takes hold anywhere within 10 cm of the weapon's line. That spot is stored for THE WEAPON IN
  HAND ONLY: calib[profile].second_cm, saved to sh3quest_weapons.ini in that weapon's own section as SecondGripRightCm /
  UpCm / ForwardCm. It is clamped to ±10, ±10, ±30 cm and confirmed with a buzz and the use sound.
- **Reset:** "Back to the natural spots" resets EVERY weapon.
  - **Open choice for message 130:** have Reset with a weapon in hand reset only that weapon. Simple: in the
    grip_spot == 2 block, reset only calib[wf_one_hand.profile] when wf_one_hand.placed, else all.
- **The math:** second_point = second_natural + second_offset (used by the hand_round path).
- **STILL MISSING:** place_weapon's native off-hand pose (`native_pose(oh, *no, wf.fwd * -(pump * kUnitsPerMetre),
  np)`) must add `+ second_offset(wf)`, so the game's own left hand follows a calibrated spot too.
- Not rig-tested yet (Adjust, then Reset).

## The grip fit (129): where it stands

vr301 (tools/sim/grip301.txt; pictures in C:/SH3Quest/shots/vr301):
- handgun p07380-p07480: eyes, left, right, front, above, below;
- SMG q07900-q08000: the same views, two-handed with lh_at_second 1 + left_grip 1. **The SMG pictures are not yet
  viewed.**

What the handgun pictures show:
- The middle, ring and little fingers lie straight along the grip's right side, not round the front strap.
- The trigger finger floats ahead of the trigger.
- The hand sits high, with the grip's bottom sticking out below.

What the log says (vr301.log):
- "her own finger hands are shown at the controllers".
- "VR native grip: handgun, right hand (her group 14 ...) her hand bone at 0.0 0.0 0.0 in the weapon's bone 0, axes
  identity; the fist's centre at 24.4 -1.1 -14.3 (the measured hold's 39.3 -8.0 -13.9, 3.4 cm off)". The SMG right
  (group 21) is the same, 3.7 cm off.
- "parts: weapon 1 drawn: the game's own groups 00006021 00000001".
- SMG left (group 22): "the game's own hand 4.1 cm from the grip ... not taken, her finger hand on the grip".

Reading so far:
- **The swap:** sh3.c's part draw (0x45ED40 wrapper) swaps group 3 (the right finger hand) or 4 (the left) for the
  native group only when sh3vr_native_hand(h) ≥ 12. That needs own_hands_now() and native_shown[h], which place_weapon
  sets each frame when native_pose succeeds.
- **What is drawn:** the pictures look like the finger hand with VR curls, not the authored group 14.
- **The VR curls:** sh3vr_body.cpp ~1960. The index on a gun is 0.10 unless touched or pulled (0.42 + 0.33*trig).
  The other fingers come from finger_contact against the weapon capsules, else weapon_hold_curl.
- **pose_fingers** (sh3vr_ik.cpp:653) rotates each finger joint about sk.bend from sk.rel. It is called at :916
  (`if (!ik_off(512)) pose_fingers(B, h, curl)`) and :1328.

Next steps, in order:
1. **Find out whether group 14 is drawn.** Log sh3vr_native_hand(h), own_hands_now() and native_shown[h] at the part
   draw for the handgun. If it is not drawn, find which condition fails, likely fingers_frame != rt_frame or the reset
   at ~1116 running between place_weapon and the draw.
2. **If it is drawn but skinned to the finger bones,** keep the game's own finger-bone pose (her animation's grip,
   relative to her hand bone) for that hand and skip VR curls.
3. **Hand position:** the native fist is 3.4/3.7 cm from the measured hold. Decide which is right from the pictures:
   the grip's bottom must sit in the palm heel, the index on the trigger.
4. **SMG left (group 22):** look at why it is 4.1 cm off (the "over -100" frame). Either take it with an offset or
   improve the finger hand's wrap on the foregrip.
5. **Check every weapon's hold whole:** handgun, shotgun, SMG, stun gun, knife, pipe, katana, maul, beam saber,
   flamethrower. Look at weapon direction, fingers and wrist, from the eyes and outside. First check the PC VR mod's
   source (C:/SH3Quest/refs/Silent-Hill-3-VR-Mod) for a grip fix already made.

## What is left after the grip fit

1. Add second_offset to the native off-hand pose. Settle the per-weapon Reset choice (message 130).
2. Build 228, one build, then short combined rig checks (~30 min at most):
   - grip301 again, as vr302;
   - shadow291 with the owner's ini and with body=1;
   - walk292 with lh_at_second 1 (no ding while walking);
   - SH3_FLAT_SCREEN=1 with vr=0;
   - a melee damage log on a real hit;
   - the button tick;
   - grip Adjust and Reset;
   - the hold: the dog if reachable, else SH3_STRUGGLE_TEST + SH3_KILL_HOLDER.
   - Check every picture WHOLE.
3. Docs:
   - RELEASE-NOTES-0.1.7.md (no player names);
   - whats_new, features, CONTROLS, HEADSET_CHECKLIST;
   - GAME_FACTS: her type-0 shadow record and 0x490810; the attack table 0x6CE170; the struggle and holder (+0x17C,
     0x712C608); the hold line in the Life Gauge (0x5A5E40).
4. Release:
   - versionCode 8 / 0.1.7;
   - `source /c/RACQuest/env.sh; C:/SH3Quest/venv/Scripts/python.exe -I tools/build_quest.py --release`;
   - apkcheck against C:/SH3Quest/SH3QuestVR-0.1.6.apk;
   - verify on the release build;
   - commit, push main, annotated tag v0.1.7, release page with the notes, without asking (message 123);
   - the next handoff, memory, and the APK path and SHA-256 to the owner.

## Rig notes

- **PC build:** `cd /c/SH3Quest/SH3QuestVR && timeout 900 cmd //c "tools\\build_pc.bat" > /c/SH3Quest/build_pc_228.log 2>&1`,
  then grep BUILD OK. Never while sh3.exe runs.
- **Sim:** from /c/SH3Quest/gamedir with QUEST_SIM_HEADSET=1 QUEST_SIM_ALIGN=7312 QUEST_SIM_GRIP_TILT=50
  SH3_KEYS="22:Return,25:Return,28:Return,31:Return,34:Return", plus QUEST_SIM_SCRIPT and QUEST_SIM_OUT. mkdir the
  shots folder first. The next run is vr302.
- **Equipping weapons:** SH3_GIVE / SH3_EQUIP / SH3_EQUIP_EVERY. Handgun = item 10 (+15 ammo), weapon 1. SMG = item 12
  (+17), weapon 3.
- **New sim scripts this session:** check295, check296, check298, bar300, grip301, shadow291, walk292.
- **Source backups:** C:/SH3Quest/*.bak-s25 (xr_openxr.cpp, xr_sim.cpp, sh3vr_combat.cpp, Options.java).
