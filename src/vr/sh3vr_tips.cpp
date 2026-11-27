// SH3 Quest VR 0.1.7: hints (owner message 143: "add some hints similar to the reload ones that pop up and fade away
// when certain events happen for the first time. like when being grabbed, when approaching a door, etc"; 144: "be sure
// the hints are all under one toggle, and only happen once. add a menu section in the in game vr menu with all the hints
// so people can look back at them").
//
// Each hint is a short line on the reload hint's own label (sh3vr_reload.cpp reload_hint_texture draws it, sh3vr.cpp
// ui_place_hint stands it level in view, a little over where the eyes look; it fades in and out with the UI's fades).
// A reload step, when one is due, comes first; a game message on the panel or a screen holds a hint back, and a hint
// held back past its moment is dropped (unshown: it shows the next time). Each hint shows once, ever: what has been shown
// is kept in the game folder's sh3quest_tips.ini. The reload's steps (sh3vr_reload.cpp) show through the first reload
// of each gun, kept there too. One switch for them all: "Hints" (VR menu, Hints page, and the launcher's View page); the
// Hints page lists every hint to read again, and "Show every hint again" clears what has been shown.
//
// What they are about, and what starts each:
// - the start of play in first person: walking, turning and running; the VR menu and recentring (two lines);
// - the flashlight first lit in play: how to switch it;
// - something the action button would use within reach (sh3.c's per-frame action check asks every 8 frames while the
//   hint is still due and the label is free): a door, a switch or a thing to look at; or an item to take;
// - a monster's grab or a knock-down (the game's struggle step, sh3vr_combat.cpp): shake to break free; it goes before
//   any other line, and ends when she is free;
// - the first gun and the first melee weapon in the hand: fire or swing, the second hand, the holster;
// - health under half in first person: her watch, and the supplies.
#include "sh3vr_internal.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace sh3q {

