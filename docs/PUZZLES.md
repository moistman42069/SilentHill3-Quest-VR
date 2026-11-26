# SH3 Quest VR: the puzzles in VR (LIST 31, 32)

How every kind of puzzle and interaction in Silent Hill 3 works in VR, and the list of the game's puzzles to check one
by one in the playthrough (LIST 38). Status 2026-11-24: the ways below are written in the code; no puzzle has been
played in VR yet (no build until the list is done). Each row gets its check mark, or its fix, in the playthrough.

## The kinds of puzzle, and how VR handles each

| Kind | What the game shows | In VR |
|------|---------------------|-------|
| Puzzle screens (keypads, the clock's hands, the books' order, dials, picture puzzles) | A 2D screen with the game's cursor or a selection moved by the pad | The screen on the floating panel (LIST 11); the pointer's beam moves the game's own cursor through its mouse support (user32.c `sh3vr_mouse`) and the trigger presses; where a screen only takes the pad, the left stick and A work as on the pad. A haptic tick per entry. |
| Examining things in the world (notes, shelves, doors, switches) | "Action" when Heather faces it | A (as the pad), or reach out and close the hand on it: the game's own action check runs at the palm instead of in front of her ("Take items with your hand", LIST 45; GAME_FACTS.md, Events). Never only one of them. |
| Using an item on something (keys, the puzzle items) | The inventory: choose the item, Use | The inventory on the panel with the pointer: point at the item to turn the ring to it, trigger to use. |
| Picking items up | "Action" near the item | Close the hand on it (her fingers close round it, the game's own pick-up runs there, a haptic), or A. |
| Doors, drawers, lockers, cabinets | "Action" at the door or the furniture | Close the hand on the handle: the game's event for what is at the hand (the door opens or loads the next room, the drawer or locker is searched). |
| Memos and the map | 2D screens | The panel, the pointer; the map's zoom on the right grip. |
| Choosing in conversations, Yes / No prompts | The game's 2D prompt | The panel and the pointer, or the stick and A. |
| Timed or chased sequences | The game runs on | Third person where the game's camera is needed (Third person where the game needs it, LIST 35). |

Rules held by the controls (docs/CONTROLS.md): nothing a puzzle needs is only reachable one way, and nothing VR adds
takes a button a puzzle screen uses (the VR menu opens only on a held menu button, outside play).

How the hand finds things (LIST 45, src/game/sh3.c `sub_004B9940`): the game's action check makes a record of
Heather's place, her facing and a point 31 cm in front of her, and tests every event region of the room against it.
While a hand closes (grip, reaching out 25 cm or down 60 cm, not holding a weapon or over a holster) the record's
front point is the palm and her facing points at it, for 4 frames, with the action button held for 3. Whatever the
game finds there runs as if she had faced it: pick-ups, doors, examining. The log says "VR: the left / right hand used
event N at x y z". Each kind of spot is to be checked in the playthrough (the table below); kinds whose regions
depend on her own place rather than the front point stay on A as before.
Since build 175 (session 14) a hand reaches over 40 cm out from the eyes, or down under 55 cm over the floor; the ray
goes from her place toward the palm, to 20 cm past it, at least 0.5 m and never past the game's own 1.02 m, so the
hand works a thing exactly where the button would, never farther. A light tick when it comes onto something it can
use ("the left hand can use event N"), the game's action when it closes.
Checked in the sim (session 15): a door both ways (vr222: the nightmare's entrance, event 5, and back, event 4); an
examine (vr206: Robbie, event 8); the circular hall's far door (event 48) found by the hand at 24323 18169 (vr226,
"can use event 48"). The game's door regions are lines a little behind the door's face (event 48's at x 24809 with
the door's face about 0.8 m nearer): the hand finds them from about 1 m away, as the button does.

## The game's puzzles (to check in the playthrough)

The list is by area. "Way" is the kind from the table above. Fill in "Checked" with the date and how, or what was
fixed. Names follow the game's own where known; anything not yet confirmed in the game is marked (to confirm).

| Area | Puzzle | Way | Checked |
|------|--------|-----|---------|
| Central Square Shopping Center | The bookstore's Shakespeare anthology (the books put back in order) | Puzzle screen | |
| Central Square Shopping Center | Keypads and locked doors found on the way (to confirm) | Puzzle screen / item | |
| Hazel Street Station | The way through the station (tickets, gates, the train) (to confirm) | Examine / item | |
| Office building and construction site | Locks and keypads (to confirm) | Puzzle screen / item | |
| Heather's apartment | No puzzle (story) | — | |
| Brookhaven Hospital | The hospital's keypads and codes from its notes; the clock (LIST 31) (to confirm each) | Puzzle screen | |
| Lakeside Amusement Park | The park's puzzles and the chase sections (to confirm) | Puzzle screen / third person | |
| The church | The church's puzzles before the end (to confirm) | Puzzle screen / item | |
| Every area | Riddle difficulty: the game's own setting changes the codes, not the way they are entered | — | |

The playthrough (LIST 38) is where each one is played: save files at each puzzle go in C:\SH3Quest\gamedir\savedata
for the PC rig's scripted runs (SH3_KEYS), and the headset-only parts go in docs/HEADSET_CHECKLIST.md.
