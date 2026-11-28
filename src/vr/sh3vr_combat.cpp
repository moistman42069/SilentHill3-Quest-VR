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
    // 0.1.7 (owner messages 129, 133: "weapon grips should look natural and accurately fitted to the guns", "whichever
    // looks best"): the second hand's thumb (0 straight .. 1 curled; -1: as its fingers, at most 0.7), and the second
    // hand moved from its point (model units: from the game's own left hand's place where there is one, else from
    // second_at) while the main hand holds at the game's own place (second_nudge(profile)), as the rig's pictures
    // showed best (vr312: the handgun's six ways and the submachine gun's).
    float second_thumb = -1.0f;
    Vec3 second_nudge{};
};
// By profile (kWeapons' order). Session 5's measurements (the meshes in tools/gun_view.py's pictures):
// - Handgun, bone 0: barrel +x, top +y; the grip's back strap from (20.8, -24) to (32.4, 30), its front strap from
//   (51.2, -23) to (57.2, 6): raked 12 degrees, 30 units (6 cm) deep, z -22.4 to -4.7; the fist's centre half way
//   between the straps at the middle finger's height. The tip at x 101.4, bone 0's end; the muzzle (session 22, owner
//   109) is the slide's front (bone 1, posed at 139.6 38.1 -13.1 in bone 0's frame: vr188), x 137.0 round y 38.1,
//   z -13.1: muzzle_point. At the tip the flash sat 7 cm back inside the slide. The second hand wraps round the main hand's
//   fingers from the left front, its palm against the grip's left side.
//   0.1.7 (vr312): with her main hand at the game's place (its fist 3 cm behind the measured one) the second hand sits
//   4 cm back and 1.4 cm up (second_nudge), its palm on the grip's left panel, its fingers round the main fist's front.
// - Submachine gun, bone 0: barrel +x, top +y; the grip (the magazine up through it) x 26.4-56.7, y -21 to 29, raked 6
//   degrees; the muzzle x 115.6, y 40-50. The second hand cups the receiver's front under the barrel, palm up, thumb
//   forward: the front block x 84-112 (z -23.8 to -0.2), its floor at y 29-31, ahead of the trigger guard (x 56-84,
//   hanging to y 6.6). Session 14 (vr197, owner 58): the fist's centre lies 1.9 cm over the palm's skin (her grip
//   space: the palm 1.9 cm behind the fist's centre), so a centre at y 16 left the palm at the guard's floor, 4.7 cm
//   under the receiver, the guard drawn across her fingers; at y 38.5 the palm lies on the floor, the fingers up its
//   right side, the little finger over the guard's front where it meets the receiver.
//   0.1.7 (vr312): at y 32.5 (the palm 1.2 cm under the floor) her fingers up its right side end at its top instead of
//   2 cm over it, and the thumb lies straight along its left side (curled, it stood up over the top).
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
     {58.0f, -10.0f, -18.0f}, {0.300f, 0.954f, 0}, {0, 0, 1}, 0.80f, 0.74f, -1.0f, {-20.0f, 7.0f, -2.0f}},
    {true, false, 1, {1, 0, 0}, {0, -1, 0}, {-282.0f, 33.0f, 0}, {0.28f, -0.96f, 0}, {0, 0, 0}, true,
     {-72.0f, 18.0f, 0}, {1, 0, 0}, {0, -1, 0}, 0.80f, 0.70f},
    {true, false, 0, {1, 0, 0}, {0, 1, 0}, {41.5f, 3.0f, -10.6f}, {0.105f, 0.994f, 0}, {115.6f, 45.0f, -11.9f}, true,
     {95.0f, 32.5f, -14.5f}, {1, 0, 0}, {0, 1, 0}, 0.78f, 0.70f, 0.15f},
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
     {155.0f, 26.0f, -18.0f}, {1, 0, 0}, {0, 1, 0}, 0.80f, 0.70f, 0.15f},
};
const Hold* hold_of(int w) {
    const int p = profile_of(w);
    return p >= 0 && kHolds[p].known ? &kHolds[p] : nullptr;
}