namespace {

// Each hint's name in sh3quest_tips.ini: 1 once it has been shown. The reload's: one per gun (its item).
const char* const kTipKeys[kTipCount] = {"start", "flashlight", "use", "item", "held", "gun", "melee", "health"};
constexpr int kReloadGuns = 4;
const int kReloadItems[kReloadGuns] = {9, 10, 11, 12};
const char* const kReloadKeys[kReloadGuns] = {"reload_stun_gun", "reload_handgun", "reload_shotgun",
                                              "reload_submachine_gun"};

int shown[kTipCount] = {};
int reload_shown[kReloadGuns] = {};
bool counts_loaded = false;

// The lines waiting (one hint's lines together, in order), and the one up now.
struct Page {
    int kind = -1;
    bool first = false;    // a hint's first line: the hint counts as shown once it has been up
    bool counted = false;  // ... and it has
    std::string text;
    double drop_at = 0;    // dropped (unshown, or cut short) when held back past this: a door left behind, a grab over
    int weapon = 0;        // a weapon's line: the weapon it is about (it goes when another is in the hand)
};
constexpr int kQueue = 8;
Page queue[kQueue];
int queued = 0;
Page current;
float current_up = 0;    // seconds the current line has been on the label in full
double gap_until = 0;    // a short gap between lines, so each fades out and the next fades in
double now = 0;          // seconds of VR frames
int asked_frame = -100;  // the label last asked for the line (tip_text)
float play_seconds = 0;  // play in first person since the view last left it
bool light_seen = false;
int weapon_seen = 0;     // the weapon in the hand now, as last looked at (0 none)
float weapon_held = 0;
float weapon_gone = 0;   // seconds with no weapon in the hand
int dropped_told = 0;

int reload_index(int item) {
    for (int i = 0; i < kReloadGuns; i++)
        if (kReloadItems[i] == item) return i;
    return -1;
}

void load_counts() {
    counts_loaded = true;
    FILE* f = std::fopen("sh3quest_tips.ini", "r");
    if (!f) {
        rt_log("VR: hints: no sh3quest_tips.ini yet: every hint still to show");
        return;
    }
    char line[128];
    while (std::fgets(line, sizeof line, f)) {
        char key[64];
        int n = 0;
        if (std::sscanf(line, " %63[^= ] = %d", key, &n) != 2) continue;
        for (int k = 0; k < kTipCount; k++)
            if (!std::strcmp(key, kTipKeys[k])) shown[k] = n > 0;
        for (int i = 0; i < kReloadGuns; i++)
            if (!std::strcmp(key, kReloadKeys[i])) reload_shown[i] = n > 0;
    }
    std::fclose(f);
    std::string seen;
    for (int k = 0; k < kTipCount; k++)
        if (shown[k]) seen += std::string(seen.empty() ? "" : ", ") + kTipKeys[k];
    for (int i = 0; i < kReloadGuns; i++)
        if (reload_shown[i]) seen += std::string(seen.empty() ? "" : ", ") + kReloadKeys[i];
    rt_log("VR: hints: sh3quest_tips.ini read; shown before: %s", seen.empty() ? "none" : seen.c_str());
}

void save_counts() {
    FILE* f = std::fopen("sh3quest_tips.ini", "w");
    if (!f) {
        rt_log("VR: hints: sh3quest_tips.ini could not be written (what was shown is kept for this run only)");
        return;
    }
    std::fprintf(f, "# SH3 Quest VR: the hints already shown (1), each shown once. The VR menu's Hints page lists them\n"
                    "# all; its \"Show every hint again\" (or deleting this file) shows them again.\n");
    for (int k = 0; k < kTipCount; k++) std::fprintf(f, "%s = %d\n", kTipKeys[k], shown[k]);
    for (int i = 0; i < kReloadGuns; i++) std::fprintf(f, "%s = %d\n", kReloadKeys[i], reload_shown[i]);
    std::fclose(f);
}

// A hint still to show, and not already up or waiting.
bool due(int kind) {
    if (!counts_loaded) load_counts();
    if (kind < 0 || kind >= kTipCount || shown[kind]) return false;
    if (current.kind == kind) return false;
    for (int i = 0; i < queued; i++)
        if (queue[i].kind == kind) return false;
    return true;
}

bool label_free() { return current.kind < 0 && queued == 0 && now >= gap_until; }

// Every hint asks for the headset's own play: VR on (not Flat's big screen), in play, the VR menu shut.
bool can_hint() { return opt.vr && opt.hints && active && in_play() && game.player_valid; }

void add(int kind, bool first, const std::string& text, double keep_seconds, bool to_front = false, int weapon = 0) {
    if (queued >= kQueue || text.empty()) return;
    Page p;
    p.kind = kind;
    p.first = first;
    p.text = text;
    p.drop_at = now + keep_seconds;
    p.weapon = weapon;
    if (to_front) {
        for (int i = queued; i > 0; i--) queue[i] = queue[i - 1];
        queue[0] = p;
    } else {
        queue[queued] = p;
    }
    queued++;
}

// The hands' sides for this player (left-handed: they swap).
std::string main_side() { return opt.left_handed ? "left" : "right"; }
std::string off_side() { return opt.left_handed ? "right" : "left"; }

// The weapon byte 0x712C656 (sh3vr_combat.cpp profile_of): 1/15 handgun, 2 shotgun, 3/16-18 submachine gun, 8 stun
// gun, 10 flamethrower; 4 knife, 5/13/14 the pipes, 6 katana, 7 maul, 9 beam saber.
bool is_gun(int w) { return w == 1 || w == 2 || w == 3 || w == 8 || w == 10 || (w >= 15 && w <= 18); }
bool is_melee(int w) { return (w >= 4 && w <= 7) || w == 9 || w == 13 || w == 14; }
bool takes_second_hand(int w) {  // kWeapons' foregrip: the long guns and the long melee weapons
    return w == 2 || w == 3 || (w >= 16 && w <= 18) || w == 5 || w == 6 || w == 7 || w == 9 || w == 10 || w == 13 ||
           w == 14;
}

// Every line, as shown in play and as listed on the VR menu's Hints page (the options as they are now).
std::string start_walk() {
    return opt.move_by == 1 ? "Left stick: walk where your left hand points. Right stick: turn. Click it: run"
                            : "Left stick: walk where you look. Right stick: turn. Left stick click: run";
}
const char* const kStartMenu = "Hold the left menu button: VR menu. Click both sticks: recentre the view";
std::string light_line() {
    return "Flashlight: the " + off_side() + " trigger" + (opt.chest_light_touch ? ", or grip at your chest pocket" : "");
}
std::string use_line() {
    return opt.take_by_hand ? "Something to use here: press A, or reach out to it and grip" : "Something to use here: press A";
}
std::string item_line() {
    return opt.take_by_hand ? "An item here: press A to take it, or reach out to it and grip" : "An item here: press A to take it";
}
std::string held_line() {
    return opt.shake_free ? "Shake both controllers hard to break free (or press buttons fast)"
                          : "Press buttons fast to break free";
}
std::string gun_fire() {
    return std::string("Pull the trigger to fire. The gun aims where it points") + (opt.laser ? " (the red dot)" : "");
}
const char* const kHandgunSteady = "Grip under it with your other hand to steady it";
const char* const kGunBothHands = "Grip the front with your other hand to hold it in both hands";
const char* const kMeleeSwing = "Swing it to attack: a faster swing hits harder";
const char* const kMeleeBothHands = "Grip it with your other hand too, to swing it in both hands";
std::string holster_line(bool gun) {
    const bool at_hip = gun == opt.gun_on_hip;  // on: the gun at the hip, the melee weapon over the shoulder
    return "Put it away: hold it at your " + main_side() + (at_hip ? " hip" : " shoulder") +
           " and let go. Grip there to draw it";
}
const char* const kHealthWatch = "Health low: turn your left wrist toward you to see it on her watch";
std::string heal_line() {
    return opt.holster_pouch ? "To heal: press B for your supplies, or grip at the belt pouch"
                             : "To heal: press B for your supplies";
}

void queue_start() {
    add(kTipStart, true, start_walk(), 30);
    add(kTipStart, false, kStartMenu, 30);
}

void queue_weapon(int w) {
    const bool gun = is_gun(w);
    const int kind = gun ? kTipGun : kTipMelee;
    std::string a, b;
    if (gun) {
        a = gun_fire();
        if (opt.two_hand && (w == 1 || w == 15)) b = kHandgunSteady;
        else if (opt.two_hand && takes_second_hand(w)) b = kGunBothHands;
    } else {
        if (!opt.physical_melee) return;
        a = kMeleeSwing;
        if (opt.two_hand && takes_second_hand(w)) b = kMeleeBothHands;
    }
    add(kind, true, a, 60, false, w);
    add(kind, false, b, 60, false, w);
    if (opt.holsters) add(kind, false, holster_line(gun), 60, false, w);
}

void queue_health() {
    if (opt.wrist_health) add(kTipHealth, true, kHealthWatch, 30);
    add(kTipHealth, !opt.wrist_health, heal_line(), 30);
}

void end_current(const char* why) {
    if (current.kind < 0) return;
    if (why && (current.counted || dropped_told++ < 40))
        rt_log("VR: frame %d: the hint %s (%s): %s", rt_frame, current.counted ? "ends" : "dropped unshown", why,
               current.text.c_str());
    current = Page{};
    current_up = 0;
    gap_until = now + 0.6;
}

void next_page() {
    while (queued > 0) {
        Page p = queue[0];
        for (int i = 1; i < queued; i++) queue[i - 1] = queue[i];
        queued--;
        if (p.drop_at < now) {
            if (dropped_told++ < 40)
                rt_log("VR: frame %d: a hint dropped unshown (its moment passed): %s", rt_frame, p.text.c_str());
            continue;
        }
        current = p;
        current_up = 0;
        return;
    }
}

}  // namespace

