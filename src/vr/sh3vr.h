/* SH3 Quest VR: the headset as the game (C: sh3.c, d3d8, dinput) sees it. sh3vr.cpp holds it, on top of the
 * Ratchet/Jak headset layer (xr.h: OpenXR on the Quest, xr_sim.cpp's scripted headset on a PC).
 *
 * Spaces: the game's world is Y down, Z forward, X right, 490 units a metre (Heather's own scale; the PC VR mod used 360); its camera space
 * is the same way round (x right, y down, z forward) and its D3D projection flips y. OpenXR's tracking space is Y up,
 * -Z forward, metres: a tracking direction (x, y, z) is the game's (x, -y, -z). */
#pragma once
#include <stdint.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---- life ---- */
/* After the GL context exists (gl_context_open): starts the headset unless VR is off. 1: VR runs (every render
 * target is per eye from here on). */
int sh3vr_start(void);
int sh3vr_active(void);
/* VR is to be used this run (sh3vr.ini; before the headset starts, e.g. for the pad bindings in game_init). */
int sh3vr_enabled(void);
/* The game's resolution while VR runs (each eye's picture), for rt_res_w/h. */
void sh3vr_eye_size(uint32_t *w, uint32_t *h);
/* Present: the frame the game drew (the back buffer, a 2-layer GL_TEXTURE_2D_ARRAY of w x h, and its depth, the same
 * kind, or 0) goes to the eyes (with VR's hands, the guide dot and the comfort shading drawn in) and the panel's picture
 * to the panel; then the next frame is waited for. Returns 1 when the desktop window should show the left eye. */
int sh3vr_present(unsigned backbuffer_tex, unsigned depth_tex, int w, int h);
/* The system ended the app (the Quest's quit): leave. */
int sh3vr_wants_exit(void);
/* LIST 37: VR's part of sh3-crash.log (view, options in force, the last poses, what VR asked of the game). */
void sh3vr_crash_info(FILE *f);

/* ---- the camera (sh3.c) ---- */
/* 0x4162A0 SetViewMatrix: `m` (the game's D3D row-vector view, 16 floats) about to be set; `main` is 1 for the frame's
 * camera (called from 0x43AD30). Replaced in place by the head's view where VR's view applies. */
void sh3vr_view(float m[16], int main);
/* After the camera block's rebuild (0x67B9B0): its view (0x72C3A0) and projection (0x72C360). */
void sh3vr_camera_built(const float view[16], const float proj[16]);
/* 0x43AB80 (axis 0: horizontal) / 0x43ABC0 (1: vertical) for the frame's camera: the game's projection scale, made
 * wide enough for both eyes while VR's view applies. */
