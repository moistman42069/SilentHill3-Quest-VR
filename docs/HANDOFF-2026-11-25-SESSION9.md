# Silent Hill 3 Quest VR: handoff for session 9 (2026-11-25)

Session 9 picked up session 8's list on the owner's "was working in another session. pick up from where it left off.
miss/forget nothing. proceed with list." (message 33). Read docs/HANDOFF-2026-11-24-SESSION3.md (rules, build, run),
SESSION4.md, HANDOFF-2026-11-24-SESSIONS5-7.md, HANDOFF-2026-11-25-SESSION8.md, then this. Every owner message is in
C:/SH3Quest/notes/owner-messages-sessions1-7.txt.

## Where it stands

- **Last build: build_pc_121, OK (not run yet). Last run: vr144** (build 120, tools/sim/wrist.txt; checked, items
  22-23). Session 9 built 109-121 and ran vr130-vr144 (about 55 minutes of checks, over the 30: vr137 and vr138 sat at
  the title without SH3_KEYS). Nothing committed, no APK.
- Shots C:/SH3Quest/shots/vr130-vr144, logs C:/SH3Quest/vr130-144.log, build_pc_109-121.log, regen_114.log, the dumped hand-sound
  clips C:/SH3Quest/sounds133 (about 1 MB), C:/SH3Quest/first_shell.txt (tiny, made on C: by mistake): all wait for
  the owner's go before deleting.

## Done in session 9

1. **The edge-out step reaches the seat's edge** (tools/sim/world.txt 7962: `rh_z 0.43`, `head_z 0.15`). vr130 (+24 cm)
   had the tips at the edge, vr131 (+38 cm, head_z and rh_z both moved 7 cm: they add) the whole hand past it, vr132
   (+18 cm, the midpoint) the palm on the seat's rounded edge with the fingers out past it.
2. **Why the fingers lay flat past the edge (vr132 frame 7985):** the palm rests on the edge (4.0 mm, normal -0.05 0.95
   0.31: the rounded nose), the fingers are drawn -0.08 0.00 0.08 0.00 0.10, flat in the air. Only 4 surface points and
   none behind the plane: the probes past the edge see the floor about 40 cm down, and read_answer kept only points
   within 12 cm beyond a probe (`pen > -0.12f`), so nothing told the hand the seat ended; the wrap (world_finger) curls
   a finger only until it meets a surface, and past the support there was none to meet.
3. **The implied front face of an edge seen from above** (src/vr/sh3vr_body.cpp, build 109):
   - read_answer keeps, for each kSurf/kTip probe that saw past it to a surface over 12 cm beyond (or to nothing: a
     point 20x out on the eye's ray), that far point (answer.beyond, beyond_n).
   - implied_face (called by fit_solids when only one plane was fitted): each such ray, and each fitted point over 3 cm
     behind plane a, crossed a where a is not. With 3 or more crossings, all out one way from a's seen points (their
     unit directions' mean at least 0.5) and none among them (the nearest crossing no nearer than the farthest seen
     point less 1 cm: a grating or slats fail), a ends halfway between (at most 3 cm past its farthest seen point):
     a second plane there, square to a, normal out along that way, no points of its own (counted everywhere), the
     pair convex. Solids.implied marks it. The fingers then wrap down against it as against a seen front face.
   - Logs: "an edge (its front face implied)" in "rests on"/"grips"/the planes, SH3_SURF_DUMP's "second plane
     (implied): at ..., normal ...; N probe(s) saw past the hand ...", the crash log's "(an edge, implied)".
