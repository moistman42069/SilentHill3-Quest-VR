# SH3 Quest VR: handoff, session 16 (2026-11-25)

Read docs/HANDOFF-2026-11-25-SESSION15.md first (its run commands and switches); this one carries on from it.
PC test builds 196-202 (build_pc_202.log, BUILD OK), regen before 198 (regen_198.log); runs vr230-vr240 (about 46
minutes of runs: vr237-vr240 the reload check the owner asked for). Releases: 0.1.0 and the patch 0.1.1 (below).

## The owner's messages this session (C:/SH3Quest/notes/owner-messages-sessions1-7.txt, 72-73)

- 72: pick up where session 15 stopped, miss/forget nothing, proceed with the list.
- 73: when done, publish the release on GitHub; no headset test by the owner first ("as long as you can guarantee
  itll all launch on quest"); thorough release notes with every needed detail; proceed with the list.
- 74: "i assume manual reload has been checked and verified too right?" (answered: by hand in the sim, sessions 4-13;
  not on a headset).
- 75: make sure reload is verified before publishing (came after v0.1.0 was up): vr237, reload.txt on build 199 (the
  release's code), below.
- 76: the owner handles Discord and the like; only git here (docs/DISCORD-ANNOUNCEMENT.txt left as it is).
- 77: people had already downloaded 0.1.0: release the fix as a quick patch (0.1.1), a first test of the updater.

## Reload verified, the shotgun's port fixed: 0.1.1 (owner 74-77)

- vr237 (reload.txt on build 199, 0.1.0's code): only the handgun reloaded; the shotgun's shells, the submachine
  gun's magazine and the stun gun's battery were let go 3-5 cm off their wells and dropped.
- Build 200 logged the places (the well minus the part's top, both in the gun's model space); vr238: the submachine
  gun's and stun gun's wells were right on their drawn models (the script's fixed hand places of session 4 were
  stale), but the shotgun's port (an aim-space offset) lay at its model's (-212.7, 47.0, -7.6): at the trigger
  guard's front, 5.4 cm under the receiver's floor, in the air. A REAL BUG in 0.1.0: a shell pushed into the visible
  port could seat early and was drawn going in under the gun.
- Build 201: the shotgun's port on its model (sh3vr_reload.cpp kGuns, GAME_FACTS "Shotgun"); the rig's
  "lh_at_reload 1/2/3" (xr_sim.cpp, sim_reload_spot in sh3vr_reload.cpp) places the off controller from the code's
  own places, and reload.txt uses it (with "lh_at_second 1" for the pump). vr239: all four guns reloaded by hand,
  pictures checked whole.
- Build 202: SH3_TEST_EMPTY_FIRE=1 (sh3.c sh3_vr_frame) calls 0x497800 through the guest stack; vr240: at 0 loaded
  the wrapper's way (answered 0, count kept 0) and loaded the game's way (10 -> 9), the stack level both times: the
  session-16 "no free ammo" pop fix verified. All four guns again.
- 0.1.1: versionCode 2, whats_new.txt and README, docs/RELEASE-NOTES-0.1.1.md; build_quest_6.log; the APK's checks as
  0.1.0's (only the version differs in the manifest; SDL2 and the loader byte-identical; libmain's needs the same, no
  text relocations, SDL_main; the same certificate 38811d96...; the updater's checks: digest, size, package, newer
  name, versionCode 2 > 1, the same signer).

## A crash fixed (would have crashed every headset at the first melee hit)

- vr231 (build 196) crashed at game frame 8361: "FATAL: indirect call to non-code address 00000000" from the message
  pump (sub_00403510 at 0x40367E: PeekMessage through esi), the game having just saved disp.ini; the same as vr167.
  Cause: sub_004ACE10 (sh3.c, the hit-volume wrapper) returned without the ret's pop of the return address when the
  swing had already hit at contact (vr == 2: the game's own attack's volumes dropped). The guest stack stayed 4 bytes
  off, the frame's pops gave esi/edi 0. It happens whenever a swing hits at contact and the game's attack then queues
  its volumes: every knife, pipe, katana or maul hit on a monster in real play. Fixed in build 197 (g_esp += 4).
- The same missing pop in sub_00497800 ("no free ammo": a gun fired at 0 loaded), never reached in a run; fixed.
- Every manual wrapper in sh3.c checked for it (44; the script: the scratchpad's audit_returns.py, the rule in
  GAME_FACTS "Combat"): no other.
- vr233 (build 198): the game's attack after a contact hit ran (the attack decision 5) and no crash; vr232-vr236 none.

## LIST 27, melee on a monster: DONE in the sim

- The knife's swings did meet the hall's monster (vr230: 85 contact hits) but no damage ever showed: Heather's hits on
  a monster are gathered by 0x4B6810, not 0x4B6A20 (the monsters' hits on her, where the "took N damage" log and a
  fist's half lived). Build 198 wraps 0x4B6810 (logged, a fist's halved; regen + the dispatch case).
- vr233: 25 knife hits of 30 (attack 14: damage 30, stun 5; her +0x18C x1, its +0x190 x1) took the monster from
  725.2 to below 0; vr235 and vr236: "the attacker 00898AC0 let go (health -1.0)" at frames 10021 and 9160 (killed).
  Each hit logged with its reaction and way ("took 30.0 damage from her (reaction 14) ... the way ...").
- The hall's attacker is the DOG, not a Closer: its model has 25 bones, 0.06-0.68 m off the floor (vr232's frame
  9000 bone dump: model 014D8050, world = the attacker + 0x20). At 0.7 m it is about 60 degrees under a level gaze,
  below the pictures' bottom edge (why no picture showed it). The monsters' kinds 120 (0.2 a frame) and 121 (2.0)
  are this dog's.
- GAME_FACTS "Combat": 0x4B6810 and the damage formula of 0x4B53D0 (the table at 0x6CE174), the wrappers' pop rule.

## LIST 45, the hall's far door (event 48): not yet by hand

- vr234 (door230.txt): the dog held her at the hall's start all run. vr235 (door231.txt, made by
  tools/sim/make_door231.py: the knife kills the dog first, then the route): every point to the door reached
  (24255 18143); the hand's palm (24645) stayed short of the door's trigger (the event scan: the game's own check
  finds event 48 standing at 24500; vr226's hand met it from 24323). vr236 (last point 24400 18170): stuck at the
  hall's start after the kill, the line to point 4 (16541 17832) runs into the railing.
- Next: add a point at the hall's start before point 4 (vr235 passed point 4 from 16892 18024 after following the
  dog), keep 24400 18170, run door231 again (about 4.5 minutes).

