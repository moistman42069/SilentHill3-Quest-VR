# Silent Hill 3 Quest VR 0.1.6

The next release after 0.1.5: **Silent Hill 3 in VR, standalone on Meta Quest**, running your own copy of the PC game.
An unofficial fan port, built directly on [sh3-nx](https://github.com/cutarev/sh3-nx) by cutarev (the game's own x86
code statically recompiled to C and then to ARM64, no emulator) with the findings of NotGodlikeUwU's
[Silent Hill 3 VR mod](https://github.com/NotGodlikeUwU/Silent-Hill-3-VR-Mod) for the PC, taken and refined, and the
VR layer and launcher of our Ratchet & Clank and Jak and Daxter Quest VR ports. Everyone is credited below, in
[CREDITS.md](https://github.com/moistman42069/SilentHill3-Quest-VR/blob/main/CREDITS.md) and in
[NOTICE](https://github.com/moistman42069/SilentHill3-Quest-VR/blob/main/NOTICE).

New here? Everything you need is on this page: [Which copies of the game work](#which-copies-of-silent-hill-3-work),
[Installing](#installing) and the rest below. Already on 0.1.0 to 0.1.5? See
[Updating](#updating-from-an-earlier-version).

## What's new in 0.1.6

0.1.6 answers the reports and videos on 0.1.5: monsters that only showed up when they were on top of you, arms
twisting when backing up with a gun in both hands, a reload hint that slid out of view, and requests for a sprint
toggle, a switch for the watch's bars, swapped holsters and a bigger ammo pouch. Updating also sets every Performance
option to its best default, once.

- **Monsters are seen at their distance, not only when they reach you.** The game decides which characters to draw
  with its own camera, not your head. In first person that camera is often a fixed one framing Heather from ahead or
  from the side, so a monster in front of you was not drawn until it came close to her. Now a character the game's
  camera leaves out is checked again from your head: it is drawn when it is in your view (within 80 degrees of where
  you look) and within the game's own draw distance. Anything the game hides on purpose stays hidden. (The PC VR mod
  gets the same from the Camera Mod, which puts the game's camera itself at Heather's head.)
- **The best performance options for everyone, set once as you update.** Every option on the Performance page goes
  back to its default, whatever was saved before: **Refresh rate 120 Hz** (the smoothest: the headset redraws your
  head's turns 120 times a second, and a frame the game is late with waits the least), Render resolution 1.0x (the
  headset's own), Dynamic resolution on, Smoother frames (AppSW) off, Foveated rendering Medium, CPU and GPU boost
  on, Sharpening off and Show performance off, with the game on three cores. This happens only once: change any of
  them afterwards (Options, Performance, or the VR menu) and they stay as you set them.
- **The gun on your hip, the melee weapon over your shoulder.** The holsters have swapped: reach to your right hip for
  your gun and over your right shoulder for your melee weapon (left-handed mirrors them). Prefer them as they were?
  Turn off "Gun on the hip, melee on the shoulder" (Options, Hands and weapons, or the VR menu).
- **How near your hand must come, your choice.** "Weapon holster radius" (18 cm to start) for the hip and shoulder
  holsters, and "Ammo pouch radius" for the magazines, shells and batteries at the front of your belt, now 14 cm to
  start (it was 10, a bit too small to find by feel). Both on the Hands and weapons page.
- **Sprint toggle, on to start** (Options, Comfort, or the VR menu): click the left stick and Heather runs until you
  click it again or let go of the stick for half a second. With it off she runs while you hold the stick in, as before.
- **Health and stamina on your wrist** now has its own switch with that name on the View page (Options and the VR
  menu): on to start. Off, the watch has no bars and first person shows the game's own HUD instead (its Life Gauge
  and Life Display, when they are turned on in the game's Extra options). It was "Health bar on your wrist" on the
  Hands and weapons page.
- **No more twisted arms when backing up with a gun in both hands.** Walking back while looking forward, Heather
  turned round to face the way she walked, so her shoulders faced behind you while your hands stayed in front, and her
  arms crossed through her body. In first person she now keeps facing where you look and steps back or to the side, as
  the PC VR mod does. With a weapon in both hands her body also follows your head sooner (from 20 degrees off instead
  of 50), so your second hand stays within her reach.
- **The reload hint stays in view.** The hint (a reload's next step, shown for your first reloads) sat at the foot of
  the game's screen, so it tilted with your head and slid out of view when you looked down at your hands. It is now a
  small label of its own, level, just above where you look and clear of your hands. It holds still for small head
  moves and glides along when you look somewhere else.

Your saves and the copied game are kept when you update.

> [!IMPORTANT]
> **Testers wanted.** This build was made and checked on our PC test rig, which runs the very same game code with a
> scripted headset: an options file from 0.1.5 with other Performance settings (set to the defaults once, as above),
> the characters tested from the head (the park's hall: its dog, left out by the game's own camera 8.7 m away right
> in front of you, now drawn and seen there), the reload hint looking down, level and turning, the sprint toggle, walking back
> and sideways with the shotgun in both hands, the watch's bars, the swapped holsters (put away, drawn, swapped) and
> both radius options (a magazine taken 12 cm from the pouch, none from 20 cm), and the VR menu's new rows, with every
> picture checked whole. The APK itself was checked piece by piece. It has **not yet been played on a headset**. If
> anything goes wrong, the launcher saves a report with the game's own logs (see
> [When something goes wrong](#when-something-goes-wrong)): please send it with an issue.

## Updating from an earlier version

- **In the launcher**: open SH3 Quest VR. The launcher checks this page when it opens (at most every 6 hours), or press
  **Check now** under **Updates**.
  1. It shows **Download and install 0.1.6**. It downloads the APK and checks it against GitHub's SHA-256 and its
     size. It also checks that the APK is SH3 Quest VR, newer than yours and signed with the same key as yours, then
     hands it to Android's installer.
  2. The first time, Android may ask you to **allow Silent Hill 3 Quest VR to install updates** (a settings screen
     opens). Allow it, come back and press **Install 0.1.6**.
  3. Confirm the update in Android's installer.

  Please tell us if the updater does not work for you.
- **Or by hand**: install `SH3QuestVR-0.1.6.apk` over your version with SideQuest or
  `adb install -r SH3QuestVR-0.1.6.apk`. Do not uninstall first: uninstalling removes the app's folder, with the copied
  game and your saves.
- Either way your copied game and saves stay as they are, with no need to choose the game folder again. Your options
  stay as you set them, except the Performance page: every option there is set to its default once (120 Hz, Render
  resolution 1.0x, Dynamic resolution on, Smoother frames off, Foveated rendering Medium, CPU and GPU boost on,
  Sharpening off, Show performance off, the game on three cores). From before 0.1.5 also: the ghost hands and
  Heather's shadow turned off once.

**No game files are included.** You need the PC release of Silent Hill 3 (2003).

## Which copies of Silent Hill 3 work

**Silent Hill 3 for Windows PC, the 2003 release**, in any edition: North American, European or Asian, on CD or DVD.
As far as we know every edition has the same `sh3.exe` build, dated 2003-09-03; the languages are in its data files.
Any of these `sh3.exe` files works:

| Your sh3.exe | 0.1.4 to 0.1.6 | 0.1.3 and earlier |
|---|---|---|
| The unprotected exe (the North American copy the port was made from) | Yes | Yes |
| As installed from the disc, copy protection and all | Yes | No ("not the release") |
| A no-CD or no-DVD exe | Yes | No |
| Changed by a fix (the SH3 PC Fix and the like) | Yes, unless it moved the game's data (the launcher says so) | No |
| Another build, or not Silent Hill 3's | No, but "Try it anyway" is offered | No |

- **Does the European release work?** Yes: a player's European disc (its protected exe) played 0.1.4 in the headset.
  As far as we know it is the same 2003-09-03 build, and its other languages are in the data files, which no longer
  need the North American sizes. If yours does not play, please send the launcher's report.
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
no check at all. Since 0.1.4 this port runs the same range of copies, and more. A disc's protected exe, with its import table
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

1. **Install `SH3QuestVR-0.1.6.apk`**: drag it onto SideQuest, or `adb install -r SH3QuestVR-0.1.6.apk`.
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

## Everything in 0.1.6

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
- **Hands that stop on surfaces**: her fingers stop on walls and objects, a ghost of her hand can show where yours went
  into a wall (VR menu; off to start), and your head in a wall darkens the view (held there 2 seconds, the view
  goes back to Heather). **Close your hand on an item, a handle or a door** to take
  or use it.
- **Her watch**: turn your left wrist toward you for her health (pulsing when low, as the game's Life Display does)
  and her stamina. In first person the watch takes the place of the game's HUD; third person shows the game's own HUD.
  "Health and stamina on your wrist" (View) turns the bars off.
- **The flashlight on her chest** moves with her as you crouch or lean and lights your hands and arms in its beam.
- **Her shadow** if you want it: her whole body as you move it, her fingers and what she holds cast soft shadows
  in the flashlight's beam, with the game's own shadows. Off to start: "Heather's shadow" on the VR menu's View
  page turns it on.
- **Every weapon in your hand**, at its size and grip, aimed where the gun points (a red guide dot shows it);
  **two-hand grips** on long guns, the steel pipe, the katana and the maul, taken and let go smoothly.
- **Physical melee with everything**: a real swing hits where the weapon went, with the game's own damage, knock-back
  and sound, the reaction going the way the weapon went; just moving your arm does not swing. **Punch** with empty hands (half the weakest weapon's hit);
  **club with a gun**.
- **Block**: hold your weapon or your forearm up between you and a monster and its hit does less damage.
- **Shake to break free** when a monster knocks Heather down or grabs her (the buttons still work too).
- **Holsters on your body**: your gun on your right hip, your melee weapon over your right shoulder (or the other way
  round: "Gun on the hip, melee on the shoulder" off), each giving back the one you last put there; supplies in a belt
  pouch, the flashlight in your chest pocket. Left-handed mirrors it. "Weapon holster radius" and "Ammo pouch radius"
  set how near your hand must come.
- **Manual reload**: drop the magazine (right stick down), take a new one from the ammo pouch at the front of your
  belt and push it in; shells into the shotgun one at a time; then rack the handgun's slide, the submachine gun's knob
  or the shotgun's pump and see them move. The game's own ammo counts and reload sounds, nothing free.
- **Spatial sound**: every sound in the world comes from where it is around your head; sounds behind you are softer.
- **A sound for everything your hands do**, heard from where they are: holsters, the pouch, the slide, the pump, a
  magazine, a hand on a wall, a gun knocked on one, the VR menu opening and closing.
- **Vibration** for shots, hits, holsters, weapon changes, walls and floors under your hands, pickups and footsteps,
  and a heartbeat at low health. Each kind can be turned off.
- **The picture as the game draws it**: no film grain, fog, sharpening, smoothing or vignette to start; no lens
  flares, depth of field or shaking double picture (they are the camera's, not the world's); the game's effects at
  their place in the world, facing you, the muzzle flash at the muzzle. Film grain, Drifting fog (far off in the world), Brightness, Contrast, Color saturation,
  Darker edges and Smooth edges, as the PC VR mod has them, in the VR menu.
- **A Silent Hill pointer for every screen**: menus, the inventory, the map, memos, puzzles. Point at what you want and
  pull the trigger; your hand ticks and the dot grows on each thing a pull can choose.
- **Screens made for VR**: the inventory, the map, memos and menus open whole in front of you and stay put while you
  point at them; what Heather says about a thing shows next to it; subtitles sit low in your view.
- **Comfort**: smooth turning (or snap), a vignette while turning if you want one, seated play, your height, a
  sprint toggle (on to start). In first person Heather faces where you look while she walks, backing up included.
- **Performance**: both eyes drawn together in one pass, 120 Hz (your head's turns redrawn 120 times a second),
  **Smoother frames (AppSW)** if you want it (off to start; the headset makes the frame between each two from the
  motion the game gives it), foveated rendering, and Dynamic resolution drawing the 3D picture a little
  smaller for a moment if the headset starts falling behind (and back again if that does not help). "Recommended performance options" puts them back in one press.
- **Every input mapped**, none missing and no two on the same button: the launcher's Controls tab and
  [docs/CONTROLS.md](https://github.com/moistman42069/SilentHill3-Quest-VR/blob/main/docs/CONTROLS.md).
- **Every VR feature is on from the start** except Smoother frames, the ghost hands and Heather's shadow, the
  screen filters off; each one can be changed in the launcher's
  Options or in the VR menu in the game (hold the left menu button for a second).
- **The launcher**: copies the game in from a folder or a .zip (any PC copy of the 2003 game: the disc's, a no-CD
  exe, a fix's) and checks every file, imports your PC saves, Options,
  Controls, What's new, Credits, the logs and a report, and **updates from this page's releases**.

## Controls (in play)

| Action | Control |
|---|---|
| Move | Left stick (towards where you look, or the left hand: Options, Comfort). Walking in the room moves her too. |
| Turn | Right stick (smooth, or snap 45 degrees) |
| Run | Left stick click: she runs until you click again or let the stick go (Sprint toggle, Options, Comfort; off: hold it in) |
| Action, examine, talk | A, or reach out and grip |
| Ready the weapon | Main trigger held |
| Attack, fire | Main trigger; melee weapons: swing them; empty hands: punch |
| Stomp a downed monster | A by it, the trigger released (the game's own stomp or kick) |
| Flashlight | Off-hand trigger, or grip at your chest pocket |
| Map / Inventory / Supplies | X / Y / B (supplies also: grip at the belt pouch) |
| Search view | Right stick click |
| Recentre the view | Both sticks clicked together |
| Pause, skip a scene | Left menu button, a short press |
| VR menu | Hold the left menu button for a second, anywhere (in play the game pauses under it) |
| First / third person | VR menu, View, "First person" |
| Draw a weapon | Main hand to its holster (gun: right hip; melee weapon: over the right shoulder; swapped with "Gun on the hip, melee on the shoulder" off), grip; let go there to put it back |
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

- No Asian `sh3.exe` has been tried yet (the North American exe, a European disc's, and exes made like a disc's, a
  no-CD exe's and a fix's have): please tell us how yours does.
- At 120 Hz the game itself makes about 55 to 95 frames a second on a Quest 3 (10 to 14.5 ms each), so the headset
  shows some of its frames twice. Your head's turns stay smooth at 120 Hz either way. If the world's motion looks
  uneven to you somewhere, 90 or 72 Hz are in Options, Performance; please tell us where.
- Smoother frames is off to start (with it on in 0.1.4 some players saw stutters and black cuts). If you turn it
  on and the picture wobbles or smears at edges as you walk or turn, turn it off again (VR menu, Performance) and
  tell us where.
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

- **SH3QuestVR-0.1.6.apk** (versionCode 7): 9,522,642 bytes, SHA-256
  `7b16d82a503b1faade43a09b8e6f62acc8cd6495af21bdff3bb0bdab52832206`,
  signed with the same release key as 0.1.0 to 0.1.5, whose
  certificate's SHA-256 is `38811d96c20802e5d592ea10e851defa5f6e1595c188d1942ef57057cd416e6a`.
- The source: this repository at tag `v0.1.6`. The recompiled game code is not in it: `tools/regen.sh` makes it from
  your own `sh3.exe` (see the README's "For developers").
- 0.1.0 to 0.1.5 stay on their own release pages,
  [v0.1.0](https://github.com/moistman42069/SilentHill3-Quest-VR/releases/tag/v0.1.0),
  [v0.1.1](https://github.com/moistman42069/SilentHill3-Quest-VR/releases/tag/v0.1.1),
  [v0.1.2](https://github.com/moistman42069/SilentHill3-Quest-VR/releases/tag/v0.1.2),
  [v0.1.3](https://github.com/moistman42069/SilentHill3-Quest-VR/releases/tag/v0.1.3),
  [v0.1.4](https://github.com/moistman42069/SilentHill3-Quest-VR/releases/tag/v0.1.4) and
  [v0.1.5](https://github.com/moistman42069/SilentHill3-Quest-VR/releases/tag/v0.1.5).
