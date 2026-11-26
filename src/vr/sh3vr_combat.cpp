// SPDX-License-Identifier: GPL-3.0-or-later
// SH3 Quest VR: weapons and combat (LIST 24-30), on the PC VR mod's findings (NotGodlikeUwU's Silent Hill 3 VR mod:
// the weapon meshes' signatures, their bone palette at c48, the calibration angles, the melee reaches and swing
// thresholds, the attack pulses, the native hit volumes through clBattleAddQue), refined:
//
// - Every weapon in the hand: the weapon's skinned draw (its mesh signature, data/chrwp.arc) has its bone palette
//   moved so its reference bone sits at the hand, with the player's calibration (sh3quest_weapons.ini, the PC mod's
//   format); hidden while holstered. Extras without a known signature stay in Heather's animated hand.
// - Aiming: shots go along the controller (sh3.c sets the game's attack direction from gun_ray), two hands on long
//   guns aim from the main hand through the other; damage, ammo, reloads and rates of fire stay the game's.
// - Physical melee: a real swing (speed, path, net travel; the knife easier, as the PC mod) presses the game's own
//   attack, and the hit volumes the game queues for it (kind 2/3) are replaced by the weapon's real path while the
//   attack lasts: the game's damage, knock-backs, stuns and hit sounds, from where the weapon actually went.
// - Punches (either empty hand) and guns swung as clubs add a hit volume of their own (the attack ids the game used
//   for the knife and the pipe, learned from its queue and kept in sh3quest_learned.ini): a punch lands the knife's
//   hit every second time (half the weakest melee hit's damage per punch; the game has no smaller hit to give), a gun
//   lands the pipe's (a club's full damage).
// - Holsters on the body, the flashlight's place, grabbing (the game's own action under the reaching hand).

#include "sh3vr_internal.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

extern "C" float sh3_rig_attacker_at[4];  // testing: sh3.c's SH3_STEER_AT_ATTACKER (the kept attacker's place, w 1)

