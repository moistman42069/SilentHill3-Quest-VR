# SH3 Quest VR: session 17 handoff (2026-11-25)

Picked up from docs/HANDOFF-2026-11-25-SESSION16.md "Part 2" (the owner: "was working in another session. pick up
from where it left off. miss/forget nothing. proceed with list"). Owner messages up to 82 are in
C:/SH3Quest/notes/owner-messages-sessions1-7.txt; this session had no new one.

PC builds 206-207 (build_pc_206.log, build_pc_207.log, BUILD OK); runs vr246-vr251.

## Done

- **vr245's pictures** (part 2, item 1): the fog on its far layer in both eyes and with the head turned 20 degrees,
  fixed in the world; the picture pass the right way up and the same as vr243 at 100% (an identity: the hand's skin
  within 3%). vr245 looked brighter than vr243 because of the fog's new place, not the pass.
- **The lamps' lens flare** (part 2, item 2), build 207:
  - What it is (vr246, SH3_HALO_DUMP=1, testing only): 0x468CDA draws the flare's streaks (rotated 128x128 quads round
    the lamp), 0x468F6E its rings along the line through the screen's middle, both at z 0.50003 (0.40 m in VR: a
    double image nearer than the wall). 0x468F6E also draws the lamp's own glow at the lamp's depth (z 0.90321 for a
    lamp 936 units off), which was right. The flare record at 0x898420 (filled by 0x4681F0 through 0x678450 from the
    light at 0x8983E0) holds the lamp's view depth at +0x18 and its view-space point at +0x40; through the projection
    at 0x72C460 that point lands exactly on the streaks' middle. GAME_FACTS.md has it all.
  - The fix: d3d8.c vr_route_pick sends those two draws at z 0.4999-0.5002 through sh3vr_depth_layer_map (sh3vr.cpp):
    each point's depth set to the lamp's (the game's clip w is the view depth), each eye's own view, and drawn over
    everything (dev->no_ztest: no z test, as on the flat screen where 0.40 m was in front of it all). The log says
    where ("the game's lens flare ... at its lamp, 3.42 m ahead ...", the first 4, then once a minute).
  - vr247 (fog.txt): the map's depth for the 936-unit lamp is 0.90321, the very z the game gave its glow. The flare's
    offset between the eyes is 208.2 px against the wall's 204 (two patches, correlation 0.91); with the head turned
    20 degrees 206.3-206.8 against 206; back in play 207.3 against 206. Before (vr245, vr246): 265.9, 62 px nearer.
- **The let-go blip** (session 13's leftover; build 206: the body's surface hold leaves the hand alone while it eases
  off the second grip, sh3vr_combat.cpp weapon_two_share): vr248 (tools/sim/letgo.txt, new: combined197's shotgun,
  both submachine guns twice and the handgun) took all six second grips at 9 cm and let them go with no "held at a
  surface" line (vr197 had two, frames 7504 and 7844). The pictures whole: both submachine guns forward, the left hand
  under the receiver's front, the let-go clean. The 5 cm the off hand moves at the ease's 12% (frame 7355 and the
  like) is the script snapping both hands home in one frame (27 cm), the same numbers as vr197: not in real use.
- **The reload on the release's code** (vr250, reload.txt on build 207, the switches of vr240): every gun by hand
  as in vr240: the handgun (the magazine seated, the slide worked; SH3_TEST_EMPTY_FIRE at 0 loaded answered 0 with
  the stack level, loaded 10 -> 9), the shotgun (two shells, 0 -> 1 -> 2 loaded, the pump worked), the submachine gun
  (the magazine seated, the knob worked), the stun gun (the battery seated, 3 loaded). The pictures match vr240's
  frame for frame (the background foggier: the fog's far layer).
- **Docs** (part 2, items 3-4): the menu hold and the new options in docs/CONTROLS.md, the launcher's controls.txt
  and features.txt, README (third person = the VR menu's switch; "The picture"; v0.1.2), whats_new.txt (0.1.2),
  HEADSET_CHECKLIST.md, LIST 14, LIST 45 (the far door by hand: DONE in vr242, event 48), LIST 50 (vr230/vr234: the
  circular hall's dog, whose kinds 120/121 GAME_FACTS' session-15 line had given to a Closer: marked corrected),
  credits.txt and CREDITS.md (the PC VR mod's fog layer and picture settings). The 0.1.0 and 0.1.1 release notes stay as they were (their versions).
- **0.1.2 prepared**: versionCode 3, "0.1.2"; docs/RELEASE-NOTES-0.1.2.md (the file's size and digest filled in at
  the build).
- GitHub: no issues; downloads v0.1.1 4, v0.1.0 3.

## The floor items by hand (LIST 45's last part), vr249 and vr251

door228.txt (the death skip to the mall) on build 207: the walk got stuck at the hall's start (15849 19377), the line
to point 4 into the railing, as vr236 (door228's header never had door231's point 0.9 m into the hall: added now,
16450 19300). The dog came only at frame 12097, and her knife guarded its hits (Block, on by default): at 1% health the
guarded 2.0 blow keeps 0.7 and she lived, so no death, no skip. No crash. The retry: the same with "blocking = 0" in
the game folder's sh3vr.ini for that run only (C:/SH3Quest/gamedir/sh3vr.ini.defaults-s17 is the defaults' copy,
put back after the run: the file matches it, "blocking = 1").

vr251 (that retry, about 3 min): points 1-4 reached (point 4 at frame 8312), stuck going for point 5 at 16858 17993
(the dog on her), and at frame 8940 "her health 0.0 after a hit: SH3_HP lets go (a death goes on)". The game went on
to the mall: at frame 10217 a new room table (0ECA0A50) with "floor items: 0 in the room, 0 not taken"; a scene at
10476; from frame 10658 she stood at -60600 0 -20625 in the mall's women's restroom (the red seal on the mirror) to
the end (12500). The pictures whole: the restroom in both eyes, both hands at the bottom; every other shot is the
pause menu's panel in the dark (the script presses the menu every 200 frames: Resume, Save, Load, Inventory, Quit),
as designed. No crash. So the death skip works with Block off; the mall's first floor items need a route out of the
restroom, which door228 has not got.

## Open, in order

1. The players' and the owner's reports on 0.1.0-0.1.2 (GitHub issues: none on 2026-11-25; the launcher's report:
   sh3-crash.log, sh3.log), first whether the launcher's updater offered and installed 0.1.1 / 0.1.2.
2. The headset-only items: docs/HEADSET_CHECKLIST.md (0.1.2's fog, lens flare, picture options and menu hold added).
3. LIST 45's floor items by hand: vr251 reached the mall (its restroom, 0 floor items); a route from there to
   the first floor items (door228.txt after the death), or the headset.

## Files changed in session 17 (with part 2's, all uncommitted until the release)

- src/d3d8/d3d8.c (the halo dump, testing; the flare branch; no_ztest in apply_render_states and the shadow pass),
  src/d3d8/d3d8.h (no_ztest), src/vr/sh3vr.cpp and sh3vr.h (sh3vr_depth_layer_map), src/vr/sh3vr_body.cpp,
  sh3vr_combat.cpp, sh3vr_internal.h (weapon_two_share).
- tools/sim/letgo.txt (new).
- docs: CONTROLS.md, GAME_FACTS.md, HEADSET_CHECKLIST.md, LIST-0.1.0.md, RELEASE-NOTES-0.1.2.md (new), this handoff;
  README.md, CREDITS.md; android: build.gradle, assets controls.txt, credits.txt, features.txt, whats_new.txt.
