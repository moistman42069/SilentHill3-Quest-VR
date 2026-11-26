# SH3 Quest VR: handoff, session 11 (2026-11-25)

Read docs/HANDOFF-2026-11-25-SESSION10.md first (the state, the owner's messages and rules); this one carries on from
it. PC test builds up to 152 (build_pc_152.log, BUILD OK); runs up to vr180. Next build 153, next run vr181.

## Recovered: session 10's last stretch (after its handoff; it stopped on the usage limit)

- Build 138 (16:38Z): the first event scan in sh3.c's 0x4B9940 wrapper (SH3_EVENT_SCAN); d3d8_shader.c's dumped draw
  lines name their target ("into XXXXXXXX (WxH)"); d3d8.c logs each target change in SH3_VSC_DUMP's frame
  ("vsc target: ...").
- vr165 (scenes.txt past 5400 to the end of the opening, open item 1): DONE, the pictures whole. The park scene in her
  eyes 4194-6708, the game's camera when her face comes up close (1.2 m, 6708), first person again 7172, "first
  person from a scene into play: the rig anew" at 7280; 5800-7400 pictures whole (scratchpad sheet v165.png).
- vr166: the scan at 7330, no event (it only turned her facing at her own place); the draw dump logged nothing.
- Build 139: the scan as if A were down (mode 1), both passes and the floor items' check 0x4B74B0; the hand hook asks
  0x4B74B0 too. vr167 crashed: after the game's check returned, the hook put the stack pointer back to its entry
  value (the return address already popped): 4 bytes off. Build 140 fixed it (g_esp as the return left it).
- vr168 (exit 0, the dump fired: 585 lines), vr169 (events_walk.txt, a walking scan every 40 frames): no event,
  for the same reason as vr166.
- After build 140 (built in 141): d3d8.c logs each new stencil state a draw uses in SH3_VSC_DUMP's frame (LIST 56:
  does the game draw shadow volumes? render state 52 = stencil enable); session 10 was reading 0x48B5D0-0x48B640
  (the stencil setup near the flashlight caster) and 0x461380 (the character draw's stencil states).

## Done in session 11 (builds 141-143, runs vr170-vr172)

- **The game's events, read** (build 141, vr170): sh3.c event_table_log() logs the room's table once with the scan:
  [0x711FCA0] -> {events, regions}; events of 5 words until one all zero; 12 fields through 0x6CF6B0's
  (word, shift, mask) table: pass, type, flag A value/index, flag B value/index (bits of 0x715D060), c8, trigger
  (0 none: found anywhere when its flags pass; 1 the action button: mode 0 skips it; 2 by place; 3), region offset,
  d4 (1: wants request = value), value, dc. Regions: a shape byte, float x, (shape < 16: a short height), float z,
  then half floats. The start room (room 1): 138 events. tools: scratchpad evdecode.py.
- **The game's action test** (GAME_FACTS-worthy): 0x4B9840 by trigger and shape: polygons (0x4B8780) test the segment
  from her place (record +0x00/+0x08) along her facing vector (+0x30/+0x38, 500 units = 1.02 m) against each edge
  (0x4B76B0); shapes 1/16 (0x4B7600) test the front point (+0x20/+0x28); 13-15/28-30 use 0x4B78B0.
- **The scan fixed** (build 142, vr171): action_check_at() runs one check for any place, facing and ray length;
  event_scan stands her at 0-6 m all round, facing 8 ways: the start room's events 10 (1.5 m), 8, 20, 18.
- **The hands on the world** (builds 142-143, ask 28): hand_ray(): from her place toward the palm, to 20 cm past it
  (at least 0.5 m, never past 1.02 m). hand_reaching(): over 40 cm out from the eyes or under 55 cm over the floor
  (25 cm took idle hands, the sim's rest is 36 cm; "60 cm under the eyes" took a hand hanging at her side).
  hand_reach_point() + the per-frame check in the 0x4B9940 wrapper: a free hand reaching onto a button event or a
  floor item feels a light tick ("the right hand can use event N"); not the game's own events (trigger != 1:
  vr171 found the flag-driven event 136 anywhere); only after 0.5 s of play (play_time). hand_action_found plays
  kSoundTake. vr172 (events_hand.txt): the walk to Robbie the Rabbit, "can use event 10" at 7536, "used event 10",
  the game's "A rabbit doll... No, it's a costume." on the panel; the pictures whole.
- Docs: LIST 45's session 11 note.

## The VR UI (owner messages 42-45; builds 144-151, runs vr173-vr180; the run budget spent, ~28 min)

- **The UI pass** (sh3vr.cpp, the block after place_panel): the panel reaches the headset through ui_frame/ui_draw
  (a shader, not a blit): kinds HUD, screen, message, subtitle; fades in over 0.15 s and out over 0.2 s (a copy of
  the picture kept for the fade out); the cinema screen (movies, "Cutscenes" screen mode) and a missing shader keep
  the old blit. d3d8.c vr_route reports each 2D draw's box (sh3vr_panel_box) so a message shows only its text.
- **A message at its thing** (vr173, verified in the headset composite v07630-msg-headset-left): Robbie's "A rabbit
  doll... No, it's a costume." stands by him where the hand closed (0.87 m off, 0.30 x 0.23 m, the text as large to
  the eye as on the screen, a soft dark backing), steady over 7615-7680; with the button, 0.9 m ahead under the eyes.
  Subtitles low in the view (1.6 m, 0.42 under the eyes): vr180 v06200-subtitles-headset-left, "Where am I...?"
  under the middle of the view in the opening scene, readable.