double sh3vr_proj_scale(int axis, double game_value);
/* 1 while the 2D is laid out for the panel: the game's 2D as if on a 4:3 screen (sh3.c's widescreen stays out). */
int sh3vr_flat_2d(void);
/* Once a frame (sh3.c, from Present): what the game is doing, read from its memory. */
typedef struct Sh3VrGame {
    int player_valid;       /* Heather's work struct is there (0x712C100, +0x80 == 0x0100) */
    float pos[3];           /* her feet, game units */
    float yaw;              /* her facing (+0x14), radians: forward = (sin yaw, 0, cos yaw) */
    float hp, hp_max;
    int weapon;             /* 0x712C656: the weapon drawn: 1 handgun, 2 shotgun, 3 submachine gun, 4 knife, 5 steel pipe,
                             * 6 katana, 7 maul, 8 stun gun, 9+ the extras (docs/GAME_FACTS.md, Weapons); -1 none (0) */
    int weapon_slot;        /* 0x712CABF: the slot equipped (the same numbers), -1 none */
    uint32_t weapons_owned; /* bit s: the player has weapon s (its item 0x6BDD24[s] set in the bits at 0x712CA80) */
    int playable;           /* the event dispatch's playable update ran this frame (it returned 5) */
    int cutscene;           /* 0x81FAB5 */
    int demo;               /* a demo (real-time cutscene) runs: 0x898308 == 2 */
    float cam_view[16];     /* the game's own camera this frame (its view before VR's), D3D row-vector */
    int cam_valid;
    /* LIST 46: the gun in her hand: its item (9 stun gun, 10 handgun, 11 shotgun, 12 submachine gun; 0 none or not one
     * that reloads), its rounds loaded (short[0x712CA8E + 2 item]), the reserve (its ammo item's, item + 5) and how many
     * it takes (byte[0x6BDD17 + item]) */
    int gun_item;
    int loaded, reserve, capacity;
    int life_display;       /* LIST 49: the game's Life Display option is on (Extra options; bit 0x80 of 0x70E66C6, 0x60CBC0) */
    int life_gauge;         /* owner message 37: the game's Life Gauge (health and stamina bars) is on: byte 0x70E6724 == 1 */
    /* LIST 49 (owner message 37): her stamina, 0..1: 1 - her tiredness 0x712C620 over its most 0x712C624 (it grows as
     * she moves, 0x4B3F60; past 30% of its most she breathes hard, 0x4D4DB1; healing items clear it) */
    float stamina;
    /* LIST 12 (session 12): the game's click zones on the screen up (0x601970 adds them, 0x601940 clears them; a click
     * acts only over one, 0x601A40): how many there are and the id of the one under the game's cursor (0 none) */
    int click_zones;
    int click_zone;
    /* 0.1.7 (owner 140): her hit reaction (a flinch, a knockdown and getting up, a monster's hold): her movement state
     * 0x712C190 when it is one of the game's reaction states (34, 35, 36: 0x4D9E60 runs it, 0x4C7190 picks its
     * animation), else 0. The game moves and turns her itself: VR's walk and turn wait (sh3.c her_reacting). */
    int reacting;
    /* 0.1.8 (owner 152): a gesture of the game's own plays (an item's pick-up bending down or reaching, the game's
     * reload: her action 0x28, sh3.c her_gesture): VR's walk and turn wait, as for a reaction. */
    int gesture;
    /* 0.1.8 (owner 152): an event of the game's holds her (an examine's message up, a door, a pick-up: sh3.c
     * event_hold): VR's walk and turn wait, the Sprint toggle's run ends, the room's walk does not follow the head. */
    int event_hold;
    /* 0.1.9 (owner 168, the puzzles' hint): the game's mode, its main loop's own step (0x58EDB0 switches on it, the
     * event dispatch 0x4B9E70 returns the next: 2 a room's load, 5 play, 6 the inventory, 14 its Options, 19 the save
     * screen; -1 not known yet), and the mode an event's script handed over to (the dispatch returned it while a script
     * ran, its step 2 or 5 before the call: a puzzle's screen; the save point's save screen 19, vr473), -1 once the
     * dispatch hands back play or a room's load. A screen the player opens (the inventory, the map, the pause menu) is
     * never one: it opens from free play. */
    int game_mode;
    int event_mode;
} Sh3VrGame;
void sh3vr_game_state(const Sh3VrGame *g);
/* What VR asks of Heather this frame (sh3.c applies it in the movement code, 0x4D83B0, and her facing). */
typedef struct Sh3VrMove {
    int steer;              /* the stick or the room walk moves her: her movement goes along `heading` */
    float heading;          /* world yaw of the movement */
    float speed;            /* 0..1 */
    int run;
    int face;               /* set her facing to `face_yaw` (first person: the body turns with the head) */
    float face_yaw;
    int aim;                /* the gun points along `aim_dir` from the weapon (0x4B3D50) */
    float aim_dir[3];       /* world, unit */
    int hold_still;         /* a heavy swing (pipe, katana, maul) is under way: no movement (the PC VR mod's finding) */
} Sh3VrMove;
void sh3vr_move(Sh3VrMove *out);
/* 0.1.7: 1 while the game is in play (its playable update, no cutscene, demo or VR menu) */
int sh3vr_in_play(void);
/* What VR asks of the game once (sh3.c, at the frame's end): a holster drawn from equips its weapon. */
typedef struct Sh3VrActions {
    int equip_slot;         /* -1 nothing; else the weapon to equip (0x497A70 with its item, then 0x4711D0) */
    int put_away;           /* 1: the weapon in her hand put away (0x497A70(0), 0x4711D0): an extra weapon holstered */
    /* LIST 46, the hands' reload: */
    int reload_commit;      /* 1: a magazine or battery seated: the game's transaction 0x497860(gun item), HUD figure */
    int load_rounds;        /* shells pushed in (the shotgun): each from the reserve into the gun while it has room */
    int sound_event;        /* -1 none; else a motion event's sound (the reload's: 0x5680F0(chr, chr + 0x60, 0, it)) */
} Sh3VrActions;
void sh3vr_actions(Sh3VrActions *out);
/* LIST 46: 1 while the hands reload the gun ("Manual reload" on, first person, the hands tracked): Heather's attack
 * decision (0x494C40) then never starts the game's own reload. */
