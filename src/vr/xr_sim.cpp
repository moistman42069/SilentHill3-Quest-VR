// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 the Ratchet & Clank Quest VR and Jak and Daxter Quest VR contributors
//
// A headset that is not there, for a desktop: the real game runs and the whole mod with it, and a
// script stands for the player. Nothing of this goes in the APK.
//
//   QUEST_SIM_SCRIPT=<file>  a line each: <frame> <name> <value>, values holding until changed.
//       a b x y left_click right_click: 0 or 1; left_x left_y right_x right_y left_trigger
//       right_trigger left_grip right_grip: -1..1; left_first_touch right_first_touch left_thumb_touch
//       right_thumb_touch left_rest_touch no_thumb_rest menu_unbound: 0 or 1; head_yaw hand_yaw (degrees, left positive);
//       head_pitch hand_pitch hand_roll (degrees, up positive); head_x head_y head_z (metres); shot <name>: the eyes and panel as
//       PNGs; quit 0: ends the run.
//   QUEST_SIM_OUT=<folder>   where the PNGs go.

#if !defined(__ANDROID__)

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <thread>
#include <type_traits>
#include <vector>

#include "crosshair.h"
#include "wrist.h"
#include "notice.h"
#include "xr.h"

// SH3 Quest VR: the Jak sound test aid (audio_loss) is not part of this port.

// SH3 Quest VR (session 13): the held weapon's second grip (sh3vr_combat.cpp), for "lh_at_second 1".
namespace sh3q { bool sim_second_spot(float out[3]); }
// SH3 Quest VR (session 16): the reload by hand's places (sh3vr_reload.cpp), for "lh_at_reload N".
namespace sh3q { bool sim_reload_spot(int what, float out[3]); }
// SH3 Quest VR (0.1.6): a holster's place (sh3vr_combat.cpp), for "lh_at_holster N" / "rh_at_holster N".
namespace sh3q { bool sim_holster_spot(int n, float out[3]); }
namespace sh3q { void sim_grip_spot(int v); }
// SH3 Quest VR (0.1.9): how long a scene of the game's has played (sh3vr.cpp), for QUEST_SIM_SKIP_SCENES.
namespace sh3q { int sim_scene_frames(); }

namespace quest {

// Set by the mod before make_headset: the port's GL.
void* (*sim_gl_proc)(const char*) = nullptr;
bool sim_left_grip = false;
// Set by the mod once the test's set-up is done (items given, Ratchet placed): QUEST_SIM_ALIGN's mark.
bool sim_ready = false;
// Set by the mod's shadow test aid (JAKVR_SHADOW_EYES, 0.1.8, LIST-0.1.8 item 9): a display frame whose shadows the
// eyes saw far apart, shot as "apart-<frame>" (the frame after it, the one being finished); -1 for none.
std::atomic<long> sim_shot_apart{-1};
// SH3 (0.1.8, session 32): set with each "shot" command, so the mod logs both hands as drawn for that picture (each
// finger's curl, the thumb's swing, the trigger: sh3vr_body.cpp body_fingers), the pictures read against numbers.
std::atomic<int> sim_shot_hands{0};

namespace {

using GLuint = unsigned int;
using GLenum = unsigned int;
using GLint = int;
using GLsizei = int;
constexpr GLenum kTexture2D = 0x0DE1, kRgba = 0x1908, kRgba8 = 0x8058, kUnsignedByte = 0x1401;
constexpr GLenum kFramebuffer = 0x8D40, kColorAttachment0 = 0x8CE0, kLinear = 0x2601;
constexpr GLenum kMinFilter = 0x2801, kMagFilter = 0x2800;
// Smoother frames' motion and depth images, as the Quest's runtime has them.
constexpr GLenum kRgba16f = 0x881A, kFloat = 0x1406, kNearest = 0x2600;
constexpr GLenum kDepth24Stencil8 = 0x88F0, kDepthStencil = 0x84F9, kUnsignedInt248 = 0x84FA;

#if defined(_WIN32)
#define GLAPI __stdcall
#else
#define GLAPI
#endif

struct Gl {
    void(GLAPI* GenTextures)(GLsizei, GLuint*);
    void(GLAPI* BindTexture)(GLenum, GLuint);
    void(GLAPI* TexImage2D)(GLenum, GLint, GLint, GLsizei, GLsizei, GLint, GLenum, GLenum, const void*);
    void(GLAPI* TexParameteri)(GLenum, GLenum, GLint);
    void(GLAPI* GenFramebuffers)(GLsizei, GLuint*);
    void(GLAPI* BindFramebuffer)(GLenum, GLuint);
    void(GLAPI* FramebufferTexture2D)(GLenum, GLenum, GLenum, GLuint, GLint);
    void(GLAPI* ReadPixels)(GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, void*);
};

std::uint32_t crc_of(const unsigned char* data, std::size_t size, std::uint32_t crc = 0xFFFFFFFFu) {
    for (std::size_t i = 0; i < size; ++i) {
        crc ^= data[i];
        for (int k = 0; k < 8; ++k) {
            crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
        }
    }
    return crc;
}

void put32(std::vector<unsigned char>& out, std::uint32_t v) {
    for (int shift = 24; shift >= 0; shift -= 8) {
        out.push_back(static_cast<unsigned char>(v >> shift));
    }
}

void chunk(std::ofstream& file, const char* name, const std::vector<unsigned char>& body) {
    std::vector<unsigned char> head;
    put32(head, static_cast<std::uint32_t>(body.size()));
    file.write(reinterpret_cast<const char*>(head.data()), 4);
    std::vector<unsigned char> sum(name, name + 4);
    sum.insert(sum.end(), body.begin(), body.end());
    file.write(reinterpret_cast<const char*>(sum.data()), static_cast<std::streamsize>(sum.size()));
    std::vector<unsigned char> tail;
    put32(tail, ~crc_of(sum.data(), sum.size()));
    file.write(reinterpret_cast<const char*>(tail.data()), 4);
}

// RGBA rows, last row first (as GL reads them), as an uncompressed PNG. `alpha`: keep it.
void write_png(const std::string& path, const std::vector<unsigned char>& rgba, int width, int height, bool alpha) {
    const int channels = alpha ? 4 : 3;
    std::vector<unsigned char> raw;
    raw.reserve(static_cast<std::size_t>(height) * (static_cast<std::size_t>(width) * channels + 1));
    for (int y = height - 1; y >= 0; --y) {
        raw.push_back(0);
        const unsigned char* row = rgba.data() + static_cast<std::size_t>(y) * width * 4;
        for (int x = 0; x < width; ++x) {
            raw.insert(raw.end(), row + x * 4, row + x * 4 + channels);
        }
    }
    std::vector<unsigned char> z = {0x78, 0x01};
    std::uint32_t a = 1, b = 0;
    for (std::size_t at = 0; at < raw.size();) {
        const std::size_t size = std::min<std::size_t>(65535, raw.size() - at);
        z.push_back(at + size == raw.size() ? 1 : 0);
        z.push_back(static_cast<unsigned char>(size));
        z.push_back(static_cast<unsigned char>(size >> 8));
        z.push_back(static_cast<unsigned char>(~size));
        z.push_back(static_cast<unsigned char>(~size >> 8));
        z.insert(z.end(), raw.begin() + static_cast<std::ptrdiff_t>(at), raw.begin() + static_cast<std::ptrdiff_t>(at + size));
        for (std::size_t i = at; i < at + size; ++i) {
            a = (a + raw[i]) % 65521;
            b = (b + a) % 65521;
        }
        at += size;
    }
    put32(z, (b << 16) | a);

    std::ofstream file(path, std::ios::binary);
    const unsigned char signature[8] = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};
    file.write(reinterpret_cast<const char*>(signature), 8);
    std::vector<unsigned char> header;
    put32(header, static_cast<std::uint32_t>(width));
    put32(header, static_cast<std::uint32_t>(height));
    header.push_back(8);
    header.push_back(alpha ? 6 : 2);
    header.push_back(0);
    header.push_back(0);
    header.push_back(0);
    chunk(file, "IHDR", header);
    chunk(file, "IDAT", z);
    chunk(file, "IEND", {});
}

struct Command {
    long frame;
    std::string name, value;
};

class Sim final : public Headset {
public:
    explicit Sim(void (*log)(int, const char*)) : m_log(log) {}

