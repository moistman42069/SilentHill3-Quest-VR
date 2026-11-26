// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 the Jak and Daxter Quest VR contributors
//
// The VR options: what the launcher sets before the game starts and the in-game VR menu changes while it runs. One
// file for each game (vr-options.txt in its settings folder), lines of "name value". Everything the port adds is on to
// start, each with its row to turn it off.

#pragma once

#include <string>

namespace jakvr {

enum class CutsceneView { kFirstPerson = 0, kThirdPerson = 1, kScreen = 2, kCount = 3 };
enum class DpadPlace { kHead = 0, kThumbRest = 1, kCount = 2 };
enum class TurnKind { kSnap = 0, kSmooth = 1, kCount = 2 };
enum class MenuHand { kBoth = 0, kRight = 1, kLeft = 2, kCount = 3 };
enum class MoveBy { kHead = 0, kLeftHand = 1, kCount = 2 };
// What of Jak the eyes see in first person (owner, 2026-11-14: "hands only, hands and arms only, arms hands and legs
// only, etc."); his hands always. The rest is still posed and casts his shadow (jak_vr.cpp body()).
enum class BodyShown {
  kWhole = 0,     // all of him but the head
  kBodyArms = 1,  // no legs
  kBodyLegs = 2,  // no arms: the hands by themselves
  kArmsLegs = 3,  // no body
  kArms = 4,
  kLegs = 5,      // and the hands
  kHands = 6,
  kCount = 7
};

// 0.1.10 (LIST-0.1.10 item 6): Vehicle view height's start, centimetres up from his eyes (0 from 0.1.3 to 0.1.9); 15,
// the least that shows the track over the race car's nose (sims vh0, vh, vh15, vh20: 0, 10, 15, 20 in the class 2 race)
constexpr int kVehicleViewHeightStart = 15;

struct Options {
  // ---- View ----
  bool first_person = true;  // play starts seen from Jak's eyes (the left menu button held switches)
  CutsceneView cutscene_view = CutsceneView::kFirstPerson;  // through his eyes where he is in the scene
  // Adaptive cutscenes (the owner, 0.1.1): a scene set to be seen through his eyes goes around you by itself while Jak
  // is not properly in it, and back when he is (jak_vr.cpp update_scene_fit)
  bool adaptive_cutscenes = true;
  // a talk in play from his eyes too (else the game's camera, around you); owner, 2026-11-13: on to start
  bool first_person_in_talks = true;
  // 0.1.7 (LIST-0.1.7 item 4; a tester: "when pole swinging there should be a on by default option that return to 1st
  // person only after feet touch ground"): once Jak has been on a pole, the view stays third person until he lands
  // (each game's vr-pole-hold?, kFlagPoleHold). Off: first person again as soon as he leaves the pole. On to start.
  bool pole_third_person = true;
  // 0.1.7 (LIST-0.1.7 item 2; the owner: "option to turn off eco effects in 1st person"): off leaves out the sparks
  // eco throws off Jak's body (and Dark and Light Jak's) while the view is in his eyes, where they burst at the eyes;
  // the eco's sound and what it does stay (each game's *vr-eco-sparks-off?*). On to start, as the game draws them.
  bool eco_sparks_first_person = true;
  // 0.1.9 (LIST-0.1.9 item 20; the owner: "making the fishing minigame work in first person vr ... motion controls,
  // proper implementation"): Jak 1's fisherman's game from Jak's eyes, the net in your hand, where you point it
  // (jak_vr.cpp fishing_net). Off: from the game's camera, the net following your raised hand side to side. On to start.
  bool fishing_first_person = true;
  // 0.1.7 (LIST-0.1.7 item 9; a tester: "is there a way to implement just the gamepad... either my vive wands are broken
  // or its just difficult to execute a roll with them"): only a gamepad plays; the VR controllers are left out of the
  // game (their buttons, sticks, gestures, hands and aim; jak_vr.cpp leave_controllers_out). The headset still turns the
  // view. The VR menu, opened from the game's pause menu (VR Options), takes the gamepad and the controllers both, so the
  // mode can always be turned off. Off to start: on would leave a player with no gamepad nothing to play with.
  bool gamepad_only = false;
  // First person in vehicles (the owner, 0.1.1): on the Zoomer, the JET-Board, in cars, the Titan suit and on the flut
  // the view sits in his head too, turning with what he rides (each game's vr-vehicle-first-person-ok?, kFlagVehicle)
  // 0.1.3 (the owner: "make that the new default"): on to start, saved as "first-person-vehicles-on" so 0.1.1's off is
  // left behind once
  bool first_person_vehicles = true;
  // ... and each kind of ride on its own (0.1.5, LIST-0.1.5 item 18; a player: "can you make vehicle types seperate 1st
  // person toggles, for example hover car, hover bike, lizard, hover board?", and the owner: "keep the first person
  // everything option"): First person in vehicles stays the switch for them all; with it on, each kind is first person
  // while its own switch is on (all on to start). The kind comes from each game's vr-ride-kind (jak_vr.h RideKind).
  bool fp_zoomer = true;          // Jak 1's A-Grav Zoomer
  bool fp_flut_flut = true;       // Jak 1's Flut Flut
  bool fp_jet_board = true;       // Jak II and 3's JET-Board (its rails and half-pipes too)
  bool fp_hover_cars = true;      // Jak II and 3's city cars (the Hellcat, the paddywagon)
  bool fp_hover_bikes = true;     // Jak II and 3's city bikes, Jak II's race bikes
  bool fp_titan_suit = true;      // Jak II's Titan suit
  bool fp_leaper_lizards = true;  // Jak 3's leaper lizards
  bool fp_desert_vehicles = true; // Jak 3's wasteland vehicles (the Tough Puppy, the Dune Hopper and the rest)
  bool fp_other_rides = true;     // slides, gun turrets, the glider, the sled and the rest
  // ... and where the view sits there (0.1.3, the owner: "add options to adjust angle/positioning as needed"; jak_vr.cpp
  // camera): up from his eyes and ahead of them (centimetres, as View height), and tilted down (degrees, less than 0 up)
  // 0.1.10 (LIST-0.1.10 item 6; a tester: "default vehicle height should be a little heigher than it is, but got to the
  // second race and couldn't see"): starts higher, under a new key (vr_options.cpp: 0.1.3 to 0.1.9's start, 0, becomes
  // this; a height a player chose is kept)
  int vehicle_view_height = kVehicleViewHeightStart;
  int vehicle_view_forward = 0;
  int vehicle_view_tilt = 0;
  // ... and kept steady there (0.1.3, a player on Jak 3's leaper lizard: "shakes camera to point where you can't see
  // where your going"): the view keeps its place in his seat and its heading without each bounce of the ride
  // (jak_vr.cpp ride_steady); off, it follows his head's every bob
  bool steady_ride_view = true;
  // Steer by grabbing the wheel (0.1.8, LIST-0.1.8 item 6; the owner: "physical driving ... a toggle that is on by
  // default. people can still use normal inputs to drive, but once they grab the wheel on whichever vehicle, that will
  // allow them to use it to steer"): in every vehicle steered by the stick (each game's vr-steer-flags: the Zoomer;
  // Jak II and 3's bikes, cars, race bikes, desert vehicles, the glider, the sled) a grip squeezed at its controls, or
  // ahead of you, holds them (jak_vr.cpp steer_wheel), and turning the hands (as a wheel at any tilt, or handlebars
  // pushed and pulled) steers, added to the stick; the hands are drawn on the ride's handles. A quick squeeze is still
  // the grip's button, squeezed again and held it holds it, and a holding hand's stick click is that button too.
  bool wheel_steer = true;
  int wheel_turn = 60;  // degrees the wheel turns for full steering (handlebars: 60% of it)
  // 0.1.9 (LIST-0.1.9 item 14, a tester: "when gripping steering on hover vehicles ... i personally think B would be
  // more convenient"): while the right hand holds a hover car's or bike's wheel (Jak II and 3: R2 their height, Circle
  // their turbo), B is the height and the right stick's click the turbo (jak_vr.cpp fill_pad); off, as 0.1.8 (B the
  // turbo, the click the height). The quick squeeze on the wheel stays the height either way. On to start.
  bool wheel_b_height = true;
  // 0.1.9 (LIST-0.1.9 item 15, a tester: "pushing forward while gripping steering do forward joystick's action of leaning
  // and pulling back should do joystick back's action"): the wheel held and pushed away is the left stick forward,
  // pulled back the stick back, added to the stick (jak_vr.cpp steer_wheel): both hands moving together (turning moves
  // them opposite ways and never leans), or one hand on a wheel (on handlebars one hand's push is its turn). On to start.
  bool wheel_lean = true;
  TurnKind turn = TurnKind::kSmooth;  // owner, 2026-11-13: smooth turn to start (snap is the VR menu's other choice)
  int snap_degrees = 45;
  int smooth_degrees = 120;  // a second, the stick full over
  // First person: 1 is life size, Jak as tall as a person (his eyes 1.6 m up: jak_vr.cpp kLifeSize); more: the world
  // bigger. To 0.1.0, 1 was a game metre a real one, which made him 2.3 m to the eyes: 1.45 here is that look.
  float world_scale = 1.0f;
  float third_person_scale = 1.0f;  // third person: the world seen at this size around the game's camera
  bool seated = false;       // a sitting player: the view at his eyes whatever the head's height
  int hud_size = 100;        // percent
  int hud_distance = 160;    // centimetres ahead
  int hud_height = 0;        // centimetres up from the eyes' line (below 0: under it)
  bool hud_follows = true;   // the HUD moves with the head; off, it stays put ahead of you until you look well away

