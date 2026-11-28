/* Hand-written replacements of sh3.exe functions, by address: tools/regen.sh passes this file to the
 * recompiler (--exclude-manual), which then leaves these functions out of src/recomp/gen. */
#include "../runtime/rt.h"
#include "../runtime/host.h"
#include "../vr/sh3vr.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* The static CRT's memcpy (memmove semantics) and its second copy: their jump tables sit past the end
 * the disassembler finds and are indexed backwards, so the lifted copies cannot follow them. */
static void crt_memmove(void) {
    uint32_t d = ARG(0), s = ARG(1), n = ARG(2);
    memmove((void *)XBOX_PTR(d), (const void *)XBOX_PTR(s), n);
    g_eax = d;
    g_esp += 4;  /* ret: cdecl */
}
void sub_00661D20(void) { crt_memmove(); }
void sub_0066F9C0(void) { crt_memmove(); }

/* ---- widescreen (ThirteenAG's SilentHill3.WidescreenFix, ported) ----
 * The display mode is always the screen's (resolution= in sh3.ini): the device is created and reset with it
 * whatever disp.ini or the options ask for. 0x72C780/84 is the mode the game wants, 0x72C788/8C the one it
 * lays the 2D out in; both feed D3DPRESENT_PARAMETERS at 0x72C664. */
extern uint32_t rt_res_w, rt_res_h;
static void force_mode(void) {
    MEM32(0x72C780) = MEM32(0x72C788) = rt_res_w;
    MEM32(0x72C784) = MEM32(0x72C78C) = rt_res_h;
}
extern void sub_004193A0_gen(void);
void sub_004193A0(void) { force_mode(); sub_004193A0_gen(); }  /* creates the device */
extern void sub_00419760_gen(void);
void sub_00419760(void) { force_mode(); sub_00419760_gen(); }  /* resets it after a mode change */
extern void sub_00416AE0_gen(void);
void sub_00416AE0(void) { ARG(0) = rt_res_w; ARG(1) = rt_res_h; sub_00416AE0_gen(); }  /* set mode (w, h) */
extern void sub_00416B70_gen(void);
void sub_00416B70(void) { ARG(0) = rt_res_w; ARG(1) = rt_res_h; sub_00416B70_gen(); }  /* set 2D size (w, h) */

/* The camera's projection scales, returned in ST(0): horizontal (0x43AB80) and vertical (0x43ABC0). Gameplay
 * widens the view (hor+); a cutscene (0x81FAB5) keeps its 4:3 width and loses some height instead. The
 * picked-up item and the mouse cursor (calls returning to 0x599C29, 0x4DDB98) keep the 4:3 width. */
#define ST0 g_fp_stack[g_fp_top & 7]
/* SH3 Quest VR: in VR the 2D goes on the 4:3 panel, so the 2D is laid out as on a 4:3 screen (the game's own layout,
 * stretched over each eye's picture and back to 4:3 on the panel), and the frame camera's projection is VR's. */
static double ws_aspect(void) { return sh3vr_flat_2d() ? 4.0 / 3.0 : (double)rt_res_w / rt_res_h; }
static double ws_fov(void) { return (4.0 / 3.0) / ws_aspect(); }  /* 1 at 4:3 */
static double ws_offset(void) { return sh3vr_flat_2d() ? 0.0 : rt_res_w - rt_res_h * 4.0 / 3.0; }
extern void sub_0043AB80_gen(void);
void sub_0043AB80(void) {
    uint32_t from = MEM32(g_esp);
    sub_0043AB80_gen();
    if (sh3vr_active()) {  /* the frame camera's set-up (0x6853C0) only: the item view and the cursor stay 4:3 */
        if (from == 0x685435u) ST0 = (float)sh3vr_proj_scale(0, ST0);
        return;
    }
    if (!MEM8(0x81FAB5) && from != 0x599C29u && from != 0x4DDB98u) ST0 = (float)(ST0 * ws_fov());
}
extern void sub_0043ABC0_gen(void);
void sub_0043ABC0(void) {
    uint32_t from = MEM32(g_esp);
    sub_0043ABC0_gen();
    if (sh3vr_active()) {
        if (from == 0x68541Du) ST0 = (float)sh3vr_proj_scale(1, ST0);
        return;
    }
    if (MEM8(0x81FAB5)) ST0 = (float)(ST0 / ws_fov());
}

/* ---- SH3 Quest VR: the camera (docs/GAME_FACTS.md, Camera) ----
 * 0x4162A0 SetViewMatrix(const float m[16]) is the one way into the camera block's view: called from 0x43AD30 (the
 * return address 0x43AD3B) with the frame camera's view, the head's view is given in its place, so the whole frame
 * (and its culling) is drawn from the head. The game's own view is kept for VR's third person and cutscenes. */
#define VR_SCRATCH 0x0EC00100u  /* in game_init's committed page: the view handed to the game */
static float game_cam_view[16];
static int game_cam_valid;
static int vr_view_at = -100;  /* rt_frame when VR_SCRATCH last took the eyes' view */
extern void sub_004162A0_gen(void);
void sub_004162A0(void) {
    uint32_t from = MEM32(g_esp);
    if (sh3vr_active() && from == 0x43AD3Bu) {
        float m[16];
        memcpy(m, GPTR(ARG(0)), sizeof m);
        memcpy(game_cam_view, m, sizeof m);
        game_cam_valid = 1;
        sh3vr_view(m, 1);
        memcpy(GPTR(VR_SCRATCH), m, sizeof m);
        vr_view_at = rt_frame;  /* the eyes' view of this frame: spatial sound's listener (below) */
        ARG(0) = VR_SCRATCH;  /* cdecl: the argument slot is the callee's; 0x43AD30 keeps its own copy in esi */
    }
    sub_004162A0_gen();
}
/* 0.1.5: the game's own view of this frame for its effects (sh3vr_effect_map: it projects them with its camera, not the
 * head's), kept apart from game_cam_valid, which the frame's end clears. */
int sh3_game_view(float out[16]) {
    if (vr_view_at < 0 || rt_frame - vr_view_at > 1) return 0;
    memcpy(out, game_cam_view, sizeof game_cam_view);
    return 1;
}
/* 0.1.6 (owner messages 115-118, a player on 0.1.5: "Enemies only become visible when they're right on top of you"):
 * whether a model is drawn is decided by 0x43A0F0 (the model draw 0x454670's only test; docs/GAME_FACTS.md, Camera)
 * against the culling camera 0x439FD0 sets up from the game's OWN camera: 0x43AD30 keeps the game's view at 0x743C34
 * beside the head's it hands to SetViewMatrix, and the camera's place comes from 0x471800(). In first person the game's
 * camera is often a fixed one framing Heather from ahead or aside, so a monster ahead in the head's view was left out
 * until it came near her, into that camera's picture. The PC VR mod has the game's camera itself at her head (the
 * Camera Mod) with a wide guard band; here a model the game's camera leaves out is tested again from the head: drawn
 * when its bounding sphere is within 80 degrees of the head's gaze (both eyes' view, with room for a quick turn) and
 * within the game's far distance. One the game hides by its list (0x742A64 pairs at 0x742A68, 0x439AA0) stays hidden. */
extern void sub_0043A0F0_gen(void);
static unsigned head_cull_drawn, head_cull_logged;
static int head_cull_told = -100000;
/* testing (0.1.6, the PC rig): SH3_CULL_TEST=1 tests every model again from the head as if the game's camera had left
 * each out, so the head's test is seen whole: a model behind the head or past the far distance is not drawn, one in
 * view is (the counts in the 1200-frame line). */
static int head_cull_test = -1;
static unsigned head_cull_seen, head_cull_hidden;
void sub_0043A0F0(void) {
    sub_0043A0F0_gen();
    if (head_cull_test < 0) head_cull_test = getenv("SH3_CULL_TEST") && atoi(getenv("SH3_CULL_TEST")) ? 1 : 0;
    int game_said = -1;  /* testing: the game's own answer, kept for a frame drawn from its camera */
    if (head_cull_test && sh3vr_active() && vr_view_at >= 0 && rt_frame - vr_view_at <= 1) {
        game_said = g_eax ? 1 : 0;
        g_eax = 0;
    }
    if (g_eax || !sh3vr_active() || vr_view_at < 0 || rt_frame - vr_view_at > 1) return;
    const float *h = (const float *)GPTR(VR_SCRATCH);
    if (!memcmp(h, game_cam_view, sizeof game_cam_view)) {  /* the frame drawn from the game's camera itself */
        if (game_said >= 0) g_eax = (uint32_t)game_said;
        return;
    }
    if (game_said >= 0) {
        head_cull_seen++;
        if (!game_said) head_cull_hidden++;  /* left out by the game's camera itself */
    }
    const uint32_t obj = MEM32(0x742A50);  /* the model 0x43A0F0 tested (0x44ACD0's, [0x7F7894]) */
    if (!obj) return;
    const int32_t listed = (int32_t)MEM32(0x742A64);
    for (int32_t i = 0; i < listed && i < 1024; i++)
        if (MEM16(0x742A68u + 4u * (uint32_t)i) == MEM16(obj + 0x80) && MEM16(0x742A6Au + 4u * (uint32_t)i) == MEM16(obj + 0x82))
            return;
    const float r = MEMF(0x742A30), far_d = MEMF(0x743BD0);  /* its bounding radius (0x439810); the far distance */
    const float px = MEMF(0x7F7884), py = MEMF(0x7F7888), pz = MEMF(0x7F788C);  /* its place (0x44AD20's) */
    /* the head's place and gaze from its view (D3D row vectors, world = (view - t) M^-1; M a rotation, maybe scaled) */
    const float s2 = h[0] * h[0] + h[1] * h[1] + h[2] * h[2];
    if (!(s2 > 1e-12f)) return;
    const float cx = -(h[12] * h[0] + h[13] * h[1] + h[14] * h[2]) / s2;
    const float cy = -(h[12] * h[4] + h[13] * h[5] + h[14] * h[6]) / s2;
    const float cz = -(h[12] * h[8] + h[13] * h[9] + h[14] * h[10]) / s2;
    float fx = h[2], fy = h[6], fz = h[10];
    const float fl = sqrtf(fx * fx + fy * fy + fz * fz);
    if (!(fl > 1e-6f)) return;
    fx /= fl, fy /= fl, fz /= fl;
    const float dx = px - cx, dy = py - cy, dz = pz - cz;
    const float dist = sqrtf(dx * dx + dy * dy + dz * dz);
    float off = 0.0f;
    if (dist > r) {
        if (far_d > 0.0f && dist - r > far_d) return;
        float c = (dx * fx + dy * fy + dz * fz) / dist;
        c = c > 1.0f ? 1.0f : c < -1.0f ? -1.0f : c;
        off = (acosf(c) - asinf(r / dist)) * 57.2958f;
        if (off > 80.0f) return;
    }
    g_eax = 1;
    head_cull_drawn++;
    if (head_cull_logged < 3) {
        head_cull_logged++;
        rt_log("VR: frame %d: a model the game's camera leaves out (%04X %04X, %.1f m from the head, %.0f degrees off "
               "the gaze, radius %.2f m) drawn: the head sees it", rt_frame, MEM16(obj + 0x80), MEM16(obj + 0x82),
               dist / 490.0f, off < 0.0f ? 0.0f : off, r / 490.0f);
    }
    if (rt_frame - head_cull_told >= (head_cull_test ? 120 : 1200)) {
        if (head_cull_test)
            rt_log("VR: frame %d: SH3_CULL_TEST: %u model tests, %u drawn from the head's view (the game's camera had "
                   "left out %u)", rt_frame, head_cull_seen, head_cull_drawn, head_cull_hidden);
        else
            rt_log("VR: frame %d: %u model draws the game's camera left out were drawn from the head's view (since the "
                   "last line)", rt_frame, head_cull_drawn);
        head_cull_drawn = head_cull_seen = head_cull_hidden = 0;
        head_cull_told = rt_frame;
    }
}

/* 0x67B9B0 rebuilds the camera block (view, projection, their products) when it is dirty: the projection the frame
 * camera's view is drawn with is taken here, for the eyes' corrections. */
extern void sub_0067B9B0_gen(void);
void sub_0067B9B0(void) {
    sub_0067B9B0_gen();
    if (sh3vr_active()) sh3vr_camera_built((const float *)GPTR(0x72C3A0), (const float *)GPTR(0x72C360));
}

/* The event dispatch (0x4B9E70) returns the next game mode: 5 is the playable update. */
static int playable_frame;
/* session 11: whether this frame's update was play so far (d3d8.c vr_route: out of play, the game's full screens) */
int sh3_playable_now(void) { return playable_frame; }
static uint32_t last_game_mode = 0xFFFFFFFFu, frame_no;
/* 0.1.8 (owner 152): the dispatch's own step after its call. 0x4B9E70 pushes a level on the game's step stack
 * (0x5F1B50), keeps its step there (0x5F1B70 sets it, 0x5F1BC0 reads it) and pops the level as it returns, so its step
 * stays at the depth + 1: 0 nothing running (free play: the stick and the button are the player's), 2 and 5 an
 * event's script (a door, an examine's message, a scene played in the room), 3 a floor item's pick-up (0x4B8040),
 * 6 her death's wait, 8 0x4B8540's. The game returns mode 5 (the playable update) through its scripts and pick-ups as
 * well (0x4BA28A: mode 2 only when 0xBA974E asks for it), so mode 5 alone never told free play from an event. */
static uint32_t event_state;
/* 0.1.9 (owner 168): the level of the game's step stack the dispatch returns to, its caller's (the main loop
 * 0x58EDB0's): its step there is the game's mode, set to the mode the dispatch returns (0x58F12C); -1 before play.
 * event_mode: the mode an event's script handed over to (the dispatch returned it with its step 2 or 5 before the
 * call: a puzzle's screen; the save point's save screen 19, vr473: "event step 2 -> 0, mode 19"); -1 once it hands
 * back play (5) or a room's load (2). */
static int main_level = -1, event_mode = -1;
static void pickup_watch(void);
static int event_trigger(int ev);
int sh3_event_is_door(int ev);
/* 0.1.8 (owner 152): 1 while the dispatch runs: as it returns it calls 0x599EA0, which draws the item an event gives
 * (0x599B70, a projection of its own); d3d8.c puts the 3D drawn then on the panel with its message */
static int in_dispatch;
int sh3_in_dispatch(void) { return in_dispatch; }
extern void sub_004B9E70_gen(void);
void sub_004B9E70(void) {
    in_dispatch = 1;
    sub_004B9E70_gen();
    in_dispatch = 0;
    last_game_mode = g_eax;
    if (g_eax == 5) playable_frame = 1;
    const uint32_t depth = MEM32(0xBCC658u), was = event_state;
    event_state = depth < 15u ? MEM32(0xBCC65Cu + 4u * (depth + 1u)) : 0u;
    if (depth < 15u) main_level = (int)depth;  /* 0.1.9: the main loop's own level (its step: the game's mode) */
    if (g_eax == 5u || g_eax == 2u) event_mode = -1;
    else if (was == 2u || was == 5u) event_mode = (int)g_eax;  /* 0.1.9: a screen an event's script opened */
    if (event_state != was) {
        static int told;
        if (told++ < 600)
            rt_log("VR: frame %d: the game's event step %u -> %u (event %d, mode %u; her action %u, movement %u)%s",
                   rt_frame, was, event_state, (int)MEM32(0xBA9768u), (unsigned)g_eax, MEM32(0x712C18Cu),
                   MEM32(0x712C190u), told == 600 ? "; no more of these lines" : "");
    }
    /* 0.1.10 (owner 170, doors by hand): a room's load after a button's event the doors' test does not count as a
     * door (sh3_event_is_door: such a door opens as a closing hand meets it, as before), told the first 20 times */
    if (g_eax == 2u) {
        const int ev = (int)MEM32(0xBA9768u);
        static int told;
        if (ev >= 0 && ev < 0x400 && event_trigger(ev) == 1 && !sh3_event_is_door(ev) && told++ < 20)
            rt_log("VR: frame %d: a room's load after event %d (the button's), which the doors' test does not count "
                   "as a door", rt_frame, ev);
    }
    pickup_watch();
}

/* Heather's work struct (0x712C100 points at it; +0x80 == 0x0100 is her). */
static uint32_t vr_player(void) {
    uint32_t p = MEM32(0x712C100);
    return p && MEM16(p + 0x80) == 0x0100 ? p : 0;
}

/* A skinned model's bones (0x45EC00, called by the model draw 0x45F5D0 before its parts upload them; docs/GAME_FACTS.md,
 * Bones): (model, skeleton, world matrix, flag), cdecl. It fills [0x6B4540] with [model+0xC] world matrices of 64
 * bytes; 0x45EC90 then makes each part's blended ones at +0x2000 from them. LIST 19 (full-body IK) changes Heather's arm
 * bones here, after the game's pose and before anything reads them. SH3_VSC_DUMP=N logs each model's bones in frame N. */
/* 0.1.9 (owner 164): a bone (rows 0-2 its axes, either way round: the determinant is the same) whose axes are turned
 * over, as a mirror's reflection of her has them; and the mirror's draw of her this frame, given her VR pose. */
static int rows_turned_over(const float *m) {
    const float d = m[0] * (m[5] * m[10] - m[6] * m[9]) - m[1] * (m[4] * m[10] - m[6] * m[8]) +
                    m[2] * (m[4] * m[9] - m[5] * m[8]);
    return d < -0.25f;
}
static uint32_t mirror_model;
static int mirror_frame = -100;
/* 0.1.9 (owner 166): her model as a scene draws it away from her place this frame (its draw cast her shadow in a scene
 * that has the eyes in her head) */
static uint32_t scene_model;
static int scene_model_frame = -100;
extern void sub_0045EC00_gen(void);
void sub_0045EC00(void) {
    const uint32_t model = ARG(0), skel = ARG(1), world = ARG(2), flag = ARG(3);
    sub_0045EC00_gen();
    {
        /* testing (LIST 24, session 5): SH3_WEAPON_BONES=1 logs each small model (1-4 bones: the weapons) drawn while a
         * weapon is out, once a model: its header, parent bytes, rest matrices and the bones as the game posed them, so
         * the weapons' parts can be put together as the game holds them. */
        static int want = -1, n_seen;
        static uint32_t seen[48];
        if (want < 0) want = getenv("SH3_WEAPON_BONES") ? 1 : 0;
        const uint32_t nb = MEM32(model + 0xC);
        if (want && nb >= 1 && nb <= 4 && MEM8(0x712C656) && n_seen < 48) {
            int k = 0;
            while (k < n_seen && seen[k] != model) k++;
            if (k == n_seen) {
                seen[n_seen++] = model;
                const uint32_t rest = model + MEM32(model + 8), par = model + MEM32(model + 0x10), base = MEM32(0x6B4540);
                rt_log("weapon model: frame %d, weapon %u, model %08X, %u bones, header %08X %08X %08X %08X %08X %08X "
                       "%08X %08X %08X %08X, parents %d %d %d %d", rt_frame, MEM8(0x712C656), model, nb, MEM32(model),
                       MEM32(model + 4), MEM32(model + 8), MEM32(model + 12), MEM32(model + 16), MEM32(model + 20),
                       MEM32(model + 24), MEM32(model + 28), MEM32(model + 32), MEM32(model + 36), (int8_t)MEM8(par),
                       nb > 1 ? (int8_t)MEM8(par + 1) : 0, nb > 2 ? (int8_t)MEM8(par + 2) : 0,
                       nb > 3 ? (int8_t)MEM8(par + 3) : 0);
                for (uint32_t b = 0; b < nb; b++) {
                    const float *r = (const float *)GPTR(rest + 64 * b), *m = (const float *)GPTR(base + 64 * b);
                    rt_log("weapon model:  bone %u rest %.4f %.4f %.4f %.4f | %.4f %.4f %.4f %.4f | %.4f %.4f %.4f %.4f | "
                           "%.2f %.2f %.2f %.4f", b, r[0], r[1], r[2], r[3], r[4], r[5], r[6], r[7], r[8], r[9], r[10],
                           r[11], r[12], r[13], r[14], r[15]);
                    rt_log("weapon model:  bone %u posed %.4f %.4f %.4f | %.4f %.4f %.4f | %.4f %.4f %.4f | %.2f %.2f %.2f",
                           b, m[0], m[1], m[2], m[4], m[5], m[6], m[8], m[9], m[10], m[12], m[13], m[14]);
                }
            }
        }
    }
    if (MEM32(model + 0xC) == 73) {
        /* Heather's parts (0x45ED40 draws part i only when bit [part+0x52] of the skeleton's 64-bit mask at +0x10 is
         * set): the list once, the mask at each change (her hands: rigid ones in play, finger-skinned ones in scenes). */
        /* (0.1.9: once a model, not at each change of model: in a mirror's room her model and its reflection's, two
         * models, take turns every frame, and the list ran 110 lines a frame, vr451) */
        static uint32_t listed[8], n_listed, mask_lo = 1, mask_hi = 1;
        uint32_t li = 0;
        while (li < n_listed && listed[li] != model) li++;
        if (li == n_listed) {
            if (n_listed < 8) listed[n_listed++] = model;
            else memmove(listed, listed + 1, 7 * sizeof listed[0]), listed[7] = model;
            uint32_t part = model + MEM32(model + 0x24);
            const uint32_t count = MEM32(model + 0x20);
            for (uint32_t i = 0; i < count && i < 128; i++) {
                rt_log("parts: Heather's part %u: group %u, words +0x44 %u +0x4C %u", i, MEM8(part + 0x52),
                       MEM32(part + 0x44), MEM32(part + 0x4C));
                if (MEM32(part) == 0) break;
                part += MEM32(part);
            }
        }
        if (MEM32(skel + 0x10) != mask_lo || MEM32(skel + 0x14) != mask_hi) {
            mask_lo = MEM32(skel + 0x10), mask_hi = MEM32(skel + 0x14);
            rt_log("parts: frame %d: Heather's shown groups %08X %08X", rt_frame, mask_hi, mask_lo);
        }
    }
    if (sh3vr_active() && MEM32(model + 0xC) >= 1 && MEM32(model + 0xC) <= 4 && MEM8(0x712C656))
        /* LIST 24 (session 13): a small model while a weapon is out: the extras (no mesh signature) carried with her
         * hand to the controller (VR checks that it hangs on her hand) */
        sh3vr_weapon_bones((float *)GPTR(MEM32(0x6B4540)), (int)MEM32(model + 0xC));
    if (sh3vr_active() && MEM32(model + 0xC) == 73) {
        /* Heather's model (73 bones) with its root within a metre (490 units) of her feet: her arms to the hands. */
        const uint32_t hp = vr_player(), hb = MEM32(0x6B4540);
        const float *root = (const float *)GPTR(hb);
        /* 0.1.9 (owner 164): a mirror's draw of her, her bones reflected about the glass (its root's axes turned over),
         * is never taken for her own, however near the glass she stands: it is given her VR pose through the
         * reflection (sh3vr_mirror_bones checks it is her animation's pose reflected, bone for bone). */
        const int reflected = rows_turned_over(root);
        if (reflected) {
            if (hp && sh3vr_mirror_bones((float *)GPTR(hb), 73)) mirror_model = model, mirror_frame = rt_frame;
        } else if (hp && fabsf(root[12] - MEMF(hp)) < 490.0f && fabsf(root[14] - MEMF(hp + 8)) < 490.0f) {
            Sh3VrModel m;
            m.parents = (const int8_t *)GPTR(model + MEM32(model + 0x10));
            m.rest = (const float *)GPTR(model + MEM32(model + 0x08));
            m.blend_mats = (const float *)GPTR(model + MEM32(model + 0x1C));
            m.blend_bones = (const uint8_t *)GPTR(model + MEM32(model + 0x18));
            m.blend_count = (int)MEM32(model + 0x14);
            sh3vr_heather_bones((float *)GPTR(hb), 73, &m);
        } else if (hp) {
            /* LIST 48 (vr146: at 5280 she was in the picture but her head's place 1200 frames old): a scene may pose
             * her model away from the player's own place; her head is still where the scene shows her */
            sh3vr_scene_head((const float *)GPTR(hb));
            scene_model = model, scene_model_frame = rt_frame;  /* 0.1.9 (owner 166): its draw casts her shadow */
        }
    }
    if (sh3vr_active() && MEM32(model + 0xC) >= 20 && MEM32(model + 0xC) <= 128 && sh3vr_in_scene()) {
        /* Each character model a scene draws, once: its bones and how far its root is from her place (dynamic
         * cutscenes need hers) */
        static uint32_t seen_models[32], seen_n;
        uint32_t i = 0;
        while (i < seen_n && seen_models[i] != model) i++;
        if (i == seen_n && seen_n < 32) {
            seen_models[seen_n++] = model;
            const uint32_t hp = vr_player(), hb = MEM32(0x6B4540);
            const float *root = (const float *)GPTR(hb);
            rt_log("scene models: frame %d: model %08X with %u bones, its root at %.0f %.0f %.0f (her place %.0f %.0f %.0f)",
                   rt_frame, model, MEM32(model + 0xC), root[12], root[13], root[14], hp ? MEMF(hp) : 0.0f,
                   hp ? MEMF(hp + 4) : 0.0f, hp ? MEMF(hp + 8) : 0.0f);
        }
    }
    static int dump_at = -2;
    if (dump_at == -2) dump_at = getenv("SH3_VSC_DUMP") ? atoi(getenv("SH3_VSC_DUMP")) : -1;
    if (dump_at < 0 || rt_frame != dump_at) return;
    const uint32_t base = MEM32(0x6B4540), n = MEM32(model + 0xC), p = vr_player();
    rt_log("bones: model %08X skeleton %08X world %08X flag %08X, %u bones at %08X", model, skel, world, flag, n, base);
    if (n < 20 || n > 128) return;  /* the characters (Heather's model has 73), not the props */
    if (p)
        rt_log("bones: Heather (%08X) at %.1f %.1f %.1f yaw %.4f", p, MEMF(p), MEMF(p + 4), MEMF(p + 8),
               MEMF(p + 0x14));
    rt_log("bones: the model's header: %08X %08X %08X %08X %08X %08X %08X %08X", MEM32(model), MEM32(model + 4),
           MEM32(model + 8), MEM32(model + 12), MEM32(model + 16), MEM32(model + 20), MEM32(model + 24),
           MEM32(model + 28));
    {
        char line[4 * 128 + 1];
        int k = 0;
        const uint32_t at = model + MEM32(model + 16);
        for (uint32_t b = 0; b < n; b++) k += snprintf(line + k, sizeof line - k, " %d", (int)(int8_t)MEM8(at + b));
        rt_log("bones: the bytes at the header's +0x10 (parents?):%s", line);
    }
    for (uint32_t b = 0; b < n; b++) {
        const float *m = (const float *)GPTR(base + b * 64);
        rt_log("  bone %3u: %8.4f %8.4f %8.4f %10.1f | %8.4f %8.4f %8.4f %10.1f | %8.4f %8.4f %8.4f %10.1f | %10.1f "
               "%10.1f %10.1f %6.3f", b, m[0], m[1], m[2], m[3], m[4], m[5], m[6], m[7], m[8], m[9], m[10], m[11],
               m[12], m[13], m[14], m[15]);
    }
}
/* LIST 41: her own hands. 0x45ED40 draws a model's parts (eax = model, stack skeleton and world, cdecl; called by the
 * model draw 0x45F5D0 after 0x45EC00 / 0x45EC90 / 0x45ECF0 made the bones, the blends and transposed both): part i is
 * drawn when bit [part+0x52] of the skeleton's 64-bit mask (+0x10 low, +0x14 high) is set. Play shows Heather's rigid
 * one-bone hands (groups 12-31, 33-38); her finger-skinned ones (group 3 her right, 4 her left) are the scenes'. In VR's
 * first person, with her fingers posed this frame, the finger hands are drawn in place of the rigid ones, except a hand
 * holding the weapon: the game's own gripping hand for that weapon (each weapon has its own: the handgun's right hand
 * group 14, the knife's 15, the stun gun's 16, the shotgun's two 18 and 19, ...), placed as the game holds it. Then the
 * ghost pass: a hand pushed through a wall is drawn once more, faint and through the walls, where the controller
 * really is (the bones and blends rebuilt from the ghost pose for that draw, then put back for the draws after). */
