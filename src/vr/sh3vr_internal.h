// SPDX-License-Identifier: GPL-3.0-or-later
// SH3 Quest VR: what the VR layer's parts share (all on the game's thread, its GL context current; no locks).
//
//   sh3vr.cpp         options, the headset's frame, the view and the eyes, the panel, the pad, movement, the C API
//   sh3vr_menu.cpp    the in-game VR menu (every option as a row), drawn with the launcher's font onto the panel
//   sh3vr_body.cpp    the hands against the world (fingers from the controls, stopped by the depth probes; a hand
//                     through a wall), the head-in-wall fade, the comfort vignette, the guide dot
//   sh3vr_ik.cpp      Heather's whole body in first person (lean, crouch, head, legs, her own arms, hands and fingers),
//                     her draws' fades and cuts, the ghost hands' pose
//   sh3vr_combat.cpp  weapons in the hand, holsters, physical melee, punches, guns as clubs, two-hand grips, the
//                     flashlight, grabbing, haptic events
//   sh3vr_reload.cpp  manual reload: magazines, shells and the battery by hand, the slide and the pump, the hints
//
// Spaces: tracking (OpenXR: Y up, -Z forward, metres) and the game's world (Y down, Z forward, 490 units a metre).
// world_point() and the others below carry tracking into the world as the eyes see it this frame (first person: at
// Heather; third person and cutscenes: at the game's camera).

#pragma once

#include <cstdint>
#include <cstdio>
#include <string>

#include "sh3vr.h"
#include "hand.h"  // dot, Mat3; before pointer.h, whose beam_pose uses dot
#include "pointer.h"
#include "rig.h"
#include "xr.h"

extern "C" {
#include "../d3d8/gl.h"
void rt_log(const char* fmt, ...);
extern char rt_game_dir[512];
extern int rt_frame;  // frames presented so far (rt.h)
}