  // ---- Controls ----
  DpadPlace dpad_place = DpadPlace::kThumbRest;  // owner 2026-11-11: left thumb on its rest makes the right stick the d-pad (kHead: left hand at the head, left stick)
  int dpad_reach = 22;       // centimetres from the head the hand counts as there
  bool vibration = true;
  int vibration_strength = 100;  // percent of every buzz
  bool world_vibration = true;   // a hand meeting a wall or the floor, harder the harder it presses (Ratchet's)
  bool dpad_buzz = true;         // 0.1.1: a quick pulse in the stick's hand as a d-pad direction goes in
  bool gun_switch_buzz = true;   // 0.1.1: Jak II and 3, a quick pulse in the gun's hand as the gun changes
  // Extra vibration (0.1.4, LIST-0.1.4 items 1 to 3; the owner: "haptics can be either as is, also given when
  // collecting items ..., or for the whole world (when taking steps, any actions really)"): 0 Off (0.1.3's haptics),
  // 1 Pickups (a buzz for each thing collected), 2 The whole world (pickups, and each step, jump, landing, roll, attack,
  // hit and the world's shakes). Pickups to start in 0.1.4 and 0.1.5; 0.1.6 the whole world (the owner, 2026-11-17:
  // "make vibarations on everything the new default"), saved as "extra-vibration-kind" (load_options: a player's Off
  // under 0.1.4's "extra-vibration" is kept, its Pickups, the start then, becomes the whole world).
  int extra_vibration = 2;
  MenuHand menu_hand = MenuHand::kBoth;
  // Stick click as menu button (0.1.1): where the runtime binds no left menu button (SteamVR keeps Touch's for its
  // dashboard), the left stick's lone click is the menu button (jak_vr.cpp fill_pad). Off: that click is L3 there too.
  bool stick_click_menu = true;
  // Owner 2026-11-12: the stick moves Jak where you look (the games turn it by the camera, which is the head); the
  // other choice, where the left hand points.
  MoveBy move_by = MoveBy::kHead;

