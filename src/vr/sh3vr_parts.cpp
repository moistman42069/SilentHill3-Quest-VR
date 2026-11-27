// SPDX-License-Identifier: GPL-3.0-or-later
// SH3 Quest VR: Heather's weapon drawn in passes, and the parts the hands hold (LIST 24, 46).
//
// - The weapon's draw goes through the game's own shader, lights, flashlight and textures in passes (d3d8.c
//   vr_draw_passes): the body; the moving part (the handgun's slide, the submachine gun's cocking knob, the shotgun's
//   pump) as its own faces moved whole, so nothing it shares with the body stretches (session 4 found the slide's faces
//   pulled into spikes when its vertices were moved in the buffer); the submachine gun's magazine left out while it is
//   out of the gun; the trigger (session 31) as its own faces turned about its pin as her finger pulls it.
// - The part in the off hand while reloading (the owner: "be sure the gripping of ammo/shells/mags is as clean as
//   possible. like re4 vr has hand poses for mag/shell gripping", and "make sure you make the models as detailed as
//   possible and as matching to the game as can be"): the game has no magazine, shell or battery model of its own (the
//   handgun's magazine is never seen, the submachine gun's shows only below the grip), so each is built here in the
//   game's own vertex layout, at real size, with the gun's own texture where it is the gun's metal or plastic (the
//   submachine gun's magazine from the texels of its own magazine below the grip) and a texture made here for brass,
//   copper and the shell's hull, and drawn through the gun's shader, lit as the gun is. Her fingers close on each
//   part's shape (sh3vr_body.cpp), and a magazine let go falls from the gun.

#include "sh3vr_internal.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <vector>

namespace sh3q {

namespace {

// ---------------------------------------------------------------- the game's vertex layout (stride 48)

struct V48 {
    float p[3];
    float weight;
    float pad[2];
    uint32_t bone;  // the bone byte at +24 (0)
    float n[3];
    float uv[2];
};
static_assert(sizeof(V48) == 48, "the game's skinned vertex is 48 bytes");

struct Mesh {
    std::vector<V48> v;
    std::vector<uint16_t> t;
    int add(Vec3 p, Vec3 n, float u, float w) {
        V48 x{};
        x.p[0] = p.x, x.p[1] = p.y, x.p[2] = p.z;
        x.weight = 1.0f;
        const Vec3 nn = normalized(n);
        x.n[0] = nn.x, x.n[1] = nn.y, x.n[2] = nn.z;
        x.uv[0] = u, x.uv[1] = w;
        v.push_back(x);
        return (int)v.size() - 1;
    }
    Vec3 pos(int i) const { return {v[i].p[0], v[i].p[1], v[i].p[2]}; }
    Vec3 nrm(int i) const { return {v[i].n[0], v[i].n[1], v[i].n[2]}; }
    // A face wound as the game's are (cross(b - a, c - a) along the normals: checked on the four guns' meshes).
    void tri(int a, int b, int c) {
        const Vec3 f = cross(pos(b) - pos(a), pos(c) - pos(a));
        const Vec3 n = nrm(a) + nrm(b) + nrm(c);
        if (dot(f, n) < 0) std::swap(b, c);
        t.push_back((uint16_t)a), t.push_back((uint16_t)b), t.push_back((uint16_t)c);
    }
    void quad(int a, int b, int c, int d) { tri(a, b, c), tri(a, c, d); }
    void clear() { v.clear(), t.clear(); }
};

struct UvRect {
    float u0, v0, u1, v1;
    float u(float s) const { return u0 + (u1 - u0) * std::clamp(s, 0.0f, 1.0f); }
    float w(float s) const { return v0 + (v1 - v0) * std::clamp(s, 0.0f, 1.0f); }
};

// ---------------------------------------------------------------- the texture made here (brass, copper, the hull)

constexpr int kAtlas = 256;
GLuint atlas_tex = 0;
// Its regions (u0, v0, u1, v1).
const UvRect kBrass{0.0f, 0.0f, 0.5f, 0.125f};
const UvRect kCopper{0.5f, 0.0f, 1.0f, 0.125f};
const UvRect kHull{0.0f, 0.125f, 1.0f, 0.5f};
const UvRect kPrimer{0.0f, 0.5f, 0.25f, 0.625f};
const UvRect kSteel{0.25f, 0.5f, 0.5f, 0.625f};
const UvRect kHole{0.5f, 0.5f, 0.5625f, 0.5625f};
const UvRect kLabel{0.0f, 0.625f, 1.0f, 0.8125f};
const UvRect kPolymer{0.0f, 0.8125f, 1.0f, 1.0f};

uint32_t hash32(uint32_t x) {
    x ^= x >> 16, x *= 0x7feb352du, x ^= x >> 15, x *= 0x846ca68bu, x ^= x >> 16;
    return x;
}
float noise(int x, int y, int seed) { return (hash32((uint32_t)(x * 73856093) ^ (uint32_t)(y * 19349663) ^ (uint32_t)seed) & 1023) / 1023.0f; }
// Smooth value noise (grime, wear), 0..1.
float smooth_noise(float x, float y, int seed) {
    const int xi = (int)std::floor(x), yi = (int)std::floor(y);
    const float fx = x - xi, fy = y - yi, sx = fx * fx * (3 - 2 * fx), sy = fy * fy * (3 - 2 * fy);
    const float a = noise(xi, yi, seed), b = noise(xi + 1, yi, seed), c = noise(xi, yi + 1, seed),
                d = noise(xi + 1, yi + 1, seed);
    return (a + (b - a) * sx) + ((c + (d - c) * sx) - (a + (b - a) * sx)) * sy;
}
float grime(float x, float y, int seed) {
    return 0.55f * smooth_noise(x / 18.0f, y / 18.0f, seed) + 0.3f * smooth_noise(x / 6.0f, y / 6.0f, seed + 1) +
           0.15f * noise((int)x, (int)y, seed + 2);
}

// Silent Hill 3's materials are worn and dark: every colour here is the clean one, darkened and dirtied by the grime.
void make_atlas() {
    if (atlas_tex) return;
    std::vector<uint32_t> px((size_t)kAtlas * kAtlas);
    auto put = [&](int x, int y, float r, float g, float b) {
        auto c = [](float v) { return (uint32_t)std::clamp((int)(v * 255.0f + 0.5f), 0, 255); };
        px[(size_t)y * kAtlas + x] = 0xFF000000u | c(b) << 16 | c(g) << 8 | c(r);
    };
    for (int y = 0; y < kAtlas; y++)
        for (int x = 0; x < kAtlas; x++) {
            const float u = (x + 0.5f) / kAtlas, v = (y + 0.5f) / kAtlas;
            const float gr = grime((float)x, (float)y, 7);
            float r, g, b;
            if (v < 0.125f) {
                // Brass (u < 0.5) and copper: a lengthwise sheen (v across the round), tarnish in the grime.
                const float across = (v / 0.125f);
                const float sheen = 0.72f + 0.38f * std::pow(std::sin(across * kPi), 3.0f) - 0.12f * std::fabs(across - 0.35f);
                const float tarnish = 0.78f + 0.22f * (1.0f - gr) - 0.08f * noise(x, y, 11);
                if (u < 0.5f) r = 0.70f, g = 0.55f, b = 0.28f;
                else r = 0.66f, g = 0.40f, b = 0.26f;
                r *= sheen * tarnish, g *= sheen * tarnish, b *= sheen * tarnish * 0.95f;
            } else if (v < 0.5f) {
                // The shell's hull: dark red plastic, its fine lengthwise ribs (u along the shell's turn), scuffs.
                const float rib = 0.86f + 0.14f * std::cos(u * kTwoPi * 48.0f);
                const float scuff = noise(x, y, 23) > 0.985f ? 1.35f : 1.0f;
                const float dirt = 0.62f + 0.38f * (1.0f - gr);
                r = 0.47f * rib * dirt * scuff, g = 0.10f * rib * dirt * scuff, b = 0.09f * rib * dirt * scuff;
            } else if (v < 0.625f && u < 0.25f) {
                // The primer: a nickel cup, its ring.
                const float du = (u / 0.25f - 0.5f) * 2.0f, dv = ((v - 0.5f) / 0.125f - 0.5f) * 2.0f;
                const float rr = std::sqrt(du * du + dv * dv);
                const float ring = rr > 0.78f && rr < 0.9f ? 0.55f : 1.0f;
                const float m = (0.55f + 0.2f * (1.0f - gr)) * ring;
                r = 0.62f * m, g = 0.60f * m, b = 0.56f * m;
            } else if (v < 0.625f && u < 0.5f) {
                // Bare steel (the battery's contacts, the magazines' feed lips' edges): grey, brushed.
                const float brush = 0.9f + 0.1f * noise(x, y / 4, 31);
                const float m = (0.48f + 0.22f * (1.0f - gr)) * brush;
                r = m, g = m * 0.99f, b = m * 0.97f;
            } else if (v < 0.625f) {
                // A hole (the magazine's witness holes): near black, a lighter rim.
                const float du = ((u - 0.5f) / 0.0625f - 0.5f) * 2.0f, dv = ((v - 0.5f) / 0.0625f - 0.5f) * 2.0f;
                const float rr = std::sqrt(du * du + dv * dv);
                const float m = rr < 0.7f ? 0.04f : 0.04f + (rr - 0.7f) * 0.5f;
                r = g = b = m;
            } else if (v < 0.8125f) {
                // The battery's label: a faded band of grey-blue with a darker stripe and wear at the edges.
                const float vv = (v - 0.625f) / 0.1875f;
                const float stripe = vv > 0.42f && vv < 0.58f ? 0.55f : 1.0f;
                const float wear = 0.7f + 0.3f * (1.0f - gr);
                r = 0.30f * stripe * wear, g = 0.34f * stripe * wear, b = 0.38f * stripe * wear;
            } else {
                // Dark polymer (the battery's casing, the floorplates where the gun has none).
                const float m = 0.13f + 0.09f * (1.0f - gr) + 0.02f * noise(x, y, 41);
                r = m, g = m, b = m * 1.05f;
            }
            put(x, y, r, g, b);
        }
    GLint prev = 0, unpack = 0, row = 0, align = 4;
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &prev);
    glGetIntegerv(GL_PIXEL_UNPACK_BUFFER_BINDING, &unpack);
    glGetIntegerv(GL_UNPACK_ROW_LENGTH, &row);
    glGetIntegerv(GL_UNPACK_ALIGNMENT, &align);
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    glGenTextures(1, &atlas_tex);
    glBindTexture(GL_TEXTURE_2D, atlas_tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, kAtlas, kAtlas, 0, GL_RGBA, GL_UNSIGNED_BYTE, px.data());
    glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, (GLuint)prev);
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, (GLuint)unpack);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, row);
    glPixelStorei(GL_UNPACK_ALIGNMENT, align);
    rt_log("VR: the parts' texture (brass, copper, the shell's hull, the battery's label) made");
}

