# SH3 Quest VR handoff, session 22 (2026-11-26): 0.1.5 finished on the rig, the APK next

Repo C:/SH3Quest/SH3QuestVR at 147792d (0.1.4 = 50047a1, tag v0.1.4), everything since **uncommitted**. Read
HANDOFF-2026-11-26-SESSION21.md for sessions 20-21's work; this one picks up from its list.

## The owner's messages this session (saved in C:/SH3Quest/notes/owner-messages-sessions1-7.txt)

- 106: "be sure game visuals are perfect for this build, and that it has no crash/black screen risks ... no
  choppyness, ghosting, lags, etc with the default settings."
- 107: "ANY fixes you do, check the pcvr version to see if it already had a plan of action ... these things shouldnt
  be taking so long."
- 108: "make sure all guns are good."
- 109: "muzzle flashes look good on the other guns, double check pistol and allat. just wanna be sure muzzle flash is
  actually at where it should be lol."
- 110: "be sure all inputs work good and are mapped properly/accordingly. doesnt silent hill 3 have a stomp? make sure
  thats mapped. i dont see it in the git release controls section. also, be sure theres nothing else missed in this
  regard and others."

## Done after owner 109-110 (PC build 223)

- **The pistol's flash** (sh3vr_combat.cpp):
  - The handgun's shot (gun_ray, so the game's flash and the guide dot) leaves from bone 1's front,
    {137.0, 38.1, -13.1} (`muzzle_point`): the slide (bone 1, posed at 139.6 38.1 -13.1 in bone 0's frame, vr188)
    reaches x 137; from the hold's tip (bone 0's end, x 101.4) the flash sat 7 cm back inside the slide. The PC VR
    mod's flash point is the same (the damage ray's muzzle).
  - Build 223 moved kHolds[0]'s tip itself there: the wall probe (sh3vr_body.cpp weapon_contact, depth samples palm
    to tip) then met the gun's own front face, "handgun held at a surface" at the start, and the gun was drawn held
    back (low, pulled toward the hand) for the whole of vr278 and vr281. Build 224 keeps the tip at 101.4 (the reach:
    the wall probe, the club's capsule) and gives only gun_ray the muzzle. Checked in vr282 / vr283.
  - The flash's quads follow the shown recoil: the game lays them on the shot's line (gun_ray, no kick), the drawn
    gun is turned up by the kick about the fist. combat_shot keeps the shot's line in the world
    (shot_from_w / shot_dir_w); combat_effect_rel turns the effects on that line (within 1.2 cm, -10..+50 cm along)
    by wf.turn * conj(wf.aim_turn) about world_point(wf.base). Effects placed later from the drawn gun (0x4954E0,
    the kick already in) are off the line and left alone.
  - vr278 (build 223, guns277.txt, SH3_GIVE 13:50): the handgun's flash starts at the slide's front along the kicked
    barrel, one image in both eyes (w7422); the shotgun's centred on its muzzle (w7664); SMG, unlimited SMG,
    flamethrower, stun gun as in vr277. The effect log reads "placed at".
- **The unlimited SMG's "no shot" in vr277** was the test's give: 13:0 is 0 loaded (item 0x0D, never decremented by
  0x497800, so a real one keeps its rounds); with 13:50 it fires (vr278 w8624-w8672).
- **The stomp** (owner 110): the game's own move, read in the code (docs/GAME_FACTS.md "The stomp"): 0x4B43D0 needs
  the action press (0x89E8AB, logical mask 0x200 = A) with the ready button (0x89E8AF, mask 0x80; the attack's test
  0x4B4340 needs it held) released, and a downed target [0x712C5B4]. VR's A is the game's Action, the main trigger
  its ready, so A with the trigger released by a downed monster stomps (or kicks), both views; no code needed. Not
  seen on the rig (no run downs a monster); a HEADSET_CHECKLIST row asks for it.
  - Added to docs/CONTROLS.md (row and paragraph), assets/controls.txt (the launcher's Controls), the notes'
    Controls table and What's new, whats_new.txt and its draft, the checklist, GAME_FACTS, AUDIT 132-134.

## Done this session (PC builds 218-222)

- **The game's effects through the game's own camera** (sh3vr.cpp `sh3vr_effect_map`, d3d8.c, d3d8_shader.c):
  - The game projects its screen-space effects with its own camera (0x743C34 via 0x678070 / 0x43AD50), not the
    head's; each is taken back through that camera (`sh3_game_view`) to its place in the world.
  - Small draws (under 0.75 m across) are turned to face the head at their place (P T P^-1).
  - Faded over the outer quarter of the game camera's picture (`rhw_fade`, the shader's `uRhwFade`); a draw that
    would not show at all is skipped.
  - In first person, the effects over 2.1 m above her feet that are not the gun's (the lamps' glows) are left out,
    as the PC VR mod puts no screen-space sprite in its eyes. At the game's camera they sit on their lamps.
  - Testing switch: `SH3_FX_LOG=A:B` logs every mapped effect in frames A..B.