  // ---- Body ----
  bool body = true;          // Jak's body seen from his eyes, its arms reaching the controllers (full-body IK)
  BodyShown body_shown = BodyShown::kWhole;  // what of him the eyes see (first person); the hands always
  bool hide_arms_near_eyes = false;  // 0.1.1: an arm's upper arm and forearm out of the eyes while near them (owner: off to start)
  // 0.1.1's refinement pass (the owner: both on to start). Fade body near your eyes: a leg swung up by the face, his
  // body and legs in a roll, a flip or a dive fade out of the eyes and back, eased (jak_vr.cpp body() step 9). Keep
  // your body upright: his hips and chest stay upright under your head whatever his animation does (a spin kick, a
  // punch's twist), his legs still reaching the game's feet (body() step 1b).
  bool fade_near_eyes = true;
  bool steady_body = true;
  bool fingers = true;       // the fingers follow the controllers' sensors
  bool predict_hands = true; // his hands drawn where the controllers will be when the frame is shown (no lag behind)
  bool hands_on_world = true;  // hands and what they hold stop at walls and floors
  bool physical_crouch = true;  // the head going down crouches him
  int crouch_drop = 30;      // centimetres down from standing
  int arm_length = 100;      // percent of his arm's own reach
  int body_height = 0;       // centimetres
  int body_forward = 0;      // centimetres his body sits ahead of the head (less: behind), as Ratchet's
  int view_height = 0;       // centimetres

