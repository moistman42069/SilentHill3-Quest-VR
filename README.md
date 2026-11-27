<div align=center>

<picture>
  <source media="(prefers-color-scheme: dark)" srcset="docs/logo.png">
  <img src="docs/logo-dark.png" alt="Silent Hill 3" width="70%">
</picture>

</div>
<h1 align=center>Silent Hill 3 Quest VR</h1>

<p align=center><b>v0.1.7</b> · standalone Meta Quest 3 (Quest 2, Pro, 3S too) · unofficial fan port · testers wanted</p>

Silent Hill 3 in VR on the Quest, standalone, running your own copy of the PC game. Built directly on
[sh3-nx](https://github.com/cutarev/sh3-nx) by cutarev (the game's x86 code statically recompiled to C, then to ARM64;
no emulator), with the [PC VR mod](https://github.com/NotGodlikeUwU/Silent-Hill-3-VR-Mod)'s findings by NotGodlikeUwU
taken and refined, and the VR layer and launcher of our Ratchet & Clank and Jak and Daxter Quest VR ports.
Everyone is credited in [CREDITS.md](CREDITS.md) and [NOTICE](NOTICE).

> [!NOTE]
> No game files are included. You need the PC release of Silent Hill 3 (2003), any edition (North American,
> European or Asian; the disc's exe, a no-CD exe or a fixed one: see [Which copies work](#which-copies-of-silent-hill-3-work)):
> its "SILENT HILL 3" folder with sh3.exe and data (about 5 GB). The launcher copies it onto the headset from a
> folder or a .zip and checks it.

**Download**: the APK is on the [Releases](https://github.com/moistman42069/SilentHill3-Quest-VR/releases/latest) page,
with everything you need to install it ([0.1.7's release notes](docs/RELEASE-NOTES-0.1.7.md): A opens what you look
at, your hands in sight in the dark, her hands closed round the guns' grips, no sticking to the dead dog, no false
weapon bumps or dings, her shadow following her body, a tick and a buzz when something is used, harder melee, the
break-free bar, a two-hand grip spot per weapon, Flat on the Quest, no getting stuck in a grab or a knockdown, hints shown once each (and a VR menu page to read them again); [0.1.6's](docs/RELEASE-NOTES-0.1.6.md): monsters seen at their
distance, the best performance options set once (120 Hz), the gun on the hip and the melee weapon on the shoulder,
holster and ammo pouch radius options, a sprint toggle, the watch's bars on a switch, no twisted arms backing up, the
reload hint in view; [0.1.5's](docs/RELEASE-NOTES-0.1.5.md): no black screen or
freezes, cutscenes in step with their voices, smoother performance defaults, the game's effects at their place in
the world, both sticks to recentre; [0.1.4's](docs/RELEASE-NOTES-0.1.4.md): every PC copy of the game accepted, the disc's protected exe, no-CD exes and the European release
among them; [0.1.3's](docs/RELEASE-NOTES-0.1.3.md): no screen filters
or fuzz by default, smooth turning, gentler melee, a softer shadow, no crash on quitting; [0.1.2's](docs/RELEASE-NOTES-0.1.2.md);
[0.1.0's](docs/RELEASE-NOTES-0.1.0.md), the first release; [0.1.1's](docs/RELEASE-NOTES-0.1.1.md)). In short:
install SH3QuestVR-0.1.7.apk (developer mode, SideQuest or adb), copy your game folder or a .zip of it to the
headset, open SH3 Quest VR, CHOOSE THE GAME FOLDER, then PLAY IN VR. Already on an earlier version? The launcher
offers 0.1.7 itself (Updates). Reports from a headset are very welcome
(the launcher's "Save a report to Download" holds the game's own logs).

## Which copies of Silent Hill 3 work

**Silent Hill 3 for Windows PC, the 2003 release**, in any edition: North American, European or Asian, CD or DVD.
As far as we know they all have the same `sh3.exe` build, dated 2003-09-03, with the languages in the data files.
The port runs its own recompiled copy of the game's code and reads only the exe's data, so any of these works:

- the exe as installed from the disc, copy protection and all;
- a no-CD or no-DVD exe;
- an exe changed by a fix such as the SH3 PC Fix;
- the unprotected exe the port was made from (SHA-1 `d851669ef609b3164e3581aa8dcfbfaec866cb79`).

The launcher checks the exe's data (163 pieces against the build's; up to 16 changed by a patch still pass) and that
the data files are there. Files of another size, such as another edition's languages or a mod, are only noted. If
the check is not sure, **Play in VR anyway** still tries it, and `sh3.log` says what the game found. Up to 0.1.3 the
launcher took only the unprotected exe; that is where "wrong version" came from. Not these: the PlayStation 2 game,
and the PS3 or Xbox 360 HD Collection. A player's European disc has played it; no Asian exe has been tried yet:
reports are welcome.
More in [0.1.7's release notes](docs/RELEASE-NOTES-0.1.7.md#which-copies-of-silent-hill-3-work).

## Features

- **First person, 6DoF**: eyes at Heather's head, head and hands tracked; walking in the room moves her; your height
  matched to hers; the world life size. **Third person** with the game's own camera, in stereo: the VR menu's
  "First person" switch (View). Places that need the game's camera switch by themselves.
- **Cutscenes in VR** by default, **dynamic**: through Heather's eyes where that makes sense, in the scene at the
  game's camera otherwise (or always at the camera, first person, or a cinema screen: the VR menu's "Cutscenes"). The **movies** play on a big cinema screen
  with their sound.
- **Heather's own body**, full-body IK: her head at the headset, her hands at the controllers, her arms, trunk and legs
  following you; look down and see her. **Her shadow** where the game casts one (under the rooms' lamps): from her body
  as you move it and the weapon in her hand, in the game's own stencil shadows (0.1.7; the old "Heather's shadow"
  switch is gone).
- **The flashlight on her chest** goes where you look and lights your hands where they are in its beam; its spill keeps
  them in sight close by, even in the darkest rooms (0.1.7).
- **Her watch**: turn your left wrist to you for her health and stamina bars (first person; "Health and stamina on
  your wrist" turns them off); third person shows the game's own HUD.
- **Monsters drawn from your head's view**: the game picks what to draw with its own camera, so a character it leaves
  out is checked again from your head and drawn when you can see it (0.1.6).
- **Sprint toggle** (on to start): click the left stick to run until you click again or let it go. In first person
  Heather faces where you look while she walks, backing up included (the PC VR mod's camera-relative strafe).
- **Blocking** with a weapon or a forearm held up, and **shake to break free** when a monster grabs her.
- **Weapons in your hand**, each at its size and grip (the PC mod's calibration, sh3quest_weapons.ini), aimed where
  the gun points with a guide dot, her fist closed round each gun's grip with her trigger finger on its trigger (0.1.7);
  **two-hand grips** on the guns, the pipe, the katana and the maul, the second hand's spot yours to set per weapon.
- **Physical melee** with everything: real swings hit where the weapon went (the game's own hit volumes, damage,
  knock-backs and sounds); **punches** at half the weakest weapon's hit; **guns as clubs**.
- **Holsters** on the body: your gun on your right hip, your melee weapon over your right shoulder (each gives back
  the one you last put there; mirrored left-handed; "Gun on the hip, melee on the shoulder" off swaps them), supplies
  in a belt pouch, the flashlight in the chest pocket. How near your hand must come: "Weapon holster radius" and
  "Ammo pouch radius" in Options (0.1.6).
- **Spatial sound**, on by default: the game's 3D sounds placed from your head (your turn and position each frame), left
  and right, softer behind you, near and far as the game sets them.
- **Manual reload**, on by default: drop the magazine, take a new one from the ammo pouch on your belt and push it in,
  load the shotgun a shell at a time, rack the slide, the cocking knob or the pump (you see them move); the game's own
  ammo counts and reload sounds, nothing free.
- **Hands** with fingers from the controllers that **stop on surfaces** (Half-Life: Alyx style), **ghost hands** in
  walls if you want them (off to start), the head in a wall darkening the view (2 seconds there and the view
  goes back to Heather), **grabbing** with the hand. **Both sticks clicked together** recentre the view.
- **A Silent Hill pointer** for every screen: menus, the inventory, the map, memos, puzzles, the VR menu; point and
  pull, the hand ticks on what a pull can choose.
- **120 Hz** to start (your head's turns redrawn 120 times a second), the game's time step the real time each frame
  stands for; 72, 80 and 90 Hz in Options. 0.1.6 sets every Performance option to its default once, on updating.
- **Smoother frames (AppSW)** if you want it (off to start): each frame the game gives the headset how the world moved and how far
  away it is (both eyes' motion and depth), and the headset makes every other frame itself; your hands and
  what they hold are left still, and cuts are not smeared.
- **Dynamic resolution**: the world drawn a little smaller only while frames come late, text and hands always sharp.
- **The picture** as the game draws it: no screen filters to start (no film grain, drifting fog, sharpening,
  smoothing or vignette); no lens flares, depth of field or shaking double picture (the camera's, not the world's);
  the game's effects at their place in the world, facing you, the muzzle flash at the muzzle, the lamps' glows left
  out in first person as the PC VR mod leaves them. **Film grain**, **Drifting fog** (on a far layer in the
  world, as the PC VR mod draws it), **Brightness**, **Contrast**, **Color saturation**, **Darker edges** and
  **Smooth edges** (FXAA) in the VR menu.
- **Every input mapped**, none missing, no conflicts: [docs/CONTROLS.md](docs/CONTROLS.md).
- **The VR menu** (hold the left menu button for a second, anywhere; in play the game pauses under it) in the game and
  **the launcher** (game files import and check, Options, PC saves import, logs and a
  report, What's new, updates from GitHub releases); every VR feature on (except Smoother frames and the ghost hands) and the best performance options
  by default.
- **Crash records that explain themselves**: sh3-crash.log with the guest and host stacks, the game's state and VR's.

## For developers

- The whole first release: [docs/LIST-0.1.0.md](docs/LIST-0.1.0.md). The game's addresses: [docs/GAME_FACTS.md](docs/GAME_FACTS.md).
  The latest handoff: docs/HANDOFF-2026-11-26-SESSION24.md.
- Recompile: `PY=<venv python> sh tools/regen.sh` (Git Bash; `--recomp-only` after changing src/game/sh3.c).
  It needs the unprotected exe (the SHA-1 above) in game/: the code is recompiled from it. tools/exe_ref.py (run by
  regen.sh) writes src/runtime/exe_ref.h (the build's data pages, sections and imports, for the loader) and
  src/recomp/gen/exe_code_data.c (the data in its code section), and the launcher's list comes from
  tools/game_manifest.py.
- PC test build: tools/build_pc.bat. Quest APK: tools/build_quest.py (NDK 27.2, SDK 34, JDK 17).
- sh3-nx's own README (the Switch port, building the NRO, its credits): [docs/README-sh3-nx.md](docs/README-sh3-nx.md).

Silent Hill 3 is © Konami.
