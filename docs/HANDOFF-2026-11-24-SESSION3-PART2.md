# Silent Hill 3 Quest VR: handoff, 2026-11-24 (session 3, part 2)

Read docs/HANDOFF-2026-11-24-SESSION3.md first (rules, how to build and run). This part covers the owner's later asks in
session 3 (LIST items 41-45, all in 0.1.0), what was found, the design worked out for each, and where to start.

## Where it stands

- **Nothing new is compiled.** The last build is build_pc_19 (OK), which only adds the "parts:" logging in the
  0x45EC00 wrapper (src/game/sh3.c). The tree still draws the box hands (src/vr/sh3vr_body.cpp `build_hand`) and has
  the arm-only IK (src/vr/sh3vr_ik.cpp). Both are to be replaced as below.
- Changed in this part, docs only: LIST items 41-45 (section H), GAME_FACTS.md Bones (parts, groups, masks, finger and
  leg chains, 0x45EC90 / 0x45ECF0 / 0x45ED40 signatures), this file.
- PC test builds are approved. No APK until the owner says. No commit unasked.

## The owner's asks (all "proceed with list"; each is in the release)

1. Jak's body fade, on by default (LIST 43).
2. Full-body IK and finger bending as smooth as possible; as native-VR as possible, building on Jak (OpenGOAL) and
   Ratchet and improving on them (LIST 41, 42).
3. **No box hands.** The owner was angry about the stand-in boxes: "if you were planning on keeping those fucking box
   fingers you need to reevaluate yourself right now". Heather's own finger-skinned hands only (LIST 41).
4. "make the FULL body as proper as possible, full inverse kinematics, etc." (LIST 42).
5. "full world haptics for steps, environment actions, etc. similar to our opengoal approach with different
   vibration/haptic toggles/options" (LIST 44).
6. "proper environment interaction where possible, like opening drawers, grabbing items physically to put in
   inventory, etc." (LIST 45, added at the end of this part; no design work done yet beyond the plan below).
7. Show the full list with what is being worked on, in each closing message.

## Facts found this part (also in GAME_FACTS.md, Bones)

- Heather's model has 55 parts. Groups: 0 body, 1 scene head, 5 play head, **3 her right hand with fingers (parts
  10-13), 4 her left hand with fingers (parts 14-18)**, 12-31 and 33-38 rigid one-bone hands (play's), 32 a chest item.
  Play shows mask 0x3021 (groups 0, 5, 12, 13); scenes 0x1B (0, 1, 3, 4). The mask is the skeleton's +0x10 / +0x14.
- 0x45ED40 (the part draw): eax = model, stack (skeleton, world), cdecl. Part i drawn when bit [part+0x52] of the mask.
- 0x45EC90(model) builds the [model+0x14] blends at [0x6B4540]+0x2000; 0x45ECF0(model) transposes the bones and the
  blends in place. Both one argument, cdecl.
- In play the finger bones (43-72) are stale: their pose must be built from the rest matrices at model+[model+0x08]
  (0x80 for Heather). Whether those are local or model-space is not yet checked (log both on first use and validate:
  axes about unit length, finger segments 3-80 units).
- A skinned draw's palette rows (c48 on) carry the bone origin in w: draws are matched to bones by origin.
- `apply_render_states` (src/d3d8/d3d8.c:716) runs on every draw, before `shader_bind_for_draw`
  (src/d3d8/d3d8_shader.c:792): a draw's GL state (depth, blend) can be overridden at the end of the latter for one
  draw and the next draw puts it back.
- Facing: src/game/sh3.c:202 sets her yaw to the stick's heading at once while steering; :364 sets it from
  `mv.face_yaw` while standing; src/vr/sh3vr.cpp:1041-1048 (plan_move) snaps it when the head is 0.8 rad off.
- Grabbing today (src/vr/sh3vr_combat.cpp:713 `update_grabbing`): a grip closing away from the head presses the game's
  action (A) for 3 frames. That is the only environment interaction so far.
- Jak's constants used below: shoulder lift 0.35, shoulder reach 0.25, elbow pole eased at 25/s in the chest frame;
  body fade out 0.08 s, held 0.35 s, back 0.28 s, dithered; near fade 7-16 cm.

## The work, in order (design settled; none of it written yet)

### A. Her own hands drawn (LIST 41) — src/game/sh3.c, then regen

1. New wrapper `sub_0045ED40`: `model = g_eax`, `skel = ARG(0)`, `world = ARG(1)`. For Heather (73 bones, 55 parts,
   part 10 group 3, part 14 group 4 — check once) in first person when `sh3vr_wants_own_hands()` says so: save the
   mask, clear groups 12-31 (low bits 12-31) and 33-38 (high bits 1-6), set groups 3 and 4, call
   `sub_0045ED40_gen()`, put the mask back.
2. The ghost pass, right after (only when VR hands it a ghost pose): save [base, base + 73*64) and
   [base+0x2000, base+0x2000 + [model+0x14]*64); copy the ghost bones (untransposed) to base;
   `guest_call(0x45EC90, 1, &model, 0)`; `guest_call(0x45ECF0, 1, &model, 0)`; mask = the ghost hand's group only;
   tell VR the ghost draw is on; push world, skel and 0xFFFFFFF0, set g_eax = model, call the gen, put g_esp back;
   VR's ghost flag off; restore both regions and the mask (the shadow draws after use them).
3. Pass the rest matrices too: `sh3vr_heather_bones(bones, 73, parents, rest)` with `rest = model + MEM32(model+8)`.
4. Regen (`PY=/c/SH3Quest/venv/Scripts/python.exe sh tools/regen.sh --recomp-only`, about 1.5 min) and add
   `case 0x0045ED40u: return sub_0045ED40;` to `recomp_lookup_manual`.

### B. The full-body module — rewrite src/vr/sh3vr_ik.cpp

Per frame, in the 0x45EC00 wrapper (bones untransposed, before 0x45EC90), first person with her body shown:
1. Copy the animated pose; note each ankle's world matrix (the feet's targets).
2. Walking = her root moving over 0.25 m/s. Standing: feet planted in the world; a foot steps (0.25 s, 5 cm lift,
   smoothstep) when its plant is over 12 cm or 25 degrees from where the animation puts it, one foot at a time; plants
   reset after a jump of over 1 m (doors, loads). Walking: the animation's feet.