    bool start(const XrSettings& settings, std::string& why) override {
        const auto load = [](auto& fn, const char* name) {
            fn = reinterpret_cast<std::decay_t<decltype(fn)>>(sim_gl_proc(name));
        };
        if (sim_gl_proc == nullptr) {
            why = "the simulated headset needs the port's OpenGL";
            return false;
        }
        load(m_gl.GenTextures, "glGenTextures");
        load(m_gl.BindTexture, "glBindTexture");
        load(m_gl.TexImage2D, "glTexImage2D");
        load(m_gl.TexParameteri, "glTexParameteri");
        load(m_gl.GenFramebuffers, "glGenFramebuffers");
        load(m_gl.BindFramebuffer, "glBindFramebuffer");
        load(m_gl.FramebufferTexture2D, "glFramebufferTexture2D");
        load(m_gl.ReadPixels, "glReadPixels");
        const int eye = static_cast<int>(1000 * settings.resolution) & ~1;
        make_target(kTargetLeftEye, eye, eye);
        make_target(kTargetRightEye, eye, eye);
        make_target(kTargetPanel, settings.panel_width, settings.panel_height);
        // Smoother frames, as on the Quest when the launcher has it on: motion images a quarter of the eye's size.
        // QUEST_SIM_SPACE_WARP=0 leaves them out.
        const char* warp = std::getenv("QUEST_SIM_SPACE_WARP");
        m_space_warp = settings.space_warp && !(warp != nullptr && std::string(warp) == "0");
        if (m_space_warp) {
            m_motion_width = eye / 4;
            m_motion_height = eye / 4;
            for (int side = 0; side < 2; ++side) {
                m_gl.GenTextures(1, &m_motion[side]);
                m_gl.BindTexture(kTexture2D, m_motion[side]);
                m_gl.TexImage2D(kTexture2D, 0, static_cast<GLint>(kRgba16f), m_motion_width, m_motion_height, 0, kRgba, kFloat,
                                nullptr);
                m_gl.TexParameteri(kTexture2D, kMinFilter, static_cast<GLint>(kNearest));
                m_gl.TexParameteri(kTexture2D, kMagFilter, static_cast<GLint>(kNearest));
                m_gl.GenTextures(1, &m_motion_depth[side]);
                m_gl.BindTexture(kTexture2D, m_motion_depth[side]);
                m_gl.TexImage2D(kTexture2D, 0, static_cast<GLint>(kDepth24Stencil8), m_motion_width, m_motion_height, 0,
                                kDepthStencil, kUnsignedInt248, nullptr);
                m_gl.TexParameteri(kTexture2D, kMinFilter, static_cast<GLint>(kNearest));
                m_gl.TexParameteri(kTexture2D, kMagFilter, static_cast<GLint>(kNearest));
                m_gl.BindTexture(kTexture2D, 0);
                m_gl.GenFramebuffers(1, &m_motion_fbo[side]);
                m_gl.BindFramebuffer(kFramebuffer, m_motion_fbo[side]);
                m_gl.FramebufferTexture2D(kFramebuffer, kColorAttachment0, kTexture2D, m_motion[side], 0);
                m_gl.BindFramebuffer(kFramebuffer, 0);
            }
        }

        if (const char* path = std::getenv("QUEST_SIM_SCRIPT")) {
            std::ifstream script(path);
            std::string line;
            while (std::getline(script, line)) {
                std::istringstream words(line);
                Command command;
                if (line.empty() || line[0] == '#' || !(words >> command.frame >> command.name >> command.value)) {
                    continue;
                }
                m_script.push_back(command);
            }
        }
        if (const char* out = std::getenv("QUEST_SIM_OUT")) {
            m_out = out;
        }
        // QUEST_SIM_ALIGN=F: the script's frames from F on count from the frame the set-up was done, which is F where
        // the script was written (Novalis: about 2620), so that it runs as well on a level whose arrival gives control
        // at another, or a varying, frame (Eudora).
        if (const char* align = std::getenv("QUEST_SIM_ALIGN")) {
            m_align = std::atol(align);
        }
        // QUEST_SIM_GRIP_TILT=D: the hands' grip pose turned D degrees up from their aim pose, as the Quest's controllers
        // have it (the grip's forward up the handle); the script's hand angles stay the aim's, where the hand points.
        if (const char* tilt = std::getenv("QUEST_SIM_GRIP_TILT")) {
            m_grip_tilt = static_cast<float>(std::atof(tilt)) * kPi / 180.0f;
        }
        // QUEST_SIM_HZ=N (0.1.3, LIST item 4): the refresh rate the simulated headset tells the game (72 to start), for
        // Game frame rate's Automatic at 90 and 120.
        if (const char* hz = std::getenv("QUEST_SIM_HZ")) {
            m_hz = std::clamp(std::atof(hz), 30.0, 144.0);
        }
        // QUEST_SIM_PERIOD_BLIPS=N (0.1.5, LIST-0.1.5 item 4): every Nth frame tells a doubled period, as SteamVR does
        // for a frame it missed; the game's rate must not follow it (jak_vr.cpp RateHold).
        if (const char* blips = std::getenv("QUEST_SIM_PERIOD_BLIPS")) {
            m_period_blips = std::max(0, std::atoi(blips));
            m_log(1, ("[sim] a doubled frame period told every " + std::to_string(m_period_blips) + " frames").c_str());
        }
        m_log(1, ("simulated headset: " + std::to_string(m_script.size()) + " script lines").c_str());
        return true;
    }

