// SPDX-License-Identifier: GPL-3.0-or-later
package dev.sh3quest.vr;

import android.os.Build;

/**
 * Every option the launcher shows, with the words players use, and the key each one has in sh3vr.ini (the same keys
 * and defaults as the in-game VR menu: src/vr/sh3vr.cpp kRows and sh3vr_internal.h Options). The defaults are the
 * recommended ones: every VR feature on, the performance options best for the Quest 3 (LIST 7, 17). sh3.ini's one
 * option the headset needs (the game's threads) is here too.
 */
class Options {
    enum Kind { SWITCH, CHOICE, NUMBER }

    static final class Option {
        final String key, label, help, page;
        final Kind kind;
        final String[] names, values;  // CHOICE: what each is called and what sh3vr.ini holds
        final float low, high, step;   // NUMBER
        final String unit;
        final String fallback;         // the default, as sh3vr.ini writes it
        final boolean nextStart;       // takes effect when the game starts again (already so from the launcher)
        final String file;             // "sh3vr.ini" or "sh3.ini"

        private Option(String file, String key, String label, String page, Kind kind, String[] names, String[] values,
                float low, float high, float step, String unit, String fallback, boolean nextStart, String help) {
            this.file = file;
            this.key = key;
            this.label = label;
            this.page = page;
            this.kind = kind;
            this.names = names;
            this.values = values;
            this.low = low;
            this.high = high;
            this.step = step;
            this.unit = unit;
            this.fallback = fallback;
            this.nextStart = nextStart;
            this.help = help;
        }
    }

    static final String VR_INI = "sh3vr.ini", GAME_INI = "sh3.ini";

    /** 0.1.3 (owner 84): the defaults' version sh3vr.ini says it has ("defaults = 3"; none before 0.1.3). A file from
     *  an older version had 0.1.2's defaults saved in it, so 0.1.3's changed defaults are put in once (Config.reload;
     *  the game does the same in sh3vr.cpp load_options): these keys, the drifting fog, smooth turning and no screen
     *  filters (no sharpening, film grain, smooth edges or comfort vignette; the picture at the game's own). */
    static final String DEFAULTS_KEY = "defaults";
    /** 4 from 0.1.4 (Smoother frames on); 5 from 0.1.5 (Smoother frames off, Render resolution 1.0x, ghost hands and
     *  Heather's shadow off):
     *  Config.newDefaults puts each version's changes in once, as sh3vr.cpp load_options does. */
    static final int DEFAULTS_VERSION = 5;
    /** 0.1.5 (the owner's 0.1.4 report: "keep appsw off by default going forward"): Smoother frames (AppSW) off to start
     *  (sh3vr_internal.h space_warp the same); 0.1.4 had it on at 120 Hz, which the game could not feed. */
    static final boolean SPACE_WARP_ON = false;
    /** The refresh rate to start (sh3vr_internal.h refresh_hz the same). Owner 98 ("set performance to a default you
     *  would see works best"): 120 Hz, as the game takes 16-17 ms a frame on a Quest 3 (the owner's 0.1.3 and 0.1.4
     *  logs), two 120 Hz refreshes, an even ~60 a second; at 72 Hz each would take two refreshes (36), at 90 two (45). */
    static final String REFRESH_DEFAULT = "120";
    static final String[] DEFAULTS_0_1_3 = {"sharpen", "film_grain", "drifting_fog", "brightness", "contrast",
        "saturation", "dark_edges", "smooth_edges", "turn", "vignette"};

    /** A Quest 3 or 3S (Android's device names eureka and panther). 0.1.0-0.1.4 started it at the top Render resolution,
     *  1.5x; 0.1.5 (owner 97: "set default resolution back to 100% for this release") starts every headset at 1.0x, and
     *  Config.newDefaults moves a Quest 3's saved 1.5x (the old default) to 1.0x once (sh3vr.cpp the same). */
    static final boolean STRONG_QUEST = "eureka".equals(Build.DEVICE) || "panther".equals(Build.DEVICE);