  // ---- Combat ----
  bool hand_melee = true;    // punch and swing with your arms: a punch is his punch, a sweep his spin kick
  int melee_speed = 25;      // tenths of a metre a second the hand must move for a hit (2.5 m/s)
  // Fists on the right grip (owner, 2026-11-13, as Ratchet's wrench): on foot, the right grip held makes fists and only
  // fists hit; the grip's game button there (Jak II and 3's JET-Board) moves to the right stick's click
  bool melee_grip = true;
  // 0.1.10 (LIST-0.1.10 item 1; the owner: "gun melee as an on by default toggle ... where you can smack enemies with
  // guns held in hand and that causes damage"), Jak II and 3: a hard swing of the hand holding the gun is his punch
  // (the game's, with the gun drawn, as Square: its hit, its punch-and-shoot combo). Faster than a fist's (Melee speed
  // and a half), and never while firing, both hands on the gun, or just after drawing it (jak_vr.cpp fill_pad).
  bool gun_melee = true;
  // Spin kick by spreading your arms (Vixel's T-pose, 0.1.1): both arms out wide at the sides, held a moment, is one
  // spin kick (jak_vr.cpp fill_pad); the arms come in before the next
  bool spread_spin_kick = false;  // off to start (the owner, 0.1.1)
  bool hand_aim = true;     // Jak II and 3's gun and Jak 1's yellow eco aim where the hand points
  bool two_hand_grip = true; // the other hand on the gun steadies it (Jak II and 3)
  bool holster_draw = true;  // a grip at the hip or over the shoulder draws the gun, let go there puts it away (Jak II and 3)
  // Holster size: how far past 0.1.0's places a hand still counts as at the hip or the shoulder (jak_vr.cpp at_holster).
  // A little bigger to start (Vixel, 0.1.0: easier to put the gun away); the owner wanted it in the VR menu.
  int holster_size = 130;    // percent
  // Holsters only on foot (0.1.1, Vixel's car height button): in a vehicle, on the board, a turret or a ride a grip at
  // the hip or shoulder stays the game's grip button (the gun cannot come out there). Off, as 0.1.0.
  bool holster_on_foot = true;
  // Hold the gun while driving (0.1.8, LIST-0.1.8 item 5; a tester: "i miss being able to hold gun while drivng"; the
  // owner: a toggle, on to start): in a vehicle that keeps his gun (Jak II and 3's city bikes and cars), the shoulder
  // holster draws it and puts it away whatever Holsters only on foot says, and it is held in the right hand as on foot;
  // not the hip's (the hand rests there seated; its grip stays the vehicle's button).
  bool ride_gun = true;
  int gun_angle = 0;         // degrees up from where the controller points
  // The gun's crosshair (0.1.3, LIST-0.1.3 item 27; the owner: "a proper dynamic crosshair that reacts to the world and is
  // accurate", as the HaloCE port's): on whatever the shot would hit first (the game's own probe along the shot's line),
  // the same size to the eye however far that is. Jak II and 3, first person with Aim with your hand. Kind: 0 off, 1 Dot,
  // 2 Cross, 3 Cross and dot, 4 Circle, 5 Circle and dot, 6 Plus, 7 Small circle (0.1.5; crosshair.h). The Dot to
  // start again (0.1.6, the owner: "lets turn the dot crosshair on by default again for this release"; 0.1.5 started
  // Off under "crosshair-kind", 0.1.3 and 0.1.4 with the Dot under "crosshair"): its key was "crosshair-shape". Off to
  // start again (0.1.7, LIST-0.1.7 item 5, the owner: "make it so crosshair is off by default again this update"):
  // its key is "crosshair-mark" now, so the earlier Dot start is left behind once; any other shape a player chose is
  // kept (load_options).
  int crosshair = 0;
  int crosshair_size = 100;     // percent; 100 is 2 degrees across
  int crosshair_color = 0;      // crosshair.h's order: white, green, yellow, cyan, red, pink
  int crosshair_opacity = 100;  // percent
  bool crosshair_enemy = true;  // red over an enemy in the gun's range (yellow when its color is red)