int sh3vr_block_game_reload(void);
/* LIST 34, "Spatial sound" (on): the game's 3D sounds placed from the head (sh3.c gives its sound code the eyes' view
 * as the listener, and the eyes' place for distance) whenever the eyes have a view of their own (not the flat screen). */
int sh3vr_spatial_sound(void);
/* "Film grain" (session 7): whether the game's own noise effect is drawn (off by default; sh3.c 0x41C090). */
int sh3vr_film_grain(void);
/* "Heather's shadow" (LIST 56, session 12): her IK body, her weapon and a reload part held, as shadow volumes from the
 * flashlight for the game's own stencil shadows: triangles (x y z each, world) wound as D3D's front faces outward for
 * the game camera given (view, proj: the game's volume pass's); 0 none this frame. d3d8.c draws them. */
int sh3vr_her_shadow(const float view[16], const float proj[16], float *out_xyz, int max_tris);
/* 0.1.7 (owner 123: "heathers normal shadow seems to be rendered weirdly"): a volume draw of the game's shadow pass
 * (its corners: stride bytes apart, x y z first, world) and the lamp of the record it is cast from: 1 when it is
 * Heather's own (her kg1 shadow in her animation's pose) and VR casts her shadow from that lamp itself (d3d8.c leaves
 * the draw out and counts sh3vr_her_room_shadow's volumes before the darkening). */
int sh3vr_her_game_volume(const uint8_t *verts, uint32_t stride, uint32_t n, const float lamp[3]);
/* Her shadow from that lamp: her body and the weapon in her hand as drawn this frame, closed volumes (x y z each,
 * world) wound as D3D's front faces outward for the game camera given, counted the z-fail way; 0 none. */
int sh3vr_her_room_shadow(const float view[16], const float proj[16], const float lamp[3], float *out_xyz, int max_tris);
/* Session 14 (owners 62-64): a skinned draw of Heather's in the eyes (her part; kind 1 the weapon or a part in her hand)
 * as drawn: its vertices (stride 48), its faces (3 local indices each) and the 96 vertex constants its shader was given;
 * key tells one draw from another across frames. Her shadow is cast from her own mesh from these. */
void sh3vr_shadow_part(const uint8_t *verts48, int nverts, const uint16_t *tris, int ntris, const float *consts,
                       uint32_t key, int kind);
/* The game reloaded Heather's gun itself (0x4B5550): the motion event's index, whose sound a reload by hand plays. */
void sh3vr_game_reload(int weapon, int event_index);
/* The display's rate the game is paced at (Hz): the frame-time tuple at 0x70E67A8 is written for it. 0: not paced. */
float sh3vr_display_hz(void);
/* 0.1.5: the real time this frame stands for, from the display times (0 when the headset does not pace the game): the
 * game's own step (sh3.c sub_0060DA50), so it runs at its true speed whatever the frame rate. */
float sh3vr_frame_seconds(void);
/* Heather fired a gun (0x4B3D50 for her): haptics, recoil for two hands. */
void sh3vr_notify_shot(void);
/* Quick gun raise (session 6): 1 from a gun's trigger pull in first person until its shot (1 s at most): sh3.c runs
 * Heather's animations that started since the pull (the raise, the shot's start) faster meanwhile. */
int sh3vr_quick_raise(void);
/* The game's weapon place (0x4954E0: a world point on its weapon and a direction, for its shots, the muzzle flash, its
 * light and effects) moved onto the weapon drawn in the hand (first person). 1 when moved; the direction keeps its length.
 * from_hand: read from her animated hand bone (mode 0), not the weapon's own matrix (mode 1, moved by sh3vr_weapon_world). */
int sh3vr_weapon_to_hand(float point[3], float dir[3], int from_hand);
/* The drawn weapon's matrix (D3D row-vector 4x4, in her space; base: her world matrix, the object's +0x20) as the game
 * attaches it to her animated hand (0x495360): moved to
 * the gun in the VR hand in first person, so every effect the game places from it follows the gun (session 6). */
int sh3vr_weapon_world(float m[16], const float base[16]);
/* The shot's line in the world: the muzzle of the weapon in the hand and the barrel's direction (the guide dot's line,
 * the recoil's kick not in it). 0: no gun in the hand, or aiming by hand is off. */
int sh3vr_shot_line(float from[3], float dir[3]);

