# Silent Hill 3 PC (sh3.exe 2003-09-03): what is known, by address

Addresses are virtual (image base 0x400000). Sources: sh3-nx (src/game/sh3.c, tools/patch_exe.py), the PC VR mod
(C:\SH3Quest\refs\Silent-Hill-3-VR-Mod, sh3vr_proxy/d3d9_hook.cpp and bootstrap_stub.cpp), our own checks.
Mark what we confirm ourselves with (checked: how).

## Player
- 0x0712C100: pointer to the player's work struct (Heather). The VR mod accepts it when +0x80 (u16) == 0x0100.
  - +0x00: position (float x, y, z, w). +0xD0: previous position (the setter writes both).
  - +0x14: yaw (float, radians). +0xE4: the simulation's previous/working yaw (the VR mod writes it before movement).
  - +0xF0 / +0xF8: movement vector x / z before slope and collision (per frame; x500 at integration).
  - +0x11C: walk/run factor fed to gait and footsteps.
  - +0x180 / +0x184: current / max HP (floats; the game's health getter, healing and life bar use them).
- 0x0712C656: the weapon drawn (u8), the same numbers as the equipped slot (see Weapons). The PC VR mod's comment
  "0 handgun, 1 shotgun, 2 submachine gun, 3 stun gun, 4-7 melee" is wrong for the guns (checked 2026-11-24, below);
  its melee 4-7 holds.
- 0x004737F0: void SetCharacterYaw(void *chr, float yaw) (cdecl).
- 0x004736C0: void SetCharacterPosition(void *chr, const float pos[4]) (cdecl): current and previous position.
- 0x004D4018 / 0x004EAE91: return addresses of the normal / aiming locomotion callers (the VR mod's movement hook).
- 0x070E67AC: the frame-time value the movement formula uses (frame time x 1.2 walking factor).
- 0x008987A4: pointer to the player model; 0x00C0E060 the default player model (OTSMod's "hide player" nulls it).

## Weapons (checked 2026-11-24, session 3: disassembly with tools/disx.py, tools/xrefs.py, tools/peek.py)
- Weapon numbers (the drawn weapon 0x0712C656 and the equipped slot 0x0712CABF): 0 none, 1 handgun, 2 shotgun,
  3 submachine gun, 4 knife, 5 steel pipe, 6 katana, 7 maul, 8 stun gun, 9 / 10 / 13 / 14 / 17 the extras, 11 / 12
  unused, 15 / 16 / 18 the handgun, the submachine gun and 17 in their other mode (0x0712CABC bit 2).
  How: 0x004711D0 sets the drawn weapon with 0x00495F80(low byte of 0x00497980()), and 0x00497980 returns
  short[0x6BDDB0 + 2 slot] = 0x800 + slot (0x801 / 0x803 / 0x811 become 0x80F / 0x810 / 0x812 with that bit).
  0x004978F0(slot) loads ammo for slots 1, 2, 3, 8 (ammo items 0x0F, 0x10, 0x11, 0x0E: short[0x6BDD68 + 2 n]).
  The names of 1-8 follow the ammo (handgun bullets, shells, submachine gun rounds, batteries) and the PC VR mod's
  melee names; which extra is which (9, 10, 13, 14, 17) is not yet known.
- short[0x006BDD24 + 2 slot]: the slot's inventory item (0 for none, 11, 12). The player has item i when bit i of
  the dwords at 0x0712CA80 is set (0x00497730). Items 9-0x15 keep a count at short[0x0712CA8E + 2 i].
- short[0x006BDD4C + 2 i], i 0..13: the inventory's i-th weapon's slot (0, 4, 5, 7, 6, 9, 10, 13, 14, 8, 1, 2, 3, 17):
  item i for i 1..13.
- 0x00497A70(int item) (cdecl): equips item `item` (its slot from the table above, the other mode picked by
  0x0712CABC bit 2) through 0x004978F0, which sets 0x0712CABF and the ammo. Called by the inventory and by events
  (0x0057FB7B: 0x00497A70(0), nothing equipped).
- 0x004711D0 (no arguments): applies the equipped slot: frees the loaded weapon models (0x800-0x87F), loads the new
  one (0x0046FD40 with 0x800 + slot, and 0x880 + slot when 0x00464A20(0)), sets the drawn weapon (0x00495F80). The
  game calls it once, from the return-to-play loader 0x005E1E20 (GameMain 0x0058EEE8).
- (checked 2026-11-24, session 3 part 3, PC rig runs 33-37) Changing weapons in play. 0x004711D0 alone crashed: the
  new part's model pointer (object +0x2CC) stays 0 when its model file is not loaded (0x0046AC70: 0x00682EB0(type)
  finds none), and the frame's copy 0x004546A0 (from 0x00685F90) read guest address 0. 0x00497A70 alone only sets the
  asked weapon (0x0712CACE); her drawn weapon stays. The inventory's closing (0x005E1E20) first loads the files the
  game now needs with the stage's file step 0x0059E3A0 (five steps, one a frame; answers 1 when done), then calls
  0x004711D0. Done that way in play (sh3.c vr_equip / swap_step): the weapon in hand put away (0x00497A70(0) +
  0x004711D0), the new one asked, 0x0059E3A0 run each frame, then 0x004711D0: the handgun out after 5 frames, no
  crash (run 37).
- The game's step stack (0x005F1B50 push, 0x005F1B60 pop, 0x005F1BC0 read, 0x005F1B70 set): depth u32 0x00BCC658, the
  steps u32[16] at 0x00BCC65C (a level's step kept from frame to frame), u32[16] at 0x00BCC69C; setting a step clears
  the levels under it. A state function called outside the game's own (0x0059E3A0 from VR) runs on a copy (sh3.c
  swap_run_loader swaps the 33 dwords in and out), or it would overwrite the game's steps.
- Ammo: each gun's count (short[0x0712CA8E + 2 item], items 9 stun gun, 10 handgun, 11 shotgun, 12 submachine gun,
  13 an extra) is its LOADED rounds; its ammo item is the gun's + 5 (14 batteries, 15 handgun bullets, 16 shells,
  17 submachine gun rounds), whose count is the reserve. Capacity byte[0x006BDD17 + gun item]: stun gun 4, handgun
  10, shotgun 6, submachine gun 32, 13: 0. 0x00497780: the current gun's reserve (items 9-12 only, else 0).
  0x00497730: the drawn gun's loaded count. u16 0x0712C644: the same figure, set every frame at 0x004B515E from
  0x00497730() (the reload event briefly writes 0x00497780() there); 0x004978F0 (the equip ammo loader) leaves it.
- The empty-gun guards 0x004B4340 and 0x004E79A0 act only when 0x00497780() == 0 and 0x0712C644 == 0 (0x004B4340
  leaves out weapon 8, the stun gun). 0x004ECC50 calls 0x00494C40(weapon, 0x0712C650, state, word[0x0712C644]).
- (Session 4, LIST 46) sh3.c wraps 0x00497780: while the hands reload by hand an empty gun answers 0 (no reserve), so
  the game's own reload never starts; and 0x00497800: a gun at 0 loaded fires nothing and stays at 0 (0x00496A20 has
  no floor: a dry fire let through wrapped 0 to 65535, run vr40).