  // ---- Graphics (the launcher's; read by the runtime) ----
  int resolution = 100;      // percent of the headset's recommended eye size
  // Smoother frames (AppSW): off to start since 0.1.1 (owner, 2026-11-14: "it seems to cause visual issues for now. the
  // title screen flickers with it on"; from 0.1.1 it is off on the title and its menus, jak_vr.cpp `here`); saved as
  // "smoother-frames", so 0.1.0's "space-warp on" is left behind once.
  // 0.1.4: the Quest starts with the owner's own Graphics (their Quest 3's vr-options.txt, 2026-11-16: "make the
  // graphics options i set in the launcher ... the new default too"): 120 Hz (set_refresh takes the most the headset
  // has up to that), Smoother frames on, High foveation, CPU and GPU boost off, no anti-aliasing or anisotropic
  // filtering, Normal sharpening; the rest were already theirs. The PC keeps 0.1.3's, its own headsets and chips (the
  // launcher's VrOptions does the same).
  // 0.1.6: CPU and GPU boost on to start on the Quest too (the owner, 2026-11-17: "put cpu/gpu boost on by default"),
  // saved as "cpu-gpu-boost-on" (load_options: the Quest's "off", the start then, is left behind once; a PC player's
  // own off is kept).
#if defined(__ANDROID__)
  bool space_warp = true;
  bool cpu_gpu_boost = true;  // CPU and GPU boost
  int foveation = 3;          // Foveated rendering (Ratchet 0.1.19): 0 off, 1 low, 2 medium, 3 high, 4 highest (0.1.1)
  int refresh_hz = 120;       // 0: the headset's own
#else
  bool space_warp = false;
  bool cpu_gpu_boost = true; // CPU and GPU boost
  int foveation = 2;         // Foveated rendering (Ratchet 0.1.19): 0 off, 1 low, 2 medium, 3 high, 4 highest (0.1.1)
  int refresh_hz = 0;        // 0: the headset's own
#endif
  bool max_draw_distance = true;  // no pop-in: every actor, effect and model drawn however far, at its highest detail (pc-vr.gc vr-graphics!)
  // 0.1.6: the Quest 3 and 3S start with the owner's own (their Quest 3's vr-options.txt, 2026-11-17: "make sure the
  // defaults in the log i sent you are the new ones on quest 3 going forward. keep quest 2 at 100% if possible"):
  // Strong sharpening and (the owner then: "for quest 3/3s, res in launcher should be 150%") 150% eyes (load_options:
  // strong_quest()); the Quest 2 and the others keep Normal sharpening and their eyes at 100%. Anti-aliasing and
  // anisotropic filtering Off to start on every headset, the PC too (the owner then: "anti aliasing/anisotropic
  // filtering should be off by default"; the PC started at 2x and 16x until 0.1.5). Saved under new keys
  // ("anti-aliasing", "anisotropic-filtering", "sharpening-strength", "eye-resolution"): a Quest 3's old start (Normal,
  // 100%) is left behind once; any other choice, and every value a PC's file has, is kept.
  int msaa = 1;        // Anti-aliasing, samples a pixel (1 off, 2, 4)
  int anisotropy = 1;  // Anisotropic filtering (1 off .. 16, at most the graphics chip's; 16 the games' own)