// ---------------------------------------------------------------- shapes (model units: 490 a metre)

// A prism along +Y from y0 to y1 over a closed outline (x front, z side; counter-clockwise seen from +Y), its corners
// marked sharp (a crease: own normals each side) or round (one normal). Its sides take `side` (s round the outline,
// t up), its ends `ends` (planar). `taper`: the top outline scaled by it (feed lips narrowing).
struct Outline {
    std::vector<float> x, z;
    std::vector<bool> sharp;
};
void prism(Mesh& m, const Outline& o, float y0, float y1, UvRect side, UvRect ends, bool cap0, bool cap1,
           float taper = 1.0f) {
    const int n = (int)o.x.size();
    float total = 0;
    std::vector<float> at(n + 1, 0.0f);
    for (int i = 0; i < n; i++) {
        const int j = (i + 1) % n;
        total += std::hypot(o.x[j] - o.x[i], o.z[j] - o.z[i]);
        at[i + 1] = total;
    }
    auto edge_n = [&](int i) {  // the outward normal of edge i -> i+1 (counter-clockwise outline: right of the edge)
        const int j = (i + 1) % n;
        return normalized(Vec3{o.z[j] - o.z[i], 0, -(o.x[j] - o.x[i])});
    };
    for (int i = 0; i < n; i++) {
        const int j = (i + 1) % n;
        // Each corner's normal: the edge's own at a sharp corner, else the two edges' meeting there averaged.
        const Vec3 ne = edge_n(i);
        const Vec3 na = o.sharp[i] ? ne : normalized(edge_n((i + n - 1) % n) + ne);
        const Vec3 nb = o.sharp[j] ? ne : normalized(ne + edge_n(j));
        const float s0 = at[i] / total, s1 = at[i + 1] / total;
        const int a = m.add({o.x[i], y0, o.z[i]}, na, side.u(s0), side.w(1));
        const int b = m.add({o.x[j], y0, o.z[j]}, nb, side.u(s1), side.w(1));
        const int c = m.add({o.x[j] * taper, y1, o.z[j] * taper}, nb, side.u(s1), side.w(0));
        const int d = m.add({o.x[i] * taper, y1, o.z[i] * taper}, na, side.u(s0), side.w(0));
        m.quad(a, b, c, d);
    }
    float x0 = 1e9f, x1 = -1e9f, z0 = 1e9f, z1 = -1e9f;
    for (int i = 0; i < n; i++) x0 = std::min(x0, o.x[i]), x1 = std::max(x1, o.x[i]), z0 = std::min(z0, o.z[i]), z1 = std::max(z1, o.z[i]);
    for (int cap = 0; cap < 2; cap++) {
        if (!(cap ? cap1 : cap0)) continue;
        const float y = cap ? y1 : y0, k = cap ? taper : 1.0f;
        const Vec3 nn{0, cap ? 1.0f : -1.0f, 0};
        const int c = m.add({(x0 + x1) * 0.5f * k, y, (z0 + z1) * 0.5f * k}, nn, ends.u(0.5f), ends.w(0.5f));
        for (int i = 0; i < n; i++) {
            const int j = (i + 1) % n;
            const int a = m.add({o.x[i] * k, y, o.z[i] * k}, nn, ends.u((o.x[i] - x0) / (x1 - x0)), ends.w((o.z[i] - z0) / (z1 - z0)));
            const int b = m.add({o.x[j] * k, y, o.z[j] * k}, nn, ends.u((o.x[j] - x0) / (x1 - x0)), ends.w((o.z[j] - z0) / (z1 - z0)));
            m.tri(c, a, b);
        }
    }
}