- The reload sounds (the reload motion event's index, learned on the rig, gamedir/sh3quest_reload.ini): handgun
  (item 10) 3, shotgun (11) 5, submachine gun (12) 7.
- The reload: 0x00497860(item, 0 = the current gun) is the transaction (loaded back to the reserve, then loaded =
  min(capacity, pool), the reserve keeps the rest). Firing: 0x00497800 (0x00496A20 takes one; items 9-0x15 keep their
  bit at 0). In play the reload is an animation event, 0x004B5550: in its motion window, with the flag
  [edx + eax + 0x168] 0, it calls 0x00497860(0x00494BA0(weapon)), sets 0x0712C644 = 0x00497780(), plays the sound
  0x005680F0(chr, chr + 0x60, 0, the event's index [esi + 6]) and sets the flag to 2. From the inventory: 0x004979D0
  through 0x00497CA0 (at once).
- 0x00494C40(weapon, ?, state, loaded) (cdecl, only from 0x004ECC50): Heather's attack decision. Seen on the rig (run
  37, the handgun given with 0 loaded and 20 in reserve, the trigger held): 3 (the game's reload) at 0 loaded, then 1
  with 10 loaded (the reserve 10). (checked 2026-11-24, session 4: disassembly) A class per weapon from the bytes at
  0x00494CD8 (weapon 0..0x12: 0 1 2 2 3 4 4 3 5 4 2 2 6 4 4 1 2 2 2), then a jump table at 0x00494CBC:
  - class 1 (handgun 1, 15): loaded > 0, or no reserve (0x00497780() == 0): 1, or 2 when byte 0x0712C65F == 1; loaded
    0 with a reserve: 3 (the reload).
  - class 2 (shotgun 2, submachine gun 3, extras 10, 11, 16, 17, 18): loaded > 0 or no reserve: 2; loaded 0 with a
    reserve: 3.
  - class 3 (knife 4, maul 7): state 1: 4, 2: 5, else 0. Class 4 (pipe 5, katana 6, extras 9, 13, 14): state 1: 4,
    2: 5, 3: 6, else 0. Class 5 (stun gun 8): always 4 (it never reloads in play). Class 0 (none): state 2: 5, else 4.
  So "no reload" is the answer the function gives with no reserve: SH3 Quest VR's "Manual reload" answers that.
- 0x00496A20(item) takes one round: for items 9-0x15 it decrements short[0x0712CA8E + 2 item] WITHOUT a floor (0
  becomes 0xFFFF) and answers 1, or 0 when it reached 0; other items lose their owned bit. 0x00497800 (fire, item 0 =
  the current gun) calls it for items 9-0x0C; item 0x0D (weapon 17) is never decremented (an unlimited extra).
- The reload event 0x004B5550(event record) (cdecl; esi = the record: [+0] the character, [+6] u16 the event's index,
  [+8] an offset to the event's done flag at [chr + offset + 0x168]): when the character's motion frame (0x00474770(chr,
  1)) is inside the window bytes [0x006CE184 + 24 index] .. [0x006CE185 + 24 index] and the flag is 0: 0x00497860(
  0x00494BA0(weapon drawn)), 0x0712C644 = 0x00497780(), the sound 0x005680F0(chr, chr + 0x60, 0, index), flag = 2. The
  reload sound is the event's own (its index).
- The handgun's draw (run 37, SH3_WEAPON_DUMP=368, vertex shader F000002D, 368 vertices, 610 primitives, stride 48):
  +0 position, +12 one weight (1.0), +24 the bone index bytes, two values (0 and 1): the gun has TWO parts on their
  own bones; +28 normal, +40 texture coordinates.
- The guns' meshes (session 4, SH3_MESH_DUMP=368,557,290,132 writes mesh_N.txt; tools/mesh_parts.py,
  tools/gun_parts.py): model units 490 a metre. Every gun has bone 0 (the body) and bone 1 (a small piece at the
  hand); none has a slide, magazine, knob or pump of its own. Faces are flat-shaded in small connected pieces.
  - Handgun 368 / 610: the muzzle along +x, the grip at x 21-52; the slide is everything of bone 0 at y 22 and over
    (127 vertices, the hammer with it; a few faces across the cut stretch at full travel).
  - Submachine gun 557 / 890: an Uzi's layout, the magazine up through the grip (x 31-54), the trigger guard ahead of
    it, the muzzle along +x; the split cocking knob on top at the rear, x 0-12.7, y 55-69.5 (88 vertices of bone 0).
  - Shotgun 290 / 490: bone 1 is the barrel assembly with its muzzle along -x; the pump is the wide block at x -129 ..
    -13 (35 vertices).
  - Stun gun 132 / 226.
  SH3 Quest VR moves those vertices in the vertex buffer for the cocking visual (d3d8.c vr_gun_part, sh3vr_reload.cpp
  kMovingParts).

## Combat
- 0x004ACE10: clBattleAddQue(const BattleCollision *) (cdecl): melee hit volumes (kind 2 or 3 = melee) with
  attacker pointer, battle id, previous/current grip and tip points (float[4] each, w = 1). BattleCollision is
  0x48 bytes: u16 kind, u16 battle id, u32 attacker, previous grip +0x08, previous tip +0x18, current grip +0x28,
  current tip +0x38 (the PC VR mod's static_assert). (checked 2026-11-24:) it copies the struct into the queue at
  0x071267A0 + 0x48 n, n = u32 0x07122438, with no bounds check; 0x004ACA80 resets the count (GameMain calls it).
- 0x004B33E0: the battle step (GameMain 0x0058F368): handles each queued entry by kind (2-15, a jump table at
  0x004B39D0) and then 0x004AD040. SH3 Quest VR queues its punches and club hits at its start.
- 0x004B3D50: build_attack_origin_direction(void *chr, u16 attack_id, float origin[4], float dir[4]) (cdecl):
  called for every attacking character; for Heather's firearms the VR mod keeps the native origin (the weapon socket,
  inside the battle collision space) and points dir through the far end of the controller's aim ray.
- (Session 9, LIST 50) The monsters' hits on Heather: her damage step 0x004EC800(chr) first calls 0x004B6A20(chr)
  (cdecl), which walks the battle entries hitting her (0x004ACE40(chr, previous entry) gives the next; entry +8 its
  kind, +0x0C its point, +0x1C its way, +0x2C, +0x2E the reaction, +0x34 the attacker) and for a damaging one adds
  0x004B53D0's damage to chr+0x15C (float, the pending damage), sets +0x160, copies the point to +0x134 and the
  normalized way (0x0062A5F0) to +0x144, the reaction to +0x154 (word), the kind to +0x16C (word), counts +0x16E and
  sets the attacker +0x17C (a character, its place at +0). 0x004EC800 then, with +0x16C set and 0x0712C666,
  0x0712C662 (her health reached 0) and 0x0712C663 clear, takes +0x15C from her health +0x180 (0x004EC95B) and adds it
  to +0x170. Heather's own hits on monsters go through 0x004B6810 (the attacker [0x0712C100]). Health +0x180, its most
  +0x184. SH3 Quest VR wraps 0x004B6A20 (sh3vr_block: a guarding weapon or arm scales what the call added).
- (Session 16, checked in vr233) 0x004B6810(chr) (cdecl) gathers Heather's hits on a monster the same way (records
  of kind 4 hitting chr: +0x15C damage, +0x160 stun, +0x134 point, +0x144 way, +0x154 reaction, +0x17C her) and,
  for some reactions, calls 0x46E8E0 / 0x4DBDF0. Its damage, 0x004B53D0 (ecx the record, eax/esi the outputs): the
  attack's own damage and stun from the table at 0x6CE174 / 0x6CE178 (24 bytes an attack, by the id byte at the
  record's +0x2E), times the attacker's +0x18C, times the target's +0x190, then times [0x68A8F0] once for each
  earlier hit this frame (+0x16E). The knife's attack 14: 30 damage, 5 stun. SH3 Quest VR wraps 0x004B6810 (the
  hit logged, a fist's halved). The circular hall's monster in the opening nightmare is a dog (25 bones), health
  725.2 in vr233 (712.5 and 696.5 in other runs).
- A manual wrapper in sh3.c that returns WITHOUT calling its _gen must do the ret's own pop (g_esp += 4, plus the
  arguments for a stdcall ret N): vr167 and vr231 crashed four bytes off ("indirect call to non-code address
  00000000" from the message pump at 0x40367E, after the game saved disp.ini).
- (Session 9, LIST 49) The Life Display (Extra options): bit 0x80 of byte 0x070E66C6 (0x0060CB90 sets or clears it,
  0x0060CBC0 reads it; 0x0060CBB0 the other 7 bits). 0x004E7C50 (0x004E8506-0x004E855D): with it on and [0x0712C662]
  == 0, her health under 10, 25 or 50 percent (floats 0x0069005C, 0x0069060C, 0x0068EFE0) calls 0x005F1C30(P),
  storing P (10, 25 or 50) in [0x00BCC6DC]. 0x005F1C40 (from 0x005F1940 at 0x005F1AF2) draws a red mark at the
  screen's right: alpha (sin(timeGetTime / P * pi/20 [0x006988A4]) + 1) * 127.5 ([0x0068A754] = 1, [0x0068F128] =
  127.5), one beat each 2 s, 1 s, 0.4 s; DrawIndexedPrimitiveUP of 12 vertices, 10 triangles (indices 0x0071FA40,
  vertices 0x0071FAE0, stride 0x14), colour 0x??FF0000; then clears [0x00BCC6DC]. The heartbeat rumbles 0x006D6B70,
  0x006D6BD8, 0x006D6C40 go through 0x005FED50 (while bit 0x20000000 of [0x0715D0CC] is clear). (Session 9 wrote
  here "SH3 has no stamina": wrong, see the Life Gauge and stamina below.)
