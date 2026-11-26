# Silent Hill 3 Quest VR 0.1.3

The next release after 0.1.2: **Silent Hill 3 in VR, standalone on Meta Quest**, running your own copy of the PC game.
An unofficial fan port, built directly on [sh3-nx](https://github.com/cutarev/sh3-nx) by cutarev (the game's own x86
code statically recompiled to C and then to ARM64, no emulator) with the findings of NotGodlikeUwU's
[Silent Hill 3 VR mod](https://github.com/NotGodlikeUwU/Silent-Hill-3-VR-Mod) for the PC, taken and refined, and the
VR layer and launcher of our Ratchet & Clank and Jak and Daxter Quest VR ports. Everyone is credited below, in
[CREDITS.md](https://github.com/moistman42069/SilentHill3-Quest-VR/blob/main/CREDITS.md) and in
[NOTICE](https://github.com/moistman42069/SilentHill3-Quest-VR/blob/main/NOTICE).

New here? Everything you need is on this page: [What you need](#what-you-need), [Installing](#installing) and the rest
below. Already on 0.1.0, 0.1.1 or 0.1.2? See [Updating](#updating-from-an-earlier-version).

## What's new in 0.1.3

From a player's report on 0.1.2: physical melee too sensitive, fog and a fuzz over the screen, snap turning, a heavy
shadow. All of it is in this release.

- **No fuzz, and no screen filters to start.** The fine grey fuzz over everything lit was a **second film grain** the
  game draws every frame, apart from its "Noise Effect": a 128 x 128 noise tile, at a new place each frame, taken off
  the picture. The Film grain option only ever turned off the first one, so the fuzz stayed. Now both are off unless
  you turn **"Film grain"** on (Options, View, or the VR menu), and the picture keeps its brightness and contrast
  (the tile is drawn as its average instead of its noise; the darkest parts are about one level of 255 darker, as
  the grain's specks no longer lift them). **Drifting fog, Sharpening, Smooth edges and the
  comfort vignette** are off to start too: the picture as the game draws it.
- **Smooth turning is the default** (snap 45 degrees is on the Options tab, Comfort). It now turns at its set speed
  in real time, however fast the game runs.
- **Physical melee no longer swings when you just move your arm**, with every weapon and with your fists. A swing's
  speed is now measured in real time: the game makes 50 to 60 frames a second on a 120 Hz headset, and 0.1.2 counted
  each frame as one of the headset's, so a gentle move was read as twice as fast as it was. A swing also needs more
  speed and a longer path than before, and has to stay that fast for a moment (at least 25 ms):

  | Swing | 0.1.3 | 0.1.2 |
  |---|---|---|
  | Knife | 2.4 m/s over 20 cm | 1.75 m/s over 14 cm |
  | Stun gun (a jab with the prongs) | 2.0 m/s over 14 cm | 1.4 m/s over 10 cm |
  | Steel pipe, katana, maul, beam saber | 3.0 m/s over 26 cm | 2.8 m/s over 30 cm |
  | A gun as a club | 2.8 m/s over 24 cm | 2.6 m/s over 26 cm |
  | A punch | 2.8 m/s over 22 cm | 2.2 m/s over 18 cm |

  0.1.2 read its speeds on the headset's frames, so in real time it asked about half of its figures. A deliberate
  swing is well past 3 m/s; a gentle move or a wave stays under 1.5. A real swing hits as before.
- **Heather's shadow is softer**: half as dark where only she shades the flashlight's beam, no darker where the
  room's own shadow already is, and none from her hand right at the lamp (within 15 cm).
- **Your options take 0.1.3's new defaults once** when you update from 0.1.2 or earlier (the ones above, and the
  picture at 100%): your saves are kept, and anything you change afterwards stays.
- **"Cutscenes" says "Dynamic"**: the VR menu's View page (and the Options tab) now reads **"Dynamic (her eyes or the
  camera)"**, the default; **"At the camera"** keeps every scene at the game's camera. 0.1.2's separate "Dynamic
  cutscenes" switch is part of this row now, your choice kept. Every VR option is in the VR menu (hold the left menu
  button for a second, anywhere) and on the launcher's Options tab, not in the game's own menus.
- **Quitting no longer ends in a crash**: the game saves its logs and closes cleanly (0.1.2 could crash on its way out).
- **Dynamic resolution takes its step back** when drawing the picture smaller does not make the frames quicker; on
  the Quest the game's own 60 frames a second, not the drawing, usually sets the pace, and the smaller picture only
  blurred it.
- **"Smoother frames (AppSW)" is gone from the options**: Silent Hill 3 never gave the headset the motion it needs
  for it, so it did nothing (the 0.1.2 log: "smoother frames on 0% of them"). It comes back once it works.
- The launcher's What's new, Features and Controls say all this.

Your saves and the copied game are kept when you update.

> [!IMPORTANT]
> **Testers wanted.** This build was made and checked on our PC test rig, which runs the very same game code with a
> scripted headset: the still picture frame to frame (no grain left) and its brightness against 0.1.2's; gentle,
> moderate and slow-frame arm moves (no swing) against real swings (each counted) with the knife, steel pipe,
> katana, maul, stun gun, handgun and empty hands; her shadow from a hand in the beam; smooth turning, the VR menu's Cutscenes row, the one-time update of a 0.1.2 options file, and quitting.
> The APK itself was checked piece by piece against 0.1.2's. It has **not yet been played on a headset**. If anything
> goes wrong, the launcher saves a report with the game's own logs (see
> [When something goes wrong](#when-something-goes-wrong)): please send it with an issue.

## Updating from an earlier version

- **In the launcher**: open SH3 Quest VR; the launcher checks this page when it opens (at most every 6 hours), or
  press **Check now** under **Updates**. It shows **Download and install 0.1.3**: it downloads the APK, checks it
  against GitHub's SHA-256, its size, that it is SH3 Quest VR, newer than yours and signed with the same key as yours,
  then hands it to Android's installer. The first time, Android may ask you to **allow Silent Hill 3 Quest VR to
  install updates** (a settings screen opens): allow it, come back and press **Install 0.1.3**. Confirm the update in
  Android's installer. (Please tell us if the updater does not work for you.)
- **Or by hand**: install `SH3QuestVR-0.1.3.apk` over your version with SideQuest or
  `adb install -r SH3QuestVR-0.1.3.apk`. Do not uninstall first: uninstalling removes the app's folder, with the copied
  game and your saves.
- Either way your copied game and saves stay as they are (no need to choose the game folder again); your options
  take 0.1.3's new defaults once, then stay as you set them.

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

1. **Install `SH3QuestVR-0.1.3.apk`**: drag it onto SideQuest, or `adb install -r SH3QuestVR-0.1.3.apk`.
   If you installed a test build of SH3 Quest VR before (not a release), uninstall it first: the releases are signed
   with the project's own release key, and every later release installs over them.
2. **Copy your game to the headset**: the "SILENT HILL 3" folder, or a .zip of it, anywhere on the headset's storage
   (the Download folder is easiest), over USB or with SideQuest's file manager.
3. **Open SH3 Quest VR** (App Library, Unknown Sources). The launcher opens as a window.
4. **CHOOSE THE GAME FOLDER** (or **A .zip of it instead**) and pick it. The launcher copies the game into the app's
   own folder, `Android/data/dev.sh3quest.vr/files/game`, and checks every file. You can also copy the folder's
   contents there yourself over USB and press **Check again**.
5. Optional: **Import PC saves (choose their folder)**: your PC saves go into the game's savedata folder, as on the
   PC (a save of the same name is replaced).
6. **PLAY IN VR**.

## Everything in 0.1.3

- **First person at Heather's eyes, 6DoF**: head and hands tracked; walk in your room and she walks; your height
  matched to hers and the world life size. **Third person** with the game's own camera, in stereo, whenever you like:
  the VR menu's "First person" switch. Places that need the game's camera switch by themselves.
- **Cutscenes in VR, dynamic**: through Heather's eyes while the scene shows her, in the scene at the game's camera
  when it shows something else or her face up close. Or always at the camera, first person, or a cinema screen: the
  VR menu's "Cutscenes" (View).
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
- **Her shadow**: her whole body as you move it, her fingers and what she holds cast soft shadows in the
  flashlight's beam, with the game's own shadows. On or off any time: "Heather's shadow" on the VR menu's View page.
- **Every weapon in your hand**, at its size and grip, aimed where the gun points (a red guide dot shows it);
  **two-hand grips** on long guns, the steel pipe, the katana and the maul, taken and let go smoothly.
- **Physical melee with everything**: a real swing hits where the weapon went, with the game's own damage, knock-back
  and sound, the reaction going the way the weapon went; just moving your arm does not swing. **Punch** with empty hands (half the weakest weapon's hit);
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
  magazine, a hand on a wall, a gun knocked on one, the VR menu opening and closing.
- **Vibration** for shots, hits, holsters, weapon changes, walls and floors under your hands, pickups and footsteps,
  and a heartbeat at low health. Each kind can be turned off.
- **The picture as the game draws it**: no film grain, fog, sharpening, smoothing or vignette to start, a lamp's
  lens flare at the lamp. Film grain, Drifting fog (far off in the world), Brightness, Contrast, Color saturation,
  Darker edges and Smooth edges, as the PC VR mod has them, in the VR menu.
- **A Silent Hill pointer for every screen**: menus, the inventory, the map, memos, puzzles. Point at what you want and
  pull the trigger; your hand ticks and the dot grows on each thing a pull can choose.
- **Screens made for VR**: the inventory, the map, memos and menus open whole in front of you and stay put while you
  point at them; what Heather says about a thing shows next to it; subtitles sit low in your view.
- **Comfort**: smooth turning (or snap), a vignette while turning if you want one, seated play, your height.
- **Performance**: both eyes drawn together in one pass, 120 Hz (the game's 60 frames a second, each shown twice),
  foveated rendering, the top resolution on a Quest 3 or 3S, and Dynamic resolution drawing the 3D picture a little
  smaller for a moment if the headset starts falling behind (and back again if that does not help). "Recommended performance options" puts them back in one press.
- **Every input mapped**, none missing and no two on the same button: the launcher's Controls tab and
  [docs/CONTROLS.md](https://github.com/moistman42069/SilentHill3-Quest-VR/blob/main/docs/CONTROLS.md).
- **Every VR feature is on from the start**, the screen filters off; each one can be changed in the launcher's
  Options or in the VR menu in the game (hold the left menu button for a second).
- **The launcher**: copies the game in from a folder or a .zip and checks every file, imports your PC saves, Options,
  Controls, What's new, Credits, the logs and a report, and **updates from this page's releases**.

## Controls (in play)

| Action | Control |
|---|---|
| Move | Left stick (towards where you look, or the left hand: Options, Comfort). Walking in the room moves her too. |
| Turn | Right stick (smooth, or snap 45 degrees) |
| Run | Left stick click |
| Action, examine, talk | A, or reach out and grip |
| Ready the weapon | Main trigger held |
| Attack, fire | Main trigger; melee weapons: swing them; empty hands: punch |
| Flashlight | Off-hand trigger, or grip at your chest pocket |
| Map / Inventory / Supplies | X / Y / B (supplies also: grip at the belt pouch) |
| Search view | Right stick click |
| Pause, skip a scene | Left menu button, a short press |
| VR menu | Hold the left menu button for a second, anywhere (in play the game pauses under it) |
| First / third person | VR menu, View, "First person" |
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

- Smoother frames (AppSW) is not in Silent Hill 3 yet (see What's new).
- The extra weapons stay in Heather's own hand (no holster for them yet).
- Checked only on the PC test rig so far, so the feel of things on a headset is still to be tuned with your reports:
  the strength a swing needs, the guard's angle for a block, the comfort options' defaults, the puzzles' screens
  with the pointer, and the manual reload's reach (where the ammo pouch, a gun's magazine well and its slide, knob or
  pump sit under your hand: each was checked by hand on the rig, for the handgun, the shotgun, the submachine gun and
  the stun gun, not yet in a headset).

## Updating

The launcher checks this page's releases when it opens (at most every 6 hours; Updates, "Check now" any time, or turn
the automatic checks off) and, when you agree, downloads the next APK, checks it is ours and hands it to Android's
installer.

## Credits

| Who | What |
|---|---|
| **cutarev** | [sh3-nx](https://github.com/cutarev/sh3-nx) (and sh2-nx): the whole port underneath (the recompiled game and its runtime), built on directly |
| **NotGodlikeUwU** | The [Silent Hill 3 VR mod](https://github.com/NotGodlikeUwU/Silent-Hill-3-VR-Mod) for the PC: its findings, taken and refined (weapons, calibration, melee, the hit volumes, aiming, movement, scale, the fog's far layer, the picture settings) |
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

- **SH3QuestVR-0.1.3.apk** (versionCode 4): 9,214,086 bytes, SHA-256
  `87e8d31550f5c1a150a991decba3ed776da5161f5c8d473ebeb7240e75b61c47`,
  signed with the same release key as 0.1.0 to 0.1.2, whose
  certificate's SHA-256 is `38811d96c20802e5d592ea10e851defa5f6e1595c188d1942ef57057cd416e6a`.
- The source: this repository at tag `v0.1.3`. The recompiled game code is not in it: `tools/regen.sh` makes it from
  your own `sh3.exe` (see the README's "For developers").
- 0.1.0 to 0.1.2 stay on their own release pages,
  [v0.1.0](https://github.com/moistman42069/SilentHill3-Quest-VR/releases/tag/v0.1.0),
  [v0.1.1](https://github.com/moistman42069/SilentHill3-Quest-VR/releases/tag/v0.1.1) and
  [v0.1.2](https://github.com/moistman42069/SilentHill3-Quest-VR/releases/tag/v0.1.2).
