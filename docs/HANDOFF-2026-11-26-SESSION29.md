# SH3 Quest VR handoff, session 29 (2026-11-26): 0.1.7

Repo C:/SH3Quest/SH3QuestVR. 0.1.6 = ba72cb1 (tag v0.1.6, versionCode 7). Session 28 (docs/HANDOFF-2026-11-26-SESSION28.md)
made PC build 242 (owner 140's fix: VR's walk and turn wait in her hit reactions) and stopped before running it.

## The owner's message (C:/SH3Quest/notes/owner-messages-sessions1-7.txt, 142)

"pick up from where it left off. miss/forget nothing. make no mistakes. this build needs to be perfect, with measures and
checks done to be sure that all will work smoothly for a full playthrough. dont take forever though, just be efficient
yet thorough while wrapping it up sooner than later. proceed with list"

## State (updated as the session goes)

- Build 242 was the release's code until the hints (below): build 244 is. C:/SH3Quest/checks242.sh (vr348-358); C:/SH3Quest/checks242b.sh queued after
  it: vr359 grab359 (new: the dog's grab with the stick held, vr347's stuck case, since vr348 had flinches only).
- vr348 spam347 (stabs spammed, the stick held, SH3_NO_KILL): three bites (kind 122 at 8737, 9096, 10227), each a
  reaction (movement 35, reaction 1) "over after 80 frames", walking and stabbing on (40 counted swings), the slashes at
  10520-10720 each starting the game's attack (action 7 -> 37); no grab this run (runs are not frame-exact). Pictures
  whole; vr347's j10620 showed her still held at the hall's start, vr348's has her walked on through the hall.
- vr349 spam346 (continuous stabs): nothing stuck; swings under the knife's 0.22 s cooldown logged and skipped; A,
  walking, the inventory as vr346's.
- Movement states checked in the exe: the table at 0x6D8ED4 gives 34 -> 0x4D9E60, 35 -> 0x4DAC30 (jmp 0x4D9E60),
  36 -> 0x4D55F0, whose step sets her walk factor +0x118/+0x11C to 0 every frame itself: all three are states the game
  never walks her in, so VR's wait there cannot hold back a normal walk.
- Docs written: RELEASE-NOTES-0.1.7.md (a bullet, the intro, the testers' line), whats_new.txt, README's summary,
  HEADSET_CHECKLIST.md (a new item), GAME_FACTS.md (the reaction states), C:/SH3Quest/commit_017.txt (the fix, the
  scripts, the switches). Draft reply for the PC VR mod's maker: C:/SH3Quest/notes/reply-pcvr-maker-0.1.7.txt (not
  posted; the owner sends it).

- vr350 elbow336: "kept clear ... 14 cm, now 23", the swing held to -19 of -38; all 20 pictures identical to vr337's
  (build 238, checked whole in session 28). vr351 anull344: both "found nothing" lines as vr344's, pictures identical.
- Release APK made during the checks (the game code unchanged since build 242; only whats_new.txt newer):
  tools/build_quest_13.log, android/build/SH3QuestVR-0.1.7.apk, 9,547,850 bytes, SHA-256
  1336dcbb99554e61a7bcc33247d1f3ad7c6906ad9270a670ce475b88997d9cde, the release key; apkcheck against 0.1.6: ALL CHECKS
  PASS (22 files, the manifest's version only, SDL2 and the OpenXR loader byte-identical). Copy at
  C:/SH3Quest/SH3QuestVR-0.1.7.apk. SUPERSEDED: the hints (below) changed the code; the APK is made again.
- vr352-358 read: every run left with code 0, no crash or error line; pictures against build 238's (339->354 ...
  343->358) the same but for the monsters' and the fog's randomness (the maul's angle, the gun smoke, Flat's drifting
  fog and camera spot), all checked whole. vr357 had no grab this run (the dog never caught her).
- vr359 grab359: the dog's grab at 8610 (late: runs are not frame-exact, the shakes came before it); during the hold
  VR's walk waited and the game's own break-free went on (0.70 to 0.41) until the run's end.

## Owner messages 143 and 144: the hints

"maybe add some hints similar to the reload ones that pop up and fade away when certain events happen for the first
time. like when being grabbed, when approaching a door, etc" and "be sure the hints are all under one toggle, and only
happen once. add a menu section in the in game vr menu with all the hints so people can look back at them".