// A ring of the outline's rounded-rectangle kind: width w (z), depth d (x), corner radius r, the front (+x) rounder.
Outline rounded_rect(float d, float w, float r_back, float r_front, int steps) {
    Outline o;
    auto arc = [&](float cx, float cz, float r, float a0, float a1, bool sharp_ends) {
        for (int k = 0; k <= steps; k++) {
            const float a = a0 + (a1 - a0) * k / steps;
            o.x.push_back(cx + r * std::cos(a));
            o.z.push_back(cz + r * std::sin(a));
            o.sharp.push_back(sharp_ends && (k == 0 || k == steps) && r < 0.05f);
        }
    };
    const float hx = d * 0.5f, hz = w * 0.5f;
    // Counter-clockwise seen from +Y (x right, z down the page there): front-right, front-left, back-left, back-right.
    arc(hx - r_front, hz - r_front, r_front, 0.0f, kPi * 0.5f, false);
    arc(-hx + r_back, hz - r_back, r_back, kPi * 0.5f, kPi, false);
    arc(-hx + r_back, -hz + r_back, r_back, kPi, kPi * 1.5f, false);
    arc(hx - r_front, -hz + r_front, r_front, kPi * 1.5f, kTwoPi, false);
    // Seen from +Y with x right and z toward the viewer's down, counter-clockwise in (x, z) is clockwise in (x, -z):
    // the prism's normals come out of edge_n either way (right of each edge); a check flips it if they point in.
    float area = 0;
    for (size_t i = 0; i < o.x.size(); i++) {
        const size_t j = (i + 1) % o.x.size();
        area += o.x[i] * o.z[j] - o.x[j] * o.z[i];
    }
    if (area < 0) {
        std::reverse(o.x.begin(), o.x.end());
        std::reverse(o.z.begin(), o.z.end());
        std::reverse(o.sharp.begin(), o.sharp.end());
    }
    return o;
}

// A body of revolution about the axis `axis` (unit) through `c`: profile points (along, radius), its turn's texture
// `uv` (s round, t along the profile), `seg` segments.
void lathe(Mesh& m, Vec3 c, Vec3 axis, Vec3 side, const std::vector<float>& along, const std::vector<float>& radius,
           UvRect uv, int seg, bool smooth_profile = true) {
    const Vec3 a = normalized(axis), s = normalized(side - a * dot(side, a)), q = cross(a, s);
    const int n = (int)along.size();
    float len = 0;
    std::vector<float> at(n, 0.0f);
    for (int i = 1; i < n; i++) at[i] = len += std::hypot(along[i] - along[i - 1], radius[i] - radius[i - 1]);
    for (int i = 0; i + 1 < n; i++) {
        // The profile's normal for this band (outward from the axis, tilted by its slope).
        const float da = along[i + 1] - along[i], dr = radius[i + 1] - radius[i];
        const float l = std::hypot(da, dr);
        if (l < 1e-5f) continue;
        const float nr = da / l, na = -dr / l;  // radial and axial parts
        for (int k = 0; k < seg; k++) {
            const float t0 = kTwoPi * k / seg, t1 = kTwoPi * (k + 1) / seg;
            const Vec3 r0 = s * std::cos(t0) + q * std::sin(t0), r1 = s * std::cos(t1) + q * std::sin(t1);
            const Vec3 n0 = smooth_profile ? r0 * nr + a * na : r0 * nr + a * na, n1 = r1 * nr + a * na;
            const float u0 = (float)k / seg, u1 = (float)(k + 1) / seg;
            const float v0 = len > 0 ? at[i] / len : 0, v1 = len > 0 ? at[i + 1] / len : 1;
            const int p0 = m.add(c + a * along[i] + r0 * radius[i], n0, uv.u(u0), uv.w(v0));
            const int p1 = m.add(c + a * along[i] + r1 * radius[i], n1, uv.u(u1), uv.w(v0));
            const int p2 = m.add(c + a * along[i + 1] + r1 * radius[i + 1], n1, uv.u(u1), uv.w(v1));
            const int p3 = m.add(c + a * along[i + 1] + r0 * radius[i + 1], n0, uv.u(u0), uv.w(v1));
            if (radius[i] < 1e-4f) m.tri(p0, p2, p3);
            else if (radius[i + 1] < 1e-4f) m.tri(p0, p1, p2);
            else m.quad(p0, p1, p2, p3);
        }
    }
}

// A flat disc facing `n` (unit) at c: radius r, `uv` mapped across it.
void disc(Mesh& m, Vec3 c, Vec3 n, Vec3 side, float r, UvRect uv, int seg) {
    const Vec3 a = normalized(n), s = normalized(side - a * dot(side, a)), q = cross(a, s);
    const int mid = m.add(c, a, uv.u(0.5f), uv.w(0.5f));
    for (int k = 0; k < seg; k++) {
        const float t0 = kTwoPi * k / seg, t1 = kTwoPi * (k + 1) / seg;
        const int p0 = m.add(c + (s * std::cos(t0) + q * std::sin(t0)) * r, a, uv.u(0.5f + 0.5f * std::cos(t0)),
                             uv.w(0.5f + 0.5f * std::sin(t0)));
        const int p1 = m.add(c + (s * std::cos(t1) + q * std::sin(t1)) * r, a, uv.u(0.5f + 0.5f * std::cos(t1)),
                             uv.w(0.5f + 0.5f * std::sin(t1)));
        m.tri(mid, p0, p1);
    }
}

// A cartridge lying along +x (its case's head at x0, the bullet's nose forward), on the axis through (x0, y, z):
// a 9 mm round, case 19 mm, the bullet's round nose 10 mm out of it.
void cartridge(Mesh& brass, Mesh& copper, float x0, float y, float z) {
    const float r = 4.6f * 0.5f;  // 9 mm: 4.6 units across
    const Vec3 c{x0, y, z}, ax{1, 0, 0}, side{0, 1, 0};
    // The case: the extractor groove near the head, then straight to the mouth.
    lathe(brass, c, ax, side, {0.0f, 0.0f, 0.5f, 0.6f, 1.2f, 9.3f}, {0.0f, r, r, r * 0.84f, r, r * 0.98f}, kBrass, 14);
    // The bullet: its ogive to a round nose.
    lathe(copper, c, ax, side, {9.3f, 10.6f, 12.0f, 13.1f, 13.8f, 14.1f},
          {r * 0.96f, r * 0.94f, r * 0.80f, r * 0.58f, r * 0.30f, 0.0f}, kCopper, 14);
}

// ---------------------------------------------------------------- the parts

// Where each gun's own texture has what a part is made of (texture coordinates measured from session 5's meshes:
// tools/gun_view.py, scratchpad regions.py; refined against the dumped textures, tools/tex_view.py).
struct GunTexels {
    UvRect metal;      // the part's steel (the magazine's walls, the battery's casing)
    UvRect metal_end;  // its ends
    UvRect plate;      // a floorplate
};
const GunTexels kHandgunTexels{{0.64f, 0.06f, 0.74f, 0.20f}, {0.30f, 0.80f, 0.34f, 0.88f}, {0.270f, 0.621f, 0.363f, 0.762f}};
const GunTexels kSmgTexels{{0.520f, 0.752f, 0.598f, 0.958f}, {0.492f, 0.752f, 0.619f, 0.958f}, {0.520f, 0.957f, 0.597f, 0.985f}};
const GunTexels kStunTexels{{0.62f, 0.10f, 0.72f, 0.40f}, {0.62f, 0.10f, 0.66f, 0.16f}, {0.62f, 0.10f, 0.66f, 0.16f}};

