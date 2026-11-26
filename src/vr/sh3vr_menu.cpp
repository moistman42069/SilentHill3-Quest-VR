// SPDX-License-Identifier: GPL-3.0-or-later
// SH3 Quest VR: the in-game VR menu (LIST 17). Every VR option (sh3vr.cpp's rows, the same keys the launcher writes)
// as a row on six pages, opened by holding the left menu button a second in any of the game's menus (the pause menu,
// the inventory, the title) and closed by B, a short press of the menu button, or holding it again. Either stick
// moves and changes; A or the trigger changes; the pointer picks pages and rows (its left half of a value steps it
// down, its right half up) with a haptic tick for each row crossed. Each change is saved to sh3vr.ini at once; the
// few that need the headset started again say so.
//
// Drawn on the CPU with the launcher's font (launcher.c ui_text) into a picture the panel's size, uploaded when it
// changed and shown on the panel in place of the game's 2D while the menu is up. The game is paused under it (it is
// opened from the game's menus) and gets none of the controllers meanwhile.

#include "sh3vr_internal.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <string>
#include <vector>

namespace sh3q {

void request_recentre();

namespace {

constexpr int kW = 1024, kH = 768;
constexpr int kTabsY = 128, kRowsY = 168, kRowH = 46, kShown = 11;

std::vector<uint32_t> canvas;
GLuint tex = 0;
bool dirty = true;

int page = 0;
int sel = 0;                  // the row on the page; -1 the page tabs
int scroll = 0;
int hover_row = -2, hover_tab = -1;
bool was_press = false, was_a = false, was_b = false;
float stick_hold = 0, stick_x_hold = 0;
int stick_dir = 0, stick_x_dir = 0;
std::string note;             // a line at the foot (what changed)

// The rows of a page (and the action rows the View page starts with).
struct Item {
    const OptionRow* row;     // null: an action
    const char* action;
};
std::vector<Item> items;

void build_items() {
    items.clear();
    int n = 0, np = 0;
    const OptionRow* rows = option_rows(n);
    const char* const* pages = option_pages(np);
    if (page == 1) items.push_back({nullptr, "Recentre the view now"});
    for (int i = 0; i < n; i++)
        if (!std::strcmp(rows[i].page, pages[page])) items.push_back({&rows[i], nullptr});
    sel = std::clamp(sel, -1, (int)items.size() - 1);
}

constexpr uint32_t rgba(int r, int g, int b, int a = 255) {
    return (uint32_t)a << 24 | (uint32_t)b << 16 | (uint32_t)g << 8 | (uint32_t)r;
}
const uint32_t kBack = rgba(10, 8, 7, 235);
const uint32_t kRust = rgba(128, 38, 24);
const uint32_t kRustDark = rgba(58, 18, 12, 240);
const uint32_t kText = rgba(214, 205, 190);
const uint32_t kDim = rgba(128, 120, 110);
const uint32_t kBright = rgba(244, 236, 220);

void fill(int x, int y, int w, int h, uint32_t c) {
    for (int yy = std::max(0, y); yy < std::min(kH, y + h); yy++)
        for (int xx = std::max(0, x); xx < std::min(kW, x + w); xx++) canvas[(size_t)yy * kW + xx] = c;
}
int text_w(int size, const char* s) { return (int)std::strlen(s) * (int)(36 * size / 80.0f + 0.5f); }
void text(int x, int y, int size, uint32_t c, const char* s) { ui_text(canvas.data(), kW, kH, x, y, size, c, s); }

void draw() {
    canvas.assign((size_t)kW * kH, kBack);
    // The frame: rust, like the game's own menus' borders.
    fill(0, 0, kW, 4, kRust);
    fill(0, kH - 4, kW, 4, kRust);
    fill(0, 0, 4, kH, kRust);
    fill(kW - 4, 0, 4, kH, kRust);
    text(48, 78, 46, kBright, "VR options");
    text(kW - 48 - text_w(20, "Silent Hill 3 Quest VR"), 72, 20, kDim, "Silent Hill 3 Quest VR");
    // Pages
    int np = 0;
    const char* const* pages = option_pages(np);
    int x = 48;
    for (int p = 0; p < np; p++) {
        const int w = text_w(22, pages[p]);
        const bool on = p == page, hov = p == hover_tab || (sel == -1 && on);
        if (on) fill(x - 6, kTabsY + 8, w + 12, 3, kRust);
        if (hov && sel == -1) fill(x - 8, kTabsY - 26, w + 16, 40, kRustDark);
        text(x, kTabsY, 22, on ? kBright : hov ? kText : kDim, pages[p]);
        x += w + 30;
    }
    // Rows
    if (sel >= 0) {
        if (sel < scroll) scroll = sel;
        if (sel >= scroll + kShown) scroll = sel - kShown + 1;
    }
    scroll = std::clamp(scroll, 0, std::max(0, (int)items.size() - kShown));
    bool any_next_start = false;
    for (int i = scroll; i < (int)items.size() && i < scroll + kShown; i++) {
        const int y = kRowsY + (i - scroll) * kRowH;
        const bool on = i == sel;
        if (on) {
            fill(40, y, kW - 80, kRowH - 4, kRustDark);
            fill(40, y, 5, kRowH - 4, kRust);
        }
        const Item& it = items[i];
        if (!it.row) {
            text(64, y + 32, 26, on ? kBright : kText, it.action);
            continue;
        }
        std::string name = it.row->name;
        if (it.row->next_start) {
            name += " *";
            any_next_start = true;
        }
        text(64, y + 32, 26, on ? kBright : kText, name.c_str());
        const std::string val = option_value_text(*it.row);
        const bool is_switch = it.row->kind == Kind::kSwitch;
        const uint32_t vc = is_switch ? (*(bool*)it.row->value ? kBright : kDim) : (on ? kBright : kText);
        const int vw = text_w(24, val.c_str());
        const int vx = kW - 96 - vw;
        text(vx, y + 31, 24, vc, val.c_str());
        if (!is_switch && on) {
            text(vx - 40, y + 31, 24, kRust, "<");
            text(kW - 80, y + 31, 24, kRust, ">");
        }
    }
    if (scroll > 0) text(kW / 2 - 10, kRowsY - 4, 20, kDim, "^");
    if (scroll + kShown < (int)items.size()) text(kW / 2 - 10, kRowsY + kShown * kRowH + 8, 20, kDim, "v");
    // The foot
    const char* hint = "Stick or point to choose - A or trigger to change - B to close (saved at once)";
    text(48, kH - 58, 19, kDim, hint);
    if (any_next_start) text(48, kH - 30, 19, kDim, "* takes effect the next time the game starts");
    if (!note.empty()) text(kW - 48 - text_w(19, note.c_str()), kH - 30, 19, kText, note.c_str());
    dirty = false;
}

void change(int dir) {
    if (sel < 0 || sel >= (int)items.size()) return;
    const Item& it = items[sel];
    if (!it.row) {
        request_recentre();
        note = "View recentred";
        dirty = true;
        return;
    }
    option_step(*it.row, dir);
    const bool saved = save_options();
    note = std::string(it.row->name) + ": " + option_value_text(*it.row) + (saved ? "" : " (not saved!)");
    rt_log("VR menu: %s = %s", it.row->key, option_value_text(*it.row).c_str());
    buzz(point_hand, 0.2f, 0.02f, 200.0f);
    dirty = true;
}

void set_page(int p) {
    int np = 0;
    option_pages(np);
    page = (p + np) % np;
    scroll = 0;
    build_items();
    dirty = true;
}

// A stick's push as steps: one on the push, then repeating while held.
int stick_steps(float v, float& held, int& dir) {
    const int d = v > 0.6f ? 1 : v < -0.6f ? -1 : 0;
    if (d == 0 || d != dir) {
        held = 0;
        dir = d;
        return d;
    }
    held += frame_dt();
    if (held > 0.4f) {
        held -= 0.16f;
        return d;
    }
    return 0;
}

}  // namespace

void menu_open(const char* why) {
    if (vr_menu_open) return;
    vr_menu_open = true;
    sel = 0;
    note.clear();
    build_items();
    dirty = true;
    // session 16 (owner 80): its sound and a tick in the left hand (the menu button's), opening and closing
    hand_sound(kSoundMenu, world_point(hands[kLeft].grip.position), 1);
    buzz(kLeft, 0.4f, 0.05f);
    rt_log("VR: the VR menu opened (%s)", why);
}

void menu_close() {
    if (!vr_menu_open) return;
    save_options();
    vr_menu_open = false;
    hand_sound(kSoundMenu, world_point(hands[kLeft].grip.position), 0);
    buzz(kLeft, 0.3f, 0.04f);
    rt_log("VR: the VR menu closed (options saved)");
}

void menu_update(const Controls& c, bool point_valid, float u, float v, bool press) {
    if (items.empty()) build_items();
    // Sticks: either one, the further pushed.
    const float sy = std::fabs(c.left_y) > std::fabs(c.right_y) ? c.left_y : c.right_y;
    const float sx = std::fabs(c.left_x) > std::fabs(c.right_x) ? c.left_x : c.right_x;
    const int dy = stick_steps(sy, stick_hold, stick_dir);
    if (dy) {
        sel = std::clamp(sel - dy, -1, (int)items.size() - 1);  // up (positive) moves to the row above
        dirty = true;
    }
    const int dx = stick_steps(sx, stick_x_hold, stick_x_dir);
    if (dx) {
        if (sel == -1) set_page(page + dx);
        else change(dx);
    }
    if (c.a && !was_a) {
        if (sel == -1) set_page(page + 1);
        else change(1);
    }
    if ((c.b || c.y) && !was_b) menu_close();
    was_a = c.a;
    was_b = c.b || c.y;

    // The pointer
    int new_hover_row = -2, new_hover_tab = -1;
    if (point_valid) {
        const float px = u * kW, py = v * kH;
        if (py > kTabsY - 30 && py < kTabsY + 16) {
            int np = 0;
            const char* const* pages = option_pages(np);
            int x = 48;
            for (int p = 0; p < np; p++) {
                const int w = text_w(22, pages[p]);
                if (px >= x - 12 && px < x + w + 18) new_hover_tab = p;
                x += w + 30;
            }
        } else if (py >= kRowsY && py < kRowsY + kShown * kRowH) {
            const int r = scroll + (int)((py - kRowsY) / kRowH);
            if (r >= 0 && r < (int)items.size()) new_hover_row = r;
        }
        if (new_hover_row != hover_row && new_hover_row >= 0) {
            sel = new_hover_row;
            buzz(point_hand, 0.12f, 0.012f, 220.0f);
            dirty = true;
        }
        if (new_hover_tab != hover_tab) {
            if (new_hover_tab >= 0) buzz(point_hand, 0.12f, 0.012f, 220.0f);
            dirty = true;
        }
        if (press && !was_press) {
            if (new_hover_tab >= 0) {
                set_page(new_hover_tab);
                sel = -1;
            } else if (new_hover_row >= 0) {
                sel = new_hover_row;
                const Item& it = items[sel];
                const bool down = it.row && it.row->kind != Kind::kSwitch && px < kW - 96 - 160;
                change(down ? -1 : 1);
            }
        }
        // Scroll by pointing at the arrows' strips above and below the rows.
        if (py >= kRowsY - 24 && py < kRowsY && scroll > 0 && press && !was_press) scroll--, dirty = true;
        if (py >= kRowsY + kShown * kRowH && py < kRowsY + kShown * kRowH + 28 && press && !was_press)
            scroll++, dirty = true;
    }
    hover_row = new_hover_row;
    hover_tab = new_hover_tab;
    was_press = press;
}

unsigned menu_texture() {
    if (!vr_menu_open) return 0;
    if (!tex) {
        glGenTextures(1, &tex);
        GLint prev = 0;
        glGetIntegerv(GL_TEXTURE_BINDING_2D, &prev);
        glBindTexture(GL_TEXTURE_2D, tex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, kW, kH, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glBindTexture(GL_TEXTURE_2D, (GLuint)prev);
        dirty = true;
    }
    if (dirty) {
        draw();
        GLint prev = 0, unpack = 0, row_len = 0, align = 4;
        glGetIntegerv(GL_TEXTURE_BINDING_2D, &prev);
        glGetIntegerv(GL_PIXEL_UNPACK_BUFFER_BINDING, &unpack);
        glGetIntegerv(GL_UNPACK_ROW_LENGTH, &row_len);
        glGetIntegerv(GL_UNPACK_ALIGNMENT, &align);
        glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
        glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
        glBindTexture(GL_TEXTURE_2D, tex);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, kW, kH, GL_RGBA, GL_UNSIGNED_BYTE, canvas.data());
        glBindTexture(GL_TEXTURE_2D, (GLuint)prev);
        glPixelStorei(GL_UNPACK_ROW_LENGTH, row_len);
        glPixelStorei(GL_UNPACK_ALIGNMENT, align);
        glBindBuffer(GL_PIXEL_UNPACK_BUFFER, (GLuint)unpack);
    }
    return tex;
}

}  // namespace sh3q