namespace sh3q {
using namespace quest;

// The world's scale: Heather's own (measured on the PC rig, 2026-11-24, her 73 bones in play: eyes about 760 units over
// her feet, shoulder joints 154 apart, hips 436 up, knees 266 up: 450-550 units a metre, 490 putting the eye height
// below (1.65 m x 0.936) on her eyes). The PC VR mod's 360 made her 1.4 times the player and the world as big again.
constexpr float kUnitsPerMetre = 490.0f;
constexpr float kHeatherHeight = 1.65f;   // metres (the PC VR mod's player height)
constexpr float kEyeRatio = 0.936f;       // eye height / body height

// ---------------------------------------------------------------- options (sh3vr.ini; the launcher writes the same keys)

struct Options {
    bool vr = true;
    // Performance (the launcher's "Recommended performance options" are these defaults: LIST 7)
    float refresh_hz = 120.0f;  // LIST 51 (the owner: 120 Hz with AppSW, the best picture with the frame rate)
    float resolution = 1.0f;  // 1.5 to start on a Quest 3 or 3S (device_defaults; as Jak's, the owner: "highest resolution")
    bool dynamic_resolution = true;  // LIST 51: the game's 3D pictures drawn smaller while the headset falls behind
    int foveation = 2;
    bool boost = true;
    int sharpen = 0;             // 0.1.3: Off (owner 84: "no screen filters or ANYTHING like that shit on by default"; the
                                 // headset's sharpening was on, Normal, from session 7's "all vr options should be on by default")
    bool space_warp = true;      // Smoother frames (AppSW): 0.1.3 off and out of the menus (SH3 drew no motion: the
                                 // owner's 0.1.2 log, "smoother frames on 0% of them"); 0.1.4 back in both menus, the
                                 // motion drawn (sh3vr.cpp motion_frame) and on to start (vr261: motion on every frame of
                                 // play, right for standing, walking, turning and the head alone). The launcher's
                                 // Options.SPACE_WARP_ON the same; a file from before 0.1.4 gets it on once (load_options).
    bool show_performance = false;
    // View
    bool first_person = true;
    int cutscenes = 0;  // 0 dynamic (her eyes while the scene shows her, else the game's camera: LIST 48; the default), 1 first
                        // person, 2 cinema screen, 3 at the camera (0.1.3, owner 87: the row says "Dynamic"; 0.1.2's
                        // own switch "Dynamic cutscenes" off with "In the scene at the camera" is 3: load_options)
    bool auto_third_person = true;  // LIST 35: third person where the game's own camera is needed
    bool height_auto = true;     // LIST 42 (session 6): the player's height measured from the headset, standing
    float player_height_cm = 165.0f;  // "Your height" when it is not measured
    bool seated = false;
    bool room_walk = true;       // walking in the room moves Heather
    int body = 0;                // Heather's body in first person: 0 shown (her head hidden), 1 hidden (hands only)
    bool arms_follow = true;     // LIST 19: her arms reach the controllers (IK on her own bones), her own hands at the ends
    bool body_fade = true;       // LIST 43: Jak's body fade near the eyes
    bool hide_arms_near = false; // LIST 43: her arms fade from further away (12-30 cm, not 7-16)
    bool spatial_sound = true;   // LIST 34: 3D sounds placed from the head (sh3.c, the listener of 0x46CDF0 / 0x46CFE0)
    bool film_grain = false;     // the game's noise effect (session 7, the owner: "sh3's built in noise/film grain effect should
                                 // be disabled by default. can be re-enabled via in game settings"): sh3.c 0x41C090
    bool drifting_fog = false;   // owner 79 (session 16): the game's drifting fog, on a far layer in the world (d3d8.c,
                                 // sh3vr_far_layer_map); 0.1.3 off (owner 84: "drifting fog should be off by default")
    // owner 81 (session 16): the PC VR mod's picture settings ([Image] ExposureStops, Contrast, Saturation,
    // VignetteStrength; [OpenXR] EnableFXAA), sh3vr.cpp picture_pass; 100 % = the game's own look
    float brightness = 100.0f;   // %, the picture's light (a multiple of it)
    float contrast = 100.0f;     // %
    float saturation = 100.0f;   // %, "Color saturation"
    float dark_edges = 0.0f;     // %, "Darker edges" (the PC VR mod's vignette)
    bool smooth_edges = false;   // FXAA (the PC VR mod's EnableFXAA); 0.1.3 off (owner 84: no screen filters by default)
    bool her_shadow = true;      // LIST 56 (session 12, owner messages 49-50): her IK body, her weapon and a part held cast
                                 // shadows from the flashlight in the game's own stencil shadows (her_shadow_tris, d3d8.c)
    float panel_distance = 1.4f;
    float panel_width = 1.2f;
    bool panel_follow = true;    // the screen comes round when the head turns far from it
    bool menus_in_view = true;   // session 11 (owner 44): a screen (menus, inventory, map) opens where you look, fades
    bool messages_at_things = true;  // session 11 (owners 42-43): the game's messages by the thing they are about
    // Comfort
    int turn = 1;                // 0 snap, 1 smooth: 0.1.3 smooth (owner 84: "smooth turn should be on by default")
    float snap_degrees = 45.0f;
    float smooth_degrees = 120.0f;  // a second, the stick fully over
    int move_by = 0;             // 0 head (the default, the owner 2026-11-24), 1 the left hand
    bool vignette = false;       // the comfort vignette while turning or moving: 0.1.3 off (owner 84: no screen filters)
    float vignette_strength = 50.0f;  // %
    bool head_fade = true;       // the head in a wall darkens the view
    // Pointer
    bool pointer = true;
    int pointer_hand = 0;        // 0 either (the last trigger pulled), 1 left, 2 right
    int beam_look = 0;           // 0 Silent Hill (rust red), 1 white
    // Hands and weapons
    bool left_handed = false;
    bool aim_by_hand = true;     // the gun aims where the controller points
    bool quick_raise = true;     // a pull raises the gun and fires at once (sh3.c: her raise and the shot's start sped up)
    bool laser = true;           // the guide dot
    bool auto_aim_first_person = false;
    bool two_hand = true;
    bool physical_melee = true;
    bool punches = true;
    bool gun_club = true;
    bool holsters = true;
    bool holster_pouch = true;   // supplies
    int flashlight = 0;          // the chest flashlight aims: 0 where the eyes look, 1 where she faces (the game's)
    bool chest_light_touch = true;
    bool finger_bending = true;
    bool ghost_hands = true;
    bool grabbing = true;
    bool take_by_hand = true;    // LIST 45: items taken by closing the hand on them (the game's own pick-up runs)
    bool manual_reload = true;   // LIST 46: guns reloaded with the hands (sh3vr_reload.cpp)
    bool reload_hints = true;    // LIST 46: the reload's next step on the screen, the first three reloads of a run
    bool hand_sounds = true;     // a sound for every hand action (holsters, the pouch, the slide, a hand on a wall...: sh3vr_sounds.cpp)
    bool wrist_health = true;    // LIST 49: the health bar on her left wrist, where her watch is, shown when looked at
    bool blocking = true;        // LIST 50: a raised weapon or arm takes a monster's hit (less damage)
    bool shake_free = true;      // owner message 38: shaking the controllers counts as the game's button mashing to get free
                                 // when she is knocked down or held (sh3vr_combat.cpp struggle_frame; sh3.c 0x4D44E0)
    // Vibration (LIST 23, 44; the keys stay haptics* so earlier sh3vr.ini files keep their settings)
    bool haptics = true;
    float haptics_strength = 100.0f;  // %
    bool heartbeat = true;
    bool damage_pulse = true;
    bool haptic_surfaces = true; // walls and floors under the hands, harder the harder a hand presses
    int haptic_extra = 2;        // 0 off, 1 pickups (and doors), 2 the whole world (steps, landings, knock-downs...)
    bool haptic_weapon_change = true;
};
extern Options opt;

enum class Kind { kSwitch, kChoice, kNumber };
struct OptionRow {
    const char* key;            // sh3vr.ini
    const char* name;           // as players say it
    const char* page;
    Kind kind;
    void* value;                // bool*, int* (choices), float* (numbers; choices too when `values` is set)
    float lo, hi, step;         // numbers
    const char* const* choices; // choices: their names
    int count;
    const float* values;        // a float option chosen from a list (refresh rate)
    const char* unit;           // numbers: shown after the value
    bool next_start;            // takes effect when the game starts again
};
const OptionRow* option_rows(int& count);
const char* const* option_pages(int& count);
void load_options();
bool save_options();
std::string option_value_text(const OptionRow& r);
void option_step(const OptionRow& r, int dir);  // -1 / +1; a switch flips either way

// ---------------------------------------------------------------- the frame (sh3vr.cpp)

enum class ViewMode { kFlat, kFirstPerson, kCamera, kScreen };

extern Headset* hs;
extern bool active;
extern FrameInfo fi;
extern bool have_frame;
extern float display_hz;
extern Sh3VrGame game;
extern ViewMode view_mode;
extern bool third_person;      // the player's toggle (the left menu button held)
extern bool auto_third;        // LIST 35: the game's camera taken for a place that needs it
extern bool vr_menu_open;
extern Sh3VrMove move;
extern Controls prev;          // last frame's controls
extern int point_hand;         // the hand that points at screens (0 left, 1 right)
extern bool mouse_on;          // the pointer is on a screen this frame
extern bool catching_up;       // Heather walks to the head (room walk)
extern float blocked_seconds;  // ... and has not got closer: something holds her

bool head_known();
bool in_play();                // playable, no cutscene or demo, no VR menu
float frame_dt();              // the display's period: the game's own tick (one a frame)
float motion_dt();             // 0.1.3: the real seconds since the last frame (hand speeds, swings, smooth turn)
int main_hand();               // kRight, or kLeft when left-handed
// LIST 45: the spot (world) a closing hand asks the game's action at this frame, and the event the game found there.
bool hand_action_at(Vec3& world);
void hand_action_found(int event);
// Session 11: a free hand reaching out (its palm, world) for the action check without the button, and what it found.
bool hand_reach_point(int hand, Vec3& world);
// Session 11: where a hand last used something (world) if within `seconds`; the GL link helper (sh3vr_body.cpp).
bool recent_hand_use(Vec3& world, float seconds);
unsigned gl_link_program(const char* vs, const char* fs, const char* what);
void hand_hover(int hand, int event);
int off_hand();

Vec3 to_game(Vec3 v);
Quat to_game(Quat q);
Vec3 game_yaw(Vec3 v, float yaw);
Quat game_yaw_quat(float yaw);
float heading_of(Vec3 f);
float wrap(float a);
Vec3 normalized(Vec3 v);
Vec3 cross(Vec3 a, Vec3 b);
Vec3 lerp(Vec3 a, Vec3 b, float t);

// Tracking <-> the game's world this frame (false from map_ready: no view yet).
bool map_ready();
Vec3 world_point(Vec3 tracking);
Quat world_quat(Quat tracking);
Vec3 world_dir(Vec3 tracking_dir);
Vec3 tracking_point(Vec3 world);
Quat tracking_quat(Quat world);
Vec3 heather_chest_world();    // first person: where her chest is (the flashlight's place)
float player_height_m();       // the player's height (m): measured standing (the default) or "Your height"

// The frame camera as the eyes drew it (for VR's own drawing and the probes).
bool camera_ready();
const float* frame_vp();               // 16 floats, column-major: P * V of the frame camera (D3D clip space)
const float* eye_corrections();        // 32 floats: the two eyes' mat4 (uEye)
float linear_depth(float d);           // a depth-buffer value (D3D z / w, 0..1) to the eye's distance (game units)
// A world point (game units) through eye `e`'s full transform: D3D clip space.
void clip_of(int e, Vec3 world, float out[4]);

// Haptics (the Haptics option and strength applied).
void buzz(int hand, float strength, float seconds, float hz = 160.0f);

// Comfort: the vignette's strength wanted this frame (0..1) and the head-in-wall darkening (0..1).
float comfort_vignette();
float head_in_wall();

// ---------------------------------------------------------------- the hands (sh3vr_body.cpp)

enum Finger { kThumb = 0, kIndex, kMiddle, kRing, kPinky };

struct HandState {
    bool valid = false;
    Pose grip, aim;            // tracking
    Vec3 vel;                  // tracking, m/s (smoothed; the room's, not Heather's walk)
    float speed = 0;           // |vel|
    float want[5]{};           // finger curls the controls ask (0 open .. 1 fist)
    float curl[5]{};           // as shown (stopped by the world; below 0 bent back against a surface)
    float stop[5] = {1, 1, 1, 1, 1};  // the world lets each finger close this far (the probes, a frame late)
    bool inside = false;       // the hand went through a surface: it is shown at `contact`, its ghost where it is
    Vec3 contact;              // tracking
    Vec3 push;                 // tracking: the palm moved out of the surface it is at (session 8), eased as it lets go
    bool resting = false;      // the palm rests on a surface (a plane fitted where it is)
    bool touching = false;     // a surface at the fingertips (a haptic tick when it starts)
    bool holding_weapon = false;  // the weapon is in this hand (sh3vr_combat.cpp)
    bool on_foregrip = false;     // this (off) hand holds the weapon's other grip
    Pose shown;                // tracking: the pose the hand is drawn at
};
extern HandState hands[2];

void body_frame();             // begin_next_frame: poses, fingers, the probes' answers
void body_draw(unsigned bb_tex, unsigned depth_tex, int w, int h);  // present: the probes, head fade, vignette
void body_crash_info(FILE* f);
// The guide dot's place (tracking) when the gun's ray met the world last frame.
bool guide_hit(Vec3& at);
float view_blocked(int* seen = nullptr, int* behind = nullptr);  // the world before the eyes in a scene in first
                                                                // person (m; 0 none that near, -1 not known)

// ---------------------------------------------------------------- Heather's body (sh3vr_ik.cpp, LIST 19, 41-44)

// One of her fingers in a controller's grip space (metres, that hand's own: not mirrored), from her model's rest pose:
// the base joint, the three segments (base to middle, middle to last joint, last joint to the tip) open, the axis it
// closes about and each joint's angle at a full fist (radians).
struct FingerGeo {
    Vec3 knuckle;
    Vec3 seg[3];
    Vec3 axis;
    float joint[3];
};
struct HerHand {
    bool valid = false;
    FingerGeo f[5];            // thumb, index, middle, ring, pinky
};
extern HerHand her_hands[2];   // the probes (sh3vr_body.cpp) use her own fingers' shapes
// A finger bent back (a curl below 0: pressed flat on a surface, or poked at a wall, as Jak's and Ratchet's fingers
// are): each joint by this share of its fist bend per unit of curl, most at the knuckle, down to kFingerBackMost.
// sh3vr_body.cpp's finger_points and sh3vr_ik.cpp's pose_fingers bend them alike. Her model's open hand is a relaxed
// one, its fingers curled toward the palm (30 degrees at the knuckle, 55 past the middle joint: vr125's hand
// geometry), so -0.4 with these shares lays them flat (within 10 degrees each), not past it: a hand pressed flat on a
// table or a wall lies flat on it (vr125: at -0.3 the fingertips held the palm 2.5 cm off the floor). The thumb's own
// fist bend is small (25 degrees at its root) and it rests 26 to 31 degrees toward the palm: three times its share at
// the root spreads it 30 degrees back, beside the palm as a hand laid flat has it (vr126: it held the palm 1.8 cm off
// the floor). [0] the thumb, [1] the fingers.
constexpr float kFingerBackMost = -0.40f;
constexpr float kFingerBack[2][3] = {{3.0f, 0.5f, 0.5f}, {1.0f, 0.45f, 0.6f}};
struct Capsule {
    Vec3 a, b;                 // world
    float r;                   // game units
};

int ik_heather_bones(float* bones, int n, const Sh3VrModel* m);
void ik_scene_head(const float* bones);  // LIST 48: her head as a scene poses her model away from her place
// The game's own pose of her hand bone h (world: its local axes, unit, and origin; the frame it came in), before VR moved
// it: the weapon's place against it is the game's grip (sh3vr_combat.cpp's native grips).
bool ik_game_hand(int h, Vec3 axes[3], Vec3& origin, int& frame);
// Her hand bone for a controller grip pose (tracking: the bone's local axes, unit; its origin, metres), and back.
bool ik_hand_bone_for_pose(int h, const Pose& pose, Vec3 axes[3], Vec3& origin);
bool ik_pose_for_hand_bone(int h, const Vec3 axes[3], Vec3 origin, Pose& out);
// Her shoulder (tracking, metres) and her arm's reach from it to the hand bone (metres), as the arm was last solved.
bool ik_arm_reach(int h, Vec3& shoulder, float& reach);
extern int arms_ik_frame;      // the frame her arms were last put at the controllers
extern Vec3 game_head;         // LIST 48: the middle of her head as the game's animation posed it (before VR), world ...
extern int game_head_frame;    // ... in this frame
extern Vec3 heather_head;      // world: the middle of her head (her head bone, as posed) ...
extern int heather_head_frame; // ... in this frame (the first-person cut's sphere; -1 never)
bool own_hands_now();          // her own finger hands are posed this frame (sh3vr_own_hands)
int ghost_bones(float* out, int n);
void ghost_pass(bool on);
void item_pass(bool on);  // the flashlight drawn in the hand (sh3.c)
int body_cut(const float vsc[96][4], uint32_t stride, int main_cam, int ffp, int pal_count, Sh3VrCut* out);
// Her arms, hands and fingers this frame (world): the probes leave out what they see of her.
int own_capsules(Capsule* out, int max);
bool ik_forearm(int h, Vec3& elbow, Vec3& wrist, Vec3* watch_out = nullptr);  // LIST 50: her forearm as drawn this
// frame (world); `watch_out`: out of the back of her wrist as its twist bone turns it (LIST 49; zero when not solved)
void ik_crash_info(FILE* f);
Vec3 tracking_dir(Vec3 world_dir);

// The frame camera's clip space back to the world (16 floats, column-major); false before the camera is known.
bool clip_to_world(float out[16]);

// ---------------------------------------------------------------- combat (sh3vr_combat.cpp)

void combat_frame();           // begin_next_frame after body_frame
void combat_pad(Sh3VrPad& p, uint16_t& add_buttons, bool& block_attack);  // map_pad in play (first person)
bool weapon_shown_in_hand();   // a weapon is drawn at the main hand (first person)
bool combat_gun_in_hand();
// Owner 61 (session 14): the beam saber in the VR hand with physical melee on: the main trigger is the game's ready alone
// (its beam lit, action 7), the attack the swings' own (vr202: the attack held with the ready gave action 37, no beam).
bool combat_trigger_readies();
// For a while after a shot, a world point (game units) of one of the game's screen-space effects that is the gun's own
// (within half a metre of the game's gun): the map that puts it at the gun in the hand (column form: R p + t), and for
// the log its distance from the game's gun, that gun's place and where the flash's middle goes (sh3vr_effect_map).
bool combat_effect_rel(const float point[3], int tex_w, float r[3][3], float t[3], float info[7]);     // a gun or the stun gun is in the main hand (first person)
// Owner 61 (session 15): the beam saber's line (game units, world), palm to the hold's tip, as the game's animated hand
// holds it (where the game draws its beam's sprites) and as the VR hand holds it; false unless the saber is in the VR
// hand in first person with both places live (sh3vr.cpp sh3vr_beam_sprite).
bool combat_beam_line(float game_a[3], float game_b[3], float vr_a[3], float vr_b[3]);
// ... and the map (column form: R p + t, game world) that moves what the game places at its saber to the VR hand's.
bool combat_beam_map(float r[3][3], float t[3]);
bool combat_weapon_world(float m[16], const float base[16]);  // the game's weapon (D3D 4x4 in her space, base her world) moved to the hand (sh3.c 0x495360)
// The gun's muzzle and pointing (tracking) for the guide dot and the shots; false: no gun in hand.
bool gun_ray(Vec3& from, Vec3& dir);
// The game's weapon (in her animated hand) to the weapon drawn in the hand, a world point and a direction (its shots,
// muzzle flash, light and effects: sh3vr_weapon_to_hand); false when no weapon was drawn in the hand last frame.
// from_hand: the place was read from her animated hand bone, not from the weapon's own (already moved) matrix.
bool combat_weapon_map(Vec3& point, Vec3& dir, bool from_hand);
bool weapon_guide_dot();     // the weapon in hand shows the guide dot (its ShowGuideDot)
bool melee_hold_still();
const char* weapon_name(int slot);
void combat_crash_info(FILE* f);
int combat_weapon_draw(uint32_t nv, uint32_t np, uint32_t stride, int pal_count, const int* pal_bone, float vsc[96][4]);
int combat_weapon_bones(float* bones, int n);  // LIST 24: the extras carried to her IK hand (sh3.c 0x45EC00 wrapper)
// The weapon in the main hand (first person, drawn there): a point of its model (model units, the held bone's) in
// tracking space, through the same frame the draw uses. false: no weapon placed in the hand.
bool weapon_point(Vec3 model, Vec3& tracking);
// ... and a direction of its model (unit, tracking).
bool weapon_dir(Vec3 model, Vec3& tracking);
// ... a tracking point in its model (model units).
bool weapon_model_point(Vec3 tracking, Vec3& model);
// ... its palm point (tracking) and its barrel's or blade's pointing (unit, tracking).
bool weapon_forward(Vec3& base, Vec3& fwd);
// (nv, np) is the weapon in the hand's mesh (first person).
bool weapon_signature(uint32_t nv, uint32_t np);
// The weapon's line as the controller holds it (tracking: the palm to the tip), for its probes against the world.
bool weapon_reach(Vec3& from, Vec3& to);
// The drawn weapon's shape (world capsules): the probes take it for the weapon, not the world.
int weapon_capsules(Capsule* out, int max, bool with_flash = true);  // with_flash: the muzzle flash just after a shot (not for shadows)
int her_shadow_tris(const float view[16], const float proj[16], float* out, int max_tris, int counts[5]);  // LIST 56 (session 12)
// LIST 56 (session 14): a skinned draw of hers in the eyes (kind 1: the weapon or a part in her hand) for her shadow
void shadow_part(const uint8_t* v48, int nv, const uint16_t* tris, int nt, const float* c, uint32_t key, int kind);
bool ik_chest_carry(Vec3 p, Vec3& out);  // a point on her chest as the game posed it, where VR's pose puts it (the chest lamp)
bool ik_hand_carry(int h, float* m, bool on_hand);  // LIST 24: a bone on her animated hand carried to her IK hand (the extras)
// The distance (metres) from a tracking segment to the weapon's surface (negative inside): its capsules, and for the
// other hand the main hand's fist round the grip. 1e9: no weapon placed.
float weapon_clearance(int hand, Vec3 a, Vec3 b);
// How a hand holds the weapon: 0 not, 1 the main hand round its grip, 2 the other hand on its second grip, 3 the other
// hand on the slide or the cocking knob. Its fingers' curl round that grip when nothing stops them (the measured one).
int weapon_hand_role(int hand);
float weapon_two_share(int hand);  // the other hand's share of the second grip, eased (0 .. 1)
float weapon_hold_curl(int hand);
// The game's own gripping hand shown for hand h on the weapon held (its group in Heather's model), -1 her finger hand.
int native_hand_group(int hand);
bool weapon_is_gun();
// A hand's pose (OpenXR's grip pose, tracking) round a grip: its fist's centre at `at`, its rod (little finger to
// thumb) along `rod`, its palm facing `palm` (unit).
Pose hand_pose_round(int hand, Vec3 at, Vec3 rod, Vec3 palm);
// The rotation whose columns are the unit axes x, y, z (a right-handed frame).
Quat quat_from_axes(Vec3 x, Vec3 y, Vec3 z);
// LIST 49: her watch on the back of hand h's wrist (sh3vr_body.cpp; grip space, from her forearm as drawn): its
// middle, along her forearm to the hand, out of the back of the wrist.
bool watch_place(int h, Vec3& at, Vec3& along, Vec3& back);  // false until her forearm is first known
// The rest of combat_frame once the hands are posed (sh3vr_body.cpp: the fingers on the weapon and the parts).
void body_fingers();
// One of hand h's fingertips at curl c, in its grip space (metres).
bool hand_finger_tip(int h, int f, float c, Vec3& tip);
bool body_light_place(int h, Vec3& centre, Vec3& lens);  // the flashlight box's middle and where its beam leaves
                                                         // (the lens' clear part) in hand h's grip space
// The weapon against the world (its probes, sh3vr_body.cpp): the free part of its line (1: free).
float weapon_wall_free();
int combat_flashlight_place(float* pos, float* dir, float* pos2, float* dir2);
int combat_flashlight_bone(float m[16]);
int combat_light_hand();  // the hand holding the flashlight, -1 on her chest
bool combat_light_held();  // the beam leaves a hand's flashlight this frame (else her chest's, or the game's)
void combat_flashlight_level(float level);  // the light's level this frame (0 off .. 1 on; sh3.c, 0x457AC0)
bool combat_beam(Vec3& pos, Vec3& dir, float& level);  // the beam as last placed (world) while the light is on
int body_light(float vsc[96][4], int pal_count, Sh3VrBeam* beam);  // her own draw lit by her flashlight (sh3vr_ik.cpp)
bool last_draw_was_weapon();
int combat_melee_volume(int weapon_slot, Sh3VrHit* hit);
int combat_extra_hit(Sh3VrHit* out);
void combat_melee_landed(const float way[3], const float point[3], uint32_t character, int battle_id);
float combat_hit_given(uint32_t character, const float way[3], const float point[3], float damage, int reaction);
// LIST 50: what a monster's hit on her keeps of its damage (0..1) as her raised weapon or arm guards it (world: the
// hit's point and way, the attacker's place if has_from).
float combat_block(const float at[3], const float way[3], const float from[3], int has_from, float damage, int kind);
void combat_actions(Sh3VrActions* out);
void combat_shot();

// ---------------------------------------------------------------- manual reload (sh3vr_reload.cpp, LIST 46)

void reload_frame();                    // combat_frame, after the weapon is known, before the holsters
bool reload_pouch_on();                 // the ammo pouch (sh3vr_combat.cpp's kAmmo) gives the off hand a part now
bool reload_take_from_pouch(int hand);  // the off hand closed at the ammo pouch: true when the reload took the grip
bool reload_owns_hand(int hand);        // a part held or the slide worked: no two-hand grip, no grabbing with it
bool reload_blocks_fire();              // the magazine out, or the slide / pump still to work
bool reload_blocks_game();              // sh3vr_block_game_reload
void reload_game_event(int weapon, int event_index);
bool reload_fingers(int hand, float out[5]);  // the off hand's shape round a magazine, a shell, the slide
void reload_actions(Sh3VrActions* out);
unsigned reload_hint_texture(int& w, int& h);  // the hint's strip (top row first), 0 none shown
void reload_shot();                     // Heather fired: the handgun's slide cycles
float reload_part_back(int item);       // how far the gun's slide, knob or pump is back now (m)
float reload_pump_back();               // the shotgun's pump drawn back by the other hand (m), 0 none
bool reload_magazine_out(int item);     // the gun's magazine is out of it (dropped, the new one not yet seated)
int reload_held_part(int hand);         // the part (PartKind) the hand holds, -1 none
// The other hand on the slide or the cocking knob: where it is shown (it stays on the part as the part moves).
bool reload_hand_spot(int hand, Pose& shown);
void reload_crash_info(FILE* f);

// ---------------------------------------------------------------- the weapon's passes, the parts (sh3vr_parts.cpp)

enum PartKind { kPartNone = -1, kPartHandgunMag = 0, kPartSmgMag, kPartShell, kPartBattery, kPartCount };
// A pass's placement: 0 the gun's own palette; 1 every palette bone times `m` (model units: a moving part); 2 every
// bone `m` (world: VR's own mesh). m: a row-major 3x4.
struct PassPlace {
    int kind = 0;
    float m[12]{};
};
int parts_passes(uint32_t nv, uint32_t np, const uint8_t* verts48, const uint16_t* tris, int ntris, Sh3VrWeaponPass* out,
                 int max);
void parts_set_pass(int pass);
int parts_pass();                      // the pass being drawn (0: the gun's body)
bool parts_pass_place(int pass, PassPlace& out);
// The part a hand holds as capsules in that hand's grip space (metres): her fingers close on it.
struct PartCaps {
    int n = 0;
    Vec3 a[3], b[3];
    float r[3]{};
};
bool parts_hand_caps(int hand, PartCaps& out);
bool parts_held_tip(int hand, Vec3& tip, Vec3& axis);  // tracking: the end that goes into the gun, and its way
bool parts_held_ends(int hand, Vec3& bottom, Vec3& top, Vec3& front);  // tracking: its ends and its front's way
bool parts_in_hand(int hand, int kind, Vec3& c, Quat& q);  // the part's frame in the hand's grip space (m)
float parts_top(int kind);             // its top's local y (model units)
float parts_length(int kind);          // bottom to top (m)
void parts_drop(int kind, Vec3 bottom, Vec3 axis, Vec3 front, Vec3 vel);  // tracking: a magazine falls from the gun
void parts_frame();
void parts_crash_info(FILE* f);

// ---------------------------------------------------------------- the hands' sounds (sh3vr_sounds.cpp)

enum HandSound {
    kSoundMagRelease = 0,   // the magazine let go from the gun
    kSoundPartTaken,        // a magazine, shell or battery taken from the pouch
    kSoundMagSeated,        // a magazine (battery) pushed home
    kSoundShellIn,          // a shell pushed into the tube
    kSoundSlideBack,        // the slide or the knob pulled back
    kSoundSlideForward,     // ... let go forward
    kSoundPumpBack,
    kSoundPumpForward,
    kSoundHolsterDraw,
    kSoundHolsterPut,
    kSoundPouch,            // the supplies pouch
    kSoundLightPocket,      // the flashlight's pocket
    kSoundTake,             // an item taken by hand
    kSoundDryClick,         // the trigger on a gun that cannot fire
    kSoundPartFloor,        // a dropped part hits the floor
    kSoundWeaponWall,       // a weapon knocked against a wall
    kSoundGrabWeapon,       // the other hand takes the weapon's second grip
    kSoundHandSurface,      // a hand laid on a wall, a table or the floor (session 8)
    kSoundStruggle,         // a shake of a controller counted toward breaking free of a hold (owner message 38)
    kSoundMenu,             // the VR menu opened (detail 1) or closed (0) (owner 80, session 16: held left menu button)
    kSoundCount
};
// A hand action's sound at a place (world), heard from there (session 9: mixed over the game's sound, src/win32/dsound.c
// ds_vr_play); `detail`: the part or weapon kind, or a speed (cm a second) for a touch; kSoundByGame + it when the
// game's own sound for it plays (the reload's): logged, not played twice.
constexpr int kSoundByGame = 1000;
void hand_sound(int what, Vec3 world, int detail = -1);
void sounds_frame();
void sounds_crash_info(FILE* f);
// Owner message 38 (sh3vr_combat.cpp): shake to break free. struggle_frame once a frame; struggle_presses from the game's
// struggle step (the presses it is given, 0..4); struggle_done after it (1: she is free; the buttons the game counted).
void struggle_frame();
int struggle_presses(float left);
void struggle_done(int freed, int buttons);
void struggle_crash_info(FILE* f);

// ---------------------------------------------------------------- the VR menu (sh3vr_menu.cpp)

void menu_open(const char* why);
void menu_close();
// One frame of the controllers while the menu is up (the game gets none of them meanwhile).
void menu_update(const Controls& c, bool point_valid, float u, float v, bool press);
// The menu's picture (panel-sized, RGBA), redrawn when it changed; 0 while closed.
unsigned menu_texture();

}  // namespace sh3q
