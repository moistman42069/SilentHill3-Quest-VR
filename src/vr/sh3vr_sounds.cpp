// SPDX-License-Identifier: GPL-3.0-or-later
// SH3 Quest VR: a sound for every hand action (the owner: "sound for every VR action"), heard where the hand did it.
//
// Each action the hands do that the game never did itself (a magazine let go, a part taken from the pouch, a magazine
// seated, a shell pushed in, the slide or the knob worked, the pump, a holster, the pouch, the flashlight's pocket, an
// item taken by hand, the trigger on a gun that cannot fire, a dropped part on the floor, a weapon knocked against a
// wall, the other hand taking the weapon's second grip, a hand laid on a surface) asks for its sound here with its
// place in the world.
//
// Session 5, build 46: the asks were kept and logged. Session 9: each is played, mixed over the game's own sound
// (src/win32/dsound.c ds_vr_play) at the game's sound-effects volume, from where it happened: louder on the near ear,
// softer and duller behind the head (as LIST 34's), quieter farther off. What the game has a sound for is the game's
// own, read from its sound archive (data/sound/sd.afs, entry n = sound id n): each gun's empty click (0x29, 0x3D,
// 0x53) for the trigger on a gun that cannot fire, its reload sound (0x28 the handgun's, 0x52 the submachine gun's, the
// shotgun's first shell of 0x3C) when the game's own reload event is not known yet. The rest is made here at start
// (a few variants each, played a little higher or lower each time, so no two sound alike): metal clicks and snaps as
// struck metal's ringing (damped modes), slides and scrapes as filtered noise, leather and cloth as soft noise grains,
// knocks and pats as a falling low thump. The game's 3D sound call (0x004A33B0) is not used: it also tells the
// monsters where a noise was (0x712C0DD..0x712C0F0), which a hand on a wall must not.

#include "sh3vr_internal.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

extern "C" {
int ds_vr_play(const float* pcm, int frames, int rate, float gl, float gr, float behind);  // src/win32/dsound.c
const char* rt_path(const char* guest, char* out, size_t n);                                // src/runtime
}