- **Screens open in front of you**: a screen opens along the gaze at the screen's distance, stays world-fixed while
  the pointer works on it, glides round after half a second turned over 35 degrees away (vr177: at 50, a 40-degree
  turn left half of it out of view; vr178: at 35 it came round) (with "Screen follows the
  view"). Options: "Menus open in front of you", "Messages next to things" (View, on).
- **The inventory whole on the screen** (vr174: its backdrop and item models went into the eyes, stretched over the
  whole view; only its text was on the panel): build 145 routes a full screen (out of play, the last frame with no
  frame-camera 3D) whole to the panel and clears the eyes to black, shown opaque (vr175: the eyes dark, the text in
  front; the backdrop and models still missing from the panel). Build 146 adds SH3_ROUTE_DUMP=N (each draw of frames
  N to N+2: route, target, views, texture layers). The inventory draws its backdrop and models into a picture of
  its own and copies it to the back buffer with a pass over a per-eye texture, which always went to the eyes: build
  147 sends that pass to the panel too in a full screen (the shader samples the left eye into a single target).
  vr177: the inventory whole on the screen (items, frames, text), the eyes black round it.
- **The pointer clicks** (vr178-179): the game polls GetKeyState (0x600A67, keys at 0x721718, bit 7 of the low
  byte), not GetAsyncKeyState; the shim answered 0. Build 149: held = 0xFFFFFF80. vr179: the trigger on "Weapons"
  opened the weapons page (knife, pipe, handgun), on "Maps" selected it. B and Y are the game's buttons while
  pointing (vr178: B closes the inventory; as a right click it did not). tools/sim/inventory_ui.txt,
  inventory_click.txt, inventory_dump.txt.
- **Owner message 45** (session 11): the ghost hand only for a hand pushed into a surface, never for a controller
  past her arm's reach (vr173 showed one by Robbie). sh3vr_ik.cpp: ghost_bits only from hands[h].inside (built in
  147). vr180 v07630-msg-headset-left: by Robbie, past her reach, no ghost; her hand at the arm's end.

## The movies (LIST 5; owner message 46 "speed up"; regen 152, build 152 BUILD OK; no run: the budget was spent)

- The owner allowed the download of pl_mpeg.h (one file, MIT, github.com/phoboslab/pl_mpeg, SHA-256
  3a8cb30c83c2a1147719c30fe0c8b93da2987aa43140077c575b39aaa75fc2c9): third_party/pl_mpeg/pl_mpeg.h. Credited in
  CREDITS.md, NOTICE, the launcher's credits.txt (FFmpeg's placeholder lines gone).
- The scrambling, the stream and the game's player: GAME_FACTS.md, Movies (rewritten).
- src/game/movie.c (new): the key block's check, the table, the stream unscrambled as pl_mpeg reads it (64 KB at a
  time), pictures to RGBA, Layer II at 48 kHz resampled to the soundtrack's 44.1 kHz at the scene's volume
  (0x72C06C), sh3vr_movie(1/0) round it, "movie:" log lines (start, stop with pictures and seconds of sound).
  Scripted runs skip the movies unless SH3_MOVIES=1 (every existing sim script is timed without them).
- src/game/sh3.c: sub_00404B40 (start), sub_00404440 (step), sub_00403DC0 (stop) replaced whole, in
  recomp_lookup_manual; regen --recomp-only rerun (11974/11981, 0 failed; regen_152.log).