- **The gun's effects** (sh3vr_combat.cpp `combat_effect_rel`): vr276 showed the game already places them at the gun
  in the hand (0x4954E0 moved by VR; the flash's quads along the hand's barrel from the shot's origin to the unit).
  They are kept at their place (no second move: build 221's full weapon map put them 33 cm off), drawn whole, facing
  the head. The PC VR mod puts its own flash at the same point. The old anchor code was removed.
- **Black-screen guard** (xr_openxr.cpp): each chain is filled at most once a frame (`filled_frame`,
  `m_frame_serial`), so nothing can acquire or wait twice in one frame.
- **Owner 106 review**: depth of field and the double picture were already out (all modes 2-7); the pose submitted is
  the one drawn with; 120 Hz kept; the D3DX natives are bit-exact.
- **Docs**: docs/RELEASE-NOTES-0.1.5.md (APK_SIZE / APK_SHA256 to fill), whats_new.txt (0.1.5 first),
  features.txt, README, HEADSET_CHECKLIST (0.1.5 rows), GAME_FACTS (sessions 20-22 section), AUDIT rows 118-131.
- **versionCode 6, versionName "0.1.5"** (android/app/build.gradle).
- **Reply for the player** drafted: C:/SH3Quest/notes/reply-player-report-0.1.5.txt (post after the release).
- tools/build_quest.py's wrapped list has sub_00648432/491/56F, sub_0049FAC0, sub_0041E200, sub_0041FF80 _gen.

## Runs

- vr270-vr271: the lamps and the head's turns (build 218-219).
- vr272-vr275: the flash shots (the shot fires about 20 frames after the pull; script and log frames differ by 26-54).
- vr276 (build 222): the flash at the muzzle (fire275.txt).
- vr277 (build 222): every gun fired (tools/sim/guns277.txt).
- vr278 (build 223): every gun again, the unlimited SMG given 50 rounds (13:50): the pistol's flash at the slide's
  front, following the kick.

## Release 0.1.5's APK

- `source /c/RACQuest/env.sh; C:/SH3Quest/venv/Scripts/python.exe -I tools/build_quest.py --release`
  (tools/build_quest_12.log; every native file compiled from the same sources as PC build 224; build_quest_11 was build 223's, superseded).
- android/build/SH3QuestVR-0.1.5.apk (copy: C:/SH3Quest/SH3QuestVR-0.1.5.apk), versionCode 6, "0.1.5",
  9,516,546 bytes, SHA-256 fb3d8873c62dd354e2b6b4c8d30ea8ab81353b1d1176a275eab7231c30e4704d, the release key
  (certificate 38811d96...). Filled into docs/RELEASE-NOTES-0.1.5.md.
- tools/apkcheck.py against 0.1.4 says "A CHECK FAILED" only for its positional manifest compare: 0.1.5 adds two
  meta-data entries, which shift every later line. A real diff (scratch manifest_diff.py: difflib over aapt2's
  xmltree, line numbers stripped) shows exactly the intended changes: versionCode 6 / "0.1.5", the icon
  (ic_launcher_sh3), com.oculus.trade_cpu_for_gpu_amount -1 and com.oculus.dualcorecpuset true. Every other check
  passes: 22 files (5 new: the icon's PNGs), SDL2 and the OpenXR loader byte-identical, libmain.so 29,797,448 bytes
  with the same NEEDED, no text relocations, SDL_main exported, the same certificate verified.
- Release re-check runs on build 223: vr278 (every gun), vr279 (check265: the scene's switch to her eyes, the D3DX
  check, both sticks, pictures in play), vr280 (check271: the lamps and effects), vr281 (reload.txt: every gun
  reloaded by hand).
- Build 224 (the APK's code; only gun_ray's muzzle differs from 223): vr282 (every gun: the pistol whole in the hand,
  its flash at the slide's front in both eyes, no "held at a surface"; the other guns' effects as in vr278) and vr283
  (reload.txt: every gun reloaded by hand, the pistol and the off hand on its slide in view as in vr250). vr279's
  scene, sticks and D3DX items and vr280's lamps do not touch the changed code. No slow frame, no error in any run.

## Next

1. Hand over the APK path and SHA-256; **ask the owner before publishing**.
2. On their yes: commit, tag v0.1.5, push, the GitHub release with docs/RELEASE-NOTES-0.1.5.md and the APK; then the
   player's reply (C:/SH3Quest/notes/reply-player-report-0.1.5.txt).
