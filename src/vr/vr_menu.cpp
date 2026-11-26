// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 the Jak and Daxter Quest VR contributors
//
// The in-game VR menu (vr_menu.h). Row names are the plain ones a player says ("Fists on the right grip"), the same as
// the launcher's; each row has a line of help under the list.

#include "vr_menu.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <mutex>
#include <string>
#include <vector>

#include "common/log/log.h"
#include "common/versions/versions.h"

#include "game/party.h"
#include "game/vr/vr_options.h"

namespace jakvr::menu {

namespace {

enum class Kind { kHeader, kToggle, kChoice, kNumber, kAction, kBack };

struct Row {
  Kind kind;
  std::string label;
  std::string help;
  int (*get)(const Options&) = nullptr;
  void (*set)(Options&, int) = nullptr;
  int low = 0, high = 1, step = 1;        // kNumber: its range and step; kChoice: 0 .. names - 1
  const char* const* names = nullptr;     // kChoice
  const char* unit = "";                  // kNumber: after the number
  bool tenths = false;                    // kNumber: shown as tenths (melee speed in m/s)
  bool next_start = false;                // used when the game starts
  bool guns_only = false;                 // Jak II and 3 only
  bool jak1_only = false;                 // Jak 1 only (multiplayer)
  bool jak2_only = false;                 // Jak II only (its multiplayer mod: 0.1.3)
  unsigned games = 0;                     // 0.1.5: shown only in these games (1 << GameVersion), 0 in all
  std::function<std::string()> value;      // kAction: what it shows
  std::function<void(int by)> act;         // kAction: a press (0), or left and right (-1, +1)
  bool cycles = false;                     // kAction: its value's halves step it back and on (the pointer)
  std::function<std::string()> live_help;  // help that changes with the game (the party's state)
};

#define VR_TOGGLE(label, field, help)                                          \
  Row{Kind::kToggle, label, help, [](const Options& o) { return o.field ? 1 : 0; }, \
      [](Options& o, int v) { o.field = v != 0; }}
#define VR_NUMBER(label, field, low, high, step, unit, help)                    \
  Row{Kind::kNumber, label, help, [](const Options& o) { return o.field; },        \
      [](Options& o, int v) { o.field = v; }, low, high, step, nullptr, unit}
#define VR_CHOICE(label, field, type, names, help)                                                   \
  Row{Kind::kChoice, label, help, [](const Options& o) { return static_cast<int>(o.field); },          \
      [](Options& o, int v) { o.field = static_cast<type>(v); }, 0,                                     \
      static_cast<int>(sizeof(names) / sizeof(names[0])) - 1, 1, names}

const char* const kView[] = {"First person", "Third person"};
const char* const kCutscenes[] = {"Through his eyes", "Around you", "On a screen"};
const char* const kTurn[] = {"Snap", "Smooth"};
const char* const kMove[] = {"Where you look", "Where your left hand points"};
const char* const kDpad[] = {"Left hand at your head", "Left thumb on its rest"};
const char* const kMenuHand[] = {"Either hand", "Right hand", "Left hand"};
// vr_options.h BodyShown, in its order
const char* const kBodyShown[] = {"Whole body",    "Body and arms", "Body and legs", "Arms and legs",
                                  "Arms only",     "Legs only",     "Hands only"};
const char* const kFoveation[] = {"Off", "Low", "Medium", "High", "Highest"};
const char* const kFrameRate[] = {"60", "The headset's", "Half the headset's", "Automatic"};
const char* const kSharpen[] = {"Off", "Normal", "Strong"};
// crosshair.h's kinds and colors, in their order (0.1.3; 0.1.5 the Small circle)
const char* const kCrosshair[] = {"Off",  "Dot",  "Cross", "Cross and dot", "Circle", "Circle and dot",
                                  "Plus", "Small circle"};
const char* const kCrosshairColor[] = {"White", "Green", "Yellow", "Cyan", "Red", "Pink"};
// vr_options.h extra_vibration, in its order (0.1.4)
const char* const kExtraVibration[] = {"Off", "Pickups", "The whole world"};
const char* const kRefresh[] = {"The headset's own", "72 Hz", "80 Hz", "90 Hz", "120 Hz"};
constexpr int kRefreshHz[] = {0, 72, 80, 90, 120};
const char* const kMsaa[] = {"Off", "2x", "4x"};
const char* const kAnisoNames[] = {"Off", "2x", "4x", "8x", "16x"};
constexpr int kAnisotropy[] = {1, 2, 4, 8, 16};

// The menu's pages: VR Options, and Multiplayer's (Jak 1), as Ratchet's Multiplayer asks: "Host which save?", the
// games to join, "Play which save?" (shown on joining one) and the party's players. 0.1.6 (LIST-0.1.6 item 22, the
// owner: a Multiplayer section in the games' main menus, "like the openrac multiplayer mod"): Multiplayer, a page of
// its own, opened by Jak 1's title and pause menus (open_multiplayer); there it is the menu's home page.
enum Page { kMain, kHostSave, kGames, kPlaySave, kPlayers, kMultiplayer };
const char* const kPageTitle[] = {"VR Options", "Host which save?", "Join a game", "Play which save?", "Players",
                                  "Multiplayer"};
void ask_page(int page);  // after this frame's input (the rows are rebuilt)
void add_jak1_multiplayer_rows(std::vector<Row>& r, bool with_header);

// The party's state while something is happening (or went wrong), else the row's own help.
std::string party_help(const char* idle_help) {
  const std::string now = party::status();
  return now == "Not in a game." || now == "Multiplayer is off." ? std::string(idle_help) : now;
}

// What is happening with a save ("Asking the host for its progress..."), else the row's own help.
std::string save_help(const std::string& idle_help) {
  const std::string now = party::save_status();
  return now.empty() ? idle_help : now;
}

std::vector<Row> make_rows() {
  std::vector<Row> r;
  r.push_back({Kind::kHeader, "View", ""});
  r.push_back(Row{Kind::kChoice, "View", "How you see play: through Jak's eyes, or the game's camera around you. The left menu button held a second switches too.",
                  [](const Options& o) { return o.first_person ? 0 : 1; },
                  [](Options& o, int v) { o.first_person = v == 0; }, 0, 1, 1, kView});
  r.push_back(VR_CHOICE("Cutscenes", cutscene_view, CutsceneView, kCutscenes,
                        "How scenes are seen: through his eyes where he is in them, in VR around you, or on a big screen."));
  r.push_back(VR_TOGGLE("Adaptive cutscenes", adaptive_cutscenes,
                        "A scene seen through his eyes goes around you by itself while Jak is not really in it, and back when he is."));
  r.push_back(VR_TOGGLE("Talks in first person", first_person_in_talks,
                        "A talk during play seen from his eyes too, rather than the game's camera around you."));
  // 0.1.7 (LIST-0.1.7 item 4)
  r.push_back(VR_TOGGLE("Third person after a pole until you land", pole_third_person,
                        "After a pole swing the view stays third person until his feet are on the ground, so your aim "
                        "does not jump in the air. Off: back in his eyes as soon as he lets go."));
  // 0.1.7 (LIST-0.1.7 item 2)
  r.push_back(VR_TOGGLE("Eco sparks in first person", eco_sparks_first_person,
                        "The sparks eco throws off Jak's body, seen from his eyes too. Off: left out while you see "
                        "through his eyes (they burst right at your eyes); the eco still works and sounds the same."));
  // 0.1.9 (LIST-0.1.9 item 20; the owner: "making the fishing minigame work in first person vr"), Jak 1's
  {
    Row row = VR_TOGGLE("Fishing game in first person", fishing_first_person,
                        "The fisherman's game seen from Jak's eyes, the net in your hand: point it where the fish "
                        "fall. Squeeze the other grip to take the net in that hand. Off: the game's own camera, the "
                        "net following your raised hand side to side.");
    row.games = 1u << static_cast<int>(GameVersion::Jak1);
    r.push_back(row);
  }
  r.push_back(VR_TOGGLE("First person in vehicles", first_person_vehicles,
                        "Everything you ride seen from his eyes too, turning with it. Each kind below has its own switch as well."));
  // 0.1.5 (LIST-0.1.5 item 18; a player: "can you make vehicle types seperate 1st person toggles, for example hover
  // car, hover bike, lizard, hover board?"): each kind of ride's own switch, in the games that have it
  {
    constexpr unsigned j1 = 1u << static_cast<int>(GameVersion::Jak1);
    constexpr unsigned j2 = 1u << static_cast<int>(GameVersion::Jak2);
    constexpr unsigned j3 = 1u << static_cast<int>(GameVersion::Jak3);
    auto ride = [&r](Row row, unsigned games) {
      row.games = games;
      r.push_back(std::move(row));
    };
    ride(VR_TOGGLE("Zoomer in first person", fp_zoomer,
                   "With First person in vehicles on: the Zoomer from his seat. Off: the game's camera there."), j1);
    ride(VR_TOGGLE("Flut Flut in first person", fp_flut_flut,
                   "With First person in vehicles on: the Flut Flut from his seat. Off: the game's camera there."), j1);
    ride(VR_TOGGLE("JET-Board in first person", fp_jet_board,
                   "With First person in vehicles on: the JET-Board from his eyes. Off: the game's camera there."), j2 | j3);
    ride(VR_TOGGLE("Hover cars in first person", fp_hover_cars,
                   "With First person in vehicles on: the city's cars from his seat. Off: the game's camera there."), j2 | j3);
    ride(VR_TOGGLE("Hover bikes in first person", fp_hover_bikes,
                   "With First person in vehicles on: the city's bikes (and the race bikes) from his seat. Off: the game's camera."),
         j2 | j3);
    ride(VR_TOGGLE("Titan suit in first person", fp_titan_suit,
                   "With First person in vehicles on: the Titan suit from his seat. Off: the game's camera there."), j2);
    ride(VR_TOGGLE("Leaper lizards in first person", fp_leaper_lizards,
                   "With First person in vehicles on: the leaper lizards from his seat. Off: the game's camera there."), j3);
    ride(VR_TOGGLE("Desert vehicles in first person", fp_desert_vehicles,
                   "With First person in vehicles on: the wasteland's vehicles from his seat. Off: the game's camera there."), j3);
    ride(VR_TOGGLE("Other rides in first person", fp_other_rides,
                   "With First person in vehicles on: slides, gun turrets and anything else he rides. Off: the game's camera there."),
         j1 | j2 | j3);
  }
  r.push_back(VR_NUMBER("Vehicle view height", vehicle_view_height, -50, 50, 5, " cm",
                        "First person in a vehicle: raises or lowers the view from his eyes there. His body stays in his seat."));
  r.push_back(VR_NUMBER("Vehicle view forward", vehicle_view_forward, -50, 50, 5, " cm",
                        "First person in a vehicle: the view ahead of his eyes (towards where it goes) or behind them."));
  r.push_back(VR_NUMBER("Vehicle view tilt", vehicle_view_tilt, -30, 30, 5, " degrees",
                        "First person in a vehicle: looking straight ahead you see this far down (more of the road), or up below 0."));
  r.push_back(VR_TOGGLE("Steady view when riding", steady_ride_view,
                        "First person in a vehicle or on an animal: the view stays in his seat without each bounce and shake of the ride."));
  // 0.1.8 (LIST-0.1.8 item 6)
  r.push_back(VR_TOGGLE("Steer by grabbing the wheel", wheel_steer,
                        "In any vehicle, grip its wheel, handlebars or bar where Jak's hands are (or just ahead of you) and turn your hands to steer. The stick still steers. A quick squeeze, or the stick's click while holding, is the grip's button."));
  r.push_back(VR_NUMBER("Wheel turn", wheel_turn, 20, 180, 10, " degrees",
                        "How far you turn the wheel for full steering. Handlebars and bars need a little over half of it."));
  // 0.1.9 (LIST-0.1.9 items 14 and 15)
  Row b_height = VR_TOGGLE("B for height while holding the wheel", wheel_b_height,
                           "Hover cars and bikes: while your right hand holds the wheel, B changes height and the right stick's click is the turbo. Off: B the turbo, the click the height.");
  b_height.guns_only = true;
  r.push_back(b_height);
  r.push_back(VR_TOGGLE("Lean by pushing the wheel", wheel_lean,
                        "Holding the wheel or handlebars, push them away from you to lean forward and pull them back to lean back, as the left stick does."));
  r.push_back(VR_CHOICE("Turning", turn, TurnKind, kTurn, "The right stick turns you smoothly, or in steps."));
  r.push_back(VR_NUMBER("Snap turn angle", snap_degrees, 15, 90, 15, " degrees", "How far one snap turns you."));
  r.push_back(VR_NUMBER("Smooth turn speed", smooth_degrees, 30, 360, 30, " degrees a second",
                        "How fast you turn with the stick full over."));
  r.push_back(VR_TOGGLE("Seated play", seated, "For playing sitting down: the view at his eyes whatever your height."));
  r.push_back(VR_NUMBER("View height", view_height, -40, 40, 5, " cm", "Raises or lowers the view in first person."));
  r.push_back(Row{Kind::kNumber, "World scale",
                  "First person: 100% is life size, Jak as tall as you. More makes the world bigger (145% is how 0.1.0 looked).",
                  [](const Options& o) { return static_cast<int>(std::lround(o.world_scale * 100.0f)); },
                  [](Options& o, int v) { o.world_scale = static_cast<float>(v) / 100.0f; }, 50, 200, 5, nullptr, "%"});
  r.push_back(Row{Kind::kNumber, "Third person world scale", "Third person: the size the world is seen at.",
                  [](const Options& o) { return static_cast<int>(std::lround(o.third_person_scale * 100.0f)); },
                  [](Options& o, int v) { o.third_person_scale = static_cast<float>(v) / 100.0f; }, 50, 200, 5, nullptr, "%"});
  r.push_back(VR_NUMBER("HUD size", hud_size, 50, 150, 10, "%", "How big the HUD and the menus are."));
  r.push_back(VR_NUMBER("HUD distance", hud_distance, 80, 300, 10, " cm", "How far ahead of you the HUD floats."));
  r.push_back(VR_NUMBER("HUD height", hud_height, -40, 40, 5, " cm", "Raises or lowers the HUD."));
  r.push_back(VR_TOGGLE("HUD follows your head", hud_follows,
                        "On, the HUD moves with your head. Off, it stays put ahead of you, and moves only when you look well away."));

  r.push_back({Kind::kHeader, "Controls", ""});
  // 0.1.7 (LIST-0.1.7 item 9); 0.1.9 (LIST-0.1.9 item 25): only while a gamepad is connected
  r.push_back(VR_TOGGLE("Gamepad only (no VR controllers)", gamepad_only,
                        "Play with a gamepad alone: while one is connected, the VR controllers are left out of the game. "
                        "With no gamepad connected they play. This menu still takes both."));
  r.push_back(VR_CHOICE("Move direction", move_by, MoveBy, kMove,
                        "The left stick moves Jak where you look, or where your left hand points."));
  r.push_back(VR_CHOICE("D-pad", dpad_place, DpadPlace, kDpad,
                        "What makes a stick the d-pad (gun choice, warp gates): the left hand by your head, or the left thumb resting on its rest."));
  r.push_back(VR_NUMBER("D-pad reach", dpad_reach, 10, 40, 2, " cm", "How near your head the left hand counts as there."));
  r.push_back(VR_TOGGLE("Buzz on d-pad presses", dpad_buzz,
                        "A quick pulse in the stick's hand each time a d-pad direction goes in."));
  r.push_back(VR_CHOICE("Menu pointer hand", menu_hand, MenuHand, kMenuHand, "Which hand points at this menu."));
  r.push_back(VR_TOGGLE("Stick click as menu button", stick_click_menu,
                        "When SteamVR keeps the left menu button, the left stick's click is the menu button. Off: it stays L3."));
  r.push_back(VR_TOGGLE("Vibration", vibration, "The controllers buzz."));
  r.push_back(VR_NUMBER("Vibration strength", vibration_strength, 25, 200, 25, "%", "How hard every buzz is."));
  r.push_back(VR_TOGGLE("Vibration on walls and floors", world_vibration,
                        "A buzz as a hand meets a wall or the floor, harder the harder you press."));
  r.push_back(VR_CHOICE("Extra vibration", extra_vibration, int, kExtraVibration,
                        "Pickups: a buzz for each orb, cell, gem and the like. The whole world: also steps, jumps, "
                        "landings, hits and shakes."));
  Row gun_buzz = VR_TOGGLE("Buzz on gun switch", gun_switch_buzz, "A quick pulse in the gun's hand as you change guns.");
  gun_buzz.guns_only = true;
  r.push_back(gun_buzz);

  r.push_back({Kind::kHeader, "Body", ""});
  r.push_back(VR_TOGGLE("Body", body, "Jak's body seen from his eyes, his arms reaching your hands."));
  r.push_back(VR_CHOICE("Body shown", body_shown, BodyShown, kBodyShown,
                        "What of Jak you see from his eyes. His hands always, and his shadow keeps all of him."));
  r.push_back(VR_TOGGLE("Hide arms near your eyes", hide_arms_near_eyes,
                        "An arm brought up close to your face fades out of the view, its hand staying."));
  r.push_back(VR_TOGGLE("Fade body near your eyes", fade_near_eyes,
                        "A leg kicked up by your face, and his body in a roll, a flip or a dive, fade out of the view and back."));
  r.push_back(VR_TOGGLE("Keep your body upright", steady_body,
                        "His body stays upright under your head whatever move he makes. Off: it turns and tips as the game moves him."));
  r.push_back(VR_TOGGLE("Finger tracking", fingers, "His fingers follow yours on the controllers."));
  r.push_back(VR_TOGGLE("Hands without lag", predict_hands, "His hands drawn where yours will be when the frame is shown."));
  r.push_back(VR_TOGGLE("Hands stop at walls", hands_on_world, "His hands, and what they hold, stop at walls and floors."));
  r.push_back(VR_TOGGLE("Physical crouch", physical_crouch, "Crouching down yourself crouches him."));
  r.push_back(VR_NUMBER("Crouch depth", crouch_drop, 10, 60, 5, " cm", "How far down your head goes to crouch him."));
  r.push_back(VR_NUMBER("Arm length", arm_length, 70, 150, 5, "%", "His arms longer or shorter, to fit yours."));
  r.push_back(VR_NUMBER("Body height", body_height, -30, 30, 5, " cm", "His body higher or lower under the view."));
  r.push_back(VR_NUMBER("Body forward", body_forward, -30, 30, 5, " cm", "His body ahead of or behind your head."));

  r.push_back({Kind::kHeader, "Combat", ""});
  r.push_back(VR_TOGGLE("Physical melee", hand_melee,
                        "Punch to punch, sweep a hand across to spin kick, punch upward from a crouch to uppercut."));
  r.push_back(VR_TOGGLE("Fists on the right grip", melee_grip,
                        "Grip held: a fist, only fists hit. Quick squeeze: right JET-Board, left Dark Jak. Left grip and Y held: Light Jak, then B held too: the shield."));
  // 0.1.10 (LIST-0.1.10 item 1; the owner: "where you can smack enemies with guns held in hand and that causes damage")
  Row gun_hit = VR_TOGGLE("Gun melee", gun_melee,
                          "Swing the gun in your hand hard to hit with it. Aiming, turning and drawing it never hit.");
  gun_hit.guns_only = true;
  r.push_back(gun_hit);
  Row speed = VR_NUMBER("Melee speed", melee_speed, 10, 50, 5, " m/s", "How fast a hand must move to hit.");
  speed.tenths = true;
  r.push_back(speed);
  r.push_back(VR_TOGGLE("Spin kick by spreading your arms", spread_spin_kick,
                        "Hold both arms out wide at your sides, as a T, for a spin kick. Bring them in for the next."));
  r.push_back(VR_TOGGLE("Aim with your hand", hand_aim, "The gun, and Jak 1's yellow eco, go where your hand points."));
  Row two = VR_TOGGLE("Two-hand grip", two_hand_grip, "The left hand on the gun's front steadies and aims it.");
  two.guns_only = true;
  r.push_back(two);
  Row holster = VR_TOGGLE("Draw from hip or shoulder", holster_draw,
                          "Grip at your hip or over your shoulder to draw the gun, back there and let go to put it away.");
  holster.guns_only = true;
  r.push_back(holster);
  Row holster_size = VR_NUMBER("Holster size", holster_size, 50, 200, 10, "%",
                               "How big the places at your hips and over your shoulders are, where a grip draws the gun.");
  holster_size.guns_only = true;
  r.push_back(holster_size);
  Row on_foot = VR_TOGGLE("Holsters only on foot", holster_on_foot,
                          "In a car, on the board or a turret, a grip at your hip or shoulder stays the game's own button.");
  on_foot.guns_only = true;
  r.push_back(on_foot);
  // 0.1.8 (LIST-0.1.8 item 5)
  Row ride_gun = VR_TOGGLE("Hold the gun while driving", ride_gun,
                           "In the city's bikes and cars, grip over your shoulder to draw the gun and hold it while you drive. The trigger shoots.");
  ride_gun.guns_only = true;
  r.push_back(ride_gun);
  Row angle = VR_NUMBER("Gun angle", gun_angle, -45, 45, 5, " degrees", "The gun tilted up or down from the controller.");
  angle.guns_only = true;
  r.push_back(angle);
  // 0.1.3 the gun's crosshair (LIST-0.1.3 item 27, crosshair.h)
  Row cross = VR_CHOICE("Crosshair", crosshair, int, kCrosshair,
                        "A mark on whatever your shot would hit first, the same size however far. First person, aiming with your hand.");
  cross.guns_only = true;
  r.push_back(cross);
  Row cross_size = VR_NUMBER("Crosshair size", crosshair_size, 50, 300, 25, "%",
                             "How big the crosshair looks. It stays that size near or far.");
  cross_size.guns_only = true;
  r.push_back(cross_size);
  Row cross_color = VR_CHOICE("Crosshair color", crosshair_color, int, kCrosshairColor,
                              "The crosshair's color. It has a dark edge so it shows on anything.");
  cross_color.guns_only = true;
  r.push_back(cross_color);
  Row cross_opacity = VR_NUMBER("Crosshair opacity", crosshair_opacity, 20, 100, 10, "%",
                                "How solid the crosshair is. Less lets you see through it.");
  cross_opacity.guns_only = true;
  r.push_back(cross_opacity);
  Row cross_enemy = VR_TOGGLE("Crosshair red on enemies", crosshair_enemy,
                              "The crosshair turns red on an enemy your gun can reach (yellow if yours is red).");
  cross_enemy.guns_only = true;
  r.push_back(cross_enemy);

  add_jak1_multiplayer_rows(r, true);

  // Jak II: its multiplayer mod (game/multiplayer/jak2; LIST-0.1.3 item 15) starts with the game or not at all, so it
  // is switched in the launcher's Mods only; this row says which, and where it is played.
  Row mp2_head{Kind::kHeader, "Multiplayer", ""};
  mp2_head.jak2_only = true;
  r.push_back(mp2_head);
  Row mp2{Kind::kAction, "Jak II multiplayer", ""};
  mp2.jak2_only = true;
  mp2.value = [] { return std::string(options().multiplayer ? "On" : "Off"); };
  mp2.live_help = [] {
    return std::string(options().multiplayer
                           ? "On the title: Host Game, Join Game. Name and colors: Options, Multiplayer. Turned off in "
                             "the launcher's Mods."
                           : "Off in the launcher's Mods: turn it on there to play together, from the game's next "
                             "start.");
  };
  r.push_back(mp2);

  r.push_back({Kind::kHeader, "Graphics (when the game next starts)", ""});
  Row res = VR_NUMBER("Eye resolution", resolution, 50, 150, 10, "%", "Of the headset's own. Higher is sharper and slower.");
  res.next_start = true;
  r.push_back(res);
  Row hz = Row{Kind::kChoice, "Refresh rate", "How many times a second the headset shows a frame.",
               [](const Options& o) {
                 for (int i = 0; i < 5; ++i) {
                   if (kRefreshHz[i] == o.refresh_hz) {
                     return i;
                   }
                 }
                 return 0;
               },
               [](Options& o, int v) { o.refresh_hz = kRefreshHz[std::clamp(v, 0, 4)]; }, 0, 4, 1, kRefresh};
  hz.next_start = true;
  r.push_back(hz);
  Row warp = VR_TOGGLE("Smoother frames", space_warp, "The headset makes every other frame (Application SpaceWarp), so the game draws half of them. On the Quest.");
  warp.next_start = true;
  r.push_back(warp);
  Row fov = VR_CHOICE("Foveated rendering", foveation, int, kFoveation, "Less detail at the edges of the lenses, for speed.");
  fov.next_start = true;
  r.push_back(fov);
  Row boost = VR_TOGGLE("CPU and GPU boost", cpu_gpu_boost, "The headset's processors at their highest speeds.");
  boost.next_start = true;
  r.push_back(boost);
  Row far = VR_TOGGLE("Max draw distance", max_draw_distance, "Everything drawn as far as you can see, at its highest detail: no pop-in.");
  far.next_start = true;
  r.push_back(far);
  Row aa = Row{Kind::kChoice, "Anti-aliasing", "Smooths jagged edges. Cheap for the graphics chip, and well worth it in VR.",
               [](const Options& o) { return o.msaa >= 4 ? 2 : (o.msaa >= 2 ? 1 : 0); },
               [](Options& o, int v) { o.msaa = v == 2 ? 4 : (v == 1 ? 2 : 1); }, 0, 2, 1, kMsaa};
  aa.next_start = true;
  r.push_back(aa);
  Row aniso = Row{Kind::kChoice, "Anisotropic filtering", "Keeps floors and walls sharp at a low angle.",
                  [](const Options& o) {
                    int i = 0;
                    while (i < 4 && kAnisotropy[i] < o.anisotropy) {
                      ++i;
                    }
                    return i;
                  },
                  [](Options& o, int v) { o.anisotropy = kAnisotropy[std::clamp(v, 0, 4)]; }, 0, 4, 1, kAnisoNames};
  aniso.next_start = true;
  r.push_back(aniso);

  // 0.1.1, the owner: "see whatever can be done to boost performance"; 0.1.3: "do EVERYTHING that can boost
  // performance ... leave on the ones you see best for performance by default"
  r.push_back({Kind::kHeader, "Performance", ""});
  r.push_back(VR_CHOICE("Game frame rate", frame_rate, int, kFrameRate, "How often the game draws. Automatic: the headset's rate while the game keeps up, else 60. Half: least work."));
  r.push_back(VR_TOGGLE("Dynamic resolution", dynamic_resolution, "The eyes are drawn smaller while the headset is not keeping up, and back up when it is."));
  r.push_back(VR_TOGGLE("Share work between eyes", share_eye_work, "The second eye uses the first eye's work of the frame: less for the processor to do."));
  r.push_back(VR_TOGGLE("Show performance", show_performance, "A small line low in your view: frames a second, the game's rate, each frame's work."));
  r.push_back(VR_TOGGLE("PS2 detail", ps2_detail, "Far things, effects and shadows as the PlayStation 2 drew them: faster, with some pop-in. Over Max draw distance."));
  Row sharp = VR_CHOICE("Sharpening", sharpen, int, kSharpen, "The headset sharpens the picture: a lower Eye resolution keeps its edges. On the Quest.");
  sharp.next_start = true;
  r.push_back(sharp);

  r.push_back({Kind::kBack, "Back", "Back to the game's menu."});
  return r;
}

// Jak 1: Teamruns' multiplayer, with a party of our own (game/party.h): a mod, on to start, that changes nothing in
// the game until the player hosts or joins. In VR Options, and the Multiplayer page's own (LIST-0.1.6 item 22).
void add_jak1_multiplayer_rows(std::vector<Row>& r, bool with_header) {
  if (with_header) {  // the Multiplayer page has it as its title
    Row mp_head{Kind::kHeader, "Multiplayer", ""};
    mp_head.jak1_only = true;
    r.push_back(mp_head);
  }
  Row mp = VR_TOGGLE("Multiplayer", multiplayer,
                     "Host or join a game here. Until you do, the game is the original. Your name and party server: in the launcher.");
  mp.jak1_only = true;
  r.push_back(mp);
  Row game{Kind::kAction, "Your game", ""};
  game.jak1_only = true;
  game.value = [] { return party::short_status(); };
  game.live_help = [] { return save_help(party::status()); };
  r.push_back(game);
  // a page of its own, or why not (Multiplayer off; already in a game)
  const auto page_row = [&r](const char* label, const char* help, int page, bool out_of_a_game) {
    Row row{Kind::kAction, label, help};
    row.jak1_only = true;
    row.value = [out_of_a_game] {
      if (!options().multiplayer) {
        return std::string("Turn it on first");
      }
      return std::string(out_of_a_game && party::in_party() ? "Leave first" : "");
    };
    row.act = [page, out_of_a_game](int by) {
      if (by == 0 && options().multiplayer && !(out_of_a_game && party::in_party())) {
        ask_page(page);
      }
    };
    const std::string idle(help);
    row.live_help = [idle] { return party_help(idle.c_str()); };
    r.push_back(row);
  };
  page_row("Host a game", "Start a game the others can join, from one of your saves: on this network, or on the party server if the launcher names one.",
           kHostSave, true);
  page_row("Join a game", "The games on this network and on the party server.", kGames, true);
  page_row("Play which save?", "In a game: play the host's progress, one of your saves, or a new game.", kPlaySave,
           false);
  Row who{Kind::kAction, "Players", ""};
  who.jak1_only = true;
  who.value = [] {
    const auto n = party::players().size();
    return n == 0 ? std::string() : std::to_string(n);
  };
  who.act = [](int by) {
    if (by == 0 && party::in_party()) {
      ask_page(kPlayers);
    }
  };
  who.live_help = [] {
    return party::in_party() ? std::string(party::hosting() ? "Who is in your game. You can take a player out of it."
                                                             : "Who is in this game.")
                             : party_help("Who is in the game, once you host or join one.");
  };
  r.push_back(who);
  Row leave{Kind::kAction, "Leave the game", ""};
  leave.jak1_only = true;
  leave.value = [] { return std::string(); };
  leave.act = [](int by) {
    if (by == 0) {
      party::leave();
    }
  };
  leave.live_help = [] { return party_help("Leave the game you are in, or stop hosting yours."); };
  r.push_back(leave);
}

#undef VR_TOGGLE
#undef VR_NUMBER
#undef VR_CHOICE

// The layout, in the games' screen.
constexpr float kBoxX = 32, kBoxY = 30, kBoxW = 448, kBoxH = 356;
constexpr float kTitleY = 40;
constexpr float kFirstRowY = 72;
constexpr float kRowStep = 24;
constexpr int kRowsShown = 10;
constexpr float kLabelX = 48;
constexpr float kValueLeft = 292, kValueRight = 466;
constexpr float kHelpY = 328;  // two lines at most
constexpr float kHintY = 366;

struct State {
  std::mutex mutex;
  int game = 1;
  bool shown = false;
  std::vector<Row> main_rows;  // VR Options
  std::vector<Row> page_rows;  // another page's, made again each frame (the games found, the players)
  int page = kMain;
  int want_page = -1;                    // ask_page's, for after this frame's input
  int home = kMain;                      // Back from a page comes here; Back here closes (0.1.6: Multiplayer too)
  bool at_title = false;                 // opened by the title's Multiplayer: no game to go on with yet
  int main_selected = 1, main_top = 0;   // where the home page was left, to come back to
  std::vector<party::Slot> slots;        // this game's saves, read as a save page opens
  std::vector<int> visible;  // the rows of this game, in order
  int selected = 1;          // into visible
  int top = 0;               // the first of visible in the window
  // the sticks' and buttons' last state, for presses and repeats
  int stick_dir_y = 0, stick_dir_x = 0;
  double stick_next = 0;
  bool confirm_was = true, back_was = true, press_was = true;
  int hover_was = -2;
  double press_next = 0;
} s;

bool selectable(const Row& row) {
  return row.kind != Kind::kHeader;
}

std::vector<Row>& rows() {
  return s.page == kMain ? s.main_rows : s.page_rows;
}

void ask_page(int page) {
  s.want_page = page;
}

// A save slot's row: "Save 2: Sentinel Beach", "23 cells".
Row slot_row(int slot, const char* doing, std::function<void()> pick) {
  const party::Slot& at = s.slots[slot];
  Row row{Kind::kAction, "Save " + std::to_string(slot + 1) + (at.present && !at.where.empty() ? ": " + at.where : ""),
          ""};
  const std::string value = at.present ? std::to_string(at.cells) + " cells" : "Empty";
  row.value = [value] { return value; };
  const std::string help =
      at.present ? std::string(doing) + std::to_string(at.cells) + " power cells, " + std::to_string(at.orbs) +
                       " orbs, " + std::to_string(at.flies) + " scout flies, " + std::to_string(at.percent) + "%."
                 : "Nothing is saved there.";
  row.live_help = [help] { return save_help(help); };
  if (at.present) {
    row.act = [pick](int by) {
      if (by == 0) {
        pick();
      }
    };
  }
  return row;
}

std::vector<Row> make_page(int page) {
  std::vector<Row> r;
  const auto action = [&r](const std::string& label, const std::string& help, std::function<std::string()> value,
                           std::function<void()> pick) {
    Row row{Kind::kAction, label, ""};
    row.value = value ? std::move(value) : std::function<std::string()>([] { return std::string(); });
    row.live_help = [help] { return save_help(help); };
    if (pick) {
      row.act = [pick](int by) {
        if (by == 0) {
          pick();
        }
      };
    }
    r.push_back(row);
  };
  switch (page) {
    case kHostSave:
      if (!s.at_title) {  // at the title there is no game yet: a save or a new game
        action("This game as it is", "Host now, playing on from where you are.", nullptr, [] {
          party::host();
          ask_page(s.home);
        });
      }
      for (int slot = 0; slot < static_cast<int>(s.slots.size()); ++slot) {
        r.push_back(slot_row(slot, "Loads it, then hosts: ", [slot] {
          party::play_slot(slot);
          party::host();
          ask_page(s.home);
        }));
      }
      action("New game", "Hosts a new game, saved nowhere until you save it from the pause menu.", nullptr, [] {
        party::play_new_game();
        party::host();
        ask_page(s.home);
      });
      break;
    case kGames: {
      const auto list = party::games();
      if (list.empty()) {
        action("Looking for games...", "Games on this network, and on the party server if the launcher names one. The list keeps itself up to date.",
               nullptr, nullptr);
      }
      for (const auto& g : list) {
        const std::string where =
            !g.same_build ? "(other version)" : g.cells >= 0 ? std::to_string(g.cells) + " cells" : "";
        const std::string help =
            g.same_build ? g.label + ". A joins it."
                         : g.label + ": another release of Jak and Daxter Quest VR. Both need the same one to play together.";
        Row row{Kind::kAction, g.host + "'s game", ""};
        row.value = [where] { return where; };
        row.live_help = [help] { return party_help(help.c_str()); };
        if (g.same_build) {
          row.act = [g](int by) {
            if (by == 0) {
              party::join(g);
              ask_page(kPlaySave);  // as Ratchet's: which save to play in it
            }
          };
        }
        r.push_back(row);
      }
      break;
    }
    case kPlaySave:
      action("The host's progress", "The host's power cells, orbs and scout flies, as far as it has got. Your own saves stay as they are.",
             [] { return std::string(party::in_party() && !party::hosting() ? "" : "Join first"); }, [] {
               party::play_host_copy();
             });
      if (!s.at_title) {
        action("This game as it is", "Play on from where you are.", nullptr, [] { ask_page(s.home); });
      }
      for (int slot = 0; slot < static_cast<int>(s.slots.size()); ++slot) {
        r.push_back(slot_row(slot, "Loads it: ", [slot] { party::play_slot(slot); }));
      }
      action("New game", "A new game, saved nowhere until you save it from the pause menu.", nullptr,
             [] { party::play_new_game(); });
      break;
    case kPlayers: {
      const auto list = party::players();
      const bool host = party::hosting();
      if (list.empty()) {
        action("Not in a game", "Host or join one first.", nullptr, nullptr);
      }
      for (const auto& p : list) {
        const std::string label = p.name + (p.index == 0 ? " (host)" : "") + (p.me ? " (you)" : "");
        Row row{Kind::kAction, label, ""};
        const bool can_remove = host && !p.me;
        row.value = [can_remove] { return std::string(can_remove ? "Remove" : ""); };
        const std::string help = can_remove ? "A takes " + p.name + " out of your game." : std::string("In this game.");
        row.live_help = [help] { return help; };
        if (can_remove) {
          const int index = p.index;
          row.act = [index](int by) {
            if (by == 0) {
              party::remove(index);
            }
          };
        }
        r.push_back(row);
      }
      break;
    }
    case kMultiplayer:
      add_jak1_multiplayer_rows(r, false);
      break;
    default:
      break;
  }
  r.push_back({Kind::kBack, "Back",
               page == s.home ? "Back to the game's menu."
                              : s.home == kMultiplayer ? "Back to Multiplayer." : "Back to VR Options."});
  return r;
}

void build_visible() {
  if (s.main_rows.empty()) {
    s.main_rows = make_rows();
  }
  if (s.page != kMain) {
    s.page_rows = make_page(s.page);
  }
  s.visible.clear();
  const bool guns = s.game != static_cast<int>(GameVersion::Jak1);
  const auto& all = rows();
  for (int i = 0; i < static_cast<int>(all.size()); ++i) {
    if ((all[i].guns_only && !guns) || (all[i].jak1_only && guns) ||
        (all[i].jak2_only && s.game != static_cast<int>(GameVersion::Jak2)) ||
        (all[i].games != 0 && s.game >= 0 && s.game < 32 && !(all[i].games & (1u << s.game)))) {
      continue;
    }
    s.visible.push_back(i);
  }
}

void keep_in_window() {
  const int count = static_cast<int>(s.visible.size());
  s.selected = std::clamp(s.selected, 0, std::max(count - 1, 0));
  if (s.selected < s.top + 1) {
    s.top = s.selected - 1;  // the header above it in view too
  }
  if (s.selected >= s.top + kRowsShown) {
    s.top = s.selected - kRowsShown + 1;
  }
  s.top = std::clamp(s.top, 0, std::max(count - kRowsShown, 0));
}

void move_selection(int by) {
  const int count = static_cast<int>(s.visible.size());
  int at = s.selected;
  for (int tries = 0; tries < count; ++tries) {
    at += by;
    if (at < 0 || at >= count) {
      return;  // no wrap: the list's ends stop it
    }
    if (selectable(rows()[s.visible[at]])) {
      s.selected = at;
      keep_in_window();
      return;
    }
  }
}

std::string value_text(const Row& row, const Options& o) {
  switch (row.kind) {
    case Kind::kToggle:
      return row.get(o) != 0 ? "On" : "Off";
    case Kind::kChoice:
      return row.names[std::clamp(row.get(o), row.low, row.high)];
    case Kind::kNumber: {
      const int v = row.get(o);
      if (row.tenths) {
        return std::to_string(v / 10) + "." + std::to_string(std::abs(v % 10)) + row.unit;
      }
      return std::to_string(v) + row.unit;
    }
    case Kind::kAction:
      return row.value ? row.value() : "";
    default:
      return "";
  }
}

// The page ask_page asked for, after the frame's input: its rows made, its first row chosen (VR Options: where it was).
void go_to_asked_page() {
  if (s.want_page < 0) {
    return;
  }
  const int to = s.want_page;
  s.want_page = -1;
  if (s.page == s.home && to != s.home) {
    s.main_selected = s.selected;
    s.main_top = s.top;
  }
  if (to == kHostSave || to == kPlaySave) {
    s.slots = party::slots();
  }
  s.page = to;
  build_visible();
  if (to == s.home) {
    s.selected = s.main_selected;
    s.top = s.main_top;
    keep_in_window();
  } else {
    s.selected = 0;
    s.top = 0;
    if (!s.visible.empty() && !selectable(rows()[s.visible[0]])) {
      move_selection(1);
    }
    s.top = 0;
  }
  s.hover_was = -2;
  lg::info("[vr-menu] page: {}", kPageTitle[to]);
}

// One step of a row's value: by -1 or +1; 0 for a press (on/off flips, a choice goes round, a number steps up and
// goes round from its top).
void change(const Row& row, int by) {
  if (row.kind == Kind::kBack) {
    if (by == 0) {
      if (s.page != s.home) {
        ask_page(s.home);
      } else {
        close();
      }
    }
    return;
  }
  if (row.kind == Kind::kAction) {
    if (row.act) {
      if (by == 0) {
        lg::info("[vr-menu] {}", row.label);
      }
      row.act(by);
    }
    return;
  }
  if (!row.get || !row.set) {
    return;
  }
  Options next = options();
  const int was = row.get(next);
  int v = was;
  switch (row.kind) {
    case Kind::kToggle:
      v = was != 0 ? 0 : 1;
      break;
    case Kind::kChoice:
      v = by == 0 ? (was >= row.high ? row.low : was + 1) : std::clamp(was + by, row.low, row.high);
      break;
    case Kind::kNumber:
      v = by == 0 ? (was + row.step > row.high ? row.low : was + row.step)
                  : std::clamp(was + by * row.step, row.low, row.high);
      break;
    default:
      return;
  }
  if (v == was) {
    return;
  }
  row.set(next, v);
  set_options(next);
  lg::info("[vr-menu] {}: {}", row.label, value_text(row, next));
}

}  // namespace

void set_game(int game_version) {
  std::lock_guard<std::mutex> lock(s.mutex);
  s.game = game_version;
  build_visible();
}

float game_screen_height() {
  return s.game == static_cast<int>(GameVersion::Jak1) ? 224.0f : kScreenHeight;
}

bool shown() {
  return s.shown;
}

namespace {
void open_at(int home, bool at_title, const char* why) {
  if (s.shown) {
    return;
  }
  s.page = s.home = home;
  s.at_title = at_title;
  s.want_page = -1;
  build_visible();
  s.shown = true;
  s.selected = 0;
  s.top = 0;
  if (!s.visible.empty() && !selectable(rows()[s.visible[0]])) {
    move_selection(1);
  }
  s.top = 0;
  // what is held as it opens (the A that picked "VR Options", the menu button) does nothing until let go
  s.confirm_was = s.back_was = s.press_was = true;
  s.stick_dir_x = s.stick_dir_y = 0;
  s.hover_was = -2;
  lg::info("[vr-menu] open {} ({})", kPageTitle[home], why);
}
}  // namespace

void open(const char* why) {
  std::lock_guard<std::mutex> lock(s.mutex);
  open_at(kMain, false, why);
}

void open_multiplayer(bool at_title) {
  std::lock_guard<std::mutex> lock(s.mutex);
  open_at(kMultiplayer, at_title, at_title ? "the title's Multiplayer" : "the pause menu's Multiplayer");
}

void close() {
  if (s.shown) {
    s.shown = false;
    lg::info("[vr-menu] closed");
  }
}

void update(const Input& in, double now) {
  std::lock_guard<std::mutex> lock(s.mutex);
  if (!s.shown) {
    return;
  }
  if (s.page != kMain) {
    // a page of Multiplayer's: the games found, the players, made again; the games' list wanted while it is shown
    if (s.page == kGames) {
      party::browse();
    }
    build_visible();
    keep_in_window();
  }
  if (s.visible.empty()) {
    return;
  }
  // The sticks: up and down choose, left and right change; held, they repeat.
  const int dir_y = in.stick_y > 0.6f ? -1 : in.stick_y < -0.6f ? 1 : 0;
  const int dir_x = std::fabs(in.stick_x) > 0.6f && std::fabs(in.stick_x) > std::fabs(in.stick_y) ? (in.stick_x > 0 ? 1 : -1) : 0;
  const bool moved = dir_y != s.stick_dir_y || (dir_y == 0 && dir_x != s.stick_dir_x);
  if (moved) {
    s.stick_next = now + 0.4;
  }
  if ((dir_y != 0 || dir_x != 0) && (moved || now >= s.stick_next)) {
    if (!moved) {
      s.stick_next = now + 0.1;
    }
    if (dir_y != 0) {
      move_selection(dir_y);
    } else {
      const Row& row = rows()[s.visible[s.selected]];
      if (row.kind != Kind::kToggle) {
        change(row, dir_x);
      } else if ((dir_x > 0) != (row.get(options()) != 0)) {
        change(row, 0);  // right is on, left is off
      }
    }
  }
  s.stick_dir_y = dir_y;
  s.stick_dir_x = dir_x;

  if (in.confirm && !s.confirm_was) {
    change(rows()[s.visible[s.selected]], 0);
  }
  s.confirm_was = in.confirm;
  if (in.back && !s.back_was) {
    if (s.page != s.home) {
      ask_page(s.home);  // B on a page: back to the home page (VR Options or Multiplayer)
    } else {
      close();
    }
  }
  s.back_was = in.back;
  if (!s.shown) {
    return;
  }

  // The pointer: a row under it is chosen as it comes onto it; the trigger changes it (on a number, its left half of
  // the value steps down and the right half up) or, above or below the list, scrolls it.
  if (in.point_valid) {
    const float x = in.point_u * kScreenWidth, y = in.point_v * kScreenHeight;
    int hover = -1;  // -1 none, -3 above the list, -4 below it
    if (x >= kBoxX && x < kBoxX + kBoxW) {
      if (y >= kFirstRowY - 2 && y < kFirstRowY - 2 + kRowStep * kRowsShown) {
        const int slot = static_cast<int>((y - (kFirstRowY - 2)) / kRowStep);
        const int at = s.top + slot;
        if (at >= 0 && at < static_cast<int>(s.visible.size()) && selectable(rows()[s.visible[at]])) {
          hover = at;
        }
      } else if (y >= kBoxY && y < kFirstRowY - 2) {
        hover = -3;
      } else if (y >= kFirstRowY - 2 + kRowStep * kRowsShown && y < kHelpY - 4) {
        hover = -4;
      }
    }
    if (hover >= 0 && hover != s.hover_was) {
      s.selected = hover;
    }
    s.hover_was = hover;
    const bool press = in.point_press && !s.press_was;
    if (hover >= 0 && press) {
      const Row& row = rows()[s.visible[hover]];
      const float middle = (kValueLeft + kValueRight) / 2;
      if ((row.kind == Kind::kNumber || row.kind == Kind::kChoice || (row.kind == Kind::kAction && row.cycles)) &&
          x >= kValueLeft) {
        change(row, x < middle ? -1 : 1);
      } else {
        change(row, 0);
      }
    }
    if ((hover == -3 || hover == -4) && in.point_press && (press || now >= s.press_next)) {
      s.press_next = now + (press ? 0.35 : 0.1);
      s.top = std::clamp(s.top + (hover == -3 ? -1 : 1), 0, std::max(static_cast<int>(s.visible.size()) - kRowsShown, 0));
      s.selected = std::clamp(s.selected, s.top, s.top + kRowsShown - 1);
      if (!selectable(rows()[s.visible[s.selected]])) {
        move_selection(hover == -3 ? -1 : 1);
      }
    }
  } else {
    s.hover_was = -2;
  }
  s.press_was = in.point_press;
  go_to_asked_page();
}

namespace {
bool box_laid_out(int index, int& kind, float out[4]);
bool text_laid_out(int index, Text& out);
}  // namespace

// The layout is in the 512 by 416 screen; the game's own is Jak 1's shorter one (game_screen_height), and Jak 1's
// font, all capitals, is wider for the same scale.
bool box(int index, int& kind, float out[4]) {
  std::lock_guard<std::mutex> lock(s.mutex);
  if (!box_laid_out(index, kind, out)) {
    return false;
  }
  const float y_scale = game_screen_height() / kScreenHeight;
  out[1] *= y_scale;
  out[3] *= y_scale;
  return true;
}

bool text(int index, Text& out) {
  std::lock_guard<std::mutex> lock(s.mutex);
  if (!text_laid_out(index, out)) {
    return false;
  }
  out.y *= game_screen_height() / kScreenHeight;
  // each game's large font at the layout's scale: a row's label and value side by side in the box (sim j1_menu2,
  // j2_menu2)
  out.scale *= s.game == static_cast<int>(GameVersion::Jak1) ? 0.85f : 0.8f;
  return true;
}

namespace {

bool box_laid_out(int index, int& kind, float out[4]) {
  if (!s.shown) {
    return false;
  }
  if (index == 0) {
    kind = 1;
    out[0] = kBoxX;
    out[1] = kBoxY;
    out[2] = kBoxW;
    out[3] = kBoxH;
    return true;
  }
  if (index == 1) {
    const int slot = s.selected - s.top;
    if (slot < 0 || slot >= kRowsShown) {
      return false;
    }
    kind = 2;
    out[0] = kBoxX + 6;
    out[1] = kFirstRowY - 2 + kRowStep * static_cast<float>(slot);
    out[2] = kBoxW - 12;
    out[3] = kRowStep;
    return true;
  }
  return false;
}

bool text_laid_out(int index, Text& out) {
  if (!s.shown || s.visible.empty()) {
    return false;
  }
  const Options& o = options();
  // 0 the title; then two a row shown (label, value); then the arrows; then the help and the hint
  if (index == 0) {
    out = {kPageTitle[s.page], kScreenWidth / 2, kTitleY - 8, 5, 1, 0.75f};
    return true;
  }
  int i = index - 1;
  const int shown_rows = std::min(kRowsShown, static_cast<int>(s.visible.size()) - s.top);
  if (i < shown_rows * 2) {
    const int at = s.top + i / 2;
    const Row& row = rows()[s.visible[at]];
    const float y = kFirstRowY + kRowStep * static_cast<float>(i / 2);
    const bool chosen = at == s.selected;
    if (i % 2 == 0) {
      if (row.kind == Kind::kHeader) {
        out = {row.label, kLabelX - 6, y, 5, 0, 0.6f};
      } else {
        out = {row.label, kLabelX, y, chosen ? 10 : 0, 0, 0.55f};
      }
    } else {
      std::string v = value_text(row, o);
      // (no "< v >" as Ratchet's: the games' fonts draw button pictures for < and >; the chosen row's colour and
      // bar say it)
      out = {v, kValueRight - 6, y, chosen ? 10 : 0, 2, 0.55f};
    }
    return true;
  }
  i -= shown_rows * 2;
  if (i == 0) {
    out = {s.top > 0 ? "more above" : "", kScreenWidth / 2, kFirstRowY - 14, 0, 1, 0.4f};
    return true;
  }
  if (i == 1) {
    const bool more = s.top + kRowsShown < static_cast<int>(s.visible.size());
    out = {more ? "more below" : "", kScreenWidth / 2, kFirstRowY - 2 + kRowStep * kRowsShown, 0, 1, 0.4f};
    return true;
  }
  if (i == 2) {
    const Row& chosen = rows()[s.visible[s.selected]];
    out = {chosen.live_help ? chosen.live_help() : chosen.help, kScreenWidth / 2, kHelpY, 0, 1, 0.42f,
           kBoxW - 24};
    return true;
  }
  if (i == 3) {
    out = {"Stick or point: choose.  A or trigger: change.  B: back.", kScreenWidth / 2, kHintY, 5, 1, 0.42f,
           kBoxW - 24};
    return true;
  }
  return false;
}

}  // namespace

}  // namespace jakvr::menu
