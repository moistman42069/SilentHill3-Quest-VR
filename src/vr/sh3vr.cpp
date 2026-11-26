// SPDX-License-Identifier: GPL-3.0-or-later
// SH3 Quest VR: the headset joined to the recompiled Silent Hill 3 (sh3vr.h says what the game sees of it).
//
// The frame: the game updates and draws one frame between two Presents. At Present the frame it drew goes to the
// headset (each eye's layer of the back buffer to its swapchain, VR's hands drawn in first, the panel's picture to the
// panel's) and the next frame is waited for (xrWaitFrame): its poses steer the next update (the pad, Heather's movement,
// the swings) and its drawing (the view at 0x4162A0, the eyes' corrections in the vertex stage).
//
// The eyes: the game draws once, from the head's centre, with its own projection widened to cover both eyes
// (sh3vr_proj_scale); every render target is a 2-layer array drawn with multiview, and each draw of the frame's camera
// has its clip position put right per eye: clip_eye = P_eye * T_eye * inverse(P_game) * clip_game.
//
// This file: the options, the frame, the view, the panel, the pad, movement and the C API. The hands, combat and the
// VR menu are sh3vr_body.cpp, sh3vr_combat.cpp and sh3vr_menu.cpp (sh3vr_internal.h).

#include "sh3vr_internal.h"
#include "wrist.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#if defined(__ANDROID__)
#include <sys/system_properties.h>  // device_defaults
#endif

#if !defined(__ANDROID__)
namespace quest {
extern void* (*sim_gl_proc)(const char*);
extern bool sim_ready;  // QUEST_SIM_ALIGN's mark: set when first person first starts in play
}
#endif

