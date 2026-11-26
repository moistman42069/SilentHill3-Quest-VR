<div align=center>

<picture>
  <source media="(prefers-color-scheme: dark)" srcset="docs/logo.png">
  <img src="docs/logo-dark.png" alt="Silent Hill 3" width="70%">
</picture>

</div>
<h1 align=center>Silent Hill 3 Quest VR</h1>

<p align=center><b>v0.1.0</b> · standalone Meta Quest 3 (Quest 2, Pro, 3S too) · unofficial fan port · testers wanted</p>

Silent Hill 3 in VR on the Quest, standalone, running your own copy of the PC game. Built directly on
[sh3-nx](https://github.com/cutarev/sh3-nx) by cutarev (the game's x86 code statically recompiled to C, then to ARM64;
no emulator), with the [PC VR mod](https://github.com/NotGodlikeUwU/Silent-Hill-3-VR-Mod)'s findings by NotGodlikeUwU
taken and refined, and the VR layer and launcher of our Ratchet & Clank and Jak and Daxter Quest VR ports.
Everyone is credited in [CREDITS.md](CREDITS.md) and [NOTICE](NOTICE).

> [!NOTE]
> No game files are included. You need the PC release of Silent Hill 3 (2003): its "SILENT HILL 3" folder with
> sh3.exe and data (about 5 GB). The launcher copies it onto the headset from a folder or a .zip and checks it.

**Download**: the APK is on the [Releases](https://github.com/moistman42069/SilentHill3-Quest-VR/releases/latest) page,
with everything you need to install it ([release notes](docs/RELEASE-NOTES-0.1.0.md)). In short: install
SH3QuestVR-0.1.0.apk (developer mode, SideQuest or adb), copy your game folder or a .zip of it to the headset, open
SH3 Quest VR, CHOOSE THE GAME FOLDER, then PLAY IN VR. The first release has not yet been played on a headset:
reports are very welcome (the launcher's "Save a report to Download" holds the game's own logs).

## Features

- **First person, 6DoF**: eyes at Heather's head, head and hands tracked; walking in the room moves her; your height
  matched to hers; the world life size. **Third person** with the game's own camera, in stereo: hold the left menu
  button for a second, any time. Places that need the game's camera switch by themselves.
- **Cutscenes in VR** by default, in the scene at the game's camera (or first person, or a cinema screen);
  **dynamic cutscenes** go through Heather's eyes where that makes sense. The **movies** play on a big cinema screen
  with their sound.
- **Heather's own body**, full-body IK: her head at the headset, her hands at the controllers, her arms, trunk and legs
  following you; look down and see her. **Her shadow**: her body as you move it, her fingers and what she holds cast
  shadows in the flashlight's beam, in the game's own stencil shadows ("Heather's shadow", on/off in the VR menu).
- **The flashlight on her chest** goes where you look and lights your hands where they are in its beam.
- **Her watch**: turn your left wrist to you for her health and stamina bars (first person); third person shows the
  game's own HUD.
- **Blocking** with a weapon or a forearm held up, and **shake to break free** when a monster grabs her.
- **Weapons in your hand**, each at its size and grip (the PC mod's calibration, sh3quest_weapons.ini), aimed where
  the gun points with a guide dot; **two-hand grips** on long guns, the pipe, the katana and the maul.
- **Physical melee** with everything: real swings hit where the weapon went (the game's own hit volumes, damage,
  knock-backs and sounds); **punches** at half the weakest weapon's hit; **guns as clubs**.
- **Holsters** on the body: your gun on your right shoulder, your melee weapon on your right hip (each gives back the
  one you last put there; mirrored left-handed), supplies in a belt pouch, the flashlight in the chest pocket.
- **Spatial sound**, on by default: the game's 3D sounds placed from your head (your turn and position each frame), left
  and right, softer behind you, near and far as the game sets them.
- **Manual reload**, on by default: drop the magazine, take a new one from the ammo pouch on your belt and push it in,
  load the shotgun a shell at a time, rack the slide, the cocking knob or the pump (you see them move); the game's own
  ammo counts and reload sounds, nothing free.
- **Hands** with fingers from the controllers that **stop on surfaces** (Half-Life: Alyx style), **ghost hands** in
  walls, the head in a wall darkening the view, **grabbing** with the hand.
- **A Silent Hill pointer** for every screen: menus, the inventory, the map, memos, puzzles, the VR menu; point and
  pull, the hand ticks on what a pull can choose.
- **Dynamic resolution**: the world drawn a little smaller only while frames come late, text and hands always sharp.
- **Every input mapped**, none missing, no conflicts: [docs/CONTROLS.md](docs/CONTROLS.md).
- **The VR menu** in the game and **the launcher** (game files import and check, Options, PC saves import, logs and a
  report, What's new, updates from GitHub releases); every VR feature on and the best performance options by default.
- **Crash records that explain themselves**: sh3-crash.log with the guest and host stacks, the game's state and VR's.

## For developers

- The whole first release: [docs/LIST-0.1.0.md](docs/LIST-0.1.0.md). The game's addresses: [docs/GAME_FACTS.md](docs/GAME_FACTS.md).
  The latest handoff: docs/HANDOFF-2026-11-25-SESSION16.md.
- Recompile: `PY=<venv python> sh tools/regen.sh` (Git Bash; `--recomp-only` after changing src/game/sh3.c).
- PC test build: tools/build_pc.bat. Quest APK: tools/build_quest.py (NDK 27.2, SDK 34, JDK 17).
- sh3-nx's own README (the Switch port, building the NRO, its credits): [docs/README-sh3-nx.md](docs/README-sh3-nx.md).

Silent Hill 3 is © Konami.