3. Lean: the spine (bone 2) subtree turned toward the eye (the eye expected 7 cm ahead of and 8 units above her head
   bone), at most 35 degrees, eased, off while walking.
4. Crouch: what drop is left between her head bone and the eye lowers the whole body (at most 0.55 m, eased, 2 cm dead
   zone; never raises her; off when seated).
5. Head and neck turned toward the headset's forward: 40% neck (6), 60% head (9), at most 70 degrees (for her shadow
   and mirrors; the head stays cut from the eyes).
6. Legs: hip -> knee -> ankle to the targets (the knee toward its animated side), the ankle's world turn put back so
   the foot and toes keep the floor.
7. Arms, Jak's refinements: her hand's wanted world matrix from the controller (below); the collar turned so the
   shoulder lifts with a raised hand (0.35) and reaches with a far one (0.25), at most about 6 cm; two-bone solve with
   the elbow hint (the controller's forearm direction plus down and out) eased at 25/s in the chest's axes; stretch up
   to 1.12x by moving the elbow and hand subtrees so the hand never leaves the controller; the hand's twist shared
   half with the forearm's twin bone (40 / 42), clamped to 90 degrees.
8. Her hand's turn from the grip pose: OpenXR grip -Y is wrist to knuckles (along), -Z pinky to thumb (across),
   palm out = (-mx, 0, 0) (mx = 1 right, -1 left). Her hand's local along = rest origin of the middle-finger base;
   local palm = -cross(along, pinky->index) for the right, +cross for the left (a right-handed frame). Build both
   orthonormal frames and map hers onto the controller's. Place the hand so her middle knuckle is at the grip point
   (0.031 mx, -0.048, -0.008) — the shown pose, i.e. at the wall when the hand went through.
9. Fingers: from the rest relations (rel[b] = rest[b] * inverse(rest[parent]) if model-space; rest[b] if local), each
   finger's three joints turned about cross(finger direction, palm) by curl x {80, 95, 65} degrees (thumb {25, 40,
   35}), the curls being `hands[h].curl` (controls, stopped by the world). Identify fingers in the rest pose: the
   thumb's base is nearest the wrist, the index nearest the thumb, then middle, ring, pinky by distance; log it.
10. Option "Heather's arms follow your hands" off: her hand still goes to the controller, the arm cut short; body
    hidden (opt.body 1): her hands alone drawn (fade everything else).
11. Fades (Jak's): trunk fades when her chest is within about 25 cm of the eye or her body is turned over (pelvis to
    chest more than 50 degrees off vertical: knocked down, grabbed); each leg when its knee or ankle is within about
    30 cm of the eye or above its hip. Out at 1/0.08 s, held 0.35 s, back at 1/0.28 s; shown through smoothstep.
12. Keep the drawn bone origins (for matching draws) and her head's place (the sphere cut), frame-stamped.
13. Ghost pose: a hand through a wall gets a second solve to the real controller on a copy; that copy is what part A2
    draws.
14. Footsteps for haptics: an ankle coming down and stopping near the floor.
15. Give body.cpp her real finger knuckles and lengths (grip space, metres) so the probes match her fingers: make
    `kFingers` a runtime copy filled from step 9's rest data.

### C. Fades in the draw — src/d3d8/d3d8_shader.c, src/vr/sh3vr.cpp, sh3vr.h

