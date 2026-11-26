// SPDX-License-Identifier: GPL-3.0-or-later
// SH3 Quest VR: manual reload (LIST 46, "Manual reload", on), built on Halo CE Quest VR's (port/linux/game/
// vr_manual_reload.c and vr_manual_reload_math.h: the steps, ammo moving only when a step commits, a haptic for each step,
// the hints for the first reloads of a run), made for Silent Hill 3's four guns and more exact about where things go:
//
// - The handgun and the submachine gun: the magazine release (the right stick pushed down) drops the magazine; the off
//   hand takes a new one from the ammo pouch at the front of the belt on its side and pushes it into the well (both
//   under the grip: the submachine gun's magazine goes up through its grip): lined up with the well first, then pushed
//   home. A new magazine can be taken before the old one is dropped (the tactical way). A gun emptied before the
//   magazine came out then needs its slide (the handgun) or cocking knob (the submachine gun, on top at the back) taken
//   with the off hand, pulled back its whole travel and let go; with a round left in the chamber it fires at once.
// - Session 5: the well, the slide and the knob are the drawn gun's own (its model's points through the weapon's frame);
//   the part in the hand is drawn (sh3vr_parts.cpp), guided onto the well's line once lined up and pushed home when it
//   seats, the hand with it (RE4 VR's way); the dropped magazine falls out of the well; the slide, the knob and the
//   pump move as their own faces (sh3vr_parts.cpp) with the hand kept on them; a sound for each step (sh3vr_sounds.cpp).
// - The shotgun: shells one at a time from the pouch, each pushed into the loading port under the gun; emptied, the
//   pump is then pulled back and pushed forward again before it fires.
// - The stun gun: its battery dropped with the release and a new one pushed in under the grip (the game itself never
//   reloads it in play, only the inventory's batteries did).
// - Ammo is the game's own: a seated magazine or battery is the game's reload transaction (0x497860: what was loaded
//   back to the reserve, then as many as the gun takes), a shell is one round from the reserve. Nothing moves before
//   that, so a reload left half-way (a weapon change, third person, the inventory's reload) loses nothing and gives
//   nothing. A pouch with no reserve gives nothing; the release then keeps the magazine in.
// - The sounds: the game's reload event's own (its motion event's index, learned when the game reloads a gun itself and
//   kept in sh3quest_reload.ini).
// - While the hands reload (first person, both controllers tracked) Heather's attack decision never starts the game's
//   own reload (sh3.c 0x494C40); the trigger does not fire with the magazine out or the slide not yet pulled (a light
//   click in the hand instead). Third person, or a controller lost: the game reloads as it always did.

#include "sh3vr_internal.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace sh3q {

