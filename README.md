<div align=center>

<picture>
  <source media="(prefers-color-scheme: dark)" srcset="docs/logo.png">
  <img src="docs/logo-dark.png" alt="Silent Hill 3" width="70%">
</picture>

</div>
<h1 align=center>Silent Hill 3 Quest VR</h1>

<p align=center><b>v0.1.0 in development</b> · standalone Meta Quest 3 and 3S (Quest 2 and Pro too) · unofficial fan port</p>

Silent Hill 3 in VR on the Quest, standalone, running your own copy of the PC game. Built directly on
[sh3-nx](https://github.com/cutarev/sh3-nx) by cutarev (the game's own x86 code statically recompiled to C, then
built for ARM64; no emulator), with the [PC VR mod](https://github.com/NotGodlikeUwU/Silent-Hill-3-VR-Mod)'s findings
by NotGodlikeUwU taken and refined, and the VR layer and launcher of our Ratchet & Clank and Jak and Daxter Quest VR
ports. Everyone is credited in [CREDITS.md](CREDITS.md) and [NOTICE](NOTICE).

> [!IMPORTANT]
> **Not released yet.** The first release (0.1.0) is being finished: the game already runs in VR on the PC test rig,
> and the Quest APK comes with the release, here under Releases. The source goes up with it.
>
> **Testers wanted:** if you have a Quest 3, 3S, 2 or Pro and your own PC copy of Silent Hill 3, watch this
> repository (Watch, then Custom, then Releases) and say hello in the Issues.

> [!NOTE]
> No game files are included. You need the PC release of Silent Hill 3 (2003): its "SILENT HILL 3" folder with
> sh3.exe and data (about 5 GB). The launcher copies it onto the headset from a folder or a .zip and checks it.

## What 0.1.0 brings

- **First person, 6DoF**: your eyes at Heather's, head and hands tracked; walk around your room, crouch, lean; your
  height matched to hers, the world life size. **Third person** with the game's own camera, in stereo: hold the left
  menu button for a second, any time. Places that need the game's camera switch by themselves.
- **Heather's own body**: full-body IK, her own hands with every finger on your controllers, her body fading smoothly
  near your eyes.
- **Cutscenes in VR**, staged in the scene at the game's camera (or first person, or a cinema screen).
- **Weapons in your hand** at their real size and grip, aimed where the gun points; **two-hand grips** on the long
  guns, the pipe, the katana and the maul; **manual reloads**; **holsters** on the body.
- **Physical melee** with everything: real swings hit where the weapon went (the game's own hits, damage and sounds);
  **punches**; **guns as clubs**.
- **Hands that stop on walls and floors**, a faint ghost hand where your controller went; **items taken and doors
  opened with your hand**.
- **Spatial sound** and **world vibration** (steps, landings, doors, pickups, hits, the radio's static), each with
  its own switch.
- **A Silent Hill pointer** for every screen: menus, the inventory, the map, memos, puzzles, the VR menu.
- **Every input mapped**, none missing, no conflicts.
- **The VR menu** in the game and **the launcher** (game files import and check, options, PC saves import, logs,
  What's new, updates from these releases); every VR feature on and the best performance options by default.
- **Crash records that explain themselves**, so reports are easy.

Silent Hill 3 is © Konami. This is an unofficial fan project, not affiliated with Konami.