    bool begin_frame(FrameInfo& out) override {
        // A real runtime's frame wait paces each frame, and a movie has no other pace (movie.cpp's headset_paces). So a frame asked for
        // far too soon waits out a 60th here, which keeps a script's frame numbers in time through a movie. The game's own frames
        // come at their 60 a second and do not wait.
        using PaceClock = std::chrono::steady_clock;
        if (PaceClock::now() - m_frame_asked < std::chrono::milliseconds(10)) {
            std::this_thread::sleep_until(m_frame_asked + std::chrono::microseconds(16667));
        }
        m_frame_asked = PaceClock::now();
        out = FrameInfo{};
        ++m_frame;
        if (m_ready_frame < 0 && sim_ready) {
            m_ready_frame = m_frame;
            if (m_align >= 0) {
                m_log(1, ("[sim] the set-up done at frame " + std::to_string(m_frame) + ": frame "
                          + std::to_string(m_align) + " of the script").c_str());
            }
        }
        while (m_next < m_script.size() && due(m_script[m_next].frame)) {
            apply(m_script[m_next++]);
        }
        out.should_render = true;
        out.focused = true;
        out.views_valid = true;
        out.frame_seconds = m_period_blips > 0 && m_frame % m_period_blips == 0 ? 2.0 / m_hz : 1.0 / m_hz;
        // 0.1.3 (owner 84): the frame's display time, one period on, two for a "late" frame (a headset shows a late
        // frame a display later): the VR layer's hand speeds take the real time between frames (motion_dt)
        m_display_ns += static_cast<std::int64_t>(1e9 / m_hz) * (number("late") != 0 ? 2 : 1);
        out.display_time = m_display_ns;
        m_work_start = std::chrono::steady_clock::now();  // 0.1.3: the frame's work from here (last_work_seconds)
        // head_yaw_rate / head_pitch_rate (0.1.8): degrees a second the head keeps turning or nodding, a frame's share
        // each frame, so a script's head moves all the time as a player's does (LIST-0.1.8 item 9, the shadows in each
        // eye); a line setting head_yaw or head_pitch sets where it is, and a rate of 0 stops it.
        for (const char* angle : {"head_yaw", "head_pitch"}) {
            const float rate = number(std::string(angle) + "_rate");
            if (rate != 0.0f) {
                m_values[angle] = number(angle) + rate / static_cast<float>(m_hz);
            }
        }
        const Quat yaw = yaw_quat(number("head_yaw") * kPi / 180.0f);
        const float pitch = number("head_pitch") * kPi / 360.0f;
        // head_roll (0.1.1): the head tilted to a shoulder, degrees, the right ear up positive
        const float roll = number("head_roll") * kPi / 360.0f;
        const Quat look = yaw * Quat{std::sin(pitch), 0, 0, std::cos(pitch)} * Quat{0, 0, std::sin(roll), std::cos(roll)};
        const Vec3 head{number("head_x"), 1.7f + number("head_y"), number("head_z")};
        // A real head nods about the neck, not about the eyes: looking down takes the eyes forward and down (the
        // neck 12 cm under the eyes and 15 cm behind them, as jak_vr.cpp's body hangs from it). Pitched about the eyes
        // the neck rose over them and the pictures showed the collar where no player sees it (sim j1_down7).
        const Vec3 neck_to_eyes{0.0f, 0.12f, -0.15f};
        // It turns about the neck too (SH3 session 6: the body hangs from the neck, rig.h neck_of; turned about the eyes
        // the neck swung round behind them and read as a step): head_x/y/z are the eyes with the head level, looking
        // ahead.
        const Vec3 eyes_at = head + rotate(look, neck_to_eyes) - neck_to_eyes;
        out.head = {eyes_at, look};
        m_frame_head = out.head;  // 0.1.9: a head-locked panel's place (stamp_panel)
        out.head_above_floor = eyes_at.y;
        // The Quest 3's eyes: 64 mm apart, each with about this field of view.
        const Vec3 half = rotate(look, {0.032f, 0, 0});
        out.eye[0] = {eyes_at - half, look};
        out.eye[1] = {eyes_at + half, look};
        // view_free 1: the eyes alone stand at view_x view_y view_z (metres from the head), turned view_yaw and
        // view_pitch: a look at the player's body from outside, the head that fits it staying where it is.
        if (number("view_free") != 0) {
            const float view_pitch = number("view_pitch") * kPi / 360.0f;
            const Quat view_look = yaw_quat(number("view_yaw") * kPi / 180.0f) * Quat{std::sin(view_pitch), 0, 0, std::cos(view_pitch)};
            const Vec3 view_at = head + Vec3{number("view_x"), number("view_y"), number("view_z")};
            const Vec3 view_half = rotate(view_look, {0.032f, 0, 0});
            out.eye[0] = {view_at - view_half, view_look};
            out.eye[1] = {view_at + view_half, view_look};
        }
        for (int eye = 0; eye < 2; ++eye) {
            out.fov[eye][0] = eye == 0 ? -0.95f : -0.75f;
            out.fov[eye][1] = eye == 0 ? 0.75f : 0.95f;
            out.fov[eye][2] = 0.85f;
            out.fov[eye][3] = -0.85f;
            // (0.1.3: the crosshair's stamp, when the port does not tell the views it drew with)
            m_frame_eye[eye] = out.eye[eye];
            std::copy(out.fov[eye], out.fov[eye] + 4, m_frame_fov[eye]);
        }
        m_drawn_set = false;
        // "late 1" (SH3 LIST 51): each frame from then counts as a display missed, as a headset falling behind tells it
        if (number("late") != 0) ++m_missed;
        const float hand_pitch = number("hand_pitch") * kPi / 360.0f;
        const float hand_roll = number("hand_roll") * kPi / 360.0f;
        const Quat hand = yaw_quat(number("hand_yaw") * kPi / 180.0f)
                          * Quat{std::sin(hand_pitch), 0, 0, std::cos(hand_pitch)}
                          * Quat{0, 0, std::sin(hand_roll), std::cos(hand_roll)};
        for (int side = 0; side < 2; ++side) {
            out.hand_valid[side] = number(side == 0 ? "left_gone" : "right_gone") == 0;
            out.hand_bound[side] = out.hand_valid[side];
            out.stick_active[side] = out.hand_valid[side];
            // held where a player holds them: turned with the head's yaw, not its pitch (looking down took them
            // behind his back, pulling his shoulders round into the eyes, sim j1_down16)
            const Vec3 at = head + rotate(yaw, {side == 0 ? -0.2f : 0.2f, -0.35f, -0.3f});
            out.hand_aim[side] = {at + Vec3{number(side == 0 ? "lh_x" : "rh_x"), number(side == 0 ? "lh_y" : "rh_y"),
                                            number(side == 0 ? "lh_z" : "rh_z")},
                                  hand};
            // SH3 (session 13), "lh_at_second 1": the left controller at the held weapon's second grip (last frame's),
            // lh_x lh_y lh_z then metres from it, so every weapon's two-hand hold is checked the same way.
            float spot[3];
            if (side == 0 && number("lh_at_second") != 0 && sh3q::sim_second_spot(spot))
                out.hand_aim[side].position = Vec3{spot[0], spot[1], spot[2]} + Vec3{number("lh_x"), number("lh_y"),
                                                                                     number("lh_z")};
            // SH3 (0.1.8), "rh_at_second 1": the right controller there instead (a left-handed player's other hand),
            // rh_x rh_y rh_z then metres from it
            if (side == 1 && number("rh_at_second") != 0 && sh3q::sim_second_spot(spot))
                out.hand_aim[side].position = Vec3{spot[0], spot[1], spot[2]} + Vec3{number("rh_x"), number("rh_y"),
                                                                                     number("rh_z")};
            // SH3 (session 16), "lh_at_reload N": the left controller where the reload by hand wants it
            // (sh3vr_reload.cpp sim_reload_spot: 1 the part lined up 5 cm short of its well, 2 pushed home, 3 at the
            // slide or the cocking knob), lh_x lh_y lh_z then metres from it: the code's own places, which follow the
            // guns as drawn, not numbers that go stale when a hold changes (vr237).
            const int at_reload = (int)number("lh_at_reload");
            if (side == 0 && at_reload != 0 && sh3q::sim_reload_spot(at_reload, spot))
                out.hand_aim[side].position = Vec3{spot[0], spot[1], spot[2]} + Vec3{number("lh_x"), number("lh_y"),
                                                                                     number("lh_z")};
            // SH3 (0.1.6), "lh_at_holster N" / "rh_at_holster N": the controller at a holster's place
            // (sh3vr_combat.cpp sim_holster_spot: 1 the gun's, 2 the melee weapon's, 3 the belt pouch, 4 the chest
            // pocket, 5 the ammo pouch), its lh_x lh_y lh_z (rh_...) then metres from it: the radius options checked.
            const int at_holster = (int)number(side == 0 ? "lh_at_holster" : "rh_at_holster");
            if (at_holster != 0 && sh3q::sim_holster_spot(at_holster, spot))
                out.hand_aim[side].position =
                    Vec3{spot[0], spot[1], spot[2]} + Vec3{number(side == 0 ? "lh_x" : "rh_x"),
                                                           number(side == 0 ? "lh_y" : "rh_y"),
                                                           number(side == 0 ? "lh_z" : "rh_z")};
            out.hand_grip[side] = {out.hand_aim[side].position,
                                   hand * Quat{std::sin(m_grip_tilt * 0.5f), 0, 0, std::cos(m_grip_tilt * 0.5f)}};
        }
        // SH3 (0.1.7), "grip_spot N": the VR menu's "Two-hand grip spot" set to N when the script changes it (1 Adjust,
        // 2 reset the weapon in the hand, 3 reset every weapon): the per-weapon spot checked on the rig.
        {
            static int last_spot = 0;
            const int gs = (int)number("grip_spot");
            if (gs != last_spot) {
                last_spot = gs;
                sh3q::sim_grip_spot(gs);
            }
        }
        Controls& c = out.controls;
        c.a = number("a") != 0;
        c.b = number("b") != 0;
        c.x = number("x") != 0;
        c.y = number("y") != 0;
        c.menu = number("menu") != 0;
        {
            // SH3 (0.1.9, session 34; vr452: a menu press timed for a scene came in play and opened the game's pause):
            // QUEST_SIM_SKIP_SCENES=N: from frame N on, the menu button pressed for 6 frames once a scene of the game's
            // has played 60 frames, and every 200 frames after while it goes on, so a script skips the scenes however
            // late they come and never presses in play (the opening's scenes before N play as the scripts expect).
            static const int skip_from =
                std::getenv("QUEST_SIM_SKIP_SCENES") ? std::atoi(std::getenv("QUEST_SIM_SKIP_SCENES")) : -1;
            if (skip_from >= 0 && due(skip_from)) {  // (the script's frames, aligned as its lines are)
                const int f = sh3q::sim_scene_frames();
                if (f >= 60 && (f - 60) % 200 < 6) c.menu = true;
            }
        }
        c.left_click = number("left_click") != 0;
        c.right_click = number("right_click") != 0;
        c.left_x = number("left_x");
        c.left_y = number("left_y");
        c.right_x = number("right_x");
        c.right_y = number("right_y");
        c.left_trigger = number("left_trigger");
        c.right_trigger = number("right_trigger");
        c.left_grip = sim_left_grip ? 1.0f : number("left_grip");
        c.right_grip = number("right_grip");
        c.left_first_touch = number("left_first_touch") != 0;
        c.right_first_touch = number("right_first_touch") != 0;
        c.left_thumb_touch = number("left_thumb_touch") != 0;
        c.right_thumb_touch = number("right_thumb_touch") != 0;
        c.left_rest_touch = number("left_rest_touch") != 0;
        // "no_thumb_rest 1": a controller with no thumb rest sensor (Index, Vive wands, WMR): the head's d-pad
        c.left_rest_sensed = number("no_thumb_rest") == 0;
        // "menu_unbound 1": the runtime binds no left menu button, as SteamVR with Touch controllers
        c.menu_sensed = number("menu_unbound") == 0;
        // "no_left_click 1": a controller with no left stick click (a Vive wand)
        c.left_click_sensed = number("no_left_click") == 0;
        return true;
    }