namespace sh3q {

// ================================================================ options

Options opt;

namespace {

const char* const kPages[] = {"Performance", "View", "Comfort", "Pointer", "Hands and weapons", "Vibration"};
const char* const kOffLowHigh[] = {"Off", "Low", "Medium", "High", "Highest"};
const char* const kSharpen[] = {"Off", "Normal", "Strong"};
const char* const kRefresh[] = {"72 Hz", "80 Hz", "90 Hz", "120 Hz"};
const float kRefreshValues[] = {72.0f, 80.0f, 90.0f, 120.0f};
// 0.1.3 (owner 87: "the vr menu doesnt seem to say \"dynamic cutscenes\" for the cutscene type"): the row says it;
// the new "At the camera" (always the game's camera) is last so 0, 1 and 2 keep their meaning in sh3vr.ini.
const char* const kCutscenes[] = {"Dynamic (her eyes or the camera)", "First person", "Cinema screen", "At the camera"};
const char* const kBody[] = {"Shown (head hidden)", "Hidden (hands only)"};
const char* const kTurn[] = {"Snap", "Smooth"};
const char* const kMoveBy[] = {"Where the head looks", "Where the left hand points"};
const char* const kPointerHand[] = {"Either (last trigger)", "Left", "Right"};
const char* const kBeamLook[] = {"Silent Hill (rust)", "White"};
const char* const kFlashlight[] = {"Where you look", "Where Heather faces (the game's)"};
const char* const kExtraVibration[] = {"Off", "Pickups", "The whole world"};

#define SW(key, name, page, next) {#key, name, page, Kind::kSwitch, &opt.key, 0, 0, 0, nullptr, 0, nullptr, "", next}
#define CH(key, name, page, list, next) \
    {#key, name, page, Kind::kChoice, &opt.key, 0, 0, 0, list, (int)(sizeof list / sizeof *list), nullptr, "", next}
#define NU(key, name, page, lo, hi, step, unit, next) \
    {#key, name, page, Kind::kNumber, &opt.key, lo, hi, step, nullptr, 0, nullptr, unit, next}

const OptionRow kRows[] = {
    // Performance
    {"refresh_hz", "Refresh rate", "Performance", Kind::kChoice, &opt.refresh_hz, 0, 0, 0, kRefresh, 4, kRefreshValues,
     "", true},
    NU(resolution, "Render resolution", "Performance", 0.6f, 1.5f, 0.05f, "x", true),
    SW(dynamic_resolution, "Dynamic resolution", "Performance", false),
    CH(foveation, "Foveated rendering", "Performance", kOffLowHigh, true),
    SW(boost, "CPU and GPU boost", "Performance", true),
    CH(sharpen, "Sharpening", "Performance", kSharpen, true),
    SW(show_performance, "Show performance", "Performance", false),
    // View
    SW(first_person, "First person", "View", false),
    CH(cutscenes, "Cutscenes", "View", kCutscenes, false),
    SW(auto_third_person, "Third person where the game needs it", "View", false),
    SW(height_auto, "Measure your height", "View", false),
    NU(player_height_cm, "Your height", "View", 120.0f, 220.0f, 1.0f, "cm", false),
    SW(seated, "Seated", "View", false),
    SW(room_walk, "Walking in the room moves Heather", "View", false),
    CH(body, "Heather's body in first person", "View", kBody, false),
    SW(arms_follow, "Heather's arms follow your hands", "View", false),
    SW(body_fade, "Fade body near your eyes", "View", false),
    SW(hide_arms_near, "Hide arms near your eyes", "View", false),
    SW(spatial_sound, "Spatial sound", "View", false),
    SW(film_grain, "Film grain", "View", false),
    SW(drifting_fog, "Drifting fog", "View", false),
    NU(brightness, "Brightness", "View", 50.0f, 300.0f, 10.0f, "%", false),
    NU(contrast, "Contrast", "View", 50.0f, 150.0f, 5.0f, "%", false),
    NU(saturation, "Color saturation", "View", 0.0f, 200.0f, 10.0f, "%", false),
    NU(dark_edges, "Darker edges", "View", 0.0f, 100.0f, 10.0f, "%", false),
    SW(smooth_edges, "Smooth edges", "View", false),
    SW(her_shadow, "Heather's shadow", "View", false),
    NU(panel_distance, "Screen distance", "View", 0.8f, 3.0f, 0.1f, "m", false),
    NU(panel_width, "Screen size", "View", 0.6f, 2.5f, 0.1f, "m", false),
    SW(panel_follow, "Screen follows the view", "View", false),
    SW(menus_in_view, "Menus open in front of you", "View", false),
    SW(messages_at_things, "Messages next to things", "View", false),
    // Comfort
    CH(turn, "Turning", "Comfort", kTurn, false),
    NU(snap_degrees, "Snap turn angle", "Comfort", 15.0f, 90.0f, 15.0f, "deg", false),
    NU(smooth_degrees, "Smooth turn speed", "Comfort", 30.0f, 360.0f, 15.0f, "deg/s", false),
    CH(move_by, "Walk towards", "Comfort", kMoveBy, false),
    SW(vignette, "Comfort vignette", "Comfort", false),
    NU(vignette_strength, "Vignette strength", "Comfort", 10.0f, 100.0f, 10.0f, "%", false),
    SW(head_fade, "Head in walls darkens the view", "Comfort", false),
    // Pointer
    SW(pointer, "Pointer", "Pointer", false),
    CH(pointer_hand, "Pointer hand", "Pointer", kPointerHand, false),
    CH(beam_look, "Beam look", "Pointer", kBeamLook, false),
    // Hands and weapons
    SW(left_handed, "Left-handed", "Hands and weapons", false),
    SW(aim_by_hand, "Aim with the controller", "Hands and weapons", false),
    SW(quick_raise, "Quick gun raise", "Hands and weapons", false),
    SW(laser, "Guide dot", "Hands and weapons", false),
    SW(auto_aim_first_person, "Game's auto-aim in first person", "Hands and weapons", false),
    SW(two_hand, "Two-hand grips", "Hands and weapons", false),
    SW(physical_melee, "Physical melee", "Hands and weapons", false),
    SW(punches, "Punches", "Hands and weapons", false),
    SW(gun_club, "Guns as clubs", "Hands and weapons", false),
    SW(holsters, "Holsters", "Hands and weapons", false),
    SW(holster_pouch, "Belt pouch (supplies)", "Hands and weapons", false),
    CH(flashlight, "Chest flashlight aims", "Hands and weapons", kFlashlight, false),
    SW(chest_light_touch, "Take the flashlight from your chest", "Hands and weapons", false),
    SW(finger_bending, "Fingers stop on surfaces", "Hands and weapons", false),
    SW(ghost_hands, "Ghost hands in walls", "Hands and weapons", false),
    SW(grabbing, "Grab with the hand", "Hands and weapons", false),
    SW(take_by_hand, "Take items with your hand", "Hands and weapons", false),
    SW(manual_reload, "Manual reload", "Hands and weapons", false),
    SW(reload_hints, "Reload hints", "Hands and weapons", false),
    SW(hand_sounds, "Hand sounds", "Hands and weapons", false),
    SW(wrist_health, "Health bar on your wrist", "Hands and weapons", false),
    SW(blocking, "Block with your weapon or arms", "Hands and weapons", false),
    SW(shake_free, "Shake to break free", "Hands and weapons", false),
    // Vibration (keys stay haptics* so earlier sh3vr.ini files keep their settings)
    SW(haptics, "Vibration", "Vibration", false),
    NU(haptics_strength, "Vibration strength", "Vibration", 10.0f, 100.0f, 10.0f, "%", false),
    SW(haptic_surfaces, "Vibration on walls and floors", "Vibration", false),
    CH(haptic_extra, "Extra vibration", "Vibration", kExtraVibration, false),
    SW(haptic_weapon_change, "Buzz on weapon change", "Vibration", false),
    SW(heartbeat, "Heartbeat at low health", "Vibration", false),
    SW(damage_pulse, "Damage pulse", "Vibration", false),
};
#undef SW
#undef CH
#undef NU

bool parse_bool(const char* v) { return std::atoi(v) != 0 || !std::strcmp(v, "true") || !std::strcmp(v, "on"); }

void clamp_options() {
    opt.player_height_cm = std::clamp(opt.player_height_cm, 100.0f, 230.0f);
    opt.resolution = std::clamp(opt.resolution, 0.5f, 1.6f);
    opt.foveation = std::clamp(opt.foveation, 0, 4);
    opt.sharpen = std::clamp(opt.sharpen, 0, 2);
    opt.cutscenes = std::clamp(opt.cutscenes, 0, 3);
    opt.body = std::clamp(opt.body, 0, 1);
    opt.turn = std::clamp(opt.turn, 0, 1);
    opt.move_by = std::clamp(opt.move_by, 0, 1);
    opt.pointer_hand = std::clamp(opt.pointer_hand, 0, 2);
    opt.beam_look = std::clamp(opt.beam_look, 0, 1);
    opt.flashlight = std::clamp(opt.flashlight, 0, 1);
    opt.haptic_extra = std::clamp(opt.haptic_extra, 0, 2);
    opt.panel_distance = std::clamp(opt.panel_distance, 0.5f, 4.0f);
    opt.panel_width = std::clamp(opt.panel_width, 0.4f, 3.0f);
    opt.snap_degrees = std::clamp(opt.snap_degrees, 5.0f, 180.0f);
    opt.smooth_degrees = std::clamp(opt.smooth_degrees, 10.0f, 720.0f);
    opt.vignette_strength = std::clamp(opt.vignette_strength, 0.0f, 100.0f);
    opt.haptics_strength = std::clamp(opt.haptics_strength, 0.0f, 100.0f);
    opt.brightness = std::clamp(opt.brightness, 50.0f, 300.0f);
    opt.contrast = std::clamp(opt.contrast, 50.0f, 150.0f);
    opt.saturation = std::clamp(opt.saturation, 0.0f, 200.0f);
    opt.dark_edges = std::clamp(opt.dark_edges, 0.0f, 100.0f);
}

}  // namespace

const OptionRow* option_rows(int& count) {
    count = (int)(sizeof kRows / sizeof *kRows);
    return kRows;
}

const char* const* option_pages(int& count) {
    count = (int)(sizeof kPages / sizeof *kPages);
    return kPages;
}

std::string option_value_text(const OptionRow& r) {
    char buf[96];
    switch (r.kind) {
    case Kind::kSwitch: return *(bool*)r.value ? "On" : "Off";
    case Kind::kChoice: {
        int i = 0;
        if (r.values) {
            const float v = *(float*)r.value;
            for (int k = 0; k < r.count; k++)
                if (std::fabs(r.values[k] - v) < std::fabs(r.values[i] - v)) i = k;
        } else i = std::clamp(*(int*)r.value, 0, r.count - 1);
        return r.choices[i];
    }
    case Kind::kNumber: {
        const float v = *(float*)r.value;
        if (r.step < 0.999f) std::snprintf(buf, sizeof buf, "%.2f %s", v, r.unit);
        else std::snprintf(buf, sizeof buf, "%.0f %s", v, r.unit);
        return buf;
    }
    }
    return "";
}

void option_step(const OptionRow& r, int dir) {
    switch (r.kind) {
    case Kind::kSwitch: *(bool*)r.value = !*(bool*)r.value; break;
    case Kind::kChoice:
        if (r.values) {
            float& v = *(float*)r.value;
            int i = 0;
            for (int k = 0; k < r.count; k++)
                if (std::fabs(r.values[k] - v) < std::fabs(r.values[i] - v)) i = k;
            v = r.values[(i + dir + r.count) % r.count];
        } else {
            int& v = *(int*)r.value;
            v = (v + dir + r.count) % r.count;
        }
        break;
    case Kind::kNumber: {
        float& v = *(float*)r.value;
        v = std::clamp(v + dir * r.step, r.lo, r.hi);
        v = std::round(v / r.step) * r.step;
        break;
    }
    }
    clamp_options();
}

// The defaults that depend on the headset (as Jak's vr_options.cpp strong_quest): a Quest 3 or 3S (Android's device
// names eureka and panther, the XR2 Gen 2) starts at the top of Render resolution, 1.5x, with Dynamic resolution
// keeping the frame rate (LIST 51, the owner: "be sure highest resolution is set with dynamic resolution"); the
// Quest 2 and any other headset keep 1.0x. sh3vr.ini's own value, when there is one, stands.
void device_defaults() {
#if defined(__ANDROID__)
    char device[PROP_VALUE_MAX] = {};
    __system_property_get("ro.product.device", device);
    if (!std::strcmp(device, "eureka") || !std::strcmp(device, "panther")) opt.resolution = 1.5f;
    rt_log("VR: the headset's device %s: Render resolution %.2fx to start", device[0] ? device : "(unknown)",
           opt.resolution);
#endif
}

// 0.1.3: the defaults' version sh3vr.ini is written with ("defaults = 3"; the launcher's Options.DEFAULTS_VERSION)
constexpr int kDefaultsVersion = 3;

void load_options() {
    device_defaults();
    FILE* f = std::fopen("sh3vr.ini", "r");
    if (!f) {
        rt_log("VR: no sh3vr.ini in %s: the defaults (every VR feature on)", rt_game_dir);
        return;
    }
    char line[256];
    int n = 0;
    const OptionRow* rows = option_rows(n);
    int defaults = 0;  // 0.1.3: the defaults' version the file says it has (none before 0.1.3)
    int dynamic_switch = -1;  // 0.1.2's "Dynamic cutscenes" as the file has it (-1: not there)
    while (std::fgets(line, sizeof line, f)) {
        char key[64], val[128];
        if (std::sscanf(line, " %63[^= ] = %127s", key, val) != 2 || key[0] == '#' || key[0] == ';') continue;
        if (!std::strcmp(key, "vr")) {
            opt.vr = parse_bool(val);
            continue;
        }
        if (!std::strcmp(key, "defaults")) {
            defaults = std::atoi(val);
            continue;
        }
        if (!std::strcmp(key, "dynamic_cutscenes")) {  // 0.1.2's own switch (now the Cutscenes row's "Dynamic")
            dynamic_switch = parse_bool(val) ? 1 : 0;
            continue;
        }
        for (int i = 0; i < n; i++) {
            if (std::strcmp(key, rows[i].key)) continue;
            if (rows[i].kind == Kind::kSwitch) *(bool*)rows[i].value = parse_bool(val);
            else if (rows[i].kind == Kind::kNumber || rows[i].values) *(float*)rows[i].value = (float)std::atof(val);
            else *(int*)rows[i].value = std::atoi(val);
        }
    }
    std::fclose(f);
    clamp_options();
    // 0.1.3 (owner 87): "Dynamic cutscenes" is the Cutscenes row's first choice; a file with 0.1.2's switch off and
    // "In the scene at the camera" keeps that look as "At the camera" (3). The key is not written again.
    const bool fold = dynamic_switch >= 0;
    if (dynamic_switch == 0 && opt.cutscenes == 0) opt.cutscenes = 3;
    if (fold && defaults >= kDefaultsVersion) {
        const bool saved = save_options();
        rt_log("VR: sh3vr.ini's \"dynamic_cutscenes = %d\" is now the Cutscenes row (%s)%s", dynamic_switch,
               kCutscenes[opt.cutscenes], saved ? "" : "; the file could not be saved");
    }
    if (defaults < kDefaultsVersion) {
        // 0.1.3 (owner 84: "drifting fog should be off by default. smooth turn should be on by default ... no screen
        // filters or ANYTHING like that shit on by default"): a file from before 0.1.3 kept 0.1.2's defaults (the VR
        // menu and the launcher write every option), so the options 0.1.3 changed take its defaults once and the file
        // says so (the launcher's Config.newDefaults does the same, with the same keys).
        const Options d{};
        opt.sharpen = d.sharpen;
        opt.film_grain = d.film_grain;
        opt.drifting_fog = d.drifting_fog;
        opt.brightness = d.brightness;
        opt.contrast = d.contrast;
        opt.saturation = d.saturation;
        opt.dark_edges = d.dark_edges;
        opt.smooth_edges = d.smooth_edges;
        opt.turn = d.turn;
        opt.vignette = d.vignette;
        const bool saved = save_options();
        rt_log("VR: sh3vr.ini from before 0.1.3 (defaults %d): 0.1.3's defaults put in once (drifting fog off, smooth "
               "turning, no sharpening, film grain, smooth edges or comfort vignette, the picture at 100%%)%s",
               defaults, saved ? "" : "; the file could not be saved");
        if (fold)
            rt_log("VR: sh3vr.ini's \"dynamic_cutscenes = %d\" is now the Cutscenes row: %s", dynamic_switch,
                   kCutscenes[opt.cutscenes]);
    }
}

// sh3vr.ini rewritten whole (the VR menu's changes; the launcher reads it back on its Options screen).
bool save_options() {
    FILE* f = std::fopen("sh3vr.ini.new", "w");
    if (!f) {
        rt_log("VR: sh3vr.ini could not be written in %s", rt_game_dir);
        return false;
    }
    std::fprintf(f, "# SH3 Quest VR options (the launcher's Options and the in-game VR menu write this file)\n");
    std::fprintf(f, "vr = %d\n", opt.vr ? 1 : 0);
    std::fprintf(f, "defaults = %d\n", kDefaultsVersion);  // 0.1.3: the defaults' version (load_options)
    int n = 0;
    const OptionRow* rows = option_rows(n);
    for (int i = 0; i < n; i++) {
        const OptionRow& r = rows[i];
        if (r.kind == Kind::kSwitch) std::fprintf(f, "%s = %d\n", r.key, *(bool*)r.value ? 1 : 0);
        else if (r.kind == Kind::kNumber || r.values) std::fprintf(f, "%s = %g\n", r.key, *(float*)r.value);
        else std::fprintf(f, "%s = %d\n", r.key, *(int*)r.value);
    }
    const bool ok = std::fclose(f) == 0;
    std::remove("sh3vr.ini");
    return ok && std::rename("sh3vr.ini.new", "sh3vr.ini") == 0;
}

// ================================================================ matrices (column-vector: m[r][c])

namespace {

struct M4 {
    float m[4][4]{};
    static M4 identity() {
        M4 r;
        for (int i = 0; i < 4; i++) r.m[i][i] = 1;
        return r;
    }
    // A D3D row-vector matrix (16 floats, row-major) as the column-vector one it is the transpose of.
    static M4 from_d3d(const float* d) {
        M4 r;
        for (int i = 0; i < 4; i++)
            for (int j = 0; j < 4; j++) r.m[i][j] = d[j * 4 + i];
        return r;
    }
    void to_gl(float* out) const {  // column-major
        for (int c = 0; c < 4; c++)
            for (int r = 0; r < 4; r++) out[c * 4 + r] = m[r][c];
    }
    static M4 from_gl(const float* g) {
        M4 r;
        for (int c = 0; c < 4; c++)
            for (int k = 0; k < 4; k++) r.m[k][c] = g[c * 4 + k];
        return r;
    }
};

M4 operator*(const M4& a, const M4& b) {
    M4 r;
    for (int i = 0; i < 4; i++)
        for (int j = 0; j < 4; j++) {
            float s = 0;
            for (int k = 0; k < 4; k++) s += a.m[i][k] * b.m[k][j];
            r.m[i][j] = s;
        }
    return r;
}

bool invert(const M4& a, M4& out) {
    const float* m = &a.m[0][0];
    float inv[16];
    inv[0] = m[5] * m[10] * m[15] - m[5] * m[11] * m[14] - m[9] * m[6] * m[15] + m[9] * m[7] * m[14] + m[13] * m[6] * m[11] - m[13] * m[7] * m[10];
    inv[4] = -m[4] * m[10] * m[15] + m[4] * m[11] * m[14] + m[8] * m[6] * m[15] - m[8] * m[7] * m[14] - m[12] * m[6] * m[11] + m[12] * m[7] * m[10];
    inv[8] = m[4] * m[9] * m[15] - m[4] * m[11] * m[13] - m[8] * m[5] * m[15] + m[8] * m[7] * m[13] + m[12] * m[5] * m[11] - m[12] * m[7] * m[9];
    inv[12] = -m[4] * m[9] * m[14] + m[4] * m[10] * m[13] + m[8] * m[5] * m[14] - m[8] * m[6] * m[13] - m[12] * m[5] * m[10] + m[12] * m[6] * m[9];
    inv[1] = -m[1] * m[10] * m[15] + m[1] * m[11] * m[14] + m[9] * m[2] * m[15] - m[9] * m[3] * m[14] - m[13] * m[2] * m[11] + m[13] * m[3] * m[10];
    inv[5] = m[0] * m[10] * m[15] - m[0] * m[11] * m[14] - m[8] * m[2] * m[15] + m[8] * m[3] * m[14] + m[12] * m[2] * m[11] - m[12] * m[3] * m[10];
    inv[9] = -m[0] * m[9] * m[15] + m[0] * m[11] * m[13] + m[8] * m[1] * m[15] - m[8] * m[3] * m[13] - m[12] * m[1] * m[11] + m[12] * m[3] * m[9];
    inv[13] = m[0] * m[9] * m[14] - m[0] * m[10] * m[13] - m[8] * m[1] * m[14] + m[8] * m[2] * m[13] + m[12] * m[1] * m[10] - m[12] * m[2] * m[9];
    inv[2] = m[1] * m[6] * m[15] - m[1] * m[7] * m[14] - m[5] * m[2] * m[15] + m[5] * m[3] * m[14] + m[13] * m[2] * m[7] - m[13] * m[3] * m[6];
    inv[6] = -m[0] * m[6] * m[15] + m[0] * m[7] * m[14] + m[4] * m[2] * m[15] - m[4] * m[3] * m[14] - m[12] * m[2] * m[7] + m[12] * m[3] * m[6];
    inv[10] = m[0] * m[5] * m[15] - m[0] * m[7] * m[13] - m[4] * m[1] * m[15] + m[4] * m[3] * m[13] + m[12] * m[1] * m[7] - m[12] * m[3] * m[5];
    inv[14] = -m[0] * m[5] * m[14] + m[0] * m[6] * m[13] + m[4] * m[1] * m[14] - m[4] * m[2] * m[13] - m[12] * m[1] * m[6] + m[12] * m[2] * m[5];
    inv[3] = -m[1] * m[6] * m[11] + m[1] * m[7] * m[10] + m[5] * m[2] * m[11] - m[5] * m[3] * m[10] - m[9] * m[2] * m[7] + m[9] * m[3] * m[6];
    inv[7] = m[0] * m[6] * m[11] - m[0] * m[7] * m[10] - m[4] * m[2] * m[11] + m[4] * m[3] * m[10] + m[8] * m[2] * m[7] - m[8] * m[3] * m[6];
    inv[11] = -m[0] * m[5] * m[11] + m[0] * m[7] * m[9] + m[4] * m[1] * m[11] - m[4] * m[3] * m[9] - m[8] * m[1] * m[7] + m[8] * m[3] * m[5];
    inv[15] = m[0] * m[5] * m[10] - m[0] * m[6] * m[9] - m[4] * m[1] * m[10] + m[4] * m[2] * m[9] + m[8] * m[1] * m[6] - m[8] * m[2] * m[5];
    const float det = m[0] * inv[0] + m[1] * inv[4] + m[2] * inv[8] + m[3] * inv[12];
    if (std::fabs(det) < 1e-20f || !std::isfinite(det)) return false;
    for (int i = 0; i < 16; i++) (&out.m[0][0])[i] = inv[i] / det;
    return true;
}

// Rotation matrix of a unit quaternion (columns: the rotated axes).
void quat_matrix(Quat q, float r[3][3]) {
    const float x = q.x, y = q.y, z = q.z, w = q.w;
    r[0][0] = 1 - 2 * (y * y + z * z); r[0][1] = 2 * (x * y - z * w);     r[0][2] = 2 * (x * z + y * w);
    r[1][0] = 2 * (x * y + z * w);     r[1][1] = 1 - 2 * (x * x + z * z); r[1][2] = 2 * (y * z - x * w);
    r[2][0] = 2 * (x * z - y * w);     r[2][1] = 2 * (y * z + x * w);     r[2][2] = 1 - 2 * (x * x + y * y);
}

}  // namespace

// ================================================================ small helpers (sh3vr_internal.h)

// Tracking (Y up, -Z forward) to the game's axes (Y down, Z forward): a half turn about X.
Vec3 to_game(Vec3 v) { return {v.x, -v.y, -v.z}; }
Quat to_game(Quat q) { return {q.x, -q.y, -q.z, q.w}; }
// A turn of the game's world about its Y (down) axis by `yaw`: forward (0,0,1) goes to (sin yaw, 0, cos yaw).
Vec3 game_yaw(Vec3 v, float yaw) {
    const float c = std::cos(yaw), s = std::sin(yaw);
    return {c * v.x + s * v.z, v.y, -s * v.x + c * v.z};
}
Quat game_yaw_quat(float yaw) { return {0, std::sin(yaw * 0.5f), 0, std::cos(yaw * 0.5f)}; }
float heading_of(Vec3 f) { return std::atan2(f.x, f.z); }
float wrap(float a) { return wrap_angle(a); }
Vec3 normalized(Vec3 v) {
    const float l = length(v);
    return l > 1e-6f ? v * (1.0f / l) : Vec3{0, 0, 0};
}
Vec3 cross(Vec3 a, Vec3 b) { return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x}; }
Vec3 lerp(Vec3 a, Vec3 b, float t) { return a + (b - a) * t; }

// ================================================================ state

Headset* hs = nullptr;
bool active = false;
FrameInfo fi;
bool have_frame = false;
float display_hz = 0;
Sh3VrGame game{};
ViewMode view_mode = ViewMode::kFlat;
bool third_person = false;
bool auto_third = false;
bool vr_menu_open = false;
Sh3VrMove move{};
Controls prev{};
int point_hand = 1;
bool mouse_on = false;
bool catching_up = false;
float blocked_seconds = 0;

namespace {

int eye_w = 0, eye_h = 0;
bool game_seen = false;
float last_hp = -1;

// The rig: tracking's head at `origin` lands at the anchor (Heather, or the game's camera); `yaw` turns tracking's
// forward to a game heading.
struct {
    float yaw = 0;
    Vec3 origin;
    Vec3 neck0;                     // first person: the neck (rig.h neck_of) at the recentre, which lands at her neck
    bool set = false;
    float cam_yaw_at_recentre = 0;  // third person: the head's tracking yaw at the recentre
} rig;

// "Your height" measured (LIST 42, session 6): the eyes' height standing, from the headset's floor. The highest the
// head has been held for half a second, eased in; a recentre standing takes the head's height at once.
float standing_eyes = -1.0f;
float above_for = 0.0f;
bool recentred_now = false;  // a recentre asked: the next first-person placement measures the standing eyes

// Tracking -> world this frame: world = head_w + game_yaw(to_game(rotate(pre, p - head_t)), yaw) * units.
struct {
    bool valid = false;
    Vec3 head_t, head_w;
    float yaw = 0;
    Quat pre;
} map;

// The frame camera's projection (0x72C360 when the main view was built) and the corrections from it.
float p_main[16];
bool p_main_valid = false;
float eye_corr[32], eye_ident[32];
M4 eye_corr_m[2];
bool corr_valid = false;
float proj_k[2] = {1.0f, 1.0f};   // our factors on the game's projection scales
float want_cot[2] = {1.0f, 1.0f}; // the projection's x and y scale that covers both eyes
float vp_gl[16];
M4 vp_m;
bool vp_valid = false;
float c2w_gl[16];      // clip space -> world (V^-1 P^-1: inverted apart, the view's large translation kept exact)
bool c2w_valid = false;
float depth_abcd[4] = {1, 0, 1, 0};

// The view this frame (what 0x4162A0 was given by us): main camera draws are recognised by it.
float our_view[16];
bool our_view_set = false;

// The panel
GLuint panel_tex = 0, panel_depth = 0, panel_fbo = 0;
int panel_w = 1024, panel_h = 768;
bool panel_used = false;         // the game drew on it this frame
bool panel_used_last = false;
Pose panel_pose;                 // tracking space
bool panel_placed = false;
bool panel_turning = false;      // following the head round (gently)

// Input
Sh3VrPad pad{};
int menu_held_frames = 0;
bool menu_long_done = false;
int pause_pulse = 0;
int fire_frames = 0;
float pull_left = 0;             // seconds a gun's pull is still kept for, until its shot (Quick gun raise)
bool pull_shot = true;           // the pull's shot came
bool snap_ready = true;
float mouse_u = 0.5f, mouse_v = 0.5f;
bool mouse_left = false, mouse_right = false;
int hovered_cell = -1;           // the pointer's haptic tick: a step of the screen it crossed

// Movement
Vec3 prev_pos;
bool prev_pos_valid = false;
float prev_dist = 0;
float far_camera_seconds = 0;

// GL objects for the eyes' copies
GLuint read_fbo = 0, draw_fbo = 0;

void log_cb(int level, const char* text) { rt_log("%s%s", level >= 2 ? "VR warning: " : "", text); }

}  // namespace

bool head_known() { return have_frame && fi.views_valid; }
bool in_play() { return game.playable && !game.cutscene && !game.demo && !vr_menu_open; }
float frame_dt() { return display_hz > 0 ? 1.0f / display_hz : 1.0f / 72.0f; }
// 0.1.3 (owner 84: "im just simply moving the knife arm gently/arm in general, and melee animation/sound happens"): the
// real seconds since the last frame, from the headset's display times (begin_next_frame). frame_dt() is the display's
// period, the game's own tick; while the game falls behind (the owner's 0.1.2 run: 34-57 frames a second at 120 Hz in
// the mall, every swing it logged among them) each frame spans two or three periods, and a hand's motion taken over
// one period read two or three times too fast: a gentle move of 0.8 m/s came out a knife's swing. No display times
// (the PC rig before 0.1.3): frame_dt().
static float motion_seconds = 0;
float motion_dt() { return motion_seconds > 0 ? motion_seconds : frame_dt(); }
int main_hand() { return opt.left_handed ? kLeft : kRight; }
int off_hand() { return opt.left_handed ? kRight : kLeft; }

bool map_ready() { return map.valid; }
Vec3 world_point(Vec3 p) {
    return map.head_w + game_yaw(to_game(rotate(map.pre, p - map.head_t)), map.yaw) * kUnitsPerMetre;
}
Quat world_quat(Quat q) { return game_yaw_quat(map.yaw) * to_game(map.pre * q); }
Vec3 world_dir(Vec3 d) { return game_yaw(to_game(rotate(map.pre, d)), map.yaw); }
Vec3 tracking_point(Vec3 w) {
    const Vec3 g = game_yaw(w - map.head_w, -map.yaw) * (1.0f / kUnitsPerMetre);
    return map.head_t + rotate(conjugate(map.pre), to_game(g));
}
Quat tracking_quat(Quat w) { return conjugate(map.pre) * to_game(game_yaw_quat(-map.yaw) * w); }
Vec3 tracking_dir(Vec3 d) { return rotate(conjugate(map.pre), to_game(game_yaw(d, -map.yaw))); }
Vec3 heather_chest_world() {
    // Her chest: 1.25 m up from her feet (Y down), a hand ahead of her middle.
    const Vec3 fwd{std::sin(game.yaw), 0, std::cos(game.yaw)};
    return Vec3{game.pos[0], game.pos[1] - 1.25f * kUnitsPerMetre, game.pos[2]} + fwd * (0.12f * kUnitsPerMetre);
}

bool camera_ready() { return vp_valid && corr_valid; }
const float* frame_vp() { return vp_gl; }
bool clip_to_world(float out[16]) {
    if (!c2w_valid || !vp_valid) return false;
    std::memcpy(out, c2w_gl, sizeof c2w_gl);
    return true;
}
const float* eye_corrections() { return corr_valid ? eye_corr : eye_ident; }
float linear_depth(float d) {
    const float a = depth_abcd[0], b = depth_abcd[1], c = depth_abcd[2], e = depth_abcd[3];
    const float den = d * c - a;
    if (std::fabs(den) < 1e-9f) return 1e9f;
    return (b - d * e) / den;
}
void clip_of(int e, Vec3 w, float out[4]) {
    const float v[4] = {w.x, w.y, w.z, 1.0f};
    float g[4];
    for (int r = 0; r < 4; r++) g[r] = vp_m.m[r][0] * v[0] + vp_m.m[r][1] * v[1] + vp_m.m[r][2] * v[2] + vp_m.m[r][3];
    const M4& E = corr_valid ? eye_corr_m[e & 1] : M4::identity();
    for (int r = 0; r < 4; r++) out[r] = E.m[r][0] * g[0] + E.m[r][1] * g[1] + E.m[r][2] * g[2] + E.m[r][3] * g[3];
}

void buzz(int hand, float strength, float seconds, float hz) {
    if (!active || !hs || !opt.haptics || strength <= 0) return;
    const float s = std::clamp(strength * opt.haptics_strength / 100.0f, 0.0f, 1.0f);
    hs->haptic(hand == kLeft ? kLeft : kRight, s, seconds, hz);
}

namespace {

// ---------------------------------------------------------------- the head in the game

// The head's pose in tracking space, relative to the recentre, as game-space offset and rotation (first person).
void place_rig_first_person(bool force) {
    if (!head_known() || !game.player_valid) return;
    if (!rig.set || force) {
        // The head as it is now looks along Heather's facing and stands at her.
        rig.yaw = wrap(game.yaw + tracking_yaw(fi.head.orientation));
        rig.origin = fi.head.position;
        rig.neck0 = neck_of(fi.head);
        rig.set = true;
        // A recentre is done standing (or seated, with Seated on): the eyes' height now is the player's.
        const float lv = fi.head_above_floor + (level_eyes(fi.head).y - fi.head.position.y);
        if ((force || recentred_now) && !opt.seated && fi.head_above_floor > 0.3f && lv > 1.0f) standing_eyes = std::min(lv, 2.1f);
        recentred_now = false;
        rt_log("VR: recentred (first person): Heather's facing %.2f, rig yaw %.2f", game.yaw, rig.yaw);
    }
}

}  // namespace

// The player's height (m): measured from the headset (the default) or "Your height".
float player_height_m() {
    if (opt.height_auto && standing_eyes > 0) return standing_eyes / kEyeRatio;
    return opt.player_height_cm / 100.0f;
}

namespace {

// Each frame in first person: the standing eyes' height, from the head with the head level (a nod is not a crouch).
void measure_height() {
    static int done_frame = -1;
    if (done_frame == rt_frame || opt.seated || fi.head_above_floor <= 0.3f) return;
    done_frame = rt_frame;
    const float lv = fi.head_above_floor + (level_eyes(fi.head).y - fi.head.position.y);
    if (lv < 1.0f || lv > 2.3f) return;
    const float before = standing_eyes;
    if (standing_eyes < 0) standing_eyes = lv;
    else if (lv > standing_eyes + 0.01f) {
        above_for += frame_dt();
        if (above_for > 0.5f) standing_eyes += (lv - standing_eyes) * (1.0f - std::exp(-2.0f * frame_dt()));
    } else above_for = 0;
    standing_eyes = std::min(standing_eyes, 2.1f);
    static float logged = -1;
    if (std::fabs(standing_eyes - logged) > 0.01f) {
        logged = standing_eyes;
        rt_log("VR: frame %d: your eyes stand %.2f m up (your height about %.0f cm%s): Heather's eyes take their place",
               rt_frame, standing_eyes, standing_eyes / kEyeRatio * 100.0f,
               opt.height_auto ? "" : ", not used: \"Measure your height\" is off");
        (void)before;
    }
}

float eye_height_units() {
    const float ratio = kHeatherHeight / player_height_m();
    if (!opt.seated && fi.head_above_floor > 0.3f) return fi.head_above_floor * ratio * kUnitsPerMetre;
    // No floor known (or seated): the head at the recentre stands at Heather's eye height.
    return (kHeatherHeight * kEyeRatio + (fi.head.position.y - rig.origin.y)) * kUnitsPerMetre;
}

// The room offset of the player's neck from Heather's, in the game's world (x, z; game units): the neck, not the eyes,
// so turning or nodding the head never reads as a step (proper IK, session 6: the body hangs from the neck).
Vec3 room_offset() {
    Vec3 t = neck_of(fi.head) - rig.neck0;
    t.y = 0;
    return game_yaw(to_game(t), rig.yaw) * kUnitsPerMetre;
}

// The eyes ahead of the neck, in the game's world (x, z; game units): about 15 cm along the head's facing.
Vec3 eyes_ahead_of_neck() {
    Vec3 t = fi.head.position - neck_of(fi.head);
    t.y = 0;
    return game_yaw(to_game(t), rig.yaw) * kUnitsPerMetre;
}

// The view (D3D row-vector) of a head at `pos` turned `q` in the game's world.
void make_view(Vec3 pos, Quat q, float v[16]) {
    float r[3][3];
    quat_matrix(q, r);  // columns: the head's right, down, forward in the world
    for (int i = 0; i < 3; i++) {
        v[i * 4 + 0] = r[i][0];
        v[i * 4 + 1] = r[i][1];
        v[i * 4 + 2] = r[i][2];
        v[i * 4 + 3] = 0;
    }
    const float p[3] = {pos.x, pos.y, pos.z};
    for (int j = 0; j < 3; j++) v[12 + j] = -(p[0] * r[0][j] + p[1] * r[1][j] + p[2] * r[2][j]);
    v[15] = 1;
}

// The game camera's position and heading from its view.
bool camera_of(const float* v, Vec3& pos, float& yaw) {
    const Vec3 f{v[0 * 4 + 2], v[1 * 4 + 2], v[2 * 4 + 2]};
    if (!std::isfinite(f.x) || f.x * f.x + f.z * f.z < 1e-6f) return false;
    for (int i = 0; i < 3; i++) {
        float s = 0;
        for (int j = 0; j < 3; j++) s += v[12 + j] * v[i * 4 + j];
        (&pos.x)[i] = -s;
    }
    yaw = heading_of(f);
    return true;
}

// The mapping (and the head's place in the world) for a view mode: first person at Heather, else at the camera whose
// view is `cam` (the game's own, D3D row-vector).
bool compute_map(ViewMode mode, const float* cam, Vec3& head_pos, Quat& head_q) {
    if (!head_known()) return false;
    if (mode == ViewMode::kFirstPerson) {
        if (!game.player_valid) return false;
        place_rig_first_person(false);
        const Vec3 off = room_offset();
        measure_height();
        // Her neck at the player's neck, the eyes ahead of it as the head has them (proper IK, session 6: the eyes
        // right over her feet put them in her neck looking down, run vr52).
        const Vec3 ahead = eyes_ahead_of_neck();
        head_pos = {game.pos[0] + off.x + ahead.x, game.pos[1] - eye_height_units(), game.pos[2] + off.z + ahead.z};
        if ((game.cutscene || game.demo) && game_head_frame >= rt_frame - 3) {
            // LIST 48: in a scene the eyes ride her head as the game's animation poses it (seated, kneeling, lying; not
            // as VR's IK leaves it, which hangs her from the eyes: that would chase itself), 8 cm ahead of
            // its middle and 2 cm over it, not at her standing height over her feet.
            const Vec3 fwd{std::sin(game.yaw), 0.0f, std::cos(game.yaw)};
            head_pos = game_head + fwd * (0.08f * kUnitsPerMetre) + Vec3{0.0f, -0.02f * kUnitsPerMetre, 0.0f};
        }
        map.pre = Quat{};
        map.yaw = rig.yaw;
    } else if (mode == ViewMode::kCamera) {
        // At the game's camera: its place and heading, the head's turn and lean on top (its pitch and roll left out).
        Vec3 c;
        float cam_yaw;
        if (!cam || !camera_of(cam, c, cam_yaw)) return false;
        if (!rig.set) {
            rig.origin = fi.head.position;
            rig.neck0 = neck_of(fi.head);
            rig.cam_yaw_at_recentre = tracking_yaw(fi.head.orientation);
            rig.set = true;
        }
        const Quat back = yaw_quat(-rig.cam_yaw_at_recentre);
        const Vec3 rel = rotate(back, fi.head.position - rig.origin);
        head_pos = c + game_yaw(to_game(rel), cam_yaw) * kUnitsPerMetre;
        map.pre = back;
        map.yaw = cam_yaw;
    } else return false;
    map.head_t = fi.head.position;
    map.head_w = head_pos;
    map.valid = true;
    head_q = world_quat(fi.head.orientation);
    return true;
}

// LIST 48, dynamic cutscenes (the owner: "switch to 1st person in moments that make sense and where the character is
// rendered, and switch to 3rd person again in cutscenes automatically when shes out of frame"; "cutscenes look normal
// (her head shown)"). In a scene with "Cutscenes" at the camera: first person while the game's camera has her head in
// its frame (within about 35 degrees across and 29 up or down of its middle) at 1.5 to 20 m (vr144: the opening's wide
// shots hold her at 14-15 m, beyond 8 m, and stayed at the camera; past 20 m she is a speck); at the camera when it
// shows something else, or her face up close (under 1.5 m: her acting is the shot, and her head shows). A change waits
// for 0.5 s of the new state and each view holds at least 1.5 s, so a scene's quick cuts do not flicker the view.
// Session 10 (vr160-vr162: the opening's bridge walk put her eyes right behind two fence posts, 14 cm off, for seconds):
// the world that near before her eyes (view_blocked, the depth's probes straight ahead, under 20 cm) is no place to
// watch from: the game's camera at once, and kept until her head has moved 35 cm from where it was.
bool scene_first_person() {
    static bool fp = false;
    static float want_for = 0.0f, held = 0.0f;
    static int last_frame = -10;
    static bool near_known = false;
    static Vec3 near_at{};
    if (rt_frame - last_frame > 2) fp = false, want_for = 0.0f, held = 1.5f, near_known = false;  // a new scene: camera
    last_frame = rt_frame;
    const float dt = frame_dt();
    held += dt;
    bool sense = false;
    float d = 0.0f, cx = 0.0f, cy = 0.0f, cz = 0.0f;
    const bool fresh = game_head_frame >= rt_frame - 3;
    if (game.player_valid && game.cam_valid && fresh) {
        const float* v = game.cam_view;  // D3D row vectors: camera space = (p, 1) V
        const Vec3 h = game_head;  // as the scene poses her (in first person VR turns her head)
        cx = h.x * v[0] + h.y * v[4] + h.z * v[8] + v[12];
        cy = h.x * v[1] + h.y * v[5] + h.z * v[9] + v[13];
        cz = h.x * v[2] + h.y * v[6] + h.z * v[10] + v[14];
        d = std::sqrt(cx * cx + cy * cy + cz * cz) / kUnitsPerMetre;
        const bool in_frame = cz > 0.0f && std::fabs(cx) < cz * 0.70f && std::fabs(cy) < cz * 0.55f;
        sense = in_frame && d >= 1.5f && d <= 20.0f;
    }
    // the world under 20 cm ahead for 6 frames running (a post meets one probe; one stray pixel does not last)
    static int near_frames = 0;
    int seen = 0, behind = 0;
    float near = -1.0f;
    if (fp && fresh && view_mode == ViewMode::kFirstPerson) near = view_blocked(&seen, &behind);
    near_frames = near > 0.0f && near < 0.20f ? near_frames + 1 : 0;
    if (near_frames >= 6 && !near_known) {
        near_known = true, near_at = game_head;
        rt_log("VR: frame %d: dynamic cutscenes: the world %.0f cm before her eyes (%d of %d probes behind it): the "
               "game's camera until she moves", rt_frame, near * 100.0f, behind, seen);
    }
    if (near_known && fresh && length(game_head - near_at) > 0.35f * kUnitsPerMetre) near_known = false;
    if (near_known) sense = false;
    // What it saw, every 2 s of a scene (session 9: vr139's opening scene never went to her eyes)
    static int told_at = -1000;
    if (rt_frame - told_at >= 240) {
        told_at = rt_frame;
        rt_log("VR: frame %d: dynamic cutscenes see: Heather %d, the game's camera %d, her head %s (%d frames old) at "
               "%.2f %.2f %.2f m in the camera's space, %.1f m away; before her eyes %.2f m (%d of %d probes behind "
               "the world; -1 not asked)%s: %s", rt_frame, game.player_valid, game.cam_valid, fresh ? "fresh" : "old",
               rt_frame - game_head_frame, cx / kUnitsPerMetre, cy / kUnitsPerMetre, cz / kUnitsPerMetre, d, near,
               behind, seen, near_known ? ", too near" : "", sense ? "her eyes" : "the camera");
    }
    want_for = sense != fp ? want_for + dt : 0.0f;
    if ((want_for >= 0.5f && held >= 1.5f) || (fp && near_known && want_for > 0.0f)) {
        fp = sense;
        want_for = held = 0.0f;
        rt_log("VR: frame %d: dynamic cutscenes: %s (her head %.1f m from the game's camera, %s)", rt_frame,
               fp ? "first person, the scene shows her" : "at the game's camera", d,
               near_known && !fp           ? "the world right before her eyes"
               : d > 0.0f && sense         ? "in its frame"
               : d > 0.0f && d < 1.5f      ? "up close in its frame"
                                           : "out of its frame");
    }
    return fp;
}

// LIST 5: one of the game's movies plays (src/game/movie.c): the view on the cinema screen, whatever the options.
bool movie_playing = false;
ViewMode pick_view_mode() {
    if (!active || !head_known()) return ViewMode::kFlat;
    if (movie_playing) return ViewMode::kScreen;  // LIST 5: the game's movies on the cinema screen
    // Seconds of play since the last frame of a scene (cutscene or scripted sequence); loading and menus neither count
    // nor reset it.
    static float since_scene = 0.0f;
    static bool scene_ended_in_eyes = false;  // LIST 48: the last scene frame was in first person
    if (game.cutscene || game.demo) since_scene = 0.0f;
    else if (game.playable) since_scene += frame_dt();
    if (game.cutscene || game.demo) {
        if (opt.cutscenes == 2) return ViewMode::kScreen;
        if (opt.cutscenes == 1 && game.player_valid) return ViewMode::kFirstPerson;
        if (opt.cutscenes == 0 && scene_first_person()) {  // Dynamic (3: always at the camera)
            scene_ended_in_eyes = true;
            return ViewMode::kFirstPerson;
        }
        scene_ended_in_eyes = false;
        return ViewMode::kCamera;
    }
    if (!game.playable) return ViewMode::kCamera;  // menus over the world: the world as the game's camera shows it
    // Checked on the PC rig (2026-11-24): the opening's scripted walk has 15 frames of play between some of its shots
    // (frames 1924 and 4144, after a load); first person waits for half a second of play after a scene, so the view
    // does not jump to Heather and back between them.
    // A scene that ended in her eyes (dynamic cutscenes) stays there.
    if (since_scene < 0.5f && opt.cutscenes != 1 && !scene_ended_in_eyes)
        return opt.cutscenes == 2 ? ViewMode::kScreen : ViewMode::kCamera;
    if (opt.first_person && !third_person && !auto_third && game.player_valid) return ViewMode::kFirstPerson;
    return ViewMode::kCamera;
}

// LIST 35: places that fight first person. The game's camera pulled far from Heather for a while in play (a fixed
// shot the game composes: a chase, a scripted look) is shown from that camera until it comes back to her.
void update_auto_third() {
    if (!opt.auto_third_person || !game.player_valid || !game.cam_valid || !game.playable || game.cutscene ||
        game.demo) {
        far_camera_seconds = 0;
        if (auto_third) {
            auto_third = false;
            rig.set = false;
            rt_log("VR: back to first person (the game's camera returned)");
        }
        return;
    }
    Vec3 c;
    float yaw;
    if (!camera_of(game.cam_view, c, yaw)) return;
    Vec3 d = c - Vec3{game.pos[0], game.pos[1], game.pos[2]};
    d.y = 0;
    const float dist = length(d) / kUnitsPerMetre;
    if (dist > 7.0f) far_camera_seconds += frame_dt();
    else if (dist < 4.5f) far_camera_seconds = std::min(far_camera_seconds, 0.0f) - frame_dt();
    if (!auto_third && far_camera_seconds > 1.5f) {
        auto_third = true;
        rig.set = false;
        rt_log("VR: the game's camera stands %.1f m from Heather: shown from it (Third person where the game needs it)",
               dist);
    } else if (auto_third && far_camera_seconds < -1.0f) {
        auto_third = false;
        rig.set = false;
        far_camera_seconds = 0;
        rt_log("VR: back to first person (the game's camera returned)");
    }
}

// ---------------------------------------------------------------- the eyes' corrections

void eye_projection(int eye, const M4& pg, M4& pe) {
    const float L = std::tan(fi.fov[eye][0]), R = std::tan(fi.fov[eye][1]);
    const float U = std::tan(fi.fov[eye][2]), D = std::tan(fi.fov[eye][3]);
    const float sx = pg.m[0][0] < 0 ? -1.0f : 1.0f;
    const float sy = pg.m[1][1] < 0 ? -1.0f : 1.0f;  // the game's camera is y down: its projection flips y
    pe = M4{};
    pe.m[0][0] = sx * 2.0f / (R - L);
    pe.m[0][2] = -(R + L) / (R - L) * (sx > 0 ? 1.0f : -1.0f);
    // y: the up tangent is -y/z on a y-down camera (sy < 0), y/z on a y-up one.
    pe.m[1][1] = sy * 2.0f / (U - D);
    pe.m[1][2] = -(U + D) / (U - D);
    for (int c = 0; c < 4; c++) {
        pe.m[2][c] = pg.m[2][c];  // the game's depth and w, as it maps them
        pe.m[3][c] = pg.m[3][c];
    }
}

void update_corrections() {
    corr_valid = false;
    if (!p_main_valid || !head_known() || view_mode == ViewMode::kScreen || view_mode == ViewMode::kFlat) return;
    const M4 pg = M4::from_d3d(p_main);
    M4 pg_inv;
    if (!invert(pg, pg_inv)) return;
    // The eyes' near plane: the game's own is far out for a camera behind her (logged once); in first person the eyes
    // clip at 5 cm, as Jak's do, so her arms, chest, skirt and legs are drawn up to the near fade (7-16 cm, dithered)
    // instead of being sliced off flat by the plane (session 6, runs vr51-vr55: forearms cut into spikes, nothing of her
    // below the collar looking down). The far plane stays the game's. The probes read depth by this projection.
    M4 pd = pg;
    {
        const float A = pg.m[2][2], B = pg.m[2][3];
        const bool plain = std::fabs(pg.m[2][0]) < 1e-6f && std::fabs(pg.m[2][1]) < 1e-6f &&
                           std::fabs(pg.m[3][0]) < 1e-6f && std::fabs(pg.m[3][1]) < 1e-6f &&
                           std::fabs(pg.m[3][2] - 1.0f) < 1e-5f && std::fabs(pg.m[3][3]) < 1e-6f;
        const float n = (plain && std::fabs(A) > 1e-9f) ? -B / A : -1.0f;
        const float f = (plain && std::fabs(1.0f - A) > 1e-9f) ? B / (1.0f - A) : -1.0f;
        static float logged_n = -2.0f;
        if (std::fabs(n - logged_n) > 0.5f) {
            logged_n = n;
            rt_log("VR: the game's camera clips at %.1f units (%.1f cm) near and %.0f units far%s", n,
                   n / kUnitsPerMetre * 100.0f, f, plain ? "" : " (not a plain projection: left as it is)");
        }
        const float want = 0.05f * kUnitsPerMetre;
        if (view_mode == ViewMode::kFirstPerson && n > want && f > n * 2.0f && std::isfinite(f)) {
            pd.m[2][2] = f / (f - want);
            pd.m[2][3] = -want * f / (f - want);
        }
    }
    const Quat hq = conjugate(fi.head.orientation);
    for (int e = 0; e < 2; e++) {
        // The eye relative to the head's centre, in the game camera's axes (x right, y down, z forward), game units.
        const Vec3 rel = rotate(hq, fi.eye[e].position - fi.head.position);
        const Vec3 t = to_game(rel) * kUnitsPerMetre;
        float r[3][3];
        quat_matrix(to_game(hq * fi.eye[e].orientation), r);
        M4 tv = M4::identity();
        for (int i = 0; i < 3; i++) {
            for (int j = 0; j < 3; j++) tv.m[i][j] = r[j][i];  // R^T
            tv.m[i][3] = -(r[0][i] * t.x + r[1][i] * t.y + r[2][i] * t.z);
        }
        M4 pe;
        eye_projection(e, pd, pe);
        eye_corr_m[e] = pe * tv * pg_inv;
        eye_corr_m[e].to_gl(eye_corr + 16 * e);
    }
    // depth = (a z + b) / (c z + e) for a point on the axis at distance z (game units)
    depth_abcd[0] = pd.m[2][2];
    depth_abcd[1] = pd.m[2][3];
    depth_abcd[2] = pd.m[3][2];
    depth_abcd[3] = pd.m[3][3];
    corr_valid = true;
}

// The projection's scales that cover both eyes (with a margin for the eyes beside the centre).
void update_wanted_fov() {
    float th = 0, tv = 0;
    for (int e = 0; e < 2; e++) {
        th = std::max({th, std::fabs(std::tan(fi.fov[e][0])), std::fabs(std::tan(fi.fov[e][1]))});
        tv = std::max({tv, std::fabs(std::tan(fi.fov[e][2])), std::fabs(std::tan(fi.fov[e][3]))});
    }
    if (th <= 0.1f || tv <= 0.1f) return;
    want_cot[0] = 1.0f / (th * 1.08f);
    want_cot[1] = 1.0f / (tv * 1.08f);
}

// ---------------------------------------------------------------- the panel

void panel_init() {
    glGenTextures(1, &panel_tex);
    glBindTexture(GL_TEXTURE_2D, panel_tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, panel_w, panel_h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glGenTextures(1, &panel_depth);
    glBindTexture(GL_TEXTURE_2D, panel_depth);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH24_STENCIL8, panel_w, panel_h, 0, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8, nullptr);
    glGenFramebuffers(1, &panel_fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, panel_fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, panel_tex, 0);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D, panel_depth, 0);
    glGenFramebuffers(1, &read_fbo);
    glGenFramebuffers(1, &draw_fbo);
}

void panel_clear() {
    glBindFramebuffer(GL_FRAMEBUFFER, panel_fbo);
    glDisable(GL_SCISSOR_TEST);
    glColorMask(1, 1, 1, 1);
    glDepthMask(1);
    glClearColor(0, 0, 0, 0);
    glClearDepthf(1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
}

// The panel in front of the head: placed at the recentre, and brought round gently when the head has turned far from
// it (LIST 11: "follows the view gently, recenters").
void place_panel(bool force) {
    if (!head_known()) return;
    const float head_yaw = tracking_yaw(fi.head.orientation);
    float panel_yaw = panel_placed ? tracking_yaw(panel_pose.orientation) : head_yaw;
    const float diff = wrap(head_yaw - panel_yaw);
    if (force || !panel_placed) {
        panel_yaw = head_yaw;
        panel_turning = false;
    } else if (opt.panel_follow) {
        if (std::fabs(diff) > 0.6f) panel_turning = true;
        if (panel_turning) {
            const float step = 3.0f * frame_dt();
            if (std::fabs(diff) <= step || std::fabs(diff) < 0.04f) {
                panel_yaw = head_yaw;
                panel_turning = false;
            } else panel_yaw = wrap(panel_yaw + (diff > 0 ? step : -step));
        }
    } else return;
    const Quat q = yaw_quat(panel_yaw);
    const Vec3 ahead = rotate(q, {0, 0, -opt.panel_distance});
    Vec3 pos = fi.head.position + ahead;
    pos.y = fi.head.position.y - 0.1f;  // a little below the eyes
    if (panel_placed && !force) pos.y = panel_pose.position.y + (pos.y - panel_pose.position.y) * 0.05f;
    panel_pose = {pos, q};
    panel_placed = true;
}

// ---------------------------------------------------------------- the VR UI (session 11, owner messages 42-44)
// 42: "find a way to make the text appear directly in front of said object, as smooth as possible, and rendered
// properly in view in vr"; 43: "said object/item/person/whatever really"; 44: "do the same for inventory as well and
// menus in general. it should be projected in front of the screen, with ability to use pointer ... smooth vr ui in
// general". The panel's picture reaches the headset through ui_draw (a shader, not a blit): faded in and out (a copy
// kept for the fade out); for the game's messages in play and the subtitles in scenes only the box its text covers
// (sh3vr_panel_box), letterboxed in the picture with a soft dark backing, so the layer stands small by the thing the
// message is about (where a hand closed on it; with the button, ahead of the eyes) or low in the view. Screens (menus,
// the inventory, the map, memos, puzzles, the VR menu) open in front of the eyes ("Menus open in front of you") and
// stay put while the pointer works on them; "Screen follows the view" glides them round when you turn far away.
enum class UiKind { kNone, kHud, kScreen, kMessage, kSubtitle };
struct UiState {
    UiKind kind = UiKind::kNone;
    float fade = 0.0f;
    float box[4] = {0, 0, 1, 1};  // the shown part of the picture (0..1 of the screen, top row first)
    bool opaque = false;          // the VR menu's canvas: its colour shown, the layer's alpha the fade
    Pose pose;                    // a message's or subtitle's layer (tracking)
    float width = 1.0f;           // that layer's width (m); its height is the picture's 3/4
    Vec3 thing;                   // a message's thing, a subtitle's spot (tracking)
    float away = 0.0f;            // seconds a message or subtitle has been far off the view
    float screen_away = 0.0f;     // ... a screen
    bool gliding = false;         // a screen coming round in front again
    int content_frame = -100;     // the last frame something was drawn on the panel
} ui;
float frame_box[4] = {2.0f, 2.0f, -1.0f, -1.0f};  // this frame's 2D on the panel (sh3vr_panel_box)
// The frame camera drew the world this frame (sh3vr_frame_world); out of play without it, the frame is one of the
// game's full screens (the inventory, the map, memos, menus without the world): its picture whole on the panel
// (d3d8.c vr_route), shown opaque, the eyes dark round it.
bool world_drawn = true;
bool full_screen_frame = false;
GLuint ui_prog = 0, ui_vao = 0, ui_tex = 0, ui_fbo = 0;
GLint ui_u_dst = -1, ui_u_src = -1, ui_u_fade = -1, ui_u_back = -1, ui_u_opaque = -1;
bool ui_gl_failed = false;

const char* kUiVs =
    "#version 300 es\nprecision highp float;\nuniform vec4 uDst;\nout vec2 vT;\n"
    "void main() {\n"
    "    vec2 k = vec2(float(gl_VertexID & 1), float(gl_VertexID >> 1));\n"
    "    vT = vec2(k.x, 1.0 - k.y);\n"
    "    gl_Position = vec4(mix(uDst.xy, uDst.zw, k), 0.0, 1.0);\n"
    "}\n";
// premultiplied out (the layer blends by the picture's alpha, premultiplied): the game's 2D as drawn, a soft dark
// backing under a message's text (uBack), all times the fade
const char* kUiFs =
    "#version 300 es\nprecision highp float;\nuniform sampler2D uTex;\nuniform vec4 uSrc;\n"
    "uniform float uFade, uBack, uOpaque;\nin vec2 vT;\nout vec4 oColor;\n"
    "void main() {\n"
    "    vec4 t = texture(uTex, mix(uSrc.xy, uSrc.zw, vT));\n"
    "    if (uOpaque > 0.5) { oColor = vec4(t.rgb, 1.0) * uFade; return; }\n"
    "    vec2 e = min(vT, 1.0 - vT);\n"
    "    float b = uBack * smoothstep(0.0, 0.06, e.x) * smoothstep(0.0, 0.18, e.y);\n"
    "    oColor = vec4(t.rgb, t.a + b * (1.0 - t.a)) * uFade;\n"
    "}\n";

bool ui_gl() {
    if (ui_prog) return true;
    if (ui_gl_failed) return false;
    ui_prog = gl_link_program(kUiVs, kUiFs, "the UI's");
    if (!ui_prog) {
        ui_gl_failed = true;
        rt_log("VR: the UI's shader failed: the panel shown as before (no fades, no messages by their things)");
        return false;
    }
    ui_u_dst = glGetUniformLocation(ui_prog, "uDst");
    ui_u_src = glGetUniformLocation(ui_prog, "uSrc");
    ui_u_fade = glGetUniformLocation(ui_prog, "uFade");
    ui_u_back = glGetUniformLocation(ui_prog, "uBack");
    ui_u_opaque = glGetUniformLocation(ui_prog, "uOpaque");
    GLint prog = 0;
    glGetIntegerv(GL_CURRENT_PROGRAM, &prog);
    glUseProgram(ui_prog);
    glUniform1i(glGetUniformLocation(ui_prog, "uTex"), 0);
    glUseProgram((GLuint)prog);
    GLint vao = 0;
    glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &vao);
    glGenVertexArrays(1, &ui_vao);
    glBindVertexArray((GLuint)vao);
    GLint tex = 0;
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &tex);
    glGenTextures(1, &ui_tex);
    glBindTexture(GL_TEXTURE_2D, ui_tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, panel_w, panel_h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, (GLuint)tex);
    glGenFramebuffers(1, &ui_fbo);
    rt_log("VR: the UI's pass ready (menus open in front of you %d, messages next to things %d)", opt.menus_in_view,
           opt.messages_at_things);
    return true;
}

// Owner 81 (session 16: "lets be sure we dont miss any other QOL things the pcvr mod does already"): the PC VR mod's
// picture settings (its sh3vr_host64.cpp ApplyImageCorrection: ExposureStops, Saturation, Contrast about 0.18,
// VignetteStrength; and EnableFXAA, on there by default). "Brightness", "Contrast", "Color saturation", "Darker edges"
// and "Smooth edges" (View): each eye's picture drawn into the headset's image through this pass in place of the blit
// when any is off the game's own look (the game's values are sRGB-like, so the light is worked on at their power 2.2).
GLuint pic_prog = 0, pic_vao = 0, pic_sampler = 0;
GLint pic_u_layer = -1, pic_u_adj = -1, pic_u_px = -1;
bool pic_gl_failed = false;

const char* kPicVs =
    "#version 300 es\nprecision highp float;\nout vec2 vT;\n"
    "void main() {\n"
    "    vec2 k = vec2(float(gl_VertexID & 1), float(gl_VertexID >> 1));\n"
    "    vT = vec2(k.x, 1.0 - k.y);\n"  // the game's pictures are top row first, the swapchain's bottom row first
    "    gl_Position = vec4(k * 2.0 - 1.0, 0.0, 1.0);\n"
    "}\n";
// uAdj: brightness (a multiple of the light), contrast, saturation, darker edges (0..1); uPx: a texel (1/w, 1/h),
// smooth edges (1). The smoothing: FXAA (the console version: the edge's way from the four diagonal neighbours' luma,
// two or four taps along it, the four kept only while they stay in the neighbourhood's range).
const char* kPicFs =
    "#version 300 es\nprecision highp float;\nprecision highp sampler2DArray;\n"
    "uniform sampler2DArray uTex;\nuniform float uLayer;\nuniform vec4 uAdj;\nuniform vec4 uPx;\nin vec2 vT;\n"
    "out vec4 oColor;\n"
    "vec3 at(vec2 t) { return texture(uTex, vec3(t, uLayer)).rgb; }\n"
    "float luma(vec3 c) { return dot(c, vec3(0.299, 0.587, 0.114)); }\n"
    "void main() {\n"
    "    vec3 c = at(vT);\n"
    "    if (uPx.z > 0.5) {\n"
    "        float nw = luma(at(vT + vec2(-1.0, -1.0) * uPx.xy)), ne = luma(at(vT + vec2(1.0, -1.0) * uPx.xy));\n"
    "        float sw = luma(at(vT + vec2(-1.0, 1.0) * uPx.xy)), se = luma(at(vT + vec2(1.0, 1.0) * uPx.xy));\n"
    "        float m = luma(c);\n"
    "        float lo = min(m, min(min(nw, ne), min(sw, se))), hi = max(m, max(max(nw, ne), max(sw, se)));\n"
    "        vec2 d = vec2(-((nw + ne) - (sw + se)), (nw + sw) - (ne + se));\n"
    "        float reduce = max((nw + ne + sw + se) * 0.03125, 1.0 / 128.0);\n"
    "        d = clamp(d / (min(abs(d.x), abs(d.y)) + reduce), vec2(-8.0), vec2(8.0)) * uPx.xy;\n"
    "        vec3 a = 0.5 * (at(vT + d * (1.0 / 3.0 - 0.5)) + at(vT + d * (2.0 / 3.0 - 0.5)));\n"
    "        vec3 b = a * 0.5 + 0.25 * (at(vT - d * 0.5) + at(vT + d * 0.5));\n"
    "        float lb = luma(b);\n"
    "        c = lb < lo || lb > hi ? a : b;\n"
    "    }\n"
    "    vec3 l = pow(max(c, vec3(0.0)), vec3(2.2)) * uAdj.x;\n"
    "    l = mix(vec3(dot(l, vec3(0.2126, 0.7152, 0.0722))), l, uAdj.z);\n"
    "    l = max((l - 0.18) * uAdj.y + 0.18, vec3(0.0));\n"
    "    l *= 1.0 - uAdj.w * smoothstep(0.55, 1.0, length((vT - 0.5) * vec2(1.0, 0.9)) * 1.4142);\n"
    "    oColor = vec4(pow(clamp(l, vec3(0.0), vec3(1.0)), vec3(1.0 / 2.2)), 1.0);\n"
    "}\n";

// Whether the picture settings are off the game's own look (the pass needed).
bool picture_changed() {
    return opt.smooth_edges || std::fabs(opt.brightness - 100.0f) > 0.5f || std::fabs(opt.contrast - 100.0f) > 0.5f ||
           std::fabs(opt.saturation - 100.0f) > 0.5f || opt.dark_edges > 0.5f;
}

// Eye `layer` of the array `src` (w x h) into the bound draw framebuffer's tw x th image with the picture settings;
// false: not needed or not possible (the caller blits as before).
bool picture_pass(unsigned src, int layer, int w, int h, int tw, int th) {
    if (!picture_changed() || pic_gl_failed) return false;
    if (!pic_prog) {
        pic_prog = gl_link_program(kPicVs, kPicFs, "the picture's");
        if (!pic_prog) {
            pic_gl_failed = true;
            rt_log("VR: the picture's shader failed: the eyes' pictures as the game draws them (Brightness, Contrast, Color "
                   "saturation, Darker edges and Smooth edges left out)");
            return false;
        }
        pic_u_layer = glGetUniformLocation(pic_prog, "uLayer");
        pic_u_adj = glGetUniformLocation(pic_prog, "uAdj");
        pic_u_px = glGetUniformLocation(pic_prog, "uPx");
        GLint prog = 0, vao = 0;
        glGetIntegerv(GL_CURRENT_PROGRAM, &prog);
        glUseProgram(pic_prog);
        glUniform1i(glGetUniformLocation(pic_prog, "uTex"), 0);
        glUseProgram((GLuint)prog);
        glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &vao);
        glGenVertexArrays(1, &pic_vao);
        glBindVertexArray((GLuint)vao);
        glGenSamplers(1, &pic_sampler);
        glSamplerParameteri(pic_sampler, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glSamplerParameteri(pic_sampler, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glSamplerParameteri(pic_sampler, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glSamplerParameteri(pic_sampler, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }
    {
        static float told[5] = {-1, -1, -1, -1, -1};
        const float now[5] = {opt.brightness, opt.contrast, opt.saturation, opt.dark_edges, opt.smooth_edges ? 1.0f : 0.0f};
        if (std::memcmp(told, now, sizeof now)) {
            std::memcpy(told, now, sizeof now);
            rt_log("VR: frame %d: the eyes' pictures through the picture's pass: brightness %.0f%%, contrast %.0f%%, color "
                   "saturation %.0f%%, darker edges %.0f%%, smooth edges %d (%dx%d into %dx%d)", rt_frame, now[0], now[1],
                   now[2], now[3], opt.smooth_edges ? 1 : 0, w, h, tw, th);
        }
    }
    GLint prog = 0, vao = 0, tex = 0, active_tex = 0, sampler = 0;
    glGetIntegerv(GL_CURRENT_PROGRAM, &prog);
    glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &vao);
    glGetIntegerv(GL_ACTIVE_TEXTURE, &active_tex);
    glActiveTexture(GL_TEXTURE0);
    glGetIntegerv(GL_TEXTURE_BINDING_2D_ARRAY, &tex);
    glGetIntegerv(GL_SAMPLER_BINDING, &sampler);
    glViewport(0, 0, tw, th);
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_BLEND);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_STENCIL_TEST);
    glColorMask(1, 1, 1, 1);
    glUseProgram(pic_prog);
    glBindVertexArray(pic_vao);
    glBindTexture(GL_TEXTURE_2D_ARRAY, src);
    glBindSampler(0, pic_sampler);
    glUniform1f(pic_u_layer, (float)layer);
    const float adj[4] = {opt.brightness / 100.0f, opt.contrast / 100.0f, opt.saturation / 100.0f, opt.dark_edges / 100.0f};
    const float px[4] = {1.0f / (float)std::max(w, 1), 1.0f / (float)std::max(h, 1), opt.smooth_edges ? 1.0f : 0.0f, 0.0f};
    glUniform4fv(pic_u_adj, 1, adj);
    glUniform4fv(pic_u_px, 1, px);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glBindSampler(0, (GLuint)sampler);
    glBindTexture(GL_TEXTURE_2D_ARRAY, (GLuint)tex);
    glActiveTexture((GLenum)active_tex);
    glBindVertexArray((GLuint)vao);
    glUseProgram((GLuint)prog);
    return true;
}

// Turned so its face (+z) looks at the eyes.
Quat face_eyes(Vec3 at, Vec3 eyes) {
    const Vec3 d = eyes - at;
    const float h = std::sqrt(d.x * d.x + d.z * d.z);
    const float yaw = std::atan2(d.x, d.z), b = -std::atan2(d.y, std::max(h, 1e-4f));
    return yaw_quat(yaw) * Quat{std::sin(b * 0.5f), 0.0f, 0.0f, std::cos(b * 0.5f)};
}

Vec3 gaze() { return rotate(fi.head.orientation, {0.0f, 0.0f, -1.0f}); }

// Where a screen opens: along the gaze (its pitch kept between 29 degrees down and 17 up) at the screen's distance, a
// little under it, facing the eyes.
Pose screen_pose_in_view() {
    const float yaw = tracking_yaw(fi.head.orientation);
    const float pitch = std::clamp(std::asin(std::clamp(gaze().y, -1.0f, 1.0f)), -0.5f, 0.3f);
    const Vec3 dir{-std::sin(yaw) * std::cos(pitch), std::sin(pitch), -std::cos(yaw) * std::cos(pitch)};
    Vec3 at = fi.head.position + dir * opt.panel_distance;
    at.y -= 0.05f;
    return {at, face_eyes(at, fi.head.position)};
}

// Each frame before the game draws: a screen up stays where it opened (world-fixed: the pointer's target holds
// still); turned over 35 degrees from it, or a metre further off than it opened, for half a second it glides round in
// front again (with "Screen follows the view"). Everything else: the panel as before (place_panel).
void ui_place_frame() {
    if (!(ui.kind == UiKind::kScreen && opt.menus_in_view)) {
        ui.gliding = false;
        ui.screen_away = 0.0f;
        place_panel(false);
        return;
    }
    if (!opt.panel_follow || !head_known() || !panel_placed) return;
    const Vec3 to = panel_pose.position - fi.head.position;
    const float dist = std::max(length(to), 1e-3f);
    // over 35 degrees off the gaze (vr177: turned 40 degrees, half the screen was out of view; looking at a corner of
    // it is about 25), or a metre further off than it opened
    const float off = dot(to * (1.0f / dist), gaze());
    const bool far_off = off < 0.819f || dist > opt.panel_distance + 1.0f;
    ui.screen_away = far_off ? ui.screen_away + frame_dt() : 0.0f;
    if (ui.screen_away > 0.5f && !ui.gliding) {
        ui.gliding = true;
        rt_log("VR: frame %d: the screen comes round in front (%.0f degrees off the gaze, %.2f m off)", rt_frame,
               std::acos(std::clamp(off, -1.0f, 1.0f)) * 57.2958f, dist);
    }
    if (!ui.gliding) return;
    const Pose goal = screen_pose_in_view();
    const float k = 1.0f - std::exp(-8.0f * frame_dt());
    panel_pose.position = panel_pose.position + (goal.position - panel_pose.position) * k;
    panel_pose.orientation = face_eyes(panel_pose.position, fi.head.position);
    if (length(goal.position - panel_pose.position) < 0.02f) ui.gliding = false;
}

// A new kind of UI comes up: where it starts.
void ui_start(UiKind kind) {
    if (!head_known()) return;
    const Vec3 eyes = fi.head.position;
    const float yaw = tracking_yaw(fi.head.orientation);
    const Vec3 level{-std::sin(yaw), 0.0f, -std::cos(yaw)};
    ui.away = 0.0f;
    if (kind == UiKind::kScreen) {
        if (opt.menus_in_view) {
            panel_pose = screen_pose_in_view();
            panel_placed = true;
            panel_turning = false;
            ui.gliding = false;
            ui.screen_away = 0.0f;
        }
        rt_log("VR: frame %d: a screen up, %s", rt_frame,
               opt.menus_in_view ? "in front of the eyes (Menus open in front of you)" : "where the screen was");
    } else if (kind == UiKind::kMessage) {
        Vec3 w;
        const bool by_hand = recent_hand_use(w, 1.5f);
        // the thing a hand closed on; else (the button) where she looks, 0.9 m ahead, a little under the eyes
        ui.thing = by_hand ? tracking_point(w) : eyes + level * 0.9f + Vec3{0.0f, -0.12f, 0.0f};
        rt_log("VR: frame %d: the game's message %s (%.2f m from the eyes)", rt_frame,
               by_hand ? "by the thing the hand used" : "ahead of the eyes (the button)", length(ui.thing - eyes));
    } else if (kind == UiKind::kSubtitle) {
        ui.thing = eyes + level * 1.6f + Vec3{0.0f, -0.42f, 0.0f};
        rt_log("VR: frame %d: subtitles low in the view", rt_frame);
    }
}

// A message's or subtitle's layer each frame: by its thing (a label 10 cm over it, kept between 0.5 and 2.5 m from the
// eyes), facing the eyes; far off the view (over 45 degrees for a message, 30 for a subtitle) for a moment, it glides
// back into view at its distance. Its size: the text as large to the eye as on the screen at the screen's distance.
void ui_place_label(int pw, int ph) {
    const Vec3 eyes = fi.head.position;
    const bool sub = ui.kind == UiKind::kSubtitle;
    Vec3 at = ui.thing + Vec3{0.0f, sub ? 0.0f : 0.10f, 0.0f};
    Vec3 d = at - eyes;
    float dist = std::max(length(d), 1e-3f);
    const float want = std::clamp(dist, 0.5f, 2.5f);
    at = eyes + d * (want / dist);
    d = at - eyes;
    dist = want;
    const float c = dot(d * (1.0f / dist), gaze());
    ui.away = c < (sub ? 0.866f : 0.707f) ? ui.away + frame_dt() : 0.0f;
    if (ui.away > (sub ? 0.3f : 0.5f)) {
        const float yaw = tracking_yaw(fi.head.orientation);
        const float pitch = std::asin(std::clamp(gaze().y, -1.0f, 1.0f)) - (sub ? 0.26f : 0.08f);
        const Vec3 dir{-std::sin(yaw) * std::cos(pitch), std::sin(pitch), -std::cos(yaw) * std::cos(pitch)};
        const Vec3 goal = eyes + dir * dist - Vec3{0.0f, sub ? 0.0f : 0.10f, 0.0f};
        ui.thing = ui.thing + (goal - ui.thing) * (1.0f - std::exp(-4.0f * frame_dt()));
        if (length(goal - ui.thing) < 0.03f) ui.away = 0.0f;
    }
    ui.pose = {at, face_eyes(at, eyes)};
    // the text's width to the eye as on the screen: its share of the screen's width, at this distance
    const float text_w = (ui.box[2] - ui.box[0]) * opt.panel_width * dist / std::max(opt.panel_distance, 0.3f);
    const float bw = (ui.box[2] - ui.box[0]) * (float)panel_w, bh = (ui.box[3] - ui.box[1]) * (float)panel_h;
    const float a = bw / std::max(bh, 1.0f), pa = (float)pw / (float)std::max(ph, 1);
    const float fx = a >= pa ? 1.0f : a / pa;
    ui.width = text_w / std::max(fx, 0.05f);
}

// Draws the shown picture (ui_tex) into the panel's swapchain picture: whole for screens and the HUD, the text's box
// letterboxed for messages and subtitles.
void ui_draw(unsigned target, int pw, int ph) {
    const bool crop = ui.kind == UiKind::kMessage || ui.kind == UiKind::kSubtitle;
    float src[4] = {0.0f, 0.0f, 1.0f, 1.0f}, dst[4] = {-1.0f, -1.0f, 1.0f, 1.0f};
    if (crop) {
        for (int k = 0; k < 4; k++) src[k] = ui.box[k];
        const float bw = (ui.box[2] - ui.box[0]) * (float)panel_w, bh = (ui.box[3] - ui.box[1]) * (float)panel_h;
        const float a = bw / std::max(bh, 1.0f), pa = (float)pw / (float)std::max(ph, 1);
        const float fx = a >= pa ? 1.0f : a / pa, fy = a >= pa ? pa / a : 1.0f;
        dst[0] = -fx, dst[1] = -fy, dst[2] = fx, dst[3] = fy;
    }
    GLint prog = 0, vao = 0, tex = 0, active_tex = 0, sampler = 0;
    glGetIntegerv(GL_CURRENT_PROGRAM, &prog);
    glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &vao);
    glGetIntegerv(GL_ACTIVE_TEXTURE, &active_tex);
    glActiveTexture(GL_TEXTURE0);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &tex);
    glGetIntegerv(GL_SAMPLER_BINDING, &sampler);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, draw_fbo);
    glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, target, 0);
    glViewport(0, 0, pw, ph);
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_BLEND);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_STENCIL_TEST);
    glColorMask(1, 1, 1, 1);
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    glUseProgram(ui_prog);
    glBindVertexArray(ui_vao);
    glBindTexture(GL_TEXTURE_2D, ui_tex);
    glBindSampler(0, 0);
    glUniform4fv(ui_u_dst, 1, dst);
    glUniform4fv(ui_u_src, 1, src);
    glUniform1f(ui_u_fade, ui.fade * ui.fade * (3.0f - 2.0f * ui.fade));  // eased
    glUniform1f(ui_u_back, crop ? 0.38f : 0.0f);
    glUniform1f(ui_u_opaque, ui.opaque ? 1.0f : 0.0f);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glBindSampler(0, (GLuint)sampler);
    glBindTexture(GL_TEXTURE_2D, (GLuint)tex);
    glActiveTexture((GLenum)active_tex);
    glBindVertexArray((GLuint)vao);
    glUseProgram((GLuint)prog);
}