static int heather_parts(uint32_t model) {
    /* (0.1.9: two models remembered, as her own and a mirror's reflection of her take turns every frame: one re-read
     * and logged them both each frame, vr451) */
    static uint32_t checked[2];
    static int oks[2], next;
    for (int k = 0; k < 2; k++)
        if (checked[k] == model) return oks[k];
    const int slot = next;
    next ^= 1;
    checked[slot] = model;
    int ok = oks[slot] = 0;
    if (MEM32(model + 0xC) != 73 || MEM32(model + 0x20) != 55) return 0;
    uint32_t part = model + MEM32(model + 0x24);
    int g10 = -1, g14 = -1;
    for (uint32_t i = 0; i < 55; i++) {
        if (i == 10) g10 = MEM8(part + 0x52);
        if (i == 14) g14 = MEM8(part + 0x52);
        if (MEM32(part) == 0) break;
        part += MEM32(part);
    }
    ok = oks[slot] = g10 == 3 && g14 == 4;
    rt_log("parts: model %08X: %s (part 10 group %d, part 14 group %d)", model,
           ok ? "Heather's, her own hands can be drawn" : "not the expected Heather model, her hands left as the game's",
           g10, g14);
    return ok;
}
extern void sub_0045ED40_gen(void);
static void heather_part_draw(uint32_t model, uint32_t skel, uint32_t world) {
    PUSH32(g_esp, world);
    PUSH32(g_esp, skel);
    PUSH32(g_esp, 0xFFFFFFF0u);
    g_eax = model;
    sub_0045ED40_gen();
    g_esp += 8;
}
extern int d3d8_stencil_on(void);
extern void d3d8_her_mark(int on);
void sub_0045ED40(void) {
    const uint32_t model = g_eax, skel = ARG(0), world = ARG(1);
    {
        /* testing (session 12, LIST 56): in SH3_VSC_DUMP's frame, each of her part draws (model 73) and whether the
         * stencil is on for it (a shadow pass draws her with it on: then her head and hands show in her shadow) and the
         * code that asked for the draw */
        static int at = -2;
        if (at == -2) at = getenv("SH3_VSC_DUMP") ? atoi(getenv("SH3_VSC_DUMP")) : -1;
        if (at >= 0 && rt_frame == at && MEM32(model + 0xC) == 73)
            rt_log("vsc her draw: frame %d: stencil %s, from %08x, groups %08x %08x, model %08x skel %08x", rt_frame,
                   d3d8_stencil_on() ? "ON" : "off", MEM32(g_esp), MEM32(skel + 0x10), MEM32(skel + 0x14), model, skel);
    }
    /* 0.1.9 (owner 166: "when she woke up from the dream and was at the table, shadows looked off in 1st person"): in a
     * scene that draws her away from her place with the eyes in her head, the game's own draw of her (the scene's pose,
     * its hands) is marked as hers in play is (bit 0x20; d3d8.c her_shadow_capture takes it for her shadow's caster):
     * her shadow from the room's lamp is then cast from her body as drawn, counted the z-fail way, in place of the game's
     * z-pass volumes of her that her eyes sit inside (d3d8.c her_game_volume). */
    if (sh3vr_active() && MEM32(model + 0xC) == 73 && model == scene_model && scene_model_frame == rt_frame &&
        !sh3vr_own_hands() && sh3vr_scene_eyes() && !rows_turned_over((const float *)GPTR(MEM32(0x6B4540)))) {
        d3d8_her_mark(1);
        sub_0045ED40_gen();
        d3d8_her_mark(0);
        return;
    }
    if (!sh3vr_active() || MEM32(model + 0xC) != 73 || !sh3vr_own_hands() || !heather_parts(model)) {
        sub_0045ED40_gen();
        return;
    }
    /* 0.1.9 (owner 164): a mirror's reflection of her (its bones turned over: the transposed bones keep the sign) has
     * her hands and the flashlight in her hand only when it was given her VR pose this frame (her fingers, through the
     * reflection); else the game's own draw of it, its rigid hands with it (before, her finger hands were drawn on the
     * bones the game leaves unposed in play: the reflection had no hands). It is never her shadow's caster nor drawn
     * as the ghost hands. */
    const int mirrored = rows_turned_over((const float *)GPTR(MEM32(0x6B4540)));
    float mirror_x[16];
    if (mirrored && (mirror_model != model || mirror_frame != rt_frame || !sh3vr_mirror_map(mirror_x))) {
        sub_0045ED40_gen();
        return;
    }
    const uint32_t lo = MEM32(skel + 0x10), hi = MEM32(skel + 0x14);
    {
        /* session 13 (LIST 24, the extras' holds): the game's own groups 40 frames after each weapon change (its
         * gripping hands for that weapon: 12-31 and 33-38) */
        static int last_w = -1, at = -1;
        const int w = MEM8(0x712C656);
        if (w != last_w) last_w = w, at = rt_frame + 40;
        if (rt_frame >= at && at >= 0) {
            at = -1;
            rt_log("parts: weapon %d drawn: the game's own groups %08X %08X (its gripping hands: bits 12-31, 33-38)", w,
                   lo, hi);
        }
    }
    uint32_t new_lo = (lo & 0xFFFu) | 0x18u;  /* groups 12-31 off, 3 and 4 on */
    uint32_t new_hi = hi & ~0x7Eu;            /* groups 33-38 off (32, the chest item, stays) */
    /* Session 7: the flashlight (group 32, part 48, on bone 4 alone) taken in a hand: off her chest here, drawn in the
     * hand below. */
    static float light_bone[16];
    const int light_held = (hi & 1u) && sh3vr_flashlight_bone(light_bone);
    if (light_held) new_hi &= ~1u;
    /* Session 5: a hand on the weapon held shows the game's own gripping hand for it (the weapon placed against it as
     * the game holds it, sh3vr_combat.cpp), its finger hand off. */
    for (int h = 0; h < 2; h++) {
        const int g = sh3vr_native_hand(h);
        {
            /* 0.1.7 (owner message 129, the grips): which hand VR draws on each side, told when it changes */
            static int told[2] = {-2, -2};
            const int shown = (g >= 12 && g <= 38) ? g : -1;
            if (shown != told[h]) {
                told[h] = shown;
                if (shown >= 0)
                    rt_log("parts: frame %d: her %s hand drawn as the game's own gripping hand (group %d), weapon %d",
                           rt_frame, h ? "right" : "left", shown, MEM8(0x712C656));
                else
                    rt_log("parts: frame %d: her %s hand drawn as her finger hand (group %d), weapon %d", rt_frame,
                           h ? "right" : "left", h ? 3 : 4, MEM8(0x712C656));
            }
        }
        if (g < 12 || g > 38) continue;
        new_lo &= ~(h == 1 ? 0x08u : 0x10u);  /* group 3 her right finger hand, 4 her left */
        if (g < 32) new_lo |= 1u << g;
        else new_hi |= 1u << (g - 32);
    }
    /* LIST 56 (session 12): her pixels marked so her own shadow volumes leave her body alone; 0.1.9: a mirror's
     * reflection of her marked the same (2), but not taken for her shadow's caster (d3d8.c her_shadow_capture) */
    d3d8_her_mark(mirrored ? 2 : 1);
    MEM32(skel + 0x10) = new_lo;
    MEM32(skel + 0x14) = new_hi;
    sub_0045ED40_gen();
    const uint32_t eax = g_eax, ecx = g_ecx, edx = g_edx;
    if (light_held) {
        /* Bone 4 as the draw has it (transposed by 0x45ECF0: each 4x4's rows its columns) put in the hand for group
         * 32's draw alone, then given back. */
        const uint32_t b4 = MEM32(0x6B4540) + 4 * 64;
        float saved4[16], t[16], lm[16];
        const float *lb = light_bone;
        if (mirrored) {
            /* 0.1.9: in the reflection's hand: the flashlight through the mirror's map (row-vector: light * X) */
            for (int r = 0; r < 4; r++)
                for (int c = 0; c < 4; c++)
                    lm[r * 4 + c] = light_bone[r * 4] * mirror_x[c] + light_bone[r * 4 + 1] * mirror_x[4 + c] +
                                    light_bone[r * 4 + 2] * mirror_x[8 + c] + light_bone[r * 4 + 3] * mirror_x[12 + c];
            lb = lm;
        }
        memcpy(saved4, GPTR(b4), sizeof saved4);
        for (int r = 0; r < 4; r++)
            for (int c = 0; c < 4; c++) t[r * 4 + c] = lb[c * 4 + r];
        memcpy(GPTR(b4), t, sizeof t);
        MEM32(skel + 0x10) = 0;
        MEM32(skel + 0x14) = 1u;
        sh3vr_item_pass(1);
        heather_part_draw(model, skel, world);
        sh3vr_item_pass(0);
        memcpy(GPTR(b4), saved4, sizeof saved4);
    }
    static float ghost[73 * 16];
    const int bits = mirrored ? 0 : sh3vr_ghost_bones(ghost, 73);
    if (bits) {
        const uint32_t base = MEM32(0x6B4540), blends = MEM32(model + 0x14);
        static uint8_t saved[0x2000 + 256 * 64];
        const uint32_t blend_bytes = (blends < 256 ? blends : 256) * 64;
        memcpy(saved, GPTR(base), 73 * 64);
        memcpy(saved + 0x2000, GPTR(base + 0x2000), blend_bytes);
        memcpy(GPTR(base), ghost, 73 * 64);
        guest_call(0x45EC90, 1, &model, 0);  /* the blends from the ghost pose */
        guest_call(0x45ECF0, 1, &model, 0);  /* both transposed, as the game's draw has them */
        MEM32(skel + 0x10) = ((bits & 1) ? 0x10u : 0) | ((bits & 2) ? 0x08u : 0);
        MEM32(skel + 0x14) = 0;
        sh3vr_ghost_pass(1);
        heather_part_draw(model, skel, world);
        sh3vr_ghost_pass(0);
        memcpy(GPTR(base), saved, 73 * 64);
        memcpy(GPTR(base + 0x2000), saved + 0x2000, blend_bytes);
    }
    d3d8_her_mark(0);
    MEM32(skel + 0x10) = lo;
    MEM32(skel + 0x14) = hi;
    g_eax = eax, g_ecx = ecx, g_edx = edx;
}

/* Film grain (session 7; the owner: "sh3's built in noise/film grain effect should be disabled by default. can be
 * re-enabled via in game settings as needed"). 0x41C090 is the screen's last pass each frame; its noise runs only while
 * 0x72C839 is set, the only place that switch is read (with 0x72C8AA). The game sets it for play (0x4321A0) and clears it
 * for its title screens and menus (0x421BD0, 0x423870, 0x423970...); nothing in the game's options reaches it. The switch
 * also picks the pass's colour grade, so it is left to the game: the grain alone is taken out in d3d8.c
 * (dev_SetRenderState, the noise's alpha test from 0x427ED0). Here the state is told in the log as it changes. */
extern void sub_0041C090_gen(void);
void sub_0041C090(void) {
    static int told = -1;
    const int now = MEM8(0x72C839u) ? (sh3vr_film_grain() ? 2 : 1) : 0;
    if (now != told) {
        static const char *const kStates[] = {"the game's noise switch off (its title or a menu)",
                                              "off (the VR option Film grain; the game's grade kept)",
                                              "on (the VR option Film grain)"};
        told = now;
        rt_log("film grain: frame %d: %s", rt_frame, kStates[now]);
    }
    {
        /* testing (session 12, owner message 51: the game's own shadows of her "at SOME points"): SH3_CHAR_SHADOWS=1
         * sets the room's character-shadow flag (0x821DDF, read by 0x450100) each frame, so 0x685C10 draws the
         * characters' shadow list (0x4D1020) in any room: the next frame's SH3_VSC_DUMP logs that list */
        static int force = -1;
        if (force < 0) force = getenv("SH3_CHAR_SHADOWS") != NULL;
        if (force) MEM8(0x821DDFu) = 1;
    }
    sub_0041C090_gen();
}

/* Depth of field (0.1.5; the owner's 0.1.4 report: "theres also a depth of field effect that seems to still be on during
 * cutscenes. see if possible to disable."). 0x420370, the game's screen-effect pass (its mode at 0x72C828, set by the
 * scenes), copies the frame into a 256x256 target (0x41CBA0, made at 0x427C70: index 2) and in modes 3 to 7 lays it back
 * over the scene blurred through 0x41E200, whose depth tests (ZFUNC, ZWRITEENABLE set round its quads) keep the sharp
 * part at the focus' depth: the depth of field. Its only callers are that pass (0x4206BE, 0x420743). In VR it is a flat
 * screen's blur laid over the eyes (the owner, message 101: no effect placed on the screen), so it is left out: a cdecl
 * call (its level in eax, five arguments its caller pops) whose result is not read. Flat, it is the game's. */
extern void sub_0041E200_gen(void);
void sub_0041E200(void) {
    const int out = sh3vr_active();
    static int told = -1;
    if (out != told) {
        told = out;
        rt_log("depth of field: frame %d: %s (the game's screen-effect mode %u, its blur %u)", rt_frame,
               out ? "left out in VR" : "the game's", MEM32(0x72C828u), MEM8(0x72C866u));
    }
    if (out) {
        g_esp += 4;  /* ret */
        return;
    }
    sub_0041E200_gen();
}

/* The same pass's mode 2 (0x420492) lays its 512x512 copy of the frame (0x41CBA0) back over it three times, each shifted
 * (0x41FF80: the shifts from 0x72C860 / 0x72C862 and the game's random 0x428800), a shaking double picture. In VR those
 * copies are fixed to the view, not the world (message 101), so they are left out as the depth of field is: a cdecl
 * call (four arguments its caller pops) whose result is not read. */
extern void sub_0041FF80_gen(void);
void sub_0041FF80(void) {
    const int out = sh3vr_active();
    static int told = -1;
    if (out != told) {
        told = out;
        rt_log("the game's shaking double picture: frame %d: %s (the screen-effect mode %u)", rt_frame,
               out ? "left out in VR" : "the game's", MEM32(0x72C828u));
    }
    if (out) {
        g_esp += 4;  /* ret */
        return;
    }
    sub_0041FF80_gen();
}

/* The lamps' test (0.1.5; the owner, on the rig's pictures: glows "on spots i dont assume they should be ... not be
 * there when out of view"). 0x49FAC0(screen place x16, view point), called only from 0x468193 (0x468180, a lamp's
 * glow and flare, shown after six frames of 1), tests a lamp: it draws the lamp's point into a 1x1 render target
 * against the scene's depth, at the place the game's own 2D projection gives (0x72C460, not the frame camera's in VR),
 * and locks that target to read the answer: a read of the GPU's work each lamp each frame, which on the Quest waits for
 * the GPU to finish everything before it. In VR: whether the lamp's point is in front of the frame camera and inside
 * its picture (sh3vr_point_in_view, through the projection the frame is drawn with); a wall before a lamp hides its
 * glow by the glow's own depth test in each eye (it is drawn at the lamp's depth with the z test on). No read back. */
extern void sub_0049FAC0_gen(void);
void sub_0049FAC0(void) {
    if (sh3vr_active() && sh3vr_world_effects()) {
        const uint32_t p = ARG(1);
        const float pt[4] = {MEMF(p), MEMF(p + 4u), MEMF(p + 8u), 1.0f};
        const int vis = sh3vr_point_in_view(pt);
        static int told;
        if (told < 2 && ++told)
            rt_log("the lamps' test: frame %d: a lamp %.2f m ahead %s (the frame camera's view, no read back of the "
                   "GPU's work)", rt_frame, pt[2] / 490.0f, vis ? "in view" : "out of view");
        g_eax = vis ? 1u : 0u;
        g_esp += 4;  /* ret (cdecl: the caller pops its two arguments) */
        return;
    }
    sub_0049FAC0_gen();
}

/* The flashlight (session 7; docs/GAME_FACTS.md, The flashlight). Heather's light record at 0x712C1D0 (made each frame
 * from her model by 0x475E00: the beam's position at her chest, its direction, then its shadow caster's position and
 * direction about 0.5 m ahead) is read by the light's glare (0x44A7B0), the monsters (0x5D27A0, from their own code)
 * and 0x457AC0, which copies it into the light buffer [0x848AF0] +0x40..+0x7F that the swap after it (0x457B70) makes
 * the one this frame's draws read: the lighting (0x457D60), the beam (c16 and c80-c82, 0x461840) and the shadows. VR
 * places the light in the record just before that copy (sh3vr_flashlight_place), so all of them follow. Its state is
 * logged as it changes: 0x712CABC bits 0-1 (0 off, 1 on, 2 turning on, 3 turning off), the strength 0x712CAC4, and
 * 0xB6B850 (set by a room's script, 0x5724B0: the light held at 1e-5 there). */
extern void sub_00457AC0_gen(void);
void sub_00457AC0(void) {
    static int was = -1;
    const int state = MEM8(0x712CABCu) & 3, held = MEM32(0xB6B850u) != 0;
    if ((state | held << 2) != was) {
        static const char *const names[4] = {"off", "on", "turning on", "turning off"};
        was = state | held << 2;
        rt_log("flashlight: frame %d: %s, strength %.2f%s", rt_frame, names[state], MEMF(0x712CAC4u),
               held ? ", held dark by the room's script" : "");
    }
    if (sh3vr_active()) {
        /* the light's level for her own draws in first person (sh3vr_her_light): 0 off or held dark by a room */
        sh3vr_flashlight_level(state && !held ? MEMF(0x712CAC4u) : 0.0f);
        sh3vr_flashlight_place((float *)GPTR(0x712C1D0u), (float *)GPTR(0x712C1E0u), (float *)GPTR(0x712C1F0u),
                               (float *)GPTR(0x712C200u));
    }
    sub_00457AC0_gen();
}

/* LIST 23: the radio's static. 0x46D130(voice, level, pan, pitch) (cdecl; the radio update 0x4AA6B0 calls it a voice
 * at a time, from 0x18-byte records: level float, pan float in radians, pitch u16) sets one of the radio's four
 * voices (DirectSound buffers of sounds 0x163-0x166, made by 0x4EEEF0) through 0x4A8550 / 0x4EEA90 ("dsFxSet /
 * radio_no"): volume index = clamp((sound option + 10) / 15, 0, 1) x [0x89E3C0] x level x 127. VR buzzes the hands
 * with the level (docs/GAME_FACTS.md, Sound). */
extern void sub_0046D130_gen(void);
void sub_0046D130(void) {
    const uint32_t voice = ARG(0);
    const float level = ARGF(1);
    sub_0046D130_gen();
    if (sh3vr_active()) sh3vr_radio((int)voice, level);
}

/* LIST 45: the world worked with the hands. The game's action check 0x4B9D60(mode, request, kind) (cdecl; the event
 * dispatch 0x4B9E70 calls it) reads Heather's position (0x472FE0) and turn (0x473000) into a record on its stack:
 * +0x00 position (x, y, z, w), +0x10 turn (+0x14 the yaw), +0x20 / +0x28 the point 150 units (31 cm) in front of her
 * (x / z), +0x30 / +0x38 her facing x 500 (x / z); then 0x4B9940(mode, room, request, kind, pass, record) (cdecl)
 * tests each of the room's events against it (0x4B9840 by the region's shape) and returns the event found, -1 none.
 * "Take items with your hand": while a closing hand asks (sh3vr_hand_action), the record's front point is the palm
 * and her facing points at it, so the game's own pick-up, door or examine runs for what is at the hand. */
extern void sub_004B9940_gen(void);
/* The PC rig, SH3_EVENT_SCAN=F (session 10: no run had seen a hand use an event yet): at the first action check
 * (pass 0) from frame F on, and again every SH3_EVENT_SCAN_EVERY frames when set, the same checks again for points
 * all round her (0.3 to 6 m, every 22.5 degrees, her facing at each), as if the action button were pressed (the
 * check's mode 1: 0x4B9D60 gives 0x4B9940 mode 1 only with the button down, 0x495B60(0)): both passes of 0x4B9940
 * and the floor items' check 0x4B74B0 (the items lying in the room, 0xBA9610: found as 0x400 + the item's place).
 * Each event found is logged with where: the spots a hand can reach in a check run. All three only read (the room's
 * tables and the record); cdecl, so the arguments stay put: each call again from the same stack. */
/* The room's event table, logged once with the scan (session 11: the start of play showed no event within 6 m, so
 * which events the room has and where). [0x711FCA0] -> {events, regions}: events of 5 words (0x14 bytes) until one all
 * zero; each field read through 0x6CF6B0's table of (word, shift, mask u16), 12 fields: pass, type, flag A's value and
 * index (0x715D060's bits), flag B's, c8, trigger (0 always, 1 the action button (mode 0 skips it), 2 by place, 3),
 * the region's offset into the regions, d4 (1: wants the request = value), value, dc. The region: a shape byte, then
 * floats (0x4B7600 shape 1/16 against the front point +0x20/+0x28, 0x4B8780 the polygons, 0x4B78B0 13-15/28-30). */
static void event_table_log(void) {
    const uint32_t tab = MEM32(0x711FCA0u);
    if (!tab || !MEM32(tab)) { rt_log("event table: none"); return; }
    const uint32_t evs = MEM32(tab), regs = MEM32(tab + 4);
    int n = 0;
    for (; n < 600; n++) {
        const uint32_t e = evs + (uint32_t)n * 0x14u;
        if (!(MEM32(e) | MEM32(e + 4) | MEM32(e + 8) | MEM32(e + 12) | MEM32(e + 16))) break;
        uint32_t f[12];
        for (int k = 0; k < 12; k++) {
            const uint32_t d = 0x6CF6B0u + 4u * (uint32_t)k;
            f[k] = (MEM32(e + 4u * MEM8(d)) >> MEM8(d + 1)) & (MEM8(d + 2) | (uint32_t)MEM8(d + 3) << 8);
        }
        const int flag_a = f[3] ? (int)((MEM32(0x715D060u + (f[3] >> 5) * 4) >> (f[3] & 31)) & 1) : -1;
        const int flag_b = f[5] ? (int)((MEM32(0x715D060u + (f[5] >> 5) * 4) >> (f[5] & 31)) & 1) : -1;
        char hex[3 * 26 + 1] = "";
        if (f[7]) {
            const uint32_t r = regs + f[8];
            for (int k = 0; k < 26; k++) snprintf(hex + 3 * k, 4, "%02x ", MEM8(r + (uint32_t)k));
        }
        /* session 15: the region's first place (its shape byte, then floats x and z, unaligned: vr189's event 18,
         * 17 / -16687 / 22911), to walk to a room's doors; ops 0x02 and 0x04 have an s16 between x and z (vr224's
         * event 4 at 15200 20800, 46 at 22443 23959: docs/GAME_FACTS.md) */
        float rx = 0, rz = 0;
        if (f[7]) {
            uint32_t bx = 0, bz = 0;
            const uint8_t op = MEM8(regs + f[8]);
            const uint32_t at_z = op == 0x02 || op == 0x04 ? 7u : 5u;
            for (int k = 0; k < 4; k++) {
                bx |= (uint32_t)MEM8(regs + f[8] + 1u + (uint32_t)k) << (8 * k);
                bz |= (uint32_t)MEM8(regs + f[8] + at_z + (uint32_t)k) << (8 * k);
            }
            memcpy(&rx, &bx, 4);
            memcpy(&rz, &bz, 4);
        }
        rt_log("event table %d: pass %u type %u flag A %u=%u (now %d) flag B %u=%u (now %d) c8 %u trigger %u region %u d4 %u "
               "value %u dc %u; words %08X %08X %08X %08X %08X; region %s(at %.0f %.0f)",
               n, f[0], f[1], f[3], f[2], flag_a, f[5], f[4], flag_b, f[6], f[7], f[8], f[9], f[10], f[11], MEM32(e),
               MEM32(e + 4), MEM32(e + 8), MEM32(e + 12), MEM32(e + 16), hex, rx, rz);
    }
    rt_log("event table: %d events", n);
    /* session 15 (LIST 45, a floor item by hand): the room's floor items, as 0x4B74B0 walks them: 0xBA9610's indices
     * (up to 64, -1 ends) into the room table's items ([tab + 0xC] + 18 x index; +0x10 the taken flag's index in
     * 0x715D060's bits; +6 / +8 a place's index, x 64, in the place table 0x450CA0 gives) */
    int items = 0, left = 0;
    for (int i = 0; i < 64; i++) {
        const int32_t idx = (int32_t)MEM32(0xBA9610u + 4u * (uint32_t)i);
        if (idx == -1) break;
        const uint32_t it = MEM32(tab + 0xC) + 18u * (uint32_t)idx;
        const int16_t fl = SMEM16(it + 0x10);
        const int taken = fl >= 0 ? (int)((MEM32(0x715D060u + (uint32_t)(fl >> 5) * 4u) >> (fl & 31)) & 1) : -1;
        char ib[3 * 18 + 1] = "";
        for (int k = 0; k < 18; k++) snprintf(ib + 3 * k, 4, "%02x ", MEM8(it + (uint32_t)k));
        rt_log("floor item %d: index %d, taken flag %d = %d; bytes %s", i, idx, fl, taken, ib);
        items++;
        left += taken == 0;
    }
    rt_log("floor items: %d in the room, %d not taken", items, left);
}
/* One action check as if the button were down, for her at (px, pz) facing `yaw` with her facing ray `reach` units long
 * (the game's 500, 1.02 m: the polygon regions test the segment from her place along +0x30/+0x38, 0x4B8780 ->
 * 0x4B76B0, and the point regions the front point +0x20/+0x28, 0x4B7600, `front` or 150 units ahead): check 0 / 1 the
 * events' passes 0 / 1 (0x4B9940 mode 1), 2 the floor items (0x4B74B0). All only read; the record is set from
 * `saved` first (the caller puts it back), the arguments and the stack as they were after. */
/* 0.1.8 (owner 152, a player: "I took the key for the bookstore. But when I went back to the book store, the key is
 * not in my inventory"): 0x4B9940 SPENDS an item when it finds an event that uses one up: at its end (0x4B9CBF), with
 * its fourth argument ("kind") 0, an event whose field d4 asks for an item (1: the item used from the inventory, 2: an
 * item held, as a key at its door) and whose field dc is set has that item taken from the inventory (0x496A20: one
 * fewer, or its bit cleared), because a found event always runs after it. VR's own checks (the hands' reach every
 * frame, the hints', the button's along her facing and where the player looks, a closing hand's) only look, and ran
 * with the game's kind 0: a hand reaching toward the bookstore's door, or A pressed at it, spent the key there and then,
 * the door not opened, and the game's own check that followed found the door's locked event (its key not held). Every
 * key's door could eat its key so. VR's checks now pass kind 1 (0x4B9940 reads it nowhere else: what is found is the
 * same), and the inventory (its bits and counts, 0x712CA80-0x712CABF) is kept as it was across each of them, put back
 * and logged if anything in the check changed it. */
static int action_check_at(uint32_t rec, const float *saved, float px, float pz, float yaw, float reach,
                           const float *front, int check) {
    const uint32_t sp = g_esp, mode = ARG(0), pass = ARG(4), kind = ARG(3);
    static int spends = -1;  /* testing: SH3_LOOK_SPENDS=1 as 0.1.7 (VR's looks spend items: the rig's comparison) */
    if (spends < 0) spends = getenv("SH3_LOOK_SPENDS") ? 1 : 0;
    uint8_t inv[0x40];
    memcpy(inv, GPTR(0x712CA80u), sizeof inv);
    memcpy(GPTR(rec), saved, 15 * sizeof(float));
    MEMF(rec) = px;
    MEMF(rec + 8) = pz;
    MEMF(rec + 0x14) = yaw;
    MEMF(rec + 0x20) = front ? front[0] : px + sinf(yaw) * 150.0f;
    MEMF(rec + 0x28) = front ? front[2] : pz + cosf(yaw) * 150.0f;
    MEMF(rec + 0x30) = sinf(yaw) * reach;
    MEMF(rec + 0x38) = cosf(yaw) * reach;
    int ev;
    if (check < 2) {
        ARG(0) = 1;
        if (!spends) ARG(3) = 1;  /* (only looking: an item the found event uses is not spent) */
        ARG(4) = (uint32_t)check;
        sub_004B9940_gen();
        ev = (int)g_eax;
        g_esp = sp;
        ARG(0) = mode;
        ARG(3) = kind;
        ARG(4) = pass;
    } else {
        ev = (int)guest_call(0x4B74B0u, 1, &rec, 0);
        g_esp = sp;
    }
    if (memcmp(inv, GPTR(0x712CA80u), sizeof inv) && spends) {
        static int told;
        if (told++ < 40)
            rt_log("testing: frame %d: VR's look (check %d) found event %d and the game spent an item for it (0.1.7's "
                   "way, SH3_LOOK_SPENDS=1)", rt_frame, check, ev);
    } else if (memcmp(inv, GPTR(0x712CA80u), sizeof inv)) {
        static int told;
        if (told++ < 40) {
            int item = -1;
            for (int b = 0; b < 112 && item < 0; b++)
                if (((inv[b >> 3] ^ MEM8(0x712CA80u + (uint32_t)(b >> 3))) >> (b & 7)) & 1) item = b;
            rt_log("VR: frame %d: VR's look for what the button would use (check %d, event %d) changed the inventory "
                   "(item %d first): put back as it was", rt_frame, check, ev, item);
        }
        memcpy(GPTR(0x712CA80u), inv, sizeof inv);
    }
    return ev;
}

