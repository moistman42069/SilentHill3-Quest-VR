"""Writes tools/sim/melee259.txt (session 18, owner 89: "melee should be fixed for eveyrthing, not just knife ofc"):
melee258 again for the kinds whose rules changed after it (a heavy weapon and a gun as a club ask a shorter fast path:
vr258's heavy weapons counted only the 48 cm swing, not the 40 cm one) and the stun gun, which vr258 never had
(item 9, not 8). About 3 minutes of play. Run with QUEST_SIM_HEADSET=1 QUEST_SIM_ALIGN=7312 QUEST_SIM_GRIP_TILT=50
SH3_KEYS="22:Return,25:Return,28:Return,31:Return,34:Return" SH3_GIVE="10:30,9:4,2:0,3:0,4:0"
SH3_EQUIP="5,6,7,8,1" SH3_EQUIP_EVERY=560: the steel pipe, the katana, the maul (heavy), the stun gun (a jab), the
handgun (a gun as a club), 560 frames each.

Each weapon: a gentle wave (0.012 m a frame, 0.86 m/s at the rig's 72 Hz), moderate moves (0.025 m a frame, 1.8 m/s),
the same moderate moves with every frame shown late, then two real swings (0.08 m a frame, 5.8 m/s; the first 40 cm,
the second 48 cm) 1.5 s apart. 0.1.3: only the real swings count, both of them, for every kind (sh3.log "the right
<weapon>'s swing counts", two a weapon); none for the wave or the moderate moves.
Run: C:/SH3Quest/venv/Scripts/python.exe -I tools/sim/make_melee259.py (from the repository's root)."""
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


START, EVERY = 7315, 560  # play starts about rt frame 7286, the script's frames about 29 ahead (vr257, vr258)
NAMES = ["steel-pipe", "katana", "maul", "stun-gun", "handgun"]
at(START - 10, "shot m-start")
for k, name in enumerate(NAMES):
    f = START + EVERY * k + 60  # the weapon out of its holster first
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
out = ["# SH3 Quest VR, the PC rig " + head[0].replace("Writes tools/sim/melee259.txt ", "")]
out += ["# " + h if h else "#" for h in head[1:]]
(here / "melee259.txt").write_text("\n".join(out + lines) + "\n", encoding="utf8", newline="\n")
print(len(lines), "lines, last frame", lines[-1])