// A part: drawn with the gun's texture (`own`) and with the texture made here (`made`), its local frame (model units:
// +Y its top, the end that goes into the gun; +X its front; its origin where the fist holds it), its length and its
// shape for the fingers (capsules, local).
struct Part {
    Mesh own, made;
    float top = 0, bottom = 0;   // local y of its ends
    int ncaps = 0;
    Vec3 ca[3], cb[3];
    float cr[3]{};
    bool built = false;
};
Part parts[kPartCount];

const char* part_kind_name(int k) {
    static const char* const kNames[] = {"handgun magazine", "submachine gun magazine", "shotgun shell", "stun gun battery"};
    return k >= 0 && k < kPartCount ? kNames[k] : "?";
}

// A magazine: a steel box of depth d (x) and width w (z), its rounded front where the rounds' noses sit, from `bottom`
// to `top`; a floorplate under it a little larger; the feed lips folded in at the top; the top round in them and the
// next one under it (a double stack: the other side); witness holes down its left side.
void build_magazine(Part& p, float d, float w, float bottom, float top, const GunTexels& tx, bool plate_from_gun) {
    p.own.clear(), p.made.clear();
    const Outline body = rounded_rect(d, w, w * 0.12f, w * 0.42f, 3);
    const float lips = 3.2f;
    // The walls, the part below the lips.
    prism(p.own, body, bottom + 4.0f, top - lips, tx.metal, tx.metal_end, false, false);
    // The feed lips: the walls folded in over the top round.
    prism(p.own, body, top - lips, top - 0.6f, tx.metal, tx.metal_end, false, false, 0.86f);
    // The floorplate: a slab a little larger, its edges rounded, the bottom closed.
    const Outline plate = rounded_rect(d + 2.0f, w + 1.6f, 1.1f, w * 0.45f, 3);
    Mesh& pm = plate_from_gun ? p.own : p.made;
    const UvRect pside = plate_from_gun ? tx.plate : kPolymer, pend = plate_from_gun ? tx.plate : kPolymer;
    prism(pm, plate, bottom, bottom + 4.0f, pside, pend, true, true);
    // The rounds: the top one centred between the lips (its case's head at the back), the next one offset to the side
    // and lower (the double stack).
    const float r = 2.3f;
    cartridge(p.made, p.made, -d * 0.5f + 0.4f, top - r * 0.9f, w * 0.16f);
    cartridge(p.made, p.made, -d * 0.5f + 0.4f, top - r * 2.6f, -w * 0.18f);
    // Witness holes down the left side (the -z wall): small dark rounds a hair proud of the steel.
    for (int k = 0; k < 5; k++) {
        const float y = top - lips - 6.0f - k * ((top - bottom - 16.0f) / 5.0f);
        if (y < bottom + 6.0f) break;
        disc(p.made, {-d * 0.18f, y, -w * 0.5f - 0.05f}, {0, 0, -1}, {1, 0, 0}, 0.75f, kHole, 8);
    }
    p.top = top, p.bottom = bottom;
    // Its shape for the fingers: its front and back edges along its length (the box's half width round each).
    const float hw = w * 0.5f, inset = d * 0.5f - hw;
    p.ncaps = 2;
    p.ca[0] = {inset, bottom, 0}, p.cb[0] = {inset, top, 0}, p.cr[0] = hw;
    p.ca[1] = {-inset, bottom, 0}, p.cb[1] = {-inset, top, 0}, p.cr[1] = hw;
    p.built = true;
}

// A 12-gauge shell: its hull 18.5 mm across and 70 mm long, the brass head 16 mm high with its rim and primer, the
// star crimp folded in at the top.
void build_shell(Part& p) {
    p.own.clear(), p.made.clear();
    const float L = 34.3f, rh = 9.1f * 0.5f, head_h = 7.8f, rim_r = 5.5f, head_r = 5.0f;
    const float y0 = -L * 0.5f, y1 = L * 0.5f;
    const Vec3 ax{0, 1, 0}, side{1, 0, 0}, c{0, 0, 0};
    // The brass head: the rim, its groove, the head up to where the hull comes out of it.
    lathe(p.made, c, ax, side, {y0, y0 + 0.75f, y0 + 0.75f, y0 + 1.3f, y0 + 1.6f, y0 + head_h},
          {rim_r, rim_r, head_r * 0.92f, head_r * 0.92f, head_r, head_r}, kBrass, 20);
    // The head's top lip, stepping in to the hull.
    lathe(p.made, c, ax, side, {y0 + head_h, y0 + head_h}, {head_r, rh}, kBrass, 20);
    // The hull: straight to the crimp's shoulder.
    lathe(p.made, c, ax, side, {y0 + head_h, y1 - 2.0f, y1 - 1.2f}, {rh, rh, rh * 0.97f}, kHull, 20);
    // The base: the brass face, the primer in it.
    disc(p.made, {0, y0, 0}, {0, -1, 0}, side, rim_r, kBrass, 20);
    disc(p.made, {0, y0 - 0.06f, 0}, {0, -1, 0}, side, 1.5f, kPrimer, 12);
    // The star crimp: eight folds from the shoulder to the hollow at its centre, each fold's crease lower than its
    // ridges.
    const int folds = 8;
    const float yc = y1 - 1.2f, yt = y1, ycenter = y1 - 1.0f;
    const int centre = p.made.add({0, ycenter, 0}, ax, kHull.u(0.5f), kHull.w(0.0f));
    for (int k = 0; k < folds * 2; k++) {
        const float t0 = kTwoPi * k / (folds * 2), t1 = kTwoPi * (k + 1) / (folds * 2);
        const bool ridge0 = (k & 1) == 0;
        const float h0 = ridge0 ? yt : yt - 0.55f, h1 = ridge0 ? yt - 0.55f : yt;
        const float rr = rh * 0.97f;
        const Vec3 a{rr * std::cos(t0), yc, rr * std::sin(t0)}, b{rr * std::cos(t1), yc, rr * std::sin(t1)};
        const Vec3 a2{rr * 0.55f * std::cos(t0), h0, rr * 0.55f * std::sin(t0)}, b2{rr * 0.55f * std::cos(t1), h1, rr * 0.55f * std::sin(t1)};
        const Vec3 n_out = normalized(cross(b - a, a2 - a));
        const Vec3 nf = dot(n_out, Vec3{a.x, 0.6f, a.z}) < 0 ? n_out * -1.0f : n_out;
        const int i0 = p.made.add(a, nf, kHull.u((float)k / (folds * 2)), kHull.w(0.02f));
        const int i1 = p.made.add(b, nf, kHull.u((float)(k + 1) / (folds * 2)), kHull.w(0.02f));
        const int i2 = p.made.add(b2, nf, kHull.u((float)(k + 1) / (folds * 2)), kHull.w(0.0f));
        const int i3 = p.made.add(a2, nf, kHull.u((float)k / (folds * 2)), kHull.w(0.0f));
        p.made.quad(i0, i1, i2, i3);
        const Vec3 n_top = normalized(cross(b2 - a2, Vec3{0, ycenter, 0} - a2));
        const Vec3 nt = n_top.y < 0 ? n_top * -1.0f : n_top;
        const int j0 = p.made.add(a2, nt, kHull.u(0.5f), kHull.w(0.0f));
        const int j1 = p.made.add(b2, nt, kHull.u(0.52f), kHull.w(0.0f));
        const int jc = p.made.add({0, ycenter, 0}, nt, kHull.u(0.51f), kHull.w(0.01f));
        p.made.tri(j0, j1, jc);
    }
    (void)centre;
    p.top = y1, p.bottom = y0;
    p.ncaps = 1;
    p.ca[0] = {0, y0 + 0.5f, 0}, p.cb[0] = {0, y1 - 0.5f, 0}, p.cr[0] = rh;
    p.built = true;
}

