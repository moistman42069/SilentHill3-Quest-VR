# Silent Hill 3 Quest VR: handoff, 2026-11-24 (session 4)

Read docs/HANDOFF-2026-11-24-SESSION3.md first (rules, how to build and run), then SESSION3-PART3.md, then this.
The owner stopped this session with "wrap up and do a handoff". Start at "Next, in order" below.

## Where it stands

- **Last build: build_pc_45, OK.** Last run: vr48, no crash. Since build 45 only docs changed (LIST 34's docs, this
  handoff, the list). No code was changed after the problems below were found.
- Nothing committed (C:/SH3Quest/SH3QuestVR is not a git repository yet: LIST 40 makes the local one). No APK.
- Check runs used this session: vr40-vr48, about 24.5 of the owner's 30 minutes. The fixes below need about four
  more short runs (each about 2.5 minutes): that goes past 30 minutes, so say the time first.

## The owner's asks this session (all in 0.1.0)

1. "add some form of visual to indicate the gun being cocked ... maybe a seperate cocking mesh": built (the slide, the
   knob and the pump move), but see SECOND below.
2. "in all the images i see in this chat with guns, they seem to be the wrong direction": TRUE, all of them. I had
   looked at those pictures for the moving parts and missed it. The owner was rightly angry ("check yourself and
   everything your doing so mistakes like that dont happen"): look at whole pictures and the numbers behind them,
   not only the part being tested.
3. "i noticed some of the models looked warped, be sure the gun cocking/meshes in general are clean and smooth."
4. "check all the guns incase, again man, be sure EVERYTHING works well and is polished in this build."
5. "gun cocking should also have proper audio, as all vr actions should."
6. Standing rules are unchanged (SESSION3-PART3.md, "The owner's rules"): the full list in each closing message, PC
   test builds approved, one build at a time and never while sh3.exe runs, short checks (say the time first), no
   `python -` heredocs (scratchpad scripts run with C:/SH3Quest/venv/Scripts/python.exe -I), edits by the Edit tool or
   plain sed only, no commit or publish unasked, files on C: need the owner's go before deleting, the APK only once
   the whole list is done (path and SHA-256 to the owner, publish nothing).

## Done this session (builds 38-45, runs vr38-vr48)

- 43 body fade: seen in pictures (vr43, vr47): looking down, her trunk dithers away, legs, arms and hands stay.
- 28 holsters: the owner's layout (gun over the main-hand shoulder, melee on the main-hand hip, each giving back the
  last put there), vr39.
- Fixed: the holsters, pouches and her shoulders were measured from the eyes, which drop about 12 cm when the head
  looks down; now from the eyes with the head level (rig.h level_eyes). Seen in vr45/vr47.
- 46 manual reload works by hand, without free ammo (vr42): handgun, shotgun shells and pump, submachine gun knob,
  stun gun battery, the game's own reload with a controller lost. The slide, knob and pump moving: d3d8.c
  vr_gun_part (see SECOND).