    void target_size(Target target, int& width, int& height) const override {
        width = m_width[target];
        height = m_height[target];
    }

    unsigned acquire(Target target) override { return m_texture[target]; }

    bool motion_target(int eye, MotionTarget& out) override {
        if (!m_space_warp || eye < 0 || eye > 1) {
            return false;
        }
        out = {m_motion[eye], m_motion_depth[eye], m_motion_width, m_motion_height};
        return true;
    }

    bool makes_frames() const override { return m_space_warp; }
    long displays_missed() const override { return m_missed; }
    double last_work_seconds() const override { return m_last_work; }

    void space_warp(const SpaceWarpFrame& warp) override {
        m_warp = warp;
        m_warp_set = true;
        m_warp_frames += warp.drawn[0] && warp.drawn[1] ? 1 : 0;
        m_warp_skipped += warp.skip ? 1 : 0;
        m_warp_kept += warp.kept ? 1 : 0;
        ++m_warp_asked;
    }

    void finish_frame(bool eyes, bool eyes_kept, const PanelLayer& panel) override {
        m_last_work = std::chrono::duration<double>(std::chrono::steady_clock::now() - m_work_start).count();  // 0.1.3
        char line[200];
        const bool warped = m_warp_set && eyes && !eyes_kept && m_warp.drawn[0] && m_warp.drawn[1] && !m_warp.skip;
        m_warp_set = false;
        if (m_frame % 300 == 0) {
            std::snprintf(line, sizeof line, "[sim] frame %ld eyes %d panel %d (hud %d)", m_frame, eyes ? 1 : 0,
                          panel.visible ? 1 : 0, panel.follows_head ? 1 : 0);
            m_log(1, line);
            if (m_space_warp) {
                std::snprintf(line, sizeof line,
                              "[sim] smoother frames: motion on %ld of the last 300 frames, drawn anew on %ld, marked to "
                              "skip on %ld (shown again on %ld of those) (depth %.1f to %.3f m)",
                              m_warp_asked, m_warp_frames, m_warp_skipped, m_warp_kept, m_warp.depth_zero,
                              m_warp.depth_one);
                m_log(1, line);
                m_warp_frames = m_warp_asked = m_warp_skipped = m_warp_kept = 0;
            }
        }
        if (m_shot.empty()) {
            const long apart = sim_shot_apart.exchange(-1);
            if (apart >= 0) {
                m_shot = "apart-" + std::to_string(apart);
            }
        }
        if (!m_shot.empty()) {
            const std::string base = m_out + "/" + m_shot;
            if (eyes) {
                save(kTargetLeftEye, base + "-left.png", false);
                save(kTargetRightEye, base + "-right.png", false);
            }
            if (m_space_warp) {
                save_motion(base + "-motion.png", warped);
            }
            if (panel.visible) {
                save(kTargetPanel, base + "-panel.png", panel.blend);
            }
            // 0.1.9 (LIST-0.1.9 items 9 and 11, the HUD's see-through squares and the map's double image, both seen
            // only in the headset): QUEST_SIM_HEADSET=1 adds each eye as the headset shows it, the panel's quad laid
            // over the eye's picture (stamp_panel), <shot>-headset-left.png and -right.png
            static const bool headset_shots = std::getenv("QUEST_SIM_HEADSET") != nullptr;
            if (headset_shots && eyes) {
                save_headset(base, panel);
            }
            std::snprintf(line, sizeof line, "[sim] shot %s: eyes %d panel %d blend %d", m_shot.c_str(),
                          eyes ? 1 : 0, panel.visible ? 1 : 0, panel.blend ? 1 : 0);
            m_log(1, line);
            if (eyes && m_cross.shown) {
                // 0.1.3: the crosshair as it is stamped on this shot's eyes
                char cross[220];
                std::snprintf(cross, sizeof cross,
                              "[sim] crosshair in shot %s: kind %d at %.2f m (%.2f %.2f %.2f), %.3f m across, color %d%s",
                              m_shot.c_str(), m_cross.kind, m_cross.distance, m_cross.pose.position.x,
                              m_cross.pose.position.y, m_cross.pose.position.z, m_cross.size, m_cross.color,
                              m_cross.enemy ? ", on an enemy" : "");
                m_log(1, cross);
            }
            m_shot.clear();
        }
    }