void tip_event(int kind) {
    if (!can_hint()) return;
    switch (kind) {
    case kTipHeld:
        // before any other line: the line up now goes back to wait after it
        if (!due(kTipHeld)) return;
        queued = std::min(queued, kQueue - 2);  // room for the line up now and this one (the last waiting go)
        if (current.kind >= 0) {
            for (int i = queued; i > 0; i--) queue[i] = queue[i - 1];
            queue[0] = current;
            queued++;
            current = Page{};
            current_up = 0;
        }
        add(kTipHeld, true, held_line(), 3, true);
        gap_until = 0;
        break;
    case kTipUse:
    case kTipItem:
        // first person only, and the label free (no other line up or waiting)
        if (view_mode != ViewMode::kFirstPerson || !label_free() || !due(kind)) return;
        add(kind, true, kind == kTipUse ? use_line() : item_line(), 1.5);
        break;
    default:
        break;
    }
}

void tip_moment_over(int kind) {
    if (kind < 0 || kind >= kTipCount) return;
    int n = 0;
    for (int i = 0; i < queued; i++)
        if (queue[i].kind != kind) queue[n++] = queue[i];
    queued = n;
    if (current.kind == kind) end_current("she is free");
}

bool tip_wants_use() {
    if (!can_hint() || view_mode != ViewMode::kFirstPerson || play_seconds < 3.0f || rt_frame % 8 != 0 || !label_free())
        return false;
    return due(kTipUse) || due(kTipItem);
}