## The release (owner 73)

- Release key made: C:/SH3Quest/keys/release.jks (PKCS12, alias sh3questvr, RSA 4096, 10000 days) and
  release.properties (random passwords; never in the repository, never printed). tools/release-cert.sha256 holds its
  certificate's SHA-256. Keep C:/SH3Quest/keys safe: every later release must be signed with it (the launcher's
  updater and Android's installer check it).
- The APK: tools/build_quest.py --release (build_quest_5.log): android/build/SH3QuestVR-0.1.0.apk. Its size and
  SHA-256 are in docs/RELEASE-NOTES-0.1.0.md's "Files" and on the release page.
- Launch checks without a headset (the owner asked for a guarantee; this is everything that can be checked without
  one): the manifest's headset entries (com.oculus.intent.category.VR, IMMERSIVE_HMD, focusaware, headtracking,
  supportedDevices quest2|questpro|quest3|quest3s, the OpenXR permissions and queries) the same as Jak and Daxter
  Quest VR 0.1.9's, which runs on the Quest; the OpenXR loader byte-identical to Jak's; libmain.so, libSDL2.so and
  the loader need only libc, libm, libdl, liblog, libandroid, libEGL, libGLESv1_CM/v2/v3 and libOpenSLES (all on
  the Quest); SDL_main exported; no text relocations; xrInitializeLoaderKHR with the activity; the 4 GB guest
  reservation is MAP_NORESERVE. NOT checked: a run on a headset (none here).