// The frame's UI, at the present (the game has drawn): what the panel's picture is, where it stands, how faded; drawn
// into `target` (0 when the headset skips drawing) and told to the headset through `panel`. False when the UI's
// shader is not there (the caller shows the panel as before).
bool ui_frame(unsigned menu_tex, unsigned target, int pw, int ph, PanelLayer& panel) {
    if (!ui_gl()) return false;
    const float dt = frame_dt();
    const bool drawn = panel_used || menu_tex;
    const bool box_ok = frame_box[2] > frame_box[0] && frame_box[3] > frame_box[1];
    UiKind want = UiKind::kNone;
    if (drawn) {
        const bool scene = game.cutscene || game.demo;
        if (menu_tex) want = UiKind::kScreen;
        else if (scene) want = box_ok && opt.messages_at_things ? UiKind::kSubtitle : UiKind::kHud;
        else if (!game.playable) want = UiKind::kScreen;
        else want = box_ok && opt.messages_at_things ? UiKind::kMessage : UiKind::kHud;
        ui.content_frame = rt_frame;
        if (want != ui.kind) {
            const bool shown = ui.kind != UiKind::kNone && ui.fade > 0.0f;
            // the HUD shows at once; the rest fades in (a kind changing while shown keeps its fade: no blink)
            ui.fade = want == UiKind::kHud ? 1.0f : shown ? ui.fade : 0.0f;
            ui.kind = want;
            ui_start(want);
        }
        // the picture kept for the fade out
        glBindFramebuffer(GL_READ_FRAMEBUFFER, read_fbo);
        glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, menu_tex ? menu_tex : panel_tex, 0);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, ui_fbo);
        glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, ui_tex, 0);
        glBlitFramebuffer(0, 0, panel_w, panel_h, 0, 0, panel_w, panel_h, GL_COLOR_BUFFER_BIT, GL_NEAREST);
        ui.opaque = menu_tex != 0 || (want == UiKind::kScreen && full_screen_frame);
        if (want == UiKind::kMessage || want == UiKind::kSubtitle) {
            // the text's box with a margin (its glyphs' edges and the backing's soft rim)
            ui.box[0] = std::max(0.0f, frame_box[0] - 0.02f);
            ui.box[1] = std::max(0.0f, frame_box[1] - 0.03f);
            ui.box[2] = std::min(1.0f, frame_box[2] + 0.02f);
            ui.box[3] = std::min(1.0f, frame_box[3] + 0.03f);
        }
        ui.fade = std::min(1.0f, ui.fade + dt / 0.15f);
    } else if (ui.kind != UiKind::kNone && rt_frame - ui.content_frame > 2) {
        // gone (two frames without it: not a frame the game skipped): fades out where it was
        ui.fade = std::max(0.0f, ui.fade - dt / 0.2f);
        if (ui.fade <= 0.0f) ui.kind = UiKind::kNone;
    }
    panel.visible = ui.kind != UiKind::kNone && ui.fade > 0.0f;
    panel.blend = true;
    if (ui.kind == UiKind::kMessage || ui.kind == UiKind::kSubtitle) {
        ui_place_label(pw, ph);
        panel.pose = ui.pose;
        panel.width = ui.width;
        panel.height = ui.width * (float)ph / (float)std::max(pw, 1);
    } else {
        panel.pose = panel_pose;
        panel.width = opt.panel_width;
        panel.height = opt.panel_width * 3.0f / 4.0f;
    }
    if (target && panel.visible) ui_draw(target, pw, ph);
    {
        static UiKind logged = UiKind::kNone;
        if (ui.kind != logged && ui.kind != UiKind::kNone) {
            static const char* const kNames[] = {"none", "the HUD", "a screen", "a message", "subtitles"};
            rt_log("VR: frame %d: the UI shows %s, %.2f x %.2f m at %.2f %.2f %.2f; the text's box %.3f %.3f %.3f %.3f",
                   rt_frame, kNames[(int)ui.kind], panel.width, panel.height, panel.pose.position.x,
                   panel.pose.position.y, panel.pose.position.z, ui.box[0], ui.box[1], ui.box[2], ui.box[3]);
        }
        logged = ui.kind;
    }
    return true;
}