namespace {

enum Part { kMagazine, kShell, kBattery };
enum Action { kNoAction, kSlide, kHandle, kPump };

// Each gun: where its part seats, the way the part goes in, where the off hand's fist takes its slide or knob and how
// far that is pulled. Session 5: from the gun's own model (the held bone's units, through the weapon's frame in the hand,
// sh3vr_combat.cpp weapon_point), so the well is where the drawn grip's bottom is however the gun is held or adjusted;
// the old places in the main hand's aim space (x right, y up, -z forward; metres from the palm) stay for a gun whose
// hold is not measured (the shotgun until its run) and for the frames before the weapon is placed.
struct GunProfile {
    int item;                  // the gun's inventory item (sh3.c vr_gun_item)
    const char* name;
    Part part;
    Action action;
    Vec3 well, insert, grab;   // the aim space's (m)
    float travel;              // m
    const char* well_words;    // the hint's "push it ..."
    bool model;                // the model's places below are measured
    Vec3 well_m;               // the well's mouth (where the part's top goes in)
    Vec3 insert_m;             // the way in (unit)
    Vec3 seat_bottom_m;        // the seated part's bottom (where a dropped one leaves from)
    Vec3 grab_m;               // the other fist's centre on the slide or the knob, at rest
    Vec3 grab_rod_m, grab_palm_m;  // that fist's rod (little finger to thumb) and the way its palm faces (unit)
};
// The models (session 5, scratchpad grip_bottom.py / grip_slices.py on run vr40's dumps; model x forward, y up):
// - Handgun: the grip's bottom raked from y -28 (back) to -22 (front), x 21-52, z -24 to -7: the well's mouth at its
//   centre, the magazine going in up the grip's rake (0.21, 0.978); the slide (y 25-50) taken over the top at its
//   rear (x 26), the fist's rod along the slide towards the shooter, the palm down on it.
// - Submachine gun: its magazine (x 35-52, z -18 to -9) comes out of the grip's bottom at y -19 and ends at y -43.4,
//   raked 6 degrees (0.105, 0.994); the cocking knob on top at the rear (x -1.5 to 13.5, y 54.5-70.5).
// - Stun gun: its box's bottom end (25, -40) on its long axis (0.451, 0.891, -0.051): the battery goes in there.
const GunProfile kGuns[4] = {
    {9, "stun gun", kBattery, kNoAction, {0.0f, -0.075f, 0.010f}, {0.0f, 0.95f, -0.31f}, {}, 0.0f,
     "Push it into the bottom end of the stun gun", true, {25.4f, -39.8f, -9.7f}, {0.451f, 0.891f, -0.051f},
     {25.4f, -39.8f, -9.7f}, {}, {}, {}},
    {10, "handgun", kMagazine, kSlide, {0.0f, -0.065f, 0.010f}, {0.0f, 0.95f, -0.31f}, {0.0f, 0.075f, 0.020f}, 0.035f,
     "Push it up into the bottom of the grip", true, {36.5f, -25.0f, -15.0f}, {0.210f, 0.978f, 0.0f},
     {36.5f, -25.5f, -15.0f}, {26.0f, 37.0f, -13.9f}, {-1.0f, 0.0f, 0.0f}, {0.0f, -1.0f, 0.0f}},
    // The shotgun's loading port on its model (session 16, vr237/vr238, owner 75): the aim-space port of session 4 lay
    // at the model's (-212.7, 47.0, -7.6) once the gun was held as the game holds it (sessions 12-14): at the trigger
    // guard's front, 5.4 cm under the receiver's floor, in the air, so a shell pushed into the port that shows "seated"
    // short of it and was drawn going in under the gun. Now the port's middle on the receiver's floor (bone 1, y down:
    // the receiver x -250 to -140, its floor y 21; the trigger guard x -240 to -212 under it), the way in up and
    // forward towards the magazine tube, the shell going its whole length in (its bottom at the port when seated).
    {11, "shotgun", kShell, kPump, {0.0f, -0.050f, -0.120f}, {0.0f, 0.45f, -0.89f}, {}, 0.060f,
     "Push the shell into the loading port under the gun", true, {-180.0f, 22.0f, 0.0f}, {0.892f, -0.451f, 0.0f},
     {-180.0f, 22.0f, 0.0f}, {}, {}, {}},
    {12, "submachine gun", kMagazine, kHandle, {0.0f, -0.085f, 0.010f}, {0.0f, 0.97f, -0.24f}, {0.0f, 0.120f, 0.080f},
     0.040f, "Push it up into the bottom of the grip", true, {44.7f, -19.3f, -13.4f}, {0.105f, 0.994f, 0.0f},
     {42.3f, -43.4f, -13.4f}, {6.0f, 56.0f, -7.0f}, {-1.0f, 0.0f, 0.0f}, {0.0f, -1.0f, 0.0f}},
};
int part_kind_of(const GunProfile& g) {
    switch (g.item) {
    case 9: return kPartBattery;
    case 10: return kPartHandgunMag;
    case 11: return kPartShell;
    case 12: return kPartSmgMag;
    }
    return kPartNone;
}

constexpr float kModelPerMetre = 490.0f;
double slide_kick_at = -10;    // the handgun's slide cycling back and forward on a shot
constexpr double kSlideKick = 0.09;
const char* part_name(Part p) { return p == kShell ? "shell" : p == kBattery ? "battery" : "magazine"; }

enum Phase { kIdle, kNeedPart, kPartHeld, kActionHeld };
const char* const kPhaseNames[] = {"ready", "waiting for a new part", "holding the new part", "pulling the slide"};

struct State {
    int phase = kIdle;
    int item = 0;
    bool old_in = false;       // kPartHeld: the old magazine still in (the new one taken first)
    bool chambered = false;    // a round stayed in the chamber when the magazine came out
    bool aligned = false;      // the part held is lined up with the well
    float aligned_along = 0;   // how far in it was when it lined up (m, along the way in)
    float action_start = 0;    // the off hand along the gun when it took the slide (m)
    float action_pulled = 0;   // how far back it is (m)
    bool pulled = false;
    bool pumping = false, pump_back = false;
    float pump_start = 0, pump_min = 0;
    double grip_down_at = -10;
    double logged_at = -10;
};
State st;
double now = 0;
// The part guided onto the well's line once lined up (RE4 VR's way: the magazine slides in straight, the hand with it),
// eased in and out; its last place kept while it eases out after it seats.
float snap_w = 0;
Pose snap_pose;
bool snap_valid = false;
float last_along = 0;          // the held part's top along the well's line (m, 0 at its mouth), last frame
// Seated: the part pushed the rest of the way home in the hand (0.09 s) before it is the gun's.
constexpr double kSeatTime = 0.09;
double seat_t0 = -10;
float seat_from = 0;
int seat_kind = -1;
bool seating() { return seat_kind >= 0 && now - seat_t0 < kSeatTime; }

Quat quat_nlerp(Quat a, Quat b, float t) {
    if (a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w < 0) b = Quat{-b.x, -b.y, -b.z, -b.w};
    Quat q{a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t, a.w + (b.w - a.w) * t};
    const float n = std::sqrt(q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w);
    return n > 1e-6f ? Quat{q.x / n, q.y / n, q.z / n, q.w / n} : a;
}
bool needs_action[16]{};       // by item: loaded from empty, the slide / handle / pump not yet worked
bool release_down = false;

// What the game is asked at the frame's end (sh3vr_actions) and what is still to show in its counts.
bool want_commit = false;
int want_rounds = 0, want_sound = -1;
int settle_frames = 0;         // the game's counts change a frame after an action: changes then are ours
int last_loaded = -1;
double last_sound_at = -10;

// The reload sounds (the motion event's index of the game's own reload of that item; -1 not yet learned).
int sound_of[16];
bool sounds_loaded = false;
void load_sounds() {
    sounds_loaded = true;
    for (int& s : sound_of) s = -1;
    FILE* f = std::fopen("sh3quest_reload.ini", "r");
    if (!f) return;
    int item, index;
    while (std::fscanf(f, " %d %d", &item, &index) == 2)
        if (item >= 0 && item < 16) sound_of[item] = index;
    std::fclose(f);
}
void save_sounds() {
    FILE* f = std::fopen("sh3quest_reload.ini", "w");
    if (!f) return;
    std::fprintf(f, "# SH3 Quest VR: each gun's reload sound (inventory item, the game's reload motion event)\n");
    for (int i = 0; i < 16; i++)
        if (sound_of[i] >= 0) std::fprintf(f, "%d %d\n", i, sound_of[i]);
    std::fclose(f);
}

// Hints (Halo's RELOAD HINTS): the step a player is at, on the screen, when it changes and again every 8 s while it
// waits, for the first three reloads of a run; each shown 4 s.
constexpr double kHintRepeat = 8.0, kHintShow = 4.0;
constexpr int kHintReloads = 3;
int reloads_done = 0;
std::string hint_wanted, hint_shown;
double hint_at = -100, no_ammo_at = -100, dry_at = -100;

const GunProfile* profile() {
    for (const GunProfile& g : kGuns)
        if (g.item == game.gun_item) return &g;
    return nullptr;
}

bool hands_ready() {
    return opt.manual_reload && weapon_shown_in_hand() && hands[main_hand()].valid && hands[off_hand()].valid;
}

float grip_of(int h) { return h == kLeft ? fi.controls.left_grip : fi.controls.right_grip; }
float prev_grip_of(int h) { return h == kLeft ? prev.left_grip : prev.right_grip; }

// The main hand's gun space.
Vec3 gun_point(Vec3 local) {
    const HandState& m = hands[main_hand()];
    return m.grip.position + rotate(m.aim.orientation, local);
}
Vec3 gun_dir(Vec3 local) { return rotate(hands[main_hand()].aim.orientation, normalized(local)); }
Vec3 to_gun(Vec3 p) {
    const HandState& m = hands[main_hand()];
    return rotate(conjugate(m.aim.orientation), p - m.grip.position);
}
// The top of the part held in the off hand's fist: the drawn part's own end (sh3vr_parts.cpp), else out of the fist's
// thumb and index side (the grip's -Z).
Vec3 part_tip(const GunProfile& g) {
    Vec3 tip, axis;
    if (parts_held_tip(off_hand(), tip, axis)) return tip;
    const HandState& o = hands[off_hand()];
    return o.grip.position + rotate(o.grip.orientation, Vec3{0, 0, g.part == kShell ? -0.03f : -0.045f});
}

// The gun's places in tracking: from its model through the weapon's frame (as drawn in the hand), else the aim space's.
bool model_placed(const GunProfile& g) {
    Vec3 t;
    return g.model && weapon_point(g.well_m, t);
}
Vec3 well_point(const GunProfile& g) {
    Vec3 t;
    return g.model && weapon_point(g.well_m, t) ? t : gun_point(g.well);
}
Vec3 well_dir(const GunProfile& g) {
    Vec3 t;
    return g.model && weapon_dir(g.insert_m, t) ? t : gun_dir(g.insert);
}
Vec3 grab_point(const GunProfile& g) {
    Vec3 t;
    return g.model && weapon_point(g.grab_m, t) ? t : gun_point(g.grab);
}
// How far forward along the gun a point is (m): along its barrel as drawn, else along the aim.
float along_gun(Vec3 p) {
    Vec3 base, fwd;
    if (weapon_forward(base, fwd)) return dot(p - base, fwd);
    return -to_gun(p).z;
}
Vec3 gun_front() {
    Vec3 base, fwd;
    return weapon_forward(base, fwd) ? fwd : gun_dir(Vec3{0, 0, -1});
}
Vec3 world_of(Vec3 tracking) { return map_ready() ? world_point(tracking) : Vec3{}; }

const char* item_name(int item) {
    for (const GunProfile& g : kGuns)
        if (g.item == item) return g.name;
    return "gun";
}

void reset(const char* why) {
    if (st.phase != kIdle)
        rt_log("VR: the reload by hand of the %s left (%s, it was %s): nothing moved", item_name(st.item), why,
               kPhaseNames[st.phase]);
    const int item = st.item;
    st = State{};
    st.item = item;
}

// The game's own reload sound for the gun (its motion event, learned from the game's reload); false when it is not
// known yet or was played under 0.35 s ago (shells pushed in fast): the hand's own sound then (sh3vr_sounds.cpp).
bool play_sound(const GunProfile& g) {
    const int s = g.item >= 0 && g.item < 16 ? sound_of[g.item] : -1;
    if (s < 0 || now - last_sound_at < 0.35) return false;
    want_sound = s;
    last_sound_at = now;
    return true;
}

void seated(const GunProfile& g) {
    const int mh = main_hand(), oh = off_hand();
    if (g.part == kShell) {
        if (game.loaded + want_rounds == 0 && !needs_action[g.item]) needs_action[g.item] = true;
        want_rounds++;
        settle_frames = 4;
        rt_log("VR: a shell pushed into the %s (%d loaded before it, %d in reserve)%s", g.name, game.loaded,
               game.reserve, needs_action[g.item] ? ": the pump next" : "");
    } else {
        want_commit = true;
        settle_frames = 4;
        if (g.action != kNoAction && !st.chambered) needs_action[g.item] = true;
        if (!needs_action[g.item]) reloads_done++;
        rt_log("VR: a %s seated in the %s (%d loaded, %d in reserve; the game's reload commits it)%s", part_name(g.part),
               g.name, game.loaded, game.reserve,
               !needs_action[g.item] ? "" : g.action == kHandle ? ": the cocking knob next" : ": the slide next");
    }
    // The game's reload sound where it is known (logged as the hand's, not played twice), else the hand's own.
    const bool by_game = play_sound(g);
    hand_sound(g.part == kShell ? kSoundShellIn : kSoundMagSeated, world_of(well_point(g)),
               by_game ? kSoundByGame + g.item : g.item);
    // Lined up when it seated: shown pushed the rest of the way home.
    if (st.aligned) {
        seat_kind = part_kind_of(g);
        seat_t0 = now;
        seat_from = std::min(last_along, 0.0f);
    }
    buzz(oh, 0.55f, 0.05f);
    buzz(mh, 0.45f, 0.04f);
    st.phase = kIdle;
    st.aligned = false;
}

void worked(const GunProfile& g) {
    needs_action[g.item] = false;
    reloads_done++;
    buzz(off_hand(), 0.6f, 0.045f);
    buzz(main_hand(), 0.35f, 0.035f);
    rt_log("VR: the %s's %s worked: it fires", g.name, g.action == kPump ? "pump" : g.action == kSlide ? "slide"
                                                                                                    : "cocking knob");
}

// The part held: lined up with the well, then pushed home (or let go lined up and nearly in).
void update_part(const GunProfile& g, bool down, bool up) {
    (void)down;
    const int oh = off_hand();
    const Vec3 tip = part_tip(g);
    const Vec3 well = well_point(g), dir = well_dir(g);
    const Vec3 r = tip - well;
    const float along = dot(r, dir);
    const float lateral = length(r - dir * along);
    last_along = along;
    const bool can_seat = !st.old_in;
    const bool aligned = can_seat && lateral < 0.045f && along > -0.08f && along < 0.04f;
    if (aligned && !st.aligned) {
        st.aligned_along = along;
        buzz(oh, 0.2f, 0.02f, 200.0f);
    }
    st.aligned = aligned;
    static float logged_along = 1.0e9f;
    static int detail_lines = 0;
    if (now - st.logged_at > 1.0 || std::fabs(along - logged_along) > 0.01f) {
        st.logged_at = now;
        logged_along = along;
        rt_log("testing: the %s held %.3f m along the %s's well, %.3f m off its line%s", part_name(g.part), along,
               g.name, lateral, st.old_in ? " (the old one still in)" : aligned ? " (lined up)" : "");
        // Session 16 (vr237, owner 75: the shotgun's, submachine gun's and stun gun's parts let go off their wells):
        // where the well and the part's top are, to tell a stale rig script from a well that misses the drawn gun.
        if (detail_lines < 60) {
            detail_lines++;
            const Vec3 d = well - tip;
            const HandState& o = hands[oh];
            Vec3 wm{}, tm{};
            const bool in_model = weapon_model_point(well, wm) && weapon_model_point(tip, tm);
            rt_log("testing: the %s's well minus the %s's top (%.3f %.3f %.3f) m, the way in (%.3f %.3f %.3f); the off "
                   "hand shown %.3f m from its controller%s; in the gun's model (x490): the well (%.1f %.1f %.1f), the "
                   "top (%.1f %.1f %.1f)%s; the well from the %s",
                   g.name, part_name(g.part), d.x, d.y, d.z, dir.x, dir.y, dir.z,
                   length(o.shown.position - o.grip.position), o.inside ? " (inside a surface)" : "", wm.x, wm.y, wm.z,
                   tm.x, tm.y, tm.z, in_model ? "" : " (not placed)", model_placed(g) ? "model" : "aim offset");
        }
    }
    if (aligned && along >= -0.015f && along - st.aligned_along >= 0.012f) {
        seated(g);
        return;
    }
    if (up) {
        if (aligned && along >= -0.03f) {
            seated(g);
            return;
        }
        // Let go away from the well: the new one dropped (it was never taken from the reserve): it falls from the hand.
        rt_log("VR: the new %s let go away from the %s (%.3f m along, %.3f off): dropped", part_name(g.part), g.name,
               along, lateral);
        Vec3 bottom, top, front;
        if (parts_held_ends(oh, bottom, top, front))
            parts_drop(part_kind_of(g), bottom, normalized(top - bottom), front, hands[oh].vel);
        buzz(oh, 0.15f, 0.03f);
        st.phase = st.old_in ? kIdle : (g.part == kShell ? kIdle : kNeedPart);
        st.old_in = false;
        st.aligned = false;
    }
}

// The slide or the cocking handle: taken near it, pulled back its travel along the gun, let go.
void update_action(const GunProfile& g, bool down, bool up) {
    const int oh = off_hand();
    const HandState& o = hands[oh];
    if (g.action == kPump) {
        // The pump: the off hand on the foregrip (sh3vr_combat.cpp's two-hand grip), pulled back towards the main hand
        // and pushed forward again.
        if (!o.on_foregrip || grip_of(oh) < 0.5f) {
            st.pumping = st.pump_back = false;
            return;
        }
        const float d = length(o.grip.position - hands[main_hand()].grip.position);
        if (!st.pumping) {
            st.pumping = true;
            st.pump_back = false;
            st.pump_start = st.pump_min = d;
        }
        st.pump_min = std::min(st.pump_min, d);
        st.action_pulled = std::clamp(st.pump_start - d, 0.0f, g.travel);
        if (!st.pump_back && st.pump_start - d >= g.travel) {
            st.pump_back = true;
            buzz(oh, 0.4f, 0.03f);
            hand_sound(kSoundPumpBack, world_of(hands[oh].shown.position), g.item);
        }
        if (st.pump_back && d - st.pump_min >= 0.04f) {
            st.pumping = st.pump_back = false;
            st.action_pulled = 0;
            hand_sound(kSoundPumpForward, world_of(hands[oh].shown.position), g.item);
            worked(g);
        }
        return;
    }
    const Vec3 p = o.grip.position;
    const float fwd = along_gun(p);
    if (st.phase != kActionHeld) {
        const bool near = length(p - grab_point(g)) < 0.08f;
        // A grip closed just before the hand reached it still takes it (Halo's grace, 0.35 s).
        const bool grace = grip_of(oh) > 0.6f && now - st.grip_down_at < 0.35;
        if (near && (down || grace)) {
            st.phase = kActionHeld;
            st.action_start = fwd;
            st.action_pulled = 0;
            st.pulled = false;
            buzz(oh, 0.3f, 0.03f);
            rt_log("VR: the %s's %s taken", g.name, g.action == kSlide ? "slide" : "cocking knob");
        }
        return;
    }
    st.action_pulled = std::clamp(st.action_start - fwd, 0.0f, g.travel + 0.02f);
    if (!st.pulled && st.action_start - fwd >= g.travel) {
        st.pulled = true;
        buzz(oh, 0.45f, 0.03f);
        hand_sound(kSoundSlideBack, world_of(grab_point(g)), g.item);
    }
    if (up) {
        st.phase = kIdle;
        // Let go: it runs forward on its spring (a sound whether it went its whole way or not).
        if (st.action_pulled > 0.005f) hand_sound(kSoundSlideForward, world_of(grab_point(g)), g.item);
        st.action_pulled = 0;
        if (st.pulled) worked(g);
        else rt_log("VR: the %s's slide let go before its end (%.3f m of %.3f)", g.name, st.action_start - fwd, g.travel);
        st.pulled = false;
    }
}

void update_hint(const GunProfile* g) {
    hint_wanted.clear();
    if (!opt.reload_hints || !g || !hands_ready() || reloads_done >= kHintReloads) return;
    const bool right = !opt.left_handed;
    const char* side = right ? "left" : "right";
    char text[160] = "";
    if (now - no_ammo_at < 2.0) std::snprintf(text, sizeof text, "No spare %s", g->part == kShell ? "shells" : "ammo");
    else if (st.phase == kNeedPart)
        std::snprintf(text, sizeof text, "Take a new %s from the front of your belt, %s side (squeeze the grip)",
                      part_name(g->part), side);
    else if (st.phase == kPartHeld)
        std::snprintf(text, sizeof text, "%s", st.old_in ? "Push the right stick down to drop the old one first"
                                                         : g->well_words);
    else if (needs_action[g->item] || st.phase == kActionHeld)
        std::snprintf(text, sizeof text, "%s",
                      g->action == kPump ? "Hold the pump with the grip, pull it back and push it forward"
                      : g->action == kSlide ? "Grip the top of the slide and pull it all the way back"
                                            : "Grip the cocking knob (on top, at the back) and pull it all the way back");
    else if (game.loaded <= 0 && game.reserve > 0) {
        if (g->part == kShell)
            std::snprintf(text, sizeof text, "Take a shell from the front of your belt, %s side (squeeze the grip)",
                          side);
        else std::snprintf(text, sizeof text, "Push the right stick down to drop the %s", part_name(g->part));
    }
    hint_wanted = text;
    if (hint_wanted.empty()) return;
    // Due when it changes, every 8 s while it waits, and at once when the trigger clicks on a gun that cannot fire.
    if (hint_wanted != hint_shown || now - hint_at >= kHintRepeat || (dry_at > hint_at && now - hint_at > kHintShow)) {
        hint_shown = hint_wanted;
        hint_at = now;
        rt_log("VR: reload hint: %s", hint_shown.c_str());
    }
}

// The hint's picture: a label drawn on the panel (sh3vr.cpp; 0.1.6 shown on its own in view), top row first like the panel's.
constexpr int kHintW = 1024, kHintH = 64;
std::vector<uint32_t> hint_canvas;
GLuint hint_tex = 0;
std::string hint_drawn;

}  // namespace

