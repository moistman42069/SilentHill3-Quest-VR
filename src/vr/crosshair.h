// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 the Ratchet & Clank Quest VR and Jak and Daxter Quest VR contributors
//
// The gun's crosshair (Jak and Daxter Quest VR 0.1.3, LIST-0.1.3 item 27): its picture, drawn by the port, in the
// player's kind and color with a dark rim so it reads on any background. The runtime (jak_vr.cpp) puts it where the
// shot would hit, the game's own probe along the shot's line (gun-util.gc vr-crosshair!), and the headset shows it
// there as a quad facing the eyes, the same size to the eye at any distance (as the HaloCE port's reticle). RGBA8,
// alpha premultiplied, the first row the bottom as a GL texture's. Plain data.

#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace quest {

inline constexpr int kCrosshairPixels = 128;   // the picture's width and height
inline constexpr float kCrosshairDegrees = 2.0f;  // across the picture at size 100%, to the eye
inline constexpr int kCrosshairKinds = 8;      // 0 off, then each below (0.1.5: 7 Small circle)
inline constexpr int kCrosshairColors = 6;

// The player's colors (vr_menu.cpp kCrosshairColor's order), and red for an enemy (yellow when theirs is red).
inline void crosshair_color(int color, bool enemy, float rgb[3]) {
  static constexpr float kColors[kCrosshairColors][3] = {
      {1.00f, 1.00f, 1.00f},  // white
      {0.35f, 1.00f, 0.35f},  // green
      {1.00f, 0.90f, 0.15f},  // yellow
      {0.25f, 0.90f, 1.00f},  // cyan
      {1.00f, 0.22f, 0.18f},  // red
      {1.00f, 0.45f, 0.85f},  // pink
  };
  const int c = std::clamp(color, 0, kCrosshairColors - 1);
  const float* pick = kColors[c];
  if (enemy) {
    pick = c == 4 ? kColors[2] : kColors[4];
  }
  rgb[0] = pick[0];
  rgb[1] = pick[1];
  rgb[2] = pick[2];
}

// How far a point (x, y), -1 to 1 across the picture, is outside the kind's shape (below 0: inside).
inline float crosshair_distance(int kind, float x, float y) {
  const auto segment = [&](float ax, float ay, float bx, float by, float half_width) {
    const float dx = bx - ax, dy = by - ay;
    const float t = std::clamp(((x - ax) * dx + (y - ay) * dy) / (dx * dx + dy * dy), 0.0f, 1.0f);
    const float px = ax + dx * t - x, py = ay + dy * t - y;
    return std::sqrt(px * px + py * py) - half_width;
  };
  const float r = std::sqrt(x * x + y * y);
  const auto dot = [&](float radius) { return r - radius; };
  const auto ring = [&](float radius, float half_width) { return std::fabs(r - radius) - half_width; };
  const auto cross = [&](float from, float to, float half_width) {
    return std::min(std::min(segment(from, 0, to, 0, half_width), segment(-from, 0, -to, 0, half_width)),
                    std::min(segment(0, from, 0, to, half_width), segment(0, -from, 0, -to, half_width)));
  };
  switch (kind) {
    case 1:  // Dot
      return dot(0.20f);
    case 2:  // Cross: four arms with a gap at the middle
      return cross(0.30f, 0.86f, 0.07f);
    case 3:  // Cross and dot
      return std::min(cross(0.34f, 0.86f, 0.07f), dot(0.11f));
    case 4:  // Circle
      return ring(0.66f, 0.07f);
    case 5:  // Circle and dot
      return std::min(ring(0.66f, 0.07f), dot(0.11f));
    case 6:  // Plus: one solid cross
      return cross(0.0f, 0.62f, 0.07f);
    case 7:  // 0.1.5 Small circle (the owner: "a plain circle one. small circle, precise looking"): a thin ring
             // about half a degree across at 100% (the Circle is 1.3), its rim thin too (crosshair_rim); its line 0.09
             // degrees: whole from 14 pixels a degree up (the Quest 3 has about 25), sampled bilinear or mipmapped
      return ring(0.24f, 0.045f);
    default:
      return 1e9f;
  }
}

// The dark rim's width, of the half picture: the Small circle's thin, as its line.
inline float crosshair_rim(int kind) {
  return kind == 7 ? 0.045f : 0.06f;
}

// The picture: the shape in the color, its rim dark, both at `opacity` (0 to 1), its edges smoothed.
inline std::vector<std::uint8_t> render_crosshair(int kind, int color, bool enemy, float opacity) {
  const int n = kCrosshairPixels;
  std::vector<std::uint8_t> out(static_cast<std::size_t>(n) * n * 4, 0);
  float rgb[3];
  crosshair_color(color, enemy, rgb);
  const float pixel = 2.0f / static_cast<float>(n);
  const float kRim = crosshair_rim(kind);  // the dark rim's width, of the half picture
  constexpr float kRimAlpha = 0.75f;  // how dark it is
  const float a_most = std::clamp(opacity, 0.0f, 1.0f);
  for (int row = 0; row < n; ++row) {
    for (int col = 0; col < n; ++col) {
      const float x = (static_cast<float>(col) + 0.5f) * pixel - 1.0f;
      const float y = (static_cast<float>(row) + 0.5f) * pixel - 1.0f;
      const float d = crosshair_distance(kind, x, y);
      const float inside = std::clamp(0.5f - d / pixel, 0.0f, 1.0f);
      const float rim = std::clamp(0.5f - (d - kRim) / pixel, 0.0f, 1.0f) * kRimAlpha;
      // the shape over its rim: color c at alpha `inside` over black at alpha `rim`, premultiplied
      const float alpha = (inside + rim * (1.0f - inside)) * a_most;
      std::uint8_t* p = out.data() + (static_cast<std::size_t>(row) * n + col) * 4;
      for (int k = 0; k < 3; ++k) {
        p[k] = static_cast<std::uint8_t>(std::lround(std::clamp(rgb[k] * inside * a_most, 0.0f, 1.0f) * 255.0f));
      }
      p[3] = static_cast<std::uint8_t>(std::lround(std::clamp(alpha, 0.0f, 1.0f) * 255.0f));
    }
  }
  return out;
}

}  // namespace quest