// The stun gun's battery: a pack 26 x 16 x 48 mm, its edges rounded, the casing of the stun gun's own plastic, a label
// band round it, two contacts on top.
void build_battery(Part& p, const GunTexels& tx) {
    p.own.clear(), p.made.clear();
    const float d = 12.7f, w = 7.8f, L = 23.5f;
    const float y0 = -L * 0.5f, y1 = L * 0.5f;
    const Outline o = rounded_rect(d, w, 1.4f, 1.4f, 3);
    prism(p.own, o, y0, y0 + 6.0f, tx.metal, tx.metal_end, true, false);
    prism(p.made, o, y0 + 6.0f, y1 - 6.0f, kLabel, kLabel, false, false);
    prism(p.own, o, y1 - 6.0f, y1, tx.metal, tx.metal_end, false, true);
    // The contacts: two short steel posts on top.
    for (int k = 0; k < 2; k++) {
        const float x = (k ? 1 : -1) * d * 0.22f;
        lathe(p.made, {x, y1, 0}, {0, 1, 0}, {1, 0, 0}, {0.0f, 1.4f, 1.4f}, {1.2f, 1.2f, 0.0f}, kSteel, 10);
    }
    p.top = y1, p.bottom = y0;
    p.ncaps = 2;
    const float hw = w * 0.5f, inset = d * 0.5f - hw;
    p.ca[0] = {inset, y0, 0}, p.cb[0] = {inset, y1, 0}, p.cr[0] = hw;
    p.ca[1] = {-inset, y0, 0}, p.cb[1] = {-inset, y1, 0}, p.cr[1] = hw;
    p.built = true;
}

void build_parts() {
    static bool done = false;
    if (done) return;
    done = true;
    // The handgun's magazine: 3.3 x 1.3 cm inside its grip, 9.6 cm long with its floorplate (the grip's own floorplate
    // texels, so it matches the gun's grip bottom it came out of).
    build_magazine(parts[kPartHandgunMag], 16.0f, 6.4f, -23.5f, 23.5f, kHandgunTexels, true);
    // The submachine gun's: the 3.4 x 1.9 cm section of its own magazine below the grip, 15 cm long (up through the
    // grip to the receiver), its walls from that magazine's own texels.
    build_magazine(parts[kPartSmgMag], 16.6f, 9.1f, -38.0f, 36.0f, kSmgTexels, true);
    build_shell(parts[kPartShell]);
    build_battery(parts[kPartBattery], kStunTexels);
    for (int k = 0; k < kPartCount; k++)
        rt_log("VR: the %s built: %d + %d vertices, %d + %d faces", part_kind_name(k), (int)parts[k].own.v.size(),
               (int)parts[k].made.v.size(), (int)parts[k].own.t.size() / 3, (int)parts[k].made.t.size() / 3);
}

// ---------------------------------------------------------------- a part in a hand (its grip space, metres)

// The part's local frame in the hand: its top (+Y) along the rod out of the thumb's side (-Z), its front (+X) to the
// fingers' side (-Y), its side (+Z) to +X; its origin at `c`.
struct InHand {
    Vec3 c;
    Quat q;  // local -> grip
};
Quat part_turn() {
    // Columns: local X -> (0,-1,0), local Y -> (0,0,-1), local Z -> (1,0,0).
    const Vec3 x{0, -1, 0}, y{0, 0, -1}, z{1, 0, 0};
    const float m00 = x.x, m11 = y.y, m22 = z.z, tr = m00 + m11 + m22;
    Quat q;
    if (tr > 0) {
        const float s = std::sqrt(tr + 1.0f) * 2.0f;
        q = {(y.z - z.y) / s, (z.x - x.z) / s, (x.y - y.x) / s, 0.25f * s};
    } else if (m00 > m11 && m00 > m22) {
        const float s = std::sqrt(1.0f + m00 - m11 - m22) * 2.0f;
        q = {0.25f * s, (y.x + x.y) / s, (z.x + x.z) / s, (y.z - z.y) / s};
    } else if (m11 > m22) {
        const float s = std::sqrt(1.0f + m11 - m00 - m22) * 2.0f;
        q = {(y.x + x.y) / s, 0.25f * s, (z.y + y.z) / s, (z.x - x.z) / s};
    } else {
        const float s = std::sqrt(1.0f + m22 - m00 - m11) * 2.0f;
        q = {(z.x + x.z) / s, (z.y + y.z) / s, 0.25f * s, (x.y - y.x) / s};
    }
    return q;
}

bool in_hand_of(int h, int kind, InHand& out) {
    const float mx = h == kLeft ? -1.0f : 1.0f;
    out.q = part_turn();
    const float m = 1.0f / kUnitsPerMetre;
    switch (kind) {
    case kPartHandgunMag:
        // The magazine in the fist, its top 4 cm past the index finger's knuckle, the index along its front.
        out.c = {0.003f * mx, -0.004f, -0.066f + parts[kind].top * m};
        return true;
    case kPartSmgMag:
        out.c = {0.003f * mx, -0.002f, -0.066f + parts[kind].top * m};
        return true;
    case kPartBattery:
        out.c = {0.004f * mx, -0.006f, -0.048f + parts[kind].top * m};
        return true;
    case kPartShell: {
        // Pinched: between the thumb's tip and the index and middle fingers' tips as they close on it, along the rod.
        Vec3 t, i, md;
        if (hand_finger_tip(h, kThumb, 0.6f, t) && hand_finger_tip(h, kIndex, 0.62f, i) &&
            hand_finger_tip(h, kMiddle, 0.58f, md)) {
            const Vec3 fingers = (i + md) * 0.5f;
            out.c = (t + fingers) * 0.5f;
            out.c.z = std::min(t.z, i.z) - 0.004f;
        } else out.c = {0.0f, -0.06f, -0.03f};
        return true;
    }
    }
    return false;
}

// ---------------------------------------------------------------- the dropped parts

struct Dropped {
    bool live = false;
    int kind = -1;
    Vec3 pos;      // world
    Quat q;        // world: local -> world
    Vec3 vel;      // world units a second
    Vec3 spin;     // world: radians a second (axis * rate)
    double rest_until = 0;
    bool resting = false;
};
Dropped dropped[2];
double now = 0;

// ---------------------------------------------------------------- the weapon's meshes (faces by group)

constexpr int kGroupBody = 0, kGroupMoving = 1, kGroupMag = 2, kGroupTrigger = 3, kGroups = 4;
struct GunMesh {
    uint32_t nv = 0, np = 0;
    int item = 0;                 // the gun's inventory item (vr_gun_item), 0 none
    std::vector<uint8_t> group;   // per face of the last face list
    std::vector<uint16_t> faces[kGroups];
    int ntris = 0;
    uint32_t tris_sum = 0;
    bool moving_ok = false;
    int moving_bone = 0;
    float moving_back = 0;        // model x for "back": -1 or +1
};
GunMesh meshes[6];
int nmeshes = 0;