// ---------------------------------------------------------------- input

bool pressed(bool now, bool before) { return now && !before; }

void update_pointer() {
    mouse_on = false;
    if (!opt.pointer || !head_known() || !panel_placed) return;
    // A screen to choose from is up: the game drew on the panel and is not in play (menus, inventory, map, puzzles),
    // or the VR menu.
    // (session 11: the UI's kind, so subtitles in a scene are not a screen to point at)
    const bool screen_up = vr_menu_open || ui.kind == UiKind::kScreen;
    if (!screen_up) {
        hovered_cell = -1;
        return;
    }
    const Controls& c = fi.controls;
    if (opt.pointer_hand == 0) {
        if (pressed(c.left_trigger > 0.6f, prev.left_trigger > 0.6f)) point_hand = 0;
        if (pressed(c.right_trigger > 0.6f, prev.right_trigger > 0.6f)) point_hand = 1;
    } else point_hand = opt.pointer_hand == 1 ? 0 : 1;
    if (!fi.hand_valid[point_hand]) return;
    float u, v;
    const float ph = opt.panel_width * 3.0f / 4.0f;
    if (!panel_point(panel_pose, opt.panel_width, ph, fi.hand_aim[point_hand], u, v)) return;
    // Tremor smoothing: still hands settle, quick moves follow at once; small steps (a menu's entries) settle more.
    const float du = u - mouse_u, dv = v - mouse_v;
    const float speed = std::sqrt(du * du + dv * dv);
    const float k = std::clamp(speed * 40.0f, 0.2f, 1.0f);
    mouse_u += du * k;
    mouse_v += dv * k;
    mouse_on = true;
    const float trig = point_hand ? c.right_trigger : c.left_trigger;
    mouse_left = trig > 0.6f;
    mouse_right = false;  // B backs out through the game's pad (map_pad; vr177: the inventory takes no right click)
    // A haptic tick for each step of the screen crossed (the game's menus are laid out on a 4:3 grid of 1/16ths
    // across, 1/24ths down: an entry's height), as the light snapping a hand feels.
    // Session 12: on a screen with click zones (the game's own, sh3.c: its entries, tabs and arrows), the tick comes on
    // entering one, a little firmer, so the hand feels what a pull would choose; off them nothing ticks.
    if (!vr_menu_open && game.click_zones > 0) {
        const int cell = 100000 + game.click_zone;
        if (hovered_cell >= 0 && cell != hovered_cell && game.click_zone) buzz(point_hand, 0.18f, 0.014f, 200.0f);
        hovered_cell = cell;
        return;
    }
    const int cell = (int)(mouse_u * 16.0f) * 64 + (int)(mouse_v * 24.0f);
    if (hovered_cell >= 0 && cell != hovered_cell) buzz(point_hand, 0.12f, 0.012f, 220.0f);
    hovered_cell = cell;
}