4. **A sound for every hand action, played** (open item 11; builds 109-110):
   - src/win32/dsound.c: ds_vr_play(pcm, frames, rate, gl, gr, behind): 24 VR voices mixed over the game's buffers
     (linear resampling, the LIST 34 behind-the-head low-pass and -30%), at the game's sound-effects volume
     (MEMF(0x89E3B8), clamped; 1 if out of 0..1.5).
   - src/vr/sh3vr_sounds.cpp: at the first frame, 3 variants of each of the 18 sounds made at 44.1 kHz (damped modes
     for clicks and snaps, band-passed noise for slides and leather, noise grains for cloth, a falling sine for
     thumps), and 6 of the game's own read from data/sound/sd.afs: the empty clicks 0x29 (handgun) 0x3D (shotgun) 0x53
     (SMG) for the dry click; the reloads 0x28 / 0x3C's first shell / 0x52 for a seated magazine or shell when the
     game's own reload event is not known yet. Each ask is played from where it happened (pan 0.75 by the head's right
     vector, 0.55/(0.25+d) clamped 0.2-1.3, behind the head duller), pitch +-6% (the game's +-3%), a variant not the
     last one; the same sound within 60 ms dropped; a part's bounces half as loud each within 0.6 s; a knock or a pat
     louder the faster (detail = speed cm/s). Log: "VR sound: frame N: <name> (detail) at x y z: played ..." with
     the clip, distance, gains and behind. The crash log counts asked and played.
   - The game's 3D call 0x4A33B0 is not used for these: it also records a noise for the monsters (0x712C0DD..F0).
   - New asks: kSoundGrabWeapon (second grip taken, combat.cpp update_two_hand), kSoundWeaponWall (combat.cpp
     place_weapon's first touch, at the line's free point, detail = speed), kSoundPouch, kSoundHolsterDraw,
     kSoundHolsterPut (update_holsters). reload.cpp: play_sound returns whether the game's reload sound was queued; the
     seat's hand sound then carries kSoundByGame + item (logged, not played twice).
   - Option "Hand sounds" (opt.hand_sounds, on): sh3vr_internal.h, sh3vr.cpp kRows, Options.java (HANDS page).
   - SH3_SOUND_DUMP=folder writes every clip as a WAV; tools/sound_survey.py --files folder measures them.
5. Game sound facts (for GAME_FACTS.md): see "Sound facts" below. New tools: tools/sound_table.py, sound_calls.py,
   afs_list.py, sound_survey.py.
6. **The edge wrap works (vr135, build 112).** Two fixes after vr133:
   - finger_clear treats an implied edge as the whole wedge: max(side of the top, side of the implied face) within
     15 cm of the edge or over the top's seen part (vr133: mid-seat the palm was lifted 49 mm, the implied face counted
     alone past the top's seen part).
   - implied_face picks the way out by a cone: the direction with the most crossings within 45 degrees, then the mean
     of those within 60 (vr133: at the seat's corner the crossings went two ways and the near-end ones failed the
     separation test). Reasons for a miss are logged (implied_try: "under 3 rays went past it", ...).
   - vr135: 7930 rests on the implied edge, palm 5.3 mm, fingers -0.25 -0.22 -0.32 -0.36 -0.29; 7984 rests on the implied
     front edge (normal 0.17 -0.26 0.95, 8.3-9.4 cm past the seen points), fingers -0.10 0.00 0.30 0.20 0.10 (the index
     stays out: its want is 0); 8013 grips the edge, 0.27 0.50 0.99 0.99 1.00, hooked over it. Zoomed pictures whole.
7. **Items 2 and 3 checked** in the vr133 pictures: the left hand relaxed by the seat's end; the wall claw with each
   finger stopped by its tip.
8. **Hand sounds tuned (vr134-135):** the kSoundByGame marker only for MagSeated/ShellIn (a 1253 cm/s touch was
   misread); the other hand's touch kept (the same-place limiter, 10 cm); the pat louder and brighter (gain floor 0.6,
   base 0.22, now 357-421 Hz); the game's clips normalized to 0.9; the shotgun shell is 0x3C's first shell (354 ms).