bool tip_reload_due(int gun_item) {
    if (!opt.hints) return false;
    if (!counts_loaded) load_counts();
    const int i = reload_index(gun_item);
    return i >= 0 && !reload_shown[i];
}

void tip_reload_done(int gun_item) {
    const int i = reload_index(gun_item);
    if (i < 0 || !opt.hints || reload_shown[i]) return;
    if (!counts_loaded) load_counts();
    reload_shown[i] = 1;
    save_counts();
    rt_log("VR: frame %d: hints: the reload's steps shown for the %s (not again)", rt_frame, kReloadKeys[i] + 7);
}

void tip_reset_all() {
    if (!counts_loaded) load_counts();
    for (int k = 0; k < kTipCount; k++) shown[k] = 0;
    for (int i = 0; i < kReloadGuns; i++) reload_shown[i] = 0;
    save_counts();
    light_seen = false;
    weapon_seen = 0;
    weapon_held = 0;
    rt_log("VR: frame %d: hints: every hint will show again", rt_frame);
}

void tip_lines(std::vector<std::string>& out) {
    out.clear();
    out.push_back(start_walk());
    out.push_back(kStartMenu);
    out.push_back(light_line());
    out.push_back(use_line());
    out.push_back(item_line());
    out.push_back("Grabbed or knocked down - " + held_line());
    out.push_back("Guns - " + gun_fire());
    if (opt.two_hand) {
        out.push_back(std::string("Handgun - ") + kHandgunSteady);
        out.push_back(std::string("Long guns - ") + kGunBothHands);
    }
    if (opt.physical_melee) out.push_back(std::string("Melee weapons - ") + kMeleeSwing);
    if (opt.physical_melee && opt.two_hand) out.push_back(std::string("Pipe, katana, maul - ") + kMeleeBothHands);
    if (opt.holsters) {
        out.push_back("Guns - " + holster_line(true));
        out.push_back("Melee weapons - " + holster_line(false));
    }
    if (opt.wrist_health) out.push_back(kHealthWatch);
    out.push_back(heal_line());
    if (opt.manual_reload) {
        out.push_back("Reload - push the right stick down to drop the magazine (the stun gun's battery)");
        out.push_back("Reload - take a new one from the front of your belt, " + off_side() + " side (squeeze the grip)");
        out.push_back("Reload - push it up into the bottom of the grip (the stun gun: into its bottom end)");
        out.push_back("Reload, handgun - grip the top of the slide and pull it all the way back");
        out.push_back("Reload, submachine gun - grip the cocking knob (on top, at the back) and pull it back");
        out.push_back("Reload, shotgun - push shells from your belt one at a time into the port under it");
        out.push_back("Reload, shotgun - when it was empty: pull the pump back and push it forward");
    }
}