void show_pointer() {
    PointerLayers p;
    p.look = opt.beam_look;
    if (mouse_on) {
        const Pose& aim = fi.hand_aim[point_hand];
        const float ph = opt.panel_width * 3.0f / 4.0f;
        const Vec3 local{(mouse_u - 0.5f) * opt.panel_width, (0.5f - mouse_v) * ph, 0.002f};
        p.dot = true;
        p.dot_pose = {panel_pose.position + rotate(panel_pose.orientation, local), panel_pose.orientation};
        // Session 12: the dot grows by a third over something a pull can choose (the game's click zones), easing in
        // and out over about 0.1 s, as the tick does in the hand.
        static float dot_scale = 1.0f;
        const float want = !vr_menu_open && game.click_zones > 0 && game.click_zone ? 1.35f : 1.0f;
        dot_scale += (want - dot_scale) * std::min(1.0f, frame_dt() * 12.0f);
        p.dot_scale = dot_scale;
        float len = 0;
        // The beam's quad runs from the hand to the dot, turned to face the eyes (pointer.h beam_pose).
        if (beam_pose(aim.position, p.dot_pose.position, fi.head.position, p.beam_pose, len)) {
            p.beam = true;
            p.beam_length = len;
        }
    }
    hs->show_pointer(p);
}

// The guide dot (LIST 25): where the gun's ray met the world (sh3vr_body.cpp's probes), a small dot the same size to
// the eye at any distance.
void show_guide_dot() {
    CrosshairLayer c;
    Vec3 at;
    if (opt.laser && view_mode == ViewMode::kFirstPerson && in_play() && guide_hit(at)) {
        const Vec3 to = at - fi.head.position;
        const float dist = length(to);
        if (dist > 0.2f) {
            c.shown = true;
            c.pose = {at, looking_along(normalized(to))};
            c.distance = dist;
            c.size = dist * 0.012f;  // ~0.7 degrees
            c.kind = 7;              // a small circle
            c.color = 4;             // red
            c.opacity = 90;
        }
    }
    hs->show_crosshair(c);
}

// LIST 49 (the owner: the health bar "on top of her wrist where her watch is", shown when looked at, fading): a 5 cm
// bar across her watch's face on her left wrist (watch_place), shown while the watch's face turns to the eyes
// (within 50 degrees) and the eyes look at it (within 35 degrees, nearer than 90 cm), fading in and out over about
// 0.15 s. Its fill is her health (the game's HP over its most); under half it pulses as the game's own Life Display
// mark does (0x5F1C40: brightness (sin(t / P * pi / 20) + 1) / 2, t in ms, P 50, 25 or 10 under 50, 25 or 10
// percent: one beat each 2 s, 1 s, 0.4 s). On with "Health bar on your wrist", in first person.
// Owner message 37 (session 10): her stamina as a second bar under it. The game's Life Gauge option (0x70E6724) has its
// own bars left out of the eyes (sh3.c 0x5A5E40: its packets cannot be routed): owner messages 39-40, the watch has
// them in first person (no bars at the top left then); third person is the game's normal HUD, so its gauge stands at
// the view's top left (VR's quad, the same bars, where the game draws them), 1.3 m off, following the view gently: 22
// degrees left and 16 up, 15 degrees wide (vr157: 15 left, 11 up, 0.26 m sat near the middle and small); back in first
// person, the watch again. With the wrist's option off, first person is the normal HUD too.
constexpr float kGaugeAhead = 1.3f, kGaugeLeft = 0.53f, kGaugeUp = 0.37f, kGaugeWide = 0.34f;
void show_wrist_bar() {
    static float shown = 0.0f;
    static double clock_ms = 0.0;
    static Vec3 gauge_way{0, 0, -1};
    static bool gauge_was = false;
    clock_ms += frame_dt() * 1000.0;
    WristLayer w;
    const int h = kLeft;
    // after half a second of play (vr159: the opening's scene has single frames of play between its shots, 4061 and
    // 4155, and the top left's bars came for one frame each), as first person waits after a scene (pick_view_mode)
    static float play_for = 0.0f;
    play_for = in_play() ? play_for + frame_dt() : 0.0f;
    const bool bars = game.player_valid && game.hp_max > 0.0f && std::isfinite(game.hp) && play_for >= 0.5f;
    const bool fp = view_mode == ViewMode::kFirstPerson, tp = view_mode == ViewMode::kCamera;
    // Owner messages 39-40 (session 10, vr152's picture of both at once): first person with "Health bar on your wrist"
    // on (the default): the watch, never the top left; third person, or the option off: the game's normal HUD.
    const bool watch_view = fp && opt.wrist_health;
    const bool gauge = !watch_view && game.life_gauge && (fp || tp) && bars && head_known();
    const bool can = watch_view && bars && hands[h].valid;
    bool looked = false;
    float faces = -2, look = -2, d = -1;
    Vec3 at, along, back;
    if (gauge) {
        // the view's way, eased (a quarter second), level of roll: the bars stay put through small head moves
        const Vec3 gaze = rotate(fi.head.orientation, Vec3{0, 0, -1});
        gauge_way = gauge_was ? normalized(gauge_way + (gaze - gauge_way) * std::clamp(frame_dt() * 4.0f, 0.0f, 1.0f))
                              : gaze;
        const Quat q = looking_along(gauge_way);
        w.pose = {fi.head.position + rotate(q, Vec3{-kGaugeLeft, kGaugeUp, -kGaugeAhead}), q};
        looked = true;
    }
    gauge_was = gauge;
    if (can && watch_place(h, at, along, back)) {
        const Pose& hp = hands[h].shown;
        const Vec3 c = hp.position + rotate(hp.orientation, at);
        Vec3 z = normalized(rotate(hp.orientation, back));
        Vec3 x = normalized(rotate(hp.orientation, cross(along, back)));  // across her wrist, as a watch's face reads
        const Vec3 to_eye = fi.head.position - c;
        d = length(to_eye);
        if (d > 0.05f) {
            const Vec3 gaze = rotate(fi.head.orientation, Vec3{0, 0, -1});
            faces = dot(z, to_eye * (1.0f / d)), look = dot(gaze, to_eye * (-1.0f / d));
            looked = faces > 0.643f && look > 0.819f && d < 0.9f;  // cos 50, cos 35
            // read left to right whichever way the arm lies: the bar's start toward the eyes' left
            if (dot(x, rotate(fi.head.orientation, Vec3{1, 0, 0})) < 0.0f) x = x * -1.0f;
            const Vec3 y = normalized(cross(z, x));
            x = cross(y, z);
            w.pose = {c + z * 0.006f, quat_from_axes(x, y, z)};  // 6 mm off the skin
        }
    }
    shown += ((looked ? 1.0f : 0.0f) - shown) * std::clamp(frame_dt() * 12.0f, 0.0f, 1.0f);
    if (!can && !gauge) shown = 0.0f;  // a menu, a cutscene, third person without the game's gauge: gone at once
    static bool watch_was = false;
    if (can && !watch_was) shown = 0.0f;  // from the top left back to the watch: it fades in from nothing when looked at
    watch_was = can;
    if ((can || gauge) && shown >= 0.01f) {
        const float health = std::clamp(game.hp / game.hp_max, 0.0f, 1.0f);
        const float pct = health * 100.0f;
        const float period = pct < 10.0f ? 10.0f : pct < 25.0f ? 25.0f : pct < 50.0f ? 50.0f : 0.0f;
        w.shown = true;
        w.width = gauge ? kGaugeWide : kWristMetresWide;
        w.height = gauge ? kGaugeWide * kWristMetresHigh / kWristMetresWide : kWristMetresHigh;
        w.health = health;
        w.stamina = std::isfinite(game.stamina) ? std::clamp(game.stamina, 0.0f, 1.0f) : 1.0f;
        w.alpha = shown;
        w.pulse = period > 0.0f
                      ? static_cast<float>((std::sin(std::fmod(clock_ms / period * 0.15708, 6.283185307)) + 1.0) * 0.5)
                      : 1.0f;
    }
    // Why it is or is not there, every 2 s in either view (vr139: her watch faced the eyes and no bar came); with
    // SH3_WRIST_LOG (the PC rig) every 15 frames, with the watch's place and way and the eyes'.
    static int told_at = -1000;
    static const int tell_every = std::getenv("SH3_WRIST_LOG") ? 15 : 240;
    if ((fp || (tp && in_play())) && rt_frame - told_at >= tell_every) {
        told_at = rt_frame;
        rt_log("VR: frame %d: the wrist's bar: can %d (%s, option %d, Life Display %d, Life Gauge %d, at the top left "
               "%d, in play %d, stamina %.2f, Heather %d, health %.0f of %.0f, hand %d); the back of the wrist faces the "
               "eyes %.2f (from 0.64), the eyes on it %.2f (from 0.82), %.2f m (to 0.9); shown %.2f",
               rt_frame, can ? 1 : 0, fp ? "first person" : "third person", opt.wrist_health ? 1 : 0, game.life_display,
               game.life_gauge, gauge ? 1 : 0, in_play() ? 1 : 0, game.stamina,
               game.player_valid, game.hp, game.hp_max, hands[h].valid ? 1 : 0, faces, look, d, shown);
        if (tell_every < 240 && can && watch_place(h, at, along, back)) {
            const Pose& hp = hands[h].shown;
            const Vec3 c = hp.position + rotate(hp.orientation, at), z = rotate(hp.orientation, back);
            const Vec3 gaze = rotate(fi.head.orientation, Vec3{0, 0, -1});
            rt_log("VR: frame %d: the watch at %.3f %.3f %.3f (the hand's %.3f %.3f %.3f; in its grip %.3f %.3f %.3f), "
                   "out of the wrist %.2f %.2f %.2f (grip %.2f %.2f %.2f), along %.2f %.2f %.2f; the eyes at %.3f %.3f "
                   "%.3f looking %.2f %.2f %.2f", rt_frame, c.x, c.y, c.z, hp.position.x, hp.position.y, hp.position.z,
                   at.x, at.y, at.z, z.x, z.y, z.z, back.x, back.y, back.z, along.x, along.y, along.z,
                   fi.head.position.x, fi.head.position.y, fi.head.position.z, gaze.x, gaze.y, gaze.z);
        }
    }
    static int was = 0;  // 0 nothing, 1 the watch, 2 the top left
    const int now = !w.shown ? 0 : gauge ? 2 : 1;
    if (now != was) {
        was = now;
        if (now)
            rt_log("VR: frame %d: the %s shown (%s; health %.0f of %.0f, stamina %.2f)", rt_frame,
                   gauge ? "game's Life Gauge bars at the view's top left" : "wrist's health bar",
                   !gauge ? "first person" : fp ? "first person, the wrist's option off"
                                                : "third person: the game's normal HUD",
                   game.hp, game.hp_max, game.stamina);
    }
    hs->show_wrist(w);
}

// The controllers as the pad (bindings: sh3.c's VR defaults, savedata/keyvr.ini; docs/CONTROLS.md).
enum PadButton { kX = 0, kA, kB, kY, kLB, kRB, kLT, kRT, kBack, kStart, kLS, kRS };

void map_pad() {
    const Controls& c = fi.controls;
    Sh3VrPad p{};
    auto set = [&p](int b, bool on) { if (on) p.buttons |= (uint16_t)(1u << b); };
    const bool play = game.playable && !game.cutscene && !game.demo;
    const bool fp = view_mode == ViewMode::kFirstPerson;
    const int mh = main_hand();
    const float main_trigger = mh == kRight ? c.right_trigger : c.left_trigger;
    const float off_trigger = mh == kRight ? c.left_trigger : c.right_trigger;
    const float main_grip = mh == kRight ? c.right_grip : c.left_grip;
    const float off_grip = mh == kRight ? c.left_grip : c.right_grip;

    // The left menu button: a short press is the pause (and skip; closes the VR menu); held a second, the VR menu,
    // everywhere (owner 80, session 16: "i actually like it being the vr menu, so leave that as is, or adjust it so it
    // works like that going forward": it had been first/third person in play). In play the game is paused first, its
    // pause under the menu as in its menus; cutscenes, menus and the title as before. First/third person is the menu's
    // "First person" switch (View).
    if (c.menu) {
        menu_held_frames++;
        if (!menu_long_done && menu_held_frames >= (int)(display_hz > 0 ? display_hz : 72)) {
            menu_long_done = true;
            if (vr_menu_open) menu_close();
            else {
                if (play && !movie_playing) pause_pulse = 4;  // the game's pause (sent while the menu is up, below)
                menu_open(play ? "the left menu button held in play: the game paused under it"
                          : game.cutscene ? "the left menu button held in a cutscene"
                                          : "the left menu button held in a menu");
            }
        }
    } else {
        if (menu_held_frames > 0 && !menu_long_done) {
            if (vr_menu_open) menu_close();
            else pause_pulse = 4;
        }
        menu_held_frames = 0;
        menu_long_done = false;
    }

    if (vr_menu_open) {
        // The VR menu has the controllers; the game gets none of them.
        const float trig = point_hand ? c.right_trigger : c.left_trigger;
        menu_update(c, mouse_on, mouse_u, mouse_v, trig > 0.6f);
        // (owner 80: the game's pause asked for as the menu opened in play still reaches it)
        if (pause_pulse > 0) {
            set(kBack, true);
            pause_pulse--;
        }
        pad = p;
        return;
    }
    // A movie (LIST 5): the game skips it on its pause button (0x4DD4A0(2), the left menu button's short press); A, B
    // and either trigger skip it too, as players reach for them (session 12, vr181: B alone left it playing).
    if (movie_playing && pause_pulse == 0 &&
        ((c.a && !prev.a) || (c.b && !prev.b) || (c.right_trigger > 0.6f && prev.right_trigger <= 0.6f) ||
         (c.left_trigger > 0.6f && prev.left_trigger <= 0.6f))) {
        pause_pulse = 4;
        rt_log("VR: frame %d: a button skips the movie (the game's skip on its pause button)", rt_frame);
    }
    if (pause_pulse > 0) {
        set(kBack, true);
        pause_pulse--;
    }

    // Buttons
    const bool pointing = mouse_on;
    set(kA, c.a);
    // B and Y are the game's own buttons, pointing or not (session 11, vr177: B as the mouse's right button left the
    // inventory open: it takes no right click; the game's cancel backs out of every screen).
    (void)pointing;
    set(kB, c.b);
    set(kY, c.x);      // map
    set(kStart, c.y);  // inventory
    set(kX, c.left_click);                               // dash (run)
    set(kLT, c.right_click);                             // search view
    if (play) {
        set(kLS, off_trigger > 0.6f);                    // flashlight
        uint16_t add = 0;
        bool block_attack = false;
        if (fp) combat_pad(p, add, block_attack);
        // The main trigger readies the weapon and fires (readied while held, the attack from the next frame). In third
        // person the main grip readies it too (the game's own aim); in first person the grip holds, draws and grabs.
        const bool trig = main_trigger > 0.6f && !block_attack;
        fire_frames = trig ? fire_frames + 1 : 0;
        // Quick gun raise (session 6): a gun's pull is kept until its shot (1 s at most), so a quick tap while the
        // last shot recovers is not lost (run vr58: a pull 26 frames after a shot, released before the recovery's
        // end, never fired); the game's raise and the shot's start run faster meanwhile (sh3.c, sh3vr_quick_raise).
        const bool gun = fp && opt.quick_raise && combat_gun_in_hand();
        if (fire_frames == 1 && gun) {
            pull_left = 1.0f;
            pull_shot = false;
        }
        if (!gun || block_attack) pull_left = 0;
        const bool kept = !pull_shot && pull_left > 0;
        pull_left -= frame_dt();
        set(kRT, trig || kept || (!fp && main_grip > 0.6f));
        if ((fire_frames >= 2 || (kept && !trig)) && !(fp && combat_trigger_readies())) set(kA, true);
        // Third person: the off grip with the stick strafes (the game's own strafe buttons).
        if (!fp && off_grip > 0.6f) {
            set(kLB, c.left_x < -0.5f);
            set(kRB, c.left_x > 0.5f);
        }
        p.buttons |= add;
    } else {
        fire_frames = 0;
        set(kLS, c.left_trigger > 0.6f && !pointing);
        set(kRS, c.right_grip > 0.6f);  // the map's zoom
        if (!pointing) set(kA, c.a || (c.right_trigger > 0.6f && prev.right_trigger <= 0.6f));
    }

    // Sticks
    const float dead = 0.15f;
    auto dz = [dead](float v) { return std::fabs(v) < dead ? 0.0f : v; };
    const float lx = dz(c.left_x), ly = dz(c.left_y);
    if (!play) {
        // Menus: the left stick is the d-pad and the stick; the right stick its own.
        p.lx = lx;
        p.ly = -ly;
        p.rx = dz(c.right_x);
        p.ry = -dz(c.right_y);
        p.dpad_x = (int8_t)(lx > 0.5f ? 1 : lx < -0.5f ? -1 : 0);
        p.dpad_y = (int8_t)(ly > 0.5f ? -1 : ly < -0.5f ? 1 : 0);
    } else if (fp) {
        // First person: VR steers (sh3vr_move: the stick, or the head's walk in the room); the game sees the stick
        // forward by that much, so it walks or runs.
        p.ly = move.steer ? -move.speed : 0.0f;
    } else {
        // Third person: the game's own control (strafing with the off grip held, above).
        if (off_grip > 0.6f) p.ly = -ly;
        else {
            p.lx = lx;
            p.ly = -ly;
        }
        p.rx = dz(c.right_x);
        p.ry = -dz(c.right_y);
    }
    pad = p;
}