// The guns' moving parts (tools/gun_parts.py from run vr40's dumps): their vertices (indices from the draw's first
// vertex), the bone they have, the box their resting places lie in (any other: not this gun's mesh).
const uint16_t kSlideVerts[] = {
    6, 7, 8, 11, 12, 13, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52,
    53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 65, 66, 67, 68, 69, 70, 73, 74, 77, 78, 79, 80, 81, 82, 83, 84, 85, 86, 87, 88,
    122, 123, 177, 178, 179, 180, 181, 182, 183, 184, 185, 186, 187, 188, 189, 190, 191, 192, 193, 194, 195, 196, 197,
    198, 199, 200, 201, 202, 203, 204, 205, 206, 207, 212, 213, 214, 215, 216, 217, 220, 221, 222, 223, 224, 225, 226,
    227, 230, 231, 232, 233, 234, 235, 236, 237, 238, 239, 240, 243, 244, 245, 276, 277, 278, 279, 280, 281};
const uint16_t kKnobVerts[] = {
    59, 60, 61, 62, 63, 64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79, 80, 81, 82, 83, 84, 85, 86, 87,
    88, 89, 90, 91, 92, 93, 94, 95, 96, 97, 98, 99, 100, 101, 102, 103, 104, 105, 106, 107, 108, 109, 110, 111, 112, 113,
    114, 115, 116, 117, 118, 119, 120, 121, 122, 123, 124, 125, 126, 127, 128, 129, 130, 131, 132, 133, 134, 135, 136,
    137, 138, 139, 140, 141, 142, 143, 144, 145, 146};
const uint16_t kPumpVerts[] = {205, 208, 209, 211, 213, 214, 215, 217, 218, 220, 221, 222, 223, 224, 225, 226, 227, 228,
                               229, 230, 231, 232, 234, 236, 237, 238, 241, 242, 245, 246, 249, 252, 255, 258, 259};
struct MovingPart {
    uint32_t nv, np;
    int item;
    const uint16_t* verts;
    int count;
    int bone;
    float lo[3], hi[3];
    float back;
};
// (`back`: the model's x drawn back. Session 31: the shotgun's was +1 since session 4, which took its barrel to run
// along -x; its muzzle is at x 0 (the hold's), its receiver at x -250 to -140 and the pump ahead of it at -129 to -13,
// with its own travel (6 cm, 29 units) the gap between them: so the pump drawn back goes along -x, as her hand on it
// does (second_spot). 0.1.7 slid it forward, past the muzzle, as the hand pulled it back.)
const MovingPart kMovingParts[] = {
    {368, 610, 10, kSlideVerts, (int)(sizeof kSlideVerts / sizeof *kSlideVerts), 0, {-1000, 21.5f, -1000},
     {1000, 1000, 1000}, -1.0f},
    {557, 890, 12, kKnobVerts, (int)(sizeof kKnobVerts / sizeof *kKnobVerts), 0, {-1.5f, 54.5f, -13}, {13.5f, 70.5f, 4},
     -1.0f},
    {290, 490, 11, kPumpVerts, (int)(sizeof kPumpVerts / sizeof *kPumpVerts), 1, {-130, -0.7f, -9.3f},
     {-12.4f, 27.8f, 9.3f}, -1.0f},
};
// Session 31 (owner message 151): each gun's trigger, its own faces (four whole pieces, flat-shaded, sharing no vertex
// with the rest: scratchpad trig_parts.py on the session 4 dumps), turned about its pin (sh3vr_combat.cpp kTriggers) by
// as far as her finger pulls it. The handgun's two front top corners (201..207) lie at y 22.9 to 23.6, inside the
// slide's cut (y 21.5 and up, kSlideVerts), so 0.1.7 moved the faces on them with the slide when it was racked: the
// trigger's faces now go with the trigger first.
const uint16_t kHandgunTrigger[] = {151, 152, 155, 156, 165, 166, 167, 168, 169, 170, 171, 172,
                                    173, 174, 175, 176, 201, 203, 205, 207, 208, 209, 210, 211};
const uint16_t kShotgunTrigger[] = {99,  100, 101, 102, 103, 104, 105, 106, 107, 108, 109, 110, 111, 112, 113,
                                    114, 115, 116, 117, 118, 119, 120, 122, 123, 124, 127, 128, 130, 131, 134};
const uint16_t kSmgTrigger[] = {239, 240, 243, 244, 251, 252, 253, 254, 255, 256,
                                257, 258, 259, 260, 261, 262, 263, 264, 265, 266};
const MovingPart kTriggerParts[] = {
    {368, 610, 10, kHandgunTrigger, (int)(sizeof kHandgunTrigger / sizeof *kHandgunTrigger), 0, {68.0f, 10.5f, -15.5f},
     {77.0f, 24.2f, -9.2f}, 0},
    {290, 490, 11, kShotgunTrigger, (int)(sizeof kShotgunTrigger / sizeof *kShotgunTrigger), 1, {-233.6f, 20.2f, -3.0f},
     {-226.3f, 32.1f, 3.0f}, 0},
    {557, 890, 12, kSmgTrigger, (int)(sizeof kSmgTrigger / sizeof *kSmgTrigger), 0, {63.2f, 10.9f, -12.6f},
     {71.5f, 25.7f, -8.9f}, 0},
};
// Whether every vertex of part `mp` is in the draw, on its bone and in its box (else the draw is not that gun's mesh).
bool part_matches(const MovingPart& mp, uint32_t nv, const uint8_t* v48);
// The guns by signature: their item, and whether the magazine shows below the grip (the submachine gun's: its faces
// all under y -19 on bone 0).
int item_of_signature(uint32_t nv, uint32_t np) {
    if (nv == 368 && np == 610) return 10;
    if (nv == 290 && np == 490) return 11;
    if (nv == 557 && np == 890) return 12;
    if (nv == 132 && np == 226) return 9;
    return 0;
}

const V48* vert(const uint8_t* v48, int i) { return (const V48*)(v48 + (size_t)i * 48); }

bool part_matches(const MovingPart& mp, uint32_t nv, const uint8_t* v48) {
    for (int i = 0; i < mp.count; i++) {
        if (mp.verts[i] >= nv) return false;
        const V48* x = vert(v48, mp.verts[i]);
        if ((x->bone & 0xFF) != (uint32_t)mp.bone) return false;
        for (int c = 0; c < 3; c++)
            if (!(x->p[c] >= mp.lo[c] && x->p[c] <= mp.hi[c])) return false;
    }
    return true;
}