namespace sh3q {

namespace {

// ---------------------------------------------------------------- the weapons

enum WeaponKind { kGun = 0, kStun, kLight, kHeavy, kUnknown };
// The owner's layout (2026-11-24, session 3: "you can only really have one weapon equipped at a time anyway. so go ahead
// and make it so right shoulder is gun holster, and right hip is melee. both can be grabbed whenever, recognizing
// whichever gun/melee weapon you last holstered"): every gun in one holster, every melee weapon in the other (0.1.6,
// owner 122: the gun at the main-hand hip, the melee weapon over the main-hand shoulder, "Gun on the hip, melee on the
// shoulder"; off, the 2026-11-24 layout); the belt pouch (supplies) and the chest pocket (flashlight) as before. LIST
// 46: the ammo pouch at the front of the belt on the off hand's side gives that hand magazines, shells and batteries
// (sh3vr_reload.cpp).
enum HolsterId { kGunHolster = 0, kMeleeHolster, kPouch, kLightPocket, kAmmo, kHolsterCount };

struct WeaponInfo {
    const char* name;
    const char* section;     // sh3quest_weapons.ini (the PC mod's names)
    WeaponKind kind;
    uint32_t nv[4], np[4];   // mesh signatures (NumVertices, primitives) of its indexed draws (the extras that share
                             // its hold too, session 13)
    int sigs;
    float reach, radius;     // the hit capsule along the weapon (m); guns: as a club
    float foregrip;          // where the other hand holds it, metres along the weapon from the main hand (0: none)
    bool aims_two_handed;    // the other hand on it sets the aim (long guns, long melee)
    float muzzle;            // guns: metres from the hand to the muzzle
    float kick;              // degrees of recoil shown
    int holster;
};
constexpr int kProfiles = 10;  // the weapons with a measured hold (profile_of)
const WeaponInfo kWeapons[kProfiles] = {
    {"handgun", "Handgun", kGun, {368, 0}, {610, 0}, 1, 0.24f, 0.09f, 0.0f, false, 0.16f, 7.0f, kGunHolster},
    {"shotgun", "Shotgun", kGun, {290, 0}, {490, 0}, 1, 0.75f, 0.10f, 0.36f, true, 0.70f, 11.0f, kGunHolster},
    {"submachine gun", "Submachine Gun", kGun, {557, 0}, {890, 0}, 1, 0.42f, 0.10f, 0.22f, true, 0.36f, 3.0f, kGunHolster},
    {"stun gun", "Stun Gun", kStun, {132, 0}, {226, 0}, 1, 0.26f, 0.12f, 0.0f, false, 0.16f, 0.0f, kGunHolster},
    {"knife", "Knife", kLight, {166, 0}, {334, 0}, 1, 0.46f, 0.19f, 0.0f, false, 0.0f, 0.0f, kMeleeHolster},
    {"steel pipe", "Steel Pipe", kHeavy, {172, 170}, {294, 290}, 2, 0.82f, 0.14f, 0.18f, true, 0.0f, 0.0f, kMeleeHolster},
    {"katana", "Katana", kHeavy, {69, 72}, {122, 114}, 2, 0.95f, 0.12f, 0.12f, true, 0.0f, 0.0f, kMeleeHolster},
    {"maul", "Maul", kHeavy, {208, 0}, {426, 0}, 1, 0.76f, 0.22f, 0.22f, true, 0.0f, 0.0f, kMeleeHolster},
    // Session 13 (vr195 mesh dumps, SH3_MESH_DUMP=W; the extras after a clear): the beam saber, its hilt (bone 0,
    // 126/154) the game holds in its own right hand (group 34); the flamethrower, its body (238/434, bones 0-1) and its
    // tank under it (49/106), held in both of the game's own hands (groups 37, 38).
    {"beam saber", "Beam Saber", kHeavy, {126, 0}, {154, 0}, 1, 0.95f, 0.10f, 0.10f, true, 0.0f, 0.0f, kMeleeHolster},
    {"flamethrower", "Flamethrower", kGun, {238, 49}, {434, 106}, 2, 0.55f, 0.12f, 0.28f, true, 0.42f, 1.0f, kGunHolster},
};

// The game's inventory order of the weapons (short[0x6BDD4C + 2 i], item i = 1..13: checked in the exe, GAME_FACTS.md
// Weapons): a holster still empty takes the first weapon of its kind the player owns, in this order.
const int kInventoryOrder[13] = {4, 5, 7, 6, 9, 10, 13, 14, 8, 1, 2, 3, 17};

// A weapon number in its first mode (15, 16, 18 are the handgun, the submachine gun and 17 in their other mode; the
// game's equip picks the mode itself).
int base_slot(int w) { return w == 15 ? 1 : w == 16 ? 3 : w == 18 ? 17 : w; }

// Which holster a weapon goes to. The extras (no mesh signature known) by the game's own attack decision 0x494C40 (its
// class table at 0x494CD8, checked 2026-11-24): weapons 10 and 17 (18) are decided as the shotgun and the submachine
// gun are (loaded rounds, a reload when empty): the gun holster; 9, 13 and 14 as the pipe and the katana (light, heavy
// and third attacks): the melee holster.
int holster_of(int w);

// The game's weapon numbers (the drawn weapon 0x712C656 and the equipped slot 0x712CABF count the same way; checked in
// the exe 2026-11-24: 0x4711D0 sets the byte from 0x497980's 0x800 + slot): 0 none, 1 handgun, 2 shotgun,
// 3 submachine gun, 4 knife, 5 steel pipe, 6 katana, 7 maul, 8 stun gun; 9, 10, 13, 14, 17 the extras (no mesh
// signature known: they stay in Heather's animated hand); 15, 16, 18 the handgun, the submachine gun and 17 in their
// other mode (0x712CABC bit 2). The PC VR mod's comment "0 handgun .. 3 stun gun" is off by this; its melee 4-7 holds.
// kWeapons is in the PC mod's profile order; profile_of() maps a game number to it.
int profile_of(int w) {
    switch (w) {
    case 1:
    case 15: return 0;
    case 2: return 1;
    case 3:
    case 16:
    case 17:  // the unlimited submachine gun (session 13, vr192): the submachine gun's own model (557/890), held as it is
    case 18: return 2;
    case 8: return 3;
    case 4:
    case 5:
    case 6:
    case 7: return w;
    // Session 13 (vr195): the gold and silver pipes are the steel pipe's own model (170/290 against its 172/294: the
    // same tube, bounds and elbow to the unit) in the same hands of the game (groups 30, 31), so held as it is.
    case 13:
    case 14: return 5;
    case 9: return 8;    // the beam saber
    case 10: return 9;   // the flamethrower
    }
    return -1;
}
const WeaponInfo* info_of(int w) {
    const int p = profile_of(w);
    return p >= 0 ? &kWeapons[p] : nullptr;
}
int holster_of(int w) {
    if (const WeaponInfo* i = info_of(w)) return i->holster;
    switch (base_slot(w)) {
    case 9:
    case 13:
    case 14: return kMeleeHolster;
    case 10:
    case 17: return kGunHolster;
    }
    return -1;
}

// ---------------------------------------------------------------- how each weapon is held (LIST 24, 26; session 5)
//
// Session 4 found every gun held backwards: the PC mod's way put bone 1 at the hand, turned 90/90/45, and bone 1 is not
// the grip (each model has a body bone and a small part on a second bone: the handgun's and the submachine gun's at the
// slide and barrel, the stun gun's electrode head, the shotgun's stock). Now each weapon is held as it is made to be
// held, from its own model (the meshes drawn whole with tools/gun_view.py):
// - Guns: the barrel along the controller's pointing (the aim pose's -Z), the top up. Melee weapons and the stun gun:
//   the blade, shaft or prongs out of the fist along its rod (the grip pose's -Z), the edge or striking face forward.
// - The weapon sets the hand (the owner: "the hand positions and grip points on weapons should feel natural and
//   native"): Heather's hand is posed round the weapon's own grip (its fist centre on the grip, its rod along the grip,
//   the index finger on the trigger of a gun), and the other hand, when it takes the weapon's second grip point, is
//   posed there (the owner: "add good two hand grip points on all weapons as needed"): the handgun's support hand
//   wrapped round the main hand's fingers, the shotgun's pump, under the submachine gun's receiver, lower on the steel
//   pipe's, the katana's and the maul's handles. Long weapons held in both hands are turned so that their second grip
//   lies towards the other controller (their aim is the line between the hands).
// - The weapon's other bones keep the game's pose against the held one (B' = H inv(B_held) B), so faces stitched
//   between them stay as the game draws them.
// Model units are the world's (490 a metre). Every weapon's points are a right-handed player's; left-handed, the second
// grip is mirrored across the weapon's centre plane (the model itself is never mirrored).
struct Hold {
    bool known;            // measured from the model; else the weapon stays as the game draws it
    bool fist;             // along the fist (the grip pose), else along the pointing (the aim pose)
    int bone;              // the model bone the hand holds
    Vec3 fwd, up;          // model (unit): the barrel / blade / prongs, and the top (melee: the side away from the edge
                           // or striking face, which faces forward)
    Vec3 palm;             // model units: the main fist's centre round the grip (the controller's grip position)
    Vec3 rod;              // model (unit): that fist's rod, little finger to thumb, along the grip
    Vec3 tip;              // model units: a gun's muzzle (the shot leaves there), a melee weapon's far end
    bool second;           // a second grip point for the other hand
    Vec3 second_at;        // model units: the other fist's centre there
    Vec3 second_rod;       // model (unit): its rod
    Vec3 second_palm;      // model (unit): the way its palm faces
    float curl, second_curl;  // the fingers round each grip (0 open .. 1 a full fist): thicker grips, less
};
// By profile (kWeapons' order). Session 5's measurements (the meshes in tools/gun_view.py's pictures):
// - Handgun, bone 0: barrel +x, top +y; the grip's back strap from (20.8, -24) to (32.4, 30), its front strap from
//   (51.2, -23) to (57.2, 6): raked 12 degrees, 30 units (6 cm) deep, z -22.4 to -4.7; the fist's centre half way
//   between the straps at the middle finger's height. The tip at x 101.4, bone 0's end; the muzzle (session 22, owner
//   109) is the slide's front (bone 1, posed at 139.6 38.1 -13.1 in bone 0's frame: vr188), x 137.0 round y 38.1,
//   z -13.1: muzzle_point. At the tip the flash sat 7 cm back inside the slide. The second hand wraps round the main hand's
//   fingers from the left front, its palm against the grip's left side.
// - Submachine gun, bone 0: barrel +x, top +y; the grip (the magazine up through it) x 26.4-56.7, y -21 to 29, raked 6
//   degrees; the muzzle x 115.6, y 40-50. The second hand cups the receiver's front under the barrel, palm up, thumb
//   forward: the front block x 84-112 (z -23.8 to -0.2), its floor at y 29-31, ahead of the trigger guard (x 56-84,
//   hanging to y 6.6). Session 14 (vr197, owner 58): the fist's centre lies 1.9 cm over the palm's skin (her grip
//   space: the palm 1.9 cm behind the fist's centre), so a centre at y 16 left the palm at the guard's floor, 4.7 cm
//   under the receiver, the guard drawn across her fingers; at y 38.5 the palm lies on the floor, the fingers up its
//   right side, the little finger over the guard's front where it meets the receiver.
// - Stun gun, bone 0: its body a box 13 x 6 x 3.8 cm whose long axis lies along (0.451, 0.891, -0.051) from (25, -40)
//   to (55, 19); the electrode head (bone 1) on its upper end. Held like a flashlight: the fist round the box's lower
//   half, the electrodes out of the thumb side.
// - Shotgun, bone 1 (the receiver and barrel; bone 0 is the stock, posed against it as the game does): barrel +x, the
//   muzzle's bore at the origin, top -y (y runs down); the receiver x -250 to -140 (y -4 to 20, 2.9 cm wide), the
//   trigger guard x -240 to -210 under it, the pump x -125 to -20 (y 9 to 27 under the barrel, 3.5 cm wide), the
//   stock's grip x -300 to -268 down to y 55 (its front edge raked along (0.28, -0.96)), the butt at x -400.
// - Knife, bone 0 (the blade bone 1 posed against it): the handle y -29 to 37 (4.1 x 2.3 cm) along (0.28, 0.955,
//   0.098), the blade on to its point at (71.4, 101.8, 0.1).
// - Steel pipe, bone 0: a 4 cm tube 92 cm long along (0.287, 0.954, 0.075) from y -114, its elbow (bone 1) at the top
//   (y 330 to 393).
// - Katana, bone 0: the handle y -70 to 38 (4.8 x 2.8 cm) along (0.396, 0.913, 0.099), the guard at y 40 to 50; the
//   blade (its own draw, bones 0 and 1) on along the same line.
// - Maul, bone 0: a 2.9 cm shaft along (0.284, 0.957, 0.06) from y -65, the spiked head (bone 1) round (200, 545, 25).
// Where the game has its own gripping hand for a weapon (native grips, below), the hand's place on it is the game's
// and the palm point here is only used until that is read (the first frames after the weapon is drawn).
const Hold kHolds[kProfiles] = {
    {true, false, 0, {1, 0, 0}, {0, 1, 0}, {39.3f, -8.0f, -13.9f}, {0.210f, 0.978f, 0}, {101.4f, 28.0f, -13.9f}, true,
     {58.0f, -10.0f, -18.0f}, {0.300f, 0.954f, 0}, {0, 0, 1}, 0.80f, 0.74f},
    {true, false, 1, {1, 0, 0}, {0, -1, 0}, {-282.0f, 33.0f, 0}, {0.28f, -0.96f, 0}, {0, 0, 0}, true,
     {-72.0f, 18.0f, 0}, {1, 0, 0}, {0, -1, 0}, 0.80f, 0.70f},
    {true, false, 0, {1, 0, 0}, {0, 1, 0}, {41.5f, 3.0f, -10.6f}, {0.105f, 0.994f, 0}, {115.6f, 45.0f, -11.9f}, true,
     {95.0f, 38.5f, -14.5f}, {1, 0, 0}, {0, 1, 0}, 0.78f, 0.70f},
    {true, true, 0, {0.451f, 0.891f, -0.051f}, {-0.892f, 0.452f, 0}, {34.9f, -19.8f, -12.1f}, {0.451f, 0.891f, -0.051f},
     {59.0f, 28.0f, -13.0f}, false, {}, {}, {}, 0.70f, 0},
    {true, true, 0, {0.28f, 0.955f, 0.098f}, {-0.96f, 0.28f, 0}, {41.0f, 5.0f, -11.0f}, {0.28f, 0.955f, 0.098f},
     {71.4f, 101.8f, 0.1f}, false, {}, {}, {}, 0.85f, 0},
    {true, true, 0, {0.287f, 0.954f, 0.075f}, {-0.957f, 0.288f, 0}, {20.3f, -45.0f, -17.9f}, {0.287f, 0.954f, 0.075f},
     {116.0f, 392.0f, 25.0f}, true, {3.75f, -100.0f, -22.2f}, {0.287f, 0.954f, 0.075f}, {0, 0, 1}, 0.82f, 0.82f},
    {true, true, 0, {0.396f, 0.913f, 0.099f}, {-0.917f, 0.398f, 0}, {37.2f, 15.0f, -16.3f}, {0.396f, 0.913f, 0.099f},
     {221.0f, 440.0f, 29.7f}, true, {11.1f, -45.0f, -22.7f}, {0.396f, 0.913f, 0.099f}, {0, 0, 1}, 0.80f, 0.80f},
    {true, true, 0, {0.284f, 0.957f, 0.0596f}, {-0.959f, 0.285f, 0}, {31.0f, -5.0f, -11.1f}, {0.284f, 0.957f, 0.0596f},
     {200.0f, 590.0f, 22.0f}, true, {15.3f, -58.0f, -14.4f}, {0.284f, 0.957f, 0.0596f}, {0, 0, 1}, 0.85f, 0.85f},
    // Beam saber (vr195 dump, bone 0): a hilt 31 cm long, 2.4 cm round, along (0.368, 0.921, 0.114) from its pommel
    // (y -84) to its emitter (y 60); the game's fist at its middle (24.4, -1.1, -14.3, on the hilt's line); the other
    // hand stacked under it (10 cm, as on the katana); the beam on 82 cm from the fist.
    {true, true, 0, {0.368f, 0.921f, 0.114f}, {-0.928f, 0.371f, 0}, {24.4f, -1.1f, -14.3f}, {0.368f, 0.921f, 0.114f},
     {171.6f, 367.3f, 31.3f}, true, {6.0f, -47.2f, -22.2f}, {0.368f, 0.921f, 0.114f}, {0, 0, 1}, 0.80f, 0.80f},
    // Flamethrower (vr195 dump, bone 0): barrel +x, top +y; its body x 3 to 231 (the nozzle x 219-231, y 13-38), the
    // pistol grip x 27-63 down to y -30 (the game's fist at 24.4, -1.1, -14.3, as on every weapon), its tank under the
    // body in front of the grip (x 63-90, y -65 to 25); the other hand cups the body's front under the barrel (x 147-159,
    // its floor at y 17 to 23 from x 144 to 172, z -28 to -3), palm up, thumb forward, as the submachine gun's; the
    // game's own left hand (group 38) is taken instead when it lies within 4 cm of it. Session 14 (vr197): its centre
    // at y 3 left the palm 4.7 cm under the body, as the submachine gun's; at y 26 the palm lies under the floor.
    {true, false, 0, {1, 0, 0}, {0, 1, 0}, {24.4f, -1.1f, -14.3f}, {0.21f, 0.978f, 0}, {231.0f, 26.0f, -20.0f}, true,
     {155.0f, 26.0f, -18.0f}, {1, 0, 0}, {0, 1, 0}, 0.80f, 0.70f},
};
const Hold* hold_of(int w) {
    const int p = profile_of(w);
    return p >= 0 && kHolds[p].known ? &kHolds[p] : nullptr;
}

// Where the shot leaves the gun (the held bone's model units): the hold's tip, but the handgun's muzzle is the front
// of its slide (bone 1, x 137.0 round y 38.1, z -13.1; see kHolds' measurements), 7 cm past its tip. Only the shot's
// line takes it (gun_ray: the shot's origin, so the game's flash, and the guide dot). The tip stays where it was for
// the weapon's reach (the wall probe, the club's capsule): build 223 put the tip itself on the slide's front face, and
// the probe's last sample then met the gun's own surface, read as a wall: the gun was held back from nothing all
// the time (vr278, vr281: drawn low and pulled back; vr277 with the old tip, whole in the hand).
Vec3 muzzle_point(const Hold* h) { return h == &kHolds[0] ? Vec3{137.0f, 38.1f, -13.1f} : h->tip; }

// Each weapon's shape as a few capsules (model units, the held bone's): her fingers close round them and stop on them
// (LIST 20, Half-Life: Alyx's fingers, on weapons too: the hand holding it wraps its real grip, the other hand's
// fingers stop on it where they touch it, with the touch's tick), and the probes take what the depth shows of them for
// the weapon itself, not the world (the weapon against the world: its own probes, sh3vr_body.cpp).
struct Cap {
    Vec3 a, b;
    float r;
};
constexpr int kCapMax = 8;
struct Shape {
    int n;
    Cap c[kCapMax];
};
// The grips from their cross-sections (session 5, scratchpad grip_slices.py: the faces' edges cut at each height):
// - Handgun: 31-32 units front to back and 20 across from y -20 to 12 (z -25.6 to -2.5, its centre drifting from -16 at
//   the bottom to -12 at the top), raked along (0.21, 0.978): a round capsule is too thin front to back, so the grip is
//   two capsules side by side along its depth (its back strap's half and its front strap's half), 10 units round each.
// - Submachine gun: its grip 24-27 units deep and only 10.7 across (z -17 to -6), so three thin capsules along its
//   depth; its magazine below (15 x 7.5) one more.
const Shape kShapes[kProfiles] = {
    // Handgun: the slide and barrel (y 25-50), the grip's back and front halves along its rake, the trigger guard's
    // floor.
    {4, {{{24, 37, -13.9f}, {100, 36, -13.9f}, 12}, {{30.6f, -22, -15.5f}, {39.6f, 20, -12.0f}, 10},
         {{41.6f, -22, -15.5f}, {50.6f, 20, -12.0f}, 10}, {{56, 2, -13.9f}, {76, 9, -13.9f}, 3}}},
    // Shotgun (y down): the barrel over its magazine tube, the pump, the receiver's upper and lower halves, the trigger
    // guard's floor, the stock's grip, the stock.
    {7, {{{-140, 4, 0}, {0, 4, 0}, 9}, {{-120, 18, 0}, {-30, 18, 0}, 9}, {{-250, 2, 0}, {-140, 2, 0}, 7},
         {{-250, 14, 0}, {-140, 14, 0}, 7}, {{-240, 33, 0}, {-212, 33, 0}, 3}, {{-272, 20, 0}, {-282, 50, 0}, 9},
         {{-300, 25, 0}, {-385, 45, 0}, 14}}},
    // Submachine gun: the receiver (y 28-62, 5 cm wide), the grip in three along its depth, the magazine under it, the
    // trigger guard's floor.
    {6, {{{-36, 45, -9.5f}, {114, 45, -9.5f}, 15}, {{32.7f, -18, -12.4f}, {37.1f, 24, -9.4f}, 5.4f},
         {{40.4f, -18, -12.4f}, {44.8f, 24, -9.4f}, 5.4f}, {{48.0f, -18, -12.4f}, {52.4f, 24, -9.4f}, 5.4f},
         {{40.5f, -42, -13.4f}, {44.5f, -19, -13.4f}, 7}, {{58, 7, -10.6f}, {80, 9, -10.6f}, 3}}},
    // Stun gun: its box (13 x 6 x 3.8 cm) along its long axis.
    {1, {{{25.0f, -39.7f, -9.7f}, {54.6f, 19.1f, -13.1f}, 13}}},
    // Knife: the handle, the blade.
    {2, {{{35.8f, -20, -13.25f}, {51.2f, 30, -7.9f}, 8}, {{54, 40, -6}, {70.5f, 98, -0.5f}, 5}}},
    // Steel pipe: the tube, its elbow in two.
    {3, {{{3.75f, -110, -22.2f}, {124, 300, 9.15f}, 9.8f}, {{124, 300, 9.15f}, {136, 365, 15}, 11},
         {{136, 365, 15}, {114, 392, 26}, 9}}},
    // Katana: the handle, the guard, the blade along the handle's line.
    {3, {{{10, -65, -23}, {42, 38, -15}, 9}, {{40, 45, -12}, {48, 47, -12}, 17}, {{48, 50, -12}, {221, 440, 29.7f}, 4}}},
    // Maul: the shaft, the spiked head.
    {2, {{{14.7f, -62, -14.5f}, {166, 460, 17.25f}, 7.2f}, {{200, 545, 25}, {200, 546, 25}, 40}}},
    // Beam saber: the hilt, the beam.
    {2, {{{-1.0f, -80, -26}, {48, 58, -10}, 12}, {{48, 58, -10}, {171.6f, 367.3f, 31.3f}, 4}}},
    // Flamethrower: the body and nozzle, the pistol grip, the tank.
    {3, {{{5, 45, -10}, {228, 26, -20}, 20}, {{30, -25, -11}, {40, 15, -11}, 8}, {{77, -58, -15}, {78, 18, -15}, 12}}},
};

// ---------------------------------------------------------------- the weapon's place this frame (tracking)

Quat quat_axis(Vec3 axis, float angle) {
    const Vec3 a = normalized(axis);
    const float s = std::sin(angle * 0.5f);
    return {a.x * s, a.y * s, a.z * s, std::cos(angle * 0.5f)};
}
// The rotation whose columns are the unit axes x, y, z (a right-handed frame).
Quat quat_of_axes(Vec3 x, Vec3 y, Vec3 z) {
    const float m00 = x.x, m11 = y.y, m22 = z.z, tr = m00 + m11 + m22;
    Quat q;
    if (tr > 0) {
        const float s = std::sqrt(tr + 1.0f) * 2.0f;
        q = {(y.z - z.y) / s, (z.x - x.z) / s, (x.y - y.x) / s, 0.25f * s};
    } else if (m00 > m11 && m00 > m22) {
        const float s = std::sqrt(1.0f + m00 - m11 - m22) * 2.0f;
        q = {0.25f * s, (y.x + x.y) / s, (z.x + x.z) / s, (y.z - z.y) / s};
    } else if (m11 > m22) {
        const float s = std::sqrt(1.0f + m11 - m00 - m22) * 2.0f;
        q = {(y.x + x.y) / s, 0.25f * s, (z.y + y.z) / s, (z.x - x.z) / s};
    } else {
        const float s = std::sqrt(1.0f + m22 - m00 - m11) * 2.0f;
        q = {(z.x + x.z) / s, (z.y + y.z) / s, 0.25f * s, (x.y - y.x) / s};
    }
    const float n = std::sqrt(q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w);
    return {q.x / n, q.y / n, q.z / n, q.w / n};
}
// The shortest turn taking unit `from` to unit `to`.
Quat quat_arc(Vec3 from, Vec3 to) {
    const Vec3 c = cross(from, to);
    const float d = dot(from, to);
    if (d < -0.9999f) return quat_axis(std::fabs(from.x) < 0.9f ? cross(from, Vec3{1, 0, 0}) : cross(from, Vec3{0, 1, 0}), kPi);
    const float w = 1.0f + d;
    const float n = std::sqrt(c.x * c.x + c.y * c.y + c.z * c.z + w * w);
    return {c.x / n, c.y / n, c.z / n, w / n};
}

// ---------------------------------------------------------------- the game's own grips (session 5)
//
// The owner: "i see the thumb clipping through the gun. i need proper grip poses for every gun/grip point". Heather's
// model has a rigid gripping hand made for each weapon (her parts' groups, GAME_FACTS.md): play shows her right hand
// group 12 and her left 13 empty, and with a weapon the game swaps in that weapon's own: the handgun's right hand 14,
// the knife's 15, the stun gun's 16, the katana's 24; both hands for the shotgun (18 right, 19 left on the pump), the
// submachine gun (21, 22), the maul (27, 28) and the steel pipe (30, 31) (run vr49's shown groups at each equip). The
// game holds the weapon against her hand bone where that hand fits its grip, thumb and fingers round it as Team Silent
// made them. VR shows the same hand on the weapon and keeps the same place: her hand bone in the weapon's held bone,
// read from the game's own pose of both (her hand from sh3vr_ik.cpp's copy of the game's pose, the weapon from its
// palette before VR moves it), taken once it has held still for 8 frames. Guns keep the barrel on the controller's
// pointing with the hand at the controller (the hand turned as the game's grip has it); the melee weapons and the stun
// gun are held by the hand at the controller as the game holds them. A left-handed player's main hand (the game's
// hands are a right-handed player's) and the hands without one of these keep the finger hand.
struct NativeGroups {
    int right, left;
};
const NativeGroups kNativeGroup[kProfiles] = {{14, -1}, {18, 19}, {21, 22}, {16, -1}, {15, -1}, {30, 31}, {24, -1}, {27, 28},
                                             {34, -1}, {37, 38}};  // session 13: the beam saber, the flamethrower
struct NativeGrip {
    bool have = false;
    Vec3 o, ax[3];             // her hand bone in the weapon's held bone: its origin (model units), its axes (unit)
    Vec3 palm;                 // the controller's grip point there (the fist's centre, model units)
    Vec3 last_o, last_ax[3];   // last frame's reading, and how many frames it has held still
    int still = 0;
};
NativeGrip native[kProfiles][2];       // by profile, [kLeft] [kRight]
int native_shown[2] = {-1, -1};  // this frame: the group shown for each hand (sh3.c's part draw asks)

int native_group_of(int profile, int h) {
    if (profile < 0 || profile >= kProfiles) return -1;
    return h == kRight ? kNativeGroup[profile].right : kNativeGroup[profile].left;
}
// The native grip for hand h on the weapon of `profile`, when read and usable (a right-handed player).
const NativeGrip* native_of(int profile, int h) {
    if (opt.left_handed || native_group_of(profile, h) < 0) return nullptr;
    return native[profile][h].have ? &native[profile][h] : nullptr;
}

struct WeaponFrame {
    bool placed = false;       // a known weapon drawn in the main hand (first person)
    const Hold* hold = nullptr;
    const Shape* shape = nullptr;
    int profile = -1;
    Vec3 fwd, up, right;       // the hold's model axes, made orthonormal
    Vec3 palm;                 // model units: the point at `base` (the game's grip's fist centre, else the hold's)
    Vec3 base;                 // tracking: the palm point's place (the main fist's centre)
    Quat turn;                 // tracking: the weapon's frame (-Z its barrel or blade, +Y its top, +X its right)
    Quat aim_turn;             // the same without the recoil's kick (the shots, the guide dot)
    float scale = 1;           // metres a model unit
    bool mirror = false;       // left-handed: the second grip mirrored across the weapon's centre plane
    float held_back = 0;       // metres the weapon is held back from a wall (eased)
};
WeaponFrame wf, wf_one_hand;   // this frame's, and the main hand's alone (the second grip's place to take it)

// A model direction (unit) in the frame's own axes (x right, y up, -z forward).
Vec3 frame_local(const WeaponFrame& f, Vec3 v) { return {dot(v, f.right), dot(v, f.up), -dot(v, f.fwd)}; }
Vec3 mirrored_dir(const WeaponFrame& f, Vec3 v) { return f.mirror ? v - f.right * (2.0f * dot(v, f.right)) : v; }
Vec3 mirrored_point(const WeaponFrame& f, Vec3 p) {
    return f.mirror ? p - f.right * (2.0f * dot(p - f.palm, f.right)) : p;
}
Vec3 frame_point(const WeaponFrame& f, Vec3 model) {
    return f.base + rotate(f.turn, frame_local(f, model - f.palm) * f.scale);
}
Vec3 frame_dir(const WeaponFrame& f, Vec3 model) { return normalized(rotate(f.turn, frame_local(f, normalized(model)))); }
// The second grip's point (model units): the fist's centre of the game's own left hand where it holds the weapon (the
// shotgun's pump, ...), else the hold's (mirrored for a left-handed player).
Vec3 second_point(const WeaponFrame& f) {
    const NativeGrip* ng = native_of(f.profile, kLeft);
    if (ng && !f.mirror && main_hand() == kRight) return ng->palm;
    return mirrored_point(f, f.hold->second_at);
}

// A hand's pose round a grip of the weapon: its fist's centre at `at` (tracking), its rod along `rod`, its palm facing
// `palm` (tracking, unit) — OpenXR's grip pose: -Z the rod, +X out of the palm on the left hand and into it on the right.
Pose hand_round(int h, Vec3 at, Vec3 rod, Vec3 palm) {
    const Vec3 z = normalized(rod * -1.0f);
    const float mx = h == kLeft ? -1.0f : 1.0f;
    Vec3 x = palm * -mx;
    x = normalized(x - z * dot(x, z));
    if (length(x) < 0.5f) x = normalized(cross(Vec3{0, 1, 0}, z));
    const Vec3 y = cross(z, x);
    return {at, quat_of_axes(x, y, z)};
}



// The player's adjustments (sh3quest_weapons.ini, written with the defaults on the first run): each weapon moved and
// turned from its natural place in the hand, in its own axes; the hand follows it.
constexpr int kCalibVersion = 2;
struct Calib {
    float scale = 1.0f;
    float move_cm[3]{};      // right, up, forward
    float turn_deg[3]{};     // muzzle up (about its right), muzzle left (about its up), roll right (about its forward)
    float aim_pitch = 0, aim_yaw = 0;  // degrees: the shot's line from the barrel
    bool guide_dot = true;
};
Calib calib[kProfiles];  // by profile
bool calib_loaded = false;
const Calib& calib_of(int w) {
    static const Calib kNone{};
    const int p = profile_of(w);
    return p >= 0 ? calib[p] : kNone;
}

void default_calibration() {
    for (Calib& c : calib) c = Calib{};
    calib[3].guide_dot = false;  // the stun gun shocks what it touches: no dot far off
}

void write_default_calibration() {
    FILE* f = std::fopen("sh3quest_weapons.ini", "w");
    if (!f) return;
    std::fprintf(f, "; SH3 Quest VR: each weapon in your hand. Every weapon is held by its own grip with the hand round it;\n"
                    "; these move and turn it from there (the hand follows it). Read when the game starts.\n"
                    "; Move: cm along the weapon (right, up, forward). Turn: degrees (muzzle up, muzzle left, roll\n"
                    "; right). Aim: degrees the shot's line (and the guide dot) leaves the barrel's.\n"
                    "Version=%d\n", kCalibVersion);
    for (int s = 0; s < kProfiles; s++) {
        const Calib& c = calib[s];
        std::fprintf(f, "\n[%s]\nScalePercent=%.0f\nMoveRightCm=%g\nMoveUpCm=%g\nMoveForwardCm=%g\nTurnUpDegrees=%g\n"
                        "TurnLeftDegrees=%g\nTurnRollDegrees=%g\n",
                     kWeapons[s].section, c.scale * 100.0f, c.move_cm[0], c.move_cm[1], c.move_cm[2], c.turn_deg[0],
                     c.turn_deg[1], c.turn_deg[2]);
        if (kWeapons[s].kind == kGun || kWeapons[s].kind == kStun)
            std::fprintf(f, "ShowGuideDot=%d\nAimPitchDegrees=%g\nAimYawDegrees=%g\n", c.guide_dot ? 1 : 0, c.aim_pitch,
                         c.aim_yaw);
    }
    std::fclose(f);
}

void load_calibration() {
    calib_loaded = true;
    default_calibration();
    FILE* f = std::fopen("sh3quest_weapons.ini", "r");
    if (!f) {
        write_default_calibration();
        rt_log("VR: sh3quest_weapons.ini written with the weapons' natural places");
        return;
    }
    char line[256];
    int cur = -1, version = 0;
    while (std::fgets(line, sizeof line, f)) {
        char name[64];
        if (std::sscanf(line, " [%63[^]]]", name) == 1) {
            cur = -1;
            for (int s = 0; s < kProfiles; s++)
                if (!std::strcmp(name, kWeapons[s].section)) cur = s;
            continue;
        }
        char key[64];
        float v;
        if (std::sscanf(line, " %63[^= ] = %f", key, &v) != 2) continue;
        const std::string k = key;
        if (k == "Version") {
            version = (int)v;
            continue;
        }
        if (cur < 0) continue;
        Calib& c = calib[cur];
        if (k == "ScalePercent") c.scale = std::clamp(v, 10.0f, 300.0f) / 100.0f;
        else if (k == "MoveRightCm") c.move_cm[0] = std::clamp(v, -30.0f, 30.0f);
        else if (k == "MoveUpCm") c.move_cm[1] = std::clamp(v, -30.0f, 30.0f);
        else if (k == "MoveForwardCm") c.move_cm[2] = std::clamp(v, -30.0f, 30.0f);
        else if (k == "TurnUpDegrees") c.turn_deg[0] = std::clamp(v, -90.0f, 90.0f);
        else if (k == "TurnLeftDegrees") c.turn_deg[1] = std::clamp(v, -90.0f, 90.0f);
        else if (k == "TurnRollDegrees") c.turn_deg[2] = std::clamp(v, -180.0f, 180.0f);
        else if (k == "AimPitchDegrees") c.aim_pitch = std::clamp(v, -30.0f, 30.0f);
        else if (k == "AimYawDegrees") c.aim_yaw = std::clamp(v, -30.0f, 30.0f);
        else if (k == "ShowGuideDot") c.guide_dot = v != 0;
    }
    std::fclose(f);
    if (version != kCalibVersion) {
        // The old file (the PC mod's angles about the controller, which held every gun backwards): written again.
        default_calibration();
        write_default_calibration();
        rt_log("VR: sh3quest_weapons.ini was the old kind (the PC mod's angles): written again with the natural places");
        return;
    }
    rt_log("VR: weapon adjustments read from sh3quest_weapons.ini");
}

// ---------------------------------------------------------------- 3x3 and affine matrices (row-major, column vectors)

struct R3 {
    float m[3][3]{};
};
R3 mul(const R3& a, const R3& b) {
    R3 r;
    for (int i = 0; i < 3; i++)
        for (int j = 0; j < 3; j++) r.m[i][j] = a.m[i][0] * b.m[0][j] + a.m[i][1] * b.m[1][j] + a.m[i][2] * b.m[2][j];
    return r;
}
Vec3 mul(const R3& a, Vec3 v) {
    return {a.m[0][0] * v.x + a.m[0][1] * v.y + a.m[0][2] * v.z, a.m[1][0] * v.x + a.m[1][1] * v.y + a.m[1][2] * v.z,
            a.m[2][0] * v.x + a.m[2][1] * v.y + a.m[2][2] * v.z};
}
R3 of_quat(Quat q) {
    const float x = q.x, y = q.y, z = q.z, w = q.w;
    R3 r;
    r.m[0][0] = 1 - 2 * (y * y + z * z); r.m[0][1] = 2 * (x * y - z * w);     r.m[0][2] = 2 * (x * z + y * w);
    r.m[1][0] = 2 * (x * y + z * w);     r.m[1][1] = 1 - 2 * (x * x + z * z); r.m[1][2] = 2 * (y * z - x * w);
    r.m[2][0] = 2 * (x * z - y * w);     r.m[2][1] = 2 * (y * z + x * w);     r.m[2][2] = 1 - 2 * (x * x + y * y);
    return r;
}
bool inv3(const R3& a, R3& out) {
    const float (*m)[3] = a.m;
    const float det = m[0][0] * (m[1][1] * m[2][2] - m[1][2] * m[2][1]) - m[0][1] * (m[1][0] * m[2][2] - m[1][2] * m[2][0]) +
                      m[0][2] * (m[1][0] * m[2][1] - m[1][1] * m[2][0]);
    if (std::fabs(det) < 1e-12f || !std::isfinite(det)) return false;
    const float k = 1.0f / det;
    out.m[0][0] = (m[1][1] * m[2][2] - m[1][2] * m[2][1]) * k;
    out.m[0][1] = (m[0][2] * m[2][1] - m[0][1] * m[2][2]) * k;
    out.m[0][2] = (m[0][1] * m[1][2] - m[0][2] * m[1][1]) * k;
    out.m[1][0] = (m[1][2] * m[2][0] - m[1][0] * m[2][2]) * k;
    out.m[1][1] = (m[0][0] * m[2][2] - m[0][2] * m[2][0]) * k;
    out.m[1][2] = (m[0][2] * m[1][0] - m[0][0] * m[1][2]) * k;
    out.m[2][0] = (m[1][0] * m[2][1] - m[1][1] * m[2][0]) * k;
    out.m[2][1] = (m[0][1] * m[2][0] - m[0][0] * m[2][1]) * k;
    out.m[2][2] = (m[0][0] * m[1][1] - m[0][1] * m[1][0]) * k;
    return true;
}
// The rotation taking unit `from` to unit `to` the shortest way.
R3 arc(Vec3 from, Vec3 to) {
    const Vec3 c = cross(from, to);
    const float d = std::clamp(dot(from, to), -1.0f, 1.0f);
    const float s = length(c);
    if (s < 1e-5f) {
        R3 r;
        r.m[0][0] = r.m[1][1] = r.m[2][2] = 1;
        return r;
    }
    const Vec3 axis = c * (1.0f / s);
    const float ang = std::atan2(s, d);
    return of_quat({axis.x * std::sin(ang * 0.5f), axis.y * std::sin(ang * 0.5f), axis.z * std::sin(ang * 0.5f),
                    std::cos(ang * 0.5f)});
}

struct Aff {
    R3 r;
    Vec3 t;
};
Aff mul(const Aff& a, const Aff& b) { return {mul(a.r, b.r), mul(a.r, b.t) + a.t}; }
bool inverse(const Aff& a, Aff& out) {
    if (!inv3(a.r, out.r)) return false;
    out.t = mul(out.r, a.t) * -1.0f;
    return true;
}

// The weapon's last draw in the hand: the map from the game's weapon (where her animated hand holds it) to the weapon
// as drawn at the controller (B' = game_to_hand B for every bone), and the frame it was drawn.
Aff game_to_hand{};
int game_to_hand_frame = -1;
// The game's weapon against the one in the hand (session 6, the muzzle flash). The game attaches its weapon to her
// animated hand each update (sh3.c 0x495360) and draws it the frame after (run vr70: each draw shows the update before
// it); its recoil moves it up to 71 units a frame (vr74), so a map measured at one draw is a frame late for the next
// update's effects. Instead: the held bone's place in the weapon's own matrix (rigid: learnt at each draw from the
// update it shows) gives the weapon's matrix as the gun in the hand has it, `vr_weapon`, and anything the game places
// from its own weapon or hand at an update maps by vr_weapon x inverse(that update's matrix, `w_game`): its shots,
// light, the flash and smoke it projects (sh3vr.cpp sh3vr_effect_map). The game's matrix itself is left as it is: the
// knife's blade is drawn from it, and moved it was moved twice, stretched a metre from the hand (run vr78).
Aff w_game{};          // the game's own weapon matrix (world) at its last update, before it was moved
int w_game_frame = -100;
Aff held_in_weapon{};  // the drawn weapon's held bone in that matrix's own space
bool held_in_weapon_known = false;
Aff vr_weapon{};       // the weapon's matrix as the gun in the hand has it
int vr_weapon_frame = -100, weapon_applied_frame = -100;

// The game's weapon (its latest update) -> the gun in the hand; else the last draw's map; false: none.
bool live_weapon_map(Aff& m) {
    Aff iw;
    if (rt_frame - vr_weapon_frame <= 2 && rt_frame - w_game_frame <= 1 && inverse(w_game, iw)) {
        m = mul(vr_weapon, iw);
        return true;
    }
    if (game_to_hand_frame >= 0 && rt_frame - game_to_hand_frame <= 1) {
        m = game_to_hand;
        return true;
    }
    return false;
}
// The gun's own effects the game draws in screen space (the muzzle flash's quads, the ejected case, smoke): for a while
// after a shot, those near the gun are told apart (sh3vr.cpp sh3vr_effect_map: drawn whole at their place in the world,
// turned to face the head; combat_effect_rel). `game_gun`: the drawn weapon's held bone as the game placed it, last draw.
float effects_until = 0;
Vec3 game_gun{};
int game_gun_frame = -100;
int shots_fired = 0;
// The last shot's line in the game's world (the muzzle and the barrel's way, the recoil's kick not in it: gun_ray), as
// the game laid the flash's quads along it (vr276: from the shot's origin forward, within 2 units of the line).
Vec3 shot_from_w{}, shot_dir_w{};
bool shot_line_known = false;

// The bone palette: c48 on, three registers (rows of a 3x4) a bone, 16 bones.
constexpr int kPaletteStart = 48, kBones = 16;
bool bone_affine(const float vsc[96][4], int b) {
    for (int r = 0; r < 3; r++) {
        const float* row = vsc[kPaletteStart + b * 3 + r];
        const float l = row[0] * row[0] + row[1] * row[1] + row[2] * row[2];
        if (!std::isfinite(l) || std::fabs(l - 1.0f) > 0.08f || !std::isfinite(row[3])) return false;
    }
    return true;
}
// A palette slot that holds a bone at all, scaled or not (the beam saber's blade is stretched as it extends; a bone
// collapsed to a point has rows of 0): finite, no row longer than 64, not a register left from something else.
bool bone_finite(const float vsc[96][4], int b) {
    for (int r = 0; r < 3; r++) {
        const float* row = vsc[kPaletteStart + b * 3 + r];
        const float l = row[0] * row[0] + row[1] * row[1] + row[2] * row[2];
        if (!std::isfinite(l) || l > 64.0f * 64.0f || !std::isfinite(row[3])) return false;
    }
    return true;
}
Aff read_bone(const float vsc[96][4], int b) {
    Aff a;
    for (int r = 0; r < 3; r++) {
        const float* row = vsc[kPaletteStart + b * 3 + r];
        a.r.m[r][0] = row[0], a.r.m[r][1] = row[1], a.r.m[r][2] = row[2];
        (&a.t.x)[r] = row[3];
    }
    return a;
}
void write_bone(float vsc[96][4], int b, const Aff& a) {
    for (int r = 0; r < 3; r++) {
        float* row = vsc[kPaletteStart + b * 3 + r];
        row[0] = a.r.m[r][0], row[1] = a.r.m[r][1], row[2] = a.r.m[r][2];
        row[3] = (&a.t.x)[r];
    }
}

// ---------------------------------------------------------------- state

double now = 0;                 // seconds of VR frames
bool in_hand = false;           // the equipped weapon is drawn at the main hand (first person)
int last_weapon = -1;
uint32_t last_owned = 0;
int pending_equip = -1;         // asked of the game (sh3vr_actions)
bool pending_put_away = false;  // an extra weapon holstered: the game puts it away (sh3vr_actions)
int awaiting_draw = -1;         // ... until its weapon shows
double awaiting_until = 0;
bool weapon_draw_now = false;   // the draw being set up is the weapon's
uint32_t skinned_strides[4]{};  // the vertex strides seen on the weapons' skinned draws
int n_strides = 0;
float kick = 0;                 // degrees, decaying
float flash_until = 0;          // the shot's flash and smoke in front of the muzzle until then: the guide dot looks past it

struct HolsterState {
    int content = -1;           // the weapon slot it holds
    bool hovered[2]{};
};
HolsterState holsters[kHolsterCount];
float body_yaw = 0;
bool body_yaw_set = false;

// Swings
struct Swing {
    Vec3 start;                 // tracking, where the path began
    Vec3 start_grip_w, start_tip_w;
    float travel = 0;
    bool active = false, armed = true;
    double last = -10;
    // Owner 65 (session 14): hits at contact. The hand's segment the frame before (world), for the frame's sweep; the
    // game's attack pressed for this swing; this swing has hit (one hit a swing, as the game's own attacks), and when.
    Vec3 seg_grip, seg_tip;
    bool seg_valid = false, fired = false, landed = false;
    double landed_at = -10;
    int sweeps = 0;             // the sweeps sent this swing (the log)
    float fast_for = 0;         // 0.1.3: real seconds the hand has kept the swing's speed (a short hold before it counts)
};
Swing swings[2];

// The sweeps sent to the game's battle step (queue_extra), to tell their hits from the game's own attack's: the way
// the game gives the hit (its tip's motion, 0x4B37AD) is each sweep's own, so a hit record with that way is its.
struct Sweep {
    int hand = -1, slot = -1, what = 0;
    float way[3]{};
    float speed = 0;
    double at = -10;
};
Sweep sweeps_sent[16];
int sweep_next = 0;
// A bare fist's hits (LIST 27: half the weakest melee weapon's damage): the characters a fist hit, until the game
// takes the damage (0x4B6A20 for them, sh3vr_hit_given).
struct FistHit {
    uint32_t character = 0;
    double at = -10;
};
FistHit fist_hits[4];

struct Pending {
    bool armed = false;
    bool landed = false;        // owner 65: the swing already hit at contact: the game's own volumes are dropped
    bool taken = false;         // the game's first volume took the swing; later ones follow the weapon frame by frame
    int slot = -1;
    double until = 0;           // the replacement holds while the game's attack lasts
    float prev_grip[3], prev_tip[3], cur_grip[3], cur_tip[3], radius = 0;
} pending;
double aim_pulse_until = 0, attack_from = 0, attack_until = 0, still_until = 0;

// The hit volumes VR adds (punches, clubs).
Sh3VrHit extra[4];
int n_extra = 0;
int punches_thrown = 0;

// The attack ids the game uses for each weapon's melee volume (learned from its queue).
struct Learned {
    int kind = 0, id = -1;
};
Learned learned[20];
bool learned_loaded = false;

int grab_frames = 0, light_frames = 0, pouch_frames = 0;
float play_time = 0.0f;  // seconds of play so far, 0 outside it (combat_frame)
double hand_used_at = -1.0;  // when a hand last used an event or took an item (hand_action_found), and where (world)
Vec3 hand_used_world;
// The flashlight (session 7, the owner's: "take it off their chest, and put it back on"): on her chest its beam goes
// where the head looks; a free hand closing on the chest pocket takes it and holds it (the beam from that hand, its
// trigger turns the light on and off); letting go puts it back on her chest.
int light_hand = -1;
bool light_trigger_down = false;
// LIST 45, "Take items with your hand": the spot a closing hand asked the game's action at (world), for the frames the
// action button is held for it (sh3.c's action check 0x4B9940 tests the event regions there, not in front of her).
struct {
    int hand = -1;
    Vec3 world;
    int frames = 0;
    bool found = false;
} hand_action;
float prev_grip_value[2]{};
bool grip_held[2]{};
bool stowing[2]{};              // this grip closed at a holster of the held weapon's kind: letting go puts it away
Vec3 last_grip_w[2], last_tip_w[2];
bool last_seg_valid = false;

void load_learned() {
    learned_loaded = true;
    FILE* f = std::fopen("sh3quest_learned.ini", "r");
    if (!f) return;
    int s, k, id;
    while (std::fscanf(f, " %d %d %d", &s, &k, &id) == 3)
        if (s >= 0 && s < 20) learned[s] = {k, id};
    std::fclose(f);
}
void save_learned() {
    FILE* f = std::fopen("sh3quest_learned.ini", "w");
    if (!f) return;
    for (int s = 0; s < 20; s++)
        if (learned[s].id >= 0) std::fprintf(f, "%d %d %d\n", s, learned[s].kind, learned[s].id);
    std::fclose(f);
}

bool owned(int slot) { return slot >= 0 && slot < 32 && (game.weapons_owned >> slot & 1u); }

bool holster_on(int h) {
    switch (h) {
    case kGunHolster:
    case kMeleeHolster: return opt.holsters;
    case kPouch: return opt.holsters && opt.holster_pouch;
    case kLightPocket: return opt.chest_light_touch;  // the flashlight: its own switch, on with holsters off too
    case kAmmo: return reload_pouch_on();  // "Manual reload", a gun in the hand that reloads by hand
    }
    return false;
}
// 0.1.6 (owner 122: "lets switch holsters ... so gun is on bottom right hip now and melee is on top right shoulder ...
// with toggles for perference"): "Gun on the hip, melee on the shoulder" (on to start) puts the gun's holster at the
// main-hand hip and the melee weapon's over the main-hand shoulder; off, as 0.1.0-0.1.5 had them (the gun over the
// shoulder, the melee weapon at the hip). Each holster still gives back the weapon of its kind last put there.
bool holster_at_shoulder(int h) {
    return (h == kGunHolster && !opt.gun_on_hip) || (h == kMeleeHolster && opt.gun_on_hip);
}
bool holster_at_hip(int h) {
    return (h == kGunHolster && opt.gun_on_hip) || (h == kMeleeHolster && !opt.gun_on_hip);
}
const char* holster_name(int h) {
    if (h == kGunHolster) return opt.gun_on_hip ? "hip (guns)" : "shoulder (guns)";
    if (h == kMeleeHolster) return opt.gun_on_hip ? "shoulder (melee)" : "hip (melee)";
    static const char* const kNames[] = {"", "", "belt pouch", "chest pocket", "ammo pouch"};
    return h >= 0 && h < kHolsterCount ? kNames[h] : "?";
}

// The holster's place (tracking: x right, y up, -z forward from the eyes): on the body, turned with it; left-handed
// mirrors left and right. The shoulder's is at the top of the main-hand shoulder, a little behind it (reached over the
// shoulder, the way a slung weapon is); the hip's at the main-hand hip; the supplies pouch at the other hip, a little
// behind it; the ammo pouch in front of that, at the front of the belt (where magazine pouches are worn).
Vec3 holster_place(int h) {
    const float H = player_height_m();
    const float mx = opt.left_handed ? -1.0f : 1.0f;
    Vec3 l;
    if (holster_at_shoulder(h)) l = {0.16f * mx, -0.10f * H, 0.08f};
    else if (holster_at_hip(h)) l = {0.20f * mx, -0.40f * H, -0.02f};
    else if (h == kPouch) l = {-0.20f * mx, -0.40f * H, 0.06f};
    else if (h == kAmmo) l = {-0.10f * mx, -0.38f * H, -0.06f};
    else l = {0.0f, -0.22f * H, -0.12f};  // the chest pocket: the flashlight
    if (opt.seated && (holster_at_hip(h) || h == kPouch || h == kAmmo)) l.y = -0.33f * H;
    // From the eyes with the head level: looking down at the belt does not take the belt down with the eyes.
    return level_eyes(fi.head) + rotate(yaw_quat(body_yaw), l);
}
// How near the hand must come (m). 0.1.6 (owner 122: "add radius options for weapon holsters AND mag holsters ... the
// radius for mag grabbing is a bit too small, so make it a bit larger"): "Weapon holster radius" (18 cm to start: the
// shoulder's was 18, the hip's 16) for both weapon holsters, "Ammo pouch radius" (14 cm to start; it was 10). Where two
// places overlap the nearest is taken (update_holsters).
float holster_radius(int h) {
    switch (h) {
    case kGunHolster:
    case kMeleeHolster: return std::clamp(opt.holster_radius_cm, 5.0f, 40.0f) * 0.01f;
    case kAmmo: return std::clamp(opt.ammo_pouch_radius_cm, 5.0f, 40.0f) * 0.01f;
    default: return 0.13f;
    }
}

// Which weapons a holster takes.
bool fits(int h, int slot) { return slot > 0 && holster_of(slot) == h; }

// The weapon's line from the hand (tracking): melee along the fist (the grip's -Z, through the fist to the thumb),
// guns along the controller's pointing.
Vec3 weapon_axis(int slot, int hand) {
    const HandState& s = hands[hand];
    const WeaponInfo* w = info_of(slot);
    if (w && (w->kind == kGun || w->kind == kStun)) return rotate(s.aim.orientation, Vec3{0, 0, -1});
    return rotate(s.grip.orientation, Vec3{0, 0, -1});
}

// Two hands on a weapon whose hold is not measured: the line from the main hand through the other (when the other
// holds the foregrip).
bool two_hand_line(int slot, Vec3& line) {
    const int mh = main_hand(), oh = off_hand();
    const WeaponInfo* w = info_of(slot);
    if (!w || !w->aims_two_handed || !hands[oh].on_foregrip || !hands[mh].valid || !hands[oh].valid) return false;
    const Vec3 d = hands[oh].grip.position - hands[mh].grip.position;
    if (length(d) < 0.08f) return false;
    line = normalized(d);
    return true;
}

// ---------------------------------------------------------------- the weapon's frame each frame

float rad(float deg) { return deg * kPi / 180.0f; }
float ease_of(float rate) { return 1.0f - std::exp(-rate * frame_dt()); }

Pose body_shown[2];        // the hands as sh3vr_body.cpp left them this frame (the controllers, or held at a wall)
float wall_back = 0;       // metres the weapon in the hand (and the hand) is held back from a wall
bool wall_touching = false;
Vec3 reach_from, reach_to; // tracking: the weapon's line as the controller holds it (palm to tip), for its probes
bool reach_valid = false;
int hand_role[2]{};        // 1 the main hand round the weapon's grip, 2 the other hand on its second grip
// The other hand's share of the second grip (0 at its controller, 1 on the grip): eased on as it takes the grip (about
// 0.1 s) and off as it lets go (about 0.15 s), so neither the hand nor a two-handed weapon's line (and the main hand
// round it) jumps (owner, session 13: "be sure two hand grip works/looks smoothly on all guns/weapons").
float two_w = 0;
// A weapon in both hands held back toward her (tracking, metres) where its second grip lies past her other arm's reach
// (session 13, vr195: the submachine gun's left hand left at her arm's end beside the receiver, not on its front): eased.
Vec3 reach_pull;
bool reach_pulled = false;
constexpr float kReachPullMax = 0.20f;
Quat q_mix(Quat a, Quat b, float t) {
    if (a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w < 0) b = Quat{-b.x, -b.y, -b.z, -b.w};
    Quat q{a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t, a.w + (b.w - a.w) * t};
    const float n = std::sqrt(q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w);
    return n > 1e-6f ? Quat{q.x / n, q.y / n, q.z / n, q.w / n} : a;
}

// The weapon's frame from the main hand (and the other hand on its second grip, `two_hands`): the hold's axes on the
// controller's (the aim pose for guns, the grip pose along the fist for the rest), the player's adjustments, two hands
// turning a long weapon so its second grip lies towards the other controller, and the recoil's kick.
bool build_frame(WeaponFrame& f, bool two_hands) {
    f = WeaponFrame{};
    const int w = game.weapon, mh = main_hand(), oh = off_hand();
    const Hold* hd = hold_of(w);
    if (!hd || !in_hand || w < 0 || view_mode != ViewMode::kFirstPerson || !hands[mh].valid) return false;
    const int p = profile_of(w);
    f.hold = hd;
    f.shape = &kShapes[p];
    f.profile = p;
    f.fwd = normalized(hd->fwd);
    f.up = normalized(hd->up - f.fwd * dot(hd->up, f.fwd));
    f.right = cross(f.fwd, f.up);
    f.mirror = opt.left_handed;
    const Calib& c = calib[p];
    f.scale = c.scale / kUnitsPerMetre;
    const HandState& m = hands[mh];
    f.palm = hd->palm;
    Quat q0 = hd->fist ? m.grip.orientation : m.aim.orientation;
    // The game's own grip read: the controller's grip point is its fist's centre on the weapon; a melee weapon (and the
    // stun gun) is turned in her hand as the game holds it (her hand bone at the controller, hand_at's way).
    if (const NativeGrip* ng = native_of(p, mh)) {
        f.palm = ng->palm;
        Vec3 axes[3], origin;
        if (hd->fist && ik_hand_bone_for_pose(mh, Pose{body_shown[mh].position, m.grip.orientation}, axes, origin)) {
            Vec3 col[3];
            const Vec3 model_axes[3] = {f.right, f.up, f.fwd * -1.0f};
            for (int k = 0; k < 3; k++) {
                col[k] = {};
                for (int i = 0; i < 3; i++) col[k] = col[k] + axes[i] * dot(model_axes[k], ng->ax[i]);
            }
            col[0] = normalized(col[0]);
            col[1] = normalized(col[1] - col[0] * dot(col[1], col[0]));
            col[2] = cross(col[0], col[1]);
            q0 = quat_of_axes(col[0], col[1], col[2]);
        }
    }
    const Quat q = q0 * quat_axis(Vec3{1, 0, 0}, rad(c.turn_deg[0])) * quat_axis(Vec3{0, 1, 0}, rad(c.turn_deg[1])) *
                   quat_axis(Vec3{0, 0, -1}, rad(c.turn_deg[2]));
    f.turn = q;
    f.base = body_shown[mh].position + rotate(q, Vec3{c.move_cm[0], c.move_cm[1], -c.move_cm[2]} * 0.01f);
    const WeaponInfo* info = info_of(w);
    if (two_hands && hd->second && info && info->aims_two_handed && hands[oh].valid) {
        const Vec3 spot = frame_point(f, second_point(f));
        const Vec3 along = spot - f.base, line = body_shown[oh].position - body_shown[mh].position;
        if (length(along) > 0.03f && length(line) > 0.08f)
            f.turn = q_mix(Quat{}, quat_arc(normalized(along), normalized(line)), two_w) * f.turn;
    }
    f.aim_turn = f.turn;
    if (kick > 0) f.turn = quat_axis(rotate(f.turn, Vec3{1, 0, 0}), rad(kick)) * f.turn;
    f.placed = true;
    return true;
}


// A tracking point in the weapon's model (the held bone's units).
Vec3 model_of(const WeaponFrame& f, Vec3 t) {
    const Vec3 l = rotate(conjugate(f.turn), t - f.base) * (1.0f / f.scale);
    return f.palm + f.right * l.x + f.up * l.y - f.fwd * l.z;
}

// The second grip's place for the other hand (tracking), the shotgun's pump drawn back with it.
Vec3 second_spot(const WeaponFrame& f) {
    Vec3 at = second_point(f);
    const float pump = reload_pump_back();
    if (pump > 0) at = at - f.fwd * (pump * kUnitsPerMetre);
    return frame_point(f, at);
}

// Closest distance between segments p1-q1 and p2-q2.
float seg_seg(Vec3 p1, Vec3 q1, Vec3 p2, Vec3 q2) {
    const Vec3 d1 = q1 - p1, d2 = q2 - p2, r = p1 - p2;
    const float a = dot(d1, d1), e = dot(d2, d2), f = dot(d2, r);
    float s = 0, t = 0;
    if (a <= 1e-9f && e <= 1e-9f) return length(r);
    if (a <= 1e-9f) t = std::clamp(f / e, 0.0f, 1.0f);
    else {
        const float c = dot(d1, r);
        if (e <= 1e-9f) s = std::clamp(-c / a, 0.0f, 1.0f);
        else {
            const float b = dot(d1, d2), den = a * e - b * b;
            s = den > 1e-9f ? std::clamp((b * f - c * e) / den, 0.0f, 1.0f) : 0.0f;
            t = (b * s + f) / e;
            if (t < 0) t = 0, s = std::clamp(-c / a, 0.0f, 1.0f);
            else if (t > 1) t = 1, s = std::clamp((b - c) / a, 0.0f, 1.0f);
        }
    }
    return length((p1 + d1 * s) - (p2 + d2 * t));
}

// The weapon's capsules (model units): its measured shape, else a rod from the palm to the tip.
int shape_caps(const WeaponFrame& f, Cap* out) {
    int n = 0;
    if (f.shape && f.shape->n > 0)
        for (int i = 0; i < f.shape->n; i++) out[n++] = f.shape->c[i];
    else out[n++] = {f.hold->palm - f.hold->rod * 10.0f, f.hold->tip, 9.0f};
    return n;
}

// The other hand on the weapon's second grip, or reaching for it (LIST 26). Weapons with a measured hold: the second
// grip's own place on the weapon (taken within 12 cm; a long weapon held while the hands keep about their distance on
// it, a short one while the hand stays within 30 cm). Else the foregrip along the weapon's line as before.
void update_two_hand() {
    const int mh = main_hand(), oh = off_hand();
    HandState& o = hands[oh];
    const float g = oh == kLeft ? fi.controls.left_grip : fi.controls.right_grip;
    const WeaponInfo* w = info_of(game.weapon);
    // The rig's record (session 13): where each weapon's second grip lies from the main controller, half a second
    // after it is drawn (the native grips read by then), so a check can put the other hand there.
    {
        static int seen_w = -1;
        static double seen_t = 0;
        static bool told = false;
        if (game.weapon != seen_w) seen_w = game.weapon, seen_t = now, told = false;
        if (!told && now - seen_t > 0.5 && wf_one_hand.placed && wf_one_hand.hold->second && hands[mh].valid) {
            told = true;
            const Vec3 d = second_spot(wf_one_hand) - body_shown[mh].position;
            rt_log("VR: frame %d: %s's second grip %.3f %.3f %.3f m from the %s controller (tracking: right, up, back; "
                   "%.1f cm)", rt_frame, weapon_name(game.weapon), d.x, d.y, d.z, mh == kRight ? "right" : "left",
                   length(d) * 100.0f);
        }
    }
    // A magazine or shell in the off hand, or the slide in it (LIST 46): not on the foregrip.
    if (!opt.two_hand || !w || !in_hand || !o.valid || !hands[mh].valid || view_mode != ViewMode::kFirstPerson ||
        g < 0.5f || reload_owns_hand(oh) || (wf_one_hand.placed && !wf_one_hand.hold->second)) {
        if (o.on_foregrip) {
            buzz(oh, 0.15f, 0.02f);
            // session 13 (vr190: the submachine gun's left hand open beside it at the picture): why the hold ended
            rt_log("VR: frame %d: the %s hand let go of %s's second grip (grip %.2f, in the hand %d, reload %d, valid "
                   "%d %d, view %d)", rt_frame, oh == kLeft ? "left" : "right", weapon_name(game.weapon), g, in_hand ? 1 : 0,
                   reload_owns_hand(oh) ? 1 : 0, o.valid ? 1 : 0, hands[mh].valid ? 1 : 0, (int)view_mode);
        }
        o.on_foregrip = false;
        return;
    }
    bool on;
    float away = 0, limit = 0;
    if (wf_one_hand.placed) {
        const Vec3 spot = second_spot(wf_one_hand);
        if (o.on_foregrip && w->aims_two_handed) {
            const float L = length(spot - wf_one_hand.base);
            const float d = length(body_shown[oh].position - body_shown[mh].position);
            away = std::fabs(d - L), limit = 0.25f;
        } else away = length(body_shown[oh].position - spot), limit = o.on_foregrip ? 0.30f : 0.12f;
    } else {
        const Vec3 axis = weapon_axis(game.weapon, mh);
        const Vec3 spot = hands[mh].grip.position + axis * (w->foregrip > 0 ? w->foregrip : -0.03f);
        limit = o.on_foregrip ? 0.30f : (w->foregrip > 0 ? 0.14f : 0.10f);  // let go only well away
        away = length(o.grip.position - spot);
    }
    on = away < limit;
    if (on && !o.on_foregrip) {
        buzz(oh, 0.3f, 0.03f);
        hand_sound(kSoundGrabWeapon, world_point(o.grip.position), game.weapon);
        rt_log("VR: frame %d: the %s hand took %s's second grip (%.2f m from it)", rt_frame,
               oh == kLeft ? "left" : "right", weapon_name(game.weapon), away);
    } else if (!on && o.on_foregrip)
        rt_log("VR: frame %d: the %s hand left %s's second grip: %.2f m out (over %.2f)", rt_frame,
               oh == kLeft ? "left" : "right", weapon_name(game.weapon), away, limit);
    o.on_foregrip = on;
}

// The weapon placed for this frame: its frame, held back from walls (its probes, sh3vr_body.cpp), and the hands posed
// round it (the owner: "the hand positions and grip points on weapons should feel natural and native").
void place_weapon() {
    const int mh = main_hand(), oh = off_hand();
    hand_role[0] = hand_role[1] = 0;
    native_shown[0] = native_shown[1] = -1;
    build_frame(wf, two_w > 0.001f);
    // The weapon's line as the controller holds it: the probes look along it for the world.
    reach_valid = false;
    if (wf.placed) {
        reach_from = wf.base;
        reach_to = frame_point(wf, wf.hold->tip);
        reach_valid = true;
    } else if (in_hand && game.weapon > 0 && view_mode == ViewMode::kFirstPerson && hands[mh].valid) {
        const WeaponInfo* w = info_of(game.weapon);
        Vec3 axis = weapon_axis(game.weapon, mh), line;
        if (two_hand_line(game.weapon, line)) axis = line;
        reach_from = body_shown[mh].position;
        reach_to = reach_from + axis * (w ? w->reach : 0.25f);
        reach_valid = true;
    }
    // Held back from a wall: the free part of the line the probes found (a frame late), at once when it goes in and
    // eased when it comes out.
    float want = 0;
    const float free_t = weapon_wall_free();
    if (reach_valid && free_t < 1.0f) want = (1.0f - free_t) * length(reach_to - reach_from);
    want = std::min(want, 0.6f);
    if (!reach_valid) want = 0;
    if (want > wall_back) wall_back = want;
    else wall_back += (want - wall_back) * ease_of(10.0f);
    const Vec3 back_dir = reach_valid ? normalized(reach_to - reach_from) : Vec3{};
    if (wall_back > 0.002f) {
        if (!wall_touching) {
            wall_touching = true;
            rt_log("VR: frame %d: %s held at a surface", rt_frame, weapon_name(game.weapon));
            // Its knock on the surface where its line met it (louder the faster it came: detail, cm a second).
            hand_sound(kSoundWeaponWall, world_point(reach_valid ? lerp(reach_from, reach_to, free_t)
                                                                 : hands[mh].grip.position),
                       (int)(hands[mh].speed * 100.0f));
            // A swing into the wall: a thump.
            if (opt.haptic_surfaces && hands[mh].speed > 1.0f) {
                buzz(mh, std::min(0.9f, 0.45f + hands[mh].speed * 0.12f), 0.06f, 80.0f);
                if (hands[oh].on_foregrip) buzz(oh, 0.5f, 0.05f, 80.0f);
            }
        }
        if (opt.haptic_surfaces) {
            buzz(mh, std::clamp(0.08f + wall_back * 3.0f, 0.08f, 0.6f), frame_dt() * 1.5f, 140.0f);
            if (hands[oh].on_foregrip) buzz(oh, std::clamp(0.05f + wall_back * 2.0f, 0.05f, 0.4f), frame_dt() * 1.5f, 140.0f);
        }
    } else if (wall_touching && wall_back < 0.001f) {
        wall_touching = false;
    }
    const Vec3 held = back_dir * -wall_back;
    if (!wf.placed) {
        // A weapon whose hold is not measured stays in her animated hand: the hand itself is held back.
        if (reach_valid) hands[mh].shown.position = hands[mh].shown.position + held;
        reach_pull = Vec3{}, reach_pulled = false;
        return;
    }
    wf.base = wf.base + held;
    const Hold& hd = *wf.hold;
    const float mxm = mh == kLeft ? -1.0f : 1.0f;
    // The main hand round the grip: its fist's centre on the palm point, its rod along the grip, its palm on the
    // grip's side (the right hand's on the right, the left hand's on the left).
    const Vec3 right_t = rotate(wf.turn, Vec3{1, 0, 0});
    // The game's own gripping hand: her hand bone where the game holds the weapon against it (the bone's place in the
    // weapon moved with the weapon: the recoil, a wall, the player's adjustments).
    auto native_pose = [&](int h, const NativeGrip& ng, Vec3 shift, Pose& out) {
        Vec3 axes[3];
        for (int i = 0; i < 3; i++) axes[i] = frame_dir(wf, ng.ax[i]);
        return ik_pose_for_hand_bone(h, axes, frame_point(wf, ng.o + shift), out);
    };
    Pose np;
    const NativeGrip* nm = native_of(wf.profile, mh);
    if (nm && native_pose(mh, *nm, Vec3{}, np)) {
        hands[mh].shown = np;
        native_shown[mh] = native_group_of(wf.profile, mh);
    } else hands[mh].shown = hand_round(mh, wf.base, frame_dir(wf, hd.rod), right_t * -mxm);
    hand_role[mh] = 1;
    // The other hand on the second grip: there, posed as that grip is held (the game's own left hand on it, else her
    // finger hand round it, mirrored for the left hand's).
    // Taking and letting go eased (two_w): the hand from its controller's place onto the grip and back.
    const bool two = two_w > 0.001f && hd.second && hands[oh].valid;
    Pose on;
    bool native_on = false;
    Vec3 pull_want{};
    float past = 0;
    if (two) {
        const NativeGrip* no = mh == kRight ? native_of(wf.profile, oh) : nullptr;
        const float pump = reload_pump_back();
        if (no && native_pose(oh, *no, wf.fwd * -(pump * kUnitsPerMetre), np)) on = np, native_on = true;
        else
            on = hand_round(oh, second_spot(wf), frame_dir(wf, mirrored_dir(wf, hd.second_rod)),
                            frame_dir(wf, mirrored_dir(wf, hd.second_palm)));
        // Her other hand's bone on the grip past her arm's reach from her shoulder (the arm as last solved): the weapon
        // and both hands held back toward that shoulder by what the arm lacks (at most 20 cm), in with the take, so the
        // hand closes on the grip instead of staying at the arm's end beside it.
        Vec3 axes[3], bone, shoulder;
        float reach = 0;
        // (only while the grip is held: letting go, the weapon turns back to the main hand's aim, its grip away from
        // the other hand: vr196's short pulls on the let-go)
        if (hands[oh].on_foregrip && ik_hand_bone_for_pose(oh, on, axes, bone) && ik_arm_reach(oh, shoulder, reach)) {
            const Vec3 to = bone - shoulder;
            const float d = length(to), fit = reach - 0.01f;
            if (d > fit && d > 0.05f) {
                past = d - fit;
                Vec3 back = to * ((fit - d) / d);
                const float n = length(back);
                if (n > kReachPullMax) back = back * (kReachPullMax / n);
                pull_want = back * two_w;
            }
        }
    }
    {
        const float a = length(pull_want), b = length(reach_pull);
        reach_pull = lerp(reach_pull, pull_want, ease_of(a > b ? 18.0f : 10.0f));
        if (a < 1e-4f && length(reach_pull) < 0.001f) reach_pull = Vec3{};
        const bool pulling = length(reach_pull) > 0.005f;
        if (pulling && !reach_pulled)
            rt_log("VR: frame %d: %s held back toward her: the %s hand's second grip %.1f cm past her arm's reach "
                   "(held %.1f cm back)", rt_frame, weapon_name(game.weapon), oh == kLeft ? "left" : "right",
                   past * 100.0f, a * 100.0f);
        else if (!pulling && reach_pulled)
            rt_log("VR: frame %d: %s back at the controllers (its second grip within her reach)", rt_frame,
                   weapon_name(game.weapon));
        reach_pulled = pulling;
        if (length(reach_pull) > 0) {
            wf.base = wf.base + reach_pull;
            hands[mh].shown.position = hands[mh].shown.position + reach_pull;
            on.position = on.position + reach_pull;
        }
    }
    if (two) {
        if (two_w >= 0.999f) hands[oh].shown = on;
        else {
            const Pose from = hands[oh].shown;
            hands[oh].shown = Pose{lerp(from.position, on.position, two_w), q_mix(from.orientation, on.orientation, two_w)};
        }
        if (native_on && two_w >= 0.5f) native_shown[oh] = native_group_of(wf.profile, oh);
        if (hands[oh].on_foregrip || two_w >= 0.5f) hand_role[oh] = 2;
        // The rig's record (session 13, the owner: "be sure two hand grip works/looks smoothly on all guns/weapons"):
        // each frame of a take or a let-go, how far along the ease is; once a take settles, how the hand is turned on
        // the grip against the weapon (tracking: x right, y up, z back).
        static int eased_lines = 0, settled_for = -1;
        if (two_w < 0.999f && eased_lines < 300) {
            eased_lines++;
            rt_log("VR: frame %d: %s's second grip %s, eased %.2f: the hand %.1f cm from the grip, %.1f from the "
                   "controller", rt_frame, weapon_name(game.weapon), hands[oh].on_foregrip ? "taken" : "let go", two_w,
                   length(hands[oh].shown.position - on.position) * 100.0f,
                   length(hands[oh].shown.position - body_shown[oh].position) * 100.0f);
        }
        if (two_w >= 0.999f && settled_for != game.weapon) {
            settled_for = game.weapon;
            const float px = oh == kLeft ? 1.0f : -1.0f;
            const Vec3 palm = rotate(on.orientation, Vec3{px, 0, 0}), rod = rotate(on.orientation, Vec3{0, 0, -1});
            const Vec3 cpalm = rotate(hands[oh].grip.orientation, Vec3{px, 0, 0});
            const Vec3 up = rotate(wf.turn, Vec3{0, 1, 0}), fwd = rotate(wf.turn, Vec3{0, 0, -1});
            rt_log("VR: frame %d: %s's second grip held (%s): the palm toward %.2f %.2f %.2f, the fist along %.2f %.2f "
                   "%.2f (little finger to thumb); the weapon's top %.2f %.2f %.2f, forward %.2f %.2f %.2f; the "
                   "controller's palm toward %.2f %.2f %.2f", rt_frame, weapon_name(game.weapon),
                   native_on ? "the game's own hand" : "her finger hand", palm.x, palm.y, palm.z, rod.x, rod.y, rod.z,
                   up.x, up.y, up.z, fwd.x, fwd.y, fwd.z, cpalm.x, cpalm.y, cpalm.z);
        }
        if (!hands[oh].on_foregrip) settled_for = -1;  // letting go: the next take is logged again
    }
    // The slide or the cocking knob in the other hand (LIST 46): the hand stays on it as it moves. A part it holds,
    // lined up with the well: the hand guided with it onto the well's line (its fingers stay on the part).
    Pose on_part;
    if (reload_hand_spot(oh, on_part)) {
        hands[oh].shown = on_part;
        native_shown[oh] = -1;
        if (reload_held_part(oh) < 0) hand_role[oh] = 3;
    }
}

void request_equip(int slot) {
    if (game.weapon > 0 && slot == base_slot(game.weapon)) {
        in_hand = true;
        rt_log("VR: %s drawn from the holster", weapon_name(slot));
        return;
    }
    pending_equip = slot;
    awaiting_draw = slot;
    awaiting_until = now + 4.0;  // the game loads its model over a few frames (sh3.c swap_step)
    rt_log("VR: %s drawn from the holster: the game is asked to equip it", weapon_name(slot));
}

// ---------------------------------------------------------------- per frame

void update_body_yaw() {
    const float head = tracking_yaw(fi.head.orientation);
    if (!body_yaw_set) {
        body_yaw = head;
        body_yaw_set = true;
    }
    const float d = wrap(head - body_yaw);
    // The body follows the head lazily: at once past 70 degrees, slowly within.
    if (std::fabs(d) > 1.2f) body_yaw = wrap(head - (d > 0 ? 1.2f : -1.2f));
    else body_yaw = wrap(body_yaw + d * std::min(1.0f, 1.5f * frame_dt()));
}

void track_weapon() {
    const int w = game.weapon;
    if (w != last_weapon) {
        // A new weapon: in the hand (the inventory equipped it, or a holster drew it).
        in_hand = w >= 0;
        if (w >= 0) rt_log("VR: weapon %d (%s) in the hand", w, weapon_name(w));
        // "Buzz on weapon change" (LIST 44): a weapon taken, changed or put away, after the first one seen.
        static bool seen = false;
        if (seen && opt.haptic_weapon_change) buzz(main_hand(), w >= 0 ? 0.35f : 0.2f, 0.05f, 140.0f);
        seen = true;
        last_weapon = w;
        kick = 0;
    }
    if (awaiting_draw >= 0 && (game.weapon == awaiting_draw || now > awaiting_until)) {
        if (game.weapon != awaiting_draw) rt_log("VR: the game did not equip %s", weapon_name(awaiting_draw));
        else in_hand = true;
        awaiting_draw = -1;
    }
    if (!opt.holsters && w >= 0) in_hand = true;
    // Each holster gives back the weapon last put there; one still empty takes the first of its kind the player owns,
    // in the inventory's order (a weapon taken away by the story leaves its holster empty again).
    if (game.weapons_owned != last_owned) {
        for (HolsterState& hst : holsters)
            if (hst.content >= 0 && !owned(hst.content)) hst.content = -1;
        for (int s : kInventoryOrder) {
            const int h = holster_of(s);
            if (h >= 0 && owned(s) && holsters[h].content < 0) {
                holsters[h].content = s;
                rt_log("VR: the %s holster holds %s (weapon %d)", holster_name(h), weapon_name(s), s);
            }
        }
        last_owned = game.weapons_owned;
    }
    const int mh = main_hand();
    hands[mh].holding_weapon = in_hand && w >= 0 && view_mode == ViewMode::kFirstPerson;
    hands[off_hand()].holding_weapon = false;
}

void update_holsters() {
    for (int h = 0; h < 2; h++) {
        const float g = h == kLeft ? fi.controls.left_grip : fi.controls.right_grip;
        const bool down = g > 0.6f && prev_grip_value[h] <= 0.6f, up = g < 0.4f && prev_grip_value[h] >= 0.4f;
        prev_grip_value[h] = g;
        if (down) grip_held[h] = true, stowing[h] = false;
        const HandState& s = hands[h];
        const bool is_main = h == main_hand();
        if (light_hand == h) {
            // The flashlight goes back on her chest when the hand lets go (anywhere: it is never lost), when the hand
            // is lost or play stops, or when the game puts a weapon in this hand.
            const bool weapon_here = is_main && in_hand && game.weapon > 0;
            if (up || g < 0.4f || !s.valid || !in_play() || view_mode != ViewMode::kFirstPerson || weapon_here) {
                light_hand = -1;
                light_trigger_down = false;
                if (s.valid) hand_sound(kSoundLightPocket, world_point(holster_place(kLightPocket)), 0);
                buzz(h, 0.3f, 0.03f);
                rt_log("VR: the flashlight back on her chest (the %s hand %s)", h == kLeft ? "left" : "right",
                       weapon_here ? "took a weapon" : up || g < 0.4f ? "let go" : "lost");
            }
            if (up) grip_held[h] = false, stowing[h] = false;
            continue;  // the hand holding it reaches no holster
        }
        if (!s.valid || !in_play()) {
            if (up) grip_held[h] = false, stowing[h] = false;
            continue;
        }
        int near_h = -1;
        float best = 1e9f;
        for (int k = 0; k < kHolsterCount; k++) {
            if (!holster_on(k)) continue;
            // Weapons from the main hand; the pouch and the light pocket from either; the ammo pouch from the off hand
            // (the main hand holds the gun).
            if (!is_main && k != kPouch && k != kLightPocket && k != kAmmo) continue;
            if (is_main && k == kAmmo) continue;
            const float d = length(s.grip.position - holster_place(k));
            const bool over = d < holster_radius(k);
            if (over && !holsters[k].hovered[h]) buzz(h, 0.22f, 0.02f, 180.0f);
            holsters[k].hovered[h] = over;
            if (over && d < best) best = d, near_h = k;
        }
        if (down && near_h >= 0) {
            if (near_h == kAmmo) {
                reload_take_from_pouch(h);  // a magazine, shell or battery (or a click: none to spare)
            } else if (near_h == kPouch) {
                pouch_frames = 4;  // the supplies (health drinks, ampoules): the game's own menu
                buzz(h, 0.3f, 0.03f);
                hand_sound(kSoundPouch, world_point(holster_place(kPouch)), 0);
            } else if (near_h == kLightPocket) {
                const bool free = !(is_main && in_hand && game.weapon > 0);
                if (free && light_hand < 0 && view_mode == ViewMode::kFirstPerson) {
                    light_hand = h;  // taken off her chest: held while the grip stays closed
                    light_trigger_down = (h == kLeft ? fi.controls.left_trigger : fi.controls.right_trigger) > 0.6f;
                    hand_sound(kSoundLightPocket, world_point(s.grip.position), 1);
                    buzz(h, 0.4f, 0.04f);
                    rt_log("VR: the flashlight taken off her chest by the %s hand", h == kLeft ? "left" : "right");
                } else {
                    light_frames = 4;  // a hand holding a weapon: the flashlight on or off where it is
                    buzz(h, 0.3f, 0.03f);
                }
            } else if (is_main) {
                const int c = holsters[near_h].content;
                if (in_hand && game.weapon > 0 && fits(near_h, game.weapon)) {
                    // Holding one of its kind: it goes in when the grip lets go (below).
                    stowing[h] = true;
                } else if (c > 0 && owned(c)) {
                    // The one held (if any) goes to its own holster (a gun to the gun holster, a melee weapon to
                    // the melee holster); the holster's comes out.
                    if (in_hand && game.weapon > 0 && holster_of(game.weapon) >= 0)
                        holsters[holster_of(game.weapon)].content = base_slot(game.weapon);
                    request_equip(c);
                    buzz(h, 0.45f, 0.04f);
                    hand_sound(kSoundHolsterDraw, world_point(holster_place(near_h)), c);
                } else rt_log("VR: the %s holster is empty", holster_name(near_h));
            }
        }
        if (up) {
            grip_held[h] = false;
            // Only a grip that closed with the weapon already in the hand puts it away: a grip held through a draw and
            // let go before the hand has left the holster keeps the weapon just drawn.
            const bool stow = stowing[h];
            stowing[h] = false;
            if (stow && is_main && in_hand && game.weapon > 0 && near_h >= 0 && fits(near_h, game.weapon)) {
                holsters[near_h].content = base_slot(game.weapon);
                in_hand = false;
                // An extra weapon's model is not known to VR, so it cannot be hidden in her hand: the game puts it
                // away itself (nothing equipped) and draws it again from the holster.
                if (!info_of(game.weapon)) pending_put_away = true;
                buzz(h, 0.35f, 0.04f);
                hand_sound(kSoundHolsterPut, world_point(holster_place(near_h)), game.weapon);
                rt_log("VR: %s put away (%s holster)", weapon_name(game.weapon), holster_name(near_h));
            }
        }
    }
}

struct Thresholds {
    float speed, travel, net, cooldown;
};
Thresholds thresholds(int what) {
    // 0.1.3 (owner 84: "physical melee seems to sensitive. im just simply moving the knife arm gently/arm in general,
    // and melee animation/sound happens"): every swing asks more than 0.1.2's (knife 1.75 m/s, 0.14 m; stun gun 1.40,
    // 0.10; heavy 2.80, 0.30; gun 2.60, 0.26; a punch 2.20, 0.18), on top of the speeds now taken over the real time
    // between frames (motion_dt). A deliberate swing passes 3 m/s; a gentle move or a wave stays under 1.5.
    // Owner 89 ("melee should be fixed for eveyrthing, not just knife ofc"), vr258: the heavy weapons' 0.32 m of fast
    // travel let a 40 cm swing at 5.8 m/s (the pipe, katana, maul) miss by a hair, so a heavy weapon and a gun as a club
    // ask a shorter fast path; the speed (twice 0.1.2's in real time, never reached by a gentle or moderate move) is
    // what keeps an arm that only moves from swinging.
    switch (what) {
    case kLight: return {2.40f, 0.20f, 0.14f, 0.22f};  // the knife
    case kStun: return {2.00f, 0.14f, 0.10f, 0.30f};   // a jab with the stun gun's prongs
    case kHeavy: return {3.00f, 0.26f, 0.20f, 0.38f};  // the steel pipe, katana, maul, beam saber
    case kGun: return {2.80f, 0.24f, 0.18f, 0.45f};    // a gun as a club
    default: return {2.80f, 0.22f, 0.15f, 0.30f};      // a punch
    }
}

// The hand's weapon segment in the world (grip and tip, game units): a weapon placed by its hold from its palm point to
// its far end (where it is drawn, held back at a wall too); else along the hand by the weapon's reach (a fist's).
void segment(int hand, int slot, Vec3& grip_w, Vec3& tip_w, float& radius) {
    const HandState& s = hands[hand];
    const WeaponInfo* w = info_of(slot);
    radius = (w ? w->radius : 0.12f) * kUnitsPerMetre;
    if (hand == main_hand() && wf.placed && slot == game.weapon) {
        grip_w = world_point(wf.base);
        tip_w = world_point(frame_point(wf, wf.hold->tip));
        return;
    }
    Vec3 axis = weapon_axis(slot, hand);
    Vec3 line;
    if (two_hand_line(slot, line)) axis = line;
    const float reach = w ? w->reach : 0.12f;  // a fist
    grip_w = world_point(s.shown.position);
    tip_w = world_point(s.shown.position + axis * reach);
}

// The PC VR mod's ExtendMeleeWeaponSegment: the game's volume is a thin swept line, so the segment is lengthened by
// the weapon's radius at both ends (the blade's width reaches past the grip and the tip).
void extend_segment(float* grip, float* tip, float radius) {
    float d[3] = {tip[0] - grip[0], tip[1] - grip[1], tip[2] - grip[2]};
    const float n = std::sqrt(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
    if (!(n > 1e-3f) || !(radius > 0)) return;
    for (int i = 0; i < 3; i++) {
        d[i] *= radius / n;
        grip[i] -= d[i];
        tip[i] += d[i];
    }
}
void extend_hit(Sh3VrHit& h) {
    extend_segment(h.prev_grip, h.prev_tip, h.radius);
    extend_segment(h.cur_grip, h.cur_tip, h.radius);
}

// A hit volume VR adds at the battle step's start (the attack learned for learned_slot). `hand` >= 0: one of a
// swing's sweeps (owner 65), kept with its way to tell its hit.
bool queue_extra(int learned_slot, const Vec3& pg, const Vec3& pt, const Vec3& cg, const Vec3& ct, float radius,
                 int hand = -1, int slot = -1, int what = 0, float speed = 0) {
    if (n_extra >= 4 || learned_slot < 0 || learned[learned_slot].id < 0) return false;
    Sh3VrHit& h = extra[n_extra++];
    h.kind = learned[learned_slot].kind;
    h.battle_id = learned[learned_slot].id;
    const Vec3 v[4] = {pg, pt, cg, ct};
    float* dst[4] = {h.prev_grip, h.prev_tip, h.cur_grip, h.cur_tip};
    for (int i = 0; i < 4; i++) dst[i][0] = v[i].x, dst[i][1] = v[i].y, dst[i][2] = v[i].z, dst[i][3] = 1.0f;
    h.radius = radius;
    extend_hit(h);
    if (hand >= 0) {
        Sweep& s = sweeps_sent[sweep_next];
        sweep_next = (sweep_next + 1) % 16;
        s.hand = hand, s.slot = slot, s.what = what, s.speed = speed, s.at = now;
        for (int i = 0; i < 3; i++) s.way[i] = h.cur_tip[i] - h.prev_tip[i];  // as the game takes it (0x4B37AD)
    }
    return true;
}

// The learned attack to borrow: punches the knife's (the weakest), clubs the pipe's; else the nearest known.
int borrow(bool club) {
    static const int kOrderClub[4] = {5, 7, 6, 4};
    static const int kOrderPunch[4] = {4, 5, 6, 7};
    const int* order = club ? kOrderClub : kOrderPunch;
    for (int i = 0; i < 4; i++)
        if (learned[order[i]].id >= 0) return order[i];
    return -1;
}

// The game's own attack for a swing of a melee weapon (its motion, its sound, her stamina; the stun gun's touch also
// its hit): the trigger pulses (sh3vr_actions), its hit volumes put where the weapon went (combat_melee_volume) unless
// the swing has already hit at contact (owner 65: then they are dropped).
void fire_game_attack(const Swing& sw, int slot, int what, const Vec3& g, const Vec3& tip, float radius) {
    pending.armed = true;
    pending.taken = false;
    pending.landed = sw.landed;
    pending.slot = slot;
    pending.until = now + (what == kHeavy ? 1.3 : 0.9);
    const Vec3 v[4] = {sw.start_grip_w, sw.start_tip_w, g, tip};
    float* dst[4] = {pending.prev_grip, pending.prev_tip, pending.cur_grip, pending.cur_tip};
    for (int i = 0; i < 4; i++) dst[i][0] = v[i].x, dst[i][1] = v[i].y, dst[i][2] = v[i].z;
    pending.radius = radius;
    if (what == kHeavy) {
        aim_pulse_until = now + 1.2;
        attack_from = now + 0.12;
        attack_until = now + 0.42;
        still_until = now + 1.2;
    } else {
        aim_pulse_until = now + 0.95;
        attack_from = now + 0.035;
        attack_until = now + 0.24;
    }
}

// What each hand swings this frame (the main hand its weapon, a gun as a club; an empty hand its fist), -1 none.
int swing_kind(int h, int& slot) {
    slot = -1;
    if (h == main_hand() && in_hand && game.weapon >= 0) {
        slot = game.weapon;
        const WeaponInfo* w = info_of(slot);
        if (!w) return -1;  // an extra weapon: the game's own attack (the trigger)
        if (w->kind == kGun && !opt.gun_club) return -1;
        return w->kind;
    }
    if (h != main_hand() && hands[h].on_foregrip) return -1;  // the other hand on the weapon swings with it
    return opt.punches ? kUnknown : -1;                       // a fist
}

void update_swings() {
    if (!opt.physical_melee || view_mode != ViewMode::kFirstPerson || !in_play() || !map_ready()) {
        for (Swing& s : swings) s.active = false, s.travel = 0, s.seg_valid = false;
        last_seg_valid = false;
        return;
    }
    for (int h = 0; h < 2; h++) {
        const HandState& s = hands[h];
        Swing& sw = swings[h];
        int slot;
        const int what = s.valid ? swing_kind(h, slot) : -1;
        if (what < 0) {
            sw.active = false, sw.travel = 0, sw.seg_valid = false;
            continue;
        }
        const Thresholds t = thresholds(what);
        Vec3 g_now, tip_now;
        float radius;
        segment(h, slot, g_now, tip_now, radius);
        if (s.speed <= t.speed * 0.25f) sw.armed = true;
        // 0.1.3 (owner 84): a swing begins at half its speed (0.1.2: 0.4), only the fast part of the path counts as
        // its travel (at 60% of the speed and over; 0.1.2 counted all of it from 40%, so a slow wave added up), over
        // the real time (motion_dt), and the speed must hold 25 ms (three frames at 120 Hz) before the swing counts.
        if (s.speed >= t.speed * 0.5f) {
            if (!sw.active) {
                sw.active = true;
                sw.start = s.grip.position;
                sw.travel = 0;
                sw.fast_for = 0;
                sw.fired = sw.landed = false;
                sw.sweeps = 0;
                // the path from where the weapon was the frame before the hand sped up
                sw.start_grip_w = sw.seg_valid ? sw.seg_grip : g_now;
                sw.start_tip_w = sw.seg_valid ? sw.seg_tip : tip_now;
            }
            if (s.speed >= t.speed * 0.6f) sw.travel = std::min(0.75f, sw.travel + s.speed * motion_dt());
            sw.fast_for = s.speed >= t.speed ? sw.fast_for + motion_dt() : 0.0f;
        } else {
            if (sw.active && sw.sweeps > 0 && !sw.landed) {
                static int told = 0;  // the rig (no one to hit at the start): the sweeps a swing sent, met no one
                if (told++ < 30)
                    rt_log("VR: frame %d: the %s %s's swing sent %d sweeps (the hand's travel %.2f m), met no one",
                           rt_frame, h == kLeft ? "left" : "right", what == kUnknown ? "fist" : weapon_name(slot),
                           sw.sweeps, sw.travel);
            }
            sw.active = false;
            sw.travel = 0;
            sw.fast_for = 0;
        }
        const float net = sw.active ? length(s.grip.position - sw.start) : 0.0f;
        const float cooldown = slot == 7 ? 0.56f : t.cooldown;
        const bool game_attack = what == kLight || what == kHeavy || what == kStun;
        // The swing felt once it is a real one (the PC mod's speed, travel and distance): a melee weapon's swing
        // presses the game's own attack. A hit is not waited for: it comes at contact (below).
        const bool heavy_busy = what == kHeavy && (now < still_until || pending.armed);
        if (sw.armed && sw.active && !sw.fired && !heavy_busy && now - sw.last >= cooldown && s.speed >= t.speed &&
            sw.fast_for >= 0.025f && sw.travel >= t.travel && net >= t.net) {
            sw.armed = false;
            sw.fired = true;
            sw.last = now;
            {
                static int told = 0;  // 0.1.3: what a swing that counted was (the first 20), to set the thresholds by
                if (told++ < 20)
                    rt_log("VR: frame %d: the %s %s's swing counts: %.2f m/s (it asks %.2f), held %.0f ms, the fast "
                           "travel %.2f m (asks %.2f), %.2f m from its start (asks %.2f)%s", rt_frame,
                           h == kLeft ? "left" : "right", what == kUnknown ? "fist" : weapon_name(slot), s.speed,
                           t.speed, sw.fast_for * 1000.0f, sw.travel, t.travel, net, t.net,
                           game_attack ? ": the game's attack" : "");
            }
            buzz(h, what == kHeavy ? 0.65f : 0.45f, 0.06f);
            if (game_attack) fire_game_attack(sw, slot, what, g_now, tip_now, radius);
            else if (what == kUnknown) punches_thrown++;
            if ((what == kGun || what == kUnknown) && borrow(what == kGun) < 0) {
                static bool told[2];
                if (!told[what == kGun]) {
                    told[what == kGun] = true;
                    rt_log("VR: %s: no melee attack seen yet to borrow (use a melee weapon once)",
                           what == kGun ? "a gun swung as a club" : "a punch");
                }
            }
        }
        // Owner 65 (session 14: "proper directional hit reactions based on when and where hand/weapon/melee weapon
        // makes impact"): while a swing is fast and has not hit, each frame the weapon's (or fist's) sweep since the
        // frame before goes to the game's battle step as that weapon's own attack (its id learned from the game's
        // queue; a fist the knife's, a gun as a club the pipe's). The game's own test (0x4B3260) finds whom it meets
        // that frame and its damage, knock-back and sound follow at once; the way it gives the hit's reaction is the
        // tip's motion that frame (0x4B37AD: cur tip - prev tip), so the reaction goes the way the weapon went where it
        // met. The first sweep that hits ends the swing's hits (combat_melee_landed). A slow touch sends nothing (80%
        // of the swing's speed, half its fast travel: 0.1.3). Kind 2 only (the way tells the sweep's hit); the stun gun's
        // touch (kind 3) stays the game's attack.
        int ls = -1;
        if (what == kGun || what == kUnknown) ls = borrow(what == kGun);
        else if (what != kStun && slot >= 0 && slot < 20 && learned[slot].id >= 0) ls = slot;
        if (ls >= 0 && learned[ls].kind != 2) ls = -1;
        const bool heavy_recovering = what == kHeavy && now < still_until && !sw.fired;
        // 0.1.3 (owner 84): a sweep from 80% of the swing's speed and half its fast travel (0.1.2: 60% and a third, a
        // gentle move's), so a hand that only moves meets no one and makes no hit's sound.
        if (ls >= 0 && sw.active && !sw.landed && sw.seg_valid && !heavy_recovering && s.speed >= t.speed * 0.8f &&
            sw.travel >= t.travel * 0.5f && now - sw.landed_at >= cooldown &&
            queue_extra(ls, sw.seg_grip, sw.seg_tip, g_now, tip_now, radius, h, slot, what, s.speed)) {
            sw.sweeps++;
            if (sh3_rig_attacker_at[3] > 0.0f) {
                // testing (session 16, LIST 27; SH3_STEER_AT_ATTACKER): how near each sweep came to the attacker's
                // place (across, the floor's plane; up, game Y is down), the first 40
                static int told = 0;
                if (told++ < 40) {
                    const float* a = sh3_rig_attacker_at;
                    auto across = [&](const Vec3& v) { return std::hypot(v.x - a[0], v.z - a[2]) / kUnitsPerMetre; };
                    rt_log("VR: frame %d: a sweep of the %s %s: grip %.2f m across from the attacker, %.2f m up; tip "
                           "%.2f m across, %.2f m up (its radius %.2f m)", rt_frame, h == kLeft ? "left" : "right",
                           what == kUnknown ? "fist" : weapon_name(slot), across(g_now), (a[1] - g_now.y) / kUnitsPerMetre,
                           across(tip_now), (a[1] - tip_now.y) / kUnitsPerMetre, radius / kUnitsPerMetre);
                }
            }
        }
        sw.seg_grip = g_now, sw.seg_tip = tip_now, sw.seg_valid = true;
    }
    const int mh = main_hand();
    // The live segment of the weapon in hand (for the volumes while the game's attack lasts).
    if (in_hand && info_of(game.weapon) && hands[mh].valid) {
        Vec3 g, tip;
        float r;
        segment(mh, game.weapon, g, tip, r);
        if (pending.armed && last_seg_valid) {
            // Before the game's first volume: from the swing's start to the weapon now (the whole path). After it:
            // from last frame's place to this frame's (each volume the attack queues sweeps the latest motion).
            if (pending.taken) {
                const Vec3 pg = last_grip_w[mh], pt = last_tip_w[mh];
                pending.prev_grip[0] = pg.x, pending.prev_grip[1] = pg.y, pending.prev_grip[2] = pg.z;
                pending.prev_tip[0] = pt.x, pending.prev_tip[1] = pt.y, pending.prev_tip[2] = pt.z;
            }
            pending.cur_grip[0] = g.x, pending.cur_grip[1] = g.y, pending.cur_grip[2] = g.z;
            pending.cur_tip[0] = tip.x, pending.cur_tip[1] = tip.y, pending.cur_tip[2] = tip.z;
        }
        last_grip_w[mh] = g;
        last_tip_w[mh] = tip;
        last_seg_valid = true;
    } else last_seg_valid = false;
    if (pending.armed && now > pending.until) pending.armed = false;
}

// A hand reaching out (its grip over 40 cm from the eyes, level) or down to the floor (under 55 cm over it, the
// player's; seated or with no floor known: 60 cm under the eyes). Session 11 (ask 28, the hand at the thing): over
// 25 cm took the hands as a player holds them idle (the sim's rest, 36 cm) and a hand hanging at her side was "down".
bool hand_reaching(const HandState& s) {
    Vec3 d = s.grip.position - fi.head.position;
    const float down = -d.y;
    d.y = 0;
    if (length(d) > 0.40f) return true;
    if (!opt.seated && fi.head_above_floor > 0.3f) return fi.head_above_floor - down < 0.55f;
    return down > 0.60f;
}

void update_grabbing() {
    if (!opt.grabbing || !in_play()) return;
    for (int h = 0; h < 2; h++) {
        const HandState& s = hands[h];
        const float g = h == kLeft ? fi.controls.left_grip : fi.controls.right_grip;
        const float pg = h == kLeft ? prev.left_grip : prev.right_grip;
        if (!s.valid || !(g > 0.6f && pg <= 0.6f)) continue;
        if (s.holding_weapon || s.on_foregrip || reload_owns_hand(h)) continue;
        bool over_holster = false;
        for (int k = 0; k < kHolsterCount; k++) over_holster |= holster_on(k) && holsters[k].hovered[h];
        if (over_holster) continue;
        // Reaching out or down: the game's own action (pick up, open, examine), at the hand ("Take items with your
        // hand": the game looks for what is there, at the palm, instead of in front of her), else in front of her.
        if (hand_reaching(s)) {
            grab_frames = 3;
            if (opt.take_by_hand && map_ready()) {
                hand_action.hand = h;
                hand_action.world = world_point(s.shown.position + rotate(s.shown.orientation, Vec3{0, -0.06f, 0}));
                hand_action.frames = 4;
                hand_action.found = false;
            } else if (opt.haptic_extra >= 1) buzz(h, 0.3f, 0.04f);  // "Extra vibration": pickups and doors
        }
    }
}

}  // namespace

// ================================================================ shared (sh3vr_internal.h)

void combat_frame() {
    now += frame_dt();
    play_time = in_play() ? play_time + frame_dt() : 0.0f;
    if (!calib_loaded) load_calibration();
    if (!learned_loaded) load_learned();
    if (!head_known()) return;
    update_body_yaw();
    track_weapon();
    for (int h = 0; h < 2; h++) body_shown[h] = hands[h].shown;
    reload_frame();
    update_holsters();
    build_frame(wf_one_hand, false);
    update_two_hand();
    {   // the other hand onto and off the second grip, eased (two_w); none without a weapon in the hand to see
        const float want = hands[off_hand()].on_foregrip ? 1.0f : 0.0f;
        two_w += (want - two_w) * ease_of(want > two_w ? 22.0f : 14.0f);
        if (std::fabs(want - two_w) < 0.01f) two_w = want;
        if (!in_hand || view_mode != ViewMode::kFirstPerson || !hands[off_hand()].valid) two_w = 0;
    }
    place_weapon();
    update_swings();
    if (hand_action.frames > 0) hand_action.frames--;
    update_grabbing();
    kick = std::max(0.0f, kick - 90.0f * frame_dt());
    parts_frame();
    sounds_frame();
    struggle_frame();  // owner message 38: shakes counted while she is held
    // Her fingers on the weapon and the parts, with the hands as just posed (sh3vr_body.cpp).
    body_fingers();
}

Pose hand_pose_round(int hand, Vec3 at, Vec3 rod, Vec3 palm) { return hand_round(hand, at, rod, palm); }

// The PC rig (xr_sim's "lh_at_second 1", session 13): the held weapon's second grip as last placed (tracking), so a
// check can put the other controller there (its lh_x/lh_y/lh_z then offsets from it) on every weapon alike.
bool sim_second_spot(float out[3]) {
    if (!wf_one_hand.placed || !wf_one_hand.hold || !wf_one_hand.hold->second) return false;
    const Vec3 s = second_spot(wf_one_hand);
    out[0] = s.x, out[1] = s.y, out[2] = s.z;
    return true;
}
// The PC rig (xr_sim's "lh_at_holster N" / "rh_at_holster N", 0.1.6): a holster's place (tracking), N 1 the gun's, 2
// the melee weapon's, 3 the belt pouch, 4 the chest pocket, 5 the ammo pouch; lh_x... then metres from it (the radius
// options checked at a known distance).
bool sim_holster_spot(int n, float out[3]) {
    if (n < 1 || n > kHolsterCount) return false;
    const Vec3 s = holster_place(n - 1);
    out[0] = s.x, out[1] = s.y, out[2] = s.z;
    return true;
}
Quat quat_from_axes(Vec3 x, Vec3 y, Vec3 z) { return quat_of_axes(x, y, z); }

bool weapon_point(Vec3 model, Vec3& tracking) {
    if (!wf.placed) return false;
    tracking = frame_point(wf, model);
    return true;
}
bool weapon_dir(Vec3 model, Vec3& tracking) {
    if (!wf.placed) return false;
    tracking = frame_dir(wf, model);
    return true;
}
bool weapon_model_point(Vec3 tracking, Vec3& model) {
    if (!wf.placed) return false;
    model = model_of(wf, tracking);
    return true;
}
bool weapon_forward(Vec3& base, Vec3& fwd) {
    if (!wf.placed) return false;
    base = wf.base;
    fwd = rotate(wf.turn, Vec3{0, 0, -1});
    return true;
}
bool weapon_signature(uint32_t nv, uint32_t np) {
    const WeaponInfo* w = info_of(game.weapon);
    if (!w || view_mode != ViewMode::kFirstPerson) return false;
    for (int i = 0; i < w->sigs; i++)
        if (w->nv[i] == nv && w->np[i] == np) return true;
    return false;
}
bool weapon_reach(Vec3& from, Vec3& to) {
    if (!reach_valid) return false;
    from = reach_from;
    to = reach_to;
    return true;
}
int weapon_capsules(Capsule* out, int max, bool with_flash) {
    int n = 0;
    if (wf.placed) {
        Cap caps[kCapMax];
        const int nc = shape_caps(wf, caps);
        const float s = calib[wf.profile].scale;
        for (int i = 0; i < nc && n < max; i++)
            out[n++] = {world_point(frame_point(wf, caps[i].a)), world_point(frame_point(wf, caps[i].b)), caps[i].r * s};
    } else if (reach_valid && n < max) {
        // A weapon still in her animated hand: a rod along its line, where it is drawn (held back with the hand).
        const Vec3 held = normalized(reach_to - reach_from) * -wall_back;
        out[n++] = {world_point(reach_from + held), world_point(reach_to + held), 0.03f * kUnitsPerMetre};
    }
    // Just after a shot: the muzzle flash and its smoke, 1.2 m along the barrel, are not the world (session 6, run
    // vr51: the guide dot jumped from the floor 3.8 m ahead onto the flash 0.6 m past the muzzle for two frames).
    Vec3 from, dir;
    if (with_flash && now < flash_until && n < max && gun_ray(from, dir))
        out[n++] = {world_point(from), world_point(from + dir * 1.2f), 0.25f * kUnitsPerMetre};
    return n;
}
float weapon_clearance(int hand, Vec3 a, Vec3 b) {
    if (!wf.placed) return 1e9f;
    const Vec3 ma = model_of(wf, a), mb = model_of(wf, b);
    Cap caps[kCapMax];
    const int nc = shape_caps(wf, caps);
    float best = 1e9f;
    for (int i = 0; i < nc; i++) best = std::min(best, seg_seg(ma, mb, caps[i].a, caps[i].b) - caps[i].r);
    if (hand != main_hand()) {
        // The main hand's fist round the grip (its fingers about 1.8 cm over the grip's surface).
        const Hold& hd = *wf.hold;
        best = std::min(best, seg_seg(ma, mb, wf.palm - hd.rod * 24.0f, wf.palm + hd.rod * 24.0f) - 20.0f);
    }
    return best * wf.scale;
}
int weapon_hand_role(int hand) { return hand >= 0 && hand < 2 ? hand_role[hand] : 0; }
// The other hand's share of the second grip (0 at its controller .. 1 on the grip; 0 for the main hand): letting go it
// eases back to the controller over about 0.15 s (session 17: the body's surface hold leaves it alone until then).
float weapon_two_share(int hand) { return hand == off_hand() ? two_w : 0.0f; }
int native_hand_group(int hand) { return hand >= 0 && hand < 2 && wf.placed ? native_shown[hand] : -1; }
float weapon_hold_curl(int hand) {
    if (!wf.placed) return 0.8f;
    return hand_role[hand] == 2 ? wf.hold->second_curl : wf.hold->curl;
}
bool weapon_is_gun() {
    const WeaponInfo* w = info_of(game.weapon);
    return w && w->kind == kGun;
}

bool hand_action_at(Vec3& world) {
    if (hand_action.frames <= 0 || hand_action.hand < 0 || hand_action.found || !opt.take_by_hand) return false;
    world = hand_action.world;
    return true;
}

// Session 11 (ask 28, "drawers, objects, handles, doors"): each frame of play, where a free hand reaching out is (its
// palm, world), so sh3.c asks the game's action check there without the button and the hand feels what it can use
// before it closes (hand_hover). The same conditions as update_grabbing's.
bool hand_reach_point(int h, Vec3& world) {
    // after 0.5 s of play (vr171: a single play frame between the opening's shots felt the game's own event)
    if (!opt.grabbing || !opt.take_by_hand || play_time < 0.5f || !map_ready() || h < 0 || h > 1) return false;
    const HandState& s = hands[h];
    if (!s.valid || s.holding_weapon || s.on_foregrip || reload_owns_hand(h) || light_hand == h) return false;
    for (int k = 0; k < kHolsterCount; k++)
        if (holster_on(k) && holsters[k].hovered[h]) return false;
    if (!hand_reaching(s)) return false;
    world = world_point(s.shown.position + rotate(s.shown.orientation, Vec3{0, -0.06f, 0}));
    return true;
}

// What hand_reach_point's check found (an event, a floor item as 0x400 + its place, or -1): a light tick when a hand
// comes onto something it can use ("Extra vibration": pickups and doors), once until it leaves it.
void hand_hover(int h, int event) {
    static int on[2] = {-1, -1};
    if (h < 0 || h > 1 || event == on[h]) return;
    if (event >= 0) {
        rt_log("VR: frame %d: the %s hand can use event %d (closing it uses it)", rt_frame, h == kLeft ? "left" : "right",
               event);
        if (opt.haptic_extra >= 1) buzz(h, 0.18f, 0.025f, 220.0f);
    }
    on[h] = event;
}

void hand_action_found(int event) {
    if (event < 0 || hand_action.found || hand_action.hand < 0) return;
    hand_action.found = true;
    rt_log("VR: the %s hand used event %d at %.0f %.0f %.0f", hand_action.hand == kLeft ? "left" : "right", event,
           hand_action.world.x, hand_action.world.y, hand_action.world.z);
    if (opt.haptic_extra >= 1) buzz(hand_action.hand, 0.45f, 0.06f, 150.0f);  // "Extra vibration": pickups and doors
    // a sound for every VR action (session 11): the hand closing on the item, the handle or the thing examined (the
    // game's own door, pick-up chime or message follows)
    hand_sound(kSoundTake, hand_action.world);
    hand_used_at = now;
    hand_used_world = hand_action.world;
}

// Session 11 (owners 42-43): the game's message after a hand used something stands by that thing (sh3vr.cpp's UI).
bool recent_hand_use(Vec3& world, float seconds) {
    if (hand_used_at < 0 || now - hand_used_at > seconds) return false;
    world = hand_used_world;
    return true;
}

void combat_pad(Sh3VrPad& p, uint16_t& add, bool& block_attack) {
    (void)p;
    enum { kA = 1, kB = 2, kRT = 7, kLS = 10 };
    // No weapon in the hand (holstered, or none): the trigger does not attack. LIST 46: nor a gun with its magazine out
    // or its slide (pump) still to work.
    block_attack = game.weapon < 0 || !in_hand || reload_blocks_fire();
    // The flashlight in a hand: that hand's trigger turns it on and off (the off hand's already does: map_pad), and
    // the main hand holding it never attacks.
    if (light_hand >= 0) {
        const bool t = (light_hand == kLeft ? fi.controls.left_trigger : fi.controls.right_trigger) > 0.6f;
        if (light_hand == main_hand()) {
            block_attack = true;
            if (t && !light_trigger_down) light_frames = 4;
        }
        light_trigger_down = t;
    }
    // Into the Radius's chest lamp (the owner: "similar to how into the radius handles its pocket lamp"): the main hand
    // at the light pocket with no weapon in it switches the lamp where it is with its trigger (its grip takes it off;
    // the off hand's trigger switches it anywhere, map_pad).
    {
        static bool was_pulled = false;
        const int mh = main_hand();
        const bool t = (mh == kLeft ? fi.controls.left_trigger : fi.controls.right_trigger) > 0.6f;
        if (t && !was_pulled && light_hand < 0 && block_attack && holster_on(kLightPocket) &&
            holsters[kLightPocket].hovered[mh] && view_mode == ViewMode::kFirstPerson) {
            light_frames = 4;
            buzz(mh, 0.3f, 0.03f);
            rt_log("VR: frame %d: the flashlight switched on her chest by the %s hand's trigger", rt_frame,
                   mh == kLeft ? "left" : "right");
        }
        was_pulled = t;
    }
    if (now < aim_pulse_until) add |= 1u << kRT;
    if (now >= attack_from && now < attack_until) add |= 1u << kA;
    if (grab_frames > 0) {
        add |= 1u << kA;
        grab_frames--;
    }
    if (light_frames > 0) {
        add |= 1u << kLS;
        light_frames--;
    }
    if (pouch_frames > 0) {
        add |= 1u << kB;
        pouch_frames--;
    }
}

bool weapon_shown_in_hand() { return in_hand && game.weapon >= 0 && view_mode == ViewMode::kFirstPerson; }
bool combat_trigger_readies() {
    return game.weapon == 9 && in_hand && opt.physical_melee && view_mode == ViewMode::kFirstPerson;
}
bool combat_gun_in_hand() {
    const WeaponInfo* w = info_of(game.weapon);
    return w && (w->kind == kGun || w->kind == kStun) && in_hand && view_mode == ViewMode::kFirstPerson;
}

bool gun_ray(Vec3& from, Vec3& dir) {
    const int mh = main_hand();
    const WeaponInfo* w = info_of(game.weapon);
    if (!w || (w->kind != kGun && w->kind != kStun) || !in_hand || !hands[mh].valid) return false;
    const Calib& c = calib_of(game.weapon);
    // Along the barrel from the muzzle, as the weapon is placed (two hands' turn in, the recoil's kick not); a weapon
    // whose hold is not measured: along the controller.
    Quat q = hands[mh].aim.orientation;
    Vec3 d = rotate(q, Vec3{0, 0, -1});
    if (wf.placed) {
        q = wf.aim_turn;
        d = rotate(q, Vec3{0, 0, -1});
    } else {
        Vec3 line;
        if (two_hand_line(game.weapon, line)) d = line;
    }
    if (c.aim_pitch != 0 || c.aim_yaw != 0) {
        const Vec3 right = rotate(q, Vec3{1, 0, 0}), up = rotate(q, Vec3{0, 1, 0});
        d = rotate(quat_axis(right, rad(c.aim_pitch)), d);
        d = rotate(quat_axis(up, rad(-c.aim_yaw)), d);
    }
    dir = normalized(d);
    if (wf.placed) {
        WeaponFrame a = wf;
        a.turn = wf.aim_turn;
        from = frame_point(a, muzzle_point(wf.hold));
    } else from = hands[mh].aim.position + dir * w->muzzle;
    return true;
}

bool combat_weapon_map(Vec3& point, Vec3& dir, bool from_hand) {
    // Drawn in the hand this frame or the one before (the game's update runs before its draw): anything older is
    // a weapon put away, a view change or a scene.
    Aff map;
    if (!in_hand || view_mode != ViewMode::kFirstPerson || !live_weapon_map(map)) return false;
    Vec3 p = mul(map.r, point) + map.t;
    Vec3 d = mul(map.r, dir);
    // Mode 0 reads her hand bone, mode 1 the weapon's matrix: both as the game animated them (the matrix is not moved,
    // combat_weapon_world), so both take the map.
    (void)from_hand;
    if (!std::isfinite(p.x + p.y + p.z + d.x + d.y + d.z)) return false;
    point = p;
    if (length(d) > 1e-6f) dir = normalized(d) * length(dir);  // the game's length kept (a unit direction stays one)
    return true;
}

namespace {
// D3D's row-vector 4x4 (row 3 the place) as the map's column-vector form (r[i][j] = m[j][i]), and back
Aff from_d3d(const float m[16]) {
    Aff a;
    for (int i = 0; i < 3; i++)
        for (int j = 0; j < 3; j++) a.r.m[i][j] = m[j * 4 + i];
    a.t = {m[12], m[13], m[14]};
    return a;
}
}  // namespace

bool combat_weapon_world(float m[16], const float base[16]) {
    // m is the weapon in her own space; base her world matrix (the game's world place of it: m then base, 0x4954E0)
    const Aff B = from_d3d(base), L = from_d3d(m);
    Aff back;
    if (!inverse(B, back)) return false;
    const Aff W = mul(B, L);
    w_game = W;
    w_game_frame = rt_frame;
    // Recorded only, never moved: the knife's blade is drawn from this matrix, and moved it was moved twice,
    // stretched a metre from the hand (run vr78). The flash follows her hand, not it (vr75).
    return false;
}

bool weapon_guide_dot() {
    const WeaponInfo* w = info_of(game.weapon);
    return w && (w->kind == kGun || w->kind == kStun) && calib_of(game.weapon).guide_dot;
}

bool melee_hold_still() { return now < still_until; }

// 0.1.6 (owner 111, the arms twisting while two-handing): the weapon in the hand has the other hand on its second grip
// (more than half taken), for her body to keep facing the head (sh3vr.cpp's facing).
bool weapon_two_handed() { return in_hand && two_w > 0.5f; }

const char* weapon_name(int slot) {
    const WeaponInfo* w = info_of(slot);
    if (base_slot(slot) == 17) return "unlimited submachine gun";
    switch (base_slot(slot)) {  // the extras (session 13, vr192 pictures), whether or not they share a hold
        case 9: return "beam saber";
        case 10: return "flamethrower";
        case 13: return "gold pipe";
        case 14: return "silver pipe";
        }
    return w ? w->name : slot > 0 ? "an extra weapon" : "no weapon";
}

bool last_draw_was_weapon() { return weapon_draw_now; }

namespace {

// The game's own grips (above): her hand bone in the weapon's held bone, from the game's pose of both this frame (the
// weapon's palette as the game sent it, before VR places it; her hand as the game posed it, sh3vr_ik.cpp), taken once
// it has held still 8 frames (a hand that is not on the weapon, or still moving to it, is never taken).
void capture_native(int slots, const int* pal_bone, const float vsc[96][4]) {
    const int p = profile_of(game.weapon);
    const Hold* hd = hold_of(game.weapon);
    if (p < 0 || !hd) return;
    int held = -1;
    for (int k = 0; k < slots && held < 0; k++)
        if (pal_bone && pal_bone[k] == hd->bone) held = k;
    if (held < 0 && hd->bone < slots && (!pal_bone || pal_bone[hd->bone] < 0)) held = hd->bone;
    if (held < 0 || !bone_affine(vsc, held)) return;
    Aff wi;
    if (!inverse(read_bone(vsc, held), wi)) return;
    static const char* const kSide[2] = {"left", "right"};
    for (int h = 0; h < 2; h++) {
        const int group = native_group_of(p, h);
        NativeGrip& n = native[p][h];
        if (group < 0 || n.have) continue;
        Vec3 axes[3], origin;
        int frame = -1;
        if (!ik_game_hand(h, axes, origin, frame) || rt_frame - frame > 1) {
            n.still = 0;
            continue;
        }
        const Vec3 o = mul(wi.r, origin) + wi.t;
        Vec3 ax[3];
        for (int i = 0; i < 3; i++) ax[i] = normalized(mul(wi.r, axes[i]));
        // Her wrist within 35 cm of where that hand holds the weapon (the hold's measured fist centre), else she is not
        // holding it with that hand now.
        const Vec3 near = h == kRight ? hd->palm : hd->second_at;
        if (length(o - near) > 0.35f * kUnitsPerMetre) {
            n.still = 0;
            continue;
        }
        bool still = n.still > 0 && length(o - n.last_o) < 0.5f;
        for (int i = 0; i < 3 && still; i++) still = dot(ax[i], n.last_ax[i]) > 0.99996f;
        n.still = still ? n.still + 1 : 1;
        n.last_o = o;
        for (int i = 0; i < 3; i++) n.last_ax[i] = ax[i];
        if (n.still < 8) continue;
        // The controller's grip point in her hand bone (her hand at a grip pose at the origin), into the weapon.
        Vec3 a0[3], o0;
        if (!ik_hand_bone_for_pose(h, Pose{}, a0, o0)) continue;
        const Vec3 l = Vec3{dot(o0 * -1.0f, a0[0]), dot(o0 * -1.0f, a0[1]), dot(o0 * -1.0f, a0[2])} * kUnitsPerMetre;
        const Vec3 palm = o + ax[0] * l.x + ax[1] * l.y + ax[2] * l.z;
        // Session 13 (vr190, vr193: the shotgun's left hand on the receiver's front, 15.8 cm behind the pump; the
        // submachine gun's open hand past its muzzle, 12.5 cm off; both drawn there as the second grip): the game's
        // own hand is taken only where it holds the grip: its fist's centre within 8 cm of the measured one on a gun
        // (the good ones were 3-4 cm off; one submachine gun pose 5.6), 12 on a melee weapon (the steel pipe's
        // right hand 9 cm along its tube). Else her finger hand goes round the measured grip, and a later still pose
        // is tried again.
        // vr194: the submachine gun's left hand taken at 5.7 cm was the game's open palm flat on the receiver's side,
        // its fingers in the air (not a grip): a gun's second hand only within 4 cm (none of the game's are), so her
        // finger hand cups the measured grip (the shotgun's pump from below, vr194 c07450).
        // Session 14: never on a gun. The submachine gun's grip raised to its receiver's floor (vr197) lies 4.1 cm from
        // that open palm (112.5 31.9 -21.4), a hair over the 4; the shotgun's (15.8 cm) and the flamethrower's (10.6) are
        // no grips either: every gun's second hand is her finger hand round the measured grip.
        const WeaponInfo* wi_ = info_of(game.weapon);
        const bool gun_ = wi_ && wi_->kind == kGun;
        const float limit = (gun_ ? (h == kRight ? 0.08f : -1.0f) : 0.12f) * kUnitsPerMetre;
        if (length(palm - near) > limit) {
            static int refused[kProfiles][2];
            if (refused[p][h]++ % 600 == 0)
                rt_log("VR native grip: %s, %s hand (her group %d): the game's own hand %.1f cm from the grip (its fist at "
                       "%.1f %.1f %.1f, the grip's %.1f %.1f %.1f, over %.0f): not taken, her finger hand on the grip",
                       weapon_name(game.weapon), kSide[h], group, length(palm - near) / kUnitsPerMetre * 100.0f, palm.x,
                       palm.y, palm.z, near.x, near.y, near.z, limit / kUnitsPerMetre * 100.0f);
            n.still = 0;
            continue;
        }
        n.o = o;
        for (int i = 0; i < 3; i++) n.ax[i] = ax[i];
        n.palm = palm;
        n.have = true;
        rt_log("VR native grip: %s, %s hand (her group %d, game frame %d against weapon frame %d): her hand bone at "
               "%.1f %.1f %.1f in the weapon's bone %d, axes %.4f %.4f %.4f | %.4f %.4f %.4f | %.4f %.4f %.4f; the "
               "fist's centre at %.1f %.1f %.1f (the measured hold's %.1f %.1f %.1f, %.1f cm off)",
               weapon_name(game.weapon), kSide[h], group, frame, rt_frame, o.x, o.y, o.z, hd->bone, ax[0].x, ax[0].y,
               ax[0].z, ax[1].x, ax[1].y, ax[1].z, ax[2].x, ax[2].y, ax[2].z, n.palm.x, n.palm.y, n.palm.z, near.x,
               near.y, near.z, length(n.palm - near) / kUnitsPerMetre * 100.0f);
    }
}

// testing (session 5): SH3_WEAPON_BONES=1 logs, for each weapon drawn in the hand, every skinned stride-48 draw of few
// bones in the first 240 frames after it changed (the melee weapons' and the extras' signatures, with their bones), and
// once, 60 frames after the change (the idle pose), the weapon's palette: each slot's model bone, its 3x4 and its place
// against the held bone (inverse(B0) Bk: how the game poses the second part against the body).
void diag_draw(uint32_t nv, uint32_t np, uint32_t stride, int pal_count, const int* pal_bone, const float vsc[96][4]) {
    static int on = -1;
    if (on < 0) on = std::getenv("SH3_WEAPON_BONES") ? 1 : 0;
    if (!on || stride != 48 || pal_count <= 0 || pal_count > 4 || view_mode != ViewMode::kFirstPerson) return;
    static int last_w = -99, change_frame = -1000, nsigs = 0;
    static uint32_t sigs[32][2];
    static bool full_logged = false;
    if (game.weapon != last_w) {
        last_w = game.weapon;
        change_frame = rt_frame;
        nsigs = 0;
        full_logged = false;
    }
    if (rt_frame - change_frame > 240) return;
    bool seen = false;
    for (int i = 0; i < nsigs; i++) seen |= sigs[i][0] == nv && sigs[i][1] == np;
    if (!seen && nsigs < 32) {
        sigs[nsigs][0] = nv, sigs[nsigs][1] = np;
        nsigs++;
        rt_log("weapon bones: weapon %d (%s), frame %d: a skinned draw %u/%u, %d bones (model bones %d %d %d %d)",
               game.weapon, weapon_name(game.weapon), rt_frame, nv, np, pal_count, pal_bone[0],
               pal_count > 1 ? pal_bone[1] : -9, pal_count > 2 ? pal_bone[2] : -9, pal_count > 3 ? pal_bone[3] : -9);
    }
    if (full_logged || rt_frame - change_frame < 60 || !weapon_signature(nv, np)) return;
    full_logged = true;
    rt_log("weapon bones: weapon %d (%s) %u/%u idle palette, %d slots:", game.weapon, weapon_name(game.weapon), nv, np,
           pal_count);
    Aff b0;
    const bool have0 = bone_affine(vsc, 0);
    if (have0) b0 = read_bone(vsc, 0);
    Aff inv0;
    const bool inv_ok = have0 && inverse(b0, inv0);
    for (int k = 0; k < pal_count; k++) {
        const Aff b = read_bone(vsc, k);
        rt_log("  slot %d bone %d: [%.4f %.4f %.4f | %.1f] [%.4f %.4f %.4f | %.1f] [%.4f %.4f %.4f | %.1f]", k, pal_bone[k],
               b.r.m[0][0], b.r.m[0][1], b.r.m[0][2], b.t.x, b.r.m[1][0], b.r.m[1][1], b.r.m[1][2], b.t.y, b.r.m[2][0],
               b.r.m[2][1], b.r.m[2][2], b.t.z);
        if (inv_ok && k > 0) {
            const Aff rel = mul(inv0, b);
            rt_log("    against slot 0: [%.4f %.4f %.4f | %.2f] [%.4f %.4f %.4f | %.2f] [%.4f %.4f %.4f | %.2f]",
                   rel.r.m[0][0], rel.r.m[0][1], rel.r.m[0][2], rel.t.x, rel.r.m[1][0], rel.r.m[1][1], rel.r.m[1][2],
                   rel.t.y, rel.r.m[2][0], rel.r.m[2][1], rel.r.m[2][2], rel.t.z);
        }
    }
}

// The pass being drawn: a moving part's offset (model units) after every bone, or VR's own mesh placed in the world.
bool apply_pass(float vsc[96][4], int slots) {
    PassPlace pp;
    const int pass = parts_pass();
    if (pass <= 0 || !parts_pass_place(pass, pp) || pp.kind == 0) return false;
    Aff m;
    for (int r = 0; r < 3; r++) {
        m.r.m[r][0] = pp.m[r * 4], m.r.m[r][1] = pp.m[r * 4 + 1], m.r.m[r][2] = pp.m[r * 4 + 2];
        (&m.t.x)[r] = pp.m[r * 4 + 3];
    }
    for (int k = 0; k < slots; k++) {
        if (pp.kind == 2) {
            write_bone(vsc, k, m);
            continue;
        }
        // Any bone with a real matrix (a weapon scaled by its adjustment has rows longer or shorter than 1; a collapsed
        // one's are zero and stay so).
        const Aff b = read_bone(vsc, k);
        bool real = true;
        for (int r = 0; r < 3 && real; r++) {
            const float l = b.r.m[r][0] * b.r.m[r][0] + b.r.m[r][1] * b.r.m[r][1] + b.r.m[r][2] * b.r.m[r][2];
            real = std::isfinite(l) && l > 1e-4f && l < 100.0f && std::isfinite((&b.t.x)[r]);
        }
        if (real) write_bone(vsc, k, mul(b, m));
    }
    return true;
}

}  // namespace

// The weapon in the hand (LIST 24): its held bone's palette slot (the model bone d3d8.c read each slot from, else the
// slot of that number) put where the hold places it, the weapon's other bones kept as the game poses them against it
// (B' = H inverse(B_held) B: faces stitched between its parts stay as drawn); hidden while holstered; a weapon whose hold
// is not measured stays in her animated hand. Then the pass being drawn (sh3vr_parts.cpp).
// The held bone's matrix as the hand in VR holds the weapon this frame (H: the hold's palm at the hand, turned with it).
bool hold_matrix(Aff& H) {
    if (!wf.placed) return false;
    const float s = calib[wf.profile].scale;
    for (int i = 0; i < 3; i++) {
        Vec3 e{};
        (&e.x)[i] = 1;
        const Vec3 col = world_dir(rotate(wf.turn, frame_local(wf, e))) * s;
        H.r.m[0][i] = col.x, H.r.m[1][i] = col.y, H.r.m[2][i] = col.z;
    }
    H.t = world_point(wf.base) - mul(H.r, wf.palm);
    return true;
}

// Owner 61 (session 15, run vr216): the beam saber's blade is a draw of its own, not part of the hilt's 126/154 mesh:
// 24 vertices, 30 indices, one palette slot (model bone 1) whose x row the game keeps at 0 while the beam is off and
// stretches as it extends. It is drawn just before the hilt and has no signature, so nothing moved it: the lit blade
// stayed at her animated hand (by her hip), the green bar across the view. It takes the hilt's map, the game's weapon
// -> the one in the hand this frame (the hold's H x inverse(the held bone in the weapon) x inverse(the game's
// weapon)), its stretch kept; holstered, collapsed with the hilt. Only a draw whose bone sits at the game's hilt.
bool saber_blade_draw(uint32_t nv, uint32_t np, int pal_count, const int* pal_bone, float vsc[96][4]) {
    if (game.weapon != 9 || nv != 24 || np != 30 || pal_count != 1 || (pal_bone && pal_bone[0] != 1)) return false;
    if (!bone_finite(vsc, 0) || rt_frame - w_game_frame > 1) return false;
    const Aff b = read_bone(vsc, 0);
    if (length(b.t - w_game.t) > 400.0f) return false;  // not at the game's own hilt (vr216: 76 units from it)
    if (!in_hand) {
        Aff z{};
        z.t = b.t;
        write_bone(vsc, 0, z);
        return true;
    }
    Aff H, ih, iw;
    if (!held_in_weapon_known || !hold_matrix(H) || !inverse(held_in_weapon, ih) || !inverse(w_game, iw)) return false;
    const Aff moved = mul(mul(mul(H, ih), iw), b);
    static int told = 0;
    if (told < 3 && b.r.m[0][0] * b.r.m[0][0] + b.r.m[1][0] * b.r.m[1][0] + b.r.m[2][0] * b.r.m[2][0] > 0.01f) {
        told++;
        rt_log("VR: frame %d: the beam saber's blade (its own draw) moved from her animated hand (%.0f %.0f %.0f) to the "
               "hilt in the hand (%.0f %.0f %.0f)", rt_frame, b.t.x, b.t.y, b.t.z, moved.t.x, moved.t.y, moved.t.z);
    }
    write_bone(vsc, 0, moved);
    return true;
}

int combat_weapon_draw(uint32_t nv, uint32_t np, uint32_t stride, int pal_count, const int* pal_bone, float vsc[96][4]) {
    weapon_draw_now = false;
    diag_draw(nv, np, stride, pal_count, pal_bone, vsc);
    {
        // testing (session 15, owner 61): SH3_SABER_BONES=F logs, at frames F and F+30, every skinned draw while the
        // beam saber is out (its blade, a 24/30 part with its own texture, is not the hilt's 126/154 draw): its
        // palette's bones (model bone, row lengths, place)
        static int at = -2;
        if (at == -2) at = std::getenv("SH3_SABER_BONES") ? std::atoi(std::getenv("SH3_SABER_BONES")) : -1;
        if (at >= 0 && game.weapon == 9 && (rt_frame == at || rt_frame == at + 30) && nv && nv < 400) {
            const int n = pal_count > 0 ? std::min(pal_count, kBones) : 0;
            rt_log("saber draw: frame %d: %u/%u, stride %u, %d palette slots", rt_frame, nv, np, stride, n);
            for (int k = 0; k < n; k++) {
                const float* r0 = vsc[kPaletteStart + k * 3];
                const float* r1 = vsc[kPaletteStart + k * 3 + 1];
                const float* r2 = vsc[kPaletteStart + k * 3 + 2];
                rt_log("  slot %d (model bone %d): x %.3f %.3f %.3f, y %.3f %.3f %.3f, z %.3f %.3f %.3f, at %.0f %.0f %.0f", k,
                       pal_bone ? pal_bone[k] : -1, r0[0], r1[0], r2[0], r0[1], r1[1], r2[1], r0[2], r1[2], r2[2], r0[3],
                       r1[3], r2[3]);
            }
        }
    }
    const WeaponInfo* w = info_of(game.weapon);
    if (w && view_mode == ViewMode::kFirstPerson && map_ready() && saber_blade_draw(nv, np, pal_count, pal_bone, vsc)) {
        weapon_draw_now = true;  // faded as the weapon is (near the eyes only), not as her body
        // 2: a glow, so d3d8.c keeps it out of her own-mesh shadow (a beam of light casts none), its pixels still
        // marked as the weapon's (not darkened by the game's shadows)
        return 2;
    }
    if (!w || view_mode != ViewMode::kFirstPerson || !map_ready() || !weapon_signature(nv, np)) return 0;
    weapon_draw_now = true;
    bool known = false;
    for (int i = 0; i < n_strides; i++) known |= skinned_strides[i] == stride;
    if (!known && n_strides < 4) {
        skinned_strides[n_strides++] = stride;
        rt_log("VR: the weapons' skinned vertices are %u bytes (Heather's own draws are told by it too)", stride);
    }
    const int slots = pal_count > 0 ? std::min(pal_count, kBones) : kBones;
    capture_native(slots, pal_bone, vsc);
    if (!in_hand) {
        // Holstered: the weapon's bones collapsed to a point (nothing drawn), its parts with it.
        for (int b = 0; b < slots; b++)
            if (bone_affine(vsc, b)) {
                Aff z{};
                z.t = read_bone(vsc, b).t;
                write_bone(vsc, b, z);
            }
        if (parts_pass() > 0) {
            PassPlace pp;
            if (parts_pass_place(parts_pass(), pp) && pp.kind == 2) apply_pass(vsc, slots);  // a part falling still shows
        }
        return 1;
    }
    bool changed = false;
    if (wf.placed) {
        int held = -1;
        for (int k = 0; k < slots && held < 0; k++)
            if (pal_bone && pal_bone[k] == wf.hold->bone) held = k;
        if (held < 0 && wf.hold->bone < slots && (!pal_bone || pal_bone[wf.hold->bone] < 0)) held = wf.hold->bone;
        static int not_found = 0;
        if (held < 0 || !bone_affine(vsc, held)) {
            if (not_found++ < 4)
                rt_log("VR: %s's held bone %d is not in its palette (%d slots): left in her hand", w->name, wf.hold->bone,
                       slots);
        } else {
            Aff H;
            hold_matrix(H);
            Aff inv_held;
            if (inverse(read_bone(vsc, held), inv_held)) {
                const Aff rel = mul(H, inv_held);
                game_gun = read_bone(vsc, held).t;
                game_gun_frame = rt_frame;
                game_to_hand = rel;  // the game's weapon -> the one in the hand (its shots and effects: 0x4954E0)
                game_to_hand_frame = rt_frame;
                // The palette drawn is always the game's own (her animated hand's, from the update before this frame:
                // run vr70), so the held bone against that update's weapon matrix is where it sits in the weapon.
                {
                    const Aff hb = read_bone(vsc, held);
                    Aff iw, ik;
                    if (rt_frame - w_game_frame <= 1 && inverse(w_game, iw)) {
                        const Aff k = mul(iw, hb);
                        static int told = 0;
                        if (told++ < 3 || (now < flash_until && told < 12))
                            rt_log("VR: frame %d: %s's held bone in its weapon matrix at %.1f %.1f %.1f (was %.1f %.1f "
                                   "%.1f)", rt_frame, w->name, k.t.x, k.t.y, k.t.z, held_in_weapon.t.x, held_in_weapon.t.y,
                                   held_in_weapon.t.z);
                        held_in_weapon = k;
                        held_in_weapon_known = true;
                    }
                    if (held_in_weapon_known && inverse(held_in_weapon, ik)) {
                        vr_weapon = mul(H, ik);
                        vr_weapon_frame = rt_frame;
                    }
                }
                // The rest of the palette with it, scaled bones too (bone_finite: a weapon part the game stretches or
                // collapses keeps that). The beam saber's blade is not here: a draw of its own (saber_blade_draw).
                {
                    static int at = -2;
                    if (at == -2) at = std::getenv("SH3_SABER_BONES") ? std::atoi(std::getenv("SH3_SABER_BONES")) : -1;
                    if (at >= 0 && (rt_frame == at || rt_frame == at + 30)) {
                        rt_log("saber bones: frame %d: %s %u/%u, %d slots, held %d:", rt_frame, w->name, nv, np, slots, held);
                        for (int k = 0; k < slots; k++) {
                            const float* r0 = vsc[kPaletteStart + k * 3];
                            const float* r1 = vsc[kPaletteStart + k * 3 + 1];
                            const float* r2 = vsc[kPaletteStart + k * 3 + 2];
                            rt_log("  slot %d (model bone %d): rows %.3f %.3f %.3f long, at %.0f %.0f %.0f, unit rows %d", k,
                                   pal_bone ? pal_bone[k] : -1, std::sqrt(r0[0] * r0[0] + r0[1] * r0[1] + r0[2] * r0[2]),
                                   std::sqrt(r1[0] * r1[0] + r1[1] * r1[1] + r1[2] * r1[2]),
                                   std::sqrt(r2[0] * r2[0] + r2[1] * r2[1] + r2[2] * r2[2]), r0[3], r1[3], r2[3],
                                   bone_affine(vsc, k));
                        }
                    }
                }
                for (int k = 0; k < slots; k++)
                    if (k == held ? bone_affine(vsc, k) : bone_finite(vsc, k))
                        write_bone(vsc, k, k == held ? H : mul(rel, read_bone(vsc, k)));
                changed = true;
            }
        }
    }
    changed |= apply_pass(vsc, slots);
    return changed ? 1 : 0;
}

// LIST 24 (session 13): the extra weapons (unlimited submachine gun, flamethrower, beam saber...: slots 9, 10, 13, 14,
// 17) have no mesh signature, so combat_weapon_draw leaves them alone and they hung in her ANIMATED hand, away from
// the controller. Their model's bones, as the game posed them (sh3.c's 0x45EC00 wrapper, a small model while a
// weapon is out), are carried with her hand to her IK hand at the controller (ik_hand_carry: from the game's hand bone
// to VR's), the way the game holds them kept; holstered, collapsed to a point as the known weapons are. Only for a
// model whose bone 0 sits on her animated right hand (where the game hangs every weapon).
int combat_weapon_bones(float* bones, int n) {
    if (n < 1 || n > 4 || view_mode != ViewMode::kFirstPerson || !map_ready() || game.weapon <= 0 || info_of(game.weapon))
        return 0;
    float m0[16];
    std::memcpy(m0, bones, sizeof m0);
    if (!ik_hand_carry(kRight, m0, true)) return 0;  // not on her animated right hand: not the weapon
    static int told = -1;
    if (told != game.weapon) {
        told = game.weapon;
        rt_log("VR: %s (weapon %d, no hold of its own): %s, carried from her animated hand to her hand as posed", weapon_name(game.weapon),
               game.weapon, in_hand ? "in the hand" : "holstered (not drawn)");
    }
    if (!in_hand) {
        for (int b = 0; b < n; b++) {
            float* m = bones + 16 * b;
            for (int i = 0; i < 12; i++) m[i] = 0.0f;
        }
        return 1;
    }
    // Her right hand, the one the game holds every weapon in (with "Left-handed" her IK right hand is where VR poses it).
    for (int b = 0; b < n; b++) ik_hand_carry(kRight, bones + 16 * b, false);
    return 1;
}


// The flashlight held in a hand (session 7): its model is Heather's part 48 (group 32), a flat box 8 x 5 x 4 cm skinned
// to bone 4 alone (mesh dump vr104: local x -7.2..32.3 her up, y -55.6..-30.4, z 66.4..85.3 her forward, the lens on
// the +z face). In the hand the box's up runs along the controller's up, its lens along the controller's pointing, its
// middle where sh3vr_body.cpp found her fingers hold it from the side (body_light_place: the palm on one side, the
// fingers over its lower front, the thumb on its top; the beam from the lens' clear upper part), else 3 cm ahead of
// the fist.
namespace {
constexpr float kLightCentre[3] = {12.55f, -43.0f, 75.85f};  // the box's middle, bone 4's space (units)
constexpr float kLightLens = 85.3f;                           // the lens face's z
struct LightFrame {
    Vec3 x, y, z, centre, lens;
};
bool light_in_hand(LightFrame& f) {
    if (light_hand < 0 || view_mode != ViewMode::kFirstPerson || !map_ready()) return false;
    const HandState& s = hands[light_hand];
    if (!s.valid) return false;
    // The pointing (aim) turned as the drawn hand is turned from the tracked grip (a hand held back at a wall).
    const Quat o = s.shown.orientation * conjugate(s.grip.orientation) * s.aim.orientation;
    f.z = normalized(world_dir(rotate(o, Vec3{0, 0, -1})));
    Vec3 up = world_dir(rotate(o, Vec3{0, 1, 0}));
    up = up - f.z * dot(up, f.z);
    if (length(up) < 1e-3f) return false;
    f.x = normalized(up);
    f.y = cross(f.z, f.x);  // right-handed as her bone 4 is (z = x cross y)
    // The beam leaves 2 units ahead of the lens' part her fingers leave clear (session 8: they hold its lower front,
    // as a phone is held to take a picture; before, its middle, under her fingers).
    Vec3 c, lens;
    if (body_light_place(light_hand, c, lens)) {
        f.centre = world_point(s.shown.position + rotate(s.shown.orientation, c));
        f.lens = world_point(s.shown.position + rotate(s.shown.orientation, lens)) + f.z * 2.0f;
    } else {
        f.centre = world_point(s.shown.position) + f.z * (0.03f * kUnitsPerMetre);
        f.lens = f.centre + f.z * (kLightLens - kLightCentre[2] + 2.0f);
    }
    return true;
}
}  // namespace

int combat_flashlight_bone(float m[16]) {
    LightFrame f;
    if (!light_in_hand(f)) return 0;
    // D3D row-vector 4x4 as the game keeps bones: rows 0-2 the bone's axes in the world, row 3 its origin, placed so
    // the box's middle (kLightCentre in the bone's space) lands at f.centre.
    const Vec3 t = f.centre - f.x * kLightCentre[0] - f.y * kLightCentre[1] - f.z * kLightCentre[2];
    const Vec3 rows[4] = {f.x, f.y, f.z, t};
    for (int r = 0; r < 4; r++) {
        m[r * 4 + 0] = rows[r].x, m[r * 4 + 1] = rows[r].y, m[r * 4 + 2] = rows[r].z;
        m[r * 4 + 3] = r == 3 ? 1.0f : 0.0f;
    }
    return 1;
}

// The light's place (session 7; docs/GAME_FACTS.md, The flashlight): sh3.c wraps the game's copy of Heather's light
// into the light buffer (0x457AC0) and hands over the beam's position and direction (buffer +0x40, +0x50, from
// 0x712C1D0 / 0x712C1E0: her chest) and its shadow caster's (+0x60, +0x70, about 0.5 m ahead along the beam). The lit
// shaders take the position as c16 and the beam's turn as c80-c82 (0x461840), the shadows the caster's, so moving these
// moves the light, its beam and the shadows together. In first person: on her chest the beam turns to where the eyes
// look (aimed at the point 4 m along the gaze, so it lights what is looked at); in a hand it leaves the flashlight's
// lens there along the controller's pointing. The caster keeps its place relative to the beam.
namespace {
// The beam as last placed and the light's level (session 8, for her own draws' light: body_light in sh3vr_ik.cpp).
struct Beam {
    Vec3 pos, dir;
    int frame = -1;
    float level = 0;
} beam;
}  // namespace

void combat_flashlight_level(float level) { beam.level = std::isfinite(level) ? std::clamp(level, 0.0f, 1.0f) : 0.0f; }

bool combat_beam(Vec3& pos, Vec3& dir, float& level) {
    if (beam.frame < rt_frame - 2 || beam.level <= 0.001f) return false;
    pos = beam.pos, dir = beam.dir, level = beam.level;
    return true;
}

int combat_flashlight_place(float* pos, float* dir, float* pos2, float* dir2) {
    // Where the beam comes from, told in the log as it changes (with the game's place and VR's).
    static int last_mode = -1;
    auto told = [&](int mode, Vec3 p, Vec3 d, Vec3 np, Vec3 nd) {
        if (mode == last_mode) return;
        last_mode = mode;
        static const char* const kModes[] = {"the game's", "her chest, aimed where the eyes look", "the left hand",
                                             "the right hand"};
        rt_log("VR: frame %d: the flashlight's beam from %s (the game's at %.0f %.0f %.0f along %.2f %.2f %.2f; VR's at "
               "%.0f %.0f %.0f along %.2f %.2f %.2f)", rt_frame, kModes[mode], p.x, p.y, p.z, d.x, d.y, d.z, np.x, np.y,
               np.z, nd.x, nd.y, nd.z);
    };
    const Vec3 p0{pos[0], pos[1], pos[2]}, d_raw{dir[0], dir[1], dir[2]};
    if (view_mode != ViewMode::kFirstPerson || !map_ready() || !game.player_valid || !(length(d_raw) > 0.5f) ||
        !std::isfinite(p0.x + p0.y + p0.z)) {
        told(0, p0, d_raw, p0, d_raw);
        return 0;
    }
    const Vec3 d0 = normalized(d_raw);
    Vec3 np, nd;
    int mode;
    LightFrame f;
    if (light_in_hand(f)) {
        np = f.lens, nd = f.z, mode = light_hand == kLeft ? 2 : 3;
    } else if (opt.flashlight == 0) {
        const Vec3 eye = world_point(fi.head.position);
        const Vec3 gaze = normalized(world_dir(rotate(fi.head.orientation, Vec3{0, 0, -1})));
        // On her chest as VR poses it (her lean and crouch carry it; session 12, vr188: at the game's place her own
        // trunk stood ahead of the lamp in the weapon stances and shaded the beam's middle).
        np = p0;
        Vec3 carried;
        if (ik_chest_carry(p0, carried)) {
            static int carry_told = 0;
            const float moved = length(carried - p0) / kUnitsPerMetre;
            if (carry_told < 4 && moved > (carry_told ? 0.15f : 0.0f)) {
                carry_told++;
                rt_log("VR: frame %d: the chest lamp rides her chest as drawn: %.2f m from the game's place", rt_frame,
                       moved);
            }
            np = carried;
        }
        nd = normalized(eye + gaze * (4.0f * kUnitsPerMetre) - np);
        mode = 1;
    } else {
        told(0, p0, d0, p0, d0);
        beam.pos = p0, beam.dir = d0, beam.frame = rt_frame;
        return 0;  // "Where Heather faces": the game's own
    }
    if (!(length(nd) > 0.5f)) return 0;
    told(mode, p0, d0, np, nd);
    const R3 D = arc(d0, nd);
    Vec3 o2 = mul(D, Vec3{pos2[0] - p0.x, pos2[1] - p0.y, pos2[2] - p0.z});
    const Vec3 n2 = mul(D, Vec3{dir2[0], dir2[1], dir2[2]});
    // The shadow caster's place against the beam, told once (session 8: her hand reaching into the beam drew dark,
    // vr116). SH3_CASTER_AHEAD (metres, the PC rig's test switch) puts it that far along the beam instead.
    static bool caster_told = false;
    if (!caster_told) {
        caster_told = true;
        const float along = dot(o2, nd);
        rt_log("VR: the flashlight's shadow caster: %.2f m along the beam, %.2f m off it, turned %.2f %.2f %.2f "
               "(the beam %.2f %.2f %.2f)", along / kUnitsPerMetre, length(o2 - nd * along) / kUnitsPerMetre, n2.x, n2.y,
               n2.z, nd.x, nd.y, nd.z);
    }
    static const char* const ahead_env = std::getenv("SH3_CASTER_AHEAD");
    if (ahead_env) o2 = nd * (static_cast<float>(std::atof(ahead_env)) * kUnitsPerMetre);
    pos[0] = np.x, pos[1] = np.y, pos[2] = np.z;
    dir[0] = nd.x, dir[1] = nd.y, dir[2] = nd.z;
    pos2[0] = np.x + o2.x, pos2[1] = np.y + o2.y, pos2[2] = np.z + o2.z;
    dir2[0] = n2.x, dir2[1] = n2.y, dir2[2] = n2.z;
    beam.pos = np, beam.dir = nd, beam.frame = rt_frame;
    return 1;
}
int combat_light_hand() { return light_hand; }
bool combat_light_held() {
    LightFrame f;
    return light_in_hand(f);
}

int combat_melee_volume(int slot, Sh3VrHit* hit) {
    if (!learned_loaded) load_learned();
    if (slot >= 0 && slot < 20 && (learned[slot].id != hit->battle_id || learned[slot].kind != hit->kind)) {
        learned[slot] = {hit->kind, hit->battle_id};
        save_learned();
        rt_log("VR: %s's melee attack id %d (kind %d) learned", weapon_name(slot), hit->battle_id, hit->kind);
    }
    if (!pending.armed || pending.slot != slot || now > pending.until || view_mode != ViewMode::kFirstPerson) return 0;
    // Owner 65: the swing that pressed this attack has already hit at contact: the game's own volumes would hit again.
    if (pending.landed) return 2;
    const float* src[4] = {pending.prev_grip, pending.prev_tip, pending.cur_grip, pending.cur_tip};
    float* dst[4] = {hit->prev_grip, hit->prev_tip, hit->cur_grip, hit->cur_tip};
    for (int i = 0; i < 4; i++) dst[i][0] = src[i][0], dst[i][1] = src[i][1], dst[i][2] = src[i][2], dst[i][3] = 1.0f;
    hit->radius = pending.radius;
    extend_hit(*hit);
    // From now on the volumes follow the weapon frame by frame (update_swings), until the attack is over.
    pending.taken = true;
    return 1;
}

int combat_extra_hit(Sh3VrHit* out) {
    if (n_extra <= 0) return 0;
    *out = extra[0];
    for (int i = 1; i < n_extra; i++) extra[i - 1] = extra[i];
    n_extra--;
    return 1;
}

// Owner 65: one of Heather's hits as the game recorded it (sh3.c after the battle step). A sweep's own way tells it
// was that sweep's: that swing has hit (no more sweeps; a melee weapon's game attack pressed now if the swing had not
// yet, for her motion and its sound, its own volumes dropped), a knock in that hand, a fist's thud. Else it was the
// game's own attack's volume (the swing's path): that swing's hits are done too.
void combat_melee_landed(const float way[3], const float point[3], uint32_t character, int battle_id) {
    if (!std::isfinite(way[0]) || !std::isfinite(way[1]) || !std::isfinite(way[2])) return;
    int found = -1;
    for (int i = 0; i < 16 && found < 0; i++) {
        const Sweep& s = sweeps_sent[i];
        if (s.hand < 0 || now - s.at > 0.5) continue;
        const float tol = 1e-3f * (std::fabs(s.way[0]) + std::fabs(s.way[1]) + std::fabs(s.way[2])) + 0.05f;
        if (std::fabs(s.way[0] - way[0]) <= tol && std::fabs(s.way[1] - way[1]) <= tol &&
            std::fabs(s.way[2] - way[2]) <= tol)
            found = i;
    }
    const Vec3 at{point[0], point[1], point[2]};
    const float wl = std::sqrt(way[0] * way[0] + way[1] * way[1] + way[2] * way[2]);
    const float wn = wl > 1e-4f ? 1.0f / wl : 0.0f;
    if (found < 0) {
        if (pending.armed && !pending.landed) {
            pending.landed = true;
            const int mh = main_hand();
            swings[mh].landed = true, swings[mh].landed_at = now;
            buzz(mh, 0.8f, 0.08f);
            rt_log("VR: frame %d: %s's own attack hit %08X (attack %d) at %.0f %.0f %.0f, its way %.2f %.2f %.2f: the "
                   "swing's hits done", rt_frame, weapon_name(pending.slot), character, battle_id, at.x, at.y, at.z,
                   way[0] * wn, way[1] * wn, way[2] * wn);
        }
        return;
    }
    const Sweep& s = sweeps_sent[found];
    Swing& sw = swings[s.hand];
    const bool again = sw.landed && sw.landed_at == now;  // a second character in the same sweep
    rt_log("VR: frame %d: the %s %s hit %08X at contact (attack %d, sweep %d of the swing, the hand at %.1f m/s): at "
           "%.0f %.0f %.0f, its way %.2f %.2f %.2f (the tip's motion, %.0f units)%s",
           rt_frame, s.hand == kLeft ? "left" : "right", s.what == kUnknown ? "fist" : weapon_name(s.slot), character,
           battle_id, sw.sweeps, s.speed, at.x, at.y, at.z, way[0] * wn, way[1] * wn, way[2] * wn, wl,
           again ? " (another in the same sweep)" : "");
    if (s.what == kUnknown)
        for (FistHit& f : fist_hits)
            if (f.character == character || now - f.at > 1.0) {
                f = {character, now};
                break;
            }
    if (again) return;
    sw.landed = true;
    sw.landed_at = now;
    // The knock of the impact: a heavy weapon hardest.
    const float strength = s.what == kHeavy ? 1.0f : s.what == kGun ? 0.85f : s.what == kUnknown ? 0.75f : 0.7f;
    buzz(s.hand, strength, s.what == kHeavy ? 0.10f : 0.07f, 90.0f);
    if (s.hand != main_hand() || s.what == kUnknown) {
        // a fist: the game's hit sound is the borrowed knife's; the knuckles' thud with it
        hand_sound(kSoundHandSurface, at, static_cast<int>(s.speed * 100.0f));
    } else if (s.what == kGun)
        hand_sound(kSoundWeaponWall, at, s.slot);
    // A melee weapon's swing that hit before it was fast enough to press the game's attack presses it now (her
    // swing's sound and stamina as the game's own), its volumes dropped (the hit is done).
    if ((s.what == kLight || s.what == kHeavy) && !sw.fired && s.hand == main_hand() && s.slot == game.weapon &&
        !(s.what == kHeavy && now < still_until)) {
        sw.fired = true;
        sw.armed = false;
        sw.last = now;
        Vec3 g, tip;
        float radius;
        segment(s.hand, s.slot, g, tip, radius);
        fire_game_attack(sw, s.slot, s.what, g, tip, radius);
    } else if (pending.armed && pending.slot == s.slot && s.hand == main_hand())
        pending.landed = true;
}

// A character other than Heather took damage from her this frame (0x4B6A20 for it): logged with the hit's way and
// reaction (owner 65: the reaction the game gives goes the way the weapon went); a fist's hit keeps half (LIST 27:
// bare hands at half the weakest melee weapon's damage, the knife's attack borrowed).
float combat_hit_given(uint32_t character, const float way[3], const float point[3], float damage, int reaction) {
    float keep = 1.0f;
    for (FistHit& f : fist_hits)
        if (f.character == character && now - f.at <= 1.0) {
            keep = 0.5f;
            f = {};
            break;
        }
    rt_log("VR: frame %d: %08X took %.1f damage from her%s (reaction %d) at %.0f %.0f %.0f, the way %.2f %.2f %.2f",
           rt_frame, character, damage * keep, keep < 1.0f ? " (a fist's: half)" : "", reaction, point[0], point[1],
           point[2], way[0], way[1], way[2]);
    return keep;
}

// LIST 50 (the owner: "physical blocking, on by default, responsive (the guard from raising the weapon or arms as a
// guard)"). SH3 has no guard of its own: a monster's hit on Heather is gathered by 0x4B6A20 into her pending damage
// (+0x15C), then taken from her health by 0x4EC800 (sh3.c wraps 0x4B6A20 and asks here). A hit is guarded when her
// weapon, or else a forearm, is up between her chest and where the hit came from: its middle 8 to 85 cm out from her
// chest, within 45 degrees of the line to the attacker seen from above (or, without one, to the hit's own point, or
// ahead of her), from her waist to over her head.
// What it keeps of the damage: a quarter behind a melee weapon (a knife a third), half behind a gun, 60% on a bare arm.
// A buzz in the guarding hand and the weapon's knock (or the hand's pat) where it took the hit.
float combat_block(const float at[3], const float way[3], const float from[3], int has_from, float damage, int kind) {
    if (!opt.blocking || view_mode != ViewMode::kFirstPerson || !map_ready() || !game.player_valid ||
        !(damage > 0.0f))
        return 1.0f;
    const Vec3 eye = world_point(fi.head.position);
    const Vec3 chest = eye + Vec3{0.0f, 0.30f * kUnitsPerMetre, 0.0f};  // the game's up is -y
    const Vec3 hit{at[0], at[1], at[2]};
    Vec3 toward;  // from her chest toward the attack
    const char* by;
    if (has_from) {
        Vec3 a{from[0], from[1], from[2]};
        a.y = chest.y;  // at her chest's height: monsters strike high and low
        toward = a - chest, by = "the attacker";
    } else {
        toward = hit - chest, by = "the hit's point";
    }
    toward.y = 0.0f;
    if (length(toward) < 0.15f * kUnitsPerMetre) {
        const Vec3 f{std::sin(game.yaw), 0.0f, std::cos(game.yaw)};  // her facing (sh3vr.h)
        toward = Vec3{f.x, 0.0f, f.z}, by = "ahead of her";
    }
    toward = normalized(toward);
    char seen[320] = "";  // each candidate's distance and angle, for the log
    int seen_n = 0;
    // Ahead of her along the attack's line, taken flat (vr147: a forearm raised before her face sat 45 degrees above
    // her chest and was missed, though monsters strike high): 8 to 85 cm out, within 45 degrees of the line seen from
    // above, and from 45 cm below her chest to 55 cm above it (her waist to over her head).
    auto guards = [&](Vec3 a, Vec3 b, float& out_cos, Vec3& mid) {
        mid = (a + b) * 0.5f;
        const Vec3 d = mid - chest;
        const Vec3 flat{d.x, 0.0f, d.z};
        const float l = length(flat), up = -d.y / kUnitsPerMetre;  // the game's up is -y
        out_cos = l > 1.0f ? dot(flat * (1.0f / l), toward) : -1.0f;
        if (seen_n < (int)sizeof seen - 48)
            seen_n += std::snprintf(seen + seen_n, sizeof seen - seen_n, "%s%.0f cm out, %.0f deg, %.0f cm up",
                                    seen_n ? "; " : "", l / kUnitsPerMetre * 100.0f,
                                    std::acos(std::clamp(out_cos, -1.0f, 1.0f)) * 180.0f / kPi, up * 100.0f);
        return l >= 0.08f * kUnitsPerMetre && l <= 0.85f * kUnitsPerMetre && out_cos >= 0.707f && up >= -0.45f &&
               up <= 0.55f;
    };
    float keep = 1.0f, c = 0.0f;
    int h_by = -1;
    Vec3 mid{};
    const char* what = nullptr;
    const WeaponInfo* w = info_of(game.weapon);
    const int mh = main_hand();
    if (w && in_hand && hands[mh].valid) {
        Vec3 g, tip;
        float r;
        segment(mh, game.weapon, g, tip, r);
        if (guards(g, tip, c, mid)) {
            keep = w->kind == kGun || w->kind == kStun ? 0.5f : game.weapon == 4 ? 0.33f : 0.25f;
            h_by = mh, what = weapon_name(game.weapon);
        }
    }
    for (int h = 0; h < 2 && h_by < 0; h++) {
        Vec3 e, wr;
        if (hands[h].valid && ik_forearm(h, e, wr) && guards(e, wr, c, mid)) {
            keep = 0.6f;
            h_by = h, what = h == kLeft ? "her left forearm" : "her right forearm";
        }
    }
    const Vec3 wv{way[0], way[1], way[2]};
    // Logged: every hit on her for the first 40, then at most one a second with how many went unwritten since
    // (session 16: a monster's touch is a hit every frame, the hall's dog's 0.2 a frame; vr230 and vr234 wrote over
    // 4,000 of these lines in five minutes, each a flushed write on the game's thread on the Quest).
    static int hits_logged = 0, hits_quiet = 0, quiet_guarded = 0;
    static float quiet_damage = 0.0f, quiet_kept = 0.0f;
    static double hit_logged_at = -10.0;
    const bool log_hit = hits_logged < 40 || now - hit_logged_at >= 1.0;
    if (log_hit) {
        char since[160] = "";
        if (hits_quiet)
            std::snprintf(since, sizeof since, " [since the last written: %d more hits, %d guarded, %.1f damage of which "
                          "%.1f kept]", hits_quiet, quiet_guarded, quiet_damage, quiet_kept);
        rt_log("VR: frame %d: a hit on her (kind %d, %.1f damage, its point %.0f cm from her chest, its way %.2f %.2f "
               "%.2f; from %s): %s (the weapon's or forearms' middles from her chest, off the attack's line: %s)%s",
               rt_frame, kind, damage, length(hit - chest) / kUnitsPerMetre * 100.0f, wv.x, wv.y, wv.z, by,
               what ? "guarded" : "not guarded", seen_n ? seen : "none to try", since);
        hits_logged++;
        hit_logged_at = now;
        hits_quiet = quiet_guarded = 0;
        quiet_damage = quiet_kept = 0.0f;
    } else {
        hits_quiet++;
        quiet_guarded += what ? 1 : 0;
        quiet_damage += damage;
        quiet_kept += what ? damage * keep : damage;
    }
    if (!what) return 1.0f;
    if (log_hit)
        rt_log("VR: frame %d: guarded by %s (%.0f degrees off the attack's line): %.1f of %.1f damage kept", rt_frame,
               what, std::acos(std::clamp(c, -1.0f, 1.0f)) * 180.0f / kPi, damage * keep, damage);
    buzz(h_by, 0.9f, 0.12f, 120.0f);
    hand_sound(w && h_by == mh && in_hand ? kSoundWeaponWall : kSoundHandSurface, mid, 400);
    return keep;
}

void combat_actions(Sh3VrActions* out) {
    if (pending_equip >= 0) {
        out->equip_slot = pending_equip;
        pending_equip = -1;
    }
    if (pending_put_away) {
        out->put_away = 1;
        pending_put_away = false;
    }
}

void combat_shot() {
    const WeaponInfo* w = info_of(game.weapon);
    if (!w || (w->kind != kGun && w->kind != kStun)) return;
    const int mh = main_hand(), oh = off_hand();
    buzz(mh, w->kind == kStun ? 0.5f : 0.8f, w->kind == kStun ? 0.15f : 0.07f, w->kind == kStun ? 90.0f : 160.0f);
    if (hands[oh].on_foregrip) buzz(oh, 0.5f, 0.06f);
    Vec3 f, d;  // the shot's line (the kick not yet in the gun's turn either way: gun_ray takes aim_turn)
    shot_line_known = map_ready() && gun_ray(f, d);
    if (shot_line_known) shot_from_w = world_point(f), shot_dir_w = normalized(world_dir(d));
    kick = w->kick * (hands[oh].on_foregrip ? 0.5f : 1.0f);
    flash_until = now + 0.2f;
    effects_until = now + 0.8f;
    shots_fired++;
}

bool combat_effect_rel(const float point[3], int tex_w, float r[3][3], float t[3], float info[7]) {
    Aff map;
    Vec3 from, dir;
    (void)tex_w;
    // (The beam saber: session 14 moved its screen-space effects here rigidly; its beam is a 3D mesh, moved in the
    // eyes by sh3vr_beam_mesh: vr208. Its other screen-space effects (sparks, the lights' halos) stay where drawn.)
    if (!in_hand || view_mode != ViewMode::kFirstPerson || now > effects_until || rt_frame - game_gun_frame > 1 ||
        !live_weapon_map(map) || !gun_ray(from, dir))
        return false;
    // the game's gun at the update these effects are of (its held bone in that update's matrix)
    const Vec3 g = held_in_weapon_known && rt_frame - w_game_frame <= 1 ? mul(w_game, held_in_weapon).t : game_gun;
    const Vec3 p{point[0], point[1], point[2]};
    const Vec3 muzzle = world_point(from);
    const float d = std::min(length(p - g), length(p - muzzle));
    if (!(d < 0.5f * kUnitsPerMetre)) return false;  // not the gun's own: within half a metre of the gun, either one
    // Session 22 (vr275): read back through the game's own camera (sh3vr_effect_map), the gun's effects are already at
    // the gun in the hand: the game places them from the weapon's place (0x4954E0), which VR moves there (sh3.c), and
    // its shot from the muzzle (0x4B3D50). The flash's quads lay along the hand's barrel from its muzzle (-15650 -615
    // 19889, the shot's origin to the unit), the ejected case from the slide. Moved again with the gun's map (build
    // 221) they landed 33 cm off, beside the slide; vr75's "on her grip" was the head's camera read for the game's.
    // So they stay at their place in the world, only turned to face the head, and are drawn whole wherever the game's
    // camera has them (no fade at its picture's edge, no lamp rule).
    // Owner 109 ("double check pistol ... be sure muzzle flash is actually at where it should be"): the flash's quads
    // stay where the shot left them, on the shot's line from the muzzle without the recoil's kick (gun_ray), while the
    // gun is drawn turned up by the kick about the fist (wf.turn = kick * aim_turn: 7 degrees on the handgun, its muzzle
    // 2.6 cm higher). So the quads on that line are turned with the gun, the kick's turn about the fist carried into the
    // game's world, and sit at the drawn muzzle. The effects the game places from the drawn gun afterwards (0x4954E0,
    // the kick already in it: vr276 frame 7373) are off that line and kept as they are.
    R3 k;
    k.m[0][0] = k.m[1][1] = k.m[2][2] = 1.0f;
    Vec3 kt{0, 0, 0};
    if (kick > 0.05f && wf.placed && shot_line_known) {
        const Vec3 rel = p - shot_from_w;
        const float along = dot(rel, shot_dir_w);
        const float off = length(rel - shot_dir_w * along);
        if (off < 0.012f * kUnitsPerMetre && along > -0.1f * kUnitsPerMetre && along < 0.5f * kUnitsPerMetre) {
            const Quat kq = wf.turn * conjugate(wf.aim_turn);
            const Vec3 e[3] = {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}};
            for (int j = 0; j < 3; j++) {
                const Vec3 c = world_dir(rotate(kq, tracking_dir(e[j])));
                k.m[0][j] = c.x, k.m[1][j] = c.y, k.m[2][j] = c.z;
            }
            const Vec3 b = world_point(wf.base);
            kt = b - mul(k, b);
        }
    }
    for (int i = 0; i < 3; i++)
        for (int j = 0; j < 3; j++) r[i][j] = k.m[i][j];
    t[0] = kt.x, t[1] = kt.y, t[2] = kt.z;
    const Vec3 q = mul(k, p) + kt;
    info[0] = d;
    info[1] = g.x, info[2] = g.y, info[3] = g.z;
    info[4] = q.x, info[5] = q.y, info[6] = q.z;
    return true;
}

bool combat_beam_map(float r[3][3], float t[3]) {
    Aff map;
    if (game.weapon != 9 || !in_hand || view_mode != ViewMode::kFirstPerson || rt_frame - w_game_frame > 1 ||
        !live_weapon_map(map))
        return false;
    for (int i = 0; i < 3; i++)
        for (int j = 0; j < 3; j++) r[i][j] = map.r.m[i][j];
    t[0] = map.t.x, t[1] = map.t.y, t[2] = map.t.z;
    return true;
}

bool combat_beam_line(float game_a[3], float game_b[3], float vr_a[3], float vr_b[3]) {
    Aff map;
    if (game.weapon != 9 || !in_hand || view_mode != ViewMode::kFirstPerson || !wf.placed || !wf.hold ||
        !held_in_weapon_known || rt_frame - w_game_frame > 1 || !live_weapon_map(map))
        return false;
    // the hold's points are in the held bone's own space (combat_weapon_draw's H), the bone as the game drew it the
    // weapon matrix times its place in it (vr208: through the weapon matrix alone the VR palm came out behind the eyes)
    const Aff hb = mul(w_game, held_in_weapon);
    const Vec3 a = mul(hb.r, wf.palm) + hb.t, b = mul(hb.r, wf.hold->tip) + hb.t;
    const Vec3 qa = mul(map.r, a) + map.t, qb = mul(map.r, b) + map.t;
    if (!(length(b - a) > 0.02f * kUnitsPerMetre) || !(length(qb - qa) > 0.02f * kUnitsPerMetre)) return false;
    game_a[0] = a.x, game_a[1] = a.y, game_a[2] = a.z;
    game_b[0] = b.x, game_b[1] = b.y, game_b[2] = b.z;
    vr_a[0] = qa.x, vr_a[1] = qa.y, vr_a[2] = qa.z;
    vr_b[0] = qb.x, vr_b[1] = qb.y, vr_b[2] = qb.z;
    return true;
}

void combat_crash_info(FILE* f) {
    std::fprintf(f, "VR weapon: game %d (%s), slot %d, owned %08x, in hand %d, awaiting %d, holsters:", game.weapon,
                 weapon_name(game.weapon), game.weapon_slot, game.weapons_owned, in_hand, awaiting_draw);
    for (int k = 0; k < kHolsterCount; k++) std::fprintf(f, " %s=%d", holster_name(k), holsters[k].content);
    std::fprintf(f, "\nVR melee: pending %d (slot %d), aim until %.2f, attack %.2f-%.2f, still until %.2f, now %.2f, "
                    "extra hits %d, punches %d\n", pending.armed, pending.slot, aim_pulse_until, attack_from,
                 attack_until, still_until, now, n_extra, punches_thrown);
    std::fprintf(f, "VR weapon frame: placed %d (profile %d), palm %.3f %.3f %.3f, turn %.3f %.3f %.3f %.3f, scale %.5f "
                    "m a unit, mirrored %d; held back from a wall %.3f m (touching %d), its line %s; hands' roles %d %d\n",
                 wf.placed, wf.profile, wf.base.x, wf.base.y, wf.base.z, wf.turn.x, wf.turn.y, wf.turn.z, wf.turn.w,
                 wf.scale, wf.mirror, wall_back, wall_touching, reach_valid ? "probed" : "none", hand_role[0],
                 hand_role[1]);
    std::fprintf(f, "VR native grips (the game's own gripping hands): shown now left %d right %d; read:", native_shown[0],
                 native_shown[1]);
    for (int p = 0; p < kProfiles; p++)
        for (int h = 0; h < 2; h++)
            if (native_group_of(p, h) >= 0)
                std::fprintf(f, " %s %s %s (still %d),", kWeapons[p].name, h == kRight ? "right" : "left",
                             native[p][h].have ? "yes" : "no", native[p][h].still);
    std::fprintf(f, " grip point %.1f %.1f %.1f\n", wf.palm.x, wf.palm.y, wf.palm.z);
}


// ---------------------------------------------------------------- shake to break free (owner message 38)
// The owner: "not sure how it works when enemies grab you, but see about adding an option to shake hands or something to
// break out of holds. on by default". The game's own way (the guides: "mash the shoulder and directional buttons"):
// while she is knocked down or held, its struggle step 0x4D44E0 (wrapped in sh3.c) counts the buttons and directions
// newly pressed each frame and takes one frame's time ([0x712C108]) off the time left to get free (0x712C608) for each,
// a fifth of one by itself; she is free at 0. "Shake to break free" counts each sharp turn of a controller's motion (a
// hand moved fast one way, then back) as one more press: shaking both hands, pulling free, works as mashing does, and
// the buttons still count. A buzz and a sound for each shake counted, a buzz in both hands as a hold starts and as she
// gets free. Shakes outside a hold count for nothing. Since a press is worth one frame's time to the game, at the
// headset's 120 Hz it would be worth half what it is at the PC game's steady 60 (vr153: [0x712C108] 1/72 on the rig);
// in VR sh3.c makes each press, shaken or a button, worth 1/60 s.
namespace {
constexpr float kShakeFast = 0.75f;    // m/s: a stroke this fast arms a turn
constexpr float kShakeBack = 0.30f;    // m/s the other way: the turn, one shake
constexpr float kShakeGap = 0.06f;     // s: at most one shake a hand this often
constexpr double kStruggleKeep = 0.5;  // s: the game's struggle step seen this recently = she struggles now
constexpr int kStruggleMostAStep = 4;  // presses given to one of the game's steps (the rest wait for the next)
struct ShakeHand {
    Vec3 dir;
    float peak = 0;
    double last = -10;
};
ShakeHand shake_hand[2];
double struggle_clock = 0, struggle_seen = -10, struggle_began = 0, struggle_told = -10;
int struggle_pending = 0, struggle_shakes = 0, struggle_given = 0, struggle_buttons = 0, struggle_holds = 0,
    struggle_freed = 0;
bool struggling = false;
float struggle_left = 0, struggle_left_first = 0;
}  // namespace

void struggle_frame() {
    struggle_clock += frame_dt();
    const bool now = struggling && struggle_clock - struggle_seen < kStruggleKeep;
    if (struggling && !now) {  // the hold ended without the step saying she got free (a cutscene, her death...)
        rt_log("VR: frame %d: the hold ended after %.1f s (%d shakes counted, %d given as presses, %d buttons)", rt_frame,
               struggle_clock - struggle_began, struggle_shakes, struggle_given, struggle_buttons);
        struggling = false;
    }
    for (int h = 0; h < 2; h++) {
        ShakeHand& s = shake_hand[h];
        const HandState& hs = hands[h];
        if (!hs.valid || !std::isfinite(hs.speed)) {
            s.peak = 0;
            continue;
        }
        const Vec3 v = hs.vel;
        const float sp = hs.speed;
        if (s.peak >= kShakeFast && dot(v, s.dir) < -kShakeBack && struggle_clock - s.last >= kShakeGap) {
            s.last = struggle_clock;
            if (now && opt.shake_free) {
                struggle_pending++;
                struggle_shakes++;
                buzz(h, 0.45f, 0.04f);
                hand_sound(kSoundStruggle, world_point(hs.grip.position), (int)(s.peak * 100.0f));
            }
            s.dir = v * (1.0f / std::max(sp, 1e-4f));
            s.peak = sp;
            continue;
        }
        if (sp > s.peak) {
            if (sp > 0.05f) s.dir = v * (1.0f / sp);
            s.peak = sp;
        }
        s.peak *= std::exp(-frame_dt() * 2.0f);  // a stroke long past forgotten
    }
    if (!now) struggle_pending = 0;
}

int struggle_presses(float left) {
    struggle_seen = struggle_clock;
    if (!struggling) {
        struggling = true;
        struggle_holds++;
        struggle_began = struggle_clock, struggle_told = struggle_clock;
        struggle_shakes = 0, struggle_given = 0, struggle_buttons = 0, struggle_pending = 0;
        struggle_left_first = left;
        if (opt.shake_free) {
            buzz(kLeft, 0.6f, 0.15f);
            buzz(kRight, 0.6f, 0.15f);
        }
        rt_log("VR: frame %d: she is knocked down or held: the game's time to get free %.2f; %s", rt_frame, left,
               opt.shake_free ? "shake the controllers (or press the buttons) to get free sooner"
                              : "\"Shake to break free\" is off: the buttons only");
    }
    struggle_left = left;
    const int n = opt.shake_free ? std::min(struggle_pending, kStruggleMostAStep) : 0;
    struggle_pending -= n;
    struggle_given += n;
    if (struggle_clock - struggle_told >= 1.0) {
        struggle_told = struggle_clock;
        rt_log("VR: frame %d: held %.1f s: the time to get free %.2f of %.2f; %d shakes counted, %d given as presses, %d "
               "buttons",
               rt_frame, struggle_clock - struggle_began, left, struggle_left_first, struggle_shakes, struggle_given,
               struggle_buttons);
    }
    return n;
}

void struggle_done(int freed, int buttons) {
    if (struggling && buttons > 0) struggle_buttons += buttons;
    if (!freed || !struggling) return;
    struggling = false;
    struggle_freed++;
    if (opt.shake_free) {
        buzz(kLeft, 0.8f, 0.2f);
        buzz(kRight, 0.8f, 0.2f);
    }
    rt_log("VR: frame %d: she got free after %.1f s (%d shakes counted, %d given as presses, %d buttons; the time to get "
           "free was %.2f)",
           rt_frame, struggle_clock - struggle_began, struggle_shakes, struggle_given, struggle_buttons, struggle_left_first);
}

void struggle_crash_info(FILE* f) {
    std::fprintf(f, "VR shake to break free: %s; %s; holds %d, got free %d; this hold: %d shakes, %d given, %d buttons, the "
                    "time to get free %.2f of %.2f\n",
                 opt.shake_free ? "on" : "off", struggling ? "held now" : "not held", struggle_holds, struggle_freed,
                 struggle_shakes, struggle_given, struggle_buttons, struggle_left, struggle_left_first);
}

}  // namespace sh3q