    private static Option sw(String key, String label, String page, boolean on, String help) {
        return new Option(VR_INI, key, label, page, Kind.SWITCH, new String[] {"On", "Off"}, new String[] {"1", "0"},
            0, 0, 0, "", on ? "1" : "0", false, help);
    }

    private static Option choice(String key, String label, String page, String[] names, String[] values, String fallback,
            String help) {
        return new Option(VR_INI, key, label, page, Kind.CHOICE, names, values, 0, 0, 0, "", fallback, false, help);
    }

    private static Option choice(String key, String label, String page, String[] names, int fallback, String help) {
        String[] values = new String[names.length];
        for (int i = 0; i < names.length; i++) {
            values[i] = Integer.toString(i);
        }
        return choice(key, label, page, names, values, Integer.toString(fallback), help);
    }

    private static Option number(String key, String label, String page, float low, float high, float step, String unit,
            float fallback, String help) {
        return new Option(VR_INI, key, label, page, Kind.NUMBER, null, null, low, high, step, unit, format(fallback),
            false, help);
    }

    /** As sh3vr.cpp writes a number (%g). */
    static String format(float value) {
        if (value == Math.rint(value)) {
            return Integer.toString((int) value);
        }
        String text = String.format(java.util.Locale.US, "%.4f", value);
        while (text.endsWith("0")) {
            text = text.substring(0, text.length() - 1);
        }
        return text.endsWith(".") ? text.substring(0, text.length() - 1) : text;
    }

    static final String PERFORMANCE = "Performance", VIEW = "View", COMFORT = "Comfort", POINTER = "Pointer",
        HANDS = "Hands and weapons", VIBRATION = "Vibration";
    static final String[] PAGES = {PERFORMANCE, VIEW, COMFORT, POINTER, HANDS, VIBRATION};

    static final Option VR = new Option(VR_INI, "vr", "VR", VIEW, Kind.SWITCH, new String[] {"On", "Off"},
        new String[] {"1", "0"}, 0, 0, 0, "", "1", false,
        "Off plays the game flat on a big screen (also tells a problem with the game from a problem with VR).");

