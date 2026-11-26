# SH3 Quest VR: handoff, session 14 (2026-11-25)

Read docs/HANDOFF-2026-11-25-SESSION13.md first (its "Open, in order" and commands); this one carries on from it.
PC test builds 173-176 (build_pc_176.log, BUILD OK); runs vr198-vr205. Nothing committed, no APK yet.

## The owner's messages this session (C:/SH3Quest/notes/owner-messages-sessions1-7.txt, 59-66)

- 59: pick up exactly where session 13 stopped, miss nothing.
- 60: "some of the wrists in the two hand holds in these images look warped/twisted ... make sure it looks clean/smooth".
- 61: "make sure beam saber works well lol, just incase".
- 62-64: the body shadow "doesnt look as smooth and accurate as id like ... the ratchet and clank shadow works fine and
  shows the whole body"; "dont take shortcuts and be proper and efficient".
- 65: "make sure physical melee is proper and responsive for everything. actually causes proper directional hit
  reactions based on when and where hand/weapon/melee weapon makes impact".
- 66: "doors, drawers, and all that have been checked right? ... be sure its all been looked at and coded to the best
  of ability before the one combined short check".

## Done in session 14

- Owners 62-64, her shadow from her own mesh (builds 173-174, vr199): each skinned draw of hers and of the weapon in
  her hand is captured (d3d8.c her_shadow_capture: 48-byte vertices, the shader's constants), skinned on the CPU as the
  shader does (sh3vr_ik.cpp shadow_part), welded (shadow_mesh), and its faces lit by the chest lamp with their
  silhouette edges make closed z-fail volumes (6 m, inside the game's 20.6 m far plane). vr199: arms, hands, the gun
  and the beam saber whole and solid on the floor and the wall. The bone capsules stay only as a fallback.
- Owner 60, the wrists (builds 174-175): the forearm from her rest pose, the upper arm on the elbow's hinge learned
  from the game's animation, the twist shared 40% forearm / 45% twin bone and kept within 200 degrees of the game's own
  twist for that hold (learned), the elbow swung for what a forearm cannot. vr204: the left hand's twist -105..-160
  against the game's -98..-120 (vr199 ran to -270); the submachine gun's four pictures clean at the watch.
- Owner 65, hits at contact (build 175, sh3vr_combat.cpp update_swings, combat_melee_landed, combat_hit_given; sh3.c
  0x4ACE10, 0x4B33E0 and 0x4B6A20 wrappers): each fast frame of a swing sends the weapon's (or fist's) sweep since the
  frame before to the game's battle step as its own attack; the game finds whom it meets that frame, the hit's way is
  the tip's motion then (0x4B37AD), so the reaction goes the way the weapon went. Hit records (0x4AEA90) tell which
  sweep landed: one hit a swing, a knock in that hand, a fist's thud, the melee weapon's game attack pressed then (its
  own volumes dropped). A fist's damage halved at 0x4B6A20 (every landed punch reacts). vr204: every swing sent its
  sweeps (knife, beam saber, fists, the submachine gun as a club), the game ran on; no monster at the start to hit.
- Owner 61, the beam saber (builds 175-176): the trigger with the saber in the VR hand is the game's ready alone (vr202:
  ready + attack held gave action 37 and no beam; vr204: the ready alone lit it every time, flag 0x712C66B = 1, state
  0 -> 4 -> 1, a swing then the game's attack). Its beam is the game's own screen-space strip (vr203: 8 primitives,
  a 64x64 texture) at her animated hand: moved with the saber to the VR hilt (combat_effect_rel's beam branch). vr204:
  its glows moved, the strip stayed (and the hand's depth probes took it for a wall); build 176 makes blended
  screen-space effects write no depth and logs each candidate: vr205.
- Owner 66, doors and drawers: the hand's own ray first, then the game's own 1.02 m toward the hand (build 175, sh3.c
  0x4B9940 wrapper, both the light tick and the closing hand). vr204 reached event 8's spot but no 8-way sweep met it;
  build 176 logs each hand ray (SH3_HAND_RAY_LOG) and vr205 runs the event scan from her very spot and a 16-way sweep.
- The list audit (owner 66): docs/AUDIT-2026-11-25.md re-read; every row is seen, coded or headset-only; floor items
  and drawers have no instance in the opening (0xBA9610 indexes the room's own table), the same game check as doors.
- tools/build_quest.py runs Gradle --offline (Ratchet's cache under C:/RACQuest/toolchain/gradle holds Gradle 8.10.2
  and the Android plugin 8.7.3: nothing downloaded).

## vr205 (build 176, tools/sim/combined205.txt), checked; session stopped here (owner: "wrap up and do a handoff")

Log C:/SH3Quest/vr205.log, pictures C:/SH3Quest/shots/vr205 (w07530-w07625 the beam saber, i08205 / i08436 event 8).

The beam saber, NOT fixed yet:
- w07570 / w07590 look the same as vr204's: the green strip starts at the VR hilt's top end and goes
  sideways to the left across the view. It does NOT follow the hilt's axis (in the picture the hilt runs from 910,830
  up to 740,525; the strip runs from 800,545 to 380,570). The game's 128x128 glows were moved with the saber
  (frame 7522's "the weapon's effect ... texture 128 ... moved with it" lines).
- No answer yet on whether the 64x64 strip reached the effect map. combat_effect_rel's candidate log
  (sh3vr_combat.cpp, "beam saber held, an effect") does `told++ < 40` on EVERY call. At frame 7472 the fog's 512x512
  quads used up all 40 (only 2 of them were within 2.5 m and printed), so no line about the strip was ever printed.
- While lit, the right hand was "held at a surface" (frame 7543, then 0.60 buzzes until 7569; the sound "weapon on a
  wall" at -15742 -639 19878). Either the strip still writes depth (it never took the world-effect path, which happens
  when its z is exactly 0 or 1, so build 176's no-depth rule never applies to it), or the blade's line meets the floor
  grate. The vertex log below settles which.
- Next (planned, NOT written):
  (a) combat_effect_rel: count only the lines it prints, and only textures of 128 or less.
  (b) d3d8.c vr_route: add SH3_BEAM_VERTS=N, which logs each rhw draw while 0x712C656 == 9 and 0x712C66B != 0 with a
      texture of 128 or less: the caller MEM32(g_esp), count, stride, texture size, blend, the z range, the route,
      rhw_world, rhw_mapped and each vertex's x y z rhw. That gives the strip's depths and whether it was mapped.
  (c) Then the fix. If the strip's vertices are true per-vertex depths on the game's blade, the rigid map is right and
      the take test is what fails: widen it (`along` both ways, in case the measured tip is the pommel's end). If they
      are not (z 0, or one z for all), draw the strip along the VR blade: project the VR blade's base and tip with the
      game's frame VP, rewrite the uploaded copy's vertices with those screen places and true depths (keep the game's
      width), and use no map. In both cases a blended effect must not write depth.
  Check with one short run: beam203.txt-style, the trigger at 7385, pictures at 7430 and later, about 3 minutes.

The door / event 8 (LIST 45, owner 66): the code works as the game's button does; the test stood her too far back:
- Event 8 is Robbie the Rabbit lying on the ground (examine), not a door (i08205: right before her, low).
- Event scan at frame 8060 from where the walk stopped (-17313 21324, facing -45): the game's own button (500 units,
  all 8 facings) does NOT find event 8 from that spot. It finds it only from 0.5 m further ahead (-17486 21497), facing
  the same way. The steer counts a point as reached within 100 units, so the walk to (-17386 21386) stopped 0.2 m short.
- The 16-way left-hand sweep (palm about 0.82 m out at 1.25 m height; the ray, then the game's 500 toward the hand)
  correctly found nothing (every "hand ray" line says event -1). vr172 had already used Robbie by hand from a nearer
  spot (the game's examine text).
- Next: SH3_STEER_TO="-17386,21386;-17560,21570". The second point is about 0.35 m past the spot where the scan found
  it, so a 0.2 m early stop still leaves her inside the button's range. Then the same sweep. Expect "can use event 8"
  (a light tick) and, on a closing hand, "used event 8" with the examine text. Doors and drawers go through the same
  game check (there is none in the opening).

Melee in vr205: every swing sent its sweeps (the beam saber 3-4, fists 3) and "met no one": there is no monster at the
start. The reactions' direction still needs a monster: a playthrough or the headset.

## Open, in order

1. The beam saber: (a)-(c) above in one build, then one short check. Then the Robbie walk with the new steer points
   (it can go in the same run if the script carries on after the beam part, as combined205 does).
2. The APK pass: `source /c/RACQuest/env.sh` then `C:/SH3Quest/venv/Scripts/python.exe -I tools/build_quest.py`
   (the Android build has never been compiled: its first pass; expect host-layer compile errors). Hand over the path
   and SHA-256; publish nothing.
3. Headset or playthrough only (HEADSET_CHECKLIST.md): melee against monsters (the reactions' way, the contact
   timing), the beam on the hilt, a door, a drawer and a floor item by hand, and session 13's list.

## Files changed in session 14

- src/vr/sh3vr_ik.cpp: shadow_part / shadow_mesh (her mesh's shadow), her_shadow_tris; solve_two's hinge; rest_sub;
  the twist center and swivel (arm_to); SH3_ARM_LOG.
- src/vr/sh3vr_combat.cpp: swings' sweeps at contact (Swing, Sweep, FistHit, fire_game_attack, swing_kind,
  queue_extra), combat_melee_landed, combat_hit_given, combat_melee_volume's drop, combat_trigger_readies, the beam
  branch of combat_effect_rel.
- src/vr/sh3vr.cpp, sh3vr.h, sh3vr_internal.h: the glue (sh3vr_shadow_part, sh3vr_melee_landed, sh3vr_hit_given,
  sh3vr_effect_map's texture width); the trigger as the ready for the beam saber.
- src/d3d8/d3d8.c, d3d8.h, d3d8_shader.c: her shadow's capture and z-fail volumes; the texture width to the effect
  map; no depth writes for blended screen-space effects in the world.
- src/game/sh3.c: SH3_BEAM_LOG; the hand's reach fallback; SH3_HAND_RAY_LOG; the steer's facing; 0x4ACE10's drop,
  0x4B33E0's hit records, 0x4B6A20 for monsters.
- tools/build_quest.py (--offline); tools/sim: combined199, beam200, beam203, combined204, combined205.
- docs: LIST-0.1.0.md (19, 24, 27, 42, 45), GAME_FACTS.md (session 14 section), HEADSET_CHECKLIST.md (melee at
  contact, the beam saber, doors by reach), HANDOFF-2026-11-25-SESSION13.md (its vr197 section), this handoff.
- Backups beside the project (C:/SH3Quest/*.before-s14*): sh3vr_combat.cpp, sh3.c, LIST, GAME_FACTS, HEADSET_CHECKLIST,
  the SESSION13 handoff.

Waiting for the owner's go (files on C:, never deleted without it): the shots folders and logs as session 13 lists,
plus vr198-vr205 and build_pc_173-176.log, and the backups above.

## The owner's rules

As in SESSION12.md's last section (unchanged).