void turn_rig(float angle) {
    // Turn about the neck so it stays where it is in the world (the eyes swing round it as a turning body takes them).
    if (!head_known()) return;
    Vec3 v = fi.head.position - rig.origin;
    v.y = 0;
    const Vec3 g = game_yaw(to_game(v), -angle);
    const Vec3 back = to_game(g);
    rig.origin.x = fi.head.position.x - back.x;
    rig.origin.z = fi.head.position.z - back.z;
    const Vec3 nk = neck_of(fi.head);
    Vec3 vn = nk - rig.neck0;
    vn.y = 0;
    const Vec3 bn = to_game(game_yaw(to_game(vn), -angle));
    rig.neck0.x = nk.x - bn.x;
    rig.neck0.z = nk.z - bn.z;
    rig.yaw = wrap(rig.yaw + angle);
}

bool turning_now = false;

void comfort_turn() {
    turning_now = false;
    if (view_mode != ViewMode::kFirstPerson || !in_play()) return;
    const float x = fi.controls.right_x;
    if (opt.turn == 0) {
        if (std::fabs(x) < 0.3f) snap_ready = true;
        else if (snap_ready && std::fabs(x) > 0.7f) {
            snap_ready = false;
            turn_rig((x > 0 ? 1.0f : -1.0f) * opt.snap_degrees * kPi / 180.0f);
        }
    } else if (std::fabs(x) > 0.15f) {
        turn_rig(x * opt.smooth_degrees * kPi / 180.0f * motion_dt());  // 0.1.3: degrees a real second
        turning_now = true;
    }
}

// What Heather is asked to do this frame.
void plan_move() {
    move = Sh3VrMove{};
    if (view_mode != ViewMode::kFirstPerson || !game.player_valid || !in_play() || !head_known()) {
        prev_pos_valid = false;
        catching_up = false;
        blocked_seconds = 0;
        return;
    }
    const Controls& c = fi.controls;
    const float lx = std::fabs(c.left_x) < 0.15f ? 0.0f : c.left_x, ly = std::fabs(c.left_y) < 0.15f ? 0.0f : c.left_y;
    const float len = std::min(1.0f, std::sqrt(lx * lx + ly * ly));
    const Vec3 here{game.pos[0], game.pos[1], game.pos[2]};

    // Heather moved since last frame: while she was catching up with the head, the head stays where it is in the world.
    if (prev_pos_valid) {
        Vec3 d = here - prev_pos;
        d.y = 0;
        if (length(d) > 2.0f * kUnitsPerMetre) rig.set = false;  // a door, a load: start again at her
        else if (catching_up) {
            const Vec3 t = to_game(game_yaw(d, -rig.yaw)) * (1.0f / kUnitsPerMetre);
            rig.origin.x += t.x;
            rig.origin.z += t.z;
            rig.neck0.x += t.x;
            rig.neck0.z += t.z;
        }
    }
    prev_pos = here;
    prev_pos_valid = true;
    const bool was_catching_up = catching_up;
    catching_up = false;

    float view_heading = rig.yaw - tracking_yaw(fi.head.orientation);
    const Vec3 off = room_offset();
    const float dist = length(off);
    if (melee_hold_still()) {
        // A heavy swing (pipe, katana, maul): the game's attack is stationary (the PC VR mod's finding).
        move.hold_still = 1;
    } else if (len > 0) {
        // The stick: along the head or the left hand, forward up.
        float base = view_heading;
        if (opt.move_by == 1 && fi.hand_valid[kLeft]) {
            const Vec3 f = to_game(rotate(fi.hand_aim[kLeft].orientation, {0, 0, -1}));
            const Vec3 fw = game_yaw(f, rig.yaw);
            if (fw.x * fw.x + fw.z * fw.z > 0.01f) base = heading_of(fw);
        }
        move.steer = 1;
        move.heading = wrap(base + std::atan2(lx, ly));
        move.speed = len;
        move.run = c.left_click;
    } else if (opt.room_walk) {
        if (dist > 0.2f * kUnitsPerMetre) {
            move.steer = 1;
            move.heading = heading_of(off);
            move.speed = std::clamp(dist / (0.6f * kUnitsPerMetre), 0.3f, 1.0f);
            catching_up = true;
        }
    }
    // The head pushed where Heather cannot follow (a wall holds her): for the fade (sh3vr_body.cpp).
    if (catching_up && was_catching_up && dist >= prev_dist - 0.002f * kUnitsPerMetre) blocked_seconds += frame_dt();
    else if (!catching_up) blocked_seconds = 0;
    prev_dist = dist;
    // Her body faces the movement, or the head when she stands and the head has turned far from her: turned smoothly
    // (LIST 42), never snapped. Walking, her facing follows the stick at up to 10 rad/s (the walk itself goes where the
    // stick says at once, sh3.c). Standing, she starts turning when the head is 50 degrees off and stops within 10, at up
    // to 4 rad/s, eased; her feet step through the turn (sh3vr_ik.cpp's plants).
    static bool turning_body = false;
    static float turn_speed = 0;
    const float now = game.yaw, dt = frame_dt();
    if (move.steer) {
        const float d = wrap(move.heading - now);
        move.face = 1;
        move.face_yaw = wrap(now + std::clamp(d, -10.0f * dt, 10.0f * dt));
        turning_body = false;
        turn_speed = 0;
    } else if (!move.hold_still) {
        const float off = wrap(view_heading - now);
        if (!turning_body && std::fabs(off) > 50.0f * kPi / 180.0f) {
            turning_body = true;
            rt_log("VR: frame %d: her body turns after the head (%.0f degrees off)", rt_frame, off * 180.0f / kPi);
        }
        if (turning_body && std::fabs(off) < 10.0f * kPi / 180.0f) {
            turning_body = false;
            rt_log("VR: frame %d: her body faces the head again (%.0f degrees off)", rt_frame, off * 180.0f / kPi);
        }
        const float want = turning_body ? std::clamp(off * 5.0f, -4.0f, 4.0f) : 0.0f;
        turn_speed += (want - turn_speed) * (1.0f - std::exp(-12.0f * dt));
        float step = turn_speed * dt;
        if (std::fabs(step) > std::fabs(off)) step = off;
        if (std::fabs(step) > 1e-5f) {
            move.face = 1;
            move.face_yaw = wrap(now + step);
        }
    }
    // The gun aims where the controller points (two hands: from the main hand to the other; sh3vr_combat.cpp).
    Vec3 from, dir;
    if (opt.aim_by_hand && !opt.auto_aim_first_person && gun_ray(from, dir)) {
        const Vec3 f = world_dir(dir);
        move.aim = 1;
        move.aim_dir[0] = f.x;
        move.aim_dir[1] = f.y;
        move.aim_dir[2] = f.z;
    }
}

// Haptics from the game's state: damage taken, the heartbeat at low health, the radio's static.
float heartbeat_clock = 0;
float radio_level[4] = {0, 0, 0, 0};
void state_haptics() {
    if (!game.player_valid || game.hp_max <= 0) {
        last_hp = -1;
        return;
    }
    if (last_hp >= 0 && game.hp < last_hp - 0.01f && opt.damage_pulse) {
        const float k = std::clamp((last_hp - game.hp) / std::max(1.0f, game.hp_max) * 6.0f, 0.35f, 1.0f);
        buzz(kLeft, k, 0.25f, 120.0f);
        buzz(kRight, k, 0.25f, 120.0f);
    }
    last_hp = game.hp;
    const float frac = game.hp / game.hp_max;
    if (opt.heartbeat && in_play() && frac > 0 && frac < 0.3f) {
        const float period = 0.6f + (frac / 0.3f) * 0.4f;  // faster as health falls
        heartbeat_clock += frame_dt();
        if (heartbeat_clock >= period) {
            heartbeat_clock = 0;
            buzz(kLeft, 0.35f, 0.05f, 60.0f);
            buzz(kRight, 0.35f, 0.05f, 60.0f);
        } else if (std::fabs(heartbeat_clock - 0.18f) < frame_dt() * 0.5f) {
            buzz(kLeft, 0.22f, 0.04f, 60.0f);
            buzz(kRight, 0.22f, 0.04f, 60.0f);
        }
    } else heartbeat_clock = 0;
    // The radio's static (LIST 23; "Extra vibration", the whole world): a crackle in both hands, denser and harder the
    // louder the game plays it (sh3vr_radio: its four voices' levels, 0..1 before the game's volume options).
    float level = 0;
    for (float l : radio_level) level = std::max(level, l);
    if (!in_play() || game.cutscene) {
        for (float& l : radio_level) l = 0;
        level = 0;
    }
    if (level > 0.03f && opt.haptic_extra >= 2) {
        static uint32_t seed = 0x2545F491u;
        seed = seed * 1664525u + 1013904223u;
        const float r = (seed >> 8) / 16777216.0f;
        if (r < 0.2f + 0.6f * level) {
            const float k = std::clamp(0.08f + 0.5f * level, 0.0f, 0.6f) * (0.6f + 0.4f * ((seed >> 4) & 15) / 15.0f);
            const int side = (seed >> 3) & 1 ? kLeft : kRight;
            buzz(side, k, 0.018f, 80.0f + 90.0f * r);
            if (level > 0.5f) buzz(side == kLeft ? kRight : kLeft, k * 0.7f, 0.018f, 120.0f);
        }
    }
}

// ---------------------------------------------------------------- dynamic resolution (LIST 51)
// The owner: "be sure highest resolution is set with dynamic resolution". Jak's way (jak_vr.cpp Scale, its 0.1.6
// tuning): the headset tells no load, only the displays a frame came too late for (displays_missed); a second with
// more than a fifth of its frames late, or a quarter second with most of them late, takes the game's 3D pictures down
// a tenth (to 60% at the least); 5 s with none late tries a tenth up; a step up that is late again within 10 s doubles
// the wait before the next try (a minute at the most). sh3.c makes the pictures at the new size after the frame
// (sh3_vr_after_present); the game's last pass stretches them over each eye, the 2D and VR's hands stay sharp.
// 0.1.3: a step down that leaves the frames' work as it was (the game's own work the limit, not the drawing) is taken
// back a second later, so the pictures never stay soft for nothing (owner 84's fuzz; their 0.1.2 run sat at 60%).
namespace {
constexpr float kScaleStep = 0.1f, kScaleLeast = 0.6f;
constexpr double kScaleCalm = 5.0, kScaleCalmMost = 60.0, kScaleQuick = 0.25;
struct Scale {
    float now = 1.0f;
    int frames = 0, late = 0, quick_frames = 0, quick_late = 0;
    double window_at = 0, quick_at = 0, calm_since = 0, calm_wait = kScaleCalm, stepped_up_at = -1;
    long missed_seen = -1;
    // 0.1.3 (owner 84's "weird fuzz"; their 0.1.2 run: the 3D pictures at 60% for minutes while each frame's work
    // stayed 14-17 ms, the game's own work the limit, not the pictures' size): each frame's work (last_work_seconds),
    // the last second's mean, and a step down judged a second later: no quicker by 7% and the step is taken back,
    // with no step down for 30 s.
    double work_sum = 0, work_mean = 0, judge_at = -1, judge_sum = 0, judge_before = 0, hold_until = -1;
    int work_n = 0, judge_n = 0, judge_skip = 0;
};
Scale scale;
double clock_seconds() {
    return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
}
}  // namespace

void dynamic_resolution_frame() {
    Scale& s = scale;
    const long missed = hs->displays_missed();
    if (!opt.dynamic_resolution) {
        if (s.now != 1.0f) rt_log("VR: frame %d: dynamic resolution off: the 3D pictures at 100%%", rt_frame);
        s = Scale{};
        s.missed_seen = missed;
        return;
    }
    const double now = clock_seconds();
    const bool late = s.missed_seen >= 0 && missed > s.missed_seen;
    s.missed_seen = missed;
    if (s.window_at == 0) s.window_at = s.quick_at = s.calm_since = now;
    ++s.frames;
    ++s.quick_frames;
    if (late) {
        ++s.late;
        ++s.quick_late;
        s.calm_since = now;
    }
    const float before = s.now;
    const bool second_up = now - s.window_at >= 1.0;
    const bool quarter_up = now - s.quick_at >= kScaleQuick;
    const bool heavy = quarter_up && s.quick_frames >= 4 && s.quick_late * 2 > s.quick_frames;
    // 0.1.3: the frames' work (0 where the headset cannot say: the step is never judged)
    const double work = hs->last_work_seconds();
    if (work > 0) {
        s.work_sum += work;
        s.work_n++;
        if (s.judge_at >= 0 && s.judge_skip-- <= 0) {
            s.judge_sum += work;
            s.judge_n++;
        }
    }
    if (s.judge_at >= 0 && now >= s.judge_at) {
        const double after = s.judge_n > 0 ? s.judge_sum / s.judge_n : 0;
        if (after > 0 && s.judge_before > 0 && after > s.judge_before * 0.93 && s.now < 1.0f - 1e-3f) {
            s.now = std::min(1.0f, s.now + kScaleStep);
            s.hold_until = now + 30.0;
            s.calm_since = now;
            rt_log("VR: frame %d: dynamic resolution: the smaller pictures did not make the frames quicker (%.2f ms a frame "
                   "before, %.2f after): the game's own work is the limit, back to %d%%, no step down for 30 s",
                   rt_frame, s.judge_before * 1000.0, after * 1000.0, (int)(s.now * 100.0f + 0.5f));
        } else if (after > 0 && s.judge_before > 0) {
            static int told = 0;
            if (told++ < 5)
                rt_log("VR: frame %d: dynamic resolution: the smaller pictures help (%.2f ms a frame before, %.2f after)",
                       rt_frame, s.judge_before * 1000.0, after * 1000.0);
        }
        s.judge_at = -1;
    }
    if ((heavy || (second_up && s.late * 5 > s.frames)) && s.now > kScaleLeast + 1e-3f && now >= s.hold_until &&
        s.judge_at < 0) {
        if (s.stepped_up_at >= 0) s.calm_wait = std::min(s.calm_wait * 2.0, kScaleCalmMost);  // the step up was too much
        s.now = std::max(kScaleLeast, s.now - kScaleStep);
        s.stepped_up_at = -1;
        s.calm_since = now;
        s.frames = s.late = 0;
        s.window_at = now;
        // judged a second on, past the frames that make the pictures at the new size
        s.judge_before = s.work_mean > 0 ? s.work_mean : (s.work_n > 0 ? s.work_sum / s.work_n : 0);
        s.judge_at = now + 1.0;
        s.judge_sum = 0, s.judge_n = 0, s.judge_skip = 10;
    }
    if (second_up) {
        s.frames = s.late = 0;
        s.window_at = now;
        if (s.work_n > 0) s.work_mean = s.work_sum / s.work_n;
        s.work_sum = 0, s.work_n = 0;
    }
    if (quarter_up) {
        s.quick_frames = s.quick_late = 0;
        s.quick_at = now;
    }
    if (s.stepped_up_at >= 0 && now - s.stepped_up_at > 10.0) {
        s.stepped_up_at = -1;  // it held
        s.calm_wait = kScaleCalm;
    }
    if (s.now < 1.0f - 1e-3f && now - s.calm_since >= s.calm_wait) {
        s.now = std::min(1.0f, s.now + kScaleStep);
        s.stepped_up_at = now;
        s.calm_since = now;
    }
    if (s.now != before)
        rt_log("VR: frame %d: dynamic resolution: the 3D pictures at %d%% (%s; %ld displays missed so far)", rt_frame,
               (int)(s.now * 100.0f + 0.5f), s.now < before ? "frames were shown late" : "the headset keeps up", missed);
}

// ---------------------------------------------------------------- the frame

void begin_next_frame() {
    have_frame = hs->begin_frame(fi);
    if (!have_frame) {
        map.valid = false;
        return;
    }
    dynamic_resolution_frame();
    if (fi.frame_seconds > 0) display_hz = (float)(1.0 / fi.frame_seconds);
    {
        // motion_dt: the time from the last frame's display to this one's (a frame shown late spans more periods)
        static std::int64_t last_display = 0;
        motion_seconds = 0;
        if (fi.display_time > 0 && last_display > 0 && fi.display_time > last_display)
            motion_seconds = std::clamp((float)((fi.display_time - last_display) * 1e-9), 0.5f * frame_dt(), 0.1f);
        last_display = fi.display_time;
        static int told = 0;
        if (motion_seconds > 1.6f * frame_dt() && told < 3 && in_play()) {
            told++;
            rt_log("VR: frame %d: this frame came %.1f ms after the last (the display's period %.1f ms): hand speeds and "
                   "swings take the real time", rt_frame, motion_seconds * 1000.0f, frame_dt() * 1000.0f);
        }
    }
    if (fi.recentred) {
        rig.set = false;
        recentred_now = true;  // the eyes' height at the recentre is the player's standing height
        place_panel(true);
    }
    update_wanted_fov();
    update_auto_third();
    view_mode = pick_view_mode();
    {
        // Each change of view in the log, with what decided it.
        static ViewMode logged = ViewMode::kFlat;
        if (view_mode != logged) {
            static const char* const kNames[] = {"flat", "first person", "the game's camera", "the cinema screen"};
            rt_log("VR: frame %d: view %s (cutscene %d, demo %d, playable %d, Heather found %d, third person %d, auto %d)",
                   rt_frame, kNames[(int)view_mode], game.cutscene, game.demo, game.playable, game.player_valid,
                   third_person ? 1 : 0, auto_third ? 1 : 0);
            logged = view_mode;
            // A new view places its rig anew (first person after a scene took the scene camera's rig: her body started
            // 90 degrees off the head and her neck away from the player's, run vr53).
            rig.set = false;
        }
        // A scene seen through her eyes (dynamic cutscenes, LIST 48) going on into play in first person is a new view
        // too, though the view's kind stays (session 10, vr156: the opening's scene ended in her eyes): the rig anew.
        static bool scene_was = false;
        const bool scene_now = game.cutscene || game.demo;
        if (view_mode == ViewMode::kFirstPerson && scene_now != scene_was) {
            rt_log("VR: frame %d: first person %s", rt_frame,
                   scene_now ? "into a scene (her eyes as the scene poses her)" : "from a scene into play: the rig anew");
            rig.set = false;
        }
        scene_was = scene_now;
#if !defined(__ANDROID__)
        // The PC rig's scripts (QUEST_SIM_ALIGN) count from the first frame of first person in play, which comes at a
        // varying frame after the opening's loads (not a scene's first person: dynamic cutscenes, LIST 48).
        if (!quest::sim_ready && view_mode == ViewMode::kFirstPerson && game.playable && !scene_now)
            quest::sim_ready = true;
#endif
    }
    // The mapping for this frame's poses, from what is known now (sh3vr_view makes it again with the game's camera).
    Vec3 hp;
    Quat hq;
    map.valid = false;
    if (view_mode == ViewMode::kFirstPerson || view_mode == ViewMode::kCamera)
        compute_map(view_mode, game.cam_valid ? game.cam_view : nullptr, hp, hq);
    ui_place_frame();  // session 11: a screen stays where it opened (place_panel otherwise)
    update_pointer();
    comfort_turn();
    body_frame();
    combat_frame();
    plan_move();
    map_pad();
    state_haptics();
    prev = fi.controls;
    panel_used_last = panel_used;
    panel_used = false;
    our_view_set = false;
    panel_clear();
    frame_box[0] = frame_box[1] = 2.0f;  // the next frame's 2D on the panel (sh3vr_panel_box)
    frame_box[2] = frame_box[3] = -1.0f;
}

// Comfort: the vignette's wanted strength this frame (moving with the stick or turning smoothly).
float vignette_amount() {
    if (!opt.vignette || view_mode != ViewMode::kFirstPerson || !in_play()) return 0;
    return (move.steer && !catching_up) || turning_now ? opt.vignette_strength / 100.0f : 0.0f;
}

}  // namespace

// The VR menu's "Recentre the view now": as the headset's own recentre.
void request_recentre() {
    rig.set = false;
    place_panel(true);
    rt_log("VR: recentred from the VR menu");
}

// For sh3vr_body.cpp's comfort pass.
float comfort_vignette() { return vignette_amount(); }
float head_in_wall() {
    if (!opt.head_fade || view_mode != ViewMode::kFirstPerson || !catching_up || blocked_seconds < 0.25f) return 0;
    const float dist = length(room_offset()) / kUnitsPerMetre;
    return std::clamp((dist - 0.3f) / 0.3f, 0.0f, 1.0f);
}

}  // namespace sh3q

// ================================================================ C API

using namespace sh3q;

