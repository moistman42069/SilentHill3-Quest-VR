"""Writes tools/sim/door231.txt (session 16): door229's knife swings (kill the hall's dog first, SH3_STEER_AT_ATTACKER)
then door230's route round the circular hall to its far door (event 48), the left hand held ahead and closing every
30 frames from 9300 to 12500, pictures every 300 frames, quitting at 12600 (vr236: the last point 24400 18170, vr235
stood 0.15 m short; then a point 0.9 m into the hall, 16450 19300, before point 4: vr236 was stuck in the doorway at
15964 19387 heading straight for point 4).
Run: C:/SH3Quest/venv/Scripts/python.exe -I tools/sim/make_door231.py (from the repository's root)."""
import pathlib

here = pathlib.Path(__file__).resolve().parent
swings = []
for line in (here / "door229.txt").read_text(encoding="utf8").splitlines():
    if not line or line.startswith("#"):
        continue
    frame, rest = line.split(" ", 1)
    f = int(frame)
    if f >= 8250 and rest.startswith("rh_"):
        swings.append((f, rest))
last = max(f for f, _ in swings)
# the swings again until 14200 (the 200-frame pattern of door229 from 8250 repeated)
pattern = [(f - 8250, r) for f, r in swings if 8250 <= f < 8450]
f0 = last + 10
while f0 < 12500:
    for d, r in pattern:
        if f0 + d < 12500:
            swings.append((f0 + d, r))
    f0 += 200

events = [
    (7740, "left_y 1"), (8100, "left_y 0"), (8105, "shot j08105-at-the-door"),
    (8110, "lh_x 0.200"), (8110, "lh_y 0.100"), (8110, "lh_z -0.300"), (8120, "left_grip 1"), (8126, "left_grip 0"),
    (8140, "lh_x 0"), (8140, "lh_y 0"), (8140, "lh_z 0"), (8200, "shot j08200-second-area"), (8200, "left_y 1"),
    (9300, "lh_x 0.200"), (9300, "lh_y 0.100"), (9300, "lh_z -0.300"),
]
for f in range(9310, 12500, 30):
    events += [(f, "left_grip 1"), (f + 5, "left_grip 0")]
for f in range(8400, 12500, 300):
    events.append((f, f"shot j{f:05d}-route"))
events += [(12520, "left_y 0"), (12560, "shot j12560-end"), (12600, "quit 0")]
events += swings
events.sort(key=lambda e: e[0])

header = """# SH3 Quest VR, the PC rig (session 16, build 198+), about 5 minutes. Made by tools/sim/make_door231.py. Run with
# QUEST_SIM_HEADSET=1 QUEST_SIM_ALIGN=7312 QUEST_SIM_GRIP_TILT=50 SH3_KEYS="22:Return,25:Return,28:Return,31:Return,34:Return"
# SH3_STEER_TO="-14500,21000;-13700,22500;-13406,23244;16450,19300;16541,17832;17472,16372;18932,15441;20300,15200;21668,15441;23128,16372;24059,17832;24550,18170;17500,-19640;22200,-19000"
# SH3_STEER_AT_ATTACKER=1 SH3_EVENT_SCAN=8100 SH3_EVENT_SCAN_EVERY=120 SH3_HAND_RAY_LOG=8 SH3_HP=100
# LIST 45, the hall's far door by hand, with LIST 27's knife first: vr234 (door230) was held at the hall's start by
# its dog all run; vr233 (door229) killed the dog with the knife at frame 9817 and walked on. So: the entrance door by
# hand, the knife swung across in front of her (door229's pattern) while the steer stands her at the dog, then the
# route round the hall to the far door (event 48, at 24330 18200), the left hand ahead closing every 30 frames.
"""
out = here / "door231.txt"
out.write_text(header + "\n".join(f"{f} {r}" for f, r in events) + "\n", encoding="utf8", newline="\n")
print(f"{out}: {len(events)} lines, the last at {events[-1][0]}")