  // ---- Performance (0.1.1 off to start; 0.1.3, the owner: "do EVERYTHING that can boost performance and have them
  // as options in the launcher ... leave on the ones you see best for performance by default") ----
  // Game frame rate: 0 sixty a second whatever the headset's rate (as 0.1.0), 1 the headset's own (each display a new
  // game frame), 2 half the headset's (half the work; the headset turns each picture to the head between), 3 (0.1.3,
  // to start) Automatic: the headset's own while the game keeps up with it, sixty while it does not (jak_vr.cpp
  // AutoRate). Smoother frames makes it half by itself (jak_vr.cpp s_game_fps). Saved as "game-frame-rate" from
  // 0.1.3; 0.1.1's "frame-rate" is read when that is missing (its 60, the start then, as Automatic).
  int frame_rate = 3;
  bool dynamic_resolution = true;   // the eyes drawn smaller while the headset is not keeping up (jak_vr.cpp Scale);
                                    // on to start from 0.1.3 ("dynamic-resolution-on"; 0.1.1's key read no more)
  bool show_performance = false;    // 0.1.3: a line on the HUD: frames a second, the game's rate, the frame's work
  bool share_eye_work = true;       // 0.1.3: the second eye reuses the first's work of the frame (the background's
                                    // colours and what is seen) instead of doing it again
#if defined(__ANDROID__)
  int sharpen = 1;                  // the headset sharpens the eyes' picture: 0 off, 1 normal, 2 strong (the Quest's);
                                    // Normal on the Quest to start from 0.1.4 (the owner's)
#else
  int sharpen = 0;                  // the headset sharpens the eyes' picture: 0 off, 1 normal, 2 strong (the Quest's)
#endif
  // PS2 detail: the distances, effects and shadows the PlayStation 2 drew (pc-vr.gc vr-graphics!); over Max draw
  // distance while it is on
  bool ps2_detail = false;

  // ---- Multiplayer (Jak 1: Teamruns' game side and a party of our own, game/party.h) ----
  // on to start (owner, 2026-11-14), the launcher's Mods turn it off: the VR menu hosts, browses and joins games; it
  // changes nothing in the game until the player hosts or joins one (party::game_on), and opens no port until then
  bool multiplayer = true;
};

// The options in force (game thread and display thread both read; a change is a whole copy).
const Options& options();
// Replaces them and saves them.
void set_options(const Options& next);
// Reads the game's file (defaults where it has nothing), at start.
void load_options(int game_version);
// Whether the file has been read yet (0.1.1: the headset reads it itself if it starts first).
bool options_loaded();
// Writes the game's file.
void save_options();
// The file's path, for the launcher and the logs.
std::string options_path();

}  // namespace jakvr