    static final Option[] ALL = {
        // Performance: the Quest 3's best by default
        new Option(VR_INI, "refresh_hz", "Refresh rate", PERFORMANCE, Kind.CHOICE,
            new String[] {"72 Hz", "80 Hz", "90 Hz", "120 Hz"}, new String[] {"72", "80", "90", "120"}, 0, 0, 0, "",
            REFRESH_DEFAULT, true, "120 Hz: the game makes up to 60 frames a second, so each one shows twice, evenly, "
            + "and the headset keeps your head's turns smooth between them. At 72 or 90 Hz a frame that takes a little "
            + "too long waits a whole refresh more."),
        number("resolution", "Render resolution", PERFORMANCE, 0.6f, 1.5f, 0.05f, "x", 1.0f,
            "1.0 is the headset's own resolution. Lower is faster, higher is sharper (a Quest 3 or 3S has room for "
            + "more). Dynamic resolution keeps the game smooth either way."),
        sw("dynamic_resolution", "Dynamic resolution", PERFORMANCE, true,
            "When the headset starts falling behind, the game's 3D picture is drawn a little smaller until it keeps up "
            + "again, so frames are not dropped. Menus and text stay sharp."),
        choice("foveation", "Foveated rendering", PERFORMANCE, new String[] {"Off", "Low", "Medium", "High", "Highest"}, 2,
            "Draws the edges of the view with fewer pixels. Medium is hard to notice and saves GPU time."),
        sw("boost", "CPU and GPU boost", PERFORMANCE, true, "Runs the headset's processors faster while the game plays."),
        choice("sharpen", "Sharpening", PERFORMANCE, new String[] {"Off", "Normal", "Strong"}, 0,
            "The headset's sharpening of the picture. Off: the picture as drawn, no shimmer on edges."),
        // 0.1.4: back, the motion drawn (sh3vr.cpp motion_frame; 0.1.3 left it out: the owner's 0.1.2 log said "smoother
        // frames on 0% of them"); 0.1.5: off to start (the owner's 0.1.4 report)
        sw("space_warp", "Smoother frames (AppSW)", PERFORMANCE, SPACE_WARP_ON,
            "The headset makes every other frame itself from the game's last picture and how the world moved, so the "
            + "game draws half of them. It needs the game to keep up with half the refresh rate (45 frames a second at "
            + "90 Hz), or the picture stutters. Off by default. Takes effect the next time the game starts."),
        sw("show_performance", "Show performance", PERFORMANCE, false, "Frame times in a corner of the screen."),
        // View
        sw("first_person", "First person", VIEW, true,
            "Off plays in third person with the game's own camera. Hold the left menu button in the game to switch."),
        // 0.1.3 (owner 87): the row says "Dynamic" (0.1.2 had its own switch); "At the camera" is last so 0-2 keep
        // their meaning in sh3vr.ini (Config.foldDynamic carries 0.1.2's switch over once)
        choice("cutscenes", "Cutscenes", VIEW,
            new String[] {"Dynamic (her eyes or the camera)", "First person", "Cinema screen", "At the camera"}, 0,
            "Dynamic: you see through Heather's eyes while the scene shows her, and stand where the game's camera is "
            + "when it shows something else or her face up close. First person: always her eyes. Cinema screen: the "
            + "scene on a big screen before you. At the camera: always where the game's camera is, the scene around you."),
        sw("auto_third_person", "Third person where the game needs it", VIEW, true,
            "Places where the game's own camera shows what matters switch to third person and back."),
        sw("height_auto", "Measure your height", VIEW, true,
            "Your height is taken from the headset as you stand (and at each recentre), so Heather's eyes are yours."),
        number("player_height_cm", "Your height", VIEW, 120f, 220f, 1f, "cm", 165f,
            "Used when Measure your height is off: scales the view so Heather's height feels like yours."),
        sw("seated", "Seated", VIEW, false, "Raises the view and the holsters for playing seated."),
        sw("room_walk", "Walking in the room moves Heather", VIEW, true, "She follows when you step in the room."),
        choice("body", "Heather's body in first person", VIEW, new String[] {"Shown (head hidden)", "Hidden (hands only)"}, 0,
            "Shown: look down and see her."),
        sw("arms_follow", "Heather's arms follow your hands", VIEW, true,
            "Her own arms reach to where your hands are, in first person."),
        sw("body_fade", "Fade body near your eyes", VIEW, true,
            "Only what of her comes right up to your eyes fades; looking down shows her whole body."),
        sw("hide_arms_near", "Hide arms near your eyes", VIEW, false,
            "Her arms fade sooner when your hands come close to your face."),
        sw("spatial_sound", "Spatial sound", VIEW, true,
            "Sounds come from where they are around your head: turn your head and they stay put; behind you they are softer."),
        sw("film_grain", "Film grain", VIEW, false,
            "The game's own grainy noise over the picture. Off: a clean picture."),
        sw("drifting_fog", "Drifting fog", VIEW, false,
            "The game's drifting fog, in the world around you (it stays put when you turn your head). Off: no drifting fog."),
        number("brightness", "Brightness", VIEW, 50f, 300f, 10f, "%", 100f,
            "How bright the picture is. 100%: the game's own; higher shows more in the dark."),
        number("contrast", "Contrast", VIEW, 50f, 150f, 5f, "%", 100f, "100%: the game's own."),
        number("saturation", "Color saturation", VIEW, 0f, 200f, 10f, "%", 100f,
            "How strong the colors are. 100%: the game's own; 0%: black and white."),
        number("dark_edges", "Darker edges", VIEW, 0f, 100f, 10f, "%", 0f,
            "Darkens the picture toward the edges of your view. 0%: off."),
        sw("smooth_edges", "Smooth edges", VIEW, false,
            "Smooths the jagged edges of the world (anti-aliasing)."),
        sw("her_shadow", "Heather's shadow", VIEW, false,
            "Her body, her hands and what she holds cast shadows in the flashlight's beam, in the game's own shadows. "
            + "Off by default."),
        number("panel_distance", "Screen distance", VIEW, 0.8f, 3.0f, 0.1f, "m", 1.4f,
            "How far the menus, map and memos are."),
        number("panel_width", "Screen size", VIEW, 0.6f, 2.5f, 0.1f, "m", 1.2f, "How wide that screen is."),
        sw("panel_follow", "Screen follows the view", VIEW, true, "The screen comes round when you turn far from it."),
        sw("menus_in_view", "Menus open in front of you", VIEW, true,
            "Menus, the inventory, the map and memos come up where you look, fading in, and stay put while you point at them."),
        sw("messages_at_things", "Messages next to things", VIEW, true,
            "What Heather says about a thing shows right by it, not on a screen; subtitles stay low in your view."),
        // Comfort
        choice("turn", "Turning", COMFORT, new String[] {"Snap", "Smooth"}, 1, "The right stick turns."),
        number("snap_degrees", "Snap turn angle", COMFORT, 15f, 90f, 15f, "deg", 45f, ""),
        number("smooth_degrees", "Smooth turn speed", COMFORT, 30f, 360f, 15f, "deg/s", 120f, ""),
        choice("move_by", "Walk towards", COMFORT, new String[] {"Where the head looks", "Where the left hand points"}, 0,
            "The left stick moves Heather this way."),
        sw("vignette", "Comfort vignette", COMFORT, false, "The edges of the view darken while moving or turning."),
        number("vignette_strength", "Vignette strength", COMFORT, 10f, 100f, 10f, "%", 50f, ""),
        sw("head_fade", "Head in walls darkens the view", COMFORT, true, ""),
        // Pointer
        sw("pointer", "Pointer", POINTER, true,
            "A beam from the controller for every screen the game shows: menus, the map, memos, puzzles."),
        choice("pointer_hand", "Pointer hand", POINTER, new String[] {"Either (last trigger)", "Left", "Right"}, 0, ""),
        choice("beam_look", "Beam look", POINTER, new String[] {"Silent Hill (rust)", "White"}, 0, ""),
        // Hands and weapons
        sw("left_handed", "Left-handed", HANDS, false, "Weapons in the left hand; the holsters mirrored."),
        sw("aim_by_hand", "Aim with the controller", HANDS, true, "Shots go where the gun points."),
        sw("quick_raise", "Quick gun raise", HANDS, true, "A pull of the trigger raises the gun and fires at once: her raise and the start of the shot play six times as fast, and a pull while the last shot recovers is kept until it fires. The game's own rate of fire stays."),
        sw("laser", "Guide dot", HANDS, true, "A small red dot where the gun points."),
        sw("auto_aim_first_person", "Game's auto-aim in first person", HANDS, false,
            "Off: you aim. On: the game's own lock-on, as in third person."),
        sw("two_hand", "Two-hand grips", HANDS, true,
            "The other hand on a long weapon's foregrip steadies and aims it."),
        sw("physical_melee", "Physical melee", HANDS, true, "Swing the weapon to hit, where it really goes."),
        sw("punches", "Punches", HANDS, true, "Empty hands punch at half the weakest weapon's damage."),
        sw("gun_club", "Guns as clubs", HANDS, true, "A gun swung hard hits like a pipe."),
        sw("holsters", "Holsters", HANDS, true,
            "Your gun on your right shoulder, your melee weapon on your right hip: reach and grip to draw the one you "
            + "last put there, let go there to put it away."),
        sw("holster_pouch", "Belt pouch (supplies)", HANDS, true, "Grip at the belt for the health drinks."),
        choice("flashlight", "Chest flashlight aims", HANDS, new String[] {"Where you look", "Where Heather faces (the game's)"}, 0, "On her chest the beam goes where you look; take it in a hand to point it yourself."),
        sw("chest_light_touch", "Take the flashlight from your chest", HANDS, true, "Grip at the chest pocket to hold the flashlight; let go and it goes back on your chest. The trigger of the hand holding it switches it."),
        sw("finger_bending", "Fingers stop on surfaces", HANDS, true, "Fingers bend against walls and objects."),
        sw("ghost_hands", "Ghost hands in walls", HANDS, false,
            "A hand pushed into a wall always stays at the wall; on, a faint one also shows where yours is."),
        sw("grabbing", "Grab with the hand", HANDS, true, "Reach and grip to pick things up and open doors."),
        sw("take_by_hand", "Take items with your hand", HANDS, true,
            "Close your hand on an item or a door to take or use it; the action button still works."),
        sw("manual_reload", "Manual reload", HANDS, true,
            "Reload with your hands: push the right stick down to drop the magazine, take a new one from the front of "
            + "your belt and push it in; pull the slide back after an empty gun. Shotgun shells one at a time, then the "
            + "pump."),
        sw("reload_hints", "Reload hints", HANDS, true,
            "The next reload step on the screen, for your first three reloads each time you play."),
        sw("hand_sounds", "Hand sounds", HANDS, true,
            "A sound for everything your hands do, heard from where they are: the slide, the pump, a magazine dropped "
            + "or pushed in, holsters, the pouch, the flashlight's pocket, a hand on a wall, a gun knocked on one."),
        sw("wrist_health", "Health bar on your wrist", HANDS, true,
            "In first person, turn your left wrist to look at her watch: her health and stamina show there (her health "
            + "pulses when it runs low), in place of the game's HUD. Third person shows the game's normal HUD, its Life "
            + "Gauge and Life Display when turned on in Extra options; back in first person, the watch again."),
        sw("blocking", "Block with your weapon or arms", HANDS, true,
            "Hold your weapon, or your forearm, up between you and a monster: its hit does a quarter of the damage behind "
            + "a melee weapon, half behind a gun, a bit over half on a bare arm."),
        sw("shake_free", "Shake to break free", HANDS, true,
            "When a monster knocks you down or holds you, shake your controllers to get free sooner, as pressing the "
            + "buttons quickly does in the game (the buttons still work too)."),
        // Vibration (the keys stay haptics* so earlier settings carry over)
        sw("haptics", "Vibration", VIBRATION, true, "Shots, hits, holsters, touching the world."),
        number("haptics_strength", "Vibration strength", VIBRATION, 10f, 100f, 10f, "%", 100f, ""),
        sw("haptic_surfaces", "Vibration on walls and floors", VIBRATION, true,
            "A hand on a wall, a door or the floor buzzes, harder the harder you press."),
        choice("haptic_extra", "Extra vibration", VIBRATION, new String[] {"Off", "Pickups", "The whole world"}, 2,
            "Pickups: taking items and opening doors. The whole world: also footsteps, landings, falls and the radio."),
        sw("haptic_weapon_change", "Buzz on weapon change", VIBRATION, true, ""),
        sw("heartbeat", "Heartbeat at low health", VIBRATION, true, ""),
        sw("damage_pulse", "Damage pulse", VIBRATION, true, ""),
    };

    /** sh3.ini: the game's threads (rt_core.c read_ini). Three cores is the game as it runs best. */
    static final Option GAME_THREADS = new Option(GAME_INI, "cores", "Game threads", PERFORMANCE, Kind.CHOICE,
        new String[] {"Three cores", "One core"}, new String[] {"3", "1"}, 0, 0, 0, "", "3", true,
        "One core is slower; only for telling a timing problem apart.");
}