extern "C" {

void sh3vr_log(int level, const char* text) { log_cb(level, text); }

int sh3vr_start(void) {
    if (active) return 1;
    if (!sh3vr_enabled()) {
        rt_log("VR: off (sh3vr.ini vr = 0): the game runs flat");
        return 0;
    }
    if (!gl_caps.multiview) {
        rt_log("VR: the graphics driver has no GL_OVR_multiview2: the game runs flat");
        return 0;
    }
    for (int i = 0; i < 32; i++) eye_ident[i] = (i % 16) % 5 == 0 ? 1.0f : 0.0f;
#if !defined(__ANDROID__)
    sim_gl_proc = SDL_GL_GetProcAddress;
#endif
    hs = make_headset(log_cb);
    XrSettings s;
    s.refresh_hz = opt.refresh_hz;
    s.resolution = opt.resolution;
    s.panel_width = panel_w;
    s.panel_height = panel_h;
    s.cpu_gpu_boost = opt.boost;
    s.foveation = opt.foveation;
    s.sharpen = opt.sharpen;
    s.space_warp = opt.space_warp;
    std::string why;
    if (!hs->start(s, why)) {
        rt_log("VR: the headset did not start (%s): the game runs flat", why.c_str());
        delete hs;
        hs = nullptr;
        return 0;
    }
    hs->target_size(kTargetLeftEye, eye_w, eye_h);
    panel_init();
    ui_font_load();
    active = true;
    rt_log("VR: on, each eye %dx%d, panel %dx%d, refresh %.0f Hz asked, render scale %.2f, foveation %d, boost %d, "
           "sharpen %d, smoother frames %d", eye_w, eye_h, panel_w, panel_h, opt.refresh_hz, opt.resolution,
           opt.foveation, opt.boost, opt.sharpen, opt.space_warp);
    rt_log("VR: first person %d, cutscenes %d, height %.0f cm, seated %d, room walk %d, turn %d, left-handed %d, "
           "holsters %d, physical melee %d, flashlight %d", opt.first_person, opt.cutscenes, opt.player_height_cm,
           opt.seated, opt.room_walk, opt.turn, opt.left_handed, opt.holsters, opt.physical_melee, opt.flashlight);
    begin_next_frame();
    return 1;
}

int sh3vr_active(void) { return active; }

int sh3vr_enabled(void) {
    static int known = -1;
    if (known < 0) {
        load_options();
        known = opt.vr ? 1 : 0;
    }
    return known;
}

void sh3vr_eye_size(uint32_t* w, uint32_t* h) {
    if (!active) return;
    // The game's own limits: the eye's size, rounded to 8.
    *w = (uint32_t)(eye_w & ~7);
    *h = (uint32_t)(eye_h & ~7);
}

int sh3vr_wants_exit(void) { return active && hs->wants_exit(); }

float sh3vr_display_hz(void) { return active && have_frame ? display_hz : 0.0f; }

int sh3vr_present(unsigned bb_tex, unsigned depth_tex, int w, int h) {
    if (!active) return 1;
    if (have_frame) {
        const bool screen = view_mode == ViewMode::kScreen;
        // Session 11 (owner message 44): one of the game's full screens (out of play, no world drawn): the eyes dark
        // round the screen in front of them (its first frame still drew into the eyes, before the routing knew).
        full_screen_frame = sh3vr_full_screens() && !game.playable && !world_drawn;
        if (full_screen_frame && fi.should_render) {
            glBindFramebuffer(GL_DRAW_FRAMEBUFFER, draw_fbo);
            glDisable(GL_SCISSOR_TEST);
            glColorMask(1, 1, 1, 1);
            glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
            for (int e = 0; e < 2; e++) {
                glFramebufferTextureLayer(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, bb_tex, 0, e);
                glClear(GL_COLOR_BUFFER_BIT);
            }
        }
        // VR's own drawing into the eyes' picture first: the hands, their ghosts, the probes, the comfort shading.
        if (!screen && fi.should_render) body_draw(bb_tex, depth_tex, w, h);
        glDisable(GL_SCISSOR_TEST);
        glBindFramebuffer(GL_READ_FRAMEBUFFER, read_fbo);
        const unsigned menu_tex = vr_menu_open ? menu_texture() : 0;
        // LIST 46: the reload's hint, a strip at the foot of the panel (over the game's 2D; Silent Hill 3 has no HUD
        // there in play), both top row first.
        int hw = 0, hh = 0;
        const unsigned hint_tex = !screen && !menu_tex && fi.should_render ? reload_hint_texture(hw, hh) : 0;
        if (hint_tex) {
            glBindFramebuffer(GL_DRAW_FRAMEBUFFER, panel_fbo);
            glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, hint_tex, 0);
            const int x0 = (panel_w - hw) / 2, y0 = panel_h - hh - panel_h / 12;
            glBlitFramebuffer(0, 0, hw, hh, x0, y0, x0 + hw, y0 + hh, GL_COLOR_BUFFER_BIT, GL_LINEAR);
            panel_used = true;
        }
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, draw_fbo);
        unsigned ptarget = 0;
        int pw = panel_w, ph = panel_h;
        if (fi.should_render) {
            if (!screen) {
                for (int e = 0; e < 2; e++) {
                    const unsigned target = hs->acquire(e == 0 ? kTargetLeftEye : kTargetRightEye);
                    if (!target) continue;
                    int tw, th;
                    hs->target_size(e == 0 ? kTargetLeftEye : kTargetRightEye, tw, th);
                    glFramebufferTextureLayer(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, bb_tex, 0, e);
                    glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, target, 0);
                    // The game's images are top row first (D3D's); the swapchain's bottom row first: flipped here
                    // (through the picture's pass when a picture setting is in use, owner 81).
                    if (!picture_pass(bb_tex, e, w, h, tw, th))
                        glBlitFramebuffer(0, 0, w, h, 0, th, tw, 0, GL_COLOR_BUFFER_BIT, GL_LINEAR);
                }
            }
            ptarget = hs->acquire(kTargetPanel);
            if (ptarget) hs->target_size(kTargetPanel, pw, ph);
        }
        PanelLayer panel;
        // Session 11: the panel through the UI's pass (fades, messages by their things, screens in front of the eyes);
        // the cinema screen (movies) and a UI pass that could not start: as before, the picture blitted whole.
        const bool cinema = screen && !menu_tex;
        bool ui_done = false;
        if (!cinema) ui_done = ui_frame(menu_tex, ptarget, pw, ph, panel);
        else {
            ui.kind = UiKind::kNone;
            ui.fade = 0.0f;
        }
        if (!ui_done) {
            if (ptarget) {
                glBindFramebuffer(GL_READ_FRAMEBUFFER, read_fbo);
                glBindFramebuffer(GL_DRAW_FRAMEBUFFER, draw_fbo);
                if (menu_tex) glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, menu_tex, 0);
                else if (screen) glFramebufferTextureLayer(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, bb_tex, 0, 0);
                else glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, panel_tex, 0);
                glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, ptarget, 0);
                const int sw = screen && !menu_tex ? w : panel_w, sh = screen && !menu_tex ? h : panel_h;
                // The menu's canvas is top row first like the game's pictures.
                glBlitFramebuffer(0, 0, sw, sh, 0, ph, pw, 0, GL_COLOR_BUFFER_BIT, GL_LINEAR);
            }
            panel.visible = screen || panel_used || menu_tex;
            panel.blend = !screen && !menu_tex;
            if (screen && !menu_tex) {
                // The cinema screen: the game's 4:3, 3 m wide at 3 m.
                panel.width = 3.0f;
                panel.height = 3.0f * h / (float)w;
                const Quat q = yaw_quat(tracking_yaw(panel_pose.orientation));
                panel.pose = {fi.head.position + rotate(q, {0, 0, -3.0f}), q};
            } else {
                panel.width = opt.panel_width;
                panel.height = opt.panel_width * 3.0f / 4.0f;
                panel.pose = panel_pose;
            }
        }
        show_pointer();
        show_guide_dot();
        show_wrist_bar();
        if (opt.show_performance) {
            char line[160];
            std::snprintf(line, sizeof line, "%.0f Hz  3D %d%%  view %d  weapon %d  hp %.0f/%.0f", display_hz,
                          (int)(sh3vr_picture_scale() * 100.0f + 0.5f), (int)view_mode, game.weapon, game.hp, game.hp_max);
            hs->show_performance(line);
        } else hs->show_performance("");
        hs->finish_frame(!screen && fi.should_render, false, panel);
    }
    begin_next_frame();
    // The desktop window shows the left eye (or the screen) on a PC.
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    return 1;
}

void sh3vr_game_state(const Sh3VrGame* g) {
    game = *g;
    game_seen = true;
}

void sh3vr_move(Sh3VrMove* out) { *out = move; }

void sh3vr_actions(Sh3VrActions* out) {
    out->equip_slot = -1;
    out->put_away = 0;
    out->reload_commit = 0;
    out->load_rounds = 0;
    out->sound_event = -1;
    if (active) {
        combat_actions(out);
        reload_actions(out);
    }
}

int sh3vr_block_game_reload(void) { return active && reload_blocks_game() ? 1 : 0; }

int sh3vr_film_grain(void) { return opt.film_grain ? 1 : 0; }

int sh3vr_her_shadow(const float view[16], const float proj[16], float* out_xyz, int max_tris) {
    if (!active || !opt.her_shadow) return 0;
    int counts[5];
    const int n = her_shadow_tris(view, proj, out_xyz, max_tris, counts);
    // Told the first time, as it starts again after none for a second, every 10 seconds while cast, and in
    // SH3_VSC_DUMP's frame.
    static int last_frame = -1000, told_frame = -1000, dump_at = -2;
    if (dump_at == -2) dump_at = getenv("SH3_VSC_DUMP") ? atoi(getenv("SH3_VSC_DUMP")) : -1;
    if (n > 0 && (rt_frame - last_frame > 60 || rt_frame - told_frame >= 600 || rt_frame == dump_at)) {
        told_frame = rt_frame;
        if (counts[4] > 0)
            rt_log("VR: frame %d: Heather's shadow from her own mesh: %d triangles (%d parts drawn; lit faces: %d of her "
                   "body, %d of the weapon or a part held; %d sides; %d faces unlit or out of the beam)",
                   rt_frame, n, counts[4], counts[0], counts[1], counts[2], counts[3]);
        else
            rt_log("VR: frame %d: Heather's shadow from her bones' capsules (no draw of hers caught): %d triangles (%d of "
                   "her body, %d of the weapon, %d of a part held; %d left out: the lamp at them or out of the beam)",
                   rt_frame, n, counts[0], counts[1], counts[2], counts[3]);
    }
    if (n > 0) last_frame = rt_frame;
    return n;
}

void sh3vr_shadow_part(const uint8_t* verts48, int nverts, const uint16_t* tris, int ntris, const float* consts,
                       uint32_t key, int kind) {
    if (active && opt.her_shadow) shadow_part(verts48, nverts, tris, ntris, consts, key, kind);
}

int sh3vr_spatial_sound(void) {
    return active && opt.spatial_sound && view_mode != ViewMode::kFlat && view_mode != ViewMode::kScreen ? 1 : 0;
}

void sh3vr_game_reload(int weapon, int event_index) {
    if (active) reload_game_event(weapon, event_index);
}

int sh3vr_quick_raise(void) { return active && !pull_shot && pull_left > 0 && view_mode == ViewMode::kFirstPerson; }

void sh3vr_notify_shot(void) {
    pull_shot = true;
    if (!active) return;
    combat_shot();
    reload_shot();
}

int sh3vr_weapon_to_hand(float point[3], float dir[3], int from_hand) {
    if (!active || !map_ready()) return 0;
    Vec3 p{point[0], point[1], point[2]}, d{dir[0], dir[1], dir[2]};
    if (!combat_weapon_map(p, d, from_hand != 0)) return 0;
    point[0] = p.x, point[1] = p.y, point[2] = p.z;
    dir[0] = d.x, dir[1] = d.y, dir[2] = d.z;
    return 1;
}

int sh3vr_weapon_world(float m[16], const float base[16]) {
    return active && map_ready() && combat_weapon_world(m, base) ? 1 : 0;
}

int sh3vr_shot_line(float from[3], float dir[3]) {
    Vec3 f, d;
    if (!active || !map_ready() || view_mode != ViewMode::kFirstPerson || !opt.aim_by_hand ||
        opt.auto_aim_first_person || !gun_ray(f, d))
        return 0;
    const Vec3 fw = world_point(f), dw = normalized(world_dir(d));
    from[0] = fw.x, from[1] = fw.y, from[2] = fw.z;
    dir[0] = dw.x, dir[1] = dw.y, dir[2] = dw.z;
    return 1;
}

int sh3vr_melee_volume(int weapon_slot, Sh3VrHit* hit) { return active ? combat_melee_volume(weapon_slot, hit) : 0; }
int sh3vr_struggle_presses(float left) { return active ? struggle_presses(left) : 0; }
void sh3vr_struggle_done(int freed, int buttons) {
    if (active) struggle_done(freed, buttons);
}
float sh3vr_block(const float at[3], const float way[3], const float from[3], int has_from, float damage,
                  int kind) {
    return active ? combat_block(at, way, from, has_from, damage, kind) : 1.0f;
}

int sh3vr_extra_hit(Sh3VrHit* out) { return active ? combat_extra_hit(out) : 0; }
void sh3vr_melee_landed(const float way[3], const float point[3], uint32_t character, int battle_id) {
    if (active) combat_melee_landed(way, point, character, battle_id);
}
float sh3vr_hit_given(uint32_t character, const float way[3], const float point[3], float damage, int reaction) {
    return active ? combat_hit_given(character, way, point, damage, reaction) : 1.0f;
}

int sh3vr_flat_2d(void) { return active; }

int sh3vr_screen_mode(void) { return active && view_mode == ViewMode::kScreen; }

int sh3vr_full_screens(void) {
    return active && opt.menus_in_view && !game.cutscene && !game.demo && view_mode != ViewMode::kScreen &&
           view_mode != ViewMode::kFlat;
}

void sh3vr_frame_world(int drawn) { world_drawn = drawn != 0; }

void sh3vr_movie(int on) {
    if ((on != 0) == movie_playing) return;
    movie_playing = on != 0;
    rt_log("VR: %s", movie_playing ? "a movie plays: shown on the cinema screen" : "the movie is over: the view as before");
}

// The game's effects drawn in screen space with their real depth (session 6: the muzzle flash, smoke, dust, sparks:
// quads it projected itself, depth strictly between 0 and 1, where its 2D sits at exactly 0 or 1): drawn in the eyes
// by the eye corrections, as the frame camera's draws are, so they stand in the world in both eyes instead of on the
// flat panel.
int sh3vr_world_effects(void) { return active && corr_valid && vp_valid && view_mode != ViewMode::kScreen; }

// The game's own Life Display (a red "+" at the screen's lower right under half health, vr148): in first person the
// wrist's bar shows her health in its place, on the same beats, so the cross is left out there (1); third person, or
// the wrist's option off, is the game's normal HUD (owner message 40): the cross goes on the HUD panel with the
// game's other 2D (0), never stamped into each eye's corner.
int sh3vr_wrist_health_on(void) {
    return active && view_mode == ViewMode::kFirstPerson && opt.wrist_health ? 1 : 0;
}
// The game's Life Gauge (sh3.c 0x5A5E40): its packets cannot be routed at the draw, so it is left out wherever VR shows
// the world in the eyes; show_wrist_bar draws the same bars (her watch in first person with the wrist's option on;
// otherwise the game's normal HUD, at the view's top left).
int sh3vr_life_gauge_skip(void) {
    return active && (view_mode == ViewMode::kFirstPerson || view_mode == ViewMode::kCamera) ? 1 : 0;
}
// LIST 51: the size the game's 3D pictures are to be drawn at, of the eye's (sh3.c sh3_vr_after_present).
float sh3vr_picture_scale(void) { return active && opt.dynamic_resolution ? scale.now : 1.0f; }

int sh3vr_effect_map(const float ndc[3], int tex_w, float out_gl[16]) {
    if (!sh3vr_world_effects()) return 0;
    M4 inv;
    if (!invert(vp_m, inv)) return 0;
    const float h[4] = {ndc[0], ndc[1], ndc[2], 1.0f};
    float w[4];
    for (int i = 0; i < 4; i++) w[i] = inv.m[i][0] * h[0] + inv.m[i][1] * h[1] + inv.m[i][2] * h[2] + inv.m[i][3] * h[3];
    if (!(std::fabs(w[3]) > 1e-9f)) return 0;
    const float p[3] = {w[0] / w[3], w[1] / w[3], w[2] / w[3]};
    float r[3][3], t[3], info[7];
    if (!combat_effect_rel(p, tex_w, r, t, info)) return 0;
    M4 R = M4::identity();
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) R.m[i][j] = r[i][j];
        R.m[i][3] = t[i];
    }
    const M4 map = vp_m * R * inv;  // the game's clip -> its world -> moved to the hand -> the game's clip
    float g[16];
    map.to_gl(g);
    for (float v : g)
        if (!std::isfinite(v)) return 0;
    std::memcpy(out_gl, g, sizeof g);
    static int told = 0;
    if (told < 6 && ++told) {
        float q[3];
        for (int i = 0; i < 3; i++) q[i] = r[i][0] * p[0] + r[i][1] * p[1] + r[i][2] * p[2] + t[i];
        rt_log("VR: frame %d: the weapon's effect at %.0f %.0f %.0f (%.0f units from the game's weapon %.0f %.0f %.0f, texture %d) moved with "
               "it to %.0f %.0f %.0f (the flash's or beam's middle at %.0f %.0f %.0f; ndc %.3f %.3f %.4f)", rt_frame, p[0], p[1],
               p[2], info[0], info[1], info[2], info[3], tex_w, q[0], q[1], q[2], info[4], info[5], info[6], ndc[0], ndc[1],
               ndc[2]);
    }
    return 1;
}

// Owner 79 (session 16; a player: "theres like a weird overlay that looks like fog. unplayable"): the game's drifting fog
// is screen-space quads (d3d8.c vr_route_pick: 0x5F572E's fans, 171 a frame in vr243), drawn without the z test. Their
// z is no real depth: here 0.60 to 0.98 (vr244; the PC VR mod saw them just past 1), so the world effects' path put
// each eye's view of them at that depth, patches of fog floating close in front of the eyes. The PC VR mod (NotGodlikeUwU's,
// d3d9_hook.cpp TransformBatchedFogVertex8) puts each one back through the game's view and projection on its far plane
// and then through each eye's: a far layer fixed in the world. Here the same with the eyes' corrections (the frame
// camera's draws' way): the map sets each point's depth to the far plane (clip z = w), takes it to the world, moves it
// halfway to the game's camera along its own ray (10 m: the game's picture keeps it where it was; half the far plane
// stays inside every eye's) and back to the game's clip; d3d8_shader.c then applies each eye's correction.
int sh3vr_far_layer_map(float out_gl[16]) {
    if (!sh3vr_world_effects()) return 0;
    M4 inv;
    if (!invert(vp_m, inv)) return 0;
    // the game's camera: the one point its projection sends to w = 0 at the middle, (0, 0, 1, 0) in its clip
    const float cw = inv.m[3][2];
    if (!(std::fabs(cw) > 1e-12f)) return 0;
    const float cam[3] = {inv.m[0][2] / cw, inv.m[1][2] / cw, inv.m[2][2] / cw};
    const float k = 0.5f;
    M4 S = M4::identity();
    for (int i = 0; i < 3; i++) S.m[i][i] = k, S.m[i][3] = (1.0f - k) * cam[i];
    M4 F = M4::identity();  // the depth to the far plane: clip z = w
    F.m[2][2] = 0.0f, F.m[2][3] = 1.0f;
    const M4 map = vp_m * S * inv * F;
    float g[16];
    map.to_gl(g);
    for (float v : g)
        if (!std::isfinite(v)) return 0;
    std::memcpy(out_gl, g, sizeof g);
    static int told_frame = -1000;
    if (rt_frame - told_frame >= 3600) {  // once a minute or so: where the layer is, and what the game's depths mean
        told_frame = rt_frame;
        float d[4];
        const float zs[4] = {0.5f, 0.9f, 0.95f, 1.0f};
        for (int n = 0; n < 4; n++) {
            float q[4];
            for (int i = 0; i < 4; i++) q[i] = inv.m[i][2] * zs[n] + inv.m[i][3];
            d[n] = std::fabs(q[3]) > 1e-12f ? std::sqrt((q[0] / q[3] - cam[0]) * (q[0] / q[3] - cam[0]) +
                                                         (q[1] / q[3] - cam[1]) * (q[1] / q[3] - cam[1]) +
                                                         (q[2] / q[3] - cam[2]) * (q[2] / q[3] - cam[2]))
                                              : -1.0f;
        }
        rt_log("VR: frame %d: the game's drifting fog on a far layer in the world, %.1f m off (the game's camera at %.0f "
               "%.0f %.0f; each eye's own view); the game's depths 0.5, 0.9, 0.95 and 1 lie %.2f, %.2f, %.2f and %.2f m "
               "from its camera", rt_frame, d[3] * k / kUnitsPerMetre, cam[0], cam[1], cam[2], d[0] / kUnitsPerMetre,
               d[1] / kUnitsPerMetre, d[2] / kUnitsPerMetre, d[3] / kUnitsPerMetre);
    }
    return 1;
}
int sh3vr_drifting_fog(void) { return opt.drifting_fog ? 1 : 0; }