// The PC rig (xr_sim's "lh_at_reload N", session 16: reload.txt's fixed places had gone stale as the guns' holds
// changed, vr237): where the off controller goes for the reload by hand, from this frame's places: 1 the part's top 5
// cm short of its well on the well's line, 2 its top 0.5 cm into the well (home), 3 the off fist at the slide's or the
// cocking knob's grab point. The part moves with the controller (the same turn), so its top's miss is the move.
bool sim_reload_spot(int what, float out[3]) {
    const GunProfile* g = profile();
    if (!g || !hands_ready()) return false;
    const HandState& o = hands[off_hand()];
    Vec3 s;
    if (what == 1 || what == 2)
        s = o.grip.position + (well_point(*g) + well_dir(*g) * (what == 1 ? -0.05f : 0.005f) - part_tip(*g));
    else if (what == 3 && g->action != kNoAction && g->action != kPump)
        s = grab_point(*g);
    else
        return false;
    out[0] = s.x, out[1] = s.y, out[2] = s.z;
    return true;
}

// ================================================================ shared (sh3vr_internal.h)

void reload_frame() {
    now += frame_dt();
    if (!sounds_loaded) load_sounds();
    const GunProfile* g = profile();
    // A weapon change, or the gun no longer in the hands' care: whatever was under way is left (nothing moved).
    if (game.gun_item != st.item) {
        reset("another weapon");
        st.item = game.gun_item;
        last_loaded = game.loaded;
    }
    if (!opt.manual_reload || view_mode != ViewMode::kFirstPerson || !weapon_shown_in_hand()) {
        if (st.phase != kIdle) reset(!opt.manual_reload ? "Manual reload off" : "not in the hand in first person");
        last_loaded = game.loaded;
        update_hint(nullptr);
        return;
    }
    // The game's counts: a rise not of the hands' making (the inventory's reload) ends a reload under way.
    if (settle_frames > 0) settle_frames--;
    else if (g && last_loaded >= 0 && game.loaded > last_loaded) {
        if (st.phase != kIdle || needs_action[g->item]) rt_log("VR: the game loaded the %s itself", g->name);
        reset("the game loaded it");
        needs_action[g->item] = false;
    }
    last_loaded = game.loaded;
    if (!g || !in_play() || !hands_ready()) {
        update_hint(g);
        return;
    }
    const int mh = main_hand(), oh = off_hand();
    const float og = grip_of(oh), opg = prev_grip_of(oh);
    const bool down = og > 0.6f && opg <= 0.6f, up = og < 0.4f && opg >= 0.4f;
    if (down) st.grip_down_at = now;

    // The magazine release: the right stick pushed down (its other way is free in first person).
    const Controls& c = fi.controls;
    const bool rel = c.right_y < -0.7f && std::fabs(c.right_x) < 0.5f;
    const bool rel_press = rel && !release_down;
    if (rel) release_down = true;
    else if (c.right_y > -0.4f) release_down = false;
    if (rel_press && g->part != kShell) {
        if (st.phase == kIdle || (st.phase == kPartHeld && st.old_in)) {
            if (game.reserve <= 0 && st.phase == kIdle) {
                // A magazine release with nothing to load keeps the magazine in (Halo's rule).
                buzz(mh, 0.12f, 0.02f);
                no_ammo_at = now;
                rt_log("VR: the %s's release with no spare ammo: the %s stays in", g->name, part_name(g->part));
            } else {
                // A round in the chamber: rounds loaded, and not a magazine seated in an empty gun still to be racked.
                st.chambered = game.loaded > 0 && !needs_action[g->item];
                if (st.phase == kIdle) st.phase = kNeedPart;
                st.old_in = false;
                buzz(mh, 0.5f, 0.05f);
                // It falls out of the well: from where it sat in the drawn gun, down the well's line and away with
                // the gun's own movement.
                Vec3 bottom;
                if (!(g->model && weapon_point(g->seat_bottom_m, bottom))) bottom = gun_point(g->well);
                const Vec3 axis = well_dir(*g);
                parts_drop(part_kind_of(*g), bottom, axis, gun_front(), hands[mh].vel - axis * 0.6f);
                hand_sound(kSoundMagRelease, world_of(well_point(*g)), g->item);
                rt_log("VR: the %s's %s dropped (%d loaded%s, %d in reserve)", g->name, part_name(g->part), game.loaded,
                       g->action != kNoAction && st.chambered ? ", one in the chamber" : "", game.reserve);
            }
        }
    }

    if (st.phase == kPartHeld) update_part(*g, down, up);
    else if (st.phase == kActionHeld || (st.phase == kIdle && needs_action[g->item])) update_action(*g, down, up);

    // The trigger on a gun that cannot fire: a light click in the hand.
    const float mt = mh == kLeft ? c.left_trigger : c.right_trigger;
    const float pmt = mh == kLeft ? prev.left_trigger : prev.right_trigger;
    if (reload_blocks_fire() && mt > 0.6f && pmt <= 0.6f) {
        buzz(mh, 0.15f, 0.02f, 220.0f);
        hand_sound(kSoundDryClick, world_of(hands[mh].shown.position), g->item);
        dry_at = now;
    }
    update_hint(g);
}