/* ---- combat (sh3.c: clBattleAddQue 0x4ACE10 and the battle step 0x4B33E0) ---- */
typedef struct Sh3VrHit {
    int kind;               /* 2 or 3: the game's melee volumes */
    int battle_id;          /* the attack (its damage, knock-back, sound) */
    float prev_grip[4], prev_tip[4], cur_grip[4], cur_tip[4];  /* the swept segment, world, w = 1 */
    float radius;           /* game units: the segment is lengthened by this at both ends (as the PC VR mod) */
} Sh3VrHit;
/* Heather's melee volume (kind 2/3) the game queues for its own attack: 1 when VR replaces it with the swing that
 * started that attack (`hit` rewritten in place), 2 when it is to be dropped (that swing has already hit: owner 65).
 * Learns the weapon's attack ids for bare hands, guns as clubs and the hits at contact. */
int sh3vr_melee_volume(int weapon_slot, Sh3VrHit *hit);
/* LIST 50 (sh3.c around 0x4B6A20, the monsters' hits on Heather gathered into her pending damage): what a hit keeps of
 * its damage (0..1) as her raised weapon or arm guards it. World: the hit's point (+0x134) and way (+0x144), the
 * attacker's place (its +0, when has_from); kind the hit's reaction (+0x154). */
float sh3vr_block(const float at[3], const float way[3], const float from[3], int has_from, float damage, int kind);
/* At the battle step's start: the next hit VR adds (a punch, a gun swung as a club), 0 when none. */
int sh3vr_extra_hit(Sh3VrHit *out);
/* testing (0.1.7, the PC rig, SH3_HOLD_HIT): one sweep of Heather's knife attack sent to the next battle step (world, game
 * units: the previous and the current grip-to-tip segments, its radius), as a swing's contact sweep is; 1 when sent */
int sh3vr_test_sweep(const float pg[3], const float pt[3], const float cg[3], const float ct[3], float radius);
/* Owner 65 (session 14): Heather's hits as the game recorded them after the battle step (0x4AEA90's records of kind 4:
 * the character hit, the point, the way = the tip's motion, the attack id): VR tells its own swings' hits from them
 * (one hit a swing; the impact's haptic, sound and log). */
void sh3vr_melee_landed(const float way[3], const float point[3], uint32_t character, int battle_id);
/* The damage a character other than Heather took this frame from her (0x4B6A20 for that character): what it keeps
 * (a bare fist's hit half of the weakest melee weapon's, LIST 27), and the hit's way and point logged. */
float sh3vr_hit_given(uint32_t character, const float way[3], const float point[3], float damage, int reaction);

/* ---- the renderer (d3d8) ---- */
/* Whether a draw used the frame camera's projection: a fixed-function draw's PROJECTION (D3D row-vector), or a vertex
 * shader's view x projection as uploaded to c2-c5 (transposed: rows of the D3D matrix's columns). */
int sh3vr_is_main_proj(const float proj[16]);
int sh3vr_is_main_vp_constants(const float c2_c5[16]);
/* The two eyes' clip-space corrections for a draw of the frame camera: 32 floats, two column-major mat4s (uEye).
 * Identity pairs for anything else. */
const float *sh3vr_eye_correction(void);
const float *sh3vr_eye_identity(void);
/* A skinned indexed draw (NumVertices nv, primitives np, stream 0's stride): Heather's weapon (the PC VR mod's mesh
 * signatures from data/chrwp.arc) gets its bone palette (c48 on, 3 rows a bone) moved to the hand holding it, or
 * hidden while holstered. pal_count: the palette slots the draw uploaded (0: not a palette shader); pal_bone[slot]:
 * the model bone each slot holds (-1 not known; d3d8.c). 1 when `vsc` was changed (the draw uploads that copy); 2 for
 * the beam saber's blade, moved too (a glow: not in her shadow, d3d8.c her_shadow_capture); 3 for the weapon in a mirror's
 * reflection of her (0.1.9: held as hers through the reflection; marked as hers, not her shadow's caster). */
int sh3vr_weapon_draw(uint32_t nv, uint32_t np, uint32_t stride, int pal_count, const int *pal_bone, float vsc[96][4]);
/* LIST 24, 46 (the moving parts drawn whole, the parts in the hand): the weapon's draw in passes, each through the
 * game's own shader, lights and textures (the PC VR mod moved vertices in the buffer, which stretched the faces a moving
 * part shares with the body). A pass is a set of faces: the game's own (local triangle indices from the draw's first
 * vertex) or VR's own mesh (`verts`, the game's stride-48 layout: position, weight 1, bone byte at +24, normal, uv),
 * with its own texture or the game's; sh3vr_weapon_draw places each pass by its own bone palette (sh3vr_weapon_pass
 * says which pass is drawn next). The draw's faces as a triangle list (local) go in; 0 passes: drawn as the game does. */