GunMesh* mesh_for(uint32_t nv, uint32_t np, const uint8_t* v48, const uint16_t* tris, int ntris) {
    uint32_t sum = (uint32_t)ntris;
    for (int i = 0; i < ntris * 3; i++) sum = sum * 31u + tris[i];
    GunMesh* g = nullptr;
    for (int k = 0; k < nmeshes; k++)
        if (meshes[k].nv == nv && meshes[k].np == np) g = &meshes[k];
    if (g && g->ntris == ntris && g->tris_sum == sum) return g;
    if (!g) {
        if (nmeshes >= 6) return nullptr;
        g = &meshes[nmeshes++];
    }
    g->nv = nv, g->np = np, g->ntris = ntris, g->tris_sum = sum;
    g->item = item_of_signature(nv, np);
    g->moving_ok = false;
    std::vector<uint8_t> in_part(nv, 0), in_trigger(nv, 0);
    for (const MovingPart& mp : kMovingParts) {
        if (mp.nv != nv || mp.np != np) continue;
        if (part_matches(mp, nv, v48)) {
            for (int i = 0; i < mp.count; i++) in_part[mp.verts[i]] = 1;
            g->moving_ok = true, g->moving_bone = mp.bone, g->moving_back = mp.back;
        } else rt_log("VR: the %u-vertex gun's moving part does not match its vertices: drawn with the body", nv);
    }
    bool trigger_ok = false;
    for (const MovingPart& tp : kTriggerParts) {
        if (tp.nv != nv || tp.np != np) continue;
        if (part_matches(tp, nv, v48)) {
            for (int i = 0; i < tp.count; i++) in_trigger[tp.verts[i]] = 1;
            trigger_ok = true;
        } else rt_log("VR: the %u-vertex gun's trigger does not match its vertices: drawn with the body (not turning)", nv);
    }
    for (auto& f : g->faces) f.clear();
    g->group.assign(ntris, kGroupBody);
    int counts[kGroups]{};
    for (int i = 0; i < ntris; i++) {
        const uint16_t* t = tris + i * 3;
        int grp = kGroupBody;
        if (t[0] >= nv || t[1] >= nv || t[2] >= nv) grp = kGroupBody;
        else if (trigger_ok && in_trigger[t[0]] && in_trigger[t[1]] && in_trigger[t[2]]) grp = kGroupTrigger;
        else if (g->moving_ok && in_part[t[0]] + in_part[t[1]] + in_part[t[2]] >= 2) grp = kGroupMoving;
        else if (g->item == 12) {
            bool below = true;
            for (int j = 0; j < 3; j++) {
                const V48* x = vert(v48, t[j]);
                below &= (x->bone & 0xFF) == 0 && x->p[1] < -19.0f;
            }
            if (below) grp = kGroupMag;
        }
        g->group[i] = (uint8_t)grp;
        g->faces[grp].insert(g->faces[grp].end(), t, t + 3);
        counts[grp]++;
    }
    rt_log("VR: the %u-vertex gun's faces: %d body, %d moving part, %d magazine, %d trigger (passes)", nv, counts[0],
           counts[1], counts[2], counts[3]);
    return g;
}

// ---------------------------------------------------------------- the passes built for a draw

struct BuiltPass {
    PassPlace place;
};
BuiltPass built[8];
int nbuilt = 0;
int current_pass = 0;

void row_major(const float r[3][3], Vec3 t, float out[12]) {
    for (int i = 0; i < 3; i++) out[i * 4] = r[i][0], out[i * 4 + 1] = r[i][1], out[i * 4 + 2] = r[i][2], out[i * 4 + 3] = (&t.x)[i];
}
// A world placement of a part's local frame: its rotation (world) and its origin (world).
void world_place(Quat wq, Vec3 origin, float out[12]) {
    const Vec3 X = rotate(wq, Vec3{1, 0, 0}), Y = rotate(wq, Vec3{0, 1, 0}), Z = rotate(wq, Vec3{0, 0, 1});
    const float r[3][3] = {{X.x, Y.x, Z.x}, {X.y, Y.y, Z.y}, {X.z, Y.z, Z.z}};
    row_major(r, origin, out);
}

bool add_part_passes(int kind, Quat wq, Vec3 origin, Sh3VrWeaponPass* out, int max) {
    const Part& p = parts[kind];
    if (!p.built) return false;
    for (int s = 0; s < 2; s++) {
        const Mesh& m = s ? p.made : p.own;
        if (m.t.empty() || nbuilt >= max) continue;
        Sh3VrWeaponPass& o = out[nbuilt];
        o.tris = m.t.data();
        o.ntris = (int)m.t.size() / 3;
        o.verts = (const float*)m.v.data();
        o.nverts = (int)m.v.size();
        o.texture = s ? atlas_tex : 0;
        built[nbuilt].place.kind = 2;
        world_place(wq, origin, built[nbuilt].place.m);
        nbuilt++;
    }
    return true;
}

}  // namespace

// ================================================================ shared (sh3vr_internal.h)

int parts_passes(uint32_t nv, uint32_t np, const uint8_t* verts48, const uint16_t* tris, int ntris,
                 Sh3VrWeaponPass* out, int max) {
    nbuilt = 0;
    if (!weapon_signature(nv, np) || ntris <= 0 || max < 2) return 0;
    GunMesh* g = mesh_for(nv, np, verts48, tris, ntris);
    if (!g) return 0;
    build_parts();
    make_atlas();
    // The body (and the submachine gun's magazine while it is in), then the moving part, moved back.
    static std::vector<uint16_t> body;
    body = g->faces[kGroupBody];
    const bool mag_in = g->item != 12 || !reload_magazine_out(12);
    if (mag_in) body.insert(body.end(), g->faces[kGroupMag].begin(), g->faces[kGroupMag].end());
    out[0] = {body.data(), (int)body.size() / 3, nullptr, 0, 0};
    built[0].place.kind = 0;
    nbuilt = 1;
    if (!g->faces[kGroupMoving].empty()) {
        const float back = g->item ? reload_part_back(g->item) : 0.0f;  // metres
        out[1] = {g->faces[kGroupMoving].data(), (int)g->faces[kGroupMoving].size() / 3, nullptr, 0, 0};
        built[1].place.kind = 1;
        const float r[3][3] = {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}};
        row_major(r, Vec3{g->moving_back * back * kUnitsPerMetre, 0, 0}, built[1].place.m);
        nbuilt = 2;
    }
    // Session 31: the trigger, turned as far as her finger, drawn, pulls it (kind 3: a turn in the held bone's model
    // units, as a moving part's; at rest the identity). Always its own pass, so the draws are the same at rest and pulled.
    if (!g->faces[kGroupTrigger].empty() && nbuilt < max) {
        out[nbuilt] = {g->faces[kGroupTrigger].data(), (int)g->faces[kGroupTrigger].size() / 3, nullptr, 0, 0};
        built[nbuilt].place.kind = 3;
        weapon_trigger_place(nv, np, built[nbuilt].place.m);
        nbuilt++;
    }
    if (!map_ready()) return nbuilt;
    // The part in the off hand, and those falling.
    for (int h = 0; h < 2; h++) {
        const int kind = reload_held_part(h);
        InHand ih;
        if (kind < 0 || !hands[h].valid || !in_hand_of(h, kind, ih)) continue;
        const Pose& s = hands[h].shown;
        const Quat wq = world_quat(s.orientation * ih.q);
        add_part_passes(kind, wq, world_point(s.position + rotate(s.orientation, ih.c)), out, max);
    }
    for (const Dropped& d : dropped)
        if (d.live) add_part_passes(d.kind, d.q, d.pos, out, max);
    return nbuilt;
}

void parts_set_pass(int pass) { current_pass = pass; }
int parts_pass() { return current_pass; }

bool parts_pass_place(int pass, PassPlace& out) {
    if (pass <= 0 || pass >= nbuilt) return false;
    out = built[pass].place;
    return true;
}

bool parts_hand_caps(int h, PartCaps& out) {
    out.n = 0;
    const int kind = reload_held_part(h);
    InHand ih;
    if (kind < 0 || !in_hand_of(h, kind, ih)) return false;
    build_parts();
    const Part& p = parts[kind];
    const float m = 1.0f / kUnitsPerMetre;
    for (int i = 0; i < p.ncaps && i < 3; i++) {
        out.a[i] = ih.c + rotate(ih.q, p.ca[i] * m);
        out.b[i] = ih.c + rotate(ih.q, p.cb[i] * m);
        out.r[i] = p.cr[i] * m;
    }
    out.n = std::min(p.ncaps, 3);
    return out.n > 0;
}

