# SH3 Quest VR: the controls (LIST 14, 15)

Where each of the game's actions is, in first person, third person and the menus. The game's own pad bindings in VR
are src/game/sh3.c `pad_defaults_vr` (saved by the game in savedata/keyvr.ini, apart from the flat game's key.ini); the
controllers are turned into that pad by src/vr/sh3vr.cpp `map_pad` and src/vr/sh3vr_combat.cpp `combat_pad`.
"Main hand" is the right hand, the left with Left-handed on (the triggers and grips swap; the face buttons stay).

Checked by reading the code (2026-11-24): every one of the game's 12 pad actions has a control in play, none shares
one with another in the same state, and every VR-only action (holsters, the pouch, the light pocket, grabbing, the
VR menu) is a place on the body or a hold, not a button the game also uses. First / third person is the VR menu's
switch (owner 80, session 16: the left menu button held is the VR menu everywhere).

The stomp (owner 110, 2026-11-26, read in the game's code) is the game's own move, not a pad action of its own: the
Action button (A) pressed with the weapon not readied, by a monster that is down (0x4B43D0: the action press
0x89E8AB, the ready button 0x89E8AF released, a downed target [0x712C5B4]; the attack 0x4B4340 needs ready held).
The game picks a stomp or a kick by the monster (0x4ECCD0). In VR the main trigger is ready (and fire), so with it
released A stomps; both views; docs/GAME_FACTS.md.

## In play

| The game's action      | First person (default)                                  | Third person                          |
|------------------------|---------------------------------------------------------|---------------------------------------|
| Move                   | Left stick (towards where the head looks by default, or the left hand: Walk towards); walking in the room | Left stick (the game's own) |
| Turn                   | Right stick: smooth (or snap 45°)                      | Right stick (the game's)              |
| Run (dash)             | Left stick click: runs until clicked again or the stick is let go ("Sprint toggle", on; off: held in) | Left stick click (the same) |
| Action / examine / talk | A; or reach out and grip (Grab with the hand)         | A                                     |
| Ready weapon (aim)     | Main trigger (held)                                    | Main grip or main trigger (held)      |
| Attack / fire          | Main trigger; melee: swing the weapon; empty hands: punch | Main trigger (with the weapon ready) |
| Stomp a downed monster | A by it, the main trigger released (the game's own stomp, or a kick for some monsters) | A by it, the weapon not readied |
| Flashlight             | Off-hand trigger; or grip at the chest pocket          | Off-hand trigger                      |
| Map                    | X                                                      | X                                     |
| Inventory              | Y                                                      | Y                                     |
| Supplies (health)      | B; or grip at the belt pouch                           | B                                     |
| Search view            | Right stick click                                      | Right stick click                     |
| Strafe left / right    | (free movement: not needed)                            | Off grip held + left stick left / right |
| Pause (and skip a scene) | Left menu button, a short press                      | Left menu button, a short press       |
| VR menu                | Left menu button held 1 second (the game pauses under it) | Left menu button held 1 second (the game pauses under it) |
| First / third person   | VR menu, View, "First person"                          | VR menu, View, "First person"         |
| Recentre the view      | Both sticks clicked together                           | Both sticks clicked together          |

## Hands and weapons (first person)

| What                      | How                                                                      |
|---------------------------|--------------------------------------------------------------------------|
| Draw a weapon             | Main hand to its holster, grip: every gun (handgun, shotgun, submachine gun, stun gun, the gun extras) on the main-hand hip; every melee weapon (knife, steel pipe, katana, maul, the melee extras) over the main-hand shoulder (0.1.6; "Gun on the hip, melee on the shoulder" off swaps them back). How near the hand must come: "Weapon holster radius" (18 cm). Each gives back the one last put there (at first, the first of its kind in the inventory); drawing one puts the held one in its own holster |
| Put it away               | Hold it at its holster and let go of the grip                            |
| Two hands                 | Off grip on the weapon's foregrip (long guns, pipe, katana, maul; under the handgun to steady it) |
| Aim                       | Where the gun points (the guide dot shows it); two hands aim along both  |
| Melee                     | Swing: a real swing (speed and path) attacks, and hits where the weapon went |
| Punch                     | Empty hand, a hard jab (every second punch lands the knife's hit: half the weakest weapon) |
| Gun as a club             | Swing a gun hard: the pipe's hit                                         |
| Block ("Block with your weapon or arms", on) | Hold the weapon, or a forearm, up between you and a monster: its hit keeps a quarter of the damage behind a melee weapon (the knife a third), half behind a gun, 60% on a bare arm |
| Health and stamina ("Health and stamina on your wrist", View, on; off: the game's HUD in first person too) | In first person, turn your left wrist toward you and look at her watch: her health (yellow, red under a quarter, pulsing when under half) and under it her stamina (green: it drains as she runs and comes back at rest). Third person shows the game's normal HUD (its Life Gauge and Life Display, when on in Extra options) |
| Get free ("Shake to break free", on) | When a monster knocks her down or holds her: shake the controllers hard side to side (each quick shake buzzes); pressing the buttons fast still works too |
| Supplies                  | Off or main hand to the belt pouch, grip                                 |
| Reload ("Manual reload", on) | Right stick down drops the magazine (the stun gun's battery). Off hand to the ammo pouch (the front of the belt, off-hand side; "Ammo pouch radius", 14 cm from 0.1.6), grip: a new one; line it up under the grip and push it up until it seats (handgun, stun gun; the submachine gun takes its magazine up through its grip too). Shotgun: a shell from the pouch pushed into the loading port under the gun, one at a time. A new magazine can be taken before the old one is dropped |
| Cock (after loading an empty gun) | Handgun: off hand grips the top of the slide, pulls it all the way back and lets go. Submachine gun: the cocking knob on top at the back, the same. Shotgun: off hand on the pump, back and forward. The slide, knob and pump move with the hand; an empty handgun's slide stays back and it cycles on each shot. Until then the trigger only clicks |
| "Manual reload" off, or a controller lost | The game reloads as it always did (by itself when the gun is empty, or from the inventory) |
| Flashlight on / off       | Either hand to the chest pocket, grip                                    |
| Pick up, open             | Reach towards it and grip (the game's action under the hand)             |

## Menus, the map, memos, puzzles (any screen the game shows)

Each opens whole on a screen in front of you where you look, the world dark round it, and stays put while you point
at it; turn over 35 degrees away for half a second and it comes round in front again ("Screen follows the view").
What Heather says about a thing shows next to it (where your hand closed on it; with A, ahead of you); subtitles sit
low in your view. Options, View: "Menus open in front of you", "Messages next to things".

| What                 | Control                                                                  |
|----------------------|--------------------------------------------------------------------------|
| Point                | Either controller (the last trigger pulled): the rust beam, a dot where it lands; over something a pull can choose the dot grows and the hand ticks |
| Choose               | Trigger on the pointed item (hovering alone changes nothing: the game acts on the click); or A. Where the game moves a selection (the inventory's tabs: Maps, Memos, Options), the first pull moves it there and the second opens it |
| Move the selection   | Left stick (as the d-pad)                                                |
| Back / cancel        | B                                                                        |
| Map zoom             | Right grip                                                               |
| Pause menu           | Left menu button, a short press                                          |
| VR menu              | Left menu button held 1 second, anywhere: in play (the game pauses under it), cutscenes, menus, the title |

## The VR menu

| What                | Control                                                                   |
|---------------------|---------------------------------------------------------------------------|
| Pick a row, change it | Point and pull the trigger: the value's left half steps down, right half up |
| Without the pointer | Left stick up / down: the row; left / right: the value; A: change or next page |
| Close               | B, Y, or the left menu button (a short press, or held again)              |

The game gets none of the controllers while the VR menu is open. Opened in play, the game's pause is under it when it
closes: a short press of the left menu button goes back to play. Opening and closing it each play a sound.