bool reload_pouch_on() {
    const GunProfile* g = profile();
    return g && hands_ready() && st.phase != kPartHeld && st.phase != kActionHeld && !seating();
}

bool reload_take_from_pouch(int hand) {
    const GunProfile* g = profile();
    if (!g || hand != off_hand() || !reload_pouch_on()) return false;
    const int in_flight = g->part == kShell ? want_rounds : 0;
    if (game.reserve - in_flight <= 0) {
        buzz(hand, 0.12f, 0.02f);
        no_ammo_at = now;
        rt_log("VR: the ammo pouch: no spare %s for the %s", g->part == kShell ? "shells" : "ammo", g->name);
        return true;
    }
    if (g->part == kShell) {
        if (game.loaded + in_flight >= game.capacity) {
            buzz(hand, 0.12f, 0.02f);
            rt_log("VR: the ammo pouch: the shotgun is full (%d)", game.capacity);
            return true;
        }
    } else if (st.phase == kIdle) {
        // Taken with the old one still in: it waits for the release.
        st.old_in = true;
    } else if (st.phase != kNeedPart) return false;
    st.phase = kPartHeld;
    st.aligned = false;
    st.logged_at = -10;
    buzz(hand, 0.35f, 0.04f);
    hand_sound(kSoundPartTaken, world_of(hands[hand].shown.position), part_kind_of(*g));
    rt_log("VR: a %s taken from the ammo pouch for the %s%s", part_name(g->part), g->name,
           st.old_in ? " (the old one still in)" : "");
    return true;
}