typedef struct Sh3VrWeaponPass {
    const uint16_t *tris;   /* 3 local indices a face */
    int ntris;              /* faces */
    const float *verts;     /* VR's own vertices (12 floats each), NULL: the game's */
    int nverts;
    unsigned texture;       /* a GL texture for stage 0, 0: the game's */
} Sh3VrWeaponPass;
int sh3vr_weapon_passes(uint32_t nv, uint32_t np, const uint8_t *verts48, const uint16_t *tris, int ntris,
                        Sh3VrWeaponPass *out, int max);
void sh3vr_weapon_pass(int pass);
/* The flashlight (session 7; sh3.c wraps the game's copy of Heather's light into its light buffer, 0x457AC0): the
 * beam's position and direction (+0x40, +0x50) and its shadow caster's (+0x60, +0x70), world, changed in place in
 * first person (on her chest: aimed where the eyes look; in a hand: from its lens); 1 when changed. */
int sh3vr_flashlight_place(float *pos, float *dir, float *pos2, float *dir2);
/* The light's level this frame (0 off .. 1 on, 0 while a room's script holds it dark; sh3.c, 0x457AC0), for: */
void sh3vr_flashlight_level(float level);
/* Her flashlight on each point of her draw (session 9: one light for a whole draw lit her forearm and left the hand
 * beside it flat grey, vr136 and vr139): the lamp (world; w 1 on, 0 none), the beam's way (w its light at full: the
 * level times the light's strength), the cone and reach (x whole to, y none past, game units from the lamp; z the
 * cosine off the beam's line none past, w the cosine whole within), the frame camera's clip space to the world
 * (column-major). The vertex stage puts it in the fourth light per point (d3d8_shader.c). */
typedef struct Sh3VrBeam {
    float pos[4], dir[4], cone[4], clip_to_world[16];
} Sh3VrBeam;
/* Her own skinned draw (or the weapon's in her hand) in first person lit by her flashlight: the beam put in the
 * shader's free fourth directional light (c11, c12-c14 w; sh3vr_ik.cpp body_light), toward the draw's middle, and per
 * point in `beam` (zeroed by the caller; pos w 1 when filled). 1 when `vsc` was changed. */
int sh3vr_her_light(float vsc[96][4], int pal_count, Sh3VrBeam *beam);
/* The flashlight held in a hand: bone 4's world matrix (a D3D row-vector 4x4, as the game keeps bones) that puts its
 * model (Heather's part 48, group 32, skinned to bone 4 alone) there; 0 while it is on her chest. */
int sh3vr_flashlight_bone(float m[16]);
/* sh3.c around the flashlight's own draw in the hand (sh3vr_body_cut fades it only right at the eyes, as a weapon). */
void sh3vr_item_pass(int on);
/* What of a draw is cut or faded (LIST 19, 41, 43): Heather's own skinned draws in first person (her head out of the
 * eyes, her body faded where it would block them; her hands never), and the ghost hand's pass. */
typedef struct Sh3VrCut {
    float near_cut;           /* game units from the eyes: nearer is cut (0 none) */
    float sphere[4];          /* world x, y, z, radius (0 none): inside is cut (her head) */
    float cap_b[4];           /* w 1: the cut is a capsule from sphere's point to this one (her neck to the top of her
                               * head, so her hair hanging past her head goes with it), sphere's radius */
    float eye[4];             /* world: the eyes' centre; w 1 when the fragment stage needs world positions */
    float fade[4];            /* x: the part's dithered fade (0 shown .. 1 gone); y, z: the near fade, gone at y and
                               * whole from z (game units from the eyes; z 0 none); w: the ghost hand's alpha (0 none) */
    float clip_to_world[16];  /* column-major: the frame camera's clip space to the world */
    int hide;                 /* draw nothing (a ghost-pass draw not for the eyes: shadows) */
    /* Her parts' fades per point, as Jak's skin-weighted one (LIST 43): her bones' lines (world), seg_a's w the fade at
     * a line's start and seg_b's w at its end (blended along it, so a limb next to a faded trunk fades out from the
     * joint and leaves no edge standing); each point takes the fades of the lines nearest it, blended over a few cm.
     * n_seg 0: none. */
    float seg_a[12][4];
    float seg_b[12][4];
    int n_seg;
} Sh3VrCut;
/* For every draw: 1 when `out` holds something (zeroed otherwise). `main_cam`: a draw of the frame camera; `ffp`: the
 * fixed-function vertex stage; `pal_count`: the bones the game uploaded to c48 on for it (0 unknown; -1 its vertex stage
 * reads no bone palette: not a character). Characters other than her and the weapon in her hand get the near-eye fade. */