/* An event's trigger (event_table_log's field 7: 0 none, 1 the action button, 2 by place, 3), -1 past the table. */
static int event_trigger(int ev) {
    const uint32_t tab = MEM32(0x711FCA0u);
    if (!tab || !MEM32(tab) || ev < 0) return -1;
    const uint32_t e = MEM32(tab) + (uint32_t)ev * 0x14u, d = 0x6CF6B0u + 4u * 7u;
    return (int)((MEM32(e + 4u * MEM8(d)) >> MEM8(d + 1)) & (MEM8(d + 2) | (uint32_t)MEM8(d + 3) << 8));
}

/* 0.1.9 (the owner, after 0.1.8: "Yes, add it": give back the keys 0.1.7 already lost in players' saves). 0.1.7's looks
 * spent a key at its door without opening it (action_check_at's note above), so a save made after that has the key
 * gone and its door locked for good. Only the keys whose door uses them up could be lost so: the stage tables' events
 * (0x6CF7D0, a table per stage) with d4 = 2 (an item held), dc set and the button's trigger are these nine doors, eight
 * keys; and the game takes these keys nowhere but at their doors (every call of 0x496A20 and 0x497D30 read, session 33).
 * So at the game's own action check (the button, or a closing hand), in the stage of one of these doors whose event is
 * still on (its flag A as the event asks it: the door not opened yet), with its key not held but given before (the flag
 * its giver sets with it), the key is lent to the check: when the game's check then finds that door, the game uses the
 * key there as its own (it spends it and the door opens, as with the key held); otherwise the key is taken back at once.
 * Nothing changes for a player who has the key, never had it, or has opened the door. `given`: the flag set with the
 * key (-1: every game has it from its start). */
static const struct {
    uint8_t stage, key;
    uint16_t event;
    int16_t given;
} key_doors[] = {
    {2, 38, 48, 42},    /* given by stage 2's event 98, which sets flag 42 */
    {4, 47, 50, 193},   /* the room script 0x589610 sets flag 193 as it gives it */
    {15, 61, 21, 401},  /* stage 17's event 59 */
    {20, 35, 65, -1},   /* given at every game's start (0x58E750, with items 1 and 36) */
    {25, 64, 8, 521},   /* stage 25's event 88 (item 63 used) gives it */
    {26, 64, 4, 521},   /* (the same door from the next stage) */
    {30, 66, 20, 689},  /* stage 32's event 80 */
    {33, 69, 119, 747}, /* the room script 0x57428C sets flag 747 as it gives it */
    {37, 78, 10, 828},  /* stage 38's event 99 */
};
#define KEY_DOORS ((int)(sizeof key_doors / sizeof key_doors[0]))
/* testing (the PC rig, whose rooms have none of these doors): SH3_KEY_BACK_TEST="frame:event:item:flag" takes `item`
 * away at that frame as 0.1.7's looks did and makes the room's event `event` (SH3_SPEND_TEST makes it a key's door)
 * one more of these doors, the key given by `flag` (-1: none) */
static uint32_t key_test_tab;
static int key_test_ev = -1, key_test_item, key_test_given = -1;

static int game_flag(int f) { return f >= 0 && f < 4096 ? (int)((MEM32(0x715D060u + 4u * (uint32_t)(f >> 5)) >> (f & 31)) & 1u) : 0; }
static int item_held(int it) { return (int)((MEM32(0x712CA80u + 4u * (uint32_t)(it >> 5)) >> (it & 31)) & 1u); }
static uint32_t event_field(uint32_t tab, int ev, int k) {
    const uint32_t e = MEM32(tab) + (uint32_t)ev * 0x14u, d = 0x6CF6B0u + 4u * (uint32_t)k;
    return (MEM32(e + 4u * MEM8(d)) >> MEM8(d + 1)) & (MEM8(d + 2) | (uint32_t)MEM8(d + 3) << 8);
}
static int key_door(int i, uint32_t* tab, int* ev, int* key, int* given) {
    if (i < KEY_DOORS) {
        *tab = MEM32(0x6CF7D0u + 4u * key_doors[i].stage);
        *ev = key_doors[i].event, *key = key_doors[i].key, *given = key_doors[i].given;
        return 1;
    }
    if (i == KEY_DOORS && key_test_ev >= 0) {
        *tab = key_test_tab, *ev = key_test_ev, *key = key_test_item, *given = key_test_given;
        return 1;
    }
    return 0;
}
/* the keys lent to this check (a bit per key_doors entry) */
static uint32_t keys_lend(void) {
    const uint32_t now = MEM32(0x711FCA0u);
    if (!now || !MEM32(now)) return 0;
    uint32_t lent = 0;
    for (int i = 0; i <= KEY_DOORS; i++) {
        uint32_t tab;
        int ev, key, given;
        if (!key_door(i, &tab, &ev, &key, &given) || tab != now) continue;
        /* the door as the table has it (a key's door: d4 2, its key, dc), still on, the key not held but given */
        if (event_field(now, ev, 9) != 2u || event_field(now, ev, 10) != (uint32_t)key || !event_field(now, ev, 11))
            continue;
        if ((uint32_t)game_flag((int)event_field(now, ev, 3)) != event_field(now, ev, 2)) continue;
        if (item_held(key) || (given >= 0 && !game_flag(given))) continue;
        MEM32(0x712CA80u + 4u * (uint32_t)(key >> 5)) |= 1u << (key & 31);
        lent |= 1u << i;
        static int told;
        if (told++ < 20)
            rt_log("VR: frame %d: key %d not held though given (flag %d), its door (event %d) still locked: lent to the "
                   "action check (0.1.7's looks spent such keys)", rt_frame, key, given, ev);
    }
    return lent;
}
/* after the game's check: a lent key the game used at its door stays used (the door opens); any other is taken back */
static void keys_settle(uint32_t lent, int found) {
    for (int i = 0; i <= KEY_DOORS; i++) {
        uint32_t tab;
        int ev, key, given;
        if (!(lent & (1u << i)) || !key_door(i, &tab, &ev, &key, &given)) continue;
        if (found == ev) {
            rt_log("VR: frame %d: key %d given back: the game's check found its door (event %d) and used the key there "
                   "(in the inventory now %d)", rt_frame, key, ev, item_held(key));
        } else {
            MEM32(0x712CA80u + 4u * (uint32_t)(key >> 5)) &= ~(1u << (key & 31));
            static int told;
            if (told++ < 20)
                rt_log("VR: frame %d: key %d taken back: the check found %d, not its door (event %d)", rt_frame, key,
                       found, ev);
        }
    }
}

/* 0.1.10 (owner 170: "grabbing the knob/handle/whatever and then pulling it open, which would then start the load into
 * the next section ... for all doors"): whether event `ev` of the room's table is a door, for a hand to take its handle
 * and pull it (sh3vr_hand_hover's `door`): the button's (trigger 1) with a room to go to (field 17: 721 of the stage
 * tables' 2429 button events, every door and gate that loads another room or area; stage 2's entrance, event 5, is
 * one), or one of the keys' doors above (their scripts load the room: field 17 is 0 there). */
int sh3_event_is_door(int ev) {
    const uint32_t tab = MEM32(0x711FCA0u);
    if (!tab || !MEM32(tab) || ev < 0 || ev >= 0x400 || event_trigger(ev) != 1) return 0;
    if (event_field(tab, ev, 17)) return 1;
    for (int i = 0; i < KEY_DOORS; i++)
        if (MEM32(0x6CF7D0u + 4u * key_doors[i].stage) == tab && key_doors[i].event == ev) return 1;
    return 0;
}

/* The ray a hand's action uses (session 11, ask 28: "drawers, objects, handles, doors", the hand at the thing): from
 * her place toward the palm, to 20 cm (98 units) past it, at least 0.5 m (a hand reaching down or close still reaches
 * what is right before her), never past the game's own 1.02 m. */
static void hand_ray(uint32_t rec, const float at[3], float *yaw, float *reach) {
    const float dx = at[0] - MEMF(rec), dz = at[2] - MEMF(rec + 8);
    *yaw = atan2f(dx, dz);
    *reach = fminf(500.0f, fmaxf(245.0f, sqrtf(dx * dx + dz * dz) + 98.0f));
}

/* The PC rig, SH3_EVENT_SCAN=F: at the first per-frame action check (pass 0) from frame F on, and again every
 * SH3_EVENT_SCAN_EVERY frames when set, the same checks with the button down (action_check_at, all three) for her
 * standing at points all round her (0 to 6 m, every 22.5 degrees) and facing 8 ways at each: each event or floor item
 * logged once, from the nearest spot it is found at. The room's event table is logged at the first. */
static void event_scan(uint32_t rec) {
    static int scan_at = -2, every = 0, next = -1;
    static uint32_t table_logged = 0;  /* session 15: the table of each room (its [0x711FCA0]) once */
    if (scan_at == -2) {
        scan_at = getenv("SH3_EVENT_SCAN") ? atoi(getenv("SH3_EVENT_SCAN")) : -1;
        every = getenv("SH3_EVENT_SCAN_EVERY") ? atoi(getenv("SH3_EVENT_SCAN_EVERY")) : 0;
        next = scan_at;
    }
    if (next < 0 || rt_frame < next || ARG(4) != 0) return;
    next = every > 0 ? rt_frame + every : -1;
    const uint32_t tab_now = MEM32(0x711FCA0u), tab_key = tab_now && MEM32(tab_now) ? tab_now ^ MEM32(tab_now) * 31u ^ MEM32(MEM32(tab_now) + 4u) * 131u ^ MEM32(tab_now + 4u) * 7u : tab_now;
    if (table_logged != tab_key) {
        table_logged = tab_key;  /* the pointer and its first event: a room loaded into the same place differs */
        rt_log("event scan: frame %d: a room's table (%08X): its events and floor items", rt_frame, table_logged);
        event_table_log();
    }
    float saved[15];
    memcpy(saved, GPTR(rec), sizeof saved);
    const float x0 = saved[0], z0 = saved[2], yaw0 = saved[5];
    static const float dists[11] = {0.0f, 0.5f, 1.0f, 1.5f, 2.0f, 2.5f, 3.0f, 3.5f, 4.0f, 5.0f, 6.0f};
    static unsigned char seen[2048];
    memset(seen, 0, sizeof seen);
    int found = 0, kinds = 0;
    rt_log("event scan: frame %d: mode %u, room %u, request %d, kind %d, pass %d; her place %.0f %.0f %.0f, yaw %.1f deg",
           rt_frame, ARG(0), ARG(1), (int)ARG(2), (int)ARG(3), (int)ARG(4), x0, saved[1], z0, yaw0 * 57.29578f);
    for (int r = 0; r < 11; r++)
        for (int k = 0; k < (r ? 16 : 1); k++)
            for (int j = 0; j < 8; j++)
                for (int check = 0; check < 3; check++) {
                    const float a = yaw0 + k * 0.3926991f, d = dists[r] * 490.0f;
                    const float px = x0 + sinf(a) * d, pz = z0 + cosf(a) * d, b = yaw0 + j * 0.7853982f;
                    const int ev = action_check_at(rec, saved, px, pz, b, 500.0f, NULL, check);
                    if (ev < 0) continue;
                    found++;
                    const int idx = ev < 2048 ? ev : 2047;
                    if (seen[idx]) continue;
                    seen[idx] = 1;
                    kinds++;
                    rt_log("event scan: %s %d: standing %.1f m away %.1f deg right of her facing (%.0f %.0f), facing "
                           "%.0f deg right of hers",
                           check == 2 ? "floor item" : check ? "event (pass 1)" : "event", ev, dists[r], k * 22.5f, px,
                           pz, j * 45.0f);
                }
    memcpy(GPTR(rec), saved, sizeof saved);
    rt_log("event scan: %d checks with an event, %d events", found, kinds);
}

/* The per-frame check (mode 0, no button; 0x4B9D60 calls it each frame of play): first, for each free hand reaching
 * out (sh3vr_hand_reach), the same check as if the button were down along the hand's ray, so the hand feels what it
 * can use before closing (sh3vr_hand_hover: a light tick). Then a closing hand's action (sh3vr_hand_action): the
 * record's front point is the palm and her facing ray goes toward it (hand_ray), so the game's own pick-up, door or
 * examine runs for what is at the hand. The hand's ray stays in the record after pass 0 (the record is 0x4B9D60's own,
 * dropped when it returns): the floor items' check 0x4B74B0 and pass 1 that follow read it too. */
