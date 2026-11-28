// SPDX-License-Identifier: GPL-3.0-or-later
// SH3 Quest VR: the hands against the world (LIST 19-21, 23, 41, 44), the guide dot and the comfort shading, at Present.
//
// The hands are Heather's own (LIST 41): the game draws her finger-skinned hands and her arms, posed by sh3vr_ik.cpp
// from the controllers (OpenXR's grip pose: -Z through the fist from the little finger to the thumb, the palm out of
// -X on the right hand and +X on the left). This file decides what the world lets them do: every finger posed from the
// controllers' touch sensors (the index finger from the trigger, the other three from the grip, the thumb from its rest
// and buttons; a held weapon closes them round its handle), stopped where the world is.
//
// The world against them, Half-Life: Alyx's way where the game lets us: the depth the game drew this frame is sampled
// at points of the hands (probes: a frame late, through a pixel buffer, no stall), the fingertips from her own fingers'
// shapes (sh3vr_ik.cpp reads them from her model). A finger closing stops where its tip would go into a surface (it
// wraps what the hand holds or touches), and a hand pushed through a wall stays at the wall while a faint ghost of her
// hand (drawn by the game, sh3.c) shows where the real one is. Session 8 (the owner: "similar to half life alyx/our
// other ports"): the surface round each hand is fitted with planes (an edge or a corner as two), and as Jak's and
// Ratchet's hands the palm rests on them and each finger lies on them, bends back against them or curls on to them,
// none through. What the depth shows of Heather herself (her arms and
// hands as drawn this frame, her body) is not a surface: a probe behind her own hand is free. The same probes find where
// the gun's ray meets the world (the guide dot). The game's collision is not callable for an arbitrary point, so this is
// the world as the eyes see it: what is hidden from the left eye cannot stop a hand (docs/HEADSET_CHECKLIST.md).
//
// Comfort: the vignette while the stick moves Heather or turns smoothly, and the view darkened while the head is in a
// wall Heather could not follow it through (she walks after the head with the game's own collision: sh3vr.cpp).

#include "sh3vr_internal.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <chrono>
#include <cstring>

#if !defined(__ANDROID__)
#include <atomic>
namespace quest {
extern std::atomic<int> sim_shot_hands;  // the rig's pictures: set with each shot (xr_sim.cpp), both hands logged
}
#endif

namespace sh3q {

HandState hands[2];

namespace {

// 0.1.10 (owner 172): her palm's skin as drawn, in each hand's grip space (body_palm_skin, below)
std::vector<Vec3> palm_skin[2];

// ---------------------------------------------------------------- the hand's shape (metres, the hand's grip space)

// Used only until her own fingers are read from her model (her_hands): an average hand, the right one.
struct FingerShape {
    Vec3 knuckle;
    float len[3];
};
// Index (at the thumb's end of the fist) to pinky along Z; the knuckles ahead of the palm (-Y), the palm's face on -X.
const FingerShape kFingers[5] = {
    {{0.022f, -0.012f, -0.036f}, {0.040f, 0.031f, 0.026f}},  // thumb
    {{0.030f, -0.046f, -0.026f}, {0.044f, 0.026f, 0.021f}},  // index
    {{0.031f, -0.048f, -0.008f}, {0.048f, 0.029f, 0.022f}},  // middle
    {{0.030f, -0.046f, 0.010f}, {0.044f, 0.027f, 0.021f}},   // ring
    {{0.028f, -0.041f, 0.026f}, {0.034f, 0.021f, 0.019f}},   // pinky
};
const float kJoint[3] = {80.0f, 95.0f, 65.0f};       // degrees at a full fist
const float kThumbJoint[3] = {25.0f, 40.0f, 35.0f};

Vec3 rot_axis(Vec3 v, Vec3 axis, float angle) {
    const Quat q{axis.x * std::sin(angle * 0.5f), axis.y * std::sin(angle * 0.5f), axis.z * std::sin(angle * 0.5f),
                 std::cos(angle * 0.5f)};
    return rotate(q, v);
}

// The joints of hand h's finger `f` (0 thumb) at curl `c`, in that hand's own grip space: 4 points from the knuckle to
// the tip. Her own fingers (their rest shape and bend axes, as sh3vr_ik.cpp poses them) once read; else the average
// right hand, mirrored for the left. A curl below 0 bends the finger back (kFingerBack, as pose_fingers does).
// Session 31 (vr381: the shotgun's and submachine gun's triggers sit higher than her straight index reaches with her other
// fingers round the grip): `spread` (degrees) swings the whole finger sideways at its knuckle, about the axis across its
// bend there (cross(bend axis, first segment): a finger spread from or toward its neighbour), as pose_fingers draws it
// (index_spread; the thumb's swing at its root, thumb_spread, alike).
// Session 36 (owner message 169: "shape the fucking hand around them with no clipping"): each joint at its own curl,
// c3[0] the knuckle's, c3[1] the middle joint's, c3[2] the last joint's (finger_wrap; pose_fingers draws them alike);
// finger_points(c) is every joint at c.
void finger_points3(int h, int f, const float c3[3], Vec3 out[4], float spread = 0.0f) {
    const HerHand& hh = her_hands[h];
    const float sr = spread * kPi / 180.0f;
    if (hh.valid) {
        const FingerGeo& g = hh.f[f];
        const Vec3 n = sr != 0.0f ? normalized(cross(g.axis, g.seg[0])) : Vec3{};
        out[0] = g.knuckle;
        float angle = 0;
        for (int j = 0; j < 3; j++) {
            const float c = c3[j];
            angle += c * g.joint[j] * (c < 0 ? kFingerBack[f == kThumb ? 0 : 1][j] : 1.0f);
            const Vec3 d = rot_axis(g.seg[j], g.axis, angle);
            out[j + 1] = out[j] + (sr != 0.0f ? rot_axis(d, n, sr) : d);
        }
        return;
    }
    const FingerShape& s = kFingers[f];
    const float mx = h == kLeft ? -1.0f : 1.0f;
    Vec3 dir, axis;
    const float* joint;
    if (f == kThumb) {
        dir = normalized(Vec3{-0.35f, -0.55f, -0.75f});
        axis = {0, 1, 0};
        joint = kThumbJoint;
    } else {
        dir = {0, -1, 0};
        axis = {0, 0, -1};
        joint = kJoint;
    }
    Vec3 p = s.knuckle;
    out[0] = {p.x * mx, p.y, p.z};
    const Vec3 n = sr != 0.0f ? normalized(cross(axis, dir)) : Vec3{};
    float angle = 0;
    for (int j = 0; j < 3; j++) {
        const float c = c3[j];
        angle += c * joint[j] * (c < 0 ? kFingerBack[f == kThumb ? 0 : 1][j] : 1.0f) * kPi / 180.0f;
        const Vec3 d = rot_axis(dir, axis, angle);
        p = p + (sr != 0.0f ? rot_axis(d, n, sr) : d) * s.len[j];
        out[j + 1] = {p.x * mx, p.y, p.z};
    }
}
void finger_points(int h, int f, float c, Vec3 out[4], float spread = 0.0f) {
    const float c3[3] = {c, c, c};
    finger_points3(h, f, c3, out, spread);
}

Vec3 grip_point(const Pose& pose, Vec3 l) { return pose.position + rotate(pose.orientation, l); }

// 0.1.10 (owner 172: "WHY ... IS THE GUN STILL CLIPPING THROUGH THE HAND"; owner 169: "shape the fucking hand around
// them with no clipping"): her hand as drawn, in its grip space, posed at any curl. body_hand_skin takes it once from
// her own vertices as skinned (sh3vr_combat.cpp hand_mesh); each vertex keeps each of its bones' places for it: her
// hand's own bone (or another, her forearm's) in the grip space, a finger's segment in that segment's own frame
// (finger_frames), so at any curl and spread it lies where the game's skinning puts it (her bones as drawn lie on this
// model's: build 278). Build 283's slices (the handgun round her hand at 0.1.8's place, vr507) showed why the gun went
// through her hand: her palm as drawn is a coarse shell 4.5 cm thick, the grip's back half lay inside it, and only a few
// of its vertices fell inside the grip (palm_points and the vertices alone never saw it). So every triangle of it counts
// too (hand_skin_clear). The grips' fit keeps all of it out of the weapon (body_grip_mesh), and her fingers close onto
// the weapon until their own skin meets it (finger_touches, joint_gaps, thumb_rest), not only the model's 8.5 mm round
// their bones.
struct SkinVert {
    int n = 0;
    int8_t tag[4]{};  // the finger's segment (finger * 3 + segment), -1 her hand's own bone (or another)
    float w[4]{};
    Vec3 l[4];        // where that bone alone puts it: the grip space (her hand's own), else the segment's own frame
};
struct HandSkin {
    bool ready = false;
    std::vector<SkinVert> v;
    std::vector<uint8_t> part;    // 0 her palm (90% or more on her hand's own bone), 1 blended, 2 + f finger f's,
                                  // kSkinWrist her wrist (on her forearm's bones too)
    std::vector<int> tri;         // 3 a triangle (indices into v)
    std::vector<Vec3> seg[5][3];  // each finger segment's own vertices (90% or more on it), in its own frame
    float reach[5][3]{};          // ... the farthest of them from the segment's line (metres)
};
HandSkin hand_skin[2];
int hand_skin_taken[2] = {0, 0};  // session 38: times each was taken (body_hand_skin_points' key)
constexpr float kSkinMargin = 0.0005f;  // her fingers close until their skin comes this near the weapon (metres)
constexpr float kSkinCheck = 0.0002f;   // a place fitted keeps all her skin at least this far out of it (body_grip_mesh)
constexpr uint8_t kSkinWrist = 7;

// Finger f's segments' frames at the joints' curls c3 and its spread (degrees), as finger_points3 lays them: each
// segment's first joint (o) and its turn from the finger's rest (q: its segment at rest turned by it is the segment
// drawn). False until her own fingers are read.
bool finger_frames(int h, int f, const float c3[3], float spread, Vec3 o[3], Quat q[3]) {
    if (h < 0 || h > 1 || f < 0 || f > 4) return false;
    const HerHand& hh = her_hands[h];
    if (!hh.valid) return false;
    const FingerGeo& g = hh.f[f];
    const float sr = spread * kPi / 180.0f;
    Quat qs{0, 0, 0, 1};
    if (sr != 0.0f) {
        const Vec3 n = normalized(cross(g.axis, g.seg[0]));
        const float s = std::sin(sr * 0.5f);
        qs = Quat{n.x * s, n.y * s, n.z * s, std::cos(sr * 0.5f)};
    }
    Vec3 p = g.knuckle;
    float angle = 0;
    for (int j = 0; j < 3; j++) {
        const float c = c3[j];
        angle += c * g.joint[j] * (c < 0 ? kFingerBack[f == kThumb ? 0 : 1][j] : 1.0f);
        const float s = std::sin(angle * 0.5f);
        o[j] = p;
        q[j] = qs * Quat{g.axis.x * s, g.axis.y * s, g.axis.z * s, std::cos(angle * 0.5f)};
        p = p + rotate(q[j], g.seg[j]);
    }
    return true;
}

// The nearest of finger f's segment j's own skin to the weapon, hand h at `pose` with the finger's frames o, q
// (metres; below 0 in it; 1e9 none of its own).
float skin_segment_gap(int h, int f, int j, const Vec3 o[3], const Quat q[3], const Pose& pose) {
    float m = 1e9f;
    for (const Vec3& l : hand_skin[h].seg[f][j]) {
        const Vec3 t = grip_point(pose, o[j] + rotate(q[j], l));
        m = std::min(m, weapon_clearance(h, t, t));
    }
    return m;
}

// Her hand h as drawn (hand_skin) posed with each finger's joints at c3 and the spreads sp (degrees, each finger's: the
// thumb's swing, the index's spread): every vertex, grip space; false before it is taken.
bool hand_skin_pose(int h, const float c3[5][3], const float sp[5], std::vector<Vec3>& out) {
    if (h < 0 || h > 1 || !hand_skin[h].ready) return false;
    Vec3 o[5][3];
    Quat q[5][3];
    for (int f = 0; f < 5; f++)
        if (!finger_frames(h, f, c3[f], sp[f], o[f], q[f])) return false;
    const HandSkin& hs = hand_skin[h];
    out.resize(hs.v.size());
    for (size_t i = 0; i < hs.v.size(); i++) {
        const SkinVert& sv = hs.v[i];
        Vec3 p{};
        for (int k = 0; k < sv.n; k++) {
            const int t = sv.tag[k];
            p = p + (t < 0 ? sv.l[k] : o[t / 3][t % 3] + rotate(q[t / 3][t % 3], sv.l[k])) * sv.w[k];
        }
        out[i] = p;  // (as the game skins it: each bone's place by its weight)
    }
    return true;
}

// Her hand h as drawn at `pose`, posed with c3 and sp (hand_skin_pose), against the weapon: its nearest (metres; below 0
// in it): every vertex, and each triangle that could come within 6 mm of it (its nearest corner's clearance less its
// longest side) at its own points no more than 2 mm apart. `part`: the nearest's (0 her palm, 1 blended, 2 + f finger
// f); `palm`: the nearest of her palm's and the blended skin's. Her wrist's skin on her forearm's bones (kSkinWrist:
// where her arm bends it, which this model does not know) is left out.
float hand_skin_clear(int h, const Pose& pose, const float c3[5][3], const float sp[5], int* part, float* palm) {
    if (part) *part = -1;
    if (palm) *palm = 1e9f;
    static std::vector<Vec3> P;
    static std::vector<float> C;
    if (!hand_skin_pose(h, c3, sp, P)) return 1e9f;
    const HandSkin& hs = hand_skin[h];
    C.resize(P.size());
    float best = 1e9f;
    auto note = [&](float c, int pt) {
        if (c < best) {
            best = c;
            if (part) *part = pt;
        }
        if (palm && pt <= 1 && c < *palm) *palm = c;
    };
    for (size_t i = 0; i < P.size(); i++) {
        P[i] = grip_point(pose, P[i]);
        C[i] = weapon_clearance(h, P[i], P[i]);
        if (hs.part[i] != kSkinWrist) note(C[i], hs.part[i]);
    }
    for (size_t t = 0; t + 2 < hs.tri.size(); t += 3) {
        const int a = hs.tri[t], b = hs.tri[t + 1], d = hs.tri[t + 2];
        const uint8_t pa = hs.part[a], pb = hs.part[b], pd = hs.part[d];
        if (pa == kSkinWrist || pb == kSkinWrist || pd == kSkinWrist) continue;
        const float L = std::max(length(P[b] - P[a]), std::max(length(P[d] - P[b]), length(P[a] - P[d])));
        if (std::min(C[a], std::min(C[b], C[d])) - L > 0.006f) continue;
        const int pt = pa == pb && pb == pd ? pa : std::min(pa, std::min(pb, pd)) <= 1 ? 1 : pa;
        const int n = std::clamp((int)std::ceil(L / 0.002f), 1, 16);
        for (int i = 0; i <= n; i++)
            for (int j = 0; i + j <= n; j++) {
                if ((i == 0 && j == 0) || i == n || j == n) continue;  // (its corners, taken above)
                const Vec3 x = P[a] + (P[b] - P[a]) * ((float)i / n) + (P[d] - P[a]) * ((float)j / n);
                note(weapon_clearance(h, x, x), pt);
            }
    }
    return best;
}

// Session 38 (the rig's pictures): her trigger finger as drawn while it pulls. The weapon's shape is the gun at rest, its
// trigger where it is before the pull, so hand_skin_clear reads the finger following the trigger back as in it (vr517:
// 0.4 to 1.6 mm). Here the index's skin (its vertices and its triangles' points no more than 2 mm apart) against the
// trigger as drawn, turned by `pull` (its blade as weapon_trigger_at gives it), and against the rest of the gun (the
// shape's reading, taken only where the trigger at rest is not what is nearest). Metres, below 0 in it; 1e9 none.
float seg_dist(Vec3 p1, Vec3 q1, Vec3 p2, Vec3 q2);  // (below)
static void hand_skin_trigger(int h, const Pose& pose, const float c3[5][3], const float sp[5], float pull,
                              float& on_trigger, float& on_gun) {
    on_trigger = on_gun = 1e9f;
    static std::vector<Vec3> P;
    if (h < 0 || h > 1 || !hand_skin_pose(h, c3, sp, P)) return;
    const GripTrigger tn = weapon_trigger_at(pull), t0 = weapon_trigger_at(0.0f);
    if (!tn.on) return;
    const HandSkin& hs = hand_skin[h];
    constexpr uint8_t kIndexPart = 2 + kIndex;
    auto take = [&](Vec3 x) {
        on_trigger = std::min(on_trigger, seg_dist(x, x, tn.a, tn.b) - tn.r);
        const float c = weapon_clearance(h, x, x);
        if (seg_dist(x, x, t0.a, t0.b) - t0.r > c + 0.0003f) on_gun = std::min(on_gun, c);
    };
    for (Vec3& p : P) p = grip_point(pose, p);
    for (size_t i = 0; i < P.size(); i++)
        if (hs.part[i] == kIndexPart) take(P[i]);
    for (size_t t = 0; t + 2 < hs.tri.size(); t += 3) {
        const int a = hs.tri[t], b = hs.tri[t + 1], d = hs.tri[t + 2];
        if (hs.part[a] == kSkinWrist || hs.part[b] == kSkinWrist || hs.part[d] == kSkinWrist) continue;
        if (hs.part[a] != kIndexPart && hs.part[b] != kIndexPart && hs.part[d] != kIndexPart) continue;
        const float L = std::max(length(P[b] - P[a]), std::max(length(P[d] - P[b]), length(P[a] - P[d])));
        const int n = std::clamp((int)std::ceil(L / 0.002f), 1, 16);
        for (int i = 0; i <= n; i++)
            for (int j = 0; i + j <= n; j++) {
                if ((i == 0 && j == 0) || i == n || j == n) continue;  // (its corners)
                take(P[a] + (P[b] - P[a]) * ((float)i / n) + (P[d] - P[a]) * ((float)j / n));
            }
    }
}

// 0.1.10 (session 38; open since session 37): the other hand h at `pose`, posed with c3 and sp, against her main hand
// as drawn where it is now (its fingers' curls and spreads as drawn, body_hand_capsules'): the nearest between their
// skins (metres; below 0 one in the other), every vertex of each against the other's triangles, the side told by the
// other's skin's normals at the nearest point (its corners' normals blended); 1e9 when either is not taken. Until now the
// other hand was kept off the main hand's model alone (body_hand_capsules: its palm 2.6 cm thick, hers as drawn 5.2).
// Her wrists' skin on her forearms' bones (kSkinWrist) is left out, as in hand_skin_clear.
static float hand_skin_between(int h, const Pose& pose, const float c3[5][3], const float sp[5]) {
    const int m = 1 - h;
    if (h < 0 || h > 1 || !hand_skin[h].ready || !hand_skin[m].ready || !hands[m].valid) return 1e9f;
    static std::vector<Vec3> P[2], N[2];
    const HandState& s = hands[m];
    float cm[5][3], spm[5] = {thumb_spread[m], index_spread[m], 0.0f, 0.0f, 0.0f};
    for (int f = 0; f < 5; f++)
        for (int j = 0; j < 3; j++) cm[f][j] = std::clamp(s.curl[f] + finger_joint_off[m][f][j], kFingerBackMost, 1.0f);
    if (!hand_skin_pose(h, c3, sp, P[0]) || !hand_skin_pose(m, cm, spm, P[1])) return 1e9f;
    for (Vec3& p : P[0]) p = grip_point(pose, p);
    for (Vec3& p : P[1]) p = grip_point(s.shown, p);
    const HandSkin* hs[2] = {&hand_skin[h], &hand_skin[m]};
    Vec3 lo[2], hi[2];
    for (int k = 0; k < 2; k++) {
        // (each skin's normals: its triangles' summed at their corners, turned outward by the most of them facing away
        // from its middle)
        N[k].assign(P[k].size(), Vec3{});
        Vec3 mid{};
        lo[k] = {1e9f, 1e9f, 1e9f}, hi[k] = {-1e9f, -1e9f, -1e9f};
        for (const Vec3& p : P[k]) {
            mid = mid + p;
            lo[k] = {std::min(lo[k].x, p.x), std::min(lo[k].y, p.y), std::min(lo[k].z, p.z)};
            hi[k] = {std::max(hi[k].x, p.x), std::max(hi[k].y, p.y), std::max(hi[k].z, p.z)};
        }
        mid = mid * (1.0f / (float)std::max<size_t>(P[k].size(), 1));
        const std::vector<int>& tr = hs[k]->tri;
        for (size_t t = 0; t + 2 < tr.size(); t += 3) {
            const int a = tr[t], b = tr[t + 1], d = tr[t + 2];
            const Vec3 n = cross(P[k][b] - P[k][a], P[k][d] - P[k][a]);
            N[k][a] = N[k][a] + n, N[k][b] = N[k][b] + n, N[k][d] = N[k][d] + n;
        }
        float vote = 0;
        for (size_t i = 0; i < P[k].size(); i++) vote += dot(N[k][i], P[k][i] - mid) > 0 ? 1.0f : -1.0f;
        const float flip = vote < 0 ? -1.0f : 1.0f;
        for (Vec3& n : N[k]) n = normalized(n) * flip;
    }
    float best = 1e9f;
    for (int k = 0; k < 2; k++) {  // (k's vertices against the other's triangles)
        const int o = 1 - k;
        const std::vector<int>& tr = hs[o]->tri;
        for (size_t i = 0; i < P[k].size(); i++) {
            if (hs[k]->part[i] == kSkinWrist) continue;
            const Vec3 x = P[k][i];
            if (x.x < lo[o].x - 0.01f || x.y < lo[o].y - 0.01f || x.z < lo[o].z - 0.01f || x.x > hi[o].x + 0.01f ||
                x.y > hi[o].y + 0.01f || x.z > hi[o].z + 0.01f)
                continue;
            float d2 = 1e9f;
            Vec3 q{}, n{};
            for (size_t t = 0; t + 2 < tr.size(); t += 3) {
                const int a = tr[t], b = tr[t + 1], c = tr[t + 2];
                if (hs[o]->part[a] == kSkinWrist || hs[o]->part[b] == kSkinWrist || hs[o]->part[c] == kSkinWrist) continue;
                // (the nearest point of triangle abc to x, by its barycentric weights: Ericson's closest point)
                const Vec3 A = P[o][a], B = P[o][b], C = P[o][c];
                const Vec3 ab = B - A, ac = C - A, ap = x - A;
                float u, v, w;  // weights of A, B, C
                const float d1 = dot(ab, ap), e1 = dot(ac, ap);
                const Vec3 bp = x - B;
                const float d3 = dot(ab, bp), e3 = dot(ac, bp);
                const Vec3 cp = x - C;
                const float d5 = dot(ab, cp), e5 = dot(ac, cp);
                const float vc = d1 * e3 - d3 * e1, vb = d5 * e1 - d1 * e5, va = d3 * e5 - d5 * e3;
                if (d1 <= 0 && e1 <= 0) u = 1, v = 0, w = 0;
                else if (d3 >= 0 && e3 <= d3) u = 0, v = 1, w = 0;
                else if (e5 >= 0 && d5 <= e5) u = 0, v = 0, w = 1;
                else if (vc <= 0 && d1 >= 0 && d3 <= 0) v = d1 / (d1 - d3), u = 1 - v, w = 0;
                else if (vb <= 0 && e1 >= 0 && e5 <= 0) w = e1 / (e1 - e5), u = 1 - w, v = 0;
                else if (va <= 0 && (e3 - d3) >= 0 && (d5 - e5) >= 0)
                    w = (e3 - d3) / ((e3 - d3) + (d5 - e5)), v = 1 - w, u = 0;
                else {
                    const float den = 1.0f / (va + vb + vc);
                    v = vb * den, w = vc * den, u = 1 - v - w;
                }
                const Vec3 y = A * u + B * v + C * w;
                const Vec3 dy = x - y;
                const float dd = dot(dy, dy);
                if (dd < d2) d2 = dd, q = y, n = N[o][a] * u + N[o][b] * v + N[o][c] * w;
            }
            if (d2 >= 1e9f) continue;
            const float d = std::sqrt(d2);
            best = std::min(best, dot(x - q, n) < 0 ? -d : d);
        }
    }
    return best;
}

// ---------------------------------------------------------------- the probes

enum ProbeKind : uint8_t { kArm = 0, kTip = 1, kRay = 2, kWeapon = 3, kSurf = 4, kView = 5 };
constexpr int kProbeMax = 160;  // session 8: 160 (the surface round each hand); the probe shader's uUV matches
constexpr int kArmSamples = 8;
constexpr int kTipSteps = 6;      // curl 0, 0.2 ... 1 (session 7: six, the stops between them from the depths)
constexpr int kRaySamples = 24;
constexpr int kWeaponSamples = 12;  // the weapon's line, the palm to its tip (the owner: "guns/melee weapons should also
                                    // have world collision")
constexpr int kCapsuleMax = 64;
constexpr int kSurfRing = 14;     // session 8: the surface round each hand (two rings, its palm's middle, under it)
constexpr int kSurfMost = kSurfRing + 5 * kTipSteps;  // the points a hand's planes are fitted to (its fingertips' too)
// Session 10 (vr160: the opening's bridge walk in her eyes had two fence posts 14 cm before them): straight out of the
// eyes during a scene in first person, eleven ways (every 8 degrees from 32 left to 32 right, 10 up and down from the
// middle; vr163: five ways 12 degrees apart missed the posts, 4 and 10 degrees wide), each at 12 and 25 cm
constexpr int kViewWays = 11, kViewSamples = kViewWays * 2;
constexpr float kViewNear = 0.12f, kViewFar = 0.25f;

struct Probe {
    Vec3 world;
    float own;                    // the point's distance along the eye's view (game units)
    uint8_t kind, hand, a, b;
    bool on_screen;
};
struct ProbeSet {
    Probe p[kProbeMax];
    int n = 0;
    Capsule caps[kCapsuleMax];    // Heather's arms and hands as drawn the frame the question was asked (world)
    int ncaps = 0;
    Vec3 eye;                     // world, that frame
    GLuint pbo = 0;
    GLsync fence = nullptr;
    bool pending = false;
    bool stencil = false;         // 0.1.7: the stencil pass ran for this set (each answer's alpha her stencil)
};
ProbeSet sets[2];
int next_set = 0;

GLuint probe_prog = 0, probe_tex = 0, probe_fbo = 0, probe_sampler = 0;
GLint probe_uv_loc = -1, probe_depth_loc = -1;
// 0.1.7: the stencil pass (her pixels); off when its program or the texture's stencil mode is refused (then the
// capsules and her column tell her apart, as before).
GLuint probe_st_prog = 0;
GLint probe_st_uv_loc = -1, probe_st_loc = -1;
int stencil_reads = -1;  // -1 not tried yet, 0 refused, 1 working

// What the probes said (the last answer read).
struct {
    float arm_pen[2][kArmSamples];
    bool arm_ok[2][kArmSamples];
    bool arm_hidden[2][kArmSamples];  // behind her own drawn arm or hand: whether the world is there is not known
    Vec3 arm_world[2][kArmSamples];
    float tip_pen[2][5][kTipSteps];
    bool tip_ok[2][5][kTipSteps];
    float ray_pen[kRaySamples];
    bool ray_ok[kRaySamples];
    Vec3 ray_world[kRaySamples];
    int ray_n = 0;
    float weapon_pen[kWeaponSamples];
    bool weapon_ok[kWeaponSamples];
    bool weapon_hidden[kWeaponSamples];  // behind the drawn weapon or her hand: not known
    bool weapon_asked = false;
    float view_pen[kViewSamples];
    bool view_ok[kViewSamples];
    bool view_asked = false;
    Vec3 surf[2][kSurfMost];      // the surface the depth shows round each hand (world, not hers): its planes' points
    int surf_n[2] = {};
    // Session 9: where a probe round a hand saw past it to a surface far beyond (over 12 cm: the floor past a seat's
    // edge) or to nothing at all (the world's end: a point far out on the eye's ray): what its plane does not reach
    // (fit_solids's implied front face of an edge seen from above).
    Vec3 beyond[2][kSurfMost];
    int beyond_n[2] = {};
    int own_hits = 0;            // probes whose surface was Heather herself (free)
    int own_stencil = 0;         // 0.1.7: of those, known by her pixels' stencil mark
    bool stencil = false;        // 0.1.7: this answer read her stencil mark
    bool weapon_her[kWeaponSamples];  // 0.1.7: the weapon's line sample's surface is hers (her body or the weapon)
    bool valid = false;
    int age = 99;                 // frames since it was read
    int serial = 0;               // answers read (a new one: the hands' planes fitted again)
} answer;

Vec3 guide_world;
bool guide_valid = false;
int guide_age = 99;

// ---------------------------------------------------------------- GL

GLuint comfort_prog = 0, vao = 0, draw_fbo_mv = 0;
GLint c_eye_center = -1, c_vignette = -1, c_fade = -1, c_size = -1;
bool gl_ready = false, gl_failed = false;

GLuint compile(GLenum type, const char* src, const char* what) {
    const GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, nullptr);
    glCompileShader(s);
    GLint ok = 0;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[1024];
        glGetShaderInfoLog(s, sizeof log, nullptr, log);
        rt_log("VR: %s shader did not compile: %s", what, log);
        glDeleteShader(s);
        return 0;
    }
    return s;
}