bool reload_owns_hand(int hand) {
    return hand == off_hand() && (st.phase == kPartHeld || st.phase == kActionHeld || seating()) && hands_ready();
}

bool reload_blocks_fire() {
    const GunProfile* g = profile();
    if (!g || !hands_ready()) return false;
    if (needs_action[g->item] || st.phase == kActionHeld) return true;
    // An empty battery: the stun gun does nothing (the game's own empty handling leaves it out, 0x4B4340).
    if (g->part == kBattery && game.loaded <= 0) return true;
    if (g->part == kShell) return false;  // a shell held does not stop the shotgun firing
    return st.phase == kNeedPart || (st.phase == kPartHeld && !st.old_in);
}

bool reload_blocks_game() {
    const GunProfile* g = profile();
    return g && g->item != 9 && hands_ready() && in_play();
}

void reload_game_event(int weapon, int event_index) {
    if (!sounds_loaded) load_sounds();
    int item = 0;
    switch (weapon) {
    case 1:
    case 15: item = 10; break;
    case 2: item = 11; break;
    case 3:
    case 16: item = 12; break;
    case 8: item = 9; break;
    }
    if (item && sound_of[item] != event_index) {
        sound_of[item] = event_index;
        save_sounds();
        rt_log("VR: item %d's reload sound learned: motion event %d", item, event_index);
    }
    if (item) needs_action[item] = false;
    if (item == st.item) reset("the game reloaded it");
}