- src/vr/sh3vr_tips.cpp (new, in CMakeLists' VR list): lines on the reload hint's label (sh3vr_reload.cpp
  reload_hint_texture asks tip_text() when no reload step is due; hint_is_tip; sh3vr.cpp hint_label_up). Kinds: start
  (two lines), flashlight, use, item, held (before any other line, ends when she is free: sh3vr_combat.cpp
  struggle_presses / struggle_done / the hold's end), gun, melee, health. Each once, ever: sh3quest_tips.ini in the
  game folder (start/flashlight/use/item/held/gun/melee/health and reload_stun_gun/handgun/shotgun/submachine_gun).
  The reload's steps through each gun's first reload (tip_reload_due / tip_reload_done). sh3.c sub_004B9940's mode-0
  block: sh3vr_tip_wants_use() every 8 frames while use/item is due and the label free, then action_check_at along
  her facing and the gaze +-30 deg (read only) -> sh3vr_tip_use(0 event, 1 floor item).
- One switch: Options.hints (key "hints"; "Reload hints" / reload_hints gone; an old sh3vr.ini's key is ignored). VR
  menu: a seventh page "Hints" (kPages' last): the switch, "Show every hint again" (tip_reset_all), every hint's line
  (tip_lines, the options as they are; size 18). Launcher: Options.java sw("hints", "Hints", VIEW, ...).
- Build 243 (vr360-363, C:/SH3Quest/checks243.sh): vr360 grab360 (new: grab359 with the shakes, stabs and stick pushes
  repeated 8600-9100): grabbed at 8290 with the stick held, the hint "Shake both controllers..." 22 frames after the
  hold, before the start's line it replaced, ended "she is free" at 8484; free after 2.5 s (5 shakes, 7 stick pushes),
  the reaction over after 275 frames, walking and the game's attacks after: owner 140 checked whole. The knife's lines
  came BEFORE the start's (the knife in hand from the first frame): fixed in build 244 (the flashlight's, a weapon's and
  low health's lines wait for the start's). vr361 reload: the start not again; each gun's reload steps once; the gun's
  lines after the handgun's reload step.
- Build 244 = the release's code: C:/SH3Quest/checks244.sh (vr364 grab360 from no file, vr365 use365 = the first
  gate's "Something to use here", vr366 reload, vr367 menu361 = the Hints page, vr368 Flat).
- vr364 grab360 (no tips file): the start's two lines first (7350, 7785); the grab's hint 22 frames into the hold
  (8251), ended "she is free" (8415); a second grab no hint; the flashlight's (8490), the melee's (8876); every reaction
  over; code 0. vr365 use365: "the hints' check: event 5 within her reach" (7992), "hint (use)" (8024), A used event 5
  (8069); the label whole before the gate, the hall after it. vr366 reload: the start not again; each gun's reload
  steps once ("not again" for the handgun, shotgun, submachine gun, stun gun); the gun's line after the handgun's
  reload, dropped when the shotgun came out ("another weapon, or none, in the hand"); every label and hand whole.
- vr367 menu361: the Hints page whole (the switch, "Show every hint again", every line to the list's end, none cut);
  A on "Show every hint again": "hints: every hint will show again", the note at the foot, the tips file all 0. The
  script then tapped the menu after B had closed it, which opened the game's pause screen, so play never lasted the 2 s
  the start's line waits for: tools/sim/menu361.txt has no tap now and runs to 8780. vr369 (C:/SH3Quest/checks244b.sh,
  from every hint shown): the same page, then the start's two lines again in play (8141, 8576), whole in both eyes.
  vr368 Flat (no tips file): no hint shown, no file written, the pictures whole. Every run left with code 0.
- Owner 145: "on by default, of course": it is (sh3vr_internal.h hints = true, Options.java sw(..., true, ...); an
  old sh3vr.ini has no hints key, so it starts on); the release notes say "on by default".
- Release APK from build 244's code (no source newer): tools/build_quest_14.log, 9,561,654 bytes, SHA-256
  82c01b5344242f756437c4d3c0c56da29a4090a99db5b44bc2bdd4f9329c0880, the release key; apkcheck against 0.1.6: ALL
  CHECKS PASS; libmain.so holds the build-244 line; copy at C:/SH3Quest/SH3QuestVR-0.1.7.apk.

## Next

1. Done: vr362-369 read, every picture whole; AUDIT rows 168/169; the release APK (above), @SIZE@/@SHA@ filled.
2. Release: commit (C:/SH3Quest/commit_017.txt), annotated tag v0.1.7, push main and the tag, gh release (without
   asking, owner 123), releases/latest's asset digest and a downloaded copy's SHA-256, the handoff commit, memory.
3. After the release: the players' reports on 0.1.7 (drafts in C:/SH3Quest/notes, never posted unasked).