namespace sh3q {

namespace {

const char* const kSoundNames[] = {
    "magazine released", "part taken", "magazine seated", "shell in", "slide back", "slide forward", "pump back",
    "pump forward", "holster draw", "holster put", "supplies pouch", "flashlight pocket", "item taken", "dry click",
    "part on the floor", "weapon on a wall", "second grip taken", "hand on a surface", "struggle shake",
    "the VR menu", "something used",
};
static_assert(sizeof kSoundNames / sizeof *kSoundNames == kSoundCount, "a name for every VR sound");

struct Ask {
    int what = -1;
    Vec3 world;
    int detail = -1;
    int frame = 0;
};
constexpr int kAsks = 16;
Ask asks[kAsks];
int next_ask = 0, total = 0, logged = 0, played = 0;
int counts[kSoundCount]{};

// ---------------------------------------------------------------- the clips

constexpr int kRate = 44100;
constexpr int kVariants = 3;
struct Clip {
    std::vector<float> pcm;
    int rate = kRate;
    int id = -1;  // the game's sound id it came from, or -1: made here
};
Clip made[kSoundCount][kVariants];  // made here
// The game's own (sd.afs): the empty clicks and reload sounds, by gun (10 handgun, 11 shotgun, 12 submachine gun).
Clip game_click[3], game_reload[3];
bool clips_ready = false;
int game_clips = 0;

struct Rng {
    uint32_t s;
    float next() {  // -1..1
        s ^= s << 13, s ^= s >> 17, s ^= s << 5;
        return (float)(s & 0xFFFFFF) / 8388608.0f - 1.0f;
    }
    float range(float lo, float hi) { return lo + (hi - lo) * (0.5f + 0.5f * next()); }
};

// A biquad (RBJ's): band-pass at constant peak gain, or low-pass.
struct Biquad {
    float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
    float run(float x) {
        const float y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }
};
Biquad band(float f, float q) {
    const float w = 2.0f * kPi * f / kRate, al = std::sin(w) / (2.0f * q), a0 = 1.0f + al;
    Biquad b;
    b.b0 = al / a0, b.b1 = 0, b.b2 = -al / a0, b.a1 = -2.0f * std::cos(w) / a0, b.a2 = (1.0f - al) / a0;
    return b;
}
Biquad low(float f, float q) {
    const float w = 2.0f * kPi * f / kRate, al = std::sin(w) / (2.0f * q), a0 = 1.0f + al, c = std::cos(w);
    Biquad b;
    b.b0 = (1.0f - c) * 0.5f / a0, b.b1 = (1.0f - c) / a0, b.b2 = (1.0f - c) * 0.5f / a0;
    b.a1 = -2.0f * c / a0, b.a2 = (1.0f - al) / a0;
    return b;
}

struct Maker {
    std::vector<float> buf;
    Rng rng{1};
    float jit = 1.0f;  // this variant's pitch (every frequency times it)
    void need(float end_s) {
        const size_t n = (size_t)(end_s * kRate) + 1;
        if (buf.size() < n) buf.resize(n, 0.0f);
    }
    // Struck metal or plastic: damped sines (random phase, each softer than the last) and a 1 ms tick of noise.
    void modes(float t0, std::initializer_list<float> f, std::initializer_list<float> tau, float amp) {
        float longest = 0;
        for (float t : tau) longest = std::max(longest, t);
        need(t0 + longest * 7.0f);
        const size_t s0 = (size_t)(t0 * kRate);
        auto fr_i = f.begin();
        auto ta_i = tau.begin();
        float w = 1.0f;
        for (; fr_i != f.end() && ta_i != tau.end(); ++fr_i, ++ta_i, w *= 0.62f) {
            const float fr = *fr_i * jit * rng.range(0.985f, 1.015f), ta = *ta_i, ph = rng.range(0.0f, 2.0f * kPi);
            const size_t n = (size_t)(ta * 7.0f * kRate);
            for (size_t i = 0; i < n && s0 + i < buf.size(); i++) {
                const float t = (float)i / kRate;
                buf[s0 + i] += amp * w * std::exp(-t / ta) * std::sin(2.0f * kPi * fr * t + ph);
            }
        }
        Biquad hp = band(5000.0f * jit, 0.7f);
        const size_t tick = (size_t)(0.0012f * kRate);
        for (size_t i = 0; i < tick && s0 + i < buf.size(); i++)
            buf[s0 + i] += amp * 0.5f * hp.run(rng.next()) * (1.0f - (float)i / tick);
    }
    // A low thump: a sine falling from f0 to f1, dying away (tau).
    void thump(float t0, float f0, float f1, float tau, float amp) {
        need(t0 + tau * 7.0f);
        const size_t s0 = (size_t)(t0 * kRate), n = (size_t)(tau * 7.0f * kRate);
        float ph = 0;
        for (size_t i = 0; i < n && s0 + i < buf.size(); i++) {
            const float t = (float)i / kRate;
            const float fr = (f1 + (f0 - f1) * std::exp(-t / (tau * 0.6f))) * jit;
            ph += 2.0f * kPi * fr / kRate;
            buf[s0 + i] += amp * std::min(1.0f, t / 0.0015f) * std::exp(-t / tau) * std::sin(ph);
        }
    }
    // Noise through a band (a scrape, a slide, leather): over `dur` s, its loudness rising (rise: the share of it spent
    // rising) and falling, roughened (grain 0..1: how much a 40-90 Hz flutter takes from it).
    void noise(float t0, float dur, float f, float q, float amp, float rise = 0.5f, float grain = 0.4f) {
        need(t0 + dur);
        Biquad b = band(f * jit, q), b2 = band(f * jit, q), fl = low(rng.range(40.0f, 90.0f), 0.7f);
        const size_t s0 = (size_t)(t0 * kRate), n = (size_t)(dur * kRate);
        for (size_t i = 0; i < n && s0 + i < buf.size(); i++) {
            const float u = (float)i / n;
            const float env = u < rise ? std::sin(0.5f * kPi * u / rise) : std::cos(0.5f * kPi * (u - rise) / (1.0f - rise));
            const float rough = 1.0f - grain + grain * std::min(1.0f, std::fabs(fl.run(rng.next())) * 6.0f);
            buf[s0 + i] += amp * env * env * rough * b2.run(b.run(rng.next())) * 2.5f;
        }
    }
    // A burst of noise dying away (a pat, a strike's grit): low-passed at f.
    void burst(float t0, float f, float tau, float amp) {
        need(t0 + tau * 6.0f);
        Biquad lp = low(f * jit, 0.8f);
        const size_t s0 = (size_t)(t0 * kRate), n = (size_t)(tau * 6.0f * kRate);
        for (size_t i = 0; i < n && s0 + i < buf.size(); i++) {
            const float t = (float)i / kRate;
            buf[s0 + i] += amp * std::min(1.0f, t / 0.001f) * std::exp(-t / tau) * lp.run(rng.next()) * 2.0f;
        }
    }
    // Cloth: soft grains of noise across `dur` s.
    void rustle(float t0, float dur, int grains, float f, float amp) {
        for (int g = 0; g < grains; g++) {
            const float at = t0 + dur * (g + rng.range(0.0f, 0.6f)) / grains;
            noise(at, rng.range(0.025f, 0.05f), f * rng.range(0.85f, 1.2f), 0.8f, amp * rng.range(0.5f, 1.0f), 0.35f, 0.6f);
        }
    }
    // Done: faded out at its end, its peak at `peak`.
    std::vector<float> finish(float peak = 0.9f) {
        float most = 1e-6f;
        for (float v : buf) most = std::max(most, std::fabs(v));
        const size_t fade = std::min(buf.size(), (size_t)(0.004f * kRate));
        for (size_t i = 0; i < fade; i++) buf[buf.size() - 1 - i] *= (float)i / fade;
        for (float& v : buf) v *= peak / most;
        return buf;
    }
};

void make_clip(int what, int variant, Maker& m) {
    switch (what) {
    case kSoundMagRelease:  // the release button's click, the magazine sliding out, clearing the well
        m.modes(0.0f, {3100, 4700, 6900}, {0.012f, 0.008f, 0.006f}, 1.0f);
        m.noise(0.004f, 0.07f, 2600, 1.2f, 0.35f, 0.3f, 0.6f);
        m.modes(0.07f, {1500, 2600}, {0.02f, 0.012f}, 0.4f);
        break;
    case kSoundPartTaken:  // out of the pouch: cloth, then the part's tap on the fingers
        m.rustle(0.0f, 0.14f, 4, 1400, 0.5f);
        m.modes(0.10f, {1800, 2900}, {0.01f, 0.007f}, 0.35f);
        break;
    case kSoundMagSeated:  // in the well, the catch snapping shut
        m.noise(0.0f, 0.03f, 2000, 1.0f, 0.4f, 0.6f, 0.4f);
        m.modes(0.03f, {2400, 3700, 5600}, {0.02f, 0.012f, 0.008f}, 1.0f);
        m.thump(0.03f, 220, 140, 0.03f, 0.6f);
        break;
    case kSoundShellIn:  // a shell pushed past the loading gate
        m.noise(0.0f, 0.04f, 1700, 1.0f, 0.4f, 0.6f, 0.4f);
        m.modes(0.04f, {1900, 3300}, {0.015f, 0.01f}, 0.8f);
        m.thump(0.04f, 300, 200, 0.02f, 0.4f);
        break;
    case kSoundSlideBack:  // metal on metal, stopping at the back with a click
        m.noise(0.0f, 0.085f, 3000, 1.0f, 0.5f, 0.85f, 0.5f);
        m.modes(0.085f, {2300, 3900, 6100}, {0.025f, 0.015f, 0.008f}, 0.8f);
        break;
    case kSoundSlideForward:  // let go: a short slide and the snap home
        m.noise(0.0f, 0.035f, 3200, 1.0f, 0.45f, 0.8f, 0.4f);
        m.modes(0.035f, {1900, 3400, 5300, 7600}, {0.04f, 0.025f, 0.015f, 0.008f}, 1.0f);
        m.thump(0.035f, 260, 160, 0.035f, 0.7f);
        break;
    case kSoundPumpBack:
        m.noise(0.0f, 0.10f, 1800, 0.9f, 0.6f, 0.8f, 0.4f);
        m.modes(0.10f, {1250, 2150, 3500}, {0.035f, 0.02f, 0.012f}, 0.8f);
        m.thump(0.10f, 170, 120, 0.04f, 0.6f);
        break;
    case kSoundPumpForward:
        m.noise(0.0f, 0.085f, 1900, 0.9f, 0.6f, 0.8f, 0.4f);
        m.modes(0.085f, {1400, 2400, 3800, 5600}, {0.045f, 0.028f, 0.016f, 0.009f}, 1.0f);
        m.thump(0.085f, 190, 130, 0.045f, 0.8f);
        break;
    case kSoundHolsterDraw:  // out of the leather: a pull, the swish, a faint tick of metal leaving it
        m.thump(0.0f, 120, 90, 0.03f, 0.3f);
        m.noise(0.0f, 0.18f, 1100, 0.7f, 0.7f, 0.4f, 0.7f);
        m.modes(0.16f, {3300, 5100}, {0.008f, 0.005f}, 0.15f);
        break;
    case kSoundHolsterPut:  // into the leather and down
        m.noise(0.0f, 0.14f, 950, 0.7f, 0.7f, 0.6f, 0.6f);
        m.thump(0.13f, 140, 95, 0.04f, 0.6f);
        m.burst(0.13f, 600, 0.008f, 0.4f);
        break;
    case kSoundPouch:  // the pouch opened: cloth and its flap
        m.rustle(0.0f, 0.2f, 5, 1600, 0.5f);
        m.thump(0.17f, 160, 110, 0.03f, 0.3f);
        break;
    case kSoundLightPocket:  // the flashlight's clip on her chest pocket
        m.rustle(0.0f, 0.11f, 3, 1500, 0.5f);
        m.modes(0.09f, {2100, 3600}, {0.01f, 0.006f}, 0.45f);
        break;
    case kSoundTake:  // an item in the hand (the game's own chime follows when it is added)
        m.rustle(0.0f, 0.07f, 2, 1300, 0.5f);
        m.thump(0.04f, 200, 150, 0.02f, 0.2f);
        break;
    case kSoundDryClick:  // the trigger on nothing (the game's own click is used where it is read)
        m.modes(0.0f, {2600, 4300, 6400}, {0.009f, 0.006f, 0.004f}, 1.0f);
        break;
    case kSoundPartFloor:  // one per part (variant 0 a magazine, 1 a shell, 2 a battery)
        if (variant == 1) {
            m.modes(0.0f, {1700, 2900, 4300}, {0.03f, 0.02f, 0.012f}, 1.0f);
            m.thump(0.0f, 260, 200, 0.015f, 0.3f);
        } else if (variant == 2) {
            m.modes(0.0f, {1100, 2000}, {0.025f, 0.015f}, 0.8f);
            m.thump(0.0f, 180, 130, 0.03f, 0.6f);
        } else {
            m.modes(0.0f, {900, 1650, 2700, 4100}, {0.06f, 0.04f, 0.025f, 0.015f}, 1.0f);
            m.thump(0.0f, 150, 100, 0.04f, 0.7f);
        }
        break;
    case kSoundWeaponWall:  // a knock: the body's thump, the weapon's ring, the wall's grit
        m.thump(0.0f, 120, 85, 0.05f, 0.9f);
        m.modes(0.0f, {1050, 2200, 3600}, {0.04f, 0.025f, 0.014f}, 0.8f);
        m.burst(0.0f, 900, 0.008f, 0.4f);
        break;
    case kSoundGrabWeapon:  // the other hand closing on the gun
        m.noise(0.0f, 0.05f, 1200, 0.8f, 0.6f, 0.3f, 0.5f);
        m.thump(0.01f, 230, 170, 0.018f, 0.4f);
        m.modes(0.01f, {1500}, {0.01f}, 0.15f);
        break;
    case kSoundHandSurface:  // a palm laid on a wall, a table, the floor: a pat (vr133: a 110 Hz thump alone, too low
                             // for a headset's speakers; the skin's slap now leads)
        m.burst(0.0f, 1300, 0.015f, 1.0f);
        m.burst(0.002f, 3200, 0.004f, 0.25f);
        m.thump(0.0f, 180, 120, 0.025f, 0.45f);
        break;
    case kSoundStruggle:  // a shake while held (owner message 38): her clothes pulled, a scuff, the hold giving a little
        m.rustle(0.0f, 0.13f, 4, 1400, 0.6f);
        m.noise(0.02f, 0.08f, 700, 0.8f, 0.6f, 0.5f, 0.5f);
        m.thump(0.03f, 150, 100, 0.03f, 0.35f);
        break;
    case kSoundUse:  // 0.1.7 (a player on 0.1.6: "Sometimes it's not very clear if one has properly interacted with a
                     // door or object"): the action found something to use: a soft low knock and a bright tick over it
        m.thump(0.0f, 210, 160, 0.02f, 0.5f);
        m.modes(0.006f, {1900, 2850}, {0.014f, 0.009f}, 0.55f);
        break;
    case kSoundMenu:  // the VR menu (owner 80): a soft two-note tick, rising as it opens (variant 0), falling as it
                      // closes (1); 0.1.5: the view recentred (both sticks clicked together), two even notes (2)
        if (variant == 2) {
            m.modes(0.0f, {2000, 3000}, {0.012f, 0.008f}, 0.6f);
            m.modes(0.070f, {2000, 3000}, {0.012f, 0.008f}, 0.6f);
        } else if (variant == 1) {
            m.modes(0.0f, {2400, 3600}, {0.012f, 0.008f}, 0.7f);
            m.modes(0.055f, {1700, 2550}, {0.016f, 0.010f}, 0.6f);
        } else {
            m.modes(0.0f, {1700, 2550}, {0.012f, 0.008f}, 0.6f);
            m.modes(0.055f, {2400, 3600}, {0.016f, 0.010f}, 0.7f);
        }
        break;
    default:
        break;
    }
}

// The gain each is played at (its clip's peak is 0.9): under the game's gunshots, over its footsteps' rustle.
float base_gain(int what) {
    switch (what) {
    case kSoundMagRelease: return 0.32f;
    case kSoundPartTaken: return 0.22f;
    case kSoundMagSeated: return 0.35f;
    case kSoundShellIn: return 0.30f;
    case kSoundSlideBack: return 0.33f;
    case kSoundSlideForward: return 0.42f;
    case kSoundPumpBack: return 0.38f;
    case kSoundPumpForward: return 0.45f;
    case kSoundHolsterDraw: return 0.26f;
    case kSoundHolsterPut: return 0.30f;
    case kSoundPouch: return 0.22f;
    case kSoundLightPocket: return 0.24f;
    case kSoundTake: return 0.16f;
    case kSoundDryClick: return 0.30f;
    case kSoundPartFloor: return 0.34f;
    case kSoundWeaponWall: return 0.40f;
    case kSoundGrabWeapon: return 0.20f;
    case kSoundHandSurface: return 0.22f;
    case kSoundStruggle: return 0.24f;
    case kSoundMenu: return 0.20f;
    case kSoundUse: return 0.18f;
    default: return 0.25f;
    }
}

// ---------------------------------------------------------------- the game's own (data/sound/sd.afs)

bool read_wav(const std::vector<uint8_t>& b, Clip& out) {
    if (b.size() < 44 || std::memcmp(b.data(), "RIFF", 4) || std::memcmp(b.data() + 8, "WAVE", 4)) return false;
    size_t pos = 12;
    int ch = 0, rate = 0, bits = 0, tag = 0;
    const uint8_t* data = nullptr;
    size_t data_n = 0;
    while (pos + 8 <= b.size()) {
        uint32_t size;
        std::memcpy(&size, b.data() + pos + 4, 4);
        const uint8_t* body = b.data() + pos + 8;
        if (pos + 8 + size > b.size()) size = (uint32_t)(b.size() - pos - 8);
        if (!std::memcmp(b.data() + pos, "fmt ", 4) && size >= 16) {
            uint16_t t, c, bi;
            uint32_t r;
            std::memcpy(&t, body, 2), std::memcpy(&c, body + 2, 2), std::memcpy(&r, body + 4, 4);
            std::memcpy(&bi, body + 14, 2);
            tag = t, ch = c, rate = (int)r, bits = bi;
        } else if (!std::memcmp(b.data() + pos, "data", 4)) {
            data = body, data_n = size;
        }
        pos += 8 + size + (size & 1);
    }
    if (tag != 1 || bits != 16 || ch < 1 || ch > 2 || rate < 8000 || rate > 96000 || !data) return false;
    const size_t frames = data_n / (2 * ch);
    out.pcm.resize(frames);
    for (size_t i = 0; i < frames; i++) {
        float s = 0;
        for (int c = 0; c < ch; c++) {
            int16_t v;
            std::memcpy(&v, data + (i * ch + c) * 2, 2);
            s += v / 32768.0f;
        }
        out.pcm[i] = s / ch;
    }
    out.rate = rate;
    return frames > 16;
}

// Only its first shell (session 9, tools/onsets.py: the shotgun's reload 0x3C is two shells, each a soft hit and three
// loud ones, the second from 369 ms of 697): cut at the quietest 10 ms in the middle of it (35-65 %), faded out.
void first_shell(Clip& c) {
    const int win = std::max(1, c.rate / 100), frames = (int)c.pcm.size() / win;
    if (frames < 8) return;
    int cut = frames / 2;
    float least = 1e9f;
    for (int k = frames * 35 / 100; k <= frames * 65 / 100; k++) {
        double e = 0;
        for (int i = 0; i < win; i++) e += c.pcm[k * win + i] * c.pcm[k * win + i];
        if (e < least) least = (float)e, cut = k;
    }
    c.pcm.resize((size_t)cut * win + win / 2);
    const size_t fade = std::min(c.pcm.size(), (size_t)(0.008f * c.rate));
    for (size_t i = 0; i < fade; i++) c.pcm[c.pcm.size() - 1 - i] *= (float)i / fade;
}

void load_game_clips() {
    char path[600];
    if (!rt_path("data\\sound\\sd.afs", path, sizeof path)) return;
    FILE* f = std::fopen(path, "rb");
    if (!f) {
        rt_log("VR sound: the game's sounds (%s) could not be opened: the hands' own made here instead", path);
        return;
    }
    uint8_t head[8];
    uint32_t n = 0;
    if (std::fread(head, 1, 8, f) == 8 && !std::memcmp(head, "AFS", 3)) std::memcpy(&n, head + 4, 4);
    struct Want {
        Clip* to;
        int id;
    };
    const Want want[] = {{&game_click[0], 0x29}, {&game_click[1], 0x3D}, {&game_click[2], 0x53},
                         {&game_reload[0], 0x28}, {&game_reload[1], 0x3C}, {&game_reload[2], 0x52}};
    for (const Want& w : want) {
        if ((uint32_t)w.id >= n || n > 4096) break;
        uint32_t entry[2];
        if (std::fseek(f, 8 + 8 * w.id, SEEK_SET) || std::fread(entry, 4, 2, f) != 2) continue;
        if (entry[1] < 44 || entry[1] > (4u << 20) || std::fseek(f, (long)entry[0], SEEK_SET)) continue;
        std::vector<uint8_t> b(entry[1]);
        if (std::fread(b.data(), 1, b.size(), f) != b.size()) continue;
        if (read_wav(b, *w.to)) {
            w.to->id = w.id;
            if (w.id == 0x3C) first_shell(*w.to);
            // its peak at 0.9 as the ones made here (the archive's are about 0.3), so each sound's gain means the same
            float most = 1e-6f;
            for (float v : w.to->pcm) most = std::max(most, std::fabs(v));
            for (float& v : w.to->pcm) v *= 0.9f / most;
            game_clips++;
        }
    }
    std::fclose(f);
    rt_log("VR sound: %d of 6 of the game's own sounds read from %s (the guns' empty clicks 0x29 0x3D 0x53, their "
           "reloads 0x28 0x3C 0x52; the shotgun's first shell %.0f ms)", game_clips, path,
           game_reload[1].pcm.size() * 1000.0f / std::max(1, game_reload[1].rate));
}

void make_clips() {
    clips_ready = true;
    for (int w = 0; w < kSoundCount; w++)
        for (int v = 0; v < kVariants; v++) {
            Maker m;
            m.rng.s = 0x9E3779B9u ^ (uint32_t)(w * 7919 + v * 104729 + 1);
            m.jit = w == kSoundPartFloor ? 1.0f : 1.0f + 0.04f * (v - 1);
            make_clip(w, v, m);
            made[w][v].pcm = m.finish();
            made[w][v].rate = kRate;
        }
    load_game_clips();
    // SH3_SOUND_DUMP=folder (the PC rig): each clip as a 16-bit WAV there (<sound>_<variant>.wav, game_<id>.wav), to
    // be measured (tools/sound_survey.py --files).
    if (const char* dir = getenv("SH3_SOUND_DUMP")) {
        auto write = [&](const Clip& c, const char* name) {
            char p[700];
            std::snprintf(p, sizeof p, "%s/%s.wav", dir, name);
            FILE* f = std::fopen(p, "wb");
            if (!f) return;
            const uint32_t n = (uint32_t)c.pcm.size(), bytes = n * 2, rate = (uint32_t)c.rate, br = rate * 2,
                           riff = 36 + bytes, fmt_n = 16;
            const uint16_t one = 1, align = 2, bits = 16;
            std::fwrite("RIFF", 1, 4, f), std::fwrite(&riff, 4, 1, f), std::fwrite("WAVEfmt ", 1, 8, f);
            std::fwrite(&fmt_n, 4, 1, f), std::fwrite(&one, 2, 1, f), std::fwrite(&one, 2, 1, f);
            std::fwrite(&rate, 4, 1, f), std::fwrite(&br, 4, 1, f), std::fwrite(&align, 2, 1, f);
            std::fwrite(&bits, 2, 1, f), std::fwrite("data", 1, 4, f), std::fwrite(&bytes, 4, 1, f);
            for (float v : c.pcm) {
                const int16_t s = (int16_t)std::lrint(std::clamp(v, -1.0f, 1.0f) * 32767.0f);
                std::fwrite(&s, 2, 1, f);
            }
            std::fclose(f);
        };
        char name[64];
        for (int w = 0; w < kSoundCount; w++)
            for (int v = 0; v < kVariants; v++) {
                std::snprintf(name, sizeof name, "%02d_%d", w, v);
                write(made[w][v], name);
            }
        for (const Clip* c : {&game_click[0], &game_click[1], &game_click[2], &game_reload[0], &game_reload[1],
                              &game_reload[2]})
            if (c->id >= 0) {
                std::snprintf(name, sizeof name, "game_%02x", c->id);
                write(*c, name);
            }
        rt_log("VR sound: the clips written to %s", dir);
    }
}

double seconds() {
    using namespace std::chrono;
    static const steady_clock::time_point t0 = steady_clock::now();
    return duration<double>(steady_clock::now() - t0).count();
}

// The clip for an ask: the game's own where it is read, else one made here (a variant not the last one played).
const Clip* clip_for(const Ask& a, int& variant) {
    const int item = a.detail >= kSoundByGame ? a.detail - kSoundByGame : a.detail;
    const int gun = item >= 10 && item <= 12 ? item - 10 : -1;
    variant = -1;
    if (a.what == kSoundDryClick && gun >= 0 && !game_click[gun].pcm.empty()) return &game_click[gun];
    if ((a.what == kSoundMagSeated || a.what == kSoundShellIn) && gun >= 0 && !game_reload[gun].pcm.empty())
        return &game_reload[gun];
    if (a.what == kSoundPartFloor) {
        variant = a.detail == kPartShell ? 1 : a.detail == kPartBattery ? 2 : 0;
        return &made[a.what][variant];
    }
    if (a.what == kSoundMenu) {  // opened (detail 1): rising; closed: falling; 2: the view recentred
        variant = a.detail == 1 ? 0 : a.detail == 2 ? 2 : 1;
        return &made[a.what][variant];
    }
    static int last[kSoundCount];
    static uint32_t pick = 12345;
    pick = pick * 1664525u + 1013904223u;
    variant = (int)((pick >> 16) % (kVariants - 1));
    if (variant >= last[a.what]) variant++;
    last[a.what] = variant;
    return &made[a.what][variant];
}

void play(const Ask& a, char* note, size_t note_n) {
    note[0] = 0;
    // (only a seated magazine or shell carries it: a touch's detail is a speed, vr133's 1253 cm/s was not the game's)
    if ((a.what == kSoundMagSeated || a.what == kSoundShellIn) && a.detail >= kSoundByGame) {
        std::snprintf(note, note_n, ": the game's own reload sound");
        return;
    }
    if (!opt.hand_sounds) {
        std::snprintf(note, note_n, ": Hand sounds off");
        return;
    }
    static double last_at[kSoundCount] = {};
    static Vec3 last_where[kSoundCount];
    static float repeat_gain[kSoundCount];
    const double now = seconds();
    // The same sound at the same place (10 cm) again within 60 ms is one (a touch flickering on and off; vr133: the
    // other hand's touch the same frame was dropped as one, so the place counts); a part's bounces come softer.
    const bool same_place = length(a.world - last_where[a.what]) < 0.10f * kUnitsPerMetre;
    if (last_at[a.what] > 0 && now - last_at[a.what] < 0.06 && same_place) {
        std::snprintf(note, note_n, ": the same sound %.0f ms before, not again", (now - last_at[a.what]) * 1000.0);
        return;
    }
    repeat_gain[a.what] = last_at[a.what] > 0 && now - last_at[a.what] < 0.6 ? repeat_gain[a.what] * 0.5f : 1.0f;
    last_at[a.what] = now;
    last_where[a.what] = a.world;
    int variant = -1;
    const Clip* c = clip_for(a, variant);
    if (!c || c->pcm.empty()) return;
    float gain = base_gain(a.what);
    if (a.what == kSoundPartFloor) gain *= repeat_gain[a.what];
    if (a.what == kSoundWeaponWall || a.what == kSoundHandSurface) {
        // louder the faster it came (vr133: a hand laid still was played at 0.04, too soft to hear)
        const float speed = std::max(0, a.detail) / 100.0f;  // m a second
        gain *= std::clamp(0.6f + speed * 0.4f, 0.6f, 1.4f);
    }
    // From where it was: the near ear louder, behind the head softer and duller, farther off quieter.
    const Vec3 t = tracking_point(a.world);
    Vec3 d = t - fi.head.position;
    const float dist = length(d);
    float side = 0, behind = 0;
    if (dist > 1e-3f && std::isfinite(dist)) {
        d = d * (1.0f / dist);
        side = dot(d, rotate(fi.head.orientation, Vec3{1, 0, 0}));
        behind = std::max(0.0f, -dot(d, rotate(fi.head.orientation, Vec3{0, 0, -1})));
    }
    const float near = std::isfinite(dist) ? std::clamp(0.55f / (0.25f + dist), 0.2f, 1.3f) : 1.0f;
    const float gl = gain * near * std::sqrt(0.5f * (1.0f - 0.75f * side));
    const float gr = gain * near * std::sqrt(0.5f * (1.0f + 0.75f * side));
    // A little higher or lower each time (the game's own a little less).
    static uint32_t r = 777;
    r = r * 1664525u + 1013904223u;
    const float pitch = (c->id >= 0 ? 0.03f : 0.06f) * (((r >> 8) & 0xFFFF) / 32767.5f - 1.0f);
    const int rate = (int)(c->rate * (1.0f + pitch));
    if (!ds_vr_play(c->pcm.data(), (int)c->pcm.size(), rate, gl, gr, behind)) {
        std::snprintf(note, note_n, ": no sound device");
        return;
    }
    played++;
    if (c->id >= 0)
        std::snprintf(note, note_n, ": played the game's 0x%02X, %.2f m off, left %.2f right %.2f, behind %.2f", c->id,
                      dist, gl, gr, behind);
    else
        std::snprintf(note, note_n, ": played (made here, variant %d, %.0f ms), %.2f m off, left %.2f right %.2f, "
                      "behind %.2f", variant, c->pcm.size() * 1000.0f / c->rate, dist, gl, gr, behind);
}

}  // namespace

void hand_sound(int what, Vec3 world, int detail) {
    if (what < 0 || what >= kSoundCount) return;
    Ask& a = asks[next_ask];
    next_ask = (next_ask + 1) % kAsks;
    a.what = what;
    a.world = world;
    a.detail = detail;
    a.frame = rt_frame;
    total++;
    counts[what]++;
}

void sounds_frame() {
    if (!clips_ready) make_clips();
    // Each ask played as it comes; the log names it (the first 200 of a run, then every 50th).
    while (logged < total) {
        const int back = total - logged;
        if (back > kAsks) {
            logged = total - kAsks;
            continue;
        }
        const Ask& a = asks[(next_ask - back + kAsks * 2) % kAsks];
        char note[200];
        play(a, note, sizeof note);
        if (logged < 200 || logged % 50 == 0)
            rt_log("VR sound: frame %d: %s (%d) at %.0f %.0f %.0f%s", a.frame, kSoundNames[a.what], a.detail, a.world.x,
                   a.world.y, a.world.z, note);
        logged++;
    }
}

void sounds_crash_info(FILE* f) {
    std::fprintf(f, "VR sounds: %d asked, %d played (%d of the game's own read);", total, played, game_clips);
    for (int i = 0; i < kSoundCount; i++)
        if (counts[i]) std::fprintf(f, " %s %d,", kSoundNames[i], counts[i]);
    const Ask& last = asks[(next_ask + kAsks - 1) % kAsks];
    if (last.what >= 0)
        std::fprintf(f, " the last: %s at frame %d", kSoundNames[last.what], last.frame);
    std::fprintf(f, "\n");
}

}  // namespace sh3q
