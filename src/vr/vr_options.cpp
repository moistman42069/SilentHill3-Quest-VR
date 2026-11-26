// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 the Jak and Daxter Quest VR contributors

#include "vr_options.h"

#if defined(__ANDROID__)
#include <sys/system_properties.h>
#endif

#include <algorithm>
#include <atomic>
#include <fstream>
#include <mutex>
#include <sstream>
#include <vector>

#include "common/log/log.h"
#include "common/util/FileUtil.h"
#include "common/versions/versions.h"

namespace jakvr {

namespace {

std::mutex s_mutex;
Options s_options;
std::string s_path;
std::atomic<bool> s_loaded{false};

template <typename T>
T clamp_enum(int v, T count) {
  return static_cast<T>(std::clamp(v, 0, static_cast<int>(count) - 1));
}

// Each option's name in the file, and how it is read and written.
struct Field {
  const char* name;
  bool* flag = nullptr;
  int* number = nullptr;
  float* real = nullptr;
  int low = 0, high = 0;
};

std::vector<Field> fields(Options& o, int& cutscene, int& dpad, int& turn, int& menu_hand, int& move_by,
                          int& body_shown) {
  return {
      {"first-person", &o.first_person},
      {"cutscenes", nullptr, &cutscene, nullptr, 0, 2},
      {"adaptive-cutscenes", &o.adaptive_cutscenes},
      {"first-person-in-talks", &o.first_person_in_talks},
      {"pole-third-person", &o.pole_third_person},  // 0.1.7: on to start
      {"eco-sparks-first-person", &o.eco_sparks_first_person},  // 0.1.7: on to start
      {"fishing-first-person", &o.fishing_first_person},  // 0.1.9: on to start
      {"gamepad-only", &o.gamepad_only},  // 0.1.7: off to start
      {"first-person-vehicles-on", &o.first_person_vehicles},  // 0.1.3: on to start (0.1.1's "first-person-vehicles" off)
      // 0.1.5 (LIST-0.1.5 item 18): each kind of ride, on to start
      {"first-person-zoomer", &o.fp_zoomer},
      {"first-person-flut-flut", &o.fp_flut_flut},
      {"first-person-jet-board", &o.fp_jet_board},
      {"first-person-hover-cars", &o.fp_hover_cars},
      {"first-person-hover-bikes", &o.fp_hover_bikes},
      {"first-person-titan-suit", &o.fp_titan_suit},
      {"first-person-leaper-lizards", &o.fp_leaper_lizards},
      {"first-person-desert-vehicles", &o.fp_desert_vehicles},
      {"first-person-other-rides", &o.fp_other_rides},
      {"vehicle-view-height-cm", nullptr, &o.vehicle_view_height, nullptr, -50, 50},  // 0.1.10: a new start (load_options)
      {"vehicle-view-forward", nullptr, &o.vehicle_view_forward, nullptr, -50, 50},
      {"vehicle-view-tilt", nullptr, &o.vehicle_view_tilt, nullptr, -30, 30},
      {"steady-ride-view", &o.steady_ride_view},  // 0.1.3
      {"wheel-steer", &o.wheel_steer},  // 0.1.8: Steer by grabbing the wheel, on to start
      {"wheel-turn", nullptr, &o.wheel_turn, nullptr, 20, 180},
      {"wheel-b-height", &o.wheel_b_height},  // 0.1.9: B for height while holding the wheel, on to start
      {"wheel-lean", &o.wheel_lean},          // 0.1.9: Lean by pushing the wheel, on to start
      {"turn", nullptr, &turn, nullptr, 0, 1},
      {"snap-degrees", nullptr, &o.snap_degrees, nullptr, 15, 90},
      {"smooth-degrees", nullptr, &o.smooth_degrees, nullptr, 30, 360},
      {"world-scale", nullptr, nullptr, &o.world_scale, 0, 0},
      {"third-person-scale", nullptr, nullptr, &o.third_person_scale, 0, 0},
      {"seated", &o.seated},
      {"hud-size", nullptr, &o.hud_size, nullptr, 50, 150},
      {"hud-distance", nullptr, &o.hud_distance, nullptr, 80, 300},
      {"hud-height", nullptr, &o.hud_height, nullptr, -40, 40},
      {"hud-follows-head", &o.hud_follows},
      {"dpad", nullptr, &dpad, nullptr, 0, 1},
      {"dpad-reach", nullptr, &o.dpad_reach, nullptr, 10, 40},
      {"vibration", &o.vibration},
      {"vibration-strength", nullptr, &o.vibration_strength, nullptr, 25, 200},
      {"world-vibration", &o.world_vibration},
      {"dpad-buzz", &o.dpad_buzz},
      {"gun-switch-buzz", &o.gun_switch_buzz},
      {"extra-vibration-kind", nullptr, &o.extra_vibration, nullptr, 0, 2},  // 0.1.6; 0.1.4's "extra-vibration": load_options
      {"menu-hand", nullptr, &menu_hand, nullptr, 0, 2},
      {"stick-click-menu", &o.stick_click_menu},
      {"move-direction", nullptr, &move_by, nullptr, 0, 1},
      {"body", &o.body},
      {"body-shown", nullptr, &body_shown, nullptr, 0, 6},
      {"hide-arms-near-eyes", &o.hide_arms_near_eyes},
      {"fade-body-near-eyes", &o.fade_near_eyes},
      {"keep-body-upright", &o.steady_body},
      {"fingers", &o.fingers},
      {"predict-hands", &o.predict_hands},
      {"hands-on-world", &o.hands_on_world},
      {"physical-crouch", &o.physical_crouch},
      {"crouch-drop", nullptr, &o.crouch_drop, nullptr, 10, 60},
      {"arm-length", nullptr, &o.arm_length, nullptr, 70, 150},
      {"body-height", nullptr, &o.body_height, nullptr, -30, 30},
      {"body-forward", nullptr, &o.body_forward, nullptr, -30, 30},
      {"view-height", nullptr, &o.view_height, nullptr, -40, 40},
      {"hand-melee", &o.hand_melee},
      {"melee-speed", nullptr, &o.melee_speed, nullptr, 10, 50},
      {"melee-grip", &o.melee_grip},
      {"gun-melee", &o.gun_melee},  // 0.1.10 (LIST-0.1.10 item 1): Gun melee, on to start
      {"spread-spin-kick", &o.spread_spin_kick},
      {"hand-aim", &o.hand_aim},
      {"two-hand-grip", &o.two_hand_grip},
      {"holster-draw", &o.holster_draw},
      {"holster-size", nullptr, &o.holster_size, nullptr, 50, 200},
      {"holster-on-foot", &o.holster_on_foot},
      {"ride-gun", &o.ride_gun},  // 0.1.8: Hold the gun while driving, on to start
      {"gun-angle", nullptr, &o.gun_angle, nullptr, -45, 45},
      {"crosshair-mark", nullptr, &o.crosshair, nullptr, 0, 7},  // 0.1.7: Off to start (0.1.6's "crosshair-shape", 0.1.5's "crosshair-kind", 0.1.3's "crosshair": load_options)
      {"crosshair-size", nullptr, &o.crosshair_size, nullptr, 50, 300},
      {"crosshair-color", nullptr, &o.crosshair_color, nullptr, 0, 5},
      {"crosshair-opacity", nullptr, &o.crosshair_opacity, nullptr, 20, 100},
      {"crosshair-enemy", &o.crosshair_enemy},
      {"eye-resolution", nullptr, &o.resolution, nullptr, 50, 150},  // 0.1.6; 0.1.0's "resolution": load_options
      {"smoother-frames", &o.space_warp},  // 0.1.0 wrote "space-warp" (on to start then): read no more
      {"cpu-gpu-boost-on", &o.cpu_gpu_boost},  // 0.1.6: on to start (0.1.4's "cpu-gpu-boost": load_options)
      {"foveation", nullptr, &o.foveation, nullptr, 0, 4},
      {"refresh-hz", nullptr, &o.refresh_hz, nullptr, 0, 120},
      {"max-draw-distance", &o.max_draw_distance},
      {"anti-aliasing", nullptr, &o.msaa, nullptr, 1, 8},             // 0.1.6; 0.1.0's "msaa": load_options
      {"anisotropic-filtering", nullptr, &o.anisotropy, nullptr, 1, 16},  // 0.1.6; 0.1.0's "anisotropy"
      {"game-frame-rate", nullptr, &o.frame_rate, nullptr, 0, 3},  // 0.1.3; 0.1.1's "frame-rate": load_options
      {"dynamic-resolution-on", &o.dynamic_resolution},  // 0.1.3: on to start (0.1.1's "dynamic-resolution" off)
      {"show-performance", &o.show_performance},
      {"share-eye-work", &o.share_eye_work},
      {"sharpening-strength", nullptr, &o.sharpen, nullptr, 0, 2},  // 0.1.6; 0.1.3's "sharpening": load_options
      {"ps2-detail", &o.ps2_detail},
      {"multiplayer", &o.multiplayer},
  };
}

// Each option as the file writes it, "name value" (save_options, and the log's copy at the start).
std::vector<std::string> option_words(Options o) {
  int cutscene = static_cast<int>(o.cutscene_view), dpad = static_cast<int>(o.dpad_place);
  int turn = static_cast<int>(o.turn), menu_hand = static_cast<int>(o.menu_hand);
  int move_by = static_cast<int>(o.move_by), body_shown = static_cast<int>(o.body_shown);
  std::vector<std::string> words;
  for (auto& f : fields(o, cutscene, dpad, turn, menu_hand, move_by, body_shown)) {
    std::ostringstream out;
    out << f.name << ' ';
    if (f.flag) {
      out << (*f.flag ? "on" : "off");
    } else if (f.number) {
      out << *f.number;
    } else if (f.real) {
      out << *f.real;
    }
    words.push_back(out.str());
  }
  return words;
}

}  // namespace

const Options& options() {
  // Read without the lock: a change replaces fields that are each a word; a reader seeing half a change sees two
  // settings each valid. The menu changes one at a time.
  return s_options;
}

void set_options(const Options& next) {
  {
    std::lock_guard<std::mutex> lock(s_mutex);
    s_options = next;
  }
  save_options();
}

std::string options_path() {
  return s_path;
}

// 0.1.6: a Quest 3 or 3S (Android's device names eureka and panther, the XR2 Gen 2), which starts with the owner's
// Graphics (vr_options.h); the Quest 2, the first Quest, the Quest Pro and any other headset keep the lighter ones.
static bool strong_quest() {
#if defined(__ANDROID__)
  char device[PROP_VALUE_MAX] = {};
  __system_property_get("ro.product.device", device);
  const std::string d = device;
  return d == "eureka" || d == "panther";
#else
  return false;
#endif
}

void load_options(int game_version) {
  std::lock_guard<std::mutex> lock(s_mutex);
  const auto dir = file_util::get_user_settings_dir(static_cast<GameVersion>(game_version));
  s_path = (dir / "vr-options.txt").string();
  Options o;
  const bool strong = strong_quest();
  if (strong) {
    o.sharpen = 2;
    o.resolution = 150;  // 0.1.6, the owner: "for quest 3/3s, res in launcher should be 150%"
  }
  int cutscene = static_cast<int>(o.cutscene_view), dpad = static_cast<int>(o.dpad_place);
  int turn = static_cast<int>(o.turn), menu_hand = static_cast<int>(o.menu_hand);
  int move_by = static_cast<int>(o.move_by), body_shown = static_cast<int>(o.body_shown);
  std::ifstream in(s_path);
  if (in) {
    auto list = fields(o, cutscene, dpad, turn, menu_hand, move_by, body_shown);
    std::string line;
    bool new_rate = false;
    int old_rate = -1;
    bool new_cross = false;  // 0.1.7: "crosshair-mark" in the file
    int shape_cross = -1;    // 0.1.6's "crosshair-shape" (the Dot to start then)
    int kind_cross = -1;     // 0.1.5's "crosshair-kind" (Off to start then)
    int old_cross = -1;      // 0.1.3's and 0.1.4's "crosshair" (the Dot to start then)
    bool new_extra = false;  // 0.1.6: "extra-vibration-kind" in the file
    int old_extra = -1;      // 0.1.4's and 0.1.5's "extra-vibration"
    bool new_boost = false;  // 0.1.6: "cpu-gpu-boost-on" in the file
    bool new_vheight = false;  // 0.1.10: "vehicle-view-height-cm" in the file
    int old_vheight = 0;       // 0.1.3 to 0.1.9's "vehicle-view-height" (0, the start then, if none)
    int old_boost = -1;      // 0.1.0's to 0.1.5's "cpu-gpu-boost" (1 on, 0 off)
    // 0.1.6: anti-aliasing, anisotropic filtering and sharpening's new keys (the Quest 3's new start), and the old
    // (and Eye resolution's: 150% on the Quest 3's new start)
    int new_gfx[4] = {0, 0, 0, 0};
    int old_gfx[4] = {-1, -1, -1, -1};
    static const char* const kNewGfx[4] = {"anti-aliasing", "anisotropic-filtering", "sharpening-strength",
                                           "eye-resolution"};
    static const char* const kOldGfx[4] = {"msaa", "anisotropy", "sharpening", "resolution"};
    while (std::getline(in, line)) {
      std::istringstream words(line);
      std::string name, value;
      if (!(words >> name >> value) || name.empty() || name[0] == '#') {
        continue;
      }
      // 0.1.3: Game frame rate's key changed with its start (Automatic); 0.1.1's own choice of the headset's or half
      // is kept, its 60 (the start then) becomes Automatic.
      if (name == "game-frame-rate") {
        new_rate = true;
      } else if (name == "frame-rate") {
        try {
          old_rate = std::stoi(value);
        } catch (...) {
        }
      }
      // The crosshair's key changed with its start: 0.1.5's "crosshair-kind" (Off to start), 0.1.6's "crosshair-shape"
      // (the Dot to start again), 0.1.7's "crosshair-mark" (Off to start again, the owner: "make it so crosshair is
      // off by default again this update"). The Dot of 0.1.6, 0.1.3 and 0.1.4 (their start) is Off now; any other
      // shape a player chose is kept.
      if (name == "crosshair-mark") {
        new_cross = true;
      } else if (name == "crosshair-shape") {
        try {
          shape_cross = std::stoi(value);
        } catch (...) {
        }
      } else if (name == "crosshair-kind") {
        try {
          kind_cross = std::stoi(value);
        } catch (...) {
        }
      } else if (name == "crosshair") {
        try {
          old_cross = std::stoi(value);
        } catch (...) {
        }
      }
      // 0.1.6: Extra vibration's and CPU and GPU boost's keys changed with their starts (the whole world, and boost on
      // the Quest too): a player's Off of Extra vibration is kept, its Pickups (the start then) is the whole world; a
      // PC player's boost off is kept (on was the PC's start), the Quest's off (its start from 0.1.4) is left behind.
      if (name == "extra-vibration-kind") {
        new_extra = true;
      } else if (name == "extra-vibration") {
        try {
          old_extra = std::stoi(value);
        } catch (...) {
        }
      }
      // 0.1.10 (LIST-0.1.10 item 6): Vehicle view height's key changed with its start; 0.1.3 to 0.1.9's start (0) is
      // the new one, a height the player chose is kept
      if (name == "vehicle-view-height-cm") {
        new_vheight = true;
      } else if (name == "vehicle-view-height") {
        try {
          old_vheight = std::stoi(value);
        } catch (...) {
        }
      }
      if (name == "cpu-gpu-boost-on") {
        new_boost = true;
      } else if (name == "cpu-gpu-boost") {
        old_boost = value == "on" || value == "1" || value == "true" ? 1 : 0;
      }
      for (int g = 0; g < 4; ++g) {
        if (name == kNewGfx[g]) {
          new_gfx[g] = 1;
        } else if (name == kOldGfx[g]) {
          try {
            old_gfx[g] = std::stoi(value);
          } catch (...) {
          }
        }
      }
      for (auto& f : list) {
        if (name != f.name) {
          continue;
        }
        try {
          if (f.flag) {
            *f.flag = value == "on" || value == "1" || value == "true";
          } else if (f.number) {
            *f.number = std::clamp(std::stoi(value), f.low, f.high);
          } else if (f.real) {
            *f.real = std::clamp(std::stof(value), 0.5f, 2.0f);
          }
        } catch (...) {
        }
      }
    }
    if (!new_rate && (old_rate == 1 || old_rate == 2)) {
      o.frame_rate = old_rate;
    }
    if (!new_cross) {
      if (shape_cross >= 0) {
        if (shape_cross >= 2 && shape_cross <= 7) {
          o.crosshair = shape_cross;  // 0.1.6: a shape the player chose (its Dot, the start then, is Off now)
        }
      } else if (kind_cross >= 0) {
        if (kind_cross >= 1 && kind_cross <= 7) {
          o.crosshair = kind_cross;  // 0.1.5: a kind the player chose (Off was the start then)
        }
      } else if (old_cross >= 2 && old_cross <= 6) {
        o.crosshair = old_cross;  // 0.1.3 and 0.1.4: a kind the player chose (the Dot was the start then)
      }
    }
    if (!new_vheight && old_vheight != 0) {
      o.vehicle_view_height = std::clamp(old_vheight, -50, 50);
    }
    if (!new_extra && old_extra == 0) {
      o.extra_vibration = 0;
    }
#if !defined(__ANDROID__)
    if (!new_boost && old_boost == 0) {
      o.cpu_gpu_boost = false;
    }
#else
    (void)new_boost;
#endif
    // The old start (every Quest's in 0.1.4 and 0.1.5: Off, Off, Normal, 100%; the PC's up to 0.1.5: 2x, 16x, Off,
    // 100%) is left behind for the new (Off and Off everywhere, the owner: "anti aliasing/anisotropic filtering should
    // be off by default"; Strong and 150% on a Quest 3 or 3S); any other choice is the player's own, kept.
    int* const gfx[4] = {&o.msaa, &o.anisotropy, &o.sharpen, &o.resolution};
#if defined(__ANDROID__)
    static const int kOldStart[4] = {1, 1, 1, 100};
#else
    static const int kOldStart[4] = {2, 16, 0, 100};
#endif
    static const int kLow[4] = {1, 1, 0, 50}, kHigh[4] = {8, 16, 2, 150};
    for (int g = 0; g < 4; ++g) {
      if (new_gfx[g] || old_gfx[g] < 0 || old_gfx[g] == kOldStart[g]) {
        continue;
      }
      *gfx[g] = std::clamp(old_gfx[g], kLow[g], kHigh[g]);
    }
    lg::info("[vr] options from {}", s_path);
  } else {
    lg::info("[vr] no {} yet: the defaults", s_path);
  }
  o.cutscene_view = clamp_enum(cutscene, CutsceneView::kCount);
  o.dpad_place = clamp_enum(dpad, DpadPlace::kCount);
  o.turn = clamp_enum(turn, TurnKind::kCount);
  o.menu_hand = clamp_enum(menu_hand, MenuHand::kCount);
  o.move_by = clamp_enum(move_by, MoveBy::kCount);
  o.body_shown = clamp_enum(body_shown, BodyShown::kCount);
  s_options = o;
  // 0.1.5 (LIST-0.1.5 item 11): every option in the log, so a game's log alone says what the player had on; read
  // again (the headset's start, then the game's: set_game) only when one changed.
  static std::vector<std::string> s_logged;
  auto words = option_words(o);
  if (words == s_logged) {
    s_loaded = true;
    return;
  }
  s_logged = words;
  std::string line;
  int on_line = 0;
  for (const auto& w : words) {
    line += (on_line ? ", " : "") + w;
    if (++on_line == 12) {
      lg::info("[vr] options: {}", line);
      line.clear();
      on_line = 0;
    }
  }
  if (on_line) {
    lg::info("[vr] options: {}", line);
  }
  s_loaded = true;
}

bool options_loaded() {
  return s_loaded.load();
}

void save_options() {
  std::lock_guard<std::mutex> lock(s_mutex);
  if (s_path.empty()) {
    return;
  }
  std::ostringstream out;
  out << "# Jak and Daxter Quest VR options: the launcher and the in-game VR menu write this.\n";
  for (const auto& w : option_words(s_options)) {
    out << w << '\n';
  }
  try {
    file_util::create_dir_if_needed_for_file(s_path);
    file_util::write_text_file(s_path, out.str());
  } catch (const std::exception& e) {
    lg::warn("[vr] could not write {}: {}", s_path, e.what());
  }
}

}  // namespace jakvr