void sub_004B9940(void) {
    const uint32_t rec = ARG(5);
    float at[3], yaw, reach;
    if (rec) event_scan(rec);
    if (rec && sh3vr_active() && ARG(0) == 0 && ARG(3) == 0 && ARG(4) == 0) {
        float saved[15];
        memcpy(saved, GPTR(rec), sizeof saved);
        for (int h = 0; h < 2; h++) {
            int ev = -1;
            if (sh3vr_hand_reach(h, at)) {
                hand_ray(rec, at, &yaw, &reach);
                /* session 14 (owner 66, doors and drawers; vr196-197: event 8's region lies within the button's 1.02 m
                 * of her spot but past the hand's own ray): the hand's ray first, then the game's own reach toward the
                 * hand (round 1), so reaching toward a door, a drawer or a switch uses it wherever the button would */
                for (int check = 0; check < 6 && ev < 0; check++) {
                    if (check == 3) reach = 500.0f;
                    ev = action_check_at(rec, saved, saved[0], saved[2], yaw, reach, at, check % 3);
                    /* only what the button uses (trigger 1) or a floor item: the room's own events with no region or
                     * by place (vr171: event 136, flag-driven, found anywhere) are the game's, not the hand's */
                    if (ev >= 0 && ev < 0x400 && event_trigger(ev) != 1) {
                        /* session 13 (vr196: event 8's place swept all round with the left hand, nothing): each event
                         * a hand's check meets but leaves to the game, once, with its trigger */
                        static unsigned char told[0x400];
                        if (!told[ev]) {
                            told[ev] = 1;
                            rt_log("VR: frame %d: the %s hand's check met event %d (trigger %d, not the button's: left "
                                   "to the game)", rt_frame, h ? "right" : "left", ev, event_trigger(ev));
                        }
                        ev = -1;
                    }
                }
                /* the PC rig, SH3_HAND_RAY_LOG=N: each reaching hand's ray every N frames (her place, the ray's way and
                 * the hand's reach, what it found), to set a hand where the button works (vr204: event 8 not met) */
                static int ray_every = -2;
                if (ray_every == -2) ray_every = getenv("SH3_HAND_RAY_LOG") ? atoi(getenv("SH3_HAND_RAY_LOG")) : 0;
                if (ray_every > 0 && rt_frame % ray_every == 0)
                    rt_log("hand ray: frame %d: %s hand, her place %.0f %.0f facing %.1f deg; the ray %.1f deg, the "
                           "hand's reach %.0f (then 500), the palm %.0f %.0f %.0f: event %d", rt_frame, h ? "right" : "left",
                           saved[0], saved[2], saved[5] * 57.29578f, yaw * 57.29578f,
                           fminf(500.0f, fmaxf(245.0f, sqrtf((at[0] - saved[0]) * (at[0] - saved[0]) +
                                                             (at[2] - saved[2]) * (at[2] - saved[2])) + 98.0f)),
                           at[0], at[1], at[2], ev);
            }
            sh3vr_hand_hover(h, ev, ev >= 0 && ev < 0x400 && sh3_event_is_door(ev));
        }
        /* 0.1.7 (owner message 143, the hints): what the action button would use within her reach now, every 8 frames
         * while that hint is still to show (sh3vr_tip_wants_use): along her facing, then where the player looks (the
         * head's heading and 30 degrees either side, as the button's own check below), the game's 1.02 m; the events
         * the button uses (trigger 1), then the floor items. Only read (action_check_at); the record is put back
         * after, as above. */
        if (sh3vr_tip_wants_use()) {
            float gaze_at[3], gaze_yaw = saved[5];
            const int gaze = sh3vr_gaze(gaze_at, &gaze_yaw);
            int found = -1, item = 0;
            for (int k = 0; k < (gaze ? 4 : 1) && found < 0; k++) {
                const float y = k == 0 ? saved[5] : gaze_yaw + (k == 2 ? 0.5236f : k == 3 ? -0.5236f : 0.0f);
                for (int check = 0; check < 3 && found < 0; check++) {
                    const int ev = action_check_at(rec, saved, saved[0], saved[2], y, 500.0f, NULL, check);
                    if (ev < 0 || (check < 2 && ev < 0x400 && event_trigger(ev) != 1)) continue;
                    found = ev;
                    item = check == 2;
                }
            }
            if (found >= 0) {
                static int told = 0;
                if (told++ < 20)
                    rt_log("VR: frame %d: the hints' check: %s %d within her reach", rt_frame,
                           item ? "floor item" : "event", found);
                sh3vr_tip_use(item);
            }
        }
        memcpy(GPTR(rec), saved, sizeof saved);
    }
    const int hand = sh3vr_active() && rec && sh3vr_hand_action(at);
    if (hand) {
        hand_ray(rec, at, &yaw, &reach);
        /* as the reach check above: the hand's own ray when it meets something (what is at the hand comes first),
         * else the game's own 1.02 m toward the hand */
        {
            float saved[15];
            memcpy(saved, GPTR(rec), sizeof saved);
            int any = -1;
            for (int check = 0; check < 3 && any < 0; check++) {
                any = action_check_at(rec, saved, saved[0], saved[2], yaw, reach, at, check);
                if (any >= 0 && any < 0x400 && event_trigger(any) != 1) any = -1;
            }
            if (any < 0) reach = 500.0f;
            memcpy(GPTR(rec), saved, sizeof saved);
        }
        MEMF(rec + 0x14) = yaw;
        MEMF(rec + 0x20) = at[0];
        MEMF(rec + 0x28) = at[2];
        MEMF(rec + 0x30) = sinf(yaw) * reach;
        MEMF(rec + 0x38) = cosf(yaw) * reach;
    }
    const uint32_t mode = ARG(0), kind = ARG(3), pass = ARG(4);
    /* 0.1.9: a key 0.1.7's looks spent, lent to the button's (or a closing hand's) check at its door (keys_lend) */
    const uint32_t lent = rec && mode && kind == 0 && pass < 2 ? keys_lend() : 0;
    /* her place, when the check below used the head's (put back after it), and the head's place it used */
    float gaze_from[2] = {0.0f, 0.0f}, gaze_used[2] = {0.0f, 0.0f};
    int gaze_moved = 0;
    /* where the check along the player's look found nothing either (logged below if the game's own finds nothing) */
    int gaze_none = 0;
    float none_yaw = 0.0f, none_her = 0.0f, none_at[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    if (!hand && sh3vr_active() && rec && mode && kind == 0 && pass < 2) {
        /* 0.1.7 (a player on 0.1.6: "I could not open the second green gate at the start of the game. The first gate I
         * could open with the A button"): the button's check looks 1.02 m along HER facing, which in first person
         * trails the head (she turns after it only past 50 degrees), so a gate the player looks at can lie beside her
         * facing's line. When her own facing finds nothing the button uses, the same check along where the player
         * looks (the head's heading, then 15, 30 and 45 degrees either side of it; from her place, then from the head's
         * own place when it stands apart from hers): what it finds first is what the game's own check gets (the
         * record's facing turned to it), so the door, item or examine runs as the game's own. */
        float gaze_at[3], gaze_yaw;
        if (sh3vr_gaze(gaze_at, &gaze_yaw)) {
            float saved[15];
            memcpy(saved, GPTR(rec), sizeof saved);
            /* testing (the PC rig, whose steering holds her facing): SH3_GAZE_TEST=1 leaves her own facing's check out,
             * so only the check along where the player looks can find what the button uses; SH3_GAZE_TEST=2 also leaves her
             * place out of that, so the head's place is tried alone (when it stands 5 to 70 cm from hers) */
            static int gaze_test = -1;
            if (gaze_test < 0) gaze_test = getenv("SH3_GAZE_TEST") ? (atoi(getenv("SH3_GAZE_TEST")) == 2 ? 2 : 1) : 0;
            int ev = gaze_test ? -1 : action_check_at(rec, saved, saved[0], saved[2], saved[5], 500.0f, NULL, (int)pass);
            if (ev >= 0 && ev < 0x400 && event_trigger(ev) != 1) ev = -1;
            if (ev < 0) {
                static const float fan[7] = {0.0f, 0.2618f, -0.2618f, 0.5236f, -0.5236f, 0.7854f, -0.7854f};
                const float hx = gaze_at[0] - saved[0], hz = gaze_at[2] - saved[2], hd = sqrtf(hx * hx + hz * hz);
                const int places = hd > 0.05f * 490.0f && hd < 0.7f * 490.0f ? 2 : 1;
                /* the game's own 1.02 m first, then 1.33 m: the hall's far door (event 48) has its line about 0.8 m
                 * behind the door's face, so standing at the door the game's reach meets it only facing it squarely */
                static const float reaches[2] = {500.0f, 650.0f};
                float use_yaw = 0.0f, use_x = saved[0], use_z = saved[2], use_reach = 500.0f;
                for (int r = 0; r < 2 && ev < 0; r++)
                    for (int pl = gaze_test == 2 && places == 2 ? 1 : 0; pl < places && ev < 0; pl++)
                        for (int k = 0; k < 7 && ev < 0; k++) {
                            const float px = pl ? gaze_at[0] : saved[0], pz = pl ? gaze_at[2] : saved[2];
                            const float y = gaze_yaw + fan[k];
                            ev = action_check_at(rec, saved, px, pz, y, reaches[r], NULL, (int)pass);
                            if (ev >= 0 && ev < 0x400 && event_trigger(ev) != 1) ev = -1;
                            if (ev >= 0) use_yaw = y, use_x = px, use_z = pz, use_reach = reaches[r];
                        }
                memcpy(GPTR(rec), saved, sizeof saved);
                if (ev < 0) {
                    gaze_none = 1;
                    none_yaw = gaze_yaw, none_her = saved[5];
                    none_at[0] = saved[0], none_at[1] = saved[2], none_at[2] = gaze_at[0], none_at[3] = gaze_at[2];
                }
                if (ev >= 0) {
                    /* the record as the game's own check reads it: her place (or the head's), facing the found way;
                     * the head's place only for the check: she is put back at her own after it (a head leaning up to
                     * 0.7 m out, into a wall even, never moves her) */
                    if (use_x != saved[0] || use_z != saved[2]) {
                        gaze_from[0] = saved[0];
                        gaze_from[1] = saved[2];
                        gaze_used[0] = use_x;
                        gaze_used[1] = use_z;
                        gaze_moved = 1;
                    }
                    MEMF(rec) = use_x;
                    MEMF(rec + 8) = use_z;
                    MEMF(rec + 0x14) = use_yaw;
                    MEMF(rec + 0x20) = use_x + sinf(use_yaw) * 150.0f;
                    MEMF(rec + 0x28) = use_z + cosf(use_yaw) * 150.0f;
                    MEMF(rec + 0x30) = sinf(use_yaw) * use_reach;
                    MEMF(rec + 0x38) = cosf(use_yaw) * use_reach;
                    static int told = 0;
                    if (told++ < 40)
                        rt_log("VR: frame %d: the action button (pass %u) found event %d where the player looks: %.0f "
                               "degrees from her facing, %.2f m out%s", rt_frame, pass, ev,
                               fabsf(remainderf(use_yaw - saved[5], 6.2831853f)) * 57.29578f, use_reach / 490.0f,
                               use_x != saved[0] || use_z != saved[2] ? ", from the head's place" : "");
                }
            } else {
                memcpy(GPTR(rec), saved, sizeof saved);
            }
        }
    }
    sub_004B9940_gen();
    if (lent) keys_settle(lent, (int)g_eax);
    /* 0.1.7 (session 28, the rig's vr335: A at the hall's far door found nothing and said nothing): a press that finds
     * nothing, along her facing nor where the player looks, is logged with the ways and places it tried (a player's log
     * then tells a door the check never reached from one it reached), at most 20 times, once a second */
    if (gaze_none && (int)g_eax < 0) {
        static int told = 0, told_at = -100;
        if (told < 20 && rt_frame - told_at >= 60) {
            told++;
            told_at = rt_frame;
            rt_log("VR: frame %d: the action button (pass %u) found nothing along her facing (%.0f degrees) nor where the "
                   "player looks (%.0f degrees and 15/30/45 either side, 1.02 and 1.33 m) from her place %.0f %.0f "
                   "(the head's %.0f %.0f)%s", rt_frame, pass, none_her * 57.29578f, none_yaw * 57.29578f, none_at[0],
                   none_at[1], none_at[2], none_at[3], told == 20 ? "; no more of these lines" : "");
        }
    }
    /* her own place back, unless the game's check moved her itself (then its place stands) */
    if (gaze_moved) {
        const int kept = MEMF(rec) == gaze_used[0] && MEMF(rec + 8) == gaze_used[1];
        if (kept) {
            MEMF(rec) = gaze_from[0];
            MEMF(rec + 8) = gaze_from[1];
        }
        static int told_back = 0;
        if (told_back++ < 20)
            rt_log("VR: frame %d: the action button's check from the head's place (%.0f cm from hers): %s", rt_frame,
                   hypotf(gaze_used[0] - gaze_from[0], gaze_used[1] - gaze_from[1]) / 4.9f,
                   kept ? "her own place put back" : "the game moved her itself: its place kept");
    }
    if (!hand) {
        /* 0.1.7 (a player on 0.1.6: "Sometimes it's not very clear if one has properly interacted with a door or
         * object ... Perhaps adding feedback on successful interaction could fix this?"): the action button's check
         * (mode set) found something the game now uses (a door, an item, a thing to examine): a tick and a buzz in the
         * hand, once a press (a hand's own use has its own, sh3vr_hand_action_found) */
        static int fed_at = -100;
        if (sh3vr_active() && rec && mode && kind == 0 && (int)g_eax >= 0 && rt_frame - fed_at > 30) {
            fed_at = rt_frame;
            sh3vr_button_action_found((int)g_eax, (int)pass);
        }
        return;
    }
    int ev = (int)g_eax;
    if (ev < 0 && pass == 0 && mode && kind == 0) {
        /* what 0x4B9D60 asks next, asked here first to know whether the hand found a floor item (the stack as the
         * return left it: its address already popped; build 139 put it back and crashed the caller) */
        const uint32_t keep = g_eax, after = g_esp;
        ev = (int)guest_call(0x4B74B0u, 1, &rec, 0);
        g_esp = after;
        g_eax = keep;
    }
    sh3vr_hand_action_found(ev);
}

/* LIST 6/33: the game's 3D pictures (three render-target textures and their depth, 0x72BFC0-0x72BFCC, the size at
 * 0x72BFD0 / 0x72BFD4) are 512x512 from start-up (0x4028C0 -> 0x402B60(512, 512)) until the player picks a
 * resolution in the game's own options (0x5E4280 -> 0x402E00(w, h), which remakes them): the world was drawn at
 * 512x512 and stretched to each eye (runs vr29 / rt1: all 341 draws of the frame camera went to 512x512 with the
 * eyes at 1000x1000). Both take the size the game draws at instead: the eye's in VR, sh3.ini's flat.
 * LIST 51 (dynamic resolution): in VR the 3D pictures may be drawn smaller than the eye while the headset falls behind
 * (sh3vr.cpp's scale); the game's last pass (0x41C090) stretches them over each eye as it did the 512x512 ones, and the
 * 2D, the menus and VR's hands stay at the eye's size. pic_w / pic_h: the size they are to be (0: the game's size). */
static uint32_t pic_w, pic_h;
static void game_picture_size(const char* who) {
    if (!rt_res_w || !rt_res_h) return;
    const uint32_t w = ARG(0), h = ARG(1);
    const uint32_t want_w = pic_w ? pic_w : rt_res_w, want_h = pic_h ? pic_h : rt_res_h;
    if (w != want_w || h != want_h)
        rt_log("3D pictures: %ux%u (the game asked %ux%u, %s)", want_w, want_h, w, h, who);
    MEM32(g_esp + 4) = want_w;
    MEM32(g_esp + 8) = want_h;
}
/* After each frame is given to the headset (d3d8.c dev_Present, once VR has used the frame's depth): the 3D pictures
 * made again through the game's own resolution change (0x402E00: it frees them, 0x402AB0, and makes them at the new
 * size, 0x402CB0, going back to the old size if it cannot) when dynamic resolution asks another size. */
void sh3_vr_after_present(void) {
    if (!sh3vr_active() || !rt_res_w || !rt_res_h) return;
    const uint32_t cur_w = MEM32(0x72BFD0), cur_h = MEM32(0x72BFD4);
    if (!cur_w || !cur_h) return; /* not made yet (start-up makes them) */
    const float s = sh3vr_picture_scale();
    uint32_t w = (uint32_t)(rt_res_w * s + 0.5f) & ~7u, h = (uint32_t)(rt_res_h * s + 0.5f) & ~7u;
    if (s >= 0.999f) w = rt_res_w, h = rt_res_h;
    if (w < 64 || h < 64 || (w == cur_w && h == cur_h)) return;
    pic_w = w, pic_h = h;
    const uint32_t args[2] = {w, h};
    /* 0x402E00 answers 0 when the pictures stand made (at the new size, or the old one again after a failure) and 1
     * when even that failed (it tried 256x256 last): what stands is read back from 0x72BFD0/D4 (vr158: the log said
     * "could not" for every change that worked, taking the 0 for a failure). */
    const uint32_t trouble = guest_call(0x402E00u, 2, args, 0);
    const int made = MEM32(0x72BFD0) == w && MEM32(0x72BFD4) == h;
    rt_log("3D pictures: frame %d: %ux%u for dynamic resolution %.0f%% (were %ux%u)%s", rt_frame, MEM32(0x72BFD0),
           MEM32(0x72BFD4), s * 100.0f, cur_w, cur_h,
           made ? "" : trouble ? ": the game could not make them at either size" : ": the game kept the old size");
    if (!made) pic_w = MEM32(0x72BFD0), pic_h = MEM32(0x72BFD4);
}
extern void sub_00402B60_gen(void);
void sub_00402B60(void) {
    game_picture_size("start-up");
    sub_00402B60_gen();
}
extern void sub_00402E00_gen(void);
void sub_00402E00(void) {
    game_picture_size("a resolution change");
    sub_00402E00_gen();
}

/* LIST 46: Heather's attack decision 0x494C40(weapon, ?, state, loaded rounds) (cdecl; from 0x4ECC50) answers 3 for
 * "reload" (the gun empty, rounds in reserve: the reload animation, whose event 0x4B5550 runs 0x497860), 2 for a shot
 * or, empty with nothing left, the dry click; 1, 4, 5 other actions. Logged as it changes. */
extern void sub_00494C40_gen(void);
void sub_00494C40(void) {
    const int weapon = (int)(ARG(0) & 0xFF), loaded = (int)(int16_t)(ARG(3) & 0xFFFF);
    sub_00494C40_gen();
    static int last = -1;
    int r = (int)(g_eax & 0xFF), held_back = 0;
    /* "Manual reload" (LIST 46): the game's own reload is not started while VR's hands reload; the answer is the one
     * the function gives with no reserve (docs/GAME_FACTS.md, Weapons: the class table at 0x494CD8): the handgun's 1, or
     * 2 when byte 0x712C65F is 1; the long guns' 2. The rounds stay where they are until the hands commit them. */
    if (r == 3 && sh3vr_active() && sh3vr_block_game_reload()) {
        const int cls = weapon <= 0x12 ? MEM8(0x494CD8u + (uint32_t)weapon) : -1;
        r = cls == 1 ? (MEM8(0x712C65F) == 1 ? 2 : 1) : 2;
        g_eax = (g_eax & ~0xFFu) | (uint32_t)r;
        held_back = 1;
    }
    if (r + 10 * held_back != last) {
        last = r + 10 * held_back;
        rt_log("VR: the attack decision for weapon %d with %d loaded: %d%s", weapon, loaded, r,
               held_back ? " (the game's reload held back: the hands reload)" : r == 3 ? " (the game's reload)" : "");
    }
}

/* LIST 46: the game's reload event 0x4B5550(record) (cdecl; docs/GAME_FACTS.md, Weapons): [record] the character,
 * [record + 6] the motion event's index (its sound: 0x5680F0(chr, chr + 0x60, 0, index)), [record + 8] the offset of its
 * done flag at chr + 0x168. When it reloads Heather's gun, the index is logged and given to VR, which plays the same
 * sound for a reload by hand. */
extern void sub_004B5550_gen(void);
void sub_004B5550(void) {
    const uint32_t rec = ARG(0);
    const uint32_t chr = rec ? MEM32(rec) : 0;
    const uint32_t flag_at = chr ? chr + MEM32(rec + 8) + 0x168 : 0;
    const uint8_t before = flag_at ? MEM8(flag_at) : 1;
    sub_004B5550_gen();
    if (flag_at && before == 0 && MEM8(flag_at) == 2 && chr == vr_player()) {
        const int w = MEM8(0x712C656), index = MEM16(rec + 6);
        const int item = w > 0 && w < 32 ? (int16_t)MEM16(0x6BDD24u + 2u * (uint32_t)w) : 0;
        rt_log("VR: the game's reload of weapon %d (item %d): motion event %d (its sound); %d loaded, %d in reserve", w,
               item, index, item >= 9 && item <= 13 ? MEM16(0x712CA8Eu + 2u * (uint32_t)item) : -1,
               item >= 9 && item <= 13 ? MEM16(0x712CA8Eu + 2u * (uint32_t)(item + 5)) : -1);
        if (sh3vr_active()) sh3vr_game_reload(w, index);
    }
}

static void set_heather_yaw(uint32_t p, float yaw) {
    uint32_t args[2];
    memcpy(&args[1], &yaw, 4);
    args[0] = p;
    guest_call(0x4737F0, 2, args, 0);  /* SetCharacterYaw(chr, yaw), cdecl */
    MEMF(p + 0xE4) = yaw;              /* the simulation's working yaw (the PC VR mod sets it before movement) */
}

/* The movement vector (0x4D83B0, called by the normal and the aiming locomotion, return addresses 0x4D4018 /
 * 0x4EAE91) before slope and collision: in VR's first person it goes along the stick's direction (the head's or the
 * left hand's), or towards the head when the player walked in the room; the game's slope, collision (walls hold) and
 * integration run unchanged. The PC VR mod's camera-relative movement, with its speed floor: in some rooms the
 * ordinary caller arrives with a zero vector though the stick is held. */
extern void sub_004D83B0_gen(void);
/* testing (session 12, LIST 45: vr189's straight walk to a door stopped at a railing): SH3_STEER_TO=x,z[;x,z...]
 * (up to 24) in game units. While VR steers her (the stick held), her walk heads for each point in turn, the next once
 * within 100 units (20 cm); after the last she stands. Stuck (under 25 units in 40 frames) it walks 70 degrees
 * left of the line for 45 frames, then right, then 90 left, 90 right and so on (a wall's edge found); each point
 * reached and each try logged with her place. 0: no switch (VR's heading); 1: steered; -1: the last point
 * reached (she stands). */
/* testing (session 15, LIST 27: melee on a monster, never yet seen in a run): SH3_STEER_AT_ATTACKER=1: once a monster
 * has hit her (sub_004B6A20 keeps the attacker), while it lives (its health +0x180 over 0) and is within 8 m, she
 * walks to 0.7 m short of it along her facing (her facing is the head's) and stands there, so it is right ahead of a
 * forward swing; logged every 60 frames with its place, distance and health. 0: no attacker to go for.
 * Session 16 (vr230: the hall's attacker, kinds 120/121, held her at 0.82 m all run, health 712.5 kept, and no
 * picture showed a monster there): she stands 0.5 m short of it now (245 units, within 40), its struct's first and
 * health words are logged once (what it is), and its place goes to the combat's sweep log (sh3_rig_attacker_at). */
static uint32_t rig_attacker;
float sh3_rig_attacker_at[4];  /* testing: the kept attacker's place (w 1 while kept), for sh3vr_combat's sweep log */
/* testing (0.1.7, the PC rig): one sweep of her knife across a character, 35 cm over its place, from side to side as
 * seen from her, sent through the game's own battle step as a swing's contact sweep goes (sh3vr_test_sweep) */
static void rig_sweep_across(uint32_t p, uint32_t a, const char *why) {
    const float cx = MEMF(a), cy = MEMF(a + 4) - 0.35f * 490.0f, cz = MEMF(a + 8);
    float dx = cx - MEMF(p), dz = cz - MEMF(p + 8);
    const float d = sqrtf(dx * dx + dz * dz);
    if (d > 1.0f) dx /= d, dz /= d;
    else dx = 0.0f, dz = 1.0f;
    const float sx = -dz, sz = dx, back = 0.3f * 490.0f, side = 0.4f * 490.0f;
    const float pg[3] = {cx - dx * back + sx * side, cy, cz - dz * back + sz * side};
    const float pt[3] = {cx + dx * back + sx * side, cy, cz + dz * back + sz * side};
    const float cg[3] = {cx - dx * back - sx * side, cy, cz - dz * back - sz * side};
    const float ct[3] = {cx + dx * back - sx * side, cy, cz + dz * back - sz * side};
    rt_log("%s: frame %d: %08X (health %.1f) at %.0f %.0f %.0f, %.2f m from her: a knife sweep across it", why, rt_frame,
           a, MEMF(a + 0x180), MEMF(a), MEMF(a + 4), MEMF(a + 8), d / 490.0f);
    sh3vr_test_sweep(pg, pt, cg, ct, 0.1f * 490.0f);
}
static int steer_at_attacker(uint32_t p, float *heading) {
    static int on = -1, logged_at = -1000;
    static uint32_t described;
    if (on < 0) on = getenv("SH3_STEER_AT_ATTACKER") ? atoi(getenv("SH3_STEER_AT_ATTACKER")) : 0;
    const uint32_t a = rig_attacker;
    sh3_rig_attacker_at[3] = 0.0f;
    /* 0.1.7 (vr308-310: a second monster at the hall's far door drew her away from the door A was tried at): a value
     * over 1 is the last game frame she goes for an attacker */
    if (!on || !a || a == p || (on > 1 && rt_frame > on)) return 0;
    const float ax = MEMF(a), az = MEMF(a + 8), hp = MEMF(a + 0x180), x = MEMF(p), z = MEMF(p + 8);
    const float yaw = MEMF(p + 0x14), dx = ax - x, dz = az - z, d = sqrtf(dx * dx + dz * dz);
    if (!isfinite(d) || !(hp > 0.0f) || d > 8.0f * 490.0f) {
        /* always logged (vr233: the dog's death at frame 9817 fell inside the 60 frames and went unlogged) */
        logged_at = rt_frame;
        rt_log("steer test: frame %d: the attacker %08X let go (health %.1f, %.1f m away)", rt_frame, a, hp,
               d / 490.0f);
        rig_attacker = 0;
        return 0;
    }
    sh3_rig_attacker_at[0] = ax, sh3_rig_attacker_at[1] = MEMF(a + 4), sh3_rig_attacker_at[2] = az;
    sh3_rig_attacker_at[3] = 1.0f;
    if (described != a) {
        described = a;
        for (uint32_t o = 0; o < 0x200; o += 0x40)
            rt_log("steer test: the attacker %08X +%03X: %08X %08X %08X %08X %08X %08X %08X %08X %08X %08X %08X %08X "
                   "%08X %08X %08X %08X", a, o, MEM32(a + o), MEM32(a + o + 4), MEM32(a + o + 8), MEM32(a + o + 12),
                   MEM32(a + o + 16), MEM32(a + o + 20), MEM32(a + o + 24), MEM32(a + o + 28), MEM32(a + o + 32),
                   MEM32(a + o + 36), MEM32(a + o + 40), MEM32(a + o + 44), MEM32(a + o + 48), MEM32(a + o + 52),
                   MEM32(a + o + 56), MEM32(a + o + 60));
        rt_log("steer test: Heather %08X for comparison: +000 %08X %08X %08X %08X %08X %08X %08X %08X", p, MEM32(p),
               MEM32(p + 4), MEM32(p + 8), MEM32(p + 12), MEM32(p + 16), MEM32(p + 20), MEM32(p + 24), MEM32(p + 28));
    }
    const float tx = ax - sinf(yaw) * 245.0f, tz = az - cosf(yaw) * 245.0f, ex = tx - x, ez = tz - z;
    if (rt_frame - logged_at >= 60) {
        logged_at = rt_frame;
        rt_log("steer test: frame %d: the attacker %08X at %.0f %.0f %.0f (%.2f m, %.0f deg right of her facing), "
               "health %.1f of %.1f; her place %.0f %.0f %.0f",
               rt_frame, a, ax, MEMF(a + 4), az, d / 490.0f,
               fmodf((atan2f(dx, dz) - yaw) * 57.29578f + 540.0f, 360.0f) - 180.0f, hp, MEMF(a + 0x184), x,
               MEMF(p + 4), z);
    }
    {
        /* testing (0.1.7, vr317: the script's knife swings went 1.29 m over the hall's low dog, which then kept her
         * from the far door): SH3_SWEEP_ATTACKER=N sends a sweep of her knife across the attacker every N frames while
         * it is within 1.5 m, through the game's own hits (its damage, "Melee damage" and all) */
        static int every = -2, sent_at = -100000;
        if (every == -2) every = getenv("SH3_SWEEP_ATTACKER") ? atoi(getenv("SH3_SWEEP_ATTACKER")) : 0;
        if (every > 0 && d < 1.5f * 490.0f && rt_frame - sent_at >= every) {
            sent_at = rt_frame;
            rig_sweep_across(p, a, "steer test");
        }
    }
    if (ex * ex + ez * ez < 40.0f * 40.0f) return -1;  /* in front of it: stand and swing */
    *heading = atan2f(ex, ez);
    return 1;
}

static int steer_rig(uint32_t p, float *heading) {
    static int n = -1, at, window, tries, try_frames;
    static float pts[24][2], last_x, last_z;
    const int chase = steer_at_attacker(p, heading);
    if (chase) return chase;
    if (n == -1) {
        n = 0;
        for (const char *e = getenv("SH3_STEER_TO"); e && *e && n < 24;) {
            if (sscanf(e, "%f,%f", &pts[n][0], &pts[n][1]) != 2) break;
            n++;
            e = strchr(e, ';');
            if (!e) break;
            e++;
        }
        if (n) rt_log("steer test: %d points, the first %.0f %.0f", n, pts[0][0], pts[0][1]);
    }
    if (!n) return 0;
    const float x = MEMF(p), z = MEMF(p + 8);
    /* a point she cannot stand on (vr225: point 4 behind a step, stuck 254 units off it for 1300 frames) is passed:
     * after 4 stuck tries within 400 units of it, or 8 anywhere */
    while (at < n) {
        const float dx = pts[at][0] - x, dz = pts[at][1] - z, d2 = dx * dx + dz * dz;
        const int passed = !try_frames && (tries >= 8 || (tries >= 4 && d2 < 400.0f * 400.0f));
        if (d2 >= 100.0f * 100.0f && !passed) break;
        rt_log("steer test: frame %d: point %d (%.0f %.0f) %s at %.0f %.0f, facing %.1f deg", rt_frame, at + 1,
               pts[at][0], pts[at][1], passed && d2 >= 100.0f * 100.0f ? "passed (cannot reach it)" : "reached", x, z,
               MEMF(p + 0x14) * 57.29578f);
        at++, tries = 0, try_frames = 0;
    }
    if (at >= n) return -1;
    /* a point over 40 m away is in the next area: she stands (at a door, for the hand) until it takes her there */
    const float fx = pts[at][0] - x, fz = pts[at][1] - z;
    if (fx * fx + fz * fz > 20000.0f * 20000.0f) {
        static int far_logged = -1;
        if (far_logged != at) {
            far_logged = at;
            rt_log("steer test: frame %d: point %d (%.0f %.0f) is in another area: standing at %.0f %.0f", rt_frame,
                   at + 1, pts[at][0], pts[at][1], x, z);
        }
        window = 0, last_x = x, last_z = z;
        return -1;
    }
    float h = atan2f(pts[at][0] - x, pts[at][1] - z);  /* forward is (sin yaw, cos yaw) */
    if (++window >= 40) {
        const float mx = x - last_x, mz = z - last_z;
        if (mx * mx + mz * mz < 25.0f * 25.0f && !try_frames) {
            tries++;
            try_frames = 45;
            rt_log("steer test: frame %d: stuck at %.0f %.0f going for point %d: %d degrees %s of the line", rt_frame,
                   x, z, at + 1, 70 + 20 * ((tries - 1) / 2), tries & 1 ? "left" : "right");
        }
        last_x = x, last_z = z, window = 0;
    }
    if (try_frames > 0) {
        try_frames--;
        const float off = (70.0f + 20.0f * (float)((tries - 1) / 2)) * 3.14159265f / 180.0f;
        h += tries & 1 ? -off : off;  /* a snap turn right raises her yaw (vr189): left is lower */
    }
    *heading = h;
    return 1;
}

/* 0.1.7 (a player's report on 0.1.6: "If you are moving and press "a" to interact with anything on the environment the
 * character keeps moving on the same direction you were last facing, even if you stop all your inputs, you need to
 * press "a" again to stop examining"): VR writes her walk's velocity (+0xF0/+0xF8, which 0x4D83B0 applies) each frame
 * it steers; the game's own walk and stand set it from her animation each frame in play, but an examine (the action
 * button's message: the playable update stops, so VR stops steering) sets nothing, and the last step's velocity carried
 * her on through the whole message. Now the frame VR's steering stops she is stopped once (in play her stand sets its
 * own the next frame, as before). */
static int vr_walked;
/* 0.1.7 (owner 140, the PC VR mod's maker: "if you spam motion controls like knife stabs, game animations would break
 * and i can't do anything besides only walking and even enemies won't attack me"; their mod's fix: no heading or
 * walking writes while she is knocked down or getting up). Run vr347 (stabs at the hall's dog with the stick held): the
 * dog's grab left her held for good, reaction 9 (the grab's start) never moving on to 10 (the struggle), no bar. The
 * grab waits for its animation (206) to end on her legs' channel (0x4D9F59: event 0x10, which 0x4C6EE0 raises when
 * 0x4746C0 finds the channel at its last frame). The reaction's start sets her walk factor +0x11C to 0 (0x4DAB8B); VR's
 * walk set it each frame (1.8 walking), and while it is not 0 the movement step 0x4D83B0 re-times her legs' animation
 * from her walk's speed (0x474580, her body's too when the action and the movement are one state, as in a reaction):
 * the grab's animation never reached its end. So while the game has her in a hit reaction (her movement state 34 or 35,
 * which run her reaction step 0x4D9E60, or 36, which 0x4C7190 gives a reaction animation as 35) VR writes neither her
 * walk (+0xF0/+0xF8, +0x11C) nor her facing: the game moves and turns her itself, as it does on the PC. */
static int her_reacting(void) {
    const uint32_t m = MEM32(0x712C190u);
    return m == 34u || m == 35u || m == 36u ? (int)m : 0;
}
/* 0.1.8 (owner 152, a player: "I took the key for the bookstore. But when I went back to the book store, the key is
 * not in my inventory"; another: "she keeps walking after interacting ... if you are moving at all when you
 * interact"): the game's own gestures. A floor item's pick-up (0x4B8040) has her bend down (0x496240(5)), waits
 * for that animation's grab frame (0x496440: her movement 0x28 with 0x712C19E set) before it shows the item's
 * message and adds the item, then has her stand up (6); other events and the game's own reload (0x4979D0, gesture
 * 0x0F) play theirs the same way: the request 0x712C64A, played as her action and movement 0x28. VR's walk wrote her
 * velocity and walk factor (+0x11C) each frame it steered, and while that factor is not 0 the movement step
 * re-times her animation from her walk's speed (0x474580: vr347's stalled grab, above). A walk into an item, the
 * stick still held or just let go, could leave her bend never reaching its grab: the item never added and the
 * event waiting on it. While a gesture plays VR neither walks nor turns her, and her walk's velocity and factor are
 * cleared as it starts: the game plays it at its own pace, as on the PC, where the stick does not move her then. */
static int her_gesture(void) {
    static int off = -1;  /* testing: SH3_NO_GESTURE_HOLD=1 as 0.1.7 (the rig's comparison) */
    if (off < 0) off = getenv("SH3_NO_GESTURE_HOLD") ? 1 : 0;
    if (off) return 0;
    return MEM32(0x712C18Cu) == 0x28u || MEM32(0x712C190u) == 0x28u ? 1 : 0;
}
/* 0.1.8 (owner 152; the 0.1.7 player's video: "The lock is broken. I can't open the door." stayed up for 25 s while
 * she walked about the hall, and "she keeps walking after interacting ... if you are moving at all when you
 * interact"): an event of the game's running (event_state not 0: an examine's message up, a door, a pick-up, a scene
 * in the room) while the game does not walk her itself (her movement not its walk 0x0A or run 0x0E). The game takes
 * the stick from her then (on the PC she stands through an examine's message, the stick held or not), but VR's walk
 * wrote her velocity whenever the game's mode was 5, which it is through its scripts too. Now VR neither walks nor
 * turns her then; the pad still carries the stick, so where the game lets her walk during an event of its own it
 * walks her and VR steers as always. SH3_EVENT_HOLD=2 (testing) holds her through every event, walking or not;
 * SH3_EVENT_HOLD=0 as 0.1.7 (the rig's comparison). */
static int event_hold(void) {
    static int how = -1;
    if (how < 0) how = getenv("SH3_EVENT_HOLD") ? atoi(getenv("SH3_EVENT_HOLD")) : 1;
    if (!how || !event_state) return 0;
    const uint32_t m = MEM32(0x712C190u);
    return how == 2 || (m != 0x0Au && m != 0x0Eu) ? 1 : 0;
}
/* 0.1.8 (owner 152, a player: "I took the key for the bookstore. But when I went back to the book store, the key is
 * not in my inventory"): a floor item's pick-up (0x4B8040, the dispatch's step 3; its record at 0xBA95D0: +0x24 its
 * own step, +0x2C set for an item low down) has her bend down (gesture 5) and waits for that animation's grab frame
 * (0x496440: her movement 0x28 with 0x712C19E set) before it shows the item's message and adds the item; standing up
 * (gesture 6) it waits the same way. An animation cut short before its grab frame (anything that changed her movement
 * from the gesture's) left it waiting for good: no message, no item, the action button dead (the dispatch never back
 * at step 0). If her movement has not been the gesture's for 90 of the dispatch's steps (1.25 s at 72) while the
 * pick-up waits on it, the gesture is asked again (the game's own 0x496240), so the pick-up ends the game's way, the
 * item in the inventory. */
static void pickup_watch(void) {
    static int stalled, told;
    const uint32_t step = MEM32(0xBA95F4u);
    const int waiting = event_state == 3u && MEM32(0xBA95FCu) && (step == 2u || step == 5u);
    if (!waiting || MEM32(0x712C190u) == 0x28u || her_reacting()) {
        stalled = 0;
        return;
    }
    if (++stalled < 90) return;
    stalled = 0;
    uint32_t gesture = step == 2u ? 5u : 6u;
    if (told++ < 20)
        rt_log("VR: frame %d: a floor item's pick-up (item %u) waited 90 steps on her %s, her movement %u, not the "
               "gesture's: the gesture asked again", rt_frame, MEM16(0xBA95D0u),
               step == 2u ? "bending down" : "standing up", MEM32(0x712C190u));
    guest_call(0x496240u, 1, &gesture, 0);
}
void sub_004D83B0(void) {
    Sh3VrMove mv;
    uint32_t p = sh3vr_active() ? vr_player() : 0;
    if (p) {
        sh3vr_move(&mv);
        const int reacting = her_reacting();
        if (reacting) mv.steer = 0, mv.face = 0;
        /* 0.1.8 (owner 152): her gesture (her_gesture) or an event of the game's holding her (event_hold): no walk, no
         * turn; the start of either clears her walk once */
        {
            static int was_gesture, told_gesture, was_event, told_event;
            const int gesture = her_gesture(), held = event_hold();
            if (gesture || held) mv.steer = 0, mv.face = 0;
            if ((gesture && !was_gesture) || (held && !was_event)) {
                if (gesture && !was_gesture && told_gesture++ < 40)
                    rt_log("VR: frame %d: her gesture (action %u, movement %u, request %u): VR's walk and turn wait, her "
                           "walk's velocity %.2f %.2f and factor %.2f cleared", rt_frame, MEM32(0x712C18Cu),
                           MEM32(0x712C190u), MEM16(0x712C64Au), MEMF(p + 0xF0), MEMF(p + 0xF8), MEMF(p + 0x11C));
                if (held && !was_event && told_event++ < 200)
                    rt_log("VR: frame %d: the game's event step %u (event %d) holds her (her movement %u): VR's walk "
                           "and turn wait, her walk's velocity %.2f %.2f and factor %.2f cleared", rt_frame, event_state,
                           (int)MEM32(0xBA9768u), MEM32(0x712C190u), MEMF(p + 0xF0), MEMF(p + 0xF8), MEMF(p + 0x11C));
                MEMF(p + 0xF0) = 0.0f;
                MEMF(p + 0xF8) = 0.0f;
                MEMF(p + 0x11C) = 0.0f;
                vr_walked = 0;
            }
            if (!gesture && was_gesture && told_gesture < 40) {
                told_gesture++;
                rt_log("VR: frame %d: her gesture over (action %u, movement %u)", rt_frame, MEM32(0x712C18Cu),
                       MEM32(0x712C190u));
            }
            if (!held && was_event && told_event < 200) {
                told_event++;
                rt_log("VR: frame %d: the event's hold over (the game's event step %u, her movement %u)", rt_frame,
                       event_state, MEM32(0x712C190u));
            }
            was_gesture = gesture;
            was_event = held;
        }
        const int rig = mv.steer ? steer_rig(p, &mv.heading) : 0;
        if (!mv.steer && vr_walked) {
            static int told_play, told_out, told_hit;
            const int playing = sh3vr_in_play();
            if (reacting ? told_hit++ < 10 : playing ? told_play++ < 2 : told_out++ < 10)
                rt_log("VR: frame %d: walking stopped (%s): her velocity %.2f %.2f %s", rt_frame,
                       reacting ? "her hit reaction: the game moves her"
                       : playing ? "the stick let go" : "the game is not in play: a message, an examine or a menu",
                       MEMF(p + 0xF0), MEMF(p + 0xF8), reacting ? "left to the game" : "cleared");
            if (!reacting) {  /* a reaction sets its own (from its +0x11C, 0x4EAE19) */
                MEMF(p + 0xF0) = 0.0f;
                MEMF(p + 0xF8) = 0.0f;
            }
        }
        vr_walked = mv.steer && rig >= 0;
        if (rig < 0) {
            MEMF(p + 0xF0) = 0.0f;
            MEMF(p + 0xF8) = 0.0f;
        } else if (mv.steer) {
            float fx = MEMF(p + 0xF0), fz = MEMF(p + 0xF8), speed = sqrtf(fx * fx + fz * fz);
            float secs = MEMF(0x070E67AC);
            if (!isfinite(secs) || secs < 0.001f || secs > 0.1f) secs = 1.0f / 72.0f;
            float factor = mv.run ? 3.6f : 1.8f;
            float floor_v = secs * factor * (mv.speed > 0.3f ? mv.speed : 0.3f);
            if (floor_v < 0.004f) floor_v = 0.004f;
            if (floor_v > 0.05f) floor_v = 0.05f;
            if (!(speed >= floor_v) || !isfinite(speed)) speed = floor_v;
            MEMF(p + 0x11C) = factor;  /* the gait and footsteps follow the same walk/run factor */
            set_heather_yaw(p, mv.face ? mv.face_yaw : mv.heading);  /* her facing eased (VR), the walk as steered */
            MEMF(p + 0xF0) = sinf(mv.heading) * speed;
            MEMF(p + 0xF8) = cosf(mv.heading) * speed;
        }
    }
    sub_004D83B0_gen();
}

/* The weapon's place (0x4954E0(origin[4], dir[4], mode), cdecl; docs/GAME_FACTS.md, Combat): the game's weapon object
 * (0x46A8F0(0x800 + the drawn weapon)) gives mode 0 its bone 0's origin and the way to bone 1, mode 1 bone 1's origin
 * and its +x (tilted per weapon), both through the object's world matrix. Heather's shots (0x4B3D50), the muzzle flash
 * and its light (0x4959A0, 0x5A47D0) and the other weapon effects (0x466B70, 0x5C2F80) all take the gun's place from
 * it. In VR's first person the weapon is drawn at the controller, not in her animated hand: both outputs are moved by
 * the same map the weapon's draw used (sh3vr_weapon_to_hand), so every one of them follows the gun in the hand. */
extern void sub_004954E0_gen(void);
static int last_shot_frame = -100;  /* rt_frame of Heather's last shot (sub_004B3D50) */
int sh3_first_shot_frame = -1;      /* rt_frame of her first shot (testing: d3d8_shader.c's SH3_VSC_DUMP_SHOT) */
void sub_004954E0(void) {
    const uint32_t org = ARG(0), dir = ARG(1), mode = ARG(2), from = MEM32(g_esp);
    sub_004954E0_gen();
    if (!sh3vr_active() || !org || !dir) return;
    float o[3] = {MEMF(org), MEMF(org + 4), MEMF(org + 8)}, d[3] = {MEMF(dir), MEMF(dir + 4), MEMF(dir + 8)};
    const float o0[3] = {o[0], o[1], o[2]};
    const int moved = sh3vr_weapon_to_hand(o, d, mode == 0);  /* mode 0 reads her hand bone, 1 the weapon's matrix */
    {
        /* testing (session 6): each caller once, and every call for 8 frames after a shot: where the flash, its light
         * and the other effects took the gun's place from, before and after VR moved it to the gun in the hand */
        static uint32_t seen[16];
        static int n_seen, logged;
        int known = 0;
        for (int i = 0; i < n_seen; i++) known |= seen[i] == from;
        if (!known && n_seen < 16) seen[n_seen++] = from;
        if ((!known || rt_frame - last_shot_frame <= 8) && logged++ < 80)
            rt_log("VR: frame %d: weapon place for %08X mode %u: %.1f %.1f %.1f -> %s %.1f %.1f %.1f along %.2f %.2f %.2f",
                   rt_frame, from, mode, o0[0], o0[1], o0[2], moved ? "moved to" : "kept", o[0], o[1], o[2], d[0], d[1],
                   d[2]);
    }
    if (!moved) return;
    MEMF(org) = o[0], MEMF(org + 4) = o[1], MEMF(org + 8) = o[2];
    MEMF(dir) = d[0], MEMF(dir + 4) = d[1], MEMF(dir + 8) = d[2];
}

/* attach_weapon(weapon_obj, chr_type) (0x495360, cdecl; called each frame by 0x473E40 for Heather, chr_type 0x100,
 * with 0x46A8F0(0x800 + the drawn weapon)): puts the weapon model's world matrix (the D3D 4x4 at
 * [[obj+0x70]+4]+8, its place row copied to +0x48; in her own space: the world place is that times her world matrix,
 * which it copies to obj+0x20, as 0x4954E0 reads it) at her animated hand bone (0x4C7660(.., 0x29)). In VR's first
 * person that matrix is moved to the gun in the VR hand (sh3vr_weapon_world), so everything the game projects or
 * places from the weapon itself (the muzzle flash quads and smoke, which it draws in screen space; run vr62 had them
 * in the left palm, where her two-hand aim holds the gun) starts at the gun in the hand. */
extern void sub_00495360_gen(void);
void sub_00495360(void) {
    const uint32_t obj = ARG(0), type = ARG(1);
    sub_00495360_gen();
    if (!sh3vr_active() || !obj || (type & 0xFFFF) != 0x100 || !MEM8(0x712C656)) return;
    const uint32_t x = MEM32(obj + 0x70);
    if (!x) return;
    const uint32_t model = MEM32(x + 4);
    if (!model) return;
    float m[16], base[16];
    for (int i = 0; i < 16; i++) m[i] = MEMF(model + 8 + 4 * i), base[i] = MEMF(obj + 0x20 + 4 * i);
    if (!sh3vr_weapon_world(m, base)) return;
    for (int i = 0; i < 16; i++) MEMF(model + 8 + 4 * i) = m[i];
    for (int i = 0; i < 4; i++) MEMF(model + 0x48 + 4 * i) = m[12 + i];
}

/* build_attack_origin_direction(chr, attack_id, origin[4], dir[4]) (0x4B3D50): Heather's shots in VR's first person
 * go exactly where the gun in the hand points: along its barrel through its muzzle (the guide dot's line). The game's
 * origin (already at the gun in the hand: 0x4954E0 above) is put on that line, so it stays as far behind the muzzle as
 * the game has it (inside its battle space); a spread the caller adds afterwards (the shotgun's) is kept. */
extern void sub_004B3D50_gen(void);
void sub_004B3D50(void) {
    uint32_t chr = ARG(0), org = ARG(2), dir = ARG(3);
    sub_004B3D50_gen();
    Sh3VrMove mv;
    if (!sh3vr_active() || !chr || chr != vr_player() || !dir) return;
    sh3vr_notify_shot();  /* haptics and the recoil shown (VR checks it is a gun) */
    last_shot_frame = rt_frame;
    if (sh3_first_shot_frame < 0) sh3_first_shot_frame = rt_frame;
    sh3vr_move(&mv);
    if (!mv.aim) return;
    float from[3], line[3];
    if (org && sh3vr_shot_line(from, line)) {
        float t = (MEMF(org) - from[0]) * line[0] + (MEMF(org + 4) - from[1]) * line[1] + (MEMF(org + 8) - from[2]) * line[2];
        if (!(t <= 0.0f)) t = 0.0f;       /* never ahead of the muzzle */
        if (t < -0.6f * 490.0f) t = -0.6f * 490.0f;
        MEMF(org) = from[0] + line[0] * t;
        MEMF(org + 4) = from[1] + line[1] * t;
        MEMF(org + 8) = from[2] + line[2] * t;
        MEMF(dir) = line[0];
        MEMF(dir + 4) = line[1];
        MEMF(dir + 8) = line[2];
        static int logged;
        if (logged++ < 6)
            rt_log("VR: frame %d: shot from %.1f %.1f %.1f along %.3f %.3f %.3f (%.1f units behind the muzzle)", rt_frame,
                   MEMF(org), MEMF(org + 4), MEMF(org + 8), line[0], line[1], line[2], -t);
        return;
    }
    float d[3] = {mv.aim_dir[0], mv.aim_dir[1], mv.aim_dir[2]};
    float n = sqrtf(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
    if (!(n > 0.5f)) return;
    MEMF(dir) = d[0] / n;
    MEMF(dir + 4) = d[1] / n;
    MEMF(dir + 8) = d[2] / n;
}

/* The frame limiter (0x41B2C0): while the headset paces the game, on its own clock (kernel32.c), one tick a frame. */
extern _Thread_local int rt_limiter_virtual;
void rt_limiter_virtual_step(void);
extern void sub_0041B2C0_gen(void);
void sub_0041B2C0(void) {
    if (sh3vr_display_hz() > 0) {
        rt_limiter_virtual_step();
        rt_limiter_virtual = 1;
        sub_0041B2C0_gen();
        rt_limiter_virtual = 0;
    } else sub_0041B2C0_gen();
}

/* 0.1.5 (the owner's 0.1.4 report: cutscenes out of step with their voices, "delayed"): the game's step (0x60DA50, from
 * the main loop at 0x5F1A74 each frame, with the step 0x5F15E0 picked from the limiter's ticks: 1, 2 or 3 sixtieths)
 * sets the frame-time tuple the whole update reads (0x070E67A8 the step in sixtieths, 0x070E67AC its seconds,
 * 0x070E67C4 the frames a second, through 0x60D8D0 / 0x60D8E0 / 0x60D8F0; 0x60D8E0 alone has 1692 callers). While the
 * headset paces the game the limiter counts one tick a frame, so the step was always 1/60 s: the game ran at the display's
 * rate over 60 (twice its speed at 120 frames a second, half at 30, slow scenes with their voices ahead). Now the step is
 * the real time from the last picture's display to this one's (sh3vr_frame_seconds, up to 3/60 s): the game's own
 * code first (its 0x5FE970 checks and the whole-step counts at 0x070E67C0 / 0x070E67A4, kept at 1), then the tuple. */
extern void sub_0060DA50_gen(void);
static void vr_time_step(void) {
    const float s = sh3vr_frame_seconds();
    if (!(s > 0)) return;
    const float base = MEMF(0x070E67AC), fps = MEMF(0x070E67C4);  /* the game's for one step: 1/60 s, 60 */
    if (!(base > 0) || !(fps > 0)) return;
    const float scale = s / base;
    MEMF(0x070E67A8) = scale;
    MEMF(0x070E67B4) = scale;
    MEMF(0x070E67AC) = s;
    MEMF(0x070E67C4) = fps / scale;
    static int told;
    static float most;
    static double sum;
    static int n;
    sum += scale;
    n++;
    if (scale > most) most = scale;
    if (n >= 600 || !told) {
        rt_log("VR: frame %d: the game's step is the real time a frame is shown: %.2f sixtieths on average over the last "
               "%d frames (the most %.2f), %.1f game frames a second", rt_frame, sum / n, n, most, 60.0 * n / (sum > 0 ? sum : 1));
        told = 1;
        sum = 0;
        n = 0;
        most = 0;
    }
}
void sub_0060DA50(void) {
    if (sh3vr_frame_seconds() > 0) {
        ARG(0) = 1;  /* the step in whole sixtieths: 1 (cdecl: the argument slot is the callee's) */
        sub_0060DA50_gen();
        vr_time_step();
    } else sub_0060DA50_gen();
}

/* ---- SH3 Quest VR: weapons (docs/GAME_FACTS.md, Weapons; checked in the exe 2026-11-24) ----
 * The drawn weapon (0x712C656) and the equipped slot (0x712CABF) count alike: 0 none, 1 handgun, 2 shotgun,
 * 3 submachine gun, 4 knife, 5 steel pipe, 6 katana, 7 maul, 8 stun gun, 9 / 10 / 13 / 14 / 17 the extras, 15 / 16 / 18
 * the handgun, the submachine gun and 17 in their other mode. A slot's inventory item is short[0x6BDD24 + 2 slot]
 * (0 for none and the unused 11 and 12); the player has item i when bit i of the dwords at 0x712CA80 is set (0x497730).
 * Equipping: 0x497A70(item) (cdecl; item i is the inventory's i-th weapon, short[0x6BDD4C + 2 i] its slot; it picks
 * the other mode itself) sets the slot, then 0x4711D0 (no arguments; the game calls it as it returns to play after
 * loading) frees the old weapon's model, loads the new one (0x46FD40) and sets the drawn weapon (0x495F80). */
static uint32_t vr_weapons_owned(void) {
    uint32_t bits = 0;
    for (int s = 1; s <= 18; s++) {
        int item = (int16_t)MEM16(0x6BDD24u + 2u * s);
        if (item > 0 && item < 64 && (MEM32(0x712CA80u + 4u * (item >> 5)) >> (item & 31) & 1u)) bits |= 1u << s;
    }
    return bits;
}
/* Heather is in the room: not parked at the spot the game keeps her while a stage loads or an event hides her
 * (-60000, 0, 60000; an equip there crashed in the model loader 0x4546A0, the PC rig, run 33). */
static int heather_placed(uint32_t p) {
    return p && !(MEMF(p) == -60000.0f && MEMF(p + 8) == 60000.0f);
}

/* LIST 28: a holster draw is the game's own weapon change, done in play over a few frames as its inventory does when it
 * closes (0x5E1E20; the game has no other way to change weapons):
 *   1. the weapon in her hand put away: 0x497A70(0) asks for none, 0x4711D0 takes her weapon part off (nothing loads);
 *   2. the new one asked (0x497A70(item): 0x4978F0 sets the asked weapon 0x712CACE and slot 0x712CABF), and its model
 *      file loaded by the stage's file step 0x59E3A0, one step a frame until it answers 1;
 *   3. 0x4711D0 makes her weapon part from that file and draws it (0x495F80 sets 0x712C656).
 * 0x4711D0 without step 2 crashed (runs 33, 34: the part's model +0x2CC was 0, 0x46AC70 finding no file, and 0x685F90's
 * copy 0x4546A0 read guest address 0). 0x59E3A0 keeps its step on the game's step stack (0x5F1B50-0x5F1BC0: the depth
 * at 0xBCC658, two arrays of 16 after it, a level's steps kept from frame to frame), so it runs on a stack of its own,
 * swapped in for the call and out after it: the game's own steps are never touched. */
#define STEP_STACK 0xBCC658u
#define STEP_STACK_WORDS 33u
static struct {
    int state;            /* 0 none, 2 loading, 3 drawing */
    int slot, frames;
    uint32_t item;
    uint32_t ctx[STEP_STACK_WORDS];
} swap;

static void swap_run_loader(int *done) {
    uint32_t game_ctx[STEP_STACK_WORDS];
    for (uint32_t w = 0; w < STEP_STACK_WORDS; w++) game_ctx[w] = MEM32(STEP_STACK + 4u * w);
    /* ours: the game's depth and the levels up to it, our own steps above it */
    const uint32_t depth = game_ctx[0] < 15u ? game_ctx[0] : 15u;
    swap.ctx[0] = game_ctx[0];
    for (uint32_t l = 0; l <= depth; l++) swap.ctx[1 + l] = game_ctx[1 + l], swap.ctx[17 + l] = game_ctx[17 + l];
    for (uint32_t w = 0; w < STEP_STACK_WORDS; w++) MEM32(STEP_STACK + 4u * w) = swap.ctx[w];
    guest_call(0x59E3A0u, 0, NULL, 0);
    *done = (int)g_eax;
    for (uint32_t w = 0; w < STEP_STACK_WORDS; w++) swap.ctx[w] = MEM32(STEP_STACK + 4u * w);
    for (uint32_t w = 0; w < STEP_STACK_WORDS; w++) MEM32(STEP_STACK + 4u * w) = game_ctx[w];
}

static void vr_equip(int slot) {
    if (!heather_placed(vr_player())) {
        rt_log("VR: weapon %d not equipped: Heather is not in the room yet", slot);
        return;
    }
    if (swap.state) {
        rt_log("VR: weapon %d asked while weapon %d is still coming out: it waits", slot, swap.slot);
        return;
    }
    int base = slot == 15 ? 1 : slot == 16 ? 3 : slot == 18 ? 17 : slot;  /* 0x497A70 picks the other mode */
    for (uint32_t i = 1; i < 14; i++) {
        if ((int16_t)MEM16(0x6BDD4Cu + 2u * i) != base) continue;
        if (!(vr_weapons_owned() >> slot & 1u)) break;
        rt_log("VR: weapon %d (item %u) from a holster: %s", slot, i,
               MEM8(0x712C656) ? "the one in her hand put away, the new one loaded" : "its model loaded");
        if (MEM8(0x712C656)) {
            uint32_t none = 0;
            guest_call(0x497A70u, 1, &none, 0);
            guest_call(0x4711D0u, 0, NULL, 0);
        }
        guest_call(0x497A70u, 1, &i, 0);
        memset(&swap, 0, sizeof swap);
        swap.state = 2, swap.slot = slot, swap.item = i;
        return;
    }
    rt_log("VR: weapon %d asked from a holster is not in the inventory", slot);
}

/* An extra weapon holstered (VR knows no model of it to hide in her hand): the game puts it away itself, as step 1 of
 * a draw does (nothing equipped; the holster draws it again through vr_equip). */
static void vr_put_away(void) {
    if (!heather_placed(vr_player()) || swap.state || !MEM8(0x712C656)) return;
    const uint32_t was = MEM8(0x712C656);
    uint32_t none = 0;
    guest_call(0x497A70u, 1, &none, 0);
    guest_call(0x4711D0u, 0, NULL, 0);
    rt_log("VR: weapon %u put away by the game (nothing equipped; in her hand now %u)", was, MEM8(0x712C656));
}

/* LIST 46, "Manual reload": the gun in her hand (its item 9 stun gun, 10 handgun, 11 shotgun, 12 submachine gun; 13,
 * the unlimited extra, never runs out and is left alone) and the hands' reload committed the game's own way
 * (docs/GAME_FACTS.md, Weapons). */
static int vr_gun_item(void) {
    const int w = MEM8(0x712C656);
    const int item = w > 0 && w < 32 ? (int16_t)MEM16(0x6BDD24u + 2u * (uint32_t)w) : 0;
    return item >= 9 && item <= 12 ? item : 0;
}
#define GUN_COUNT(i) MEM16(0x712CA8Eu + 2u * (uint32_t)(i))
/* A magazine or a battery seated: the game's reload transaction 0x497860 (what was loaded back to the reserve, then as
 * many as the gun takes), the HUD's figure as the reload event sets it. */
static void vr_reload_commit(void) {
    const int item = vr_gun_item();
    if (!item) return;
    const int l0 = GUN_COUNT(item), r0 = GUN_COUNT(item + 5);
    uint32_t arg = (uint32_t)item;
    guest_call(0x497860u, 1, &arg, 0);
    MEM16(0x712C644) = (uint16_t)guest_call(0x497780u, 0, NULL, 0);
    rt_log("VR: the hands reloaded item %d: %d loaded and %d in reserve, now %d and %d", item, l0, r0, GUN_COUNT(item),
           GUN_COUNT(item + 5));
}
/* Shells pushed in one at a time (the shotgun): each one from the reserve into the gun while it has room. */
static void vr_load_rounds(int n) {
    const int item = vr_gun_item();
    if (!item) return;
    const int cap = (int8_t)MEM8(0x6BDD17u + (uint32_t)item);
    int done = 0;
    for (; done < n && GUN_COUNT(item) < cap && GUN_COUNT(item + 5) > 0; done++) {
        GUN_COUNT(item) = (uint16_t)(GUN_COUNT(item) + 1);
        GUN_COUNT(item + 5) = (uint16_t)(GUN_COUNT(item + 5) - 1);
    }
    MEM16(0x712C644) = (uint16_t)guest_call(0x497780u, 0, NULL, 0);
    rt_log("VR: the hands loaded %d round%s into item %d: %d loaded, %d in reserve", done, done == 1 ? "" : "s", item,
           GUN_COUNT(item), GUN_COUNT(item + 5));
}
/* A motion event's sound (the reload event's own: 0x5680F0(chr, chr + 0x60, 0, index)). */
static void vr_event_sound(uint32_t p, int index) {
    uint32_t a[4] = {p, p + 0x60u, 0u, (uint32_t)index};
    guest_call(0x5680F0u, 4, a, 0);
}

/* LIST 46: 0x497780 (no arguments) answers the equipped gun's reserve (its ammo item's count). The game's own empty-gun
 * handling (0x4B4340, 0x4E79A0: the reserve 0 and the loaded figure 0x712C644 0) only runs when it answers 0, so while
 * the hands reload an empty gun it answers 0 in play: the game then treats the gun as out of ammo (no shot, its own
 * empty behaviour) instead of firing it at 0 loaded (run vr40: 0 became 65535). The real reserve stays where it is
 * (VR reads it from memory; the menus and the inventory, not in play, see it as ever). */
extern void sub_00497780_gen(void);
void sub_00497780(void) {
    sub_00497780_gen();
    static int logged = -1;
    int held = 0;
    if (g_eax && sh3vr_active() && sh3vr_block_game_reload()) {
        const int item = vr_gun_item();
        if (item && GUN_COUNT(item) == 0) {
            g_eax = 0;
            held = item;
        }
    }
    if (held != logged) {
        logged = held;
        if (held) rt_log("VR: item %d is empty while the hands reload: the game is told it has no reserve", held);
    }
}

/* LIST 46, "no free ammo": 0x497800(item, 0 = the equipped gun) takes a round through 0x496A20, which has no floor (0
 * becomes 65535: a gun fired empty, as a weapon drawn in the middle of another's shot was in run vr40). A gun already at
 * 0 keeps 0 and answers 0. */
extern void sub_00497800_gen(void);
void sub_00497800(void) {
    int item = (int)ARG(0);
    if (!item) item = (int16_t)MEM16(0x6BDD24u + 2u * (uint32_t)MEM8(0x712CABF));
    if (item >= 9 && item <= 12 && (MEM32(0x712CA80) >> item & 1u) && GUN_COUNT(item) == 0) {
        static int logged;
        if (logged++ < 20) rt_log("VR: item %d fired at 0 loaded: kept at 0 (0x496A20 has no floor)", item);
        g_eax = 0;
        g_esp += 4; /* ret: cdecl (session 16: missing, the stack was left 4 bytes off as in vr231) */
        return;
    }
    sub_00497800_gen();
}

/* ---- SH3 Quest VR: spatial sound (LIST 34, "Spatial sound", on; docs/GAME_FACTS.md, Sound) ----
 * The game places a 3D sound in 0x46CDF0 and 0x46CFE0: the sound's place through the listener matrix that
 * 0x4A0500() gives (0xB7F5B0, the game camera's view), the angle atan2(x, z) of it (0x60BAE0) for the pan (left and
 * right only), and its fall-off by the distance (0x46CB10) from the camera's place (0x471800() = 0x89E850, mode 0) or
 * Heather's (mode 2). Inside those three (on the thread running them), with "Spatial sound" and the eyes' view in
 * use, the listener is the eyes: their view (the one VR hands the frame camera, VR_SCRATCH) and their place; how far
 * behind the head the sound is goes to the mix (dsound.c ds_set_behind), which the game's pan cannot tell. The
 * listener matrix is checked against the game camera's view first (the same matrix, D3D's row-vector view, is what
 * makes the eyes' view a fit); if it never matches, the game's listener is kept and sh3.log says so. */
void ds_set_behind(float behind);
static _Thread_local int in_sound3d;
static int listener_check;  /* 0 not yet, 1 the camera's view (the eyes may take its place), -1 never matched */

static int head_listener(void) {
    if (!sh3vr_spatial_sound() || !game_cam_valid || rt_frame - vr_view_at > 2 || listener_check < 0) return 0;
    if (listener_check == 0) {
        static int tries;
        const float *l = (const float *)GPTR(0xB7F5B0u);
        float worst = 0;
        for (int i = 0; i < 16; i++) {
            const float d = fabsf(l[i] - game_cam_view[i]) / (1.0f + fabsf(game_cam_view[i]));
            if (d > worst) worst = d;
        }
        if (worst < 1e-3f) {
            listener_check = 1;
            rt_log("VR: spatial sound: the game's listener is its camera's view (checked, try %d): the eyes take its "
                   "place (the projection's w from %+.0f z)", tries + 1, MEMF(0x72C360u + 11 * 4));
        } else if (++tries >= 1200) {
            listener_check = -1;
            rt_log("VR: spatial sound: the game's listener never matched its camera's view (%g off): its own kept", worst);
        }
    }
    return listener_check == 1;
}

/* The eyes' place in the world: the camera of VR_SCRATCH's row-vector view (p R + t = 0, R a rotation times a scale). */
static void eyes_place(float p[3]) {
    const float *m = (const float *)GPTR(VR_SCRATCH);
    const float s2 = m[0] * m[0] + m[1] * m[1] + m[2] * m[2];
    for (int j = 0; j < 3; j++)
        p[j] = -(m[12] * m[j * 4 + 0] + m[13] * m[j * 4 + 1] + m[14] * m[j * 4 + 2]) / (s2 > 1e-12f ? s2 : 1.0f);
}

/* The two that place a 3D sound and are called (0x46CCD0 has no caller: the recompiler leaves it out). Written out:
 * the regen's scan reads each definition at a line's start. */
extern void sub_0046CDF0_gen(void);
void sub_0046CDF0(void) {
    in_sound3d++;
    sub_0046CDF0_gen();
    in_sound3d--;
    ds_set_behind(-1);
}
extern void sub_0046CFE0_gen(void);
void sub_0046CFE0(void) {
    in_sound3d++;
    sub_0046CFE0_gen();
    in_sound3d--;
    ds_set_behind(-1);
}

/* The listener matrix. */
extern void sub_004A0500_gen(void);
void sub_004A0500(void) {
    sub_004A0500_gen();
    if (in_sound3d && head_listener()) g_eax = VR_SCRATCH;
}

/* The camera's place (distance mode 0): the eyes'. */
extern void sub_00471800_gen(void);
void sub_00471800(void) {
    const uint32_t out = ARG(0);
    sub_00471800_gen();
    if (in_sound3d && head_listener()) {
        float p[3];
        eyes_place(p);
        memcpy(GPTR(out), p, sizeof p);
    }
}

/* The angle of a sound in the listener's view: how far behind the head it is, for the mix. Forward is the side the
 * projection's w takes (D3D's +z). */
extern void sub_0060BAE0_gen(void);
void sub_0060BAE0(void) {
    if (in_sound3d && head_listener()) {
        const uint32_t v = ARG(0);
        const float x = MEMF(v), z = MEMF(v + 8) * (MEMF(0x72C360u + 11 * 4) < 0 ? -1.0f : 1.0f);
        const float r = sqrtf(x * x + z * z);
        ds_set_behind(r > 1e-3f && z < 0 ? -z / r : 0);
        static int logged, logged_at = -100;
        if (logged < 60 && rt_frame - logged_at >= 30) {
            logged++;
            logged_at = rt_frame;
            rt_log("testing: frame %d: a 3D sound at %.0f %.0f from the eyes (x right, z ahead): %.2f behind", rt_frame,
                   x, z, r > 1e-3f && z < 0 ? -z / r : 0.0f);
        }
    }
    sub_0060BAE0_gen();
}

/* Each playable frame while a holster draw is under way (above). */
static void swap_step(int playable) {
    if (!swap.state) return;
    if (!playable || !heather_placed(vr_player())) {
        /* a menu or an event: the game's own steps take over (its inventory's closing loads the asked weapon) */
        rt_log("VR: weapon %d's draw stopped (the game left play after %d frames)", swap.slot, swap.frames);
        swap.state = 0;
        return;
    }
    swap.frames++;
    int done = 0;
    swap_run_loader(&done);
    if (!done && swap.frames < 600) return;
    if (!done) {
        rt_log("VR: weapon %d's model did not load in %d frames: not drawn", swap.slot, swap.frames);
        swap.state = 0;
        return;
    }
    guest_call(0x4711D0u, 0, NULL, 0);
    rt_log("VR: weapon %d drawn after %d frames (asked %u, slot %u, in her hand %u)", swap.slot, swap.frames,
           MEM16(0x712CACE), MEM8(0x712CABF), MEM8(0x712C656));
    swap.state = 0;
}

/* ---- SH3 Quest VR: physical melee (LIST 15, 16, 25) ----
 * clBattleAddQue (0x4ACE10, cdecl) copies one 0x48-byte BattleCollision (u16 kind, u16 battle id, u32 attacker, then
 * previous grip, previous tip, current grip, current tip as float[4]) into the queue at 0x71267A0 (count 0x7122438,
 * no bounds check; reset by 0x4ACA80 each frame). The battle step 0x4B33E0 (called from GameMain 0x58EDB0) handles the
 * queue by kind (2-15; 2 and 3 the melee volumes). Heather's melee volumes are replaced by the swing VR saw, on a copy
 * (the PC VR mod's HookedNativeAddBattleCollision); VR's own hits (punches, guns as clubs) are queued at the battle
 * step's start, so they are handled the same frame. */
#define VR_HIT_SCRATCH 0x0EC00200u    /* a replaced volume (0x48 bytes) */
#define VR_EXTRA_SCRATCH 0x0EC00280u  /* an added one */
static int vr_injecting;
static void vr_write_segments(uint32_t at, const Sh3VrHit *h) {
    memcpy(GPTR(at + 0x08), h->prev_grip, 16);
    memcpy(GPTR(at + 0x18), h->prev_tip, 16);
    memcpy(GPTR(at + 0x28), h->cur_grip, 16);
    memcpy(GPTR(at + 0x38), h->cur_tip, 16);
}
extern void sub_004ACE10_gen(void);
void sub_004ACE10(void) {
    uint32_t src = ARG(0), slot_at = g_esp + 4;
    uint32_t p = sh3vr_active() && !vr_injecting && src ? vr_player() : 0;
    uint16_t kind = p ? MEM16(src) : 0;
    if (p && MEM32(src + 4) == p && (kind == 2 || kind == 3)) {
        Sh3VrHit hit;
        memset(&hit, 0, sizeof hit);
        hit.kind = kind;
        hit.battle_id = MEM16(src + 2);
        memcpy(hit.prev_grip, GPTR(src + 0x08), 16);
        memcpy(hit.prev_tip, GPTR(src + 0x18), 16);
        memcpy(hit.cur_grip, GPTR(src + 0x28), 16);
        memcpy(hit.cur_tip, GPTR(src + 0x38), 16);
        int w = MEM8(0x712C656);
        const int vr = sh3vr_melee_volume(w ? w : -1, &hit);
        if (vr == 2) {
            /* owner 65: the swing already hit at contact; not queued. The ret's own pop of the return address
             * (cdecl: the caller takes the argument off). Session 16, vr231: without it the stack stayed 4 bytes off
             * after a knife hit on the hall's monster, the frame's pops gave esi/edi 0 and the message pump called 0. */
            g_esp += 4;
            return;
        }
        if (vr) {
            memcpy(GPTR(VR_HIT_SCRATCH), GPTR(src), 0x48);
            vr_write_segments(VR_HIT_SCRATCH, &hit);
            MEM32(slot_at) = VR_HIT_SCRATCH;
            sub_004ACE10_gen();
            MEM32(slot_at) = src;  /* cdecl: the caller's argument slot as it was */
            return;
        }
    }
    sub_004ACE10_gen();
}

/* Owner message 38 (session 10): "shake hands or something to break out of holds". 0x4D44E0 (no arguments; from
 * 0x4D97A0, her reaction to a hit) is her struggle step while she is knocked down or held: the time to get free
 * 0x712C608 is set as the hold starts (0x4DAE70, reaction kinds 5, 9 and 12 of 0x712C665); each frame the step counts
 * the buttons newly pressed (0x5FE910 masks 0x200 0x400 8 0x10 0x20 0x40 0x100) and the directions (0x472050 2 4 6 8)
 * and takes [0x712C108] x 0.2 plus [0x712C108] a press off it; at 0 it returns 1 (she is free). VR's shakes of the
 * controllers (sh3vr_combat.cpp struggle_frame) count as presses the same way.
 * [0x712C108] is the frame's time (vr153: 1/72 on the 72 Hz rig), so a press is worth half as much at the headset's
 * 120 Hz as at the PC game's steady 60: in VR each press, a shake or a button the step counted (read back from what it
 * took off), is worth 1/60 s, as on the PC. The fifth of a frame it takes by itself is time, the same at any rate. */
extern void sub_004D44E0_gen(void);
/* 0.1.7 (a player on 0.1.6: "When the dog grabs you on the intro sequence ... it will keep you stuck to the dog's last
 * position even if he dies"; their video: shaking frees her while the dog lives, never after it died): the game never
 * lets a monster die while it holds her (she cannot attack then), but VR's physical melee can, and a holder dead in its
 * hold never lets her go. The frame the struggle step last ran (she is held or down now): sub_004B6810 keeps her
 * holder alive through a hit and the hit breaks the hold instead; and a holder found dead here frees her. */
static int struggle_step_frame = -100;
void sub_004D44E0(void) {
    if (!sh3vr_active()) {
        sub_004D44E0_gen();
        return;
    }
    {
        /* testing (0.1.7, the PC rig): SH3_KILL_HOLDER=N: the N-th struggle step of a hold gives her holder a deadly
         * hit's damage straight (+0x15C, as 0.1.6's physical melee could), so a holder dying in its hold is seen and
         * whether she is freed after it */
        static int kill_at = -2, steps = 0, last = -100;
        if (kill_at == -2) kill_at = getenv("SH3_KILL_HOLDER") ? atoi(getenv("SH3_KILL_HOLDER")) : -1;
        steps = rt_frame - last <= 2 ? steps + 1 : 1;
        last = rt_frame;
        const uint32_t p = vr_player(), a = p ? MEM32(p + 0x17C) : 0;
        if (kill_at > 0 && steps == kill_at && a && a != p && a >= 0x400000u && a < 0x0F000000u &&
            MEMF(a + 0x180) > 0.0f) {
            rt_log("hold test: frame %d: her holder %08X (health %.1f) given %.1f damage in its hold", rt_frame, a,
                   MEMF(a + 0x180), MEMF(a + 0x180) + 10.0f);
            MEMF(a + 0x15C) += MEMF(a + 0x180) + 10.0f;
            if (!MEM16(a + 0x16C)) MEM16(a + 0x16C) = 4;
            MEM32(a + 0x17C) = p;
        }
        /* SH3_HOLD_HIT=N (vr316: the dog never took the damage above while it held her, its health stayed 731.2 of
         * 731.2): the N-th struggle step sends one sweep of her knife across her holder, 35 cm over its place, through
         * the game's own battle step as a swing's contact sweep goes (sh3vr_test_sweep): the hit 0x4B6810 gathers on it
         * is what a real swing at a holder gives */
        static int hit_at = -2;
        if (hit_at == -2) hit_at = getenv("SH3_HOLD_HIT") ? atoi(getenv("SH3_HOLD_HIT")) : -1;
        if (hit_at > 0 && steps == hit_at && a && a != p && a >= 0x400000u && a < 0x0F000000u)
            rig_sweep_across(p, a, "hold test: her holder");
    }
    struggle_step_frame = rt_frame;
    {
        const uint32_t p = vr_player(), a = p ? MEM32(p + 0x17C) : 0;
        if (a && a != p && a >= 0x400000u && a < 0x0F000000u && !(MEMF(a + 0x180) > 0.0f) &&
            MEMF(0x712C608) > 0.0f) {
            rt_log("VR: frame %d: her holder %08X is dead (health %.1f): she is freed (the time to get free %.2f -> 0)",
                   rt_frame, a, MEMF(a + 0x180), MEMF(0x712C608));
            MEMF(0x712C608) = 0.0f;
        }
    }
    const float press = 1.0f / 60.0f, dt = MEMF(0x712C108);
    const int shakes = sh3vr_struggle_presses(MEMF(0x712C608));
    if (shakes > 0) MEMF(0x712C608) -= (float)shakes * press;
    const float before = MEMF(0x712C608);
    sub_004D44E0_gen();
    const uint32_t freed = g_eax;
    int buttons = 0;
    if (!freed && dt > 0.0f && isfinite(before) && isfinite(MEMF(0x712C608))) {
        buttons = (int)floorf((before - MEMF(0x712C608)) / dt - 0.2f + 0.5f);
        if (buttons > 0 && dt < press) {
            const float left = MEMF(0x712C608) - (float)buttons * (press - dt);
            MEMF(0x712C608) = left > 0.0f ? left : 0.0f;  /* at 0 the next step frees her */
        }
    }
    sh3vr_struggle_done(freed != 0, buttons > 0 ? buttons : 0);
    g_eax = freed;
}

/* Owner message 37 (session 10): the game's Life Gauge, 0x5A5E40 (cdecl, one argument; from 0x5A6559, its task while
 * 0xBCB08C is set): her health bar in yellow (+0x180 over +0x184), her stamina in green (the most 0x712C624 less her
 * tiredness 0x712C620), the time to get free (0x712C608) as a thin line under them, at the screen's top left; drawn
 * while byte 0x70E6724 is 1 and the frame's flags allow (0x58F0E2). Its 2D was stamped into each eye's corner (vr152).
 * It draws nothing itself: its rectangles (0x474D70, 0x474EA0) are packets written into the game's command buffer
 * (0x678900 / 0x678930), drawn later, so they cannot be told apart at the draw (vr155: still in the eyes in third
 * person with a flag set around this call). While VR shows the world in the eyes (first or third person) it is left
 * out: the watch shows the same bars in first person (owner messages 39-40: the top left empty then); third person,
 * or the wrist's option off, is the game's normal HUD: VR shows them at the view's top left where the game does
 * (sh3vr.cpp show_wrist_bar); the cinema screen keeps the game's own. */
extern void sub_005A5E40_gen(void);
void sub_005A5E40(void) {
    if (sh3vr_life_gauge_skip()) {
        g_esp += 4; /* ret: cdecl, the caller takes its argument off */
        return;
    }
    sub_005A5E40_gen();
}

/* LIST 50 (physical blocking): the monsters' hits on a character this frame, gathered (0x4B6A20(chr), cdecl; Heather's
 * damage step 0x4EC800 calls it first): each hit adds its damage to chr+0x15C, its point to +0x134 (world), its way to
 * +0x144, its reaction to +0x154, its kind to +0x16C (word), counts +0x16E and sets the attacker +0x17C (its place at
 * +0). 0x4EC800 then takes +0x15C from her health (+0x180). For Heather VR scales what this call added by how her raised
 * weapon or arm guarded it (sh3vr_block). */
extern void sub_004B6A20_gen(void);
void sub_004B6A20(void) {
    const uint32_t chr = ARG(0);
    const uint32_t p = sh3vr_active() ? vr_player() : 0;
    const float before = p && chr == p ? MEMF(chr + 0x15C) : 0.0f;
    const float before_other = p && chr != p ? MEMF(chr + 0x15C) : 0.0f;
    sub_004B6A20_gen();
    if (p && chr != p) {
        /* owner 65: a character Heather hit (its attacker +0x17C her): the hit's damage, way and reaction logged, a
         * fist's halved (LIST 27) */
        const float got = MEMF(chr + 0x15C) - before_other;
        if (MEM32(chr + 0x17C) == p && got > 0.0f && got < 1.0e6f) {
            float at[3], way[3];
            memcpy(at, GPTR(chr + 0x134), 12);
            memcpy(way, GPTR(chr + 0x144), 12);
            const float keep = sh3vr_hit_given(chr, way, at, got, MEM16(chr + 0x154));
            if (keep >= 0.0f && keep <= 16.0f && keep != 1.0f) MEMF(chr + 0x15C) = before_other + got * keep;  /* 0.1.7: over 1 for "Melee damage" (at most 9: 400% x the knife 1.5 x a fast swing 1.5) */
        }
        return;
    }
    if (!p || chr != p) return;
    /* SH3_HIT_TEST=frame:damage,... (the PC rig, LIST 50; the start has no monster): a hit on her at that frame, 1 m
     * ahead of her at her chest's height and coming at her, its damage added as a monster's would be, so a raised
     * weapon's or arm's guard can be seen, and the game seen taking what is kept (her health logged as it changes) */
    float test = 0.0f;
    {
        static int n_test = -1;
        static unsigned test_at[8], watch_until;
        static float test_dmg[8], hp_was;
        if (n_test < 0) {
            n_test = 0;
            const char *s = getenv("SH3_HIT_TEST");
            while (s && *s && n_test < 8) {
                unsigned f = 0;
                float d = 0;
                if (sscanf(s, "%u:%f", &f, &d) != 2) break;
                test_at[n_test] = f, test_dmg[n_test++] = d;
                s = strchr(s, ',');
                if (s) s++;
            }
        }
        if (frame_no <= watch_until && MEMF(chr + 0x180) != hp_was) {
            rt_log("hit test: frame %u: her health now %.1f (was %.1f)", frame_no, MEMF(chr + 0x180), hp_was);
            hp_was = MEMF(chr + 0x180);
        }
        for (int i = 0; i < n_test; i++)
            if (test_at[i] && frame_no >= test_at[i]) {
                test += test_dmg[i];
                rt_log("hit test: frame %u: %.1f damage on her (asked for frame %u), her health %.1f of %.1f", frame_no,
                       test_dmg[i], test_at[i], MEMF(chr + 0x180), MEMF(chr + 0x184));
                test_at[i] = 0, watch_until = frame_no + 60, hp_was = MEMF(chr + 0x180);
            }
    }
    if (test > 0.0f) {
        const float yaw = MEMF(chr + 0x14), fx = sinf(yaw), fz = cosf(yaw);
        MEMF(chr + 0x15C) += test;
        /* her damage step 0x4EC800 takes +0x15C only while the kind word +0x16C is set (vr147: without it her health
         * stayed 100); 4 as the game's own direct-damage call 0x46E7D0 sets it */
        if (!MEM16(chr + 0x16C)) MEM16(chr + 0x16C) = 4;
        MEMF(chr + 0x134) = MEMF(chr) + fx * 490.0f;
        MEMF(chr + 0x138) = MEMF(chr + 4) - 1.2f * 490.0f; /* her chest's height (the game's up is -y) */
        MEMF(chr + 0x13C) = MEMF(chr + 8) + fz * 490.0f;
        MEMF(chr + 0x144) = -fx, MEMF(chr + 0x148) = 0.0f, MEMF(chr + 0x14C) = -fz;
    }
    const float added = MEMF(chr + 0x15C) - before;
    if (!(added > 0.0f) || !(added < 1.0e6f)) return;
    float at[3], way[3], from[3] = {0, 0, 0};
    memcpy(at, GPTR(chr + 0x134), 12);
    memcpy(way, GPTR(chr + 0x144), 12);
    const uint32_t a = MEM32(chr + 0x17C);
    int has_from = 0;
    if (!(test > 0.0f) && a != chr && a >= 0x400000u && a < 0x0F000000u) {
        memcpy(from, GPTR(a), 12);
        const float dx = from[0] - MEMF(chr), dz = from[2] - MEMF(chr + 8);
        has_from = isfinite(dx) && isfinite(dz) && dx * dx + dz * dz < 2.4e7f;  /* within 10 m (490 units a metre) */
    }
    if (has_from) rig_attacker = a;  /* testing: SH3_STEER_AT_ATTACKER (steer_rig) */
    const float keep = sh3vr_block(at, way, from, has_from, added, MEM16(chr + 0x154));
    if (keep < 1.0f && keep >= 0.0f) MEMF(chr + 0x15C) = before + added * keep;
}
extern void sub_004B33E0_gen(void);
void sub_004B33E0(void) {
    Sh3VrHit h;
    uint32_t p = sh3vr_active() ? vr_player() : 0;
    while (sh3vr_extra_hit(&h)) {
        if (!p || MEM32(0x7122438) >= 48) continue;  /* not in play, or the queue is unusually full: dropped */
        uint8_t *s = (uint8_t *)GPTR(VR_EXTRA_SCRATCH);
        uint16_t kind = (uint16_t)h.kind, id = (uint16_t)h.battle_id;
        memset(s, 0, 0x48);
        memcpy(s, &kind, 2);
        memcpy(s + 2, &id, 2);
        memcpy(s + 4, &p, 4);
        vr_write_segments(VR_EXTRA_SCRATCH, &h);
        uint32_t arg = VR_EXTRA_SCRATCH;
        vr_injecting = 1;
        guest_call(0x4ACE10u, 1, &arg, 0);
        vr_injecting = 0;
    }
    /* Owner 65: Heather's hits this step, as the game recorded them (0x4AEA90: a hit on a character writes two records
     * of 0x38 bytes at 0x7120800, count [0x7128A68]; the second, kind 4 at +0x08, is the character's: +0x04 the
     * character, +0x0C the point, +0x1C the way, +0x2E the attack id, +0x34 the attacker): VR tells its swings' hits. */
    const uint32_t n0 = MEM32(0x7128A68);
    sub_004B33E0_gen();
    if (!p) return;
    const uint32_t n1 = MEM32(0x7128A68);
    for (uint32_t i = n1 >= n0 ? n0 : 0; i < n1 && i < 595u; i++) {
        const uint32_t r = 0x7120800u + 0x38u * i;
        if (MEM32(r) != 1u || MEM32(r + 8) != 4u || MEM32(r + 0x34) != p) continue;
        float way[3], at[3];
        memcpy(way, GPTR(r + 0x1C), 12);
        memcpy(at, GPTR(r + 0x0C), 12);
        sh3vr_melee_landed(way, at, MEM32(r + 4), MEM16(r + 0x2E));
        {
            /* Session 16 (LIST 27: vr230/vr232's knife met the hall's monster 85 and 24 times, its health never moved):
             * the damage 0x4B53D0 gives is the attack's own (the table at 0x6CE174, 24 bytes an attack, by the id byte
             * +0x2E) times her +0x18C times the one hit's +0x190; the first 12 logged with them */
            static int told;
            const uint32_t chr = MEM32(r + 4), id = MEM8(r + 0x2E);
            if (told++ < 12 && chr >= 0x400000u && chr < 0x0F000000u)
                rt_log("VR: frame %d: her hit on %08X: attack %u's damage %.2f (stun %.2f), hers x%.2f, its x%.2f; its "
                       "health %.1f of %.1f", rt_frame, chr, id, MEMF(0x6CE174u + id * 24u), MEMF(0x6CE178u + id * 24u),
                       MEMF(p + 0x18C), MEMF(chr + 0x190), MEMF(chr + 0x180), MEMF(chr + 0x184));
        }
    }
}

/* Owner 65 / LIST 27 (session 16): Heather's hits on a monster are gathered by 0x4B6810(chr) (cdecl), not 0x4B6A20
 * (that one is the monsters' hits on her): for each record hitting the character it adds 0x4B53D0's damage to
 * chr+0x15C, the point to +0x134, the way to +0x144, the reaction to +0x154 and her as the attacker +0x17C. What this
 * call added is logged ("took N damage from her"), a fist's halved (sh3vr_hit_given). */
extern void sub_004B6810_gen(void);
void sub_004B6810(void) {
    const uint32_t chr = ARG(0);
    const uint32_t p = sh3vr_active() ? vr_player() : 0;
    const float before = p && chr && chr != p ? MEMF(chr + 0x15C) : 0.0f;
    sub_004B6810_gen();
    if (!p || !chr || chr == p) return;
    const float got = MEMF(chr + 0x15C) - before;
    if (MEM32(chr + 0x17C) == p && got > 0.0f && got < 1.0e6f) {
        float at[3], way[3];
        memcpy(at, GPTR(chr + 0x134), 12);
        memcpy(way, GPTR(chr + 0x144), 12);
        const float keep = sh3vr_hit_given(chr, way, at, got, MEM16(chr + 0x154));
        if (keep >= 0.0f && keep <= 16.0f && keep != 1.0f) MEMF(chr + 0x15C) = before + got * keep;  /* 0.1.7: over 1 for "Melee damage" (at most 9: 400% x the knife 1.5 x a fast swing 1.5) */
        /* 0.1.7: she is held (the struggle step ran this frame or the last) and this is her holder (her attacker
         * +0x17C): the hit breaks its hold (the time to get free to 0: the next struggle step frees her, as shaking
         * free does) and leaves it at least 1 health, so the game's own release runs; it dies to the next hit after. */
        if (rt_frame - struggle_step_frame <= 2 && MEM32(p + 0x17C) == chr) {
            const float hp = MEMF(chr + 0x180);
            float most = hp - 1.0f;
            if (!(most > 0.0f)) most = 0.0f;
            const int capped = MEMF(chr + 0x15C) > most;
            if (capped) MEMF(chr + 0x15C) = most;
            rt_log("VR: frame %d: her holder %08X hit while it holds her (health %.1f%s): its hold broken (the time to "
                   "get free %.2f -> 0)", rt_frame, chr, hp, capped ? ", kept alive until she is free" : "",
                   MEMF(0x712C608));
            MEMF(0x712C608) = 0.0f;
        }
        {
            /* testing (0.1.7, session 28, owner 140; the PC rig): SH3_NO_KILL=1: her hits on monsters do no damage
             * (their reactions and stun stay), so a monster lives and attacks through a long spam of swings */
            static int no_kill = -2, told;
            if (no_kill == -2) no_kill = getenv("SH3_NO_KILL") ? atoi(getenv("SH3_NO_KILL")) : 0;
            if (no_kill > 0) {
                if (told++ < 5)
                    rt_log("testing: frame %d: SH3_NO_KILL: her hit's %.1f damage on %08X taken back", rt_frame,
                           MEMF(chr + 0x15C) - before, chr);
                MEMF(chr + 0x15C) = before;
            }
        }
    }
}

/* LIST 12 (session 13): the game's click zones as the game itself sees them. Each screen registers its zones (0x601970)
 * and asks at once which is under the cursor (0x601A40); by Present the list is cleared again, so sh3_vr_frame's walk
 * found "0 of 0" on every screen (vr193, vr194: the inventory, OPTIONS). Read here, at the game's own question: the
 * zones, the one under the cursor and the screen that asked (its return address). */
static int zone_walk(int32_t zr[][5], int max, int *sum_out) {
    const uint32_t heap_hi = 0x10000000u + rt_heap_used();
    int n = 0, sum = 0;
    for (uint32_t z = MEM32(0x70E6580); z >= 0x10000000u && z + 0x18 <= heap_hi && n < 64; z = MEM32(z + 0x14)) {
        const int32_t x0 = (int32_t)MEM32(z), y0 = (int32_t)MEM32(z + 4), x1 = (int32_t)MEM32(z + 8),
                      y1 = (int32_t)MEM32(z + 0xC), id = (int32_t)MEM32(z + 0x10);
        if (n < max) zr[n][0] = x0, zr[n][1] = y0, zr[n][2] = x1, zr[n][3] = y1, zr[n][4] = id;
        sum += x0 * 3 + y0 * 5 + x1 * 7 + y1 * 11 + id * 13;
        n++;
    }
    if (sum_out) *sum_out = sum;
    return n;
}
static struct {
    int frame, n, sum, hit;
    uint32_t caller;
    int32_t zr[12][5];
} zones_seen = {-100};
extern void sub_00601A40_gen(void);
void sub_00601A40(void) {
    const uint32_t caller = MEM32(g_esp);
    sub_00601A40_gen();
    const uint32_t hit = g_eax;
    if (sh3vr_active()) {
        zones_seen.n = zone_walk(zones_seen.zr, 12, &zones_seen.sum);
        zones_seen.frame = rt_frame;
        zones_seen.hit = (int)hit;
        zones_seen.caller = caller;
        /* OPTIONS (its question at 0x5E69A4) needs nothing more: vr195 (build 169, where this wrapper was not yet
         * reached) showed the game itself highlighting the row under the moving pointer (Subtitles, then Exit) and one
         * pull on Exit choosing it ("Change the settings?"): GAME_FACTS.md "OPTIONS". */
    }
    g_eax = hit;
}

/* Once a frame, from Present before VR's: what the game is doing, for VR; the frame-time tuple for the headset's rate
 * (the PC VR mod's "virtual mode 4": 60/Hz frame scale, 1/Hz seconds, so movement and animation keep their speed). */
void sh3_vr_frame(void) {
    frame_no++;
    if (!sh3vr_active()) return;
    Sh3VrGame g;
    memset(&g, 0, sizeof g);
    uint32_t p = vr_player();
    if (p) {
        g.player_valid = 1;
        memcpy(g.pos, GPTR(p), 12);
        g.yaw = MEMF(p + 0x14);
        {
            /* testing (session 14, owner 61: the beam saber): SH3_BEAM_LOG=N logs every N frames while weapon 9 is in
             * her hand, and whenever its beam flag changes: the flag 0x712C66B (0x4D4E90 sets it when 0x712C18C is 7,
             * the attack set 0x4E6136 reads it), the action 0x712C18C, its step 0x712C189, 0x712C18A, the beam's timers
             * 0x712C1A0 / 0x712C1A8 and the byte the step sets from the animation's frame ([player+0x27C]+0xB) */
            static int beam_log = -2, last_flag = -1;
            if (beam_log == -2) beam_log = getenv("SH3_BEAM_LOG") ? atoi(getenv("SH3_BEAM_LOG")) : 0;
            if (beam_log > 0 && MEM8(0x712C656u) == 9) {
                const int flag = MEM8(0x712C66Bu);
                const uint32_t s = MEM32(p + 0x27C);
                if (flag != last_flag || frame_no % beam_log == 0)
                    rt_log("beam saber: frame %d: beam flag %d, action %u step %u (%u), timers %08x %08x / %08x %08x, "
                           "state byte %d", rt_frame, flag, MEM32(0x712C18Cu), MEM8(0x712C189u), MEM8(0x712C18Au),
                           MEM32(0x712C1A0u), MEM32(0x712C1A4u), MEM32(0x712C1A8u), MEM32(0x712C1ACu),
                           s >= 0x400000u && s < 0x7FFF0000u ? MEM8(s + 0xB) : -1);
                last_flag = flag;
            }
        }
        {
            /* testing (0.1.7, session 28, owner 140: knife stabs spammed left her only walking, monsters not
             * attacking): SH3_STATE_LOG=1 logs her states whenever one changes, 600 lines at most: the action
             * 0x712C18C, its steps 0x712C189 / 0x712C18A, the movement state 0x712C190, the reaction 0x712C665 and the
             * bytes beside it, the weapon 0x712C656, and the animation playing and next on her two channels */
            static int state_log = -2, told;
            static uint32_t last[12];
            if (state_log == -2) state_log = getenv("SH3_STATE_LOG") ? atoi(getenv("SH3_STATE_LOG")) : 0;
            if (state_log > 0 && told < 600) {
                uint32_t now[12];
                now[0] = MEM32(0x712C18Cu), now[1] = MEM8(0x712C189u), now[2] = MEM8(0x712C18Au);
                now[3] = MEM32(0x712C190u), now[4] = MEM8(0x712C665u), now[5] = MEM8(0x712C666u);
                now[6] = MEM8(0x712C663u), now[7] = MEM8(0x712C656u);
                for (int c = 0; c < 2; c++) {
                    const uint32_t e0 = MEM32(p + 0x1B8 + 0x88 * (uint32_t)c + 0x38);
                    const uint32_t e1 = MEM32(p + 0x1B8 + 0x88 * (uint32_t)c + 0x3C);
                    now[8 + 2 * c] = e0 >= 0x6C0000u && e0 < 0x6E0000u ? MEM16(e0) : 0xFFFFu;
                    now[9 + 2 * c] = e1 >= 0x6C0000u && e1 < 0x6E0000u ? MEM16(e1) : 0xFFFFu;
                }
                /* and in a reaction every 30 frames: its sub-step 0x712C18B, the time to get free, her walk factor
                 * +0x11C, the event flags 0x712C1C0/C4 and her legs' channel (+0x238: its time +0x18 in 1/4096
                 * frames, the end 0x4746C0 waits for, (last - first) of its entry +0x44) */
                if (memcmp(now, last, sizeof now) != 0 || (now[4] && frame_no % 30 == 0)) {
                    told++;
                    const uint32_t le = MEM32(p + 0x238 + 0x44);
                    const int lvalid = le >= 0x400000u && le < 0x0F000000u;
                    rt_log("her state: frame %d: action %u steps %u %u, movement %u, reaction %u (%u %u), weapon %u, "
                           "animations %03X next %03X | %03X next %03X; sub-step %u, free in %.2f, walk factor %.2f, "
                           "events %02X %02X, legs at %.2f of %d", rt_frame, now[0], now[1], now[2], now[3],
                           now[4], now[5], now[6], now[7], now[8], now[9], now[10], now[11], MEM8(0x712C18Bu),
                           MEMF(0x712C608u), MEMF(p + 0x11C), MEM32(0x712C1C0u) & 0xFFu, MEM32(0x712C1C4u) & 0xFFu,
                           (int32_t)MEM32(p + 0x238 + 0x18) / 4096.0f,
                           lvalid ? (int)MEM16(le + 8) - (int)MEM16(le + 6) : -1);
                    memcpy(last, now, sizeof now);
                }
            }
        }
        {
            /* SH3_HP=percent (the PC rig, LIST 49): her health held there, to see the wrist's bar low and pulsing.
             * Session 16: let go once a hit has taken it to 0 or below, so a death goes on (door228's death skip at
             * 1%: holding it there each frame could undo the bite before the game's death check saw it). */
            static int hp_pct = -2, hp_logged;
            if (hp_pct == -2) hp_pct = getenv("SH3_HP") ? atoi(getenv("SH3_HP")) : -1;
            if (hp_pct >= 0 && MEMF(p + 0x184) > 0) {
                if (MEMF(p + 0x180) > 0.0f) MEMF(p + 0x180) = MEMF(p + 0x184) * (float)hp_pct / 100.0f;
                else if (!hp_logged++)
                    rt_log("testing: frame %d: her health %.1f after a hit: SH3_HP lets go (a death goes on)", rt_frame,
                           MEMF(p + 0x180));
            }
            /* SH3_TEST_EMPTY_FIRE=1 (the PC rig, session 16, owner 75: reload verified): 0x497800 (a round taken from
             * the equipped gun, LIST 46's "no free ammo" wrapper) called through the guest stack as the game calls it
             * (cdecl, item 0 = the equipped gun), once with the equipped gun at 0 loaded (the wrapper's own answer: its
             * ret's pop was missing until session 16, never reached in a run since a gun fires at 0 only when drawn
             * mid-shot) and once loaded 300 frames later (the game's own way): the stack must come back level. Every
             * guest register is kept. */
            static int fire_test = -2, fire_stage;
            static uint32_t fire_at;
            if (fire_test == -2) fire_test = getenv("SH3_TEST_EMPTY_FIRE") ? atoi(getenv("SH3_TEST_EMPTY_FIRE")) : 0;
            if (fire_test && fire_stage < 2 && (fire_stage == 0 || frame_no - fire_at >= 300)) {
                const int item = (int16_t)MEM16(0x6BDD24u + 2u * (uint32_t)MEM8(0x712CABF));
                if (item >= 9 && item <= 12 && (MEM32(0x712CA80) >> item & 1u) &&
                    (fire_stage == 0) == (GUN_COUNT(item) == 0)) {
                    const uint32_t keep[6] = {g_eax, g_ecx, g_edx, g_ebx, g_esi, g_edi};
                    const uint32_t esp0 = g_esp;
                    const int before = GUN_COUNT(item);
                    g_esp -= 4;
                    MEM32(g_esp) = 0;            /* the item: 0, the equipped gun */
                    g_esp -= 4;
                    MEM32(g_esp) = 0x00497800u;  /* the call's return address (the wrapper is called directly) */
                    sub_00497800();
                    const uint32_t answer = g_eax;
                    g_esp += 4;                  /* cdecl: the caller pops its argument */
                    const int off = (int)(g_esp - esp0);
                    rt_log("testing: frame %u: 0x497800 called for item %d %s: the stack %s (%+d bytes), answered %u, "
                           "its count %d -> %d", frame_no, item,
                           fire_stage == 0 ? "at 0 loaded (the wrapper's own way)" : "loaded (the game's own way)",
                           off == 0 ? "level" : "OFF", off, answer, before, GUN_COUNT(item));
                    g_esp = esp0;
                    g_eax = keep[0], g_ecx = keep[1], g_edx = keep[2], g_ebx = keep[3], g_esi = keep[4], g_edi = keep[5];
                    fire_stage++;
                    fire_at = frame_no;
                }
            }
            /* SH3_LIFE_DISPLAY=1 (the PC rig): the game's Life Display option on (bit 0x80 of 0x70E66C6, as the
             * Extra options' switch 0x60CB90 sets it), to see what the game itself draws for it */
            static int life_on = -2;
            if (life_on == -2) life_on = getenv("SH3_LIFE_DISPLAY") ? atoi(getenv("SH3_LIFE_DISPLAY")) : 0;
            if (life_on) MEM8(0x70E66C6) |= 0x80;
            /* SH3_LIFE_BARS=1 (the PC rig, session 10): byte 0x70E6724 at 1, the one the game's bar drawing 0x5A5E40
             * waits for (0x58F0EE: shown while it is 1, bit 6 of 0x70E66A0 clear and bit 2 set), to see those bars */
            static int bars_on = -2;
            if (bars_on == -2) bars_on = getenv("SH3_LIFE_BARS") ? atoi(getenv("SH3_LIFE_BARS")) : 0;
            if (bars_on) MEM8(0x70E6724) = 1;
            /* SH3_STAMINA=percent (the PC rig, owner message 37): her stamina held there (her tiredness 0x712C620 at
             * the rest of its most 0x712C624), to see the wrist's stamina bar part spent */
            static int st_pct = -2;
            if (st_pct == -2) st_pct = getenv("SH3_STAMINA") ? atoi(getenv("SH3_STAMINA")) : -1;
            if (st_pct >= 0 && MEMF(0x712C624) > 0) MEMF(0x712C620) = MEMF(0x712C624) * (float)(100 - st_pct) / 100.0f;
            /* SH3_STRUGGLE_TEST=frame:time[,frame:time...] (the PC rig, owner message 38; the start has no monster): from
             * each frame the game's struggle step 0x4D44E0 runs once a frame with the time to get free first set to
             * `time`, as a monster's hold sets it (0x4DAE70: 0.5 to 2.4 by the action level and the hold), until it says
             * she is free; VR's shakes and the buttons are counted into it as in a real hold (the wrapper above) */
            static int sg_at[4] = {-2, -1, -1, -1}, sg_running = 0, sg_next = 0;
            static float sg_time[4] = {0};
            static uint32_t sg_from = 0;
            if (sg_at[0] == -2) {
                const char *e = getenv("SH3_STRUGGLE_TEST");
                sg_at[0] = -1;
                for (int i = 0; e && *e && i < 4; i++) {
                    if (sscanf(e, "%d:%f", &sg_at[i], &sg_time[i]) != 2) {
                        sg_at[i] = -1;
                        break;
                    }
                    e = strchr(e, ',');
                    if (e) e++;
                }
            }
            if (!sg_running && sg_next < 4 && sg_at[sg_next] >= 0 && frame_no >= (uint32_t)sg_at[sg_next]) {
                sg_running = 1, sg_from = frame_no;
                MEMF(0x712C608) = sg_time[sg_next];
                rt_log("struggle test: frame %u: the time to get free set to %.2f (the frame time [0x712C108] %.5f)",
                       frame_no, sg_time[sg_next], MEMF(0x712C108));
                sg_next++;
            }
            if (sg_running) {
                const uint32_t freed = guest_call(0x4D44E0u, 0, NULL, 0);
                if (freed || frame_no - sg_from > 3000) {
                    rt_log("struggle test: frame %u: %s after %u frames (the time to get free %.3f)", frame_no,
                           freed ? "free" : "given up", frame_no - sg_from, MEMF(0x712C608));
                    sg_running = 0;
                }
            }
        }
        g.hp = MEMF(p + 0x180);
        g.hp_max = MEMF(p + 0x184);
        {
            /* Owner message 37: her stamina, as the game's own bar draws it (0x5A6227: the most 0x712C624 less her
             * tiredness 0x712C620, which grows as she runs, 0x4B3F60, and falls as she rests) */
            const float most = MEMF(0x712C624), tired = MEMF(0x712C620);
            g.stamina = most > 0.0f && isfinite(tired) ? fminf(fmaxf(1.0f - tired / most, 0.0f), 1.0f) : 1.0f;
        }
    } else {
        g.stamina = 1.0f;
    }
    g.weapon = MEM8(0x712C656) ? MEM8(0x712C656) : -1;  /* 0: none (docs/GAME_FACTS.md, Weapons) */
    g.weapon_slot = MEM8(0x712CABF) ? MEM8(0x712CABF) : -1;
    g.weapons_owned = vr_weapons_owned();
    g.life_display = (MEM8(0x70E66C6) & 0x80) != 0;  /* LIST 49 (0x60CBC0) */
    g.life_gauge = MEM8(0x70E6724) == 1;              /* owner message 37: the game's Life Gauge (0x60CE60) */
    g.gun_item = vr_gun_item();
    if (g.gun_item) {
        g.loaded = GUN_COUNT(g.gun_item);
        g.reserve = GUN_COUNT(g.gun_item + 5);
        g.capacity = (int8_t)MEM8(0x6BDD17u + (uint32_t)g.gun_item);
    }
    g.reacting = p ? her_reacting() : 0;
    g.gesture = p ? her_gesture() : 0;
    g.event_hold = p && playable_frame ? event_hold() : 0;
    {
        /* 0.1.9 (owner 168, the puzzles' hint): the game's mode (its main loop's step, at the level the event dispatch
         * returns to: main_level) and the mode an event's script handed over to; each change of mode logged */
        static int mode_was = -2, modes_told;
        g.game_mode = main_level >= 0 ? (int)MEM32(0xBCC65Cu + 4u * (uint32_t)main_level) : -1;
        g.event_mode = event_mode;
        if (g.game_mode != mode_was) {
            if (modes_told++ < 400)
                rt_log("VR: frame %d: the game's mode %d -> %d (%s; the event step %u, event %d, a scene %d, a demo %d)%s",
                       rt_frame, mode_was, g.game_mode,
                       event_mode >= 0 && event_mode == g.game_mode ? "an event's script opened it" : "not from an event",
                       event_state, (int)MEM32(0xBA9768u), MEM8(0x81FAB5) != 0, MEM32(0x898308) == 2,
                       modes_told == 400 ? "; no more of these lines" : "");
            mode_was = g.game_mode;
        }
    }
    {
        /* 0.1.8 (owner 152, a player: "I took the key for the bookstore. But when I went back to the book store, the
         * key is not in my inventory"): what the inventory gains and loses (its items' bits at 0x712CA80, which
         * 0x4972B0 sets for each item the game gives; the counts follow from 0x712CA8E), each with the game's event
         * step, its event and mode then: a player's log tells an item never given from one given and lost after.
         * A load (more than 6 at once) is one line. */
        static uint32_t inv[4];
        static int inv_ready, told;
        if (p && told < 400) {
            uint32_t now[4], changed = 0;
            for (uint32_t w = 0; w < 4; w++) {
                now[w] = MEM32(0x712CA80u + 4u * w) & (w == 3u ? 0xFFFFu : 0xFFFFFFFFu);
                uint32_t d = now[w] ^ inv[w];
                for (; d; d &= d - 1u) changed++;
            }
            if (inv_ready && changed > 6u) {
                told++;
                rt_log("VR: frame %d: the inventory changed by %u items at once (a save loaded or a new game)", rt_frame,
                       changed);
            } else if (inv_ready && changed) {
                for (uint32_t w = 0; w < 4; w++)
                    for (uint32_t b = 0; b < 32u; b++) {
                        const uint32_t bit = 1u << b;
                        if ((now[w] ^ inv[w]) & bit) {
                            told++;
                            rt_log("VR: frame %d: item %u %s the inventory (the game's event step %u, event %d, mode "
                                   "%d)", rt_frame, w * 32u + b, now[w] & bit ? "now in" : "no longer in", event_state,
                                   (int)MEM32(0xBA9768u), (int)last_game_mode);
                        }
                    }
            }
            memcpy(inv, now, sizeof inv);
            inv_ready = 1;
        }
    }
    {
        /* testing (0.1.8, session 32, owner 152: "the thing where she keeps walking after interacting is still there if
         * you are moving at all when you interact"): SH3_WALK_LOG=N logs every N frames while she moves or her walk's
         * velocity is not 0 (and each frame the game's mode changes), 3000 lines at most: her place, her walk's
         * velocity (+0xF0/+0xF8) and factor (+0x11C), the dispatch's mode (5 the playable update), the action
         * 0x712C18C and movement state 0x712C190, and what VR asks (sh3vr_move: steering, its speed) */
        static int walk_log = -2, told;
        static float lx, lz;
        static uint32_t last_mode = 0xFFFFFFFFu;
        if (walk_log == -2) walk_log = getenv("SH3_WALK_LOG") ? atoi(getenv("SH3_WALK_LOG")) : 0;
        if (walk_log > 0 && p && told < 3000) {
            Sh3VrMove wm;
            sh3vr_move(&wm);
            const float x = MEMF(p), z = MEMF(p + 8), vx = MEMF(p + 0xF0), vz = MEMF(p + 0xF8);
            const int moved = fabsf(x - lx) > 0.5f || fabsf(z - lz) > 0.5f || vx != 0.0f || vz != 0.0f;
            if ((moved && frame_no % (uint32_t)walk_log == 0) || last_game_mode != last_mode) {
                told++;
                rt_log("her walk: frame %d: at %.1f %.1f (moved %.1f), velocity %.3f %.3f, factor %.2f; the game's mode %d "
                       "(playable %d, its event state %u), action %u, movement %u, gesture %u (grab %u); VR steers %d "
                       "(speed %.2f, heading %.2f)",
                       rt_frame, x, z, sqrtf((x - lx) * (x - lx) + (z - lz) * (z - lz)), vx, vz, MEMF(p + 0x11C),
                       (int)last_game_mode, playable_frame, event_state,
                       MEM32(0x712C18Cu), MEM32(0x712C190u), MEM16(0x712C64Au), MEM8(0x712C19Eu), wm.steer, wm.speed,
                       wm.heading);
                lx = x, lz = z;
            }
            last_mode = last_game_mode;
        }
        /* testing (0.1.8, session 32): SH3_GESTURE_TEST="frame:gesture,..." asks the game for a gesture at each frame
         * given, as an event does (0x496240(gesture): 5 an item's pick-up bending down, 6 standing up from it, 0 none),
         * so the rig plays a floor item's bend with the stick held (no floor item is within the rig's reach) */
        static const char* gt = (const char*)-1;
        if (gt == (const char*)-1) gt = getenv("SH3_GESTURE_TEST");
        if (gt && p) {
            for (const char* s = gt; s && *s;) {
                int at = 0, ge = 0;
                if (sscanf(s, "%d:%d", &at, &ge) == 2 && at == rt_frame) {
                    uint32_t a = (uint32_t)ge;
                    guest_call(0x496240u, 1, &a, 0);
                    rt_log("testing: frame %d: gesture %d asked (0x496240): action %u, movement %u, her walk factor %.2f",
                           rt_frame, ge, MEM32(0x712C18Cu), MEM32(0x712C190u), MEMF(p + 0x11C));
                }
                s = strchr(s, ',');
                if (s) s++;
            }
        }
        /* testing (0.1.8, session 32, owner 152: "When I took the key, I saw it double on my headset"): an item given
         * by a room's script shows its model turning in a box on the screen while its message is up (0x599A60(item,
         * 0.2): the display at 0xBC9000, drawn by 0x599EA0 each frame in play; 0x599A80(0.2) fades it out).
         * SH3_ITEM_SHOW_TEST="frame:item,frame:-1,..." opens it for that item at each frame given, -1 closes it.
         * (vr393, vr396: opened so, it loads the item's model and reaches its steady step 4, but draws nothing, with a
         * message up or not: a room script's give (0x568910: the item added, 0x599A60, then the message 0x4BA500)
         * draws it; the rig's rooms have none. The route dump showed no draw at all inside the dispatch in play, so
         * d3d8.c's in-dispatch rule moves only the item's.) */
        static const char* it = (const char*)-1;
        if (it == (const char*)-1) it = getenv("SH3_ITEM_SHOW_TEST");
        if (it && p) {
            for (const char* s = it; s && *s;) {
                int at = 0, item = 0;
                if (sscanf(s, "%d:%d", &at, &item) == 2 && at == rt_frame) {
                    const float fade = 0.2f;
                    uint32_t a[2];
                    memcpy(&a[1], &fade, 4);
                    a[0] = (uint32_t)item;
                    /* (0x599EA0's own first step loads the item's model: 0x599A00, the file from the table at
                     * 0x710A30 through 0x4DC560; its step and the display's state logged a second later) */
                    if (item >= 0) guest_call(0x599A60u, 2, a, 0);
                    else guest_call(0x599A80u, 1, &a[1], 0);
                    rt_log("testing: frame %d: the item display %s (item %d; its state %u, step %u)", rt_frame,
                           item >= 0 ? "opened" : "closed", item, MEM32(0xBC900Cu), MEM32(0xBC9008u));
                }
                if (sscanf(s, "%d:%d", &at, &item) == 2 && at + 72 == rt_frame) {
                    rt_log("testing: frame %d: the item display a second on: its state %u, step %u, model %u (slot 4's "
                           "file %u)", rt_frame, MEM32(0xBC900Cu), MEM32(0xBC9008u), MEM32(0xBC9004u),
                           (uint32_t)guest_call(0x4DC520u, 1, (uint32_t[]){4u}, 0));
                }
                s = strchr(s, ',');
                if (s) s++;
            }
        }
        /* testing (0.1.8, session 32, owner 152: the bookstore's key not in the inventory; action_check_at): the rig's
         * rooms have no key's door within reach, so SH3_SPEND_TEST="frame:event:item" makes the room's event `event`
         * one that needs `item` held and uses it up (its fields d4 = 2, value = item, dc = 1, as a key's door) and gives
         * Heather the item, at that frame. The inventory's log then tells when it went: with VR's looks only looking,
         * when A used the event (the game's own check, the event run); 0.1.7's way (SH3_LOOK_SPENDS=1) at a look of
         * VR's, the event not run. */
        static const char* sp = (const char*)-1;
        if (sp == (const char*)-1) sp = getenv("SH3_SPEND_TEST");
        int sp_at = 0, sp_ev = 0, sp_item = 0;
        const uint32_t tab = MEM32(0x711FCA0u);
        if (sp && p && sscanf(sp, "%d:%d:%d", &sp_at, &sp_ev, &sp_item) == 3 && sp_at == rt_frame && tab && MEM32(tab)) {
            const uint32_t e = MEM32(tab) + (uint32_t)sp_ev * 0x14u;
            const uint32_t fields[3] = {0x6CF6D4u, 0x6CF6D8u, 0x6CF6DCu}, values[3] = {2u, (uint32_t)sp_item, 1u};
            for (int k = 0; k < 3; k++) {
                const uint32_t d = fields[k], w = e + 4u * MEM8(d), sh = MEM8(d + 1), mask = MEM16(d + 2);
                MEM32(w) = (MEM32(w) & ~(mask << sh)) | ((values[k] & mask) << sh);
            }
            uint32_t give = (uint32_t)sp_item;
            guest_call(0x4972B0u, 1, &give, 0);
            rt_log("testing: frame %d: event %d now needs item %d held and uses it up (trigger %d); item %d given (in the "
                   "inventory %u)", rt_frame, sp_ev, sp_item, event_trigger(sp_ev), sp_item,
                   (MEM32(0x712CA80u + (uint32_t)(sp_item >> 5) * 4u) >> (sp_item & 31)) & 1u);
        }
        /* testing (0.1.9, keys_lend): SH3_KEY_BACK_TEST="frame:event:item:flag", see key_test_tab */
        static const char* kb = (const char*)-1;
        if (kb == (const char*)-1) kb = getenv("SH3_KEY_BACK_TEST");
        int kb_at = 0, kb_ev = 0, kb_item = 0, kb_flag = -1;
        if (kb && p && sscanf(kb, "%d:%d:%d:%d", &kb_at, &kb_ev, &kb_item, &kb_flag) >= 3 && kb_at == rt_frame && tab &&
            MEM32(tab) && kb_item > 0 && kb_item < 112) {
            MEM32(0x712CA80u + 4u * (uint32_t)(kb_item >> 5)) &= ~(1u << (kb_item & 31));
            key_test_tab = tab, key_test_ev = kb_ev, key_test_item = kb_item, key_test_given = kb_flag;
            rt_log("testing: frame %d: item %d taken away as 0.1.7's looks did; event %d (d4 %u, item %u, dc %u, flag A "
                   "%u wants %u, now %d) a key's door given by flag %d (now %d)", rt_frame, kb_item, kb_ev,
                   event_field(tab, kb_ev, 9), event_field(tab, kb_ev, 10), event_field(tab, kb_ev, 11),
                   event_field(tab, kb_ev, 3), event_field(tab, kb_ev, 2), game_flag((int)event_field(tab, kb_ev, 3)),
                   kb_flag, game_flag(kb_flag));
        }
    }
    g.playable = playable_frame;
    playable_frame = 0;
    g.cutscene = MEM8(0x81FAB5) != 0;
    g.demo = MEM32(0x898308) == 2;
    memcpy(g.cam_view, game_cam_view, sizeof g.cam_view);
    g.cam_valid = game_cam_valid;
    game_cam_valid = 0;
    /* LIST 12 (session 12): the game's screens take a click only over one of their click zones (0x601970 adds one:
     * x0 y0 x1 y1 in its cursor's 0..512 space, an id, the next; 0x601940 clears them; 0x601A40 finds the one under the
     * cursor, x0 <= x < x1 and y0 <= y <= y1), and only while it counts its cursor as shown (0x70E6418, 0x601A40's
     * gate). It hides the cursor 3 s after it last moved (0x600BF0, the deadline 0x70E6414; 0x600920 counts a cursor
     * standing still as out of use, 0x70E6410 = 0), so a pull with the hand held still on an entry found no zone (the
     * PC rig's still hand, vr189; a hand resting while reading). While VR's pointer is on a screen the cursor stays
     * shown; it hides when the pointer goes. */
    {
        float mx, my;
        int ml, mr;
        static int was_on, logged_n = -1, logged_sum, pulls_logged, was_left;
        const int on = sh3vr_mouse(&mx, &my, &ml, &mr);
        if (on) {
            MEM8(0x70E6418) = 1;
            MEM32(0x70E6414) = 0xFFFFFFF0u;
        } else if (was_on) {
            MEM8(0x70E6418) = 0;
            MEM32(0x70E6414) = 0;
        }
        was_on = on;
        const int32_t cx = (int32_t)MEM32(0x70E6408), cy = (int32_t)MEM32(0x70E640C);
        int32_t zr[12][5];
        int n, sum;
        if (zones_seen.frame >= rt_frame - 1) {
            /* the zones as the game's own question saw them this frame (sub_00601A40 above), and its answer */
            n = zones_seen.n, sum = zones_seen.sum;
            memcpy(zr, zones_seen.zr, sizeof zr);
            g.click_zone = zones_seen.hit;
        } else {
            n = zone_walk(zr, 12, &sum);
            for (int i = 0; i < n && i < 12 && !g.click_zone; i++)
                if (zr[i][0] <= cx && cx < zr[i][2] && zr[i][1] <= cy && cy <= zr[i][3]) g.click_zone = zr[i][4] ? zr[i][4] : -1;
        }
        g.click_zones = n;
        if (g.click_zone < 0) g.click_zone = 0;
        /* the rig's record: each new set of zones once (a screen's layout), and each pull with what it was over */
        if (on && (n != logged_n || sum != logged_sum) && n > 0) {
            static int sets_logged;
            if (sets_logged++ < 40) {
                char line[700];
                int k = snprintf(line, sizeof line, "VR: frame %d: the game's click zones: %d:", rt_frame, n);
                for (int i = 0; i < n && i < 12 && k < (int)sizeof line - 40; i++)
                    k += snprintf(line + k, sizeof line - k, " %X (%d,%d)-(%d,%d)", (unsigned)zr[i][4], zr[i][0],
                                  zr[i][1], zr[i][2], zr[i][3]);
                rt_log("%s", line);
            }
        }
        if (on) logged_n = n, logged_sum = sum;
        if (on && ml && !was_left && pulls_logged++ < 60)
            rt_log("VR: frame %d: a pull at the game's cursor %d %d (the beam at %.3f %.3f): over click zone %X of %d",
                   rt_frame, cx, cy, mx, my, (unsigned)g.click_zone, n);
        was_left = on && ml;
    }
    sh3vr_game_state(&g);
    /* Her body turns with the head while she stands (first person). */
    Sh3VrMove mv;
    sh3vr_move(&mv);
    if (p && mv.face && !mv.steer && !her_reacting() && !her_gesture() && !(g.playable && event_hold()))
        set_heather_yaw(p, mv.face_yaw);
    MEMF(0x0EC00000u) = 0.0f;  /* no cutscene borders in VR */
    /* A weapon drawn from a holster: equipped between frames, in play only. */
    Sh3VrActions act;
    sh3vr_actions(&act);
    if (act.put_away && p && g.playable) vr_put_away();
    if (act.equip_slot > 0 && p && g.playable) vr_equip(act.equip_slot);
    if (act.reload_commit && p && g.playable) vr_reload_commit();
    if (act.load_rounds > 0 && p && g.playable) vr_load_rounds(act.load_rounds);
    if (act.sound_event >= 0 && p && g.playable) vr_event_sound(p, act.sound_event);
    swap_step(g.playable);
    /* testing (LIST 46, the PC rig): SH3_GIVE=item:count,... at the first playable frame (a gun's count is its loaded
     * rounds, its ammo item's the reserve: handgun 10 + bullets 15, shotgun 11 + shells 16, submachine gun 12 +
     * rounds 17, stun gun 9 + batteries 14), then SH3_EQUIP=weapon number. */
    static int gave, give_at = -1;
    /* (0.1.9: SH3_GIVE_AT=frame, the first playable frame from then: after the dream's death the game gives her a new
     * inventory, so a weapon for the restroom's mirror is given there) */
    if (give_at < 0) give_at = getenv("SH3_GIVE_AT") ? atoi(getenv("SH3_GIVE_AT")) : 0;
    if (!gave && RT_ENV("SH3_GIVE") && heather_placed(p) && g.playable && rt_frame >= give_at) {
        gave = 1;
        rt_log("testing: the items she has before the give: %08X %08X %08X %08X", MEM32(0x712CA80u), MEM32(0x712CA84u),
               MEM32(0x712CA88u), MEM32(0x712CA8Cu));
        for (const char* s = getenv("SH3_GIVE"); *s;) {
            int item = atoi(s), count = 0;
            const char* c = strchr(s, ':');
            if (c) count = atoi(c + 1);
            if (item > 0 && item < 112) {  /* bits 112 on would overlap the counts (0x712CA8E) */
                MEM32(0x712CA80u + 4u * (uint32_t)(item >> 5)) |= 1u << (item & 31);
                if (item >= 9 && item <= 0x15) MEM16(0x712CA8Eu + 2u * (uint32_t)item) = (uint16_t)count;
                rt_log("testing: item %d given, count %d", item, count);
            }
            s = strchr(s, ',');
            if (!s) break;
            s++;
        }
        if (getenv("SH3_EQUIP")) {
            const int first = atoi(getenv("SH3_EQUIP"));  /* 0: the weapon in her hand put away (an empty hand) */
            if (first) vr_equip(first);
            else vr_put_away();
        }
    }
    /* testing: the gun's rounds logged as they change (a dry fire must never take a round below 0: 0x496A20 has no
     * floor) */
    if (gave) {
        static int last_item = -1, last_l = -1, last_r = -1;
        if (g.gun_item != last_item || g.loaded != last_l || g.reserve != last_r) {
            last_item = g.gun_item, last_l = g.loaded, last_r = g.reserve;
            if (g.gun_item)
                rt_log("testing: frame %d: item %d has %d loaded, %d in reserve", rt_frame, g.gun_item, g.loaded,
                       g.reserve);
        }
    }
    /* SH3_EQUIP=a,b,c...: the next one every SH3_EQUIP_EVERY (120) frames of play, cutscenes and demos not counted
     * (each gun drawn in turn, for SH3_MESH_DUMP and the reloads). */
    {
        static const char *next_equip;
        static int equip_wait, equip_started, equip_every;
        if (gave && !equip_started) {
            equip_started = 1;
            const char *e = getenv("SH3_EQUIP");
            next_equip = e ? strchr(e, ',') : NULL;
            if (next_equip) next_equip++;
            equip_every = getenv("SH3_EQUIP_EVERY") ? atoi(getenv("SH3_EQUIP_EVERY")) : 120;
        }
        if (next_equip && g.playable && !g.cutscene && !g.demo && heather_placed(p) && ++equip_wait >= equip_every &&
            !swap.state) {
            equip_wait = 0;
            vr_equip(atoi(next_equip));
            next_equip = strchr(next_equip, ',');
            if (next_equip) next_equip++;
        }
    }
    /* testing (session 6, the trigger's delay): SH3_PLAYER_DIFF=from:to logs each dword of Heather's struct (0x800
     * bytes) that changed since the frame before, frames from..to (her ready stance found by what changes as it goes). */
    {
        static int diff_from = -2, diff_to;
        static uint32_t before[0x200];
        if (diff_from == -2) {
            diff_from = -1;
            const char *e = getenv("SH3_PLAYER_DIFF");
            if (e) sscanf(e, "%d:%d", &diff_from, &diff_to);
        }
        if (diff_from >= 0 && p && rt_frame >= diff_from - 1 && rt_frame <= diff_to) {
            char line[1400];
            int n = snprintf(line, sizeof line, "player diff frame %d:", rt_frame);
            for (uint32_t i = 0; i < 0x200; i++) {
                const uint32_t v = MEM32(p + 4 * i);
                if (rt_frame >= diff_from && v != before[i] && n < (int)sizeof line - 40) {
                    float f;
                    memcpy(&f, &v, 4);
                    n += snprintf(line + n, sizeof line - n, " %03X=%08X(%g)", 4 * i, v, isfinite(f) ? f : 0.0f);
                }
                before[i] = v;
            }
            if (rt_frame >= diff_from) rt_log("%s", line);
        }
    }
    /* testing (session 12, LIST 12): SH3_MEM_SNAP=A,B,C,D (game frames) finds a game variable by what a press changes.
     * The game's data (SH3_MEM_SNAP_RANGE=lo:hi in hex; the exe's .data, 0x6B0000:0x717A900, by default) is copied at
     * A; every dword that changed by B is noise (counters, timers, the cursor's blink: no press between A and B); copied
     * again at B; at C the dwords changed since B that are not noise are kept (a press between B and C); at D those of
     * them that changed again are logged with their values at B, C and D (a second press between C and D): the
     * OPTIONS screen's highlighted row steps on each down press. */
    {
        enum { kCand = 4096 };
        static int ms_at[4] = {-2, -1, -1, -1}, ms_step;
        static uint32_t ms_lo = 0x6B0000u, ms_hi = 0x717A900u, ms_n, ms_nc;
        static uint32_t *ms_copy, *ms_noise, ms_idx[kCand], ms_b[kCand], ms_c[kCand];
        if (ms_at[0] == -2) {
            ms_at[0] = -1;
            const char *e = getenv("SH3_MEM_SNAP"), *r = getenv("SH3_MEM_SNAP_RANGE");
            if (e && sscanf(e, "%d,%d,%d,%d", &ms_at[0], &ms_at[1], &ms_at[2], &ms_at[3]) != 4) ms_at[0] = -1;
            if (r) {
                unsigned lo = 0, hi = 0;
                if (sscanf(r, "%x:%x", &lo, &hi) == 2 && hi > lo) ms_lo = lo & ~3u, ms_hi = hi & ~3u;
            }
            ms_n = (ms_hi - ms_lo) / 4;
            if (ms_at[0] >= 0) {
                ms_copy = (uint32_t *)malloc((size_t)ms_n * 4);
                ms_noise = (uint32_t *)calloc((size_t)ms_n / 32 + 1, 4);
                if (!ms_copy || !ms_noise) {
                    rt_log("memory snapshots: no room for %u MB", ms_n / (1024 * 256));
                    ms_at[0] = -1;
                } else {
                    rt_log("memory snapshots: %08X to %08X at game frames %d, %d, %d, %d", ms_lo, ms_hi, ms_at[0],
                           ms_at[1], ms_at[2], ms_at[3]);
                }
            }
        }
        if (ms_at[0] >= 0 && ms_step < 4 && rt_frame >= ms_at[ms_step]) {
            const volatile uint32_t *m = (const volatile uint32_t *)XBOX_PTR(ms_lo);
            uint32_t changed = 0;
            if (ms_step == 0) {
                for (uint32_t i = 0; i < ms_n; i++) ms_copy[i] = m[i];
                changed = ms_n;
            } else if (ms_step == 1) {
                for (uint32_t i = 0; i < ms_n; i++) {
                    const uint32_t v = m[i];
                    if (v != ms_copy[i]) ms_noise[i >> 5] |= 1u << (i & 31), changed++;
                    ms_copy[i] = v;
                }
            } else if (ms_step == 2) {
                for (uint32_t i = 0; i < ms_n; i++) {
                    const uint32_t v = m[i];
                    if (v == ms_copy[i] || (ms_noise[i >> 5] >> (i & 31) & 1)) continue;
                    changed++;
                    if (ms_nc < kCand) ms_idx[ms_nc] = i, ms_b[ms_nc] = ms_copy[i], ms_c[ms_nc] = v, ms_nc++;
                }
            } else {
                for (uint32_t k = 0; k < ms_nc; k++) {
                    const uint32_t v = m[ms_idx[k]];
                    if (v == ms_c[k]) continue;
                    if (changed++ < 300)
                        rt_log("memory snapshots: %08X: %08X at B, %08X at C, %08X at D (steps %d, %d)",
                               ms_lo + 4 * ms_idx[k], ms_b[k], ms_c[k], v, (int)(ms_c[k] - ms_b[k]),
                               (int)(v - ms_c[k]));
                }
                free(ms_copy);
                free(ms_noise);
                ms_copy = ms_noise = NULL;
            }
            static const char *kWhat[4] = {"copied", "changed with no press (noise)",
                                           "changed after the first press, not noise", "changed again after the second"};
            rt_log("memory snapshots: frame %d (%c): %u dwords %s%s", rt_frame, 'A' + ms_step, changed, kWhat[ms_step],
                   ms_step == 2 && changed > kCand ? " (the first 4096 kept)" : "");
            ms_step++;
        }
    }
    /* Quick gun raise (session 6, docs/GAME_FACTS.md, Heather's animations): a fresh pull took the game 47 frames to
     * shoot (run vr58): the raise (entry 0x6D01A8: 10 frames at rate 0x600, 28 VR frames with its blend) and the shot's
     * start (0x6D0280: the shot 5 frames in, 19 VR frames). Her two animation channels (+0x1B8 and +0x240, 0x88 apart)
     * keep the playing entry at +0x38 (12 bytes: id, frames, rate, first, last, flags; rate 0x1000 a frame a frame) and
     * the next at +0x3C; the game takes the rate from the entry every frame (run vr59: the channel's copy at +0x20
     * written 4 times over changed nothing). From the pull to its shot, an entry started since the pull plays 6 times
     * as fast (its rate in the table raised, put back at the shot: the shot's recovery, the game's rate of fire, at its
     * own rate). */
    {
        static int was_quick, n_sped;
        static uint32_t at_pull[2], sped_at[8];
        static uint16_t sped_rate[8];
        const int quick = p && sh3vr_quick_raise();
        if (quick && !was_quick) {
            for (int c = 0; c < 2; c++) at_pull[c] = MEM32(p + 0x1B8 + 0x88 * (uint32_t)c + 0x38);
        }
        for (int c = 0; c < 2 && quick; c++) {
            const uint32_t entry = MEM32(p + 0x1B8 + 0x88 * (uint32_t)c + 0x38);
            if (entry < 0x6C0000 || entry >= 0x6E0000 || entry == at_pull[c] || !MEM16(entry + 4)) continue;
            int seen = 0;
            for (int i = 0; i < n_sped; i++) seen |= sped_at[i] == entry;
            if (seen || n_sped >= 8) continue;
            sped_at[n_sped] = entry;
            sped_rate[n_sped++] = MEM16(entry + 4);
            const uint32_t r = MEM16(entry + 4) * 6u;
            MEM16(entry + 4) = (uint16_t)(r > 0x3000u ? 0x3000u : r);
            rt_log("VR: frame %d: quick gun raise: her animation %03X (frames %u-%u) at %04X, was %04X", rt_frame,
                   MEM16(entry), MEM16(entry + 6), MEM16(entry + 8), MEM16(entry + 4), sped_rate[n_sped - 1]);
        }
        if (!quick) {
            for (int i = 0; i < n_sped; i++) MEM16(sped_at[i] + 4) = sped_rate[i];
            n_sped = 0;
        }
        if (quick != was_quick) rt_log("VR: frame %d: quick gun raise %s", rt_frame, quick ? "from the pull" : "ended");
        was_quick = quick;
    }
    /* 0.1.5: the real time this frame is shown, as sub_0060DA50 gives the update (60 / hz before, which the game's own
     * step setter overwrote with 1/60 s each frame in play) */
    const float secs = sh3vr_frame_seconds();
    if (secs > 0) {
        MEMF(0x070E67A8) = 60.0f * secs;
        MEMF(0x070E67AC) = secs;
        MEM32(0x070E67C0) = 1;
        MEMF(0x070E67C4) = 1.0f / secs;
    }
}

/* float screen_x(int x) (0x682BA0): the 2D's x, in 1/16 units around 2048, to pixels. The 2D keeps a centered
 * 4:3 area; the elements meant to span the screen (ThirteenAG's list, by the call's return address) keep the
 * original mapping. */
static const uint32_t full_width_calls[] = {
    0x468B8A, 0x468BDF, 0x468C25, 0x468C71, 0x468E31, 0x468E78, 0x468EB7, 0x468F0C,
    0x47A684, 0x47A6D8, 0x47A726, 0x47A771, 0x47A7B9, 0x49FAF0, 0x5F6D27, 0x5F80F4,
};
void sub_00682BA0(void) {
    uint32_t from = MEM32(g_esp);
    int32_t x = (int32_t)ARG(0);
    int full = 0;
    for (size_t i = 0; i < sizeof full_width_calls / sizeof *full_width_calls; i++) full |= from == full_width_calls[i];
    double w = (int32_t)guest_call(0x402BA0, 0, NULL, 0);  /* the target's width */
    double left = full ? 256.0 : 192.0 * ws_aspect(), scale = full ? 1.0 / 512 : 1.0 / (384.0 * ws_aspect());
    rt_fpush(w * (x * MEMF(0x68E6E0) - MEMF(0x68E6DC) + left) * scale);
    g_esp += 4;  /* ret: cdecl */
}

/* Cave 0x401ED8, called from 0x5F1D15 in place of `fstp [ecx-0x18]; fld st(0)`: the health indicator's
 * vertices keep their place in the 4:3 area at the right of the screen. */
void sub_00401ED8(void) {
    double x = rt_fpop();
    MEMF(g_ecx - 0x18) = (float)(x * ws_fov() + ws_offset());
    rt_fpush(ST0);
    g_esp += 4;
}

/* The pad's default bindings, (button << 8 | action) shorts at 0x7214E4 that the game copies when there is no
 * savedata/key.ini: laid out as in sh2-nx. Buttons are 0x0C + dinput.c's numbering (X A B Y LB RB LT RT Back
 * Start LS RS); actions 0 search view, 1 ready weapon, 2/3 strafe left/right, 4 map, 5 action, 6 dash, 7 light,
 * 8 pause, 9 zoom, 0x0A supplies, 0x0B inventory. The other 13 entries (stick, camera, menus) stay. */
static const uint16_t pad_defaults[12] = {
    0x0C06, 0x0D05, 0x0E07, 0x0F04,  /* Switch Y dash, A action, B light (and cancel), X map */
    0x1002, 0x1103, 0x1200, 0x1301,  /* L / R strafe, ZL search view, ZR ready weapon */
    0x150B, 0x1408, 0x160A, 0x1709,  /* + inventory, − pause (and skip), stick clicks supplies / zoom */
};
/* VR's (docs/CONTROLS.md): each action keeps its place; the flashlight on LS (the left trigger: sh3vr.cpp map_pad),
 * supplies on B (cancel in menus as always). */
static const uint16_t pad_defaults_vr[12] = {
    0x0C06, 0x0D05, 0x1607, 0x0F04,  /* X dash (left stick click), A action, LS light (left trigger), Y map (X) */
    0x1002, 0x1103, 0x1200, 0x1301,  /* LB / RB strafe (third person: left grip + stick), LT search view (right stick
                                      * click), RT ready weapon (right grip; the right trigger readies and fires) */
    0x150B, 0x1408, 0x0E0A, 0x1709,  /* Start inventory (Y), Back pause and skip (left menu), B supplies, RS zoom (the
                                      * map: right grip) */
};

/* int can_do(what) (0x495B60): the pause menu (calls returning to 0x5E88E2 / 0x5E88F7) asks it whether Save (0x0D)
 * and Load (0x0E) are allowed, and it refuses while the screen fade (type 0x897E38, done flag 0x897E3C) runs. The
 * pause freezes the fade, so pausing during a fade-in (types 5, 9, 13: back from the save screen, a load or a door)
 * left Save / Load greyed out until the menu was closed and opened again. For the pause menu a fade-in counts as
 * done; fade-outs still refuse. */
extern void sub_00495B60_gen(void);
void sub_00495B60(void) {
    uint32_t from = MEM32(g_esp);
    uint8_t type = MEM8(0x897E38), done = MEM8(0x897E3C);
    int fade_in = type == 5 || type == 9 || type == 13;
    if (fade_in && (from == 0x5E88E2u || from == 0x5E88F7u)) MEM8(0x897E3C) = 1;
    sub_00495B60_gen();
    MEM8(0x897E3C) = done;
}

/* LIST 37: what the game was doing, for sh3-crash.log (rt_fatal): the frame, the game mode, a demo or cutscene,
 * Heather's place, health and weapon, and VR's state. Read from guest memory that is always committed (the
 * exe's data), Heather's struct only once her pointer checks out. */
void game_crash_info(FILE *f) {
    if (!g_xbox_code_hi) { fprintf(f, "game: sh3.exe not loaded yet\n"); return; }
    fprintf(f, "game: frame %u, mode %d%s, demo status %u, cutscene %u, fade type %u done %u\n", frame_no,
            (int)last_game_mode, last_game_mode == 5 ? " (playable)" : "", MEM32(0x898308), MEM8(0x81FAB5),
            MEM8(0x897E38), MEM8(0x897E3C));
    uint32_t p = MEM32(0x712C100);
    if (p >= 0x10000u && p < 0x0F000000u && MEM16(p + 0x80) == 0x0100)
        fprintf(f, "Heather: at %.1f %.1f %.1f yaw %.2f, health %.0f / %.0f, weapon %u\n", MEMF(p), MEMF(p + 4),
                MEMF(p + 8), MEMF(p + 0x14), MEMF(p + 0x180), MEMF(p + 0x184), MEM8(0x712C656));
    else fprintf(f, "Heather: not in play\n");
    sh3vr_crash_info(f);
}

/* ---- LIST 5: the movies (src/game/movie.c plays them; docs/GAME_FACTS.md, Movies) ----
 * The game's player (0x00403C80-0x00404C13) is DirectShow with a source filter that unscrambles the .000 files and a
 * renderer filter that fills a texture. The movie scene (0x0059C860, started by 0x0059C9C0(index)) calls the start
 * 0x00404B40(path) (0: playing), then each frame the step 0x00404440 and the state 0x00403DF0 (the byte 0x0072C064:
 * 3 while playing), and the stop 0x00403DC0 when the state is not 3 or a button skips it. The volume the scene set with
 * 0x00403E00 (its music level, 0..1) waits at 0x0072C06C. */
int sh3_movie_start(const char *path, float volume);
int sh3_movie_frame(void);
void sh3_movie_stop(const char *why);
void sub_00404B40(void) {
    char host[512];
    const char *guest = GSTR(ARG(0));
    const int ok = guest && sh3_movie_start(rt_path(guest, host, sizeof host), MEMF(0x0072C06Cu));
    MEM8(0x0072C064u) = ok ? 3 : 1;
    g_eax = ok ? 0 : 1;
    g_esp += 4;  /* ret: cdecl */
}
void sub_00404440(void) {
    if (!sh3_movie_frame() && MEM8(0x0072C064u) == 3) MEM8(0x0072C064u) = 2;  /* its end: the scene stops it */
    g_esp += 4;
}
void sub_00403DC0(void) {
    sh3_movie_stop("the game's movie scene ended it: skipped or over");
    MEM8(0x0072C064u) = 1;
    g_eax = 0;
    g_esp += 4;
}

/* ---- D3DX 8's x87 math, done natively (0.1.5, owner: "COMPLETELY MAXIMIZE PERFORMANCE") ----
 * The hottest code on the game's thread (vr264's profile: 15% of it) is five of D3DX's x87 routines, which D3DX reaches
 * through the function pointers it fills at start-up (so by address: recomp_lookup_manual below): the 4x4 matrix
 * product (0x648639), a point by a matrix to x, y, z, w (0x6483A7), a 4-vector by a matrix (0x648491), normalize
 * (0x64856F, 1/sqrt from its table at 0x726820) and a direction by a matrix (0x648432). The lifted bodies walk the
 * emulated x87 stack an instruction at a time; these do the same multiplies and adds in the same order, each rounded
 * as the lifted code rounds it (RECOMP_FP_PC: to 24 bits while the game's precision setting asks), with every input
 * read before the first store as the originals do (so an output that is also an input comes out the same), and leave
 * the registers as the originals do: eax the output, the arguments popped (stdcall), the framed two's ebp published.
 * Each result is bit for bit the lifted code's: SH3_D3DX_CHECK=1 runs both on every call and logs any difference. */
#pragma STDC FP_CONTRACT OFF
#define DX_MEM(va) ((void *)XBOX_PTR(va))
#define DX_PC24 const int pc24 = !(g_fp_control_word & 0x300u)
#define DXP(x) dx_round(pc24, (x))
static inline double dx_round(int pc24, double x) { return pc24 ? recomp_fp_round24(x) : x; }

/* D3DXVec3Transform (out: x y z w, v: x y z, m): out = v * m with v's w = 1. */
static void dx_vec3_transform(void) {
    DX_PC24;
    const uint32_t out = ARG(0);
    float v[3], m[16], r[4];
    memcpy(v, DX_MEM(ARG(1)), sizeof v);
    memcpy(m, DX_MEM(ARG(2)), sizeof m);
    for (int i = 0; i < 4; i++) {
        double s = DXP(DXP((double)v[0] * m[i]) + DXP((double)v[1] * m[4 + i]));
        s = DXP(s + DXP((double)v[2] * m[8 + i]));
        r[i] = (float)DXP(s + m[12 + i]);
    }
    memcpy(DX_MEM(out), r, sizeof r);
    g_eax = out;
    g_esp += 16;  /* ret 12 */
}

/* D3DXVec3TransformNormal (out: x y z, v: x y z, m): out = v * m's 3x3. */
static void dx_vec3_transform_normal(void) {
    DX_PC24;
    const uint32_t out = ARG(0);
    float v[3], m[12], r[3];
    memcpy(v, DX_MEM(ARG(1)), sizeof v);
    memcpy(m, DX_MEM(ARG(2)), sizeof m);
    for (int i = 0; i < 3; i++) {
        const double s = DXP(DXP((double)v[0] * m[i]) + DXP((double)v[1] * m[4 + i]));
        r[i] = (float)DXP(s + DXP((double)v[2] * m[8 + i]));
    }
    memcpy(DX_MEM(out), r, sizeof r);
    g_eax = out;
    g_esp += 16;  /* ret 12 */
}

/* D3DXVec4Transform (out: x y z w, v: x y z w, m): out = v * m. */
static void dx_vec4_transform(void) {
    DX_PC24;
    const uint32_t out = ARG(0);
    float v[4], m[16], r[4];
    memcpy(v, DX_MEM(ARG(1)), sizeof v);
    memcpy(m, DX_MEM(ARG(2)), sizeof m);
    for (int i = 0; i < 4; i++) {
        double s = DXP(DXP((double)v[0] * m[i]) + DXP((double)v[1] * m[4 + i]));
        s = DXP(s + DXP((double)v[2] * m[8 + i]));
        r[i] = (float)DXP(s + DXP((double)v[3] * m[12 + i]));
    }
    memcpy(DX_MEM(out), r, sizeof r);
    g_eax = out;
    g_esp += 16;  /* ret 12 */
}

/* D3DXMatrixMultiply (out, a, b): out = a * b; out may be a or b (the original copies b aside when it is both, and
 * goes column by column when it is b: the same products and sums either way). */
static void dx_matrix_multiply(void) {
    DX_PC24;
    const uint32_t out = ARG(0);
    float a[16], b[16], r[16];
    memcpy(a, DX_MEM(ARG(1)), sizeof a);
    memcpy(b, DX_MEM(ARG(2)), sizeof b);
    for (int i = 0; i < 4; i++) {
        const float *row = a + 4 * i;
        for (int j = 0; j < 4; j++) {
            const double p0 = DXP((double)row[0] * b[j]), p1 = DXP((double)row[1] * b[4 + j]),
                         p2 = DXP((double)row[2] * b[8 + j]), p3 = DXP((double)row[3] * b[12 + j]);
            r[4 * i + j] = (float)DXP(DXP(p0 + p2) + DXP(p1 + p3));
        }
    }
    memcpy(DX_MEM(out), r, sizeof r);
    g_ebp = g_seh_ebp = g_esp - 4;  /* the frame the original publishes and leaves */
    g_eax = out;
    g_esp += 16;  /* ret 12 */
}

/* D3DXVec3Normalize (out, v): a zero vector gives zeros, one within 1e-5 of unit length is copied, any other is scaled
 * by 1/sqrt(length squared) from the table (a line through the mantissa) and the exponent's bits. */
static void dx_vec3_normalize(void) {
    DX_PC24;
    const uint32_t out = ARG(0), in = ARG(1);
    float v[3];
    memcpy(v, DX_MEM(in), sizeof v);
    const double len = DXP(DXP(DXP((double)v[0] * v[0]) + DXP((double)v[1] * v[1])) + DXP((double)v[2] * v[2]));
    const float lf = (float)len;
    uint32_t lb;
    memcpy(&lb, &lf, 4);
    if (!lb) {
        static const uint32_t zero[3];
        memcpy(DX_MEM(out), zero, sizeof zero);
        g_fp_top = (g_fp_top + 7) & 7;  /* the original's ffree st(0) leaves the length on the x87 stack */
        g_fp_stack[g_fp_top] = len;
    } else {
        const float d = (float)DXP(len - 1.0);
        uint32_t db;
        memcpy(&db, &d, 4);
        if ((int32_t)(db & 0x7FFFFFFFu) > 0x3727C5AC) {
            const uint32_t fb = (lb & 0xFFFFFFu) | 0x3F000000u, eb = ((0xBEFFFFFFu - lb) >> 1) & 0xFF800000u;
            const uint32_t at = 0x726820u + ((lb >> 12) & 0xFF8u);
            float f, e;
            memcpy(&f, &fb, 4);
            memcpy(&e, &eb, 4);
            double s = DXP((double)f * MEMF(at));
            s = DXP(s + MEMF(at + 4));
            s = DXP(s * e);
            const float r[3] = {(float)DXP(v[0] * s), (float)DXP(v[1] * s), (float)DXP(v[2] * s)};
            memcpy(DX_MEM(out), r, sizeof r);
        } else if (out != in)
            memcpy(DX_MEM(out), v, sizeof v);
    }
    g_ebp = g_seh_ebp = g_esp - 4;  /* the frame the original publishes and leaves */
    g_eax = out;
    g_esp += 12;  /* ret 8 */
}

/* SH3_D3DX_CHECK=1 (the PC rig): the lifted body runs first on the same inputs, then the native one on the restored
 * output (which restores an input that is the output too); any difference is logged and the lifted result kept. */
static void dx_checked(const char *what, void (*native)(void), void (*lifted)(void), int bytes) {
    static int calls, differ;
    const uint32_t out = ARG(0), esp0 = g_esp;
    const int top0 = g_fp_top;
    uint8_t before[64], want[64];
    memcpy(before, DX_MEM(out), bytes);
    lifted();
    const uint32_t eax = g_eax, esp = g_esp, ebp = g_ebp;
    const int top = g_fp_top;
    const double pushed = g_fp_stack[top];
    memcpy(want, DX_MEM(out), bytes);
    memcpy(DX_MEM(out), before, bytes);
    g_esp = esp0;
    g_fp_top = top0;
    native();
    calls++;
    const uint8_t *got = DX_MEM(out);
    if (memcmp(want, got, bytes) || eax != g_eax || esp != g_esp || ebp != g_ebp || top != g_fp_top ||
        (top != top0 && memcmp(&pushed, &g_fp_stack[top], sizeof pushed))) {
        int word = 0;
        while (word < bytes / 4 && !memcmp(want + 4 * word, got + 4 * word, 4)) word++;
        uint32_t w = 0, g = 0;
        if (word < bytes / 4) {
            memcpy(&w, want + 4 * word, 4);
            memcpy(&g, got + 4 * word, 4);
        }
        if (++differ <= 20)
            rt_log("D3DX check: %s differs from the lifted code at call %d: word %d lifted %08X native %08X, eax %08X/%08X,"
                   " esp %08X/%08X, ebp %08X/%08X, x87 top %d/%d", what, calls, word, w, g, eax, g_eax, esp, g_esp, ebp,
                   g_ebp, top, g_fp_top);
        memcpy(DX_MEM(out), want, bytes);
        g_eax = eax;
        g_esp = esp;
        g_ebp = g_seh_ebp = ebp;
        g_fp_top = top;
        g_fp_stack[top] = pushed;
    }
    if (calls == 1000 || calls % 200000 == 0)
        rt_log("D3DX check: %d calls checked, %d differ from the lifted code", calls, differ);
}
#define DX_CALL(what, native, lifted, bytes) \
    do { if (RT_ENV("SH3_D3DX_CHECK")) dx_checked(what, native, lifted, bytes); else native(); } while (0)
extern void sub_006483A7_gen(void);
void sub_006483A7(void) { DX_CALL("Vec3Transform (0x6483A7)", dx_vec3_transform, sub_006483A7_gen, 16); }
extern void sub_00648432_gen(void);
void sub_00648432(void) { DX_CALL("Vec3TransformNormal (0x648432)", dx_vec3_transform_normal, sub_00648432_gen, 12); }
extern void sub_00648491_gen(void);
void sub_00648491(void) { DX_CALL("Vec4Transform (0x648491)", dx_vec4_transform, sub_00648491_gen, 16); }
extern void sub_0064856F_gen(void);
void sub_0064856F(void) { DX_CALL("Vec3Normalize (0x64856F)", dx_vec3_normalize, sub_0064856F_gen, 12); }
extern void sub_00648639_gen(void);
void sub_00648639(void) { DX_CALL("MatrixMultiply (0x648639)", dx_matrix_multiply, sub_00648639_gen, 64); }
#pragma STDC FP_CONTRACT DEFAULT

/* Called once after sh3.exe is loaded: the variables tools/patch_exe.py points code at. */
void game_init(void) {
    host_arena_commit(0x0EC00000u, 0x1000);
    MEMF(0x0EC00000u) = ws_aspect() > 1.5 ? 0.0f : 0.5f;  /* cutscene borders: none on a wide screen */
    /* SH3 Quest VR: VR's own bindings (savedata/keyvr.ini, kernel32.c) start from VR's defaults (docs/CONTROLS.md). */
    memcpy(GPTR(0x7214E4), sh3vr_enabled() ? pad_defaults_vr : pad_defaults, sizeof pad_defaults);
}

/* Indirect calls and the dispatch table reach the generated bodies (sub_X_gen) of wrapped functions: calls
 * by address are routed here first. */
recomp_func_t recomp_lookup_manual(uint32_t va) {
    switch (va) {
    case 0x00661D20u: return sub_00661D20;
    case 0x0066F9C0u: return sub_0066F9C0;
    case 0x004193A0u: return sub_004193A0;
    case 0x00419760u: return sub_00419760;
    case 0x00416AE0u: return sub_00416AE0;
    case 0x00416B70u: return sub_00416B70;
    case 0x0043AB80u: return sub_0043AB80;
    case 0x0043ABC0u: return sub_0043ABC0;
    case 0x004954E0u: return sub_004954E0;
    case 0x00495360u: return sub_00495360;
    case 0x00682BA0u: return sub_00682BA0;
    case 0x00401ED8u: return sub_00401ED8;
    case 0x00495B60u: return sub_00495B60;
    case 0x004162A0u: return sub_004162A0;
    case 0x0067B9B0u: return sub_0067B9B0;
    case 0x004B9E70u: return sub_004B9E70;
    case 0x004D83B0u: return sub_004D83B0;
    case 0x004B3D50u: return sub_004B3D50;
    case 0x0041B2C0u: return sub_0041B2C0;
    case 0x004ACE10u: return sub_004ACE10;
    case 0x004B6A20u: return sub_004B6A20;
    case 0x004B6810u: return sub_004B6810;
    case 0x004D44E0u: return sub_004D44E0;
    case 0x005A5E40u: return sub_005A5E40;
    case 0x004B33E0u: return sub_004B33E0;
    case 0x0045EC00u: return sub_0045EC00;
    case 0x0045ED40u: return sub_0045ED40;
    case 0x00457AC0u: return sub_00457AC0;
    case 0x0041C090u: return sub_0041C090;
    case 0x0046D130u: return sub_0046D130;
    case 0x004B9940u: return sub_004B9940;
    case 0x00402B60u: return sub_00402B60;
    case 0x00494C40u: return sub_00494C40;
    case 0x004B5550u: return sub_004B5550;
    case 0x00497780u: return sub_00497780;
    case 0x00497800u: return sub_00497800;
    case 0x0046CDF0u: return sub_0046CDF0;
    case 0x0046CFE0u: return sub_0046CFE0;
    case 0x004A0500u: return sub_004A0500;
    case 0x00471800u: return sub_00471800;
    case 0x0060BAE0u: return sub_0060BAE0;
    case 0x00402E00u: return sub_00402E00;
    case 0x00404B40u: return sub_00404B40;
    case 0x00404440u: return sub_00404440;
    case 0x00403DC0u: return sub_00403DC0;
    case 0x00601A40u: return sub_00601A40;  /* LIST 12 (session 13): the click zones at the game's own question */
    case 0x0060DA50u: return sub_0060DA50;  /* 0.1.5: the game's step from the real frame time */
    case 0x0041E200u: return sub_0041E200;  /* 0.1.5: the depth of field left out in VR */
    case 0x0041FF80u: return sub_0041FF80;  /* 0.1.5: the shaking double picture left out in VR */
    case 0x0049FAC0u: return sub_0049FAC0;  /* 0.1.5: the lamps' test from the frame camera in VR, no read back */
    case 0x0043A0F0u: return sub_0043A0F0;  /* 0.1.6: the models' culling tested again from the head (0x454670 calls it
                                             * by address: without this line the wrapper is never reached, vr286) */
    case 0x006483A7u: return sub_006483A7;  /* 0.1.5: D3DX's math, natively */
    case 0x00648432u: return sub_00648432;
    case 0x00648491u: return sub_00648491;
    case 0x0064856Fu: return sub_0064856F;
    case 0x00648639u: return sub_00648639;
    }
    return NULL;
}