// Testing (0.1.7, the rig's grip pictures): SH3_SECOND_TEST="frame:profile,dx,dy,dz,curl,thumb;..." each entry from its
// game frame on, for its profile: the second hand moved dx dy dz (model units) from its point, its fingers' curl and
// its thumb's (-1: the hold's own). Entries in frame order; the latest begun is used, logged as it begins.
struct SecondTest {
    int from, profile;
    Vec3 shift;
    float curl, thumb;
};
const SecondTest* second_test(int profile) {
    static SecondTest tests[48];
    static int n = -1, last[kProfiles];
    if (n < 0) {
        n = 0;
        for (int& l : last) l = -1;
        const char* s = getenv("SH3_SECOND_TEST");
        while (s && *s && n < 48) {
            SecondTest t{};
            if (std::sscanf(s, "%d:%d,%f,%f,%f,%f,%f", &t.from, &t.profile, &t.shift.x, &t.shift.y, &t.shift.z, &t.curl,
                            &t.thumb) == 7)
                tests[n++] = t;
            s = std::strchr(s, ';');
            if (s) s++;
        }
    }
    int at = -1;
    for (int i = 0; i < n; i++)
        if (tests[i].profile == profile && tests[i].from <= rt_frame) at = i;
    if (profile >= 0 && profile < kProfiles && at != last[profile]) {
        last[profile] = at;
        if (at >= 0)
            rt_log("VR: frame %d: second hand test %d (profile %d): moved %.1f %.1f %.1f, curl %.2f, thumb %.2f", rt_frame,
                   at, profile, tests[at].shift.x, tests[at].shift.y, tests[at].shift.z, tests[at].curl, tests[at].thumb);
    }
    return at >= 0 ? &tests[at] : nullptr;
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
// 0.1.7 (owner messages 129, 133: "weapon grips should look natural and accurately fitted to the guns", "whichever
// looks best"): a hand on a weapon with the game's own gripping hand is placed where the game holds it (its hand bone in
// the weapon's), and drawn either as that rigid hand (as Team Silent made it for the third-person view: vr302, the
// trigger finger straight past the trigger, the other fingers loose) or as her finger hand there, its fingers closed on
// the weapon and the trigger finger following the controller's trigger: kFingerHandOn says which, by profile.
// vr303: her finger hand round the old measured hold sat 3 cm ahead of the grip, its fingers closed on air.
// Testing: SH3_GRIP_HAND=0 her finger hand at the measured hold (the old way), 1 the game's own hand on every weapon
// that has one, 2 her finger hand at the game's place on every weapon (the rig's comparisons).
// Chosen from the rig's pictures (vr302 the game's hands, vr309 her finger hand at the game's place): the guns take her
// finger hand (her fist closed round the grip, the trigger finger at the trigger; the game's own has it straight past the
// trigger and the other fingers loose); the knife keeps the game's hand (her fingers splayed through its thin handle),
// and the pipe, katana, maul and beam saber too (alike either way, the game's made for them).
// 0.1.8 (owner messages 146-148: "by default though, make ALL guns and weapons/melee etc, grip properly and accurately",
// the fingers' collision "guns/weapons/melee weapons, etc."): every weapon in her finger hand, each finger closing on the
// weapon's own shape (sh3vr_shape.cpp) at its fitted place (the game's rigid hands do not close on the weapon in VR:
// the knife's fingers stood through its handle, the katana's past its grip, as the rig's pictures showed).
bool kFingerHandOn[kProfiles] = {true, true, true, true, true, true, true, true, true, true};
int grip_hand_test() {
    static int t = -2;
    if (t == -2) t = getenv("SH3_GRIP_HAND") ? atoi(getenv("SH3_GRIP_HAND")) : -1;
    return t;
}
bool finger_drawn(int profile) {
    const int t = grip_hand_test();
    if (t == 0 || t == 2) return true;
    if (t == 1) return false;
    return profile >= 0 && profile < kProfiles && kFingerHandOn[profile];
}
const NativeGrip* native_of(int profile, int h) {
    if (opt.left_handed || native_group_of(profile, h) < 0 || grip_hand_test() == 0) return nullptr;
    return native[profile][h].have ? &native[profile][h] : nullptr;
}
// 0.1.8: the main hand's own grip, the game's (its right hand's, read once it holds still). A left-handed player's main
// hand holds each weapon as the mirror image of it across the weapon's middle (main_hand_pose, build_frame: the game's
// hands are a right-handed player's, the weapon itself is never mirrored), so a left hand sits on every weapon as the
// right hand does, its trigger finger on the trigger; before 0.1.8 it kept the measured hold (3 cm off the grip).
int grip_fit_mode();
const NativeGrip* main_native(int profile) {
    if (!opt.left_handed) return native_of(profile, kRight);
    if (grip_fit_mode() == 0 || native_group_of(profile, kRight) < 0 || grip_hand_test() == 0) return nullptr;
    return native[profile][kRight].have ? &native[profile][kRight] : nullptr;
}
// The second hand moved by the hold's second_nudge, which is set for the main hand at the game's own place (its fist
// there lies about 3 cm behind the measured hold's; mirrored for a left-handed player, main_native).
Vec3 second_nudge(int profile) {
    if (profile < 0 || profile >= kProfiles) return Vec3{};
    if (const SecondTest* t = second_test(profile)) return t->shift;
    return main_native(profile) ? kHolds[profile].second_nudge : Vec3{};
}

// ---------------------------------------------------------------- the hands' fitted places (0.1.8)
//
// Owner message 146 ("why is the default grip positions not actually perfect off the bat? ... the fuckin finger isnt
// even on the trigger and two hand grip hand is still a bit off"): 0.1.7 drew her finger hand where the game holds each
// gun (its own rigid hand's bone), but that hand is made for the third-person view. At its place her fist's centre lies
// on the handgun's back strap, so her fingers closed inside the grip (none came round its front), her palm stood behind
// it and her trigger finger stopped behind the trigger (the 0.1.7 rig's own pictures, vr309 and vr313, show it, as the
// owner's video does: the hand low on the grip). VR games pose a hand on a grab point and close each finger until it
// meets the object's shape, the trigger finger on its own trigger pose (HurricaneVR's grab points, Auto Hand's grab
// solver, Meta's hand grabs): so each gun's hands here are placed by a fit of her own hand (her fingers and palm as
// drawn, sh3vr_body.cpp body_grip_fit) against the weapon's own shape (its mesh as drawn, sh3vr_shape.cpp; kShapes'
// capsules only where that is not built) and its trigger (kTriggers): her knuckles
// and palm out of it, her palm against it, every finger starting clear and closing round it, the trigger finger's pad
// on the trigger's blade, the other hand's fingers round the weapon and the main hand's fingers. The search
// (SH3_GRIP_FIT=1 on the PC rig, fit_step: about 64,000 places and turns a hand) found the places below; the game uses
// them as they are, with no search on the headset. A place: the hand moved (model units) and turned (degrees about the
// weapon's right, then about the grip's own line) from where 0.1.7 put it, about its fist's centre. The main hand's
// fist's centre stays at the controller's grip point (the weapon moved against it by the shift); the turn turns the
// hand on the weapon, which keeps 0.1.7's way on the controller (build_frame): a gun aims where it aimed.
struct GripPlace {
    Vec3 shift;
    float pitch = 0, roll = 0;
};
// By profile: the main hand from the game's own place (once read; mirrored for a left-handed player); the weapons held in
// the fist keep 0.1.7's place along their handle (main_fit). 0.1.10 (owner 172: the guns' grips through her palm):
// fitted with her hand as drawn (SH3_GRIP_FIT=1; body_grip_fit's score: her drawn palm against the weapon and never more
// than 1 mm in, every finger closing round it, the trigger finger's pad on the trigger the whole pull; body_grip_mesh:
// her whole hand as drawn, every finger as drawn there, at least 0.2 mm out of the weapon). The handgun's main hand: the
// search at every turn about its grip (session 38, build 291, vr516; 0.1.8's turn, 52 degrees round from that place, held
// the grip across the middle of her palm) found {{17.7, -16.1, 10.5}, 20, 0}, clear but at the foot of the grip, her index
// rising 16 degrees to the trigger (vr517); then the climb up the grip's line from there, a fine search at each 2 mm
// (build 296, SH3_FIT_CLIMB, vr525), its highest: 20 mm up, the web of her hand under the slide's tang, her index 8
// degrees up to the trigger (none found from 22 mm up). Every other weapon fitted again on build 296 (vr526: the
// in-play thumb resting on her drawn skin, the trigger finger's rise weighed, the guns' main hands at every turn about
// their grips; the knife's, katana's and maul's places before no longer clear); the stun gun's the same as before.
// (Before: shotgun {{3.9, 1.3, 2.5}, -16, -16}, submachine gun {{22.4, -0.7, 9.4}, -8, -4}, knife {{7.3, -3.0, 6.5}, -8,
// 28}, steel pipe {{-0.9, -0.2, 6.0}, -12, 20}, katana {{-0.8, 0.3, -0.3}, -16, 36}, maul {{-4.9, 1.4, 0.2}, -12, -12},
// beam saber {{-6.2, 2.3, -0.6}, -16, 8}.) The flamethrower's kept from build 292 (vr517: 0.2 mm off it as played):
// vr526's {{12.5, -15.7, 2.7}, 20, -152}, 0.2 mm in the fit, came 6.8 mm into it as played (her thumb resting closed on
// her drawn skin) with her wrist bent 147 degrees (vr530, vr533). Every place is now taken from the pictures as played
// (the hand as drawn, by bone and by triangles), the fit's own reading the search's alone (SH3_FIT_PLAY=1 makes them
// one, build 301). (0.1.8's, build 266, vr400, against the
// hand model's palm: handgun {{25.5, -3.5, -8.0}, -4, -52}, shotgun {{-3.5, 5.5, 14.0}, -4, -32}, submachine gun
// {{15.0, 0.0, -9.0}, -8, -44}, stun gun {{31.9, 20.9, 0.8}, -16, 12}, knife {{1.4, -15.6, 0.1}, -16, 4}, steel pipe
// {{9.4, 23.6, -3.7}, -12, 20}, katana {{7.1, 21.9, -3.3}, -8, 12}, maul {{-12.4, -22.2, -3.8}, -20, -28}, beam saber
// {{-13.4, -24.1, 1.7}, -16, 40}, flamethrower {{10.0, -19.5, -3.0}, 0, 36}.)
GripPlace fit_main[kProfiles] = {
    {{16.3f, -6.3f, 12.5f}, -2.0f, -8.0f},     // handgun (vr525's climb, 20 mm up its grip: 4.19, index 8 up; 0.4 mm)
    {{4.4f, 1.3f, 2.5f}, -16.0f, -16.0f},      // shotgun (vr526: 5.98, index 13 down; 0.2 mm)
    {{21.8f, -0.7f, 9.4f}, -8.0f, -4.0f},      // submachine gun (vr526: 5.79, index 2 up; 0.3 mm)
    {{26.1f, -12.8f, 5.0f}, -32.0f, 16.0f},    // stun gun (vr526: 6.57; 0.7 mm)
    {{18.3f, -5.9f, 2.5f}, -20.0f, 32.0f},     // knife (vr526: 6.61; 0.3 mm)
    {{10.6f, -3.5f, 4.6f}, -24.0f, 40.0f},     // steel pipe (vr526: 6.02; 0.2 mm)
    {{2.2f, -0.5f, -2.1f}, -8.0f, 48.0f},      // katana (vr526: 6.90; 0.3 mm)
    {{6.1f, -2.0f, 4.0f}, 4.0f, -28.0f},       // maul (vr526: 6.00; 0.4 mm)
    {{5.0f, -1.8f, 0.5f}, -20.0f, 32.0f},      // beam saber (vr526: 5.92; 0.2 mm)
    {{16.0f, -5.2f, 0.7f}, 12.0f, -168.0f},    // flamethrower (build 292's; vr517: 0.2 mm as played)
};
// By profile: the other hand on the second grip (mirrored for a left-handed player), fitted as fit_main with the main
// hand at its place there, its skin also kept off her main hand's as drawn (session 38: hand_skin_between). The
// two-handed melee weapons' other hands: a move and turn from the main hand's mirrored place (second_hand_pose; session
// 38: until then the mirror alone, vr517: the beam saber's 5.8 mm into it, the pipe's 0.2). vr526 (build 296) with
// each main hand at its place above; the shotgun's pump the same as before. Build 297 (SH3_FIT_SECOND, the other hand
// alone): the handgun's round its main hand's place 20 mm higher (vr527; 3.0 mm into the grip there before), the
// submachine gun's front searched at every turn, free of its grip's line (vr528: her palm on the receiver's left side,
// her fingers over its top: owner 178, "who ... holds a smg like that", rejected). Build 299: an other hand that cups
// the weapon (its hold's palm up: the submachine gun's front, the shotgun's pump, the flamethrower's front) is searched
// only round a palm-up place (fit_step's cup rule), and the melee weapons' other hands close every finger round the
// handle, the index too (body_grip_fit; vr530: the maul's index lay straight). vr533 (build 299): the submachine gun's
// front cupped under the receiver's front, her fingers up its side; the pipe's and the maul's every finger round the
// handle (each 1.1 to 1.2 mm in as played before, vr530; now 0.3 to 0.4 mm off). The katana's and beam saber's kept
// (vr533's crowded her main hand). The maul's from vr533 came 3.3 mm into the shaft as played (vr536: its thumb rested
// against her main hand's skin in the fit alone): fitted as played (build 301, SH3_FIT_PLAY=1, vr539), its fingers
// round the shaft from below, the thumb along it. (Before: handgun {{-12.1, -14.1, -5.3}, -24, -24} with the main hand
// at the foot of its grip; submachine gun vr528's {{17.8, 10.2, 14.0}, -32, -84}, 0.1.8's {{0.5, 10.0, 5.0}, -48, 8} 15
// mm into the receiver with her palm as drawn; steel pipe {{11.0, 17.1, 3.2}, 8, -8}; maul {{-2.0, -3.1, 0.0}, 12, -28}
// and vr533's {{-2.0, -3.1, 0.0}, 8, -32};
// flamethrower vr526's {{-1.5, -4.2, 6.0}, -8, 12} with its main hand there. 0.1.8's: handgun {{1.6, -7.7, -2.6},
// -16, -24}, shotgun none (0.1.7's place), flamethrower {{-7.5, 8.0, -1.5}, 28, 12}.)
GripPlace fit_second[kProfiles] = {
    {{-12.1f, -14.1f, -5.3f}, -32.0f, -24.0f}, // handgun (vr527, the main hand high: 5.65; 0.5 mm)
    {{0.0f, 5.0f, 0.0f}, -16.0f, 0.0f},        // shotgun (vr526: 5.29; 0.3 mm)
    {{5.0f, -14.0f, 5.0f}, -8.0f, 8.0f},       // submachine gun (vr533, cupped under its front: 5.81; 0.5 mm as played)
    {}, {},                                    // stun gun, knife (one hand)
    {{4.0f, -8.7f, 3.0f}, 8.0f, -32.0f},       // steel pipe (vr533: 5.68; 0.4 mm as played)
    {{-1.8f, -7.9f, -2.2f}, 0.0f, 32.0f},      // katana (vr526: 5.99; 0.4 mm)
    {{-5.6f, -15.2f, 1.8f}, 12.0f, -16.0f},    // maul (vr539, fitted as played: 5.99; 0.4 mm by bone as played)
    {{0.7f, 26.6f, -4.0f}, -16.0f, 20.0f},     // beam saber (vr526: 5.86; 0.4 mm)
    {{-7.4f, -10.6f, 0.1f}, 8.0f, 24.0f},      // flamethrower (build 292's, with that main hand; vr517: 0.2 mm)
};
// 0.1.10 (session 38, vr537: the first left-handed pictures of every weapon read with her hand as drawn; vr518's and
// session 37's vr500 drew the handgun alone, SH3_EQUIP=1): the right-handed places, mirrored, took her left hand
// 6.7 mm into the katana's handle, 15 into the beam saber's, 9.3 into the flamethrower's grip, 1.6 into the stun gun's,
// and the submachine gun's front hand 8.3 into it; the knife's and the pipe's, the weapons alike on both sides, stayed
// clear.
// The weapons are not the same on both sides and her hands as drawn not quite each other's mirror, so a left-handed
// player has places of their own, fitted with her left hand as drawn (SH3_GRIP_FIT=1 with left_handed = 1, the same
// mirrored convention: main_hand_pose, second_hand_pose); main_fit and second_fit give these to a left-handed player.
// Each started as the right-handed place. vr540 (build 302, SH3_GRIP_FIT=1 SH3_FIT_PLAY=1, left_handed = 1: the fit
// as played) fitted the submachine gun's, stun gun's, katana's, beam saber's and flamethrower's main hands and the beam
// saber's and flamethrower's other hands; vr541 (SH3_FIT_SECOND=1) the maul's other hand. Each chosen by its pictures
// as played (vr540/vr541: every one round its grip as the right-handed hold is; the readings below as played). Three
// of the fit's other hands were clear but not natural, and are not taken: the submachine gun's front {{7.0, -11.5,
// 11.0}, -20, 8} (its fingers spread in the air beside the receiver), the katana's {{-7.7, -10.6, -4.1}, -20, 16}
// (under the end of its handle, its main hand turned 24 degrees off the right-handed one) and the handgun's {{-3.0,
// -18.6, -1.2}, -64, -4} (her wrist bent 117 degrees, the fingers hanging under the grip). Instead: the handgun's other
// hand searched only round the right-handed place (SH3_FIT_LOCAL=1, vr542); the submachine gun's front the
// right-handed place itself, clear with this main hand and cupped as the right-handed one (vr543; searched round it, it
// went back to the spread fingers); the katana's main hand searched round the right-handed place (vr544: turned 40
// degrees, as the right-handed 48), then its other hand from the right-handed place (vr546: round the end of the
// handle behind the main hand, every finger closed; the right-handed place itself 3.5 mm in, 2 mm out still 1.7).
// Her forearms as drawn (session 38, vr555/556, the rig's forearm reading): at the right-handed place the maul's end
// went 10 mm into her left forearm 4.5 cm behind the wrist, its metal cap out through the skin in the pictures from
// her eyes (vr549). Its main hand searched round that place with her forearm kept out (SH3_FIT_ARM=1, SH3_FIT_LOCAL=1,
// vr557): turned 8 degrees more each way, the forearm then 20 mm off as played, the shaft's end beside it as on 0.1.9,
// her wrist bent 31 degrees as drawn (102 before); its other hand, the main hand's mirror, fitted from there.
GripPlace fit_main_left[kProfiles] = {
    {{16.3f, -6.3f, 12.5f}, -2.0f, -8.0f},     // handgun (the right-handed place; vr537: 0.5 mm)
    {{4.4f, 1.3f, 2.5f}, -16.0f, -16.0f},      // shotgun (the right-handed place; vr537: clear)
    {{21.2f, -1.7f, 10.3f}, -4.0f, -8.0f},     // submachine gun (vr540: 6.34; 0.3 mm; its trigger pad 0.5 mm off)
    {{26.0f, -12.9f, 2.5f}, -4.0f, 24.0f},     // stun gun (vr540: 6.60; 0.3 mm)
    {{18.3f, -5.9f, 2.5f}, -20.0f, 32.0f},     // knife (the right-handed place; vr537: 0.3 mm)
    {{10.6f, -3.5f, 4.6f}, -24.0f, 40.0f},     // steel pipe (the right-handed place; vr537: clear)
    {{4.7f, -1.5f, 0.0f}, 0.0f, 40.0f},        // katana (vr544, round the right-handed place: 6.20; 0.3 mm)
    {{6.1f, -2.0f, 4.0f}, 12.0f, -36.0f},      // maul (vr557, round the right-handed place: 6.00; 0.4 mm, forearm 20)
    {{10.1f, -4.2f, 5.6f}, -28.0f, 44.0f},     // beam saber (vr540: 6.38; 0.3 mm; the right-handed place 15 mm in)
    {{16.6f, -1.5f, -4.3f}, 16.0f, -160.0f},   // flamethrower (vr540: 6.59; 0.6 mm; the right-handed place 9.3 in)
};
GripPlace fit_second_left[kProfiles] = {
    {{-7.1f, -12.2f, -3.5f}, -32.0f, -28.0f},  // handgun (vr542, round the right-handed place: 5.00; 0.2 mm)
    {{0.0f, 5.0f, 0.0f}, -16.0f, 0.0f},        // shotgun (the right-handed place; vr537: clear)
    {{5.0f, -14.0f, 5.0f}, -8.0f, 8.0f},       // submachine gun (the right-handed place; vr543: 0.5 mm, cupped)
    {}, {},                                    // stun gun, knife (one hand)
    {{4.0f, -8.7f, 3.0f}, 8.0f, -32.0f},       // steel pipe (the right-handed place; vr537: clear)
    {{-15.0f, -15.5f, -1.0f}, -8.0f, -12.0f},  // katana (vr546: 6.11; 0.3 mm, round the handle's end)
    {{-4.0f, -15.2f, 2.0f}, 12.0f, -8.0f},     // maul (vr557: 5.42; 0.4 mm, over 10 off her main hand)
    {{2.6f, 17.8f, 0.2f}, 0.0f, 36.0f},        // beam saber (vr540: 7.00; 0.5 mm, 0.2 off her main hand)
    {{-2.8f, -7.9f, 4.9f}, 0.0f, 28.0f},       // flamethrower (vr540: 5.78; 0.5 mm)
};
// Testing: SH3_GRIP_FIT=0 no fitted places (0.1.7's), 1 the search (fit_step) on each weapon drawn, its places taken.
int grip_fit_mode() {
    static int m = -2;
    if (m == -2) m = std::getenv("SH3_GRIP_FIT") ? std::atoi(std::getenv("SH3_GRIP_FIT")) : -1;
    return m;
}
// Each gun's trigger: its blade's middle line and half its thickness (model units, the held bone), from the mesh dumps
// (tools/grip_fit.py points, scratchpad pictures an146): the handgun's from (70.3, 21.5) under the frame down to its
// curled tip at (76.1, 11.1), z -9.8 to -14.9; the shotgun's (bone 1, y down) a thin blade at x -232 from y 23.5 to its
// tip at 31.3; the submachine gun's from (65.4, 24.5) to its tip at (70.9, 11.4). The flamethrower's model has no
// trigger: its trigger finger closes on the grip's front with the others, as the stun gun's.
// Session 31 (owner message 151: "make sure the fingers can actually be seen properly pulling the triggers"): each
// trigger also turns about its pin as it is pulled, its blade swinging back towards the grip as a real one does
// (sh3vr_parts.cpp draws its own faces turned; the finger's pad stays on it, body_trigger_pull_curls). The pin: just
// above the blade's front top corner, inside the frame (from the dumps: the handgun's blade's top runs from (68.6,
// 21.7) to (74.1, 23.6), the submachine gun's from (63.8, 25.2) to (67.1, 24.9), the shotgun's (y down) from (-230.8,
// 20.7) to (-226.8, 20.8)), so the blade's top turns up into the frame and no gap opens under it. The turn all the way:
// the pad's travel about 6 to 7 mm (a pistol trigger's travel), the tip well clear of the guard.
struct TriggerBlade {
    bool on;
    Vec3 a, b;
    float r;
    Vec3 pin;    // model units, the held bone
    float turn;  // degrees, pulled all the way
};
const TriggerBlade kTriggers[kProfiles] = {
    {true, {70.3f, 21.5f, -12.8f}, {72.2f, 13.5f, -12.8f}, 1.6f, {74.2f, 24.0f, -12.3f}, 24.0f},
    {true, {-231.7f, 23.5f, 0.0f}, {-232.0f, 30.5f, 0.0f}, 1.0f, {-227.2f, 20.3f, 0.0f}, 22.0f},
    {true, {65.4f, 24.5f, -10.8f}, {67.2f, 15.0f, -10.8f}, 1.5f, {67.1f, 25.4f, -10.7f}, 26.0f},
    {false, {}, {}, 0, {}, 0}, {false, {}, {}, 0, {}, 0}, {false, {}, {}, 0, {}, 0}, {false, {}, {}, 0, {}, 0},
    {false, {}, {}, 0, {}, 0}, {false, {}, {}, 0, {}, 0}, {false, {}, {}, 0, {}, 0},
};
// 0.1.10 (session 38, vr530: her trigger finger as drawn against the trigger as drawn, hand_skin_trigger): the
// shotgun's index, its pad laid on the blade by the finger's model (8.5 mm of skin round its line), came 0.7 mm into the
// blade at rest and 1.4 mm all the way pulled, and as far into the guard behind it (the handgun's 0.9 to 1.9 mm off,
// the submachine gun's 0.5 to 1.8). Her drawn pad is fuller than the model where it meets that thin blade side-on: on
// the shotgun the pad stops 2 mm sooner (metres, added to the blade's thickness where the finger closes on it,
// pad_on_blade; the blade itself, drawn and turned, is the same).
const float kTriggerPad[kProfiles] = {0.0f, 0.002f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
// The trigger of profile p turned by `pull` (0 at rest .. 1 all the way) about its pin: the turn (model units), its
// axis such that the blade's tip swings back, against the weapon's forward (`fwd`, model: the frame's).
Quat trigger_turn(int p, Vec3 fwd, float pull) {
    const TriggerBlade& k = kTriggers[p];
    const Vec3 n = cross(k.b - k.pin, Vec3{} - fwd);
    if (dot(n, n) < 1e-8f) return Quat{};
    return quat_axis(normalized(n), std::clamp(pull, 0.0f, 1.0f) * k.turn * kPi / 180.0f);
}
Vec3 trigger_moved(int p, Quat q, Vec3 x) { return kTriggers[p].pin + rotate(q, x - kTriggers[p].pin); }
// The main hand's fitted place on the weapon of `profile` (held in hand h, the main hand): from the game's own place
// once it is read (before that, the first frames after the draw, 0.1.7's), mirrored for a left-handed player
// (main_native); none where the game's own rigid hand is drawn (SH3_GRIP_HAND=1, the rig's comparisons).
// 0.1.8 (vr400; owner: "regress nothing", "make sure beam saber is still alligned and looks right"): a weapon held in
// the fist (the knife, stun gun, steel pipe, katana, maul, beam saber) keeps 0.1.7's place ALONG its handle in the main
// hand; only its place round it (across the handle) and its turn are the fit's. Along a plain handle every place grips
// alike, so the fit's choice there was chance (vr400: the maul's 4.5 cm and the beam saber's 4.9 cm towards the pommel,
// the pipe's and katana's 4.8 cm towards the head, the stun gun's 6.7 cm up to its prongs); it moved the weapon in the
// hand by as much (its reach, the beam's, the prongs') and left the maul's other hand no room on its shaft (its fingers
// past the end, vr400's g7-maul-2close). 0.1.7's place is the game's own fist's, with room for both hands. (The search,
// SH3_GRIP_FIT=1, works on the full place.)
// 0.1.10 (owner, after 169: "WHY ... IS THE GUN STILL CLIPPING THROUGH THE HAND"): the main hand moved out of the
// weapon by its palm as drawn, by weapon, for a right-handed player [0] and a left-handed one [1] (the place's shift
// units; testing only, SH3_HEEL_SETTLE=1: settle_heel finds it once, main_fit adds it to the fitted place; zero in play,
// the places being fitted with her palm as drawn).
Vec3 heel_extra[kProfiles][2];
int heel_seen[kProfiles][2];
bool heel_done[kProfiles][2];
const GripPlace* main_fit(int profile, int h) {
    (void)h;
    if (profile < 0 || profile >= kProfiles || grip_fit_mode() == 0) return nullptr;
    if (!main_native(profile) || !finger_drawn(profile)) return nullptr;
    // (session 38, vr537: a left-handed player's own places, fit_main_left)
    GripPlace* const table = opt.left_handed ? fit_main_left : fit_main;
    const GripPlace* base = &table[profile];
    // (0.1.10: the search too works on the place played, along the handle 0.1.7's: fit_candidate)
    if (kHolds[profile].fist && kShapes[profile].n > 0 && grip_fit_mode() == 1) {
        const Cap& c0 = kShapes[profile].c[0];
        const Vec3 u = normalized(c0.b - c0.a);
        table[profile].shift = table[profile].shift - u * dot(table[profile].shift, u);
        return base;
    }
    if (kHolds[profile].fist && kShapes[profile].n > 0 && grip_fit_mode() != 1) {
        static GripPlace kept[kProfiles];
        const Cap& c0 = kShapes[profile].c[0];
        const Vec3 u = normalized(c0.b - c0.a);
        kept[profile] = table[profile];
        kept[profile].shift = table[profile].shift - u * dot(table[profile].shift, u);
        base = &kept[profile];
    }
    if (grip_fit_mode() == 1) return base;  // (the search works on the fitted places themselves)
    static GripPlace adj[kProfiles];
    adj[profile] = *base;
    adj[profile].shift = base->shift + heel_extra[profile][opt.left_handed ? 1 : 0];
    return &adj[profile];
}
// The other hand's fitted place on the second grip (her finger hand there; mirrored for a left-handed player by
// second_natural and second_hand_pose; the game's own left hand keeps its place where it is drawn, SH3_GRIP_HAND=1).
const GripPlace* second_fit(int profile) {
    if (profile < 0 || profile >= kProfiles || grip_fit_mode() == 0) return nullptr;
    if (native_of(profile, kLeft) && !opt.left_handed && main_hand() == kRight && !finger_drawn(profile)) return nullptr;
    // the handgun's other hand wraps the main fist: its place was fitted with the main hand at its own (the game's place
    // read); before that, none
    if (native_group_of(profile, kRight) >= 0 && !main_native(profile)) return nullptr;
    // (0.1.8, vr395: a two-handed melee weapon's other hand is its main hand's mirrored, second_hand_pose; 0.1.10,
    // session 38: then moved and turned by its fitted place, fit_second, from that mirrored pose)
    return opt.left_handed ? &fit_second_left[profile] : &fit_second[profile];  // (session 38: fit_second_left)
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
    // (0.1.8: across the measured hold's plane, the weapon's middle: the fitted main hand may sit off it)
    return f.mirror ? p - f.right * (2.0f * dot(p - f.hold->palm, f.right)) : p;
}
Vec3 frame_point(const WeaponFrame& f, Vec3 model) {
    return f.base + rotate(f.turn, frame_local(f, model - f.palm) * f.scale);
}
Vec3 frame_dir(const WeaponFrame& f, Vec3 model) { return normalized(rotate(f.turn, frame_local(f, normalized(model)))); }
// The second grip's point (model units): the fist's centre of the game's own left hand where it holds the weapon (the
// shotgun's pump, ...), else the hold's (mirrored for a left-handed player); 0.1.7: moved by the player's own two-hand
// grip spot (second_offset, "Two-hand grip spot").
Vec3 second_offset(const WeaponFrame& f);
// 0.1.8: moved by its fitted place (second_fit; the fit search's candidates by `trial`).
Vec3 second_natural(const WeaponFrame& f, const GripPlace* trial = nullptr) {
    const NativeGrip* ng = native_of(f.profile, kLeft);
    const Vec3 nudge = second_nudge(f.profile);
    // (0.1.8: her finger hand on the second grip takes the hold's point and its fitted place, every time: the game's own
    // left hand, read only in some of its poses, moved it between the two from one draw to the next)
    if (ng && !f.mirror && main_hand() == kRight && !finger_drawn(f.profile)) return ng->palm + nudge;
    const GripPlace* g = trial ? trial : second_fit(f.profile);
    return mirrored_point(f, f.hold->second_at + nudge + (g ? g->shift : Vec3{}));
}
Vec3 second_point(const WeaponFrame& f);  // (0.1.8: below, after the holds where the hand took hold)

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
// 3 (0.1.8): the guns' hands fitted to them anew (owner message 146), so a 0.1.7 file's gun moves and turns and every
// two-hand grip spot (made against 0.1.7's hand places) are put back once; its sizes, aims and guide dots are kept, and
// the melee weapons' moves and turns (their hands as before).
constexpr int kCalibVersion = 3;
struct Calib {
    float scale = 1.0f;
    float move_cm[3]{};      // right, up, forward
    float turn_deg[3]{};     // muzzle up (about its right), muzzle left (about its up), roll right (about its forward)
    float aim_pitch = 0, aim_yaw = 0;  // degrees: the shot's line from the barrel
    bool guide_dot = true;
    float second_cm[3]{};    // 0.1.7 (owner message 124): the two-hand grip moved from its natural spot (right, up, forward)
};
Calib calib[kProfiles];  // by profile
bool calib_loaded = false;
const Calib& calib_of(int w) {
    static const Calib kNone{};
    const int p = profile_of(w);
    return p >= 0 ? calib[p] : kNone;
}

// 0.1.7 (owner message 124: "two hand grip calibration which makes it so you can adjust where it locks to as needed"):
// the player's own two-hand grip spot, per weapon, as moved from its natural one in the weapon's own axes (model units;
// "right" the weapon's right as a right-handed player holds it, mirrored for a left-handed one).
constexpr float kSecondMostCm[3] = {10.0f, 10.0f, 30.0f};  // right, up, forward: each way
Vec3 second_offset(const WeaponFrame& f) {
    if (f.profile < 0 || f.profile >= kProfiles) return Vec3{};
    const float* cm = calib[f.profile].second_cm;
    const float u = 0.01f * kUnitsPerMetre;
    return (f.right * (f.mirror ? -cm[0] : cm[0]) + f.up * cm[1] + f.fwd * cm[2]) * u;
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
                    "; SecondGrip: cm your other hand's grip lies from its natural spot (right, up, forward); the VR\n"
                    "; menu's \"Two-hand grip spot\" sets it where you take hold, or puts every weapon's back.\n"
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
        if (kHolds[s].second)
            std::fprintf(f, "SecondGripRightCm=%g\nSecondGripUpCm=%g\nSecondGripForwardCm=%g\n", c.second_cm[0],
                         c.second_cm[1], c.second_cm[2]);
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
        else if (k == "SecondGripRightCm") c.second_cm[0] = std::clamp(v, -kSecondMostCm[0], kSecondMostCm[0]);
        else if (k == "SecondGripUpCm") c.second_cm[1] = std::clamp(v, -kSecondMostCm[1], kSecondMostCm[1]);
        else if (k == "SecondGripForwardCm") c.second_cm[2] = std::clamp(v, -kSecondMostCm[2], kSecondMostCm[2]);
    }
    std::fclose(f);
    if (version == 2) {
        int moved = 0, spots = 0;
        for (int s = 0; s < kProfiles; s++) {
            Calib& c = calib[s];
            // The guns and the stun gun (0.1.7's finger hand, now at their fitted places); the knife, steel pipe,
            // katana, maul and beam saber keep 0.1.7's place along the handle (main_fit), so their moves stay.
            // (Session 33, vr408: this tested kFingerHandOn, which 0.1.8 turned on for every weapon, so the melee
            // weapons' moves were put back too, against the log line and the notes.)
            if (kWeapons[s].kind == kGun || kWeapons[s].kind == kStun) {
                for (int k = 0; k < 3; k++) {
                    moved += c.move_cm[k] != 0.0f || c.turn_deg[k] != 0.0f;
                    c.move_cm[k] = c.turn_deg[k] = 0.0f;
                }
            }
            for (int k = 0; k < 3; k++) {
                spots += c.second_cm[k] != 0.0f;
                c.second_cm[k] = 0.0f;
            }
        }
        write_default_calibration();
        rt_log("VR: sh3quest_weapons.ini was 0.1.7's: the guns' and stun gun's moves and turns (%d set) and the two-hand "
               "grip spots (%d set) put back for 0.1.8's fitted hands; sizes, aims, guide dots and the melee weapons' "
               "kept", moved, spots);
        return;
    }
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
// 0.1.8: the last weapon draw moved to the hand: its held palette slot (-1: not moved) and which of the weapon's draws
// it is (its signature, kWeapons' nv/np); the weapon placed for how many frames running (its own shape: once settled).
int mesh_slot = -1, mesh_sig = -1;
int placed_frames = 0, placed_weapon = -1;
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
    // 0.1.7 (owner 140): the most this swing reached (the line telling why a fast swing gave no game attack)
    float most_speed = 0, most_fast = 0, most_net = 0;
    bool was_armed = false;
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
    int hover = -1;  // 0.1.7: what the hand could use as it closed (hand_hover's), for the log when nothing was used
} hand_action;
int hover_event[2] = {-1, -1};  // what each reaching hand can use now (hand_hover), -1 nothing
bool hover_door[2]{};            // 0.1.10: ... a door (sh3.c sh3_event_is_door)
// 0.1.10 (owner 170: "grabbing the knob/handle/whatever and then pulling it open, which would then start the load into
// the next section ... for all doors"; 171: on by default), "Open doors by hand": a hand closing on a door takes its
// handle and holds it; moved 10 cm level while held (pulled toward her, pushed away or slid aside), the door is used
// where the hand took it, as a closing hand used it before (the game's own door: its sound, its fade and the next
// room's load). Let go first, held 6 s, or the hand carried 50 cm from the handle (walked away): nothing opens.
struct DoorHold {
    int hand = -1, event = -1;
    Vec3 world;          // the palm where it took the handle (world): the game's check runs there
    Vec3 start, toward;  // the controller there, and level from it toward her head (tracking)
    float held = 0.0f, buzz_in = 0.0f;
} door_hold;
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

// 0.1.8: a fitted place's turn (model space): about the weapon's right, then about the grip's own line `rod`.
Quat place_turn(const WeaponFrame& f, Vec3 rod, const GripPlace* g) {
    if (!g || (g->pitch == 0.0f && g->roll == 0.0f)) return Quat{};
    return quat_axis(f.right, rad(g->pitch)) * quat_axis(normalized(rod), rad(g->roll));
}
// 0.1.8: a right hand's pose (tracking) mirrored across the weapon's middle (the measured hold's plane, mirrored_point's)
// into the left hand's: OpenXR's two grip poses are mirror images of each other with their x the other way.
Pose mirrored_pose(const WeaponFrame& f, const Pose& r) {
    const Vec3 n = frame_dir(f, f.right), c = frame_point(f, f.hold->palm);
    auto M = [&](Vec3 v) { return v - n * (2.0f * dot(v, n)); };
    const Vec3 x = M(rotate(r.orientation, Vec3{1, 0, 0})) * -1.0f, y = M(rotate(r.orientation, Vec3{0, 1, 0})),
               z = M(rotate(r.orientation, Vec3{0, 0, 1}));
    return {r.position - n * (2.0f * dot(r.position - c, n)), quat_of_axes(x, y, z)};
}
// The main hand's pose (tracking) round the weapon's grip at place g (null: 0.1.7's): the game's own hand's bone moved
// and turned about its fist's centre (a left-handed player's: the right hand's there, mirrored), else the measured
// hold's fist.
bool main_hand_pose(const WeaponFrame& f, int h, const GripPlace* g, Pose& out) {
    const Hold& hd = *f.hold;
    const Quat R = place_turn(f, hd.rod, g);
    const Vec3 shift = g ? g->shift : Vec3{};
    if (const NativeGrip* ng = f.mirror ? main_native(f.profile) : native_of(f.profile, h)) {
        Vec3 axes[3];
        for (int i = 0; i < 3; i++) axes[i] = frame_dir(f, rotate(R, ng->ax[i]));
        const Vec3 o = frame_point(f, ng->palm + rotate(R, ng->o - ng->palm) + shift);
        if (!f.mirror) return ik_pose_for_hand_bone(h, axes, o, out);
        Pose r;
        if (!ik_pose_for_hand_bone(kRight, axes, o, r)) return false;
        out = mirrored_pose(f, r);
        return true;
    }
    const float mx = h == kLeft ? -1.0f : 1.0f;
    out = hand_round(h, frame_point(f, hd.palm + shift), frame_dir(f, rotate(R, hd.rod)),
                     frame_dir(f, rotate(R, f.right * -mx)));
    return true;
}
Vec3 model_dir(const WeaponFrame& f, Vec3 t);  // (below)
// The other hand's pose (tracking) on the second grip at its natural spot, at place g (the fitted one; the search's
// candidates), her finger hand: the hold's second rod and palm turned by the place, mirrored for a left-handed player.
Pose second_hand_pose(const WeaponFrame& f, int h, const GripPlace* g) {
    const Hold& hd = *f.hold;
    // 0.1.8 (owner 147: "every second hand wrapped properly"; vr395's pictures): the steel pipe's, katana's, maul's and
    // beam saber's other hands, fitted on their own, lay under the handle palm up with a third of the palm on it and
    // their fingers closing in the air below it, while each main hand's fist (fitted, its fingers closed round the
    // handle) was right. Two hands on one handle hold it alike, each the mirror image of the other across the plane of
    // the handle and the blade's back (the right hand's palm on the handle's right, the left's on its left, both thumbs
    // over the top): the other hand is the main hand's grip, as drawn, mirrored across that plane through the handle's
    // own line (its first capsule, kShapes) and slid along that line to where the other hand takes hold (the second
    // grip's spot, the player's Two-hand grip spot with it), so it closes round the handle as the main hand does.
    // vr400: slid along the weapon's forward instead (the katana's 6 degrees off its handle's line) and by the hold's
    // spacing from a main hand the fit had moved, the maul's other hand closed on air past its shaft's end: the slide is
    // now along the handle itself, the hand kept on it (its centre 3.5 cm in from either end, so its little finger stays
    // on the pommel) and a fist's width (8.5 cm) from the main hand, on the main hand's other side where that side has
    // no room. SH3_GRIP_FIT=0 (the rig): 0.1.7's.
    if (hd.fist && hd.second && f.shape && f.shape->n > 0 && grip_fit_mode_on()) {
        const int mh = main_hand();
        Pose m;
        if (h != mh && main_hand_pose(f, mh, main_fit(f.profile, mh), m)) {
            const Cap& c0 = f.shape->c[0];
            const float len = length(c0.b - c0.a);
            const Vec3 u = (c0.b - c0.a) * (1.0f / std::max(len, 1e-4f));
            const float per_m = 1.0f / std::max(f.scale, 1e-6f);  // model units a metre
            const float end = 0.035f * per_m, apart = 0.085f * per_m;
            const float dm = dot(f.palm - c0.a, u);  // the main fist's centre along the handle
            static const GripPlace kNoPlace{};  // (its slide along the handle: the hold's spot, not the fitted place's)
            float d2 = std::clamp(dot(second_natural(f, &kNoPlace) + second_offset(f) - c0.a, u), end,
                                  std::max(end, len - end));
            if (std::fabs(d2 - dm) < apart) {
                const float side = d2 < dm ? -1.0f : 1.0f;
                d2 = dm + side * apart;
                if (d2 < end || d2 > len - end) d2 = dm - side * apart;
                d2 = std::clamp(d2, end, std::max(end, len - end));
            }
            // the mirror's plane: through the handle's line, square to the weapon's right made square to that line
            const Vec3 r_model = normalized(f.right - u * dot(f.right, u));
            const Vec3 n = frame_dir(f, r_model), c = frame_point(f, c0.a + u * dm);
            auto M = [&](Vec3 v) { return v - n * (2.0f * dot(v, n)); };
            const Vec3 x = M(rotate(m.orientation, Vec3{1, 0, 0})) * -1.0f, y = M(rotate(m.orientation, Vec3{0, 1, 0})),
                       z = M(rotate(m.orientation, Vec3{0, 0, 1}));
            const Vec3 slide = frame_point(f, u * (d2 - dm)) - frame_point(f, Vec3{});
            Pose out{m.position - n * (2.0f * dot(m.position - c, n)) + slide, quat_of_axes(x, y, z)};
            // 0.1.10 (session 38; vr517: the beam saber's other hand 5.8 mm into its handle as drawn, the steel pipe's
            // 0.2: her left hand as drawn is not her right's mirrored, 476 vertices to 442): then turned about its
            // fist's centre and moved by its fitted place (fit_second, the search's candidates by g), as the main hand's
            // place turns and moves it: about the weapon's right, then the handle's line; a left-handed player's
            // mirrored (the turn about the line the other way, the move across the weapon's middle)
            if (g && (g->pitch != 0.0f || g->roll != 0.0f || g->shift.x != 0.0f || g->shift.y != 0.0f ||
                      g->shift.z != 0.0f)) {
                GripPlace gm = *g;
                if (f.mirror) gm.roll = -gm.roll;
                const Quat R = place_turn(f, u, &gm);
                auto turn = [&](Vec3 v) { return normalized(frame_dir(f, rotate(R, model_dir(f, v)))); };
                const Vec3 x2 = turn(rotate(out.orientation, Vec3{1, 0, 0})),
                           y2 = turn(rotate(out.orientation, Vec3{0, 1, 0})),
                           z2 = turn(rotate(out.orientation, Vec3{0, 0, 1}));
                out.orientation = quat_of_axes(x2, y2, z2);
                out.position = out.position + (frame_point(f, mirrored_dir(f, g->shift)) - frame_point(f, Vec3{}));
            }
            return out;
        }
    }
    const Quat R = place_turn(f, hd.second_rod, g);
    Vec3 at = second_natural(f, g) + second_offset(f);
    const float pump = reload_pump_back();
    if (pump > 0) at = at - f.fwd * (pump * kUnitsPerMetre);
    return hand_round(h, frame_point(f, at), frame_dir(f, mirrored_dir(f, rotate(R, hd.second_rod))),
                      frame_dir(f, mirrored_dir(f, rotate(R, hd.second_palm))));
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
    // 0.1.8: the main hand's fitted place (main_fit): the controller's grip point is its fist's centre there, so the
    // weapon moves against the hand by the place's shift. Its turn turns the hand on the weapon about that centre
    // (main_hand_pose), never the weapon against the controller (session 31: the fits turn a gun's main hand up to 32
    // degrees about its grip, which turning the gun instead would have swung its barrel, and so every shot, off where the
    // controller points): each weapon keeps 0.1.7's way on the controller, its aim and the melee weapons' feel.
    const GripPlace* g = main_fit(p, mh);
    if (g) f.palm = f.palm + g->shift;
    // (a left-handed player: main_native, the right hand's grip mirrored across the weapon's middle: its fist's centre
    // mirrored; a fist hold's weapon axes in the right grip's own space as the right hand holds it, mirrored into the
    // left grip's, x the other way and the weapon's right the other way again, then turned with the left controller)
    if (const NativeGrip* ng = f.mirror ? main_native(p) : native_of(p, mh)) {
        f.palm = mirrored_point(f, ng->palm + (g ? g->shift : Vec3{}));
        Vec3 axes[3], origin;
        const Pose at = f.mirror ? Pose{} : Pose{body_shown[mh].position, m.grip.orientation};
        if (hd->fist && ik_hand_bone_for_pose(f.mirror ? kRight : mh, at, axes, origin)) {
            Vec3 col[3];
            const Vec3 model_axes[3] = {f.right, f.up, f.fwd * -1.0f};
            for (int k = 0; k < 3; k++) {
                col[k] = {};
                for (int i = 0; i < 3; i++) col[k] = col[k] + axes[i] * dot(model_axes[k], ng->ax[i]);
                if (f.mirror) {
                    col[k].x = -col[k].x;
                    if (k == 0) col[k] = col[k] * -1.0f;
                    col[k] = rotate(m.grip.orientation, col[k]);
                }
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

// ---------------------------------------------------------------- the other hand wrapped where it takes hold (0.1.8)
//
// Owner message 146 ("even when trying the mode where you can grip anywhere on the gun, it doesnt really look good since
// it isnt exactly wrapping the hand on the gun naturally. theres games in vr that have different gun grab points that
// keep it natural and dynamic ... the grip gun anywhere mode should be a seperate toggle"): 0.1.7's Adjust only moved
// the second hand's natural pose (its fist turned as on the natural grip) to the spot. Now a hand taking hold away from
// the natural grip (Hold weapons anywhere, or a Two-hand grip spot moved there) takes the part of the weapon there as
// Auto Hand's grab solver does: the nearest part thick enough to hold (a capsule of the weapon's shape, 1 cm round or
// more, not the main hand's grip), its fist along that part (the way nearest the hand's own), on the side the hand is,
// her palm laid on it (moved in until her palm's skin meets it, never a knuckle or the palm in it), and her fingers
// closing round it until they meet it (sh3vr_body.cpp, as on every grip).
struct SurfaceHold {
    bool on = false;
    int profile = -1;
    Vec3 at, rod, palm;  // model: her fist's centre, its rod (little finger to thumb), the way her palm faces
    float key[4]{};      // the spot it was made for (the moved spot's: its cm, left-handed)
};
SurfaceHold anywhere_hold;  // taken by Hold weapons anywhere, for the hold
SurfaceHold moved_hold;     // the Two-hand grip spot's, when moved off the natural grip
struct AnywhereAsk {
    bool on = false;
    Vec3 spot, rod, palm;   // model: the controller's grip point and its rod and palm, when it took hold
} anywhere_ask;

// A tracking direction in the weapon's model (unit).
Vec3 model_dir(const WeaponFrame& f, Vec3 t) {
    const Vec3 l = rotate(conjugate(f.turn), t);
    return normalized(f.right * l.x + f.up * l.y - f.fwd * l.z);
}

// The part of the weapon nearest `spot` (model units) that a hand can take: its capsule, the point on its line, how far
// the spot is from its surface (units). -1: none (only the main hand's grip, or none 1 cm round).
int part_near(const WeaponFrame& f, Vec3 spot, Vec3& on_line, float& surface) {
    Cap caps[kCapMax];
    const int nc = shape_caps(f, caps);
    int best = -1;
    surface = 1e9f;
    for (int i = 0; i < nc; i++) {
        const Cap& c = caps[i];
        if (c.r < 5.0f || seg_seg(f.palm, f.palm, c.a, c.b) < 20.0f) continue;  // too thin, or the main hand's grip
        const Vec3 ab = c.b - c.a;
        const float L2 = dot(ab, ab);
        const float t = L2 > 1e-6f ? std::clamp(dot(spot - c.a, ab) / L2, 0.0f, 1.0f) : 0.0f;
        const Vec3 p = c.a + ab * t;
        const float d = length(spot - p) - c.r;
        if (d < surface) surface = d, best = i, on_line = p;
    }
    return best;
}

// Hand h's hold on the part nearest `spot` (within `reach` units of its surface), the fist's rod the way nearest
// `ref_rod` (model, unit), on the spot's side of it (else the side `ref_palm` faces from). Needs this frame's weapon
// placed (wf) and the main hand posed: her palm is laid on the part against both.
bool surface_hold_at(const WeaponFrame& f, int h, Vec3 spot, Vec3 ref_rod, Vec3 ref_palm, float reach, SurfaceHold& out) {
    Vec3 p;
    float surface = 0;
    const int ci = part_near(f, spot, p, surface);
    if (ci < 0 || surface > reach) return false;
    Cap caps[kCapMax];
    shape_caps(f, caps);
    const Cap& c = caps[ci];
    const Vec3 ab = c.b - c.a;
    const float L = length(ab);
    const Vec3 axis = L > 1e-3f ? ab * (1.0f / L) : f.fwd;
    // (her hand kept on the part: its middle at least 3 cm from either end, or the part's middle)
    const float margin = std::min(15.0f, L * 0.5f);
    p = c.a + axis * std::clamp(dot(spot - c.a, axis), margin, std::max(margin, L - margin));
    Vec3 n = spot - p;
    n = n - axis * dot(n, axis);
    if (length(n) < 0.5f) n = ref_palm * -1.0f - axis * dot(ref_palm * -1.0f, axis);
    if (length(n) < 1e-3f) n = cross(axis, f.right);
    n = normalized(n);
    out.rod = dot(axis, ref_rod) < 0 ? axis * -1.0f : axis;
    out.palm = n * -1.0f;
    out.profile = f.profile;
    // Her palm laid on it: from 6 cm out, in 2 mm steps, until her palm's skin is within 3 mm of the weapon (or the
    // step before a knuckle or her palm would go in).
    const float step = 0.002f / f.scale;
    float last_ok = -1e9f;
    for (float D = c.r + 0.06f / f.scale; D >= 0.0f; D -= step) {
        const Vec3 at = p + n * D;
        const Pose pose = hand_round(h, frame_point(f, at), frame_dir(f, out.rod), frame_dir(f, out.palm));
        float palm_clear = 0;
        bool inside = false;
        body_palm_touch(h, pose, palm_clear, inside);
        if (inside) break;
        last_ok = D;
        if (palm_clear <= 0.003f) break;
    }
    if (last_ok < -1e8f) return false;
    out.at = p + n * last_ok;
    out.on = true;
    return true;
}

// The other hand's hold where it took hold, for the weapon of frame f: taken anywhere, else at its moved Two-hand grip
// spot (resolve_moved_hold); null: the natural grip.
const SurfaceHold* other_surface(const WeaponFrame& f) {
    if (anywhere_hold.on && anywhere_hold.profile == f.profile) return &anywhere_hold;
    if (moved_hold.on && moved_hold.profile == f.profile) return &moved_hold;
    return nullptr;
}
Vec3 second_point(const WeaponFrame& f) {
    if (const SurfaceHold* s = other_surface(f)) return s->at;
    return second_natural(f) + second_offset(f);
}

// 0.1.8: the Two-hand grip spot moved 2 cm or more off the natural grip: the other hand wraps the part there (once a
// spot, a weapon, a hand: kept until any of them changes). Nearer, the natural grip's pose moved there (0.1.7's way).
// The game's own left hand (the steel pipe's, the maul's) keeps 0.1.7's way.
void resolve_moved_hold(const WeaponFrame& f, int h) {
    if (!f.placed || !f.hold->second || f.profile < 0) return;
    const float* cm = calib[f.profile].second_cm;
    const float key[4] = {cm[0], cm[1], cm[2], (f.mirror ? 1.0f : 0.0f) + (her_hands[h].valid ? 2.0f : 0.0f) +
                                                   (shape_ready(f.profile) ? 4.0f : 0.0f)};  // (again on its own shape)
    if (moved_hold.profile == f.profile && std::memcmp(moved_hold.key, key, sizeof key) == 0) return;
    moved_hold = SurfaceHold{};
    moved_hold.profile = f.profile;
    std::memcpy(moved_hold.key, key, sizeof key);
    if (std::sqrt(cm[0] * cm[0] + cm[1] * cm[1] + cm[2] * cm[2]) < 2.0f || grip_fit_mode() == 0) return;
    if (native_of(f.profile, kLeft) && !f.mirror && main_hand() == kRight && !finger_drawn(f.profile)) return;
    const Hold& hd = *f.hold;
    SurfaceHold s;
    if (surface_hold_at(f, h, second_natural(f) + second_offset(f), mirrored_dir(f, hd.second_rod),
                        mirrored_dir(f, hd.second_palm), 0.08f / f.scale, s)) {
        std::memcpy(s.key, key, sizeof key);
        moved_hold = s;
        rt_log("VR: frame %d: %s's moved two-hand grip spot (%.1f %.1f %.1f cm): the %s hand wraps its part there, its "
               "fist at %.1f %.1f %.1f along %.2f %.2f %.2f, its palm toward %.2f %.2f %.2f (model)", rt_frame,
               weapon_name(game.weapon), cm[0], cm[1], cm[2], h == kLeft ? "left" : "right", s.at.x, s.at.y, s.at.z,
               s.rod.x, s.rod.y, s.rod.z, s.palm.x, s.palm.y, s.palm.z);
    } else
        rt_log("VR: frame %d: %s's moved two-hand grip spot (%.1f %.1f %.1f cm): no part of it within 8 cm to wrap; the "
               "natural grip's pose moved there", rt_frame, weapon_name(game.weapon), cm[0], cm[1], cm[2]);
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
    // 0.1.7 (owner messages 124, 130: "make two hand grip calibration PER GUN/weapon"): "Two-hand grip spot" on "Reset
    // the weapon in your hand": that weapon's own spot again (with none in the hand, nothing changes); on "Reset every
    // weapon": every weapon's. Either way the row goes back to "As saved", with a buzz and the use sound when done.
    if (opt.grip_spot == 2 || opt.grip_spot == 3) {
        const int p = in_hand && game.weapon > 0 ? profile_of(game.weapon) : -1;
        int reset = 0;
        if (opt.grip_spot == 3) {
            for (Calib& c : calib) c.second_cm[0] = c.second_cm[1] = c.second_cm[2] = 0.0f;
            reset = kProfiles;
        } else if (p >= 0) {
            calib[p].second_cm[0] = calib[p].second_cm[1] = calib[p].second_cm[2] = 0.0f;
            reset = 1;
        }
        if (reset) write_default_calibration();
        const bool all = opt.grip_spot == 3;
        opt.grip_spot = 0;
        const bool saved = save_options();
        if (reset && hands[mh].valid) {
            buzz(mh, 0.4f, 0.05f);
            hand_sound(kSoundUse, world_point(hands[mh].grip.position), 0);
        }
        rt_log("VR: frame %d: %s (sh3quest_weapons.ini %s%s)", rt_frame,
               all ? "every weapon's two-hand grip is back at its natural spot"
                   : reset ? "the weapon in the hand has its two-hand grip back at its natural spot"
                           : "no weapon in the hand: no two-hand grip reset",
               reset ? "written" : "unchanged", saved ? "" : "; sh3vr.ini could not be saved");
        if (!all && reset) rt_log("VR: the reset weapon: %s", weapon_name(game.weapon));
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
    // 0.1.7 (owner message 124): "Two-hand grip spot" on "Adjust": the other hand takes hold anywhere along the weapon
    // (within 10 cm of its line from the main hand to its far end), and where it took hold is that weapon's two-hand
    // grip from then on (sh3quest_weapons.ini).
    const bool adjusting = opt.grip_spot == 1 && wf_one_hand.placed && !o.on_foregrip;
    if (adjusting) {
        // The weapon's whole length: from its tip back past the main hand, or past its natural second grip where that
        // lies behind the main hand (the pipe, the katana, the maul: down the handle below it; vr302 never took hold
        // there), 10 cm more.
        const Vec3 tip = frame_point(wf_one_hand, wf_one_hand.hold->tip);
        const Vec3 nat = frame_point(wf_one_hand, second_natural(wf_one_hand));
        Vec3 back = wf_one_hand.base;
        if (length(nat - tip) > length(back - tip)) back = nat;
        back = back + normalized(back - tip) * 0.10f;
        away = seg_seg(body_shown[oh].position, body_shown[oh].position, back, tip), limit = 0.10f;
    } else if (wf_one_hand.placed && opt.grip_anywhere && !o.on_foregrip) {
        // 0.1.8 (owner message 146): Hold weapons anywhere. Within 6 cm of the natural grip (its fitted place): that grip;
        // else the part of the weapon nearest the hand, within 5 cm of its surface, wrapped where the hand is
        // (surface_hold_at, done in place_weapon this frame with the hand's own turn as it took hold).
        const WeaponFrame& f = wf_one_hand;
        const Vec3 nat = frame_point(f, second_natural(f) + second_offset(f));
        Vec3 on_line;
        float surface = 0;
        const Vec3 spot = model_of(f, body_shown[oh].position);
        const int part = part_near(f, spot, on_line, surface);
        anywhere_hold.on = false;
        if (length(body_shown[oh].position - nat) < 0.06f) away = length(body_shown[oh].position - nat), limit = 0.12f;
        else if (part >= 0 && surface * f.scale < 0.05f) {
            away = 0, limit = 1.0f;
            const float px = oh == kLeft ? 1.0f : -1.0f;
            anywhere_ask.on = true;
            anywhere_ask.spot = spot;
            anywhere_ask.rod = model_dir(f, rotate(o.grip.orientation, Vec3{0, 0, -1}));
            anywhere_ask.palm = model_dir(f, rotate(o.grip.orientation, Vec3{px, 0, 0}));
        } else away = 1.0f, limit = 0.0f;
    } else if (wf_one_hand.placed) {
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
        if (adjusting) {
            const WeaponFrame& f = wf_one_hand;
            const Vec3 d = model_of(f, body_shown[oh].position) - second_natural(f);
            const float u = 0.01f * kUnitsPerMetre;
            const float cm[3] = {dot(d, f.right) / u * (f.mirror ? -1.0f : 1.0f), dot(d, f.up) / u, dot(d, f.fwd) / u};
            Calib& c = calib[f.profile];
            for (int k = 0; k < 3; k++) c.second_cm[k] = std::clamp(cm[k], -kSecondMostCm[k], kSecondMostCm[k]);
            write_default_calibration();
            buzz(oh, 0.5f, 0.06f);
            hand_sound(kSoundUse, world_point(o.grip.position), 0);
            rt_log("VR: frame %d: %s's two-hand grip set where the %s hand took hold: %.1f cm right, %.1f up, %.1f "
                   "forward of its natural spot (saved in sh3quest_weapons.ini)", rt_frame, weapon_name(game.weapon),
                   oh == kLeft ? "left" : "right", c.second_cm[0], c.second_cm[1], c.second_cm[2]);
        }
    } else if (!on && o.on_foregrip)
        rt_log("VR: frame %d: the %s hand left %s's second grip: %.2f m out (over %.2f)", rt_frame,
               oh == kLeft ? "left" : "right", weapon_name(game.weapon), away, limit);
    o.on_foregrip = on;
}

// ---------------------------------------------------------------- the grips' fit (0.1.8: SH3_GRIP_FIT=1, the PC rig)
//
// Each weapon drawn: its main hand's place searched once the game's own place is read, then, with the other hand held on
// the second grip (and the main hand's fingers closed round its grip at its new place), the other hand's. Each place
// tried: her hand posed there (main_hand_pose, second_hand_pose) and scored by body_grip_fit. Three rounds a hand:
// turned and moved together (session 31: turned 8 degrees at a time, up to 32 each way about the grip's own line and 16
// about the weapon's right, and moved 5 units at a time, 25 each way along the weapon and up, 12.5 to its sides in
// 2.5s: session 30's vr380 showed her straight index running 27 degrees across a pistol grip, so with turns only round a
// place first found unturned, the guns' main hands found none), then round the best turned 4 degrees at a time (8 each
// way) and moved 2.5 units at a time (5 each way), again round the new best while it lies on that round's edge (up to 3
// times more), then moved 0.5 units at a time round that. The best is taken at once and logged as its table's line
// (fit_main, fit_second).
struct FitRun {
    int weapon = -1, stage = 0, round = 0, next = 0, wait = 0, tried = 0, climbs = 0;
    GripPlace from, best, ref;  // ref: the round's middle (the best when it began; vr371: not the best as it moves)
    float best_score = -1, start_score = -1;
    GripFit best_fit, start_fit;
    int why[10]{};  // (8, session 38: the other hand's wrist bent too far; 9, her forearm in the weapon)
    int in_by[5]{}, curl_by[5]{};  // why 3 and why 4, by finger (thumb, index, middle, ring, little)
    int curl_sub[4]{}, trig_sub[6]{}, off_sub[3]{};  // 0.1.10: why 4, 6 and 7 by GripFit::sub
    // 0.1.10 (owner 172, session 38): a main hand not round a handle (the guns') searched at every turn about the grip's
    // line (kWideRolls of 8 degrees, the whole circle) and wide_pitches() about the weapon's right, each turn's places
    // round the place where her palm as drawn just meets the weapon at that turn (fit_palm_meet)
    bool wide = false;
    Vec3 meet[45 * 11]{};       // each turn's meeting place's shift
    int meet_steps[45 * 11]{};  // units moved to it (+ out of the weapon, - in), 99 none found
    float turn_best[45 * 11]{};
    int roll_why[45][10]{};  // each turn about the grip's line: its places turned away, by why
} fit_run;
constexpr int kWideRolls = 45, kWidePitchesMost = 11;
// The wide search's turns about the weapon's right, 8 degrees apart round its place's: 5 (16 degrees each way); the
// rig's SH3_FIT_PITCHES asks up to 11 (40 each way; session 38, vr521: the handgun's hand, at the foot of its grip, could
// not move up it, the skin between her knuckles in the frame 2 mm up and her middle finger in the trigger guard 6 mm up,
// so it turns to rise there: its web up onto the back strap, its knuckles kept at the guard)
int wide_pitches() {
    static const int n = [] {
        const char* v = std::getenv("SH3_FIT_PITCHES");
        return std::clamp(v && *v ? std::atoi(v) : 5, 1, kWidePitchesMost) | 1;
    }();
    return n;
}
int fit_round_size(const FitRun& r) {
    return r.round == 0 ? (r.wide ? kWideRolls * wide_pitches() : 9 * 5) * 11 * 11 * 11 : r.round == 1 ? 25 * 125 : 9 * 9 * 9;
}
// Turn t of a wide search (session 38): about the grip's line from -176 to +176 degrees round ref's, 8 at a time, and
// about the weapon's right from -16 to +16 round ref's (wide_pitches); the turn about the line kept within -180 to 180.
GripPlace fit_wide_turn(const GripPlace& ref, int t) {
    GripPlace g = ref;
    float roll = ref.roll + (float)(t % kWideRolls - kWideRolls / 2) * 8.0f;
    while (roll >= 180.0f) roll -= 360.0f;
    while (roll < -180.0f) roll += 360.0f;
    g.roll = roll;
    g.pitch = ref.pitch + (float)(t / kWideRolls - wide_pitches() / 2) * 8.0f;
    return g;
}
// Session 37 (vr507's slices, vr514): at 0.1.8's places a gun's grip ran through the middle of her palm as drawn. Hand h
// (stage 0 the main hand, 1 the other) at place `from` moved the way her palm's back faces, 1 unit at a time (up to 60),
// out of the weapon until her palm as drawn is clear of it (0.5 mm), or, already clear by more than 2 mm, in toward it
// until it is within 2 mm (never into it): the place where her palm meets the weapon at from's turn. `steps`: units moved
// (+ out, - in), 99 when none was found (the place kept); `first`, `least`: her palm's nearest before and after (m).
void fit_pose_at(int stage, const GripPlace& g, Pose& pose) {
    if (stage == 0) main_hand_pose(wf, main_hand(), &g, pose);
    else pose = second_hand_pose(wf, off_hand(), &g);
}
GripPlace fit_palm_meet(int stage, int h, const GripPlace& from, int* steps, float* first_out, float* least_out) {
    Pose pose;
    Vec3 back_t;
    fit_pose_at(stage, from, pose);
    const float first = body_palm_skin_clear(h, pose, &back_t);
    const Vec3 hand0 = model_of(wf, pose.position);
    Vec3 back = normalized(model_dir(wf, back_t));
    // (the way the hand itself moves in the weapon's model for a shift along `back`: a shift moves the weapon against a
    // main hand held at the controller)
    GripPlace t = from;
    t.shift = from.shift + back;
    fit_pose_at(stage, t, pose);
    if (dot(model_of(wf, pose.position) - hand0, back) < 0) back = back * -1.0f;
    float least = first;
    int s = 0;
    bool found = first >= 0.0005f && first <= 0.002f;
    if (first < 0.0005f) {
        for (; least < 0.0005f && s < 60; s++) {
            t.shift = from.shift + back * (float)(s + 1);
            fit_pose_at(stage, t, pose);
            least = body_palm_skin_clear(h, pose);
        }
        found = least >= 0.0005f;
    } else if (first > 0.002f) {
        float prev = first;
        for (int k = 1; k <= 60; k++) {
            t.shift = from.shift - back * (float)k;
            fit_pose_at(stage, t, pose);
            const float c = body_palm_skin_clear(h, pose);
            if (c <= 0.002f) {
                if (c >= 0.0005f) s = -k, least = c;
                else s = -(k - 1), least = prev;  // (one unit too far: the step before, still clear)
                found = true;
                break;
            }
            prev = c;
        }
    }
    if (steps) *steps = found ? s : 99;
    if (first_out) *first_out = first;
    if (least_out) *least_out = found ? least : first;
    GripPlace out = from;
    if (found) out.shift = from.shift + back * (float)s;
    return out;
}
GripPlace fit_candidate(const WeaponFrame& f, const FitRun& r, int i) {
    const GripPlace ref = r.ref;
    GripPlace g = ref;
    auto move = [&](float x, float y, float z) { g.shift = ref.shift + f.fwd * x + f.up * y + f.right * z; };
    if (r.round == 0 && r.wide) {
        // (session 38: every turn about the grip's line, round the place where her palm meets the weapon at that turn)
        const int t = i / 1331, j = i % 1331;
        g = fit_wide_turn(ref, t);
        const Vec3 at = r.meet[t];
        g.shift = at + f.fwd * ((j / 121 - 5) * 5.0f) + f.up * ((j / 11 % 11 - 5) * 5.0f) + f.right * ((j % 11 - 5) * 2.5f);
    } else if (r.round == 0) {
        const int t = i / 1331, j = i % 1331;
        g.roll = ref.roll + (t % 9 - 4) * 8.0f;
        g.pitch = ref.pitch + (t / 9 - 2) * 8.0f;
        move((j / 121 - 5) * 5.0f, (j / 11 % 11 - 5) * 5.0f, (j % 11 - 5) * 2.5f);
    } else if (r.round == 1) {
        const int t = i / 125, j = i % 125;
        g.pitch = ref.pitch + (t / 5 - 2) * 4.0f;
        g.roll = ref.roll + (t % 5 - 2) * 4.0f;
        move((j / 25 - 2) * 2.5f, (j / 5 % 5 - 2) * 2.5f, (j % 5 - 2) * 2.5f);
    } else move((i / 81 - 4) * 0.5f, (i / 9 % 9 - 4) * 0.5f, (i % 9 - 4) * 0.5f);
    // 0.1.8 (owner 147: "every second hand wrapped properly"; vr388's pictures): the second hands and the melee
    // weapons' main hands kept on their grip's line. The search scores the fingers' and the palm's touch, and a fist
    // moved off the line with the grip at its finger tips scored about as well as one round it: the katana's other hand
    // 5.8 cm across its handle (its index straight along it), the submachine gun's up its receiver's side (its thumb
    // standing over the top, vr388 g2-smg). Each place keeps its fist's centre within 1.2 cm (0.1.10: 2.4, below) of
    // the grip's line through its first place (the hold's own, on that line), moving freely along it. The hands that cup
    // a gun's body from below, palm up (the shotgun's pump, the submachine gun's and the flamethrower's front: their
    // holds' second palm up), may rise 2 cm to it (0.1.7's palms lay up to 1.2 cm under it) and sink 0.8 cm (0.1.10:
    // 2.5, below), and turn 12 degrees about the line, so the palm stays under the gun. The guns' main hands stay free:
    // their place is the game's own hand's, moved for the trigger finger.
    // vr395: the stun gun's fist (its body a 6 cm box, the fist round it with its centre off the box's line) found no
    // place within 1.2 cm of that line: its main hand is left free, as vr388's was. The guns' other hands slid along
    // their grips' lines freely and the submachine gun's left its front block 4.7 cm back, beside the trigger guard, its
    // fingers standing up the receiver's side with nothing to close round: a gun's other hand moves at most 2 cm along
    // its grip from the hold's place (the pump, the front block, the grip's front).
    if (f.hold && (r.stage == 1 || (f.hold->fist && f.profile != 3)) && !(r.stage == 1 && r.wide)) {
        const Hold& h = *f.hold;
        const Vec3 rod_m = r.stage == 1 ? h.second_rod : h.rod;
        // (the rod in the weapon's frame as placed, from its parts along the hold's barrel or blade and its top; its
        // part across them is 0.11 at most, on the beam saber, and left out)
        Vec3 rod = f.fwd * dot(rod_m, h.fwd) + f.up * dot(rod_m, h.up);
        const float rl = length(rod);
        if (rl > 1e-4f) {
            rod = rod * (1.0f / rl);
            const GripPlace& o = r.from;
            const Vec3 d = g.shift - o.shift;
            const float run = dot(d, rod);
            const Vec3 along = rod * (r.stage == 1 && !h.fist ? std::clamp(run, -10.0f, 10.0f) : run);
            Vec3 across = d - rod * run;
            const bool cup = r.stage == 1 && dot(h.second_palm, h.up) > 0.9f;
            if (cup) {
                // (0.1.10, vr512: her palm as drawn is 1.8 cm thicker than the hand model's on its face, so a palm
                // under the gun sinks 2.5 cm at most, 12.5 units: the shotgun's pump and the submachine gun's front
                // found no place sinking 0.8)
                const float up = std::clamp(dot(across, f.up), -12.5f, 10.0f);
                const float side = std::clamp(dot(across, f.right), -6.0f, 6.0f);
                across = f.up * up + f.right * side;
                g.roll = std::clamp(g.roll, o.roll - 12.0f, o.roll + 12.0f);
            } else {
                // (0.1.10, vr512: with her palm as drawn 1.8 cm thicker on its face than the hand model's, a handle
                // against it lies further toward her fingers from the model's fist's centre: 2.4 cm off the line at
                // most, 12 units; her drawn palm against the handle, body_grip_fit, keeps the hand round it)
                const float al = length(across);
                if (al > 12.0f) across = across * (12.0f / al);
            }
            g.shift = o.shift + along + across;
        }
    }
    // 0.1.10: a main hand on a weapon held in the fist keeps 0.1.7's place along its handle as played (main_fit), so the
    // search tries only the places played: none along it
    if (r.stage == 0 && f.hold && f.hold->fist && f.profile >= 0 && kShapes[f.profile].n > 0) {
        const Cap& c0 = kShapes[f.profile].c[0];
        const Vec3 u = normalized(c0.b - c0.a);
        g.shift = g.shift - u * dot(g.shift, u);
    }
    return g;
}
const char* mesh_part_name(int part) {
    static const char* const kNames[9] = {"her palm", "the skin blended between bones", "her thumb", "her index finger",
                                          "her middle finger", "her ring finger", "her little finger",
                                          "her main hand as drawn", "not measured"};
    return kNames[part >= 0 && part < 8 ? part : 8];
}
void fit_log(const FitRun& r, const char* which, int h) {
    const GripPlace& b = r.best_score > 0 ? r.best : r.from;
    const GripFit& t = r.best_score > 0 ? r.best_fit : r.start_fit;
    rt_log("VR: grip fit: %s, the %s hand (%s): {{%.1ff, %.1ff, %.1ff}, %.0ff, %.0ff}, score %.2f (its place before: "
           "%.2f, why %d finger %d): palm %.2f, wrap %.2f, curls %.2f %.2f %.2f %.2f %.2f (opened %.2f, pads off %.1f "
           "mm, thumb swung %.1f, in %.1f mm), trigger %.2f (its pad %.1f mm off, spread %.0f, rising %.0f degrees, "
           "pushing it %.2f along its pull, %s); %d tried (round 1 %d times more); turned away: knuckles in %d, palm in "
           "%d, a finger in %d (by finger %d %d %d %d %d), a curl off %d (%d %d %d %d %d), the trigger %d, the palm off "
           "%d, her hand as drawn in it %d; her palm as drawn %.1f mm off it at its nearest (before: %.1f); her whole "
           "hand as drawn %.1f mm off it at its nearest (%s), her palm's and the blended skin's %.1f (before: %.1f, %s)",
           weapon_name(game.weapon), h == kLeft ? "left" : "right", which, b.shift.x, b.shift.y, b.shift.z, b.pitch,
           b.roll, r.best_score, r.start_score, r.start_fit.why, r.start_fit.finger, t.palm, t.wrap, t.curl[0],
           t.curl[1], t.curl[2], t.curl[3], t.curl[4], t.opened, t.pads * 1000.0f, t.thumb_swing, t.thumb_in * 1000.0f,
           t.trig_curl, t.trig_err * 1000.0f, t.trig_spread, t.trig_rise, t.trig_push,
           t.trig_whole ? "on it the whole pull" : "off it part of the pull", r.tried, r.climbs, r.why[1], r.why[2],
           r.why[3], r.in_by[0], r.in_by[1], r.in_by[2], r.in_by[3], r.in_by[4], r.why[4], r.curl_by[0], r.curl_by[1],
           r.curl_by[2], r.curl_by[3], r.curl_by[4], r.why[6], r.why[7], r.why[5], t.skin * 1000.0f,
           r.start_fit.skin * 1000.0f, t.mesh * 1000.0f, mesh_part_name(t.mesh_part), t.mesh_palm * 1000.0f,
           r.start_fit.mesh * 1000.0f, mesh_part_name(r.start_fit.mesh_part));
    rt_log("VR: grip fit: %s, the %s hand: curls off: never meeting it %d, under the least %d, over the most %d; the "
           "trigger: its pad off it %d, no rest off it %d, too steep %d, its skin in the gun %d, not pulling its way %d; "
           "the palm off: not touching %d, too little of it on it %d; her wrist bent %.0f degrees there from her real "
           "hand's, the controller's (at its place before: %.0f), places turned away for her wrist %d; her forearm as "
           "drawn %.1f mm off it there (at its place before: %.1f; 1000000000 not read), places turned away for it %d",
           weapon_name(game.weapon), h == kLeft ? "left" : "right", r.curl_sub[1], r.curl_sub[2], r.curl_sub[3],
           r.trig_sub[1], r.trig_sub[2], r.trig_sub[3], r.trig_sub[4], r.trig_sub[5], r.off_sub[1], r.off_sub[2],
           (r.best_score > 0 ? r.best_fit : r.start_fit).wrist, r.start_fit.wrist, r.why[8],
           std::min((r.best_score > 0 ? r.best_fit : r.start_fit).arm * 1000.0f, 1e9f),
           std::min(r.start_fit.arm * 1000.0f, 1e9f), r.why[9]);
}
// Testing: SH3_WEAPON_MESH=0 her fingers on the capsules alone (0.1.7's), for the rig's comparisons.
bool weapon_mesh_on() {
    static int m = -2;
    if (m == -2) m = std::getenv("SH3_WEAPON_MESH") ? std::atoi(std::getenv("SH3_WEAPON_MESH")) : 1;
    return m != 0;
}
void fit_step() {
    if (grip_fit_mode() != 1 || !wf.placed || wf.profile < 0) return;
    FitRun& r = fit_run;
    const int p = wf.profile, mh = main_hand(), oh = off_hand();
    if (game.weapon != r.weapon) {
        r = FitRun{};
        r.weapon = game.weapon;
    }
    if (r.stage >= 2) return;
    // (session 38, the rig: SH3_FIT_SECOND set fits the other hand alone, the main hand kept at its place)
    static const bool second_only = std::getenv("SH3_FIT_SECOND") != nullptr;
    if (second_only && r.stage == 0) r.stage = 1;
    const int h = r.stage == 0 ? mh : oh;
    GripPlace* slot = r.stage == 0 ? const_cast<GripPlace*>(main_fit(p, mh)) : const_cast<GripPlace*>(second_fit(p));
    const bool ready = r.stage == 0 ? slot != nullptr
                                    : slot && wf.hold->second && hands[oh].on_foregrip && two_w >= 0.999f &&
                                          hand_role[oh] == 2 && !other_surface(wf) && native_shown[oh] < 0;
    // (0.1.8: fitted against the weapon's own shape; 0.1.10: with her palm as drawn, body_palm_skin, and her whole hand
    // as drawn, body_hand_skin)
    const bool shaped = (!weapon_mesh_on() || shape_ready(p)) && body_palm_skin_ready(h) && body_hand_skin_ready(h);
    if (!ready || !shaped) {
        // (the main hand: waits for the game's place to be read, however long; none for a melee weapon in the game's
        // own hands: vr370's handgun was drawn long before its place was read. The other hand: waits for it on the
        // grip, the whole time the weapon is drawn)
        if (r.stage == 0 && !finger_drawn(p)) r.stage = 1;
        r.wait = 0;
        return;
    }
    if (r.round == 0 && r.next == 0 && r.tried == 0) {
        if (++r.wait < (r.stage == 0 ? 10 : 30)) return;  // (the other hand: its fingers and the main hand's settled)
        r.from = *slot;
        Pose pose;
        // 0.1.10 (owner 172; vr507's slices, vr514): at 0.1.8's places a gun's grip ran through the middle of her palm,
        // between the hand model's palm points at her knuckles and at its heel (all it checked), so no place within the
        // search's reach round them had her palm as drawn out of it. A hand not round a handle in its fist (the guns'
        // main hands and every second hand) first moves out of the weapon the way her palm's back faces, 1 unit at a
        // time (up to 12 cm), until her palm as drawn is clear of it; the search then works round that place.
        // (session 38, owner 178: "who ... holds a smg like that": vr528's search at every turn put the submachine gun's
        // front hand on the receiver's left side, its fingers over the top. A hand that cups the weapon, the hold's palm
        // up under it (the submachine gun's front, the flamethrower's, the shotgun's pump: palm up, thumb forward, the
        // fingers up its far side), keeps that cup: from a place where her palm as drawn does not face up (within 37
        // degrees), the search starts again from the hold's own cup, unturned; and it is never searched at every turn)
        const bool cup = r.stage == 1 && wf.hold && dot(wf.hold->second_palm, wf.hold->up) > 0.9f;
        if (cup && body_palm_skin_ready(h)) {
            const Pose cp = second_hand_pose(wf, oh, &r.from);
            Vec3 back_t{};
            body_palm_skin_clear(h, cp, &back_t);
            const float up = -dot(normalized(model_dir(wf, back_t)), wf.up);
            if (up < 0.8f) {
                rt_log("VR: grip fit: %s, the %s hand cups the weapon, but at its place her palm faces %.0f degrees off up: "
                       "the search from the hold's own cup", weapon_name(game.weapon), h == kLeft ? "left" : "right",
                       std::acos(std::clamp(up, -1.0f, 1.0f)) * 57.29578f);
                r.from = GripPlace{};
            }
        }
        if (!(r.stage == 0 && wf.hold && wf.hold->fist) && body_palm_skin_ready(h)) {
            int s = 0;
            float first = 0, least = 0;
            const GripPlace t = fit_palm_meet(r.stage, h, r.from, &s, &first, &least);
            if (s != 0 && s != 99) {
                rt_log("VR: grip fit: %s, the %s hand: her palm as drawn %.1f mm off the weapon at its place (in it below "
                       "0): moved %d units (%.1f mm; + out the way her palm's back faces, - in), her palm then %.1f mm off "
                       "it; the search round there", weapon_name(game.weapon), h == kLeft ? "left" : "right",
                       first * 1000.0f, s, s * wf.scale * 1000.0f, least * 1000.0f);
                r.from = t;
            } else if (s == 99)
                rt_log("VR: grip fit: %s, the %s hand: her palm as drawn %.1f mm off the weapon at its place, not meeting "
                       "it within 60 units along her palm's back: the search round its place", weapon_name(game.weapon),
                       h == kLeft ? "left" : "right", first * 1000.0f);
            // (session 38: a main hand not round a handle, the guns', is searched at every turn about the grip's line:
            // 0.1.8's turns, 32 degrees each way round its fitted place's, held the grip across her palm, vr507's slices)
            r.wide = r.stage == 0 || (s == 99 && !cup);
            // (session 38: and an other hand whose palm meets the weapon nowhere along its back from its place, not one
            // that cups the weapon (above): searched at every turn too, free of its grip's line, fit_candidate)
        }
        // Testing (session 38, the left-handed places): SH3_FIT_LOCAL=1 searches only round the place before (after her
        // palm is brought to meet the weapon, above): round 1 (8 degrees and 5 units each way) and round 2 (2 units),
        // never at every turn, never round 0, never climbing off round 1's edge. vr540's search from the right-handed
        // place found clear places for three left-handed other hands that were not natural (the submachine gun's
        // front's fingers in the air, the katana's under its handle's end, the handgun's wrist bent 117 degrees); the
        // right-handed holds are natural, so their places kept and moved only as far as her left hand needs.
        static const bool fit_local = std::getenv("SH3_FIT_LOCAL") != nullptr;
        if (fit_local) {
            r.wide = false, r.round = 1, r.climbs = 3;
            rt_log("VR: grip fit: %s, the %s hand: searched only round its place (SH3_FIT_LOCAL)",
                   weapon_name(game.weapon), h == kLeft ? "left" : "right");
        }
        if (r.stage == 0) main_hand_pose(wf, mh, &r.from, pose);
        else pose = second_hand_pose(wf, oh, &r.from);
        body_grip_fit(h, pose, r.stage == 0, r.stage == 0 ? weapon_trigger() : GripTrigger{}, r.start_fit);
        body_grip_mesh(h, pose, r.stage == 0, r.start_fit);  // (0.1.10: her whole hand as drawn there)
        r.start_score = r.start_fit.score;
        // (the rig's record: how her hand meets the weapon along it and up it from there, each finger's knuckle
        // clearance, the curl it meets it at and the curl it comes out of it from)
        static char line[1800];
        // (0.1.10: and across it, the weapon's right, from -9 to +15 units)
        for (int iy = -2; iy <= 2; iy++)
            for (int ix = -2; ix <= 6; ix++) {
                GripPlace g = r.from;
                const bool across = iy == 2;
                g.shift = across ? r.from.shift + wf.right * ((ix - 1) * 3.0f)
                                 : r.from.shift + wf.fwd * (ix * 5.0f) + wf.up * (iy * 10.0f);
                if (r.stage == 0) main_hand_pose(wf, mh, &g, pose);
                else pose = second_hand_pose(wf, oh, &g);
                body_grip_report(h, pose, line, sizeof line);
                // (vr380: the trigger finger's pad's nearest pass by the trigger's blade, its curl and where the pad is
                // from the blade then, and the finger's root and straight tip: model units)
                if (r.stage == 0 && kTriggers[p].on) {
                    const TriggerBlade& k = kTriggers[p];
                    float best = 1e9f, best_c = -1;
                    Vec3 off{};
                    for (int i = 0; i <= 40; i++) {
                        const float c = i * 0.025f;
                        const Vec3 j2 = body_finger_joint(h, pose, kIndex, c, 2), j3 = body_finger_joint(h, pose, kIndex, c, 3);
                        const Vec3 pm = model_of(wf, j2 + (j3 - j2) * 0.55f);
                        const Vec3 ab = k.b - k.a;
                        const float t = std::clamp(dot(pm - k.a, ab) / std::max(dot(ab, ab), 1e-6f), 0.0f, 1.0f);
                        const Vec3 near = k.a + ab * t;
                        const float d = length(pm - near) - k.r;
                        if (d < best) best = d, best_c = c, off = pm - near;
                    }
                    const Vec3 root = model_of(wf, body_finger_joint(h, pose, kIndex, 0, 0));
                    const Vec3 tip = model_of(wf, body_finger_joint(h, pose, kIndex, 0, 3));
                    const size_t used = std::strlen(line);
                    std::snprintf(line + used, sizeof line - used,
                                  "; trigger pad nearest %.1f mm (skin) at curl %.3f, pad-blade %.1f %.1f %.1f; index root "
                                  "%.1f %.1f %.1f, straight tip %.1f %.1f %.1f",
                                  (best * wf.scale - 0.0085f) * 1000.0f, best_c, off.x, off.y, off.z, root.x, root.y,
                                  root.z, tip.x, tip.y, tip.z);
                }
                GripFit gf;
                body_grip_fit(h, pose, r.stage == 0, r.stage == 0 ? weapon_trigger() : GripTrigger{}, gf);
                body_grip_mesh(h, pose, r.stage == 0, gf, true);
                rt_log("VR: grip fit record: %s, the %s hand %+d forward %+d up %+.1f across: %s; fit %.2f why %d "
                       "finger %d sub %d trigger %.2f (%.1f mm off, spread %.0f); her palm as drawn %.1f mm off it, her "
                       "whole hand %.1f (%s)", weapon_name(game.weapon), r.stage == 0 ? "main" : "other",
                       across ? 0 : ix * 5, across ? 0 : iy * 10, across ? (ix - 1) * 3.0f : 0.0f, line, gf.score,
                       gf.why, gf.finger, gf.sub, gf.trig_curl, gf.trig_err * 1000.0f, gf.trig_spread,
                       gf.skin * 1000.0f, gf.mesh * 1000.0f, mesh_part_name(gf.mesh_part));
            }
    }
    const GripTrigger trig = r.stage == 0 ? weapon_trigger() : GripTrigger{};
    for (int k = 0; k < 1200 && r.round < 3; k++) {  // (session 31: round 0 is 59,895 places, about 50 frames)
        if (r.next == 0) r.ref = r.round == 0 || r.best_score < 0 ? r.from : r.best;
        // (session 38, a wide round 0: each turn's places round where her palm meets the weapon at that turn, found as
        // the turn's first place comes up; 299,475 places, about 250 frames)
        if (r.wide && r.round == 0 && r.next % 1331 == 0) {
            const int t = r.next / 1331;
            int s = 0;
            const GripPlace m = fit_palm_meet(r.stage, h, fit_wide_turn(r.ref, t), &s, nullptr, nullptr);
            r.meet[t] = m.shift, r.meet_steps[t] = s, r.turn_best[t] = -1;
        }
        const GripPlace g = fit_candidate(wf, r, r.next);
        Pose pose;
        bool ok = true;
        if (r.stage == 0) ok = main_hand_pose(wf, mh, &g, pose);
        else pose = second_hand_pose(wf, oh, &g);
        GripFit fit;
        if (ok) body_grip_fit(h, pose, r.stage == 0, trig, fit);
        // 0.1.10 (owner 172): her whole hand as drawn, for a place that would be the best (it only turns a place away
        // or lowers its score)
        if (ok && fit.score > r.best_score) body_grip_mesh(h, pose, r.stage == 0, fit);
        r.tried++;
        if (r.wide && r.round == 0) r.turn_best[r.next / 1331] = std::max(r.turn_best[r.next / 1331], fit.score);
        if (fit.score > r.best_score) r.best_score = fit.score, r.best = g, r.best_fit = fit;
        else if (fit.score < 0 && fit.why > 0 && fit.why < 10) {
            r.why[fit.why]++;
            if (r.wide && r.round == 0) r.roll_why[(r.next / 1331) % kWideRolls][fit.why]++;
            if (fit.finger >= 0 && fit.finger < 5) (fit.why == 3 ? r.in_by : r.curl_by)[fit.finger]++;
            if (fit.why == 4 && fit.sub > 0 && fit.sub < 4) r.curl_sub[fit.sub]++;
            if (fit.why == 6 && fit.sub > 0 && fit.sub < 6) r.trig_sub[fit.sub]++;
            if (fit.why == 7 && fit.sub > 0 && fit.sub < 3) r.off_sub[fit.sub]++;
        }
        if (++r.next >= fit_round_size(r)) {
            if (r.wide && r.round == 0) {
                // (the rig's record: each turn about the grip's line, its best over the turns about the weapon's right,
                // and the units its palm moved to meet the weapon at the middle one)
                static char tl[4096];
                int at = 0;
                for (int ri = 0; ri < kWideRolls && at < (int)sizeof tl - 80; ri++) {
                    float b = -1;
                    int bp = wide_pitches() / 2;  // (session 38: and the turn about the weapon's right it was best at)
                    for (int pi = 0; pi < wide_pitches(); pi++)
                        if (r.turn_best[pi * kWideRolls + ri] > b) b = r.turn_best[pi * kWideRolls + ri], bp = pi;
                    int top = 1;
                    for (int w = 2; w < 10; w++)
                        if (r.roll_why[ri][w] > r.roll_why[ri][top]) top = w;
                    at += std::snprintf(tl + at, sizeof tl - (size_t)at, "%s%.0f: %.2f at %.0f (%d; why %d %d)",
                                        ri ? ", " : "", fit_wide_turn(r.ref, ri + kWideRolls * (wide_pitches() / 2)).roll,
                                        b, fit_wide_turn(r.ref, ri + kWideRolls * bp).pitch,
                                        r.meet_steps[ri + kWideRolls * (wide_pitches() / 2)], top, r.roll_why[ri][top]);
                }
                rt_log("VR: grip fit: %s, the %s hand, every turn about the grip's line (its best score; units moved to "
                       "meet her palm, the most common reason its places were turned away and how often): %s",
                       weapon_name(game.weapon), h == kLeft ? "left" : "right", tl);
            }
            // (session 31, vr384 and vr386: the handgun's hands were best at the turns' very edge, 40 degrees: round 1
            // again round its best while that lies on its edge, a turn of 8 or a move of 5, up to 3 times more)
            const Vec3 d = r.best.shift - r.ref.shift;
            const bool edge = r.round == 1 && r.best_score > 0 && r.climbs < 3 &&
                              (std::fabs(r.best.roll - r.ref.roll) > 7.9f || std::fabs(r.best.pitch - r.ref.pitch) > 7.9f ||
                               std::fabs(dot(d, wf.fwd)) > 4.9f || std::fabs(dot(d, wf.up)) > 4.9f ||
                               std::fabs(dot(d, wf.right)) > 4.9f);
            if (edge) r.climbs++;
            else r.round++;
            r.next = 0;
        }
    }
    if (r.round < 3) return;
    if (r.best_score > 0) *slot = r.best;
    fit_log(r, r.stage == 0 ? "main, on its grip" : "other, on the second grip", h);
    // Session 38 (owner 172; vr517: the handgun's hand fitted at the foot of its grip, the gun sat on her fist): the rig's
    // record of what holds a gun's main hand down: from the best place, the hand moved up the weapon 1 unit at a time
    // (up to 16), at each height her palm brought back to meet the weapon (fit_palm_meet), and what the fit says there.
    if (r.stage == 0 && r.wide && r.best_score > 0) {
        Pose pose;
        fit_pose_at(0, r.best, pose);
        const Vec3 at0 = model_of(wf, pose.position);
        GripPlace t = r.best;
        t.shift = r.best.shift + wf.up;
        fit_pose_at(0, t, pose);
        const float sign = dot(model_of(wf, pose.position) - at0, wf.up) > 0 ? 1.0f : -1.0f;  // (the hand up the weapon)
        for (int k = 1; k <= 16; k++) {
            GripPlace u = r.best;
            u.shift = r.best.shift + wf.up * (sign * (float)k);
            int s = 0;
            float first = 0, least = 0;
            const GripPlace m = fit_palm_meet(0, h, u, &s, &first, &least);
            fit_pose_at(0, m, pose);
            GripFit gf;
            body_grip_fit(h, pose, true, trig, gf);
            body_grip_mesh(h, pose, true, gf, true);
            rt_log("VR: grip fit, up the weapon: %s, %d units (%.1f mm) up from the best, her palm moved %d units to meet "
                   "it (%.1f mm off it before): fit %.2f why %d sub %d finger %d, trigger curl %.2f (%.1f mm off) rising "
                   "%.0f degrees, pushing %.2f; palm share %.2f, wrap %.2f; her palm as drawn %.1f mm off it, her whole "
                   "hand %.1f (%s)", weapon_name(game.weapon), k, k * wf.scale * 1000.0f, s, first * 1000.0f, gf.score,
                   gf.why, gf.sub, gf.finger, gf.trig_curl, gf.trig_err * 1000.0f, gf.trig_rise, gf.trig_push, gf.palm,
                   gf.wrap, gf.skin * 1000.0f, gf.mesh * 1000.0f, mesh_part_name(gf.mesh_part));
        }
        // Session 38 (vr521's record above: the hand moved straight up meets the frame with the skin between her
        // knuckles and the trigger guard with her middle finger; vr523: the wide search's 5 mm grid at 11 turns about
        // the weapon's right found nothing at any turn, its places too far apart for the few millimetres a hand has
        // round a grip): testing, SH3_FIT_CLIMB=1: the hand moved up the grip's own line 1 unit (2 mm) at a time up to
        // 12, and at each height searched finely round there (turned 6 degrees at a time to 24 each way about the
        // weapon's right and 4 to 16 about the grip's line, her palm brought to meet the weapon at each turn, then moved
        // 2 units along the weapon and 1.5 across it, 4 and 3 each way), each height's best logged; the highest height
        // with a place taken (the pictures after show it).
        static const bool climb = std::getenv("SH3_FIT_CLIMB") != nullptr;
        if (climb) {
            const Vec3 rod = normalized(wf.hold->rod);
            fit_pose_at(0, r.best, pose);
            const Vec3 h0 = model_of(wf, pose.position);
            GripPlace t1 = r.best;
            t1.shift = r.best.shift + rod;
            fit_pose_at(0, t1, pose);
            const float rs = dot(model_of(wf, pose.position) - h0, wf.up) > 0 ? 1.0f : -1.0f;  // (the hand up the grip)
            GripPlace top{};
            int top_k = -1;
            for (int k = 0; k <= 12; k++) {
                GripPlace hb{};
                GripFit hf;
                float hs = -1;
                int why_n[8]{};
                for (int ti = 0; ti < 81; ti++) {
                    GripPlace g = r.best;
                    g.shift = r.best.shift + rod * (rs * (float)k);
                    g.pitch = r.best.pitch + (float)(ti / 9 - 4) * 6.0f;
                    g.roll = r.best.roll + (float)(ti % 9 - 4) * 4.0f;
                    int s = 0;
                    const GripPlace m = fit_palm_meet(0, h, g, &s, nullptr, nullptr);
                    for (int j = 0; j < 25; j++) {
                        GripPlace c = m;
                        c.shift = m.shift + wf.fwd * ((float)(j / 5 - 2) * 2.0f) + wf.right * ((float)(j % 5 - 2) * 1.5f);
                        fit_pose_at(0, c, pose);
                        GripFit gf;
                        body_grip_fit(h, pose, true, trig, gf);
                        if (gf.score > hs) body_grip_mesh(h, pose, true, gf);
                        if (gf.score > hs) hs = gf.score, hb = c, hf = gf;
                        else if (gf.score < 0 && gf.why > 0 && gf.why < 8) why_n[gf.why]++;
                    }
                }
                int top_why = 1;
                for (int w = 2; w < 8; w++)
                    if (why_n[w] > why_n[top_why]) top_why = w;
                rt_log("VR: grip fit, up the grip: %s, %d units (%.1f mm) up its line: best %.2f at {{%.1ff, %.1ff, %.1ff}, "
                       "%.0ff, %.0ff}: palm %.2f, wrap %.2f, curls %.2f %.2f %.2f %.2f %.2f, trigger %.2f rising %.0f "
                       "degrees, pushing %.2f; her palm as drawn %.1f mm off it, her whole hand %.1f (%s); turned away "
                       "most for why %d (%d of 2025)", weapon_name(game.weapon), k, k * wf.scale * 1000.0f, hs,
                       hb.shift.x, hb.shift.y, hb.shift.z, hb.pitch, hb.roll, hf.palm, hf.wrap, hf.curl[0], hf.curl[1],
                       hf.curl[2], hf.curl[3], hf.curl[4], hf.trig_curl, hf.trig_rise, hf.trig_push, hf.skin * 1000.0f,
                       hf.mesh * 1000.0f, mesh_part_name(hf.mesh_part), top_why, why_n[top_why]);
                if (hs > 0) top = hb, top_k = k;
            }
            if (top_k > 0) {
                *slot = top;
                rt_log("VR: grip fit, up the grip: %s, the place %d units up taken for the pictures: {{%.1ff, %.1ff, "
                       "%.1ff}, %.0ff, %.0ff}", weapon_name(game.weapon), top_k, top.shift.x, top.shift.y, top.shift.z,
                       top.pitch, top.roll);
            }
        }
    }
    const int stage = r.stage + 1;
    const int weapon = r.weapon;
    r = FitRun{};
    r.weapon = weapon;
    r.stage = stage;
}

// The weapon placed for this frame: its frame, held back from walls (its probes, sh3vr_body.cpp), and the hands posed
// round it (the owner: "the hand positions and grip points on weapons should feel natural and native").
void place_weapon() {
    const int mh = main_hand(), oh = off_hand();
    hand_role[0] = hand_role[1] = 0;
    native_shown[0] = native_shown[1] = -1;
    build_frame(wf, two_w > 0.001f);
    // (0.1.8: how long it has been in the hand, for its own shape: combat_weapon_mesh)
    if (wf.placed && game.weapon == placed_weapon) placed_frames++;
    else placed_frames = 0, placed_weapon = wf.placed ? game.weapon : -1;
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
            rt_log("VR: frame %d: %s held at a surface (%s)", rt_frame, weapon_name(game.weapon), weapon_wall_line());
            // Its knock on the surface where its line met it (louder the faster it came: detail, cm a second); 0.1.7:
            // not again within 0.4 s (a weapon resting at a wall's edge, its line on and off it).
            static double knocked_at = -10.0;
            if (now - knocked_at > 0.4) {
                knocked_at = now;
                hand_sound(kSoundWeaponWall, world_point(reach_valid ? lerp(reach_from, reach_to, free_t)
                                                                     : hands[mh].grip.position),
                           (int)(hands[mh].speed * 100.0f));
            }
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
    // (0.1.8: at its fitted place, main_fit; the game's own rigid hand on a melee weapon as before)
    if (main_hand_pose(wf, mh, main_fit(wf.profile, mh), np)) {
        hands[mh].shown = np;
        if (nm && !finger_drawn(wf.profile)) native_shown[mh] = native_group_of(wf.profile, mh);
    } else hands[mh].shown = hand_round(mh, wf.base, frame_dir(wf, hd.rod), right_t * -mxm);
    hand_role[mh] = 1;
    // 0.1.8 (owner message 146): Hold weapons anywhere: the other hand took hold away from the natural grip this frame
    // (update_two_hand): its hold on the part there, laid on it against the main hand as just posed. Should none fit
    // (a knuckle in the weapon even 6 cm out), the hand stays as it took hold, its fingers closing on what they meet.
    if (anywhere_ask.on) {
        anywhere_ask.on = false;
        SurfaceHold s;
        if (surface_hold_at(wf, oh, anywhere_ask.spot, anywhere_ask.rod, anywhere_ask.palm, 0.07f / wf.scale, s)) {
            anywhere_hold = s;
            rt_log("VR: frame %d: the %s hand took hold of %s away from its second grip: its fist at %.1f %.1f %.1f "
                   "along %.2f %.2f %.2f, its palm toward %.2f %.2f %.2f (model; it took hold at %.1f %.1f %.1f)",
                   rt_frame, oh == kLeft ? "left" : "right", weapon_name(game.weapon), s.at.x, s.at.y, s.at.z, s.rod.x,
                   s.rod.y, s.rod.z, s.palm.x, s.palm.y, s.palm.z, anywhere_ask.spot.x, anywhere_ask.spot.y,
                   anywhere_ask.spot.z);
        } else {
            anywhere_hold = SurfaceHold{};
            anywhere_hold.on = true;
            anywhere_hold.profile = wf.profile;
            anywhere_hold.at = anywhere_ask.spot;
            anywhere_hold.rod = anywhere_ask.rod;
            anywhere_hold.palm = anywhere_ask.palm;
            rt_log("VR: frame %d: the %s hand took hold of %s away from its second grip, where it is (no part there to "
                   "lay its palm on)", rt_frame, oh == kLeft ? "left" : "right", weapon_name(game.weapon));
        }
    }
    if (!hands[oh].on_foregrip && two_w <= 0.001f) anywhere_hold.on = false;
    resolve_moved_hold(wf, oh);
    // The other hand on the second grip: there, posed as that grip is held (the game's own left hand on it, else her
    // finger hand round it, mirrored for the left hand's).
    // Taking and letting go eased (two_w): the hand from its controller's place onto the grip and back.
    const bool two = two_w > 0.001f && hd.second && hands[oh].valid;
    Pose on;
    bool native_on = false;
    Vec3 pull_want{};
    float past = 0;
    if (two) {
        const NativeGrip* no = mh == kRight && !finger_drawn(wf.profile) ? native_of(wf.profile, oh) : nullptr;
        const float pump = reload_pump_back();
        // (0.1.7, owner message 124: the game's own other hand follows the player's two-hand grip spot too)
        // 0.1.8: where it took hold (Hold weapons anywhere) or at its moved spot: her finger hand wrapping the part there;
        // else at the natural grip, at its fitted place (second_fit).
        if (const SurfaceHold* sh = other_surface(wf)) {
            Vec3 at = sh->at;
            if (pump > 0) at = at - wf.fwd * (pump * kUnitsPerMetre);
            on = hand_round(oh, frame_point(wf, at), frame_dir(wf, sh->rod), frame_dir(wf, sh->palm));
        } else if (no && native_pose(oh, *no, wf.fwd * -(pump * kUnitsPerMetre) + second_offset(wf) + second_nudge(wf.profile), np))
            on = np, native_on = true;
        else
            on = second_hand_pose(wf, oh, second_fit(wf.profile));
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
        if (native_on && two_w >= 0.5f && !finger_drawn(wf.profile)) native_shown[oh] = native_group_of(wf.profile, oh);
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
                   native_on ? "the game's own hand" : other_surface(wf) ? "her finger hand where it took hold"
                                                                         : "her finger hand", palm.x, palm.y, palm.z, rod.x, rod.y, rod.z,
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
    fit_step();  // (the PC rig's grip fit, SH3_GRIP_FIT=1; nothing otherwise)
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
Thresholds thresholds(int what, bool as_asked = true) {
    // 0.1.3 (owner 84: "physical melee seems to sensitive. im just simply moving the knife arm gently/arm in general,
    // and melee animation/sound happens"): every swing asks more than 0.1.2's (knife 1.75 m/s, 0.14 m; stun gun 1.40,
    // 0.10; heavy 2.80, 0.30; gun 2.60, 0.26; a punch 2.20, 0.18), on top of the speeds now taken over the real time
    // between frames (motion_dt). A deliberate swing passes 3 m/s; a gentle move or a wave stays under 1.5.
    // Owner 89 ("melee should be fixed for eveyrthing, not just knife ofc"), vr258: the heavy weapons' 0.32 m of fast
    // travel let a 40 cm swing at 5.8 m/s (the pipe, katana, maul) miss by a hair, so a heavy weapon and a gun as a club
    // ask a shorter fast path; the speed (twice 0.1.2's in real time, never reached by a gentle or moderate move) is
    // what keeps an arm that only moves from swinging.
    // 0.1.8 (owner message 146: "make sure the melee isnt too senstiive off the bat either"; his 0.1.7 log counted a
    // knife swing at 2.88 m/s, a gun clubbing at 2.96 while it was only moved about): "Swing strength needed". Light is
    // 0.1.7's; Normal (to start) asks a fifth more speed (the knife 2.88 m/s, a punch or a gun 3.36, a heavy weapon
    // 3.60), so only a firm swing counts; Hard asks half as much again and a tenth more of the fast path. The fast path
    // is measured above the speed asked, so a higher speed already asks a longer firm stretch of the swing.
    Thresholds t;
    switch (what) {
    case kLight: t = {2.40f, 0.20f, 0.14f, 0.22f}; break;  // the knife
    case kStun: t = {2.00f, 0.14f, 0.10f, 0.30f}; break;   // a jab with the stun gun's prongs
    case kHeavy: t = {3.00f, 0.26f, 0.20f, 0.38f}; break;  // the steel pipe, katana, maul, beam saber
    case kGun: t = {2.80f, 0.24f, 0.18f, 0.45f}; break;    // a gun as a club
    default: t = {2.80f, 0.22f, 0.15f, 0.30f}; break;      // a punch
    }
    if (!as_asked) return t;  // (0.1.7's: a harder swing's damage stays measured from them, swing_speed_factor)
    if (opt.swing_needed == 1) t.speed *= 1.2f;
    else if (opt.swing_needed == 2) t.speed *= 1.5f, t.travel *= 1.1f;
    return t;
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
                sw.most_speed = sw.most_fast = sw.most_net = 0;
                sw.was_armed = sw.armed;
                // the path from where the weapon was the frame before the hand sped up
                sw.start_grip_w = sw.seg_valid ? sw.seg_grip : g_now;
                sw.start_tip_w = sw.seg_valid ? sw.seg_tip : tip_now;
            }
            if (s.speed >= t.speed * 0.6f) sw.travel = std::min(0.75f, sw.travel + s.speed * motion_dt());
            sw.fast_for = s.speed >= t.speed ? sw.fast_for + motion_dt() : 0.0f;
        } else {
            if (sw.active && !sw.fired && (what == kLight || what == kHeavy || what == kStun) &&
                sw.most_speed >= t.speed && sw.most_fast >= 0.025f && sw.travel >= t.travel) {
                // 0.1.7 (owner 140): a swing fast and long enough that pressed no game attack, and why
                static int told = 0;
                if (told++ < 20)
                    rt_log("VR: frame %d: the %s %s's swing ended without the game's attack: armed %d (at its start %d), "
                           "%.2f s since the last (it asks %.2f), the most speed %.2f m/s held %.0f ms, the fast travel "
                           "%.2f m, %.2f m from its start (it asks %.2f), the game's attack still pressed %d%s", rt_frame,
                           h == kLeft ? "left" : "right", weapon_name(slot), sw.armed, sw.was_armed, now - sw.last,
                           slot == 7 ? 0.56f : t.cooldown, sw.most_speed, sw.most_fast * 1000.0f, sw.travel,
                           sw.most_net, t.net, pending.armed, told == 20 ? "; no more of these lines" : "");
            }
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
        if (sw.active) {
            sw.most_speed = std::max(sw.most_speed, s.speed);
            sw.most_fast = std::max(sw.most_fast, sw.fast_for);
            sw.most_net = std::max(sw.most_net, net);
        }
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
        if (door_hold.hand == h) continue;  // 0.1.10: still holding a door's handle (its grip eased and closed again)
        if (s.holding_weapon || s.on_foregrip || reload_owns_hand(h)) continue;
        bool over_holster = false;
        for (int k = 0; k < kHolsterCount; k++) over_holster |= holster_on(k) && holsters[k].hovered[h];
        if (over_holster) continue;
        // Reaching out or down: the game's own action (pick up, open, examine), at the hand ("Take items with your
        // hand": the game looks for what is there, at the palm, instead of in front of her), else in front of her.
        if (hand_reaching(s)) {
            // 0.1.10: a door's handle taken (one door at a time; the other hand closing on a door meanwhile takes
            // nothing)
            if (opt.door_by_hand && opt.take_by_hand && map_ready() && hover_door[h] && hover_event[h] >= 0 &&
                hover_event[h] < 0x400) {
                if (door_hold.hand >= 0) continue;
                door_hold.hand = h;
                door_hold.event = hover_event[h];
                door_hold.world = world_point(s.shown.position + rotate(s.shown.orientation, Vec3{0, -0.06f, 0}));
                door_hold.start = s.grip.position;
                Vec3 to = fi.head.position - s.grip.position;
                to.y = 0;
                door_hold.toward = length(to) > 0.02f ? normalized(to) : Vec3{0, 0, 1};
                door_hold.held = 0.0f;
                door_hold.buzz_in = 0.1f;
                buzz(h, 0.30f, 0.035f, 180.0f);
                hand_sound(kSoundHandSurface, door_hold.world, 30);
                rt_log("VR: frame %d: the %s hand took door event %d's handle at %.0f %.0f %.0f (pull it toward her, "
                       "push it or slide it 10 cm to open it)", rt_frame, h == kLeft ? "left" : "right",
                       door_hold.event, door_hold.world.x, door_hold.world.y, door_hold.world.z);
                continue;
            }
            grab_frames = 3;
            if (opt.take_by_hand && map_ready()) {
                hand_action.hand = h;
                hand_action.world = world_point(s.shown.position + rotate(s.shown.orientation, Vec3{0, -0.06f, 0}));
                hand_action.frames = 4;
                hand_action.found = false;
                hand_action.hover = hover_event[h];
            } else if (opt.haptic_extra >= 1) buzz(h, 0.3f, 0.04f);  // "Extra vibration": pickups and doors
        }
    }
}

// 0.1.10: the door's handle held (door_hold): opened once moved 10 cm level, else let go (a soft buzz while it is
// pulled, firmer the farther it has come).
void update_door_hold() {
    DoorHold& d = door_hold;
    if (d.hand < 0) return;
    const int h = d.hand;
    const HandState& s = hands[h];
    const float g = h == kLeft ? fi.controls.left_grip : fi.controls.right_grip;
    d.held += frame_dt();
    Vec3 moved = s.grip.position - d.start;
    moved.y = 0;
    const float far = length(moved);
    const char* why = nullptr;
    if (!opt.door_by_hand || !opt.take_by_hand || !opt.grabbing || !in_play() || !s.valid || !map_ready())
        why = "the hand or play gone";
    else if (g < 0.35f)
        why = "let go";
    else if (d.held > 6.0f)
        why = "held 6 s";
    else if (far < 0.10f &&
             length(world_point(s.shown.position + rotate(s.shown.orientation, Vec3{0, -0.06f, 0})) - d.world) >
                 0.5f * kUnitsPerMetre)
        why = "the hand carried 50 cm from it";
    if (why) {
        rt_log("VR: frame %d: the %s hand let go of door event %d's handle without opening it (%s; moved %.1f cm, "
               "held %.2f s)", rt_frame, h == kLeft ? "left" : "right", d.event, why, far * 100.0f, d.held);
        d.hand = -1;
        return;
    }
    // (the other hand's action still running: the door waits for it, held, a frame at a time)
    if (far >= 0.10f && hand_action.frames == 0 && grab_frames == 0) {
        const float along = dot(moved, d.toward);
        const char* how = along > 0.07f ? "pulled it toward her" : along < -0.07f ? "pushed it away" : "slid it aside";
        rt_log("VR: frame %d: the %s hand %s %.1f cm (held %.2f s): door event %d used where the hand took it",
               rt_frame, h == kLeft ? "left" : "right", how, far * 100.0f, d.held, d.event);
        hand_action.hand = h;
        hand_action.world = d.world;
        hand_action.frames = 4;
        hand_action.found = false;
        hand_action.hover = d.event;
        grab_frames = 3;
        d.hand = -1;
        return;
    }
    d.buzz_in -= frame_dt();
    if (d.buzz_in <= 0.0f && far > 0.01f) {
        buzz(h, 0.10f + 0.35f * far / 0.10f, 0.03f, 120.0f);
        d.buzz_in = 0.08f;
    }
}

// 0.1.10 (owner 172: "WHY ... IS THE GUN STILL CLIPPING THROUGH THE HAND"; vr493: the handgun's back strap across the
// rear of her palm, by the wrist, where the palm points the places were fitted and settled with never were): her
// hands as drawn against the weapon as drawn. Each skinned draw of hers in her own pose (not the ghost's copy of
// another, not the flashlight in her hand) is skinned as the game's shader skins it (sh3vr_ik.cpp shadow_part's way)
// and each vertex kept by the hand its weights are mostly on (that hand's own bone, its fingers' or a blend riding on
// one of them: ik_hand_of_bone), with that hand's bones as drawn; the weapon's own draw in the same frame (its body's
// pass) gives its held bone as drawn, so each vertex goes into the weapon's own units exactly (the held bone's inverse),
// wherever the head or the hands are, and is tested against the weapon's own shape (sh3vr_shape.cpp).
struct HandMesh {
    int frame = -100;
    std::vector<Vec3> v;      // world (the game's units)
    std::vector<int8_t> vt;   // ... each one's bone: its finger's segment (finger * 3 + segment, 90 percent or more on it),
                              // -2 the hand's own bone (90 percent or more), -1 blended between them
    std::vector<Vec3> rigid;  // ... those of them on the hand's own bone alone (90 percent or more): its palm, which
                              // no finger's curl moves
    std::vector<Vec3> fv;         // ... those on one finger's segment alone (90 percent or more) ...
    std::vector<uint8_t> fseg;    // ... and which (finger * 3 + segment)
    Vec3 wrist, knuckle[4], thumb[3];
    Vec3 joint[5][3];             // each finger's joints as drawn (its segments' bones)
    // ... each vertex's bones as skinned, 4 a vertex (0.1.10: her hand as drawn, posed at any curl in the hand's model,
    // sh3vr_body.cpp body_hand_skin): the finger's segment each is or rides on (finger * 3 + segment), -1 the hand's
    // own bone, -2 another (her forearm's); its weight (0: none); where that bone alone puts the vertex (world)
    std::vector<int8_t> it;
    std::vector<float> iw;
    std::vector<Vec3> iq;
    std::vector<int> tri;         // her hand's triangles (3 a triangle: indices into v, every corner this hand's)
    bool bones = false;
    // 0.1.10 (session 38): the draws this frame with any of this hand's vertices: their vertices and triangles, summed
    // (her model's own counts, the same whatever the pose: another costume's model gives others; skin_watch)
    int draw_nv = 0, draw_tri = 0;
};
HandMesh hand_mesh[2];
// Session 38 (vr549: the maul's end out through her left forearm, unseen by any hand's reading): each forearm's own
// vertices as drawn this frame (half their weight or more on her forearm's bones: ik_forearm_of_bone), world; the
// pictures' lines read them against the weapon (combat_hand_mesh_report)
struct ArmMesh {
    int frame = -100;
    std::vector<Vec3> v;
    bool bones = false;   // ... her elbow and wrist as drawn then (world), and the hand's pose shown then (tracking)
    Vec3 elbow, wrist;
    Pose shown;
};
ArmMesh arm_mesh[2];
// 0.1.10 (session 38; open since session 37: a costume change kept the hand skin taken from the costume before): her
// hand as drawn is watched (its draws' counts, HandMesh draw_nv/draw_tri), every 3 seconds once taken
// (hand_mesh_wanted; every frame on the rig); other counts than those it was taken with, the same at every look for 2
// seconds or more (two looks at least), are another model of her (a costume): her palm and whole hand as drawn are
// taken again (hand_skin_step), at most 8 times a session. Testing (rig): SH3_SKIN_FORGET_AT=frame takes them again at
// that frame as if changed.
struct SkinWatch {
    int nv = -1, tri = 0;           // the counts it was taken with (-1 not taken)
    int new_nv = -1, new_tri = 0;   // other counts seen ...
    int since = -1;                 // ... from this frame, the same at every look since (-1 none)
    int last = -100;                // the frame of the last look
    int again = 0;                  // times taken again
};
SkinWatch skin_watch[2];
constexpr int kSkinLookEvery = 270;  // frames between looks once taken (3 seconds at 90 a second)
Aff mesh_held{};  // the weapon's held bone as drawn (its body's pass) ...
int mesh_held_frame = -100;  // ... in this frame
enum HandPart { kPartPalm = 0, kPartKnuckles, kPartFingers, kPartThumb, kPartWrist, kHandParts };
const char* const kPartName[kHandParts] = {"palm", "knuckles", "fingers", "thumb", "wrist"};
struct HandDepth {
    int frame = -100, n = 0;
    float deep[kHandParts];   // metres off the weapon at the nearest (below 0: in it)
    int touching[kHandParts]; // vertices within 1 mm of it or in it
    int count[kHandParts];
    Vec3 where[kHandParts];   // the nearest's place on her hand, cm: along it from the wrist, across it toward the little
                              // finger, off the palm's plane
    Vec3 lo{1e9f, 1e9f, 1e9f}, hi{-1e9f, -1e9f, -1e9f};  // the palm's vertices' places so (cm)
    // ... and by the bone each vertex is on (HandMesh::vt): each finger's segments 0-14 (finger * 3 + segment), 15 the
    // hand's own bone, 16 blended between them; their nearest (metres) and how many
    float bone_deep[17];
    int bone_count[17]{};
};
// (wanted while a main hand's place is to be settled; on the rig always, for its pictures' lines)
bool hand_mesh_wanted() {
    if (view_mode != ViewMode::kFirstPerson || !wf.placed || !in_hand || wf.profile < 0 || placed_weapon != game.weapon)
        return false;
#if !defined(__ANDROID__)
    static const bool rig = std::getenv("QUEST_SIM_SCRIPT") != nullptr;
    if (rig) return true;
#endif
    // (0.1.10: until her hands as drawn are taken, body_hand_skin)
    if (!body_hand_skin_ready(0) || !body_hand_skin_ready(1)) return true;
    // (session 38: a look every 3 seconds: skin_watch)
    if (rt_frame % kSkinLookEvery == 0) return true;
    return !heel_done[wf.profile][opt.left_handed ? 1 : 0] && main_fit(wf.profile, main_hand()) != nullptr;
}
// Which part of hand h (its bones as drawn, hm) a vertex of it is: the thumb (its two outer segments and its tip), the
// fingers past 1 cm beyond their knuckles, the knuckles (from 0.8 cm before their line to 1 cm past it), the palm (to
// 2 cm behind the wrist's joint), the wrist (2 to 6 cm behind it); -1 farther back.
int hand_part(const HandMesh& hm, int h, Vec3 v, Vec3* where) {
    const float U = kUnitsPerMetre;
    const Vec3 W = hm.wrist, a = normalized(hm.knuckle[1] - W);
    const Vec3 k0 = hm.knuckle[0], across = hm.knuckle[3] - k0;
    const float al = dot(v - W, a);
    const Vec3 t1 = hm.thumb[1], t2 = hm.thumb[2], tip = t2 + (t2 - t1) * 0.8f;
    int part;
    if (seg_seg(v, v, t1, t2) < 0.016f * U || seg_seg(v, v, t2, tip) < 0.016f * U) {
        part = kPartThumb;
    } else {
        const float s = std::clamp(dot(v - k0, across) / std::max(dot(across, across), 1e-6f), 0.0f, 1.0f);
        const float past = dot(v - (k0 + across * s), a);  // past the knuckles' line
        if (past > 0.010f * U) part = kPartFingers;
        else if (past > -0.008f * U) part = kPartKnuckles;
        else if (al >= -0.02f * U) part = kPartPalm;
        else if (al >= -0.06f * U) part = kPartWrist;
        else return -1;
    }
    if (where) {
        const Vec3 side = normalized(across - a * dot(across, a));
        const Vec3 off = cross(a, side) * (h == kLeft ? -1.0f : 1.0f);
        const Vec3 d = v - W;
        *where = Vec3{al, dot(d, side), dot(d, off)} * (100.0f / U);
    }
    return part;
}
// Hand h as last drawn against the weapon as drawn in the same frame: false when not both caught in the last two
// frames, or the weapon's shape not yet taken; `pts`: its palm's, wrist's and knuckles' vertices in the weapon's units.
bool hand_depth(int h, HandDepth& d, std::vector<Vec3>* pts) {
    d = HandDepth{};
    for (int k = 0; k < kHandParts; k++) d.deep[k] = 1e9f;
    for (float& b : d.bone_deep) b = 1e9f;
    if (h < 0 || h > 1 || !wf.placed || wf.profile < 0 || !shape_ready(wf.profile)) return false;
    const HandMesh& hm = hand_mesh[h];
    if (!hm.bones || hm.v.empty() || hm.frame != mesh_held_frame || hm.frame < rt_frame - 2) return false;
    Aff iH;
    if (!inverse(mesh_held, iH) || length(mesh_held.t - hm.wrist) > 0.35f * kUnitsPerMetre) return false;
    d.frame = hm.frame;
    for (size_t i = 0; i < hm.v.size(); i++) {
        const Vec3& v = hm.v[i];
        Vec3 where;
        const int part = hand_part(hm, h, v, &where);
        const Vec3 m = mul(iH.r, v) + iH.t;
        const float c = shape_point(wf.profile, m) * wf.scale;
        if (i < hm.vt.size()) {
            const int b = hm.vt[i] >= 0 ? hm.vt[i] : hm.vt[i] == -2 ? 15 : 16;
            d.bone_count[b]++;
            d.bone_deep[b] = std::min(d.bone_deep[b], c);
        }
        if (part < 0) continue;
        d.n++, d.count[part]++;
        if (c < 0.001f) d.touching[part]++;
        if (c < d.deep[part]) d.deep[part] = c, d.where[part] = where;
        if (part == kPartPalm)
            d.lo = {std::min(d.lo.x, where.x), std::min(d.lo.y, where.y), std::min(d.lo.z, where.z)},
            d.hi = {std::max(d.hi.x, where.x), std::max(d.hi.y, where.y), std::max(d.hi.z, where.z)};
        if (pts && (part == kPartPalm || part == kPartWrist || part == kPartKnuckles)) pts->push_back(m);
    }
    return d.n > 0;
}
// The parts' line (each part's nearest, mm, how many of its vertices within 1 mm of the weapon or in it, and where)
int hand_parts_text(const HandDepth& d, char* out, int n) {
    int at = 0;
    out[0] = 0;
    for (int k = 0; k < kHandParts && at < n - 1; k++) {
        if (d.count[k] <= 0) continue;
        at += std::snprintf(out + at, (size_t)(n - at), "%s%s %.1f mm (%d of %d within 1 mm; at %.1f/%.1f/%.1f cm)",
                            at ? ", " : "", kPartName[k], d.deep[k] * 1000.0f, d.touching[k], d.count[k], d.where[k].x,
                            d.where[k].y, d.where[k].z);
        at = std::min(at, n - 1);
        if (k == kPartPalm && at < n - 1) {  // (where its vertices lie on her hand: along it, off its plane)
            at += std::snprintf(out + at, (size_t)(n - at), " [its vertices %.1f..%.1f cm along, %.1f..%.1f off the plane]",
                                d.lo.x, d.hi.x, d.lo.z, d.hi.z);
            at = std::min(at, n - 1);
        }
    }
    return at;
}

// Her palm as drawn, for the hand's own model (sh3vr_body.cpp body_palm_skin): once for each hand, the vertices on its
// own bone alone (no finger's curl moves them) from the palm's rear (1 cm behind the wrist's joint) to the knuckles, with
// that hand's bones as drawn the same frame.
void hand_skin_step() {
    static std::vector<Vec3> keep;
#if !defined(__ANDROID__)
    static const int forget_at = std::getenv("SH3_SKIN_FORGET_AT") ? std::atoi(std::getenv("SH3_SKIN_FORGET_AT")) : -1;
#else
    constexpr int forget_at = -1;
#endif
    static const char* const kSide[2] = {"left", "right"};
    for (int h = 0; h < 2; h++) {
        const HandMesh& hm = hand_mesh[h];
        SkinWatch& sw = skin_watch[h];
        // (session 38: another model of her drawn, a costume: skin_watch)
        if (hm.bones && hm.draw_nv > 0 && hm.frame >= rt_frame - 2 && hm.frame != sw.last && sw.nv >= 0 &&
            body_hand_skin_ready(h)) {
            sw.last = hm.frame;
            const bool same = hm.draw_nv == sw.nv && hm.draw_tri == sw.tri;
            const bool testing = forget_at >= 0 && hm.frame >= forget_at && sw.again == 0;
            if (same) sw.since = -1;
            else if (sw.since < 0 || hm.draw_nv != sw.new_nv || hm.draw_tri != sw.new_tri)
                sw.since = hm.frame, sw.new_nv = hm.draw_nv, sw.new_tri = hm.draw_tri;
            if (testing || (sw.since >= 0 && hm.frame - sw.since >= 180 && sw.again < 8)) {
                rt_log("VR: frame %d: her %s hand drawn by another model of her (%d vertices and %d triangles in its "
                       "draws, %d and %d when its skin was taken%s): her palm and whole hand as drawn taken again",
                       rt_frame, kSide[h], hm.draw_nv, hm.draw_tri, sw.nv, sw.tri,
                       testing ? "; testing, SH3_SKIN_FORGET_AT" : "");
                body_hand_skin_forget(h);
                sw.nv = -1, sw.since = -1, sw.again++;
            }
        }
        // (0.1.10: her whole hand as drawn, every vertex with its bones, sh3vr_body.cpp body_hand_skin; once, and again
        // for another model of her)
        HandDrawn d;
        if (!body_hand_skin_ready(h) && combat_hand_drawn(h, d) && d.ntri > 0) {
            body_hand_skin(h, d);
            if (body_hand_skin_ready(h)) {
                sw.nv = hm.draw_nv, sw.tri = hm.draw_tri, sw.last = hm.frame, sw.since = -1;
                if (sw.again > 0)
                    rt_log("VR: frame %d: her %s hand as drawn taken again (%d vertices and %d triangles in its draws)",
                           rt_frame, kSide[h], hm.draw_nv, hm.draw_tri);
            }
        }
        if (body_palm_skin_ready(h) || !hm.bones || hm.frame < rt_frame - 2 || hm.rigid.size() < 20) continue;
        keep.clear();
        for (const Vec3& v : hm.rigid) {
            Vec3 where;
            const int part = hand_part(hm, h, v, &where);
            if (part >= 0 && part != kPartWrist && where.x >= -1.0f) keep.push_back(v);
        }
        body_palm_skin(h, keep.data(), (int)keep.size(), (int)hm.rigid.size(), hm.wrist, hm.knuckle, hm.thumb);
    }
}

// Then her main hand on each weapon (and handedness), once: the first time it is held at rest at its fitted place with
// the weapon's own shape taken, her hand as drawn is measured against the weapon as drawn and logged (each part's
// nearest, the palm, the knuckles, the fingers, the thumb and the wrist): the places are fitted with her palm as drawn
// (body_palm_skin, body_grip_fit), so this is their check in play. Testing, SH3_HEEL_SETTLE=1 (build 277's way: moving
// the hand alone does not take a grip out of a flat palm, and it took the trigger out of the finger's reach): while its
// palm, wrist and knuckles lie within 1.5 mm of the weapon or in it, the hand's place is moved out (the deepest vertex's
// way out of the shape, the best place found kept, at most 3 cm in all) and measured again, up to three moves; kept in
// heel_extra, which main_fit adds to the place.
int heel_round[kProfiles][2], heel_moved_at[kProfiles][2];
float heel_moved[kProfiles][2];  // model units moved in all
void settle_heel() {
    if (!wf.placed || wf.profile < 0 || grip_fit_mode() == 1 || view_mode != ViewMode::kFirstPerson) return;
    const int p = wf.profile, m = opt.left_handed ? 1 : 0, mh = main_hand();
    if (heel_done[p][m] || !hands[mh].valid) return;
    const GripPlace* g = main_fit(p, mh);
    if (!g) {
        heel_seen[p][m] = 0;
        return;
    }
    static const char* const kSide[2] = {"right-handed", "left-handed"};
    const int seen = ++heel_seen[p][m];
    if (seen >= 3600) {
        heel_done[p][m] = true;
        rt_log("VR: frame %d: %s, the main hand's place (%s): her hand as drawn not caught at rest against the weapon's "
               "own shape (%s) in %d frames: left %s", rt_frame, weapon_name(game.weapon), kSide[m],
               shape_ready(p) ? "the shape taken" : "the shape not taken", seen,
               heel_round[p][m] ? "where it was moved" : "as fitted");
        return;
    }
    // (held at rest: no reload, no shot just made, the hand drawn at its place a while)
    if (seen < 8 || placed_frames < 8 || rt_frame - heel_moved_at[p][m] < 20 || now < flash_until + 0.3f ||
        reload_blocks_fire() || reload_owns_hand(kLeft) || reload_owns_hand(kRight))
        return;
    static std::vector<Vec3> pts;
    pts.clear();
    HandDepth d;
    if (!hand_depth(mh, d, &pts) || pts.empty() || d.frame <= heel_moved_at[p][m] + 2) return;
    const float want = 0.0015f;
    auto worst = [&](Vec3 D, int& wi) {
        float least = 1e9f;
        wi = -1;
        for (size_t i = 0; i < pts.size(); i++) {
            const float c = shape_point(p, pts[i] + D) * wf.scale;
            if (c < least) least = c, wi = (int)i;
        }
        return least;
    };
    int wi = -1;
    const float first = worst(Vec3{}, wi);
    static char parts[700];
    hand_parts_text(d, parts, sizeof parts);
    const int r = heel_round[p][m];
    // (testing: SH3_HEEL_SETTLE=1 moves the hand out too)
    static const int settle_on = std::getenv("SH3_HEEL_SETTLE") ? std::atoi(std::getenv("SH3_HEEL_SETTLE")) : 0;
    if (first >= want - 1e-4f || r >= 3 || !settle_on) {
        heel_done[p][m] = true;
        rt_log("VR: frame %d: %s, the main hand's place (%s): her hand as drawn (frame %d, %d vertices) off the weapon "
               "at the nearest (below 0: in it): %s; its palm, wrist and knuckles %.1f mm off it at the least: %s",
               rt_frame, weapon_name(game.weapon), kSide[m], d.frame, d.n, parts, first * 1000.0f,
               r == 0 ? "left as fitted" : first >= want - 1e-4f ? "clear of it where it was moved" : "still in it after "
               "3 moves: left where it was moved");
        if (r > 0)
            rt_log("VR: ... %s moved %.1f mm in all from its fitted place (%d moves)", weapon_name(game.weapon),
                   heel_moved[p][m] * wf.scale * 1000.0f, r);
        return;
    }
    // The move out: the deepest vertex's way out of the shape (its gradient there), all of them moved together.
    const float room = std::max(0.0f, 0.03f / std::max(wf.scale, 1e-6f) - heel_moved[p][m]);
    Vec3 D{}, best_D{};
    float least = first, best = first;
    int steps = 0;
    for (; steps < 12 && least < want && wi >= 0; steps++) {
        const Vec3 q = pts[(size_t)wi] + D;
        const float e = 0.5f;  // model units
        const Vec3 grad{shape_point(p, q + Vec3{e, 0, 0}) - shape_point(p, q - Vec3{e, 0, 0}),
                        shape_point(p, q + Vec3{0, e, 0}) - shape_point(p, q - Vec3{0, e, 0}),
                        shape_point(p, q + Vec3{0, 0, e}) - shape_point(p, q - Vec3{0, 0, e})};
        if (!(length(grad) > 1e-9f)) break;
        D = D + normalized(grad) * ((want - least) / std::max(wf.scale, 1e-6f));
        if (length(D) > room) D = normalized(D) * room;
        least = worst(D, wi);
        if (least > best) best = least, best_D = D;
    }
    heel_round[p][m]++;
    if (!(length(best_D) > 1e-4f)) {
        heel_done[p][m] = true;
        rt_log("VR: frame %d: %s, the main hand's place (%s): her hand as drawn (frame %d, %d vertices) off the weapon "
               "at the nearest (below 0: in it): %s; its palm, wrist and knuckles %.1f mm off it at the least, and no "
               "move of the hand (%.1f mm left of the 3 cm) takes them farther out: left %s", rt_frame,
               weapon_name(game.weapon), kSide[m], d.frame, d.n, parts, first * 1000.0f, room * wf.scale * 1000.0f,
               r == 0 ? "as fitted" : "where it was moved");
        return;
    }
    heel_extra[p][m] = heel_extra[p][m] + mirrored_dir(wf, best_D);
    heel_moved[p][m] += length(best_D);
    heel_moved_at[p][m] = rt_frame;
    rt_log("VR: frame %d: %s, the main hand's place (%s), move %d: her hand as drawn (frame %d, %d vertices) off the "
           "weapon at the nearest (below 0: in it): %s; its palm, wrist and knuckles %.1f mm off it at the least: the "
           "hand moved %.1f mm out (model %.2f %.2f %.2f, %d steps; %.1f mm in all), those vertices then %.1f mm off it "
           "at the least (her hand to be measured there as drawn)", rt_frame, weapon_name(game.weapon), kSide[m],
           r + 1, d.frame, d.n, parts, first * 1000.0f, length(best_D) * wf.scale * 1000.0f, best_D.x, best_D.y,
           best_D.z, steps, heel_moved[p][m] * wf.scale * 1000.0f, best * 1000.0f);
}

}  // namespace

// ================================================================ shared (sh3vr_internal.h)

// 0.1.10 (owner 172): each skinned draw of hers or of the weapon in her hand, as drawn (sh3vr_shadow_part): the weapon's
// body's pass gives its held bone; hers, her hands' vertices (hand_mesh, above).
void combat_hand_mesh(const uint8_t* v48, int nv, const uint16_t* tris, int ntris, const float* c, int kind) {
    if (!v48 || !c || nv <= 0 || !hand_mesh_wanted()) return;
    if (kind == 1) {
        if (parts_pass() != 0 || mesh_slot < 0 || mesh_slot >= kBones) return;
        Aff H;
        for (int r = 0; r < 3; r++) {
            const float* row = c + (kPaletteStart + mesh_slot * 3 + r) * 4;
            H.r.m[r][0] = row[0], H.r.m[r][1] = row[1], H.r.m[r][2] = row[2];
            (&H.t.x)[r] = row[3];
        }
        mesh_held = H;
        mesh_held_frame = rt_frame;
        return;
    }
    if (kind != 0 || !ik_plain_pass()) return;
    bool ok[2];
    for (int h = 0; h < 2; h++) {
        HandMesh& hm = hand_mesh[h];
        if (hm.frame != rt_frame)
            hm.frame = rt_frame, hm.v.clear(), hm.vt.clear(), hm.rigid.clear(), hm.fv.clear(), hm.fseg.clear(),
            hm.it.clear(), hm.iw.clear(), hm.iq.clear(), hm.tri.clear(), hm.bones = false, hm.draw_nv = 0,
            hm.draw_tri = 0;
        if (!hm.bones) hm.bones = ik_hand_drawn(h, hm.wrist, hm.knuckle, hm.thumb) && ik_fingers_drawn(h, hm.joint);
        ok[h] = hm.bones;
        if (arm_mesh[h].frame != rt_frame) {
            ArmMesh& am = arm_mesh[h];
            am.frame = rt_frame, am.v.clear();
            am.bones = ik_forearm(h, am.elbow, am.wrist), am.shown = hands[h].shown;
        }
    }
    if (!ok[0] && !ok[1]) return;
    int row_hand[46];  // each palette row's hand (-1 neither), looked up as met (-2 not yet)
    bool row_own[46]{};  // ... the hand's own bone
    int row_seg[46];     // ... the finger's segment (finger * 3 + segment; -1 none)
    int row_arm[46];     // ... the forearm it is of (-1 neither; -2 not yet): ArmMesh
    for (int& r : row_hand) r = -2;
    for (int& r : row_arm) r = -2;
    const float one = c[0], scale = c[3] / 255.0f, U = kUnitsPerMetre;
    static std::vector<int> at_hand, at_index;  // (each of the draw's vertices kept: its hand and its index there)
    at_hand.assign((size_t)nv, -1);
    at_index.assign((size_t)nv, -1);
    bool counted[2] = {false, false};  // (this draw's counts added to the hand's: skin_watch)
    for (int i = 0; i < nv; i++) {
        const uint8_t* x = v48 + (size_t)i * 48;
        float f[6];
        std::memcpy(f, x, sizeof f);
        const float wt[4] = {f[3], f[4], f[5], one - one * (f[3] + f[4] + f[5])};
        float to[2] = {0, 0}, own = 0;
        int rows[4];
        for (int k = 0; k < 4; k++) {
            rows[k] = -1;
            if (wt[k] == 0.0f) continue;
            const int a0 = (int)std::floor((float)x[24 + k] * scale + 1e-3f);
            if (a0 < 0 || a0 > 45) continue;
            rows[k] = a0;
            if (row_hand[a0] == -2) {
                const float* r = c + (48 + a0) * 4;
                // (its axes too: a forearm bone shares her wrist's origin, build 286)
                const Vec3 axes[3] = {{r[0], r[4], r[8]}, {r[1], r[5], r[9]}, {r[2], r[6], r[10]}};
                row_hand[a0] = ik_hand_of_bone(Vec3{r[3], r[7], r[11]}, &row_own[a0], &row_seg[a0], axes);
            }
            if (row_hand[a0] >= 0) to[row_hand[a0]] += wt[k], own += row_own[a0] ? wt[k] : 0.0f;
        }
        // (the finger segment it is mostly on, 90 percent or more of its weight: -1 none)
        int seg = -1;
        for (int k = 0; k < 4 && seg < 0; k++) {
            if (rows[k] < 0 || row_hand[rows[k]] < 0 || row_seg[rows[k]] < 0) continue;
            float w = 0;
            for (int m = 0; m < 4; m++)
                if (rows[m] >= 0 && row_hand[rows[m]] == row_hand[rows[k]] && row_seg[rows[m]] == row_seg[rows[k]])
                    w += wt[m];
            if (w >= 0.9f) seg = row_seg[rows[k]];
        }
        const int h = to[0] >= 0.5f ? 0 : to[1] >= 0.5f ? 1 : -1;
        if (h < 0) {  // (her forearm's own skin, half its weight or more on that forearm's bones: ArmMesh; the rig only)
#if !defined(__ANDROID__)
            static const bool arm_wanted = std::getenv("QUEST_SIM_SCRIPT") != nullptr;
#else
            constexpr bool arm_wanted = false;
#endif
            if (!arm_wanted) continue;
            float fa[2] = {0, 0};
            for (int k = 0; k < 4; k++) {
                if (rows[k] < 0) continue;
                const int a0 = rows[k];
                if (row_arm[a0] == -2) {
                    const float* r = c + (48 + a0) * 4;
                    const Vec3 axes[3] = {{r[0], r[4], r[8]}, {r[1], r[5], r[9]}, {r[2], r[6], r[10]}};
                    row_arm[a0] = ik_forearm_of_bone(Vec3{r[3], r[7], r[11]}, axes);
                }
                if (row_arm[a0] >= 0) fa[row_arm[a0]] += wt[k];
            }
            const int s = fa[0] >= 0.5f ? 0 : fa[1] >= 0.5f ? 1 : -1;
            if (s < 0 || !ok[s] || arm_mesh[s].v.size() >= 6000) continue;
            Vec3 q{};
            for (int k = 0; k < 4; k++) {
                if (rows[k] < 0) continue;
                const float* r = c + (48 + rows[k]) * 4;
                q = q + Vec3{r[0] * f[0] + r[1] * f[1] + r[2] * f[2] + r[3], r[4] * f[0] + r[5] * f[1] + r[6] * f[2] + r[7],
                             r[8] * f[0] + r[9] * f[1] + r[10] * f[2] + r[11]} * wt[k];
            }
            if (length(q - hand_mesh[s].wrist) <= 0.40f * U) arm_mesh[s].v.push_back(q);
            continue;
        }
        if (!ok[h] || hand_mesh[h].v.size() >= 6000) continue;
        Vec3 q{}, qk[4];
        for (int k = 0; k < 4; k++) {
            if (rows[k] < 0) continue;
            const float* r = c + (48 + rows[k]) * 4;
            qk[k] = Vec3{r[0] * f[0] + r[1] * f[1] + r[2] * f[2] + r[3], r[4] * f[0] + r[5] * f[1] + r[6] * f[2] + r[7],
                         r[8] * f[0] + r[9] * f[1] + r[10] * f[2] + r[11]};
            q = q + qk[k] * wt[k];
        }
        if (length(q - hand_mesh[h].wrist) > 0.25f * U) continue;
        HandMesh& hm = hand_mesh[h];
        if (!counted[h]) counted[h] = true, hm.draw_nv += nv, hm.draw_tri += ntris;  // (skin_watch)
        at_hand[(size_t)i] = h, at_index[(size_t)i] = (int)hm.v.size();
        for (int k = 0; k < 4; k++) {
            const bool on = rows[k] >= 0;
            // (-1 her hand's own bone, -2 another: her forearm's, which bends against her hand)
            hm.it.push_back((int8_t)(!on ? -1 : row_hand[rows[k]] == h ? row_seg[rows[k]] : -2));
            hm.iw.push_back(on ? wt[k] : 0.0f);
            hm.iq.push_back(on ? qk[k] : q);
        }
        hand_mesh[h].v.push_back(q);
        hand_mesh[h].vt.push_back((int8_t)(seg >= 0 ? seg : own >= 0.9f ? -2 : -1));
        if (own >= 0.9f) hand_mesh[h].rigid.push_back(q);
        if (seg >= 0) hand_mesh[h].fv.push_back(q), hand_mesh[h].fseg.push_back((uint8_t)seg);
    }
    for (int t = 0; tris && t < ntris; t++) {
        const int a = tris[t * 3], b = tris[t * 3 + 1], d = tris[t * 3 + 2];
        if (a >= nv || b >= nv || d >= nv) continue;
        const int h = at_hand[(size_t)a];
        if (h < 0 || at_hand[(size_t)b] != h || at_hand[(size_t)d] != h) continue;
        std::vector<int>& tr = hand_mesh[h].tri;
        tr.push_back(at_index[(size_t)a]), tr.push_back(at_index[(size_t)b]), tr.push_back(at_index[(size_t)d]);
    }
}

int combat_hand_finger_mesh(int h, const Vec3** v, const uint8_t** seg, Vec3 joint[5][3], Vec3& wrist, Vec3 knuckle[4],
                            Vec3 thumb[3]) {
    if (h < 0 || h > 1) return 0;
    const HandMesh& hm = hand_mesh[h];
    if (!hm.bones || hm.fv.empty() || hm.frame < rt_frame - 2) return 0;
    *v = hm.fv.data(), *seg = hm.fseg.data();
    std::memcpy(joint, hm.joint, sizeof hm.joint);
    wrist = hm.wrist;
    for (int k = 0; k < 4; k++) knuckle[k] = hm.knuckle[k];
    for (int k = 0; k < 3; k++) thumb[k] = hm.thumb[k];
    return (int)hm.fv.size();
}

bool combat_hand_drawn(int h, HandDrawn& out) {
    out = HandDrawn{};
    if (h < 0 || h > 1) return false;
    const HandMesh& hm = hand_mesh[h];
    if (!hm.bones || hm.v.empty() || hm.frame < rt_frame - 2 || hm.it.size() != hm.v.size() * 4 ||
        hm.iw.size() != hm.it.size() || hm.iq.size() != hm.it.size())
        return false;
    out.n = (int)hm.v.size(), out.ntri = (int)hm.tri.size() / 3, out.frame = hm.frame;
    out.v = hm.v.data(), out.it = hm.it.data(), out.iw = hm.iw.data(), out.iq = hm.iq.data();
    out.tri = hm.tri.empty() ? nullptr : hm.tri.data();
    out.wrist = hm.wrist;
    for (int k = 0; k < 4; k++) out.knuckle[k] = hm.knuckle[k];
    for (int k = 0; k < 3; k++) out.thumb[k] = hm.thumb[k];
    std::memcpy(out.joint, hm.joint, sizeof hm.joint);
    return true;
}

int combat_hand_mesh_report(int h, char* out, int n) {
    if (!out || n <= 1) return 0;
    HandDepth d;
    if (!hand_depth(h, d, nullptr)) return 0;
    int at = std::snprintf(out, (size_t)n, "her hand as drawn (frame %d, %d vertices) off the weapon at the nearest (below "
                           "0: in it): ", d.frame, d.n);
    at = std::min(std::max(at, 0), n - 1);
    at += hand_parts_text(d, out + at, n - at);
    at = std::min(at, n - 1);
    // (by the bone each vertex is on: the hand's own, the blends between bones, each finger's segments; - none on it)
    auto mm = [&](int b) { return d.bone_count[b] ? d.bone_deep[b] * 1000.0f : 0.0f; };
    if (at < n - 300) {
        at += std::snprintf(out + at, (size_t)(n - at), "; by bone (mm, vertices): its own %.1f (%d), blended %.1f (%d)",
                            mm(15), d.bone_count[15], mm(16), d.bone_count[16]);
        for (int f = 0; f < 5 && at < n - 60; f++) {
            at += std::snprintf(out + at, (size_t)(n - at), ", %c", "TIMRL"[f]);
            for (int j = 0; j < 3; j++) {
                const int b = f * 3 + j;
                at += d.bone_count[b] ? std::snprintf(out + at, (size_t)(n - at), "%s%.1f (%d)", j ? "/" : " ", mm(b),
                                                      d.bone_count[b])
                                      : std::snprintf(out + at, (size_t)(n - at), "%s-", j ? "/" : " ");
            }
        }
    }
    at = std::min(at, n - 1);
    // (session 38, vr549: her forearm's own skin as drawn against the weapon, its nearest and how far behind her wrist
    // along her forearm; ArmMesh, the rig only)
    const ArmMesh& am = arm_mesh[h];
    Aff iH;
    if (at < n - 160 && !am.v.empty() && am.frame == mesh_held_frame && inverse(mesh_held, iH)) {
        float best = 1e9f;
        int in = 0;
        Vec3 near{};
        for (const Vec3& v : am.v) {
            const float c = shape_point(wf.profile, mul(iH.r, v) + iH.t) * wf.scale;
            if (c < 0) in++;
            if (c < best) best = c, near = v;
        }
        // (how far behind her wrist the nearest lies, along her forearm, and how far her forearm's vertices reach: cm)
        Vec3 E, W;
        float behind = -1.0f, lo = 0, hi = 0;
        if (ik_forearm(h, E, W) && length(W - E) > 1e-3f) {
            const Vec3 a = normalized(W - E);
            const float k = 100.0f / kUnitsPerMetre;
            behind = dot(W - near, a) * k, lo = 1e9f, hi = -1e9f;
            for (const Vec3& v : am.v) lo = std::min(lo, dot(W - v, a) * k), hi = std::max(hi, dot(W - v, a) * k);
        }
        at += std::snprintf(out + at, (size_t)(n - at), "; her forearm as drawn off the weapon at the nearest: %.1f mm, "
                            "%.1f cm behind her wrist, %d of %d vertices in it (they reach %.1f to %.1f cm behind it)",
                            best * 1000.0f, behind, in, (int)am.v.size(), lo, hi);
    }
    return std::min(at, n - 1);
}

float combat_forearm_clear(int h, const Pose& pose) {
    if (h < 0 || h > 1 || !wf.placed || wf.profile < 0 || !shape_ready(wf.profile)) return 1e9f;
    const ArmMesh& am = arm_mesh[h];
    Aff iH;
    if (am.v.empty() || !am.bones || am.frame < rt_frame - 2 || am.frame != mesh_held_frame || !inverse(mesh_held, iH))
        return 1e9f;
    // (her wrist in the hand's own frame as it was drawn, then where `pose` puts it)
    const Vec3 l = rotate(conjugate(am.shown.orientation), tracking_point(am.wrist) - am.shown.position);
    const Vec3 W1 = world_point(pose.position + rotate(pose.orientation, l));
    // (her forearm turned about her elbow onto it, stretched or shortened along its line as her arm reaches)
    const Vec3 a = am.wrist - am.elbow, b = W1 - am.elbow;
    const float la = length(a), lb = length(b);
    if (la < 1e-3f || lb < 1e-3f) return 1e9f;
    const Vec3 an = a * (1.0f / la);
    const Quat q = quat_arc(an, b * (1.0f / lb));
    float best = 1e9f;
    for (const Vec3& v : am.v) {
        Vec3 r = v - am.elbow;
        r = r + an * (dot(r, an) * (lb / la - 1.0f));
        const Vec3 v1 = am.elbow + rotate(q, r);
        best = std::min(best, shape_point(wf.profile, mul(iH.r, v1) + iH.t) * wf.scale);
    }
    return best;
}

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
    {   // 0.1.8: the weapons' own shapes (sh3vr_shape.cpp): built once their faces are taken; one just ready is checked
        // against its measured points (the grips' and capsules' lines lie inside it, the trigger's blade in or on it)
        static const char* names[kProfiles];
        for (int i = 0; i < kProfiles; i++) names[i] = kWeapons[i].name;
        const int p = shape_frame(names);
        if (p >= 0 && p < kProfiles) {
            const Hold& hd = kHolds[p];
            const Shape& sp = kShapes[p];
            char line[700];
            int at = std::snprintf(line, sizeof line, "palm point %.1f, second point %.1f; capsules' ends and middles:",
                                   shape_point(p, hd.palm), hd.second ? shape_point(p, hd.second_at) : 0.0f);
            for (int i = 0; i < sp.n && at < (int)sizeof line - 40; i++)
                at += std::snprintf(line + at, sizeof line - at, " %.1f/%.1f/%.1f (r %.1f)", shape_point(p, sp.c[i].a),
                                    shape_point(p, (sp.c[i].a + sp.c[i].b) * 0.5f), shape_point(p, sp.c[i].b), sp.c[i].r);
            if (kTriggers[p].on && at < (int)sizeof line - 40)
                std::snprintf(line + at, sizeof line - at, "; the trigger's blade %.1f/%.1f (r %.1f)",
                              shape_point(p, kTriggers[p].a), shape_point(p, kTriggers[p].b), kTriggers[p].r);
            rt_log("VR: %s's own shape against its measured points (units, negative inside): %s", kWeapons[p].name, line);
        }
    }
    place_weapon();
    hand_skin_step();  // 0.1.10: her palm as drawn, for the hand's own model (once)
    settle_heel();     // 0.1.10: her hand as drawn against the weapon, once per weapon (logged)
    update_swings();
    if (hand_action.frames > 0 && --hand_action.frames == 0 && !hand_action.found && hand_action.hand >= 0) {
        // 0.1.7 (a player's 0.1.6 log: the left hand could use the hall's far door, event 48, again and again and no
        // use was ever logged): a closing hand's use that found nothing says so, with what the hand could use then
        // (every time it could use something; a hand closed on nothing only the first 20 times, so grabs at the air
        // never fill the log)
        static int told_nothing = 0;
        if (hand_action.hover >= 0 || told_nothing < 20) {
            if (hand_action.hover < 0) told_nothing++;
            rt_log("VR: frame %d: the %s hand closed at %.0f %.0f %.0f and the game used nothing there (the hand could "
                   "use %s%d as it closed)%s", rt_frame, hand_action.hand == kLeft ? "left" : "right",
                   hand_action.world.x, hand_action.world.y, hand_action.world.z,
                   hand_action.hover >= 0 ? "event " : "nothing: ", hand_action.hover,
                   hand_action.hover < 0 && told_nothing == 20 ? "; no more of these lines for a hand that could use nothing"
                                                                : "");
        }
        hand_action.hand = -1;
    }
    update_grabbing();
    update_door_hold();  // 0.1.10 (owner 170)
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
    // (0.1.8: the natural spot as moved by the player's own, not where the hand wrapped it: the hand is put where a
    // player's would be)
    Vec3 at = second_natural(wf_one_hand) + second_offset(wf_one_hand);
    const float pump = reload_pump_back();
    if (pump > 0) at = at - wf_one_hand.fwd * (pump * kUnitsPerMetre);
    const Vec3 s = frame_point(wf_one_hand, at);
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
// The PC rig (xr_sim's "grip_spot N", 0.1.7): the VR menu's "Two-hand grip spot" set as the menu would.
void sim_grip_spot(int v) {
    opt.grip_spot = std::clamp(v, 0, 3);
    rt_log("[sim] Two-hand grip spot set to %d", opt.grip_spot);
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
    Vec3 ma = model_of(wf, a), mb = model_of(wf, b);
    float best = 1e9f;
    const int mh = main_hand();
    if (weapon_mesh_on() && shape_ready(wf.profile)) {
        // 0.1.8: the weapon's own shape (sh3vr_shape.cpp); the other hand on the shotgun's pump as it is drawn back
        // meets the pump where it is (the shape at rest, the hand's query moved forward with it)
        const float pump = hand != mh && hand_role[hand] == 2 ? reload_pump_back() : 0.0f;
        if (pump > 0) ma = ma + wf.fwd * (pump * kUnitsPerMetre), mb = mb + wf.fwd * (pump * kUnitsPerMetre);
        // (0.1.10: a point, one sample, as shape_segment takes it: her hand's skin, a vertex at a time)
        best = ma.x == mb.x && ma.y == mb.y && ma.z == mb.z ? shape_point(wf.profile, ma)
                                                           : shape_segment(wf.profile, ma, mb);
    } else {
        Cap caps[kCapMax];
        const int nc = shape_caps(wf, caps);
        for (int i = 0; i < nc; i++) best = std::min(best, seg_seg(ma, mb, caps[i].a, caps[i].b) - caps[i].r);
    }
    if (hand != mh && hand_role[mh] == 1 && native_shown[mh] < 0 && grip_fit_mode() != 0) {
        // 0.1.8: her main hand's own fingers and palm as drawn round the grip (the other hand wraps them, never through)
        Vec3 ha[18], hb[18];
        float hr[18];
        const int nh = body_hand_capsules(mh, ha, hb, hr, 18);
        float closest = best * wf.scale;
        for (int i = 0; i < nh; i++) closest = std::min(closest, seg_seg(a, b, ha[i], hb[i]) - hr[i]);
        // (session 38: and, in the grips' fit, her main hand's skin as drawn, each vertex 2.5 mm round, its vertices
        // about 5 mm apart; the capsules, inside it, stand for its inside: body_hand_skin_points. The fit alone: in play
        // its 400 vertices against every query of the other hand's fingers each frame would cost the headset's frame)
        // Session 38 (vr536, build 300: the maul's other hand, 0.3 mm off its shaft in vr533's fit, came 3.3 mm into it
        // as played: in the fit its thumb rested against her main hand's skin, as played it closes on to 0.65 and the
        // skin between it and her palm goes into the shaft): SH3_FIT_PLAY=1 (the rig) leaves this out of the fit too,
        // so the fit poses and reads the other hand exactly as it is played; her main hand as drawn is still kept out
        // of it by the fit's reading of her whole hand (hand_skin_between, body_grip_mesh).
        static const bool as_played = std::getenv("SH3_FIT_PLAY") != nullptr;
        const Vec3* sk = nullptr;
        Vec3 lo, hi;
        const int ns = grip_fit_mode() == 1 && !as_played ? body_hand_skin_points(mh, &sk, &lo, &hi) : 0;
        if (ns > 0) {
            const Vec3 gap{std::max(0.0f, std::max(lo.x - std::max(a.x, b.x), std::min(a.x, b.x) - hi.x)),
                           std::max(0.0f, std::max(lo.y - std::max(a.y, b.y), std::min(a.y, b.y) - hi.y)),
                           std::max(0.0f, std::max(lo.z - std::max(a.z, b.z), std::min(a.z, b.z) - hi.z))};
            if (length(gap) - 0.0025f < closest)
                for (int i = 0; i < ns; i++) closest = std::min(closest, seg_seg(a, b, sk[i], sk[i]) - 0.0025f);
        }
        return closest;
    }
    if (hand != mh) {
        // The main hand's fist round the grip (its fingers about 1.8 cm over the grip's surface).
        const Hold& hd = *wf.hold;
        best = std::min(best, seg_seg(ma, mb, wf.palm - hd.rod * 24.0f, wf.palm + hd.rod * 24.0f) - 20.0f);
    }
    return best * wf.scale;
}
bool grip_fit_mode_on() { return grip_fit_mode() != 0; }
GripTrigger weapon_trigger_at(float pull) {
    GripTrigger t;
    if (!wf.placed || wf.profile < 0 || !kTriggers[wf.profile].on || !weapon_is_gun()) return t;
    const int p = wf.profile;
    const TriggerBlade& k = kTriggers[p];
    const Quat q = trigger_turn(p, wf.fwd, pull);
    t.on = true;
    t.a = frame_point(wf, trigger_moved(p, q, k.a));
    t.b = frame_point(wf, trigger_moved(p, q, k.b));
    t.r = k.r * wf.scale;
    t.pad = kTriggerPad[p];
    t.up = frame_dir(wf, wf.up);
    return t;
}
GripTrigger weapon_trigger() { return weapon_trigger_at(0.0f); }
// Session 31: how far (degrees) the trigger of the gun in the main hand is turned as drawn: as far as her trigger
// finger, drawn, has pushed it (body_trigger_shown); 0 in the third-person view (the game's own hands).
float weapon_trigger_turn() {
    if (!wf.placed || !in_hand || wf.profile < 0 || !kTriggers[wf.profile].on || !weapon_is_gun() ||
        view_mode != ViewMode::kFirstPerson)
        return 0.0f;
    return body_trigger_shown(main_hand()) * kTriggers[wf.profile].turn;
}
bool weapon_trigger_place(uint32_t nv, uint32_t np, float m[12]) {
    for (int i = 0; i < 12; i++) m[i] = i % 5 == 0 ? 1.0f : 0.0f;
    const WeaponInfo* w = info_of(game.weapon);
    if (!w || weapon_trigger_turn() <= 0.0f) return false;
    bool same = false;
    for (int s = 0; s < w->sigs && s < 4; s++) same |= w->nv[s] == nv && w->np[s] == np;
    if (!same) return false;  // (not the gun in her hand)
    const int p = wf.profile;
    const Quat q = trigger_turn(p, wf.fwd, weapon_trigger_turn() / kTriggers[p].turn);
    const Vec3 X = rotate(q, Vec3{1, 0, 0}), Y = rotate(q, Vec3{0, 1, 0}), Z = rotate(q, Vec3{0, 0, 1});
    const Vec3 t = kTriggers[p].pin - rotate(q, kTriggers[p].pin);
    const float r[3][4] = {{X.x, Y.x, Z.x, t.x}, {X.y, Y.y, Z.y, t.y}, {X.z, Y.z, Z.z, t.z}};
    for (int i = 0; i < 3; i++)
        for (int j = 0; j < 4; j++) m[i * 4 + j] = r[i][j];
    return true;
}
int weapon_hand_role(int hand) { return hand >= 0 && hand < 2 ? hand_role[hand] : 0; }
// Session 36: what the fingers on the weapon meet (sh3vr_body.cpp's wrap kept while it is the same): the profile held
// and whether its own shape is ready (-1: no weapon placed).
int weapon_shape_key() {
    if (!wf.placed || wf.profile < 0) return -1;
    return wf.profile * 2 + (weapon_mesh_on() && shape_ready(wf.profile) ? 1 : 0);
}
// The other hand's share of the second grip (0 at its controller .. 1 on the grip; 0 for the main hand): letting go it
// eases back to the controller over about 0.15 s (session 17: the body's surface hold leaves it alone until then).
float weapon_two_share(int hand) { return hand == off_hand() ? two_w : 0.0f; }
int native_hand_group(int hand) { return hand >= 0 && hand < 2 && wf.placed ? native_shown[hand] : -1; }
float weapon_hold_curl(int hand) {
    if (!wf.placed) return 0.8f;
    if (hand_role[hand] != 2) return wf.hold->curl;
    const SecondTest* t = second_test(wf.profile);
    return t && t->curl >= 0 ? t->curl : wf.hold->second_curl;
}
float weapon_hold_thumb(int hand) {
    if (!wf.placed || hand_role[hand] != 2) return -1.0f;
    const SecondTest* t = second_test(wf.profile);
    return t && t->thumb >= 0 ? t->thumb : wf.hold->second_thumb;
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
    if (door_hold.hand == h) return false;  // 0.1.10: holding a door's handle
    for (int k = 0; k < kHolsterCount; k++)
        if (holster_on(k) && holsters[k].hovered[h]) return false;
    if (!hand_reaching(s)) return false;
    world = world_point(s.shown.position + rotate(s.shown.orientation, Vec3{0, -0.06f, 0}));
    return true;
}

// What hand_reach_point's check found (an event, a floor item as 0x400 + its place, or -1): a light tick when a hand
// comes onto something it can use ("Extra vibration": pickups and doors), once until it leaves it.
void hand_hover(int h, int event, bool door) {
    int* on = hover_event;
    if (h < 0 || h > 1) return;
    hover_door[h] = event >= 0 && door;  // (0.1.10)
    const bool by_hand = hover_door[h] && opt.door_by_hand;  // (0.1.10: take_by_hand is on, or no hand hovers)
    // (0.1.10: the door's hint, once, as soon as the label is free while a hand is on a door; vr492: at the first
    // touch another line was up, so it waited for the second)
    if (by_hand) tip_event(kTipDoor);
    if (event == on[h]) return;
    if (event >= 0) {
        rt_log("VR: frame %d: the %s hand can use event %d (%s)", rt_frame, h == kLeft ? "left" : "right", event,
               by_hand ? "a door: gripping its handle and pulling it opens it" : "closing it uses it");
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

// 0.1.7 (a player on 0.1.6: "Sometimes it's not very clear if one has properly interacted with a door or object"): the
// action button found something the game uses now (a door, an item, a thing to examine): a soft knock and tick from the
// hand that pressed it (A: the right controller's) and a short buzz in it, before the game's own door, chime or message.
void button_action_found(int event, int pass) {
    const int h = kRight;
    const Vec3 at = hands[h].valid ? world_point(hands[h].grip.position) : world_point(fi.head.position);
    rt_log("VR: frame %d: the action button used event %d (pass %d): a tick and a buzz in the %s hand", rt_frame, event,
           pass, h == kLeft ? "left" : "right");
    buzz(h, 0.40f, 0.05f, 160.0f);
    hand_sound(kSoundUse, at);
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

// 0.1.9 (owner 164): the weapon drawn again in a mirror's reflection of her: its bones turned over (the first whole
// bone's determinant below 0), as the game reflects them about the glass.
static bool weapon_palette_mirrored(const float vsc[96][4], int slots) {
    for (int k = 0; k < slots; k++) {
        if (!bone_affine(vsc, k)) continue;
        const R3 r = read_bone(vsc, k).r;
        const float d = r.m[0][0] * (r.m[1][1] * r.m[2][2] - r.m[1][2] * r.m[2][1]) -
                        r.m[0][1] * (r.m[1][0] * r.m[2][2] - r.m[1][2] * r.m[2][0]) +
                        r.m[0][2] * (r.m[1][0] * r.m[2][1] - r.m[1][1] * r.m[2][0]);
        return d < -0.25f;
    }
    return false;
}
// ... never taken for the one in her hand (its place would set the shots', the effects' and the hold's numbers from the
// mirror's side of the glass). With the mirror's map X (sh3vr_ik.cpp ik_mirror_bones, its reflection of her this frame
// or the last) and the hand's map of this frame or the last (game_to_hand, from the weapon in her hand), it is placed
// as that one is, through the reflection: each bone back through the glass, moved as the one in the hand, the pass's
// part with it, then reflected again (B' = X rel X^-1 B): the reflection holds it where she does. Holstered, collapsed
// as the one in her hand. Without those maps, left as the game drew it. 3: a reflection's weapon (marked as hers in
// the stencil, never her shadow's caster: d3d8.c).
static int mirror_weapon_draw(float vsc[96][4], int slots) {
    if (!in_hand) {
        for (int b = 0; b < slots; b++)
            if (bone_affine(vsc, b)) {
                Aff z{};
                z.t = read_bone(vsc, b).t;
                write_bone(vsc, b, z);
            }
        return 3;
    }
    float x[16];
    Aff X, iX;
    if (!ik_mirror_map(x) || game_to_hand_frame < 0 || rt_frame - game_to_hand_frame > 1) return 3;
    for (int i = 0; i < 3; i++)
        for (int j = 0; j < 3; j++) X.r.m[i][j] = x[j * 4 + i];  // row-vector rows -> column form
    X.t = Vec3{x[12], x[13], x[14]};
    if (!inverse(X, iX)) return 3;
    for (int k = 0; k < slots; k++)
        if (bone_finite(vsc, k)) write_bone(vsc, k, mul(game_to_hand, mul(iX, read_bone(vsc, k))));
    apply_pass(vsc, slots);
    for (int k = 0; k < slots; k++)
        if (bone_finite(vsc, k)) write_bone(vsc, k, mul(X, read_bone(vsc, k)));
    static int told = -100000;
    if (rt_frame - told >= 600) {
        told = rt_frame;
        rt_log("VR: frame %d: the weapon in her reflection (a mirror) held as the one in her hand, through the "
               "reflection", rt_frame);
    }
    return 3;
}

int combat_weapon_draw(uint32_t nv, uint32_t np, uint32_t stride, int pal_count, const int* pal_bone, float vsc[96][4]) {
    weapon_draw_now = false;
    mesh_slot = mesh_sig = -1;
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
    // 0.1.9 (owner 167, a player's 0.1.7 video at 11:21, the store's back room, "Stay back!": "during a cutscene, a
    // pistol didnt render in the 1st person view of her shooting, but it rendered in the 3rd person one"): a scene that
    // draws her away from her place with the eyes in her head (ik_scene_eyes) poses her hands and the weapon in them
    // itself, but the weapon was still moved to the controller (nowhere near the scene's hands) or, holstered, put away
    // (nothing drawn). Now it is left where the scene's hands hold it, marked as hers as the weapon in her hand is (her
    // own shadows leave it alone, it casts its shadow with her body; faded only right at the eyes); the beam saber's
    // blade, a draw of its own, stays on its hilt. A part of VR's own falling still shows where it is.
    if (w && view_mode == ViewMode::kFirstPerson && ik_scene_eyes()) {
        const bool blade =
            game.weapon == 9 && nv == 24 && np == 30 && pal_count == 1 && (!pal_bone || pal_bone[0] == 1);
        if (blade || weapon_signature(nv, np)) {
            weapon_draw_now = true;
            PassPlace pp;
            if (!blade && parts_pass() > 0 && parts_pass_place(parts_pass(), pp) && pp.kind == 2)
                apply_pass(vsc, pal_count > 0 ? std::min(pal_count, kBones) : kBones);
            static int told = -100000;
            if (rt_frame - told >= 600) {
                told = rt_frame;
                rt_log("VR: frame %d: a scene draws her with the eyes in her head: %s%s (weapon %d) left in the scene's "
                       "hands", rt_frame, w->name, blade ? "'s blade" : "", game.weapon);
            }
            return blade ? 2 : 1;
        }
    }
    if (w && view_mode == ViewMode::kFirstPerson && map_ready() && saber_blade_draw(nv, np, pal_count, pal_bone, vsc)) {
        weapon_draw_now = true;  // faded as the weapon is (near the eyes only), not as her body
        // 2: a glow, so d3d8.c keeps it out of her own-mesh shadow (a beam of light casts none), its pixels still
        // marked as the weapon's (not darkened by the game's shadows)
        return 2;
    }
    if (!w || view_mode != ViewMode::kFirstPerson || !map_ready() || !weapon_signature(nv, np)) return 0;
    if (weapon_palette_mirrored(vsc, pal_count > 0 ? std::min(pal_count, kBones) : kBones))
        return mirror_weapon_draw(vsc, pal_count > 0 ? std::min(pal_count, kBones) : kBones);
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
                // (0.1.8: this draw's held slot and which of the weapon's draws it is, for its own shape)
                mesh_slot = held;
                for (int i = 0; i < w->sigs; i++)
                    if (w->nv[i] == nv && w->np[i] == np) mesh_sig = i;
            }
        }
    }
    changed |= apply_pass(vsc, slots);
    return changed ? 1 : 0;
}

// 0.1.8 (owner messages 146-148: the fingers' collision on the weapons too, "guns/weapons/melee weapons"): the weapon's
// faces for its own shape (sh3vr_shape.cpp), from a draw of it just made in the hand (d3d8.c her_shadow_capture, by
// sh3vr_shadow_part): each vertex skinned as the game's vertex shader skins it (sh3vr_ik.cpp shadow_part's way: three
// weights and a fourth, each a palette slot) with the constants it was drawn with, then into the held bone's model units
// by the inverse of the held slot as drawn, each face wound to its vertices' normals' side (outward). Taken once the
// weapon has been in the hand for 36 frames running, whole and at rest: no reload under way (no part out of it or in a
// hand, the slide or pump forward), no shot within 0.3 s of its flash; its own moving part at rest, its trigger not
// pulled.
void combat_weapon_mesh(const uint8_t* v48, int nv, const uint16_t* tris, int nt, const float* c, uint32_t key) {
    const WeaponInfo* w = info_of(game.weapon);
    if (!w || !wf.placed || !in_hand || mesh_slot < 0 || mesh_slot >= kBones || mesh_sig < 0 || !v48 || !tris || !c ||
        nv <= 0 || nt <= 0 || wf.profile < 0)
        return;
    const int p = wf.profile;
    if (!shape_wanted(p) || placed_frames < 36 || placed_weapon != game.weapon || now < flash_until + 0.3f ||
        reload_blocks_fire() || reload_owns_hand(kLeft) || reload_owns_hand(kRight) || reload_pump_back() > 0.0f)
        return;
    // (session 31: every pass of it at rest in the frame taken, as the shape takes one frame's draws: its moving part
    // forward and its trigger not pulled past 2 degrees (one turned less is taken where it is drawn, a fraction of a
    // millimetre off); before, a frame with the slide held back took the frame without the slide)
    if (weapon_trigger_turn() > 2.0f) return;
    for (int k = 1; k < 8; k++) {
        PassPlace pp;
        if (!parts_pass_place(k, pp)) break;
        if (pp.kind != 1) continue;
        const float id[12] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0};
        for (int i = 0; i < 12; i++)
            if (std::fabs(pp.m[i] - id[i]) > (i % 4 == 3 ? 0.01f : 1e-4f)) return;  // the moving part not at rest
    }
    if (const int pass = parts_pass()) {
        PassPlace pp;
        if (!parts_pass_place(pass, pp) || pp.kind == 2) return;  // a part in a hand or falling: not the weapon
    }
    Aff H, iH;
    for (int r = 0; r < 3; r++) {
        const float* row = c + (kPaletteStart + mesh_slot * 3 + r) * 4;
        H.r.m[r][0] = row[0], H.r.m[r][1] = row[1], H.r.m[r][2] = row[2];
        (&H.t.x)[r] = row[3];
    }
    if (!inverse(H, iH)) return;
    static std::vector<Vec3> pos, nrm;
    static std::vector<float> faces;
    pos.resize((size_t)nv);
    nrm.resize((size_t)nv);
    const float one = c[0], scale = c[3] / 255.0f;
    for (int i = 0; i < nv; i++) {
        const uint8_t* x = v48 + (size_t)i * 48;
        float f[12];
        std::memcpy(f, x, sizeof f);
        const float wt[4] = {f[3], f[4], f[5], one - one * (f[3] + f[4] + f[5])};
        Vec3 q{}, n{};
        for (int k = 0; k < 4; k++) {
            if (wt[k] == 0.0f) continue;
            const int a0 = (int)std::floor((float)x[24 + k] * scale + 1e-3f);
            if (a0 < 0 || a0 > 45) continue;
            const float* r = c + (48 + a0) * 4;
            q = q + Vec3{r[0] * f[0] + r[1] * f[1] + r[2] * f[2] + r[3], r[4] * f[0] + r[5] * f[1] + r[6] * f[2] + r[7],
                         r[8] * f[0] + r[9] * f[1] + r[10] * f[2] + r[11]} * wt[k];
            n = n + Vec3{r[0] * f[7] + r[1] * f[8] + r[2] * f[9], r[4] * f[7] + r[5] * f[8] + r[6] * f[9],
                         r[8] * f[7] + r[9] * f[8] + r[10] * f[9]} * wt[k];
        }
        pos[(size_t)i] = mul(iH.r, q) + iH.t;
        nrm[(size_t)i] = mul(iH.r, n);
        if (!std::isfinite(pos[(size_t)i].x) || !std::isfinite(pos[(size_t)i].y) || !std::isfinite(pos[(size_t)i].z)) return;
    }
    faces.clear();
    faces.reserve((size_t)nt * 9);
    for (int t = 0; t < nt; t++) {
        int a = tris[t * 3], b = tris[t * 3 + 1], cc = tris[t * 3 + 2];
        if (a >= nv || b >= nv || cc >= nv) continue;
        const Vec3 g = cross(pos[(size_t)b] - pos[(size_t)a], pos[(size_t)cc] - pos[(size_t)a]);
        if (dot(g, g) < 1e-10f) continue;  // no area
        if (dot(g, nrm[(size_t)a] + nrm[(size_t)b] + nrm[(size_t)cc]) < 0) std::swap(b, cc);
        for (int v : {a, b, cc}) faces.push_back(pos[(size_t)v].x), faces.push_back(pos[(size_t)v].y), faces.push_back(pos[(size_t)v].z);
    }
    if (!faces.empty()) shape_add(p, mesh_sig, w->sigs, key, faces.data(), (int)(faces.size() / 9));
}

// LIST 24 (session 13): the extra weapons (unlimited submachine gun, flamethrower, beam saber...: slots 9, 10, 13, 14,
// 17) have no mesh signature, so combat_weapon_draw leaves them alone and they hung in her ANIMATED hand, away from
// the controller. Their model's bones, as the game posed them (sh3.c's 0x45EC00 wrapper, a small model while a
// weapon is out), are carried with her hand to her IK hand at the controller (ik_hand_carry: from the game's hand bone
// to VR's), the way the game holds them kept; holstered, collapsed to a point as the known weapons are. Only for a
// model whose bone 0 sits on her animated right hand (where the game hangs every weapon).
int combat_weapon_bones(float* bones, int n) {
    // (0.1.9, owner 167: in a scene that draws her with the eyes in her head the weapon stays in the scene's hands)
    if (n < 1 || n > 4 || view_mode != ViewMode::kFirstPerson || !map_ready() || game.weapon <= 0 || info_of(game.weapon) ||
        ik_scene_eyes())
        return 0;
    float m0[16];
    std::memcpy(m0, bones, sizeof m0);
    {
        // 0.1.9 (owner 164): the same weapon in a mirror's reflection of her (its bone turned over): carried through the
        // reflection to the reflection's hand (sh3vr_ik.cpp ik_mirror_carry), never taken for the one on her hand
        const float d = m0[0] * (m0[5] * m0[10] - m0[6] * m0[9]) - m0[1] * (m0[4] * m0[10] - m0[6] * m0[8]) +
                        m0[2] * (m0[4] * m0[9] - m0[5] * m0[8]);
        if (d < -0.25f) return ik_mirror_carry(kRight, bones, n, in_hand) ? 1 : 0;
    }
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

// testing (0.1.7, the PC rig, sh3.c's SH3_HOLD_HIT): one sweep of the knife's attack (as learned from the game; else the
// nearest known) sent to the game's battle step the way a swing's contact sweep is (queue_extra), so the game takes it
// as a hit of Heather's through its own test, damage and hit records.
int combat_test_sweep(const float* pg, const float* pt, const float* cg, const float* ct, float radius) {
    if (!learned_loaded) load_learned();
    const int ls = learned[4].id >= 0 ? 4 : borrow(false);
    const bool sent = queue_extra(ls, Vec3{pg[0], pg[1], pg[2]}, Vec3{pt[0], pt[1], pt[2]}, Vec3{cg[0], cg[1], cg[2]},
                                  Vec3{ct[0], ct[1], ct[2]}, radius);
    rt_log("hold test: frame %d: a sweep of attack %d (kind %d, learned slot %d) %s", rt_frame,
           ls >= 0 ? learned[ls].id : -1, ls >= 0 ? learned[ls].kind : -1, ls, sent ? "sent to the battle step" : "not sent");
    return sent ? 1 : 0;
}

// 0.1.7 (a player on 0.1.6: "The enemies seem to take an absurd amount of knifing to kill"): the game's own damage is
// light for a swing that took a whole arm (its table 0x6CE170, 24 bytes a kind: the knife's slashes 30, its strong 40;
// the hall's dog has about 712 health, so 25 stabs, vr242; in the game a button mashed them). Physical melee and punches
// deal "Melee damage" of it (200%), and a swing faster than its kind asks up to half as much again (at twice the speed).
// The characters a swing hit, with that swing's own factor, until the game takes the damage (sh3vr_hit_given).
namespace {
struct MeleeHit {
    uint32_t character = 0;
    double at = -10;
    float speed_factor = 1.0f;
};
MeleeHit melee_hits[4];
float swing_speed_factor(int what, float speed) {
    const float ask = thresholds(what, false).speed;
    if (!(ask > 0.0f) || !std::isfinite(speed)) return 1.0f;
    return 1.0f + 0.5f * std::clamp(speed / ask - 1.0f, 0.0f, 1.0f);
}
// Owner message 128 ("make sure the melee damage option is on by default and adjusted to make combat feel better in
// VR"): in VR a knife's slash or a punch takes a whole arm, as a pipe's swing does, and the game's knife deals a third
// of the pipe's (its table: 40 against 130), so the light weapons hit half as hard again on top: at the default 200%
// the hall's dog takes about 6 slashes of the knife (4 hard ones), 3 swings of the pipe (2 hard ones), 12 punches.
float melee_class_factor(int what) { return what == kLight || what == kUnknown ? 1.5f : 1.0f; }
void note_melee_hit(uint32_t character, float factor) {
    MeleeHit* slot = &melee_hits[0];
    for (MeleeHit& m : melee_hits) {
        if (m.character == character) {
            slot = &m;
            break;
        }
        if (m.at < slot->at) slot = &m;
    }
    *slot = {character, now, factor};
}
}  // namespace

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
        if (pending.armed) {  // 0.1.7: the swing's own game attack ("Melee damage")
            const WeaponInfo* pw = info_of(pending.slot);
            note_melee_hit(character, pw ? melee_class_factor(pw->kind) : 1.0f);
        }
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
    note_melee_hit(character, swing_speed_factor(s.what, s.speed) * melee_class_factor(s.what));  // 0.1.7: "Melee damage"
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
    // 0.1.7 (owner 140, the PC VR mod's maker: knife stabs spammed broke the game's animations; their mod's fix: a
    // cooldown a weapon, and no new swing while a heavy one's sequence or its hit is pending): this press keeps the same
    // limits as a swing that counted (update_swings), so contact hits never press the game's attack faster than it.
    const float landed_cooldown = s.slot == 7 ? 0.56f : thresholds(s.what).cooldown;
    const bool landed_busy = s.what == kHeavy && (now < still_until || pending.armed);
    if ((s.what == kLight || s.what == kHeavy) && !sw.fired && s.hand == main_hand() && s.slot == game.weapon &&
        !landed_busy && now - sw.last >= landed_cooldown) {
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
    bool fist = false;
    for (FistHit& f : fist_hits)
        if (f.character == character && now - f.at <= 1.0) {
            keep = 0.5f;
            fist = true;
            f = {};
            break;
        }
    // 0.1.7: a swing's or a punch's hit (never a shot): "Melee damage" of the game's, a harder swing more
    float melee = 0.0f;
    for (MeleeHit& m : melee_hits)
        if (m.character == character && now - m.at <= 1.0) {
            melee = m.speed_factor;
            m = {};
            break;
        }
    if (melee > 0.0f) keep *= std::clamp(opt.melee_damage_pct, 50.0f, 400.0f) * 0.01f * melee;
    char why[96] = "";
    if (melee > 0.0f)
        std::snprintf(why, sizeof why, " (%s: %.0f%% of the game's, the swing's speed and kind x%.2f)",
                      fist ? "a fist's, half of a knife's" : "melee", keep * 100.0f, melee);
    rt_log("VR: frame %d: %08X took %.1f damage from her%s (reaction %d) at %.0f %.0f %.0f, the way %.2f %.2f %.2f",
           rt_frame, character, damage * keep, melee > 0.0f ? why : fist ? " (a fist's: half)" : "", reaction, point[0],
           point[1], point[2], way[0], way[1], way[2]);
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
        tip_moment_over(kTipHeld);
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
        tip_event(kTipHeld);  // 0.1.7 (owner message 143): how to get free, the first holds
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
    tip_moment_over(kTipHeld);  // 0.1.7: the break-free hint ends with the hold
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

StruggleBar struggle_bar() {
    StruggleBar b;
    b.held = struggling;
    b.start = struggle_left_first;
    b.left = struggle_left;
    b.presses = struggle_given + struggle_buttons;
    b.freed = struggle_freed;
    b.holds = struggle_holds;
    return b;
}

}  // namespace sh3q