- Replace `sh3vr_near_cull` with `int sh3vr_body_cut(const float vsc[96][4], uint32_t stride, Sh3VrCut *out)`:
  `near_cut`, `sphere[4]`, `eye[4]` (w > 0: world positions needed), `fade[4]` (x the part's dithered fade, y / z the
  near fade's 7 / 16 cm x 490 from the eye, w the ghost's alpha), `clip_to_world[16]`.
- Classify each of her draws by matching palette origins to the drawn bones: any finger bone = hand (never fades, no
  cut); any head bone (9 and below) = head (the 17 cm sphere); leg bones without arm bones = leg (max of trunk and that
  leg); arm bones = arm (the near fade; "Hide arms near your eyes" widens it to 12-30 cm); else trunk. No match but
  hers = trunk. Keep the 0.25 m flat fallback when her bones are unknown.
- Shader: vertex stage computes vCutWorld when uCutEye.w > 0; fragment stage: hard sphere cut, then
  `f = max(fade.x, 1 - smoothstep(fade.y, fade.z, distance(vCutWorld, eye)))` against a 4x4 Bayer threshold
  (discard). Ghost (fade.w > 0): colour alpha = fade.w, and at the end of `shader_bind_for_draw`: depth test off,
  depth writes off, blend SRC_ALPHA / ONE_MINUS_SRC_ALPHA (the next draw's apply_render_states restores).
- Ghost hand alpha about 0.3; the other hand not drawn in that pass.

### D. src/vr/sh3vr_body.cpp

- Delete `build_hand`, the box/segment mesh code, the stand-in forearm and the box ghosts (the hand program stays only
  if something else draws with it; otherwise remove it too). Keep the probes, guide dot, vignette, head fade.
- Probes from her real finger shapes (B15). Probe stops: close at once, release eased (no flicker); curls eased
  exponentially (`1 - exp(-rate dt)`), not the linear `smooth_to`.

### E. Options (src/vr/sh3vr.cpp option rows; the launcher's Options.java with the same keys)

- "Fade body near your eyes" (on), "Hide arms near your eyes" (off), "Vibration on walls and floors" (on),
  "Extra vibration": Off / Pickups / The whole world (default the whole world), "Buzz on weapon change" (on),
  "Take items with your hand" (on, LIST 45). Rename "Haptics" to "Vibration" and its strength to "Vibration strength".

### F. Smooth body turning — src/vr/sh3vr.cpp plan_move, src/game/sh3.c:202

- Standing: start turning when the head is about 50 degrees off, stop within 10, at most 4 rad/s, eased; the feet step
  through B2's plants. Steering: her facing limited to 10 rad/s (sh3.c:202 uses `mv.face_yaw` when set; the velocity
  keeps `mv.heading`).

### G. Haptics (LIST 44)

- Footsteps from B14 (stronger running), landings and knock-downs (the root's drop, the turned-over state of B11),
  walls and floors pressure from the probes' penetration (harder the deeper), pickups and doors (from LIST 45's
  hook), the game's rumble (already: dinput.c -> sh3vr_rumble), the radio's static (LIST 23: needs the radio's state),
  weapon change buzz. Each under its option.

### H. Environment interaction (LIST 45) — needs reverse engineering first

- Find the game's action check: what the action button (A) tests (event regions or item spots near Heather, her
  facing), where the active spots and items are kept (positions, kinds: pick-up, door, drawer, examine). Start from
  the pad action 5 ("action") in the input path and the event update 0x4B8B70 (GAME_FACTS.md, Events), with
  tools/xrefs.py and tools/disx.py.
- Then: a closing hand within reach of a spot runs the game's action at that spot (Heather turned to it for the
  frame), never by pressing A blindly; the item shown in her hand (its model, or the inventory's picture) until put
  away over the shoulder or at the hip; doors, drawers, lockers pushed or pulled with a grip; a haptic and the game's
  sound. Each kind of spot recorded in docs/PUZZLES.md.

### I. Checks after the build (say the time first; about 3 min a run)

- tools/sim/arms.txt with QUEST_SIM_GRIP_TILT=50; plus a look-down, a crouch (`head_y -0.5`) and a turn-in-place
  script; SH3_VSC_DUMP on one frame to see the hand parts drawn and the finger bones' pose. Check from her eyes
  (view_free cannot show her from outside).

## The list after that

5 movies, 6 speed, 12 inventory hit-testing, 22, 23, 24, 34, then the playthrough (29, 31, 38). The full list with
each item's state is docs/LIST-0.1.0.md.

## Files left on C: (the owner's go before deleting)

C:\SH3Quest\shots\vr3 (172 MB), vr11 (58 MB), vr7 (23 MB), run2 (17 MB), run6, vr1, run7, parts1; logs
C:\SH3Quest\vr*.log, run*.log, cut1.log, parts1.log, build_pc_*.log.