bool parts_held_ends(int h, Vec3& bottom, Vec3& top, Vec3& front) {
    const int kind = reload_held_part(h);
    InHand ih;
    if (kind < 0 || !hands[h].valid || !in_hand_of(h, kind, ih)) return false;
    build_parts();
    const Pose& s = hands[h].shown;
    const Quat q = s.orientation * ih.q;
    const Vec3 origin = s.position + rotate(s.orientation, ih.c), axis = rotate(q, Vec3{0, 1, 0});
    top = origin + axis * (parts[kind].top / kUnitsPerMetre);
    bottom = origin + axis * (parts[kind].bottom / kUnitsPerMetre);
    front = rotate(q, Vec3{1, 0, 0});
    return true;
}

bool parts_in_hand(int h, int kind, Vec3& c, Quat& q) {
    InHand ih;
    if (kind < 0 || kind >= kPartCount || !in_hand_of(h, kind, ih)) return false;
    build_parts();
    c = ih.c;
    q = ih.q;
    return true;
}

float parts_top(int kind) {
    if (kind < 0 || kind >= kPartCount) return 0.0f;
    build_parts();
    return parts[kind].top;
}

float parts_length(int kind) {
    if (kind < 0 || kind >= kPartCount) return 0.0f;
    build_parts();
    return (parts[kind].top - parts[kind].bottom) / kUnitsPerMetre;
}

bool parts_held_tip(int h, Vec3& tip, Vec3& axis) {
    const int kind = reload_held_part(h);
    InHand ih;
    if (kind < 0 || !hands[h].valid || !in_hand_of(h, kind, ih)) return false;
    build_parts();
    const Pose& s = hands[h].shown;
    const Quat q = s.orientation * ih.q;
    axis = rotate(q, Vec3{0, 1, 0});
    tip = s.position + rotate(s.orientation, ih.c) + axis * (parts[kind].top / kUnitsPerMetre);
    return true;
}

void parts_drop(int kind, Vec3 bottom, Vec3 axis, Vec3 front, Vec3 vel) {
    if (kind < 0 || kind >= kPartCount || !map_ready()) return;
    build_parts();
    Dropped* d = &dropped[0];
    for (Dropped& x : dropped)
        if (!x.live) d = &x;
    if (d->live && dropped[1].live) d = dropped[0].rest_until < dropped[1].rest_until ? &dropped[0] : &dropped[1];
    const Vec3 y = normalized(axis), x = normalized(front - y * dot(front, y)), z = cross(x, y);
    // The part's frame in tracking, its origin above its bottom by its own length below the origin.
    const Vec3 origin = bottom - y * (parts[kind].bottom / kUnitsPerMetre);
    // Its world turn: the tracking frame's axes carried into the world.
    const Vec3 wx = world_dir(x), wy = world_dir(y), wz = world_dir(z);
    const float m00 = wx.x, m11 = wy.y, m22 = wz.z, tr = m00 + m11 + m22;
    Quat q;
    if (tr > 0) {
        const float s = std::sqrt(tr + 1.0f) * 2.0f;
        q = {(wy.z - wz.y) / s, (wz.x - wx.z) / s, (wx.y - wy.x) / s, 0.25f * s};
    } else if (m00 > m11 && m00 > m22) {
        const float s = std::sqrt(1.0f + m00 - m11 - m22) * 2.0f;
        q = {0.25f * s, (wy.x + wx.y) / s, (wz.x + wx.z) / s, (wy.z - wz.y) / s};
    } else if (m11 > m22) {
        const float s = std::sqrt(1.0f + m11 - m00 - m22) * 2.0f;
        q = {(wy.x + wx.y) / s, 0.25f * s, (wz.y + wy.z) / s, (wz.x - wx.z) / s};
    } else {
        const float s = std::sqrt(1.0f + m22 - m00 - m11) * 2.0f;
        q = {(wz.x + wx.z) / s, (wz.y + wy.z) / s, 0.25f * s, (wx.y - wy.x) / s};
    }
    const float n = std::sqrt(q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w);
    d->q = {q.x / n, q.y / n, q.z / n, q.w / n};
    d->kind = kind;
    d->pos = world_point(origin);
    d->vel = world_dir(vel) * kUnitsPerMetre;
    // A little tumble about its side as it leaves the well.
    d->spin = world_dir(z) * 2.5f;
    d->live = true;
    d->resting = false;
    d->rest_until = now + 6.0;
    rt_log("VR: the %s falls from the gun", part_kind_name(kind));
}

void parts_frame() {
    const float dt = frame_dt();
    now += dt;
    for (Dropped& d : dropped) {
        if (!d.live) continue;
        if (now > d.rest_until || !game.player_valid) {
            d.live = false;
            continue;
        }
        if (d.resting) continue;
        // World: Y down, 490 units a metre; the floor where Heather stands.
        d.vel.y += 9.8f * kUnitsPerMetre * dt;
        d.pos = d.pos + d.vel * dt;
        const float rate = length(d.spin);
        if (rate > 1e-4f) {
            const Vec3 ax = d.spin * (1.0f / rate);
            const float a = rate * dt;
            const Quat dq{ax.x * std::sin(a * 0.5f), ax.y * std::sin(a * 0.5f), ax.z * std::sin(a * 0.5f), std::cos(a * 0.5f)};
            d.q = dq * d.q;
        }
        // Its lowest end on the floor: it settles on its side there.
        const float floor_y = game.pos[1];
        const float half = std::max(std::fabs(parts[d.kind].top), std::fabs(parts[d.kind].bottom));
        const float low = d.pos.y + std::fabs(rotate(d.q, Vec3{0, 1, 0}).y) * half;
        if (low >= floor_y) {
            d.pos.y -= low - floor_y;
            if (std::fabs(d.vel.y) > 0.6f * kUnitsPerMetre) {
                // A bounce: most of it gone, a turn toward lying flat.
                d.vel = Vec3{d.vel.x * 0.35f, -d.vel.y * 0.22f, d.vel.z * 0.35f};
                d.spin = d.spin * 0.4f;
                hand_sound(kSoundPartFloor, d.pos, d.kind);
            } else {
                d.resting = true;
                // Laid on its side: its long axis level.
                const Vec3 y = rotate(d.q, Vec3{0, 1, 0});
                const Vec3 level = normalized(Vec3{y.x, 0, y.z});
                if (length(Vec3{y.x, 0, y.z}) > 0.05f) {
                    const Vec3 c = cross(y, level);
                    const float s = length(c), cth = dot(y, level);
                    if (s > 1e-4f) {
                        const Vec3 ax = c * (1.0f / s);
                        const float a = std::atan2(s, cth);
                        d.q = Quat{ax.x * std::sin(a * 0.5f), ax.y * std::sin(a * 0.5f), ax.z * std::sin(a * 0.5f),
                                   std::cos(a * 0.5f)} * d.q;
                    }
                }
                // On the floor by its own thickness (its shape's radius: a magazine's half width, the shell's).
                d.pos.y = floor_y - parts[d.kind].cr[0];
            }
        }
    }
}

void parts_crash_info(FILE* f) {
    std::fprintf(f, "VR parts: pass %d of %d, meshes %d, dropped:", current_pass, nbuilt, nmeshes);
    for (const Dropped& d : dropped)
        std::fprintf(f, " %s %s at %.0f %.0f %.0f", d.live ? "live" : "-", d.kind >= 0 ? part_kind_name(d.kind) : "",
                     d.pos.x, d.pos.y, d.pos.z);
    std::fprintf(f, "\n");
}

}  // namespace sh3q
