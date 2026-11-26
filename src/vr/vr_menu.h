// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 the Jak and Daxter Quest VR contributors
//
// The in-game VR menu (list item 29), the same in all three games: every VR option (vr_options.h) as a row, on or off
// or a value to step, chosen with either stick or pointed at with a hand, as Ratchet's VR Options. Opened from each
// game's own options menu ("VR Options", goal_src/jakN/pc/pc-vr.gc pc-vr-menu-open!) or by holding the left menu button
// a second in a game menu. The runtime keeps the rows and the layout; each game draws what it is given with its own
// font (vr-menu-draw in pc-vr.gc) on the HUD, in the games' 512 by 416 screen.

#pragma once

#include <cstdint>
#include <string>

namespace jakvr::menu {

// The controllers this tick, for the menu.
struct Input {
  float stick_x = 0, stick_y = 0;  // the further pushed of the two sticks (up positive)
  bool confirm = false;            // A held
  bool back = false;               // B or Y held
  bool point_valid = false;        // the pointing hand's ray meets the panel
  float point_u = 0, point_v = 0;  // there: 0..1 from the left, 0..1 from the top
  bool point_press = false;        // the pointing hand's trigger held
};

// The games' screen, where the menu is laid out.
inline constexpr float kScreenWidth = 512.0f;
inline constexpr float kScreenHeight = 416.0f;
// The game's own 2D screen's height: Jak II and 3's text and boxes are 512 by 416, Jak 1's 512 by 224 (box() and
// text() give the game its own; the pointer on the games' menus is in it too).
float game_screen_height();

void set_game(int game_version);
bool shown();
void open(const char* why);
// Jak 1's Multiplayer, a page of its own (LIST-0.1.6 item 22): opened by the title's and the pause menu's
// "Multiplayer"; Back there closes it. At the title there is no game to play on with, only a save or a new game.
void open_multiplayer(bool at_title);
void close();
// One tick of the pad while shown (game thread). The game gets none of the controllers meanwhile.
void update(const Input& in, double now);

// What to draw, in the games' screen. Boxes: 1 the menu's background, 2 the chosen row.
bool box(int index, int& kind, float out[4]);
// Words: colour is the games' font-color (0 default, 5 yellow, 10 menu-selected); align 0 left, 1 centre, 2 right;
// x is the left edge, the middle or the right edge; the game wraps them within width.
struct Text {
  std::string words;
  float x = 0, y = 0;
  int color = 0;
  int align = 0;
  float scale = 0.6f;
  float width = 420.0f;
};
bool text(int index, Text& out);

}  // namespace jakvr::menu