bool reload_fingers(int hand, float out[5]) {
    const GunProfile* g = profile();
    if (!g || hand != off_hand() || !hands_ready()) return false;
    if (st.phase == kPartHeld || seating()) {
        if (g->part == kShell) {
            // A shell pinched between the thumb and the first two fingers, the others tucked.
            out[kThumb] = 0.62f, out[kIndex] = 0.62f, out[kMiddle] = 0.55f, out[kRing] = 0.9f, out[kPinky] = 0.9f;
        } else {
            // A magazine (a battery) in the fist, the index finger along its front.
            out[kThumb] = 0.55f, out[kIndex] = 0.4f, out[kMiddle] = 0.78f, out[kRing] = 0.78f, out[kPinky] = 0.8f;
        }
        return true;
    }
    if (st.phase == kActionHeld) {
        if (g->action == kSlide) {
            // The slide's rear held over the top, thumb on one side and the fingers on the other.
            out[kThumb] = 0.6f, out[kIndex] = 0.72f, out[kMiddle] = 0.78f, out[kRing] = 0.82f, out[kPinky] = 0.86f;
        } else {
            // The cocking knob pinched between the thumb and the first fingers.
            out[kThumb] = 0.62f, out[kIndex] = 0.7f, out[kMiddle] = 0.78f, out[kRing] = 0.88f, out[kPinky] = 0.9f;
        }
        return true;
    }
    return false;
}