9. **tools/sim/shadow.txt rewritten and run (vr136):** the light on from the start, the gestures, the floor shots. Found:
   - No hand shadows on the wall: the game's shadow caster sits 0.51 m along the beam, past the hand, and the hand was
     27 degrees above the beam's middle (outside its lit cone). vr137 puts the hand in the beam and tests the caster at
     0.25 m (SH3_CASTER_AHEAD, a rig switch; if the shadows show, that becomes VR's default for the chest light).
   - At 7455 (looking down 40) the right hand drew only as the faint ghost (a smooth blend, not the near-eye dither):
     the solid hand counted as held in something. vr137 dumps the arm's probes there (SH3_VSC_DUMP=7455).
   - Looking down 65: her shoes and the left arm with her watch whole; no stray parts.
10. **LIST 49, the health bar on her wrist (build 113):**
   - The game's Life Display (Extra options "Life Display"): bit 0x80 of byte 0x70E66C6 (set 0x60CB90, read 0x60CBC0).
     0x4E7C50 (0x4E8506-0x4E855D): with it on and [0x712C662] == 0, HP under 10/25/50 percent (floats 0x69005C,
     0x69060C, 0x68EFE0) calls 0x5F1C30(P) storing P in [0xBCC6DC]; 0x5F1C40 draws a red mark at the screen's right,
     alpha (sin(timeGetTime / P * pi/20 [0x6988A4]) + 1) * 127.5 (0x68A754 = 1, 0x68F128 = 127.5): a beat each 2 s, 1 s,
     0.4 s; DrawIndexedPrimitiveUP 12 vertices 10 triangles (0x71FA40 / 0x71FAE0); then clears [0xBCC6DC]. Heartbeat
     rumble 0x6D6B70/0x6D6BD8/0x6D6C40 via 0x5FED50. SH3 has no stamina: the bar is her health.
   - src/vr/wrist.h: render_wrist(health, alpha, pulse), 256 x 64, a dark rounded plate, light rim, the bar green over
     half, amber to a quarter, red under, ticks each tenth, pulsing under half. 6 cm x 1.5 cm on her wrist.
   - xr.h WristLayer + show_wrist; xr_openxr.cpp: a quad in the room (its own chain, remade only on a change of fill,
     fade or pulse; layers[12]); xr_sim.cpp: logged ("[sim] wrist bar: ...") and stamped on the pictures
     (stamp_wrist, a ray per pixel).
   - sh3vr.cpp show_wrist_bar: over her watch (sh3vr_body.cpp watch_place: 3.5 cm behind the heel of the hand, on the
     back of the wrist), shown while the back of the wrist faces the eyes (50 degrees) and is looked at (35 degrees,
     within 90 cm), fading over about 0.15 s, read left to right whichever way the arm lies, pulsing as the game's
     mark under half. On with "Health bar on your wrist" (opt.wrist_health, on; sh3vr.cpp kRows, Options.java) or the
     game's own Life Display (sh3.c passes the bit: Sh3VrGame.life_display).
   - SH3_HP=percent (sh3.c, the rig): her health held there.
11. **LIST 50, physical blocking (written, build 114; needs a monster to see):**
   - The game's damage to Heather: her damage step 0x4EC800(chr) first calls 0x4B6A20(chr), which walks the battle
     entries hitting her (0x4ACE40(chr, prev)); for a damaging one (entry +8 its kind, +0x2C, +0x2E the reaction, +0x34
     the attacker, +0xC its point, +0x1C its way) it adds 0x4B53D0's damage to chr+0x15C, sets +0x160, copies the point
     to +0x134 and the normalized way to +0x144, the reaction to +0x154, the kind to +0x16C (word), counts +0x16E and
     sets the attacker +0x17C. 0x4EC800 then (with +0x16C set and 0x712C666 / 0x712C662 / 0x712C663 clear) takes
     +0x15C from her health +0x180 (0x4EC95B) and adds it to +0x170. 0x712C662 is set when her health reaches 0.
     Monsters' hits from Heather's side: 0x4B6810 (the attacker Heather, [ebx+0x34] == [0x712C100]).
   - src/game/sh3.c wraps 0x4B6A20 (regen done): for Heather, what the call added to +0x15C is scaled by
     sh3vr_block(point, way, attacker's place (its +0, if within 10 m), damage, reaction).
   - src/vr/sh3vr_combat.cpp combat_block: guarded when her weapon's segment (segment()) or else a forearm
     (sh3vr_ik.cpp ik_forearm) has its middle 12-85 cm from her chest (eyes less 30 cm) and within 45 degrees of the
     line to the attacker (else the hit's point, else ahead of her), level. Keeps 0.25 of the damage behind a melee
     weapon (the knife 0.33), 0.5 behind a gun or the stun gun, 0.6 on a bare forearm. Buzz 0.9 for 0.12 s in the
     guarding hand; the weapon's knock (kSoundWeaponWall) or the hand's pat at detail 400. Logs every hit on her
     ("a hit on her (kind, damage, its point N cm from her chest, its way; from ...): guarded / not guarded") and the
     guard. Option "Block with your weapon or arms" (opt.blocking, on): internal.h, sh3vr.cpp kRows, Options.java.
12. **LIST 48, dynamic cutscenes (written, build 115):** sh3vr.cpp scene_first_person: in a scene with "Cutscenes" at
   the camera and "Dynamic cutscenes" on (opt.dynamic_cutscenes, on; rows in sh3vr.cpp and Options.java), first person
   while the game's camera has her head (as the game poses it: sh3vr_ik.cpp game_head, before VR's IK) in its frame
   (|x| < 0.70 z, |y| < 0.55 z) at 1.5-8 m; at the camera otherwise (her face up close stays at the camera). A change
   waits 0.5 s; each view holds 1.5 s; a new scene starts at the camera. A scene that ends in first person stays
   there (no half-second camera). In a scene, first person's eyes ride game_head (8 cm ahead, 2 cm up), for both
   "Dynamic" and "Cutscenes: First person". The rig's script alignment (sim_ready) counts only first person outside
   scenes. Logs "dynamic cutscenes: first person / at the game's camera (her head N m from the game's camera, in /
   up close in / out of its frame)".
13. **LIST 51 launcher, part (build 115, Java not compiled until the APK):** a features popup shown once when the game
   files first check out ready (LauncherActivity showFeaturesOnce after needCheck; text android/.../assets/features.txt:
   dynamic cutscenes first and how to turn them off, hands, weapons, controls as the Controls tab has them, comfort,
   picture), and a "Features" button beside "What's new"; What's new lists the new features. Refresh rate 120 Hz by
   default (sh3vr_internal.h, Options.java), with AppSW on (the owner's ask 16). Still open in LIST 51: dynamic
   resolution (Jak has it; SH3's eye pictures are made at a fixed size by the game's own render targets).
14. **Owner message 34 (session 9): "i know we had an option in our jak mod for example where both eyes share work or
   something, enable that too by default."** Jak's "Share work between eyes" (vr_options.h share_eye_work: Jak draws
   each eye apart and its second eye reuses the first's work). SH3 draws both eyes in one pass always (every render
   target a 2-layer array drawn with GL_OVR_multiview2: each of the game's draws goes to both eyes at once), so the
   work is shared already and a switch would do nothing; told in features.txt and What's new.
15. vr137 and vr138 waited at the title (their commands left out SH3_KEYS, the title menu's presses): no pictures;
   tools/sim/shadow.txt's header now names the keys. vr139 (build 115) is the run that checks items 9-10.
16. **vr139 checked:** no hand shadow on the wall with the caster at 0.25 m either (the difference from the start
   picture shows none: her own draws seem not to be in the shadow's pass), so the game's 0.51 m stays and LIST 56's
   "her head and gestures in her shadow" stays open. The "ghost" right hand at 7455 is not the ghost pass (the arm's
   probes: free, no ghost drawn): her forearm's draw got the flashlight whole (its middle in the cone) and her hand's
   none, a lit wristband beside a flat grey hand. No wrist bar at all (no "[sim] wrist bar" lines).
17. **Her flashlight per point (build 118, seen in vr142):** body_light also fills an Sh3VrBeam (the lamp, the way, the
   light at full, the cone 15/30 degrees, whole to 1.5 m, none past 3 m, clip-to-world) for every draw of hers while
   the light is on; d3d8_shader.c builds her dir4 vertex shaders with c11-c14 read as gC11-gC14 (sp_renamed) and, with
   uBeamPos.w > 0, runs vs_main a second time with the light from the lamp to that point. vr142 (pixels against
   vr139): the cuff in the beam 199,109,67 (was 145,72,39), the sleeve in the beam 88,71,58 (was 45,37,28); at 7455 the
   raised forearm 68,52,52 (was 169,99,76, lit beside the unlit hand): the hand and arm out of the beam now alike, lit
   only by the game's own lights (the hand is solid: the zoom shows no wall through it).
18. **The wrist bar found and placed (builds 116-119, runs vr140-vr143):** the eyes were on her real watch, but
   watch_place took the way of her straight fingers for the forearm's, and her model's fingers bend 42 degrees toward
   the palm at rest: the bar's point sat 12.7 cm off her wrist (SH3_WRIST_LOG=1 logs the place every 15 frames). Now
   from her forearm as drawn (ik_forearm: elbow, wrist joint and, new, the back of the wrist as the twist bone turns
   it, which takes half the hand's roll): vr142 showed it (frame 7529: faces 0.77, looked at 0.96, 0.18 m, shown 0.92;
   4733 pixels; faded to 0.04 when turned away) but along the forearm and 2.2 cm short of the watch's face, which sat
   45 degrees round. Build 119: 4.7 cm up the forearm, out of the watch's face, across the wrist as a watch reads, 5 cm
   wide. vr143 checks it.
19. **Owner message 35: "if im not wrong the game has two different meter bars, but yea just be sure."** From the
   code: one health indicator. The Life Display is a red "+" cross at the lower right (its 12 corners from 0x71FA84),
   only under 50%, pulsing faster under 25% and 10%; plus the heartbeat rumble and her hurt motion (0x4A3300 from her
   motion code). The only other health-share reader is a colour (0x5475C0), not a meter. No second bar, no stamina
   (GAME_FACTS.md).
20. **Dynamic cutscenes not seen yet:** vr139's log never left the game's camera in the opening's scene (vr2/vr3 show
   that scene flagged cutscene 1), so scene_first_person never held "her head in frame at 1.5-8 m". It now logs what it
   sees every 2 s of a scene ("dynamic cutscenes see: ..."); tools/sim/scenes.txt (new, about 1.5 minutes) runs the
   opening for it.
21. Docs: GAME_FACTS.md gained the flashlight and her lighting, the weapon in her hand, the film grain (sessions 5-9)
   and the Life Display's cross; HEADSET_CHECKLIST.md gained the features popup, dynamic cutscenes, film grain, 120 Hz,
   hands on surfaces, hand sounds, the flashlight on her hands, the watch and blocking; What's new the flashlight line.
   A slip: one command held an empty stdin-python line and hung; it was stopped (the rule stands: scripts in files).
22. **vr143 and vr144, the wrist bar:** vr143 (build 119) never showed it: the back of the wrist faced the eyes only
   0.33, as the twist bone took half the hand's roll. Build 120: the twist bone takes 0.8 of it (sh3vr_ik.cpp
   kTwistShare; also how her forearm looks turned). vr144, every picture whole: o07345 and o07370 the bar across her
   watch, amber, 4 of 10 filled from the eyes' left (40%), faces 0.72, eyes on it 0.96, 0.18 m, shown 0.99-1.00,
   about 3440 pixels; o07390 (palm to the eyes) faded to 0.04, the open hand and the band natural; o07410 back, 0.96.
   Her forearm turned at 0.8 shows no pinch or stretched skin. But the bar's middle sat 1.4 cm toward the thumb side
   short of the watch's face's middle and 0.5 cm toward the elbow (zoomed: the bar over the face's left half). Build
   121 (sh3vr_body.cpp watch_place): out of the face turned 25 degrees round toward the thumb, 4.1 cm up the forearm
   (was 4.7). tools/sim/wrist.txt gained o07430 (25 degrees less turned) and o07450 (25 more): the bar must stay on
   the face as her forearm twists.
23. **Dynamic cutscenes, what vr144's log saw in the opening:** at 1939 and 4161 her head 1.7 m away in frame (her
   eyes); at 2419 17.7 m, 2899 14.1 m, 3139 and 3379 14.8 m in frame but past the 8 m limit (the camera); the rest
   her head not drawn (the camera). Build 121 (sh3vr.cpp scene_first_person): 1.5 to 20 m, so a wide shot that shows
   her goes to her eyes too, as the owner's "where the character is rendered" asks; past 20 m she is a speck.

## Sound facts (found in session 9)

- 0x4A33B0(sound, volume float, float* pos, flags), cdecl: sets 0x712C0DD = 1, copies the position and volume to
  0x712C0E0-0x712C0F0 (the noise monsters hear, likely), then 0x46CDF0(sound, volume, pos, 10000.0f, 0, flags).
- 0x46CDF0(sound, volume, pos, range, pitch [-0x400..0x2FFF], flags): the 3D sound without the noise record. Volume
  index = [0x89E3B8] (SE master float) x volume x 127. 0x4A82B0(sound, volume index, pan, pitch) -> 0x4EE620 ->
  0x4EE440 (-1 for a sound not loaded). 0x46CD50(sound, volume, ?): the 2D wrapper.
- Sound id N = entry N of data/sound/sd.afs (378 entries; plain PCM WAVs up to 0x169, CRI/AIXF after), confirmed by
  the radio voices 0x163-0x166 and the guns. Fire / reload / empty click: handgun 0x27 / 0x28 (vol 0.5) / 0x29;
  shotgun 0x3B / 0x3C (6 hits, 0.7 s) / 0x3D; SMG 0x51 / 0x52 / 0x53. The empty clicks come from the empty-gun guard
  0x4E79A0 (by weapon byte 0x712C656, 1/2/3, through 0x4E5DE0, volume 0.5). 0x34 = item obtained (2D), after every
  0x4972B0 (add item) and by the pick-up 0x4B8040.
- 0x5680F0(chr, pos, ?, index): event index n plays sound row n-1 (bytes 0x5683FC, jumps 0x5683D8): rows 0-1 0x27,
  2 0x28, 3 0x3B, 4 0x3C, 5 0x51, 6 0x52, 69-70 0x48, 71 0x28, 72/75 0x49, 73 0x52, 74 0x51.

## Open, in order

1. **Run vr145** (build 121, tools/sim/wrist.txt, about 2.7 minutes; the command in item 22's run, vr144, with
   SH3_WRIST_LOG=1 SH3_HP=40 QUEST_SIM_ALIGN=7312 QUEST_SIM_GRIP_TILT=50 SH3_GIVE="0:0" SH3_EQUIP=0 SH3_KEYS): the bar
   centred on the watch's face in o07345, o07430 and o07450 (zoom 490-690 x 400-520, x4), amber at 40%; faded at
   o07390.
2. **Dynamic cutscenes (LIST 48):** run tools/sim/scenes.txt on build 121 (SH3_KEYS, no ALIGN; about 1.5 minutes):
   the log's "her eyes" at the 14-15 m wide shots now; its pictures (s02880 ... s05280) whole: her eyes' view where
   she is drawn, the camera where she is not, no flicker at cuts.
3. World items: drawers, handles, doors (ask 28): the probe-based finger stops reach any drawn object; to see at a door
   or handle in a world run.
4. Blocking (LIST 50): written; to see with a monster's hit.
5. LIST 56: her head and gestures in her shadow: her draws are not in the shadow's pass (vr139); to find that pass.
6. Launcher (LIST 51): dynamic resolution still open; the Java compiles with the APK.
7. Grips (LIST 52).
8. Hand sounds: done in code; still to hear in a weapons run (tools/sim/weapons.txt drives the reload, the holsters
   and the pouch) and on the headset.
9. Weapons against the world, and the Alyx finger feel on weapons.
10. The handgun slide drawn as its own faces; the forearm twist.
11. Re-run tools/sim/weapons.txt and check every picture whole.
12. LIST 5 movies, 6 speed, 12 inventory hit-testing, 22-24, 29, 31, 38.
13. Docs: GAME_FACTS.md (sessions 5-9 facts in), CONTROLS.md (block and health rows in), HEADSET_CHECKLIST.md and
    What's new (updated in session 9): to keep current.
14. The APK, last: only once everything is tested and refined; path and SHA-256 to the owner; publish nothing.

## Files changed in session 9

- src/vr/sh3vr_body.cpp: answer.beyond/beyond_n and their collection; Solids.implied; implied_face; fit_solids calls
  it; the logs.
- src/vr/sh3vr_sounds.cpp: rewritten (the clips, the game's own, playing).
- src/win32/dsound.c: ds_vr_play, mix_vr.
- src/vr/sh3vr_internal.h: opt.hand_sounds; kSoundByGame; hand_sound's comment.
- src/vr/sh3vr.cpp: the "Hand sounds" row. android/.../Options.java: the "Hand sounds" row.
- src/vr/sh3vr_combat.cpp: the five new hand_sound calls. src/vr/sh3vr_reload.cpp: play_sound returns bool; seated.
- tools/sim/world.txt: the edge-out step. tools/sound_survey.py (--files), sound_table.py, sound_calls.py, afs_list.py.
- Build 111-113: src/vr/sh3vr_body.cpp finger_clear's wedge, implied_face's cone, implied_try, watch_place;
  sh3vr_sounds.cpp tuning; src/vr/wrist.h (new); xr.h WristLayer/show_wrist; xr_openxr.cpp and xr_sim.cpp the wrist
  layer; sh3vr.cpp show_wrist_bar and the row; sh3vr_internal.h opt.wrist_health, watch_place; sh3vr.h
  Sh3VrGame.life_display; src/game/sh3.c the Life Display bit and SH3_HP; Options.java the row; tools/sim/shadow.txt;
  tools/onsets.py.
- Builds 116-121: sh3vr_body.cpp watch_place (from the forearm, bool, the face's turn) and body_light (Sh3VrBeam);
  sh3vr_ik.cpp watch_out, kTwistShare 0.8, ik_forearm's watch_out; sh3vr_internal.h, sh3vr.h (Sh3VrBeam,
  sh3vr_her_light); d3d8_shader.c her per-point flashlight (uBeam*); sh3vr.cpp show_wrist_bar, the wrist and
  dynamic-cutscene logs, scene_first_person's 20 m; wrist.h 5 cm; tools/sim/wrist.txt and scenes.txt (new).

## The owner's rules (unchanged)

As SESSION8.md: the full list with states in each closing message; PC test builds approved, one at a time, never while
sh3.exe runs; short checks (2-3.5 minutes a run, about 30 minutes in all; say the time first); no `python -` heredocs
or stdin python; Edit tool or plain sed; no commit or publish unasked; files on C: wait for the owner's go; plain option
names; every VR option on by default (film grain off); never box hands; check every picture whole with numbers; a sound
for every VR action; crash logs explain themselves; downloads need permission (numpy was not installed); the APK only
when everything is tested and refined. Write the handoff and the memory as soon as the state changes much.
