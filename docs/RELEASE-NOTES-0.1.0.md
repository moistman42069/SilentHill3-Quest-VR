# Silent Hill 3 Quest VR 0.1.0

The first release: **Silent Hill 3 in VR, standalone on Meta Quest**, running your own copy of the PC game. An
unofficial fan port, built directly on [sh3-nx](https://github.com/cutarev/sh3-nx) by cutarev (the game's own x86 code
statically recompiled to C and then to ARM64, no emulator) with the findings of NotGodlikeUwU's
[Silent Hill 3 VR mod](https://github.com/NotGodlikeUwU/Silent-Hill-3-VR-Mod) for the PC, taken and refined, and the
VR layer and launcher of our Ratchet & Clank and Jak and Daxter Quest VR ports. Everyone is credited below, in
[CREDITS.md](https://github.com/moistman42069/SilentHill3-Quest-VR/blob/main/CREDITS.md) and in
[NOTICE](https://github.com/moistman42069/SilentHill3-Quest-VR/blob/main/NOTICE).

> [!IMPORTANT]
> **First release: testers wanted.** This build was made and checked on our PC test rig, which runs the very same game
> code with a scripted headset: the opening, doors and handles by hand, the menus and the pointer, holsters, reloads,
> melee on a monster (the knife killed the circular hall's dog), blocking, and no crash in any of it. The APK itself
> was checked piece by piece against our Jak and Daxter Quest VR APK, which runs on the Quest: the same OpenXR loader,
> the same headset entries in its manifest, only system libraries the Quest has. It has **not yet been played on a
> headset**. If anything goes wrong, the launcher saves a report with the game's own logs (see
> [When something goes wrong](#when-something-goes-wrong)): please send it with an issue.

**No game files are included.** You need the PC release of Silent Hill 3 (2003).

## What you need

- **A Meta Quest 3 or 3S** (the best picture and performance). **Quest 2 and Quest Pro** are supported too.
- **Developer mode** on the headset, to install an APK (SideQuest, Meta Quest Developer Hub or `adb`).
- **Your own copy of Silent Hill 3 for PC**, the 2003 release: its "SILENT HILL 3" folder with `sh3.exe` and the
  `data` folder (81 files, about 5 GB).
  - `sh3.exe` must be the **original, unpatched** 2003-09-03 PC release: the port's code was recompiled from it. The
    launcher checks its SHA-1 and accepts `d851669ef609b3164e3581aa8dcfbfaec866cb79` or
    `aa2aca6dba19bf510c9c2e92eefdc15e98266581`. An exe changed by a patch or a no-CD crack is refused, and the launcher
    shows the SHA-1 it found. (Keep a copy of the original exe if you patched yours.)
  - The `data` files are checked by size: keep them as the game installed them. A mod that changes a file's size is
    refused (the launcher names each file that does not match).
- **Free space** on the headset: about 5 GB for the game, plus the .zip itself while it is imported if you use one.

## Installing

1. **Install `SH3QuestVR-0.1.0.apk`**: drag it onto SideQuest, or `adb install -r SH3QuestVR-0.1.0.apk`.
   If you installed a test build of SH3 Quest VR before, uninstall it first: this release is signed with the
   project's own release key, and every later release installs over it.
2. **Copy your game to the headset**: the "SILENT HILL 3" folder, or a .zip of it, anywhere on the headset's storage
   (the Download folder is easiest), over USB or with SideQuest's file manager.
3. **Open SH3 Quest VR** (App Library, Unknown Sources). The launcher opens as a window.
4. **CHOOSE THE GAME FOLDER** (or **A .zip of it instead**) and pick it. The launcher copies the game into the app's
   own folder, `Android/data/dev.sh3quest.vr/files/game`, and checks every file. You can also copy the folder's
   contents there yourself over USB and press **Check again**.
5. Optional: **Import PC saves (choose their folder)**: your PC saves go into the game's savedata folder, as on the
   PC (a save of the same name is replaced).
6. **PLAY IN VR**.

## What's in 0.1.0

- **First person at Heather's eyes, 6DoF**: head and hands tracked; walk in your room and she walks; your height
  matched to hers and the world life size. **Third person** with the game's own camera, in stereo, whenever you like:
  hold the left menu button for a second. Places that need the game's camera switch by themselves.
- **Cutscenes in VR**: in the scene at the game's camera (or first person, or a cinema screen: Options, View).
  **Dynamic cutscenes**: through Heather's eyes while the scene shows her, from the game's camera when it shows
  something else or her face up close (turn it off for steady third-person cutscenes).
- **The game's movies play**: the opening, the endings and the others, on a big cinema screen with their sound.
  Skip one with the game's own button, as on the PC.
- **Heather's whole body follows yours** (full-body IK): her own arms, hands and fingers go where your hands are; she
  leans, crouches, turns her head and steps as you do; her body fades where it would fill your view. Look down and see
  her.
- **Hands that stop on surfaces**: her fingers stop on walls and objects, a ghost of her hand shows where yours went
  into a wall, and your head in a wall darkens the view. **Close your hand on an item, a handle or a door** to take
  or use it.
- **Her watch**: turn your left wrist toward you for her health (pulsing when low, as the game's Life Display does)
  and her stamina. In first person the watch takes the place of the game's HUD; third person shows the game's own HUD.
- **The flashlight on her chest** moves with her as you crouch or lean and lights your hands and arms in its beam.
- **Her shadow**: her whole body as you move it, her fingers and what she holds cast shadows in the flashlight's beam,
  in the game's own shadows. On or off any time: "Heather's shadow" on the VR menu's View page.
- **Every weapon in your hand**, at its size and grip, aimed where the gun points (a red guide dot shows it);
  **two-hand grips** on long guns, the steel pipe, the katana and the maul.
- **Physical melee with everything**: a real swing hits where the weapon went, with the game's own damage, knock-back
  and sound, the reaction going the way the weapon went. **Punch** with empty hands (half the weakest weapon's hit);
  **club with a gun**.
- **Block**: hold your weapon or your forearm up between you and a monster and its hit does less damage.
- **Shake to break free** when a monster knocks Heather down or grabs her (the buttons still work too).
- **Holsters on your body**: your gun over your right shoulder, your melee weapon on your right hip, each giving back
  the one you last put there; supplies in a belt pouch, the flashlight in your chest pocket. Left-handed mirrors it.
- **Manual reload**: drop the magazine (right stick down), take a new one from the ammo pouch at the front of your
  belt and push it in; shells into the shotgun one at a time; then rack the handgun's slide, the submachine gun's knob
  or the shotgun's pump and see them move. The game's own ammo counts and reload sounds, nothing free.
- **Spatial sound**: every sound in the world comes from where it is around your head; sounds behind you are softer.
- **A sound for everything your hands do**, heard from where they are: holsters, the pouch, the slide, the pump, a
  magazine, a hand on a wall, a gun knocked on one.
- **Vibration** for shots, hits, holsters, weapon changes, walls and floors under your hands, pickups and footsteps,
  and a heartbeat at low health. Each kind can be turned off.
- **A Silent Hill pointer for every screen**: menus, the inventory, the map, memos, puzzles. Point at what you want and
  pull the trigger; your hand ticks and the dot grows on each thing a pull can choose.
- **Screens made for VR**: the inventory, the map, memos and menus open whole in front of you and stay put while you
  point at them; what Heather says about a thing shows next to it; subtitles sit low in your view.
- **Comfort**: snap or smooth turning, a vignette while turning, seated play, your height.
- **Performance**: both eyes drawn together in one pass, 120 Hz with Smoother frames (AppSW), foveated rendering, the
  top resolution on a Quest 3 or 3S, and Dynamic resolution drawing the 3D picture a little smaller for a moment if
  the headset starts falling behind. "Recommended performance options" puts them back in one press.
- **Every input mapped**, none missing and no two on the same button: the launcher's Controls tab and
  [docs/CONTROLS.md](https://github.com/moistman42069/SilentHill3-Quest-VR/blob/main/docs/CONTROLS.md).
- **Every VR feature is on from the start**; each one can be turned off in the launcher's Options or in the VR menu in
  the game (pause, then hold the left menu button).
- **The launcher**: copies the game in from a folder or a .zip and checks every file, imports your PC saves, Options,
  Controls, What's new, Credits, the logs and a report, and **updates from this page's releases**.

## Controls (in play)

| Action | Control |
|---|---|
| Move | Left stick (towards where you look, or the left hand: Options, Comfort). Walking in the room moves her too. |
| Turn | Right stick (snap 45 degrees, or smooth) |
| Run | Left stick click |
| Action, examine, talk | A, or reach out and grip |
| Ready the weapon | Main trigger held |
| Attack, fire | Main trigger; melee weapons: swing them; empty hands: punch |
| Flashlight | Off-hand trigger, or grip at your chest pocket |
| Map / Inventory / Supplies | X / Y / B (supplies also: grip at the belt pouch) |
| Search view | Right stick click |
| Pause, skip a scene | Left menu button, a short press |
| First / third person | Hold the left menu button for a second |
| VR menu | Pause, then hold the left menu button |
| Draw a weapon | Main hand to its holster (gun: right shoulder; melee weapon: right hip), grip; let go there to put it back |
| Reload by hand | Right stick down drops the magazine; a new one from the front of your belt, pushed up into the grip |
| Menus, map, memos, puzzles | Point with either controller and pull the trigger; or the left stick and A. Back: B |

Left-handed (Options, Hands and weapons) swaps the hands.

## When something goes wrong

- The game writes its own logs in its folder (`Android/data/dev.sh3quest.vr/files/game`): `sh3.log` for each run and
  `sh3-crash.log` after a crash, which explains itself: what the game was doing, what VR was doing, and both stacks.
- In the launcher: **Save a report to Download** (it goes to `Download/SH3QuestVR`) or **Share the report**, then
  attach it to an issue on this repository. It holds the logs of the last run and the game files check, nothing else.
- "sh3.exe is not the release this port was made from": your exe is patched or a different release (see
  [What you need](#what-you-need)).

## Known for now

- The extra weapons stay in Heather's own hand (no holster for them yet).
- Checked only on the PC test rig so far, so the feel of things on a headset is still to be tuned with your reports:
  the strength a swing needs, the guard's angle for a block, the comfort options' defaults, the puzzles' screens
  with the pointer, and the manual reload's reach (where the ammo pouch, a gun's magazine well and its slide, knob or
  pump sit under your hand: each was checked by hand on the rig, for the handgun, the shotgun, the submachine gun and
  the stun gun, not yet in a headset).

## Updating

From this release on, the launcher checks this page's releases when it opens (at most every 6 hours; Updates, "Check
now" any time, or turn the automatic checks off) and, when you agree, downloads the next APK, checks it is ours and
hands it to Android's installer.

## Credits

| Who | What |
|---|---|
| **cutarev** | [sh3-nx](https://github.com/cutarev/sh3-nx) (and sh2-nx): the whole port underneath (the recompiled game and its runtime), built on directly |
| **NotGodlikeUwU** | The [Silent Hill 3 VR mod](https://github.com/NotGodlikeUwU/Silent-Hill-3-VR-Mod) for the PC: its findings, taken and refined (weapons, calibration, melee, the hit volumes, aiming, movement, scale) |
| **heathercherymason-beep** | [SH3AndroidEvolved](https://github.com/heathercherymason-beep/SH3AndroidEvolved): sh3-nx's runtime on Android GLES |
| **sp00nznet** | xboxrecomp, the static recompiler sh3-nx uses |
| **ThirteenAG** | [WidescreenFixesPack](https://github.com/ThirteenAG/WidescreenFixesPack): sh3-nx's widescreen fixes |
| **zealottormunds** | [sh3cammod](https://github.com/zealottormunds/sh3cammod): the first-person camera idea |
| **Steam006** | Silent Hill 3 PC Fix |
| **The Khronos Group** | The OpenXR loader and headers, the OpenGL ES headers |
| **SDL** (Sam Lantinga and contributors) | SDL2 |
| **Dominic Szablewski** | [pl_mpeg](https://github.com/phoboslab/pl_mpeg): the movies |

Silent Hill 3 is © Konami. This is an unofficial fan project, not made or approved by Konami; it includes no game files
and runs your own copy of the PC game.

## Files

- **SH3QuestVR-0.1.0.apk**: 9,198,022 bytes, SHA-256 `3d974b4d9b0c4b2eff16877500e507a6f46bb9f18120b4f6fd614fbe60a544b0`, signed with the release key whose certificate's
  SHA-256 is `38811d96c20802e5d592ea10e851defa5f6e1595c188d1942ef57057cd416e6a`.
- The source: this repository at tag `v0.1.0`. The recompiled game code is not in it: `tools/regen.sh` makes it from
  your own `sh3.exe` (see the README's "For developers").