void reload_actions(Sh3VrActions* out) {
    if (want_commit) out->reload_commit = 1;
    if (want_rounds > 0) out->load_rounds = want_rounds;
    if (want_sound >= 0) out->sound_event = want_sound;
    want_commit = false;
    want_rounds = 0;
    want_sound = -1;
}

unsigned reload_hint_texture(int& w, int& h, int& box_w) {
    static int drawn_box_w = kHintW;
    w = kHintW;
    h = kHintH;
    box_w = drawn_box_w;
    if (hint_shown.empty() || hint_wanted.empty() || now - hint_at > kHintShow) return 0;
    if (!hint_tex) {
        glGenTextures(1, &hint_tex);
        GLint prev_tex = 0;
        glGetIntegerv(GL_TEXTURE_BINDING_2D, &prev_tex);
        glBindTexture(GL_TEXTURE_2D, hint_tex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, kHintW, kHintH, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glBindTexture(GL_TEXTURE_2D, (GLuint)prev_tex);
        hint_drawn.clear();
    }
    if (hint_drawn != hint_shown) {
        hint_drawn = hint_shown;
        auto rgba = [](int r, int gg, int b, int a) {
            return (uint32_t)a << 24 | (uint32_t)b << 16 | (uint32_t)gg << 8 | (uint32_t)r;
        };
        // 0.1.6 (the owner's 0.1.5 video): the backing only round the text (the rest clear), so the hint stands as a
        // small label of its own in view (sh3vr.cpp ui_place_hint), cropped to box_w, not a strip the panel's width.
        const int size = 26;
        const int tw = (int)hint_drawn.size() * (int)(36 * size / 80.0f + 0.5f);
        const int bw = std::min(kHintW, tw + 64), bx = (kHintW - bw) / 2;
        drawn_box_w = box_w = bw;
        hint_canvas.assign((size_t)kHintW * kHintH, 0u);
        const uint32_t back = rgba(10, 8, 7, 210), rust = rgba(128, 38, 24, 255);
        for (int y = 0; y < kHintH; y++)
            for (int x = bx; x < bx + bw; x++)
                hint_canvas[(size_t)y * kHintW + x] = y == 0 || y == kHintH - 1 || x == bx || x == bx + bw - 1 ? rust : back;
        ui_text(hint_canvas.data(), kHintW, kHintH, std::max(8, (kHintW - tw) / 2), 42, size, rgba(236, 228, 212, 255),
                hint_drawn.c_str());
        GLint prev_tex = 0, unpack = 0, row_len = 0, align = 4;
        glGetIntegerv(GL_TEXTURE_BINDING_2D, &prev_tex);
        glGetIntegerv(GL_PIXEL_UNPACK_BUFFER_BINDING, &unpack);
        glGetIntegerv(GL_UNPACK_ROW_LENGTH, &row_len);
        glGetIntegerv(GL_UNPACK_ALIGNMENT, &align);
        glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
        glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
        glBindTexture(GL_TEXTURE_2D, hint_tex);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, kHintW, kHintH, GL_RGBA, GL_UNSIGNED_BYTE, hint_canvas.data());
        glBindTexture(GL_TEXTURE_2D, (GLuint)prev_tex);
        glBindBuffer(GL_PIXEL_UNPACK_BUFFER, (GLuint)unpack);
        glPixelStorei(GL_UNPACK_ROW_LENGTH, row_len);
        glPixelStorei(GL_UNPACK_ALIGNMENT, align);
    }
    return hint_tex;
}

void reload_shot() {
    if (game.gun_item != 10 || !hands_ready()) return;
    slide_kick_at = now;
    static int logged;
    if (logged++ < 4) rt_log("testing: the handgun's slide cycles on a shot (%d loaded)", game.loaded);
}

