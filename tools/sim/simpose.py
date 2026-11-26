import math, sys

# The sim's hand poses (xr_sim.cpp): hand = yaw(Y) * pitch(X) * roll(Z); grip = hand * tiltX(50).
# Grip space (sh3vr_body.cpp): open fingers -Y, palm out -X (right hand) / +X (left), thumb side -Z.

def qmul(a, b):
    ax, ay, az, aw = a
    bx, by, bz, bw = b
    return (aw*bx + ax*bw + ay*bz - az*by, aw*by - ax*bz + ay*bw + az*bx, aw*bz + ax*by - ay*bx + az*bw,
            aw*bw - ax*bx - ay*by - az*bz)

def rot(q, v):
    x, y, z, w = q
    p = (v[0], v[1], v[2], 0.0)
    c = (-x, -y, -z, w)
    r = qmul(qmul(q, p), c)
    return r[:3]

def axis_q(axis, deg):
    h = math.radians(deg) / 2
    s = math.sin(h)
    return (axis[0]*s, axis[1]*s, axis[2]*s, math.cos(h))

def grip(yaw, pitch, roll, tilt=50):
    hand = qmul(qmul(axis_q((0, 1, 0), yaw), axis_q((1, 0, 0), pitch)), axis_q((0, 0, 1), roll))
    return qmul(hand, axis_q((1, 0, 0), tilt))

def dot(a, b):
    return sum(x*y for x, y in zip(a, b))

def search(side, palm_want, fingers_want):
    mx = -1 if side == "left" else 1
    best = None
    for yaw in range(-180, 181, 5):
        for pitch in range(-90, 91, 5):
            for roll in range(-180, 181, 5):
                g = grip(yaw, pitch, roll)
                palm = rot(g, (-mx, 0, 0))
                fing = rot(g, (0, -1, 0))
                s = dot(palm, palm_want) + 0.7 * dot(fing, fingers_want)
                if best is None or s > best[0]:
                    best = (s, yaw, pitch, roll, palm, fing)
    return best

if sys.argv[1] == "search":
    side = sys.argv[2]
    pw = tuple(float(v) for v in sys.argv[3:6])
    fw = tuple(float(v) for v in sys.argv[6:9])
    s, yaw, pitch, roll, palm, fing = search(side, pw, fw)
    print(f"{side}: hand_yaw {yaw} hand_pitch {pitch} hand_roll {roll}: palm {palm[0]:.2f} {palm[1]:.2f} {palm[2]:.2f}, "
          f"fingers {fing[0]:.2f} {fing[1]:.2f} {fing[2]:.2f}")
elif sys.argv[1] == "offset":
    # offset side head_yaw head_y target_x target_y target_z: the lh_/rh_ offsets putting the hand there, and its
    # distance from her shoulder (sh3vr_body.cpp shoulder(): level eyes + yaw * (+-0.18, -0.24, 0.06))
    side, yaw, hy = sys.argv[2], float(sys.argv[3]), float(sys.argv[4])
    tx, ty, tz = (float(v) for v in sys.argv[5:8])
    q = axis_q((0, 1, 0), yaw)
    sx = -1 if side == "left" else 1
    head = (0.0, 1.7 + hy, 0.0)
    base = rot(q, (0.2 * sx, -0.35, -0.3))
    sh = rot(q, (0.18 * sx, -0.24, 0.06))
    off = (tx - head[0] - base[0], ty - head[1] - base[1], tz - head[2] - base[2])
    d = math.dist((tx, ty, tz), (head[0] + sh[0], head[1] + sh[1], head[2] + sh[2]))
    p = "lh" if side == "left" else "rh"
    print(f"{p}_x {off[0]:.3f} {p}_y {off[1]:.3f} {p}_z {off[2]:.3f}: {d:.2f} m from the shoulder")