GLuint link(const char* vs, const char* fs, const char* what) {
    const GLuint v = compile(GL_VERTEX_SHADER, vs, what), f = compile(GL_FRAGMENT_SHADER, fs, what);
    if (!v || !f) return 0;
    const GLuint p = glCreateProgram();
    glAttachShader(p, v);
    glAttachShader(p, f);
    glLinkProgram(p);
    glDeleteShader(v);
    glDeleteShader(f);
    GLint ok = 0;
    glGetProgramiv(p, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[1024];
        glGetProgramInfoLog(p, sizeof log, nullptr, log);
        rt_log("VR: %s program did not link: %s", what, log);
        return 0;
    }
    return p;
}

const char* kComfortVs =
    "#version 320 es\n#extension GL_OVR_multiview2 : require\nlayout(num_views = 2) in;\nprecision highp float;\n"
    "flat out int vView;\n"
    "void main() {\n"
    "  vec2 p = vec2(float((gl_VertexID & 1) * 4 - 1), float((gl_VertexID >> 1) * 4 - 1));\n"
    "  gl_Position = vec4(p, 0.0, 1.0); vView = int(gl_ViewID_OVR);\n}\n";
const char* kComfortFs =
    "#version 320 es\nprecision highp float;\nuniform vec2 uCenter[2]; uniform vec2 uVignette; uniform float uFade;\n"
    "uniform vec2 uSize;\nflat in int vView;\nout vec4 o;\n"
    "void main() {\n"
    "  vec2 ndc = gl_FragCoord.xy / uSize * 2.0 - 1.0;\n"
    "  float r = length(ndc - uCenter[vView]);\n"
    "  float v = uVignette.y * smoothstep(uVignette.x, uVignette.x + 0.45, r);\n"
    "  o = vec4(0.0, 0.0, 0.0, max(v, uFade));\n}\n";
const char* kProbeVs =
    "#version 320 es\nprecision highp float;\n"
    "void main() {\n"
    "  vec2 p = vec2(float((gl_VertexID & 1) * 4 - 1), float((gl_VertexID >> 1) * 4 - 1));\n"
    "  gl_Position = vec4(p, 0.0, 1.0);\n}\n";
// The probes' places, two to a vec4 (one upload of 80 vec4s, kProbeMax / 2).
const char* kProbeFs =
    "#version 320 es\nprecision highp float;\nprecision highp sampler2DArray;\n"
    "uniform sampler2DArray uDepth; uniform vec4 uUV[80];\nout vec4 o;\n"
    "void main() {\n"
    "  int i = int(gl_FragCoord.x); vec4 p = uUV[i >> 1]; vec2 uv = (i & 1) == 0 ? p.xy : p.zw;\n"
    "  float d = texture(uDepth, vec3(uv, 0.0)).r;\n"
    "  float v = floor(clamp(d, 0.0, 1.0) * 16777215.0 + 0.5);\n"
    "  float r = floor(v / 65536.0); float g = floor((v - r * 65536.0) / 256.0); float b = v - r * 65536.0 - g * 256.0;\n"
    "  o = vec4(r, g, b, 0.0) / 255.0;\n}\n";
// 0.1.7 (owner 123: "weapons are acting like theyre colliding with things mid walk/in the air despite there being
// nothing ... a ding happens too"): the same places' stencil, into the alpha the depth pass left 0. Every draw of
// Heather's (her whole model: body, skirt, hair, feet, the arms and hands, the parts faded near the eyes, whose dithered
// pixels still write depth) and the weapon or part in her hand carries bit 0x20 (d3d8.c draw_setup, her_mark /
// shader_held_draw), so a probe's surface is known to be hers to the pixel; the capsules round her bones missed her
// skirt, her chest and shoulders, her feet, and the faded parts, and the probes took those for walls.
const char* kProbeStencilFs =
    "#version 320 es\nprecision highp float;\nprecision highp usampler2DArray;\n"
    "uniform usampler2DArray uStencil; uniform vec4 uUV[80];\nout vec4 o;\n"
    "void main() {\n"
    "  int i = int(gl_FragCoord.x); vec4 p = uUV[i >> 1]; vec2 uv = (i & 1) == 0 ? p.xy : p.zw;\n"
    "  uint s = texture(uStencil, vec3(uv, 0.0)).r;\n"
    "  o = vec4(0.0, 0.0, 0.0, float(s & 255u) / 255.0);\n}\n";

bool gl_init() {
    if (gl_ready || gl_failed) return gl_ready;
    comfort_prog = link(kComfortVs, kComfortFs, "VR comfort");
    probe_prog = link(kProbeVs, kProbeFs, "VR probes");
    if (!comfort_prog || !probe_prog) {
        gl_failed = true;
        rt_log("VR: the probes and the comfort shading are off this run (a shader failed; see above)");
        return false;
    }
    c_eye_center = glGetUniformLocation(comfort_prog, "uCenter");
    c_vignette = glGetUniformLocation(comfort_prog, "uVignette");
    c_fade = glGetUniformLocation(comfort_prog, "uFade");
    c_size = glGetUniformLocation(comfort_prog, "uSize");
    probe_uv_loc = glGetUniformLocation(probe_prog, "uUV");
    probe_depth_loc = glGetUniformLocation(probe_prog, "uDepth");
    probe_st_prog = link(kProbeVs, kProbeStencilFs, "VR probes' stencil");
    if (probe_st_prog) {
        probe_st_uv_loc = glGetUniformLocation(probe_st_prog, "uUV");
        probe_st_loc = glGetUniformLocation(probe_st_prog, "uStencil");
    } else {
        stencil_reads = 0;
        rt_log("VR: the probes' stencil pass is off (its shader failed; see above): her body told by her bones' capsules");
    }
    glGenVertexArrays(1, &vao);
    glGenFramebuffers(1, &draw_fbo_mv);
    glGenTextures(1, &probe_tex);
    glBindTexture(GL_TEXTURE_2D, probe_tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, kProbeMax, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glGenFramebuffers(1, &probe_fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, probe_fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, probe_tex, 0);
    glGenSamplers(1, &probe_sampler);
    glSamplerParameteri(probe_sampler, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glSamplerParameteri(probe_sampler, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glSamplerParameteri(probe_sampler, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glSamplerParameteri(probe_sampler, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glSamplerParameteri(probe_sampler, GL_TEXTURE_COMPARE_MODE, GL_NONE);
    for (ProbeSet& s : sets) {
        glGenBuffers(1, &s.pbo);
        glBindBuffer(GL_PIXEL_PACK_BUFFER, s.pbo);
        glBufferData(GL_PIXEL_PACK_BUFFER, kProbeMax * 4, nullptr, GL_STREAM_READ);
    }
    glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
    gl_ready = true;
    rt_log("VR: probes and comfort shading ready");
    return true;
}

// The GL state VR's drawing touches, put back as the game's renderer left it.
struct GlSave {
    GLint prog, vao_b, abuf, pack, active_tex, tex2da, tex2d, sampler0, rfbo, dfbo, vp[4], blend, depth, cull, scissor,
        dmask, dfunc, cmask[4], bsrc_rgb, bdst_rgb, bsrc_a, bdst_a, beq;
    GlSave() {
        glGetIntegerv(GL_CURRENT_PROGRAM, &prog);
        glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &vao_b);
        glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &abuf);
        glGetIntegerv(GL_PIXEL_PACK_BUFFER_BINDING, &pack);
        glGetIntegerv(GL_ACTIVE_TEXTURE, &active_tex);
        glActiveTexture(GL_TEXTURE0);
        glGetIntegerv(GL_TEXTURE_BINDING_2D_ARRAY, &tex2da);
        glGetIntegerv(GL_TEXTURE_BINDING_2D, &tex2d);
        glGetIntegerv(GL_SAMPLER_BINDING, &sampler0);
        glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &rfbo);
        glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &dfbo);
        glGetIntegerv(GL_VIEWPORT, vp);
        glGetIntegerv(GL_BLEND, &blend);
        glGetIntegerv(GL_DEPTH_TEST, &depth);
        glGetIntegerv(GL_CULL_FACE, &cull);
        glGetIntegerv(GL_SCISSOR_TEST, &scissor);
        glGetIntegerv(GL_DEPTH_WRITEMASK, &dmask);
        glGetIntegerv(GL_DEPTH_FUNC, &dfunc);
        glGetIntegerv(GL_COLOR_WRITEMASK, cmask);
        glGetIntegerv(GL_BLEND_SRC_RGB, &bsrc_rgb);
        glGetIntegerv(GL_BLEND_DST_RGB, &bdst_rgb);
        glGetIntegerv(GL_BLEND_SRC_ALPHA, &bsrc_a);
        glGetIntegerv(GL_BLEND_DST_ALPHA, &bdst_a);
        glGetIntegerv(GL_BLEND_EQUATION_RGB, &beq);
    }
    ~GlSave() {
        glUseProgram((GLuint)prog);
        glBindVertexArray((GLuint)vao_b);
        glBindBuffer(GL_ARRAY_BUFFER, (GLuint)abuf);
        glBindBuffer(GL_PIXEL_PACK_BUFFER, (GLuint)pack);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D_ARRAY, (GLuint)tex2da);
        glBindTexture(GL_TEXTURE_2D, (GLuint)tex2d);
        glBindSampler(0, (GLuint)sampler0);
        glActiveTexture((GLenum)active_tex);
        glBindFramebuffer(GL_READ_FRAMEBUFFER, (GLuint)rfbo);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, (GLuint)dfbo);
        glViewport(vp[0], vp[1], vp[2], vp[3]);
        if (blend) glEnable(GL_BLEND); else glDisable(GL_BLEND);
        if (depth) glEnable(GL_DEPTH_TEST); else glDisable(GL_DEPTH_TEST);
        if (cull) glEnable(GL_CULL_FACE); else glDisable(GL_CULL_FACE);
        if (scissor) glEnable(GL_SCISSOR_TEST); else glDisable(GL_SCISSOR_TEST);
        glDepthMask((GLboolean)dmask);
        glDepthFunc((GLenum)dfunc);
        glColorMask((GLboolean)cmask[0], (GLboolean)cmask[1], (GLboolean)cmask[2], (GLboolean)cmask[3]);
        glBlendFuncSeparate((GLenum)bsrc_rgb, (GLenum)bdst_rgb, (GLenum)bsrc_a, (GLenum)bdst_a);
        glBlendEquation((GLenum)beq);
    }
};

// ---------------------------------------------------------------- the probes: asking and reading

// The shoulder of hand h (tracking): beside and below the head, turned with it; from the eyes with the head level (a
// nod moves the eyes, not the shoulders).
Vec3 shoulder(int h) {
    const Quat yaw = yaw_quat(tracking_yaw(fi.head.orientation));
    return level_eyes(fi.head) + rotate(yaw, Vec3{h == kLeft ? -0.18f : 0.18f, -0.24f, 0.06f});
}

// Heather's body (world): the column she stands in, from her feet to above her head.
bool in_her_column(Vec3 world) {
    if (!game.player_valid) return false;
    const float dx = world.x - game.pos[0], dz = world.z - game.pos[2];
    const float up = game.pos[1] - world.y;  // Y down
    return dx * dx + dz * dz < (0.20f * kUnitsPerMetre) * (0.20f * kUnitsPerMetre) && up > 0.05f * kUnitsPerMetre &&
           up < 1.75f * kUnitsPerMetre;
}

bool in_capsule(const Capsule& c, Vec3 p) {
    const Vec3 ab = c.b - c.a;
    const float l2 = dot(ab, ab);
    const float t = l2 > 1e-6f ? std::clamp(dot(p - c.a, ab) / l2, 0.0f, 1.0f) : 0.0f;
    const Vec3 d = p - (c.a + ab * t);
    const float r = c.r + 0.012f * kUnitsPerMetre;
    return dot(d, d) < r * r;
}

void add_probe(ProbeSet& s, Vec3 tracking, uint8_t kind, uint8_t hand, uint8_t a, uint8_t b) {
    if (s.n >= kProbeMax) return;
    Probe& p = s.p[s.n++];
    p.world = world_point(tracking);
    p.kind = kind, p.hand = hand, p.a = a, p.b = b;
    float c[4];
    clip_of(0, p.world, c);
    p.on_screen = c[3] > 1.0f && std::fabs(c[0]) < c[3] && std::fabs(c[1]) < c[3] && c[2] > 0 && c[2] < c[3];
    p.own = p.on_screen ? linear_depth(c[2] / c[3]) : 0.0f;
}

void ask_probes(float uv_out[kProbeMax][2], ProbeSet& s) {
    s.n = 0;
    for (int h = 0; h < 2; h++) {
        const HandState& hs_ = hands[h];
        if (!hs_.valid) continue;
        const Vec3 sh = shoulder(h);
        for (int i = 0; i < kArmSamples; i++)
            add_probe(s, lerp(sh, hs_.grip.position, (i + 1) / (float)kArmSamples), kArm, (uint8_t)h, (uint8_t)i, 0);
        // The fingertips at each curl where the hand is drawn (session 8: not at the controller, which may be in the
        // wall the hand rests on: every tip there was "in it", and every finger stopped open).
        for (int f = 0; f < 5; f++)
            for (int k = 0; k < kTipSteps; k++) {
                Vec3 pts[4];
                finger_points(h, f, k / (float)(kTipSteps - 1), pts);
                add_probe(s, grip_point(hs_.shown, pts[3]), kTip, (uint8_t)h, (uint8_t)f, (uint8_t)k);
            }
        // The surface round the hand (session 8), for the planes it is at (body_frame): two rings about the palm's
        // middle across the eyes' view (the outer past the outline of her own hand drawn there, which hides the wall
        // under it), the palm's middle and the space under the palm (Jak's "under" probe), midway between the
        // controller and where the hand is drawn.
        {
            const float mx = h == kLeft ? -1.0f : 1.0f;
            const Vec3 palm_l{0.031f * mx, -0.015f, -0.008f};
            const Vec3 c = lerp(grip_point(hs_.grip, palm_l), grip_point(hs_.shown, palm_l), 0.5f);
            const Vec3 right = rotate(fi.head.orientation, Vec3{1, 0, 0}), up = rotate(fi.head.orientation, Vec3{0, 1, 0});
            for (int k = 0; k < 12; k++) {
                const float r = k < 8 ? 0.12f : 0.065f;
                const float a = (k < 8 ? k * 45.0f : 22.5f + (k - 8) * 90.0f) * kPi / 180.0f;
                add_probe(s, c + right * (r * std::cos(a)) + up * (r * std::sin(a)), kSurf, (uint8_t)h, (uint8_t)k, 0);
            }
            add_probe(s, c, kSurf, (uint8_t)h, 12, 0);
            add_probe(s, c + rotate(hs_.shown.orientation, Vec3{-mx, 0, 0}) * 0.07f, kSurf, (uint8_t)h, 13, 0);
        }
    }
    Vec3 from, dir;
    if (opt.laser && weapon_guide_dot() && gun_ray(from, dir)) {
        float d = 0.15f;
        for (int i = 0; i < kRaySamples; i++, d *= 1.25f) add_probe(s, from + dir * d, kRay, 0, (uint8_t)i, 0);
    }
    // The weapon's line as the controller holds it, the palm to the tip: where it first goes behind a surface the
    // weapon is held back (sh3vr_combat.cpp place_weapon).
    Vec3 wa, wb;
    if (weapon_reach(wa, wb))
        for (int i = 0; i < kWeaponSamples; i++)
            add_probe(s, lerp(wa, wb, (i + 1) / (float)kWeaponSamples), kWeapon, 0, (uint8_t)i, 0);
    // The world right before the eyes during a scene in first person (dynamic cutscenes: sh3vr.cpp
    // scene_first_person, view_blocked below).
    if (view_mode == ViewMode::kFirstPerson && (game.cutscene || game.demo)) {
        static const float yaw[kViewWays] = {0, -8, 8, -16, 16, -24, 24, -32, 32, 0, 0};
        static const float pitch[kViewWays] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 10, -10};
        for (int k = 0; k < kViewWays; k++) {
            const float y = yaw[k] * kPi / 180.0f, p = pitch[k] * kPi / 180.0f;
            const Vec3 dir = rotate(fi.head.orientation,
                                    Vec3{std::sin(y) * std::cos(p), std::sin(p), -std::cos(y) * std::cos(p)});
            add_probe(s, fi.head.position + dir * kViewNear, kView, 0, (uint8_t)(k * 2), 0);
            add_probe(s, fi.head.position + dir * kViewFar, kView, 0, (uint8_t)(k * 2 + 1), 0);
        }
    }
    // Her arms and hands as the game draws them this frame, the weapon in her hand, and the eyes: the depth's own
    // surfaces told apart later.
    s.ncaps = own_capsules(s.caps, kCapsuleMax);
    s.ncaps += weapon_capsules(s.caps + s.ncaps, kCapsuleMax - s.ncaps);
    s.eye = world_point(fi.head.position);
    for (int i = 0; i < s.n; i++) {
        float c[4];
        clip_of(0, s.p[i].world, c);
        const float w = std::fabs(c[3]) > 1e-6f ? c[3] : 1e-6f;
        // D3D clip to the texture: x right; the picture's first row is the top (D3D's +y).
        uv_out[i][0] = 0.5f + 0.5f * c[0] / w;
        uv_out[i][1] = 0.5f - 0.5f * c[1] / w;
    }
}

void read_answer(ProbeSet& s) {
    if (!s.pending || !s.fence) return;
    const GLenum r = glClientWaitSync(s.fence, 0, 0);
    if (r != GL_ALREADY_SIGNALED && r != GL_CONDITION_SATISFIED) return;
    glDeleteSync(s.fence);
    s.fence = nullptr;
    s.pending = false;
    glBindBuffer(GL_PIXEL_PACK_BUFFER, s.pbo);
    const uint8_t* px = (const uint8_t*)glMapBufferRange(GL_PIXEL_PACK_BUFFER, 0, kProbeMax * 4, GL_MAP_READ_BIT);
    if (!px) {
        glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
        return;
    }
    for (int h = 0; h < 2; h++) {
        for (int i = 0; i < kArmSamples; i++) answer.arm_ok[h][i] = answer.arm_hidden[h][i] = false;
        for (int f = 0; f < 5; f++)
            for (int k = 0; k < kTipSteps; k++) answer.tip_ok[h][f][k] = false;
    }
    for (int i = 0; i < kRaySamples; i++) answer.ray_ok[i] = false;
    for (int i = 0; i < kWeaponSamples; i++) answer.weapon_ok[i] = answer.weapon_hidden[i] = false;
    answer.weapon_asked = false;
    for (int i = 0; i < kViewSamples; i++) answer.view_ok[i] = false;
    answer.view_asked = false;
    answer.ray_n = 0;
    answer.own_hits = 0;
    answer.own_stencil = 0;
    answer.stencil = s.stencil;
    for (int i = 0; i < kWeaponSamples; i++) answer.weapon_her[i] = false;
    answer.surf_n[0] = answer.surf_n[1] = 0;
    answer.beyond_n[0] = answer.beyond_n[1] = 0;
    for (int i = 0; i < s.n; i++) {
        const Probe& p = s.p[i];
        const float d = (px[i * 4] * 65536.0f + px[i * 4 + 1] * 256.0f + px[i * 4 + 2]) / 16777215.0f;
        // Nothing drawn there (the far plane) or off the picture: no surface known.
        bool ok = p.on_screen && d < 0.999999f;
        float pen = -1.0f;
        bool hidden = false, own = false;
        Vec3 at{0, 0, 0};
        if (ok) {
            const float surface_depth = linear_depth(d);
            pen = (p.own - surface_depth) / kUnitsPerMetre;  // metres behind the surface
            // The surface the depth shows, on the eye's ray through the probe: Heather's own arm, hand or body is
            // not the world, and a probe behind it is free (her hand over a fingertip, her arm along the arm's line).
            at = s.eye + (p.world - s.eye) * (p.own > 1e-3f ? surface_depth / p.own : 1.0f);
            // 0.1.7: her pixels' stencil mark (her whole model and the weapon in her hand) where it was read: exact.
            // Without it, her column (the view's own probes: only her drawn arms and hands; her column holds her eyes,
            // so a post a few cm before them would count as her) and the capsules round her bones.
            // (0.1.9, owner 166: a scene drawing her away from her place with the eyes in her head marks her draws now,
            // for her shadow, sh3.c; the probes before the eyes still take her own body right there as no place to
            // watch the scene from, as before: her chest before her eyes while she is slumped over the table)
            const bool her_px = s.stencil && (px[i * 4 + 3] & 0x20) != 0 && !(p.kind == kView && ik_scene_eyes());
            own = her_px || (!s.stencil && p.kind != kView && in_her_column(at));
            for (int c = 0; c < s.ncaps && !own; c++) own = in_capsule(s.caps[c], at);
            if (own) {
                hidden = pen > 0;  // behind her: free for a fingertip, not known for the arm's line
                pen = -0.01f;
                answer.own_hits++;
                if (her_px) answer.own_stencil++;
            }
        }
        // Session 8: the world's surface near a hand (12 cm beyond the probe at most, 45 cm before it: a hand pushed
        // that far into a wall), for the planes the hand is at.
        if (ok && !own && (p.kind == kSurf || p.kind == kTip) && p.hand < 2 && pen > -0.12f && pen < 0.45f &&
            answer.surf_n[p.hand] < kSurfMost)
            answer.surf[p.hand][answer.surf_n[p.hand]++] = at;
        // Session 9 (vr132: a hand on the bench's seat, the fingers out past its edge, flat in the air: the seat's
        // front face is seen edge-on, the floor below was over 12 cm beyond the probes and dropped above, so nothing
        // told the hand the seat ended there): the far surface, or a point far out on the ray when nothing is drawn.
        if (p.on_screen && !own && (p.kind == kSurf || p.kind == kTip) && p.hand < 2 &&
            answer.beyond_n[p.hand] < kSurfMost) {
            if (ok && pen <= -0.12f) answer.beyond[p.hand][answer.beyond_n[p.hand]++] = at;
            else if (!ok) answer.beyond[p.hand][answer.beyond_n[p.hand]++] = s.eye + (p.world - s.eye) * 20.0f;
        }
        switch (p.kind) {
        case kArm:
            answer.arm_pen[p.hand][p.a] = pen;
            answer.arm_ok[p.hand][p.a] = ok;
            answer.arm_hidden[p.hand][p.a] = hidden;
            answer.arm_world[p.hand][p.a] = p.world;
            break;
        case kTip:
            answer.tip_pen[p.hand][p.a][p.b] = pen;
            answer.tip_ok[p.hand][p.a][p.b] = ok;
            break;
        case kRay:
            answer.ray_pen[p.a] = pen;
            answer.ray_ok[p.a] = ok;
            answer.ray_world[p.a] = p.world;
            answer.ray_n = std::max(answer.ray_n, (int)p.a + 1);
            break;
        case kWeapon:
            answer.weapon_pen[p.a] = pen;
            answer.weapon_ok[p.a] = ok;
            answer.weapon_hidden[p.a] = hidden;
            answer.weapon_her[p.a] = own;
            answer.weapon_asked = true;
            break;
        case kView:
            answer.view_pen[p.a] = pen;
            answer.view_ok[p.a] = ok;
            answer.view_asked = true;
            break;
        default:  // kSurf: its point taken above
            break;
        }
    }
    glUnmapBuffer(GL_PIXEL_PACK_BUFFER);
    glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
    answer.valid = true;
    answer.age = 0;
    answer.serial++;
    {
        // 0.1.7: every 10 s or so, how many of the probes met her and how many of those her stencil mark told.
        static int told_at = -100000;
        if (rt_frame - told_at >= 600) {
            told_at = rt_frame;
            rt_log("VR probes: frame %d: %d probes, %d met her or the weapon in her hand, %d of them by her pixels' "
                   "stencil mark (%s)", rt_frame, s.n, answer.own_hits, answer.own_stencil,
                   s.stencil ? "read" : "not read: her bones' capsules and column");
        }
    }

    // The guide dot: the first step of the ray that went behind a surface, between it and the step before.
    guide_valid = false;
    for (int i = 0; i < answer.ray_n; i++) {
        if (!answer.ray_ok[i]) continue;
        const float pen = answer.ray_pen[i];
        if (pen > 0) {
            Vec3 at = answer.ray_world[i];
            if (i > 0 && answer.ray_ok[i - 1] && answer.ray_pen[i - 1] <= 0) {
                const float a = answer.ray_pen[i - 1], b = pen;
                at = lerp(answer.ray_world[i - 1], answer.ray_world[i], a / (a - b));
            }
            guide_world = at;
            guide_valid = true;
            guide_age = 0;
            break;
        }
    }
}

void issue_probes(unsigned depth_tex) {
    ProbeSet& s = sets[next_set];
    if (s.pending) return;  // its answer has not come yet: no new question this frame
    static float uv[kProbeMax][2];
    ask_probes(uv, s);
    if (s.n == 0) return;
    glBindFramebuffer(GL_FRAMEBUFFER, probe_fbo);
    glViewport(0, 0, kProbeMax, 1);
    glDisable(GL_BLEND);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_SCISSOR_TEST);
    glColorMask(1, 1, 1, 1);
    glUseProgram(probe_prog);
    glUniform1i(probe_depth_loc, 0);
    // Unused slots read the middle of the picture (their answers are ignored).
    for (int i = s.n; i < kProbeMax; i++) uv[i][0] = uv[i][1] = 0.5f;
    // Two places a vec4, the whole array in one upload.
    if (probe_uv_loc >= 0) glUniform4fv(probe_uv_loc, kProbeMax / 2, &uv[0][0]);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D_ARRAY, depth_tex);
    glBindSampler(0, probe_sampler);
    glBindVertexArray(vao);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    // 0.1.7: her pixels' stencil mark into the alpha (the texture read as its stencil for this one draw, then as its
    // depth again for the game). Tried once: a refused stencil mode turns the pass off for the run.
    s.stencil = false;
    if (probe_st_prog && stencil_reads != 0) {
        if (stencil_reads < 0)
            while (glGetError() != GL_NO_ERROR) {
            }
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_DEPTH_STENCIL_TEXTURE_MODE, GL_STENCIL_INDEX);
        if (stencil_reads < 0) {
            const GLenum e = glGetError();
            stencil_reads = e == GL_NO_ERROR ? 1 : 0;
            if (e != GL_NO_ERROR)
                rt_log("VR: the probes' stencil pass is off (the depth's stencil mode was refused, GL error %04X): her "
                       "body told by her bones' capsules", (unsigned)e);
            else
                rt_log("VR: the probes read her pixels' stencil mark (her body and the weapon in her hand are never a wall)");
        }
        if (stencil_reads == 1) {
            glUseProgram(probe_st_prog);
            glUniform1i(probe_st_loc, 0);
            if (probe_st_uv_loc >= 0) glUniform4fv(probe_st_uv_loc, kProbeMax / 2, &uv[0][0]);
            glColorMask(0, 0, 0, 1);
            glDrawArrays(GL_TRIANGLES, 0, 3);
            glColorMask(1, 1, 1, 1);
            s.stencil = true;
        }
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_DEPTH_STENCIL_TEXTURE_MODE, GL_DEPTH_COMPONENT);
    }
    glBindBuffer(GL_PIXEL_PACK_BUFFER, s.pbo);
    glReadPixels(0, 0, kProbeMax, 1, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
    s.fence = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
    s.pending = true;
    next_set ^= 1;
}

// ---------------------------------------------------------------- the hands each frame

float ease(float rate) { return 1.0f - std::exp(-rate * frame_dt()); }

void wanted_curls(int h, float out[5]) {
    const Controls& c = fi.controls;
    const float trig = h == kLeft ? c.left_trigger : c.right_trigger;
    const float grip = h == kLeft ? c.left_grip : c.right_grip;
    const bool first = h == kLeft ? c.left_first_touch : c.right_first_touch;
    const bool thumb = h == kLeft ? (c.left_thumb_touch || c.x || c.y) : (c.right_thumb_touch || c.a || c.b);
    // Her fingers follow the player's (the owner: "finger gestures (pointing, thumbs up, middle finger, etc)", Jak's
    // update_fingers for reference): the index by the trigger's touch sensor and its pull, the middle, ring and pinky by
    // the grip, the thumb by its sensors (a button, the stick, the thumb rest). So a fist, a point (the index off the
    // trigger, the grip held), a thumbs up (the thumb lifted, the grip held: the thumb stands straight), an open hand
    // and a pinch (the trigger pulled, the grip open) are the player's own. A controller without thumb sensors counts
    // the thumb as down (xr_openxr.cpp).
    // Jak's values (jak_vr.cpp update_fingers) for the rest: an open hand's fingers relaxed (12% curled, not stiff), the
    // grip closing them from 10% to 90% of its travel. The index resting on the trigger is half curled round it, as the
    // player's is (run vr83: Jak's quarter left it sticking out of a thumbs up).
    out[kIndex] = trig > 0.05f ? 0.5f + 0.5f * trig : (first ? 0.5f : 0.0f);
    out[kMiddle] = out[kRing] = out[kPinky] = 0.12f + 0.88f * std::clamp((grip - 0.1f) / 0.8f, 0.0f, 1.0f);
    out[kThumb] = thumb ? std::max(0.65f, grip * 0.75f) : 0.0f;
    // The middle finger: no controller senses it alone. Raised, the player's index pulls the trigger while the middle
    // finger leaves the grip and the ring and pinky keep it half pressed, the thumb down: that shape, held a sixth of a
    // second (a fist closing passes through it), raises her middle finger over the closed others. A fist (the grip
    // pressed all the way) or an open hand (the grip let go) is never it.
    static float middle_for[2] = {};
    const bool middle = trig > 0.75f && grip > 0.2f && grip < 0.7f && thumb;
    middle_for[h] = middle ? middle_for[h] + frame_dt() : 0.0f;
    if (middle_for[h] > 0.16f) {
        out[kIndex] = out[kRing] = out[kPinky] = 0.95f;
        out[kMiddle] = 0.0f;
        out[kThumb] = 0.75f;
    }
    if (hands[h].holding_weapon || hands[h].on_foregrip) {
        // Round the handle: the index finger on the trigger (the gun's) or round the grip with the others.
        out[kMiddle] = out[kRing] = out[kPinky] = std::max(grip, 0.82f);
        out[kThumb] = 0.7f;
        out[kIndex] = 0.45f + 0.35f * trig;
    }
    // LIST 46: the off hand round a magazine, a shell or the slide while it reloads.
    float r[5];
    if (reload_fingers(h, r))
        for (int f = 0; f < 5; f++) out[f] = r[f];
}

