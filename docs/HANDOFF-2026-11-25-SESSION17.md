# SH3 Quest VR: session 17 handoff (2026-11-25)

Picked up from docs/HANDOFF-2026-11-25-SESSION16.md "Part 2" (the owner: "was working in another session. pick up
from where it left off. miss/forget nothing. proceed with list"). Owner messages up to 83 (this pickup) are in
C:/SH3Quest/notes/owner-messages-sessions1-7.txt; no other this session.

PC builds 206-207 (build_pc_206.log, build_pc_207.log, BUILD OK); runs vr246-vr252.

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
- **0.1.2 PUBLISHED** (2026-11-25, a new release as the owner said: "just new release going forward"):
  - build_quest_7 (`tools/build_quest.py --release`, every native file compiled from this code, the same sources as
    PC build 207 but for a comment) gave android/build/SH3QuestVR-0.1.2.apk: versionCode 3, "0.1.2", 9,208,862 bytes,
    SHA-256 a480ee2d5c306a3e3afc3bd7d9370c489179cd0fe6a6f327f034c75e851def0b, the release key (certificate 38811d96...).
  - Static checks against 0.1.1: the same 17 files; changed only AndroidManifest.xml (versionCode/versionName lines
    alone), the four assets, classes.dex (the launcher's picture options) and libmain.so (29,744,528 bytes, the same
    NEEDED, no text relocations, SDL_main exported); libSDL2 and the OpenXR loader byte-identical; both verify with
    the same certificate. The one build warning (extractNativeLibs in the manifest) is 0.1.1's too.
  - main 78780ee (the 0.1.2 commit), annotated tag v0.1.2 (b5e2ac0) on it, release "Silent Hill 3 Quest VR 0.1.2"
    with docs/RELEASE-NOTES-0.1.2.md and the APK: not a draft, not a prerelease. releases/latest = v0.1.2, GitHub's
    digest sha256:a480ee2d... matches; v0.1.0 and v0.1.1 untouched. The launcher's updater asks releases/latest for
    "SH3QuestVR-" + the tag + ".apk" = SH3QuestVR-0.1.2.apk: the asset's name.
  - Verified on this code before publishing (build 207): vr247 the fog and the flare, the VR menu hold; vr248 the
    let-go; vr250 every gun's reload by hand; vr251 the death skip with no crash.
- GitHub: no issues; downloads (before 0.1.2) v0.1.1 4, v0.1.0 3.

## The floor items by hand (LIST 45's last part), vr249, vr251 and vr252

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

Why the scans never found a floor item (checked in the game's code after vr251): every run's tables read "floor items:
0" (17 of them), but that is read once, when a table loads. The game fills 0xBA9610 itself (0x5777F0, reached from
0x577FB0 by a table of functions: it clears the list with 0x4B6D60, then walks the items at [0x6F4D84], 18 bytes each,
until +0x10 is 0, and adds those 0x4B6DC0 passes through 0x4B6D80). That fill can run later than the table log, but the
event scans every 120 frames call the game's own floor-item check (0x4B74B0) all round her, and no run's scan ever found
one ("event scan: floor item" appears in no log). So in every place a run has stood (the nightmare's areas and the
mall's restroom) the game itself had no floor item to give: the session-15 finding stands, no code change.

vr252 (tools/sim/door252.txt, new: door228's death, then the walk to the restroom's window, events 10-12's region from
-59850 -19225, event 11 the live one, with the left hand ahead closing every 30 frames; Block off for that run only,
the defaults put back after; about 4 min): the entrance door by the left hand again ("the left hand used event 5 at
-13089 -616 23239", frame 8261), point 4 at 8486, and at 8783 "her health 0.0 after a hit: SH3_HP lets go". But this
time the game never started her death: she walked on at 0 of 100 to the end (the wrist's log: "health 0 of 100, in
play 1"), the dog beside her with no more hits, and no event scan ran after 8700. SH3_HP writes her health from
outside the game's hit (it holds 1%, then lets go), so whether the game's own death check sees the 0 depends on the
blow (vr251's did, vr252's did not). A player never has SH3_HP; the game's hit kills as it always did (vr251).
The pictures whole: the hall, the "CAUTION KEEP OUT" gate, the knife in the right hand, the left arm reaching ahead in
both eyes at 11000-12300 (the hand at the gate, the forearm foreshortened from the eyes as in vr242). No crash.

## Open, in order

1. The players' and the owner's reports on 0.1.0-0.1.2 (GitHub issues: none on 2026-11-25; the launcher's report:
   sh3-crash.log, sh3.log), first whether the launcher's updater offered and installed 0.1.1 / 0.1.2.
2. The headset-only items: docs/HEADSET_CHECKLIST.md (0.1.2's fog, lens flare, picture options and menu hold added).
3. LIST 45's floor items by hand: vr251 reached the mall (its restroom, no floor item there); vr252's death did not
   go on (SH3_HP). On the PC rig the death skip is a coin toss and the restroom's window is the next step
   (tools/sim/door252.txt, uncommitted, does both); a sure way is the headset ("Reach and grip at an item" in
   HEADSET_CHECKLIST.md) or a save in the mall, if the owner has one. This session's runs used about 25 of the 30
   minutes (vr246-vr252).

## Files changed in session 17 (with part 2's; all in the 0.1.2 commit 78780ee)

- src/d3d8/d3d8.c (the halo dump, testing; the flare branch; no_ztest in apply_render_states and the shadow pass),
  src/d3d8/d3d8.h (no_ztest), src/vr/sh3vr.cpp and sh3vr.h (sh3vr_depth_layer_map), src/vr/sh3vr_body.cpp,
  sh3vr_combat.cpp, sh3vr_internal.h (weapon_two_share).
- tools/sim/letgo.txt (new).
- docs: CONTROLS.md, GAME_FACTS.md, HEADSET_CHECKLIST.md, LIST-0.1.0.md, RELEASE-NOTES-0.1.2.md (new), this handoff;
  README.md, CREDITS.md; android: build.gradle, assets controls.txt, credits.txt, features.txt, whats_new.txt.

## Part 2: the owner's report on 0.1.2 (message 84) — the 0.1.3 list, NOTHING coded yet

The owner tried 0.1.2 on the headset (owner message 84, verbatim in C:/SH3Quest/notes/owner-messages-sessions1-7.txt),
then said "your weekly usage is almost up here, so wrap up and do a handoff" (85). Do this list in order, all in a NEW
release 0.1.3 (versionCode 4; the owner: "proceed with this in mind and do a new release"; publish it as 0.1.1 and
0.1.2 were, asking nothing). The rules stand: code it all, ONE PC build, one short combined check (each feature on
the exact build), then the release.

1. **Physical melee too sensitive**: "im just simply moving the knife arm gently/arm in general, and melee
   animation/sound happens". Raise what counts as a swing in sh3vr_combat.cpp (the swing detection: speed, the
   speed's rise, the distance the tip travels in the swing, a short time held, for every melee weapon, bare hands and
   a gun used as a club). A gentle move or a slow wave must never start the attack, its animation or its sound; a
   real swing still hits at contact (owner 65). Look at the swing log of an earlier melee run (vr204/vr233/vr235) for
   the speeds a real swing gives against the script's gentle moves, then set the thresholds with a margin.
2. **"Drifting fog" OFF by default**: sh3vr_internal.h line 76 (`drifting_fog = true`) and the launcher's
   Options.java line 142 (`sw("drifting_fog", ..., true, ...)`).
3. **Smooth turn ON by default**: `opt.turn` (sh3vr_internal.h; sh3vr.cpp line 162 clamps 0..1, line 1812 is the
   `turn == 0` branch: check which value is smooth) and its launcher entry in Options.java.
4. **"No screen filters or ANYTHING like that on by default"**, and the "weird fuzz over the screen" still there,
   wanted completely off by default:
   - "Smooth edges" (FXAA) OFF by default: sh3vr_internal.h line 84 (`smooth_edges = true`) and Options.java.
   - "Brightness", "Contrast", "Color saturation" stay 100% and "Darker edges" 0 (the picture pass off with those).
   - "Film grain" is already off (sh3vr_internal.h line 74): find what the fuzz still is. Candidates to check in a
     picture whole (look at dark walls, zoomed): the game's own noise drawn by another path than the one the switch
     stops, the drifting fog's far layer (off by default anyway after item 2), the dithered body fade, the picture
     pass or FXAA, dynamic resolution / AppSW on the Quest. Turn each off in a run and compare the same frame (lum.py
     on a dark patch: the noise's spread); whatever it is goes off by default, with a switch only if it is useful.
   - **0.1.2 players keep their saved options**: the VR menu writes sh3vr.ini on close and the launcher keeps its own
     settings, so changing the defaults alone does not reach the owner. Add a defaults version (in sh3vr.ini and in
     the launcher's preferences): when it is older than 0.1.3's, set these options (drifting fog, smooth turn, smooth
     edges, film grain, the picture values) to the new defaults once, then save. Say so in the release notes.
5. **Heather's body shadows "a bit overbearing", refine them**: LIST 56's shadow from her own mesh (sh3vr_ik.cpp
   her_shadow_tris, d3d8.c vr_her_shadow, the z-fail volumes). Look at a shadow picture (vr199/vr187/vr189) first: how
   dark and how large her shadow is against the game's own shadows. Likely: lighter (not the game's full darkening
   for her part), softer at the edges, and no shadow from the parts near the lamp (the hand holding the flashlight,
   the arm under the chest lamp) that throw huge shapes ahead. Keep the option "Heather's shadow" on.
6. Docs: whats_new.txt (0.1.3), features.txt, CONTROLS.md if anything changes, HEADSET_CHECKLIST.md (the melee's
   gentle moves, the defaults, the fuzz gone, the shadow), README v0.1.3, docs/RELEASE-NOTES-0.1.3.md (from 0.1.2's),
   build.gradle versionCode 4 / "0.1.3", AUDIT rows for owner 84-85, LIST 56 / melee item notes.
7. Release: re-check every change on the exact build, `tools/build_quest.py --release`, apkcheck.py against 0.1.2,
   commit, push, tag v0.1.3, `gh release create`, check releases/latest and the digest. Never change v0.1.0-v0.1.2.

Uncommitted now (left so, no commit asked): tools/sim/door252.txt; this handoff's vr252 and Part 2 sections; LIST 45's
vr252 lines; GAME_FACTS (the floor list's fill, the restroom). They go into the 0.1.3 commit.