int sh3vr_body_cut(const float vsc[96][4], uint32_t stride, int main_cam, int ffp, int pal_count, Sh3VrCut *out);
/* A skinned model as sh3.c reads it from the game's model header (docs/GAME_FACTS.md, Bones). */
typedef struct Sh3VrModel {
    const int8_t *parents;      /* +0x10: each bone's parent, -1 a root */
    const float *rest;          /* +0x08: the rest pose, a matrix a bone */
    const float *blend_mats;    /* +0x1C: the blends' own matrices (each blend = this x its bone) */
    const uint8_t *blend_bones; /* +0x18: two bytes a blend, the second its bone */
    int blend_count;            /* +0x14 */
} Sh3VrModel;
/* LIST 19, 41-43: a skinned model's bones as the game posed them (0x45EC00: n D3D row-vector 4x4s), before anything reads
 * them: Heather's whole body in first person (lean, crouch, head, planted legs, arms and her own fingers at the
 * controllers), her head's place for the cut. 1 when changed. */
int sh3vr_heather_bones(float *bones, int n, const Sh3VrModel *model);
/* LIST 24 (session 13): a small model (1-4 bones) posed while a weapon is out: when it is the weapon in her hand
 * (its bone 0 on her animated right hand) and VR has no hold for it (the extras: no mesh signature), its bones are
 * carried with her hand to the controller in first person. 1 when changed. */
int sh3vr_weapon_bones(float *bones, int n);
/* LIST 48: her model (73 bones) drawn away from the player's own place, as a scene may pose her: during a scene her
 * head's place is taken from it (dynamic cutscenes), nothing else. */
void sh3vr_scene_head(const float *bones);
/* 0.1.9 (owner 164): a reflected draw of her model (a mirror: the game draws her again with her animation's bones
 * reflected about the glass) given her VR pose through the same reflection, her hands with it. 1 when changed; 0 not her
 * reflection (left as the game posed it). */
int sh3vr_mirror_bones(float *bones, int n);
/* The mirror's map of this frame or the last (row-vector 4x4, as the bones): 0 without one. */
int sh3vr_mirror_map(float x[16]);
/* 0.1.9 (owner 166): 1 while a scene draws her away from her place with the eyes in her head (first person): her draws
 * then go to her shadow as in play (sh3.c, d3d8.c her_shadow_capture). */
int sh3vr_scene_eyes(void);
/* 1 during a cutscene or a scripted sequence (VR active). */
int sh3vr_in_scene(void);
/* 1 while her own finger-skinned hands (her model's groups 3 and 4) are to be drawn in place of play's rigid ones: her
 * fingers were posed this frame (sh3.c's part draw 0x45ED40 swaps the groups). */
int sh3vr_own_hands(void);
/* Session 5 ("i need proper grip poses for every gun/grip point"): the game's own gripping hand for the weapon held in
 * VR, by hand (0 her left, 1 her right): its group in her model (14 the handgun's right hand, 18/19 the shotgun's, ...),
 * drawn in place of that hand's finger hand; -1 the finger hand. */
int sh3vr_native_hand(int hand);
/* The ghost hands (a hand pushed through a wall: where the controller really is): 0 none, else bit 0 her left, bit 1 her
 * right, with `out` (n matrices, untransposed) the pose to draw them from. */
int sh3vr_ghost_bones(float *out, int n);
/* sh3.c around the ghost hands' pass (sh3vr_body_cut draws them faint and through the walls, for the eyes only). */
void sh3vr_ghost_pass(int on);
/* The panel (HUD, menus, the 2D): a GL framebuffer of w x h (4:3), cleared to clear at each frame's start. */
unsigned sh3vr_panel_fbo(int *w, int *h);
void sh3vr_panel_drawn(void);
/* Session 11 (owners 42-44): a 2D draw onto the panel covers x0..x1, y0..y1 of the screen (0..1, top row first): the
 * box the game's text takes, so VR can show a message by its thing or a subtitle low in the view. */