// ---------------------------------------------------------------- the fingers on the weapon and the parts

float weapon_free = 1.0f;     // the free part of the weapon's line (its probes): 1 nothing in the way
bool weapon_touch[2]{};       // a free hand's finger on the weapon (its tick when it starts)
float finger_target[2][5]{};  // the curls body_fingers chose (the crash log)
float trigger_shown[2]{};     // session 31: how far each hand's trigger finger, drawn, pushes the trigger (0..1)

// Session 31: the trigger finger's curl at pull t (0..1) between its pull curls (body_trigger_pull_curls), and how far
// a finger drawn at curl c has pushed the trigger (the same, inverted: 0 short of the trigger, 1 past the end).
float curl_of_pull(const float pc[5], float t) {
    t = std::clamp(t, 0.0f, 1.0f) * 4.0f;
    const int i = std::min(3, (int)t);
    return pc[i] + (pc[i + 1] - pc[i]) * (t - (float)i);
}
float pull_of_curl(const float pc[5], float c) {
    if (!(c > pc[0])) return 0.0f;
    for (int i = 0; i < 4; i++)
        if (c <= pc[i + 1]) return ((float)i + (c - pc[i]) / std::max(pc[i + 1] - pc[i], 1e-5f)) * 0.25f;
    return 1.0f;
}

// The closest distance between segments p1-q1 and p2-q2.
float seg_dist(Vec3 p1, Vec3 q1, Vec3 p2, Vec3 q2) {
    const Vec3 d1 = q1 - p1, d2 = q2 - p2, r = p1 - p2;
    const float a = dot(d1, d1), e = dot(d2, d2), f = dot(d2, r);
    float s = 0, t = 0;
    if (a <= 1e-12f && e <= 1e-12f) return length(r);
    if (a <= 1e-12f) t = std::clamp(f / e, 0.0f, 1.0f);
    else {
        const float c = dot(d1, r);
        if (e <= 1e-12f) s = std::clamp(-c / a, 0.0f, 1.0f);
        else {
            const float b = dot(d1, d2), den = a * e - b * b;
            s = den > 1e-12f ? std::clamp((b * f - c * e) / den, 0.0f, 1.0f) : 0.0f;
            t = (b * s + f) / e;
            if (t < 0) t = 0, s = std::clamp(-c / a, 0.0f, 1.0f);
            else if (t > 1) t = 1, s = std::clamp((b - c) / a, 0.0f, 1.0f);
        }
    }
    return length((p1 + d1 * s) - (p2 + d2 * t));
}

// Whether finger f of hand h (drawn at `pose`, tracking) at curl c touches the part it holds (`part`: capsules in its
// grip space) or the weapon (its capsules, sh3vr_combat.cpp weapon_clearance).
bool finger_touches(int h, int f, float c, const Pose& pose, const PartCaps* part, bool weapon, float spread = 0.0f) {
    const float r = f == kThumb ? 0.0095f : 0.0085f;  // her finger's half thickness
    Vec3 p[4];
    finger_points(h, f, c, p, spread);
    float line[3] = {1e9f, 1e9f, 1e9f};
    for (int j = 0; j < 3; j++) {
        if (part)
            for (int k = 0; k < part->n; k++)
                if (seg_dist(p[j], p[j + 1], part->a[k], part->b[k]) - part->r[k] < r) return true;
        if (weapon && (line[j] = weapon_clearance(h, grip_point(pose, p[j]), grip_point(pose, p[j + 1]))) < r)
            return true;
    }
    // 0.1.10 (owner 172): on the weapon, her finger's own skin as drawn (hand_skin) 0.5 mm out of it, where its line
    // comes within the skin's reach of it
    if (weapon && h >= 0 && h < 2 && hand_skin[h].ready) {
        Vec3 o[3];
        Quat q[3];
        const float c3[3] = {c, c, c};
        bool framed = false;
        for (int j = 0; j < 3; j++) {
            if (hand_skin[h].seg[f][j].empty() || line[j] > hand_skin[h].reach[f][j] + kSkinMargin) continue;
            if (!framed && !(framed = finger_frames(h, f, c3, spread, o, q))) break;
            if (skin_segment_gap(h, f, j, o, q, pose) < kSkinMargin) return true;
        }
    }
    return false;
}

// The curl (0..upto) where the finger closing first meets what it holds or touches: the last curl free of it (found
// between two steps by halving). -1: it meets nothing; `free_from` then says from which curl it is clear (0 always clear,
// above 0 it begins inside the shape, as a thumb laid along a slide does, and comes out of it there; -1 never clear).
// (session 31: `spread` as finger_points')
float finger_contact(int h, int f, const Pose& pose, const PartCaps* part, bool weapon, float upto, float& free_from,
                     float spread = 0.0f) {
    constexpr int kSteps = 10;
    bool was = finger_touches(h, f, 0.0f, pose, part, weapon, spread);
    free_from = was ? -1.0f : 0.0f;
    float prev = 0;
    for (int i = 1; i <= kSteps; i++) {
        const float c = upto * i / kSteps;
        const bool now_touching = finger_touches(h, f, c, pose, part, weapon, spread);
        if (now_touching != was) {
            float lo = prev, hi = c;  // `was` holds at lo, the other at hi
            for (int k = 0; k < 6; k++) {
                const float m = (lo + hi) * 0.5f;
                if (finger_touches(h, f, m, pose, part, weapon, spread) == was) lo = m;
                else hi = m;
            }
            if (!was) return lo;  // closed onto it
            free_from = hi;       // came out of it: closing on
        }
        was = now_touching;
        prev = c;
    }
    return -1.0f;
}

// Session 36 (owner message 169: "all grips should be perfect. shape the fucking hand around them with no clipping";
// the 0.1.9 video of the handgun, and vr413's own report: her fingers closed with every joint together, so each stopped
// where its first part met the weapon; on the handgun the ring and little fingers met the grip with their tips while
// their middle segments stood 10-14 mm off it, vr490: R 0.63 at every joint, its segments 5.2 13.8 0.1 mm off): a hand
// round a grip closes joint by joint, each part coming to rest on it, the middle joint bent most (a power grip: about 50
// degrees at the knuckle, 85 at the middle joint, 30 at the last). From the curl c where the finger closing first meets
// the weapon (finger_contact, every joint at c): the knuckle's own curl from 0.05 under c to 0.35 over, the middle
// joint's from 0.25 under to 0.45 over (0.05 apart), the last joint then closed as far as it goes clear of the weapon
// (halving, from 0.7 under the middle joint's to 0.1 over it). Every segment's skin stays out of the weapon, as
// finger_contact asks, and so does every pose on the way there from c (a quarter at a time: the joints ease along that
// way, so a finger never passes through a thin part such as a trigger guard to its far side). Of those, the one whose
// segments lie nearest the weapon (the middle and last segments' gaps, the first's at half, each counted to 15 mm), a
// finger bent at its knuckle but straight past it weighing against it, and a little the joints' change from c. c at
// every joint is one of them, so these weighed gaps are never more than before.
constexpr float kWrapHalf = 0.0085f;  // (finger_touches': her finger's half thickness)
// The finger's segments' skin from the weapon (metres; below 0 in it), segments `from` to `to` - 1 (the others kept).
void joint_gaps(int h, int f, const Pose& pose, const float c3[3], float g[3], int from, int to = 3,
                float spread = 0.0f) {
    Vec3 p[4];
    finger_points3(h, f, c3, p, spread);
    Vec3 o[3];
    Quat q[3];
    bool framed = false, tried = false;
    for (int j = from; j < to; j++) {
        const float line = weapon_clearance(h, grip_point(pose, p[j]), grip_point(pose, p[j + 1]));
        g[j] = line - kWrapHalf;
        // 0.1.10 (owner 172): the segment's own skin as drawn (hand_skin), where its line comes within 5 mm past the
        // skin's reach, kept 0.5 mm farther out (so at finger_contact's curl it reads 0 at least, as the capsule does;
        // joints_clear's 0.3 mm in then leaves it 0.2 mm off the weapon, kSkinCheck)
        if (h < 0 || h > 1 || !hand_skin[h].ready || hand_skin[h].seg[f][j].empty() ||
            line > hand_skin[h].reach[f][j] + 0.005f)
            continue;
        if (!tried) tried = true, framed = finger_frames(h, f, c3, spread, o, q);
        if (framed) g[j] = std::min(g[j], skin_segment_gap(h, f, j, o, q, pose) - kSkinMargin);
    }
}
bool joints_clear(int h, int f, const Pose& pose, const float c3[3]) {
    float g[3];
    joint_gaps(h, f, pose, c3, g, 0);
    return g[0] >= -0.0003f && g[1] >= -0.0003f && g[2] >= -0.0003f;
}
float wrap_cost(float c, const float c3[3], const float g[3]) {
    auto mm = [](float v) { return std::clamp(v * 1000.0f, 0.0f, 15.0f); };
    return mm(g[0]) * 0.5f + mm(g[1]) + mm(g[2]) + 15.0f * std::max(0.0f, c3[0] - c3[1] - 0.15f) +
           3.0f * (std::fabs(c3[0] - c) + std::fabs(c3[1] - c) + std::fabs(c3[2] - c));
}
bool finger_wrap(int h, int f, const Pose& pose, float c, float out[3]) {
    out[0] = out[1] = out[2] = c;
    if (!(c >= 0.0f && c <= 1.0f) || f == kThumb) return false;
    float g[3];
    joint_gaps(h, f, pose, out, g, 0);
    if (g[0] < 0.0f || g[1] < 0.0f || g[2] < 0.0f) return false;  // (c itself in it: left as it is)
    float best = wrap_cost(c, out, g);
    for (int i = -1; i <= 7; i++) {
        const float a0 = c + 0.05f * (float)i;
        if (a0 < 0.0f || a0 > 1.0f) continue;
        float g0[3];
        const float k0[3] = {a0, a0, a0};
        joint_gaps(h, f, pose, k0, g0, 0, 1);  // (the first segment: the knuckle's curl alone)
        if (g0[0] < 0.0f) {
            if (i > 0) break;  // (closing the knuckle further only goes deeper)
            continue;
        }
        for (int k = -5; k <= 9; k++) {
            const float a1 = c + 0.05f * (float)k;
            if (a1 < 0.0f || a1 > 1.0f) continue;
            // (its cost no lower than its first segment's gap and these two joints' terms: past the best, not tried)
            const float least = std::clamp(g0[0] * 1000.0f, 0.0f, 15.0f) * 0.5f +
                                15.0f * std::max(0.0f, a0 - a1 - 0.15f) + 3.0f * (std::fabs(a0 - c) + std::fabs(a1 - c));
            if (!(least < best - 1e-4f)) continue;
            float t[3] = {a0, a1, std::max(0.0f, a1 - 0.7f)}, q[3] = {g0[0], 0.0f, 0.0f};
            joint_gaps(h, f, pose, t, q, 1);
            if (q[1] < 0.0f || q[2] < 0.0f) continue;
            float lo = t[2], lo_g = q[2], hi = std::min(1.0f, a1 + 0.1f);
            t[2] = hi;
            joint_gaps(h, f, pose, t, q, 2);
            if (q[2] >= 0.0f) {
                lo = hi, lo_g = q[2];
            } else {
                for (int s = 0; s < 6; s++) {
                    t[2] = (lo + hi) * 0.5f;
                    joint_gaps(h, f, pose, t, q, 2);
                    if (q[2] >= 0.0f) lo = t[2], lo_g = q[2];
                    else hi = t[2];
                }
            }
            t[2] = lo, q[2] = lo_g;
            const float cost = wrap_cost(c, t, q);
            if (!(cost < best - 1e-4f)) continue;
            bool way = true;  // (the way there from c, a quarter at a time)
            for (int s = 1; s <= 3 && way; s++) {
                const float u = s * 0.25f, w3[3] = {c + (t[0] - c) * u, c + (t[1] - c) * u, c + (t[2] - c) * u};
                way = joints_clear(h, f, pose, w3);
            }
            if (way) best = cost, out[0] = t[0], out[1] = t[1], out[2] = t[2];
        }
    }
    return true;
}
// Each finger's wrap on the weapon held (finger_wrap), kept while the weapon and its shape, the hand's role on it, the
// hand's place on it (the finger's knuckle and straight tip in the weapon's model, each within half a unit: about a
// millimetre) and the finger's contact curl (within 0.02) stay, and while it lies clear of the weapon (checked every
// frame: the other hand's fingers count as the weapon for the hand on the second grip, and they move). A new one is
// solved for at most one finger of each hand a frame (about a thousand of the shape's segment distances); meanwhile a
// finger is drawn at its contact curl, every joint alike, as before.
struct WrapKept {
    bool solved = false;
    int key = -2, role = 0;
    float c = -1.0f;
    Vec3 knuckle, tip;
    float j[3]{};
};
WrapKept wraps[2][5];
int wraps_solved[2];  // (how many each hand has solved: the rig's pictures report it)
bool wrap_of(int h, int f, int role, const Pose& pose, float c, int& solves, float out[3]) {
    WrapKept& w = wraps[h][f];
    Vec3 p[4], mk, mt;
    finger_points(h, f, 0.0f, p);
    if (!weapon_model_point(grip_point(pose, p[0]), mk) || !weapon_model_point(grip_point(pose, p[3]), mt)) {
        w.solved = false;
        return false;
    }
    const int key = weapon_shape_key();
    const bool same = w.solved && w.key == key && w.role == role && std::fabs(w.c - c) < 0.02f &&
                      length(mk - w.knuckle) < 0.5f && length(mt - w.tip) < 0.5f;
    if (same && joints_clear(h, f, pose, w.j)) {
        for (int j = 0; j < 3; j++) out[j] = w.j[j];
        return true;
    }
    if (solves <= 0) return false;
    solves--;
    wraps_solved[h]++;
    const auto t0 = std::chrono::steady_clock::now();
    w.solved = finger_wrap(h, f, pose, c, w.j);
    const double took = std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - t0).count();
    w.key = key, w.role = role, w.c = c, w.knuckle = mk, w.tip = mt;
    if (!w.solved) return false;
    static int told = 0;
    if (told < 64) {
        told++;
        float g0[3], g1[3];
        const float cc[3] = {c, c, c};
        joint_gaps(h, f, pose, cc, g0, 0);
        joint_gaps(h, f, pose, w.j, g1, 0);
        static const char* const kNames[5] = {"thumb", "index", "middle", "ring", "little"};
        rt_log("VR: frame %d: the %s hand's %s finger on the weapon (as %d) closed joint by joint: %.2f %.2f %.2f "
               "(every joint at %.2f before); its segments' skin %.1f %.1f %.1f mm off it (before %.1f %.1f %.1f); solved in %.0f us",
               rt_frame, h == kLeft ? "left" : "right", kNames[f], role, w.j[0], w.j[1], w.j[2], c, g1[0] * 1000.0f,
               g1[1] * 1000.0f, g1[2] * 1000.0f, g0[0] * 1000.0f, g0[1] * 1000.0f, g0[2] * 1000.0f, took);
    }
    for (int j = 0; j < 3; j++) out[j] = w.j[j];
    return true;
}

// 0.1.8 (vr371: her straight thumb stands up into a gun's frame, so finger_contact finds it in the weapon at every curl
// or out of it only past half): the thumb's curl resting on the weapon held: of the curls where it is clear of it (every
// 0.05), the one nearest a relaxed thumb's 0.65 among those lying on it (`rests`), else the one nearest 0.65. -1: in it
// at every curl. vr373's record: her thumb's sweep runs across the grip, so it touches it at every curl; it rests on it
// as a real thumb presses on a grip, its pad up to 4 mm into its skin, its root (the palm's) clear of it by 2 mm at its
// middle. vr374: her thumb only curls (it cannot swing out to the side), so with her fingers round a gun's grip its sweep
// crosses the gun at every curl: then the curl it goes in least (`depth`: how far past that, metres; the fit weighs it).
// vr376 (the guns' own shapes): with her fist round a pistol grip her curled thumb meets the frame's side at every curl;
// a shooter's thumb lies straight along it: so the curls tried run from bent back (-0.4, as a hand laid flat has it) to 1.
// Session 31 (vr384, the handgun from the eyes: her thumb met the frame with its root at 0.65 and stood out to the left at
// about 45 degrees, its tip 1.5 cm off the gun; owner message 151 "PERFECT as can be"): the thumb also swings sideways at
// its root (`swing`, degrees, finger_points' spread: up to 45 each way, as a real thumb's root moves it towards or away
// from the palm), and it rests ON the weapon only with its pad lying on it (the pad's skin within 4 mm, the middle
// segment's within 8: lying along the frame, or round a handle), not with its root or a knuckle only. Every curl (0.1 apart) at every
// swing (15 apart), then the best one's neighbours (0.05, 7.5); the least swung and nearest 0.65 of those lying on it.
namespace {
constexpr float kThumbSwings[7] = {0, 15, -15, 30, -30, 45, -45};
}
float thumb_rest(int h, const Pose& pose, bool* rests, float* depth, float* swing) {
    float best = -1.0f, best_key = 1e9f, best_sw = 0.0f, most = -1e9f, most_c = -1.0f, most_sw = 0.0f;
    bool best_on = false;
    auto one = [&](float c, float sw) {
        Vec3 p[4];
        finger_points(h, kThumb, c, p, sw);
        if (weapon_clearance(h, grip_point(pose, p[0]), grip_point(pose, p[1])) < 0.002f) return;
        const float d1 = weapon_clearance(h, grip_point(pose, p[1]), grip_point(pose, p[2]));
        const float d2 = weapon_clearance(h, grip_point(pose, p[2]), grip_point(pose, p[3]));
        float clear = std::min(d1, d2);
        // 0.1.10 (owner 172): its two outer segments' own skin as drawn (hand_skin), where their lines come within its
        // reach: read as the capsules' 5.5 mm is (below, a rest's least) once it is 0.5 mm off the weapon
        if (hand_skin[h].ready) {
            Vec3 o[3];
            Quat q[3];
            const float c3[3] = {c, c, c};
            if (finger_frames(h, kThumb, c3, sw, o, q))
                for (int j = 1; j < 3; j++)
                    if (!hand_skin[h].seg[kThumb][j].empty() &&
                        (j == 1 ? d1 : d2) < hand_skin[h].reach[kThumb][j] + 0.006f)
                        clear = std::min(clear, skin_segment_gap(h, kThumb, j, o, q, pose) + 0.005f);
        }
        // (vr388: a thumb bent back, below 0, is the last a hand does: one clear of the weapon with a segment near it
        // comes first; straight along a pistol's frame, 0 to 0.2, is a shooter's)
        auto pref_of = [](float cc, float s) {
            return std::fabs(cc - 0.65f) + std::fabs(s) / 45.0f * 0.6f + (cc < -0.01f ? 1.5f : 0.0f);
        };
        const float pref = pref_of(c, sw);
        if (clear > most + 0.0005f || (std::fabs(clear - most) <= 0.0005f && pref < pref_of(most_c, most_sw)))
            most = clear, most_c = c, most_sw = sw;
        if (clear < 0.0055f) return;
        // (vr386: the stun gun's thumb "rested" bent back, -0.3, its last joint at the box's corner and its tip pointing
        // away: the pad itself, 55% along the last segment, must be on it)
        const Vec3 pad = grip_point(pose, p[2] + (p[3] - p[2]) * 0.55f);
        const float dp = weapon_clearance(h, pad, pad);
        const bool on = dp < 0.0135f && d1 < 0.0175f;
        const float key = pref * 0.5f + (on ? 0.0f : dp < 0.0135f || d1 < 0.0175f ? 1.0f : 2.0f);
        if (key < best_key) best_key = key, best = c, best_sw = sw, best_on = on;
    };
    for (const float sw : kThumbSwings)
        for (int i = -4; i <= 10; i++) one(i * 0.1f, sw);
    if (best > -0.5f) {
        const float c0 = best, s0 = best_sw;
        for (const float dc : {-0.05f, 0.0f, 0.05f})
            for (const float ds : {-7.5f, 0.0f, 7.5f})
                if ((dc != 0.0f || ds != 0.0f) && c0 + dc >= -0.4f && c0 + dc <= 1.0f && std::fabs(s0 + ds) <= 45.0f)
                    one(c0 + dc, s0 + ds);
    }
    if (depth) *depth = 0.0f;
    if (best < -0.5f && most_c > -0.5f) {  // (-1: none; a curl is -0.4 .. 1)
        best = most_c, best_sw = most_sw, best_on = true;
        if (depth) *depth = 0.0055f - most;
    }
    if (rests) *rests = best_on;
    if (swing) *swing = best < -0.5f ? 0.0f : best_sw;
    return best;
}

// Session 31: thumb_rest for the hand as drawn (body_fingers), kept while the hand stays where it is on the weapon
// (its middle and two points 5 cm along its axes, in the weapon's model, within 1 mm; the weapon and the pump's draw
// the same) and searched again at least every 30 frames (the main hand's fingers, which the other hand's thumb is
// clear of, move with the trigger): the search is 113 tries of three segments against the weapon's own shape.
struct ThumbKeep {
    bool known = false;
    Vec3 at[3];
    int weapon = -1, frame = 0;
    float pump = 0, curl = -1, swing = 0;
} thumb_keep[2];

float thumb_resting(int h, const Pose& pose, float& swing) {
    ThumbKeep& k = thumb_keep[h];
    Vec3 at[3];
    const Vec3 tp[3] = {pose.position, pose.position + rotate(pose.orientation, Vec3{0.05f, 0, 0}),
                        pose.position + rotate(pose.orientation, Vec3{0, 0.05f, 0})};
    for (int i = 0; i < 3; i++)
        if (!weapon_model_point(tp[i], at[i])) {
            k.known = false;
            swing = 0.0f;
            return -1.0f;
        }
    const float pump = reload_pump_back();
    bool same = k.known && k.weapon == game.weapon && std::fabs(k.pump - pump) < 0.001f && rt_frame - k.frame < 30;
    for (int i = 0; i < 3 && same; i++) same = length(at[i] - k.at[i]) < 0.001f * kUnitsPerMetre;
    if (!same) {
        k.curl = thumb_rest(h, pose, nullptr, nullptr, &k.swing);
        k.known = true, k.weapon = game.weapon, k.frame = rt_frame, k.pump = pump;
        for (int i = 0; i < 3; i++) k.at[i] = at[i];
    }
    swing = k.curl > -0.5f ? k.swing : 0.0f;
    return k.curl;
}

// The flashlight in a hand (session 7; the owner: "make sure hand/finger grip pose for flashlight is proper and not
// jank, just like how we did it for the guns", "similar to how into the radius handles its pocket lamp"). Its box
// (8.1 x 5.1 x 3.9 cm, the lens on a long face) is held as a box lamp is, from the side: the palm against one side,
// the fingers over its lower front (as a phone is held to take a picture), the thumb on its top, the lens along the
// controller's pointing, its upper third clear (the beam leaves there). Where it sits in the fist is searched once
// a hand (again if her own fingers are read or the controller's aim turns against its grip): each place on a grid in
// the grip space is tried with her fingers closing on it (two capsules, as the parts are); kept only with her knuckles
// and palm clear of it and every finger starting outside it; scored as hold_score says (grip first: her palm on it, her fingers round it; the lens after; the first
// version scored the lens first and left her hand open beside it, vr109).
constexpr float kBoxHalf[3] = {19.75f / kUnitsPerMetre, 12.6f / kUnitsPerMetre, 9.45f / kUnitsPerMetre};
struct BoxAxes {
    Vec3 x, y, z;  // up (the controller's up), across, the lens (the controller's pointing)
};
struct LightHold {
    bool known = false, her = false;
    Quat rel{0, 0, 0, 1};  // the aim in the grip space it was searched for
    Vec3 c;                // the box's middle, grip space
    Vec3 lens;             // where the beam leaves: the middle of the lens' part her fingers leave clear, grip space
    // The search, a slice a frame (session 8): its grid's next place, the best so far, why places failed.
    bool searching = false;
    int next = 0, tried = 0;
    float best = -1, best_cs[5]{}, best_clear = 0, best_palm = 0, best_wrap = 0;
    Vec3 best_c, best_lens;
    int why[8]{};
} light_hold[2];
int hold_why = 0;  // the reason the last hold_score said -1 (1 knuckle, 2 palm inside, 3 finger starts inside, 4 finger
                   // curl, 5 thumb, 6 lens, 7 palm off it)

BoxAxes light_axes(const Quat& rel) {
    BoxAxes a;
    a.z = normalized(rotate(rel, Vec3{0, 0, -1}));
    Vec3 up = rotate(rel, Vec3{0, 1, 0});
    up = up - a.z * dot(up, a.z);
    a.x = normalized(up);
    a.y = cross(a.z, a.x);
    return a;
}

void light_caps(Vec3 c, const BoxAxes& a, PartCaps& pc) {
    const float r = kBoxHalf[2], len = kBoxHalf[0] - r, w = kBoxHalf[1] - r;
    pc.n = 2;
    for (int i = 0; i < 2; i++) {
        const Vec3 m = c + a.y * (i ? w : -w);
        pc.a[i] = m - a.x * len;
        pc.b[i] = m + a.x * len;
        pc.r[i] = r;
    }
}

bool in_box(Vec3 p, Vec3 c, const BoxAxes& a, float margin) {
    const Vec3 d = p - c;
    return std::fabs(dot(d, a.x)) < kBoxHalf[0] + margin && std::fabs(dot(d, a.y)) < kBoxHalf[1] + margin &&
           std::fabs(dot(d, a.z)) < kBoxHalf[2] + margin;
}

// A place's score (-1: the hand cannot hold it there), each finger's contact curl, the lens' clear share, how much of
// her palm lies against it and how many of her finger joints lie along it (a wrap, not fingertips on a face).
// Session 8 (vr109: the box stood past her open fingertips, her palm 0% on it; the pictures showed a hand reaching for
// it, not holding it): a hold now needs her palm against it (40% of her palm's points within 1.2 cm), her index,
// middle and ring fingers closed on it (curl 0.15-0.9), her thumb resting on it, and no finger starting inside it; it is
// scored grip first (palm and wrap), the lens' clear share after (at least 30%). This lamp is a flat box with its lens on
// a broad face: no grip wraps it with the whole lens clear, so the fingers hold its lower front and the thumb lies on
// its top (offline with her own fingers at the sim's 50 degrees: her palm 50% on it, the fingers at 0.60 0.39 0.24
// 0.21, the thumb at 0.11, the lens 34% clear; vr111 in the game: the same, 34% clear, and no valid place clears more
// however the lens is weighted, as a phone is held to take a picture: the fingers over the lower back, the camera's top
// clear). `lens` is the middle of the clear part of the face: the beam and its glare leave from there, never from
// under her fingers (session 8; before, the face's middle).
float hold_score(int h, Vec3 c, const BoxAxes& a, float cs[5], float& clear, float& palm, float& wrap, Vec3& lens) {
    const float mx = h == kLeft ? -1.0f : 1.0f;
    Vec3 kn[5];
    for (int f = 0; f < 5; f++) {
        Vec3 p[4];
        finger_points(h, f, 0.0f, p);
        kn[f] = p[0];
        if (in_box(p[0], c, a, 0.006f)) return hold_why = 1, -1;
    }
    // Her palm: a 5 x 4 grid of its skin, 1.1 cm inside her four knuckles' line towards the palm's side, from the
    // knuckles back towards the wrist (+Y) and across the fist (Z).
    const float palm_x = (kn[kIndex].x + kn[kMiddle].x + kn[kRing].x + kn[kPinky].x) * 0.25f - 0.011f * mx;
    Vec3 pp[20];
    for (int i = 0; i < 5; i++)
        for (int k = 0; k < 4; k++) pp[i * 4 + k] = Vec3{palm_x, -0.040f + 0.015f * i, -0.026f + 0.0167f * k};
    for (const Vec3& p : pp)
        if (in_box(p, c, a, -0.002f)) return hold_why = 2, -1;  // her palm more than 2 mm inside it
    PartCaps pc;
    light_caps(c, a, pc);
    const Pose& pose = hands[h].shown;
    for (int f = 0; f < 5; f++) {
        if (finger_touches(h, f, 0.0f, pose, &pc, false)) return hold_why = 3, -1;
        float free_from = 0;
        cs[f] = finger_contact(h, f, pose, &pc, false, 1.0f, free_from);
    }
    for (int f = kIndex; f <= kRing; f++)
        if (!(cs[f] >= 0.15f && cs[f] <= 0.9f)) return hold_why = 4, -1;
    if (!(cs[kThumb] >= 0.03f && cs[kThumb] <= 0.95f)) return hold_why = 5, -1;
    // The lens: a 7 x 5 grid on its face, each point's beam (its first 8 cm) clear of every finger at its curl.
    Vec3 sa[15], sb[15];
    float sr[15];
    int n = 0;
    for (int f = 0; f < 5; f++) {
        Vec3 p[4];
        finger_points(h, f, cs[f] >= 0 ? cs[f] : 0.9f, p);
        for (int j = 0; j < 3; j++, n++) sa[n] = p[j], sb[n] = p[j + 1], sr[n] = f == kThumb ? 0.0095f : 0.0085f;
    }
    int ok = 0, all = 0;
    float su = 0, sv = 0;
    for (int iu = 0; iu < 7; iu++)
        for (int iv = 0; iv < 5; iv++, all++) {
            const float u = (iu / 3.0f - 1.0f) * kBoxHalf[0] * 0.85f, v = (iv / 2.0f - 1.0f) * kBoxHalf[1] * 0.85f;
            const Vec3 o = c + a.z * (kBoxHalf[2] + 0.001f) + a.x * u + a.y * v;
            const Vec3 e = o + a.z * 0.08f;
            bool free = true;
            for (int s = 0; s < n && free; s++) free = seg_dist(o, e, sa[s], sb[s]) > sr[s];
            ok += free;
            if (free) su += u, sv += v;
        }
    clear = ok / (float)all;
    lens = c + a.z * kBoxHalf[2] + (ok ? a.x * (su / ok) + a.y * (sv / ok) : Vec3{0, 0, 0});
    if (clear < 0.3f) return hold_why = 6, -1;
    // Held, not touched (vr108/vr109: the box stood off her palm against her fingertips): her palm's points within
    // 1.2 cm of its surface, and her finger joints (the middle and end ones, and the tips) along it.
    auto surface = [&](Vec3 p) {
        float d = 1e9f;
        for (int k = 0; k < pc.n; k++) d = std::min(d, seg_dist(p, p, pc.a[k], pc.b[k]) - pc.r[k]);
        return d;
    };
    int near_palm = 0, along = 0;
    for (const Vec3& p : pp) near_palm += surface(p) < 0.012f;
    for (int f = kIndex; f <= kPinky; f++) {
        if (cs[f] < 0) continue;
        Vec3 p[4];
        finger_points(h, f, cs[f], p);
        for (int j = 1; j < 4; j++) along += surface(p[j]) < 0.012f;
    }
    palm = near_palm / 20.0f;
    wrap = along / 12.0f;
    if (palm < 0.4f) return hold_why = 7, -1;
    return 1.0f + (cs[kPinky] >= 0 ? 0.5f : 0.0f) + clear + palm * 3.0f + wrap * 3.0f;
}

