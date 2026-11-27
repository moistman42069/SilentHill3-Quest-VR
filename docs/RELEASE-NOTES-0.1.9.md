# Silent Hill 3 Quest VR 0.1.9

The next release after 0.1.8: **Silent Hill 3 in VR, standalone on Meta Quest**, running your own copy of the PC game.
An unofficial fan port, built directly on [sh3-nx](https://github.com/cutarev/sh3-nx) by cutarev (the game's own x86
code statically recompiled to C and then to ARM64, no emulator) with the findings of NotGodlikeUwU's
[Silent Hill 3 VR mod](https://github.com/NotGodlikeUwU/Silent-Hill-3-VR-Mod) for the PC, taken and refined, and the
VR layer and launcher of our Ratchet & Clank and Jak and Daxter Quest VR ports. Everyone is credited below, in
[CREDITS.md](https://github.com/moistman42069/SilentHill3-Quest-VR/blob/main/CREDITS.md) and in
[NOTICE](https://github.com/moistman42069/SilentHill3-Quest-VR/blob/main/NOTICE).

New here? Everything you need is on this page: [Which copies of the game work](#which-copies-of-silent-hill-3-work),
[Installing](#installing) and the rest below. Already on 0.1.0 to 0.1.8? See
[Updating](#updating-from-an-earlier-version).

## What's new in 0.1.9

0.1.9 builds on 0.1.8: saves that already lost a key on 0.1.7 can open its door, the flashlight's hint says what the
chest pocket does, a new hint shows how to work puzzles, a mirror shows your body as you move it, the gun in her hand
and her shadow are right in the scenes you see from her eyes, and no pointer shows over a black screen.

- **A key that 0.1.7 used up without opening its door now opens that door.** On 0.1.7, coming up to a key's door
  (a hand reaching toward it, a hint, or A looking where you look) could use the key up there and then, the door
  still locked (two players' reports: the store's key gone from the inventory as they stood at its door). 0.1.8
  stopped that from happening, but a save made after it had happened still had the key gone and its door locked for
  good. Now, at such a door, A (or closing your hand on it) opens it, the key used there just as the game uses it,
  as long as the game had given you that key and the door is still locked. This covers every door in the game that
  uses up its key (nine doors, eight keys). The game itself takes these keys nowhere else, so a key missing while its
  door is still locked can only have been lost this way. With the key in your inventory, at a door whose key you
  have not found yet, or at a door you have already opened, everything is as on 0.1.8. The key does not come back
  into the inventory screen: it is used at its door as you open it.
- **The flashlight's hint says what the chest pocket does.** It read "Flashlight: the left trigger, or grip at your
  chest pocket", which left a player in a video wondering what gripping there would do. It is now two lines: "Flashlight:
  the left trigger switches it on and off", then "Or grip at your chest pocket to take it in your hand and aim it" (the
  light comes off her chest into your hand while you hold the grip, points where your hand points, and goes back when
  you let go). It shows once more, the next time the flashlight is on, even if you saw the old one; the VR menu's Hints
  page lists both lines. (With "Take the flashlight from your chest" off, only the first line.)
- **A hint for puzzles.** The hints covered the controls, doors and items, grabs, weapons, the flashlight and low
  health, but not puzzles. Now the first time something you examine opens a screen (a puzzle's), two lines show at the
  top of the screen: "Puzzles and screens: point with either hand and pull the trigger to choose", then "Or the left stick and A,
  as on a pad. B backs out" (with "Pointer hand" set to one hand, it names that hand; with the pointer off, only the
  pad's line). It shows once, like every hint, and the VR menu's Hints page lists it. It comes on screens that the
  game opens from something you examine (a puzzle's), never on the inventory, the map, memos you open, the pause menu
  or the save screen.
- **The gun in her hand in the scenes you see from her eyes.** In a player's video, in a scene where Heather shoots,
  her pistol showed from the scene's camera but not from her eyes. In such a scene the game itself poses her hands and
  the weapon in them, but VR still moved the weapon to your controller, or put it away when it was holstered, so it was
  nowhere near her hands. In a scene seen from her eyes the weapon now stays in her hands where the scene puts it, with
  its shadow, as in the scene's own camera. In play nothing changes: the weapon is in your hand as before.
- **A mirror shows your body as you move it.** In the mall's restroom, Heather's reflection was the game's own
  animation: it stood still while you turned your head and moved your hands, and its hands were missing (the game
  leaves her fingers unposed in play). The game draws a reflection as a second, mirrored copy of her; that copy now
  takes her VR body's pose bone for bone, mirrored through the glass: your head's turns, your arms, hands and fingers,
  and the flashlight on her chest, just as you see them on her. This works for any mirror that reflects her this way;
  the reflection is never taken for Heather herself, so nothing about her own body changes. (The game draws no weapon
  in its mirrors, so a gun or melee weapon in her hand does not show in the reflection.)
- **Her shadow in the scenes you see from her eyes.** When she wakes at Happy Burger's table after the dream, her
  shadow looked wrong from her eyes and right from the scene's camera. With the eyes inside her head, the eyes are also
  inside the game's own shadow of her, which the game's way of drawing shadows cannot show from there. In a scene seen
  from her eyes, the game's shadow of her is now left out and her shadow is cast from her body as the scene draws it,
  the same way her shadow under the rooms' lamps has been cast in play since 0.1.7 (this reaches the opening dream's
  scenes too). Seen from the scene's camera, or in third person, nothing changes.
- **No pointer over black screens.** The pointer's beam and dot showed over the black screens and fades between
  scenes, with nothing there to point at. Now a screen that is only a fade or black shows no pointer; menus, the
  inventory, the map, memos and puzzles show it as before.
- **A much smaller log in front of a mirror.** While the mirror's copy of her was drawn, the log listed her shown
  parts again every frame (126,000 of a short test's 146,000 lines), all of it written to the headset's storage. It
  lists them only when they change now (127 lines in the same test).

Your saves, options and the copied game are kept when you update; 0.1.9 adds no options.

> [!IMPORTANT]
> **Testers wanted.** Checked on our PC test rig, which runs the very same game code with a scripted headset: a key's
> door with its key taken away as 0.1.7 did, opened with A; the same with a key the game had never given (the door
> stays locked, nothing given); the key in the inventory (everything as before); the flashlight's two hint lines on
> their label and on the Hints page; the puzzles' two lines at the top of a screen and on the Hints page (no puzzle is
> reached on the rig, so a save point's screen stood in for one in a test; the save screen, the inventory, maps, memos
> and options showed no puzzle hint); the restroom's mirror with the head turned, each hand raised and held out,
> looking down, and with the handgun drawn; Happy Burger's table and the opening dream seen from her eyes; the pointer
> over the fades and black screens and on the menus; the beam saber, the melee weapons' swings and every gun's manual
> reload; all on the release build itself, with every picture checked whole. The APK itself was checked piece by piece.
> It has **not yet been played on a headset**. Two things are not yet seen at all, because the test rig reaches neither:
> the puzzle hint on a real puzzle, and a gun in a scene seen from her eyes (the scenes the rig reaches draw her hands
> empty). If anything goes wrong, please send the launcher's report with an issue (see
> [When something goes wrong](#when-something-goes-wrong)).

## What came in 0.1.8

0.1.8 fixes what players reported on 0.1.7 (a key gone from the inventory, Heather walking on after using something,
the item you take seen double: see Fixed below) and is about the hands on the weapons. On 0.1.7 the trigger finger was not on the trigger, the second hand sat a
little off, the fingers went through the weapons, holding a weapon anywhere did not wrap the hand round it, and a
gentle move of the arm could count as a swing. Every weapon is now held by Heather's own hand closing on its own
shape, the way VR games pose a hand on a grab point.

- **Her trigger finger on the trigger, pulling it.** On the handgun, shotgun and submachine gun her index finger lies
  on the trigger itself while your finger rests on the controller's trigger, and lies along the frame, clear of the
  trigger guard, when your finger is off it. As you pull, the gun's own trigger swings back about its pin, as far as
  you pull, and her finger closes with it, its pad on the trigger the whole way, so you see her pull it. Where a
  trigger sits further than her straight finger reaches, the finger spreads a little to the side to it, as a real one
  does. The flamethrower and the stun gun have no trigger in their models: there the finger closes round the grip
  with the others.
- **Every weapon gripped properly, both hands.** Each hand's place on each weapon was found by fitting her own hand,
  as drawn, against the weapon's own shape: tens of thousands of places and turns a hand, her knuckles and palm out
  of the weapon, her palm against it, every finger closing round it with its pad on it, the trigger finger hooked on
  the trigger and pushing it the way it moves. The second hand wraps the weapon's support grip, pump or handle, and
  on the handgun your main hand's fingers, never going through them. The knife, steel pipe, katana, maul and beam
  saber are now in her own hand too, her fingers closed on each handle (the game's own hands, made for the
  third-person view, had their fingers through the knife's handle and past the katana's grip). Held in both hands,
  the steel pipe, katana, maul and beam saber have your other hand holding the handle as your main hand does, its
  mirror image, beside it on the handle. Each melee weapon still sits in your hand where it did on 0.1.7 along its
  handle, so its reach, and the beam saber's beam, are where they were.
- **Her thumb where a thumb goes.** Her thumb now swings at its root as well as curling, as a real one does, so it
  lies along a pistol's frame or round a grip or handle, its pad on the weapon, instead of standing out to the side.
- **Fingers that stop on the weapons themselves.** Her fingers already stopped on walls and objects; now they stop on
  the weapon in her hand and on the other hand's, using the weapon's own mesh as drawn (its trigger guard, slide,
  pump, tank and blade), not a few rough shapes round it.
- **A gun still aims where it aimed.** The hand moves on the gun; the gun keeps 0.1.7's way in your hand, so the red
  guide dot and every shot go where they went, and the melee weapons swing as before.
- **Hold weapons anywhere** (Hands and weapons, off to start): your other hand takes hold wherever you grip the weapon
  (the barrel, the slide, the shaft), wrapped round that part, its palm laid on it and its fingers closing round it.
  Off, it takes each weapon's two-hand grip as before. A Two-hand grip spot moved 2 cm or more off the natural grip now
  wraps the part there too.
- **Swing strength needed** (Hands and weapons: Light, Normal, Hard; Normal to start). Normal asks a firm swing (a
  fifth more speed than 0.1.7), so moving the weapon or your arm about never attacks; Light is 0.1.7's; Hard asks a
  full swing. A harder swing still hits harder, as before.

Fixed:

- **Keys used up at their doors without the door opening** (two players' reports: the bookstore's key gone from the
  inventory). The game uses up a key the moment its check finds the door that takes it, because on a PC a door it
  finds always opens. The checks VR makes only to look (your free hand reaching out, the hints, and A looking where
  you look as well as where Heather faces) asked the game the same way, so coming up to a key's door, even reaching a
  hand toward it, could use the key there and then: the door still locked, the key gone. They now only look, and the
  key is used once, when the door opens. This touched every key's door in the game. Nothing else VR does can take an
  item away: every part of the game VR calls on was checked against every part that changes the inventory, and that
  check was the only one that could. If a key went missing this way on 0.1.7, load a save made before you came up to
  its door: the key is there, and 0.1.8 keeps it until the door opens (since 0.1.9 the door also opens without it:
  see What's new in 0.1.9).
- **She kept walking after you used something** if the stick was held (or the Sprint toggle's run was on): VR went on
  moving her while the game ran its event (a message, a door, an item). She now stands while the game's event runs,
  as on a PC, and walks again with the stick as soon as it is over; a Sprint toggle run ends with the event.
- **The item you take, seen double**: as you take an item, the game shows it turning in a box beside its message. It
  was drawn in the eyes with no depth, so it showed double against the message. It now shows on the message's panel
  with it.
- **A pick-up that could stall**: taking an item from the floor waits for Heather's bend to reach the item. If the bend
  was cut short, the game waited for good (no message, no item, the action button dead). The bend is now asked again
  after a second and a quarter.
- **The shotgun's pump went the wrong way.** Pumping it, the pump slid forward along the barrel, past the muzzle, while
  your hand went back. It now goes back with your hand, to the receiver, and forward again.
- **A piece of the handgun's trigger moved with the slide** when the slide went back. The trigger now stays with the
  gun.

Your saves and the copied game are kept when you update. Because the hands now sit differently on the guns, a 0.1.7
weapon file's moves and turns for the guns and the stun gun, and every Two-hand grip spot, are put back once (sizes,
aims, guide dots and the melee weapons' moves are kept).

Research behind it: HurricaneVR's grab points (a posed hand per grab point, the trigger finger blended between off and
pulled), Auto Hand's grab solver (each finger swept closed until it meets the object's collider), Meta Interaction
SDK's hand grabs (authored hand poses per grab point), Half-Life: Alyx's hands snapped to the weapon's pose, and signed
distance fields (SDFGen) for the weapon's own shape.

> [!IMPORTANT]
> **Testers wanted.** This build was made and checked on our PC test rig, which runs the very same game code with a
> scripted headset: every weapon held one-handed and two-handed, from the eyes, looking down and close from both
> sides, right-handed and left-handed; the handgun's, shotgun's and submachine gun's triggers at rest, a third, just
> short of a shot and all the way, seen from both sides; the trigger finger off the trigger; the empty handgun's slide
> held back and the shotgun's pump drawn back and pushed forward; the beam saber lit, held in one hand and in both;
> Hold weapons anywhere, a moved Two-hand grip spot and its resets, and a 0.1.7 weapon file brought up to date; a
> swing at Normal; a key kept until its door was opened, and Heather standing still through a message after using
> something; with every picture checked whole. The APK itself was checked piece by piece.
> It has **not yet been played on a headset**. If anything goes wrong, the launcher saves a report with the game's own
> logs (see [When something goes wrong](#when-something-goes-wrong)): please send it with an issue.

## Updating from an earlier version

- **In the launcher**: open SH3 Quest VR. The launcher checks this page when it opens (at most every 6 hours), or press
  **Check now** under **Updates**.
  1. It shows **Download and install 0.1.9**. It downloads the APK and checks it against GitHub's SHA-256 and its
     size. It also checks that the APK is SH3 Quest VR, newer than yours and signed with the same key as yours, then
     hands it to Android's installer.
  2. The first time, Android may ask you to **allow Silent Hill 3 Quest VR to install updates** (a settings screen
     opens). Allow it, come back and press **Install 0.1.9**.
  3. Confirm the update in Android's installer.

  Please tell us if the updater does not work for you.
- **Or by hand**: install `SH3QuestVR-0.1.9.apk` over your version with SideQuest or
  `adb install -r SH3QuestVR-0.1.9.apk`. Do not uninstall first: uninstalling removes the app's folder, with the copied
  game and your saves.
- Either way your copied game and saves stay as they are, with no need to choose the game folder again. Your options
  stay as you set them (0.1.9 adds none). From 0.1.7 or earlier also: 0.1.8's new options start at their defaults
  (Hold weapons anywhere off, Swing strength needed Normal), and because the hands sit differently on the guns since
  0.1.8, your weapon file's moves and turns for the guns and the stun gun, and every Two-hand grip spot, are put back
  once (sizes, aims, guide dots and the melee weapons' moves are kept). From 0.1.6 or earlier also: Melee damage 200% and Break-free bar when grabbed on, and the old "Heather's
  shadow" switch is gone (the game's own shadow, following her body, is the one). From 0.1.5 or earlier also: every
  Performance option set to its default once (120 Hz, Render resolution 1.0x, Dynamic resolution on, Smoother frames
  off, Foveated rendering Medium, CPU and GPU boost on, Sharpening off, Show performance off, the game on three
  cores); from 0.1.4 or earlier, the ghost hands turned off once.

**No game files are included.** You need the PC release of Silent Hill 3 (2003).

## Which copies of Silent Hill 3 work

**Silent Hill 3 for Windows PC, the 2003 release**, in any edition: North American, European or Asian, on CD or DVD.
As far as we know every edition has the same `sh3.exe` build, dated 2003-09-03; the languages are in its data files.
Any of these `sh3.exe` files works:

| Your sh3.exe | 0.1.4 to 0.1.9 | 0.1.3 and earlier |
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

1. **Install `SH3QuestVR-0.1.9.apk`**: drag it onto SideQuest, or `adb install -r SH3QuestVR-0.1.9.apk`.
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

## Everything in 0.1.9

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
- **The flashlight on her chest** moves with her as you crouch or lean and lights your hands and arms in its beam;
  its spill keeps them in sight close by, even in the darkest rooms.
- **Her shadow** under the rooms' lamps, cast by her body as you move it and the weapon in her hand, in the game's
  own shadows, in play and in the scenes you see from her eyes. **Mirrors** show her with your body's pose: your head,
  hands and fingers, and the flashlight.
- **Every weapon in your hand**, at its size, aimed where the gun points (a red guide dot shows it), **held by
  Heather's own hand closing on its own shape**: each hand's place fitted against the weapon's own mesh, her fingers
  round it and stopping on it, her thumb along the frame or round the grip, her **trigger finger on the trigger,
  pulling it** (the gun's trigger swings back as far as you pull); **two-hand grips** on the guns, the steel pipe,
  the katana and the maul, the second hand wrapped round each, taken and let go smoothly, where it takes hold yours to
  set for each weapon, or anywhere you grip it ("Hold weapons anywhere").
- **Physical melee with everything**: a real swing hits where the weapon went, with the game's own damage, knock-back
  and sound, the reaction going the way the weapon went; just moving your arm does not swing ("Swing strength
  needed": Light, Normal, Hard). **Punch** with empty hands (half the weakest weapon's hit); **club with a gun**.
  "Melee damage" (200% to start) sets how hard they hit.
- **Block**: hold your weapon or your forearm up between you and a monster and its hit does less damage.
- **Shake to break free** when a monster knocks Heather down or grabs her (the buttons still work too), with the
  game's purple break-free bar in front of you; hit the monster holding her to break its hold.
- **Holsters on your body**: your gun on your right hip, your melee weapon over your right shoulder (or the other way
  round: "Gun on the hip, melee on the shoulder" off), each giving back the one you last put there; supplies in a belt
  pouch, the flashlight in your chest pocket. Left-handed mirrors it. "Weapon holster radius" and "Ammo pouch radius"
  set how near your hand must come.
- **Manual reload**: drop the magazine (right stick down), take a new one from the ammo pouch at the front of your
  belt and push it in; shells into the shotgun one at a time; then rack the handgun's slide, the submachine gun's knob
  or the shotgun's pump and see them move. The game's own ammo counts and reload sounds, nothing free.
- **Hints**, each shown once: a short line in front of you the first time something new comes up (the controls as
  you start, a door or an item within reach, a grab, your first weapons, the flashlight, low health, each gun's first
  reload, the first puzzle's screen). The VR menu's Hints page lists them all to read again.
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
  pull the trigger; your hand ticks and the dot grows on each thing a pull can choose. Fades and black screens show
  none.
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
- **Every VR feature is on from the start** except Smoother frames and the ghost hands, the
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
| Hints, to read again | VR menu, Hints page (each hint shows once in play; "Show every hint again" brings them back) |
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
  the strength a swing needs (Swing strength needed sets it), the guard's angle for a block, the comfort options'
  defaults, the puzzles' screens with the pointer, and the manual reload's reach (where the ammo pouch, a gun's
  magazine well and its slide, knob or pump sit under your hand: each was checked by hand on the rig, for the handgun,
  the shotgun, the submachine gun and the stun gun, not yet in a headset).
- The hands' places on each weapon were fitted on the rig against each weapon's own shape and checked in its
  pictures. If one looks off to you in the headset, "Two-hand grip spot" moves the second hand's and "Hold weapons
  anywhere" lets it take hold where you like; please tell us which weapon and how.
- A finds what you look at within about a metre and a quarter, a little either side. If a door or an item does not
  answer, step closer and look straight at it, and tell us where.
- In a player's video, two things in scenes we have not found the cause of yet: walls showing flat grey for a few
  seconds in two of Douglas's scenes, and, as a couple of scenes began, the picture squeezed into a narrow strip for a
  moment. 0.1.9's log now notes when the game draws a scene into only part of the picture; if you see either, please
  send the launcher's report and say which scene.

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

- **SH3QuestVR-0.1.9.apk** (versionCode 10): 9,630,950 bytes, SHA-256
  `ee3e10b52609ebfe16cff1a319bfb5e447e7a6f21ba25fd8ace6a1aa29fc05c2`,
  signed with the same release key as 0.1.0 to 0.1.8, whose
  certificate's SHA-256 is `38811d96c20802e5d592ea10e851defa5f6e1595c188d1942ef57057cd416e6a`.
- The source: this repository at tag `v0.1.9`. The recompiled game code is not in it: `tools/regen.sh` makes it from
  your own `sh3.exe` (see the README's "For developers").
- 0.1.0 to 0.1.8 stay on their own release pages,
  [v0.1.0](https://github.com/moistman42069/SilentHill3-Quest-VR/releases/tag/v0.1.0),
  [v0.1.1](https://github.com/moistman42069/SilentHill3-Quest-VR/releases/tag/v0.1.1),
  [v0.1.2](https://github.com/moistman42069/SilentHill3-Quest-VR/releases/tag/v0.1.2),
  [v0.1.3](https://github.com/moistman42069/SilentHill3-Quest-VR/releases/tag/v0.1.3),
  [v0.1.4](https://github.com/moistman42069/SilentHill3-Quest-VR/releases/tag/v0.1.4),
  [v0.1.5](https://github.com/moistman42069/SilentHill3-Quest-VR/releases/tag/v0.1.5),
  [v0.1.6](https://github.com/moistman42069/SilentHill3-Quest-VR/releases/tag/v0.1.6),
  [v0.1.7](https://github.com/moistman42069/SilentHill3-Quest-VR/releases/tag/v0.1.7) and
  [v0.1.8](https://github.com/moistman42069/SilentHill3-Quest-VR/releases/tag/v0.1.8).