void sh3vr_panel_box(float x0, float y0, float x1, float y1);
/* Session 11 (owner message 44): 1 while the game's full screens (the inventory, the map, memos, menus without the
 * world) go whole onto the panel in front of the eyes ("Menus open in front of you"; not in scenes or screen mode). */
int sh3vr_full_screens(void);
/* At each present: whether the frame camera's 3D (the world) was drawn this frame (a frame out of play without it is a
 * full screen). */
void sh3vr_frame_world(int drawn);
/* 0.1.9: the frame's full screen (out of play, without the world): its 2D draws and how many of them had a texture */
void sh3vr_frame_screen(int draws, int textured);
/* LIST 5 (src/game/movie.c): 1 while one of the game's movies plays (the view on the cinema screen, the frame the
 * movie's picture), 0 at its end. */
void sh3vr_movie(int on);
/* Screen mode (menus without a world, movies, the cinema screen): the back buffer's left layer shown on a big screen,
 * the eyes dark. */
int sh3vr_screen_mode(void);
/* The game's screen-space effects with real depth (the flash, smoke, dust, sparks) go to the eyes, put right by the eye
 * corrections like the frame camera's draws (1), or to the panel as its 2D (0). */
int sh3vr_world_effects(void);
/* 1 while the wrist's health bar stands in for the game's Life Display cross (first person; sh3vr.cpp). */
int sh3vr_wrist_health_on(void);
/* 1 while the game's Life Gauge (0x5A5E40) is to be left out: VR shows the world in the eyes (first or third person;
 * VR draws the same bars itself), not the cinema screen or flat. */
int sh3vr_life_gauge_skip(void);
/* LIST 51 (dynamic resolution): the share of the eye's size the game's 3D pictures are drawn at now (0.6 to 1). */
float sh3vr_picture_scale(void);
/* Owner message 38 (sh3.c around 0x4D44E0, her struggle step while knocked down or held; `left` the time to get free
 * 0x712C608): the controllers' shakes since the last step, each one press more (0..4); then whether she got free and how
 * many buttons the game itself counted in that step. */
int sh3vr_struggle_presses(float left);
void sh3vr_struggle_done(int freed, int buttons);
/* One of the game's effects drawn in screen space at its real depth (its middle in the game's clip space: ndc x, y and
 * D3D z; d3d8.c vr_route_pick): 1 and the map (GL column-major, the game's clip to the frame camera's) that takes it
 * back through the camera the game projected it with (`game_view`: the game's own view, sh3_game_view; NULL: the frame
 * camera's) to its place in the world, and moves the gun's own, just after a shot near the game's gun, with the gun to
 * the hand. 0: as the game's clip says. `fade` (0.1.5, session 22): how much of it to show, 1 in the middle of the game
 * camera's picture down to 0 at its edge (the game draws its effects only inside its own camera's view, which in first
 * person is not the head's: a lamp's glow fades out as its lamp leaves the game's view instead of popping); 1 for the
 * weapon's own. */
/* `half`: the draw's half size in the game's clip (x, y), from its points' extent: a small one (under 0.75 m across) is turned
 * to face the head at its place (session 22). */
int sh3vr_effect_map(const float ndc[3], const float half[2], int tex_w, const float game_view[16], float out_gl[16],
                     float* fade);
/* sh3.c: the game's own view this frame (what it gave 0x4162A0 for the frame camera, before the head's took its place);
 * 0 when there is none. */
int sh3_game_view(float out[16]);
/* Owner 79 (session 16): the game's drifting fog (d3d8.c vr_route_pick: 0x5F572E's fans at z just past 1) on a far layer
 * fixed in the world: 1 and the map (GL column-major, the game's clip to its clip) that moves each point halfway to the
 * game's camera along its own ray (the same place in the game's picture, inside every eye's far plane), so each eye's
 * correction puts it where that eye sees it; 0 while the eyes have no corrections. "Drifting fog": whether it is drawn. */
int sh3vr_far_layer_map(float out_gl[16]);
int sh3vr_drifting_fog(void);
/* Session 17: the game's lens flare round a lamp (d3d8.c vr_route_pick: 0x468CDA, 0x468F6E, drawn at z 0.50003) at the
 * lamp's view depth (its flare record's, 0x898438): 1 and the map (GL column-major, the game's clip to its clip) that
 * sets each point's depth to the lamp's at (ndc_x, ndc_y), the draw's middle; 0 when the depth is no lamp's. */
