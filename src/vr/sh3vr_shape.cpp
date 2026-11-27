// SPDX-License-Identifier: GPL-3.0-or-later
// SH3 Quest VR (0.1.8): each weapon's own shape, from its mesh as the game draws it in her hand.
//
// Owner messages 146-148 ("we already have proper world collision with finger reactions and such, and i recall asking
// for that collision to also be applicable to the guns. it wasnt", "guns/weapons/melee weapons, etc."): her fingers
// stopped on a few capsules round each weapon (sh3vr_combat.cpp kShapes), which are not the weapon: the handgun's grip
// is a raked block with straps and a guard, the submachine gun's a thin slab, the knife's handle flat, so a finger closed
// inside the grip or stopped short of it. VR games close each finger until it meets the object's own collider
// (HurricaneVR's and Auto Hand's finger sweeps, Meta's hand grabs, Half-Life: Alyx's hands); the collider here is the
// weapon's mesh itself: its faces taken once from the game's own draws of it in the hand (every part, every bone as the
// game poses it against the held one: sh3vr_combat.cpp combat_weapon_mesh), in the held bone's model units, made into a
// signed distance field (a grid of the distance to its surface, negative inside) on a thread of its own, as SDFGen and
// the engines' mesh colliders make one: the exact distance to the nearest face within 2.4 cm of the surface (each face
// over its own box of grid points), and in or out by rays along the grid's rows, both ways along all three axes, each a
// vote (a face crossed counts its side's sign, so the count past a point is 1 inside a closed part and 0 outside it; a
// part left open or two parts overlapping are still right by the majority), a tie by the nearest face's side. Her
// fingers, palm and knuckles then ask the field (trilinear, sampled along each segment), the capsules until it is ready.
// About 2 mm a grid step (the long melee weapons a little coarser: at most 2.5 million points a weapon, 2 bytes each).

#include "sh3vr_internal.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <thread>
#include <utility>
#include <vector>