- 34 spatial sound, DONE (vr48): sh3.c wraps the two 3D sound calls (0x46CDF0, 0x46CFE0) and inside them gives the
  game the eyes' view and place as its listener (0x4A0500, 0x471800); 0x60BAE0's angle also tells dsound.c how far
  behind the head a sound is (low-pass and up to 30% softer). The listener check matched the game camera's view on
  the first try. "Spatial sound" (View, on). GAME_FACTS.md Sound, README, What's new, HEADSET_CHECKLIST updated. Not
  heard on the rig: a sound off to one side while the head turns (the opening's café plays none); on the checklist.

## Next, in order

### FIRST: every weapon placed right (LIST 24, 46): the guns point backwards

Evidence: the pictures (vr42 k07400, k07940, k08560; vr44 c07420 from the side: the grip out at the far end, the hand
on the muzzle). And the numbers: with run vr37's dumped bones (c48-c53 of the 368-vertex draw) and
combat_weapon_draw's math, the handgun's barrel (model +x) points straight back (forward -1.00), the muzzle 7.8 cm
ahead of the hand, the grip 21 cm ahead (that dump came from inside the game's reload animation, so its pose between
the two bones is not the idle one; the direction is the calibration's either way).

Cause (src/vr/sh3vr_combat.cpp, combat_weapon_draw): the reference bone (bone 1 if whole, the PC mod's choice) goes to
the hand turned by the PC mod's default angles (pitch 90, yaw 90, roll 45, kept in sh3quest_weapons.ini). Those were
for the PC mod's controller frame; in ours (hand_w = world_quat(grip pose): x right, y down, z forward) yaw 90 sends
model +x, the barrel, to -z: backwards. And bone 1 is not the grip: in every gun it is a smaller part stitched to the
body by shared faces (tools/gun_pieces.py: pieces with bones [0, 1]), so the gun's place hangs on it.

The fix (designed, not written):
1. Place each weapon by its BODY bone (the one with the most vertices) in the reload module's gun frame
   (sh3vr_reload.cpp gun_point: the grip's centre at the palm, the controller's aim turn: x right, y up, -z forward):
   origin = hands[mh].shown.position (the grip pose, or the wall contact when the hand is in a wall), axes from
   hands[mh].aim.orientation. Model barrel axis -> aim forward, model top (+y) -> aim up, model +z -> aim right (the
   game's bone matrices are proper rotations, det +1, checked; so this keeps the model unmirrored). Then
   H.r = [fwd up right] in world axes (world_dir) times the scale, H.t = world_point(origin) - H.r * grip_point; every
   other bone B' = H * inverse(B_body as the game posed it) * B (the game's pose between the bones, so the stitched
   faces stay as the game draws them, the game's own reload animation included when "Manual reload" is off). The
   two-hand line and the recoil kick turn this frame as they turn hand_w now.
2. The model points (body bone units, 490 a metre; tools/gun_extents.py and tools/gun_pieces.py on
   C:/SH3Quest/gamedir/mesh_N.txt):
   - Handgun 368/610, body bone 0 (282 vertices), barrel +x, top +y. Grip straps: back x 20.8 -> 32.4 over y -24 ->
     30, front x 51.2 -> 57.2 over y -23 -> 6 (a 12 degree grip). Palm about (39.3, -8, -13.9); the grip's bottom
     (the magazine well) about (36.5, -28, -13.9); the slide's rear top x 25-40, y 34-48; the muzzle x 101.4. Centre
     plane z about -13.9 (the sights sit at z -8 to -10: the model is not quite symmetric). Bone 1: 86 vertices.
   - Submachine gun 557/890, body bone 0 (404), barrel +x, top +y. Grip x 26.4-56.7, y -21 to 29, z -17.8 to -3.5
     (upright; palm about (41.5, 8, -10.6)); the magazine's bottom y -43 at x 35-52; the knob x 0-12.7, y 55-69.5;
     the front sight x 96-105, y 50-62; the rear cap x -40 to -23; the muzzle about x 115. Bone 1: 153 vertices.
   - Shotgun 290/490, body bone 1 (244): the barrel along -x, from the receiver (x 0 to -13) to the muzzle x -257.6;
     the pump is whole pieces of bone 1 at x -129 to -13. Bone 0 (46 vertices, y down to -99) is the stock with the
     grip: where it sits from bone 1 needs the idle dump (step 4).
   - Stun gun 132/226, body bone 0 (60): x 10.4-71.7, y -46.9 to 27.1 (the grip down to y -47 at x about 39);
     bone 1: 72 vertices.
   - Knife 166/334, steel pipe 172/294, katana 69/122 + 72/114 (two draws), maul 208/426: no dumps yet (step 4).
     They use the same placement now, so they are probably wrong too: check every one.
3. sh3quest_weapons.ini: its angles and position become adjustments from that frame (0 by default); write a Version
   line and rewrite a file without it (the PC rig's gamedir has the old 90/90/45 one; players have none yet). The
   reload module's kGuns points (well, insert, grab, port) and kWeapons' muzzle distances computed from the same model
   points through the same frame instead of the hand-set numbers.
4. One diagnostic build and run before the fix is final: log each weapon's bones as the game poses them while idle
   (60 or more frames after the equip, not reloading, not firing), SH3_MESH_DUMP=166,172,69,72,208 for the melee
   meshes, SH3_GIVE with every weapon (sh3.c:843 has the item numbers; the melee items in GAME_FACTS.md Weapons),
   SH3_EQUIP="1,2,3,8,4,5,6,7" SH3_EQUIP_EVERY=150, and pictures of each from the eyes and the side.
5. The check run after it: every weapon from the eyes (the gun along the red dot), from the side, two-handed, drawn
   from and put in its holster, swung (melee), and the reload points where the model's well, slide and port are.
   Look at every whole picture, and compare with the numbers.

Dead end, do not repeat: fitting the pose between the two bones by closing their stitched edges (Horn's fit) gave
wrong poses (the edges are real, a few cm long; it put the shotgun's stock in front of its muzzle).

### SECOND: the moving parts must not stretch faces (the owner: "clean and smooth")

- The submachine gun's knob and the shotgun's pump are whole pieces (no vertex shared with the rest): they move cleanly.
- The handgun's slide is cut by height (kSlideVerts, y >= 22, 127 vertices), but its sides are one piece with the
  frame and bone 1, so the faces across the cut stretch into spikes when it is back (vr44 c07450, the side view).
- The fix (designed): d3d8.c draws a gun whose part is moved as two draws of its own triangle lists (the game's strip
  turned into triangles, the winding kept): the frame's faces from the buffer at rest, then the part's faces (a face
  is the part's when 2 of its 3 vertices are in the part's set and all 3 on the part's bone) with all their vertices
  moved: glBufferSubData of that range, the draw, the rest range sent back. The seam opens as a real slide's does and
  nothing stretches. Guest memory stays untouched (vr_gun_part's saved slots go away). ROUTED draws twice (the eyes
  and the panel), so each call does both draws; dev->index_buf (the UP draws' element buffer) can carry the lists,
  32-bit indices. Strip type 5 and list type 4 only; anything else draws as the game's.
- Check in pictures from the side and the eyes: the slide back when empty, racked, the kick on each shot; the knob;
  the pump. No spikes, no holes other than the seam.
- "Meshes in general": also look again at her hands, fingers, wrists and arms in the pictures for skinning twists.

### THIRD: sound for every hand action (the owner: "proper audio, as all vr actions should")

Now only the game's reload sound plays, when a magazine seats or a shell goes in (the indices learned: handgun 3,
shotgun 5, submachine gun 7; sh3quest_reload.ini). To do: a sound for the magazine dropped, a magazine, shell or
battery taken from the pouch, seated, the slide pulled and let go, the knob, the pump back and forward, a holster draw
and put-away, the supplies pouch, the flashlight pocket, items taken by hand, a dry click. From the game's own sounds
where it has fitting ones: find its weapon and inventory sound numbers (how 0x4A33B0 and the reload motion event pick
them; GAME_FACTS.md Sound has 0x4A82B0), and whether its reload sounds are one clip (magazine and rack together), in
which case the rack needs another sound. Play them at the hand, through spatial sound.

### Then the rest of the list (order unchanged)

- 5 movies: the decryption is known (GAME_FACTS.md Movies; the result is an MPEG-1 system stream with MPEG-1 video
  and MPEG audio). No FFmpeg here. Found on this PC: C:/RACQuest/RatchetQuestVR/third_party/OpenRAC/port/media/mpeg2.h
  (in our Ratchet port's tree): read what it is and its license, and whether it decodes MPEG-1 video (most MPEG-2
  decoders do) and MPEG audio. Any download (pl_mpeg or another) needs the owner's yes first (name, source, size).
  Then the player hook (0x404B40 / 0x59C9C0), the big screen in VR, the sound through dsound.c's ds_track_write.
- 6 speed (uniform uploads by change, frame time), 12 inventory hit-testing (puzzle cursor 0x4DD130 / 0x4DD2F0),
  22 fingers on world objects, 23 the radio's static (check), 24 the extras' signatures, 29, 31, 38 (playthrough
  items, short runs only).
- The docs, this handoff's successor, memory; 40: the APK (tools/build_quest.py), its path and SHA-256 to the owner,
  publish nothing.

## Files

- New tools: tools/gun_extents.py (each bone's extents and the lowest and highest vertices of a mesh dump),
  tools/gun_pieces.py (a mesh's connected pieces and which bones each joins). tools/gun_parts.py is the part cutter.
- New sim scripts: tools/sim/cocking.txt (the knob from the side), tools/sim/sound.txt (spatial sound); reload.txt
  rewritten.
- Mesh dumps: C:/SH3Quest/gamedir/mesh_132.txt, mesh_290.txt, mesh_368.txt, mesh_557.txt (keep).
- Waiting for the owner's go before deleting (all on C:): shots vr38 92 MB, vr39 58, vr40 64, vr41 84, vr42 136,
  vr43 92, vr44 70, vr45 58, vr46 55, vr47 92, vr48 54 (its mix.f32 is 44 MB of it); logs vr38-vr48 and build logs
  38-45 in C:/SH3Quest. Earlier sessions' shots are listed in their handoffs.