int sh3vr_depth_layer_map(float view_depth, float ndc_x, float ndc_y, unsigned from, float out_gl[16]);
/* 0.1.5: the flare's lamp test (sh3.c, 0x49FAC0) without the GPU's read back: whether a point in the view space is in front of
 * the camera and inside its picture (which covers both eyes). The flare itself is left out in VR (d3d8.c). */
int sh3vr_point_in_view(const float view_pt[4]);
/* Owner 61 (session 15): the beam saber's beam is a 3D mesh of the game's at the saber in her animated hand. One small
 * blended draw of the frame camera's while the saber is lit (`count` its vertices, for the log): 1 and the map (GL
 * column-major, the game's clip to its clip) that moves it with the saber to the VR hand; 0: no saber in the VR hand. */
int sh3vr_beam_mesh(float out_gl[16], uint32_t count);
/* Session 15: while the beam saber is out, its green point light (of the draw's c16-c19, colours c20-c23) on the game's saber
 * in her animated hand is moved with the saber to the VR hand. 1: a light moved (the constants changed). */
int sh3vr_beam_light(float vsc[96][4]);
/* testing (session 15): the beam saber's line as the game holds it and as the VR hand holds it, palm and the hold's tip
 * each, projected with the frame's camera into the viewport `vp` (x y w h): x y z four times (game palm, game tip, VR
 * palm, VR tip); 0: no saber in the VR hand */
int sh3vr_beam_screen(const float vp[4], float out[12]);

/* ---- input (dinput.c, user32.c) ---- */
typedef struct Sh3VrPad {
    float lx, ly, rx, ry;   /* sticks, -1..1, y down positive (DirectInput's way) */
    uint16_t buttons;       /* bit i: dinput.c's button i (X A B Y LB RB LT RT Back Start LS RS) */
    int8_t dpad_x, dpad_y;  /* -1..1, y down positive */
} Sh3VrPad;
/* 1 when VR has the pad this frame (*out filled; a real gamepad's state is added to it by the caller). */
int sh3vr_pad(Sh3VrPad *out);
/* The pointer (LIST 12): where the beam lands on the panel, in 0..1 of it, and its trigger; 0 while no beam. */
int sh3vr_mouse(float *x, float *y, int *left, int *right);
/* The game's pad rumble (DirectInput force feedback): both controllers. */
void sh3vr_rumble(float strength, float seconds);
/* LIST 45 ("Take items with your hand"): 1 while a closing hand asks the game's action at `at` (world) this frame;
 * sh3.c's action check then tests the event regions there, and tells VR what it found (-1 nothing). */
int sh3vr_hand_action(float at[3]);
void sh3vr_hand_action_found(int event);
/* 0.1.7: the action button's check found something the game uses (event; pass 0 or 1): a tick and a buzz in the hand */
void sh3vr_button_action_found(int event, int pass);
/* Session 11 (ask 28): 1 while hand `hand` (0 left, 1 right) is free and reaching out, its palm at `at` (world): sh3.c
 * asks the action check there each frame without the button, and tells VR what it found (sh3vr_hand_hover, -1 none). */
/* 0.1.7: the head's place and heading in the game's world (yaw: x = sin, z = cos), first person only */
int sh3vr_gaze(float at[3], float *yaw);
int sh3vr_hand_reach(int hand, float at[3]);
/* 0.1.10 (owner 170): `door` 1 when that event is a door (sh3.c sh3_event_is_door): with "Open doors by hand" the hand
 * takes its handle and pulls it open */
void sh3vr_hand_hover(int hand, int event, int door);
/* 0.1.7 (owner message 143, the hints): 1 when sh3.c should look for what the action button would use within her reach
 * this frame (every 8 frames in first person while that hint is still to show); what it found: 0 a door or a thing to
 * use or look at (an event the button uses), 1 an item to take. */
int sh3vr_tip_wants_use(void);
void sh3vr_tip_use(int item);
/* The radio's static (LIST 23): one of its four voices set to `level` (0..1, before the game's volume options). */
void sh3vr_radio(int channel, float level);

/* ---- the log ---- */
void sh3vr_log(int level, const char *text);

/* ---- the launcher's font (src/d3d8/launcher.c), for VR's own screens ---- */
int ui_font_load(void);
int ui_text(uint32_t *dst, int dw, int dh, int x, int y, int size, uint32_t color, const char *s);

#ifdef __cplusplus
}
#endif