// Session 17 (the handoff's item 2): the game's lens flare round a lamp (d3d8.c vr_route_pick: 0x468CDA's streaks,
// 0x468F6E's rings) is drawn at a fixed depth, z 0.50003: 0.40 m from the eyes in VR, a double image in front of the
// wall the lamp is on. The game's own view depth of the lamp (its flare record, 0x898420 + 0x18) is where it belongs:
// the map sets each point's depth to that one (the game's clip w is the view depth: 0x72C460's projection, vr246), and
// each eye's correction (d3d8_shader.c) then puts that eye's view of it at the lamp. 0 when the depth is no lamp's.
int sh3vr_depth_layer_map(float view_depth, float ndc_x, float ndc_y, unsigned from, float out_gl[16]) {
    if (!sh3vr_world_effects()) return 0;
    if (!(view_depth > 30.0f && view_depth < 1.0e6f)) return 0;  // 6 cm .. 2 km: the record's, nothing else
    M4 inv;
    if (!invert(vp_m, inv)) return 0;
    // the game's clip w of the point at (x, y, z) in its ndc is 1 / (inv's last row . (x, y, z, 1))
    const float a = inv.m[3][2], b = inv.m[3][0] * ndc_x + inv.m[3][1] * ndc_y + inv.m[3][3];
    if (!(std::fabs(a) > 1e-12f)) return 0;
    float zn = (1.0f / view_depth - b) / a;
    if (!std::isfinite(zn)) return 0;
    zn = std::clamp(zn, 1.0e-4f, 0.9999f);
    M4 F = M4::identity();  // each point's depth set to the lamp's: clip z = zn * w
    F.m[2][2] = 0.0f, F.m[2][3] = zn;
    float g[16];
    F.to_gl(g);
    std::memcpy(out_gl, g, sizeof g);
    static int told = 0, told_frame = -100000;
    if (told < 4 || rt_frame - told_frame >= 3600) {  // the first few, then once a minute or so
        told++, told_frame = rt_frame;
        const float cw = inv.m[3][2];
        float dist = -1.0f, half = -1.0f;
        if (std::fabs(cw) > 1e-12f) {
            const float cam[3] = {inv.m[0][2] / cw, inv.m[1][2] / cw, inv.m[2][2] / cw};
            const float zs[2] = {zn, 0.5f};
            for (int n = 0; n < 2; n++) {
                float q[4];
                for (int i = 0; i < 4; i++)
                    q[i] = inv.m[i][0] * ndc_x + inv.m[i][1] * ndc_y + inv.m[i][2] * zs[n] + inv.m[i][3];
                if (!(std::fabs(q[3]) > 1e-12f)) continue;
                const float dx = q[0] / q[3] - cam[0], dy = q[1] / q[3] - cam[1], dz = q[2] / q[3] - cam[2];
                (n == 0 ? dist : half) = std::sqrt(dx * dx + dy * dy + dz * dz) / kUnitsPerMetre;
            }
        }
        rt_log("VR: frame %d: the game's lens flare (%s, from %06X) at its lamp, %.2f m ahead (%.0f units; depth %.5f in "
               "place of the game's 0.50003, which lies %.2f m off; %.2f m from the game's camera along its ray), over "
               "everything, each eye's own view", rt_frame, from == 0x468CDAu ? "its streaks" : "a ring", from,
               view_depth / kUnitsPerMetre, view_depth, zn, half, dist);
    }
    return 1;
}

int sh3vr_beam_screen(const float vp[4], float out[12]) {
    float l[4][3];
    if (!active || !vp_valid || !combat_beam_line(l[0], l[1], l[2], l[3])) return 0;
    for (int k = 0; k < 4; k++) {
        float c[4];
        for (int i = 0; i < 4; i++)
            c[i] = vp_m.m[i][0] * l[k][0] + vp_m.m[i][1] * l[k][1] + vp_m.m[i][2] * l[k][2] + vp_m.m[i][3];
        const float rhw = c[3] > 1e-6f ? 1.0f / c[3] : 0.0f;
        out[k * 3] = vp[0] + (c[0] * rhw + 1.0f) * 0.5f * vp[2];
        out[k * 3 + 1] = vp[1] + (1.0f - c[1] * rhw) * 0.5f * vp[3];
        out[k * 3 + 2] = c[2] * rhw;
    }
    return 1;
}

// Owner 61 (session 15): the beam saber's glow as it lights is a 3D mesh of the game's (vr203: vs F0000007, 6 vertices,
// added light, a 512x512 texture, two draws a frame), placed by its own constants at the saber in her animated hand.
// (The blade itself is a skinned draw of its own, moved in sh3vr_combat.cpp saber_blade_draw: vr216/vr217; left at her
// animated hand it was the green bar across the view of vr205-215.) Not the screen-space sprites (vr207: 0x468F6E's and 0x468CDA's 128x128 quads at z 0.5 are the lights' halos, there before the beam
// lights; the 64x64 ones dust at its true depth, vr208). Its output, the game's clip space, is moved as the saber is from
// the game's hand to the VR hand: the game's clip -> its world -> the weapon's map -> its clip (GL column-major), which
// d3d8_shader.c puts before each eye's correction.
int sh3vr_beam_mesh(float out_gl[16], uint32_t count) {
    if (!sh3vr_world_effects()) return 0;
    float r[3][3], t[3];
    if (!combat_beam_map(r, t)) return 0;
    M4 inv;
    if (!invert(vp_m, inv)) return 0;
    M4 R = M4::identity();
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) R.m[i][j] = r[i][j];
        R.m[i][3] = t[i];
    }
    const M4 map = vp_m * R * inv;
    float g[16];
    map.to_gl(g);
    for (float x : g)
        if (!std::isfinite(x)) return 0;
    std::memcpy(out_gl, g, sizeof g);
    static int told = 0, told_frame = -1;
    if (rt_frame != told_frame && told < 12 && (told < 3 || rt_frame % 20 == 0) && ++told) {
        told_frame = rt_frame;
        // how far the map moves things: its turn, and where it takes the game's saber line
        const float tr = r[0][0] + r[1][1] + r[2][2];
        const float turn = std::acos(std::clamp((tr - 1.0f) * 0.5f, -1.0f, 1.0f)) * 57.29578f;
        float s[12] = {0}, l[4][3] = {{0}};
        const float vp[4] = {0, 0, 1000, 1000};
        const int have = sh3vr_beam_screen(vp, s) && combat_beam_line(l[0], l[1], l[2], l[3]);
        const float dx = l[2][0] - l[0][0], dy = l[2][1] - l[0][1], dz = l[2][2] - l[0][2];
        rt_log("VR: frame %d: the beam saber's beam (the game's mesh, %u vertices) moved with the saber to the hand: the "
               "map turns %.1f deg and moves the game's palm %.2f m; the saber's line (the frame camera, 1000 x 1000) the "
               "game's %.0f %.0f - %.0f %.0f, the hand's %.0f %.0f - %.0f %.0f%s", rt_frame, count, turn,
               std::sqrt(dx * dx + dy * dy + dz * dz) / kUnitsPerMetre, s[0], s[1], s[3], s[4], s[6], s[7], s[9], s[10],
               have ? "" : " (no line)");
    }
    return 1;
}

// The beam saber's light (session 15). The game's shaders take four point lights (vs F0000031, the lit hilt's, and the
// world's: c16-c19 their places, c20-c23 their colours, c24-c25 the falloff), and a lit saber lights what is near it by
// a green one (vr218: c17, colour 0.40 0.60 0.20) put on the saber in her animated hand (by her hip in first person),
// which goes out along the blade as it extends (0.27 m from the hilt's line at ignition, 0.49 m later). A green light
// within 0.6 m of the game's saber, the hold's palm to 1.5 m past its tip (combat_beam_line), is moved by the weapon's
// map to the saber in the VR hand, so the hilt and the walls are lit from where the blade is. Her flashlight's (c16,
// white, 0.8 m off) and the rest stay. While the saber is out, not only while 0x712C66B is set: the game drops that
// flag after a swing with the blade still out (vr218). Logged the first times with each light's distance.
int sh3vr_beam_light(float vsc[96][4]) {
    if (!active || !sh3vr_world_effects()) return 0;
    float r[3][3], t[3], ga[3], gb[3], va[3], vb[3];
    if (!combat_beam_map(r, t) || !combat_beam_line(ga, gb, va, vb)) return 0;
    const float near_u = 0.6f * kUnitsPerMetre, past_tip_u = 1.5f * kUnitsPerMetre;
    int moved = 0;
    static int told = 0, told_frame = -1;
    // the lights of one draw a frame, the first 3 lit frames and every 20th after, 12 frames in all
    const bool tell = rt_frame != told_frame && told < 12 && (told < 3 || rt_frame % 20 == 0);
    if (tell) told++, told_frame = rt_frame;
    for (int i = 16; i < 20; i++) {
        const float* c = vsc[i + 4];
        if (c[0] == 0 && c[1] == 0 && c[2] == 0) continue;  // no light in this slot
        float* p = vsc[i];
        if (!std::isfinite(p[0]) || !std::isfinite(p[1]) || !std::isfinite(p[2])) continue;
        // its distance from the game's saber line
        float ab[3], ap[3], ab2 = 0, d = 0;
        for (int k = 0; k < 3; k++) ab[k] = gb[k] - ga[k], ap[k] = p[k] - ga[k], ab2 += ab[k] * ab[k], d += ab[k] * ap[k];
        const float s_max = ab2 > 1e-6f ? 1.0f + past_tip_u / std::sqrt(ab2) : 0.0f;  // the blade's whole length
        const float s = ab2 > 1e-6f ? std::clamp(d / ab2, 0.0f, s_max) : 0.0f;
        float dist2 = 0;
        for (int k = 0; k < 3; k++) {
            const float e = ap[k] - s * ab[k];
            dist2 += e * e;
        }
        const float dist = std::sqrt(dist2);
        const bool green = c[1] > c[0] && c[1] > c[2];
        // vr219: 0.25-0.57 m off the line as the blade extends, 0.92 m on the swing's frame (7600: the light trails
        // the blade): a green light near the line, or within 2 m of the game's hilt
        const float from_hilt = std::sqrt(ap[0] * ap[0] + ap[1] * ap[1] + ap[2] * ap[2]);
        const bool ours = green && (dist < near_u || from_hilt < 2.0f * kUnitsPerMetre);
        float q[3];
        for (int k = 0; k < 3; k++) q[k] = r[k][0] * p[0] + r[k][1] * p[1] + r[k][2] * p[2] + t[k];
        if (tell)
            rt_log("VR: frame %d: point light c%d at %.0f %.0f %.0f (colour %.2f %.2f %.2f), %.2f m from the game's saber "
                   "line: %s %.0f %.0f %.0f%s", rt_frame, i, p[0], p[1], p[2], c[0], c[1], c[2], dist / kUnitsPerMetre,
                   ours ? "the saber's, moved to" : "another light, left (the map would put it at", q[0], q[1], q[2],
                   ours ? "" : ")");
        if (!ours) continue;
        p[0] = q[0], p[1] = q[1], p[2] = q[2];
        moved = 1;
    }
    return moved;
}

void sh3vr_view(float m[16], int main) {
    if (!active || !main || !head_known()) return;
    const ViewMode mode = view_mode;
    if (mode == ViewMode::kFlat || mode == ViewMode::kScreen) return;
    Vec3 pos;
    Quat q;
    if (!compute_map(mode, m, pos, q)) return;
    make_view(pos, q, m);
    std::memcpy(our_view, m, sizeof our_view);
    our_view_set = true;
}

void sh3vr_camera_built(const float view[16], const float proj[16]) {
    if (!active || !our_view_set || std::memcmp(view, our_view, sizeof our_view) != 0) return;
    std::memcpy(p_main, proj, sizeof p_main);
    p_main_valid = true;
    // How far the game's scales are from covering both eyes: our factors follow (the scale is proportional to them).
    for (int a = 0; a < 2; a++) {
        const float have = std::fabs(proj[a * 5]);
        if (have > 1e-4f && std::isfinite(have)) proj_k[a] = std::clamp(proj_k[a] * want_cot[a] / have, 0.05f, 20.0f);
    }
    update_corrections();  // the eyes' poses relative to the head can change (IPD dial, a canted runtime)
    vp_m = M4::from_d3d(proj) * M4::from_d3d(view);
    vp_m.to_gl(vp_gl);
    vp_valid = true;
    M4 vinv, pinv;
    c2w_valid = invert(M4::from_d3d(view), vinv) && invert(M4::from_d3d(proj), pinv);
    if (c2w_valid) (vinv * pinv).to_gl(c2w_gl);
}

double sh3vr_proj_scale(int axis, double game_value) {
    if (!active || view_mode == ViewMode::kFlat || view_mode == ViewMode::kScreen) return game_value;
    return game_value * proj_k[axis & 1];
}

int sh3vr_is_main_proj(const float proj[16]) {
    if (!corr_valid) return 0;
    for (int i = 0; i < 16; i++)
        if (std::fabs(proj[i] - p_main[i]) > 1e-4f * (1.0f + std::fabs(p_main[i]))) return 0;
    return 1;
}

int sh3vr_is_main_vp_constants(const float c[16]) {
    if (!corr_valid) return 0;
    // c2-c5 hold VP's columns (VP transposed): VP = X * P with X a rigid view when this is the frame camera's.
    float vp[16];
    for (int r = 0; r < 4; r++)
        for (int k = 0; k < 4; k++) vp[r * 4 + k] = c[k * 4 + r];
    const M4 vpc = M4::from_d3d(vp), pc = M4::from_d3d(p_main);
    M4 pinv;
    if (!invert(pc, pinv)) return 0;
    const M4 x = pinv * vpc;  // column form of X = VP * P^-1 (row-vector): X_col = P_col^-1 * VP_col
    // Rigid: last row (0, 0, 0, 1), the 3x3's rows unit length.
    if (std::fabs(x.m[3][0]) > 1e-3f || std::fabs(x.m[3][1]) > 1e-3f || std::fabs(x.m[3][2]) > 1e-3f ||
        std::fabs(x.m[3][3] - 1.0f) > 1e-3f)
        return 0;
    for (int r = 0; r < 3; r++) {
        const float n = x.m[r][0] * x.m[r][0] + x.m[r][1] * x.m[r][1] + x.m[r][2] * x.m[r][2];
        if (std::fabs(n - 1.0f) > 2e-2f) return 0;
    }
    return 1;
}

const float* sh3vr_eye_correction(void) { return corr_valid ? eye_corr : eye_ident; }
const float* sh3vr_eye_identity(void) { return eye_ident; }

int sh3vr_weapon_draw(uint32_t nv, uint32_t np, uint32_t stride, int pal_count, const int* pal_bone, float vsc[96][4]) {
    return active ? combat_weapon_draw(nv, np, stride, pal_count, pal_bone, vsc) : 0;
}

int sh3vr_flashlight_place(float* pos, float* dir, float* pos2, float* dir2) {
    return active ? combat_flashlight_place(pos, dir, pos2, dir2) : 0;
}
int sh3vr_flashlight_bone(float m[16]) { return active ? combat_flashlight_bone(m) : 0; }
void sh3vr_flashlight_level(float level) { combat_flashlight_level(level); }
int sh3vr_her_light(float vsc[96][4], int pal_count, Sh3VrBeam* beam) {
    return active ? body_light(vsc, pal_count, beam) : 0;
}
void sh3vr_item_pass(int on) { item_pass(on != 0); }

int sh3vr_weapon_passes(uint32_t nv, uint32_t np, const uint8_t* verts48, const uint16_t* tris, int ntris,
                        Sh3VrWeaponPass* out, int max) {
    return active ? parts_passes(nv, np, verts48, tris, ntris, out, max) : 0;
}
void sh3vr_weapon_pass(int pass) { parts_set_pass(pass); }

void sh3vr_scene_head(const float* bones) {
    if (active && (game.cutscene || game.demo)) ik_scene_head(bones);
}
int sh3vr_in_scene(void) { return active && (game.cutscene || game.demo) ? 1 : 0; }
int sh3vr_weapon_bones(float* bones, int n) { return active ? combat_weapon_bones(bones, n) : 0; }
int sh3vr_heather_bones(float* bones, int n, const Sh3VrModel* model) {
    return active ? ik_heather_bones(bones, n, model) : 0;
}

int sh3vr_own_hands(void) { return own_hands_now() ? 1 : 0; }

int sh3vr_native_hand(int hand) { return active && own_hands_now() ? native_hand_group(hand) : -1; }

int sh3vr_ghost_bones(float* out, int n) { return active ? ghost_bones(out, n) : 0; }

void sh3vr_ghost_pass(int on) { ghost_pass(on != 0); }

int sh3vr_body_cut(const float vsc[96][4], uint32_t stride, int main_cam, int ffp, int pal_count, Sh3VrCut* out) {
    return body_cut(vsc, stride, main_cam, ffp, pal_count, out);
}

unsigned sh3vr_panel_fbo(int* w, int* h) {
    if (!active) return 0;
    *w = panel_w;
    *h = panel_h;
    return panel_fbo;
}

void sh3vr_panel_drawn(void) { panel_used = true; }
void sh3vr_panel_box(float x0, float y0, float x1, float y1) {
    frame_box[0] = std::min(frame_box[0], x0);
    frame_box[1] = std::min(frame_box[1], y0);
    frame_box[2] = std::max(frame_box[2], x1);
    frame_box[3] = std::max(frame_box[3], y1);
}

int sh3vr_pad(Sh3VrPad* out) {
    if (!active || !have_frame) return 0;
    *out = pad;
    return 1;
}

int sh3vr_mouse(float* x, float* y, int* left, int* right) {
    if (!active || !mouse_on || vr_menu_open) return 0;
    *x = std::clamp(mouse_u, 0.0f, 1.0f);
    *y = std::clamp(mouse_v, 0.0f, 1.0f);
    *left = mouse_left;
    *right = mouse_right;
    return 1;
}

void sh3vr_rumble(float strength, float seconds) {
    // The game's own rumble (the world's shakes, its hits): "Extra vibration", the whole world.
    if (!active || strength <= 0 || opt.haptic_extra < 2) return;
    buzz(kLeft, strength, seconds);
    buzz(kRight, strength, seconds);
}

int sh3vr_hand_action(float at[3]) {
    Vec3 w;
    if (!active || !hand_action_at(w)) return 0;
    at[0] = w.x, at[1] = w.y, at[2] = w.z;
    return 1;
}

void sh3vr_hand_action_found(int event) {
    if (active) hand_action_found(event);
}

int sh3vr_hand_reach(int hand, float at[3]) {
    Vec3 w;
    if (!active || !hand_reach_point(hand, w)) return 0;
    at[0] = w.x, at[1] = w.y, at[2] = w.z;
    return 1;
}

void sh3vr_hand_hover(int hand, int event) {
    if (active) hand_hover(hand, event);
}

void sh3vr_radio(int channel, float level) {
    if (!active || channel < 0 || channel > 3 || !std::isfinite(level)) return;
    level = std::clamp(level, 0.0f, 1.0f);
    static float logged[4] = {-1, -1, -1, -1};
    if (std::fabs(level - logged[channel]) > 0.2f) {  // checking: the static's levels as the game sets them
        rt_log("VR: the radio's voice %d at %.2f", channel, level);
        logged[channel] = level;
    }
    radio_level[channel] = level;
}

void sh3vr_crash_info(FILE* f) {
    if (!active) {
        std::fprintf(f, "VR: off\n");
        return;
    }
    static const char* const kModes[] = {"flat", "first person", "at the game's camera", "cinema screen"};
    std::fprintf(f, "VR: view %s%s%s, menu %s, display %.0f Hz, frame %s\n", kModes[(int)view_mode],
                 third_person ? " (third person chosen)" : "", auto_third ? " (third person for this place)" : "",
                 vr_menu_open ? "open" : "closed", display_hz, have_frame ? "begun" : "none");
    std::fprintf(f, "VR options: first person %d, cutscenes %d, height %.0f, seated %d, room walk %d, left-handed %d, "
                    "aim by hand %d, two hands %d, melee %d, punches %d, clubs %d, holsters %d, flashlight %d, refresh %.0f, "
                    "resolution %.2f, dynamic resolution %d (the 3D pictures at %.0f%%, %ld displays missed), foveation %d\n",
                 opt.first_person, opt.cutscenes, opt.player_height_cm, opt.seated, opt.room_walk, opt.left_handed,
                 opt.aim_by_hand, opt.two_hand, opt.physical_melee, opt.punches, opt.gun_club, opt.holsters, opt.flashlight,
                 opt.refresh_hz, opt.resolution, opt.dynamic_resolution ? 1 : 0, sh3vr_picture_scale() * 100.0f,
                 hs ? hs->displays_missed() : 0L, opt.foveation);
    if (head_known())
        std::fprintf(f, "VR head (tracking): %.3f %.3f %.3f; floor %.2f m\n", fi.head.position.x, fi.head.position.y,
                     fi.head.position.z, fi.head_above_floor);
    std::fprintf(f, "VR rig: yaw %.3f, set %d; map %s\n", rig.yaw, rig.set, map.valid ? "ready" : "none");
    std::fprintf(f, "VR asked: steer %d heading %.2f speed %.2f run %d face %d aim %d hold still %d; pad buttons %04x "
                    "sticks %.2f %.2f %.2f %.2f\n", move.steer, move.heading, move.speed, move.run, move.face, move.aim,
                 move.hold_still, pad.buttons, pad.lx, pad.ly, pad.rx, pad.ry);
    body_crash_info(f);
    ik_crash_info(f);
    combat_crash_info(f);
    reload_crash_info(f);
    parts_crash_info(f);
    sounds_crash_info(f);
    struggle_crash_info(f);
}

}  // extern "C"