void tips_frame() {
    now += frame_dt();
    if (!counts_loaded) load_counts();
    if (!can_hint()) {
        // "Hints" off: nothing up or waiting. A menu, a scene or a screen: the lines wait (each dropped when its moment
        // has passed: a grab or a door soon, the start or a weapon's after a while)
        if (!opt.hints || !opt.vr) {
            if (current.kind >= 0) end_current(nullptr);
            queued = 0;
        }
        play_seconds = 0;
        return;
    }
    const bool fp = view_mode == ViewMode::kFirstPerson && head_known();
    play_seconds = fp ? play_seconds + frame_dt() : 0.0f;

    // What starts a hint this frame (the rest come from tip_event). The start's lines first: the flashlight's, a
    // weapon's and low health's wait until it has shown (vr360: the knife in hand from the first frame put its lines
    // before the start's).
    if (fp && play_seconds > 2.0f && due(kTipStart)) queue_start();
    const bool started = shown[kTipStart] != 0;
    if (fp && play_seconds > 2.0f && started && !light_seen) {
        Vec3 lp, ld;
        float level = 0;
        if (combat_beam(lp, ld, level) && level > 0.5f) {
            light_seen = true;
            if (due(kTipFlashlight)) add(kTipFlashlight, true, light_line(), 30);
        }
    }
    const int w = fp && weapon_shown_in_hand() ? game.weapon : 0;
    if (w != weapon_seen) weapon_seen = w, weapon_held = 0;
    weapon_held += frame_dt();
    // A weapon's lines are about the weapon in the hand: another in the hand, or none for a second, and they go
    // (vr361: the handgun's "Grip under it..." came up with the submachine gun in the hand, the reload's steps between)
    weapon_gone = w > 0 ? 0.0f : weapon_gone + frame_dt();
    if (w > 0 || weapon_gone > 1.0f) {
        int n = 0;
        for (int i = 0; i < queued; i++)
            if (!queue[i].weapon || queue[i].weapon == w) queue[n++] = queue[i];
        queued = n;
        if (current.weapon && current.weapon != w) end_current("another weapon, or none, in the hand");
    }
    if (w > 0 && weapon_held >= 0.8f && started) {  // in the hand 0.8 s (not a weapon passed through on a change)
        const int kind = is_gun(w) ? kTipGun : is_melee(w) ? kTipMelee : -1;
        if (kind >= 0 && due(kind)) queue_weapon(w);
    }
    if (fp && started && game.hp_max > 0 && game.hp > 0 && game.hp < 0.5f * game.hp_max && due(kTipHealth))
        queue_health();

    // The line up now: its time counts while the label shows it in full; then a short gap; then the next
    if (current.kind < 0 && now >= gap_until) next_page();
    if (current.kind < 0) return;
    const bool up = asked_frame >= rt_frame - 2 && hint_label_up();
    if (!up) {
        // held back (a reload step, the game's message, a screen) past its moment: dropped, or cut short
        if (current.drop_at < now) end_current("its moment passed");
        return;
    }
    current_up += frame_dt();
    if (!current.counted && current_up > 0.3f) {
        current.counted = true;
        if (current.first && !shown[current.kind]) {
            shown[current.kind] = 1;
            save_counts();
        }
        rt_log("VR: frame %d: hint (%s): %s", rt_frame, kTipKeys[current.kind], current.text.c_str());
    }
    // long enough to read: 2 s and a little per letter, 4 to 6 s
    const float show = std::clamp(2.0f + 0.045f * (float)current.text.size(), 4.0f, 6.0f);
    if (current_up >= show) end_current(nullptr);
}

const char* tip_text() {
    if (current.kind < 0 || !can_hint()) return nullptr;
    asked_frame = rt_frame;
    return current.text.c_str();
}

void tips_crash_info(FILE* f) {
    std::fprintf(f, "VR hints: %s; shown:", opt.hints ? "on" : "off");
    for (int k = 0; k < kTipCount; k++) std::fprintf(f, " %s %d", kTipKeys[k], shown[k]);
    for (int i = 0; i < kReloadGuns; i++) std::fprintf(f, " %s %d", kReloadKeys[i], reload_shown[i]);
    std::fprintf(f, "; up now: \"%s\", %d waiting\n", current.kind >= 0 ? current.text.c_str() : "", queued);
}

}  // namespace sh3q
