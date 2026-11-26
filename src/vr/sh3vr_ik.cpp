// SPDX-License-Identifier: GPL-3.0-or-later
// SH3 Quest VR: Heather's whole body in first person, on her own 73 bones (LIST 19, 41, 42, 43; footsteps for 44).
//
// The game poses a skinned model's bones into one array before drawing it (0x45EC00 fills [0x6B4540] with each bone's
// skeleton matrix times the model's world matrix: a D3D row-vector 4x4 a bone, rows 0-2 its axes in the world, row 3
// its origin; docs/GAME_FACTS.md, Bones). src/game/sh3.c hands that array here between the game's pose and anything
// that reads it (0x45EC90's blends, the parts' palettes), so her parts, her shadow and any reflection show one body.
// Each frame in first person, on a copy of the game's pose:
//
//   1. lean: her spine (bone 2 and all above it) turned so her neck goes to the player's neck (rig.h neck_of: the eyes
//      15 cm ahead of it, so looking down never takes the eyes into her; 35 degrees at most,
//      eased; not while she walks);
//   2. crouch: what the player's neck is lower than hers lowers her whole body (0.55 m at most, eased, a 2 cm dead zone,
//      never raised; not seated); a controller past her arm's reach bends her at the waist toward it (20 degrees at
//      most standing, 85 deeply crouched; the least that brings the shoulder within reach; not walking);
//   3. her head and neck turned as the headset (40% neck, 60% head, 70 degrees at most): her shadow and mirrors;
//   4. legs: each hip -> knee -> ankle solved to its foot's target, the knee to the side the animation bends it, the
//      foot turned back to its target. Standing, the feet stay planted in the world and step (0.25 s, a 5 cm lift, one
//      at a time) once the body has turned or moved 12 cm / 25 degrees from them; walking, the animation's feet;
//   5. arms (Jak's, refined): the collar lifts the shoulder with a raised hand and reaches with a far one (6 cm at
//      most); shoulder -> elbow -> wrist solved to her hand's place, the elbow toward where the controller's forearm
//      points (eased in her chest's axes), the arm stretching up to 1.12 times; past that her hand stays at the
//      arm's end, turned as the controller (no ghost there: owner message 45, the ghost only for a hand pushed into a
//      surface);
//      the hand's twist shared half with the forearm's twist bone;
//   6. hands: her own hand turned as the controller is (her wrist-to-knuckles and palm on the grip's), her middle
//      knuckle at the grip's middle knuckle; each finger's three joints closed by the curls the controls ask, stopped by
//      the world (sh3vr_body.cpp), on the joints and lengths of her own fingers (her model's rest pose; the probes use
//      the same shapes);
//   7. the fades (Jak's): her trunk when the eyes are inside it or her chest bone comes within 21 cm of them (looking
//      down far; 55 degrees keeps her whole), or the
//      game holds her turned over (knocked down, grabbed), a leg when its knee or ankle comes to the eyes
//      or above its hip: out in 0.08 s, held 0.35 s, back in 0.28 s, dithered in the draw (d3d8_shader.c through
//      body_cut); her head alone (her hair) fades whole; what is left of her 7-16 cm from the eyes fades too; her
//      hands never.
//
// The finger-skinned hands are her model's own (groups 3 and 4, shown in scenes): sh3.c's part draw (0x45ED40) shows
// them in first person in place of the rigid hands play swaps in. A hand pushed through a wall stays at the wall
// (sh3vr_body.cpp's probes) and a faint ghost of her own hand shows where the controller is (a hand past her reach
// stays on her arm with no ghost: owner message 45, session 11); the ghost is
// a second solve on a copy (ghost_bones), drawn by sh3.c as a second pass of that hand alone.

#include "sh3vr_internal.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <vector>