    void haptic(Hand hand, float strength, float seconds, float) override {
        char line[80];
        std::snprintf(line, sizeof line, "[sim] haptic hand %d %.2f for %.3f", static_cast<int>(hand), strength, seconds);
        m_log(1, line);
    }

    // 0.1.3 the eyes' poses and fields the pictures were drawn with (the crosshair's stamp sees it from there)
    void drawn_views(const Pose eye[2], const float fov[2][4]) override {
        for (int e = 0; e < 2; ++e) {
            m_drawn_eye[e] = eye[e];
            std::copy(fov[e], fov[e] + 4, m_drawn_fov[e]);
        }
        m_drawn_set = true;
    }

    // 0.1.3 the gun's crosshair: logged as it comes and goes, as an enemy comes under it, and every 2 s while shown;
    // laid on the eyes' pictures as the headset would show it (stamp_crosshair).
    void show_crosshair(const CrosshairLayer& crosshair) override {
        const bool changed = crosshair.shown != m_cross.shown || (crosshair.shown && crosshair.enemy != m_cross.enemy);
        m_cross = crosshair;
        if (changed || (crosshair.shown && m_frame - m_cross_logged >= 120)) {
            m_cross_logged = m_frame;
            char line[220];
            if (!crosshair.shown) {
                std::snprintf(line, sizeof line, "[sim] crosshair: none");
            } else {
                std::snprintf(line, sizeof line,
                              "[sim] crosshair: kind %d at %.2f m (%.2f %.2f %.2f), %.3f m across, color %d%s",
                              crosshair.kind, crosshair.distance, crosshair.pose.position.x, crosshair.pose.position.y,
                              crosshair.pose.position.z, crosshair.size, crosshair.color,
                              crosshair.enemy ? ", on an enemy" : "");
            }
            m_log(1, line);
        }
    }

    // The crosshair over eye `eye`'s picture (its first row the bottom), as the headset shows its quad: its middle seen
    // from the eye the picture was drawn from, its size to that eye, the picture laid on (premultiplied).
    void stamp_crosshair(int eye, std::vector<unsigned char>& pixels, int width, int height) {
        const Pose& e = m_drawn_set ? m_drawn_eye[eye] : m_frame_eye[eye];
        const float* fov = m_drawn_set ? m_drawn_fov[eye] : m_frame_fov[eye];
        const Vec3 local = rotate(conjugate(e.orientation), m_cross.pose.position - e.position);
        if (local.z > -0.01f) {
            return;
        }
        const float tl = std::tan(fov[0]), tr = std::tan(fov[1]), tu = std::tan(fov[2]), td = std::tan(fov[3]);
        const float cx = (local.x / -local.z - tl) / (tr - tl) * static_cast<float>(width);
        const float cy = (local.y / -local.z - td) / (tu - td) * static_cast<float>(height);
        const float half = 0.5f * m_cross.size / -local.z / (tr - tl) * static_cast<float>(width);
        if (!(half > 0.5f) || !std::isfinite(cx) || !std::isfinite(cy)) {
            return;
        }
        const std::vector<std::uint8_t> picture = render_crosshair(m_cross.kind, m_cross.color, m_cross.enemy,
                                                                   static_cast<float>(m_cross.opacity) / 100.0f);
        const int n = kCrosshairPixels;
        for (int py = static_cast<int>(std::floor(cy - half)); py <= static_cast<int>(std::ceil(cy + half)); ++py) {
            for (int px = static_cast<int>(std::floor(cx - half)); px <= static_cast<int>(std::ceil(cx + half)); ++px) {
                if (px < 0 || py < 0 || px >= width || py >= height) {
                    continue;
                }
                const int u = static_cast<int>((static_cast<float>(px) + 0.5f - (cx - half)) / (2.0f * half) * n);
                const int v = static_cast<int>((static_cast<float>(py) + 0.5f - (cy - half)) / (2.0f * half) * n);
                if (u < 0 || v < 0 || u >= n || v >= n) {
                    continue;
                }
                const std::uint8_t* s = picture.data() + (static_cast<std::size_t>(v) * n + u) * 4;
                unsigned char* d = pixels.data() + (static_cast<std::size_t>(py) * width + px) * 4;
                const float a = static_cast<float>(s[3]) / 255.0f;
                for (int k = 0; k < 3; ++k) {
                    d[k] = static_cast<unsigned char>(std::clamp(static_cast<float>(s[k]) + d[k] * (1.0f - a), 0.0f, 255.0f));
                }
            }
        }
    }


    // LIST 49 the health bar on her wrist: logged as it comes and goes and every 2 s while shown, laid on the eyes'
    // pictures as the headset would show it (stamp_wrist).
    void show_wrist(const WristLayer& wrist) override {
        const bool on = wrist.shown && wrist.alpha > 0.01f, was = m_wrist.shown && m_wrist.alpha > 0.01f;
        m_wrist = wrist;
        if (on != was || (on && m_frame - m_wrist_logged >= 120)) {
            m_wrist_logged = m_frame;
            char line[220];
            if (!on) {
                std::snprintf(line, sizeof line, "[sim] wrist bar: none");
            } else {
                std::snprintf(line, sizeof line,
                              "[sim] wrist bar: health %.2f, stamina %.2f, shown %.2f, pulse %.2f, at %.2f %.2f %.2f "
                              "(%.3f x %.3f m)",
                              wrist.health, wrist.stamina, wrist.alpha, wrist.pulse, wrist.pose.position.x,
                              wrist.pose.position.y, wrist.pose.position.z, wrist.width, wrist.height);
            }
            m_log(1, line);
        }
    }