- The repository: C:/SH3Quest/SH3QuestVR is now a git repository (main), pushed to
  github.com/moistman42069/SilentHill3-Quest-VR on top of its README commit; .gitignore keeps out /game (sh3.exe),
  src/recomp/gen, build/, .gradle/, keys, APKs and logs. PUBLISHED 2026-11-25: main a2435a8 (the source, 683 files)
  and b38e2c1 (the notes: the reload's reach still to be felt on a headset), tag v0.1.0 (annotated), the release
  github.com/moistman42069/SilentHill3-Quest-VR/releases/tag/v0.1.0 (not a draft or prerelease) with
  SH3QuestVR-0.1.0.apk (GitHub's digest sha256:3d974b4d... matches) and the notes; the GitHub API's releases/latest
  gives v0.1.0 and the asset name the launcher's updater looks for. The repository's local git config: user
  moistman42069 with GitHub's noreply address, core.autocrlf false (files stay LF).
  The first commit's message says "for the Meta Quest (standalone, OpenXR) and PC VR": the PC build is the test rig,
  not a PC VR release (left as it is: a force push to reword it is not worth it).
- docs/RELEASE-NOTES-0.1.0.md: what you need (the exe SHA-1s, the data by size), installing, everything in 0.1.0,
  controls, logs and reports, known for now, updating, credits, files.

## Open, in order

1. The players' and the owner's reports on 0.1.0 (GitHub issues, the launcher's report: sh3-crash.log, sh3.log).
2. LIST 45: the far door by hand (door231 with a point at the hall's start, above).
3. LIST 45: floor items by hand via the death skip (door228.txt, SH3_HP=1; the SH3_HP hold now lets go once a hit
   takes her to 0, so a death goes on).
4. The headset-only items (docs/HEADSET_CHECKLIST.md, AUDIT-2026-11-25.md).

## Files changed in session 16

- src/game/sh3.c: sub_004ACE10's vr == 2 return pops (the crash); sub_00497800's skip pops; sub_004B6810 wrapper
  (and its dispatch case); the records loop's damage-factor log (first 12); test code: SH3_HP lets go at 0,
  steer_at_attacker stands 0.5 m short (245 units, within 40), describes the attacker once, exports
  sh3_rig_attacker_at, always logs its let-go.
- src/vr/sh3vr_combat.cpp: the sweep log against the kept attacker (test, only with SH3_STEER_AT_ATTACKER).
- tools/sim: door229.txt header, make_door231.py and door231.txt.
- docs: LIST-0.1.0.md (27 done, 45, 40), GAME_FACTS.md (Combat), README.md (v0.1.0, download, this handoff),
  RELEASE-NOTES-0.1.0.md, this handoff; HANDOFF-2026-11-24-SESSIONS5-7.md and SESSION8.md (paths made %USERPROFILE%).
- .gitignore: .gradle/, local.properties, *.jks, *.keystore, release.properties, *.apk, *.log.
- For 0.1.1: src/vr/sh3vr_reload.cpp (the shotgun's port on its model, the place log, sim_reload_spot);
  src/vr/xr_sim.cpp ("lh_at_reload"); src/game/sh3.c (SH3_TEST_EMPTY_FIRE, test only); tools/sim/reload.txt (the
  places from the code); android/app/build.gradle (versionCode 2, 0.1.1); android/app/src/main/assets/whats_new.txt;
  README.md; docs/RELEASE-NOTES-0.1.1.md; docs/GAME_FACTS.md (the shotgun's frame and port); LIST 40 and 46.

Waiting for the owner's go (files on C:, never deleted without it): as session 15 lists, plus vr230-vr236's logs and
shots, build_pc_196-199.log, regen_198.log, build_quest_5.log.

## The owner's rules

As in SESSION12.md's last section, with owner 73: publishing 0.1.0 was asked for; later releases only when asked.
