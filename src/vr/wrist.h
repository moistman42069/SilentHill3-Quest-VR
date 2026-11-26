// SPDX-License-Identifier: GPL-3.0-or-later
// SH3 Quest VR (LIST 49, the owner: the health bar "on top of her wrist where her watch is", shown only when looked at,
// fading; message 37: "silent hill 3 has a health and stamina bar"): its picture, drawn by the port. A dark plate with
// a light rim; inside, the health bar (the game's HP over its most: Heather's +0x180 / +0x184) in the game's yellow, red
// under a quarter, pulsing under half as the game's Life Display, a tick every tenth; under it a thinner green stamina
// bar (the most 0x712C624 less her tiredness 0x712C620, as the game's own bar 0x5A5E40 draws it). The headset shows it
// as a quad on the back of her left wrist (xr_openxr.cpp), the PC rig lays it on its pictures (xr_sim.cpp). RGBA8, alpha
// premultiplied, the first row the bottom as a GL texture's. Plain data.

#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace quest {

inline constexpr int kWristWidth = 256;           // the picture's pixels
inline constexpr int kWristHeight = 64;
inline constexpr float kWristMetresWide = 0.050f;  // the quad across her wrist's back (vr142: 6 cm along it overhung)
inline constexpr float kWristMetresHigh = kWristMetresWide * kWristHeight / kWristWidth;

// The health bar's color at `health` (0..1): the game's yellow (its Life Gauge's health bar, owner message 37), red
// under a quarter (the game's own warning steps: its red cross pulses under 50, 25 and 10 percent).
inline void wrist_color(float health, float rgb[3]) {
  if (health > 0.25f) rgb[0] = 0.95f, rgb[1] = 0.80f, rgb[2] = 0.22f;
  else rgb[0] = 0.92f, rgb[1] = 0.18f, rgb[2] = 0.15f;
}

// The stamina bar's color: the game's green (its Life Gauge's second bar).
inline void wrist_stamina_color(float rgb[3]) { rgb[0] = 0.35f, rgb[1] = 0.85f, rgb[2] = 0.38f; }

// How much of pixel (x, y)'s middle is inside a rounded box (x0..x1, y0..y1, corner radius r): 0..1, a pixel's soft edge.
inline float wrist_inside(float x, float y, float x0, float y0, float x1, float y1, float r) {
  const float cx = std::clamp(x, x0 + r, x1 - r), cy = std::clamp(y, y0 + r, y1 - r);
  const float d = std::sqrt((x - cx) * (x - cx) + (y - cy) * (y - cy)) - r;
  return std::clamp(0.5f - d, 0.0f, 1.0f);
}

// One bar (box x0..x1, y0..y1, the first row the bottom) filled to `fill` (0..1) in `color`, `glow` its light (the
// pulse), ticks every tenth when `ticks`: blended over rgb/a at pixel (px, py).
inline void wrist_bar(float px, float py, float x0, float y0, float x1, float y1, float fill, const float color[3],
                      float glow, bool ticks, float rgb[3], float& a) {
  const float in_bar = wrist_inside(px, py, x0, y0, x1, y1, std::min(6.0f, (y1 - y0) * 0.5f));
  if (in_bar <= 0.0f) return;
  const float end = x0 + (x1 - x0) * fill;
  const float lit = std::clamp(end - px + 0.5f, 0.0f, 1.0f);  // the filled part, its end soft
  float c3[3];
  for (int c = 0; c < 3; ++c) c3[c] = color[c] * (0.20f + 0.80f * lit * glow);
  // a little lighter along the top, as a lit tube
  const float sheen = 1.0f + 0.25f * std::clamp((py - (y0 + y1) * 0.5f) / ((y1 - y0) * 0.5f), 0.0f, 1.0f);
  float tick = 1.0f;
  if (ticks) {
    const float tenth = (px - x0) / (x1 - x0) * 10.0f;
    if (std::fabs(tenth - std::round(tenth)) * (x1 - x0) / 10.0f < 1.0f && tenth > 0.5f && tenth < 9.5f) tick = 0.55f;
  }
  for (int c = 0; c < 3; ++c) rgb[c] += (std::min(1.0f, c3[c] * sheen * tick) - rgb[c]) * in_bar;
  a += (0.97f - a) * in_bar;
}

// `pulse` (0..1): the health bar's light, the game's Life Display rhythm while her health is under half (1: steady).
// `stamina` (0..1): the second, thinner bar under it (owner message 37: "silent hill 3 has a health and stamina bar").
inline std::vector<std::uint8_t> render_wrist(float health, float alpha, float pulse = 1.0f, float stamina = 1.0f) {
  const int w = kWristWidth, h = kWristHeight;
  std::vector<std::uint8_t> out(static_cast<std::size_t>(w) * h * 4, 0);
  health = std::clamp(std::isfinite(health) ? health : 0.0f, 0.0f, 1.0f);
  stamina = std::clamp(std::isfinite(stamina) ? stamina : 1.0f, 0.0f, 1.0f);
  alpha = std::clamp(std::isfinite(alpha) ? alpha : 0.0f, 0.0f, 1.0f);
  pulse = std::clamp(std::isfinite(pulse) ? pulse : 1.0f, 0.0f, 1.0f);
  float bar[3], green[3];
  wrist_color(health, bar);
  wrist_stamina_color(green);
  const float bx0 = 14, bx1 = w - 14.0f;
  const float hy0 = 29, hy1 = h - 13.0f;  // the health bar (the upper; the first row is the bottom)
  const float sy0 = 13, sy1 = 24;         // the stamina bar under it
  for (int y = 0; y < h; ++y) {
    for (int x = 0; x < w; ++x) {
      const float px = x + 0.5f, py = y + 0.5f;
      const float plate = wrist_inside(px, py, 0.5f, 0.5f, w - 0.5f, h - 0.5f, 16.0f);
      if (plate <= 0.0f) continue;
      const float inner = wrist_inside(px, py, 3.0f, 3.0f, w - 3.0f, h - 3.0f, 13.5f);
      float rgb[3] = {0.06f, 0.06f, 0.07f}, a = 0.80f;
      if (inner < 1.0f) {  // the rim
        const float k = 1.0f - inner;
        for (int c = 0; c < 3; ++c) rgb[c] += (0.62f - rgb[c]) * k;
        a += (0.95f - a) * k;
      }
      wrist_bar(px, py, bx0, hy0, bx1, hy1, health, bar, 0.45f + 0.55f * pulse, true, rgb, a);
      wrist_bar(px, py, bx0, sy0, bx1, sy1, stamina, green, 1.0f, false, rgb, a);
      a *= plate * alpha;
      std::uint8_t* d = out.data() + (static_cast<std::size_t>(y) * w + x) * 4;
      for (int c = 0; c < 3; ++c) d[c] = static_cast<std::uint8_t>(std::lround(std::clamp(rgb[c] * a, 0.0f, 1.0f) * 255.0f));
      d[3] = static_cast<std::uint8_t>(std::lround(std::clamp(a, 0.0f, 1.0f) * 255.0f));
    }
  }
  return out;
}

}  // namespace quest