    // A quad's picture (`picture`, pw x ph, premultiplied, its first row the bottom) at `p`, `qw` x `qh` metres, over eye
    // `eye`'s picture, as the runtime lays a quad: each pixel takes the picture where its ray meets the quad (nearest
    // texel). The pixels drawn.
    int stamp_quad(int eye, std::vector<unsigned char>& pixels, int width, int height, const Pose& p, float qw, float qh,
                   const std::vector<std::uint8_t>& picture, int pw, int ph) {
        const Pose& e = m_drawn_set ? m_drawn_eye[eye] : m_frame_eye[eye];
        const float* fov = m_drawn_set ? m_drawn_fov[eye] : m_frame_fov[eye];
        const Vec3 normal = rotate(p.orientation, {0, 0, 1});
        const Vec3 right = rotate(p.orientation, {1, 0, 0});
        const Vec3 up = rotate(p.orientation, {0, 1, 0});
        if (!(qw > 0.0f) || !(qh > 0.0f) || picture.size() < static_cast<std::size_t>(pw) * ph * 4) {
            return 0;
        }
        const float tl = std::tan(fov[0]), tr = std::tan(fov[1]), tu = std::tan(fov[2]), td = std::tan(fov[3]);
        const Vec3 to = p.position - e.position;
        int drawn = 0;
        for (int py = 0; py < height; ++py) {
            for (int px = 0; px < width; ++px) {
                const Vec3 local{tl + (static_cast<float>(px) + 0.5f) / static_cast<float>(width) * (tr - tl),
                                 td + (static_cast<float>(py) + 0.5f) / static_cast<float>(height) * (tu - td), -1.0f};
                const Vec3 d = rotate(e.orientation, local);
                const float denom = d.x * normal.x + d.y * normal.y + d.z * normal.z;
                if (std::fabs(denom) < 1e-6f) {
                    continue;
                }
                const float t = (to.x * normal.x + to.y * normal.y + to.z * normal.z) / denom;
                if (!(t > 0.0f)) {
                    continue;
                }
                const Vec3 rel = e.position + Vec3{d.x * t, d.y * t, d.z * t} - p.position;
                const float u = (rel.x * right.x + rel.y * right.y + rel.z * right.z) / qw + 0.5f;
                const float v = (rel.x * up.x + rel.y * up.y + rel.z * up.z) / qh + 0.5f;
                if (u < 0.0f || u >= 1.0f || v < 0.0f || v >= 1.0f) {
                    continue;
                }
                const int tx = std::min(pw - 1, static_cast<int>(u * pw));
                const int ty = std::min(ph - 1, static_cast<int>(v * ph));
                const std::uint8_t* s = picture.data() + (static_cast<std::size_t>(ty) * pw + tx) * 4;
                unsigned char* out = pixels.data() + (static_cast<std::size_t>(py) * width + px) * 4;
                const float a = static_cast<float>(s[3]) / 255.0f;
                for (int k = 0; k < 3; ++k) {
                    out[k] = static_cast<unsigned char>(std::clamp(static_cast<float>(s[k]) + out[k] * (1.0f - a), 0.0f, 255.0f));
                }
                ++drawn;
            }
        }
        return drawn;
    }

    // The wrist's bar over eye `eye`'s picture (stamp_quad).
    void stamp_wrist(int eye, std::vector<unsigned char>& pixels, int width, int height) {
        const int drawn = stamp_quad(eye, pixels, width, height, m_wrist.pose, m_wrist.width, m_wrist.height,
                                     render_wrist(m_wrist.health, m_wrist.alpha, m_wrist.pulse, m_wrist.stamina),
                                     kWristWidth, kWristHeight);
        if (eye == 0 && !m_shot.empty()) {
            char line[200];
            std::snprintf(line, sizeof line,
                          "[sim] wrist bar in shot %s: %d pixels of the left eye (health %.2f, stamina %.2f, shown %.2f)",
                          m_shot.c_str(), drawn, m_wrist.health, m_wrist.stamina, m_wrist.alpha);
            m_log(1, line);
        }
    }

    // 0.1.7 the break-free bar (owner message 128): logged as it comes and goes and twice a second while shown, laid on
    // the eyes' pictures as the headset would show it (stamp_hold).
    void show_hold(const HoldLayer& hold) override {
        const bool on = hold.shown && hold.alpha > 0.01f, was = m_hold.shown && m_hold.alpha > 0.01f;
        m_hold = hold;
        if (on != was || (on && m_frame - m_hold_logged >= 30)) {
            m_hold_logged = m_frame;
            char line[220];
            if (!on) {
                std::snprintf(line, sizeof line, "[sim] break-free bar: none");
            } else {
                std::snprintf(line, sizeof line,
                              "[sim] break-free bar: fill %.2f, shown %.2f, light %.2f, at %.2f %.2f %.2f (%.3f x %.3f m)",
                              hold.fill, hold.alpha, hold.glow, hold.pose.position.x, hold.pose.position.y,
                              hold.pose.position.z, hold.width, hold.height);
            }
            m_log(1, line);
        }
    }

    void stamp_hold(int eye, std::vector<unsigned char>& pixels, int width, int height) {
        const int drawn = stamp_quad(eye, pixels, width, height, m_hold.pose, m_hold.width, m_hold.height,
                                     render_hold(m_hold.fill, m_hold.alpha, m_hold.glow), kHoldWidth, kHoldHeight);
        if (eye == 0 && !m_shot.empty()) {
            char line[200];
            std::snprintf(line, sizeof line,
                          "[sim] break-free bar in shot %s: %d pixels of the left eye (fill %.2f, shown %.2f)",
                          m_shot.c_str(), drawn, m_hold.fill, m_hold.alpha);
            m_log(1, line);
        }
    }
    void show_pointer(const PointerLayers& pointer) override {
        if (pointer.dot != m_pointer_dot || pointer.beam != m_pointer_beam || pointer.dot2 != m_pointer_dot2 || pointer.beam2 != m_pointer_beam2) {
            m_pointer_dot = pointer.dot;
            m_pointer_beam = pointer.beam;
            m_pointer_dot2 = pointer.dot2;
            m_pointer_beam2 = pointer.beam2;
            char line[200];
            std::snprintf(line, sizeof line, "[sim] pointer: beam %d (%.2f m) dot %d at (%.2f %.2f %.2f); second beam %d dot %d at (%.2f %.2f %.2f)",
                          pointer.beam ? 1 : 0, pointer.beam_length, pointer.dot ? 1 : 0, pointer.dot_pose.position.x, pointer.dot_pose.position.y,
                          pointer.dot_pose.position.z, pointer.beam2 ? 1 : 0, pointer.dot2 ? 1 : 0, pointer.dot2_pose.position.x,
                          pointer.dot2_pose.position.y, pointer.dot2_pose.position.z);
            m_log(1, line);
        }
        // session 12: the dot's growth over a click zone, each time it crosses a third of the way
        const bool big = pointer.dot && pointer.dot_scale > 1.3f;
        if (big != m_pointer_big) {
            m_pointer_big = big;
            char line[120];
            std::snprintf(line, sizeof line, "[sim] frame %ld: the pointer's dot %s (scale %.2f)", m_frame,
                          big ? "grown over a click zone" : "back to its size", pointer.dot_scale);
            m_log(1, line);
        }
    }

    void show_notice(const char* text) override {
        if (text != nullptr && text[0] != 0) {
            write_png(m_out + "/notice.png", render_notice(text), kNoticeWidth, kNoticeHeight, false);
        }
    }

    // 0.1.3 Show performance: the line in the log every 5 s of the script, and its picture (perf.png) the first time.
    void show_performance(const char* text) override {
        if (text == nullptr || text[0] == 0 || (m_perf_frame >= 0 && m_frame - m_perf_frame < 360)) {
            return;
        }
        if (m_perf_frame < 0) {
            write_png(m_out + "/perf.png", render_notice(text, true), kNoticeWidth, kNoticeHeight, false);
        }
        m_perf_frame = m_frame;
        m_log(1, (std::string("[sim] performance: ") + text).c_str());
    }