- (Session 9, owner message 35: "two different meter bars"?) The mark is a red "+" cross, not a bar: its 12 corners
  come from 0x0071FA84 (screen fractions: the upright x 0.9345-0.9583, y 0.906-0.969; the arms x 0.9107-0.9821, y
  0.927-0.948), its indices at 0x0071FA40 a strip of 8 for the upright and two arms. The game's code reads her health
  over its most (fdiv of +0x180 by +0x184) in only four places: the healing item (0x0046E850), her damage step
  (0x004EC800), 0x004A3300 ((1 - health share)^2 times a weight, capped; called about 30 times from her motion code
  0x004F80CF-0x004FAEE9: how hurt she moves) and 0x005475C0 (a colour from the share squared: 255 - 150, 95, 65
  times it, through 0x00438ED0 / 0x00438F90 / 0x00439420 with a character's +0x80 / +0x82; not a screen meter).
  Session 9 concluded from this "one health indicator, no meter bar": WRONG (owner message 37, "silent hill 3 has a
  health and stamina bar"). The bars' draw reads +0x180 / +0x184 through its own math, not that fdiv pattern.
- (Session 10, checked: disassembly and run vr152's pictures) The game's Life Gauge: 0x005A5E40 (cdecl, 1 argument)
  draws two bars at the screen's top left, her health (+0x180 over +0x184, yellow) and her stamina (the most
  0x0712C624 less the tiredness 0x0712C620, green), a mark from 0x0712C640 (180.0 right after a heal) and a thin line
  from 0x0712C608 (the time to get free, below). It is called from 0x005A6559 in the task handler 0x005A6520, while
  [0x00BCB08C] is set. 0x005A6450 turns it on ([0x00BCB08C] = 1, task 0x3D); 0x005A64A0 turns it off unless byte
  0x070E6724 == 1 (getter 0x0060CE60), bit 6 of 0x070E66A0 is clear (0x0060B330(6)) and bit 2 is set
  (0x0060B330(2)). Each frame 0x0058F0DD calls 0x0060B350, which clears bits 2-6 of 0x070E66A0; bit 2 is set again by
  0x006859D0 (push 2; call 0x0060B360), from many of the play loop's mode cases (0x0058EEAD, 0x0058F3A6, ...).
  Byte 0x070E6724 comes from the options menu's copy 0x00BCC1B2 (0x0060CE50, called at 0x005E5D2A); its menu line's
  text is drawn by 0x005E2D50 (On / Off ids 0x27 / 0x28; changed-flag 0x80000 in 0x00BCC1DC). The play set-up
  0x004321E4 also reads it (0x0041B800(3) when set, else 7). Which of the Extra options names it carries is not yet
  pinned (msg.arc's Extra options texts include "Life Display / Displays Life Gauge", "Noise Effect", "Bullet Adjust",
  "Blood Volume", "Beginner Mode"). Run vr152 (byte held at 1, health 40%, stamina 60%): the yellow and green bars
  drew at the top left of both eyes, stamped into each eye's picture. 0x005A5E40 draws nothing itself: its rectangles
  (0x00474D70, 0x00474EA0) write packets into the game's command buffer (0x00678900 begins one, 0x00678930 closes it),
  drawn later, so they cannot be told apart at the draw (vr155). SH3 Quest VR wraps 0x005A5E40: skipped while the
  eyes show the world (first or third person; VR draws the same bars, on her watch or at the view's top left), kept on
  the cinema screen.
- (Session 10, checked: disassembly; vr152) Stamina: the tiredness 0x0712C620 (float) grows as she moves
  (0x004B3F60: her speed squared times the frame time, weighted by the movement state 0x0712C190), up to its most
  0x0712C624 (2000.0 in vr152 and vr155), and falls at rest. Past 30% of the most she breathes hard (0x004D4DB1). Healing clears
  it. The stamina shown is 1 - tiredness / most. VR's wrist shows it as the green bar under the health bar. vr155
  (running in a circle, movement state 0x0712C190 = 0x0E, 0x0A walking): it grew about 100 a second running, to 1084
  of 2000 in 17 s (46% stamina left), and fell about 150 a second at rest (state 0).
- (Session 10, checked: disassembly; runs vr153, vr154) Getting free when knocked down or held: 0x004D44E0 (no arguments) counts this
  frame's press edges, 0x005FE910(0, 1, mask) for the masks 0x200, 0x400, 8, 0x10, 0x20, 0x40, 0x100 (and more) and
  0x00472050(2, 4, 6, 8) for the four directions. Each frame the time to get free 0x0712C608 (float) loses
  [0x0712C108] x 0.2 plus presses x [0x0712C108]; at 0 or under it is set to 0 and the call returns 1 (free). It is
  called at 0x004D9FE6 inside 0x004D97A0, her reaction state machine on byte 0x0712C665 (jump table 0x004DABA8). The
  reaction kinds 5, 9 and 12 start the timer through 0x004DAE70: a base of 0.5 / 1.0 / 1.2 / 2 + 0.2 x level by the
  action level (0x0060C370), times a factor per hold animation. 0x0712C665 is set at 0x004C721B from 0x004B47E0 (the
  reaction by animation ids). SH3 Quest VR wraps 0x004D44E0: each controller shake counts as one more press
  ("Shake to break free", on).
- (Session 9, checked: run vr149) Her damage as applied is 1.5 times the pending sum (health 100 -> 85 guarded by the
  gun, 85 -> 55 unguarded, 55 -> 37 and 37 -> 19 guarded by the forearm): the guard keeps 0.5 (a gun) or 0.6 (the
  forearm) of the damage.

## Movies (session 2 found them; session 11 solved them: LIST 5, build 152)
- data/movie/NAME.000: op (the opening, 97 MB, about 2.6 min), ed_b, ed_n, ed_u (the endings), henshin, huxx, muxx
  (11-165 MB). Scrambled with a key of their own (session 11, the game's 0x00405680 / 0x00405640 / 0x00405530):
  - the first 0x8000 bytes are a key block. seed = b[0x55E2] << 24 | b[0x2A5C] << 16 | b[0x3D40] << 8 | b[0x2DDE];
    x = x * 0x5D588B65 + 1 per 32-bit word, XORed over the block, leaves it all zero apart from those 4 bytes (the
    game's check that the key is right);
  - the next 0x400 values of x (little-endian words) are a 0x1000-byte table; the data from 0x8000 on is XORed with
    table[(offset - 0x8000) & 0xFFF].
  - What comes out: an MPEG-1 program stream (pack header 0x21, a system header 0xBB), MPEG-1 video 512x384 at 29.97 fps
    (pixel aspect 1; no MPEG-2 extensions) on stream E0, MPEG-1 Layer II sound 48 kHz stereo on stream C0. Checked off
    the game (session 11): op.000's first 30 s gave 900 pictures and 30.02 s of sound in step; huxx.000 ends after
    540 pictures, 18 s; the picture at 30 s whole (Heather in the wooden room).
- 0x0059C9C0(int index) (cdecl; callers 0x004BAFF3 in 0x004BAFD0, 0x005D9645 in 0x005D9470): the movie's name from
  the table at 0x00710C48 (8 bytes an entry, the name pointer first), logs "movie: %s" (0x006954B4), sets 0x00BC93B0
  (the index) / 0x00BC93B4 (the scene's step), sets the volume with 0x00403E00(float: the music level byte 0x070E66C8
  x 1/15; it waits at 0x0072C06C and goes to IBasicAudio as hundredths of dB), and starts it with 0x00404B40(path from
  0x005F1200(2, name)).
- The movie scene (0x0059C860, each frame): 0x00403E70 (the picture's place), then 0x00404440 (the player's step: it
  drew the texture) between two 0x00416B70 (the 2D size), and the state 0x00403DF0 (the byte 0x0072C064: 3 playing,
  2 at the end, 1 stopped); not 3, or 0x004DD4A0(2) (a button: skip), and it calls 0x00403DC0 (stop) and goes on.
- The game's player (0x00403C80-0x00404C13): DirectShow. 0x00404B40 needs 0x0072C0B7 (DirectShow came up, else it
  returns 1: no movie), builds the graph in 0x00404780 (CoCreateInstance CLSID_FilterGraph / IID_IGraphBuilder at
  0x0072C050, its own source filter 0x00405810 (0x1034 bytes, the unscrambling) and renderer filter 0x00403FB0
  (0x1E8 bytes, a CBaseVideoRenderer copying frames into two textures at 0x0072C09C), IMediaControl 0x0072C054 (Run
  +0x1C, Stop +0x24), IMediaEventEx 0x0072C058 (SetNotifyWindow +0x34, message 0x40D), IBasicAudio 0x0072C05C
  (put_Volume +0x1C)), makes the textures in 0x004040E0 and runs it. sh3-nx answers CoCreateInstance "no such class",
  so the game skipped every movie.
- The port (session 11, build 152): src/game/sh3.c replaces 0x00404B40 (start: src/game/movie.c opens the file,
  checks the key block, unscrambles as it reads, decodes with pl_mpeg; 0 and the state 3 when it plays), 0x00404440
  (the step: the movie moves on by the time since the last frame, at most 0.1 s; its end sets the state 2) and
  0x00403DC0 (stop: skipped or over). The picture is drawn over the frame at Present (d3d8.c movie_draw, its 4:3 kept
  with black round it, into each eye's layer), VR shows the frame on the cinema screen while it plays (sh3vr_movie),
  the sound goes to dsound.c's movie soundtrack (resampled to 44.1 kHz) at the scene's volume. Scripted runs
  (QUEST_SIM_SCRIPT) skip the movies unless SH3_MOVIES=1 (tools/sim/movie.txt).

## Events, demos, game state
- 0x00898308: demo status (u32): 0 idle, 1 loading, 2 running (getter 0x004670B0). 0x00898310: demo descriptor.
- 0x00467530: demo start (cdecl, arg = descriptor); 0x00467130: demo finish.
- 0x004B8B70: event update; 0x004B9E70: event dispatch, returns the next game mode; GameMain's callers at
  0x0058F0E7 / 0x0058F547 take 5 as "playable update" (others: loading, menus...).
- 0x0081FAB5 (u8): set during cutscenes (sh3-nx's widescreen keeps the 4:3 width then).
- 0x00897E38 (u8) screen fade type (5, 9, 13 = fade-ins), 0x00897E3C (u8) fade done (sh3-nx's pause-menu fix).
- 0x00495B60: int can_do(what) (0x0D save, 0x0E load allowed).
- The action check (checked 2026-11-24, session 3; LIST 45): 0x004B9D60(mode, request, kind) (cdecl; from the event
  dispatch 0x004B9E70 at 0x004BA05D with (request == 0, [0x00BA9608], 0), and from 0x00496F90 with (0, x, 1)) reads
  Heather's position (0x00472FE0) and turn (0x00473000, the yaw second) into a 0x3C-byte record on its stack: +0x00
  position, +0x10 turn (+0x14 the yaw), +0x20 / +0x28 the point 150 units (31 cm) in front of her (x / z), +0x30 /
  +0x38 her facing x 500 (x / z). It then calls 0x004B9940(mode, room 0x00496B90(), request, kind, pass, record)
  (cdecl) twice (pass 0, then 1; 0x004B74B0(record) between when the first found nothing), which walks the room's
  events ([0x0711FCA0]: [+0] 5-dword records read through bit-field descriptors at 0x006CF6B0-0x006CF6DE, [+4] the
  regions, the first byte each region's shape 0-0x1E) and tests each with 0x004B9840(eax = record, ecx = trigger
  type, edx = region offset) by the region's shape (0x004B7600, 0x004B8780, 0x004B78B0). It returns the event found
  (-1 none; 0x400 and over are the other kind the dispatch tells apart). Flags of events done: bits at 0x0715D060.
- The item pick-up (0x004B8040, a step machine on the record at 0x00BA95D0: +0x00 item, +0x10 the done flag's index,
  +0x24 the step, +0x28 the message): sets the done flag and calls 0x004972B0(item) (adds it: bit item of 0x0712CA80,
  counts). 0x004B8B70 is the event script itself (ops by a jump table at 0x004B97AC).
- SH3 Quest VR wraps 0x004B9940 (src/game/sh3.c): while a closing hand asks ("Take items with your hand"), the record's
  front point is the palm and its facing points there for that search, so the game's own events run for what is at
  the hand (docs/PUZZLES.md).

## Camera (checked 2026-11-24, session 2: disassembly, tools/vcalls.py, tools/xrefs.py)
- 0x0072C610: the IDirect3DDevice8 pointer every draw call goes through (vtable slots: SetTransform 0x94,
  SetRenderState 0xC8, SetTextureStageState 0xFC, SetTexture 0xF4, SetVertexShaderConstant 0x13C).
- The engine's camera block (the graphics library at 0x678000-0x67C000), rebuilt by 0x0067B9B0 when the dirty byte
  0x0072C5B2 is set (every setter below sets it):
  - 0x0072C3A0: the view matrix (D3D row-vector 4x4), written only by 0x004162A0 SetViewMatrix(const float m[16])
    (cdecl; callers 0x0043AD30, which 0x006853C0 calls with 0x004A0500()'s matrix each frame, and 0x00415FF0).
  - 0x0072C360: the projection, built by 0x004162F0 from the FOV (0x0072C250 scale, 0x0072C254 / 0x0072C258
    angles, mode byte 0x0072C5B4) and near/far (0x0072C25C-0x0072C28C); 0x0072C5B5 + 0x004140E0: a projection given
    whole. 0x0072C320: the PS2-style screen matrix (2048 offsets) beside it.
  - 0x0072C3E0 = the view, times the matrix at 0x0072C5A0 when 0x0072C5B3 is set (0x0067B920; a camera shake?).
  - 0x0072C420 = 0x0072C3E0 x 0x0072C320 (getter 0x00678620): view to the PS2 screen (culling, 2D projection).
  - 0x0072C460 = 0x0072C3E0 x 0x0072C360 (getter 0x00678C20): view x projection.
  - 0x0067B9B0 sets D3D's PROJECTION (0x0072C360), VIEW (0x0072C3A0) and WORLD (identity 0x006B0980).
- 0x0062AF9C: D3DX-style matrix multiply (out, a, b); 0x0062B1E5 (jmp [0x72586C]): matrix transpose.
- 0x00459220 (the PC VR mod's "frame renderer"): uploads 0x00678C20()'s matrix, transposed into 0x00848C04, to vertex
  shader constants c2-c5 (also 0x00458820, 0x00459C00): the main camera's view x projection for every world shader.
  c8-c10: a 3x4 view (the VR mod: equal to the VIEW transform); c80-c82: the flashlight's projection (VR mod).
- Checked on the PC rig (2026-11-24, session 3, SH3_VSC_DUMP=N: every shader draw's constants in frame N):
  - Skinned draws (vertex stride 48, shaders F0000020 / F0000021 under sh3-nx's numbering from 0xF0000001) carry a
    16-bone palette from c48: three registers a bone, each a row of a world-space 3x4 (unit rotation rows, the
    translation in w; Heather's pelvis bone at her position). Their shaders read the bones through a0, never c80
    directly, so their c80-c82 are bones 10 and 11.
  - Constants stay set between draws: an unskinned draw's c48 on can hold the last skinned draw's bones.
  - 17 world shaders read c80-c82 directly (F0000002-F0000014, F0000017, later F0000069): the lit ones; with the
    flashlight off their c80-c82 are near zero (each entry about 1e-4, no translation). The PC VR mod's list of the
    game's D3D8 handles (0x0F, 0x15, 0x19, 0x2D) is a subset; d3d8_shader.c marks a shader lit when its code reads
    c[80] and c[82].
  - Heather in the opening's walk (frame 5280): eight skinned draws of shader F0000020 (vertices / primitives 208/430,
    205/482, 126/242, 83/150, 120/262, 183/446, 1155/3122 the head and hair, 547/1206 the body), 15 bones each, many
    bones shared between draws (the same rows); some joints have two or three matrices at one place (twist bones).
  - Heights (y down, the floor at y 0): in play (frame 8000, her 73 bones, below) her eyes are about 755-760 up, the
    shoulder joints 154 apart, the hips 436 up, the knees 266 up: 450-550 units a metre. kUnitsPerMetre is 490 (her
    eyes then at a 1.65 m player's eye height, 1.55 m); the PC VR mod's 360 made her 1.4 times the player.

## Bones (checked 2026-11-24, session 3: SH3_VSC_DUMP=N logs every bone of each character model in frame N)

- The model draw 0x0045F5D0 calls 0x0045EC00(model, skeleton, world, flag) (cdecl), which fills [0x006B4540] with
  [model+0x0C] matrices of 64 bytes: D3D row-vector 4x4s in the world (rows 0-2 the bone's axes, row 3 its origin).
  0x0045EC90 then builds each part's blended matrices at +0x2000 from them, 0x0045ECF0 transposes them all in place,
  and the parts upload them with 0x0045ED40 (the loop at 0x0045F3E0: bone idx at c48 + 3i; at 0x0045F440: bone
  idx + 0x80). Anything changed in the array between 0x0045EC00 and 0x0045EC90 shows in every part, her shadow and
  reflections alike. src/game/sh3.c wraps 0x0045EC00 (regen with --exclude-manual src/game/sh3.c).
- The model's header: +0x08 the offset of the rest matrices (0x80), +0x0C the bone count, +0x10 the offset of the
  parent table (signed bytes, -1 a root), +0x14 the blend count (91 for Heather), +0x18 / +0x1C the blend tables.
- Heather: 73 bones ([model] 0x00C0E060 at frame 8000 of the opening). Parents:
  -1 -1 0 1 2 1 4 3 5 6 7 4 3 3 8 4 5 5 9 9 9 10 9 9 11 11 11 14 9 9 15 15 15 21 26 27 32 34 36 37 37 38 38 39 39 39
  39 39 41 41 41 41 41 43 44 45 46 48 49 50 51 53 54 55 56 47 57 58 59 60 52 65 70
  - 0 the upper body's root, 1 the lower body's, 2 the spine, 4 the chest; neck 6 -> head 9 -> 18 (the face's bones
    19-23, 28, 29 below 9); collars 11 and 15 -> shoulders 26 and 32 -> elbows 34 and 36 -> forearms 37 and 38 (a
    second bone at each, 40 and 42) -> hands 39 and 41 -> fingers 43-72 (not moved in play: stale far places); legs
    1 -> 3 / 5 -> 7 / 8 -> 10 / 14 -> 21 / 27 -> 33 / 35.
  - The upper arm is 122 units (25 cm), the forearm 132 (27 cm). Her head bone 9 is at about her eyes' height.
- SH3 Quest VR (LIST 19, 41-43, src/vr/sh3vr_ik.cpp): her whole body is posed in that array in first person (lean,
  crouch, head turn, planted feet, both arms to the controllers with her collars, elbows and forearm twist, her own
  fingers from the controls); her head bone's place is the centre of the first-person cut (a 17 cm sphere instead of
  a flat cut, so her arms stay in view). The model's rest matrices are read once and their kind found by matching
  the game's animated bone offsets (logged "VR body: rest pose ..."), and her finger shapes and bend axes come from
  them. Heights in play (units over the floor): pelvis 519, spine 579, chest 630, neck 708, head 749, collars 688,
  shoulders about 670, ankles about 71, eyes about 760; her chest bone is 26 cm under her eyes, so Jak's "chest near
  the eyes" fade rule became "the eyes within 15 cm of the pelvis-chest line".
- The blends (0x0045EC90): blend j = A_j (the 64-byte matrix at model+[model+0x1C]+64j) x bone[byte
  model+[model+0x18]+2j+1], written at [0x6B4540]+0x2000+64j. The palette goes up one bone (3 registers) at a time
  from 0x0045F403 (bones) and 0x0045F465 (blends); d3d8.c counts them per draw (pal_count) for the draw matching.
- 0x0045EC90(model) and 0x0045ECF0(model) take the model alone (cdecl; 0x45EC90 uses its argument slot as a local):
  the blends are [model+0x14] matrices at [0x6B4540]+0x2000; 0x45ECF0 transposes the [model+0x0C] bones and those
  blends in place. Re-posing after them: write the untransposed bones, call both again (session 3, the generated code).
- 0x0045ED40 draws the parts: eax = model, stack (skeleton, world), cdecl. The first part is model+[model+0x24], the
  count [model+0x20], the next part part+[part]; [part+0x44] its vertex count. Part i is drawn only when bit
  (byte [part+0x52]) of the skeleton's 64-bit mask (+0x10 low, +0x14 high) is set: the part's group.
- Heather's 55 parts by group (parts1.log, 2026-11-24): 0 the body; 1 the scenes' detailed head; 5 play's head; 3 her
  right hand with fingers (parts 10-13, on hand 41 and its finger bones); 4 her left hand with fingers (parts 14-18,
  on hand 39 and its finger bones; which finger bones each part uses: palette_bones.py on a scene's dump); 12-31 and 33-38 rigid hands (one bone each, the
  poses play swaps in); 32 part 48, an item on her chest. Masks seen: scenes 0x1B (groups 0, 1, 3, 4); play 0x3021
  (groups 0, 5, 12, 13); one frame high 1, low 0xA021.
- Her finger chains (base -> middle -> tip): left hand 39: 43-53-61, 44-54-62, 45-55-63, 46-56-64, 47-65-71; right
  hand 41: 48-57-66, 49-58-67, 50-59-68, 51-60-69, 52-70-72 (47 and 52 judged the thumbs from their chains; to be
  confirmed by the base nearest the wrist in the rest pose). In play the game leaves the finger bones stale, so their
  pose must come from the rest matrices at model+[model+0x08] (local or model-space: to be detected, e.g. by the
  hand's rest origin, local when under 300 units). Arm A (11, 26, 34, 37, 39; twin 40) is her LEFT, arm B (15, 32,
  36, 38, 41; twin 42) her RIGHT (x her right, y down, z forward: a right-handed frame). Legs: hips 3 / 5, knees 7 / 8
  (twins 10 / 14), ankles 21 / 27, toes 33 / 35; pelvis bones 0 and 1 at y -519, the floor at about y 0.
- In a skinned draw's palette (c48 on, three rows a bone after the transpose) each row's w is the bone origin's x, y
  or z: a draw's bones can be matched to the posed bones by origin (tools/palette_bones.py does it from a dump).
- SH3 Quest VR wraps 0x0045ED40 (src/game/sh3.c): in first person with her fingers posed, groups 12-31 and 33-38 are
  cleared and 3 and 4 set for the draw (her own finger hands in place of play's rigid ones), the mask put back after;
  then the ghost pass (a hand through a wall): the bones and blends saved, the ghost pose written, 0x45EC90 and
  0x45ECF0 called, the mask the ghost hand's group alone, 0x45ED40's generated code called again (VR draws it faint,
  over the walls, for the eyes only), everything put back.
- 0x006853C0: the frame's camera set-up: projection scales (0x0043AB80 / 0x0043ABC0, sh3-nx's widescreen hooks),
  0x00471800 / 0x00471830 (camera position / look-at?) copied to 0x0072B9E0 / 0x0072BA00 / 0x0072BA10, and
  0x004A0500 / 0x004A0510 / 0x004A0520 (the camera module's matrices) to 0x0043AD30 / 0x0043AD70 / 0x0043ADB0.
- SH3 Quest VR uses this: 0x004162A0 is wrapped (src/game/sh3.c) so that the frame's view is the headset's centre eye
  in first person (the whole render and its culling follow the head), and the eyes are then put right per draw in the
  vertex stage from the projection at 0x0072C360 (src/d3d8/d3d8_shader.c, uEye).

## Sound (checked 2026-11-24, session 3: disassembly)
- (Session 4, LIST 34) SH3 never uses DirectSound3D (the IIDs of the 3D buffer and listener are in the exe but never
  referenced): it pans and fades every voice itself. A 3D sound is placed by 0x0046CDF0 (from 0x004A33B0(sound,
  volume, position, flags), which character sounds 0x005680F0 use) and 0x0046CFE0 (the radio's voices too);
  0x0046CCD0 is the same kind but has no caller. Each: the distance fall-off 0x0046CB10(eax = position, range, mode)
  (mode 0: from the camera's place, 0x00471800() = 0x0089E850; mode 2: across the floor from Heather's, 0x00472FE0();
  mode 1: none), the place through the listener matrix 0x004A0500() = 0x00B7F5B0 (the game camera's view, the same
  row-vector matrix as D3D's view; 0x004A04A0 writes it), the angle atan2(x, z) of that (0x0060BAE0; forward is +z),
  then volume and pan indexes 0-127 to 0x004A82B0(sound, volume, pan, pitch). The pan is the sine of the angle:
  left and right only, front and back alike.
- SH3 Quest VR's "Spatial sound" (on): sh3.c wraps those two, 0x004A0500, 0x00471800 and 0x0060BAE0: inside them the
  listener is the eyes (the view VR hands the frame camera) and their place, and how far behind the head the sound is
  goes to dsound.c's mix (softer and duller behind). Checked on the rig (run vr48): the listener matched the camera's
  view on the first try; Heather's footsteps came out under the eyes.
- The movies' scrambling (LIST 5): see Movies.
- The radio: 0x004EEEF0 (the sound set-up) makes four radio voices, DirectSound buffers of sounds 0x163-0x166, kept at
  0x00BBAF90 + 12 n ({sound, ?, buffer}); errors "SD/EE: ERR!! dsRadioPlay / ch (%d)". 0x004EEA90(voice, volume
  index 0-127, pan 0-127, pitch) (cdecl; "dsFxSet / radio_no") sets a voice's volume from the table at 0x006B79B8
  (hundredths of a dB, 0 the quietest at -960), pan and frequency. 0x004A8550 is its wrapper (mono when [0x00BA8204]).
- 0x0046D130(voice, level, pan, pitch) (cdecl; called a voice at a time by the radio update 0x004AA6B0 from 0x18-byte
  records: level float, pan float in radians, pitch u16): volume index = clamp((sound option 0x1F + 10) / 15, 0, 1)
  x [0x0089E3C0] x level x 127; pan index = (sin(pi/2 - pan) + 1) / 2 x 127. The level (0..1) is the static's
  loudness before the volume options: SH3 Quest VR wraps it for the radio's vibration (LIST 23).
- (Session 9) 0x004A33B0(sound, volume float, float *pos, flags) (cdecl) sets 0x0712C0DD = 1 and copies the position
  and volume to 0x0712C0E0-0x0712C0F0 (the noise the monsters hear, likely), then 0x0046CDF0(sound, volume, pos,
  10000.0f, 0, flags). 0x0046CDF0(sound, volume, pos, range, pitch -0x400..0x2FFF, flags): the 3D sound alone; volume
  index = [0x0089E3B8] (the sound effects' level) x volume x 127; 0x004A82B0(sound, volume index, pan, pitch) ->
  0x004EE620 -> 0x004EE440 (-1 for a sound not loaded). 0x0046CD50(sound, volume, ?): the 2D one. SH3 Quest VR's
  hand sounds never use 0x004A33B0 (they would call the monsters).
- Sound id N = entry N of data/sound/sd.afs (378 entries; plain PCM WAVs up to 0x169, CRI/AIXF after). Fire / reload /
  empty click: handgun 0x27 / 0x28 (volume 0.5) / 0x29; shotgun 0x3B / 0x3C (6 hits in 0.7 s: two shells, the second
  from 369 ms) / 0x3D; submachine gun 0x51 / 0x52 / 0x53. The empty clicks come from the empty-gun guard 0x004E79A0 (by
  the weapon byte 0x0712C656 through 0x004E5DE0, volume 0.5). 0x34 = an item obtained (2D), after every 0x004972B0
  (add an item) and by the pick-up 0x004B8040. 0x005680F0(chr, pos, ?, index): event index n plays sound row n-1
  (bytes 0x005683FC, jumps 0x005683D8): rows 0-1 0x27, 2 0x28, 3 0x3B, 4 0x3C, 5 0x51, 6 0x52, 69-70 0x48, 71 0x28,
  72 and 75 0x49, 73 0x52, 74 0x51.

## Rendering and display
- 0x0072C780/84: the display mode the game wants; 0x0072C788/8C: the size it lays the 2D out in;
  0x0072C664: D3DPRESENT_PARAMETERS. 0x004193A0 creates the device, 0x00419760 resets it, 0x00416AE0 sets the mode,
  0x00416B70 sets the 2D size (sh3-nx's widescreen).
- 0x0043AB80 / 0x0043ABC0: the camera's horizontal / vertical projection scales (returned in ST(0)).
- 0x00682BA0: float screen_x(int x) (2D x in 1/16 units around 2048 to pixels); full-width callers listed in sh3.c.
- 0x0072C834..0x0072C83D: post-process switches; 0x0072C839 the Noise Effect switch (the VR mod reads it).
- 0x0072D150: read with the post-process switches by the VR mod (not named there).
- 0x0072C790 / 0x0072C798 / 0x0072C7E8: frame-rate values (the VR mod writes 90 for its 90 fps mode); the
  ThirteenAG frame-rate fix patches the timestamp accumulator at 0x0041B5D1 (DEC EDX).
- 0x00401ED8: nop cave after a ret (sh3-nx: health indicator position).
- (session 3 part 3) The 3D scene is drawn into three render-target textures and a depth surface sized by
  u32 0x0072BFD0 / 0x0072BFD4, made by 0x00402B60 (start-up) and 0x00402E00 (a resolution change), both (w, h) on the
  stack: 512 x 512 by default whatever the screen. sh3.c wraps both to give the eye's size (1000 x 1000 on the rig,
  run 32: the 3D world's detail as sharp as the 2D).
- The scene's own depth surface is not the device's auto depth (which kept a flat value): the probes read the depth
  surface the frame camera's draws used most (d3d8.c note_scene_depth / scene_depth; logged when it changes).
- The film grain (sessions 5-7): 0x0041C090 is the screen's last pass; the noise runs only while 0x0072C839 is set
  (play sets it, 0x004321A0; the title and menus clear it). It is an alpha test turned on by 0x00427ED0 (return address
  0x00427EF0). VR keeps it off unless "Film grain" is on (LIST 55); the soft mottle left in dark places is the fog's
  own texture.

## The flashlight and her lighting (sessions 5-9: disassembly, runs vr107-vr142)
- State machine 0x00496BE0: 0x0712CABC bits 0-1 (off, on, turning on, turning off); its strength 0x0712CAC4; held dark
  while [0x00B6B850] != 0 (room scripts set it through 0x00496BD0; a new game clears it, 0x00497690). The item is 31
  (0x1F). 0x00450660 turns it off per map.
- The light buffers, double-buffered at 0x00848AF8 / 0x00848B78: +0x10 intensity, +0x20 colour, +0x40 position, +0x50
  direction, +0x60 / +0x70 the shadow caster's position and direction. Her light record: 0x0712C1D0 the position (her
  chest), 0x0712C1E0 the direction, 0x0712C1F0 / 0x0712C200 the shadow caster's. Read by the glare (0x0044A7B0), the
  monsters (0x005D27A0) and 0x004C9280; 0x00457AC0 copies it to the buffer (VR places the light there: sh3.c,
  sh3vr_flashlight_place).
- The frame's order (the glare follows the lamp): the frame routine calls 0x00685B30 (-> 0x004571B0 -> 0x00457AC0,
  where VR writes the record) at 0x0058F068, and 0x00685F90 (-> 0x00455780 -> 0x0044A7B0, the glare, reading the record
  through 0x00475D60) at 0x0058F097; between them only 0x00685B60 (draw calls). The record's builder 0x00475E00 (via
  0x00473EC0) runs from the game's update (0x0046AD26, 0x004740E8, 0x004A19C6).
- The shadow caster sits 0.51 m along the beam from her chest, 0.10 m off it, turned about 12 degrees down from it
  (vr136). Put at 0.25 m (SH3_CASTER_AHEAD, vr139) her hand held in the beam at 0.35 m still cast no shadow on the wall
  ahead: her own draws seem not to be in the shadow's pass (still open: LIST 56, her head and gestures in her shadow).
- The lit world shaders read c16 (the light's place) and c80-c82 (the beam's rotation rows: its cone). Her model's
  flashlight: part 48 (group 32), 43 vertices, 78 primitives, shader F0000045, all on bone 4, a box 8.1 x 5.1 x 3.9 cm
  centred at (12.55, -43.0, 75.85), the lens its +z face. Bones at draw time are transposed (0x0045ECF0).
- Her skinned shaders (F0000030 in the flashlight run's numbering) never take her own lamp: four directional lights
  (c8-c11 their directions, c12-c14 their red, green and blue, a light a column) and an ambient (c6), times c7; the
  game fills three (c11 and the fourth column stay 0, vr119); the point lights (c16-c22) and the beam's cone (c80-c82)
  are not in them. VR puts the beam in the free fourth light (sh3vr_ik.cpp body_light): from session 9 per point of
  her draw in the vertex stage (d3d8_shader.c: her dir4 shaders read c11-c14 as gC11-gC14 and run a second time with
  the light from the lamp to that point, whole within 15 degrees of the beam's line, none past 30, whole to 1.5 m, none
  past 3 m). One light per draw (session 8) lit her forearm's draw and left her hand's beside it flat grey (vr136, vr139
  frame 7455), which looked like the ghost hand.

## The weapon in her hand (sessions 5-7)
- Weapon bone 0 is her right hand bone (identity); the controller's grip point in her hand bone is (24.4, -1.1, -14.3)
  units.
- 0x00495360(weapon_obj, chr_type) attaches the weapon each frame (from 0x00473E40 / 0x00473EC0 / 0x00473FB0); its
  world matrix at [[obj+0x70]+4]+8 (sh3.c wraps it: the weapon object moved to the VR gun, so the flash, light and smoke
  follow it).
- The shot: 0x004B6260 -> 0x004B3D50 (origin and direction via 0x004954E0 mode 1); 0x004ACE10 queues the battle entry,
  0x00495040 the flash level [0x00B57344], 0x005680F0 -> 0x005C1660 the trace, 0x00497800 the fire. The flash is
  screen-space rhw quads (FVF 0x144, texture 13EAE730).
- Heather's struct (MEM32(0x0712C100)): animation channels at +0x1B8 and +0x240; the animation table 0x006D00D0
  (12-byte entries: id, frames, rate 0x1000 = 1 frame a tick, first, last, flags); the handgun's raise 0xDA, fire 0xEC,
  lower 0xDF.

## Shadows (session 11, disassembly; LIST 56)
- The stencil shadow pass: 0x685D30 takes a caster list from 0x687F30 and skips it when 0x5EF950("shadow") is 1 (a
  setting named "shadow", string 0x6942AC); 0x4913C0(list): 0x48BBC0 sets the states (STENCILENABLE 1, STENCILFUNC
  ALWAYS, STENCILPASS 7 at 0x48BC6B, masks 0x1F, ...), a stencil clear, 0x491030(list, 1), a clear again, then
  0x48CF70 (the darkening pass). 0x491030 walks the list (0x10-byte entries, +0xC the kind: 2 a group header) and
  sends each item by its kind to 0x490810, 0x490CF0 or 0x490F00 (mode 1); the same walker draws the normal pass with
  mode 0 (0x491320 -> 0x491030(list, 0)). Items carry three row pointers (+0x10/+0x20/+0x30 -> 0x8E4390..98) and
  their data at +0x44 (0x8E439C). The list: 0x687F30 = 0x47BF50(the two area ids 0x4CC540 gives), the same call
  the normal pass makes (0x685DA0): by the code, the map's own geometry by area, so these volumes are the room's
  (static casters; read from the code, not pictured: a SH3_VSC_DUMP frame can confirm).
  Open: where Heather's own shadow comes from (another pass) and whether it is built from the
  bones 0x45EC00 leaves (LIST 19's IK) or from an earlier pose. Leads (every "push 1; push 0x34", STENCILENABLE on,
  tools: scratchpad findbytes.py): 19 in 0x41C213-0x420F73 (a block of its own), 0x4613A7/0x461457/0x4615B7/0x461667
  (beside the model code: 0x4613A0 is called from 0x4594C7 and 0x4599D9, among texture-stage and shader calls), the
  map pass above (0x48B6CB, 0x48B973, 0x48BC21, 0x48D469) and 0x5F5363-0x5F84B6. Next: SH3_VSC_DUMP in a frame
  with her in the flashlight's beam by a wall, then which of these runs (a breakpoint log on each).
- Session 12 (builds 154-158, runs vr183-vr186; the stencil-on log and the list dump in d3d8.c dev_SetRenderState):
  - One frame's stencil users, in order (vr183 frame 7455, vr185 frame 7385): 0x4613B2 and 0x461462 (the model code:
    ALWAYS, REPLACE ref 64 with the count bits kept; 0x461462 comes right after her part draw 0x45ED40, so the game
    draws Heather herself with it), 0x48B97E (inside 0x48B930: the volume states, front faces INCR on cull 3, back
    faces DECR on cull 2, masks 0x1F), then 0x48D474 (the darkening: the stencil test LESSEQUAL ref 17 mask 0x1F,
    into the mask target 0x1411EE10 set from 0x48D3D2, then 256x256 targets from 0x48D883/0x48DA64 and back). The
    stencil is cleared to 16 (0x48B2B0: Clear flags 7, stencil 0x10, called from 0x685B73), so a pixel is in shadow
    at a count of 17 or more; a count below 16 is never shadow.
  - The frame's shadow drawing is 0x685C10: esi = 0x4D1020() (the characters' shadow list), edi = 0x687F00(1, 1)
    (the map's), 0x5EF950("shadow") == 1 skips both (the shadows setting off); then, if esi, 0x48B600 and
    0x491320(esi) (called from 0x685C55, returns to 0x685C5B); if edi, 0x48B600 and 0x491320(edi) (returns to
    0x685C87). 0x491320(list) = 0x48B930 (states) + 0x491030(list, 0) + 0x48B230.
  - 0x491030's list: 16-byte records (pointer, type 0-8 picked by the jump table at 0x4912F8, ?, end flag at +0xC;
    "illegal shadow type" 0x690AC0 for a type over 8). Type 0 and 2 records point at an object with three rows at
    +0x1C0/+0x1A0/+0x1B0 (to 0x8E43AC/B0/B4) and +0x1F8 (to 0x8E43B8); type 1 at +0x120/+0xC0 (0x8E43A0/A4).
  - The start room (vr185, vr186): only the map's list, one record, type 0, object 0x0712D260 whose first row is a
    point about 4 m ahead of her and 0.6 m up (x -14171, y -1022, z 20267: a light's place): the room's own volumes.
  - The characters' list: 0x4D1020 loops over the shadow-light slots i: 0x447220(i) = [0x7F4C18 + 4i] (on),
    0x447280(i) = [0x7F4C7C + 4i] (its data), 0x4D0EF0 sets up each light's buffer ([0x7119FA8 + 4i], 0x683550
    allocations, 0x4D09C0), then 0x4D12E0/0x4D1350 build the list (0x7119FE0, 0x7119FC0, 0x711A1E4). The start room
    has no shadow light: the list is empty (0x4D1020 returns 0), and setting the byte 0x821DDF (read by 0x450100)
    does not make one (vr186, SH3_CHAR_SHADOWS=1). So the game shadows characters, Heather too, only in rooms whose
    lights are set up as shadow lights: the owner's "at SOME points" (owner message 51), GameSpot's PC preview's
    "animated shadows, which lengthen and deform realistically as Heather steps into and out of the light".
  - Characters' shadow models: the exe names a .kg1 beside each character and weapon model (data/chr/pl/chhdd.kg1 and
    chhcc.kg1 for Heather; wp_hguns, wp_subs, wp_pipes, wp_pipeg, wp_u_sub.kg1 for weapons; items and background
    pieces too). Model replacement mods report wrong shadows unless the shadow files are redone (Nexus SH3 mods 73,
    77), so her shadow is her kg1 mesh, not her drawn parts. The shadow walkers (0x490810, 0x490CF0, 0x490F00) never
    call 0x45EC00, where VR's IK writes her bones (the draw's scratch 0x6B4540), so in those rooms her shadow keeps
    her animation's pose. zealottormunds' SH3 camera mod (github.com/zealottormunds/sh3cammod, README only) tells
    first-person players to turn realtime shadows off.
  - Who turns a character shadow light on: the event-script commands 0x5E8D90(i, data) (0x447250: [0x7F4C18 + 4i] = 1,
    then 0x447290(i, data) keeps its data at [0x7F4C7C + 4i]) and 0x5E8DB0(i) (0x447230: off); 0x5E8D20 / 0x5E8D70
    do the same for a second set at 0x7F4C08 (0x447260 / 0x447240, its data through 0x4472A0). So a room's own script
    turns them on. 0x4D0EF0 (up to 4 lights, count at 0x7119FA0) reads a 16-byte record from its argument (4 dwords;
    session 12: its +4 low 16 bits a count over 0, 0x78 bytes allocated per unit (0x683550), so not simply a place;
    likely the light's place) and allocates the light's buffers (0x683550 into [0x7119FA8 + 4i], +0x44), then
    0x4D09C0. Lead for the playthrough (message 51): in such a room, VR could cast her IK volumes from these lights too
    and leave the game's kg1 shadow of her out (its list entry), so that shadow follows her body as well; needs a save
    there to see the record's layout and the list.
  - VR's own: her IK body, the weapon held and a reload part as volumes from the flashlight, counted into the game's
    stencil before its darkening (sh3vr_ik.cpp her_shadow_tris, d3d8.c vr_her_shadow); her own pixels carry stencil
    bit 0x20 so her volumes leave her body alone. In a room with the game's character shadows the log says "the
    game's own character shadows are on here": the playthrough checks her shadow there (HEADSET_CHECKLIST).

## Timing (the PC VR mod's findings)
- The game scales its simulation by frame time: 0x070E67A8 frame scale (60 / fps: 1.0 at 60, 0.6667 at 90),
  0x070E67AC frame seconds (1/fps), 0x070E67C0 = 1, 0x070E67C4 nominal fps. The VR mod's "virtual mode 4" writes
  this tuple each frame for 90 updates a second, and says movement and animation speed stay right.
- 0x0072C790 / 0x0072C798 / 0x0072C7E8 are the limiter's internal multi-tick state, not target-fps controls: writing
  90 there dropped gameplay to about 1 fps (the VR mod retired that setting).
- The native game fluctuates between 60 and 30; ThirteenAG's fix (0x0041B5D1 DEC EDX 0x4A -> INC EDX 0x42) keeps it
  at a steady 60. Steam006's PC Fix "fps mode 1" is the 60 Hz baseline.

## Input
- 0x007214E4: the pad's default bindings, 12 shorts (button << 8 | action) copied when savedata/key.ini is missing;
  buttons are 0x0C + dinput.c's numbering (X A B Y LB RB LT RT Back Start LS RS); actions 0 search view, 1 ready
  weapon, 2/3 strafe left/right, 4 map, 5 action, 6 dash, 7 light, 8 pause, 9 zoom, 0x0A supplies, 0x0B inventory.
  The other 13 entries (stick, camera, menus) stay.
- The 25 pad entries at 0x7214E4 (checked: tools/peek.py on the owner's exe), u16 = button << 8 | action:
  12 bindable (above), then actions 0x0C-0x0F on 0x18-0x1B (the d-pad: forward, turn right, back, turn left by the
  keyboard table), 0x20-0x25 on axes 0, 1, 2, 5, 8, 9 (analog: move x/y, camera x/y...), 0x10 confirm on A (0x0D),
  0x11 cancel on B (0x0E), 0x12 on Back (0x14; Esc on the keyboard as well: skip?).
- The keyboard table just before it (0x7214B0, action then DIK code): 0x0C W, 0x0E S, 0x0D E, 0x0F Q, 0x15-0x18 the
  arrow keys (up, down, left, right), 5 action Space, 7 light L, 4 map M, 0 search view Tab, 1 ready weapon B,
  0x0B inventory I, 6 dash LShift, 3 strafe right D, 2 strafe left A, 0x10 confirm Return, 0x11 cancel Esc,
  0x12 Esc, 8 pause Esc, 9 zoom Z, 0x0A supplies H, 0x13 F5, 0x14 F9.
- The mouse (checked: disassembly of 0x600920 and xrefs): the game reads the Windows cursor itself, once a frame
  (0x600920, called from 0x5FE830): GetCursorPos, ScreenToClient (unless fullscreen, 0x416C90), then x and y in
  0..512 of the client area (x << 9 / width) at 0x070E6408 / 0x070E640C (getters 0x600B60 / 0x600B70). The mouse
  counts as in use once it moved more than 4 px three times (counter 0x070E6410 >= 3; getter 0x600B50, read by 20
  places: menus). Buttons by GetKeyState (import 0x68A378; NOT GetAsyncKeyState, which it does not import: session
  11, vr178-179) polled at 0x600A67 for the VK codes at 0x721718 (3 shorts: 1 left, 4 middle, 2 right), testing bit 7
  of the low byte (Windows returns the SHORT sign-extended, 0xFF80 held; user32.c answers 0xFFFFFF80): held
  0x070E641B/1D/1F
  (getters 0x600B90/BB0/BA0), pressed this frame 0x070E641C/1E/20 (0x600BC0/BE0/BD0). 0x600AB0 sets the cursor
  (SetCursorPos, 512 units). Readers of the position: 0x4DD130, 0x4DD2F0, 0x59B350, 0x601A40; of the left button:
  0x59B350, 0x59C300, 0x5D9B10, 0x5DBD80, 0x5DC240, 0x5E64E0, 0x5E6650, 0x603F20 and the click edge in 0x59B350,
  0x5D9760, 0x5DC240, 0x5E3CC0, 0x5E4A70, 0x5E62A0, 0x5E68F0, 0x5E7970, 0x5ED1D0, 0x5ED3D6, 0x5EDC50. sh3-nx's
  user32 answers GetCursorPos with 0,0, so the mouse never moves there. The pointer (LIST 12) drives exactly this.
- Click zones (session 12, read in the lifted code; build 162 logs them): the screens register what a click can act
  on each frame. 0x601940 frees the list at 0x070E6580 (head; 0 none); 0x601970(x0, y0, x1, y1, id) adds a 0x18-byte
  node {x0, y0, x1, y1, id, next} at its head, x as given, y from the screens' 448-high layout to the cursor's 512
  ((y - 224) * 512 / 448 + 224); 0x601A40 returns the id of the first node with x0 <= x < x1 and y0 <= y <= y1, 0 if
  none or if the cursor is hidden. A screen acts when that id is nonzero and the left button was pressed this frame
  (0x600BC0), e.g. 0x5E62A0 (state 0xBCC178 == 0x17): zones 0x20 (256,121)-(290,149) and 0x21 set 0xBCC17C to 6 or 5
  and play sound 5. Hovering alone changes nothing; the click acts on the zone under the cursor. Lists register one
  zone per row with ids from 1 (0x5E89BA, 0x5ED158, 0x5EDBBD: x 8 to 0x1F8, rows from y 0x56). Callers of 0x601970:
  0x594-0x59A, 0x5D7-0x5D9, 0x5E2-0x5EE, 0x5F9 (about 45).
- The cursor's shown flag (0x070E6418, byte; getter 0x600B40, 0x601A40's gate): 0x600BF0 runs when a screen asks
  for the cursor (0x070E641A set, 0x600B80 path; 0x070E6419 skips a frame). It re-arms (shown, deadline 0x070E6414 =
  the time import 0x68A39C + 3000 ms) when the mouse is in use (0x070E6410 >= 3) or a button was pressed this frame,
  and otherwise hides the cursor once the deadline has passed; while shown, it tail-jumps to 0x4DD300 (draws the
  cursor). 0x600920 sets the in-use counter to 0 on any frame the cursor did not move, so a cursor held still is
  hidden 3 s later and a click then finds no zone (the PC rig's still hand). sh3.c keeps it shown while VR's
  pointer is on a screen (deadline 0xFFFFFFF0) and hides it when the pointer goes.
- The click zones are cleared again before Present (session 13, vr193-vr194: a walk of 0x070E6580 there found "0 of
  0" on every screen); sh3.c reads them inside the game's own question, a wrapper of 0x601A40 (the zones, its answer
  and the caller's return address), and the Present block uses that.
- OPTIONS (session 13, disassembly; the inventory's Options tab): per-frame function 0x5E68F0, run by the state
  machine 0x5E6B20 in its state 3 (state stack: depth 0xBCC658, states 0xBCC65C[depth]; 0x5F1B70 sets one, 0x5F1BC0
  reads one). 0xBCC178 (byte) the highlighted row: 0 the title, 1 Button Config, 2 Language, 3 Subtitles, 4 Brightness
  Level, 5 Vibration, 6 Sound, 7 BGM Volume, 8 SE Volume, 9 Control Type, 10 Exit (the other page's rows from 0x11);
  0xBCC179 last frame's row (copied each frame); 0xBCC17A the dialog (0 none, 4 done); 0xBCC17C the input;
  0xBCC182[i] the rows enabled. Its init writes the row at 0x5E4618. Each frame with row == last row, 0x5E2E60
  registers the zones (ids = rows; 0x1E/0x1F the page arrows, 0x20/0x21 a value's arrows) and 0x601A40 is asked at
  return address 0x5E69A4. A click (0x600B50's edge) over another row's zone sets 0xBCC179 to it and plays
  0x46CD50(1, 1.0f, 0) (the next frame highlights it); a click on the highlighted row confirms it (the row's handler):
  the PC game's two clicks. B always asks "Change the settings? Yes/No" (Yes highlighted): A on Yes leaves (saving to
  savedata/data.sys), a second B goes back to the rows. Seen in vr195 (session 13): with the cursor moving (the
  pointer swept down the rows, the mouse "in use"), the game itself highlights the row under it (Subtitles, then
  Exit, with its help line), and one pull on the highlighted Exit chose it ("Change the settings? Yes. No."; A on Yes
  left, data.sys unchanged). VR needs nothing more here (build 168's own highlight was dropped).
- The mouse cursor is drawn in 3D-projected 2D (calls returning to 0x004DDB98 inside 0x4DDAE0; sh3-nx keeps its 4:3
  width).

## C runtime
- 0x00662954 _setjmp3, 0x006628D8 longjmp (regen.sh passes them to the lifter).
- 0x00661D20 / 0x0066F9C0: the CRT's memcpy (memmove semantics), replaced by hand in sh3.c.
- 0x005F0C40: the CD check (GetDriveTypeA == CDROM, GetVolumeInformationA), result cached at 0x00BCC60C.

## Session 14 (2026-11-25): skinning, melee hits, the beam saber, the action check's reach

Heather's skinned vertices (shaders F0000020/21, read from vr102's GLSL; the weapons' too):
- 48 bytes: position at +0, weights w0..w2 at +12/+16/+20, w3 = c0.x - c0.x x (w0 + w1 + w2); the bone bytes at +24
  (byte k for weight k): row a = floor(byte x c0.w / 255), the bone's rows c[48 + a], c[49 + a], c[50 + a] (world =
  dot(row.xyz, p) + row.w); the normal at +28, uv at +40. VR skins her draws on the CPU the same way for her shadow.
- Her shadow: z-fail stencil (back faces z-fail INCR, front faces z-fail DECR, else KEEP), closed volumes (near cap the
  lit faces, far cap reversed, the sides by each silhouette edge's winding count), 6 m long: the game's far plane is
  10080 units (20.6 m).

Melee (the battle step 0x4B33E0, its jump table 0x4B39D0):
- A queued volume (0x4ACE10 copies 0x48 bytes into 0x71267A0: u16 kind, u16 battle id, the attacker, prev grip +8,
  prev tip +0x18, cur grip +0x28, cur tip +0x38). Kind 2 (0x4B37AD): 0x4B3260(attacker, &cur grip) with edi = prev
  grip, esi = prev tip, ebx = cur tip (registers) splits the swept quad by the fractions at 0x6CE148 and tests each
  piece with 0x4B29C0 until one meets something, filling the contacts at 0x711FCC0 (stride 0x28: +0 type 1 a
  character / 2-3 others, +4 the character, +8 the point, +0x14 a byte, +0x24 another pointer; count [0x711FCA4],
  reset by each test). Ids 0x28-0x2B, 0x2E, 0x2F also run 0x4ACF90 (effects at the contacts only). Then the way =
  cur tip - prev tip and 0x4AEA90(&way) (ecx = the volume).
- 0x4AEA90 writes hit records at 0x7120800 (0x38 bytes, count [0x7128A68], reset in 0x4ACA10; consumed records get
  +0 = -1 by 0x4AD040): for a character (once per character per volume) two records: +0 1, +4 the attacker, +8 1,
  +0x0C the point, +0x34 the character; and +4 the character, +8 4, +0x0C the point, +0x1C the way, +0x2C/+0x2E the
  volume's kind and id, +0x30 the contact's byte, +0x34 the attacker. Types 2/3: one record (+8 2 or 3).
- 0x4B6A20(chr) gathers a character's hits into chr+0x15C (damage), +0x134 (point), +0x144 (way), +0x154 (reaction),
  +0x16C (kind), +0x17C (the attacker): Heather's own (0x4EC800) and, VR assumes, the monsters' too.
- Heather's learned attacks (sh3quest_learned.ini): the knife kind 2 id 14, the stun gun kind 3 id 39, the beam saber
  kind 2 id 47.

The beam saber (weapon 9, item 5; item 9 is the stun gun):
- The ready (the trigger, kRT) enters action 7 at 0x712C18C; 0x4D4E90 sets 0x712C66B = 1 (the beam), timers 0x18; the
  state byte [[player + 0x27C] + 0xB] goes 0 -> 4 -> 1 as it lights. The attack (A) from the ready: action 9, the
  flag cleared (0x4A1720), timers 0x300, state byte 3. Pressed while the draw's action 7 (state 1) still ran, ready
  and attack together gave the attack decision 0x494C40 = 5 and action 37: no beam (vr202).
- The beam is the game's own screen-space strip (XYZRHW|DIFFUSE|TEX1 = 0x144, 8 primitives, a 64x64 texture, added
  light) at its animated hand (vr203); the fog is 0x144 quads with a 512x512 texture. CORRECTED in session 15 below:
  the blade is a skinned draw; the 64x64 8-vertex draw is a small ribbon.

## Session 15 (2026-11-25): the beam saber's draws (runs vr206-vr219)

- The blade: a skinned draw of its own, 24 vertices / 30 indices, stride 48, ONE palette slot holding model bone 1,
  drawn just before the hilt (126/154, one slot, model bone 0). Bone 1's x row is 0 while the beam is off and is
  stretched along the blade as it extends (the game scales the bone, not the mesh). Its bone sits 76 units from the
  hilt's in her animated hand (vr216). After a swing the game clears 0x712C66B while the blade is still drawn out
  (vr218: flag 0 from frame 7596, state byte 20, action 8), so the flag is "lit by the ready", not "blade shown".
- The lit hilt's shader F0000031 differs from the unlit F000002D only by the point lights: every lit shader takes
  four (c16-c19 places, c20-c23 colours, c24-c25 the falloff per light; c32/c36/c44-c47 the flashlight's cone). The
  saber's light is a green one, 0.40 0.60 0.20, at c17 in the hilt's draws, on the blade in her animated hand: 0.25 m
  off the palm-to-tip line at ignition, 0.45-0.57 m as it extends, 0.92 m on a swing's frame (it trails). Her
  flashlight is the white 2.0 2.0 2.0 point light at c16 (0.6-0.9 m from the saber). Her own skinned shaders take no
  point lights (directional c8-c14 only).
- 0x468F6E and 0x468CDA (sub_004690A0) draw 128x128 sprites at z 0.50003, rhw 1: the scene lamps' halos, drawn every
  frame, before the beam lights. Not the saber's.
- 0x5F6937 draws 64x64 0x144 quads: 4-vertex dust at its true depth, and an 8-vertex ribbon only in lit frames, tiny,
  z about 0.95.
- vs F0000007, 6 vertices, blended (added light), a 512x512 texture: two draws a frame at ignition (frames 7518-7540
  in beam207.txt): the ignition glow.
- No existing mod or tool documents the saber's mesh (searched session 15: sh3-evil-effect, Nexus Silent Hill 3 mods,
  silent-hill-museum (SH2 only), the PC fix and RTX pages).

The action check's reach (LIST 45): 0x4B9940's polygon regions test the 1.02 m segment from her place along her
facing; a region can lie past a hand's reach yet within the button's (event 8 at the opening, vr196-197).
The room's floor items: 0xBA9610 lists indices into the room table's items ([[0x711FCA0] + 0xC] + 18 x index: +6 /
+8 a place index (x 64), +0x10 the flag that it was taken).

## Session 15, later (2026-11-25): the stage's event table, doors (runs vr220-vr225, sh3.c's table log)
- The opening's table ([0x711FCA0], 79 events) covers the whole nightmare stage: the first area (x < 0, z > 0), the
  circular hall (x 15000-27000, z 14000-24000) and the next area (z < 0, events 58-78). The nightmare has NO floor
  items (0xBA9610 empty in both areas, vr223-224); the first are in the mall (Happy Burger).
- An event's 5 dwords: the 2nd word's low byte is the kind seen so far: 0x0B and 0x06 a door to another area, 0x0A an
  examine text, 0x00 others; a door's two sides share the 3rd word (BE38: event 5 in the first area with 4 in the
  hall; BE40: 46 at 22443 23959 with 45 at -21000 -23750; BE48: 48 at 24809 17840 with 47 at 16563 -19640).
- Regions are a byte stream of ops, each an op byte then its fields: 0x02 and 0x04 (13 bytes) x float, s16, z float,
  s16; 0x11-0x14 (11 bytes) x float, z float, 2 bytes; 0x15 (13) x, z, 4 bytes; 0x16 (17) and 0x17 (21) x, z and
  more. The table log's "(at x z)" reads the floats at +1 and +5, right for every op but 0x02 / 0x04 (their z at +7).
- The opening's entrance is event 5 (stand at -13406 23244 facing +x; the tall door, event 18, is behind the stone
  rail at z 22118). The hall's west side is a half circle with a railing (vr224 slid along it to 17139 19235 going
  +x); a walkthrough (hxchector.com): turn right in the circular hall and follow it round to a yellow door.
- The button's region test (read from sh3.exe, session 15): 0x4B9840 picks it by the region's op through the byte
  table at 0x4B98D8 (trigger 1): ops 0x01 and 0x10 a point (0x4B7600); 0x02-0x0C and 0x11-0x1B 0x4B8780; 0x0D-0x0F
  and 0x1C-0x1E 0x4B78B0 (trigger 2 has its own table at 0x4B9904). In 0x4B8780 an op under 0x10 has a height (2
  bytes, checked against hers) after x; an op of 0x10 or more counts op - 0x0F; then z, then 2-byte steps decoded by
  0x4B6C30 that walk the shape's edges. Op 0x12 (event 5, event 48) is a corner and one step: event 5 a line at x
  -13157 from z 22453 to about 23950, event 48 a line at x 24809 from z 17840 toward +z (found by the hand from
  24323 18169, vr226). The door's face stands about 0.8 m before its line; the 1.02 m test reaches it from there.
- Monsters' hits on her (vr225-vr227): the hall's dog bites with kind 122 (5.0); the Closer's arm kind 120 (0.2 a
  frame while it sweeps), its blow kind 121 (2.0). The runs are not frame-exact: which monster comes first, and when,
  changes run to run.