namespace sh3q {
namespace {
constexpr int kShapeProfiles = 16;
constexpr int kShapeSigs = 4;
constexpr float kBand = 12.0f;         // model units (2.4 cm): the distance exact within it, +-kBand beyond
constexpr double kMostPoints = 2.5e6;  // grid points a weapon at most
constexpr float kQuant = 64.0f;        // the field's distances in 1/64 units

struct Field {
    std::atomic<int> state{0};  // 0 taking faces, 1 building, 2 ready, 3 failed
    bool told = false;
    int sigs = 0;
    int sig_frame[kShapeSigs];
    std::vector<uint32_t> keys;  // the draws taken (each once)
    std::vector<float> tri;      // 9 floats a face (model units, wound outward)
    int draws = 0;
    // the field (written by the builder before state 2, read after)
    float lo[3]{};
    float cell = 1.0f, inv_cell = 1.0f;
    int n[3]{};
    std::vector<int16_t> d;
    float hi[3]{};               // the faces' bounds
    float mn[3]{};
    int faces = 0, inside = 0, ties = 0;
    float build_ms = 0;
    Field() {
        for (int& f : sig_frame) f = -1;
    }
};
Field* fields[kShapeProfiles];  // (never freed: a build still running at exit keeps its own)

inline Vec3 closest_on_tri(Vec3 p, Vec3 a, Vec3 b, Vec3 c) {
    // (Ericson, Real-Time Collision Detection 5.1.5)
    const Vec3 ab = b - a, ac = c - a, ap = p - a;
    const float d1 = dot(ab, ap), d2 = dot(ac, ap);
    if (d1 <= 0 && d2 <= 0) return a;
    const Vec3 bp = p - b;
    const float d3 = dot(ab, bp), d4 = dot(ac, bp);
    if (d3 >= 0 && d4 <= d3) return b;
    const float vc = d1 * d4 - d3 * d2;
    if (vc <= 0 && d1 >= 0 && d3 <= 0) return a + ab * (d1 / (d1 - d3));
    const Vec3 cp = p - c;
    const float d5 = dot(ab, cp), d6 = dot(ac, cp);
    if (d6 >= 0 && d5 <= d6) return c;
    const float vb = d5 * d2 - d1 * d6;
    if (vb <= 0 && d2 >= 0 && d6 <= 0) return a + ac * (d2 / (d2 - d6));
    const float va = d3 * d6 - d5 * d4;
    if (va <= 0 && (d4 - d3) >= 0 && (d5 - d6) >= 0) return b + (c - b) * ((d4 - d3) / ((d4 - d3) + (d5 - d6)));
    const float den = 1.0f / (va + vb + vc);
    return a + ab * (vb * den) + ac * (vc * den);
}
inline Vec3 tri_normal(const float* q) {
    const Vec3 u{q[3] - q[0], q[4] - q[1], q[5] - q[2]}, v{q[6] - q[0], q[7] - q[1], q[8] - q[2]};
    return {u.y * v.z - u.z * v.y, u.z * v.x - u.x * v.z, u.x * v.y - u.y * v.x};
}

void build(Field* f) {
    const auto t0 = std::chrono::steady_clock::now();
    const std::vector<float>& T = f->tri;
    const int nt = (int)(T.size() / 9);
    float mn[3] = {1e30f, 1e30f, 1e30f}, mx[3] = {-1e30f, -1e30f, -1e30f};
    for (size_t i = 0; i + 2 < T.size(); i += 3)
        for (int k = 0; k < 3; k++) mn[k] = std::min(mn[k], T[i + k]), mx[k] = std::max(mx[k], T[i + k]);
    if (nt <= 0 || !(mx[0] > mn[0]) || mx[0] - mn[0] > 4000.0f || mx[1] - mn[1] > 4000.0f || mx[2] - mn[2] > 4000.0f) {
        f->state.store(3, std::memory_order_release);
        return;
    }
    float cell = 1.0f, lo[3];
    int n[3];
    for (;;) {
        double points = 1;
        for (int k = 0; k < 3; k++) {
            lo[k] = mn[k] - kBand - cell;
            n[k] = (int)std::ceil((mx[k] + kBand + cell - lo[k]) / cell) + 1;
            points *= n[k];
        }
        if (points <= kMostPoints) break;
        cell *= 1.05f;
    }
    const float inv = 1.0f / cell;
    const size_t N = (size_t)n[0] * n[1] * n[2];
    auto at = [&](int i, int j, int k) { return ((size_t)k * n[1] + j) * n[0] + i; };
    // The exact distance (squared) to the nearest face within the band, and which face.
    std::vector<float> dist2(N, kBand * kBand);
    std::vector<int> near(N, -1);
    for (int t = 0; t < nt; t++) {
        const float* q = &T[(size_t)t * 9];
        const Vec3 A{q[0], q[1], q[2]}, B{q[3], q[4], q[5]}, C{q[6], q[7], q[8]};
        int i0[3], i1[3];
        for (int k = 0; k < 3; k++) {
            const float a = std::min(q[k], std::min(q[3 + k], q[6 + k])) - kBand;
            const float b = std::max(q[k], std::max(q[3 + k], q[6 + k])) + kBand;
            i0[k] = std::max(0, (int)std::ceil((a - lo[k]) * inv));
            i1[k] = std::min(n[k] - 1, (int)std::floor((b - lo[k]) * inv));
        }
        for (int k = i0[2]; k <= i1[2]; k++)
            for (int j = i0[1]; j <= i1[1]; j++) {
                const size_t row = at(0, j, k);
                for (int i = i0[0]; i <= i1[0]; i++) {
                    const Vec3 p{lo[0] + i * cell, lo[1] + j * cell, lo[2] + k * cell};
                    const Vec3 dd = p - closest_on_tri(p, A, B, C);
                    const float d2 = dot(dd, dd);
                    if (d2 < dist2[row + i]) dist2[row + i] = d2, near[row + i] = t;
                }
            }
    }
    // In or out: along each axis' rows, the signed crossings past each point (+1 a face whose outside faces the way
    // the ray goes: leaving the part), both ways; 6 votes a point.
    std::vector<uint8_t> votes(N, 0);
    std::vector<std::vector<std::pair<float, int>>> rows;
    for (int a = 0; a < 3; a++) {
        const int u = (a + 1) % 3, v = (a + 2) % 3;
        rows.assign((size_t)n[u] * n[v], {});
        for (int t = 0; t < nt; t++) {
            const float* q = &T[(size_t)t * 9];
            const float* P[3] = {q, q + 3, q + 6};
            // its normal's part along a (u, v, a cyclic: the projected area's own sign)
            const float area = (P[1][u] - P[0][u]) * (P[2][v] - P[0][v]) - (P[1][v] - P[0][v]) * (P[2][u] - P[0][u]);
            if (std::fabs(area) < 1e-9f) continue;  // edge-on to the rows: crossed by none
            const int s = area > 0 ? 1 : -1;
            if (area < 0) std::swap(P[1], P[2]);    // counter-clockwise in (u, v)
            const float umin = std::min(P[0][u], std::min(P[1][u], P[2][u])), umax = std::max(P[0][u], std::max(P[1][u], P[2][u]));
            const float vmin = std::min(P[0][v], std::min(P[1][v], P[2][v])), vmax = std::max(P[0][v], std::max(P[1][v], P[2][v]));
            const int ju0 = std::max(0, (int)std::ceil((umin - lo[u]) * inv));
            const int ju1 = std::min(n[u] - 1, (int)std::floor((umax - lo[u]) * inv));
            const int jv0 = std::max(0, (int)std::ceil((vmin - lo[v]) * inv));
            const int jv1 = std::min(n[v] - 1, (int)std::floor((vmax - lo[v]) * inv));
            for (int jv = jv0; jv <= jv1; jv++)
                for (int ju = ju0; ju <= ju1; ju++) {
                    const float pu = lo[u] + ju * cell, pv = lo[v] + jv * cell;
                    float w[3], sum = 0;
                    bool in = true;
                    for (int e = 0; e < 3 && in; e++) {
                        // the edge opposite corner e; a point on an edge counted by one of the two faces sharing it
                        // (the top-left rule, as a rasteriser's)
                        const float* E0 = P[(e + 1) % 3];
                        const float* E1 = P[(e + 2) % 3];
                        const float du = E1[u] - E0[u], dv = E1[v] - E0[v];
                        w[e] = du * (pv - E0[v]) - dv * (pu - E0[u]);
                        if (w[e] < 0 || (w[e] == 0 && !(dv < 0 || (dv == 0 && du > 0)))) in = false;
                        sum += w[e];
                    }
                    if (!in || sum <= 0) continue;
                    const float x = (w[0] * P[0][a] + w[1] * P[1][a] + w[2] * P[2][a]) / sum;
                    rows[(size_t)jv * n[u] + ju].push_back({x, s});
                }
        }
        for (int jv = 0; jv < n[v]; jv++)
            for (int ju = 0; ju < n[u]; ju++) {
                auto& r = rows[(size_t)jv * n[u] + ju];
                if (r.empty()) continue;
                std::sort(r.begin(), r.end());
                int total = 0;
                for (const auto& c : r) total += c.second;
                int below = 0;
                size_t ci = 0;
                int g[3];
                g[u] = ju, g[v] = jv;
                for (int ia = 0; ia < n[a]; ia++) {
                    const float x = lo[a] + ia * cell;
                    while (ci < r.size() && r[ci].first <= x) below += r[ci++].second;
                    g[a] = ia;
                    votes[at(g[0], g[1], g[2])] += (uint8_t)((total - below > 0 ? 1 : 0) + (-below > 0 ? 1 : 0));
                }
            }
    }
    rows.clear();
    rows.shrink_to_fit();
    std::vector<int16_t> d(N);
    int inside = 0, ties = 0;
    for (int k = 0; k < n[2]; k++)
        for (int j = 0; j < n[1]; j++)
            for (int i = 0; i < n[0]; i++) {
                const size_t idx = at(i, j, k);
                bool in = votes[idx] >= 4;
                if (votes[idx] == 3) {
                    ties++;
                    in = false;
                    if (near[idx] >= 0) {
                        const float* q = &T[(size_t)near[idx] * 9];
                        const Vec3 p{lo[0] + i * cell, lo[1] + j * cell, lo[2] + k * cell};
                        in = dot(p - closest_on_tri(p, Vec3{q[0], q[1], q[2]}, Vec3{q[3], q[4], q[5]}, Vec3{q[6], q[7], q[8]}),
                                 tri_normal(q)) < 0;
                    }
                }
                const float dd = std::min(std::sqrt(dist2[idx]), kBand);
                inside += in ? 1 : 0;
                d[idx] = (int16_t)std::lround((in ? -dd : dd) * kQuant);
            }
    f->d = std::move(d);
    for (int k = 0; k < 3; k++) f->lo[k] = lo[k], f->n[k] = n[k], f->mn[k] = mn[k], f->hi[k] = mx[k];
    f->cell = cell, f->inv_cell = inv;
    f->faces = nt, f->inside = inside, f->ties = ties;
    f->build_ms = std::chrono::duration<float, std::milli>(std::chrono::steady_clock::now() - t0).count();
    f->state.store(2, std::memory_order_release);
}

const Field* ready_field(int p) {
    if (p < 0 || p >= kShapeProfiles || !fields[p]) return nullptr;
    return fields[p]->state.load(std::memory_order_acquire) == 2 ? fields[p] : nullptr;
}

// The field at p (model units): trilinear inside the grid; outside it, the edge's value and the way out (the field is
// 1-Lipschitz, so this is never under the true distance by more than the band).
float sample(const Field& f, Vec3 p) {
    const float g[3] = {(p.x - f.lo[0]) * f.inv_cell, (p.y - f.lo[1]) * f.inv_cell, (p.z - f.lo[2]) * f.inv_cell};
    float out = 0, fr[3];
    int i[3];
    for (int k = 0; k < 3; k++) {
        const float top = (float)(f.n[k] - 1);
        float c = g[k];
        if (!(c >= 0)) out += c * c, c = 0;  // (NaN too)
        else if (c > top) out += (c - top) * (c - top), c = top;
        int ik = (int)c;
        if (ik > f.n[k] - 2) ik = f.n[k] - 2;
        i[k] = ik, fr[k] = c - (float)ik;
    }
    const int16_t* d = f.d.data();
    const size_t sy = (size_t)f.n[0], sz = (size_t)f.n[0] * f.n[1];
    const size_t b = (size_t)i[2] * sz + (size_t)i[1] * sy + (size_t)i[0];
    const float x0 = d[b] + (d[b + 1] - d[b]) * fr[0];
    const float x1 = d[b + sy] + (d[b + sy + 1] - d[b + sy]) * fr[0];
    const float x2 = d[b + sz] + (d[b + sz + 1] - d[b + sz]) * fr[0];
    const float x3 = d[b + sz + sy] + (d[b + sz + sy + 1] - d[b + sz + sy]) * fr[0];
    const float y0 = x0 + (x1 - x0) * fr[1], y1 = x2 + (x3 - x2) * fr[1];
    const float v = (y0 + (y1 - y0) * fr[2]) * (1.0f / kQuant);
    return out > 0 ? v + std::sqrt(out) * f.cell : v;
}
// Testing (the rig, SH3_SHAPE_SLICES=1): the field's three middle slices (through the faces' bounds' middle, one a
// main axis) written as greyscale pictures, shape_<profile>_<axis>.pgm in the game's folder, 3 pixels a grid point: in
// the weapon dark (darker deeper), out of it light, its skin (within half a grid step) black. tools/pgm_to_png.py
// turns them into PNGs.
void write_slices(const Field& f, int profile) {
    for (int a = 0; a < 3; a++) {
        const int u = a == 0 ? 2 : 0, v = a == 1 ? 2 : 1;  // the picture's across and down (x: z-y, y: x-z, z: x-y)
        const int k = (int)std::lround(((f.mn[a] + f.hi[a]) * 0.5f - f.lo[a]) * f.inv_cell);
        if (k < 0 || k >= f.n[a]) continue;
        constexpr int kScale = 3;
        const int w = f.n[u] * kScale, hgt = f.n[v] * kScale;
        std::vector<uint8_t> img((size_t)w * hgt);
        for (int y = 0; y < hgt; y++)
            for (int x = 0; x < w; x++) {
                int g[3];
                g[a] = k, g[u] = x / kScale, g[v] = f.n[v] - 1 - y / kScale;  // up the picture: up the axis
                const float d = f.d[((size_t)g[2] * f.n[1] + g[1]) * f.n[0] + g[0]] / kQuant;
                uint8_t c;
                if (std::fabs(d) < f.cell * 0.5f) c = 0;
                else if (d < 0) c = (uint8_t)std::clamp(110.0f + d * 6.0f, 30.0f, 110.0f);
                else c = (uint8_t)std::clamp(170.0f + d * 7.0f, 170.0f, 255.0f);
                img[(size_t)y * w + x] = c;
            }
        char path[600];
        std::snprintf(path, sizeof path, "%s/shape_%d_%c.pgm", rt_game_dir, profile, "xyz"[a]);
        if (FILE* fp = std::fopen(path, "wb")) {
            std::fprintf(fp, "P5\n%d %d\n255\n", w, hgt);
            std::fwrite(img.data(), 1, img.size(), fp);
            std::fclose(fp);
            rt_log("VR: shape %d's slice across %c at %.1f written to %s (%dx%d)", profile, "xyz"[a],
                   f.lo[a] + k * f.cell, path, w, hgt);
        }
    }
}
}  // namespace

bool shape_wanted(int profile) {
    if (profile < 0 || profile >= kShapeProfiles) return false;
    return !fields[profile] || fields[profile]->state.load(std::memory_order_acquire) == 0;
}

void shape_add(int profile, int sig, int sigs, uint32_t key, const float* tri9, int ntris) {
    if (profile < 0 || profile >= kShapeProfiles || sig < 0 || sig >= kShapeSigs || sigs <= 0 || sigs > kShapeSigs ||
        ntris <= 0 || !tri9)
        return;
    if (!fields[profile]) fields[profile] = new Field();
    Field& f = *fields[profile];
    if (f.state.load(std::memory_order_acquire) != 0) return;
    f.sigs = sigs;
    if (f.sig_frame[sig] >= 0 && f.sig_frame[sig] != rt_frame) return;  // this draw's part taken in an earlier frame
    for (uint32_t k : f.keys)
        if (k == key) return;  // the same draw again (the other eye, a reflection)
    f.sig_frame[sig] = rt_frame;
    f.keys.push_back(key);
    f.draws++;
    f.tri.insert(f.tri.end(), tri9, tri9 + (size_t)ntris * 9);
}

int shape_frame(const char* const* names) {
    int became = -1;
    for (int p = 0; p < kShapeProfiles; p++) {
        Field* f = fields[p];
        if (!f) continue;
        const int st = f->state.load(std::memory_order_acquire);
        const char* name = names && p < 10 ? names[p] : "a weapon";
        if (st == 0 && f->sigs > 0 && !f->tri.empty()) {
            bool all = true;
            int last = -1, first = 1 << 30;
            for (int s = 0; s < f->sigs; s++) {
                all = all && f->sig_frame[s] >= 0;
                last = std::max(last, f->sig_frame[s]);
                if (f->sig_frame[s] >= 0) first = std::min(first, f->sig_frame[s]);
            }
            // (every draw of it taken; or, should one of its draws never come, those taken 30 frames on: a weapon's
            // draws all come every frame it is drawn, so one not come by then is another model of it, as the steel
            // pipe's second signature, never drawn with its first. Session 31: it waited 10 s, and vr384's steel pipe
            // was put away before its shape was built, so its hand was never fitted)
            if ((all && last < rt_frame) || rt_frame - first > 30) {
                f->state.store(1, std::memory_order_release);
                rt_log("VR: %s's own shape: %d faces from its %d draws in the hand, its field being built", name,
                       (int)(f->tri.size() / 9), f->draws);
                std::thread([f]() { build(f); }).detach();
            }
        } else if (st >= 2 && !f->told) {
            f->told = true;
            if (st == 2) {
                rt_log("VR: %s's own shape ready (%.0f ms): %d faces, bounds %.1f..%.1f %.1f..%.1f %.1f..%.1f (model units), "
                       "a grid %dx%dx%d at %.2f units (%.1f mm), %d points inside, %d ties",
                       name, f->build_ms, f->faces, f->mn[0], f->hi[0], f->mn[1], f->hi[1], f->mn[2], f->hi[2], f->n[0],
                       f->n[1], f->n[2], f->cell, f->cell / kUnitsPerMetre * 1000.0f, f->inside, f->ties);
                became = p;
                static const bool slices = std::getenv("SH3_SHAPE_SLICES") && std::atoi(std::getenv("SH3_SHAPE_SLICES")) != 0;
                if (slices) write_slices(*f, p);
            } else
                rt_log("VR: %s's own shape: its faces out of bounds, left to its capsules", name);
        }
    }
    return became;
}

bool shape_ready(int profile) { return ready_field(profile) != nullptr; }

float shape_point(int profile, Vec3 p) {
    const Field* f = ready_field(profile);
    return f ? sample(*f, p) : 1e9f;
}

float shape_segment(int profile, Vec3 a, Vec3 b) {
    const Field* f = ready_field(profile);
    if (!f) return 1e9f;
    const Vec3 ab = b - a;
    const float L = length(ab);
    const int steps = std::max(1, std::min(400, (int)std::ceil(L * f->inv_cell / 0.75f)));
    float best = 1e9f;
    int at = 0;
    for (int s = 0; s <= steps; s++) {
        const float v = sample(*f, a + ab * ((float)s / (float)steps));
        if (v < best) best = v, at = s;
    }
    if (steps > 1) {
        // the least between the samples next to the best (the field smooth there): a few thirds
        float t0 = (float)std::max(0, at - 1) / (float)steps, t1 = (float)std::min(steps, at + 1) / (float)steps;
        for (int it = 0; it < 5; it++) {
            const float m0 = t0 + (t1 - t0) / 3.0f, m1 = t1 - (t1 - t0) / 3.0f;
            const float v0 = sample(*f, a + ab * m0), v1 = sample(*f, a + ab * m1);
            best = std::min(best, std::min(v0, v1));
            if (v0 < v1) t1 = m1;
            else t0 = m0;
        }
    }
    return best;
}

}  // namespace sh3q