// Hand h's flashlight place (grip space) for its controller's aim. The search runs a slice a call (session 8: a 4 mm
// grid is about 3000 places, too many for one frame on the headset), from the moment her hands are read
// (body_light_prepare, every frame), so it is done before the lamp is first taken; until then the offline search's
// place is used. The grid: every 4 mm from 4 cm past the fist's middle (away from the palm) to 1.2 cm towards it, 4.8
// cm ahead of the knuckles' side to 0.8 cm behind, 3.2 cm past the thumb's end to 2.4 cm towards the little finger
// (the offline search over a wider box, run vr109's hand, found its best inside this one); then 2 mm about the best.
constexpr int kGridX = 14, kGridY = 15, kGridZ = 15, kGrid = kGridX * kGridY * kGridZ, kSlice = 60;
const LightHold& light_hold_of(int h, const Quat& rel) {
    LightHold& lh = light_hold[h];
    const float mx = h == kLeft ? -1.0f : 1.0f;
    const float same = std::fabs(lh.rel.x * rel.x + lh.rel.y * rel.y + lh.rel.z * rel.z + lh.rel.w * rel.w);
    const bool fits = lh.her == her_hands[h].valid && same > 0.99966f;  // the same hand, the aim within 3 degrees
    if (lh.known && fits) return lh;
    if (!lh.searching || !fits) {
        lh.searching = true, lh.known = false, lh.her = her_hands[h].valid, lh.rel = rel;
        lh.next = 0, lh.tried = 0, lh.best = -1;
        lh.best_c = lh.c = Vec3{-0.012f * mx, -0.02f, 0.0f};  // the offline search's place, until this one ends
        lh.best_lens = lh.lens = lh.c + light_axes(rel).z * kBoxHalf[2];  // its face's middle
        for (int& w : lh.why) w = 0;
    }
    const BoxAxes a = light_axes(rel);
    float cs[5], clear = 0, palm = 0, wrap = 0;
    Vec3 lens;
    auto at = [&](Vec3 c) {
        lh.tried++;
        hold_why = 0;
        const float s = hold_score(h, c, a, cs, clear, palm, wrap, lens);
        lh.why[hold_why & 7]++;
        if (s > lh.best) {
            lh.best = s, lh.best_c = c, lh.best_lens = lens;
            lh.best_clear = clear, lh.best_palm = palm, lh.best_wrap = wrap;
            for (int f = 0; f < 5; f++) lh.best_cs[f] = cs[f];
        }
    };
    for (int n = 0; n < kSlice && lh.next < kGrid; n++, lh.next++) {
        const int ix = lh.next / (kGridY * kGridZ), iy = lh.next / kGridZ % kGridY, iz = lh.next % kGridZ;
        at(Vec3{(-0.040f + 0.004f * ix) * mx, -0.048f + 0.004f * iy, -0.032f + 0.004f * iz});
    }
    if (lh.next < kGrid) return lh;
    if (lh.best >= 0) {
        const Vec3 c0 = lh.best_c;
        for (int ix = -1; ix <= 1; ix++)
            for (int iy = -1; iy <= 1; iy++)
                for (int iz = -1; iz <= 1; iz++)
                    if (ix || iy || iz) at(c0 + Vec3{0.002f * ix, 0.002f * iy, 0.002f * iz});
    }
    lh.searching = false, lh.known = true, lh.c = lh.best_c, lh.lens = lh.best_lens;
    const Vec3 fwd = rotate(rel, Vec3{0, 0, -1});
    const float tilt = std::acos(std::clamp(-fwd.z, -1.0f, 1.0f)) * 180.0f / kPi;
    if (lh.best >= 0)
        rt_log("VR: the flashlight's place in the %s fist (%s fingers, the aim %.0f degrees from the grip): %.1f %.1f "
               "%.1f cm; the fingers meet it at %.2f %.2f %.2f %.2f %.2f (thumb to little finger), her palm %.0f%% "
               "against it, %.0f%% of her finger joints along it, the lens %.0f%% clear, the beam from its clear part "
               "%.1f cm up and %.1f cm across from its middle (%d places tried)",
               h == kLeft ? "left" : "right", lh.her ? "her own" : "the average", tilt, lh.c.x * 100, lh.c.y * 100,
               lh.c.z * 100, lh.best_cs[0], lh.best_cs[1], lh.best_cs[2], lh.best_cs[3], lh.best_cs[4],
               lh.best_palm * 100, lh.best_wrap * 100, lh.best_clear * 100,
               dot(lh.lens - lh.c, a.x) * 100, dot(lh.lens - lh.c, a.y) * 100, lh.tried);
    else
        rt_log("VR: the flashlight's place in the %s fist: no place on the grid holds it (the aim %.0f degrees from the "
               "grip, %d places tried); the usual place %.1f %.1f %.1f cm",
               h == kLeft ? "left" : "right", tilt, lh.tried, lh.c.x * 100, lh.c.y * 100, lh.c.z * 100);
    rt_log("VR: the %s hand's flashlight search: places failing on a knuckle in it %d, her palm in it %d, a finger "
           "starting in it %d, a finger's curl %d, the thumb %d, the lens %d, her palm off it %d (the box: up %.2f %.2f "
           "%.2f, the lens %.2f %.2f %.2f in the grip)",
           h == kLeft ? "left" : "right", lh.why[1], lh.why[2], lh.why[3], lh.why[4], lh.why[5], lh.why[6], lh.why[7],
           a.x.x, a.x.y, a.x.z, a.z.x, a.z.y, a.z.z);
    return lh;
}

// The weapon against the world: its line's probes (the controller's line, palm to tip, a frame late) as the arm's
// are read: the first sample behind a surface (by less than 30 cm) after free or unknown ones is where the line meets
// it; a sample behind the drawn weapon or her own hand tells nothing, and with the line's end so hidden the last
// contact is kept (a weapon held at a wall covers the wall where its line goes on).
// 0.1.7 (owner 123): a contact starts only when two answers in a row show it, so one answer's stray sample never holds
// the weapon back or knocks it on nothing; what the line's samples said when it started is kept for the log.
int weapon_hit_runs = 0, weapon_hit_serial = -1;
char weapon_hit_line[256] = "";
void weapon_contact() {
    if (!answer.valid || answer.age > 3 || !answer.weapon_asked) {
        if (!answer.valid || answer.age > 3) weapon_free = 1.0f, weapon_hit_runs = 0;
        return;
    }
    float t_hit = 1.0f;
    bool hit = false, end_hidden = false, any = false;
    int last_free = -1;
    for (int i = 0; i < kWeaponSamples; i++) {
        if (!answer.weapon_ok[i]) continue;
        any = true;
        if (answer.weapon_hidden[i]) {
            end_hidden = true;
            continue;
        }
        end_hidden = false;
        const float b = answer.weapon_pen[i];
        if (b <= 0) {
            last_free = i;
            continue;
        }
        if (b >= 0.3f) {  // far behind something in front of it: not a wall at the weapon
            last_free = -1;
            continue;
        }
        const float t1 = (i + 1) / (float)kWeaponSamples;
        if (last_free >= 0 && last_free == i - 1) {
            const float a = answer.weapon_pen[last_free], t0 = (last_free + 1) / (float)kWeaponSamples;
            t_hit = t0 + (t1 - t0) * (a / (a - b));
        } else t_hit = t1 - 0.5f / kWeaponSamples;
        hit = true;
        break;
    }
    if (answer.serial != weapon_hit_serial) {
        weapon_hit_serial = answer.serial;
        weapon_hit_runs = hit ? weapon_hit_runs + 1 : 0;
    }
    const bool in_contact = weapon_free < 1.0f;
    if (hit && (in_contact || weapon_hit_runs >= 2)) {
        if (!in_contact) {
            int n = std::snprintf(weapon_hit_line, sizeof weapon_hit_line, "its line met it %.0f%% along; the line's "
                                  "probes, palm to tip, cm behind the surface (her: on her or the weapon):", t_hit * 100.0f);
            for (int i = 0; i < kWeaponSamples && n < (int)sizeof weapon_hit_line - 8; i++)
                n += !answer.weapon_ok[i]   ? std::snprintf(weapon_hit_line + n, sizeof weapon_hit_line - n, " off")
                     : answer.weapon_her[i] ? std::snprintf(weapon_hit_line + n, sizeof weapon_hit_line - n, " her")
                                            : std::snprintf(weapon_hit_line + n, sizeof weapon_hit_line - n, " %.0f",
                                                            answer.weapon_pen[i] * 100.0f);
        }
        weapon_free = std::clamp(t_hit - 0.02f, 0.0f, 1.0f);
    } else if (!(any && end_hidden && in_contact)) weapon_free = 1.0f;
}

// ---------------------------------------------------------------- the surfaces at each hand (session 8)

// The owner (session 8): "im not seeing the fingers/hands properly bending to the walls ... i want it similar to half
// life alyx/our other ports". Jak's and Ratchet's hands (jak_vr.cpp pose_arm) are held by what they are at, as planes:
// the palm rests on them, and each finger is tried against them, bending back, lying flat or curling on until it rests
// on them, never through. Here the planes come from the depth: the surface the eyes see round the hand (kSurf's rings,
// past the outline of her own hand, and the fingertips' probes) is fitted with a plane, its farthest points dropped
// until the rest lie on it within kFitRms; what is left over makes a second plane when it lies on one (an edge: a
// table's top and front, a backrest's top; or a corner: the floor and a wall). Kept in the world a few frames (the
// probes answer a frame or two late), eased into the last fit when near it.
// Where a plane was seen (vr128: the bench's seat seen from above, its front face hidden by its top, fitted as one plane
// out to the world's end, so fingers out over the seat's edge lay flat in the air and a grip there could not curl over
// it): its fitted points, and their middle. A finger's point counts it only over them (kSupportMargin past the
// farthest that way), so past a table's edge the fingers curl down over it; the palm counts the whole plane (her own
// hand hides the surface under it).
constexpr int kSupportMost = 24;
constexpr float kSupportMargin = 0.03f;
struct Plane {
    Vec3 at, n;  // a point on it, and its normal out of the surface (toward the eyes)
    Vec3 sup[kSupportMost];
    int nsup = 0;
    Vec3 mid{0, 0, 0};
};
struct Solids {
    Plane p[2];
    int n = 0;
    bool convex = false;  // two planes at an edge (the solid behind both); else each a surface of its own
    bool implied = false; // its second plane (an edge's front face) not seen but told by where the first one ends
    float rms = 0;       // metres: how near its points lie
    int points = 0;
};
constexpr float kFitRms = 0.008f;   // metres: the points left on a plane lie this near it
constexpr float kFitOn = 0.015f;    // a point this near the first plane is on it (the rest may make a second)
constexpr int kPlaneKeep = 6;       // frames a plane is kept with no new fit
constexpr float kPalmGap = 0.004f;  // metres: her palm's skin kept this far off a surface
constexpr float kPushMost = 0.45f;  // a surface the palm would be moved farther than this out of is not the one at it
// The fingers (Jak's, jak_vr.cpp): curls a second, out of a surface fast and on to one slower (a probe that misses a
// frame never flicks them); the steps tried; how far past the controls' curl a finger over a surface the palm faces
// curls on to rest on it; half a finger's thickness, kept off the surface.
constexpr float kFingerStops = 18.0f, kFingerFollows = 7.0f;
constexpr float kFingerFine = 0.02f, kFingerWrapMost = 0.70f, kFingerThick = 0.009f;

struct KeptPlanes {
    Solids w;      // the world's (game units)
    int age = 99;  // frames since it was fitted
} kept[2];
Solids hand_at[2];       // this frame's (tracking)
float finger_off[2][5];  // each finger off the controls' curl (the world's doing)
bool palm_faces[2];      // the palm faced a surface last frame (taken within 60 degrees, let go past 70)
bool solved_was[2];      // the fingers were the world's last frame (else they start from where they are drawn)
int answer_seen = -1;

float side_of(const Plane& p, Vec3 q) { return dot(p.n, q - p.at); }

// The plane through pts (least squares; its normal toward `eye`) and how near they lie (rms, metres); false for points
// along a line or bunched (under about 1.5 cm across).
bool fit_plane(const Vec3* pts, int n, Vec3 eye, Plane& out, float& rms) {
    if (n < 3) return false;
    Vec3 c{0, 0, 0};
    for (int i = 0; i < n; i++) c = c + pts[i];
    c = c * (1.0f / n);
    float xx = 0, xy = 0, xz = 0, yy = 0, yz = 0, zz = 0;
    for (int i = 0; i < n; i++) {
        const Vec3 r = pts[i] - c;
        xx += r.x * r.x, xy += r.x * r.y, xz += r.x * r.z;
        yy += r.y * r.y, yz += r.y * r.z, zz += r.z * r.z;
    }
    const float dx = yy * zz - yz * yz, dy = xx * zz - xz * xz, dz = xx * yy - xy * xy;
    const float most = std::max(dx, std::max(dy, dz));
    if (!(most > 5e-8f * n * n)) return false;
    Vec3 nn;
    if (most == dx) nn = {dx, xz * yz - xy * zz, xy * yz - xz * yy};
    else if (most == dy) nn = {xz * yz - xy * zz, dy, xy * xz - yz * xx};
    else nn = {xy * yz - xz * yy, xy * xz - yz * xx, dz};
    nn = normalized(nn);
    if (dot(nn, eye - c) < 0) nn = nn * -1.0f;
    out = Plane{};
    out.at = c, out.n = nn;
    float e = 0;
    for (int i = 0; i < n; i++) {
        const float d = side_of(out, pts[i]);
        e += d * d;
    }
    rms = std::sqrt(e / n);
    return std::isfinite(rms);
}

// The plane most of pts lie on: the farthest dropped one by one until the rest are within kFitRms (pts reordered, the
// ones kept first).
bool fit_most(Vec3* pts, int n, int least, Vec3 eye, Plane& out, float& rms, int& used) {
    while (n >= least) {
        if (!fit_plane(pts, n, eye, out, rms)) return false;
        if (rms <= kFitRms) {
            used = n;
            return true;
        }
        int worst = 0;
        float farthest = -1;
        for (int i = 0; i < n; i++) {
            const float d = std::fabs(side_of(out, pts[i]));
            if (d > farthest) farthest = d, worst = i;
        }
        std::swap(pts[worst], pts[n - 1]);
        n--;
    }
    return false;
}

// A plane's seen part: its first `used` points (fit_most keeps them first) and their middle.
void set_support(Plane& p, const Vec3* pts, int used) {
    p.nsup = 0;
    Vec3 m{0, 0, 0};
    for (int i = 0; i < used && p.nsup < kSupportMost; i++) m = m + (p.sup[p.nsup++] = pts[i]);
    p.mid = p.nsup > 0 ? m * (1.0f / p.nsup) : p.at;
}

// Session 9: an edge seen from above with its front face hidden (vr132: the bench's seat, its front face edge-on to
// the eyes, so the fingers out past the edge lay flat in the air with nothing to curl down against). Where a probe's
// ray went past plane a to a surface well behind it (the floor) or to nothing, it crossed the plane where the plane is
// not (it would have been seen there). When every such crossing lies out one way from the points a was fitted to, and
// none among them (not a grating or the gaps between slats), a ends between the two: an edge, its front face square
// to a there (an implied plane: no points of its own, so counted everywhere; a itself is counted over its points).
constexpr float kHoleBehind = 0.03f;  // metres: a ray's surface this far behind the plane went past it
constexpr int kHoleLeast = 3;         // crossings needed
// The last try at each hand, for the planes dump (SH3_SURF_DUMP): how many crossings, how much one way (1: all the
// same way), the seen points' and the crossings' reach that way (cm from the seen part's middle), and why not.
struct ImpliedTry {
    int crossings = 0;
    float oneway = 0, reach = 0, near = 0;
    const char* why = "";
} implied_try[2];
bool implied_face(int h, const Plane& a, const Vec3* pts, int n, Vec3 eye, Plane& out) {
    ImpliedTry& tr = implied_try[h];
    tr = ImpliedTry{};
    const float e = side_of(a, eye);
    if (a.nsup < 5 || e < 0.05f) {
        tr.why = a.nsup < 5 ? "under 5 seen points" : "the eyes nearly in its plane";
        return false;
    }
    Vec3 holes[2 * kSurfMost];
    int nh = 0;
    auto add = [&](Vec3 q) {
        const float s = side_of(a, q);
        if (s > -kHoleBehind || nh >= 2 * kSurfMost) return;
        holes[nh++] = eye + (q - eye) * (e / (e - s));
    };
    for (int i = 0; i < n; i++) add(pts[i]);
    const int nb = std::min(answer.beyond_n[h], kSurfMost);
    for (int i = 0; i < nb; i++) add(tracking_point(answer.beyond[h][i]));
    tr.crossings = nh;
    if (nh < kHoleLeast) {
        tr.why = "under 3 rays went past it";
        return false;
    }
    // The way out: the direction along a (from its seen part's middle) the most crossings share, each counted once (a
    // far crossing, a ray near edge-on to a, no more than a near one): the crossing with the most others within 45
    // degrees, then the mean of those within 60 (vr134: a hand on the seat's corner had crossings past its front edge
    // and past its near end, the near end's behind the middle along the front's way: the corner's other edge is left
    // out, its main one kept).
    Vec3 dir[2 * kSurfMost];
    for (int i = 0; i < nh; i++) {
        Vec3 d = holes[i] - a.mid;
        d = d - a.n * dot(d, a.n);
        const float len = length(d);
        dir[i] = len > 1e-3f ? d * (1.0f / len) : Vec3{0, 0, 0};
    }
    int best = -1, best_n = 0;
    for (int i = 0; i < nh; i++) {
        int c = 0;
        for (int j = 0; j < nh; j++) c += dot(dir[i], dir[j]) > 0.707f;
        if (c > best_n) best_n = c, best = i;
    }
    Vec3 u{0, 0, 0};
    int used = 0;
    if (best >= 0)
        for (int j = 0; j < nh; j++)
            if (dot(dir[best], dir[j]) > 0.5f) u = u + dir[j], used++;
    const float ulen = length(u);
    tr.oneway = (float)used / nh;
    if (best_n < kHoleLeast || ulen < 1e-3f) {
        tr.why = "under 3 rays past it one way";
        return false;
    }
    u = u * (1.0f / ulen);
    float reach = -1e9f, near = 1e9f;
    for (int i = 0; i < a.nsup; i++) reach = std::max(reach, dot(a.sup[i] - a.mid, u));
    for (int i = 0; i < nh; i++)
        if (dot(dir[i], u) > 0.5f) near = std::min(near, dot(holes[i] - a.mid, u));
    tr.reach = reach * 100.0f, tr.near = near * 100.0f;
    if (near < reach - 0.01f) {  // a crossing among the points: a gap, not an end
        tr.why = "a ray past it among its points";
        return false;
    }
    tr.why = "found";
    const float edge = std::min((reach + near) * 0.5f, reach + 0.03f);
    out = Plane{};
    out.at = out.mid = a.mid + u * edge;
    out.n = u;
    return true;
}

// The planes at hand h from the last answer's points (tracking).
bool fit_solids(int h, Solids& out) {
    out = Solids{};
    const int n = std::min(answer.surf_n[h], kSurfMost);
    Vec3 pts[kSurfMost];
    for (int i = 0; i < n; i++) pts[i] = tracking_point(answer.surf[h][i]);
    const Vec3 eye = fi.head.position;
    Plane a;
    float rms = 0;
    int used = 0;
    if (!fit_most(pts, n, 5, eye, a, rms, used)) {
        implied_try[h] = ImpliedTry{};
        implied_try[h].why = "no plane fitted";
        return false;
    }
    set_support(a, pts, used);
    out.p[0] = a, out.n = 1, out.rms = rms, out.points = used;
    Vec3 rest[kSurfMost];
    int m = 0;
    for (int i = 0; i < n; i++)
        if (std::fabs(side_of(a, pts[i])) > kFitOn) rest[m++] = pts[i];
    Plane b;
    if (m >= 4 && fit_most(rest, m, 4, eye, b, rms, used) && dot(a.n, b.n) < 0.87f) {
        set_support(b, rest, used);
        // An edge when each face lies behind the other's plane (a table's top and front), a corner when in front.
        const float ab = side_of(a, b.at), ba = side_of(b, a.at);
        if ((ab < -0.01f && ba < -0.01f) || (ab > 0.01f && ba > 0.01f)) {
            out.p[1] = b, out.n = 2, out.convex = ab < 0;
            out.points += used;
        }
    }
    if (out.n == 1) {
        Plane face;
        if (implied_face(h, a, pts, n, eye, face)) out.p[1] = face, out.n = 2, out.convex = out.implied = true;
    } else {
        implied_try[h] = ImpliedTry{};
        implied_try[h].why = "two planes seen";
    }
    return true;
}

Plane plane_world(const Plane& t) {
    Plane w = t;
    w.at = world_point(t.at);
    w.n = normalized(world_point(t.at + t.n * 0.1f) - w.at);
    for (int i = 0; i < t.nsup; i++) w.sup[i] = world_point(t.sup[i]);
    w.mid = world_point(t.mid);
    return w;
}
Plane plane_tracking(const Plane& w) {
    Plane t = w;
    t.at = tracking_point(w.at);
    t.n = normalized(tracking_point(w.at + w.n * (0.1f * kUnitsPerMetre)) - t.at);
    for (int i = 0; i < w.nsup; i++) t.sup[i] = tracking_point(w.sup[i]);
    t.mid = tracking_point(w.mid);
    return t;
}

// Whether q lies over the part of plane p the depth showed: along the plane, no farther from its points' middle than
// the farthest of them that way, and kSupportMargin.
bool supported(const Plane& p, Vec3 q) {
    if (p.nsup < 3) return true;
    Vec3 u = q - p.mid;
    u = u - p.n * dot(u, p.n);
    const float len = length(u);
    if (len < 1e-4f) return true;
    u = u * (1.0f / len);
    float reach = 0;
    for (int i = 0; i < p.nsup; i++) reach = std::max(reach, dot(p.sup[i] - p.mid, u));
    return len <= reach + kSupportMargin;
}

// The planes hand h is at this frame (tracking): fitted again from a new answer and eased into the last fit when that
// was near it (within 25 degrees and 3 cm), else taken; kept kPlaneKeep frames with no new fit.
Solids hand_solids(int h, bool fresh) {
    KeptPlanes& k = kept[h];
    k.age++;
    Solids t;
    if (fresh && fit_solids(h, t)) {
        Solids w = t;
        for (int i = 0; i < t.n; i++) w.p[i] = plane_world(t.p[i]);
        if (k.age <= kPlaneKeep && k.w.n == w.n && k.w.convex == w.convex) {
            bool close = true;
            for (int i = 0; i < w.n; i++)
                close = close && dot(k.w.p[i].n, w.p[i].n) > 0.9f &&
                       std::fabs(side_of(k.w.p[i], w.p[i].at)) < 0.03f * kUnitsPerMetre;
            if (close)
                for (int i = 0; i < w.n; i++) {
                    w.p[i].n = normalized(lerp(k.w.p[i].n, w.p[i].n, 0.4f));
                    w.p[i].at = lerp(k.w.p[i].at, w.p[i].at, 0.4f);
                }
        }
        k.w = w;
        k.age = 0;
    }
    Solids out;
    if (k.age > kPlaneKeep) return out;
    out = k.w;
    for (int i = 0; i < out.n; i++) out.p[i] = plane_tracking(k.w.p[i]);
    return out;
}

int solid_count(const Solids& so) { return so.convex ? 1 : so.n; }
// How far q is in front of solid k's surface (metres; below 0 in it): an edge's solid is behind both its planes.
float solid_clear(const Solids& so, int k, Vec3 q) {
    return so.convex ? std::max(side_of(so.p[0], q), side_of(so.p[1], q)) : side_of(so.p[k], q);
}
// The same for a finger's point, each plane counted only over its seen part (supported): past a table's edge, out of
// its top's reach, a finger is free to curl down (in front of its front face where that was seen).
float finger_clear(const Solids& so, int k, Vec3 q) {
    if (!so.convex) return supported(so.p[k], q) ? side_of(so.p[k], q) : 1.0f;
    // An edge whose front face is implied (session 9) is the whole wedge, under its top and behind its face, within
    // 15 cm of the edge or over the top's seen part (vr133: judged by the implied face alone past the top's seen part,
    // fingers lying on the seat counted as in it and lifted the palm 49 mm off the seat).
    if (so.implied) {
        const float a = side_of(so.p[0], q), b = side_of(so.p[1], q);
        return supported(so.p[0], q) || b > -0.15f ? std::max(a, b) : 1.0f;
    }
    float c = -1e9f;
    bool any = false;
    for (int i = 0; i < 2; i++)
        if (supported(so.p[i], q)) c = std::max(c, side_of(so.p[i], q)), any = true;
    return any ? c : 1.0f;
}

// The palm's skin in hand h's grip space: its middle from each finger's knuckle to the heel of the hand, on the palm's
// face and on the back of the hand, and the thumb's root: what rests on a surface.
int palm_points(int h, Vec3 out[18]) {
    const float mx = h == kLeft ? -1.0f : 1.0f;
    const Vec3 face{-mx * 0.013f, 0, 0};  // out of the palm
    Vec3 p[4];
    finger_points(h, kMiddle, 0, p);
    const Vec3 along = normalized(p[3] - p[0]);
    int n = 0;
    for (int f = kIndex; f <= kPinky; f++) {
        finger_points(h, f, 0, p);
        const Vec3 heel = p[0] - along * 0.075f;
        out[n++] = p[0] + face, out[n++] = p[0] - face, out[n++] = heel + face, out[n++] = heel - face;
    }
    finger_points(h, kThumb, 0, p);
    out[n++] = p[0] + face, out[n++] = p[0] - face;
    return n;
}

