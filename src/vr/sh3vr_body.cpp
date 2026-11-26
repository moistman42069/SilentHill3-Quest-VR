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
#include <cstring>

namespace sh3q {

HandState hands[2];

namespace {

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
void finger_points(int h, int f, float c, Vec3 out[4]) {
    const HerHand& hh = her_hands[h];
    if (hh.valid) {
        const FingerGeo& g = hh.f[f];
        out[0] = g.knuckle;
        float angle = 0;
        for (int j = 0; j < 3; j++) {
            angle += c * g.joint[j] * (c < 0 ? kFingerBack[f == kThumb ? 0 : 1][j] : 1.0f);
            out[j + 1] = out[j] + rot_axis(g.seg[j], g.axis, angle);
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
    float angle = 0;
    for (int j = 0; j < 3; j++) {
        angle += c * joint[j] * (c < 0 ? kFingerBack[f == kThumb ? 0 : 1][j] : 1.0f) * kPi / 180.0f;
        p = p + rot_axis(dir, axis, angle) * s.len[j];
        out[j + 1] = {p.x * mx, p.y, p.z};
    }
}

Vec3 grip_point(const Pose& pose, Vec3 l) { return pose.position + rotate(pose.orientation, l); }

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
};
ProbeSet sets[2];
int next_set = 0;

GLuint probe_prog = 0, probe_tex = 0, probe_fbo = 0, probe_sampler = 0;
GLint probe_uv_loc = -1, probe_depth_loc = -1;

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
const char* kProbeFs =
    "#version 320 es\nprecision highp float;\nprecision highp sampler2DArray;\n"
    "uniform sampler2DArray uDepth; uniform vec2 uUV[160];\nout vec4 o;\n"
    "void main() {\n"
    "  int i = int(gl_FragCoord.x);\n"
    "  float d = texture(uDepth, vec3(uUV[i], 0.0)).r;\n"
    "  float v = floor(clamp(d, 0.0, 1.0) * 16777215.0 + 0.5);\n"
    "  float r = floor(v / 65536.0); float g = floor((v - r * 65536.0) / 256.0); float b = v - r * 65536.0 - g * 256.0;\n"
    "  o = vec4(r, g, b, 255.0) / 255.0;\n}\n";

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
            // (the view's own probes: only her drawn arms and hands; her column holds her eyes, so a post a few cm
            // before them would count as her)
            own = p.kind != kView && in_her_column(at);
            for (int c = 0; c < s.ncaps && !own; c++) own = in_capsule(s.caps[c], at);
            if (own) {
                hidden = pen > 0;  // behind her: free for a fingertip, not known for the arm's line
                pen = -0.01f;
                answer.own_hits++;
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
    if (probe_uv_loc >= 0) {
        // A vec2 array: uploaded two floats at a time through its elements' locations.
        static GLint locs[kProbeMax];
        static bool have_locs = false;
        if (!have_locs) {
            for (int i = 0; i < kProbeMax; i++) {
                char name[24];
                std::snprintf(name, sizeof name, "uUV[%d]", i);
                locs[i] = glGetUniformLocation(probe_prog, name);
            }
            have_locs = true;
        }
        for (int i = 0; i < kProbeMax; i++)
            if (locs[i] >= 0) glUniform2f(locs[i], uv[i][0], uv[i][1]);
    }
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D_ARRAY, depth_tex);
    glBindSampler(0, probe_sampler);
    glBindVertexArray(vao);
    glDrawArrays(GL_TRIANGLES, 0, 3);
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
bool finger_touches(int h, int f, float c, const Pose& pose, const PartCaps* part, bool weapon) {
    const float r = f == kThumb ? 0.0095f : 0.0085f;  // her finger's half thickness
    Vec3 p[4];
    finger_points(h, f, c, p);
    for (int j = 0; j < 3; j++) {
        if (part)
            for (int k = 0; k < part->n; k++)
                if (seg_dist(p[j], p[j + 1], part->a[k], part->b[k]) - part->r[k] < r) return true;
        if (weapon && weapon_clearance(h, grip_point(pose, p[j]), grip_point(pose, p[j + 1])) < r) return true;
    }
    return false;
}

// The curl (0..upto) where the finger closing first meets what it holds or touches: the last curl free of it (found
// between two steps by halving). -1: it meets nothing; `free_from` then says from which curl it is clear (0 always clear,
// above 0 it begins inside the shape, as a thumb laid along a slide does, and comes out of it there; -1 never clear).
float finger_contact(int h, int f, const Pose& pose, const PartCaps* part, bool weapon, float upto, float& free_from) {
    constexpr int kSteps = 10;
    bool was = finger_touches(h, f, 0.0f, pose, part, weapon);
    free_from = was ? -1.0f : 0.0f;
    float prev = 0;
    for (int i = 1; i <= kSteps; i++) {
        const float c = upto * i / kSteps;
        const bool now_touching = finger_touches(h, f, c, pose, part, weapon);
        if (now_touching != was) {
            float lo = prev, hi = c;  // `was` holds at lo, the other at hi
            for (int k = 0; k < 6; k++) {
                const float m = (lo + hi) * 0.5f;
                if (finger_touches(h, f, m, pose, part, weapon) == was) lo = m;
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
void weapon_contact() {
    if (!answer.valid || answer.age > 3 || !answer.weapon_asked) {
        if (!answer.valid || answer.age > 3) weapon_free = 1.0f;
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
    if (hit) weapon_free = std::clamp(t_hit - 0.02f, 0.0f, 1.0f);
    else if (!(any && end_hidden && weapon_free < 1.0f)) weapon_free = 1.0f;
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
        const Vec3 raw = was ? (s.grip.position - last) * (1.0f / frame_dt()) : Vec3{};
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
        const bool on_weapon = s.on_foregrip;
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
    for (int h = 0; h < 2; h++) {
        HandState& s = hands[h];
        if (!s.valid) {
            weapon_touch[h] = false;
            continue;
        }
        const int role = weapon_hand_role(h);
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
        for (int f = 0; f < 5; f++) {
            float target = s.want[f], free_from = 0;
            if (role == 1 || role == 2) {
                const float hold = weapon_hold_curl(h);
                if (role == 1 && f == kIndex && weapon_is_gun()) {
                    target = trig > 0.05f || first ? 0.42f + 0.33f * trig : 0.10f;
                } else {
                    const float c = finger_contact(h, f, s.shown, nullptr, true, 1.0f, free_from);
                    const float base = f == kThumb ? std::min(hold, 0.7f) : hold;
                    target = c >= 0 ? c : std::max(base, free_from);
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
        }
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
}

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

float weapon_wall_free() { return answer.valid && answer.age <= 3 ? weapon_free : 1.0f; }

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
    std::fprintf(f, "VR probes: answer %s (age %d, %d on her own body), guide dot %s, GL %s, the weapon's line %.0f%% "
                    "free\n",
                 answer.valid ? "read" : "none", answer.age, answer.own_hits, guide_valid ? "on a surface" : "none",
                 gl_ready ? "ready" : gl_failed ? "failed" : "not yet", weapon_free * 100.0f);
}

// Session 11: the shader helpers for sh3vr.cpp's UI pass (a compile or link error logged, 0 returned).
unsigned gl_link_program(const char* vs, const char* fs, const char* what) { return link(vs, fs, what); }

}  // namespace sh3q