// The cocking visual (the owner, 2026-11-24: "add some form of visual to indicate the gun being cocked"): how far the
// gun's slide, knob or pump is drawn back (sh3vr_parts.cpp moves that part's own faces by it).
float reload_part_back(int item) {
    const GunProfile* g = profile();
    if (!g || g->item != item || view_mode != ViewMode::kFirstPerson || !hands_ready()) return 0.0f;
    float back = 0;
    if (g->action == kSlide) {
        // The slide stays back on an empty handgun and on one loaded from empty and not yet racked; the hand pulls it;
        // a shot sends it back and forward again.
        if (game.loaded <= 0 || needs_action[g->item]) back = g->travel;
        if (st.phase == kActionHeld) back = std::max(back, st.action_pulled);
        const double t = now - slide_kick_at;
        if (t >= 0 && t < kSlideKick) back = std::max(back, g->travel * (float)(1.0 - std::fabs(2.0 * t / kSlideKick - 1.0)));
    } else if (g->action == kHandle) {
        if (st.phase == kActionHeld) back = st.action_pulled;
    } else if (g->action == kPump && st.pumping) {
        back = st.action_pulled;
    }
    return std::clamp(back, 0.0f, g->travel);
}

float reload_pump_back() {
    const GunProfile* g = profile();
    if (!g || g->action != kPump || !st.pumping || !hands_ready()) return 0.0f;
    return std::clamp(st.action_pulled, 0.0f, g->travel);
}

bool reload_magazine_out(int item) {
    const GunProfile* g = profile();
    if (!g || g->item != item || g->part == kShell || !hands_ready()) return false;
    return st.phase == kNeedPart || (st.phase == kPartHeld && !st.old_in) || seating();
}

int reload_held_part(int hand) {
    const GunProfile* g = profile();
    if (!g || hand != off_hand() || !hands_ready()) return kPartNone;
    if (seating()) return seat_kind;
    return st.phase == kPartHeld ? part_kind_of(*g) : kPartNone;
}

// The other hand shown where the reload holds it: on the slide or the knob as it moves (its fist round the part, the
// palm over it), or with the part it holds guided onto the well's line once lined up and pushed home as it seats.
bool reload_hand_spot(int hand, Pose& shown) {
    const GunProfile* g = profile();
    if (!g || hand != off_hand() || !hands_ready()) {
        snap_w = 0;
        snap_valid = false;
        return false;
    }
    if (st.phase == kActionHeld && g->model && g->action != kPump) {
        // The handgun's and the submachine gun's barrels lie along their model's +x: the slide goes back along -x.
        const float pulled = std::min(st.action_pulled, g->travel);
        Vec3 at, rod, palm;
        if (weapon_point(g->grab_m + Vec3{-1.0f, 0.0f, 0.0f} * (pulled * kModelPerMetre), at) &&
            weapon_dir(g->grab_rod_m, rod) && weapon_dir(g->grab_palm_m, palm)) {
            shown = hand_pose_round(hand, at, rod, palm);
            snap_w = 0;
            snap_valid = false;
            return true;
        }
    }
    const int kind = reload_held_part(hand);
    float target = 0;
    Vec3 c;
    Quat q;
    if (kind >= 0 && (seating() || (st.phase == kPartHeld && st.aligned)) && parts_in_hand(hand, kind, c, q)) {
        const Vec3 y = well_dir(*g), well = well_point(*g);
        Vec3 x = gun_front();
        x = x - y * dot(x, y);
        if (length(x) < 1e-3f) x = cross(y, Vec3{1, 0, 0});
        x = normalized(x);
        const Quat part_q = quat_from_axes(x, y, cross(x, y));
        float along;
        if (seating()) {
            // Pushed home: from where it was to where it sits, its whole length in (the submachine gun's bottom
            // below the grip, the others' flush with it).
            Vec3 bottom = well;
            if (g->model) weapon_point(g->seat_bottom_m, bottom);
            const float to = dot(bottom + y * parts_length(kind) - well, y);
            const float t = std::clamp((float)((now - seat_t0) / kSeatTime), 0.0f, 1.0f);
            along = seat_from + (to - seat_from) * (t * t * (3.0f - 2.0f * t));
        } else along = std::min(last_along, 0.0f);  // up to the well's mouth while it is still being lined up
        const Vec3 tip = well + y * along;
        const Vec3 origin = tip - y * (parts_top(kind) / kModelPerMetre);
        const Quat hq = part_q * conjugate(q);
        snap_pose = {origin - rotate(hq, c), hq};
        snap_valid = true;
        target = 1;
    }
    snap_w += (target - snap_w) * (1.0f - std::exp(-(target > snap_w ? 18.0f : 10.0f) * frame_dt()));
    if (!snap_valid || snap_w < 0.01f) {
        if (target == 0) snap_valid = false;
        return false;
    }
    const Pose& from = hands[hand].shown;
    shown.position = lerp(from.position, snap_pose.position, snap_w);
    shown.orientation = quat_nlerp(from.orientation, snap_pose.orientation, snap_w);
    return true;
}

void reload_crash_info(FILE* f) {
    std::fprintf(f, "VR reload: option %d, item %d (%d loaded, %d reserve, %d capacity), phase %s, old in %d, "
                    "chambered %d, aligned %d (%.3f m along), guided %.2f, seating %d, slide %.3f, needs the "
                    "slide/pump:",
                 opt.manual_reload, game.gun_item, game.loaded, game.reserve, game.capacity,
                 st.phase >= 0 && st.phase < 4 ? kPhaseNames[st.phase] : "?", st.old_in, st.chambered, st.aligned,
                 last_along, snap_w, seating(), st.action_pulled);
    for (int i = 9; i <= 12; i++) std::fprintf(f, " %d=%d", i, needs_action[i]);
    std::fprintf(f, ", sounds:");
    for (int i = 9; i <= 12; i++) std::fprintf(f, " %d=%d", i, sound_of[i]);
    std::fprintf(f, ", reloads done %d, hint \"%s\"\n", reloads_done, hint_shown.c_str());
}

}  // namespace sh3q