// How far hand h moves (tracking) to rest on the surfaces `so` instead of in them, the controller at `pose`: out along
// their normals (a hand pressed on a wall slides along it with the controller). A surface the shoulder is behind is
// not one the arm went through; an edge moves the hand out of the face it needs least (kept while near).
// What rests on it: the palm, and with `want` (a free hand: the controls' curls) each finger at the curl, from how the
// player holds it to bent back as far as it gives (half a curl, kFingerBackMost at most), that keeps it out of the
// surface best (Half-Life: Alyx's hand; vr124: with the palm alone held at a wall, fingers pointed at it were folded
// into a fist to get out of it; vr125: held by each finger bent back by a fixed share, a thumb bent back into the floor
// held the hand 5 cm over it and a hand laid flat stood 2.5 cm up on its fingertips). So a hand poked at a wall stops
// at its fingertips, bent back against it; laid flat on it, its palm rests there with the fingers flat; a fist stops
// at its knuckles. body_fingers then draws each finger at the free curl nearest the player's.
constexpr int kPushCurls = 6;
Vec3 palm_push(int h, const Solids& so, const Pose& pose, float* clear_out = nullptr, const float* want = nullptr) {
    Vec3 pts[18];
    const int n = palm_points(h, pts);
    for (int i = 0; i < n; i++) pts[i] = grip_point(pose, pts[i]);
    // each finger's knuckle, and its joints and tip at kPushCurls curls from the controls' to the most bent back
    const int nf = want ? 5 : 0;
    Vec3 root[5], fp[5][kPushCurls][3];
    for (int f = 0; f < nf; f++) {
        const float w = std::isfinite(want[f]) ? std::clamp(want[f], 0.0f, 1.0f) : 0.0f;
        const float lo = std::max(kFingerBackMost, w - 0.5f);
        for (int c = 0; c < kPushCurls; c++) {
            Vec3 p[4];
            finger_points(h, f, w + (lo - w) * c / (kPushCurls - 1), p);
            root[f] = grip_point(pose, p[0]);
            for (int j = 0; j < 3; j++) fp[f][c][j] = grip_point(pose, p[j + 1]);
        }
    }
    const float finger_gap = kFingerThick + 0.002f;  // a finger's skin kept 2 mm off it
    const Vec3 sh = shoulder(h);
    static int face[2];
    Vec3 push{0, 0, 0};
    for (int pass = 0; pass < 2; pass++)
        for (int k = 0; k < solid_count(so); k++) {
            if (solid_clear(so, k, sh) < 0) continue;
            if (so.convex) {
                float need[2] = {0, 0};
                bool in = false;
                for (int i = 0; i < n; i++) {
                    const Vec3 q = pts[i] + push;
                    const float a = side_of(so.p[0], q), b = side_of(so.p[1], q);
                    if (a < kPalmGap && b < kPalmGap) {
                        in = true;
                        need[0] = std::max(need[0], kPalmGap - a), need[1] = std::max(need[1], kPalmGap - b);
                    }
                }
                for (int f = 0; f < nf; f++) {
                    if (solid_clear(so, k, root[f] + push) < 0) continue;
                    float best[2] = {1e9f, 1e9f};  // the least move out of each face that frees it at some curl
                    for (int c = 0; c < kPushCurls; c++) {
                        float nc[2] = {0, 0};
                        for (int j = 0; j < 3; j++) {
                            // (each face counted where it was seen; out of either face, the same move)
                            const float cl = finger_clear(so, 0, fp[f][c][j] + push);
                            if (cl < finger_gap)
                                nc[0] = std::max(nc[0], finger_gap - cl), nc[1] = std::max(nc[1], finger_gap - cl);
                        }
                        best[0] = std::min(best[0], nc[0]), best[1] = std::min(best[1], nc[1]);
                    }
                    if (best[0] > 0 && best[1] > 0) {
                        in = true;
                        need[0] = std::max(need[0], best[0]), need[1] = std::max(need[1], best[1]);
                    }
                }
                if (!in) continue;
                int j = face[h] & 1;
                if (need[1 - j] + 0.03f < need[j]) j = 1 - j;
                face[h] = j;
                if (need[j] < kPushMost) push = push + so.p[j].n * need[j];
            } else {
                float need = 0;
                for (int i = 0; i < n; i++) need = std::max(need, kPalmGap - side_of(so.p[k], pts[i] + push));
                for (int f = 0; f < nf; f++) {
                    if (side_of(so.p[k], root[f] + push) < 0) continue;
                    float best = 1e9f;
                    for (int c = 0; c < kPushCurls; c++) {
                        float nc = 0;
                        for (int j = 0; j < 3; j++) nc = std::max(nc, finger_gap - finger_clear(so, k, fp[f][c][j] + push));
                        best = std::min(best, nc);
                    }
                    need = std::max(need, best);
                }
                if (need > 0 && need < kPushMost) push = push + so.p[k].n * need;
            }
        }
    if (clear_out) {
        // how near it is: the palm's skin, or a finger's (at its best curl) less the gap the palm keeps
        float least = 1.0f;
        for (int k = 0; k < solid_count(so); k++) {
            for (int i = 0; i < n; i++) least = std::min(least, solid_clear(so, k, pts[i] + push));
            for (int f = 0; f < nf; f++) {
                if (solid_clear(so, k, root[f] + push) < 0) continue;
                float best = -1e9f;
                for (int c = 0; c < kPushCurls; c++) {
                    float m = 1e9f;
                    for (int j = 0; j < 3; j++) m = std::min(m, finger_clear(so, k, fp[f][c][j] + push));
                    best = std::max(best, m);
                }
                least = std::min(least, best - finger_gap + kPalmGap);
            }
        }
        *clear_out = least;
    }
    return push;
}

// Whether hand h's finger f at curl c, the hand at `pose`, meets a surface of `so`: its joints and tip within
// kFingerThick of one its knuckle is in front of (jak_vr.cpp's meets), each plane where it was seen.
bool finger_meets(int h, int f, float c, const Pose& pose, const Solids& so) {
    Vec3 p[4];
    finger_points(h, f, c, p);
    const Vec3 root = grip_point(pose, p[0]);
    for (int k = 0; k < solid_count(so); k++) {
        if (solid_clear(so, k, root) < 0) continue;
        for (int j = 1; j < 4; j++)
            if (finger_clear(so, k, grip_point(pose, p[j])) < kFingerThick) return true;
    }
    return false;
}

// The nearest any finger's joint or tip of hand h comes to the surfaces (metres, below 0 in them), drawn at `curl`.
float fingers_clear(int h, const Solids& so, const Pose& pose, const float curl[5]) {
    float least = 1.0f;
    for (int f = 0; f < 5; f++) {
        Vec3 p[4];
        finger_points(h, f, curl[f], p);
        const Vec3 root = grip_point(pose, p[0]);
        for (int k = 0; k < solid_count(so); k++) {
            if (solid_clear(so, k, root) < 0) continue;
            for (int j = 1; j < 4; j++) least = std::min(least, finger_clear(so, k, grip_point(pose, p[j])));
        }
    }
    return least;
}

// Finger f of hand h against the surfaces it is at (Jak's and Ratchet's touch_fingers): from the curl it is drawn at
// toward the one the controls ask (`want`, no more than `cap`: the fingertips' probes and the weapon), stopped where
// it meets a surface; over a surface the palm faces (`wrap`) a finger not held out on purpose curls on until it rests
// on it (kFingerWrapMost at most); found in one (the hand moved), the nearest free curl either way, bending back to
// kFingerBackMost or curling on. Out of a surface at kFingerStops a second, on to one at kFingerFollows.
float world_finger(int h, int f, float want, float cap, const Pose& pose, const Solids& so, bool wrap) {
    float& off = finger_off[h][f];
    if (!std::isfinite(off)) off = 0;
    const float cur = std::clamp(want + off, kFingerBackMost, 1.0f);
    float goal = std::min(want, cap);
    bool in_it = false;
    float to = cur;
    if (so.n > 0) {
        if (wrap) {
            const float top = std::min(std::min(want + kFingerWrapMost, cap), 1.0f);
            for (float c = want + kFingerFine; c <= top + 1e-3f; c += kFingerFine)
                if (finger_meets(h, f, c, pose, so)) {
                    goal = std::max(goal, c - kFingerFine);
                    break;
                }
        }
        in_it = finger_meets(h, f, cur, pose, so);
        if (in_it) {
            const float first = goal >= cur ? 1.0f : -1.0f;
            bool found = false;
            for (int step = 1; step <= 70 && !found; step++) {
                const float d = kFingerFine * step;
                const float tries[2] = {cur + first * d, cur - first * d};
                for (float c : tries)
                    if (!found && c >= kFingerBackMost && c <= 1.0f && !finger_meets(h, f, c, pose, so)) {
                        to = c;
                        found = true;
                    }
            }
        } else {
            const float way = goal >= cur ? 1.0f : -1.0f;
            for (int step = 0; step < 70 && to != goal; step++) {
                const float next = std::fabs(goal - to) <= kFingerFine ? goal : to + way * kFingerFine;
                if (finger_meets(h, f, next, pose, so)) break;
                to = next;
            }
        }
    } else {
        to = goal;
    }
    const float wanted = to - want;
    const float rate = (in_it || wanted < off ? kFingerStops : kFingerFollows) * frame_dt();
    off += std::clamp(wanted - off, -rate, rate);
    return std::clamp(want + off, kFingerBackMost, 1.0f);
}

}  // namespace

// LIST 49: her watch on the back of hand h's wrist (grip space, metres; her model wears it on the left, vr136's
// pictures): its face's middle 4.1 cm up her forearm from the wrist joint and 2.4 cm out of the back of the wrist, the
// way along her forearm to her hand, and out of the watch's face (unit, square to `along`). From her forearm as drawn
// (ik_forearm: the elbow, the wrist joint and the back of the wrist as the twist bone turns it), kept from the last frame
// it was known. Runs vr140-141 took the way of her straight fingers for the forearm's, but her model's fingers bend 42
// degrees toward the palm at rest: the bar's point sat 12.7 cm off her wrist, and the eyes, on her watch, were never on
// it. vr142 (2.5 cm up, out of the back of the hand): the watch's face 2.2 cm further up and 45 degrees round, as the
// twist bone takes half the hand's roll (now 0.8 of it: vr144's bar faced the eyes 0.72, from 0.33).
bool watch_place(int h, Vec3& at, Vec3& along, Vec3& back) {
    static Vec3 k_at[2], k_along[2], k_back[2];
    static bool known[2] = {false, false};
    if (h < 0 || h > 1) return false;
    Vec3 e, w, out_w;
    if (map_ready() && hands[h].valid && ik_forearm(h, e, w, &out_w)) {
        const Pose& g = hands[h].shown;
        const Quat inv = conjugate(g.orientation);
        const Vec3 wrist = rotate(inv, tracking_point(w) - g.position);
        const Vec3 arm = wrist - rotate(inv, tracking_point(e) - g.position);
        if (length(arm) > 0.1f) {
            const float mx = h == kLeft ? -1.0f : 1.0f;
            const Vec3 a = normalized(arm);
            Vec3 b = length(out_w) > 0.5f ? rotate(inv, tracking_dir(out_w)) : Vec3{mx, 0, 0};
            b = b - a * dot(b, a);
            if (length(b) < 0.3f) b = Vec3{mx, 0, 0} - a * a.x;
            b = normalized(b);
            // the watch's face sits 25 degrees round from the twist bone's back of the wrist, toward the thumb
            // (vr144: the bar's middle 1.4 cm toward the thumb and 0.5 cm short of the face's middle)
            const Vec3 thumb = cross(a, b) * -mx;
            b = normalized(b * 0.906f + thumb * 0.423f);  // cos, sin 25
            k_along[h] = a, k_back[h] = b;
            k_at[h] = wrist - a * 0.041f + b * 0.024f;
            known[h] = true;
        }
    }
    if (!known[h]) return false;
    at = k_at[h], along = k_along[h], back = k_back[h];
    return true;
}

void body_frame() {
    answer.age++;
    guide_age++;
    const bool fresh = answer.valid && answer.serial != answer_seen;  // a new answer: the planes fitted again
    answer_seen = answer.serial;
    for (int h = 0; h < 2; h++) {
        HandState& s = hands[h];
        const bool was = s.valid;
        s.valid = head_known() && fi.hand_valid[h];
        if (!s.valid) {
            s.inside = s.touching = s.resting = false;
            s.push = Vec3{0, 0, 0};
            kept[h].age = 99;
            hand_at[h] = Solids{};
            solved_was[h] = false;
            continue;
        }
        const Vec3 last = s.grip.position;
        s.grip = fi.hand_grip[h];
        s.aim = fi.hand_aim[h];
        const Vec3 raw = was ? (s.grip.position - last) * (1.0f / motion_dt()) : Vec3{};  // 0.1.3: the real time (owner 84)
        s.vel = was ? lerp(s.vel, raw, 0.5f) : Vec3{};
        s.speed = length(s.vel);
        wanted_curls(h, s.want);

        // What the probes said: each finger closes until its tip meets a surface; the arm through a wall. A stop
        // closes at once and lets go eased (a probe a frame late never makes a finger flicker).
        // Session 7 (the owner: the fingers' grip "should go for all world items really. drawers, objects, handles,
        // doors"): six curls probed, not four, and the stop found between the last free one and the first at the
        // surface from their depths, where the finger's skin (not its tip's middle) reaches the surface: a finger
        // stops on a handle, a drawer's edge or an item where it meets it, not at the nearest third of its curl short
        // of it.
        bool touching = false;
        // A hand on a weapon's second grip has no surface hold of its own (session 13, vr196): its place is the
        // weapon's, which the weapon's own probes hold back from walls (sh3vr_combat.cpp place_weapon). Its arm's line,
        // crossing in front of her to a short gun's front, passes behind the gun and her other arm as the eyes see it,
        // and the probes took that for a wall: "the left hand is held at a surface (the arm's line met it)", the hand
        // held by her chest and the two-hand line swinging the submachine gun 90 degrees to the left.
        // (and while it eases off the grip after letting go: vr197, "held at a surface" the frame after the let-go)
        const bool on_weapon = s.on_foregrip || weapon_two_share(h) > 0.001f;
        if (answer.valid && answer.age <= 3 && !on_weapon) {
            constexpr float kSkin = 0.0085f;  // her finger's half thickness
            for (int f = 0; f < 5; f++) {
                float stop = 1.0f, prev_pen = 0;
                int prev_k = -1;
                bool known = false;
                for (int k = 0; k < kTipSteps; k++) {
                    if (!answer.tip_ok[h][f][k]) {
                        prev_k = -1;
                        continue;
                    }
                    known = true;
                    const float pen = answer.tip_pen[h][f][k];
                    if (pen > -kSkin && pen < 0.06f) {
                        float t = (float)k;
                        if (prev_k == k - 1 && prev_pen <= -kSkin && pen > prev_pen)
                            t = (k - 1) + (-kSkin - prev_pen) / (pen - prev_pen);
                        stop = std::max(0.0f, t / (kTipSteps - 1) - 0.02f);
                        if (k == 0) touching = true;
                        break;
                    }
                    prev_k = k, prev_pen = pen;
                }
                if (!known) stop = 1.0f;  // off the picture: letting go slowly
                if (stop < s.stop[f]) s.stop[f] = stop;
                else s.stop[f] += (stop - s.stop[f]) * ease(known ? 8.0f : 2.0f);
            }
            // The arm's line from the shoulder to the controller: where it first goes behind a surface (its samples
            // in front of everything are free), the hand stays. A sample behind her own drawn arm or hand tells
            // nothing (her hand held at a wall covers the wall behind it): with the controller's end of the line
            // hidden so, a held hand stays held (run vr26: a hand at the floor was never held, the floor beside her
            // hand counted as her and the samples behind her hand as free).
            const bool was_inside = s.inside;
            s.inside = false;
            int last_free = -1;
            bool end_hidden = false;
            for (int i = 0; i < kArmSamples; i++) {
                if (!answer.arm_ok[h][i]) continue;
                if (answer.arm_hidden[h][i]) {
                    end_hidden = true;
                    continue;
                }
                end_hidden = false;
                const float b = answer.arm_pen[h][i];
                if (b <= 0) {
                    last_free = i;
                    continue;
                }
                if (b >= 0.3f || last_free < 0) {  // far behind something in front of it: not a wall at the hand
                    last_free = -1;
                    continue;
                }
                {
                    const Vec3 sh = shoulder(h);
                    const float t0 = (last_free + 1) / (float)kArmSamples, t1 = (i + 1) / (float)kArmSamples;
                    float t = t1;
                    if (last_free == i - 1) {
                        const float a = answer.arm_pen[h][last_free];
                        t = t0 + (t1 - t0) * (a / (a - b));
                    } else t = (t0 + t1) * 0.5f;
                    s.contact = lerp(sh, s.grip.position, std::max(0.0f, t - 0.02f));
                    s.inside = true;
                }
                break;
            }
            if (!s.inside && was_inside && end_hidden) s.inside = true;  // the last contact kept
            {
                // 0.1.7 (owner 123): a new hold only when two answers in a row show the line behind a surface.
                static int arm_runs[2] = {}, arm_serial[2] = {-1, -1};
                if (answer.serial != arm_serial[h]) {
                    arm_serial[h] = answer.serial;
                    arm_runs[h] = s.inside ? arm_runs[h] + 1 : 0;
                }
                if (s.inside && !was_inside && arm_runs[h] < 2) s.inside = false;
            }
            static int dump_at = -2;  // SH3_VSC_DUMP=N (the PC rig): what the arm's probes said in frame N
            if (dump_at == -2) dump_at = getenv("SH3_VSC_DUMP") ? atoi(getenv("SH3_VSC_DUMP")) : -1;
            if (rt_frame == dump_at) {
                char line[400];
                int n = std::snprintf(line, sizeof line, "VR body: %s arm's probes (shoulder to controller, cm behind "
                                      "the surface):", h == kLeft ? "left" : "right");
                for (int i = 0; i < kArmSamples && n < (int)sizeof line - 16; i++)
                    n += !answer.arm_ok[h][i]       ? std::snprintf(line + n, sizeof line - n, " off")
                         : answer.arm_hidden[h][i] ? std::snprintf(line + n, sizeof line - n, " her")
                                                   : std::snprintf(line + n, sizeof line - n, " %.0f",
                                                                   answer.arm_pen[h][i] * 100.0f);
                rt_log("%s; %s, answer %d frames old, %d of her", line, s.inside ? "held" : "free", answer.age,
                       answer.own_hits);
            }
        } else {
            for (float& v : s.stop) v += (1.0f - v) * ease(4.0f);
            s.inside = false;
        }
        // Session 8: the palm on the surfaces it is at (Jak's and Ratchet's hands, Half-Life: Alyx's): where a plane
        // is fitted round the hand the palm rests on it, moved out of it along its normal (a hand pressed on a wall
        // slides along it with the controller, its fingers on it, none in it: body_fingers). It lets go eased, so a
        // fit missing a frame does not drop the hand into the wall and out again. Where none is fitted (a thin rail,
        // a post), the arm's line above holds the hand as before.
        const Solids so = map_ready() && !on_weapon ? hand_solids(h, fresh) : Solids{};
        if (on_weapon) s.push = Vec3{0, 0, 0};
        hand_at[h] = so;
        float palm_clear = 1.0f;
        const bool free_hand = opt.finger_bending && !s.holding_weapon && !s.on_foregrip;
        const Vec3 push = so.n > 0 ? palm_push(h, so, s.grip, &palm_clear, free_hand ? s.want : nullptr) : Vec3{0, 0, 0};
        if (length(push) + 1e-4f >= length(s.push)) s.push = push;
        else s.push = lerp(s.push, push, ease(14.0f));
        {
            // SH3_SURF_DUMP=N[,N2...] (the PC rig; up to 6 frames, session 9): each hand's surface points and planes
            // for three frames from each N: how many points the probes gave, each one's distance off the first plane
            // (mm), what was fitted and the move, and the try at an implied edge.
            static int surf_dump[6], surf_dumps = -1;
            if (surf_dumps < 0) {
                surf_dumps = 0;
                for (const char* p = getenv("SH3_SURF_DUMP"); p && *p && surf_dumps < 6;) {
                    surf_dump[surf_dumps++] = atoi(p);
                    p = std::strchr(p, ',');
                    if (p) p++;
                }
            }
            bool dump_now = false;
            for (int i = 0; i < surf_dumps; i++) dump_now |= rt_frame >= surf_dump[i] && rt_frame < surf_dump[i] + 3;
            if (dump_now && map_ready()) {
                char line[700];
                int n = std::snprintf(line, sizeof line, "VR body: frame %d: the %s hand's surface: %d points (the "
                                      "answer %d frames old); %d plane(s)%s, %d points within %.1f mm, fitted %d frames "
                                      "ago; the hand moved %.1f cm; off the first plane (mm):",
                                      rt_frame, h == kLeft ? "left" : "right", answer.surf_n[h], answer.age, so.n,
                                      so.implied ? " (an edge, its front face implied)" : so.convex ? " (an edge)" : "", so.points, so.rms * 1000.0f, kept[h].age,
                                      length(push) * 100.0f);
                for (int i = 0; i < answer.surf_n[h] && n < (int)sizeof line - 12; i++) {
                    const Vec3 q = tracking_point(answer.surf[h][i]);
                    n += so.n > 0 ? std::snprintf(line + n, sizeof line - n, " %.0f", side_of(so.p[0], q) * 1000.0f)
                                  : std::snprintf(line + n, sizeof line - n, " (%.2f %.2f %.2f)", q.x, q.y, q.z);
                }
                rt_log("%s", line);
                if (so.n > 0)
                    rt_log("VR body: frame %d: the %s hand's first plane: at %.3f %.3f %.3f, normal %.2f %.2f %.2f "
                           "(the room's metres; the eyes at %.2f %.2f %.2f)", rt_frame, h == kLeft ? "left" : "right",
                           so.p[0].at.x, so.p[0].at.y, so.p[0].at.z, so.p[0].n.x, so.p[0].n.y, so.p[0].n.z,
                           fi.head.position.x, fi.head.position.y, fi.head.position.z);
                if (so.n > 1)
                    rt_log("VR body: frame %d: the %s hand's second plane%s: at %.3f %.3f %.3f, normal %.2f %.2f %.2f; "
                           "%d probe(s) saw past the hand to a surface over 12 cm beyond or to nothing",
                           rt_frame, h == kLeft ? "left" : "right", so.implied ? " (implied)" : "", so.p[1].at.x,
                           so.p[1].at.y, so.p[1].at.z, so.p[1].n.x, so.p[1].n.y, so.p[1].n.z, answer.beyond_n[h]);
                else
                    rt_log("VR body: frame %d: the %s hand: %d probe(s) saw past it to a surface over 12 cm beyond or "
                           "to nothing", rt_frame, h == kLeft ? "left" : "right", answer.beyond_n[h]);
                const ImpliedTry& tr = implied_try[h];
                rt_log("VR body: frame %d: the %s hand's implied edge: %s (%d rays crossed its plane where it is not, "
                       "%.2f of them one way; its seen points reach %.1f cm that way, the nearest crossing %.1f cm)",
                       rt_frame, h == kLeft ? "left" : "right", tr.why, tr.crossings, tr.oneway, tr.reach, tr.near);
            }
        }
        const bool was_resting = s.resting;
        s.resting = so.n > 0 && palm_clear < 0.012f;  // its skin within a centimetre of it, or moved out of it
        const bool by_plane = so.n > 0 || length(s.push) > 0.001f;
        if (by_plane) {
            s.inside = length(s.push) > 0.02f;  // the ghost shows where the controller is once it is 2 cm in
            s.contact = s.grip.position + s.push;
        }
        {
            // A hand laid on a surface: a soft touch, heard there and felt (not again within a third of a second of
            // leaving it, so a hand hovering at the surface is not a run of them).
            static int left_at[2] = {-100, -100};
            if (s.resting && !was_resting && rt_frame - left_at[h] > 20) {
                hand_sound(kSoundHandSurface, world_point(s.grip.position + s.push), (int)(s.speed * 100.0f));
                if (opt.haptic_surfaces) buzz(h, std::clamp(0.15f + 0.3f * s.speed, 0.15f, 0.6f), 0.03f, 180.0f);
            }
            if (!s.resting && was_resting) left_at[h] = rt_frame;
        }
        if (opt.haptic_surfaces) {
            // A fingertip meeting a surface ticks; a hand pressed into a wall or the floor buzzes, harder the deeper.
            if (touching && !s.touching) buzz(h, 0.18f, 0.015f, 200.0f);
            if (s.inside) {
                const float depth = length(s.grip.position - s.contact);
                buzz(h, std::clamp(0.06f + depth * 2.0f, 0.06f, 0.5f), frame_dt() * 1.5f, 160.0f);
            }
        }
        s.touching = touching;
        {
            // The log says when a hand is first held back by a wall or the floor and when it is let go (after half a
            // second free, so a hand resting on a surface is one pair of lines), with how far the controller went in.
            static bool held[2] = {};
            static int free_frames[2] = {};
            static float deepest[2] = {};
            const char* name = h == kLeft ? "left" : "right";
            if (s.inside) {
                const float depth = length(s.grip.position - s.contact);
                if (!held[h]) {
                    held[h] = true;
                    deepest[h] = 0;
                    if (so.n > 0) {
                        const Vec3 n0 = so.p[0].n;
                        rt_log("VR: frame %d: the %s hand is held at a surface: %s fitted to %d points within %.1f mm, "
                               "its normal %.2f %.2f %.2f in the room (up is y); the palm moved %.1f cm out of it",
                               rt_frame, name, so.n == 1 ? "a plane" : so.implied ? "an edge (its front face implied)" : so.convex ? "an edge (two faces)" : "a corner (two faces)",
                               so.points, so.rms * 1000.0f, n0.x, n0.y, n0.z, length(s.push) * 100.0f);
                    } else {
                        rt_log("VR: frame %d: the %s hand is held at a surface (the arm's line met it; no plane fitted "
                               "there)", rt_frame, name);
                    }
                }
                deepest[h] = std::max(deepest[h], depth);
                free_frames[h] = 0;
            } else if (held[h] && ++free_frames[h] >= 30) {
                held[h] = false;
                rt_log("VR: frame %d: the %s hand is free again (the controller went %.0f cm past the surface)", rt_frame,
                       name, deepest[h] * 100.0f);
            }
        }
        // The fingers are eased in body_fingers (after combat_frame has posed the hands round the weapon).
        s.shown = s.grip;
        if (by_plane) s.shown.position = s.grip.position + s.push;
        else if (s.inside) s.shown.position = s.contact;
    }
    weapon_contact();
}

#if !defined(__ANDROID__)
// 0.1.10 (owner 172; the rig's pictures): her fingers as drawn against this hand model's at the pose drawn. Each
// segment's vertices (those on its bone alone, sh3vr_combat.cpp combat_hand_finger_mesh) are carried into the grip space
// by the palm's frame (as body_palm_skin) and measured from the model's segment at the joints' curls drawn: how far its
// skin reaches toward the palm (the way the segment turns as its joint closes), toward the back and to either side, at
// most (mm), from where to where along the segment they lie (% of its length), and how far its drawn joint is off the
// model's (mm). The model's skin lies 8.5 mm round each segment (the thumb's 9.5).
int finger_skin_text(int h, const HandState& s, char* out, int n) {
    out[0] = 0;
    const Vec3* v = nullptr;
    const uint8_t* sg = nullptr;
    Vec3 joint[5][3], wrist, knuckle[4], thumb[3];
    const int nv = combat_hand_finger_mesh(h, &v, &sg, joint, wrist, knuckle, thumb);
    if (nv <= 0 || n < 200 || h < 0 || h > 1 || !her_hands[h].valid) return 0;
    Vec3 gi[4], gm[4], gl[4], gt[4];
    finger_points(h, kIndex, 0, gi);
    finger_points(h, kMiddle, 0, gm);
    finger_points(h, kPinky, 0, gl);
    finger_points(h, kThumb, 0, gt);
    auto frame = [](Vec3 m, Vec3 i, Vec3 l, Vec3 t, Vec3 e[3]) {
        e[0] = normalized(l - i);
        const Vec3 u = t - m;
        e[1] = normalized(u - e[0] * dot(u, e[0]));
        e[2] = cross(e[0], e[1]);
    };
    Vec3 ew[3], eg[3];
    frame(knuckle[1], knuckle[0], knuckle[3], thumb[0], ew);
    frame(gm[0], gi[0], gl[0], gt[0], eg);
    const float U = kUnitsPerMetre;
    auto to_grip = [&](Vec3 w) {
        const Vec3 d = w - knuckle[1];
        return gm[0] + (eg[0] * dot(d, ew[0]) + eg[1] * dot(d, ew[1]) + eg[2] * dot(d, ew[2])) * (1.0f / U);
    };
    int at = std::snprintf(out, (size_t)n, "her fingers as drawn off this model's (%d vertices; each segment: vertices, "
                           "mm toward the palm/back/side at most, along it %%, its joint off mm):", nv);
    for (int f = 0; f < 5 && at < n - 120; f++) {
        const float sp = f == kIndex ? index_spread[h] : f == kThumb ? thumb_spread[h] : 0.0f;
        float c3[3];
        for (int j = 0; j < 3; j++) c3[j] = std::clamp(s.curl[f] + finger_joint_off[h][f][j], kFingerBackMost, 1.0f);
        Vec3 p[4];
        finger_points3(h, f, c3, p, sp);
        at += std::snprintf(out + at, (size_t)(n - at), " %c", "TIMRL"[f]);
        for (int j = 0; j < 3; j++) {
            const Vec3 d = p[j + 1] - p[j];
            const float L = length(d);
            if (!(L > 1e-5f)) continue;
            const Vec3 a = d * (1.0f / L);
            // (the palm's way for this segment: where its end goes as its own joint closes a little)
            float cb[3] = {c3[0], c3[1], c3[2]};
            const float e = c3[j] <= 0.98f ? 0.02f : -0.02f;
            cb[j] += e;
            Vec3 q[4];
            finger_points3(h, f, cb, q, sp);
            const Vec3 dq = (q[j + 1] - q[j]) * (e > 0 ? 1.0f : -1.0f);
            const Vec3 P = normalized(dq - a * dot(dq, a)), S = cross(a, P);
            int cnt = 0;
            float pal = -1.0f, back = -1.0f, side = 0, s0 = 1e9f, s1 = -1e9f;
            for (int i = 0; i < nv; i++) {
                if (sg[i] != f * 3 + j) continue;
                const Vec3 g = to_grip(v[i]) - p[j];
                const float t = dot(g, a);
                const Vec3 w = g - a * t;
                cnt++;
                pal = std::max(pal, dot(w, P)), back = std::max(back, -dot(w, P));
                side = std::max(side, std::fabs(dot(w, S)));
                s0 = std::min(s0, t / L), s1 = std::max(s1, t / L);
            }
            const float off = length(to_grip(joint[f][j]) - p[j]);
            if (cnt)
                at += std::snprintf(out + at, (size_t)(n - at), "%s%d %.1f/%.1f/%.1f %.0f..%.0f%% (%.1f)", j ? ", " : " ",
                                    cnt, pal * 1000.0f, back * 1000.0f, side * 1000.0f, s0 * 100.0f, s1 * 100.0f,
                                    off * 1000.0f);
            else
                at += std::snprintf(out + at, (size_t)(n - at), "%s0 (%.1f)", j ? ", " : " ", off * 1000.0f);
            at = std::min(at, n - 1);
        }
    }
    return at;
}

