# Silent Hill 3 Quest VR 0.1.4

The next release after 0.1.3: **Silent Hill 3 in VR, standalone on Meta Quest**, running your own copy of the PC game.
An unofficial fan port, built directly on [sh3-nx](https://github.com/cutarev/sh3-nx) by cutarev (the game's own x86
code statically recompiled to C and then to ARM64, no emulator) with the findings of NotGodlikeUwU's
[Silent Hill 3 VR mod](https://github.com/NotGodlikeUwU/Silent-Hill-3-VR-Mod) for the PC, taken and refined, and the
VR layer and launcher of our Ratchet & Clank and Jak and Daxter Quest VR ports. Everyone is credited below, in
[CREDITS.md](https://github.com/moistman42069/SilentHill3-Quest-VR/blob/main/CREDITS.md) and in
[NOTICE](https://github.com/moistman42069/SilentHill3-Quest-VR/blob/main/NOTICE).

New here? Everything you need is on this page: [Which copies of the game work](#which-copies-of-silent-hill-3-work),
[Installing](#installing) and the rest below. Already on 0.1.0 to 0.1.3? See
[Updating](#updating-from-an-earlier-version).

## What's new in 0.1.4

- **Every PC copy of Silent Hill 3 (2003) should now work, and the launcher says which copies do.** Players saw
  "wrong version" and asked about the European release. Up to 0.1.3 the launcher took only the exact `sh3.exe` file
  the port was made from. That is the unprotected North American exe. The port never runs the exe's code, though: it
  has its own recompiled copy. So 0.1.4 checks what the game actually reads from `sh3.exe`, which is its data. That
  data is the same in every copy of the game's one PC build, dated 2003-09-03. These copies of that build now pass:
  - **the exe as installed from the disc**, with its copy protection (its code encrypted, its import table wiped);
  - **a no-CD or no-DVD `sh3.exe`**;
  - **an exe changed by a fix**, such as the SH3 PC Fix;
  - **the European and other editions**, which as far as we know have the same build (none tried yet).

  The game takes the data stored in the exe's code section (switch tables and the like) from its own copy of the
  build, so data that copy protection encrypted does no harm. It also takes the list of Windows functions the game
  calls from the build. In detail: [Which copies of Silent Hill 3 work](#which-copies-of-silent-hill-3-work).
- **Data files of another size no longer stop the game.** Another edition's languages or a mod can change a file's
  size; the launcher now only names those files. Only a missing file still counts against the copy.
- **A game copied a folder too deep is moved up by itself.** For example, `game/SILENT HILL 3/sh3.exe`, from
  choosing the folder above it or copying the whole folder over USB, becomes `game/sh3.exe`. `SH3.EXE`, `Sh3.exe` and
  other spellings are found whatever their case.
- **"Try it anyway"**: whenever the check is not sure, but there is a `sh3.exe`, the launcher offers **Play in VR
  anyway** and **Play flat anyway**. The game's log then says what it found in the exe and where it stopped, if it
  does.
- **Smoother frames (AppSW) works, and is on from the start.** Each frame, the game now gives the headset how the
  world moved and how far away it is, for both eyes (Meta's Application SpaceWarp). At 120 Hz the headset makes every
  other frame itself from the game's last picture, instead of showing that picture twice. Walking and turning look
  smooth, and the headset has time to spare. The game still makes its own 60 frames a second, as it always has.
  - Your head's own movement stays the headset's to show, so the motion given is the world's against your room.
  - In first person, anything within a metre of your eyes, such as your hands and what they hold, is given no motion.
  - A camera cut, a snap turn or a jump is marked as a cut, so the headset does not smear it.
  - Off or on: **"Smoother frames (AppSW)"** in the VR menu (Performance) or the launcher's Options. It takes effect the
    next time the game starts. Updating from 0.1.3 or earlier turns it on once (0.1.2 saved it when it still did
    nothing). Anything you change afterwards stays.
- Show performance and the game's log count missed displays against the display's own rate, with Smoother frames
  on too.
- The launcher's What's new, Features and Credits say all this.

Your saves and the copied game are kept when you update.

> [!IMPORTANT]
> **Testers wanted.** This build was made and checked on our PC test rig, which runs the very same game code with a
> scripted headset:
> - the unprotected exe;
> - an exe made like a copy-protected disc's, with its code encrypted, a new section and entry point, and its import
>   table wiped. The game started, played and quit cleanly with it;
> - the launcher's check against seven changed exes;
> - the motion given to the headset standing still (none), walking (spreading out from the middle), turning right
>   (the world going left) and moving only the head (none), with every picture checked whole.
>
> The APK itself was checked piece by piece against 0.1.3's. It has **not yet been played on a headset**, and no
> European or Asian exe has been tried yet. If anything goes wrong, the launcher saves a report with the game's own
> logs (see [When something goes wrong](#when-something-goes-wrong)): please send it with an issue. A report from a
> European or Asian copy is especially welcome.

## Updating from an earlier version

- **In the launcher**: open SH3 Quest VR. The launcher checks this page when it opens (at most every 6 hours), or press
  **Check now** under **Updates**.
  1. It shows **Download and install 0.1.4**. It downloads the APK and checks it against GitHub's SHA-256 and its
     size. It also checks that the APK is SH3 Quest VR, newer than yours and signed with the same key as yours, then
     hands it to Android's installer.
  2. The first time, Android may ask you to **allow Silent Hill 3 Quest VR to install updates** (a settings screen
     opens). Allow it, come back and press **Install 0.1.4**.
  3. Confirm the update in Android's installer.

  Please tell us if the updater does not work for you.
- **Or by hand**: install `SH3QuestVR-0.1.4.apk` over your version with SideQuest or
  `adb install -r SH3QuestVR-0.1.4.apk`. Do not uninstall first: uninstalling removes the app's folder, with the copied
  game and your saves.
- Either way your copied game and saves stay as they are, with no need to choose the game folder again. Your options
  stay as you set them, except Smoother frames, which is turned on once.

**No game files are included.** You need the PC release of Silent Hill 3 (2003).

## Which copies of Silent Hill 3 work

**Silent Hill 3 for Windows PC, the 2003 release**, in any edition: North American, European or Asian, on CD or DVD.
As far as we know every edition has the same `sh3.exe` build, dated 2003-09-03; the languages are in its data files.
Any of these `sh3.exe` files works:

| Your sh3.exe | 0.1.4 | 0.1.3 and earlier |
|---|---|---|
| The unprotected exe (the North American copy the port was made from) | Yes | Yes |
| As installed from the disc, copy protection and all | Yes | No ("not the release") |
| A no-CD or no-DVD exe | Yes | No |
| Changed by a fix (the SH3 PC Fix and the like) | Yes, unless it moved the game's data (the launcher says so) | No |
| Another build, or not Silent Hill 3's | No, but "Try it anyway" is offered | No |

- **Does the European release work?** It should. As far as we know it is the same 2003-09-03 build, and its other
  languages are in the data files, which no longer need the North American sizes. No European exe has been tried yet: if yours does not
  play, please send the launcher's report.
- **Not these**: the PlayStation 2 game, and the Silent Hill HD Collection on PlayStation 3 and Xbox 360. They are not
  the PC game, so they have no `sh3.exe`.
- **How the launcher checks**: it compares the exe's data with the 2003-09-03 build's, 163 pieces of 4 KB each. A copy
  with up to 16 pieces changed by a patch still passes. The data files are only checked to be there (all 79, about
  5 GB), with their sizes compared and noted.
- **How the game uses your exe**: it reads only its data. It runs its own copy of the game's code, recompiled from the
  2003-09-03 build. The data inside the exe's code section (switch tables and the like) comes from that copy too, as
  does the list of Windows functions the game calls. The first lines of `sh3.log` say what it found, for example
  "the 2003-09-03 build's data, all 163 pages the same".

The other Android port of the game, SH3AndroidEvolved on sh3-nx's runtime, loads whatever `sh3.exe` it is given, with
no check at all. 0.1.4 now runs the same range of copies, and more. A disc's protected exe, with its import table
wiped, still starts here because the function list comes from the build. The protected exe's encrypted code-section
data is replaced with the build's.

## What you need

- **A Meta Quest 3 or 3S** (the best picture and performance). **Quest 2 and Quest Pro** are supported too.
- **Developer mode** on the headset, to install an APK (SideQuest, Meta Quest Developer Hub or `adb`).
- **Your own copy of Silent Hill 3 for PC**, the 2003 release (see
  [Which copies of Silent Hill 3 work](#which-copies-of-silent-hill-3-work)): its "SILENT HILL 3" folder with
  `sh3.exe` and the `data` folder (79 files, about 5 GB).
- **Free space** on the headset: about 5 GB for the game, plus the .zip itself while it is imported if you use one.

## Installing

1. **Install `SH3QuestVR-0.1.4.apk`**: drag it onto SideQuest, or `adb install -r SH3QuestVR-0.1.4.apk`.
   If you installed a test build of SH3 Quest VR before (not a release), uninstall it first: the releases are signed
   with the project's own release key, and every later release installs over them.
2. **Copy your game to the headset**: the "SILENT HILL 3" folder, or a .zip of it, anywhere on the headset's storage
   (the Download folder is easiest), over USB or with SideQuest's file manager.
3. **Open SH3 Quest VR** (App Library, Unknown Sources). The launcher opens as a window.
4. **CHOOSE THE GAME FOLDER** (or **A .zip of it instead**) and pick it. The launcher copies the game into the app's
   own folder, `Android/data/dev.sh3quest.vr/files/game`, and checks every file. You can also copy the folder's
   contents there yourself over USB and press **Check again**. A folder copied one level too deep is moved up.
5. Optional: **Import PC saves (choose their folder)**: your PC saves go into the game's savedata folder, as on the
   PC (a save of the same name is replaced).
6. **PLAY IN VR**.

## Everything in 0.1.4

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
- **Performance**: both eyes drawn together in one pass, 120 Hz with **Smoother frames (AppSW)** (the game's 60
  frames a second, the headset making the one between each two from the motion the game gives it),
  foveated rendering, the top resolution on a Quest 3 or 3S, and Dynamic resolution drawing the 3D picture a little
  smaller for a moment if the headset starts falling behind (and back again if that does not help). "Recommended performance options" puts them back in one press.
- **Every input mapped**, none missing and no two on the same button: the launcher's Controls tab and
  [docs/CONTROLS.md](https://github.com/moistman42069/SilentHill3-Quest-VR/blob/main/docs/CONTROLS.md).
- **Every VR feature is on from the start**, the screen filters off; each one can be changed in the launcher's
  Options or in the VR menu in the game (hold the left menu button for a second).
- **The launcher**: copies the game in from a folder or a .zip (any PC copy of the 2003 game: the disc's, a no-CD
  exe, a fix's) and checks every file, imports your PC saves, Options,
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
- "sh3.exe is not the game's 2003-09-03 build": the launcher says why (another build, its data still encrypted,
  or not a Windows program) and what works (see
  [Which copies of Silent Hill 3 work](#which-copies-of-silent-hill-3-work)). **Play in VR anyway** still tries it;
  if it stops, `sh3.log`'s first lines say what the game found in the exe. Please send the report.

## Known for now

- No European or Asian `sh3.exe` has been tried yet (the North American exe and exes made like a disc's, a no-CD
  exe's and a fix's have): please tell us how yours does.
- Smoother frames was checked on the PC rig's motion, not yet in a headset: if the picture wobbles or smears at
  edges as you walk or turn, turn it off (VR menu, Performance) and tell us where.
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
| **heathercherymason-beep** | [SH3AndroidEvolved](https://github.com/heathercherymason-beep/SH3AndroidEvolved): sh3-nx's runtime on Android GLES, and an Android port that takes whatever sh3.exe it is given (0.1.4's wider check follows its lead) |
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

- **SH3QuestVR-0.1.4.apk** (versionCode 5): 9,381,206 bytes, SHA-256
  `9d0fee94ccfb5f635149a59238069fc534ab7dc899b7017d43714e6b4edbf70d`,
  signed with the same release key as 0.1.0 to 0.1.3, whose
  certificate's SHA-256 is `38811d96c20802e5d592ea10e851defa5f6e1595c188d1942ef57057cd416e6a`.
- The source: this repository at tag `v0.1.4`. The recompiled game code is not in it: `tools/regen.sh` makes it from
  your own `sh3.exe` (see the README's "For developers").
- 0.1.0 to 0.1.3 stay on their own release pages,
  [v0.1.0](https://github.com/moistman42069/SilentHill3-Quest-VR/releases/tag/v0.1.0),
  [v0.1.1](https://github.com/moistman42069/SilentHill3-Quest-VR/releases/tag/v0.1.1),
  [v0.1.2](https://github.com/moistman42069/SilentHill3-Quest-VR/releases/tag/v0.1.2) and
  [v0.1.3](https://github.com/moistman42069/SilentHill3-Quest-VR/releases/tag/v0.1.3).