- src/d3d8/d3d8.c: movie_draw at Present (the picture over the frame, 4:3 kept, black round it, each eye's layer).
- src/vr/sh3vr.cpp/.h: sh3vr_movie; pick_view_mode gives the cinema screen while a movie plays.
- Checked off the game with a scratch harness (scratchpad movtest.c, the same key and stream code): op.000's first
  30 s = 900 pictures and 30.02 s of sound in step; huxx.000 to its end (540 pictures, 18 s); the picture at 30 s
  whole (Heather in the wooden room, colours right).
- Next: vr181 = tools/sim/movie.txt with SH3_MOVIES=1 (no QUEST_SIM_ALIGN): the opening movie on the cinema screen,
  pictures whole at 1200-2400, B skips at 2700, the opening goes on (about 2 minutes).
- Also checked this session: LIST 52's native left grips, LIST 53's weapon held back from walls, the slide drawn as
  its own faces (sh3vr_parts.cpp) and the forearm twist (0.8 of the hand's roll, sh3vr_ik.cpp) are all in the code;
  what they wait on is pictures (tools/sim/weapons.txt).

## Open, in order (written at the owner's message 47, "wrap up and do a handoff. forget nothing.")

0. Build 153 first: since build 152 only movie.c's first comment line changed (a nested "/*" warning fixed); no regen
   needed (sh3.c is unchanged since regen 152). Never build while sh3.exe runs.
1. vr181, the movies in the game (about 2 minutes; the next session's run budget starts fresh, about 30 minutes):
   mkdir -p /c/SH3Quest/shots/vr181; cd /c/SH3Quest/gamedir; QUEST_SIM_HEADSET=1 QUEST_SIM_GRIP_TILT=50 SH3_MOVIES=1
   SH3_KEYS="22:Return,25:Return,28:Return,31:Return,34:Return" QUEST_SIM_SCRIPT=C:/SH3Quest/SH3QuestVR/tools/sim/movie.txt
   QUEST_SIM_OUT=C:/SH3Quest/shots/vr181 timeout 300 /c/SH3Quest/SH3QuestVR/build/pc/sh3.exe C:/SH3Quest/gamedir
   > /c/SH3Quest/vr181.log 2>&1   (no QUEST_SIM_ALIGN: the script counts frames from the start).
   Check: the log's "movie: op.000: 512x384 at 29.97 fps ..." and "movie: pictures 512x384 drawn into the WxH frame",
   "VR: a movie plays: shown on the cinema screen"; m01200-m02400 the movie whole on the cinema screen (4:3, black
   round it, no tearing; m02060 with the head turned 30 degrees: the screen stays put); at 2700 B: "movie: op.000
   stopped (the game's movie scene ended it ...) after N pictures, S s of sound" with S about N / 29.97, and the
   opening goes on (m02760, m03000). If B does not skip, the game's skip (0x4DD4A0(2)) is another button: try y
   (Start) in movie.txt. If no picture: movie_draw runs at Present (d3d8.c); if the movie never starts: "movie: ...
   skipped" lines say why (the path from rt_path, the key block, the headers).
2. LIST 56, her head and gestures in her shadow: GAME_FACTS "Shadows" (session 11) has the map's pass (0x685D30 ->
   0x4913C0 -> 0x491030 mode 1; its caster list 0x687F30 is the room's geometry by area) and the leads for her own
   shadow. Planned and NOT written yet (do it first, it makes one run decide it): in d3d8.c dev_SetRenderState, in
   SH3_VSC_DUMP's frame, when state 52 (STENCILENABLE) goes to 1, log MEM32(g_esp) (the game code that turned it
   on: which lead runs); in sh3.c's 0x45ED40 wrapper (her part draw), log whether dev->rs[52] is on while her parts
   draw (a small d3d8.c accessor, e.g. d3d8_stencil_on()). Then tools/sim/shadow.txt with SH3_VSC_DUMP at a frame
   where a room light throws her shadow on a wall: is her shadow drawn from her parts (then from the bones 0x45EC00
   leaves: LIST 19's IK already in it) or from another pose? Her hand in her own flashlight's beam casts none (her
   draws are not in that pass, session 9).
3. tools/sim/weapons.txt again (the pictures whole): LIST 52 grips (the shotgun's pump hand, the submachine gun's
   left, the pipe's and maul's left: the log's "VR native grip: ..., left hand" lines say whether the game's own left
   hand was read; else the finger hand round second_at), thumbs and fingers not through the weapon; LIST 53 a weapon
   held back from a wall (wall_back, "held at a surface" lines), the Alyx finger feel on weapons; the handgun's slide
   moving as its own faces (sh3vr_parts.cpp); the forearm twist (0.8 of the hand's roll, sh3vr_ik.cpp). All of these
   are written; they wait only on pictures.
4. World items: a door (the start alley's doors: events 18/20/8? check with the scan's spots), a floor item (none in
   the start room), docs/PUZZLES.md's list of spot kinds.
5. LIST 12, the rest of the screens with the pointer: the map, memos, options, the save screen, puzzles. First look
   at vr179's c07510-maps-clicked picture (Maps was clicked in that run too). The game's mouse counter 0x70E6410 is
   only touched inside its input module (0x600912-0x600C0D), so a run is the check, not more disassembly.
6. LIST 6 speed on the Quest, 22-24, 29, 31, 38 (the playthrough, HEADSET_CHECKLIST.md).
7. The APK, last: only once everything is tested and refined; its path and SHA-256 to the owner; publish nothing.

Waiting for the owner's go (files on C:, never deleted without it): C:/SH3Quest/dl_plmpeg (pl_mpeg.h as downloaded;
the copy in use is third_party/pl_mpeg), the shots folders C:/SH3Quest/shots/vr* and the vr*.log, build_pc_*.log and
regen_*.log files from sessions 1-11.

## Files changed in session 11

- src/game/sh3.c: event_table_log, event_trigger, action_check_at, hand_ray, event_scan (rewritten), the 0x4B9940
  wrapper (per-frame hand check, the hand's ray).
- src/vr/sh3vr_combat.cpp: play_time, hand_reaching, hand_reach_point, hand_hover, update_grabbing uses
  hand_reaching, hand_action_found's sound. src/vr/sh3vr_internal.h, sh3vr.h, sh3vr.cpp: their declarations and glue.
- tools/sim/events_hand.txt (new). docs/LIST-0.1.0.md (45), this handoff.
- The VR UI: src/vr/sh3vr.cpp (the UI block after place_panel: ui_gl, ui_place_frame, ui_start, ui_place_label,
  ui_draw, ui_frame; sh3vr_present through it; the options; sh3vr_full_screens, sh3vr_frame_world, sh3vr_panel_box;
  B and Y the game's while pointing), sh3vr.h, sh3vr_body.cpp (gl_link_program), sh3vr_internal.h (options);
  src/d3d8/d3d8.c (vr_route: the text's box, full screens whole to the panel, per-eye passes too; SH3_ROUTE_DUMP;
  the present's world bookkeeping); src/game/sh3.c (sh3_playable_now); src/win32/user32.c (GetKeyState: the mouse
  buttons); src/vr/sh3vr_ik.cpp (owner 45: the ghost only in a surface). android Options.java (two View options),
  assets/whats_new.txt; docs CONTROLS.md, HEADSET_CHECKLIST.md, GAME_FACTS.md (Shadows; the mouse buttons), LIST 11/12.
  tools/sim/inventory_ui.txt, inventory_click.txt, inventory_dump.txt (new).
- The movies: src/game/movie.c (new), third_party/pl_mpeg/pl_mpeg.h (new), src/game/sh3.c (three replacements and
  their lookup cases), src/d3d8/d3d8.c (movie_draw), src/vr/sh3vr.cpp/.h (sh3vr_movie), src/recomp/gen (regen 152),
  tools/sim/movie.txt (new); CREDITS.md, NOTICE, android assets credits.txt and whats_new.txt; docs GAME_FACTS.md
  (Movies), LIST-0.1.0.md (5, 39), HEADSET_CHECKLIST.md (the movies); tools/movie_check.c and .bat (new: the
  movies decoded off the game; muxx.000 3 s = 91 pictures, 3.02 s of sound).
- C:/SH3Quest/notes/owner-messages-sessions1-7.txt: messages 45-47 and the pl_mpeg permission.

## The owner's rules

SESSION10.md's last section, plus messages 41-46, in short:
- Never stop on my own (41): work down this list until usage runs out; only a question that blocks everything left
  ends a turn. Speed (46): code every open item properly in batches, one build, one or two combined short runs; no
  research or runs that are not strictly needed (memory feedback-speed-up).
- PC test builds allowed, one at a time, never while sh3.exe runs. Runs 2-3.5 min each, about 30 min a session; say
  the time first; when spent, keep coding and writing docs.
- No `python -` heredocs or stdin python (scripts in files, C:/SH3Quest/venv/Scripts/python.exe -I); edit with the
  Edit tool (Read first) or plain sed; no commit, push or publish unless asked; downloads need the owner's permission
  each time (pl_mpeg.h was allowed once, for the movies); files on C: deleted only on the owner's go.
- Plain option names, every VR option on by default (film grain off); never box hands; the ghost hand only for a hand
  pushed into a surface (45); HUD per view (first person the watch only, third person the game's normal HUD); a
  sound for every VR action; crash logs that explain themselves; think VR comfort through unasked.
- Check every picture whole, backed with numbers; verify game claims in code and a picture before stating them.
- The APK only once everything is tested and refined: its path and SHA-256 to the owner, published nowhere.
- Never raise the license question; credit every author (CREDITS.md, NOTICE, the launcher's credits.txt).
- Each closing message shows the whole list with states; write the handoff and the memory as the state changes.