// Testing (SH3_GRIP_SLICES=1, the rig): once for each hand on each weapon, the weapon round hand h at the pose drawn in
// its grip space, in slices 5 mm apart across the hand (z, index to little finger), each a grid 3 mm a cell of x (out of
// the palm's face: the back of the hand at +x for the right hand) by y (along: the knuckles at -47 mm, the wrist +y):
// '#' in the weapon, '+' within 3 mm of it; her drawn vertices there: 'P' her palm (its own bone), 'F' a finger's
// segment (90 percent or more on it), each 'X' when inside the weapon.
void grip_slices(int h, const HandState& s) {
    static const bool on = std::getenv("SH3_GRIP_SLICES") != nullptr;
    static int done[2] = {-1, -1};
    if (!on || h < 0 || h > 1 || done[h] == game.weapon) return;
    done[h] = game.weapon;
    Vec3 gi[4], gm[4], gl[4], gt[4];
    finger_points(h, kIndex, 0, gi);
    finger_points(h, kMiddle, 0, gm);
    finger_points(h, kPinky, 0, gl);
    finger_points(h, kThumb, 0, gt);
    // her vertices as drawn (every one, by bone: sh3vr_combat.cpp)
    const Vec3* fv = nullptr;
    const uint8_t* sg = nullptr;
    Vec3 joint[5][3], wrist, knuckle[4], thumb[3];
    const int nf = combat_hand_finger_mesh(h, &fv, &sg, joint, wrist, knuckle, thumb);
    auto frame = [](Vec3 m, Vec3 i, Vec3 l, Vec3 t, Vec3 e[3]) {
        e[0] = normalized(l - i);
        const Vec3 u = t - m;
        e[1] = normalized(u - e[0] * dot(u, e[0]));
        e[2] = cross(e[0], e[1]);
    };
    Vec3 ew[3], eg[3];
    if (nf > 0) frame(knuckle[1], knuckle[0], knuckle[3], thumb[0], ew);
    frame(gm[0], gi[0], gl[0], gt[0], eg);
    const float U = kUnitsPerMetre;
    auto to_grip = [&](Vec3 w) {
        const Vec3 d = w - knuckle[1];
        return gm[0] + (eg[0] * dot(d, ew[0]) + eg[1] * dot(d, ew[1]) + eg[2] * dot(d, ew[2])) * (1.0f / U);
    };
    constexpr int kX = 40, kY = 56;  // x -60..60 mm, y -100..68 mm (3 mm cells)
    const float x0 = -0.060f, y0 = -0.100f, cell = 0.003f;
    static char grid[kY][kX + 1];
    std::vector<Vec3> palm_pts(palm_skin[h]);
    rt_log("VR: grip slices: %s hand, weapon %d: x %.0f..%.0f mm (cells of 3), y %.0f..%.0f, z every 5 mm; %d palm "
           "points, %d finger vertices", h == kLeft ? "left" : "right", game.weapon, x0 * 1000.0f,
           (x0 + kX * cell) * 1000.0f, y0 * 1000.0f, (y0 + kY * cell) * 1000.0f, (int)palm_pts.size(), nf);
    for (int iz = -12; iz <= 10; iz++) {
        const float z = iz * 0.005f;
        bool any = false;
        for (int iy = 0; iy < kY; iy++) {
            for (int ix = 0; ix < kX; ix++) {
                const Vec3 l{x0 + (ix + 0.5f) * cell, y0 + (iy + 0.5f) * cell, z};
                const Vec3 t = grip_point(s.shown, l);
                const float c = weapon_clearance(h, t, t);
                grid[iy][ix] = c < 0 ? '#' : c < 0.003f ? '+' : '.';
                any |= c < 0.003f;
            }
            grid[iy][kX] = 0;
        }
        auto mark = [&](Vec3 g, char ch) {
            if (std::fabs(g.z - z) > 0.0025f) return;
            const int ix = (int)std::floor((g.x - x0) / cell), iy = (int)std::floor((g.y - y0) / cell);
            if (ix < 0 || ix >= kX || iy < 0 || iy >= kY) return;
            const Vec3 t = grip_point(s.shown, g);
            const bool in = weapon_clearance(h, t, t) < 0;
            any = true;
            grid[iy][ix] = in ? 'X' : ch;
        };
        for (const Vec3& g : palm_pts) mark(g, 'P');
        for (int i = 0; i < nf; i++) mark(to_grip(fv[i]), 'F');
        if (!any) continue;
        for (int iy = 0; iy < kY; iy++)
            rt_log("VR: grip slice z %+.0f y %+.0f: %s", z * 1000.0f, (y0 + (iy + 0.5f) * cell) * 1000.0f, grid[iy]);
    }
}
#endif

// Her fingers this frame (combat_frame's end, the hands posed): Half-Life: Alyx's fingers on the weapon and on what the
// hand holds as well as on the world (the owner: "the half life alyx finger movement/world collision ... should also be
// able to feel weapons and guns"; "be sure the gripping of ammo/shells/mags is as clean as possible").
// - The hand round the weapon's grip, or on its second grip: each finger closes until it meets the weapon's shape (its
//   capsules, sh3vr_combat.cpp kShapes), else to the hold's own curl. A gun's index finger lies on the trigger, pulled
//   with it (along the frame while the finger is off the trigger's touch sensor).
// - The hand on the slide or the cocking knob: its reload shape, each finger stopped on the weapon.
// - The hand holding a magazine, a shell or a battery: each finger wraps the part (its capsules) from the reload's shape,
//   so a magazine sits in a closed fist and a shell is pinched between the fingertips, nothing through it.
// - A free hand: the controls' curl, stopped by the world (the probes) and by the weapon (a touch's tick when a finger
//   first meets it).
void body_fingers() {
    // (session 36: wrap_of solves at most one new wrap a hand a frame)
    for (int h = 0; h < 2; h++) {
        HandState& s = hands[h];
        if (!s.valid) {
            weapon_touch[h] = false;
            index_spread[h] = 0.0f;
            thumb_spread[h] = 0.0f;
            thumb_keep[h].known = false;
            trigger_shown[h] = 0.0f;
            std::memset(finger_joint_off[h], 0, sizeof finger_joint_off[h]);
            continue;
        }
        const int role = weapon_hand_role(h);
        int wrap_solves = 1;  // (session 36: new wraps this hand solves this frame at most, wrap_of)
        PartCaps pc;
        bool part = parts_hand_caps(h, pc);
        const Quat rel = conjugate(s.grip.orientation) * s.aim.orientation;
        // The flashlight's hold is searched ahead, a slice a frame once her hands are read, so it is ready when the
        // lamp is first taken (a finished search returns at once).
        if (opt.chest_light_touch && her_hands[h].valid) light_hold_of(h, rel);
        // The flashlight off her chest: held as a part is, every finger closing until it meets the box.
        if (!part && combat_light_hand() == h) {
            light_caps(light_hold_of(h, rel).c, light_axes(rel), pc);
            part = true;
        }
        const bool holding = role != 0 || part || s.holding_weapon || s.on_foregrip;
        const Controls& ctl = fi.controls;
        const float trig = h == kLeft ? ctl.left_trigger : ctl.right_trigger;
        const float grip = h == kLeft ? ctl.left_grip : ctl.right_grip;
        const bool first = h == kLeft ? ctl.left_first_touch : ctl.right_first_touch;
        const bool world = !holding && opt.finger_bending;  // the fingers are the world's (Jak's solver below)
        bool touched = false;
        // Session 8: the surfaces the hand is at (body_frame's planes) and whether the palm faces one near it (Jak's
        // palm_side: taken with the palm turned within 60 degrees of it, let go only past 70, so a hand edge on does
        // not flick its little finger on and off it), its skin within 6 cm: the fingers then rest on it.
        const Solids& at = hand_at[h];
        bool palm_side = false;
        float palm_clear = 1.0f;
        if (world && at.n > 0) {
            const Vec3 out = rotate(s.shown.orientation, Vec3{h == kLeft ? 1.0f : -1.0f, 0, 0});
            float most = 1.0f;
            for (int q = 0; q < at.n; q++) most = std::min(most, dot(at.p[q].n, out));
            Vec3 pp[18];
            const int np = palm_points(h, pp);
            for (int i = 0; i < np; i++)
                for (int k = 0; k < solid_count(at); k++)
                    palm_clear = std::min(palm_clear, solid_clear(at, k, grip_point(s.shown, pp[i])));
            palm_side = most < (palm_faces[h] ? -0.34f : -0.5f) && palm_clear > -0.05f && palm_clear < 0.06f;
        }
        palm_faces[h] = palm_side;
        float spread_target = 0.0f;  // (session 31: the trigger finger's sideways spread, degrees: index_spread)
        float pull_c[5]{};           // (session 31: its curls along the trigger's pull, body_trigger_pull_curls)
        bool pulls = false;
        float thumb_target = 0.0f;   // (session 31: the thumb's swing at its root, degrees: thumb_spread)
        if (role != 1 && role != 2) thumb_keep[h].known = false;
        trigger_shown[h] = 0.0f;
        float joint_target[5][3]{};  // (session 36: each joint's curl over its finger's, finger_joint_off)
        for (int f = 0; f < 5; f++) {
            float target = s.want[f], free_from = 0;
            if (role == 1 || role == 2) {
                const float hold = weapon_hold_curl(h);
                // 0.1.8 (owner message 146): a gun's trigger finger lays its pad on the trigger itself (its blade from
                // the model) while the player's finger rests on the controller's trigger, following it as it is pulled
                // (session 31); off it, straight beside the frame. A gun without a trigger in its model (the flamethrower):
                // the finger closes on its grip with the others.
                const GripTrigger tg = role == 1 && f == kIndex && weapon_is_gun() ? weapon_trigger() : GripTrigger{};
                if (tg.on) {
                    // (session 31: spread sideways as far as it takes to reach the trigger, body_trigger_curl; off the
                    // trigger, where it lies clear of the gun: beside the frame or above the guard)
                    float sp_on = 0.0f, sp_off = 0.0f;
                    const float ct = body_trigger_curl(h, s.shown, tg, nullptr, &sp_on);
                    const float rest = body_trigger_rest(h, s.shown, sp_on, &sp_off);
                    const bool on_it = trig > 0.05f || first;
                    if (ct >= 0) {
                        // (session 31, owner message 151: pulled, the trigger turns back about its pin and her finger
                        // follows it closing, its pad on the blade the whole pull, body_trigger_pull_curls; the trigger
                        // is drawn as far as her finger, drawn, has pushed it: trigger_shown, below)
                        const bool exact = body_trigger_pull_curls(h, s.shown, ct, sp_on, pull_c);
                        pulls = true;
                        static int told = 0;
                        static bool was_on[2]{};
                        if (on_it && !was_on[h] && told < 24) {
                            told++;
                            rt_log("VR: frame %d: the %s hand's trigger finger on the trigger (curl %.2f, spread %.0f "
                                   "degrees); pulled a quarter at a time it closes to %.2f %.2f %.2f %.2f, %s",
                                   rt_frame, h == kLeft ? "left" : "right", ct, sp_on, pull_c[1], pull_c[2], pull_c[3],
                                   pull_c[4], exact ? "its pad on the blade the whole way"
                                                    : "its pad losing the blade somewhere (evenly from there)");
                        }
                        was_on[h] = on_it;
                        target = on_it ? curl_of_pull(pull_c, trig) : rest > -0.5f ? rest : std::min(0.06f, ct);
                        spread_target = on_it || rest < -0.5f ? sp_on : sp_off;
                    } else {
                        // (the trigger out of its reach, as where the player has moved the gun in the hand: 0.1.7's
                        // curls, no farther than where it meets the gun)
                        const float c = finger_contact(h, kIndex, s.shown, nullptr, true, 1.0f, free_from);
                        target = std::min(on_it ? 0.42f + 0.33f * trig : 0.10f, c >= 0 ? c : 1.0f);
                    }
                } else {
                    const float c = finger_contact(h, f, s.shown, nullptr, true, 1.0f, free_from);
                    const float th = f == kThumb ? weapon_hold_thumb(h) : -1.0f;
                    const float base = f == kThumb ? (th >= 0 ? th : std::min(hold, 0.7f)) : hold;
                    target = c >= 0 ? c : std::max(base, free_from);
                    // Session 36 (owner message 169): closed joint by joint round the weapon, each part resting on it
                    // (finger_wrap; the trigger finger's own work above is left as it is)
                    // (taken in as the finger's curl comes to it, so a finger still closing or opening keeps its
                    // joints together)
                    float jw[3];
                    if (f != kThumb && c >= 0 && wrap_of(h, f, role, s.shown, c, wrap_solves, jw)) {
                        const float in = std::clamp(1.0f - std::fabs(target - s.curl[f]) / 0.1f, 0.0f, 1.0f);
                        for (int j = 0; j < 3; j++) joint_target[f][j] = (jw[j] - target) * in;
                    }
                    // 0.1.8 (vr371): her thumb in the weapon at every curl, or out of it only past half: it rests
                    // where it is clear of it (thumb_rest, along the frame or round the grip), as the grips were fitted.
                    // Session 31 (vr384): always so, swung at its root, its pad's segment on the weapon (thumb_resting)
                    if (f == kThumb && grip_fit_mode_on()) {
                        float sw = 0.0f;
                        const float tr = thumb_resting(h, s.shown, sw);
                        if (tr > -0.5f) target = tr, thumb_target = sw;  // (-1: none)
                    }
                }
            } else if (role == 3 || part) {
                const float c = finger_contact(h, f, s.shown, part ? &pc : nullptr, role == 3, 1.0f, free_from);
                target = c >= 0 ? c : std::max(s.want[f], free_from);
            } else {
                float want = s.want[f];
                // A grip closed on something in the world (a handle, a drawer's edge, an item: the fingers stopped
                // by it, or the palm on it): the thumb closes on it too, from its side, whatever the thumb rests on.
                if (f == kThumb && world && grip > 0.6f) {
                    bool on_world = s.resting;
                    for (int k = kIndex; k <= kPinky; k++) on_world |= s.stop[k] < 0.85f;
                    if (on_world) want = std::max(want, 0.75f);
                }
                // No farther than the fingertips' probes let it close (what no plane is fitted to: a handle, an
                // item) nor into the weapon in the other hand.
                float cap = world ? std::max(s.stop[f], 0.0f) : 1.0f;
                const float c = finger_contact(h, f, s.shown, nullptr, true, s.want[f], free_from);
                if (c >= 0) {
                    cap = std::min(cap, c);
                    touched = true;
                }
                if (world) {
                    // Jak's fingers on the world (world_finger), from where it is drawn when it was not the world's.
                    if (!solved_was[h]) finger_off[h][f] = s.curl[f] - want;
                    const bool wrap =
                        palm_side && (f >= kMiddle || (f == kThumb && want > 0.5f) || (f == kIndex && want > 0.1f));
                    s.curl[f] = world_finger(h, f, want, cap, s.shown, at, wrap);
                    finger_target[h][f] = s.curl[f];
                    continue;
                }
                target = std::min(want, cap);
            }
            finger_target[h][f] = target;
            s.curl[f] += (target - s.curl[f]) * ease(22.0f);
            if (f == kIndex && pulls) trigger_shown[h] = pull_of_curl(pull_c, s.curl[f]);
        }
        index_spread[h] += (spread_target - index_spread[h]) * ease(22.0f);
        thumb_spread[h] += (thumb_target - thumb_spread[h]) * ease(22.0f);
        // (a hand on the world: its fingers' joints together at once, as the world's solver placed them)
        if (world) std::memset(finger_joint_off[h], 0, sizeof finger_joint_off[h]);
        for (int f = 0; f < 5; f++)
            for (int j = 0; j < 3; j++)
                finger_joint_off[h][f][j] += (joint_target[f][j] - finger_joint_off[h][f][j]) * ease(22.0f);
        solved_was[h] = world;
        if (touched && !weapon_touch[h] && opt.haptic_surfaces) buzz(h, 0.15f, 0.012f, 220.0f);
        weapon_touch[h] = touched;
        // A hand on the world for a third of a second is told once, resting and again gripping (session 8): what it
        // is at, how far its palm and its fingers are from it, how far each finger is drawn closed and where the
        // fingertips' probes stop them, so a hand on a wall, an edge or a handle reads in the log as well as in the
        // pictures (the headset's log too).
        static int rest_frames[2], grip_frames[2];
        bool on_world = s.resting || palm_side;
        for (int k = kIndex; k <= kPinky; k++) on_world |= s.stop[k] < 0.85f;
        if (world && on_world) {
            const bool gripping = grip > 0.6f;
            int& frames = gripping ? grip_frames[h] : rest_frames[h];
            (gripping ? rest_frames[h] : grip_frames[h]) = 0;
            if (++frames == 20 && at.n == 0) {
                rt_log("VR: frame %d: the %s hand %s a surface no plane was fitted to (the fingertips' probes only): "
                       "fingers drawn %.2f %.2f %.2f %.2f %.2f, stopped at %.2f %.2f %.2f %.2f %.2f (thumb to little "
                       "finger; 0 open, 1 a fist); the controller %.1f cm from where the hand is drawn",
                       rt_frame, h == kLeft ? "left" : "right", gripping ? "grips" : "rests on", s.curl[0], s.curl[1],
                       s.curl[2], s.curl[3], s.curl[4], s.stop[0], s.stop[1], s.stop[2], s.stop[3], s.stop[4],
                       length(s.shown.position - s.grip.position) * 100.0f);
            } else if (frames == 20) {
                const float fc = fingers_clear(h, at, s.shown, s.curl);
                const Vec3 n0 = at.p[0].n;
                rt_log("VR: frame %d: the %s hand %s %s (normal %.2f %.2f %.2f, %d points within %.1f mm): its palm "
                       "%.1f mm off it, the controller %.1f cm past it; fingers drawn %.2f %.2f %.2f %.2f %.2f (thumb to "
                       "little finger; 0 open, 1 a fist, below 0 bent back), the fingertips' probes stop them at %.2f "
                       "%.2f %.2f %.2f %.2f; the fingers' nearest joint %.1f mm off it, their skin %.1f mm (below 0: in "
                       "it); palm %s it",
                       rt_frame, h == kLeft ? "left" : "right", gripping ? "grips" : "rests on",
                       at.n == 1 ? "a plane" : at.implied ? "an edge (its front face implied)" : at.convex ? "an edge (two faces)" : "a corner (two faces)",
                       n0.x, n0.y, n0.z, at.points, at.rms * 1000.0f, palm_clear * 1000.0f, length(s.push) * 100.0f,
                       s.curl[0], s.curl[1], s.curl[2], s.curl[3], s.curl[4], s.stop[0], s.stop[1], s.stop[2],
                       s.stop[3], s.stop[4], fc * 1000.0f, (fc - kFingerThick) * 1000.0f,
                       palm_side ? "facing" : "not facing");
            }
        } else {
            rest_frames[h] = grip_frames[h] = 0;
        }
    }
#if !defined(__ANDROID__)
    // The rig's pictures (session 32): both hands as drawn for each shot, each finger's curl and the curl it closes to,
    // the thumb's swing, the trigger finger's spread and the trigger as drawn, and a hand on the weapon's report (each
    // finger's knuckle clearance, the curl it meets the weapon at, the thumb's rest, the palm), so every picture is
    // read against numbers.
    if (quest::sim_shot_hands.exchange(0) != 0) {
        static char line[4800];
        for (int h = 0; h < 2; h++) {
            const HandState& s = hands[h];
            if (!s.valid) continue;
            const int role = weapon_hand_role(h);
            line[0] = 0;
            if (role == 1 || role == 2) {
                body_grip_report(h, s.shown, line, sizeof line);
                // (session 36: each finger's joints as drawn, knuckle/middle/last, and its segments' skin from the
                // weapon there, mm, below 0 in it: finger_wrap's)
                int n = (int)std::strlen(line);
                for (int f = kIndex; f <= kPinky && n < (int)sizeof line; f++) {
                    float c3[3], g[3];
                    for (int j = 0; j < 3; j++)
                        c3[j] = std::clamp(s.curl[f] + finger_joint_off[h][f][j], kFingerBackMost, 1.0f);
                    joint_gaps(h, f, s.shown, c3, g, 0, 3, f == kIndex ? index_spread[h] : 0.0f);
                    n += std::snprintf(line + n, sizeof line - n, "%s%c %.2f/%.2f/%.2f [%.1f %.1f %.1f]",
                                       f == kIndex ? "; joints drawn " : ", ", "TIMRL"[f], c3[0], c3[1], c3[2],
                                       g[0] * 1000.0f, g[1] * 1000.0f, g[2] * 1000.0f);
                }
                // (0.1.10: her palm as drawn in this hand's model, body_palm_skin, at the pose drawn: its nearest to the
                // weapon, which the drawn hand's own measure below should agree with)
                if (!palm_skin[h].empty() && n < (int)sizeof line - 80) {
                    float skin = 1.0f;
                    int in = 0;
                    Vec3 mid{};  // (the middle of those in it, grip space)
                    for (const Vec3& l : palm_skin[h]) {
                        const Vec3 t = grip_point(s.shown, l);
                        const float c = weapon_clearance(h, t, t);
                        skin = std::min(skin, c);
                        if (c < 0) in++, mid = mid + l;
                    }
                    if (in) mid = mid * (1000.0f / (float)in);
                    n += std::snprintf(line + n, sizeof line - n, "; the model's palm as drawn (%d points) %.1f mm off it "
                                       "(%d in it, about %.0f %.0f %.0f mm in the grip's space)",
                                       (int)palm_skin[h].size(), skin * 1000.0f, in, mid.x, mid.y, mid.z);
                }
                // (0.1.10, owner 172: her hand's own vertices as drawn against the weapon as drawn, each part's nearest)
                if (n < (int)sizeof line - 8) {
                    n += std::snprintf(line + n, sizeof line - n, "; ");
                    const int k = combat_hand_mesh_report(h, line + n, (int)sizeof line - n);
                    if (k > 0) n += k;
                    else n -= 2, line[n] = 0;
                }
                // (... and her bones as drawn against this hand model's at the pose drawn: each knuckle's and the
                // thumb's root's offset, mm, world axes)
                Vec3 dw, dk[4], dt[3];
                if (n < (int)sizeof line - 200 && ik_hand_drawn(h, dw, dk, dt)) {
                    n += std::snprintf(line + n, sizeof line - n, "; her bones drawn off this model's (mm):");
                    for (int f = kThumb; f <= kPinky && n < (int)sizeof line - 40; f++) {
                        Vec3 p[4];
                        finger_points(h, f, 0.0f, p);
                        const Vec3 d = ((f == kThumb ? dt[0] : dk[f - kIndex]) - world_point(grip_point(s.shown, p[0]))) *
                                       (1000.0f / kUnitsPerMetre);
                        n += std::snprintf(line + n, sizeof line - n, " %c %.0f %.0f %.0f (%.0f)", "TIMRL"[f], d.x, d.y,
                                           d.z, length(d));
                    }
                }
            }
            rt_log("VR: frame %d: the %s hand drawn for the picture: on the weapon as %d, curls %.2f %.2f %.2f %.2f %.2f "
                   "(closing to %.2f %.2f %.2f %.2f %.2f), thumb swung %.1f, trigger finger spread %.1f, the trigger "
                   "drawn %.2f, wraps solved %d%s%s",
                   rt_frame, h == kLeft ? "left" : "right", role, s.curl[0], s.curl[1], s.curl[2], s.curl[3], s.curl[4],
                   finger_target[h][0], finger_target[h][1], finger_target[h][2], finger_target[h][3],
                   finger_target[h][4], thumb_spread[h], index_spread[h], trigger_shown[h], wraps_solved[h], line[0] ? "; " : "", line);
            // (0.1.10: her fingers as drawn against the model's)
            if (finger_skin_text(h, s, line, (int)sizeof line) > 0)
                rt_log("VR: frame %d: the %s hand drawn for the picture: %s", rt_frame, h == kLeft ? "left" : "right", line);
            // (session 38: the other hand on the weapon, its skin as drawn against her main hand's as drawn)
            if (role == 2 && hand_skin[h].ready && hand_skin[1 - h].ready) {
                float cb[5][3], sb[5] = {thumb_spread[h], index_spread[h], 0.0f, 0.0f, 0.0f};
                for (int f = 0; f < 5; f++)
                    for (int j = 0; j < 3; j++)
                        cb[f][j] = std::clamp(s.curl[f] + finger_joint_off[h][f][j], kFingerBackMost, 1.0f);
                const float b = hand_skin_between(h, s.shown, cb, sb);
                if (b < 0.01f)
                    rt_log("VR: frame %d: the %s hand drawn for the picture: its skin as drawn %.1f mm off her main hand's "
                           "as drawn at the nearest (below 0: in it)", rt_frame, h == kLeft ? "left" : "right", b * 1000.0f);
                else
                    rt_log("VR: frame %d: the %s hand drawn for the picture: its skin as drawn more than 1 cm off her main "
                           "hand's as drawn", rt_frame, h == kLeft ? "left" : "right");
            }
            if (role == 1 || role == 2) grip_slices(h, s);
            // (0.1.10: her hand as this model poses it at the curls drawn, against her hand as drawn: each vertex's
            // distance (mm) at most and on average; and, on the weapon, its nearest to it as the fit reads it)
            HandDrawn hd;
            static std::vector<Vec3> posed;
            float c3[5][3];
            const float sp[5] = {thumb_spread[h], index_spread[h], 0.0f, 0.0f, 0.0f};
            for (int f = 0; f < 5; f++)
                for (int j = 0; j < 3; j++)
                    c3[f][j] = std::clamp(s.curl[f] + finger_joint_off[h][f][j], kFingerBackMost, 1.0f);
            if (hand_skin[h].ready && combat_hand_drawn(h, hd) && hand_skin_pose(h, c3, sp, posed)) {
                float most = -1.0f, sum = 0.0f, by_part[8]{};  // (the most off by part: hand_skin's)
                int worst_i = -1;
                Vec3 worst_g{};
                if ((int)posed.size() == hd.n) {
                    Vec3 gi[4], gm[4], gl[4], gt[4];
                    finger_points(h, kIndex, 0, gi);
                    finger_points(h, kMiddle, 0, gm);
                    finger_points(h, kPinky, 0, gl);
                    finger_points(h, kThumb, 0, gt);
                    auto frame = [](Vec3 m, Vec3 i, Vec3 l, Vec3 t, Vec3 e[3]) {
                        e[0] = normalized(l - i);
                        const Vec3 u = t - m;
                        e[1] = normalized(u - e[0] * dot(u, e[0]));
                        e[2] = cross(e[0], e[1]);
                    };
                    Vec3 ew[3], eg[3];
                    frame(hd.knuckle[1], hd.knuckle[0], hd.knuckle[3], hd.thumb[0], ew);
                    frame(gm[0], gi[0], gl[0], gt[0], eg);
                    for (int i = 0; i < hd.n; i++) {
                        const Vec3 e = hd.v[i] - hd.knuckle[1];
                        const Vec3 g = gm[0] + (eg[0] * dot(e, ew[0]) + eg[1] * dot(e, ew[1]) + eg[2] * dot(e, ew[2])) *
                                                   (1.0f / kUnitsPerMetre);
                        const float d = length(g - posed[(size_t)i]);
                        if (d > most) worst_i = i, worst_g = g;
                        most = std::max(most, d), sum += d;
                        const int pt = std::min<int>(hand_skin[h].part[(size_t)i], 7);
                        by_part[pt] = std::max(by_part[pt], d);
                    }
                }
                int pt = -1;
                float pm = 1e9f;
                const float m = role == 1 || role == 2 ? hand_skin_clear(h, s.shown, c3, sp, &pt, &pm) : 1e9f;
                static const char* const kPart[8] = {"her palm", "blended skin", "thumb", "index", "middle", "ring",
                                                     "little", "-"};
                rt_log("VR: frame %d: the %s hand drawn for the picture: her hand as this model poses it (%d vertices) "
                       "off her hand as drawn (%d, frame %d): %.2f mm at most, %.2f on average (at most: her palm %.2f, "
                       "blended %.2f, T %.2f I %.2f M %.2f R %.2f L %.2f, her wrist on her forearm too %.2f); against "
                       "the weapon as the fit reads it: %.1f mm (%s), her palm's and the blended skin's %.1f mm",
                       rt_frame, h == kLeft ? "left" : "right", (int)posed.size(), hd.n, hd.frame, most * 1000.0f,
                       hd.n ? sum / hd.n * 1000.0f : 0.0f, by_part[0] * 1000.0f, by_part[1] * 1000.0f,
                       by_part[2] * 1000.0f, by_part[3] * 1000.0f, by_part[4] * 1000.0f, by_part[5] * 1000.0f,
                       by_part[6] * 1000.0f, by_part[7] * 1000.0f, m * 1000.0f, kPart[pt >= 0 && pt < 7 ? pt : 7],
                       pm * 1000.0f);
                // (session 38, owner 178: her wrist as drawn, its arm's bend, and the hand's turn from the controller's)
                if (role == 1 || role == 2)
                    rt_log("VR: frame %d: the %s hand drawn for the picture: its wrist bent %.0f degrees as drawn, the "
                           "hand %.0f degrees from her real hand's (the controller's) along it", rt_frame,
                           h == kLeft ? "left" : "right", ik_wrist_bend(h), body_wrist_swing(h, s.shown));
                // (session 38: the main hand's trigger finger as drawn against the trigger as drawn, turned as far as
                // her finger has pushed it, and against the rest of the gun, hand_skin_trigger)
                if (role == 1 && trigger_shown[h] > 0.01f) {
                    float on_t = 1e9f, on_g = 1e9f;
                    hand_skin_trigger(h, s.shown, c3, sp, trigger_shown[h], on_t, on_g);
                    if (on_t < 1e8f)
                        rt_log("VR: frame %d: the %s hand drawn for the picture: its trigger finger as drawn against the "
                               "trigger as drawn (pulled %.2f): %.1f mm; against the rest of the gun, the trigger at rest "
                               "left out: %.1f mm (below 0: in it)", rt_frame, h == kLeft ? "left" : "right",
                               trigger_shown[h], on_t * 1000.0f, std::min(on_g, 0.05f) * 1000.0f);
                }
                // (testing: the vertex most off, drawn and posed, mm in the grip space, and its bones then and now)
                if (worst_i >= 0 && most > 0.002f) {
                    const SkinVert& sv = hand_skin[h].v[(size_t)worst_i];
                    const Vec3 a = worst_g * 1000.0f, b = posed[(size_t)worst_i] * 1000.0f;
                    char bones[200];
                    int at = 0;
                    for (int k = 0; k < sv.n; k++)
                        at += std::snprintf(bones + at, sizeof bones - at, " %d:%.2f", sv.tag[k], sv.w[k]);
                    for (int k = 0; k < 4; k++)
                        at += std::snprintf(bones + at, sizeof bones - at, "%s%d:%.2f", k ? " " : " | now", hd.it[worst_i * 4 + k],
                                            hd.iw[worst_i * 4 + k]);
                    rt_log("VR: ... its vertex %d most off: drawn %.1f %.1f %.1f, posed %.1f %.1f %.1f; bones (taken):%s",
                           worst_i, a.x, a.y, a.z, b.x, b.y, b.z, bones);
                }
            }
        }
    }
#endif
}

