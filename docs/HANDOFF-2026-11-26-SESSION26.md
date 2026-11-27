# SH3 Quest VR handoff, session 26 (2026-11-26): 0.1.7

Repo C:/SH3Quest/SH3QuestVR. 0.1.6 = ba72cb1 (tag v0.1.6, versionCode 7). Session 25's handoff
(docs/HANDOFF-2026-11-26-SESSION25.md) lists 0.1.7's first changes (the ding, her room shadow, A walking on, the dead
dog's hold, the tick and buzz, the Flat screen, melee damage, the break-free bar, the two-hand grip spot). This
session took 0.1.7 to its last checks; it stopped on its usage limit before the release (build 235 failed on a held sh3.exe). Session 27 carried on: see docs/HANDOFF-2026-11-26-SESSION27.md.

## The owner's messages (C:/SH3Quest/notes/owner-messages-sessions1-7.txt, 132-136)

- 132: pick up from the other session, miss nothing, proceed with the list.
- 133: everything should look the best it can: "whichever looks best, works best".
- 134: a player on 0.1.6: the second green gate at the start (past the dog) would not open with A; past the first
  gate the hands could not be seen. Owner: "make sure there arent any issues in this next build ... make this build
  PERFECT".
- 135: the game's own shadow looks great now; the separate body shadow switch is not needed. Do what is best.
- 136: proceed with the list.

## What session 26 changed

### A opens what you look at (134), src/game/sh3.c sub_004B9940

The game's action check (0x4B9940) tests a 1.02 m segment along HER facing. In first person she turns after the head
only past 50 degrees, so a gate looked at could lie beside her line. When her own facing finds nothing the button
uses, the check is tried again along the head's heading, then 15, 30 and 45 degrees either side, from her place and
from the head's (when 5 to 70 cm from her), 1.02 m then 1.33 m; what it finds is written into the record for the
game's own check and logged ("the action button (pass N) found event E where the player looks: D degrees from her
facing, R m out"). When the head's place was used, her own place is put back right after the game's check (build
234), so a head leaning up to 0.7 m out (into a wall even) never moves her; the record's facing stays turned to what
she used.

- vr306: A at the hall's far door (event 48) by her facing.
- vr311 (SH3_GAZE_TEST=1, her facing's check left out): event 5, the park's entrance, found 45 degrees from her
  facing, 1.02 m out, used, with the tick and buzz.
- SH3_GAZE_TEST=1 is a rig switch only.
- SH3_STEER_AT_ATTACKER=N (N over 1) now stops going for an attacker after game frame N (vr308-310: a second monster
  at the far door drew her away from it).

### Hands in the dark (134), sh3vr_ik.cpp body_light and d3d8_shader.c

Past the first gate, hands held low or at rest were outside the chest flashlight's 30-degree beam and were black. The
beam now spills round itself onto what is right by the lamp: 0.6 of the beam from 32 to 70 degrees off, to 0.75 m,
none past 1.5 m (kHerSpill), as the PC VR mod lights its hand (DrawLeftHandEye8). vr307: hands at rest, at the waist
and looked at are seen in the dark hall; with the flashlight off they go dark with the room.

### The shadow switch (135)

"Heather's shadow" (her IK body's volumes in the flashlight's beam) is gone from the menu and the launcher; her_shadow
stays false and is no longer read from sh3vr.ini. The game's own shadow (session 25: her drawn mesh in the game's
count) is the one.

### Grips (129, 133), sh3vr_combat.cpp

Compared on the rig from the eyes and both sides: vr302 (the game's own rigid hands everywhere), vr303 (her finger hand
at the old measured hold: 3 cm ahead of the grip, closed on air), vr309 (her finger hand at the game's place), vr312
(six second-hand ways each on the handgun and the submachine gun, SH3_SECOND_TEST), vr313 (all ten weapons as chosen).

- **kFingerHandOn**: her finger hand at the game's place on the handgun, shotgun, submachine gun, stun gun and
  flamethrower (fist closed round the grip, the trigger finger on the trigger); the game's own hands on the knife (her
  fingers splayed through its thin handle), pipe, katana, maul and beam saber.
- **Hold.second_nudge** (applied only while the main hand holds at the game's place, i.e. right-handed;
  second_nudge(profile)): the handgun's support hand {-20, 7, -2} model units (4 cm back, 1.4 up): its palm on the
  grip's left panel, its fingers round the main fist's front (at second_at alone they stuck out open ahead of it).
- **Hold.second_thumb**: 0.15 on the submachine gun and the flamethrower (curled 0.7 it stood up over the receiver's
  top); body.cpp's role-2 thumb base takes it (weapon_hold_thumb).
- **The submachine gun's second_at** y 38.5 to 32.5: her fingers up its right side end at the top, not 2 cm over it.
- The game's own left hands are not used on the submachine gun, shotgun or flamethrower (8.8 to 15.8 cm from the
  measured second grip): her finger hand holds those grips.
- vr313's side views show a scrap of her shirt behind the arm (the side camera sits by her faded shoulder); the eyes
  never see from there.

## Checks on the rig

See docs/HANDOFF-2026-11-26-SESSION27.md, "Checks on the rig" (vr302-vr324 are summarised there too).

## Release

Not made in session 26. See docs/HANDOFF-2026-11-26-SESSION27.md.

## Rig notes

- PC build: `cd /c/SH3Quest/SH3QuestVR && timeout 900 cmd //c "tools\\build_pc.bat" > /c/SH3Quest/build_pc_N.log 2>&1`.
- Sim: from /c/SH3Quest/gamedir with QUEST_SIM_HEADSET=1 QUEST_SIM_ALIGN=7312 QUEST_SIM_GRIP_TILT=50
  SH3_KEYS="22:Return,25:Return,28:Return,31:Return,34:Return", QUEST_SIM_SCRIPT and QUEST_SIM_OUT (mkdir first).
- Weapon numbers for SH3_EQUIP: 1 handgun, 2 shotgun, 3 submachine gun, 8 stun gun, 4 knife, 5 steel pipe, 6 katana,
  7 maul, 9 beam saber, 10 flamethrower.
- SH3_SECOND_TEST="frame:profile,dx,dy,dz,curl,thumb;..." tries the second hand's place and curls (second312.txt).
- New sim scripts: gate305, gate306, hands307, gaze308, gaze309, gate310, second312.
