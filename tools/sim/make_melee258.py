"""Writes tools/sim/melee258.txt (session 18, owner 89: "melee should be fixed for eveyrthing, not just knife ofc"):
0.1.3's swing rules with every kind of melee, after check256 (vr257) did the knife. About 4 minutes of play. Run with
QUEST_SIM_HEADSET=1 QUEST_SIM_ALIGN=7312 QUEST_SIM_GRIP_TILT=50 SH3_KEYS="22:Return,25:Return,28:Return,31:Return,34:Return"
SH3_GIVE="10:30,1:0,2:0,3:0,4:0,5:0,6:0,7:0,8:0" SH3_EQUIP="0,5,6,7,8,1" SH3_EQUIP_EVERY=560: empty hands (punches),
the steel pipe, the katana, the maul (heavy), the stun gun (a jab), the handgun (a gun as a club), 560 frames each.

Each weapon: a gentle wave (0.012 m a frame, 0.86 m/s at the rig's 72 Hz), moderate moves (0.025 m a frame, 1.8 m/s),
the same moderate moves with every frame shown late ("late 1": the real speed half), then two real swings (0.08 m a
frame, 5.8 m/s) 1.5 s apart (a heavy weapon's attack takes 1.3 s). 0.1.3: only the real swings count, for every kind
(sh3.log "the right <weapon>'s swing counts", two a weapon). The empty hands' turn first has her shadow, A/B: the hand
in the flashlight's beam (as check256), then lowered, the head still, so the pictures differ only by the hand and its
shadow (0.1.3: half the game's darkening).
Run: C:/SH3Quest/venv/Scripts/python.exe -I tools/sim/make_melee258.py (from the repository's root)."""
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


START, EVERY = 7315, 560  # play starts about rt frame 7286, the script's frames about 29 ahead (vr257)
NAMES = ["empty", "steel-pipe", "katana", "maul", "stun-gun", "handgun"]
at(START - 10, "shot m-start")
for k, name in enumerate(NAMES):
    f = START + EVERY * k + 60  # the weapon out of its holster first
    if k == 0:
        # her shadow, A/B: the hand in the beam before her chest, then lowered; the head still
        at(f, "rh_x -0.2")
        at(f, "rh_y 0.06")
        at(f, "rh_z -0.15")
        at(f, "hand_pitch 20")
        at(f + 20, "shot m-shadow-hand-in-beam")
        at(f + 22, "rh_y -0.45")
        at(f + 22, "rh_x 0.15")
        at(f + 40, "shot m-shadow-hand-lowered")
        at(f + 42, "rh_x 0")
        at(f + 42, "hand_pitch 0")
        f += 50
    at(f, "rh_y 0.10")
    at(f, "rh_z -0.10")
    at(f, "rh_x -0.15")
    f += 3
    f = glide(f, -0.15, 0.15, 0.012)  # the gentle wave
    f = glide(f + 3, 0.15, -0.15, 0.012) + 5
    at(f, f"shot m-{name}-moderate")
    for _ in range(2):  # moderate moves
        f = glide(f, -0.15, 0.15, 0.025) + 12
        f = glide(f, 0.15, -0.15, 0.025) + 12
    at(f, "late 1")
    f += 5
    f = glide(f, -0.15, 0.15, 0.025) + 12  # the same, every frame late
    f = glide(f, 0.15, -0.15, 0.025) + 12
    at(f, "late 0")
    f += 30
    at(f, f"shot m-{name}-swing")
    f = glide(f, -0.24, 0.24, 0.08) + 100  # two real swings, 1.4 s apart
    f = glide(f, 0.24, -0.24, 0.08) + 100
    assert f < START + EVERY * (k + 1) + 40, (name, f)
    at(f, "rh_x 0")
    at(f, "rh_y 0")
    at(f, "rh_z 0")
f = START + EVERY * len(NAMES) + 20
at(f, "shot m-end")
at(f + 20, "quit 0")

lines = []
for fr, s in sorted(ev, key=lambda e: e[0]):
    if s.startswith("shot m-"):
        s = f"shot m{fr:05d}-" + s[len("shot m-"):]
    lines.append(f"{fr} {s}")
head = __doc__.split("Run:")[0].strip().splitlines()
out = ["# SH3 Quest VR, the PC rig " + head[0].replace("Writes tools/sim/melee258.txt ", "")]
out += ["# " + h if h else "#" for h in head[1:]]
(here / "melee258.txt").write_text("\n".join(out + lines) + "\n", encoding="utf8", newline="\n")
print(len(lines), "lines, last frame", lines[-1])