float body_trigger_shown(int h) { return h >= 0 && h < 2 && hands[h].valid ? trigger_shown[h] : 0.0f; }

bool body_light_place(int h, Vec3& centre, Vec3& lens) {
    if (h < 0 || h > 1 || !hands[h].valid) return false;
    const LightHold& lh = light_hold_of(h, conjugate(hands[h].grip.orientation) * hands[h].aim.orientation);
    centre = lh.c, lens = lh.lens;
    return true;
}

bool hand_finger_tip(int h, int f, float c, Vec3& tip) {
    if (h < 0 || h > 1 || f < 0 || f > 4) return false;
    Vec3 p[4];
    finger_points(h, f, c, p);
    tip = p[3];
    return true;
}

// 0.1.8 (owner message 146): hand h's palm and fingers as drawn this frame, as capsules (tracking, metres; kept for the
// frame): the other hand on the second grip wraps the main hand's fingers and never goes through them (0.1.7 had a
// fist-sized rod round the grip stand for them, so the second hand's fingers stopped short of them or sank into them).
int body_hand_capsules(int h, Vec3* a, Vec3* b, float* r, int max) {
    static int count[2] = {-1, -1};
    static float key[2][29];
    static Vec3 ca[2][18], cb[2][18];
    static float cr[2][18];
    if (h < 0 || h > 1 || !hands[h].valid) return 0;
    // (kept while the hand's pose, curls and spreads stay the same: the hand is posed in combat_frame, after some callers;
    // session 36: each finger's joints as drawn, finger_joint_off)
    const HandState& hs = hands[h];
    float now_key[29] = {hs.shown.position.x, hs.shown.position.y, hs.shown.position.z, hs.shown.orientation.x,
                         hs.shown.orientation.y, hs.shown.orientation.z, hs.shown.orientation.w, hs.curl[0],
                         hs.curl[1], hs.curl[2], hs.curl[3], hs.curl[4], index_spread[h], thumb_spread[h]};
    std::memcpy(now_key + 14, finger_joint_off[h], sizeof finger_joint_off[h]);
    if (count[h] < 0 || std::memcmp(key[h], now_key, sizeof now_key) != 0) {
        std::memcpy(key[h], now_key, sizeof now_key);
        const HandState& s = hs;
        int n = 0;
        for (int f = 0; f < 5; f++) {
            Vec3 p[4];
            const float c3[3] = {std::clamp(s.curl[f] + finger_joint_off[h][f][0], kFingerBackMost, 1.0f),
                                 std::clamp(s.curl[f] + finger_joint_off[h][f][1], kFingerBackMost, 1.0f),
                                 std::clamp(s.curl[f] + finger_joint_off[h][f][2], kFingerBackMost, 1.0f)};
            finger_points3(h, f, c3, p, f == kIndex ? index_spread[h] : f == kThumb ? thumb_spread[h] : 0.0f);
            for (int j = 0; j < 3; j++, n++) {
                ca[h][n] = grip_point(s.shown, p[j]);
                cb[h][n] = grip_point(s.shown, p[j + 1]);
                cr[h][n] = f == kThumb ? 0.0095f : 0.0085f;
            }
        }
        // The palm: the knuckles' row, and the row 6.5 cm back towards the wrist (palm_points' heel), 2.6 cm thick.
        Vec3 pi[4], pp[4], pm[4];
        finger_points(h, kIndex, 0, pi);
        finger_points(h, kPinky, 0, pp);
        finger_points(h, kMiddle, 0, pm);
        const Vec3 along = normalized(pm[3] - pm[0]);
        ca[h][n] = grip_point(s.shown, pi[0]), cb[h][n] = grip_point(s.shown, pp[0]), cr[h][n++] = 0.011f;
        ca[h][n] = grip_point(s.shown, pi[0] - along * 0.065f), cb[h][n] = grip_point(s.shown, pp[0] - along * 0.065f);
        cr[h][n++] = 0.013f;
        count[h] = n;
    }
    const int n = std::min(count[h], max);
    for (int i = 0; i < n; i++) a[i] = ca[h][i], b[i] = cb[h][i], r[i] = cr[h][i];
    return n;
}

// 0.1.10 (session 38; open since session 37): hand h's skin as drawn (hand_skin) posed where and as it is drawn now
// (its fingers' curls and spreads, body_hand_capsules'), every vertex but her wrist's (tracking, metres), and their box;
// kept while the hand's pose, curls and spreads stay the same. The other hand on the second grip closes onto it and is
// measured against it in the grips' fit (sh3vr_combat.cpp weapon_clearance): the capsules alone are the hand model's,
// its palm 2.6 cm thick against her drawn palm's 5.2, so the other hand fitted round them went into her hand as drawn
// or, measured against it only after (hand_skin_between), stood off it. 0 before it is taken.
int body_hand_skin_points(int h, const Vec3** out, Vec3* lo, Vec3* hi) {
    static std::vector<Vec3> pts[2];
    static float key[2][30];
    static bool have[2] = {false, false};
    static Vec3 blo[2], bhi[2];
    if (h < 0 || h > 1 || !hands[h].valid || !hand_skin[h].ready) return 0;
    const HandState& hs = hands[h];
    float now_key[30] = {hs.shown.position.x, hs.shown.position.y, hs.shown.position.z, hs.shown.orientation.x,
                         hs.shown.orientation.y, hs.shown.orientation.z, hs.shown.orientation.w, hs.curl[0],
                         hs.curl[1], hs.curl[2], hs.curl[3], hs.curl[4], index_spread[h], thumb_spread[h]};
    std::memcpy(now_key + 14, finger_joint_off[h], sizeof finger_joint_off[h]);
    now_key[29] = (float)hand_skin_taken[h];
    if (!have[h] || std::memcmp(key[h], now_key, sizeof now_key) != 0) {
        std::memcpy(key[h], now_key, sizeof now_key);
        float c3[5][3], sp[5] = {thumb_spread[h], index_spread[h], 0.0f, 0.0f, 0.0f};
        for (int f = 0; f < 5; f++)
            for (int j = 0; j < 3; j++) c3[f][j] = std::clamp(hs.curl[f] + finger_joint_off[h][f][j], kFingerBackMost, 1.0f);
        have[h] = hand_skin_pose(h, c3, sp, pts[h]);
        if (!have[h]) return 0;
        size_t k = 0;
        blo[h] = {1e9f, 1e9f, 1e9f}, bhi[h] = {-1e9f, -1e9f, -1e9f};
        for (size_t i = 0; i < pts[h].size(); i++) {
            if (hand_skin[h].part[i] == kSkinWrist) continue;
            const Vec3 p = grip_point(hs.shown, pts[h][i]);
            pts[h][k++] = p;
            blo[h] = {std::min(blo[h].x, p.x), std::min(blo[h].y, p.y), std::min(blo[h].z, p.z)};
            bhi[h] = {std::max(bhi[h].x, p.x), std::max(bhi[h].y, p.y), std::max(bhi[h].z, p.z)};
        }
        pts[h].resize(k);
    }
    *out = pts[h].data();
    if (lo) *lo = blo[h];
    if (hi) *hi = bhi[h];
    return (int)pts[h].size();
}

// 0.1.10 (owner 172: "WHY ... IS THE GUN STILL CLIPPING THROUGH THE HAND"; build 278, her hand as drawn against the
// weapon as drawn, sh3vr_combat.cpp hand_depth: at 0.1.8's fitted place the handgun's grip lies 18 mm into the middle
// of her palm, where palm_points, at the knuckles and the heel alone, never looked): her palm's skin as drawn, in hand
// h's grip space. Her bones as drawn lie exactly on this model's (build 278: 0 mm off at every knuckle and the thumb's
// root), so each vertex is carried over by the frame of the same joints, drawn and modelled: the index, middle and
// little fingers' knuckles and the thumb's root (all on the hand's own bone). Points within 1.5 mm of one kept are
// left out.
bool body_palm_skin_ready(int h) { return h >= 0 && h < 2 && !palm_skin[h].empty(); }
float body_palm_skin_clear(int h, const Pose& pose, Vec3* back) {
    if (h < 0 || h > 1 || palm_skin[h].empty()) return 1e9f;
    if (back) *back = rotate(pose.orientation, Vec3{h == kLeft ? -1.0f : 1.0f, 0, 0});  // (palm_points' face is -mx x)
    float least = 1e9f;
    for (const Vec3& l : palm_skin[h]) {
        const Vec3 p = grip_point(pose, l);
        least = std::min(least, weapon_clearance(h, p, p));
    }
    return least;
}
void body_palm_skin(int h, const Vec3* world, int n, int of, Vec3 wrist, const Vec3 knuckle[4], const Vec3 thumb[3]) {
    if (h < 0 || h > 1 || !her_hands[h].valid || !world || n <= 0 || !palm_skin[h].empty()) return;
    Vec3 gi[4], gm[4], gl[4], gt[4];
    finger_points(h, kIndex, 0, gi);
    finger_points(h, kMiddle, 0, gm);
    finger_points(h, kPinky, 0, gl);
    finger_points(h, kThumb, 0, gt);
    const float U = kUnitsPerMetre;
    static const char* const kSide[2] = {"left", "right"};
    // (the same joints, drawn and modelled, the same distances apart within 2 mm: else her bones are not this model's)
    const float d1 = std::fabs(length(knuckle[3] - knuckle[0]) / U - length(gl[0] - gi[0]));
    const float d2 = std::fabs(length(thumb[0] - knuckle[1]) / U - length(gt[0] - gm[0]));
    const float d3 = std::fabs(length(thumb[0] - knuckle[0]) / U - length(gt[0] - gi[0]));
    if (d1 > 0.002f || d2 > 0.002f || d3 > 0.002f) {
        static int told = 0;
        if (told++ < 4)
            rt_log("VR: her %s palm as drawn not taken: her bones drawn are not this hand model's (%.1f %.1f %.1f mm "
                   "apart)", kSide[h], d1 * 1000.0f, d2 * 1000.0f, d3 * 1000.0f);
        return;
    }
    auto frame = [](Vec3 m, Vec3 i, Vec3 l, Vec3 t, Vec3 e[3]) {
        e[0] = normalized(l - i);
        const Vec3 u = t - m;
        e[1] = normalized(u - e[0] * dot(u, e[0]));
        e[2] = cross(e[0], e[1]);
    };
    Vec3 ew[3], eg[3];
    frame(knuckle[1], knuckle[0], knuckle[3], thumb[0], ew);
    frame(gm[0], gi[0], gl[0], gt[0], eg);
    std::vector<Vec3>& out = palm_skin[h];
    Vec3 lo{1e9f, 1e9f, 1e9f}, hi{-1e9f, -1e9f, -1e9f};
    for (int i = 0; i < n; i++) {
        const Vec3 d = world[i] - knuckle[1];
        const Vec3 g = gm[0] + (eg[0] * dot(d, ew[0]) + eg[1] * dot(d, ew[1]) + eg[2] * dot(d, ew[2])) * (1.0f / U);
        bool close = false;
        for (const Vec3& k : out)
            if (length(k - g) < 0.0015f) {
                close = true;
                break;
            }
        if (close) continue;
        out.push_back(g);
        lo = {std::min(lo.x, g.x), std::min(lo.y, g.y), std::min(lo.z, g.z)};
        hi = {std::max(hi.x, g.x), std::max(hi.y, g.y), std::max(hi.z, g.z)};
    }
    const float mx = h == kLeft ? -1.0f : 1.0f;
    rt_log("VR: her %s palm as drawn taken for the grips' fit: %d points (of %d vertices on its own bone from the "
           "palm's rear to the knuckles, %d on it in all), its grip space x %.1f..%.1f, y %.1f..%.1f, z %.1f..%.1f cm "
           "(the hand model's palm faces at x %.1f and %.1f; the wrist's joint %.1f cm from the middle knuckle)",
           kSide[h], (int)out.size(), n, of, lo.x * 100.0f, hi.x * 100.0f, lo.y * 100.0f, hi.y * 100.0f,
           lo.z * 100.0f, hi.z * 100.0f, (gm[0].x - mx * 0.013f) * 100.0f, (gm[0].x + mx * 0.013f) * 100.0f,
           length(wrist - knuckle[1]) / U * 100.0f);
    // (testing: SH3_PALM_DUMP=1 logs the points, mm, eight a line)
    static const bool dump = std::getenv("SH3_PALM_DUMP") != nullptr;
    for (size_t i = 0; dump && i < out.size(); i += 8) {
        char row[400];
        int at = 0;
        for (size_t k = i; k < out.size() && k < i + 8; k++)
            at += std::snprintf(row + at, sizeof row - at, " %.0f %.0f %.0f;", out[k].x * 1000.0f, out[k].y * 1000.0f,
                                out[k].z * 1000.0f);
        rt_log("VR: her %s palm as drawn, points %d-: %s", kSide[h], (int)i, row);
    }
}

// 0.1.10 (owner 172): her hand h as drawn (every vertex with its bones, sh3vr_combat.cpp combat_hand_drawn) kept in its
// grip space, for this model to pose at any curl (hand_skin). Taken when her bones as drawn lie on this model's as it
// poses them now (every finger's joint within 0.6 mm: the frame caught was drawn with the curls she has now), else
// tried again the next frame.
bool body_hand_skin_ready(int h) { return h >= 0 && h < 2 && hand_skin[h].ready; }
// 0.1.10 (session 38): her hand h drawn by another model of her (a costume; sh3vr_combat.cpp skin_watch): its palm and
// whole hand as drawn let go, to be taken again from the model drawn now
void body_hand_skin_forget(int h) {
    if (h < 0 || h > 1) return;
    palm_skin[h].clear();
    hand_skin[h] = HandSkin{};
}
void body_hand_skin(int h, const HandDrawn& d) {
    if (h < 0 || h > 1 || hand_skin[h].ready || !her_hands[h].valid || d.n <= 0 || !d.v || !d.it || !d.iw || !d.iq)
        return;
    const HandState& s = hands[h];
    if (!s.valid) return;
    Vec3 gi[4], gm[4], gl[4], gt[4];
    finger_points(h, kIndex, 0, gi);
    finger_points(h, kMiddle, 0, gm);
    finger_points(h, kPinky, 0, gl);
    finger_points(h, kThumb, 0, gt);
    auto frame = [](Vec3 m, Vec3 i, Vec3 l, Vec3 t, Vec3 e[3]) {
        e[0] = normalized(l - i);
        const Vec3 u = t - m;
        e[1] = normalized(u - e[0] * dot(u, e[0]));
        e[2] = cross(e[0], e[1]);
    };
    Vec3 ew[3], eg[3];
    frame(d.knuckle[1], d.knuckle[0], d.knuckle[3], d.thumb[0], ew);
    frame(gm[0], gi[0], gl[0], gt[0], eg);
    const float U = kUnitsPerMetre;
    auto to_grip = [&](Vec3 w) {
        const Vec3 e = w - d.knuckle[1];
        return gm[0] + (eg[0] * dot(e, ew[0]) + eg[1] * dot(e, ew[1]) + eg[2] * dot(e, ew[2])) * (1.0f / U);
    };
    // (the curls and spreads she is drawn with: body_fingers')
    float c3[5][3];
    const float sp[5] = {thumb_spread[h], index_spread[h], 0.0f, 0.0f, 0.0f};
    for (int f = 0; f < 5; f++)
        for (int j = 0; j < 3; j++) c3[f][j] = std::clamp(s.curl[f] + finger_joint_off[h][f][j], kFingerBackMost, 1.0f);
    Vec3 o[5][3];
    Quat q[5][3];
    float worst = 0;
    for (int f = 0; f < 5; f++) {
        if (!finger_frames(h, f, c3[f], sp[f], o[f], q[f])) return;
        for (int j = 0; j < 3; j++) worst = std::max(worst, length(to_grip(d.joint[f][j]) - o[f][j]));
    }
    static const char* const kSide[2] = {"left", "right"};
    static int tries[2];
    if (worst > 0.0006f) {
        if (tries[h]++ < 6)
            rt_log("VR: her %s hand as drawn not taken this frame: its joints drawn %.1f mm off this model's as it poses "
                   "them now (tried again)", kSide[h], worst * 1000.0f);
        return;
    }
    HandSkin& hs = hand_skin[h];
    hs.v.assign((size_t)d.n, SkinVert{});
    hs.part.assign((size_t)d.n, 1);
    for (int f = 0; f < 5; f++)
        for (int j = 0; j < 3; j++) hs.seg[f][j].clear(), hs.reach[f][j] = 0.0f;
    int counts[4] = {};
    for (int i = 0; i < d.n; i++) {
        SkinVert& sv = hs.v[(size_t)i];
        float own = 0, arm = 0, W = 0, segw[15]{};
        for (int k = 0; k < 4; k++) {
            const float w = d.iw[i * 4 + k];
            if (!(w > 0.0f)) continue;
            const int8_t raw = d.it[i * 4 + k];
            const int t = raw >= 0 && raw < 15 ? raw : raw == -2 ? -2 : -1;
            const Vec3 g = to_grip(d.iq[i * 4 + k]);
            sv.tag[sv.n] = (int8_t)t;
            sv.w[sv.n] = w;
            sv.l[sv.n] = t < 0 ? g : rotate(conjugate(q[t / 3][t % 3]), g - o[t / 3][t % 3]);
            sv.n++;
            W += w;
            if (t == -2) arm += w;
            else if (t < 0) own += w;
            else segw[t] += w;
        }
        int seg = -1;
        for (int t = 0; t < 15 && seg < 0; t++)
            if (segw[t] >= 0.9f * W && W > 0) seg = t;
        // (her wrist's skin on her forearm's bones too, 5% or more: where her arm bends it, not this model; kept apart,
        // kSkinWrist, out of the fit's reading)
        if (arm >= 0.05f * W && W > 0) hs.part[(size_t)i] = kSkinWrist, counts[3]++;
        else if (own >= 0.9f * W && W > 0) hs.part[(size_t)i] = 0, counts[0]++;
        else if (seg >= 0) {
            hs.part[(size_t)i] = (uint8_t)(2 + seg / 3), counts[2]++;
            const int f = seg / 3, j = seg % 3;
            const Vec3 l = rotate(conjugate(q[f][j]), to_grip(d.v[i]) - o[f][j]);
            hs.seg[f][j].push_back(l);
            hs.reach[f][j] = std::max(hs.reach[f][j], seg_dist(l, l, Vec3{}, her_hands[h].f[f].seg[j]));
        } else
            counts[1]++;
    }
    hs.tri.clear();
    for (int t = 0; d.tri && t < d.ntri; t++) {
        const int a = d.tri[t * 3], b = d.tri[t * 3 + 1], c = d.tri[t * 3 + 2];
        if (a < 0 || b < 0 || c < 0 || a >= d.n || b >= d.n || c >= d.n) continue;
        hs.tri.push_back(a), hs.tri.push_back(b), hs.tri.push_back(c);
    }
    hs.ready = true;
    hand_skin_taken[h]++;
    char reach[400];
    int at = 0;
    for (int f = 0; f < 5; f++)
        at += std::snprintf(reach + at, sizeof reach - at, " %c %.1f/%.1f/%.1f (%d/%d/%d)", "TIMRL"[f],
                            hs.reach[f][0] * 1000.0f, hs.reach[f][1] * 1000.0f, hs.reach[f][2] * 1000.0f,
                            (int)hs.seg[f][0].size(), (int)hs.seg[f][1].size(), (int)hs.seg[f][2].size());
    rt_log("VR: her %s hand as drawn taken for her grips and fingers (frame %d, joints %.2f mm off this model's): %d "
           "vertices (her palm %d, blended between bones %d, on a finger's segment %d, her wrist on her forearm too "
           "%d), %d triangles; each finger segment's own skin reaches from its line (mm, vertices):%s",
           kSide[h], d.frame, worst * 1000.0f, d.n,
           counts[0], counts[1], counts[2], counts[3], (int)hs.tri.size() / 3, reach);
}

// 0.1.8: hand h at `pose` against the weapon: her palm's skin's nearest (metres), and a knuckle or her palm in it (the
// other hand laid on a part of the weapon where it takes hold: sh3vr_combat.cpp surface_hold_at).
void body_palm_touch(int h, const Pose& pose, float& palm_clear, bool& inside) {
    palm_clear = 1e9f;
    inside = false;
    if (h < 0 || h > 1) return;
    for (int f = 0; f < 5; f++) {
        Vec3 p[4];
        finger_points(h, f, 0.0f, p);
        const Vec3 k = grip_point(pose, p[0]);
        if (weapon_clearance(h, k, k) < 0.002f) inside = true;
    }
    Vec3 pp[18];
    const int np = palm_points(h, pp);
    for (int i = 0; i < np; i++) {
        const Vec3 q = grip_point(pose, pp[i]);
        const float d = weapon_clearance(h, q, q);
        if (d < -0.002f) inside = true;
        if (i % 2 == 0) palm_clear = std::min(palm_clear, d);
    }
}

// 0.1.8 (owner message 146: "the fuckin finger isnt even on the trigger"): the trigger finger closing on the gun's
// trigger (its blade as a capsule) from straight along the frame: the curl where it first meets it, kept only where it
// meets it with its pad (the end segment's middle on the blade; err: how far off, metres), not with its root.
// Session 31 (session 30's vr380: her straight index runs across the gun, so where her other fingers wrap the grip its
// sweep passes beside the trigger's blade, never meeting it): when the sweep does not meet the blade, the pad's nearest
// pass is taken if its skin comes within 3 mm of the blade (err: that gap), as a finger laid on a trigger's side.
// Session 31 (vr381: on the shotgun and the submachine gun the trigger sits above where her straight index reaches with
// her other fingers round the grip, so no place was found for either): the finger may also spread sideways at its
// knuckle, as a real index does reaching for a trigger (up to 16 degrees each way, the least that works taken: 0, 4, -4,
// 8, ...; `spread`, degrees, finger_points'); a pad more than 10 mm off the blade where the sweep meets it (its root or
// middle on it instead), a curl outside 0.12-0.85, or one that would lay the finger more than 3 mm into the gun itself,
// is passed over.
namespace {
constexpr float kTriggerSpreads[9] = {0, 4, -4, 8, -8, 12, -12, 16, -16};
// The index at curl c and spread sp no more than 3 mm into the weapon (its skin).
bool index_clear(int h, const Pose& pose, float c, float sp) {
    Vec3 p[4];
    finger_points(h, kIndex, c, p, sp);
    for (int j = 0; j < 3; j++)
        if (weapon_clearance(h, grip_point(pose, p[j]), grip_point(pose, p[j + 1])) < 0.0085f - 0.003f) return false;
    return true;
}
}  // namespace
// The trigger finger at spread sp on the blade `trig`: the curl where it closing meets it with its pad (its skin within
// 10 mm of it there), else where its pad passes nearest it (within 3 mm); -1 neither, or straight it starts in it.
// gap: the pad's skin's distance from the blade there (metres).
static float pad_on_blade(int h, const Pose& pose, const GripTrigger& trig, float sp, float& gap) {
    gap = 1.0f;
    const Quat inv = conjugate(pose.orientation);
    PartCaps pc;
    pc.n = 1;
    pc.a[0] = rotate(inv, trig.a - pose.position);
    pc.b[0] = rotate(inv, trig.b - pose.position);
    pc.r[0] = trig.r + trig.pad;
    auto pad_gap = [&](float cc) {
        Vec3 p[4];
        finger_points(h, kIndex, cc, p, sp);
        const Vec3 pad = p[2] + (p[3] - p[2]) * 0.55f;
        return seg_dist(pad, pad, pc.a[0], pc.b[0]) - pc.r[0] - 0.0085f;
    };
    float free_from = 0;
    const float c = finger_contact(h, kIndex, pose, &pc, false, 1.0f, free_from, sp);
    if (free_from != 0.0f) return -1.0f;  // (straight, it starts in the blade)
    if (c >= 0) {
        gap = std::max(0.0f, pad_gap(c));
        return gap <= 0.010f ? c : -1.0f;
    }
    float best = 1e9f, best_c = -1.0f;
    for (int i = 0; i <= 40; i++) {
        const float g = pad_gap(i * 0.025f);
        if (g < best) best = g, best_c = i * 0.025f;
    }
    if (best_c >= 0 && best <= 0.003f) {
        gap = std::max(0.0f, best);
        return best_c;
    }
    return -1.0f;
}
float body_trigger_curl(int h, const Pose& pose, const GripTrigger& trig, float* err, float* spread) {
    if (err) *err = 1.0f;
    if (spread) *spread = 0.0f;
    if (!trig.on || h < 0 || h > 1) return -1.0f;
    for (const float sp : kTriggerSpreads) {
        float gap = 1.0f;
        const float got = pad_on_blade(h, pose, trig, sp, gap);
        // (a natural trigger finger's curl: 0.12 to 0.85, as the fit asks, body_grip_fit)
        if (got < 0.12f || got > 0.85f || !index_clear(h, pose, got, sp)) continue;
        if (err) *err = gap;
        if (spread) *spread = sp;
        return got;
    }
    return -1.0f;
}

// Session 31 (owner message 151: "make sure the fingers can actually be seen properly pulling the triggers"): as the
// trigger turns back about its pin (sh3vr_combat.cpp kTriggers), her finger follows it closing, its pad on the blade at
// each quarter of the pull (at the spread it has on it); between them, in proportion. Where the pad would lose the blade
// (its middle meeting it first, or no pass near it), the curls from there rise evenly to 0.1 more (0.1.8's first pull).
bool body_trigger_pull_curls(int h, const Pose& pose, float ct, float sp, float out[5]) {
    out[0] = ct;
    bool ok = h >= 0 && h < 2;
    for (int i = 1; i <= 4; i++) {
        float gap = 1.0f;
        const GripTrigger t = ok ? weapon_trigger_at(i * 0.25f) : GripTrigger{};
        const float c = t.on ? pad_on_blade(h, pose, t, sp, gap) : -1.0f;
        if (!(c >= out[i - 1] + 1e-4f && c <= 1.0f)) ok = false;
        out[i] = ok ? c : out[i - 1] + 0.025f;
    }
    return ok;
}

// Session 31 (session 30's list): the trigger finger off the trigger: straight beside the frame (curl 0.06), else
// lifted off the trigger guard (0, -0.1, -0.2: a shooter's finger laid along the frame above the guard), the first whose
// skin is no more than 3 mm into the gun; -1 when each is deeper. Unspread first, then at `prefer` (the spread it has on
// the trigger); `spread`: the one taken.
float body_trigger_rest(int h, const Pose& pose, float prefer, float* spread) {
    if (spread) *spread = 0.0f;
    if (h < 0 || h > 1) return -1.0f;
    for (const float sp : {0.0f, prefer}) {
        for (const float c : {0.06f, 0.0f, -0.1f, -0.2f})
            if (index_clear(h, pose, c, sp)) {
                if (spread) *spread = sp;
                return c;
            }
        if (prefer == 0.0f) break;
    }
    return -1.0f;
}

