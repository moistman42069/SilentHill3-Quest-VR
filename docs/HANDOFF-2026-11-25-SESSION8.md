# Silent Hill 3 Quest VR: handoff for session 8 (2026-11-25)

Session 8 ended on the owner's "wrap up and do a handoff". Read docs/HANDOFF-2026-11-24-SESSION3.md (rules, build,
run), SESSION4.md, HANDOFF-2026-11-24-SESSIONS5-7.md (the owner's asks 1-30 verbatim, the facts, the open list), then
this. Every owner message is in C:/SH3Quest/notes/owner-messages-sessions1-7.txt (31 and 32 are session 8's).

## Where it stands

- **Last build: build_pc_108, OK. Last run: vr129** (tools/sim/world.txt on build 108, about 2.9 minutes, exit 0).
  Session 8 built 97-108 and ran vr110-vr129. Nothing committed (not a git repository yet), no APK.
- The owner's messages this session:
  - 31: "hmm, im not seeing the fingers/hands properly bending to the walls in some of these images. again, i want it
    similar to half life alyx/our other ports. proceed with list."
  - 32: "wrap up and do a handoff"
- Shots on C: (C:/SH3Quest/shots, vr1-vr129), the logs C:/SH3Quest/vr*.log and build_pc_*.log, and the clang crash
  files %USERPROFILE%/AppData/Local/Temp/sh3vr_body-cb03de.cpp / .sh all wait for the owner's go before deleting.

## Done in session 8

1. State rebuilt from sessions 5-7 (docs/HANDOFF-2026-11-24-SESSIONS5-7.md, C:/SH3Quest/notes).
2. Film grain off by default, checked again in play (LIST 55).
3. The flashlight (LIST 54, now [x]): the hold in the fist (palm on its side, fingers round its lower body, thumb over
   its top, the lens clear above the fingers), the beam from the lens' clear part, the glare follows the lamp in the
   hand (the frame's order: the record is written at 0x58F068, the glare reads it at 0x58F097), her own hands lit by
   it (body_light). Pictures vr111-vr113.
4. **Her hands on the world as in Half-Life: Alyx and the Jak and Ratchet ports** (owner message 31; asks 4 and 28;
   LIST 53). Built 100-108, seen in vr124-vr129:
   - Each hand fits planes to the scene's depth around it: a ring of 12 probes (12 cm and 6.5 cm about the palm, in
     the eye's screen plane), the palm's middle, a point under the palm, and the fingertips' probes. Points on her own
     body are left out. A least-squares plane, dropping the farthest point until it fits within 8 mm; a second plane
     from the rest when it is 30 degrees off or more (an edge or a corner). Planes are kept 6 frames in the world and
     blended when close (25 degrees, 3 cm). Since build 108 a plane counts for a finger only over the part that was
     seen (its fitted points plus 3 cm), so fingers past a seat's or a ledge's edge are free to curl down over it.
   - The palm is moved out of the surface (4 mm gap) together with each finger's best curl (bent back down to -0.4,
     11 mm gap for the fingers); a push of over 45 cm, or the shoulder behind the plane, is ignored.
   - The fingers: Jak's world_finger solver (curl = wanted + its offset; up to +0.7 wrap when the palm faces a plane
     within 6 cm; in a plane: the nearest free curl in 0.02 steps; 18/s when stopped or opening, 7/s otherwise; capped
     by the tip probes and the weapon). Bent back (below 0) the joints share the bend as kFingerBack in
     sh3vr_internal.h ({3.0, 0.5, 0.5} the thumb, {1.0, 0.45, 0.6} the fingers) so the hand lies flat.
   - A sound ("hand on a surface", kSoundHandSurface; logged only, see open 11) and a buzz when a hand comes to rest.
   - The crash log names each hand's surface (resting or free, planes or an edge, how old, how far the palm moved).
   - New logs: "rests on" / "grips" (the plane, the palm's mm, the controller's cm, each curl, each stop, the fingers'
     nearest joint and skin mm); SH3_SURF_DUMP=frame logs each hand's points and planes for three frames.
   - **vr129 numbers (build 108):** floor palm 4.0 mm off, fingers -0.27..-0.38, skin 0.3 mm; floor grip a fist
     (palm 60 mm); wall palm (fingers up) 5.8 mm off, fingers -0.19..-0.34, skin 0.8 mm; wall grip a claw (palm 60 mm,
     fingers 0.27 0.24 0.98 1.00 1.00); the seat top 5.2-5.8 mm off, fingers -0.12..-0.29, skin 0.1-0.3 mm. The same
     as vr127/vr128: the support change broke nothing.
   - tools/sim/world.txt (new, about 2.9 minutes): the floor (palm, grip, poke), the walk to the brick wall, the wall
     (poke, poke grip, palm with the fingers up, palm grip), the bench (palm on the seat, grip, 8 cm nearer the front
     edge, grip). Its header explains each step and the reach sums (scratch simpose.py, copied below).

## Open, in order

1. **The edge wrap over the bench seat, not yet seen.** In vr129 the right hand lies flat on the seat with its fingers
   pointing at the seat's front edge (left in the picture) but its fingertips stop about 6-10 cm short of that edge,
   so no finger is past it (frame 7983's dump: one plane, 5 points within 0.8 mm, one point 70 mm below it). The seat's
   front face is seen edge-on from her eyes (eye z 0.15, the plane's middle z 0.09), so it gives no second plane: the
   seen-part support (build 108) is what lets fingers curl there. Next: in world.txt move the edge-out step 12 cm
   further left and lean with it so the reach stays 0.58 m: at 7962 `head_z 0.12` and `rh_z 0.40` (instead of rh_z
   0.28); then the grip at 7987. Check that the fingers past the edge curl down (about +0.7) while those on the seat
   stay flat, in the "rests on" line and the zoomed pictures. If the hand moves past her reach the log says "past her
   arm's reach" and the ghost hand shows.
2. The left hand at the bench (vr129 f07935/f07985) rests against the seat's near end face (normal +X, palm 17 mm,
   the controller 4.2 cm past it): it reads, but its pose (fingers spread toward the eyes, over her knee) should be
   looked at whole once more; it is the off hand kept out of the way (lh_y -0.3).
3. Mixed curls in a grip against a wall (vr129 f07805: 0.75 0.50 0.28 1.00 1.00, the middle finger stopped by its tip
   probe at 0.28): may be right for a claw on bricks; look at it zoomed against Jak's.
4. Then the world items of ask 28 (drawers, handles, doors, items: the finger stops of session 7 with the planes),
   in a room that has them.
5. Rewrite tools/sim/shadow.txt (Read it first; drop the left trigger at 7335, which turns the light off): her body
   shadow with the IK body, the gestures (point, thumbs up, middle finger, open) and the head in the shadow; re-shoot
   the thumbs up [asks 16; LIST 19/42/56].
6. Wrist health/stamina bar at her watch (LIST 49): not started.
7. Physical blocking, on by default (LIST 50): not started (the game's guard: kRT with a melee weapon raised; needs RE).
8. Dynamic cutscenes (LIST 48): not started.
9. Launcher (LIST 51): best graphics by default, the first-run features popup (dynamic cutscenes first), the updater
   and What's new like Jak/Ratchet.
10. Session 5's grips (LIST 52): pipe/maul left, SMG left, shotgun pump, thumb swing and finger solver, RE4-like holds.
11. A sound for every hand action: hand_sound() in sh3vr_sounds.cpp only logs; play through the game's 3D sound
    0x004A33B0 (holsters, the pouch, the new "hand on a surface").
12. Weapons against the world and the Alyx finger feel on weapons (LIST 53's weapon half).
13. The handgun slide drawn as its own faces; the forearm twist.
14. Re-run tools/sim/weapons.txt on the latest build and check every picture whole.
15. LIST 5 movies, 6 speed, 12 inventory hit-testing, 22-24, 29, 31, 38.
16. Docs: GAME_FACTS.md (the flashlight facts, the film grain, the shot path, 0x495360, her shader lighting /
    body_light, the glare's order, the hand planes), CONTROLS.md, HEADSET_CHECKLIST.md (hands on walls, floors and
    seats), What's new.
17. LIST 40, the APK, last: only once everything is tested and refined; path and SHA-256 to the owner; publish nothing.

## Files changed in session 8 (hands on the world)

- src/vr/sh3vr_internal.h: HandState curl[5], push, resting; kFingerBackMost, kFingerBack; kSoundHandSurface.
- src/vr/sh3vr_sounds.cpp: the sound's name.
- src/vr/sh3vr_ik.cpp: pose_fingers bends back below 0 by kFingerBack.
- src/vr/sh3vr_body.cpp: the kSurf probes (kProbeMax 160; the shader's uUV[160] matches), the answer's surface points
  and serial, and the block "the surfaces at each hand (session 8)" (fit_plane, fit_solids, set_support, supported,
  hand_solids, finger_clear, palm_push, world_finger, ...); body_frame (push, resting, the sound, the logs, the dump);
  body_fingers (the solver for a free hand); body_crash_info.
- tools/sim/world.txt (new).

## How to run the world check

```
cd /c/SH3Quest/SH3QuestVR && cmd //c "tools\\build_pc.bat" > /c/SH3Quest/build_pc_N.log 2>&1   # look for BUILD OK
mkdir -p /c/SH3Quest/shots/vrN
cd /c/SH3Quest/gamedir && SH3_SURF_DUMP=7983 QUEST_SIM_ALIGN=7312 QUEST_SIM_GRIP_TILT=50 SH3_GIVE="0:0" SH3_EQUIP=0 \
  SH3_KEYS="22:Return,25:Return,28:Return,31:Return,34:Return" QUEST_SIM_SCRIPT=C:/SH3Quest/SH3QuestVR/tools/sim/world.txt \
  QUEST_SIM_OUT=C:/SH3Quest/shots/vrN timeout 340 /c/SH3Quest/SH3QuestVR/build/pc/sh3.exe C:/SH3Quest/gamedir > /c/SH3Quest/vrN.log 2>&1
grep -n "rests on\|grips\|held at a surface" /c/SH3Quest/vrN.log
```
The sim's hands: head + yaw(head) * (+-0.2, -0.35, -0.3) + the rh_/lh_ offsets (room axes); her reach is about 0.58 m
from the shoulder (eyes + yaw * (+-0.18, -0.24, 0.06)). tools/sim/simpose.py (copied from session 8's scratch) finds
hand angles (`search right palm_x palm_y palm_z fingers_x fingers_y fingers_z`) and offsets with the shoulder distance
(`offset right head_yaw head_y x y z`). Run it with C:/SH3Quest/venv/Scripts/python.exe -I.

## The owner's rules (unchanged)

As SESSIONS5-7.md: the full list with states in each closing message; PC test builds approved, one at a time, never
while sh3.exe runs; short checks (2-3.5 minutes a run, about 30 minutes in all; say the time first); no `python -`
heredocs or stdin python (scripts in files, run with -I); Edit tool or plain sed; no commit or publish unasked; files
on C: wait for the owner's go; plain option names; every VR option on by default (film grain off); never box hands;
check every picture whole with numbers; a sound for every VR action; crash logs explain themselves; the APK only when
confident everything is tested and refined. Write the handoff and the memory as soon as the state changes much.