    bool wants_exit() const override { return m_quit; }
    void stop() override {}

private:
    long m_perf_frame = -1;  // show_performance's last line in the log

    // Whether the script's line for `frame` is due (QUEST_SIM_ALIGN above).
    bool due(long frame) const {
        if (m_align < 0 || frame < m_align) {
            return frame <= m_frame;
        }
        return m_ready_frame >= 0 && m_ready_frame + (frame - m_align) <= m_frame;
    }

    float number(const std::string& name) const {
        const auto found = m_values.find(name);
        return found == m_values.end() ? 0.0f : found->second;
    }

    void apply(const Command& command) {
        if (command.name == "shot") {
            m_shot = command.value;
            sim_shot_hands = 1;
        } else if (command.name == "quit") {
            m_quit = true;
        } else {
            m_values[command.name] = std::stof(command.value);
        }
    }

    void make_target(int target, int width, int height) {
        m_width[target] = width;
        m_height[target] = height;
        m_gl.GenTextures(1, &m_texture[target]);
        m_gl.BindTexture(kTexture2D, m_texture[target]);
        m_gl.TexImage2D(kTexture2D, 0, static_cast<GLint>(kRgba8), width, height, 0, kRgba, kUnsignedByte, nullptr);
        m_gl.TexParameteri(kTexture2D, kMinFilter, static_cast<GLint>(kLinear));
        m_gl.TexParameteri(kTexture2D, kMagFilter, static_cast<GLint>(kLinear));
        m_gl.BindTexture(kTexture2D, 0);
        m_gl.GenFramebuffers(1, &m_fbo[target]);
        m_gl.BindFramebuffer(kFramebuffer, m_fbo[target]);
        m_gl.FramebufferTexture2D(kFramebuffer, kColorAttachment0, kTexture2D, m_texture[target], 0);
        m_gl.BindFramebuffer(kFramebuffer, 0);
    }

    // The left eye's motion as a picture (red right, green up, from grey; full at 0.05 of the picture a frame, a twentieth
    // of its half-width), and its numbers in the log: the mean movement across and up, how much of it moves, the most.
    void save_motion(const std::string& path, bool warped) {
        const int w = m_motion_width, h = m_motion_height;
        std::vector<float> motion(static_cast<std::size_t>(w) * h * 4);
        m_gl.BindFramebuffer(kFramebuffer, m_motion_fbo[0]);
        m_gl.ReadPixels(0, 0, w, h, kRgba, kFloat, motion.data());
        m_gl.BindFramebuffer(kFramebuffer, 0);
        std::vector<unsigned char> picture(static_cast<std::size_t>(w) * h * 4);
        double sum_x = 0, sum_y = 0, sum_z = 0, most = 0;
        long moving = 0;
        const auto byte = [](float v) { return static_cast<unsigned char>(std::clamp(128.0f + v * 2560.0f, 0.0f, 255.0f)); };
        for (std::size_t i = 0; i < static_cast<std::size_t>(w) * h; ++i) {
            const float x = motion[i * 4], y = motion[i * 4 + 1], z = motion[i * 4 + 2];
            sum_x += x;
            sum_y += y;
            sum_z += z;
            const double size = std::sqrt(static_cast<double>(x) * x + static_cast<double>(y) * y);
            most = std::max(most, size);
            moving += size > 0.002 ? 1 : 0;
            picture[i * 4] = byte(x);
            picture[i * 4 + 1] = byte(y);
            picture[i * 4 + 2] = byte(z * 10.0f);
            picture[i * 4 + 3] = 255;
        }
        write_png(path, picture, w, h, false);
        const double count = static_cast<double>(w) * h;
        char line[240];
        std::snprintf(line, sizeof line,
                      "[sim] motion %s: given %d, mean across %.4f up %.4f depth %.5f, moving %.0f%%, most %.4f (of NDC, a frame)",
                      m_shot.c_str(), warped ? 1 : 0, sum_x / count, sum_y / count, sum_z / count, 100.0 * moving / count, most);
        m_log(1, line);
    }

    void save(int target, const std::string& path, bool alpha) {
        std::vector<unsigned char> pixels(static_cast<std::size_t>(m_width[target]) * m_height[target] * 4);
        m_gl.BindFramebuffer(kFramebuffer, m_fbo[target]);
        m_gl.ReadPixels(0, 0, m_width[target], m_height[target], kRgba, kUnsignedByte, pixels.data());
        m_gl.BindFramebuffer(kFramebuffer, 0);
        if (m_cross.shown && (target == kTargetLeftEye || target == kTargetRightEye)) {
            stamp_crosshair(target == kTargetLeftEye ? 0 : 1, pixels, m_width[target], m_height[target]);
        }
        if (m_wrist.shown && m_wrist.alpha > 0.01f && (target == kTargetLeftEye || target == kTargetRightEye)) {
            stamp_wrist(target == kTargetLeftEye ? 0 : 1, pixels, m_width[target], m_height[target]);
        }
        if (m_hold.shown && m_hold.alpha > 0.01f && (target == kTargetLeftEye || target == kTargetRightEye)) {
            stamp_hold(target == kTargetLeftEye ? 0 : 1, pixels, m_width[target], m_height[target]);
        }
        write_png(path, pixels, m_width[target], m_height[target], alpha);
    }

    std::vector<unsigned char> read_target(int target) {
        std::vector<unsigned char> pixels(static_cast<std::size_t>(m_width[target]) * m_height[target] * 4);
        m_gl.BindFramebuffer(kFramebuffer, m_fbo[target]);
        m_gl.ReadPixels(0, 0, m_width[target], m_height[target], kRgba, kUnsignedByte, pixels.data());
        m_gl.BindFramebuffer(kFramebuffer, 0);
        return pixels;
    }

    // 0.1.9: each eye as the headset shows it (QUEST_SIM_HEADSET): the eye's picture with the crosshair, then the HUD
    // panel's quad over it as the runtime lays a quad layer, each of the eye's pixels taking the panel where its ray
    // meets the quad (bilinear), premultiplied when the panel uses its alpha (xr_openxr.cpp sets no unpremultiplied bit),
    // opaque when not.
    void save_headset(const std::string& base, const PanelLayer& panel) {
        std::vector<unsigned char> hud;
        if (panel.visible) {
            hud = read_target(kTargetPanel);
        }
        for (int eye = 0; eye < 2; ++eye) {
            const int target = eye == 0 ? kTargetLeftEye : kTargetRightEye;
            std::vector<unsigned char> pixels = read_target(target);
            if (m_cross.shown) {
                stamp_crosshair(eye, pixels, m_width[target], m_height[target]);
            }
            if (m_wrist.shown && m_wrist.alpha > 0.01f) {
                stamp_wrist(eye, pixels, m_width[target], m_height[target]);
            }
            if (m_hold.shown && m_hold.alpha > 0.01f) {
                stamp_hold(eye, pixels, m_width[target], m_height[target]);
            }
            if (panel.visible) {
                stamp_panel(eye, pixels, m_width[target], m_height[target], panel, hud);
            }
            write_png(base + (eye == 0 ? "-headset-left.png" : "-headset-right.png"), pixels, m_width[target],
                      m_height[target], false);
        }
    }