// 0.1.8 (owner message 146: "by default though, make ALL guns and weapons/melee etc, grip properly and accurately"):
// hand h's hold on the weapon at `pose`, for the grips' fit (sh3vr_combat.cpp fit_step), as the flashlight's is scored
// (hold_score) and as VR games pose a hand on a grab point (its fingers closing until they meet the object's shape, the
// trigger finger on its own trigger pose): her knuckles and palm out of the weapon, her palm against it, every finger
// starting clear of it and closing onto it (the main hand's index onto the trigger, when the gun has one), scored by
// how much of her palm lies on it, how many of her finger joints lie along it, the trigger finger's pad on the trigger
// and the fingers' curls near a natural grip's.
// 0.1.10 (owner 172, session 38; vr517: the handgun's hand "fitted" at the foot of its grip, the gun sat on her fist and
// her index rising 15 degrees to the trigger): how steeply the trigger finger may rise to the trigger (degrees up the
// gun), the degrees free of the score's weight and that weight per degree. A shooter's hand sits high on the grip, the
// web of it up under the slide, its trigger finger running level along the frame or a little down to the trigger. The
// fit alone reads these (body_grip_fit runs only there): SH3_FIT_RISE, SH3_FIT_RISE_FREE, SH3_FIT_RISE_W on the rig.
static float fit_env(const char* name, float dflt) {
    const char* v = std::getenv(name);
    return v && *v ? (float)std::atof(v) : dflt;
}
static float fit_rise_up() {
    static const float v = fit_env("SH3_FIT_RISE", 25.0f);
    return v;
}
static float fit_rise_free() {
    static const float v = fit_env("SH3_FIT_RISE_FREE", 8.0f);
    return v;
}
static float fit_rise_weight() {
    static const float v = fit_env("SH3_FIT_RISE_W", 0.03f);
    return v;
}
// Session 38 (owner 178: "make sure the weapon grips look natural. who ... holds a smg like that"): the wrist. Her arm
// follows the controller's forearm (sh3vr_ik.cpp), so a hand drawn turned far from her real hand (the controller) bends
// her drawn wrist that far: vr530's handgun's other hand came in from the side, its wrist bent, where 0.1.9's came up
// from below its grip. body_wrist_swing: the angle between the drawn hand's wrist-to-knuckles line and the controller's
// (a turn about that line her forearm takes up, as a real forearm turns). The fit turns away an other hand whose wrist
// bends past fit_wrist_most (degrees) and weighs each degree past fit_wrist_free against the place; the rig:
// SH3_FIT_WRIST, SH3_FIT_WRIST_FREE, SH3_FIT_WRIST_W. The main hand's is logged, not limited (vr530: every main hand
// looked natural; its place on a gun keeps the gun's aim on the controller). Off unless asked (vr533): the rig's
// controllers both point ahead, as no player holds a pump or a gun's front, so a cupped hold reads 86 to 90 degrees
// there (the shotgun's pump, the submachine gun's front, the flamethrower's) and a 40-degree limit turned them all away;
// the pictures' own reading (her wrist bent as drawn against her forearm, ik_wrist_bend) is what the places were
// chosen by.
static float fit_wrist_most() {
    static const float v = fit_env("SH3_FIT_WRIST", 180.0f);
    return v;
}
static float fit_wrist_free() {
    static const float v = fit_env("SH3_FIT_WRIST_FREE", 15.0f);
    return v;
}
static float fit_wrist_weight() {
    static const float v = fit_env("SH3_FIT_WRIST_W", 0.0f);
    return v;
}
float body_wrist_swing(int h, const Pose& pose) {
    if (h < 0 || h > 1 || !hands[h].valid) return 0.0f;
    // (her grip space: the knuckles ahead of the palm, -Y, either hand)
    const Vec3 a = rotate(pose.orientation, Vec3{0, -1, 0}), b = rotate(hands[h].grip.orientation, Vec3{0, -1, 0});
    return std::acos(std::clamp(dot(a, b), -1.0f, 1.0f)) * 57.29578f;
}

void body_grip_fit(int h, const Pose& pose, bool main, const GripTrigger& trig, GripFit& out) {
    out = GripFit{};
    if (h < 0 || h > 1) return;
    out.wrist = body_wrist_swing(h, pose);
    if (!main && out.wrist > fit_wrist_most()) {
        out.why = 8;
        return;
    }
    // (session 38, vr555/556: her forearm as drawn there kept out of the weapon too, SH3_FIT_ARM=1: the left-handed
    // maul's end went 10 mm into it, the right-handed flamethrower's body 6.4, the left-handed beam saber's hilt 5.1)
    static const bool fit_arm = std::getenv("SH3_FIT_ARM") != nullptr;
    if (fit_arm) {
        out.arm = combat_forearm_clear(h, pose);
        if (out.arm < kSkinCheck) {
            out.why = 9;
            return;
        }
    }
    auto clear = [&](Vec3 l) {
        const Vec3 p = grip_point(pose, l);
        return weapon_clearance(h, p, p);
    };
    auto seg_clear = [&](Vec3 la, Vec3 lb) { return weapon_clearance(h, grip_point(pose, la), grip_point(pose, lb)); };
    for (int f = 0; f < 5; f++) {
        Vec3 p[4];
        finger_points(h, f, 0.0f, p);
        if (clear(p[0]) < 0.004f) {
            out.why = 1;
            return;
        }
    }
    int near_palm = 0, faces = 0;
    out.skin = 1.0f;
    if (palm_skin[h].empty()) {
        Vec3 pp[18];
        const int np = palm_points(h, pp);
        for (int i = 0; i < np; i++) {
            const float d = clear(pp[i]);
            if (d < -0.002f) {
                out.why = 2;
                return;
            }
            if (i % 2 == 0) faces++, near_palm += d < 0.012f;  // palm_points: the palm's side first, then the back's
        }
    } else {
        // 0.1.10 (owner 172: "WHY ... IS THE GUN STILL CLIPPING THROUGH THE HAND"): her palm as drawn (body_palm_skin)
        // is the palm here, in and against the weapon. Her hand is the game's: its palm as drawn runs from x 0.0 to 5.2
        // cm in the grip's space (the right hand's; vr512), the hand model's palm_points from 1.8 to 4.4, so its face
        // lies 1.8 cm nearer the grip than the model's. 0.1.8's fit kept the model's palm out of the weapon and against
        // it (palm_points within 1.2 cm), so the grip lay up to 1.9 cm into her palm as drawn (vr511). Builds 279 to 287
        // turned away a place with her drawn palm more than 1 cm in but still asked the model's palm against it: no
        // place on any gun's main hand then had both (vr504, vr512). Now no part of her drawn palm may be more than 1 mm
        // in (body_grip_mesh then asks her whole hand as drawn out of it), and her palm against it is her drawn palm's
        // face (its side of her knuckles' line) within 1 cm of it, its nearest within 4 mm.
        Vec3 k[4];
        finger_points(h, kMiddle, 0.0f, k);
        const float mx = h == kLeft ? -1.0f : 1.0f;
        for (const Vec3& l : palm_skin[h]) {
            const float d = clear(l);
            out.skin = std::min(out.skin, d);
            if (mx * (l.x - k[0].x) < 0) faces++, near_palm += d < 0.010f;
        }
        if (out.skin < -0.001f) {
            out.why = 2;
            return;
        }
        if (out.skin > 0.004f) {
            out.why = 7, out.sub = 1;
            return;
        }
    }
    const bool trigger = main && trig.on;
    // vr370 (every place turned away, a finger in): a hand laid on a grip has its fingers in it when straight, as a real
    // one would; what counts is that each comes out of it early as it opens (a third of its curl at most, the thumb
    // half), never buried at every curl, and then closes onto it from outside (body_fingers takes that curl, as here).
    // vr371 (the thumb turned away most places, 2932 of the handgun's 4102): her thumb straight stands up into a gun's
    // frame, and a thumb on a gun lies along the frame or round the grip: it rests where it is clear (thumb_rest), as
    // body_fingers draws it.
    // Session 31 (vr384: the handgun's thumb closing met the frame with its root and stood out, its tip 1.5 cm off): the
    // thumb always rests as thumb_rest lays it (swung at its root, its pad's segment on the weapon), as drawn.
    for (int f = 0; f < 5; f++) {
        if (f == kIndex && trigger) continue;
        float free_from = 0;
        if (f == kThumb) {
            bool rests = false;
            out.curl[f] = thumb_rest(h, pose, &rests, &out.thumb_in, &out.thumb_swing);
            if (out.curl[f] < -0.5f || out.thumb_in > 0.006f) {  // (its root in the weapon, or its pad 6 mm past resting)
                out.why = 3;
                out.finger = f;
                return;
            }
            if (!rests) out.curl[f] = -1.0f;  // (clear of it, not on it: no thumb's share in the score)
            continue;
        }
        out.curl[f] = finger_contact(h, f, pose, nullptr, true, 1.0f, free_from);
        if (free_from < 0 || free_from > 0.5f) {
            out.why = 3;
            out.finger = f;
            return;
        }
        out.opened += free_from;
        // (session 38, owner 178; vr530: the maul's other hand's index lay straight along the shaft, curled 0.14, as
        // pointing: on a melee weapon's handle every finger of either hand closes round it, a third of the way at least)
        const bool melee = !weapon_is_gun();
        const bool needed = f >= kMiddle || (f == kIndex && (main || melee));
        // (vr371: the pipe's other hand "fitted" lying along it, its fingers meeting it at 0.1 to 0.2, nearly straight:
        // a hand round a part curls a third of the way at least. 0.1.10, vr513: with her palm as drawn, its heel 1.8 cm
        // thicker than the hand model's, a grip against it meets the fingers sooner: an eighth at least, and each
        // finger then closes joint by joint round it, finger_wrap; the score still weighs a curl off a natural grip's)
        const float least = palm_skin[h].empty() || melee ? 0.3f : 0.12f, most = palm_skin[h].empty() ? 0.95f : 1.0f;
        if (needed && !(out.curl[f] >= least && out.curl[f] <= most)) {
            out.why = 4, out.sub = out.curl[f] < 0 ? 1 : out.curl[f] < least ? 2 : 3;
            out.finger = f;
            return;
        }
    }
    if (trigger) {
        float err = 1.0f, sp = 0.0f;
        out.trig_curl = body_trigger_curl(h, pose, trig, &err, &sp);
        out.trig_err = err;
        out.trig_spread = sp;
        // (session 31, vr388: a finger meeting the blade with its middle "fitted" with its pad 5.6 mm off it, a gap seen
        // between them: the pad itself on the trigger, within 2.5 mm)
        if (out.trig_curl < 0.12f || out.trig_curl > 0.85f || err > 0.0025f) {
            out.why = 6, out.sub = 1;
            return;
        }
        // On the trigger, and off it lying straight beside the frame or lifted off the guard (body_trigger_rest):
        // never more than 3 mm into the gun.
        float rest_sp = 0.0f;
        const float rest = body_trigger_rest(h, pose, sp, &rest_sp);
        if (rest < -0.5f) {
            out.why = 6, out.sub = 2;
            return;
        }
        // Session 31 (vr382: the submachine gun's hand "fitted" 4 cm down its grip, holding the magazine, her index rising
        // steeply up to the trigger): a shooter's hand sits high on the grip, its trigger finger running along the gun to
        // the trigger, so from its knuckle to its pad it rises no more than 25 degrees up the gun (nor falls more than 35,
        // the hand over the frame).
        {
            Vec3 p[4];
            finger_points(h, kIndex, out.trig_curl, p, sp);
            const Vec3 v = grip_point(pose, p[2] + (p[3] - p[2]) * 0.55f) - grip_point(pose, p[0]);
            const float L = length(v);
            out.trig_rise = L > 1e-4f && length(trig.up) > 0.5f
                                ? std::asin(std::clamp(dot(v, trig.up) / L, -1.0f, 1.0f)) * 180.0f / kPi
                                : 0.0f;
            if (out.trig_rise > fit_rise_up() || out.trig_rise < -35.0f) {
                out.why = 6, out.sub = 3;
                return;
            }
        }
        const float tried[2][2] = {{out.trig_curl, sp}, {rest, rest_sp}};
        for (const auto& t : tried) {
            Vec3 p[4];
            finger_points(h, kIndex, t[0], p, t[1]);
            for (int j = 0; j < 3; j++)
                if (seg_clear(p[j], p[j + 1]) < 0.0085f - 0.003f) {
                    out.why = 6, out.sub = 4;
                    return;
                }
        }
        // Session 31 (vr386; owner message 151: "make sure the fingers can actually be seen properly pulling the
        // triggers"): the shotgun's finger lay nearly straight along the blade (curl 0.16, closing only 0.05 over the
        // whole pull) and the submachine gun's lost the blade part of the way. A finger pulls a trigger: its pad, closing,
        // goes the way the blade goes as it is pulled (`trig_push`, the cosine between the two at the blade's point
        // nearest the pad: none under 0.3), and stays on it the whole pull (`trig_whole`, body_trigger_pull_curls); the
        // score below asks those, a shooter's hooked finger (about 0.4: its joints 32, 38 and 26 degrees) and the finger
        // near level from its knuckle (8 degrees free).
        {
            Vec3 a[4], b[4];
            finger_points(h, kIndex, out.trig_curl, a, sp);
            finger_points(h, kIndex, std::min(out.trig_curl + 0.05f, 1.0f), b, sp);
            const Vec3 pa = grip_point(pose, a[2] + (a[3] - a[2]) * 0.55f);
            const Vec3 pb = grip_point(pose, b[2] + (b[3] - b[2]) * 0.55f);
            const GripTrigger t1 = weapon_trigger_at(1.0f);
            const Vec3 ab = trig.b - trig.a;
            const float t = std::clamp(dot(pa - trig.a, ab) / std::max(dot(ab, ab), 1e-12f), 0.0f, 1.0f);
            const Vec3 m = (t1.a + (t1.b - t1.a) * t) - (trig.a + ab * t), v = pb - pa;
            const float lm = length(m), lv = length(v);
            out.trig_push = t1.on && lm > 1e-6f && lv > 1e-6f ? dot(v, m) / (lm * lv) : 0.0f;
            if (out.trig_push < 0.3f) {
                out.why = 6, out.sub = 5;
                return;
            }
            float pc[5];
            out.trig_whole = body_trigger_pull_curls(h, pose, out.trig_curl, sp, pc);
        }
        out.curl[kIndex] = out.trig_curl;
    }
    // (session 31, vr386: the submachine gun's other hand "fitted" its palm on the receiver's side, its fingers meeting
    // the top edge with their roots and standing 2 cm above it: a finger round a part has its pad on it, or near it.
    // `pads`: how far each finger's pad, 55% along its last segment, is off the weapon past 1 cm (metres, summed; the
    // trigger finger on the trigger is the trigger's). vr388 asked them within 3 mm: her fingers close with every joint
    // together, so one meeting a grip's edge with its middle first keeps its pad a few millimetres off, and the fit
    // then laid the handgun's hand flat along the grip, its fingers barely curled)
    int along = 0, joints = 0;
    for (int f = kIndex; f <= kPinky; f++) {
        if (out.curl[f] < 0) continue;
        Vec3 p[4];
        finger_points(h, f, out.curl[f], p, f == kIndex && trigger ? out.trig_spread : 0.0f);
        for (int j = 1; j < 4; j++, joints++) along += clear(p[j]) < 0.012f;
        if (!(f == kIndex && trigger)) out.pads += std::max(0.0f, clear(p[2] + (p[3] - p[2]) * 0.55f) - 0.0085f - 0.010f);
    }
    out.palm = faces ? near_palm / (float)faces : 0.0f;
    out.wrap = joints ? along / (float)joints : 0.0f;
    // (her drawn palm's face: its nearest within 4 mm (above) and an eighth of it within 1 cm, a tenth for the other
    // hand, its heel bulging (vr513); the model's palm_points, 0.1.8: 0.3 and 0.25 of its points within 1.2 cm)
    const bool drawn_palm = !palm_skin[h].empty();
    if (out.palm < (drawn_palm ? (main ? 0.12f : 0.1f) : (main ? 0.3f : 0.25f))) {
        out.why = 7, out.sub = 2;
        return;
    }
    float natural = 0;
    for (int f = kMiddle; f <= kPinky; f++) natural += std::fabs(out.curl[f] - 0.62f);
    // (session 31: a spread trigger finger weighs a little against the place, so the least spread hand wins; and the
    // trigger finger pulling it as a shooter's does, above)
    const float pulling = trigger ? std::fabs(out.trig_curl - 0.4f) * 1.5f + (1.0f - out.trig_push) * 1.5f +
                                        (out.trig_whole ? 0.0f : 0.8f) +
                                        std::max(0.0f, std::fabs(out.trig_rise) - fit_rise_free()) * fit_rise_weight()
                                  : 0.0f;
    out.score = 4.0f + out.palm * 3.0f + out.wrap * 3.0f + (out.curl[kThumb] > -0.5f ? 0.4f : 0.0f) -
                out.trig_err * 60.0f - natural * 1.5f - out.opened * 1.5f - out.thumb_in * 300.0f -
                std::fabs(out.trig_spread) * 0.02f - pulling - out.pads * 100.0f -
                std::max(0.0f, 0.0005f - out.skin) * 500.0f -
                (main ? 0.0f : std::max(0.0f, out.wrist - fit_wrist_free()) * fit_wrist_weight());
}

// 0.1.10 (owner 172): hand h at `pose` on the weapon as body_fingers draws it there at rest (role 1 the main hand, 2
// the other; the same calls): each finger's joints' curls (c3) and the spreads (sp: the thumb's swing, the index's
// spread), the main hand's trigger finger on the trigger, not pulled (on), or off it.
static void grip_drawn_curls(int h, const Pose& pose, int role, bool on, float c3[5][3], float sp[5]) {
    const float hold = weapon_hold_curl(h);
    for (int f = 0; f < 5; f++) {
        sp[f] = 0.0f;
        float target = 0.0f, free_from = 0.0f, jw[3];
        bool wrapped = false;
        const GripTrigger tg = role == 1 && f == kIndex && weapon_is_gun() ? weapon_trigger() : GripTrigger{};
        if (tg.on) {
            float sp_on = 0.0f, sp_off = 0.0f;
            const float ct = body_trigger_curl(h, pose, tg, nullptr, &sp_on);
            const float rest = body_trigger_rest(h, pose, sp_on, &sp_off);
            if (ct >= 0) {
                target = on ? ct : rest > -0.5f ? rest : std::min(0.06f, ct);
                sp[f] = on || rest < -0.5f ? sp_on : sp_off;
            } else {
                const float c = finger_contact(h, kIndex, pose, nullptr, true, 1.0f, free_from);
                target = std::min(on ? 0.42f : 0.10f, c >= 0 ? c : 1.0f);
            }
        } else {
            const float c = finger_contact(h, f, pose, nullptr, true, 1.0f, free_from);
            const float th = f == kThumb ? weapon_hold_thumb(h) : -1.0f;
            const float base = f == kThumb ? (th >= 0 ? th : std::min(hold, 0.7f)) : hold;
            target = c >= 0 ? c : std::max(base, free_from);
            if (f != kThumb && c >= 0) wrapped = finger_wrap(h, f, pose, c, jw);
            if (f == kThumb && grip_fit_mode_on()) {
                float sw = 0.0f;
                const float tr = thumb_rest(h, pose, nullptr, nullptr, &sw);
                if (tr > -0.5f) target = tr, sp[f] = sw;
            }
        }
        for (int j = 0; j < 3; j++) c3[f][j] = std::clamp(wrapped ? jw[j] : target, kFingerBackMost, 1.0f);
    }
}

void body_grip_mesh(int h, const Pose& pose, bool main, GripFit& out, bool any) {
    if (h < 0 || h > 1 || !hand_skin[h].ready || (out.score < 0 && !any)) return;
    const bool trigger = main && weapon_is_gun() && weapon_trigger().on;
    float least = 1e9f, palm = 1e9f;
    int part = -1;
    for (int pass = 0; pass < (trigger ? 2 : 1); pass++) {
        float c3[5][3], sp[5], pm = 1e9f;
        int pt = -1;
        grip_drawn_curls(h, pose, main ? 1 : 2, pass == 0, c3, sp);
        const float m = hand_skin_clear(h, pose, c3, sp, &pt, &pm);
        if (m < least) least = m, part = pt;
        palm = std::min(palm, pm);
        // (session 38: the other hand against her main hand as drawn too, part 7)
        if (!main) {
            const float b = hand_skin_between(h, pose, c3, sp);
            if (b < least) least = b, part = 7;
        }
    }
    out.mesh = least, out.mesh_part = part, out.mesh_palm = palm;
    if (out.score < 0) return;  // (any: measured only)
    if (least < kSkinCheck) {
        out.why = 5;
        out.score = -1.0f;
        return;
    }
    // (her palm against the weapon: its skin's nearest past 2 mm off it weighs against the place, 0.3 a millimetre)
    out.score -= std::max(0.0f, palm - 0.002f) * 300.0f;
}

// The rig's record of a hand's place on the weapon (the grips' fit, SH3_GRIP_FIT=1): each finger's knuckle clearance
// (mm), its curl meeting the weapon and the curl it comes out of it from, the thumb's rest; the palm's nearest and
// farthest skin (mm).
void body_grip_report(int h, const Pose& pose, char* out, int size) {
    int n = 0;
    for (int f = 0; f < 5 && n < size; f++) {
        Vec3 p[4];
        finger_points(h, f, 0.0f, p);
        const Vec3 k = grip_point(pose, p[0]);
        float free_from = 0;
        const float c = finger_contact(h, f, pose, nullptr, true, 1.0f, free_from);
        // (vr377: and each segment's skin from the weapon at curls 0 and 0.6, mm: which part of it is in)
        float sg[2][3];
        for (int q = 0; q < 2; q++) {
            Vec3 s[4];
            finger_points(h, f, q ? 0.6f : 0.0f, s);
            for (int j = 0; j < 3; j++)
                sg[q][j] = (weapon_clearance(h, grip_point(pose, s[j]), grip_point(pose, s[j + 1])) -
                            (f == kThumb ? 0.0095f : 0.0085f)) * 1000.0f;
        }
        // (vr380: the curls where it is out of the weapon, its skin at most 1 mm in: the first such run)
        float w0 = -1, w1 = -1;
        for (int i = 0; i <= 20; i++) {
            const float cc = i * 0.05f;
            Vec3 s[4];
            finger_points(h, f, cc, s);
            float most_in = 1e9f;
            for (int j = 0; j < 3; j++)
                most_in = std::min(most_in, weapon_clearance(h, grip_point(pose, s[j]), grip_point(pose, s[j + 1])));
            const bool out_of = most_in >= (f == kThumb ? 0.0095f : 0.0085f) - 0.001f;
            if (out_of) {
                if (w0 < 0) w0 = cc;
                w1 = cc;
            } else if (w0 >= 0) break;
        }
        n += std::snprintf(out + n, size - n, "%s%c k%.0f c%.2f o%.2f w%.2f-%.2f [%.0f %.0f %.0f | %.0f %.0f %.0f]",
                           f ? ", " : "", "TIMRL"[f], weapon_clearance(h, k, k) * 1000.0f, c, free_from, w0, w1, sg[0][0],
                           sg[0][1], sg[0][2], sg[1][0], sg[1][1], sg[1][2]);
    }
    bool rests = false;
    float tin = 0;
    float tsw = 0;
    const float tr = thumb_rest(h, pose, &rests, &tin, &tsw);
    Vec3 pp[18];
    const int np = palm_points(h, pp);
    float lo = 1e9f, hi = -1e9f;
    for (int i = 0; i < np; i += 2) {
        const Vec3 q = grip_point(pose, pp[i]);
        const float d = weapon_clearance(h, q, q);
        lo = std::min(lo, d), hi = std::max(hi, d);
    }
    if (n < size)
        std::snprintf(out + n, size - n, "; thumb rest %.2f swung %.1f%s (in %.1f mm); palm %.0f..%.0f mm", tr, tsw,
                      rests ? " on it" : "", tin * 1000.0f, lo * 1000.0f, hi * 1000.0f);
}

Vec3 body_finger_joint(int h, const Pose& pose, int f, float c, int j) {
    if (h < 0 || h > 1 || f < 0 || f > 4) return pose.position;
    Vec3 p[4];
    finger_points(h, f, c, p);
    return grip_point(pose, p[std::clamp(j, 0, 3)]);
}

float weapon_wall_free() { return answer.valid && answer.age <= 3 ? weapon_free : 1.0f; }
const char* weapon_wall_line() { return weapon_hit_line; }

bool guide_hit(Vec3& at) {
    if (!guide_valid || guide_age > 3 || !map_ready()) return false;
    at = tracking_point(guide_world);
    return true;
}

// Session 10: how near the world stands right before the eyes during a scene in first person (metres, the nearest
// surface any of the view's probes is behind; a post 4 degrees wide meets one way: the caller asks it to last); 0 when
// nothing is that near; -1 when not known (no answer of the last 3 frames asked them). `seen`: the probes on the
// picture with a surface behind them, `behind`: those past one.
float view_blocked(int* seen, int* behind_out) {
    int behind = 0, on = 0;
    float nearest = kViewFar;
    const bool known = answer.valid && answer.age <= 3 && answer.view_asked;
    for (int i = 0; known && i < kViewSamples; i++) {
        if (!answer.view_ok[i]) continue;
        on++;
        if (answer.view_pen[i] <= 0.0f) continue;
        behind++;
        nearest = std::min(nearest, std::max(0.0f, (i % 2 ? kViewFar : kViewNear) - answer.view_pen[i]));
    }
    if (seen) *seen = on;
    if (behind_out) *behind_out = behind;
    if (!known) return -1.0f;
    return behind >= 1 ? std::max(nearest, 0.01f) : 0.0f;
}

void body_draw(unsigned bb_tex, unsigned depth_tex, int w, int h) {
    if (!map_ready() || !camera_ready() || !gl_init()) return;
    if (view_mode != ViewMode::kFirstPerson && view_mode != ViewMode::kCamera) return;
    GlSave save;
    // The probes' last answer, then this frame's question (the depth the game drew).
    for (ProbeSet& s : sets) read_answer(s);
    if (depth_tex) issue_probes(depth_tex);

    // Comfort: the vignette and the head-in-wall darkening over both eyes.
    const float vignette = comfort_vignette(), fade = head_in_wall();
    static float vig_shown = 0;
    vig_shown += (vignette - vig_shown) * ease(8.0f);
    if (vig_shown < 0.01f && fade < 0.01f) return;
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, draw_fbo_mv);
    glFramebufferTextureMultiviewOVR(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, bb_tex, 0, 0, 2);
    glViewport(0, 0, w, h);
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_CULL_FACE);
    glColorMask(1, 1, 1, 1);
    glBindVertexArray(vao);
    glUseProgram(comfort_prog);
    float centers[4];
    for (int e = 0; e < 2; e++) {
        const float L = std::tan(fi.fov[e][0]), R = std::tan(fi.fov[e][1]);
        const float U = std::tan(fi.fov[e][2]), D = std::tan(fi.fov[e][3]);
        centers[e * 2] = R - L > 1e-4f ? -(R + L) / (R - L) : 0.0f;
        centers[e * 2 + 1] = U - D > 1e-4f ? (U + D) / (U - D) : 0.0f;  // GL's y is the D3D one negated
    }
    if (c_eye_center >= 0) {
        glUniform2f(c_eye_center, centers[0], centers[1]);
        const GLint second = glGetUniformLocation(comfort_prog, "uCenter[1]");
        if (second >= 0) glUniform2f(second, centers[2], centers[3]);
    }
    glUniform2f(c_vignette, 1.0f - 0.55f * vig_shown, std::min(1.0f, vig_shown * 1.2f));
    glUniform1f(c_fade, fade * 0.92f);
    glUniform2f(c_size, (float)w, (float)h);
    glEnable(GL_BLEND);
    glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ZERO, GL_ONE);
    glBlendEquation(GL_FUNC_ADD);
    glDisable(GL_DEPTH_TEST);
    glDepthMask(0);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glFramebufferTextureMultiviewOVR(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, 0, 0, 0, 2);
}

void body_crash_info(FILE* f) {
    for (int h = 0; h < 2; h++) {
        const HandState& s = hands[h];
        if (!s.valid) {
            std::fprintf(f, "VR %s hand: not tracked\n", h == kLeft ? "left" : "right");
            continue;
        }
        std::fprintf(f, "VR %s hand: at %.3f %.3f %.3f, speed %.2f m/s, curls %.2f %.2f %.2f %.2f %.2f, stops %.2f %.2f "
                        "%.2f %.2f %.2f, inside %d, weapon %d, foregrip %d, her own finger shapes %s\n",
                     h == kLeft ? "left" : "right", s.grip.position.x, s.grip.position.y, s.grip.position.z, s.speed,
                     s.curl[0], s.curl[1], s.curl[2], s.curl[3], s.curl[4], s.stop[0], s.stop[1], s.stop[2], s.stop[3],
                     s.stop[4], s.inside, s.holding_weapon, s.on_foregrip, her_hands[h].valid ? "read" : "not yet");
        std::fprintf(f, "VR %s hand on the world: %s, %d plane(s)%s fitted %d frames ago, the palm moved %.1f cm out of "
                        "it\n",
                     h == kLeft ? "left" : "right", s.resting ? "resting on a surface" : "free", hand_at[h].n,
                     hand_at[h].implied ? " (an edge, implied)" : hand_at[h].convex ? " (an edge)" : "", kept[h].age, length(s.push) * 100.0f);
        std::fprintf(f, "VR %s fingers: on the weapon as %d (0 free, 1 its grip, 2 its second grip, 3 the slide), part %d, "
                        "chosen curls %.2f %.2f %.2f %.2f %.2f, touching the weapon %d\n",
                     h == kLeft ? "left" : "right", weapon_hand_role(h), reload_held_part(h), finger_target[h][0],
                     finger_target[h][1], finger_target[h][2], finger_target[h][3], finger_target[h][4], weapon_touch[h]);
    }
    std::fprintf(f, "VR probes: answer %s (age %d, %d on her own body, %d of them by her stencil mark; the stencil pass "
                    "%s), guide dot %s, GL %s, the weapon's line %.0f%% free\n",
                 answer.valid ? "read" : "none", answer.age, answer.own_hits, answer.own_stencil,
                 stencil_reads == 1 ? "on" : stencil_reads == 0 ? "off" : "not tried", guide_valid ? "on a surface" : "none",
                 gl_ready ? "ready" : gl_failed ? "failed" : "not yet", weapon_free * 100.0f);
}

// Session 11: the shader helpers for sh3vr.cpp's UI pass (a compile or link error logged, 0 returned).
unsigned gl_link_program(const char* vs, const char* fs, const char* what) { return link(vs, fs, what); }

}  // namespace sh3q
