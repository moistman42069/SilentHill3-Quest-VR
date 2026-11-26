"""Writes tools/sim/check256.txt (session 18, 0.1.3's combined check; the same script on build 207 = 0.1.2 is the
baseline). About 3 minutes. Run with QUEST_SIM_HEADSET=1 QUEST_SIM_ALIGN=7312 QUEST_SIM_GRIP_TILT=50 SH3_EQUIP=4
SH3_KEYS="22:Return,25:Return,28:Return,31:Return,34:Return", from a 0.1.2 sh3vr.ini (no "defaults" line: drifting fog
on, snap turning, sharpening, smooth edges, the comfort vignette), so 0.1.3 puts its defaults in once.

1. Still frames (7400-7402, as vr256 on build 207 with the same ini): the fuzz, frame to frame (owner 84), and the
   picture's brightness against 0.1.2's (the second grain drawn as its average keeps the game's look).
2. Melee with the knife (owner 84: "im just simply moving the knife arm gently/arm in general, and melee
   animation/sound happens"): a gentle wave (0.012 m a frame, 0.86 m/s at the rig's 72 Hz), then moderate moves
   (0.025 m a frame, 1.8 m/s: over 0.1.2's knife speed of 1.75), then the same moderate moves with every frame shown
   late ("late 1": two display periods a frame, as the owner's 50-57 frames a second at 120 Hz; the real speed is half,
   0.9 m/s, which 0.1.2 read as 1.8), then real swings (0.08 m a frame, 5.8 m/s). 0.1.3: only the real swings count.
   The late spell also tries dynamic resolution (a step down that does not make the rig's frames quicker is taken
   back).
3. Her shadow from the hand in the flashlight's beam, on the wall and the floor (owner 84: "a bit overbearing").
4. Smooth turning (owner 84: "smooth turn should be on by default"): the right stick held 0.5 s.
5. The VR menu (owner 87: "the vr menu doesnt seem to say \"dynamic cutscenes\" for the cutscene type"): held open in
   play, the stick up to the page tabs and over to View, where "Cutscenes" reads "Dynamic (her eyes or the camera)";
   B closes it, a short press of the menu button goes back to play.
6. Still frames again, then quit (the quit's log line: no exit-time teardown).
Run: C:/SH3Quest/venv/Scripts/python.exe -I tools/sim/make_check256.py (from the repository's root)."""
import pathlib

here = pathlib.Path(__file__).resolve().parent
ev = []


def at(f, s):
    ev.append((f, s))


def glide(f0, x0, x1, step, axis="rh_x"):
    """The right hand from x0 to x1 by `step` metres a frame; returns the frame after the last."""
    n = max(1, round(abs(x1 - x0) / step))
    for i in range(1, n + 1):
        at(f0 + i - 1, f"{axis} {x0 + (x1 - x0) * i / n:.4f}")
    return f0 + n


at(7318, "shot c07318-start")
at(7400, "shot c07400-still-a")
at(7401, "shot c07401-still-b")
at(7402, "shot c07402-still-c")
# 2. melee: the hand a little forward and up, in front of her
at(7405, "rh_y 0.10")
at(7405, "rh_z -0.10")
f = 7410
for _ in range(2):  # the gentle wave
    f = glide(f, -0.15, 0.15, 0.012)
    f = glide(f + 3, 0.15, -0.15, 0.012)
f += 15
at(f, "shot c-moderate-start")
for _ in range(3):  # moderate moves, 0.3 m each, a pause between
    f = glide(f, -0.15, 0.15, 0.025) + 12
    f = glide(f, 0.15, -0.15, 0.025) + 12
at(f, "late 1")
f += 5
for _ in range(3):  # the same, every frame late
    f = glide(f, -0.15, 0.15, 0.025) + 12
    f = glide(f, 0.15, -0.15, 0.025) + 12
f += 60
at(f, "late 0")
f += 30
at(f, "shot c-real-swings")
for _ in range(3):  # real swings, 0.48 m in 6 frames
    f = glide(f, -0.24, 0.24, 0.08) + 18
    f = glide(f, 0.24, -0.24, 0.08) + 18
# 3. her shadow: the hand in the beam before her chest (tools/sim/shadow.txt), then the eyes on the floor
f += 10
at(f, "rh_x -0.2")
at(f, "rh_y 0.06")
at(f, "rh_z -0.15")
at(f, "hand_pitch 20")
at(f + 20, "shot c-shadow-hand-wall")
at(f + 25, "head_pitch -55")
at(f + 45, "shot c-shadow-floor")
at(f + 50, "head_pitch 0")
at(f + 50, "rh_x 0")
at(f + 50, "rh_y 0")
at(f + 50, "rh_z 0")
at(f + 50, "hand_pitch 0")
f += 70
# 4. smooth turning: the right stick over for 36 frames (0.5 s)
at(f, "shot c-before-turn")
at(f + 5, "right_x 0.9")
at(f + 41, "right_x 0")
at(f + 45, "shot c-after-turn")
f += 60
# 5. the VR menu's View page: held open (1.25 s), up to the tabs, right to View, the picture, B, a short press
at(f, "menu 1")
at(f + 90, "menu 0")
at(f + 100, "right_y 1")
at(f + 104, "right_y 0")
at(f + 110, "right_x 1")
at(f + 114, "right_x 0")
at(f + 125, "shot c-vr-menu-view")
at(f + 130, "b 1")
at(f + 134, "b 0")
at(f + 145, "menu 1")
at(f + 151, "menu 0")
f += 190
# 6. still frames, quit
at(f, "shot c-still-d")
at(f + 1, "shot c-still-e")
at(f + 20, "quit 0")

lines = []
for fr, s in sorted(ev, key=lambda e: e[0]):
    if s.startswith("shot c-"):
        s = f"shot c{fr:05d}-" + s[len("shot c-"):]
    lines.append(f"{fr} {s}")
head = __doc__.split("Run:")[0].strip().splitlines()
out = ["# SH3 Quest VR, the PC rig " + head[0].replace("Writes tools/sim/check256.txt ", "")]
out += ["# " + h if h else "#" for h in head[1:]]
(here / "check256.txt").write_text("\n".join(out + lines) + "\n", encoding="utf8", newline="\n")
print(len(lines), "lines, last frame", lines[-1])