    void stamp_panel(int eye, std::vector<unsigned char>& pixels, int width, int height, const PanelLayer& panel,
                     const std::vector<unsigned char>& hud) {
        // the eye and field the picture was drawn with, as stamp_crosshair (0.1.9: the headset's own fields, each eye's
        // middle 100 pixels off the picture's, put the panel 200 pixels apart between the eyes in sims s09/s10 hud and
        // map: a double map in the pictures that the headset never shows)
        const Pose& e = m_drawn_set ? m_drawn_eye[eye] : m_frame_eye[eye];
        const float* fov = m_drawn_set ? m_drawn_fov[eye] : m_frame_fov[eye];
        Pose p = panel.pose;
        if (panel.follows_head) {
            p = {m_frame_head.position + rotate(m_frame_head.orientation, panel.pose.position),
                 m_frame_head.orientation * panel.pose.orientation};
        }
        const Vec3 normal = rotate(p.orientation, {0, 0, 1});
        const Vec3 right = rotate(p.orientation, {1, 0, 0});
        const Vec3 up = rotate(p.orientation, {0, 1, 0});
        const int pw = m_width[kTargetPanel], ph = m_height[kTargetPanel];
        const float tl = std::tan(fov[0]), tr = std::tan(fov[1]), tu = std::tan(fov[2]), td = std::tan(fov[3]);
        const auto texel = [&](int x, int y, int k) {
            x = std::clamp(x, 0, pw - 1);
            y = std::clamp(y, 0, ph - 1);
            return static_cast<float>(hud[(static_cast<std::size_t>(y) * pw + x) * 4 + k]);
        };
        for (int py = 0; py < height; ++py) {
            for (int px = 0; px < width; ++px) {
                const Vec3 local{tl + (static_cast<float>(px) + 0.5f) / static_cast<float>(width) * (tr - tl),
                                 td + (static_cast<float>(py) + 0.5f) / static_cast<float>(height) * (tu - td), -1.0f};
                const Vec3 d = rotate(e.orientation, local);
                const float denom = d.x * normal.x + d.y * normal.y + d.z * normal.z;
                if (std::fabs(denom) < 1e-6f) {
                    continue;
                }
                const Vec3 to = p.position - e.position;
                const float t = (to.x * normal.x + to.y * normal.y + to.z * normal.z) / denom;
                if (!(t > 0.0f)) {
                    continue;
                }
                const Vec3 rel = e.position + Vec3{d.x * t, d.y * t, d.z * t} - p.position;
                const float u = (rel.x * right.x + rel.y * right.y + rel.z * right.z) / panel.width + 0.5f;
                const float v = (rel.x * up.x + rel.y * up.y + rel.z * up.z) / panel.height + 0.5f;
                if (u < 0.0f || u >= 1.0f || v < 0.0f || v >= 1.0f) {
                    continue;
                }
                // the panel's rows as the eyes' (the first the bottom)
                const float fx = u * static_cast<float>(pw) - 0.5f, fy = v * static_cast<float>(ph) - 0.5f;
                const int x0 = static_cast<int>(std::floor(fx)), y0 = static_cast<int>(std::floor(fy));
                const float ax = fx - static_cast<float>(x0), ay = fy - static_cast<float>(y0);
                float s[4];
                for (int k = 0; k < 4; ++k) {
                    s[k] = (texel(x0, y0, k) * (1 - ax) + texel(x0 + 1, y0, k) * ax) * (1 - ay) +
                           (texel(x0, y0 + 1, k) * (1 - ax) + texel(x0 + 1, y0 + 1, k) * ax) * ay;
                }
                unsigned char* out = pixels.data() + (static_cast<std::size_t>(py) * width + px) * 4;
                const float a = panel.blend ? s[3] / 255.0f : 1.0f;
                for (int k = 0; k < 3; ++k) {
                    out[k] = static_cast<unsigned char>(std::clamp(s[k] + out[k] * (1.0f - a), 0.0f, 255.0f));
                }
            }
        }
    }

    void (*m_log)(int, const char*);
    Gl m_gl{};
    GLuint m_texture[kTargetCount]{};
    GLuint m_fbo[kTargetCount]{};
    int m_width[kTargetCount]{};
    int m_height[kTargetCount]{};
    std::vector<Command> m_script;
    std::size_t m_next = 0;
    std::map<std::string, float> m_values;
    std::string m_out = ".";
    std::string m_shot;
    long m_frame = 0;
    long m_align = -1;        // QUEST_SIM_ALIGN, -1 without
    long m_missed = 0;        // the script's "late" frames so far (displays_missed)
    std::int64_t m_display_ns = 0;  // 0.1.3: each frame's display time (a "late" frame two periods on: motion_dt)
    std::chrono::steady_clock::time_point m_work_start = std::chrono::steady_clock::now();  // 0.1.3: begin_frame's end
    double m_last_work = 0;   // 0.1.3: the last frame's work, wall time (last_work_seconds: dynamic resolution)
    float m_grip_tilt = 0;    // QUEST_SIM_GRIP_TILT, radians
    double m_hz = 72.0;       // QUEST_SIM_HZ, the refresh rate told
    int m_period_blips = 0;   // QUEST_SIM_PERIOD_BLIPS (0.1.5)
    // 0.1.3 the gun's crosshair (show_crosshair) and the views the pictures were drawn with (stamp_crosshair)
    CrosshairLayer m_cross;
    long m_cross_logged = -1000;
    WristLayer m_wrist;  // LIST 49 (show_wrist, stamp_wrist)
    long m_wrist_logged = -1000;
    HoldLayer m_hold;  // 0.1.7 the break-free bar (show_hold, stamp_hold)
    long m_hold_logged = -1000;
    Pose m_frame_eye[2], m_drawn_eye[2];
    Pose m_frame_head;  // 0.1.9 (stamp_panel)
    float m_frame_fov[2][4]{}, m_drawn_fov[2][4]{};
    bool m_drawn_set = false;
    std::chrono::steady_clock::time_point m_frame_asked{};  // when begin_frame last ran
    long m_ready_frame = -1;  // the frame the set-up was done
    bool m_pointer_dot = false, m_pointer_beam = false, m_pointer_dot2 = false, m_pointer_beam2 = false;
    bool m_pointer_big = false;  // session 12: the dot grown over a click zone
    bool m_quit = false;
    // Smoother frames.
    bool m_space_warp = false;
    int m_motion_width = 0, m_motion_height = 0;
    GLuint m_motion[2]{}, m_motion_depth[2]{}, m_motion_fbo[2]{};
    SpaceWarpFrame m_warp;
    bool m_warp_set = false;
    long m_warp_frames = 0, m_warp_asked = 0, m_warp_skipped = 0, m_warp_kept = 0;
};

}  // namespace

#if defined(_WIN32)
Headset* make_openxr_headset(void (*log)(int, const char*));
#endif

// A test (QUEST_SIM_SCRIPT, or QUEST_SIM=1 to stand still) gets this headset; on Windows a player's game gets
// the real one through OpenXR (xr_openxr.cpp, the PC release's).
Headset* make_headset(void (*log)(int, const char*)) {
#if defined(_WIN32)
    if (std::getenv("QUEST_SIM_SCRIPT") == nullptr && std::getenv("QUEST_SIM") == nullptr) {
        return make_openxr_headset(log);
    }
#endif
    return new Sim(log);
}

}  // namespace quest

#endif  // !__ANDROID__