namespace sh3q {

int arms_ik_frame = -1;
Vec3 heather_head;
int heather_head_frame = -1;
Vec3 game_head;          // LIST 48: her head as the game posed it (no VR turn or IK), world
int game_head_frame = -1;
HerHand her_hands[2];

namespace {

constexpr int kN = 73;
constexpr int kUpperRoot = 0, kPelvis = 1, kSpine = 2, kChest = 4, kNeck = 6, kHead = 9;
constexpr float kU = kUnitsPerMetre;
const float kJointDeg[3] = {80.0f, 95.0f, 65.0f};   // a finger's three joints at a full fist
const float kThumbDeg[3] = {25.0f, 40.0f, 35.0f};

// ---------------------------------------------------------------- bones (D3D row-vector: rows = axes, o = origin)

struct Bone {
    Vec3 r[3];
    Vec3 o;
};

Bone load(const float* m) {
    return {{{m[0], m[1], m[2]}, {m[4], m[5], m[6]}, {m[8], m[9], m[10]}}, {m[12], m[13], m[14]}};
}
// A matrix stored the other way round (columns as rows; the origin in the fourth column).
Bone load_transposed(const float* m) {
    return {{{m[0], m[4], m[8]}, {m[1], m[5], m[9]}, {m[2], m[6], m[10]}}, {m[3], m[7], m[11]}};
}
void store(float* m, const Bone& b) {
    for (int i = 0; i < 3; i++) m[i * 4] = b.r[i].x, m[i * 4 + 1] = b.r[i].y, m[i * 4 + 2] = b.r[i].z, m[i * 4 + 3] = 0;
    m[12] = b.o.x, m[13] = b.o.y, m[14] = b.o.z, m[15] = 1;
}
Vec3 apply_dir(const Bone& b, Vec3 l) { return b.r[0] * l.x + b.r[1] * l.y + b.r[2] * l.z; }
Vec3 apply(const Bone& b, Vec3 l) { return apply_dir(b, l) + b.o; }
Vec3 local_dir(const Bone& b, Vec3 w) { return {dot(w, b.r[0]), dot(w, b.r[1]), dot(w, b.r[2])}; }
// rel * parent: a bone given in its parent's axes, in the world.
Bone compose(const Bone& rel, const Bone& parent) {
    return {{apply_dir(parent, rel.r[0]), apply_dir(parent, rel.r[1]), apply_dir(parent, rel.r[2])}, apply(parent, rel.o)};
}
// child * inverse(parent), for an orthonormal parent: the child in its parent's axes.
Bone relative(const Bone& child, const Bone& parent) {
    return {{local_dir(parent, child.r[0]), local_dir(parent, child.r[1]), local_dir(parent, child.r[2])},
            local_dir(parent, child.o - parent.o)};
}
Bone rigid_inverse(const Bone& b) {
    Bone r;
    r.r[0] = {b.r[0].x, b.r[1].x, b.r[2].x};
    r.r[1] = {b.r[0].y, b.r[1].y, b.r[2].y};
    r.r[2] = {b.r[0].z, b.r[1].z, b.r[2].z};
    r.o = Vec3{dot(b.o, b.r[0]), dot(b.o, b.r[1]), dot(b.o, b.r[2])} * -1.0f;
    return r;
}
const Bone kIdentity{{{1, 0, 0}, {0, 1, 0}, {0, 0, 1}}, {0, 0, 0}};

// ---------------------------------------------------------------- turns

Quat qnorm(Quat q) {
    const float n = std::sqrt(q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w);
    return n > 1e-9f ? Quat{q.x / n, q.y / n, q.z / n, q.w / n} : Quat{};
}
Quat axis_angle(Vec3 axis, float a) {
    axis = normalized(axis);
    const float s = std::sin(a * 0.5f);
    return {axis.x * s, axis.y * s, axis.z * s, std::cos(a * 0.5f)};
}
float quat_angle(Quat q) { return 2.0f * std::acos(std::clamp(std::fabs(q.w), 0.0f, 1.0f)); }
Quat slerp(Quat a, Quat b, float t) {
    float d = a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;
    if (d < 0) b = {-b.x, -b.y, -b.z, -b.w}, d = -d;
    if (d > 0.9995f) return qnorm({a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t, a.w + (b.w - a.w) * t});
    const float th = std::acos(d), s = std::sin(th);
    const float wa = std::sin((1 - t) * th) / s, wb = std::sin(t * th) / s;
    return {a.x * wa + b.x * wb, a.y * wa + b.y * wb, a.z * wa + b.z * wb, a.w * wa + b.w * wb};
}
Quat limit_turn(Quat q, float max_angle) {
    const float a = quat_angle(q);
    return a > max_angle && a > 1e-6f ? slerp(Quat{}, q, max_angle / a) : q;
}
// The shortest turn taking direction a onto b.
Quat arc(Vec3 a, Vec3 b) {
    a = normalized(a);
    b = normalized(b);
    const float d = dot(a, b);
    if (d < -0.9999f) {
        Vec3 axis = cross(a, Vec3{1, 0, 0});
        if (length(axis) < 1e-3f) axis = cross(a, Vec3{0, 1, 0});
        axis = normalized(axis);
        return Quat{axis.x, axis.y, axis.z, 0.0f};
    }
    const Vec3 c = cross(a, b);
    return qnorm(Quat{c.x, c.y, c.z, 1.0f + d});
}
// The turn whose matrix has columns x, y, z (orthonormal).
Quat quat_of_columns(Vec3 x, Vec3 y, Vec3 z) {
    const float m00 = x.x, m01 = y.x, m02 = z.x, m10 = x.y, m11 = y.y, m12 = z.y, m20 = x.z, m21 = y.z, m22 = z.z;
    const float tr = m00 + m11 + m22;
    Quat q;
    if (tr > 0) {
        const float s = std::sqrt(tr + 1.0f) * 2.0f;
        q = {(m21 - m12) / s, (m02 - m20) / s, (m10 - m01) / s, 0.25f * s};
    } else if (m00 > m11 && m00 > m22) {
        const float s = std::sqrt(1.0f + m00 - m11 - m22) * 2.0f;
        q = {0.25f * s, (m01 + m10) / s, (m02 + m20) / s, (m21 - m12) / s};
    } else if (m11 > m22) {
        const float s = std::sqrt(1.0f + m11 - m00 - m22) * 2.0f;
        q = {(m01 + m10) / s, 0.25f * s, (m12 + m21) / s, (m02 - m20) / s};
    } else {
        const float s = std::sqrt(1.0f + m22 - m00 - m11) * 2.0f;
        q = {(m02 + m20) / s, (m12 + m21) / s, 0.25f * s, (m10 - m01) / s};
    }
    return qnorm(q);
}
Quat quat_of(const Bone& b) { return quat_of_columns(normalized(b.r[0]), normalized(b.r[1]), normalized(b.r[2])); }
void set_turn(Bone& b, Quat q) {
    b.r[0] = rotate(q, {1, 0, 0});
    b.r[1] = rotate(q, {0, 1, 0});
    b.r[2] = rotate(q, {0, 0, 1});
}
// A frame from its forward and up (the same construction on both sides of a comparison).
Quat frame_quat(Vec3 fwd, Vec3 up) {
    const Vec3 z = normalized(fwd);
    Vec3 x = normalized(cross(up, z));
    if (length(x) < 0.5f) x = normalized(cross(Vec3{0, 0, 1}, z));
    const Vec3 y = cross(z, x);
    return quat_of_columns(x, y, z);
}
float ease(float rate) { return 1.0f - std::exp(-rate * frame_dt()); }
float smooth01(float t) {
    t = std::clamp(t, 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

// ---------------------------------------------------------------- her skeleton (checked once a model)

enum PartClass : uint8_t { kTrunkC = 0, kHeadC, kLegLeftC, kLegRightC, kArmC, kHandC, kFingerC };

struct Skel {
    const float* rest_from = nullptr;  // the rest matrices it was read from (a new model: looked at again)
    int state = -1;                    // -1 not looked at, 0 not usable (logged), 1 ready
    bool fingers = false;              // her finger chains and rest pose read: her own hands can be shown
    int parent[kN];
    uint8_t sub[kN][kN];               // sub[top][b]: b is top or one of its descendants
    int collar[2], shoulder[2], elbow[2], forearm[2], twin[2], hand[2];  // [kLeft], [kRight]
    int hip[2], knee[2], ankle[2];
    int finger[2][5][3];               // each hand's thumb, index, middle, ring, pinky: base, middle, last joint
    Bone rel[kN];                      // each bone in its parent's axes at rest
    // Her hand in its own bone's axes: wrist to knuckles, out of the palm, their cross; her middle knuckle; each finger's
    // joints and tip at rest, and the way each one closes.
    Vec3 along[2], palm[2], across[2], knuckle[2];
    Vec3 joint[2][5][4];
    Vec3 bend[2][5];
    uint8_t cls[kN];
    const char* rest_kind = "";
} sk;

struct Arm {
    int collar, shoulder, elbow, forearm, twin, hand;
};
const Arm kArmA{11, 26, 34, 37, 40, 39}, kArmB{15, 32, 36, 38, 42, 41};  // A her left, B her right (checked below)
struct Leg {
    int hip, knee, twin, ankle;
};
const Leg kLegA{3, 7, 10, 21}, kLegB{5, 8, 14, 27};

bool chain_ok(const int8_t* p) {
    for (const Arm& a : {kArmA, kArmB})
        if (p[a.collar] != kChest || p[a.shoulder] != a.collar || p[a.elbow] != a.shoulder || p[a.forearm] != a.elbow ||
            p[a.hand] != a.forearm || p[a.twin] != a.forearm)
            return false;
    for (const Leg& l : {kLegA, kLegB})
        if (p[l.hip] != kPelvis || p[l.knee] != l.hip || p[l.twin] != l.knee || p[l.ankle] != l.twin) return false;
    return p[kSpine] == kUpperRoot && p[kChest] == kSpine && p[kNeck] == kChest && p[kHead] == kNeck;
}

Vec3 her_right(float yaw) { return {std::cos(yaw), 0.0f, -std::sin(yaw)}; }
Vec3 her_forward(float yaw) { return {std::sin(yaw), 0.0f, std::cos(yaw)}; }

// The rest pose: which way round the model keeps it (in each bone's parent's axes, in the model's space, or the model
// space's inverse; rows or columns), told by how well each reading gives the bone offsets the game's own animated
// arms, legs and spine show (a bone's place in its parent's axes does not change with the pose).
void read_rest(const float* rest, const Bone* anim) {
    static const int kTest[][2] = {{34, 26}, {37, 34}, {39, 37}, {36, 32}, {38, 36}, {41, 38}, {7, 3},
                                   {21, 10}, {8, 5},   {27, 14}, {4, 2},   {6, 4},   {9, 6}};
    static const char* const kKinds[6] = {"each bone in its parent's axes", "the model's space", "the model's space inverted",
                                          "each bone in its parent's axes (columns)", "the model's space (columns)",
                                          "the model's space inverted (columns)"};
    float best_err = 1e30f;
    int best = 0;
    static Bone cand[kN];
    for (int k = 0; k < 6; k++) {
        for (int b = 0; b < kN; b++) {
            const Bone m = k >= 3 ? load_transposed(rest + 16 * b) : load(rest + 16 * b);
            const int p = sk.parent[b];
            if (k % 3 == 0 || p < 0) cand[b] = k % 3 == 2 ? rigid_inverse(m) : m;
            else if (k % 3 == 1) cand[b] = relative(m, k >= 3 ? load_transposed(rest + 16 * p) : load(rest + 16 * p));
            else cand[b] = relative(rigid_inverse(m), rigid_inverse(k >= 3 ? load_transposed(rest + 16 * p) : load(rest + 16 * p)));
        }
        float err = 0;
        for (const auto& t : kTest) err += length(cand[t[0]].o - relative(anim[t[0]], anim[t[1]]).o);
        err /= (float)(sizeof kTest / sizeof *kTest);
        rt_log("VR body: rest pose read as %s: bone offsets %.2f units off the game's animation on average", kKinds[k],
               err);
        if (err < best_err) best_err = err, best = k;
    }
    for (int b = 0; b < kN; b++) {
        const Bone m = best >= 3 ? load_transposed(rest + 16 * b) : load(rest + 16 * b);
        const int p = sk.parent[b];
        if (best % 3 == 0 || p < 0) sk.rel[b] = best % 3 == 2 ? rigid_inverse(m) : m;
        else if (best % 3 == 1) sk.rel[b] = relative(m, best >= 3 ? load_transposed(rest + 16 * p) : load(rest + 16 * p));
        else sk.rel[b] = relative(rigid_inverse(m), rigid_inverse(best >= 3 ? load_transposed(rest + 16 * p) : load(rest + 16 * p)));
    }
    sk.rest_kind = kKinds[best];
    float worst_axis = 0;
    for (int b = 0; b < kN; b++)
        for (int i = 0; i < 3; i++) worst_axis = std::max(worst_axis, std::fabs(length(sk.rel[b].r[i]) - 1.0f));
    rt_log("VR body: the rest pose is %s (%.2f units off; axes within %.3f of unit length)%s", sk.rest_kind, best_err,
           worst_axis, best_err > 6.0f ? ": TOO FAR, her own fingers are not shown" : "");
    sk.fingers = best_err <= 6.0f && worst_axis < 0.05f;
}

// Her hands' fingers from the rest pose: the chains, which finger is which, her hand's own axes.
bool read_hand(int h) {
    const int hand = sk.hand[h];
    int bases[8], nb = 0;
    for (int b = 0; b < kN; b++)
        if (sk.parent[b] == hand && nb < 8) bases[nb++] = b;
    if (nb != 5) {
        rt_log("VR body: hand bone %d has %d children, not five fingers", hand, nb);
        return false;
    }
    struct F {
        int c[3];
        Vec3 p[4];
    } f[5];
    for (int i = 0; i < 5; i++) {
        f[i].c[0] = bases[i];
        for (int j = 1; j < 3; j++) {
            int child = -1, count = 0;
            for (int b = 0; b < kN; b++)
                if (sk.parent[b] == f[i].c[j - 1]) child = b, count++;
            if (count != 1) {
                rt_log("VR body: finger bone %d has %d children", f[i].c[j - 1], count);
                return false;
            }
            f[i].c[j] = child;
        }
        Bone w = compose(sk.rel[f[i].c[0]], kIdentity);
        f[i].p[0] = w.o;
        for (int j = 1; j < 3; j++) {
            w = compose(sk.rel[f[i].c[j]], w);
            f[i].p[j] = w.o;
        }
        f[i].p[3] = f[i].p[2] + (f[i].p[2] - f[i].p[1]) * 0.8f;  // the fingertip past the last joint
    }
    // The thumb's base is nearest the wrist. The other four go across the hand: each base's place sideways from the
    // line out to their knuckles (the along offsets differ more than the gaps between fingers: by plain distance her
    // ring finger came before the middle one, run vr20), the index finger's nearest the thumb's, then middle, ring,
    // pinky by their distance from the index finger's.
    int order[5], used = 0, thumb = 0;
    for (int i = 1; i < 5; i++)
        if (length(f[i].p[0]) < length(f[thumb].p[0])) thumb = i;
    order[used++] = thumb;
    Vec3 out{};
    for (int i = 0; i < 5; i++)
        if (i != thumb) out = out + f[i].p[0];
    out = normalized(out);
    auto side = [&](int i) { return f[i].p[0] - out * dot(f[i].p[0], out); };
    bool taken[5]{};
    taken[thumb] = true;
    int index = -1;
    for (int i = 0; i < 5; i++)
        if (!taken[i] && (index < 0 || length(side(i) - side(thumb)) < length(side(index) - side(thumb)))) index = i;
    taken[index] = true;
    order[used++] = index;
    for (int k = 2; k < 5; k++) {
        int pick = -1;
        for (int i = 0; i < 5; i++)
            if (!taken[i] && (pick < 0 || length(side(i) - side(index)) < length(side(pick) - side(index)))) pick = i;
        taken[pick] = true;
        order[used++] = pick;
    }
    for (int k = 0; k < 5; k++)
        for (int j = 0; j < 3; j++) sk.finger[h][k][j] = f[order[k]].c[j];
    const Vec3 along = normalized(f[order[kMiddle]].p[0]);
    const Vec3 p2i = f[order[kIndex]].p[0] - f[order[kPinky]].p[0];
    Vec3 palm = normalized(cross(along, p2i) * (h == kRight ? -1.0f : 1.0f));
    palm = normalized(palm - along * dot(palm, along));
    if (length(along) < 0.5f || length(palm) < 0.5f) {
        rt_log("VR body: hand %d's axes could not be found from its fingers", hand);
        return false;
    }
    sk.along[h] = along;
    sk.palm[h] = palm;
    sk.across[h] = cross(along, palm);
    sk.knuckle[h] = f[order[kMiddle]].p[0];
    for (int k = 0; k < 5; k++) {
        for (int j = 0; j < 4; j++) sk.joint[h][k][j] = f[order[k]].p[j];
        const Vec3 dir = normalized(f[order[k]].p[1] - f[order[k]].p[0]);
        Vec3 axis = normalized(cross(dir, palm));
        if (length(axis) < 0.5f) axis = sk.across[h];
        // Closing moves the finger toward the palm's side.
        if (dot(rotate(axis_angle(axis, 0.3f), dir), palm) < dot(dir, palm)) axis = axis * -1.0f;
        sk.bend[h][k] = axis;
    }
    static const char* const kNames[5] = {"thumb", "index", "middle", "ring", "pinky"};
    for (int k = 0; k < 5; k++)
        rt_log("VR body: %s hand's %s: bones %d %d %d, base %.1f %.1f %.1f, segments %.1f %.1f units",
               h == kLeft ? "left" : "right", kNames[k], sk.finger[h][k][0], sk.finger[h][k][1], sk.finger[h][k][2],
               sk.joint[h][k][0].x, sk.joint[h][k][0].y, sk.joint[h][k][0].z,
               length(sk.joint[h][k][1] - sk.joint[h][k][0]), length(sk.joint[h][k][2] - sk.joint[h][k][1]));
    rt_log("VR body: %s hand (bone %d): wrist to middle knuckle %.1f units (%.1f cm)", h == kLeft ? "left" : "right", hand,
           length(sk.knuckle[h]), length(sk.knuckle[h]) / kU * 100.0f);
    return true;
}

// Her finger shapes in each controller's grip space (metres, that hand's own: not mirrored), for the probes: her hand
// is placed so its axes are the grip's and its middle knuckle the grip's (sh3vr_body.cpp kFingers' middle knuckle).
Vec3 grip_knuckle(int h) { return {0.031f * (h == kLeft ? -1.0f : 1.0f), -0.048f, -0.008f}; }
Vec3 to_grip_dir(int h, Vec3 v) {
    const float mx = h == kLeft ? -1.0f : 1.0f;
    const float a = dot(v, sk.along[h]), p = dot(v, sk.palm[h]), c = dot(v, sk.across[h]);
    // along -Y, out of the palm -mx X, across their cross (0, 0, -mx)
    return Vec3{-mx * p, -a, -mx * c} * (1.0f / kU);
}
void fill_her_hand(int h) {
    HerHand& hh = her_hands[h];
    for (int f = 0; f < 5; f++) {
        FingerGeo& g = hh.f[f];
        g.knuckle = grip_knuckle(h) + to_grip_dir(h, sk.joint[h][f][0] - sk.knuckle[h]);
        for (int j = 0; j < 3; j++) g.seg[j] = to_grip_dir(h, sk.joint[h][f][j + 1] - sk.joint[h][f][j]);
        g.axis = normalized(to_grip_dir(h, sk.bend[h][f]));
        for (int j = 0; j < 3; j++) g.joint[j] = (f == kThumb ? kThumbDeg[j] : kJointDeg[j]) * kPi / 180.0f;
        // For tools/grip_fit.py (her fingers on the weapons' grips, fitted offline): the finger in the grip space (m).
        rt_log("VR hand geometry: %s %d: knuckle %.4f %.4f %.4f seg %.4f %.4f %.4f | %.4f %.4f %.4f | %.4f %.4f %.4f "
               "axis %.4f %.4f %.4f", h == kLeft ? "left" : "right", f, g.knuckle.x, g.knuckle.y, g.knuckle.z,
               g.seg[0].x, g.seg[0].y, g.seg[0].z, g.seg[1].x, g.seg[1].y, g.seg[1].z, g.seg[2].x, g.seg[2].y,
               g.seg[2].z, g.axis.x, g.axis.y, g.axis.z);
    }
    rt_log("VR hand geometry: %s bone axes: along %.4f %.4f %.4f palm %.4f %.4f %.4f across %.4f %.4f %.4f knuckle "
           "%.2f %.2f %.2f (units)", h == kLeft ? "left" : "right", sk.along[h].x, sk.along[h].y, sk.along[h].z,
           sk.palm[h].x, sk.palm[h].y, sk.palm[h].z, sk.across[h].x, sk.across[h].y, sk.across[h].z, sk.knuckle[h].x,
           sk.knuckle[h].y, sk.knuckle[h].z);
    hh.valid = true;
}

bool setup(const int8_t* parents, const float* rest, const Bone* anim) {
    sk.rest_from = rest;
    sk.fingers = false;
    for (HerHand& hh : her_hands) hh.valid = false;
    int8_t p[kN];
    for (int b = 0; b < kN; b++) p[b] = parents[b], sk.parent[b] = parents[b];
    if (!chain_ok(p)) {
        rt_log("VR body: Heather's model has other bone chains than expected: her body keeps the game's pose");
        return false;
    }
    for (int t = 0; t < kN; t++)
        for (int b = 0; b < kN; b++) {
            int x = b, guard = 0;
            while (x >= 0 && x != t && guard++ < 64) x = sk.parent[x];
            sk.sub[t][b] = x == t;
        }
    // Which arm and leg is her left: by where the game's pose has them against her facing.
    const Vec3 right = her_right(game.yaw);
    const bool a_left = dot(anim[kArmA.hand].o - anim[kArmB.hand].o, right) < 0 ||
                        dot(anim[kArmA.shoulder].o - anim[kArmB.shoulder].o, right) < 0;
    const Arm& L = a_left ? kArmA : kArmB;
    const Arm& R = a_left ? kArmB : kArmA;
    sk.collar[kLeft] = L.collar, sk.shoulder[kLeft] = L.shoulder, sk.elbow[kLeft] = L.elbow;
    sk.forearm[kLeft] = L.forearm, sk.twin[kLeft] = L.twin, sk.hand[kLeft] = L.hand;
    sk.collar[kRight] = R.collar, sk.shoulder[kRight] = R.shoulder, sk.elbow[kRight] = R.elbow;
    sk.forearm[kRight] = R.forearm, sk.twin[kRight] = R.twin, sk.hand[kRight] = R.hand;
    const bool la_left = dot(anim[kLegA.hip].o - anim[kLegB.hip].o, right) < 0;
    const Leg& LL = la_left ? kLegA : kLegB;
    const Leg& RL = la_left ? kLegB : kLegA;
    sk.hip[kLeft] = LL.hip, sk.knee[kLeft] = LL.knee, sk.ankle[kLeft] = LL.ankle;
    sk.hip[kRight] = RL.hip, sk.knee[kRight] = RL.knee, sk.ankle[kRight] = RL.ankle;
    rt_log("VR body: Heather's model found (73 bones): left arm %d-%d-%d-%d hand %d, right arm %d-%d-%d-%d hand %d, left "
           "leg %d-%d-%d, right leg %d-%d-%d", L.collar, L.shoulder, L.elbow, L.forearm, L.hand, R.collar, R.shoulder,
           R.elbow, R.forearm, R.hand, LL.hip, LL.knee, LL.ankle, RL.hip, RL.knee, RL.ankle);
    // Each bone's part of her (the draws are matched to these: body_cut).
    for (int b = 0; b < kN; b++) {
        uint8_t c = kTrunkC;
        if (sk.sub[kHead][b]) c = kHeadC;
        else if (sk.sub[sk.hip[kLeft]][b]) c = kLegLeftC;
        else if (sk.sub[sk.hip[kRight]][b]) c = kLegRightC;
        else if (b == sk.hand[kLeft] || b == sk.hand[kRight]) c = kHandC;
        else if (sk.sub[sk.hand[kLeft]][b] || sk.sub[sk.hand[kRight]][b]) c = kFingerC;
        else if (sk.sub[sk.collar[kLeft]][b] || sk.sub[sk.collar[kRight]][b]) c = kArmC;
        sk.cls[b] = c;
    }
    if (rest) {
        read_rest(rest, anim);
        if (sk.fingers) sk.fingers = read_hand(kLeft) && read_hand(kRight);
        if (sk.fingers) {
            fill_her_hand(kLeft);
            fill_her_hand(kRight);
            // session 14 (owner 60): her rest pose's arms (the forearm starts from it): how far each elbow is bent
            for (int h = 0; h < 2; h++) {
                const Bone e = sk.rel[sk.elbow[h]], f = compose(sk.rel[sk.forearm[h]], e);
                const Vec3 w = compose(sk.rel[sk.hand[h]], f).o, t = compose(sk.rel[sk.twin[h]], f).o;
                const Vec3 up = normalized(e.o), fo = normalized(w - e.o);
                rt_log("VR body: her %s arm at rest: elbow bent %.1f degrees, upper arm %.1f units, forearm %.1f, the "
                       "forearm bone %.1f units from the elbow, the twist bone %.1f (%.1f off the forearm's line)",
                       h ? "right" : "left", std::acos(std::clamp(dot(up, fo), -1.0f, 1.0f)) * 180.0f / kPi,
                       length(e.o), length(w - e.o), length(f.o - e.o), length(t - e.o),
                       length((t - e.o) - fo * dot(t - e.o, fo)));
            }
        }
    }
    rt_log("VR body: %s", sk.fingers ? "her own finger hands are shown at the controllers" :
                                       "her own fingers could not be read: the game's hands are shown at the controllers");
    return true;
}

// ---------------------------------------------------------------- the pose being built

Bone P[kN];      // this frame's pose (the game's, then ours)
Bone anim[kN];   // the game's pose as it came
int anim_frame = -1;  // the frame it came in (combat reads her hands from it: the game's own grips)

void turn_sub(Bone* B, int top, Quat q, Vec3 pivot) {
    for (int b = 0; b < kN; b++) {
        if (!sk.sub[top][b]) continue;
        for (int i = 0; i < 3; i++) B[b].r[i] = rotate(q, B[b].r[i]);
        B[b].o = pivot + rotate(q, B[b].o - pivot);
    }
}
void move_sub(Bone* B, int top, Vec3 d) {
    for (int b = 0; b < kN; b++)
        if (sk.sub[top][b]) B[b].o = B[b].o + d;
}
// The chain a -> b -> c (each one below the one before): c's origin to `target`, b bent toward `hint`. `stretch`: how
// much longer than its bones the chain may grow before the end falls short.
// `hinge` (an arm, session 14): the elbow's hinge axis in a's own axes; a is then turned about its own length, before
// b bends, so that axis lies across the bend (b bends about its hinge as an elbow does, never sideways), by up to 120
// degrees and fully only once the bend is over 30 degrees (`roll_out`: the turn, radians).
void solve_two(Bone* B, int a, int b, int c, Vec3 target, Vec3 hint, float stretch, const Vec3* hinge = nullptr,
               float* roll_out = nullptr) {
    const Vec3 A = B[a].o, Bo = B[b].o, C = B[c].o;
    const float l1 = length(Bo - A), l2 = length(C - Bo);
    if (!(l1 > 1.0f && l2 > 1.0f)) return;
    const Vec3 to = target - A;
    float d = length(to);
    if (d < 1e-3f) return;
    const Vec3 dir = to * (1.0f / d);
    const float s = std::clamp(d / (0.999f * (l1 + l2)), 1.0f, std::max(1.0f, stretch));
    const float L1 = l1 * s, L2 = l2 * s;
    d = std::clamp(d, 0.15f * (L1 + L2), 0.999f * (L1 + L2));
    const Vec3 T = A + dir * d;
    const float along = (L1 * L1 - L2 * L2 + d * d) / (2.0f * d);
    const float h = std::sqrt(std::max(0.0f, L1 * L1 - along * along));
    Vec3 side = hint - dir * dot(hint, dir);
    if (length(side) < 1e-4f) side = (Bo - A) - dir * dot(Bo - A, dir);
    if (length(side) < 1e-4f) side = cross(dir, Vec3{1, 0, 0});
    const Vec3 E = A + dir * along + normalized(side) * h;
    turn_sub(B, a, arc(Bo - A, E - A), A);
    if (s > 1.0f) move_sub(B, b, E - B[b].o);
    if (roll_out) *roll_out = 0.0f;
    if (hinge) {
        const Vec3 u = normalized(E - A), n = cross(E - A, T - E);
        const float nl = length(n);
        const float bend = std::acos(std::clamp(dot(u, normalized(T - E)), -1.0f, 1.0f));
        const float w = smooth01(std::clamp((bend - 10.0f * kPi / 180.0f) / (20.0f * kPi / 180.0f), 0.0f, 1.0f));
        Vec3 now = apply_dir(B[a], *hinge);
        now = now - u * dot(now, u);
        if (nl > 1e-3f && w > 0.0f && length(now) > 1e-3f) {
            const Vec3 nn = n * (1.0f / nl);
            now = normalized(now);
            const float ang = std::clamp(std::atan2(dot(cross(now, nn), u), dot(now, nn)) * w, -kPi * 2.0f / 3.0f,
                                         kPi * 2.0f / 3.0f);
            turn_sub(B, a, axis_angle(u, ang), A);
            if (roll_out) *roll_out = ang;
        }
    }
    turn_sub(B, b, arc(B[c].o - E, T - E), E);
    if (s > 1.0f) move_sub(B, c, T - B[c].o);
}

// The bones under `top` (not top) put back to their rest relations, each from its parent (a parent's number is always
// below its children's in her skeleton).
void rest_sub(Bone* B, int top) {
    for (int b = top + 1; b < kN; b++)
        if (sk.sub[top][b] && sk.parent[b] >= 0) B[b] = compose(sk.rel[b], B[sk.parent[b]]);
}

// ---------------------------------------------------------------- state from frame to frame

struct Fade {
    float f = 0, hold = 0;
    void update(bool want) {
        const float dt = frame_dt();
        if (want) {
            f = std::min(1.0f, f + dt / 0.08f);
            hold = 0.35f;
        } else if (hold > 0) hold -= dt;
        else f = std::max(0.0f, f - dt / 0.28f);
    }
    float faded() const { return smooth01(f); }  // 0 shown, 1 gone
};

struct Foot {
    Vec3 pos;           // the plant (world): the ankle's origin and turn
    Quat rot;
    bool stepping = false;
    float t = 0;
    Vec3 from_pos;
    Quat from_rot;
    float lowest = 1e9f;  // the ankle's height over the floor at its lowest lately (footsteps)
    float last_h = 0;
    float since_step = 1;
};

struct State {
    int frame = -1;
    Vec3 anim_root;              // the game's root this frame (a second draw of the same pose reuses the result)
    Bone out[kN];
    Bone ghost[kN];
    int ghost_bits = 0;
    int ghost_frame = -1;
    bool beyond[2] = {};         // the controller out of her arm's reach this frame (her hand stays on the arm)
    Vec3 shoulder_w[2];          // session 13: her shoulder as the arm was last solved (world, the collar's lift in) ...
    float reach_u[2] = {};       // ... and how far the arm reaches from it (units, its stretch in) ...
    int reach_frame[2] = {-1, -1};  // ... in this frame
    bool beyond_logged[2] = {};
    Vec3 drawn[kN];
    Vec3 blend_o[128];
    uint8_t blend_cls[128];
    int blends = 0;
    int bones_frame = -1;        // the frame `drawn` and `blend_o` are of
    int fingers_frame = -1;      // the frame her fingers were posed (her own hands can be shown)
    Vec3 root_prev;
    bool root_valid = false;
    float speed = 0;             // her root's horizontal speed, m/s (smoothed)
    float walk = 0;              // 0 standing .. 1 walking
    Foot feet[2];
    bool feet_valid = false;
    Quat lean;
    float drop = 0;              // units her body is lowered (crouch)
    Vec3 hang;                   // units her whole body is moved (x, z) to hang from the player's neck past the lean
    float hang_k = 1.0f;         // 0 while the game holds her down (knocked down, grabbed) .. 1 standing
    float lift = 0;              // units her body is raised to the player's neck (the game's stoops, lunges)
    Quat bend;                  // her spine bent toward a hand past her reach
    Vec3 hint_local[2];
    bool hint_valid[2]{};
    Vec3 collar_off[2];
    float twist_deg[2]{};        // the hand's twist about the forearm last solved (half on the twist bone), for the log
    // Session 14 (owner 60, the wrists in the two-hand holds): each elbow's hinge in its upper arm's axes as the game's
    // own animation bends it (learned while the game's elbow is bent), how many frames it was learned from; the hand's
    // twist from her rest pose last frame (unwrapped, so turning on past half a turn never flips it), the elbow's swing
    // about the shoulder-to-wrist line that takes what a forearm cannot (eased) and, for the log, the upper arm's turn
    // onto its hinge and the wrist's bend.
    Vec3 hinge_local[2];
    int hinge_frames[2]{};
    float twist_prev[2]{};
    float swivel[2]{};
    float hinge_roll_deg[2]{};
    float wrist_bend_deg[2]{};
    float swivel_deg[2]{};
    // ... and the middle of a forearm's turn as the game's own animation keeps her hand (measured as the twist is, from
    // her rest pose's hand: vr199 showed her rest hand a half turn from a palm-up grip, so a range about the rest let
    // the twist run on to 270 degrees), averaged as a direction; the game's own shares of it on the forearm and twist
    // bones (for the log)
    float center_c[2]{}, center_s[2]{};
    float twist_center[2]{};
    int center_frames[2]{};
    float game_share_fa[2]{}, game_share_tw[2]{};
    Vec3 watch_out[2];           // LIST 49: out of the back of her wrist as the twist bone turns it (world, unit)
    int watch_frame[2] = {-1, -1};
    float chest_d = 1.0f;        // her chest bone from the eyes (m), for the trunk's fade and the log
    bool chest_close = false;
    Fade trunk, leg[2];
    Fade over;                   // her whole body, knocked down or grabbed (the legs go with the trunk then)
    bool turned_over = false;
    float fall_speed = 0;        // units/s down
    float falling_for = 0;
    float eye_dist_chest = 0;
} st;

// Her hand where the controller (pose: tracking) puts it: her axes on the grip's, her middle knuckle at the grip's.
Bone hand_at(int h, const Pose& pose) {
    const float mx = h == kLeft ? -1.0f : 1.0f;
    const Quat q = pose.orientation;
    const Vec3 along = world_dir(rotate(q, {0, -1, 0}));
    const Vec3 palm = world_dir(rotate(q, {-mx, 0, 0}));
    const Vec3 across = cross(along, palm);
    Bone b;
    const Vec3 la = sk.along[h], lp = sk.palm[h], lc = sk.across[h];
    b.r[0] = along * la.x + palm * lp.x + across * lc.x;
    b.r[1] = along * la.y + palm * lp.y + across * lc.y;
    b.r[2] = along * la.z + palm * lp.z + across * lc.z;
    const Vec3 K = world_point(pose.position + rotate(q, grip_knuckle(h)));
    b.o = K - apply_dir(b, sk.knuckle[h]);
    return b;
}

// Her fingers below her hand, closed by `curl` (0 open .. 1 fist; thumb, index, middle, ring, pinky; below 0 bent back
// against a surface, kFingerBack's share at each joint, as sh3vr_body.cpp's finger_points has them).
void pose_fingers(Bone* B, int h, const float curl[5]) {
    const Bone& H = B[sk.hand[h]];
    for (int f = 0; f < 5; f++) {
        const Vec3 axis = normalized(apply_dir(H, sk.bend[h][f]));
        const float* deg = f == kThumb ? kThumbDeg : kJointDeg;
        const float c = std::isfinite(curl[f]) ? std::clamp(curl[f], kFingerBackMost, 1.0f) : 0.0f;
        Bone parent = H;
        for (int j = 0; j < 3; j++) {
            const int b = sk.finger[h][f][j];
            Bone w = compose(sk.rel[b], parent);
            const float a = c * (c < 0 ? kFingerBack[f == kThumb ? 0 : 1][j] : 1.0f) * deg[j] * kPi / 180.0f;
            if (std::fabs(a) > 1e-4f) {
                const Quat q = axis_angle(axis, a);
                for (int i = 0; i < 3; i++) w.r[i] = rotate(q, w.r[i]);
            }
            B[b] = w;
            parent = w;
        }
    }
}

// Testing (the PC rig, session 7): SH3_IK_OFF="frame:mask,frame:mask,..." leaves IK steps out from each frame on (1 lean,
// 2 crouch and lift, 4 hang, 8 reach bend, 16 head turn, 32 legs, 64 collar, 128 arms, 256 forearm twist, 512 fingers,
// 1024 her own finger hands: the game's rigid ones drawn, 2048 her head's cut, 4096 the near cut; session 14: 8192 the
// forearm from her rest pose, 16384 the upper arm turned onto its hinge, 32768 the elbow swung for the hand's twist),
// to find which one draws a fault.
bool ik_off(int bit) {
    static int n = -1, at[32], mask[32], told = -1;
    if (n < 0) {
        n = 0;
        if (const char* e = getenv("SH3_IK_OFF"))
            while (*e && n < 32) {
                at[n] = atoi(e);
                const char* c = std::strchr(e, ':');
                if (!c) break;
                mask[n++] = (int)std::strtol(c + 1, nullptr, 0);
                e = std::strchr(c, ',');
                if (!e) break;
                e++;
            }
    }
    int m = 0, k = -1;
    for (int i = 0; i < n; i++)
        if (rt_frame >= at[i]) m = mask[i], k = i;
    if (k != told) {
        told = k;
        rt_log("VR body: testing: frame %d: IK steps left out %#x", rt_frame, m);
    }
    return (m & bit) != 0;
}

// One arm to the controller `pose`; `ghost`: on the ghost copy (the eased state is used, not moved on).
void arm_to(Bone* B, int h, const Pose& pose, const float curl[5], bool ghost) {
    const int sh = sk.shoulder[h], el = sk.elbow[h], hd = sk.hand[h], col = sk.collar[h];
    const Bone want = hand_at(h, pose);
    if (opt.arms_follow && !ik_off(128)) {
        const float l1 = length(B[el].o - B[sh].o), l2 = length(B[hd].o - B[el].o), L = l1 + l2;
        // The collar: the shoulder lifts with a raised hand and reaches toward a far one (Jak's 0.35 and 0.25).
        Vec3 off = st.collar_off[h];
        if (!ghost) {
            const Vec3 S = B[sh].o, to = want.o - S;
            const float raise = std::clamp((S.y - want.o.y) / std::max(1.0f, L), 0.0f, 1.0f);  // y down
            const float reach = std::clamp((length(to) - 0.75f * L) / std::max(1.0f, 0.25f * L), 0.0f, 1.0f);
            Vec3 o = Vec3{0, -1, 0} * (0.35f * raise * l1) + normalized(to) * (0.25f * reach * l1);
            const float n = length(o), cap = 0.06f * kU;
            if (n > cap) o = o * (cap / n);
            st.collar_off[h] = off = lerp(st.collar_off[h], o, ease(12.0f));
        }
        if (length(off) > 0.1f && !ik_off(64)) {
            const Vec3 C = B[col].o;
            turn_sub(B, col, arc(B[sh].o - C, B[sh].o + off - C), C);
        }
        // Session 14 (owner 60: "some of the wrists in the two hand holds in these images look warped/twisted... just
        // make sure it looks clean/smooth"): the forearm, its twist bone and the hand start from her rest pose under the
        // upper arm each frame (a straight, unturned arm), not from the game's animated arm: the game's own forearm roll
        // for its stance had been carried by the turns below and the twist added on top of it, so a wrist could end up
        // turned well past what the twist measured. Her elbow's hinge in the upper arm's axes is learned from the
        // game's own animation while its elbow is bent (the bend's axis, in the upper arm's axes, averaged).
        const bool rest_arm = sk.fingers && !ik_off(8192);
        if (!ghost && anim_frame == rt_frame) {
            const Vec3 ua = anim[el].o - anim[sh].o, fa = anim[hd].o - anim[el].o;
            const Vec3 na = cross(ua, fa);
            const float bend_a = std::acos(std::clamp(dot(normalized(ua), normalized(fa)), -1.0f, 1.0f));
            if (bend_a > 25.0f * kPi / 180.0f && length(na) > 1e-3f) {
                const Vec3 l = normalized(local_dir(anim[sh], normalized(na)));
                Vec3& hl = st.hinge_local[h];
                hl = st.hinge_frames[h] == 0 ? l : normalized(lerp(hl, l, 0.05f));
                if (st.hinge_frames[h]++ == 0)
                    rt_log("VR body: frame %d: her %s elbow's hinge learned from the game's bend of %.0f degrees: %.2f "
                           "%.2f %.2f in the upper arm's axes", rt_frame, h ? "right" : "left", bend_a * 180.0f / kPi, l.x,
                           l.y, l.z);
            }
            if (rest_arm) {
                // the game's own twist: her rest pose's arm under the game's upper arm, its forearm turned the shortest
                // way onto the game's (a straight, unturned wrist), against the game's hand
                const int fb = sk.forearm[h], tb = sk.twin[h];
                const Bone e = compose(sk.rel[el], anim[sh]), f = compose(sk.rel[fb], e);
                const Bone hr = compose(sk.rel[hd], f), tr = compose(sk.rel[tb], f);
                const Vec3 d1 = anim[hd].o - anim[el].o;
                const Quat q = arc(hr.o - e.o, d1);
                const Vec3 ax = normalized(d1);
                auto roll = [&](Vec3 a, Vec3 b, bool& ok) {
                    a = a - ax * dot(a, ax), b = b - ax * dot(b, ax);
                    ok = length(a) > 1e-3f && length(b) > 1e-3f;
                    if (!ok) return 0.0f;
                    a = normalized(a), b = normalized(b);
                    return std::atan2(dot(cross(a, b), ax), dot(a, b));
                };
                bool ok_h, ok_f, ok_t;
                const float tg = roll(rotate(q, apply_dir(hr, sk.palm[h])), apply_dir(anim[hd], sk.palm[h]), ok_h);
                // the forearm and twist bones by their own first axes (any axis off their length tells their turn)
                Vec3 af = rotate(q, f.r[0]), at = rotate(q, tr.r[0]);
                if (std::fabs(dot(normalized(af), ax)) > 0.9f) af = rotate(q, f.r[1]);
                if (std::fabs(dot(normalized(at), ax)) > 0.9f) at = rotate(q, tr.r[1]);
                const bool same_f = std::fabs(dot(normalized(f.r[0]), normalized(hr.o - e.o))) <= 0.9f;
                const bool same_t = std::fabs(dot(normalized(tr.r[0]), normalized(hr.o - e.o))) <= 0.9f;
                const float rf = roll(af, same_f ? anim[fb].r[0] : anim[fb].r[1], ok_f);
                const float rt = roll(at, same_t ? anim[tb].r[0] : anim[tb].r[1], ok_t);
                if (ok_h) {
                    const float k = st.center_frames[h] == 0 ? 1.0f : 0.02f;
                    st.center_c[h] += (std::cos(tg) - st.center_c[h]) * k;
                    st.center_s[h] += (std::sin(tg) - st.center_s[h]) * k;
                    st.twist_center[h] = std::atan2(st.center_s[h], st.center_c[h]);
                    if (std::fabs(tg) > 0.35f && ok_f && ok_t) {
                        st.game_share_fa[h] += (rf / tg - st.game_share_fa[h]) * 0.05f;
                        st.game_share_tw[h] += (rt / tg - st.game_share_tw[h]) * 0.05f;
                    }
                    if (st.center_frames[h]++ == 0)
                        rt_log("VR body: frame %d: her %s hand turned %.0f degrees on its forearm from her rest pose in "
                               "the game's own animation (the forearm bone %.0f, the twist bone %.0f)", rt_frame,
                               h ? "right" : "left", tg * 180.0f / kPi, rf * 180.0f / kPi, rt * 180.0f / kPi);
                }
            }
        }
        if (rest_arm) rest_sub(B, sh);
        // The elbow: toward where the controller's forearm points back to (where the elbow is with the wrist straight),
        // with down and out mixed in so a forearm pointing at the shoulder still bends the usual way; session 14: that
        // mix only as much as the forearm's own way is lost (it had bent every wrist that pointed its forearm clear of
        // the shoulder by up to 35 degrees), eased in her chest's axes.
        const Vec3 S = B[sh].o;
        const Vec3 toward = normalized(world_dir(rotate(pose.orientation, {0.0f, 0.8619f, 0.5070f})));
        const Vec3 out = her_right(game.yaw) * (h == kRight ? 1.0f : -1.0f);
        const Vec3 hint_point = want.o + toward * l2;
        float clear = 1.0f;
        {
            const Vec3 st_dir = normalized(want.o - S), to_e = hint_point - S;
            const Vec3 perp = to_e - st_dir * dot(to_e, st_dir);
            clear = std::clamp(length(perp) / std::max(1.0f, 0.5f * l2), 0.0f, 1.0f);
        }
        const float mix = 1.0f - 0.75f * clear;
        Vec3 hint = normalized(hint_point - S) + (Vec3{0, 1, 0} * 0.6f + out * 0.3f) * mix;
        const Bone& chest = B[kChest];
        const Vec3 local = normalized(local_dir(chest, hint));
        if (!ghost) {
            if (!st.hint_valid[h]) st.hint_local[h] = local, st.hint_valid[h] = true;
            st.hint_local[h] = normalized(lerp(st.hint_local[h], local, ease(25.0f)));
        }
        hint = apply_dir(chest, st.hint_valid[h] ? st.hint_local[h] : local);
        if (!ghost) st.shoulder_w[h] = S, st.reach_u[h] = 0.999f * 1.12f * L, st.reach_frame[h] = rt_frame;
        float roll = 0.0f;
        const bool hinge_known = rest_arm && st.hinge_frames[h] >= 10 && !ik_off(16384);
        solve_two(B, sh, el, hd, want.o, hint, 1.12f, hinge_known ? &st.hinge_local[h] : nullptr, &roll);
        if (!ghost) st.hinge_roll_deg[h] = roll * 180.0f / kPi;
        if (!ghost) st.beyond[h] = length(B[hd].o - want.o) > (st.beyond_logged[h] ? 0.01f : 0.02f) * kU;
        // Her hand turned as the controller. Its twist about the forearm is measured from her rest pose's hand on this
        // forearm (a straight, unturned wrist), unwrapped against the last frame's (a hand turned on past half a turn
        // keeps turning the same way instead of flipping to the other side). Session 14 (owner 60): a forearm turns
        // about a quarter turn each way from there; what a hand asks past that is taken by the whole arm turning
        // about the line from the shoulder to the wrist (the elbow swinging round, as a shoulder turns with a palm
        // turned right up or down; eased, at most 60 degrees), and the rest spread along the forearm as a forearm
        // twists: 40 percent at the forearm bone (from the elbow), 85 at the twist bone (her wrist and watch), all of
        // it at the hand, so no one blend of her skin takes more than about half of it (session 9's 80 percent on the
        // twist bone, held to 90 degrees, left up to 90 degrees each in the forearm's middle and at the wrist: the
        // warped wrists of vr197's two-hand holds).
        const int fa = sk.forearm[h], tw = sk.twin[h];
        auto twist_now = [&](Vec3& axis, Vec3& p0) {
            const Bone untwisted = compose(sk.rel[hd], B[fa]);
            axis = normalized(B[hd].o - B[el].o);
            p0 = apply_dir(untwisted, sk.palm[h]);
            Vec3 p1 = apply_dir(want, sk.palm[h]);
            p0 = p0 - axis * dot(p0, axis);
            p1 = p1 - axis * dot(p1, axis);
            if (length(p0) < 1e-3f || length(p1) < 1e-3f) return 0.0f;
            p0 = normalized(p0);
            p1 = normalized(p1);
            const float raw = std::atan2(dot(cross(p0, p1), axis), dot(p0, p1));
            // on from last frame's (no flip past half a turn), but never more than 200 degrees from the middle of the
            // game's own range (session 14, vr199: unwrapped alone it ran on to 270 on the submachine gun's front grip)
            float t = st.twist_prev[h] + wrap_angle(raw - st.twist_prev[h]);
            const float mid = st.center_frames[h] > 0 ? st.twist_center[h] : 0.0f, far_most = 200.0f * kPi / 180.0f;
            if (t - mid > far_most) t -= 2.0f * kPi;
            if (t - mid < -far_most) t += 2.0f * kPi;
            return t;
        };
        Vec3 axis, p0;
        float twist = twist_now(axis, p0);
        if (rest_arm && !ik_off(32768)) {
            // a forearm turns about a quarter turn each way from the middle of the game's own range
            const float kRange = 0.5f * kPi, kSwingMost = kPi / 3.0f;
            const float mid = st.center_frames[h] > 0 ? st.twist_center[h] : 0.0f;
            const Vec3 line = normalized(B[hd].o - S);
            const float c = std::max(0.35f, dot(axis, line));
            const float over = (twist - mid) - std::clamp(twist - mid, -kRange, kRange);
            const float want_swing = std::clamp(over / c, -kSwingMost, kSwingMost);
            if (!ghost) st.swivel[h] += (want_swing - st.swivel[h]) * ease(8.0f);
            const float swing = std::clamp(st.swivel[h], -kSwingMost, kSwingMost);
            if (!ghost) st.swivel_deg[h] = swing * 180.0f / kPi;
            if (std::fabs(swing) > 1e-4f) {
                turn_sub(B, sh, axis_angle(line, swing), S);
                twist = twist_now(axis, p0);
            }
        }
        if (!ghost) st.twist_prev[h] = twist;
        // LIST 49: the back of her wrist where her watch is, as the twist bone turns it (vr142: her watch's face sat
        // 45 degrees round from the back of her hand rolled 90 degrees)
        if (!ghost && length(p0) > 0.5f) st.watch_out[h] = p0 * -1.0f, st.watch_frame[h] = rt_frame;
        if (length(p0) > 0.5f && !ik_off(256)) {
            if (!ghost) st.twist_deg[h] = twist * 180.0f / kPi;
            const float t = std::clamp(twist, -kPi * 0.9f, kPi * 0.9f);
            const Vec3 E = B[el].o;
            if (rest_arm) {
                const float at_forearm = 0.40f * t, at_twin = 0.85f * t;
                turn_sub(B, fa, axis_angle(axis, at_forearm), E);
                turn_sub(B, tw, axis_angle(axis, at_twin - at_forearm), E);
                if (!ghost) st.watch_out[h] = rotate(axis_angle(axis, at_twin), p0) * -1.0f;
            } else {
                // her rest pose unread: session 9's share on the twist bone alone
                const float half = std::clamp(twist * 0.8f, -kPi * 0.5f, kPi * 0.5f);
                if (!ghost) st.watch_out[h] = rotate(axis_angle(axis, half), p0) * -1.0f;
                turn_sub(B, tw, axis_angle(axis, half), B[tw].o);
            }
        }
        if (!ghost) {
            // the wrist's bend: her rest pose's hand on this forearm (straight) against the controller's
            const Bone straight = compose(sk.rel[hd], B[fa]);
            st.wrist_bend_deg[h] = std::acos(std::clamp(dot(normalized(apply_dir(straight, sk.along[h])),
                                                            normalized(apply_dir(want, sk.along[h]))), -1.0f, 1.0f)) *
                                   180.0f / kPi;
            // testing (session 14, owner 60): SH3_ARM_LOG=N logs each arm every N frames: the elbow's bend, the upper
            // arm's turn onto its hinge, the hand's twist from her rest pose and the elbow's swing for it, the wrist's bend
            static int arm_log = -2, arm_logged = 0;
            if (arm_log == -2) arm_log = getenv("SH3_ARM_LOG") ? atoi(getenv("SH3_ARM_LOG")) : 0;
            if (arm_log > 0 && rt_frame % arm_log == 0 && arm_logged < 400) {
                arm_logged++;
                const Vec3 u = normalized(B[el].o - B[sh].o), f = normalized(B[hd].o - B[el].o);
                rt_log("VR arm: frame %d: %s: elbow bent %.0f deg, upper arm turned %.0f deg onto its hinge (learned from "
                       "%d frames), hand twist %.0f deg (the game's middle %.0f, from %d frames; elbow swung %.0f deg for "
                       "it; forearm %.0f, twist bone %.0f; the game's shares %.2f %.2f), wrist bent %.0f deg, hand %.1f cm "
                       "off the controller%s", rt_frame, h ? "right" : "left",
                       std::acos(std::clamp(dot(u, f), -1.0f, 1.0f)) * 180.0f / kPi, st.hinge_roll_deg[h],
                       st.hinge_frames[h], st.twist_deg[h], st.twist_center[h] * 180.0f / kPi, st.center_frames[h],
                       st.swivel_deg[h], 0.40f * st.twist_deg[h], 0.85f * st.twist_deg[h], st.game_share_fa[h],
                       st.game_share_tw[h], st.wrist_bend_deg[h], length(B[hd].o - want.o) / kU * 100.0f,
                       rest_arm ? "" : " (rest pose unread: the game's arm)");
            }
        }
    }
    // The hand at the controller (arms not following: the arm keeps the game's pose and the hand goes there alone).
    // Past her arm's reach her hand stays at the arm's end, turned as the controller (never pulled off the wrist);
    // no ghost for that (owner message 45: only a hand pushed into a surface). The ghost copy is only that hand.
    const Vec3 wrist = B[hd].o;
    B[hd] = want;
    if (opt.arms_follow && !ghost) B[hd].o = wrist;
    if (!ik_off(512)) pose_fingers(B, h, curl);
}

void footstep(int side, float strength) {
    if (opt.haptic_extra < 2) return;
    buzz(side, strength, 0.03f, 70.0f);
}

}  // namespace

// The game's own pose of her hand bone (world: the bone's local axes, unit, and its origin) in the frame it came, before
// VR moved it: the weapon's place against it is the game's own grip (sh3vr_combat.cpp, session 5).
bool ik_game_hand(int h, Vec3 axes[3], Vec3& origin, int& frame) {
    if (sk.state != 1 || anim_frame < 0 || h < 0 || h > 1) return false;
    const Bone& b = anim[sk.hand[h]];
    for (int i = 0; i < 3; i++) axes[i] = b.r[i];
    origin = b.o;
    frame = anim_frame;
    return true;
}

// Her hand bone for a controller grip pose, in tracking space (hand_at's way: her axes on the grip's, her middle
// knuckle at the grip's): the bone's local axes (unit) and its origin (metres).
bool ik_hand_bone_for_pose(int h, const Pose& pose, Vec3 axes[3], Vec3& origin) {
    if (sk.state != 1 || h < 0 || h > 1) return false;
    const float mx = h == kLeft ? -1.0f : 1.0f;
    const Quat q = pose.orientation;
    const Vec3 along = rotate(q, {0, -1, 0}), palm = rotate(q, {-mx, 0, 0}), across = cross(along, palm);
    const Vec3 la = sk.along[h], lp = sk.palm[h], lc = sk.across[h];
    axes[0] = along * la.x + palm * lp.x + across * lc.x;
    axes[1] = along * la.y + palm * lp.y + across * lc.y;
    axes[2] = along * la.z + palm * lp.z + across * lc.z;
    const Vec3 K = pose.position + rotate(q, grip_knuckle(h));
    const Vec3 k = sk.knuckle[h];
    origin = K - (axes[0] * k.x + axes[1] * k.y + axes[2] * k.z) * (1.0f / kU);
    return true;
}

// Her shoulder (tracking, metres) and how far her arm reaches from it to her hand bone (metres, the IK's stretch in), as
// the arm was last solved: a weapon held in both hands is kept where her other hand reaches its second grip (session 13,
// the owner: "BE SURE THE FUCKIN SMG LOOKS GOOD WHEN TWO HANDED").
bool ik_arm_reach(int h, Vec3& shoulder, float& reach) {
    if (h < 0 || h > 1 || !opt.arms_follow || st.reach_frame[h] < 0 || rt_frame - st.reach_frame[h] > 2 ||
        st.reach_u[h] < 1.0f || !map_ready())
        return false;
    shoulder = tracking_point(st.shoulder_w[h]);
    reach = st.reach_u[h] / kU;
    return true;
}

// The controller grip pose that puts her hand bone where `axes` (its local axes, tracking, unit) and `origin` (tracking,
// metres) say: the inverse of the above.
bool ik_pose_for_hand_bone(int h, const Vec3 axes[3], Vec3 origin, Pose& out) {
    if (sk.state != 1 || h < 0 || h > 1) return false;
    const float mx = h == kLeft ? -1.0f : 1.0f;
    const Vec3 la = sk.along[h], lp = sk.palm[h], k = sk.knuckle[h];
    const Vec3 along = normalized(axes[0] * la.x + axes[1] * la.y + axes[2] * la.z);
    Vec3 palm = axes[0] * lp.x + axes[1] * lp.y + axes[2] * lp.z;
    palm = normalized(palm - along * dot(palm, along));
    if (length(along) < 0.5f || length(palm) < 0.5f) return false;
    const Vec3 X = palm * -mx, Y = along * -1.0f, Z = cross(X, Y);
    out.orientation = quat_from_axes(X, Y, Z);
    const Vec3 K = origin + (axes[0] * k.x + axes[1] * k.y + axes[2] * k.z) * (1.0f / kU);
    out.position = K - rotate(out.orientation, grip_knuckle(h));
    return true;
}

// LIST 48 (vr146): during a scene her model may be drawn away from the player's own place (sh3.c's 1 m test): her head
// as the scene poses it is still what dynamic cutscenes need. Her bones are left as they are.
void ik_scene_head(const float* bones) {
    if (!active || !bones) return;
    const Vec3 h = load(bones + 16 * kHead).o;
    if (!std::isfinite(h.x) || !std::isfinite(h.y) || !std::isfinite(h.z)) return;
    static int told_at = -1000;
    if (rt_frame - told_at >= 240) {
        told_at = rt_frame;
        rt_log("VR: frame %d: her model drawn away from her place in a scene: her head at %.0f %.0f %.0f", rt_frame, h.x,
               h.y, h.z);
    }
    game_head = h + Vec3{0.0f, -12.0f, 0.0f};
    game_head_frame = rt_frame;
}

// From the game's model draw (src/game/sh3.c, after 0x45EC00): 1 when her bones were changed.
int ik_heather_bones(float* bones, int n, const Sh3VrModel* m) {
    if (!active || !game.player_valid || n != kN || !m) return 0;
    if (sk.state < 0 || sk.rest_from != m->rest) {
        for (int b = 0; b < kN; b++) anim[b] = load(bones + 16 * b);
        sk.state = setup(m->parents, m->rest, anim) ? 1 : 0;
    }
    if (sk.state != 1) return 0;
    // Her head where the game posed it, for the cut (sh3vr_body_cut) whatever the view.
    const Vec3 anim_root = load(bones + 16 * kPelvis).o;
    // LIST 48: her head as the game's animation poses it, before VR moves her (a scene's first-person eyes ride it).
    game_head = load(bones + 16 * kHead).o + Vec3{0.0f, -12.0f, 0.0f};
    game_head_frame = rt_frame;
    if (view_mode != ViewMode::kFirstPerson || !map_ready() || !head_known()) {
        heather_head = load(bones + 16 * kHead).o + Vec3{0.0f, -12.0f, 0.0f};
        heather_head_frame = rt_frame;
        st.feet_valid = false;
        st.root_valid = false;
        return 0;
    }
    // A second draw of her this frame (her shadow, a reflection): the same pose again.
    if (st.frame == rt_frame) {
        if (length(anim_root - st.anim_root) > 1.0f) return 0;
        for (int b = 0; b < kN; b++) store(bones + 16 * b, st.out[b]);
        return 1;
    }
    st.frame = rt_frame;
    st.anim_root = anim_root;
    const float dt = frame_dt();
    for (int b = 0; b < kN; b++) anim[b] = P[b] = load(bones + 16 * b);
    anim_frame = rt_frame;

    const Vec3 feet{game.pos[0], game.pos[1], game.pos[2]};
    const Vec3 fwd = her_forward(game.yaw), up{0, -1, 0};
    const Vec3 eye = world_point(fi.head.position);
    // The head nods and turns about the neck (12 cm under the eyes, 15 cm behind them, rig.h neck_of): looking down
    // takes the eyes forward and down with the body still. Her body hangs from the player's neck: her lean and crouch
    // put her own neck bone there (proper IK, session 6, run vr52: placed by the eyes, her neck stood 15 cm ahead of
    // the player's and looking down 75 degrees put the eyes 2 cm from her neck bone, inside her collar).
    const Vec3 neck_w = world_point(neck_of(fi.head));

    // Her root's speed: walking or standing; a jump of over a metre (a door, a load) starts the feet again.
    if (st.root_valid) {
        Vec3 d = feet - st.root_prev;
        const float vy = d.y / dt;
        d.y = 0;
        if (length(d) > 1.0f * kU) st.feet_valid = false;
        st.speed += (length(d) / dt / kU - st.speed) * ease(10.0f);
        // Landings: a fall that stops (haptics, "The whole world").
        if (vy > 1.2f * kU) st.falling_for += dt, st.fall_speed = std::max(st.fall_speed, vy);
        else {
            if (st.falling_for > 0.12f && opt.haptic_extra >= 2) {
                const float k = std::clamp(st.fall_speed / (4.0f * kU), 0.3f, 1.0f);
                buzz(kLeft, k, 0.08f, 60.0f);
                buzz(kRight, k, 0.08f, 60.0f);
            }
            st.falling_for = 0;
            st.fall_speed = 0;
        }
    }
    st.root_prev = feet;
    st.root_valid = true;
    st.walk += ((st.speed > 0.25f ? 1.0f : 0.0f) - st.walk) * ease(8.0f);

    // Turned over by the game (knocked down, grabbed): her pelvis-to-chest far from upright.
    {
        const Vec3 spine = normalized(anim[kChest].o - anim[kPelvis].o);
        const bool over = dot(spine, up) < std::cos(50.0f * kPi / 180.0f);
        if (over && !st.turned_over && opt.haptic_extra >= 2) {
            buzz(kLeft, 0.8f, 0.18f, 70.0f);
            buzz(kRight, 0.8f, 0.18f, 70.0f);
        }
        st.turned_over = over;
    }
    const bool upright = !st.turned_over;

    // 1. Lean: her neck toward the player's neck in the room (not while walking, not when the game holds her down).
    {
        Vec3 off{neck_w.x - P[kNeck].o.x, 0.0f, neck_w.z - P[kNeck].o.z};
        const float n = length(off);
        off = n > 0.02f * kU ? off * ((n - 0.02f * kU) / n) : Vec3{};
        const Vec3 pivot = P[kSpine].o, to_head = P[kNeck].o - pivot;
        Quat want = limit_turn(arc(to_head, to_head + off), 35.0f * kPi / 180.0f);
        want = slerp(Quat{}, want, upright ? 1.0f - st.walk : 0.0f);
        st.lean = slerp(st.lean, want, ease(8.0f));
        if (quat_angle(st.lean) > 1e-4f && !ik_off(1)) turn_sub(P, kSpine, st.lean, pivot);
    }
    // 2. Crouch: what the player's neck is lower than hers lowers her whole body (her feet stay: the legs bend, below).
    {
        float want = std::clamp(neck_w.y - P[kNeck].o.y - 0.02f * kU, 0.0f, 0.55f * kU);  // y down: the neck lower
        if (opt.seated || !upright) want = 0;
        st.drop += (want - st.drop) * ease(10.0f);
        if (st.drop > 0.1f && !ik_off(2))
            for (Bone& b : P) b.o.y += st.drop;
    }
    // 2'. Lifted: her neck lower than the player's (the game's own stoops and lunges: run vr80, build 78, the knife's
    // swing with the trigger held took her neck 33 cm down, her head 35 cm under the eyes and her raised hand out of her
    // arm's reach) raises her whole body to it, as far as her legs reach their feet on the floor (each leg at most 98%
    // straight over its foot). The player standing is her height.
    {
        float want = 0;
        const float below = P[kNeck].o.y - neck_w.y - 0.02f * kU;  // y down: her neck lower
        if (below > 0 && upright && !opt.seated && st.drop < 0.5f) {
            float room = 0.45f * kU;
            for (int s = 0; s < 2; s++) {
                const Vec3 hip = P[sk.hip[s]].o, ank = anim[sk.ankle[s]].o;
                const float leg = 0.98f * (length(anim[sk.knee[s]].o - anim[sk.hip[s]].o) +
                                           length(anim[sk.ankle[s]].o - anim[sk.knee[s]].o));
                const float flat = length(Vec3{ank.x - hip.x, 0.0f, ank.z - hip.z});
                const float tall = flat < leg ? std::sqrt(leg * leg - flat * flat) : 0.0f;
                room = std::min(room, tall - (ank.y - hip.y));  // y down: the hip over the ankle by ank.y - hip.y
            }
            want = std::clamp(below, 0.0f, std::max(0.0f, room));
        }
        st.lift += (want - st.lift) * ease(25.0f);
        if (st.lift > 0.1f && !ik_off(2))
            for (Bone& b : P) b.o.y -= st.lift;
    }
    // 2a. Hanging from the neck: what of the player's neck offset the lean did not take (walking, past 35 degrees, the
    // game's own lunges) moves her whole body there, as Jak's body hangs from his head, up to 60 cm (a step further in
    // the room is the game's own move). Run vr78, build 76: in the knife's swing the game's animation put her neck 40 cm
    // ahead of the eyes and her head and hair came into view. Her feet step after it (4). Not eased while she stands
    // (run vr79, eased at 30: the swing's lunge left her neck 5 cm ahead), only into and out of the game's holds.
    {
        Vec3 off{neck_w.x - P[kNeck].o.x, 0.0f, neck_w.z - P[kNeck].o.z};
        const float n = length(off);
        off = n > 0.02f * kU ? off * ((n - 0.02f * kU) / n) : Vec3{};
        if (length(off) > 0.60f * kU) off = normalized(off) * (0.60f * kU);
        st.hang_k += ((upright ? 1.0f : 0.0f) - st.hang_k) * ease(8.0f);
        st.hang = off * st.hang_k;
        if (length(st.hang) > 0.1f && !ik_off(4))
            for (Bone& b : P) b.o.x += st.hang.x, b.o.z += st.hang.z;
    }
    const Vec3 hang{st.hang.x, 0.0f, st.hang.z};
    // 2b. Reaching: a controller past her arm's reach bends her at the waist toward it, the least that brings her
    // shoulder within reach (eased), as a player bends to the floor or far ahead (run vr24: crouched 55 cm with the
    // controller at the floor, her arm 42 cm short; vr25: 60 degrees still 21 cm short). At most 20 degrees standing
    // (a controller set down on the floor never folds her over), 85 crouched 45 cm or more. Not while she walks.
    {
        Quat want{};
        float most = 0;
        const float most_bend = (20.0f + 65.0f * std::clamp(st.drop / (0.45f * kU), 0.0f, 1.0f)) * kPi / 180.0f;
        if (upright && opt.arms_follow && st.walk < 0.5f)
            for (int h = 0; h < 2; h++) {
                if (!hands[h].valid) continue;
                const int sh = sk.shoulder[h], el = sk.elbow[h], hd = sk.hand[h];
                const float reach = 1.10f * (length(P[el].o - P[sh].o) + length(P[hd].o - P[el].o));
                const Vec3 T = hand_at(h, hands[h].shown).o, pivot = P[kSpine].o, S = P[sh].o;
                if (length(T - S) <= reach) continue;
                const Quat full = limit_turn(arc(S - pivot, T - pivot), most_bend);
                float k = 1.0f;
                for (int i = 1; i <= 10; i++) {
                    const float f = i / 10.0f;
                    if (length(pivot + rotate(slerp(Quat{}, full, f), S - pivot) - T) <= reach) {
                        k = f;
                        break;
                    }
                }
                const Quat q = slerp(Quat{}, full, k);
                if (quat_angle(q) > most) most = quat_angle(q), want = q;
            }
        st.bend = slerp(st.bend, want, ease(6.0f));
        if (quat_angle(st.bend) > 1e-4f && !ik_off(8)) turn_sub(P, kSpine, st.bend, P[kSpine].o);
    }
    // 3. Her head and neck turned as the headset (her shadow and mirrors; her head is cut from the eyes).
    if (upright && !ik_off(16)) {
        const Vec3 hf = world_dir(rotate(fi.head.orientation, {0, 0, -1}));
        const Vec3 hu = world_dir(rotate(fi.head.orientation, {0, 1, 0}));
        const Quat body = frame_quat(rotate(st.lean, fwd), rotate(st.lean, up));
        const Quat turn = limit_turn(frame_quat(hf, hu) * conjugate(body), 70.0f * kPi / 180.0f);
        turn_sub(P, kNeck, slerp(Quat{}, turn, 0.4f), P[kNeck].o);
        turn_sub(P, kHead, slerp(Quat{}, turn, 0.6f), P[kHead].o);
    }
    // 4. Legs: the feet planted while she stands, stepping after the body; the animation's feet while she walks.
    if (upright && !ik_off(32)) {
        if (!st.feet_valid) {
            for (int s = 0; s < 2; s++) {
                Foot& f = st.feet[s];
                f.pos = anim[sk.ankle[s]].o + hang;
                f.rot = quat_of(anim[sk.ankle[s]]);
                f.stepping = false;
            }
            st.feet_valid = true;
        }
        const bool walking = st.walk > 0.5f;
        int stepping = -1;
        for (int s = 0; s < 2; s++)
            if (st.feet[s].stepping) stepping = s;
        if (walking) {
            for (Foot& f : st.feet) f.stepping = false;
            stepping = -1;
        } else if (stepping < 0) {
            // The foot furthest from where the animation would put it steps, when far enough.
            float worst = 1.0f;
            for (int s = 0; s < 2; s++) {
                const Foot& f = st.feet[s];
                const Vec3 a = anim[sk.ankle[s]].o + hang;  // where the animation puts it, under her hung body
                const float dpos = length(Vec3{a.x - f.pos.x, 0, a.z - f.pos.z}) / (0.12f * kU);
                const float drot = quat_angle(quat_of(anim[sk.ankle[s]]) * conjugate(f.rot)) / (25.0f * kPi / 180.0f);
                const float e = std::max(dpos, drot);
                if (e > worst) worst = e, stepping = s;
            }
            if (stepping >= 0) {
                Foot& f = st.feet[stepping];
                f.stepping = true;
                f.t = 0;
                f.from_pos = f.pos;
                f.from_rot = f.rot;
            }
        }
        for (int s = 0; s < 2; s++) {
            Foot& f = st.feet[s];
            const Bone& a = anim[sk.ankle[s]];
            const Quat arot = quat_of(a);
            const Vec3 ao = a.o + hang;  // the animation's foot under her hung body (2a)
            Vec3 tpos;
            Quat trot;
            if (walking) {
                f.pos = ao;
                f.rot = arot;
                tpos = ao;
                trot = arot;
            } else if (f.stepping) {
                f.t += dt / 0.25f;
                const float k = smooth01(f.t);
                tpos = lerp(f.from_pos, ao, k) + up * (0.05f * kU * std::sin(kPi * std::min(f.t, 1.0f)));
                trot = slerp(f.from_rot, arot, k);
                if (f.t >= 1.0f) {
                    f.stepping = false;
                    f.pos = ao;
                    f.rot = arot;
                    footstep(s, 0.10f);
                }
            } else {
                // Planted; blended from the animation's foot as walking ends.
                tpos = lerp(f.pos, ao, st.walk);
                trot = slerp(f.rot, arot, st.walk);
            }
            // The knee toward the side the animation bends it (her forward when the leg is straight).
            const Vec3 hip = anim[sk.hip[s]].o, knee = anim[sk.knee[s]].o, ank = a.o;
            const Vec3 line = ank - hip;
            const float t = dot(knee - hip, line) / std::max(1.0f, dot(line, line));
            const Vec3 bend = knee - (hip + line * t);
            const Vec3 hint = normalized(bend) * std::min(1.0f, length(bend) / (0.03f * kU)) + fwd * 0.5f;
            solve_two(P, sk.hip[s], sk.knee[s], sk.ankle[s], tpos, hint, 1.0f);
            turn_sub(P, sk.ankle[s], trot * conjugate(quat_of(P[sk.ankle[s]])), P[sk.ankle[s]].o);
            // Footsteps while walking: the animation's ankle coming down to the floor.
            const float hgt = feet.y - a.o.y;  // over the floor (y down)
            f.lowest = std::min(f.lowest + 0.01f * kU * dt, hgt);
            f.since_step += dt;
            if (walking && hgt < f.lowest + 0.015f * kU && f.last_h >= f.lowest + 0.015f * kU && f.since_step > 0.2f) {
                footstep(s, st.speed > 2.5f ? 0.22f : 0.12f);
                f.since_step = 0;
            }
            f.last_h = hgt;
        }
    } else st.feet_valid = false;

    // 5, 6. Arms and hands at the controllers (the ghost copy first takes the body as it stands now).
    static Bone G[kN];
    std::memcpy(G, P, sizeof P);
    bool posed = false;
    for (int h = 0; h < 2; h++) {
        st.beyond[h] = false;
        if (!hands[h].valid) continue;
        arm_to(P, h, hands[h].shown, hands[h].curl, false);
        posed = true;
        if (st.beyond[h] != st.beyond_logged[h]) {
            st.beyond_logged[h] = st.beyond[h];
            rt_log("VR body: frame %d: the %s controller is %s", rt_frame, h == kLeft ? "left" : "right",
                   st.beyond[h] ? "past her arm's reach (her hand stays on the arm; no ghost: only for a hand in a surface)"
                                : "back within her reach");
        }
    }
    st.ghost_bits = 0;
    if (opt.ghost_hands && sk.fingers)
        for (int h = 0; h < 2; h++)
            if (hands[h].valid && hands[h].inside) {  // owner message 45: only pushed into a surface
                arm_to(G, h, hands[h].grip, hands[h].want, true);
                st.ghost_bits |= 1 << h;
            }
    if (st.ghost_bits) {
        std::memcpy(st.ghost, G, sizeof G);
        st.ghost_frame = rt_frame;
    }
    // Hands not tracked: her fingers rest open on her own hands (her hand bones as the game posed them).
    if (sk.fingers) {
        static const float kRest[5] = {0.2f, 0.25f, 0.3f, 0.35f, 0.4f};
        for (int h = 0; h < 2; h++)
            if (!hands[h].valid) pose_fingers(P, h, kRest);
        st.fingers_frame = rt_frame;
    }

    // 7. The fades. Her trunk: only with the eyes inside it (within 10 cm of her spine's line, pelvis to chest: a deep
    // lean or crouch into her own body), or her chest bone come within 21 cm of the eyes (out of it again past 23.5 cm),
    // or while she is knocked down or grabbed: Jak's rule (his chest within 28 cm, 40 standing) on her own measure (31 cm
    // standing, 24 looking down 55 degrees, 19 at 75: there the near fade hollowed her top's middle and left its edges
    // as a white shell, run vr56). Looking down 55 degrees keeps her whole: her top, her skirt's edge, her shoes.
    {
        const Vec3 a = P[kPelvis].o, b = P[kChest].o, ab = b - a;
        const float t = std::clamp(dot(eye - a, ab) / std::max(1.0f, dot(ab, ab)), 0.0f, 1.0f);
        const bool inside = length(eye - (a + ab * t)) < 0.10f * kU;
        st.chest_d = length(eye - b) / kU;
        st.chest_close = st.chest_d < (st.chest_close ? 0.235f : 0.21f);
        st.trunk.update(opt.body_fade && (inside || st.chest_close || st.turned_over));
        st.over.update(opt.body_fade && st.turned_over);
        for (int s = 0; s < 2; s++) {
            const Vec3 hip = P[sk.hip[s]].o, knee = P[sk.knee[s]].o, ank = P[sk.ankle[s]].o;
            const bool near = std::min(length(knee - eye), length(ank - eye)) < 0.30f * kU;
            const bool raised = knee.y < hip.y || ank.y < hip.y;  // y down
            st.leg[s].update(opt.body_fade && (near || raised));
        }
    }

    // What the draws are matched against (body_cut), and her head for the cut.
    for (int b = 0; b < kN; b++) st.drawn[b] = P[b].o;
    st.blends = 0;
    if (m->blend_mats && m->blend_bones)
        for (int j = 0; j < m->blend_count && j < 128; j++) {
            const int k = m->blend_bones[2 * j + 1];
            if (k >= kN) continue;
            const Bone a = load(m->blend_mats + 16 * j);
            st.blend_o[st.blends] = apply(P[k], a.o);
            st.blend_cls[st.blends++] = sk.cls[k];
        }
    st.bones_frame = rt_frame;
    heather_head = P[kHead].o + Vec3{0.0f, -12.0f, 0.0f};
    heather_head_frame = rt_frame;
    if (posed) arms_ik_frame = rt_frame;
    std::memcpy(st.out, P, sizeof P);
    for (int b = 0; b < kN; b++) store(bones + 16 * b, P[b]);

    // SH3_BODY_LOG=N (the PC rig): every N frames, the eyes and her upper body in her own axes from her feet (cm ahead,
    // to her right, up), as posed: where the view sits in her body.
    static int body_log = -2, body_logged = 0;
    if (body_log == -2) body_log = getenv("SH3_BODY_LOG") ? atoi(getenv("SH3_BODY_LOG")) : 0;
    if (body_log > 0 && rt_frame % body_log == 0 && body_logged < 80) {
        body_logged++;
        const Vec3 rt{-fwd.z, 0.0f, fwd.x};
        auto cm = [&](Vec3 p, char* out) {
            const Vec3 d = p - feet;
            std::snprintf(out, 40, "%.1f %.1f %.1f", dot(d, fwd) / kU * 100.0f, dot(d, rt) / kU * 100.0f,
                          -d.y / kU * 100.0f);
        };
        char e[40], le[40], hd[40], nk[40], ch[40], cl[40], sh[40], pv[40], el0[40], el1[40];
        cm(eye, e), cm(neck_w, le), cm(P[kHead].o, hd), cm(P[kNeck].o, nk), cm(P[kChest].o, ch);
        cm(P[sk.collar[kRight]].o, cl), cm(P[sk.shoulder[kRight]].o, sh), cm(P[kPelvis].o, pv);
        cm(P[sk.elbow[kLeft]].o, el0), cm(P[sk.elbow[kRight]].o, el1);
        rt_log("VR body place: frame %d: eyes %s, the player's neck %s | her head %s, neck %s, chest %s, right collar %s, "
               "right shoulder %s, pelvis %s | crouch %.1f cm, lifted %.1f cm, hung %.1f cm, lean %.1f deg, trunk fade %.2f (chest %.1f cm), twist %.0f %.0f deg, elbows "
               "%s %s", rt_frame, e, le, hd, nk,
               ch, cl, sh, pv, st.drop / kU * 100.0f, st.lift / kU * 100.0f, length(st.hang) / kU * 100.0f, quat_angle(st.lean) * 180.0f / kPi, st.trunk.faded(), st.chest_d * 100.0f, st.twist_deg[0],
               st.twist_deg[1], el0, el1);
    }

    static int dump_at = -2;  // SH3_VSC_DUMP=N (the PC rig): the body's pose in frame N
    if (dump_at == -2) dump_at = getenv("SH3_VSC_DUMP") ? atoi(getenv("SH3_VSC_DUMP")) : -1;
    if (rt_frame == dump_at) {
        rt_log("VR body: frame %d: lean %.1f deg, crouch %.1f cm, hung %.1f cm, bend to reach %.1f deg, walk %.2f (%.2f m/s), turned "
               "over %d, fades trunk %.2f legs %.2f %.2f", rt_frame, quat_angle(st.lean) * 180.0f / kPi,
               st.drop / kU * 100.0f, length(st.hang) / kU * 100.0f, quat_angle(st.bend) * 180.0f / kPi, st.walk, st.speed, st.turned_over,
               st.trunk.faded(), st.leg[0].faded(), st.leg[1].faded());
        for (int h = 0; h < 2; h++) {
            if (!hands[h].valid) continue;
            const Bone want = hand_at(h, hands[h].shown);
            rt_log("VR body: %s hand at %.0f %.0f %.0f (wanted %.0f %.0f %.0f, %.1f off), elbow %.0f %.0f %.0f, curls %.2f "
                   "%.2f %.2f %.2f %.2f", h == kLeft ? "left" : "right", P[sk.hand[h]].o.x, P[sk.hand[h]].o.y,
                   P[sk.hand[h]].o.z, want.o.x, want.o.y, want.o.z, length(P[sk.hand[h]].o - want.o), P[sk.elbow[h]].o.x,
                   P[sk.elbow[h]].o.y, P[sk.elbow[h]].o.z, hands[h].curl[0], hands[h].curl[1], hands[h].curl[2],
                   hands[h].curl[3], hands[h].curl[4]);
        }
        for (int s = 0; s < 2; s++)
            rt_log("VR body: %s foot at %.0f %.0f %.0f, the game's %.0f %.0f %.0f, stepping %d", s ? "right" : "left",
                   P[sk.ankle[s]].o.x, P[sk.ankle[s]].o.y, P[sk.ankle[s]].o.z, anim[sk.ankle[s]].o.x,
                   anim[sk.ankle[s]].o.y, anim[sk.ankle[s]].o.z, st.feet[s].stepping);
    }
    return 1;
}

bool own_hands_now() {
    return active && sk.state == 1 && sk.fingers && st.fingers_frame == rt_frame && view_mode == ViewMode::kFirstPerson &&
           !ik_off(1024);  // testing: 1024 the game's own rigid hands
}

int ghost_bones(float* out, int n) {
    if (n != kN || st.ghost_frame != rt_frame || !st.ghost_bits || !own_hands_now()) return 0;
    for (int b = 0; b < kN; b++) store(out + 16 * b, st.ghost[b]);
    return st.ghost_bits;
}

namespace {
bool ghost_on = false;
bool item_on = false;  // the flashlight's own draw in the hand (sh3.c): a held thing, faded only right at the eyes
}
void ghost_pass(bool on) { ghost_on = on; }
void item_pass(bool on) { item_on = on; }

// The chest lamp (session 12, vr188): a point riding on her chest as the game posed it (the game's light, 0x712C1D0,
// from its animation) carried to where VR's pose puts her chest (her lean and crouch), so the lamp, the light it
// gives and her shadow's lamp stay on the chest drawn: with the game's place her own trunk stood ahead of the lamp
// in the game's weapon stances and shaded the beam's middle. False without a whole pose of one frame, or for a carry
// over 60 cm (a load, a scene).
bool ik_chest_carry(Vec3 p, Vec3& out) {
    if (sk.state != 1 || st.bones_frame < rt_frame - 1 || anim_frame != st.bones_frame) return false;
    const Bone& a = anim[kChest];
    const Bone& b = st.out[kChest];
    const Vec3 w = p - a.o;
    Vec3 l;
    float* lv[3] = {&l.x, &l.y, &l.z};
    for (int i = 0; i < 3; i++) {
        const float s = dot(a.r[i], a.r[i]);
        if (s < 1e-8f) return false;
        *lv[i] = dot(w, a.r[i]) / s;  // the game's bones may carry a scale: undone per axis
    }
    out = apply(b, l);
    return std::isfinite(out.x + out.y + out.z) && length(out - p) < 0.6f * kU;
}

// LIST 24 (session 13): the extra weapons (slots 9, 10, 13, 14, 17: no mesh signature, so sh3vr_combat.cpp cannot
// place them at the controller) hang on her ANIMATED hand as the game holds every weapon (its bone 0 on her hand bone,
// session 5). A bone of such a model, as the game keeps it (rows 0-2 its axes, row 3 its origin, world units), carried
// with the hand from the game's pose of it to VR's: her IK hand at the controller. False without a whole pose of this
// frame or the last, or when `on_hand` and the bone's origin is over 12 cm from her animated hand's (not on her hand).
bool ik_hand_carry(int h, float* m, bool on_hand) {
    if (sk.state != 1 || h < 0 || h > 1 || st.bones_frame < rt_frame - 1 || anim_frame != st.bones_frame) return false;
    const Bone& a = anim[sk.hand[h]];
    const Bone& b = st.out[sk.hand[h]];
    const Bone w = load(m);
    if (on_hand && length(w.o - a.o) > 0.12f * kU) return false;
    float inv[3];
    for (int i = 0; i < 3; i++) {
        const float s = dot(a.r[i], a.r[i]);
        if (s < 1e-8f) return false;
        inv[i] = 1.0f / s;  // the game's bones may carry a scale: undone per axis
    }
    auto local = [&](Vec3 d) { return Vec3{dot(d, a.r[0]) * inv[0], dot(d, a.r[1]) * inv[1], dot(d, a.r[2]) * inv[2]}; };
    Bone out;
    for (int i = 0; i < 3; i++) out.r[i] = apply_dir(b, local(w.r[i]));
    out.o = apply(b, local(w.o - a.o));
    if (!std::isfinite(out.o.x + out.o.y + out.o.z) || length(out.o - w.o) > 1.5f * kU) return false;
    store(m, out);
    return true;
}

// LIST 50: hand h's forearm as drawn this frame (world): elbow to wrist; false before her bones are posed.
bool ik_forearm(int h, Vec3& elbow, Vec3& wrist, Vec3* watch_out) {
    if (st.bones_frame != rt_frame || sk.state != 1 || h < 0 || h > 1) return false;
    elbow = st.drawn[sk.elbow[h]];
    wrist = st.drawn[sk.hand[h]];
    if (watch_out) *watch_out = st.watch_frame[h] == rt_frame ? st.watch_out[h] : Vec3{};
    return true;
}

int own_capsules(Capsule* out, int max) {
    if (st.bones_frame != rt_frame || sk.state != 1) return 0;
    int n = 0;
    auto add = [&](Vec3 a, Vec3 b, float r) {
        if (n < max) out[n++] = {a, b, r * kU};
    };
    for (int h = 0; h < 2; h++) {
        const Vec3 S = st.drawn[sk.shoulder[h]], E = st.drawn[sk.elbow[h]], W = st.drawn[sk.hand[h]];
        // Her own sizes (the probes add 1.2 cm): a floor or wall beside her hand is not her (run vr26: 5 cm
        // round the palm took the floor under it for her hand).
        add(S, E, 0.05f);
        add(E, W, 0.04f);
        if (!sk.fingers) {
            add(W, W, 0.08f);
            continue;
        }
        add(W, st.drawn[sk.finger[h][kMiddle][0]], 0.03f);
        for (int f = 0; f < 5; f++) {
            const int* c = sk.finger[h][f];
            add(st.drawn[c[0]], st.drawn[c[1]], 0.016f);
            add(st.drawn[c[1]], st.drawn[c[2]], 0.014f);
            const Vec3 tip = st.drawn[c[2]] + (st.drawn[c[2]] - st.drawn[c[1]]) * 0.8f;
            add(st.drawn[c[2]], tip, 0.013f);
        }
    }
    // Her legs and trunk: crouched, her knees come out in front of her (run vr29: the arm's line behind her thigh read
    // as a wall and held her hand there).
    for (int s = 0; s < 2; s++) {
        add(st.drawn[sk.hip[s]], st.drawn[sk.knee[s]], 0.08f);
        add(st.drawn[sk.knee[s]], st.drawn[sk.ankle[s]], 0.06f);
    }
    add(st.drawn[kPelvis], st.drawn[kChest], 0.15f);
    return n;
}

// LIST 56, her shadow from her own mesh (session 14; the owner, 62-64: "the ratchet and clank shadow works fine and
// shows the whole body... in the images, shadows seem to be cut off, inconsistent", "dont take shortcuts and be proper
// and efficient"). Session 12 cast capsules round her bones, each a prism counted the z-pass way: a caster whose
// volume held the eyes had to be left out whole (or the count turned over there) and a capsule is not her shape, so her
// shadow came and went by pieces and showed rounded sticks. Now each skinned draw of hers in the eyes (her parts, the
// weapon in her hand and a part held for a reload: d3d8.c her_shadow_capture) comes here with the constants it was
// drawn with and is skinned as the game's vertex shader skins it (F0000020/21, run vr102's text: three weights at +12
// and a fourth of 1 less their sum, each weight's palette slot a byte at +24 times c0.w / 255, three rows a bone from
// c48 + 3 slot; the normal at +28 by the same rows). Her shadow is cast from that very mesh (shadow_mesh).
namespace {
struct ShadowPart {
    uint32_t key = 0;
    int frame = -100000;
    int kind = 0;               // 0 her body, 1 the weapon or a part in her hand
    std::vector<Vec3> pos;      // its vertices skinned (world)
    std::vector<uint16_t> tri;  // 3 a face, wound so its cross product points out of her (its skinned normals' side)
};
constexpr int kShadowParts = 48;
ShadowPart shadow_parts[kShadowParts];
int shadow_parts_frame = -100000;  // the last frame a part was caught
}  // namespace

void shadow_part(const uint8_t* v48, int nv, const uint16_t* tris, int nt, const float* c, uint32_t key, int kind) {
    if (ghost_on || item_on || view_mode != ViewMode::kFirstPerson || !v48 || !tris || !c || nv <= 0 || nt <= 0) return;
    // this draw's own slot (the same draw each frame), else the one longest unused
    ShadowPart* s = nullptr;
    ShadowPart* oldest = &shadow_parts[0];
    for (auto& p : shadow_parts) {
        if (p.key == key && p.frame > -100000) {
            s = &p;
            break;
        }
        if (p.frame < oldest->frame) oldest = &p;
    }
    if (!s) s = oldest;
    s->key = key, s->frame = rt_frame, s->kind = kind;
    shadow_parts_frame = rt_frame;
    static std::vector<Vec3> nrm;
    s->pos.resize((size_t)nv);
    nrm.resize((size_t)nv);
    const float one = c[0], scale = c[3] / 255.0f;
    for (int i = 0; i < nv; i++) {
        const uint8_t* x = v48 + (size_t)i * 48;
        float f[12];
        std::memcpy(f, x, sizeof f);
        const float w[4] = {f[3], f[4], f[5], one - one * (f[3] + f[4] + f[5])};
        Vec3 p{}, n{};
        for (int k = 0; k < 4; k++) {
            if (w[k] == 0.0f) continue;
            const int a0 = (int)std::floor((float)x[24 + k] * scale + 1e-3f);
            if (a0 < 0 || a0 > 45) continue;
            const float* r = c + (48 + a0) * 4;
            p = p + Vec3{r[0] * f[0] + r[1] * f[1] + r[2] * f[2] + r[3], r[4] * f[0] + r[5] * f[1] + r[6] * f[2] + r[7],
                         r[8] * f[0] + r[9] * f[1] + r[10] * f[2] + r[11]} * w[k];
            n = n + Vec3{r[0] * f[7] + r[1] * f[8] + r[2] * f[9], r[4] * f[7] + r[5] * f[8] + r[6] * f[9],
                         r[8] * f[7] + r[9] * f[8] + r[10] * f[9]} * w[k];
        }
        s->pos[(size_t)i] = p;
        nrm[(size_t)i] = n;
    }
    s->tri.clear();
    s->tri.reserve((size_t)nt * 3);
    for (int t = 0; t < nt; t++) {
        uint16_t a = tris[t * 3], b = tris[t * 3 + 1], cc = tris[t * 3 + 2];
        if (a >= nv || b >= nv || cc >= nv) continue;
        const Vec3 g = cross(s->pos[b] - s->pos[a], s->pos[cc] - s->pos[a]);
        if (dot(g, nrm[a] + nrm[b] + nrm[cc]) < 0) std::swap(b, cc);
        s->tri.push_back(a), s->tri.push_back(b), s->tri.push_back(cc);
    }
}

namespace {
// Her shadow's volume from her mesh (world triangles, 9 floats each, wound outward by the cross product; `flip` turns
// them for the game camera): every face of hers lit by the lamp (its outside towards the lamp, its middle in the beam
// within `cone`, nearer than `reach`, farther than `clear`) is a closed prism away from the lamp; the prisms of all
// the lit faces are drawn as one volume: the faces themselves (the near cap), the same pushed `len` away from the lamp
// (the far cap, turned over) and a side on each edge as many times as the lit faces' turns about it leave over (an edge
// two lit faces share both ways cancels; a seam's or a part's border edge stays), so it counts as the sum of the
// prisms, with no inner walls. Corners are welded on a 1/32-unit grid (a seam's two vertices, or two parts meeting,
// are one corner, the volume built from those places so faces meeting there meet exactly). Counted the z-fail way
// (d3d8.c vr_her_shadow), which a closed volume needs and which stays right with the eyes inside it (session 12's
// count had to leave such casters out). -1: no part of hers caught in the last two frames; 0 nothing lit.
struct WeldCell {
    int32_t x, y, z;
    int id, gen;
};
struct EdgeCell {
    uint32_t a, b;
    int count, gen;
};
constexpr int kWeldSize = 1 << 15, kEdgeSize = 1 << 17, kMaxShadowCorners = 16384;
WeldCell weld_cells[kWeldSize];
EdgeCell edge_cells[kEdgeSize];
Vec3 shadow_near[kMaxShadowCorners], shadow_far[kMaxShadowCorners];
int shadow_gen = 0;

int shadow_mesh(Vec3 lp, Vec3 ld, bool flip, float cone, float reach, float clear, float len, float* out, int max_tris,
                int counts[5]) {
    if (shadow_parts_frame < rt_frame - 1) return -1;
    shadow_gen++;
    int ncorner = 0;
    auto corner_id = [&](Vec3 p) {
        const int32_t qx = (int32_t)std::lround(p.x * 32.0f), qy = (int32_t)std::lround(p.y * 32.0f),
                      qz = (int32_t)std::lround(p.z * 32.0f);
        const uint32_t h = ((uint32_t)qx * 73856093u) ^ ((uint32_t)qy * 19349663u) ^ ((uint32_t)qz * 83492791u);
        for (int probe = 0; probe < kWeldSize; probe++) {
            WeldCell& w = weld_cells[(h + (uint32_t)probe) & (kWeldSize - 1)];
            if (w.gen != shadow_gen) {
                if (ncorner >= kMaxShadowCorners) return -1;
                w = {qx, qy, qz, ncorner, shadow_gen};
                shadow_near[ncorner] = {(float)qx / 32.0f, (float)qy / 32.0f, (float)qz / 32.0f};
                return ncorner++;
            }
            if (w.x == qx && w.y == qy && w.z == qz) return w.id;
        }
        return -1;
    };
    static std::vector<int> lit, used;
    lit.clear();
    used.clear();
    const int max_lit = max_tris / 2;
    int parts = 0, unlit = 0, kinds[2] = {0, 0};
    for (const auto& s : shadow_parts) {
        if (s.frame < rt_frame - 1 || s.pos.empty()) continue;
        parts++;
        for (size_t t = 0; t + 2 < s.tri.size(); t += 3) {
            const Vec3 a = s.pos[s.tri[t]], b = s.pos[s.tri[t + 1]], c = s.pos[s.tri[t + 2]];
            const Vec3 m = (a + b + c) * (1.0f / 3.0f), d = m - lp;
            const float l = length(d);
            if (dot(cross(b - a, c - a), lp - m) <= 0 || l > reach || l < clear || dot(d, ld) < cone * l) {
                unlit++;
                continue;
            }
            if ((int)lit.size() / 3 >= max_lit) break;
            const int ia = corner_id(a), ib = corner_id(b), ic = corner_id(c);
            if (ia < 0 || ib < 0 || ic < 0 || ia == ib || ib == ic || ia == ic) continue;
            lit.push_back(ia), lit.push_back(ib), lit.push_back(ic);
            kinds[s.kind ? 1 : 0]++;
        }
    }
    // each lit face's edges, counted by their turn (+1 from the lower corner to the higher, -1 back)
    for (size_t t = 0; t < lit.size(); t += 3)
        for (int e = 0; e < 3; e++) {
            const uint32_t u = (uint32_t)lit[t + (size_t)e], v = (uint32_t)lit[t + (size_t)((e + 1) % 3)];
            const uint32_t lo = std::min(u, v), hi = std::max(u, v);
            const uint32_t h = lo * 2654435761u ^ hi * 40503u;
            for (int probe = 0; probe < kEdgeSize; probe++) {
                const int at = (int)((h + (uint32_t)probe) & (kEdgeSize - 1));
                EdgeCell& ec = edge_cells[at];
                if (ec.gen != shadow_gen) {
                    ec = {lo, hi, 0, shadow_gen};
                    used.push_back(at);
                }
                if (ec.a == lo && ec.b == hi) {
                    ec.count += u == lo ? 1 : -1;
                    break;
                }
            }
        }
    int sides = 0;
    for (const int at : used) sides += std::abs(edge_cells[at].count);
    const int nlit = (int)lit.size() / 3;
    counts[0] = kinds[0], counts[1] = kinds[1], counts[2] = sides, counts[3] = unlit, counts[4] = parts;
    if (nlit == 0) return 0;
    if (2 * nlit + 2 * sides > max_tris) {
        static int told = 0;
        if (told++ < 5)
            rt_log("VR: frame %d: her shadow from her mesh is over its %d triangles (%d faces lit, %d sides): not cast",
                   rt_frame, max_tris, nlit, sides);
        return 0;
    }
    for (int i = 0; i < ncorner; i++) shadow_far[i] = shadow_near[i] + normalized(shadow_near[i] - lp) * len;
    int n = 0;
    auto tri = [&](Vec3 a, Vec3 b, Vec3 c) {
        if (flip) std::swap(b, c);
        float* o = out + n * 9;
        o[0] = a.x, o[1] = a.y, o[2] = a.z, o[3] = b.x, o[4] = b.y, o[5] = b.z, o[6] = c.x, o[7] = c.y, o[8] = c.z;
        n++;
    };
    const Vec3* P = shadow_near;
    const Vec3* F = shadow_far;
    for (size_t t = 0; t < lit.size(); t += 3) {
        const int a = lit[t], b = lit[t + 1], c = lit[t + 2];
        tri(P[a], P[b], P[c]);
        tri(F[a], F[c], F[b]);
    }
    for (const int at : used) {
        const EdgeCell& ec = edge_cells[at];
        if (ec.count == 0) continue;
        const int from = (int)(ec.count > 0 ? ec.a : ec.b), to = (int)(ec.count > 0 ? ec.b : ec.a);
        for (int k = std::abs(ec.count); k > 0; k--) {
            tri(P[from], F[from], P[to]);
            tri(P[to], F[from], F[to]);
        }
    }
    return n;
}
}  // namespace

// LIST 56, her shadow (session 12; owner messages 49-50: "making sure heathers full body ik is also taken into account
// for the games stencil shadows", "be sure all objects like weapons, melee weapons, etc, also are taken into account").
// The game's stencil shadows are only the room's own geometry: 0x491320 sets 0x48B930's states and 0x491030 draws the
// area's casters (0x687F30); in vr183's frame 7455 her one part draw was outside any stencil pass and no shadow of her
// shows in the pictures. So VR adds hers to that same count: her body as drawn this frame (the IK's bones: head, neck,
// trunk, collars, arms, palms, every finger joint, legs, feet), the weapon in her hand (guns and melee: its measured
// capsules, the muzzle flash's left out) and a magazine, shell or battery held for a reload, each a closed volume from
// the flashlight drawn into the game's stencil just before its darkening step (d3d8.c), so her shadow is the game's own:
// as soft, as dark, from the same lamp. A caster is a quad through its capsule's axis across the light's way (a palm
// flat in its own plane), its prism running 8 m on from the lamp; the triangles are wound outward and turned so the
// game's camera takes their outsides as its front faces (D3D's clockwise), as the game's volumes are counted: front
// faces add one, back faces take one away (the stencil starts at 16, shadow at 17 and over: a miscount only loses a
// little shadow, it never darkens). Left out: a caster the lamp is in or at (her chest round the chest lamp, the hand
// round a held lamp), one over 3 m from it or more than 50 degrees off the beam, and one whose volume the eyes are in.
// Session 14: cast from her own mesh (shadow_mesh) whenever her draws were caught; these capsules are only for a frame
// without them. Both are counted the z-fail way now, so no caster is left out for the eyes being in its shadow.
int her_shadow_tris(const float view[16], const float proj[16], float* out, int max_tris, int counts[5]) {
    counts[0] = counts[1] = counts[2] = counts[3] = counts[4] = 0;
    if (view_mode != ViewMode::kFirstPerson || sk.state != 1 || st.bones_frame < rt_frame - 1) return 0;
    Vec3 lp, ld;
    float level = 0;
    if (!combat_beam(lp, ld, level)) return 0;
    ld = normalized(ld);
    // The game's camera from its view matrix (D3D rows: world row vector times the matrix): its place and axes.
    float m[3][3], inv[3][3];
    for (int i = 0; i < 3; i++)
        for (int j = 0; j < 3; j++) m[i][j] = view[i * 4 + j];
    const float det = m[0][0] * (m[1][1] * m[2][2] - m[1][2] * m[2][1]) - m[0][1] * (m[1][0] * m[2][2] - m[1][2] * m[2][0]) +
                      m[0][2] * (m[1][0] * m[2][1] - m[1][1] * m[2][0]);
    if (std::fabs(det) < 1e-12f) return 0;
    for (int i = 0; i < 3; i++)
        for (int j = 0; j < 3; j++) {
            const int a1 = (j + 1) % 3, a2 = (j + 2) % 3, b1 = (i + 1) % 3, b2 = (i + 2) % 3;
            inv[i][j] = (m[a1][b1] * m[a2][b2] - m[a1][b2] * m[a2][b1]) / det;
        }
    const float t[3] = {view[12], view[13], view[14]};
    Vec3 cam;
    {
        float c[3];
        for (int j = 0; j < 3; j++) c[j] = -(t[0] * inv[0][j] + t[1] * inv[1][j] + t[2] * inv[2][j]);
        cam = {c[0], c[1], c[2]};
    }
    const Vec3 right = normalized(Vec3{inv[0][0], inv[0][1], inv[0][2]}), up = normalized(Vec3{inv[1][0], inv[1][1], inv[1][2]});
    const Vec3 fwd = normalized(Vec3{inv[2][0], inv[2][1], inv[2][2]});
    auto screen = [&](Vec3 p, float& x, float& y) {
        float v[4], c[4];
        const float in[4] = {p.x, p.y, p.z, 1.0f};
        for (int j = 0; j < 4; j++) v[j] = in[0] * view[j] + in[1] * view[4 + j] + in[2] * view[8 + j] + in[3] * view[12 + j];
        for (int j = 0; j < 4; j++) c[j] = v[0] * proj[j] + v[1] * proj[4 + j] + v[2] * proj[8 + j] + v[3] * proj[12 + j];
        if (c[3] <= 1e-6f) return false;
        x = c[0] / c[3], y = -c[1] / c[3];  // D3D's screen: y down
        return true;
    };
    // Which way round a triangle facing the camera comes out on its screen: clockwise (D3D's front) keeps the outward
    // winding, else every triangle is turned over.
    bool flip;
    {
        const float k = 0.5f * kU;
        const Vec3 a = cam + fwd * k;
        Vec3 b = a + right * (0.2f * k), c = a + up * (0.2f * k);
        if (dot(cross(b - a, c - a), cam - a) < 0) std::swap(b, c);
        float ax, ay, bx, by, cx, cy;
        if (!screen(a, ax, ay) || !screen(b, bx, by) || !screen(c, cx, cy)) return 0;
        const float area = (bx - ax) * (cy - ay) - (by - ay) * (cx - ax);
        if (std::fabs(area) < 1e-9f) return 0;
        flip = area < 0;
    }
    // How far the volumes run on from the lamp: z-fail counts their far ends, so those stay this side of the game
    // camera's far plane (D3D's projection: z' = z A + B, w = z; far = -B / (A - 1)), 6 m at most.
    float zfar = 1e9f;
    if (std::fabs(proj[10] - 1.0f) > 1e-6f && proj[11] > 0.5f) {
        const float f = -proj[14] / (proj[10] - 1.0f);
        if (f > 0.0f) zfar = f;
    }
    const float kFar = std::clamp(zfar * 0.85f - 1.5f * kU, 1.0f * kU, 6.0f * kU);
    {
        static int told = 0;
        if (!told++)
            rt_log("VR: her shadow: the game camera's far plane %.0f units (%.1f m; projection %.4f %.4f %.4f), her "
                   "volumes run %.1f m",
                   zfar, zfar / kU, proj[10], proj[11], proj[14], kFar / kU);
    }
    const float kCone = 0.643f /* 50 degrees */, kReach = 3.0f * kU, kClear = 0.04f * kU;
    {
        // her own mesh: every face the beam lights within 30 degrees of its line (the game's beam is dark past about
        // 28, vr117), to 3 m, not within 3 cm of the lamp
        const int got = shadow_mesh(lp, ld, flip, 0.866f, kReach, 0.03f * kU, kFar, out, max_tris, counts);
        if (got >= 0) return got;
    }
    int n = 0;
    auto tri = [&](Vec3 a, Vec3 b, Vec3 c) {
        if (flip) std::swap(b, c);
        float* o = out + n * 9;
        o[0] = a.x, o[1] = a.y, o[2] = a.z, o[3] = b.x, o[4] = b.y, o[5] = b.z, o[6] = c.x, o[7] = c.y, o[8] = c.z;
        n++;
    };
    auto seg_dist = [](Vec3 p, Vec3 a, Vec3 b) {
        const Vec3 ab = b - a;
        const float tt = std::clamp(dot(p - a, ab) / std::max(dot(ab, ab), 1e-6f), 0.0f, 1.0f);
        return length(p - (a + ab * tt));
    };
    auto in_beam = [&](Vec3 p) {
        const Vec3 d = p - lp;
        const float l = length(d);
        return l < kReach && l > 1e-3f && dot(d, ld) > kCone * l;
    };
    // A caster's outline: a convex polygon (up to 12 corners, one plane) and its facing (Newell's sum about its middle).
    constexpr int kMaxCorners = 12;
    auto facing = [](const Vec3* q, int nq, Vec3& mid) {
        mid = {};
        for (int i = 0; i < nq; i++) mid = mid + q[i];
        mid = mid * (1.0f / (float)nq);
        Vec3 s{};
        for (int i = 0; i < nq; i++) s = s + cross(q[i] - mid, q[(i + 1) % nq] - mid);
        return s;
    };
    // A prism from the outline away from the lamp: its near face (the outline, facing the lamp), its far face, its sides
    // (closed, as the z-fail count needs; the eyes may be inside it).
    auto prism = [&](const Vec3* qin, int nq, int kind) {
        if (nq < 3 || nq > kMaxCorners || n + 4 * nq - 4 > max_tris) return;
        Vec3 q[kMaxCorners], f[kMaxCorners], mid;
        const bool turn = dot(facing(qin, nq, mid), lp - mid) < 0;
        for (int i = 0; i < nq; i++) q[i] = qin[turn ? nq - 1 - i : i];
        for (int i = 0; i < nq; i++) f[i] = q[i] + normalized(q[i] - lp) * kFar;
        for (int i = 1; i + 1 < nq; i++) {
            tri(q[0], q[i], q[i + 1]);
            tri(f[0], f[i + 1], f[i]);
        }
        for (int i = 0; i < nq; i++) {
            const int j = (i + 1) % nq;
            tri(q[i], f[i], q[j]);
            tri(q[j], f[i], f[j]);
        }
        counts[kind]++;
    };
    // A capsule (world, r in units): its outline across the light's way, rounded at both ends as a capsule's shadow is
    // (vr184: square-ended quads gave her legs boxy shadows on the floor).
    // The caster drawn nearest the lamp (its surface), told now and then: a caster close to the lamp throws the
    // biggest shadow (vr188: a U-shaped shade in the beam's middle with both hands out to the sides).
    const char* near_what = nullptr;
    float near_d = 1e9f;
    auto capsule = [&](Vec3 a, Vec3 b, float r, int kind, const char* what = "a finger") {
        const float dl = seg_dist(lp, a, b);
        if (dl < r + kClear || !(in_beam(a) || in_beam(b) || in_beam((a + b) * 0.5f))) {
            counts[3]++;
            return;
        }
        if (dl - r < near_d) near_d = dl - r, near_what = what;
        Vec3 d = (a + b) * 0.5f - lp;
        d = normalized(d);
        Vec3 u = b - a, w{};
        const float len = length(u);
        if (len > 1e-3f) {
            u = u * (1.0f / len);
            w = cross(u, d);
        }
        if (len <= 1e-3f || length(w) < 0.05f) {
            // a ball, or a rod pointing at the lamp: its shadow a disc round its nearer end
            const Vec3 nearer = length(a - lp) < length(b - lp) ? a : b;
            a = b = nearer;
            w = normalized(cross(d, std::fabs(d.y) < 0.9f ? Vec3{0, 1, 0} : Vec3{1, 0, 0}));
            u = normalized(cross(d, w));
        } else
            w = normalized(w);
        // round a's end from +w through -u to -w, then b's from -w through +u to +w (45 degrees a corner)
        Vec3 q[10];
        for (int i = 0; i < 5; i++) {
            const float ta = kPi * (0.5f + 0.25f * (float)i), tb = kPi * (-0.5f + 0.25f * (float)i);
            q[i] = a + (u * std::cos(ta) + w * std::sin(ta)) * r;
            q[5 + i] = b + (u * std::cos(tb) + w * std::sin(tb)) * r;
        }
        prism(q, 10, kind);
    };
    const auto& D = st.drawn;
    // A lamp worn on her chest sits on her trunk's front: her trunk and collars cannot shade anything it lights (their
    // shadows fall behind her), and their rough capsules (13 cm round her spine) stood ahead of the lamp in the game's
    // weapon stances (vr188). A flashlight in a hand keeps them: held high, it throws her body's shadow ahead.
    const bool chest_lamp = !combat_light_held();
    // Her body (kind 0).
    for (int h = 0; h < 2; h++) {
        const Vec3 Cl = D[sk.collar[h]], S = D[sk.shoulder[h]], E = D[sk.elbow[h]], W = D[sk.hand[h]];
        if (chest_lamp)
            counts[3]++;
        else
            capsule(Cl, S, 0.045f * kU, 0, h ? "the right collar" : "the left collar");
        capsule(S, E, 0.045f * kU, 0, h ? "the right upper arm" : "the left upper arm");
        capsule(E, W, 0.035f * kU, 0, h ? "the right forearm" : "the left forearm");
        if (!sk.fingers) {
            capsule(W, W, 0.06f * kU, 0, h ? "the right hand" : "the left hand");
            continue;
        }
        const Vec3 K1 = D[sk.finger[h][kIndex][0]], K2 = D[sk.finger[h][kMiddle][0]], K4 = D[sk.finger[h][kPinky][0]];
        // the palm flat in its own plane (wrist 5.6 cm across to the knuckles), and its thickness along its middle
        if (seg_dist(lp, W, K2) < 0.06f * kU || !(in_beam(W) || in_beam(K1) || in_beam(K4)))
            counts[3]++;
        else {
            const Vec3 a = normalized(K1 - K4) * (0.028f * kU);
            const Vec3 q[4] = {W + a, K1, K4, W - a};
            prism(q, 4, 0);
        }
        capsule(W, K2, 0.014f * kU, 0, h ? "the right palm" : "the left palm");
        for (int f = 0; f < 5; f++) {
            const int* c = sk.finger[h][f];
            const float r0 = f == kThumb ? 0.011f : 0.0095f, r1 = f == kThumb ? 0.0095f : 0.0085f,
                        r2 = f == kThumb ? 0.0085f : 0.0075f;
            capsule(D[c[0]], D[c[1]], r0 * kU, 0);
            capsule(D[c[1]], D[c[2]], r1 * kU, 0);
            capsule(D[c[2]], D[c[2]] + (D[c[2]] - D[c[1]]) * 0.8f, r2 * kU, 0);
        }
    }
    for (int s = 0; s < 2; s++) {
        capsule(D[sk.hip[s]], D[sk.knee[s]], 0.075f * kU, 0, s ? "the right thigh" : "the left thigh");
        capsule(D[sk.knee[s]], D[sk.ankle[s]], 0.055f * kU, 0, s ? "the right shin" : "the left shin");
        // the foot: the ankle's own child bone (her toes), carried on a little past it
        for (int b = 0; b < kN; b++)
            if (sk.parent[b] == sk.ankle[s]) {
                capsule(D[sk.ankle[s]], D[sk.ankle[s]] + (D[b] - D[sk.ankle[s]]) * 1.3f, 0.045f * kU, 0,
                        s ? "the right foot" : "the left foot");
                break;
            }
    }
    if (chest_lamp)
        counts[3] += 2;
    else {
        capsule(D[kPelvis], D[kChest], 0.13f * kU, 0, "her trunk");
        capsule(D[kChest], D[kNeck], 0.06f * kU, 0, "her upper chest");
    }
    {
        // her head and hair, as the body cut takes them (sh3vr_ik.cpp body_cut): from over her neck to past its top
        const Vec3 nk = D[kNeck], hd = D[kHead], hu = hd - nk;
        const float ul = length(hu);
        const Vec3 dir = ul > 1.0f ? hu * (1.0f / ul) : Vec3{0.0f, -1.0f, 0.0f};
        capsule(nk + hu * 0.3f, hd + dir * (0.09f * kU), 0.09f * kU, 0, "her head");
    }
    // The weapon in her hand (kind 1): guns and melee weapons by their measured capsules (world, units).
    {
        Capsule wc[24];
        const int nw = weapon_capsules(wc, 24, false);
        for (int i = 0; i < nw; i++) capsule(wc[i].a, wc[i].b, std::max(wc[i].r, 0.008f * kU), 1, "the weapon");
    }
    // A magazine, shell or battery in a hand for a reload (kind 2).
    for (int h = 0; h < 2; h++) {
        const int kind = reload_held_part(h);
        Vec3 tip, axis;
        if (kind < 0 || !parts_held_tip(h, tip, axis)) continue;
        static const float kR[] = {0.015f, 0.017f, 0.011f, 0.015f};
        const float r = kind < 4 ? kR[kind] : 0.015f;
        capsule(world_point(tip - axis * parts_length(kind)), world_point(tip), r * kU, 2, "a reload part");
    }
    static int near_told = 0, near_at = -100000;
    if (near_what && near_d < 0.30f * kU && near_told < 30 && rt_frame - near_at >= 120) {
        near_told++;
        near_at = rt_frame;
        rt_log("VR: frame %d: her shadow's caster nearest the %s lamp: %s, %.2f m from it", rt_frame,
               chest_lamp ? "chest" : "held", near_what, near_d / kU);
    }
    return n;
}

// Her own draws lit by her flashlight in first person (session 8; run vr116: her hand reaching into the beam stayed
// dark, grey 23 against the lit wall's 91, and moving the beam's shadow caster changed nothing, vr117/vr118). The game
// never lights her by her own lamp: her skinned shaders (F0000030 in the flashlight run's numbering) light her by four
// directional lights (c8-c11 their directions, c12-c14 their red, green and blue, a light a column) and an ambient
// (c6), times c7, and the game fills three (c11 and the fourth column stay 0, vr119's constants); the point lights
// (c16-c22) are not in her shader and the beam's cone (c80-c82) is only in the world's. So a draw of hers (her bones
// matched as body_cut does) or of the weapon in her hand takes the beam as the fourth light: from the lamp towards the
// draw's middle (its bones' mean), as bright as the beam is there (whole within 15 degrees of its line, none past 30,
// as the game's own beam on the wall fades from about 20 to 28 degrees, vr117; vr120 at 22-38 lit her far arm beside the
// dark floor;
// whole to 1.5 m, none past 3 m), times the light's level (0 off; it ramps while turning on and off).
// Session 9 (vr136, vr139: her forearm's draw lit whole and her hand's beside it, its middle out of the cone, flat grey
// at the wristband): the same light per point of the draw, in `beam` for the vertex stage (d3d8_shader.c), from the lamp
// to each point and as bright as the beam is at that point, so a hand reaching into the beam lights where it is in it,
// and its side facing away stays dark. Filled for every draw of hers while the light is on, its middle in the cone or
// not; the per-draw light in c11-c14 stays for a vertex stage without it.
constexpr float kHerLight = 1.3f;  // the beam's light on her at full (a little warm); her shader halves it about (c7)
int body_light(float vsc[96][4], int pal_count, Sh3VrBeam* beam) {
    if (!active || ghost_on || view_mode != ViewMode::kFirstPerson || !game.player_valid || pal_count <= 0) return 0;
    Vec3 lp, ld;
    float level = 0;
    if (!combat_beam(lp, ld, level)) return 0;
    for (int i = 0; i < 4; i++)
        if (vsc[11][i] != 0) return 0;  // the fourth light in use (never seen): left as the game has it
    if (vsc[12][3] != 0 || vsc[13][3] != 0 || vsc[14][3] != 0) return 0;
    const bool weapon = last_draw_was_weapon();
    const bool known = st.bones_frame == rt_frame;
    Vec3 sum{};
    int n = 0, matched = 0, unmatched = 0;
    for (int b = 0; b < std::min(pal_count, 16); b++) {
        bool ok = true;
        for (int r = 0; r < 3 && ok; r++) {
            const float* row = vsc[48 + b * 3 + r];
            const float l = row[0] * row[0] + row[1] * row[1] + row[2] * row[2];
            ok = std::isfinite(l) && std::fabs(l - 1.0f) <= 0.08f && std::isfinite(row[3]);
        }
        if (!ok) continue;
        const Vec3 o{vsc[48 + b * 3][3], vsc[48 + b * 3 + 1][3], vsc[48 + b * 3 + 2][3]};
        sum = sum + o, n++;
        if (weapon || !known) continue;
        bool found = false;
        for (int k = 0; k < kN && !found; k++)
            found = std::fabs(st.drawn[k].x - o.x) < 0.5f && std::fabs(st.drawn[k].y - o.y) < 0.5f &&
                    std::fabs(st.drawn[k].z - o.z) < 0.5f;
        for (int k = 0; k < st.blends && !found; k++)
            found = std::fabs(st.blend_o[k].x - o.x) < 0.5f && std::fabs(st.blend_o[k].y - o.y) < 0.5f &&
                    std::fabs(st.blend_o[k].z - o.z) < 0.5f;
        if (found) matched++;
        else unmatched++;
    }
    if (n == 0) return 0;
    const Vec3 mid = sum * (1.0f / n);
    if (!weapon) {
        if (known && (matched == 0 || unmatched > matched)) return 0;  // another character's, a monster's
        const float dx = mid.x - game.pos[0], dz = mid.z - game.pos[2];
        if (!known && dx * dx + dz * dz > (0.9f * kU) * (0.9f * kU)) return 0;
    }
    Vec3 v = mid - lp;
    const float d = length(v);
    if (!(d > 1.0f)) return 0;
    v = v * (1.0f / d);
    auto smooth = [](float a, float b, float x) {
        const float t = std::clamp((x - a) / (b - a), 0.0f, 1.0f);
        return t * t * (3.0f - 2.0f * t);
    };
    const float cone = smooth(std::cos(30.0f * kPi / 180.0f), std::cos(15.0f * kPi / 180.0f), dot(v, ld));
    const float k = kHerLight * level * cone * (1.0f - smooth(1.5f * kU, 3.0f * kU, d));
    if (beam && level > 0.0f && d < 3.5f * kU && clip_to_world(beam->clip_to_world)) {
        beam->pos[0] = lp.x, beam->pos[1] = lp.y, beam->pos[2] = lp.z, beam->pos[3] = 1.0f;
        beam->dir[0] = ld.x, beam->dir[1] = ld.y, beam->dir[2] = ld.z, beam->dir[3] = kHerLight * level;
        beam->cone[0] = 1.5f * kU, beam->cone[1] = 3.0f * kU;
        beam->cone[2] = std::cos(30.0f * kPi / 180.0f), beam->cone[3] = std::cos(15.0f * kPi / 180.0f);
        static bool told_beam = false;
        if (!told_beam) {
            told_beam = true;
            rt_log("VR: frame %d: her flashlight lights her draws per point (the lamp at %.0f %.0f %.0f, along %.2f "
                   "%.2f %.2f, %.2f at full)", rt_frame, lp.x, lp.y, lp.z, ld.x, ld.y, ld.z, beam->dir[3]);
        }
    }
    if (k < 0.01f) return 0;
    vsc[11][0] = v.x, vsc[11][1] = v.y, vsc[11][2] = v.z, vsc[11][3] = 0;  // the light's way (the shader: -N . it)
    vsc[12][3] = k, vsc[13][3] = k * 0.95f, vsc[14][3] = k * 0.85f;
    static int told = 0;  // the first few times, with the numbers, for the log
    if (told < 3 && cone > 0.5f) {
        told++;
        const float off = std::acos(std::clamp(dot(v, ld), -1.0f, 1.0f)) * 180.0f / kPi;
        rt_log("VR: frame %d: her flashlight lights a draw of %s %.2f m from the lamp, %.0f degrees off the beam: %.2f",
               rt_frame, weapon ? "the weapon in her hand" : "hers", d / kU, off, k);
    }
    return 1;
}

// Each draw of her in first person: what of it is cut or faded (d3d8_shader.c; docs/GAME_FACTS.md, Bones: a skinned draw's
// palette rows carry each bone's or blend's origin in w).
int body_cut(const float vsc[96][4], uint32_t stride, int main_cam, int ffp, int pal_count, Sh3VrCut* out) {
    std::memset(out, 0, sizeof *out);
    if (!active) return 0;
    if (ghost_on) {
        // The ghost hand: faint and through the walls, for the eyes only (her shadow and the rest draw nothing).
        if (!main_cam || ffp) out->hide = 1;
        else out->fade[3] = 0.32f;
        return 1;
    }
    if (!main_cam || ffp || view_mode != ViewMode::kFirstPerson || !game.player_valid || pal_count < 0) return 0;
    // Jak's 0.1.9 near-eye fade for everything else (its LIST-0.1.9 item 22): another character, a monster or the
    // weapon in her hand that comes into the eyes fades out as her own body does, never cut open by the near plane to
    // show its inside. The weapon only from 9 cm in (a gun raised to the eye to aim stays whole).
    auto near_only = [&](float gone, float whole) {
        if (!opt.body_fade || !map_ready() || !clip_to_world(out->clip_to_world)) return 0;
        const Vec3 e = world_point(fi.head.position);
        out->eye[0] = e.x, out->eye[1] = e.y, out->eye[2] = e.z, out->eye[3] = 1.0f;
        out->fade[1] = gone * kU, out->fade[2] = whole * kU;
        return 1;
    };
    if (last_draw_was_weapon() || item_on) return near_only(0.05f, 0.09f);
    const int slots = pal_count > 0 ? std::min(pal_count, 16) : 16;
    auto affine = [&](int b) {
        for (int r = 0; r < 3; r++) {
            const float* row = vsc[48 + b * 3 + r];
            const float l = row[0] * row[0] + row[1] * row[1] + row[2] * row[2];
            if (!std::isfinite(l) || std::fabs(l - 1.0f) > 0.08f || !std::isfinite(row[3])) return false;
        }
        return true;
    };
    const Vec3 eye = map_ready() ? world_point(fi.head.position) : Vec3{};
    if (st.bones_frame != rt_frame) {
        // Her bones are not known this frame: what of her is nearer than 25 cm is cut (all of her when hidden).
        int near = 0, far = 0;
        for (int b = 0; b < slots; b++) {
            if (!affine(b)) continue;
            const float x = vsc[48 + b * 3][3], y = vsc[48 + b * 3 + 1][3], z = vsc[48 + b * 3 + 2][3];
            const float dx = x - game.pos[0], dz = z - game.pos[2], upw = game.pos[1] - y;
            if (dx * dx + dz * dz < (0.9f * kU) * (0.9f * kU) && upw > -0.2f * kU && upw < 2.0f * kU) near++;
            else far++;
        }
        if (near < 2 || far > 0) return near_only(0.07f, 0.16f);
        out->near_cut = (opt.body == 1 ? 3.0f : 0.25f) * kU;
        (void)stride;
        return 1;
    }
    // Match the palette to her bones and blends as drawn this frame.
    unsigned classes = 0;
    int matched = 0, unmatched = 0;
    bool only_hand_bones = true, all_near = true, all_at_head = true;
    for (int b = 0; b < slots; b++) {
        if (!affine(b)) continue;
        const Vec3 o{vsc[48 + b * 3][3], vsc[48 + b * 3 + 1][3], vsc[48 + b * 3 + 2][3]};
        if (length(o - heather_head) > 0.30f * kU) all_at_head = false;
        {
            const float dx = o.x - game.pos[0], dz = o.z - game.pos[2], upw = game.pos[1] - o.y;
            if (!(dx * dx + dz * dz < (1.6f * kU) * (1.6f * kU) && upw > -0.3f * kU && upw < 2.2f * kU)) all_near = false;
        }
        int c = -1;
        for (int k = 0; k < kN && c < 0; k++)
            if (std::fabs(st.drawn[k].x - o.x) < 0.5f && std::fabs(st.drawn[k].y - o.y) < 0.5f &&
                std::fabs(st.drawn[k].z - o.z) < 0.5f)
                c = sk.cls[k];
        for (int k = 0; k < st.blends && c < 0; k++)
            if (std::fabs(st.blend_o[k].x - o.x) < 0.5f && std::fabs(st.blend_o[k].y - o.y) < 0.5f &&
                std::fabs(st.blend_o[k].z - o.z) < 0.5f)
                c = st.blend_cls[k];
        if (c < 0) {
            unmatched++;
            continue;
        }
        matched++;
        classes |= 1u << c;
        if (c != kHandC) only_hand_bones = false;
    }
    // Not hers (another character, the world): nothing of it matched, or most of it did not and it is not all at her.
    if (matched == 0 || (unmatched > matched && !all_near)) {
        // Something drawn with bones all at her head that are not hers as posed (her hair drawn on its own, run vr51:
        // a lock of it hung in the air ahead of the eyes): never seen from her own eyes (her shadow keeps it).
        if (all_at_head && matched + unmatched > 0) {
            out->near_cut = 0.07f * kU;
            out->fade[0] = 1.0f;
            return 1;
        }
        return near_only(0.07f, 0.16f);
    }
    {
        static int dump_at = -2, logged = 0;  // SH3_VSC_DUMP=N: each of her draws' parts in frame N
        if (dump_at == -2) dump_at = getenv("SH3_VSC_DUMP") ? atoi(getenv("SH3_VSC_DUMP")) : -1;
        if (rt_frame == dump_at && logged++ < 64)
            rt_log("VR body: her draw (palette %d): %d bones matched, %d not, parts %02x (trunk 1 head 2 legs 4/8 arm 16 "
                   "hand 32 fingers 64)", slots, matched, unmatched, classes);
    }
    // Her hands: never cut or faded.
    if (classes & (1u << kFingerC) || only_hand_bones) return 0;
    if (!clip_to_world(out->clip_to_world)) {
        out->near_cut = 0.12f * kU;
        return 1;
    }
    out->eye[0] = eye.x, out->eye[1] = eye.y, out->eye[2] = eye.z, out->eye[3] = 1.0f;
    const bool has_trunk = classes & (1u << kTrunkC);
    const bool has_arm = classes & (1u << kArmC);
    const bool has_leg = classes & ((1u << kLegLeftC) | (1u << kLegRightC));
    // What of her is at the eyes: dithered out by its distance from them (Jak's smooth near fade), gone inside 5 cm and
    // whole from 20, with a hard cut only at 3 cm (the eyes' own place). Run vr90 (session 7): the old hard cut at 7 cm
    // of depth sliced her shoulder's top, which looking down 65 degrees is level with the eyes and seen edge on, into a
    // sliver standing out of her top to the left (gone with the cut off: the shoulder whole at the eyes). Her arms
    // alone keep the old 7-16 cm. With "Fade body near your eyes" off: the old hard cut at 7 cm, no fade.
    const bool arm_only = has_arm && !has_trunk && !(classes & (1u << kHeadC));
    out->near_cut = (opt.body_fade ? 0.03f : 0.07f) * kU;  // nothing of her inside the eyes, faded or not
    float fade = 0, near_from = arm_only ? 0.07f : 0.05f, near_to = arm_only ? 0.16f : 0.20f;
    if (classes & (1u << kHeadC)) {
        // Her head and hair: a capsule from just over her neck to past the top of her head, 14 cm round (her hair
        // hangs to her jaw and past the old 17 cm sphere about her head; the owner, session 6: "pretty sure i see some
        // hair"), cut inside 11 cm and dithered out to 14; her shoulders (18 cm from it) and her shadow stay whole.
        const Vec3 nk = st.drawn[kNeck], hd = st.drawn[kHead];
        const Vec3 up = hd - nk;
        const float ul = length(up);
        const Vec3 dir = ul > 1.0f ? up * (1.0f / ul) : Vec3{0.0f, -1.0f, 0.0f};
        const Vec3 a = nk + up * 0.3f, top = hd + dir * (0.09f * kU);
        out->sphere[0] = a.x, out->sphere[1] = a.y, out->sphere[2] = a.z;
        out->sphere[3] = 0.14f * kU;
        out->cap_b[0] = top.x, out->cap_b[1] = top.y, out->cap_b[2] = top.z, out->cap_b[3] = 1.0f;
        fade = has_trunk ? st.trunk.faded() : 0.0f;
        // A draw of her head alone (her face, her hair): never seen from her own eyes, whole (her hair hangs past the
        // sphere, run vr21 looking down 70 degrees).
        if (classes == (1u << kHeadC)) fade = 1.0f;
    } else if (has_leg && !has_arm) {
        fade = has_trunk ? st.trunk.faded() : 0.0f;
        if (classes & (1u << kLegLeftC)) fade = std::max(fade, st.leg[kLeft].faded());
        if (classes & (1u << kLegRightC)) fade = std::max(fade, st.leg[kRight].faded());
    } else if (has_arm) {
        fade = has_trunk ? st.trunk.faded() : 0.0f;
        if (opt.hide_arms_near) near_from = 0.12f, near_to = 0.30f;
    } else fade = st.trunk.faded();
    // Per point (Jak's skin-weighted fade, the owner: "fades away smoothly"): while any part fades, every draw of her
    // takes each point's fade from her bone lines nearest it, so her hips' draw (trunk and both legs) loses only the
    // raised leg, her shoulders' draw (trunk and arm) keeps the arm while her trunk goes, each up to its joint, blended
    // over about 3 cm. Her arms and hands never fade this way (Jak's: nor his arms unless that option is on).
    // Each leg keeps its own fade (looking down past her faded trunk shows her legs and shoes, as Jak's does); the
    // legs go with the trunk only while she is knocked down or grabbed (PC rig run vr38, build 37: the legs took the
    // trunk's fade, so looking down lost her shoes and left her skirt's dithered edge floating).
    const float tf = st.trunk.faded(), of = st.over.faded();
    const float lf[2] = {std::max(of, st.leg[kLeft].faded()), std::max(of, st.leg[kRight].faded())};
    if (opt.body != 1 && classes != (1u << kHeadC) && (tf > 0.002f || lf[0] > 0.002f || lf[1] > 0.002f)) {
        // Each line's fade at its start and its end: a limb next to a faded trunk fades out from the joint (her skirt at
        // her hips, her vest's armholes at her shoulders), so no edge of her top, vest or skirt is left standing (run
        // vr50: one fade a line left such shards).
        auto line = [&](int a, int b, float fa, float fb) {
            if (a < 0 || b < 0 || out->n_seg >= 12) return;
            float* A = out->seg_a[out->n_seg];
            float* B = out->seg_b[out->n_seg];
            A[0] = st.drawn[a].x, A[1] = st.drawn[a].y, A[2] = st.drawn[a].z, A[3] = fa;
            B[0] = st.drawn[b].x, B[1] = st.drawn[b].y, B[2] = st.drawn[b].z, B[3] = fb;
            out->n_seg++;
        };
        line(kPelvis, kChest, tf, tf);
        line(kChest, kNeck, tf, tf);
        for (int h = 0; h < 2; h++) {
            line(sk.collar[h], sk.shoulder[h], tf, tf);
            line(sk.shoulder[h], sk.elbow[h], tf, 0.0f);
            line(sk.elbow[h], sk.hand[h], 0.0f, 0.0f);
            line(sk.hip[h], sk.knee[h], std::max(tf, lf[h]), lf[h]);
            line(sk.knee[h], sk.ankle[h], lf[h], lf[h]);
        }
        fade = 0.0f;
        static int told = 0, told_frame = -1;
        if (told < 3 && rt_frame != told_frame) {
            told++;
            told_frame = rt_frame;
            rt_log("VR body: frame %d: her parts fade per point along %d bone lines (trunk %.2f, legs %.2f %.2f; draw "
                   "parts %02x)", rt_frame, out->n_seg, tf, lf[0], lf[1], classes);
        }
    }
    if (opt.body == 1) fade = 1.0f;  // "Hidden (hands only)"
    if (ik_off(2048)) out->sphere[3] = 0.0f, out->cap_b[3] = 0.0f;  // testing (SH3_IK_OFF): 2048 no head cut
    if (ik_off(4096)) out->near_cut = 0.0f;                          // ... 4096 no near cut
    out->fade[0] = fade;
    if (opt.body_fade) out->fade[1] = near_from * kU, out->fade[2] = near_to * kU;
    {
        static int dump_at2 = -2, logged2 = 0;  // SH3_VSC_DUMP=N: what each of her draws is given in frame N
        if (dump_at2 == -2) dump_at2 = getenv("SH3_VSC_DUMP") ? atoi(getenv("SH3_VSC_DUMP")) : -1;
        if (rt_frame == dump_at2 && logged2++ < 64)
            rt_log("VR body: her draw (parts %02x): fade %.2f, near %.0f-%.0f, cut %.0f, sphere %.0f at %.0f %.0f %.0f, lines %d", classes,
                   out->fade[0], out->fade[1], out->fade[2], out->near_cut, out->sphere[3], out->sphere[0], out->sphere[1],
                   out->sphere[2], out->n_seg);
    }
    return 1;
}

void ik_crash_info(FILE* f) {
    std::fprintf(f, "VR body: model %s, fingers %s (rest pose: %s), lean %.1f deg, crouch %.1f cm, hung %.1f cm, bend %.1f deg, "
                    "walk %.2f, turned over %d, fades trunk %.2f legs %.2f %.2f, ghost hands %d, last posed frame %d\n",
                 sk.state == 1 ? "ready" : sk.state == 0 ? "not usable" : "not seen", sk.fingers ? "read" : "not read",
                 sk.rest_kind, quat_angle(st.lean) * 180.0f / kPi, st.drop / kU * 100.0f, length(st.hang) / kU * 100.0f,
                 quat_angle(st.bend) * 180.0f / kPi, st.walk, st.turned_over,
                 st.trunk.faded(), st.leg[0].faded(), st.leg[1].faded(), st.ghost_bits, st.bones_frame);
}

}  // namespace sh3q
