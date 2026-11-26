# Silent Hill 3 Quest VR: the 0.1.0 list (the whole first release)

The owner's bar (2026-11-24): standalone Quest 3, set up like our other mods (Ratchet, Jak): launcher, full-body IK,
proper world collision, Half-Life: Alyx finger bending against the world, grabbing world objects with fingers wrapping
them, our other mods' features where they fit (performance options...), 6DoF, proper UI, the PC VR mod's features taken
and refined; weapons all working as well as possible with proper two-hand grips and accurate aiming that respects every
combat mechanic of the game; every puzzle looked at for a VR implementation; a proper 1st/3rd person toggle mid-game;
every input mapped, none missing, no conflicts; "make this first release as polished as possible".

Base: sh3-nx (cutarev; built on directly, by the owner's decision), the PC VR mod (NotGodlikeUwU), our Ratchet/Jak VR
layer and launcher. Everyone credited in the release.

Rules: everything here ships in 0.1.0. PC test builds run as we go (the game cannot be checked here otherwise); the
APK is built once, after every item is done. Every crash or close leaves a detailed record in the game's own log.
Options named as players say them. Every VR feature on by default (owner 2026-11-24: "all vr features enabled by
default"), each one with its switch; the launcher's performance options default to the best ones for the Quest 3
(item 7: the recommended set is the default, not something to press).

Marks: [ ] open, [~] started, [x] done (with how it was checked), [?] needs the owner on the headset.

## A. The game running on the Quest
1. [~] Recompile pipeline on this PC: sh3-nx's regen (xboxrecomp) from the owner's exe, with our venv. Find the
   bytes where the owner's exe (SHA-1 d851669e...) differs from sh3-nx's (aa2aca6d...) and accept both.
   2026-11-24: regen on the owner's exe: 11977/11981 functions, 0 failed; the build runs it to the title, New Game
   and the opening. The differing bytes need sh3-nx's exe to find (we only have its hash); the launcher's file
   check must accept both hashes (and say which one it found).
2. [~] PC test build (Windows, clang, desktop GL, SDL2): a Windows host layer (threads, 4 GB guest window, crash
   handler, backtrace). Boots to the title, New Game, the opening; scripted keys and frame dumps for checks.
   2026-11-24 (checked: SH3_KEYS run, SH3_SHOT frames): title at 20 s, New Game, the opening cutscene with
   subtitles, steady 60 fps on the RTX A4500. Windows fixes: 64-bit stat/off_t (GetFileSize's high half), the
   program cache's append header (MSVC ftell), the bridge table ends, COFF .incbin, windows.h kept out of gl.h.
   Left: the crash log starts fresh each run; a game folder outside Program Files (C:\SH3Quest\gamedir, data
   linked) for writes.
3. [~] Android build (NDK 27.2, arm64): the Android host layer (the Linux one plus logcat, unwinding crash
   reports), the renderer on GLES 3.2 (shader versions, BGRA, S3TC decoded or transcoded, border and mirror-once
   clamps, no polygon mode), sound and threads.
   Session 15: COMPILED for the first time with no compile errors; the one link error (the bridges' section dropped by
   lld's start-stop-gc) fixed with -Wl,-z,nostart-stop-gc. Not yet run on a headset.
4. [~] Game files on the headset: the launcher imports the PC game folder (sh3.exe + data, ~5 GB) from a folder or a
   zip anywhere the Quest can reach (Downloads...), checks sh3.exe and every data file, and keeps saves apart.
5. [~] The movies (data/movie/NAME.000; sh3-nx skips them): shown on a big screen in VR with sound. Session 11 (build
   152): the scrambling solved from the exe (a key block and an XOR table, GAME_FACTS.md Movies), MPEG-1 video and
   Layer II sound decoded with pl_mpeg (MIT, downloaded with the owner's permission), the game's player replaced
   (0x404B40 start, 0x404440 step, 0x403DC0 stop), the picture on the cinema screen, the sound through the movie
   soundtrack at the game's music level, the game's own skip. Checked off the game (op 30 s: 900 pictures, 30.02 s of
   sound; the picture whole); still to see in the game: tools/sim/movie.txt with SH3_MOVIES=1.
6. [~] Speed on the Quest: the game at a steady rate with headroom (the frame rate fix for 60 game frames if it holds;
   see B10), the draw path cheap (uniform uploads by change only, the program cache warmed).

## B. VR core
7. [~] OpenXR on the Quest from Ratchet/Jak (xr_openxr.cpp): refresh rate, render resolution, foveation, "Smoother
   frames (AppSW)", "CPU and GPU boost", performance stats in the log; xr_sim on PC for checks here. A fresh install
   starts on the best set for the Quest 3, measured on the PC rig's numbers and the headset's (refresh rate, render
   scale, foveation level, AppSW, boost): the launcher's "Recommended performance options" is the default.
8. [~] Stereo: both eyes in one pass (multiview), each eye's view put right in the vertex stage from the game's own
   projection; the game's render targets doubled per eye where the scene goes through them (post effects, noise,
   reflections, shadows); the game's view widened so nothing is culled at the edges of the eyes.
9. [~] 6DoF first person: the eyes at Heather's head, head rotation and position tracked; walking in the room moves
   Heather (the game's own movement, so walls hold); height matched to Heather (the PC mod's 165 cm idea) with a
   "Player height" option; world scale right (doors, cars, rooms feel life size).
10. [~] Frame pacing: game logic rate vs display rate settled (60 game frames if the game takes it cleanly; else 30
    with the head always tracked at the display rate); no judder in head motion.
11. [~] HUD, menus, inventory, map, memo, save screen, options, puzzles' 2D screens: on a clean floating panel in front
    of you (follows the view gently, recenters); subtitles readable, placed in view; fades and the noise effect handled
    right in VR (no head-locked smear).
12. [~] The pointer, usable anywhere the game shows a screen to choose from (owner 2026-11-24: "a proper pointer for
    menus, in game menus, etc... basically use it anywhere as needed in the game... inventory too as best as
    possible", like our other VR work but better and Silent Hill 3 themed): title and main menu, options (every page,
    sliders dragged), pause menu, the inventory (its item ring: point at an item to turn the ring to it, trigger to
    use/equip/examine/combine; ammo and weapon pages), supplies, the map (point to move, trigger to zoom, grip-drag
    to scroll), memos (point at a memo; flip pages), save and load screens, game over and the results screen,
    puzzle screens (keypads, the clock, book order, every one in docs/PUZZLES.md), our VR menu and the launcher.
    How: the game's own cursor follows the beam (hover moves the selection, trigger confirms, B or the other grip goes
    back), through the PC game's own mouse support where a screen has it and our own hit-testing of what the screen
    draws where it does not; the thumbstick and buttons keep working alongside. Better than Ratchet/Jak: the dot
    settles on entries (light snapping, tremor smoothing on small targets), either hand points (the last trigger
    pulled picks it), a haptic tick per entry, the beam hidden while no screen is up. Themed: a thin rust-red beam
    fading along its length, a soft dot with a faint static flicker like the game's noise, the hovered entry lit the
    way the game lights its own cursor. VR menu: Pointer on/off, Pointer hand (either / left / right), Beam look.
13. [~] Cutscenes: real-time cutscenes shown the way the player chooses ("Cutscenes: first person / cinema screen /
    in the scene at the camera"); VR cutscenes ON by default (owner 2026-11-24: "make sure vr cutscenes are on by
    default too"): real-time cutscenes play in VR, in stereo with head tracking, at the scene's own camera (the
    cinema screen is the switch's other setting, not the default); movies on the screen (A5).
14. [~] 1st/3rd person toggle mid-game: the VR menu's "First person" switch (owner 80, session 16: the left menu
    button held ~1 s is now the VR menu everywhere, the game paused under it in play; a short press stays the pause);
    third person = the game's own camera in stereo with head look, comfortable placement; the switch keeps
    aim and movement right in both.
15. [~] Every input mapped, none missing, no conflicts: action, confirm, cancel, attack, ready weapon, flashlight,
    dash, strafe, map, map zoom, inventory, supplies, search view, pause, skip, menus' directions, recenter, the VR
    menu. Written out as a table (docs/CONTROLS.md) and checked against the game's 25 pad entries and its keyboard.
16. [~] Turning and comfort: snap turn (default) and smooth turn, movement by head or hand direction, optional
    vignette when moving, seated mode, recenter.
17. [~] In-game VR menu with every VR switch (from the pause menu and a button), and the same settings in the launcher.
18. [~] The launcher (Ratchet's): Play, game-file import and check, Options (Graphics, Performance, VR, Controls,
    Comfort, Game), "Recommended performance options", What's new, the updater from GitHub releases, logs and a report
    to send, About with every credit.

## C. Body, hands, world
19. [~] Full-body IK on Heather's own model: head on the headset, hands on the controllers, arms solved (elbows
    natural), body turns and steps smoothly, legs follow walking; head hidden from the eyes only; her shadow and
    reflections show the body.
    Session 14 (owners 62-64, "the whole body ... like the ratchet and clank shadow"): her shadow from her own mesh,
    not capsules. Each skinned draw of hers and the weapon in her hand is captured (48-byte vertices skinned on the CPU
    exactly as the shader does: weights at +12/+16/+20, the fourth 1 - their sum, bone bytes at +24, rows c48 on),
    welded; its faces lit by the chest lamp and the silhouette edges make closed z-fail volumes (6 m, inside the
    game's 20.6 m far plane), her pixels kept out. vr199: her arms, hands, the gun and the beam saber whole and solid
    on the floor and the wall (4516 triangles, 1119 body faces and 208 weapon faces lit); the capsules only as a
    fallback when no draw of hers was caught.
20. [~] Hands: Heather's hands on the controllers, fingers from the controllers' touch sensors (point, fist, thumbs);
    finger bending against the world like Half-Life: Alyx (each finger stops on walls, tables, doors, smoothly).
21. [~] World collision for hands and head: hands stay out of walls (ghost hand shown faintly when the real one goes
    through), head in a wall fades the view; uses the game's own collision.
22. [~] Grabbing: items in the world (ammo, health drinks, ampoules, keys, puzzle items, weapons) are picked up with
    the hand, fingers wrapping the object's shape; the game's own pick-up runs when it is taken; doors and handles
    where the game allows; let go = back where it was or into the inventory by the body.
23. [~] Haptics: shots, hits given and taken (the PC mod's damage pulse), melee impacts, grabbing, touching the world,
    the radio's static, heartbeat at low health (optional).

## D. Weapons and combat
24. [~] Every weapon in the hand at the right size and grip: handgun, shotgun, submachine gun, steel pipe, knife,
    maul, katana, stun gun, and the extras (unlimited submachine gun, flamethrower, beam saber, Heather beam...);
    per-weapon calibration (the PC mod's sh3vr_weapons.ini values as starting points).
    Session 13: the extras (weapons 9 beam saber, 10 flamethrower, 13 gold pipe, 14 silver pipe, 17 unlimited
    submachine gun: items 5-8 and 13) had no hold and hung in her ANIMATED hand. Build 165: their model's bones, as
    the game posed them (sh3.c's 0x45EC00 wrapper, a 1-4 bone model while a weapon is out), carried from her animated
    hand bone to her IK hand at the controller (sh3vr_ik.cpp ik_hand_carry, sh3vr_combat.cpp combat_weapon_bones);
    holstered, collapsed. vr192 (tools/sim/extras.txt): all five in her hand at the controller, but held as her idle
    hangs them: the pipes' and the beam saber's long part out past the little finger, the unlimited submachine gun
    across her body. So each gets a real hold: build 166 holds weapon 17 (18) as the submachine gun (the same model,
    557/890, logged in vr192: its hold, shape, two-hand grip and the game's own gripping hands; its reload stays the
    game's, item 13 has no hand reload) and dumps each extra's own draws (SH3_MESH_DUMP=W) for the others' holds: the
    pipes' 170/290 against the steel pipe's 172/294, the flamethrower's 238/434 + 49/106, the beam saber's.
    Builds 167-170 (vr194-vr196): every extra has its own hold now. The gold and silver pipes are the steel pipe's
    model (170/290, a second signature on its row) and are held as it (both hands on the shaft). The beam saber
    (profile 8: its hilt 31 cm along (0.368, 0.921, 0.114), the beam on 82 cm from the fist) takes the game's own right
    hand (her group 34, read in vr196) and a second hand stacked under it as on the katana. The flamethrower (profile
    9: barrel +x, its pistol grip, the tank under the front) takes the game's own right hand (group 37); the game's
    left hand (38) sits 10.6 cm off the front grip, so her finger hand holds it there, palm up (vr196: "the palm
    toward -0.01 1.00 0.00", the barrel straight ahead). The game's fist centre is (24.4, -1.1, -14.3) in weapon bone 0
    for every weapon (the weapon is posed against her hand bone). weapon_name names 9, 10, 13 and 14 by their slot.
    vr196's pictures: all five in her hand at the controller, held as used, both hands where each takes two.
    Session 14 (owner 61, the beam saber end to end): vr199 lit the beam with the trigger (the game's ready: action
    7, 0x712C66B = 1) but drew it where her ANIMATED hand holds the saber, across the view by her hip; vr202 (the
    trigger 12 frames after the draw) got action 37 and no beam from the attack held with the ready. Build 175: with
    the saber in the VR hand (physical melee on) the trigger is the ready alone, the swings attack; the beam (vr203:
    the game's own screen-space strip, 8 primitives, a 64x64 texture, added light) moves rigidly with the saber to
    the VR hilt (the effect map with the weapon's own map, only small-textured draws along the game's saber line:
    not the fog's quads, not blood); its swings hit at contact with its attack 47 (item 27). vr204-205: the glows
    moved, but the strip still runs sideways from the hilt (not along it). The cause is not known yet: the
    candidate log's counter was used up by fog quads. NOT DONE: the vertex log SH3_BEAM_VERTS and the fix are planned
    in HANDOFF-2026-11-25-SESSION14.md's vr205 section.
    Session 15 (builds 177-190, vr206-vr219): DONE in the sim. The green bar was the saber's BLADE, a skinned draw of
    its own (24 vertices, 30 indices, one palette slot: model bone 1, its x row 0 while off, stretched as the beam
    extends), drawn just before the hilt (126/154) and without a signature, so it stayed at her animated hand by her
    hip. sh3vr_combat.cpp saber_blade_draw moves it with the hilt's map (this frame's hold, its stretch kept;
    collapsed when holstered; a glow, so kept out of her own-mesh shadow, build 191). vr217: the 19,496-pixel bar
    gone, the blade straight out of the hilt in the hand at
    ignition, held up and across (w07570, w07660, w07690), off at w07720. The screen-space sprites moved before were
    the scene lights' halos, now left alone (combat_effect_rel's beam branch removed). The saber's own green point
    light (c17, 0.40 0.60 0.20, on the blade in her animated hand) is moved to the blade in the hand too
    (sh3vr_beam_light, vr218-219; her flashlight's white c16 stays). The ignition glow meshes (vs F0000007, 6
    vertices) take the same map (sh3vr_beam_mesh). The headset checks the look of the swing.
25. [~] Accurate aiming: shots go where the barrel points (the game's aim vector set from the gun), sights usable,
    optional laser/guide dot, the game's auto-aim off in first person by default (option); damage, ammo, reload and
    rates of fire stay the game's.
26. [~] Two-hand grips: shotgun, submachine guns, maul, katana, steel pipe (and the handgun's support hand): the second
    hand steadies and aims, both hands shown on the weapon.
27. [x] Melee: real swings hit (the weapon's actual path against enemies, the PC mod's speed/travel thresholds
    refined), the game's light and strong attacks, stomping downed enemies, guarding; knife easier as the PC mod did.
    Physical melee with everything (owner 2026-11-24): every melee weapon (knife, steel pipe, katana, maul, beam saber,
    stun gun's touch) at full damage; bare hands (punches with either fist, nothing equipped or the off hand) at half
    the weakest melee hit's damage; any gun swung as a club at full damage (the pistol-whip/butt-stroke, the gun's own
    hit volume along its length). Hits come from real contact speed and direction (the game's clBattleAddQue volumes
    from our hand/weapon path), with the game's knock-backs and stuns, haptics and the game's hit sounds; a lazy
    touch does nothing. Each one checked against every enemy kind.
    Session 14 (owner 65: "proper directional hit reactions based on when and where hand/weapon/melee weapon makes
    impact"), build 175: hits at contact, not at the game's animation frame. While a swing is fast (60% of its
    speed, a third of its travel) and has not hit, each frame the weapon's or fist's sweep since the frame before
    (grip and tip, lengthened by its radius) goes to the game's battle step as that weapon's own attack (its id
    learned from the game's queue; a fist the knife's, a gun as a club the pipe's): the game's own test (0x4B3260,
    0x4B29C0) finds whom it meets that very frame and its damage, knock-back, blood and sound follow, and the way
    the game gives the hit's reaction is the tip's motion that frame (0x4B37AD: cur tip - prev tip): the reaction
    goes the way the weapon went where it met. The game's hit records (0x4AEA90, kind 4: the character, the point,
    the way, the attack, the attacker) tell which sweep hit: one hit a swing, a knock in that hand (hardest for a
    heavy weapon), a fist's thud; a melee weapon's game attack is pressed then if it was not yet (her swing, its
    sound, her stamina) and its own volumes are dropped. A fist's hit keeps half its damage (0x4B6A20 for the
    monster: every punch that lands reacts). The stun gun's touch (kind 3) stays the game's attack on the weapon's
    path. Each hit on a monster is logged with its damage, reaction and way ("took N damage from her").
    Session 15 (vr217-219): the beam saber's swings sent their sweeps (4, 3 and 2 a swing, the hand's travel 0.59,
    0.32 and 0.16 m) and met no one (no monster in the opening); its haptics fired. The reactions' way against a
    monster waits for the playthrough or the headset.
    Later in session 15: no run has yet logged a hit on a monster ("took N damage from her"). vr226: the knife swung
    across in front of her while the hall's dog bit from her left (met no one); vr227: circles round her at 3-frame
    steps never ended as swings (the smoothed speed stayed up; nothing logged) and the Closer's body stood past the
    knife's reach. Build 195: SH3_STEER_AT_ATTACKER=1 stands her 0.7 m before the monster that hit her; tools/sim/
    door229.txt (swings every 10 frames, chest-high and low) is the next run (HANDOFF-2026-11-25-SESSION15.md).
    Session 16 (builds 196-198, runs vr230-vr233): DONE in the sim. The knife's swings met the hall's monster at
    contact (vr230: 85 times) but no damage was ever logged: Heather's hits on a monster are gathered by 0x4B6810, not
    0x4B6A20 (the monsters' hits on her), so the "took N damage" log and the fist's half never ran. Build 198 wraps
    0x4B6810: vr233, 25 knife hits of 30 (the knife's attack 14: 30 damage, 5 stun; her x1, its x1) took the monster
    from 725.2 to below 0 (it died at frame 9817; the steer let it go and walked on), each with its reaction and way
    logged. The hall's attacker is the DOG, not a Closer (a low 25-bone model, 0.06-0.68 m off the floor, under the
    bottom of a level view at 0.7 m). And a CRASH fixed (vr231): a swing that hit at contact dropped the game's own
    attack's volumes in sub_004ACE10 without the ret's pop: the stack 4 bytes off, the frame's pops gave esi/edi 0 and
    the message pump called address 0 ("indirect call to non-code address 00000000", the save of disp.ini first).
    Every hit at contact followed by the game's attack would have crashed on the headset. The same missing pop fixed
    in the "no free ammo" wrapper (sub_00497800: a gun fired at 0 loaded); every other wrapper checked (44).
    0.1.3 (owner 84: "physical melee seems to sensitive. im just simply moving the knife arm gently/arm in
    general"; owner 89: "melee should be fixed for eveyrthing, not just knife ofc"): 0.1.2 read the hands'
    speeds on the headset's frames while the game made half as many (a gentle move read twice as fast). Now the
    real time between frames (motion_dt), a swing's travel only above 60% of its speed, the speed held 25 ms, a
    sweep from 80% and half the travel, and higher thresholds (knife 2.4 m/s over 20 cm, stun gun 2.0 / 14,
    heavy 3.0 / 26, a gun as a club 2.8 / 24, a punch 2.8 / 22). Checked (gentle 0.86 m/s, moderate 1.8 m/s and
    the same with late frames: no swing; real swings at 5.8 m/s: each counted): vr257 the knife 6 of 6; vr258 the
    fists and the handgun 2 of 2, the heavy weapons 1 of 2 (0.32 m then: the 40 cm swing missed by a hair, so
    the heavy and gun paths shortened, build 209); vr259 (build 209) the steel pipe, katana, maul, stun gun and
    handgun 2 of 2 each, none of the other moves.
28. [x] Holsters on the body, on by default (the owner, 2026-11-24, session 3: "for silent hill you can only really
    have one weapon equipped at a time anyway. so go ahead and make it so right shoulder is gun holster, and right
    hip is melee. both can be grabbed whenever, recognizing whichever gun/melee weapon you last holstered"): **the
    right shoulder holds a gun, the right hip a melee weapon**; reaching to either and gripping draws the gun or the
    melee weapon last put there (the game's own weapon change as you draw; the first time, the one the inventory has
    of that kind), letting go of a held weapon at the right kind of holster puts it away there; the flashlight in the
    chest pocket (item 30), health drinks and ampoules on a belt pouch; a haptic when the hand is over one; holsters
    follow the body (IK) and work in 3rd person too; left-handed mirrored. The game's inventory still equips as before.
29. [ ] Every combat mechanic checked in VR: dodge/defend, enemies' grabs and knock-downs, the beef jerky lure,
    reload, empty weapon, running out of ammo, the flashlight's effect on enemies, the radio.
30. [~] Flashlight: on the chest pocket as in the game (default), optional in the hand or head; toggled by a button
    and by touching the chest.

## E. Puzzles and interaction
31. [~] Every puzzle reviewed for VR and noted in docs/PUZZLES.md: keypads, the Shakespeare books, the clock and the
    hospital puzzles, the amusement park, the church... Puzzle screens usable by pointing and pulling the trigger
    (the game's own mouse cursor driven by the hand) or touching with a fingertip; where the game allows, the real
    object worked with the hands.
32. [~] Examining, doors, items on the ground, save points: natural in VR (reach or point and press), never blocked.

## F. Look, sound, polish
33. [~] Graphics right in stereo: no missing shadows or blood (the PC mod's known gaps), sewer reflections right, no
    invisible models, the flashlight's flare in first person fixed, the noise effect ("Film grain") right per eye.
34. [x] Sound: everything plays (CRI audio's single-core issue as sh3-nx handles it). **Spatial audio, on by default
    and working well** (the owner, 2026-11-24, session 3): every 3D sound placed from your head (its turn and
    position each frame, not Heather's facing), left/right and in front/behind, near and far as the game sets them;
    "Spatial sound" in the VR menu and the launcher.
35. [~] Story places that fight first person (the mansion's red fog corridor, chases, fixed-camera scenes): each one
    checked and handled (switch to third person automatically if needed).
36. [~] Saves: savedata on the headset, PC saves importable, never lost on update.
37. [~] Crash and error logs that explain themselves (sh3-crash.log and the Quest report: guest and host stacks, last
    calls, frame, area), shown and sendable from the launcher.

## G. Release
38. [ ] A full playthrough checked on the PC test rig (scripted where possible): every area from the mall to the church,
    no roadblocks; what only the headset can show listed in docs/HEADSET_CHECKLIST.md.
39. [~] README, CREDITS, NOTICE, release notes, What's new and "Recommended performance options"; credits: cutarev
    (sh3-nx, sh2-nx), NotGodlikeUwU (SH3 VR mod), heathercherymason-beep (Android port), sp00nznet (xboxrecomp),
    ThirteenAG (WidescreenFixesPack), zealottormunds (camera mod), Steam006 (PC Fix), Khronos (OpenXR), SDL, pl_mpeg (Dominic Szablewski).
40. [x] The APK (one build pass, signed with our own key), its SHA-256, local git repo. The owner (2026-11-24, session
    3): once every item is done, build the APK and hand it over (path and SHA-256) for the owner to test first; publish
    nothing until the owner says.
    Session 15: built (build_quest.py, debug key: no release key yet) and handed over, not published:
    android/build/SH3QuestVR-0.1.0.apk, 10,493,334 bytes, SHA-256
    3c2d90625fae25d2a721a90b6b2dbe05e9a36359b03026b07300d462a36e33c5. Waiting for the owner's headset run.
    Session 16 (owner 73: publish without the owner's headset test): the release key made (C:/SH3Quest/keys, its
    certificate's SHA-256 in tools/release-cert.sha256), the release APK built (build_quest.py --release):
    SH3QuestVR-0.1.0.apk, 9,198,022 bytes, SHA-256 3d974b4d9b0c4b2eff16877500e507a6f46bb9f18120b4f6fd614fbe60a544b0;
    its launch checks without a headset (HANDOFF-2026-11-25-SESSION16.md, "The release"). PUBLISHED 2026-11-25: the
    source pushed to main (a2435a8, b38e2c1), tag v0.1.0, the release
    github.com/moistman42069/SilentHill3-Quest-VR/releases/tag/v0.1.0 with the APK (GitHub's digest matches) and
    docs/RELEASE-NOTES-0.1.0.md; the launcher's updater sees it as the latest (not a prerelease).
    0.1.1 (owner 75-77: reload verified, then a patch release since 0.1.0 had been downloaded; the updater's first
    test): versionCode 2, SH3QuestVR-0.1.1.apk 9,200,394 bytes, SHA-256
    d902aab2e77b886fb716bbffcc23aa8831afe9c9b2a374bad9dbfaa6f18cc2e9 (the same release key), tag v0.1.1,
    docs/RELEASE-NOTES-0.1.1.md (the patch first, then the whole guide).

## H. Added by the owner (2026-11-24, session 3), all in 0.1.0
41. [~] Heather's own hands at the controllers, no stand-in hands (the owner: "as native vr as possible keeping in mind
    things weve done in recent ports and improving from there"): the scenes' finger-skinned hands (her model's groups
    3 and 4, five parts a hand on finger bones 43-72) shown in first person in place of the rigid ones play shows
    (groups 12-31, 33-38); each finger's three bones posed from the controls, stopped by the world (the probes use her
    own fingers' lengths and joints), the hand turned as the controller is; at a wall her hand stays at the wall and
    a faint ghost of her own hand shows where the controller is.
42. [~] Full-body IK, as proper as possible (the owner: "make the FULL body as proper as possible, full inverse
    kinematics, etc. you know the drill"; Jak's body refined): her head turned as yours (her shadow and mirrors);
    crouch and lean followed, her legs solved with her feet planted; standing, her body turns smoothly after the head
    with steps, not at once; the arms with Jak's refinements (the elbow eased in her chest's axes, the shoulder lifting
    with a raised hand and reaching with a far one, the hand's twist shared with the forearm, the hand kept on the
    controller); everything eased so nothing pops.
    Session 14 (owner 60, the wrists in two-hand holds): the forearm from her rest pose (no animated twist under
    it), the upper arm turned on the elbow's hinge learned from the game's own animation, the hand's twist shared
    40% by the forearm and 45% by its twin bone, kept within 200 degrees of the game's own twist for that hold
    (learned: vr199's palm-up front grips ran away to -270 degrees), and the elbow swung for what a forearm cannot
    (past 90 degrees of that middle), eased.
43. [x] Body fade near the eyes (Jak's, on by default, "Fade body near your eyes"): a leg near the eyes or raised by the
    face, or her body with her chest at the eyes or held turned over by the game (knocked down, grabbed), fades out and
    back smoothly (dithered, out in 0.08 s, held 0.35 s, back in 0.28 s); what is left of her within 7-16 cm of the
    eyes fades; "Hide arms near your eyes" (off). Her hands never fade. (The owner, session 3 part 3: "our jak vr port
    ... had a much smoother fade effect for the body ... it like fades away smoothly basically".) As Jak's: each point
    of her fades by the bone lines nearest it, so a part fades up to its joint only (her hips' draw loses only a
    raised leg, her shoulders' draw keeps the arm while the trunk goes); her head's cut dithers at its edge; other
    characters, monsters and the weapon in her hand fade out of the eyes too instead of being cut open by the near
    plane (Jak 0.1.9's item 22).
44. [~] Haptics as good as can be (the owner: "full world haptics for steps, environment actions, etc. similar to our
    opengoal approach with different vibration/haptic toggles"): Vibration, Vibration strength, "Vibration on walls
    and floors" (harder the harder a hand presses), "Extra vibration: Off / Pickups / The whole world" (the whole world
    the default: each step, landing, knock-down, door, pickup, hit given and taken, the world's shakes, the radio's
    static), "Buzz on weapon change", heartbeat, damage pulse; each its own switch, all on by default.
45. [~] The world worked with the hands wherever the game allows (the owner: "proper environment interaction where
    possible, like opening drawers, grabbing items physically to put in inventory, etc."): an item on the ground or a
    shelf is taken by closing the hand on it (her fingers wrap it, the game's own pick-up runs then, never a button),
    carried in the hand and put away over the shoulder or at the hip (or kept in the hand to look at); doors, drawers,
    lockers, cabinets, switches, handles and the save points opened or pressed by reaching and gripping or pushing
    them (the game's own action runs at that spot, with a haptic and its sound); each kind of spot listed in
    docs/PUZZLES.md with how it is worked; "Take items with your hand" (on), and the button still works.
    Session 11 (builds 141-143, vr170-vr172): the game's action test is a 1.02 m segment from her place along her
    facing against each event's region edges (0x4B8780 -> 0x4B76B0; point regions 0x4B7600 at the front point), so a
    hand's action goes from her place toward the palm, to 20 cm past it (at least 0.5 m, never past 1.02 m); a hand
    counts as reaching when over 40 cm out from the eyes or under 55 cm over the floor (not idle or hanging at her
    side); a free hand reaching onto something it can use feels a light tick first (only button events and floor
    items, after 0.5 s of play); closing it runs the game's own action with a haptic and the "take" sound. vr172: the
    right hand on Robbie the Rabbit at the start of play gave the game's own "A rabbit doll... No, it's a costume."
    Session 12 (vr189, tools/sim/events_door.txt): a walk along -z from the start to event 20 (vr171's scan stood her
    at -16000 17550) stops at z 19129: the striped railing before Robbie's bench; the scan stands her anywhere, even
    past a railing. Next: a route round it (the scan's place lines every 40 frames), or event 8 (4 m, 225 degrees
    right of her start facing: -17386 21386), then a door by hand.
    Build 164 (not run): SH3_STEER_TO=x,z[;x,z...] (sh3.c steer_rig) walks her to points round what is in the way
    (70 then 90 degrees off the line when stuck, logged) and stands her at the last. The room's table gives event
    18's region at -16687 22911 (the bytes after its shape byte; events 8-11 and 12-15 share regions, 17-19 have
    flag B 780), 3 m from her start: tools/sim/door191.txt walks there and grips it with the right hand (the game's
    own reaction in sh3.log and the pictures). A door that changes rooms waits for the playthrough if 18 is not one.
    Session 13 (vr191, build 164): the walk went round to -16387 22118 and stood against the brick ledge before the
    tall carved door (d07705): event 18's region (z 22911) is 1.6 m past where she can stand, beyond the game's own
    1.02 m segment, so neither the button nor a hand reaches it from this side (the scan: pass 0 all along). And the
    right hand held the knife (the game gives it at the start): a hand holding a weapon never takes, as meant (the
    left hand is the free one). The left hand pressed into the ledge showed the ghost hand, the right one reaching
    into the door's face too ("free again: 7 cm past the surface"): the surface rule as it should be. Next (vr193,
    combined193.txt part 6): event 8 at the scan's own place (-17386 21386, facing 315) with the LEFT hand.
    Session 14 (owner 66: "doors, drawers, and all that have been checked right?"): vr196-197 stood her at event 8's
    place and swept the free left hand all round: nothing met. Its region lies within the button's 1.02 m of her
    spot but past the hand's own ray (the palm and 20 cm). Build 175: the hand's own ray first (what is at the hand
    comes first), then the game's own 1.02 m toward the hand, for the light tick and for the closing hand: reaching
    toward a door, a drawer, a locker or a switch uses it wherever the button would, never farther. Doors, drawers,
    lockers, cabinets and examines are all the same thing to the game (an event region worked by the action), so
    one hand path covers them all; the floor items are the room's own table (0xBA9610 indexes the room data, none
    in the opening): the playthrough and the headset show those. vr205 corrects the above: event 8 is Robbie the
    Rabbit, and from where the walk stopped not even the game's button reaches him (the game's scan found him only from
    0.5 m nearer; the steer stops 0.2 m short of its point). So the hand behaves as the button does. To check it
    again, steer on to -17560,21570 (HANDOFF-2026-11-25-SESSION14.md, vr205).
    Session 15 (vr206, SH3_STEER_TO="-17386,21386;-17560,21570"): DONE in the sim. She stood at -17497 21507, the
    left hand's ray (reach 488, then the game's 500) met event 8 at frame 8196 ("the left hand can use event 8" with
    its light tick, then "the left hand used event 8" on the closing hand), and the game's own examine text ("It's a
    giant rabbit costume. Why would this be here?") showed by the hand (i08436). Event 12 worked the same way.
    A REAL DOOR by hand, both ways (owner 70-71; vr222, tools/sim/door223.txt): the opening's entrance is event 5
    (the tall door, 18, is behind the stone rail); stood at -13406 23244 facing +x, the left hand held ahead and
    closed: "the left hand used event 5", the game's door, the next area (the circular hall); there the hand used
    event 4, the same door back, and she stood in the first area again. vr223 the same from the script alone.
    Floor items: the nightmare has none (sh3.c's room log: "floor items: 0 in the room" in both areas); the first are
    in the mall's Happy Burger: the sim walks the nightmare there (vr224-, HANDOFF-2026-11-25-SESSION15.md).
    Session 16: the entrance door (event 5) by hand again in vr230-vr236. The hall's far door (event 48): vr234 was
    held at the hall's start by its dog all run; vr235 (tools/sim/door231.txt: the knife kills the dog, then the route)
    reached every point to the door (24255 18143), but the hand's palm (24645, 0.8 m ahead) stayed short of the
    door's trigger (the game's own check finds it from 24500; vr226's hand met it from 24323); vr236 (the last point
    24400 18170) was stuck at the hall's start after the kill (the line to point 4 runs into the railing). Next: a
    point at the hall's start before point 4, then the same run. The death skip to the mall (door228.txt) not run yet.
    Session 16, the far door DONE (vr242, door231.txt with a point 0.9 m into the hall and the last at 24550 18170;
    vr241 had stopped 0.2 m short): at frame 11742 "the left hand can use event 48" (its light tick), then "the left
    hand used event 48 at 24770 -626 18209": the game's own door, by hand. Left here: floor items by hand, through the
    death skip to the mall (door228.txt).
    Session 17, the death skip on build 207: vr249 lived (her knife guarded the dog's blow at 1%: Block is on by
    default); vr251, the same with Block off for that run only, died at frame 8940 ("SH3_HP lets go"), and the game
    went on to the mall: at frame 10217 a new room table (0ECA0A50), "floor items: 0 in the room", and from frame
    10658 she stood at -60600 0 -20625 in the mall's women's restroom (the red seal on the mirror), the pause menu's
    panel working there (the script's menu presses). No crash. Left: the mall's first floor items need a route out of
    the restroom (door228 has none after the death); the headset covers it too (HEADSET_CHECKLIST: "Reach and grip
    at an item"). vr252 (door252.txt: the same death, then the restroom's window by hand): the game never started
    her death that time (SH3_HP writes her health from outside the hit; she walked on at 0), so on the rig the skip is
    a coin toss. No scan in any run ever found a floor item with the game's own check (0x4B74B0): none where a run
    stood. Left for the headset or a mall save.

46. [~] **Manual reload, on by default, as good as possible** (the owner, 2026-11-24, session 3: "we already had a
    similar thing done in our halo decomp work, just make it more accurate and efficient in this port"): every gun
    and every reload read from the game (the handgun's magazine, the shotgun's shells, the submachine gun's
    magazine, the extras), each reloaded by hand the way that gun loads: the magazine out (a button or a flick), a
    new one taken from the belt and pushed in until it seats, then the gun cocked the way that gun cocks (the
    handgun's slide, the shotgun's pump, the submachine gun's bolt); shells loaded one at a time; Heather's fingers
    gripping each part properly through it; the game's own ammo and reload kept right (counts, the reload sound, no
    free ammo); "Manual reload" in the VR menu and the launcher (on; off gives the game's reload on its button).
    Session 16 (owner 74-75, reload verified on the release's build): vr237 (build 199, 0.1.0's code) reloaded only
    the handgun: reload.txt's fixed hand places (session 4) had gone stale as the guns' holds changed (sessions 12-14),
    and the shotgun's port, an aim-space offset, lay at its model's (-212.7, 47.0, -7.6), 5.4 cm under the receiver's
    floor in the air (vr238's place log). Build 201: the shotgun's port on its model (-180, 22, 0), the way in
    (0.892, -0.451, 0), seated bottom at the port; the rig's "lh_at_reload 1/2/3" (xr_sim.cpp, sim_reload_spot) takes
    the places from the code. vr239/vr240 (build 202): all four guns by hand (the handgun 0/20 -> 10/10, the slide; the
    shotgun two shells 0/12 -> 2/10, the pump; the submachine gun 0/60 -> 32/28, the knob; the stun gun 0/3 -> 3/0),
    pictures checked whole; "no free ammo" called through the guest stack both ways (SH3_TEST_EMPTY_FIRE: at 0 loaded
    answered 0, kept 0; loaded 10 -> 9; the stack level both times). Released as 0.1.1. Still [~]: the feel on a
    headset (HEADSET_CHECKLIST).
47. [x] A GitHub repository ready for the first release (the owner, session 3): github.com/moistman42069/
    SilentHill3-Quest-VR, public, README (what 0.1.0 brings, testers wanted), CREDITS.md, NOTICE; the source and the
    APK go up with the release. A Discord announcement asking for testers: docs/DISCORD-ANNOUNCEMENT.txt (1840
    characters, under Discord's 2000).

## I. Added by the owner (sessions 5-7, 2026-11-24 evening), all in 0.1.0
The owner's words for each are in docs/HANDOFF-2026-11-24-SESSIONS5-7.md ("The owner's asks", 1-30).
48. [~] Dynamic cutscenes, on by default (like Jak): first person in moments where she is drawn and it makes sense,
    third person again when she is out of frame; cutscenes look normal (her head shown); no conflict with the
    "Cutscenes" choice; option rows in the VR menu, the launcher and sh3vr.ini.
    Session 9 (build 115): written: first person while the game's camera has her head in its frame at 1.5-8 m, at the
    camera otherwise (her face up close too: her head shows); 0.5 s to change, 1.5 s held; the eyes ride her head as
    the scene poses it; "Dynamic cutscenes" in the VR menu and the launcher (on). To see in a scene on the rig.
    Session 10 (build 129, vr156-vr157): the scenes pose her model away from the player's own hidden place, so her
    head went unseen from frame 4155 on (vr146); now taken from her model wherever a scene draws it. The opening's
    park scene then follows her in her eyes (4195-6709, 1.6-8.9 m off the game's camera) and goes to the camera when her
    face comes up close (1.2 m); a scene that ends in her eyes goes on into play in first person with the rig placed
    anew (build 131). The pictures of a scene run (tools/sim/scenes.txt) still to look at whole.
    vr160 (build 135, scenes.txt): the pictures whole; 3600 and 4200 dark as the game's own (vr146 the same frames at
    the camera: alike 1.000, both black at 4200). But the bridge walk (2316-3018, 3127-3508 in her eyes) stood her eyes
    right behind two fence posts (the same place in the picture at 2880 and 3350: she stands at the ride's fence; not
    her own: vr162's dump of her draws at 2880 has her head faded whole and the rest cut round her eyes). Build 137:
    during a scene in her eyes, 22 depth probes straight ahead (11 ways, 12 and 25 cm): the world under 20 cm for 6
    frames running sends the view to the game's camera until her head moves 35 cm (logs "the world N cm before her
    eyes", and each 2 s what the probes see). vr164 (tools/sim/scenes_near.txt): at 2328, 7 frames into the bridge
    walk, "6 cm (6 of 21 probes)": the camera, kept through 3864 as she stood there; the park scene still in her eyes
    from 4200 (its picture whole). vr163 (build 136, five ways 12 degrees apart, two needed) had missed the posts.
49. [~] The health/stamina bar (the game's unlock after beating it) on top of her wrist where her watch is: shown only
    when looked at, fading, a VR menu switch, on by itself when the game's own bar option is on.
    Session 9 (build 113): written: the game's Life Display found (bit 0x80 of 0x70E66C6; its red mark pulses under
    50/25/10 percent, one beat each 2 s, 1 s, 0.4 s); SH3 has no stamina, so the bar is her health. A 6 cm bar over
    her watch on the back of her left wrist, shown while the wrist faces the eyes and is looked at, fading, its fill
    her health (green, amber under half, red under a quarter), pulsing as the game's mark; "Health bar on your
    wrist" (on) or the game's Life Display shows it. To see in vr139's pictures.
    Later in session 9 (builds 116-120, vr140-vr144): vr139 showed no bar (its point sat 12.7 cm off the wrist: the
    way of her bent rest fingers taken for the forearm's); now placed from her forearm as drawn and the watch's face
    as the twist bone turns it, 4.7 cm up from the wrist joint, across the wrist as a watch reads, 5 cm wide; seen in
    vr142 (amber, 40%, faded when turned away) before the last placement. The forearm twist bone now takes 0.8 of the
    hand's roll (was 0.5: the watch lagged 45 degrees). Owner message 35: the game's own Life Display is one red "+"
    cross (lower right, under half), no second meter. vr144 checks the last placement.
    Owner message 37 ("silent hill 3 has a health and stamina bar bru ... verify this kinda shit"): session 9 was
    wrong. Session 10 (builds 126-127, vr152): the game's Life Gauge found and seen (0x5A5E40: a yellow health bar and
    a green stamina bar at the top left, GAME_FACTS "Combat"); stamina is 1 - tiredness 0x712C620 / its most
    0x712C624. The wrist now has both: health on top (yellow, red under a quarter, the ticks and the pulse), stamina
    under it (green), seen in vr152 (40% and 60% as set, both readable). The game's own gauge is not drawn in the eyes
    in first person (the wrist has it; vr153: 0 of its pixels). Its rectangles are packets the game draws later, so they
    cannot go to the panel (vr155: still in the eyes in third person): in third person with the game's Life Gauge on, VR
    shows the same bars at the view's top left (builds 130-132, vr157: shown, following the view, the game's gone).
    Owner messages 39-40 (vr152's picture had both at once; "when they switch to third person, all hud features should
    switch to normal ... back to 1st person ... back to wrist"): build 134: first person with "Health bar on your
    wrist" on (the default): the watch only, nothing at the top left, the Life Display cross left out; third person:
    the game's normal HUD (its bars at the top left, the cross on the panel), no watch; back in first person, the
    watch again. The option off: the normal HUD in first person too. vr159 (build 134, tools/sim/bars39.txt, the gauge
    and cross forced on, health 40%): first person ahead 0 yellow and 0 green pixels in both eyes; the wrist raised:
    the watch's bars (455 yellow, 380 green); third person: the bars at the view's top left in both eyes (following the
    view as the head turned down), the cross on the panel (1929 red), no watch; first person again: 0 and 0 ahead, the
    watch at the wrist. Its log showed the top left's bars for one frame at 4061 and 4155, single frames of play
    between the opening scene's shots: build 135 shows any bars only after half a second of play (vr160 checks).
    vr155: stamina runs down about 5% a second running (most 2000) and comes back about 7.5% a second at rest.
50. [~] Physical blocking, on by default, responsive (the guard from raising the weapon or arms as a guard).
    Session 9 (build 114): written: the game's hits on her found (0x4B6A20 gathers them, 0x4EC800 takes them); a
    weapon or forearm up between her chest and the attacker keeps a quarter of the damage (melee), half (a gun), 60%
    (an arm), with a buzz and a knock; "Block with your weapon or arms" (on). To see with a monster.
    Later in session 9 (vr149): checked with the game's damage step: health 100 -> 85 guarded by the gun, 85 -> 55
    unguarded, 55 -> 37 and 37 -> 19 guarded by the forearm (the guard keeps 0.5 and 0.6 of the damage, as written).
    Session 15, against a REAL monster (vr227, build 194: the circular hall's Closer (session 16: its dog) held her
    at the hall's start from game frame 8303 to the run's end, her health held): of its 2782 hits (2781 of its arm's kind 120 at 0.2 damage a
    frame, one blow of kind 121 at 2.0), 2451 were guarded: 2419 by the knife (a third kept; the log rounds it to "0.0
    of 0.2 damage kept"), 31 by her right forearm (0.6 kept), the 2.0 blow by the knife (0.7 kept); 331 came from off the guard's line.
    Session 16 (vr230 and vr234, builds 195 and 198: the circular hall's dog, 25 bones by session 16's bone dump,
    0.67 m off, 12 degrees right of her facing, its health 712.5): 2106 and 2561 hits guarded by the knife, its 2.0
    blows (kind 121) 33 and 20 degrees off the attack's line ("0.7 of 2.0 damage kept"), its 0.2 a frame (kind 120)
    at a third ("0.0 of 0.2 kept", rounded). The block works against a real monster in both runs; its feel (the
    guard's angle) is for the headset.
51. [~] Launcher: a features popup after the game files are verified (first dynamic cutscenes and how to turn them off
    for steady third-person cutscenes, then controls and first-time tips); the updater and What's new as in the Jak and
    Ratchet launchers; the best graphics by default (top resolution with dynamic resolution, AppSW, 120 Hz).
    Session 9: the features popup (once, when the files check out; a Features button), 120 Hz with AppSW by default,
    What's new updated; the updater and What's new were there. Open: dynamic resolution.
    Session 10 (build 132): "Dynamic resolution" (Performance, on): while the headset shows frames late (its missed
    displays) the game's 3D pictures are made smaller through its own resolution change (0x402E00), a tenth at a time to
    60%, and back up after 5 s with none late (Jak's rules); its last pass stretches them over each eye, the 2D, menus and
    VR's hands stay at full size. A Quest 3 or 3S starts at the top Render resolution (1.5x), the others at 1.0x (as
    Jak's). "Show performance" and the crash log give the 3D size. vr158 (the rig's frames made late on purpose at the
    start): 100% -> 90 -> 80 -> 70 -> 60% (1000x1000 down to 600x600, frames 7334-7380, the probes' depth following each
    size), then back a tenth each 5 s of calm to 100% at frame 8583; the eyes whole at every size (each third of both
    eyes lit, 23-33% near-black as the dark room is, the 60% picture alike to the 100% one: 0.88). The log's "could not
    make them" on every change was wrong (0x402E00 answers 0 when made, 1 only when even the old size failed): read
    back from the game's size in build 133.
52. [~] Native two-hand grips on every gun and melee weapon, no thumb or finger clipping (session 5: the right hands are
    the game's own; to do: the pipe's and maul's left, the submachine gun's left, the shotgun's left on the pump, a
    thumb swing and finger solver for finger-hand grips); magazine, shell and battery holds as clean as RE4 VR's.
    Session 12 (vr188, weapons.txt): the handgun in both hands (the left wraps the right) and the katana (both hands
    stacked on the hilt) seen whole and right; the game's own left hands read for the shotgun (15.8 cm from the
    measured pump hold) and the submachine gun (12.6 cm); their two-hand pictures had the gun at the edge: vr189 looks
    25 degrees right at the shotgun's pump hand.
    Session 13 (builds 163-171, vr190-vr196; the owner: "just be sure two hand grip works/looks smoothly on all
    guns/weapons", then "BE SURE THE FUCKIN SMG LOOKS GOOD WHEN TWO HANDED"): taking and letting go are eased
    (two_w, about 0.1 s on and 0.15 s off): the other hand glides from its controller onto the grip and back, and a
    two-handed weapon's turn toward the other hand (build_frame) goes with it, so neither the hand nor the gun's line
    (with the main hand round it) jumps; the rig logs each frame of the ease and, once held, how the palm and fist
    lie against the weapon. The submachine gun's game hand is taken only within 4 cm (its game left hand opens past
    the muzzle, 12.5 cm off: her finger hand cups the receiver's front instead). vr195 showed all ten two-hand
    weapons taking their grip at 9 cm (the shotgun's pump, the handgun's wrap, the pipes', katana's and maul's
    stacked hands seen right), but both submachine guns looked off: the log said why, "the left controller is past
    her arm's reach" for the whole hold (the rig held the right hand 35 cm out right and ahead, an unreal pose), so
    her hand stayed at the arm's end beside the receiver. Build 171: where the second grip lies past her other arm's
    reach (her shoulder and reach as the arm was last solved, ik_arm_reach), the weapon and both hands are held back
    toward that shoulder by what the arm lacks (at most 20 cm), eased in with the take ("held back toward her"), so
    her hand closes on the grip at any stretch; vr196 holds the guns as a player does (the right hand before her
    chest).
53. [~] Fingers and weapons against the world: Alyx finger feel with haptics on weapons; guns and melee weapons stopped
    by the world; finger grips closing on world items (drawers, handles, doors, items; session 7: six curls probed,
    the thumb closing too, built in 96, to see in pictures). Session 8 (builds 97-108, runs vr114-vr129): a free hand
    rests on walls, floors and seats as in Alyx (planes fitted per hand, the palm 4-6 mm off, the fingers bent back
    flat on it, the ghost hand at the controller, a sound and a buzz on touching); the edge wrap over a seat or a
    ledge is written (each plane counts only over its seen part, build 108). Session 9 (builds 109-112, vr130-vr135):
    the edge wrap seen whole: an edge seen only from above gets its front face implied from the probes that saw past
    it, the fingers hook over the bench seat's edge (vr135); a sound for every hand action. Still open: world items
    (drawers, handles, doors), weapons against the world, the Alyx finger feel on weapons.
    Session 12 (vr188): the maul walked into the brick wall and pushed 35 cm on: the hand held at the wall (the arm's
    line met it), its ghost at the controller 36 cm in, the handle along the wall, "held at a surface" for the pipe and
    the maul; her arms' and the maul's shadows on the bricks.
54. [x] The flashlight as Into the Radius's pocket lamp: on her chest (the beam where you look), its trigger switches
    it at the pocket, taken off by a grip and held (her fingers round it), its trigger switches it in the hand, let go
    to put it back; on by default (session 7, builds 90-96). Session 8: the hold checked in pictures (palm on its side,
    fingers round its body, thumb over its top, the lens clear), the beam from the lens, the glare follows the lamp in
    the hand, her own hands lit by it (vr111-vr113).
55. [x] The game's film grain off by default, "Film grain" in the VR menu and the launcher puts it back (build 95-96;
    the grain's alpha test held off, the game's colour grade kept). Every VR option on by default (Sharpening Normal,
    AppSW on).
56. [~] Polish asked in sessions 5-6: head-facing movement the default, muzzle flash and effects at the gun (done,
    vr76-77), bullets where you aim, the whole body shown like Jak's with the dithered fade only near the eyes (done
    for the knife swing, vr79-81), no parts sticking out, her head and gestures in her shadow (to check), no control
    conflicts (audit done, session 6).
    Session 9: her hand held in the beam casts no shadow even with the game's shadow caster moved before it (0.25 m,
    vr139): her draws seem not to be in the shadow's pass; still to find. Her body lit by her flashlight per point
    (build 118, vr142): what is in the beam lit, what is out of it alike (no lit cuff beside a grey hand).
    Session 12 (owner messages 49-51: her full-body IK and everything she holds in the game's stencil shadows; builds
    154-158, vr183-vr187; GAME_FACTS "Shadows"): the game's shadows are stencil volumes (cleared to 16, counted,
    darkened at 17 and over), drawn from the room's list and, in rooms with shadow lights, a characters' list of kg1
    shadow models in their animation pose (the start room has none; the log flags a room that has). VR now adds its
    own: her IK body (arms, palms, each finger's three bones, legs and feet, body, neck and head), the weapon held and
    a reload part as rounded volumes from the flashlight, counted into the game's stencil just before its darkening,
    so they darken the world as the game's own shadows do (sh3vr_ik.cpp her_shadow_tris, d3d8.c vr_her_shadow);
    her own pixels carry stencil bit 0x20 (through the game's REPLACE 64 for her parts) so her volumes never darken
    her. vr187: 1428 triangles (41 casters), her arms lit as before, her legs' shadows rounded at her feet, her
    raised hand shading the beam's middle on the floor ahead. "Heather's shadow" (View, on) in the VR menu and the
    launcher. Left: the headset's look, and a room with the game's character shadows (HEADSET_CHECKLIST).
57. [~] Shake to break free, on by default (owner message 38: "see about adding an option to shake hands or something
    to break out of holds"). Session 10 (builds 127-128, vr153-vr154): the game's struggle step 0x4D44E0 found (the
    buttons and directions newly pressed while she is knocked down or held take time off the time to get free); each
    sharp shake of a controller counts as one more press, with a buzz and a rustle; a buzz as a hold starts and as she
    gets free; in VR each press is worth 1/60 s as at the PC game's 60 (the game takes a frame's time, half as much at
    120 Hz). "Shake to break free" in the VR menu and the launcher (on). vr153: shaking freed her at 2.4 s of a 5 s
    hold. To feel with a monster on the headset.

## Where each item stands (2026-11-24, session 2)

Nothing below has been compiled: the owner's rule is one build pass once every item is done. "Written" means the code
is in the tree and read through; the build pass and the runs check it (docs/HANDOFF-2026-11-24-SESSION2.md lists what
each piece risks).

1. regen --recomp-only rerun after the new wrappers (0x4ACE10, 0x4B33E0): 11977/11981, 0 failed. The bytes that
   differ between the two exes still need sh3-nx's exe; the launcher accepts both SHA-1s and names the one found.
2. Session 1's PC build ran; the GLES conversion and everything since are written, not built.
3. Written: CMake's ANDROID branch (libmain.so, SDL2 from source, the OpenXR loader), QuestGameActivity on SDL2's
   activity (the game folder as SDL_main's argument), tools/build_quest.py (stage, Gradle, APK checks). Not built.
4. Written: the launcher copies a folder (system picker) or a .zip (zip-slip guarded, its top folder dropped) in a
   foreground service, resumes a stopped copy, checks sh3.exe's SHA-1 and all 79 files' sizes.
5. Found: the movies go through DirectShow COM objects sh3-nx refuses, and the .000 files are encrypted
   (GAME_FACTS.md, Movies). Session 11: done in code (build 152), see item 5.
6. Written: the game paced at the headset's rate (the frame-time tuple, the virtual limiter); sh3-nx's program cache.
   Still to do: uniform uploads by change only; measuring on the headset.
   Session 12 (build 160, d3d8_shader.c): uniform uploads by change only: each program keeps what it last had of the
   96 vertex constants, the pixel constants, the texture matrices, the eyes' corrections and the fixed-function
   material and lights (uniform_changed), and sends a block only when it differs; uPower, uLtSpecular and uLtAmbient
   are looked up once per program, not by name every draw. To see in the next run's pictures (nothing drawn
   differently) and to measure on the headset.
7-10. Written (session 2): OpenXR from Ratchet/Jak, multiview stereo with per-eye corrections from the game's own
   projection, 2-layer render targets and depth, 6DoF with room walking through the game's movement, height and scale.
11. Written: the panel (follows gently, recentre); the noise effect and fades per eye to check on the headset.
   Session 11 (owner messages 42-44, "smooth vr ui in general"): the UI pass (sh3vr.cpp ui_frame): fades in and out;
   the game's messages in play stand by their thing (where the hand closed; vr173: Robbie's text by him), subtitles
   low in the view; screens open in front of the eyes and stay put while pointed at, gliding round when turned far
   away; full screens (out of play, no world drawn: the inventory, the map, memos) go whole onto the panel, opaque,
   the eyes dark round them (d3d8.c vr_route). Options "Menus open in front of you", "Messages next to things".
12. Written: the rust beam with a haptic tick per entry, either hand, the game's own mouse driven by it, the VR menu.
   Session 11 (vr178-179): the game reads its mouse buttons with GetKeyState (polled at 0x600A67 for the keys at
   0x721718, bit 7 of the low byte: Windows' sign-extended 0xFF80), not GetAsyncKeyState; our shim answered 0, so
   no click ever landed. Fixed (user32.c): the trigger on "Weapons" opens the weapons page, on "Maps" selects it. The
   inventory has the game's own mouse support: no hit-testing of ours needed there. B and Y stay the game's buttons
   while pointing (B as a right click left the inventory open, vr177).
   Still to do: check each screen takes the mouse (the map, memos, options, save, puzzles; hit-testing only where one
   does not); tremor smoothing is in (update_pointer: still hands settle, quick moves follow).
   Session 12 (vr187): the inventory's top tabs take the mouse: on "Maps" the first pull moves the selection there,
   the second chooses it (the game's "I don't have a map of this area." in the start room); on "Memos" the first
   pull selects it, the second enters the tab (its arrows show; nothing opens: no memos yet at the start). That is
   the game's own mouse model (as a PC player clicks twice), kept; CONTROLS.md says so. B inside the inventory closes
   it (the game's own). Options (vr189, after vr188's pulls came before the inventory had opened): the first pull
   on "Options" selects it (its arrows), the second opens the OPTIONS screen (Button Config, Language, Subtitles,
   Brightness Level, Vibration, Sound, BGM and SE Volume, Control Type, Exit) with the pointer on it. A third pull
   with the pointer on the page arrow opened Button Config, the highlighted entry: the game's click confirms what is
   selected, and its arrows are not hover targets (the left stick turns its pages, as on the pad). Still to do: an
   entry chosen by hovering it, a slider dragged, the arrows (our hit-test if the game's mouse never takes them),
   the save screen and puzzles.
   Session 12, builds 161-162 (not run yet; tools/sim/combined190.txt): the game's click zones found in the code
   (docs/GAME_FACTS.md, "Click zones"): each screen registers what a click acts on (0x601970: a rectangle in the
   cursor's 512 space and an id; lists one zone per row), and a click acts on the zone under the cursor, so hovering
   alone never moves a selection: point at a row and pull. The game hides its cursor 3 s after it last moved, and a
   hidden cursor's click finds no zone: a hand held still while reading lost its pull. Build 162 keeps the cursor
   shown while the pointer is on a screen, ticks the hand on entering each zone (the grid tick only where a screen
   has none), and logs each screen's zones and each pull's place and zone ("the game's click zones", "a pull at the
   game's cursor"): vr190 shows which of the OPTIONS rows and arrows have zones. Build 161's SH3_MEM_SNAP finds the
   OPTIONS screen's highlighted row (two down presses) for any row or arrow without a zone: our own hit-test then
   moves that variable.
   Session 13 (builds 168-170, vr194-vr195): the zones' wrapper was never called (vr194: "over click zone 0 of 0"):
   generated code calls through the dispatch table, which still mapped 0x601A40 to the generated function; sh3.c's
   override switch now names the wrapper (build 170), so each screen's zones are read where the game asks for them
   (they are cleared before Present, so they are read in the wrapper, not after). vr195 answered the open question
   in pictures: on OPTIONS the game itself highlights the row under the moving pointer (its own mouse hover; our
   own row-highlighting written for it in build 168 is taken out, so no doubled sound or memory write), and one
   pull on Exit chooses it (the screen closes to the inventory). So OPTIONS is the game's own mouse model whole:
   hover moves the row, the trigger chooses, B goes back. vr196 (build 171): the zones are read where the game asks
   for them: the inventory registers 11 ("the game's click zones: 11": its three tabs, the item ring's places, the
   arrows), and a pull on the Options tab landed "over click zone 3 of 11" (the first pull, as the inventory opened,
   still met none: the screen had not registered them yet; the game's own second click opens it as before). Left: a
   slider dragged, the save screen and puzzles (HEADSET_CHECKLIST).
13. Written: "In the scene at the camera" the default; movies wait on 5.
14. Written: hold the left menu button 1 s; a short press pauses. Session 16 (owner 80): the hold is the VR menu
    everywhere; first/third person is its "First person" switch (CONTROLS.md, controls.txt, features.txt, README).
15. Written: docs/CONTROLS.md, every one of the game's 12 pad actions reachable, no two in one state; the other 13
   entries (stick, camera, menus) stay the game's. The keyboard check needs the run.
16. Written: snap 45° (default) and smooth, head or hand direction, vignette, seated, recentre (VR menu).
17. Written: the VR menu (every option, six pages) and the launcher's Options with the same keys in sh3vr.ini.
18. Written: Play (VR or flat), game files, Options with "Recommended performance options", Controls, Logs (report to
   Download and share), Credits, What's new, the GitHub updater.
19. Done (session 3, checked on the PC rig): her arms reach the controllers on her own bones, her head is cut by a
   sphere about her head bone so her arms and chest stay in view (the last section has the details).
20. Written: hands with five fingers from the triggers, grips and touch, each finger stopped by the world (depth
   probes, a frame late). Heather's own hand meshes are not used. (Later: her own finger-skinned hands are used, LIST 41, session 3.)
21. Written: depth-probe collision (the screen's depth, not the game's collision mesh), ghost hands, the head-in-wall fade.
22. Written: reach and grip runs the game's own action under the hand; fingers wrap the weapon's handle. Fingers
   wrapping a world object's own shape: done since (session 8, sh3vr_body.cpp world_finger: each finger the palm
   faces curls on until it rests on the surface, over a table's or seat's edge).
23. Written: shots, damage pulse, heartbeat, melee, holsters, touches. The radio's static: written (sessions 5-7:
   sh3.c wraps the radio's voice levels 0x46D130, sh3vr.cpp crackles both hands, denser and harder the louder it
   plays; "Extra vibration"); the hook seen (vr100: "the radio's voice N at 0.00" at the start, no monster near). To
   feel with a monster near on the headset (HEADSET_CHECKLIST).
24. Written: the eight weapons by the PC mod's mesh signatures with its calibration; this session the game's weapon
   numbers were checked in the exe and corrected (0 none, 1-3 guns, 4-7 melee, 8 stun gun). Extras: no signature yet.
25-26. Written: aim through 0x4B3D50 from the controller or both hands, guide dot, auto-aim off in first person.
27. Written: swings press the game's attack and replace its hit volumes with the weapon's path, lengthened by the
   radius (as the PC mod); punches and clubs queued at the battle step's start. Per-enemy checks need the playthrough.
28. Written: holsters with switches. (Session 3 part 3:) drawing now runs the game's own weapon change in play, as
   its inventory does on closing (the weapon in hand put away, the new one's model file loaded by 0x59E3A0 a step a
   frame on a step stack of its own, then 0x4711D0): the handgun out in 5 frames, no crash (PC rig run 37; the old
   0x497A70 + 0x4711D0 crashed in run 34). STILL TO DO: the owner's layout (right shoulder = gun, right hip = melee,
   each gives back the last put there) in sh3vr_combat.cpp's holster places, kWeapons' holster column and the options.
   A sound on hover is not possible from here: a haptic tick instead. (Later: the layout written, LIST 28 [x].)
29. Needs the playthrough.
30. Written: chest (the game's), off hand or head; the chest pocket switches it.
31-32. docs/PUZZLES.md: each kind of puzzle and interaction and how VR does it; each puzzle checked in the playthrough.
33. Written: render targets per eye. The PC mod's gaps (shadows, blood, sewer reflections) to check.
34. sh3-nx's sound as it is; sound following the head still to do. (Later: done, LIST 34 [x], session 3 part 3.)
35. Written: third person by itself when the game's camera goes far from Heather, back when it returns.
36. Written: saves in the game folder's savedata (kept through updates), PC saves imported from the launcher.
37. Written: the crash log has the game's and VR's state (view, options, poses, hands, weapon, melee); the launcher
   shows it and shares it.
38. docs/HEADSET_CHECKLIST.md written; the playthrough needs the build.
39. Written: README, CREDITS.md, NOTICE, What's new (the release notes), the launcher's credits.
40. tools/build_quest.py written; the build waits for the owner's go.

## The PC test build (2026-11-24, session 3; the owner: "Yes, PC build")

tools/build_pc.bat (clang + clang++, Ninja) built everything since session 1 in one pass: every C and C++ file compiled
first time; the link needed opengl32 (xr_openxr.cpp's wglGetCurrentDC / wglGetCurrentContext). Runs from
C:\SH3Quest\gamedir with the scripted New Game (SH3_KEYS 22-34 s Return), frames as PNGs; scripts in tools/sim/.
- 2: flat (vr = 0): title, New Game, the opening; 60 fps, guest heap 202 MB; the same pictures as session 1's desktop
  GL (shots/run2), colours right on the GLES 3.2 path.
- 3: the Quest's texture path on the PC (SH3_NO_S3TC=1, DXT decoded): the same pictures, 55-60 fps.
- 5: the opening movie's one COM request logged in full: CLSID_FilterGraph for IID_IGraphBuilder (GAME_FACTS.md).
- 7-10: VR through the simulated headset (QUEST_SIM_SCRIPT, every VR option at its default): each eye 1000x1000,
  multiview stereo, the title on the panel with the pointer's dot, the opening's scenes from the game's camera
  ("In the scene at the camera"), then play in first person from frame 7315: Heather's eyes, walking with the stick
  moves the view, turning the head turns it, the knife in the right hand at the controller, our hands at the
  controllers. No crash in four runs (2, 4 and 2.5 minutes).
- 13/35: fixed: the opening's scripted walk has 15-frame gaps of play between its shots (after loads); the view
  jumped to first person and back. First person now waits for half a second of play after a scene; each change of
  view is in sh3.log with its frame and reasons.
- 30: fixed: the flashlight's c80-c82 rewrite now runs only for shaders that read c80-c82 directly (17 lit world
  shaders); a skinned draw's c80-c82 are bones 10 and 11 and are never touched, and a light that is off is left alone.
- 19: done on the PC rig. Heather's bone palette confirmed (c48 on, world-space 3x4 rows); the game's own bone array
  found (0x45EC00 fills [0x6B4540], 64 bytes a bone, from the model draw 0x45F5D0) with her 73 bones' parent table
  (GAME_FACTS.md, Bones). src/vr/sh3vr_ik.cpp solves each arm in that array (shoulder -> elbow -> wrist, the elbow
  bent down and out toward where the controller's forearm points, the bones' lengths kept, her hand folded into the
  wrist where our hand shows); the option "Heather's arms follow your hands" (View, on). Checked with
  tools/sim/arms.txt (QUEST_SIM_GRIP_TILT=50, the eyes at 1.55 m) and SH3_VSC_DUMP: each wrist lands on its hand (0
  units off; a hand beyond her reach gets her arm straight toward it), upper arm 122 and forearm 132 unchanged; the
  pictures show her sleeves, cuffs and watch running into our hands at rest, reaching, apart and raised.
  - Fixed on the way: the first-person cut (everything of her nearer than 25 cm) also took her arms, whose elbows sit
    9-12 cm in front of the eyes (tools/arm_clip.py). Now a 17 cm sphere about her head bone (d3d8_shader.c's
    uCutSphere, the world point from the camera's clip space) and a 12 cm flat cut for eyes inside her body; her
    head and hair stay out of view, her arms and chest stay in.
  - The world's scale is now 490 units a metre (her own: eyes 755-760 units up, shoulders 154 apart, hips 436 up);
    the PC VR mod's 360 made her 1.4 times the player.
  - The sim's view_free cannot show her from outside (the game draws from the head; the eyes only re-aim that
    picture), so her arms are checked from her eyes, as the player sees them.
- 24: SH3_VSC_DUMP=N and tools/bones_from_dump.py / skeleton_from_dump.py list every skinned draw's signature
  (vertices, primitives, stride) for the extras' capture.

## Session 3 part 3 (2026-11-24): built and checked on the PC rig (builds 20-37, runs vr20-vr37)

- 41-45 written and built; checked from her eyes with tools/sim/body.txt: her own finger hands at the controllers;
  her body turns smoothly after the head ("her body turns after the head" / "faces the head again" in sh3.log, about
  a second); the trunk fades to 1.00 looking down; a controller past her reach leaves her hand on her arm with the
  ghost hand at the controller; crouched with the hand pushed into the floor, the hand is held at the floor (vr30,
  vr32: "the right hand is held at a surface"). Fixed on the way: the probes read the device's flat auto depth (now
  the scene's own depth surface), her thigh read as a wall (leg and trunk capsules), a reach-bend at the waist so
  the floor can be reached crouched.
- The 3D world was drawn at 512 x 512 whatever the eye size: now at the eye's size (sh3.c wraps 0x402B60 / 0x402E00;
  run 32, 1000 x 1000).
- 43 refined to Jak's per-point fade (build 36; the log line "her parts fade per point along 12 bone lines" seen in
  run 36). NOT yet looked at in pictures: run tools/sim/body.txt and compare b07640-look-down-fist with vr32's. (Later: seen, vr36-vr81, LIST 43 [x].)
- 28: holster draws work in play (run 37). The shoulder / hip layout is not written yet. (Later: written, LIST 28 [x].)
- 46 (manual reload): the game's ammo, capacity, reload transaction, reload event and attack decision read
  (GAME_FACTS.md, Weapons); the handgun has two parts on their own bones. Run 37 (SH3_GIVE="10:0,15:20"
  SH3_EQUIP=1, tools/sim/gun.txt): the game decided its own reload (3) at 0 loaded, then loaded 10 from 20. The
  module itself is NOT written yet (plan in docs/HANDOFF-2026-11-24-SESSION3-PART3.md). (Later: written and seen, sh3vr_reload.cpp, session 4.)
- 34 (spatial audio): not started. (Later: done, LIST 34 [x].)
- 47: done (repository, Discord post).
- Testing switches added: SH3_GIVE=item:count,... (at the first frame she is in the room), SH3_EQUIP=weapon,
  SH3_WEAPON_DUMP=N (each 4-byte column's values of an N-vertex draw in the SH3_VSC_DUMP frame).

## Session 4 (2026-11-24): built and checked on the PC rig (builds 38-44, runs vr38-vr47)

- 43: the per-point fade looked at in pictures (vr43, again in vr47 after the shoulder change below): looking down
  70 degrees her trunk dithers away, her skirt, legs and feet stay, her arms and hands stay, no hard edge at the neck
  (her hair's ends past the cut head show at the top of the view, as in vr32).
- 28: the owner's layout built and checked (vr39): every gun over the main-hand shoulder, every melee weapon on the
  main-hand hip, each giving back the one last put there.
- 46 done: src/vr/sh3vr_reload.cpp (on by default, "Manual reload" and "Reload hints" in the VR menu and the
  launcher), built from Halo's vr_manual_reload.c and placed from the guns' own models. Run vr42 (tools/sim/reload.txt):
  the handgun's magazine seated (0 -> 10 loaded, 20 -> 10 in reserve), its slide racked, shots; two shells into the
  shotgun and its pump, shots, then the game's own reload with the left controller lost; the submachine gun's
  magazine up through the grip (0 -> 32, 60 -> 28), its cocking knob, a burst; the stun gun's battery. No free ammo
  (a dry fire stays at 0: sh3.c's 0x497800 floor; the game's own reload held off while the hands reload: 0x497780).
  The reload sounds are the game's own (learned: handgun 3, shotgun 5, submachine gun 7).
  - The cocking visual (the owner, mid-session: "add some form of visual to indicate the gun being cocked"): the game
    has no moving parts, so the slide, the knob and the pump are cut from the guns' meshes (tools/gun_parts.py) and
    moved in the vertex buffer before each draw (d3d8.c vr_gun_part): the handgun's slide back while racked, locked
    back when empty or not yet racked, kicking back on each shot; the knob and the pump with the hand. Seen in
    pictures: the slide (vr42, side view vr44) and the pump (vr42). The knob sits under the hand in every view the rig
    can take; sh3.log's "the moving part of the 557-vertex gun now ..." line shows it moving.
  - Fixed on the way: the holsters, the pouches and the arms' shoulders were measured from the eyes, which drop about
    12 cm when the head looks down (the nod is about the neck): looking down at the belt, the ammo pouch was out of
    reach (vr45). Now from the eyes with the head level (rig.h level_eyes, as her lean already was).
- 34 done (build 45, run vr48): SH3 pans and fades every 3D sound itself (no DirectSound3D), through its listener
  matrix 0x004A0500 = the game camera's view and the camera's place 0x00471800. sh3.c wraps the two 3D sound calls
  (0x0046CDF0, 0x0046CFE0) and, inside them only, hands the game the eyes' view and place instead, so the pan and the
  fall-off follow the head's turn and position each frame; the angle 0x0060BAE0 also tells dsound.c how far behind
  the head the sound is (a one-pole low-pass from 20 kHz down to 2.5 kHz and up to 30% softer directly behind).
  "Spatial sound" (View, on) in the VR menu and the launcher; flat and the cinema screen keep the game's sound.
  vr48: the listener check matched the game camera's view on its first try (so the swap is exact), Heather's
  footsteps came out at the eyes (0 across). The opening's café plays no 3D sound off to one side while the head
  turns, so the turn itself is left to the headset checklist (it is the eyes' own view, the one the picture uses).
- STOPPED here (the owner: "wrap up and do a handoff"). Found at the end, NOT fixed yet (docs/HANDOFF-2026-11-24-
  SESSION4.md has the numbers and the designed fixes):
  - Every gun is held backwards (the owner saw it in the pictures; I had missed it): the hand on the muzzle, the
    grip 20 cm out, the barrel pointing back. combat_weapon_draw puts bone 1 (a small stitched part, not the grip) at
    the hand turned by the PC mod's angles (90/90/45), which in our hand frame send the barrel backwards. The melee
    weapons use the same placement: to check. 24 and 46 are open again until every weapon is placed by its body
    bone in the reload module's gun frame and checked in pictures.
  - The handgun's slide stretches faces into spikes when racked (it is cut by height from faces it shares with the
    frame); the knob and the pump are whole pieces and move cleanly. Fix: the moving part drawn as its own faces.
  - Sound for every hand action (the owner: "gun cocking should also have proper audio, as all vr actions should"):
    only the game's reload sound plays now (on seating a magazine or a shell).
