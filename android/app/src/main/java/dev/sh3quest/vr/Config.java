// SPDX-License-Identifier: GPL-3.0-or-later
package dev.sh3quest.vr;

import android.content.Context;

import java.io.File;
import java.io.IOException;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.StandardCopyOption;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;

/**
 * The player's choices, kept where the game reads them: sh3vr.ini and sh3.ini in the game folder. The in-game VR menu
 * writes sh3vr.ini too, so the launcher reads it again each time it shows the options: one place, never two that
 * disagree. Values no longer offered (an older version's) read as the default.
 */
final class Config extends Options {
    private final File folder;
    private final Map<String, String> vr = new LinkedHashMap<>();
    private final List<String> gameLines = new ArrayList<>();

    Config(Context context) {
        folder = GameFiles.folder(context);
        reload();
    }

    /** sh3vr.ini and sh3.ini read again (the VR menu may have changed them). */
    void reload() {
        vr.clear();
        for (String line : lines(new File(folder, VR_INI))) {
            String[] kv = pair(line);
            if (kv != null) {
                vr.put(kv[0], kv[1]);
            }
        }
        gameLines.clear();
        gameLines.addAll(lines(new File(folder, GAME_INI)));
        foldDynamic();
        newDefaults();
    }

    /**
     * 0.1.3 (owner 87): 0.1.2's own switch "Dynamic cutscenes" is the Cutscenes row's first choice now. Off with "In
     * the scene at the camera" (0) keeps that look as "At the camera" (3); the key is not written again (sh3vr.cpp
     * load_options does the same).
     */
    private void foldDynamic() {
        String dynamic = vr.remove("dynamic_cutscenes");
        if (dynamic == null) {
            return;
        }
        boolean off = "0".equals(dynamic) || "false".equalsIgnoreCase(dynamic) || "off".equalsIgnoreCase(dynamic);
        String scenes = vr.get("cutscenes");
        if (off && (scenes == null || same(scenes, "0"))) {
            vr.put("cutscenes", "3");
        }
        try {
            writeVr();
        } catch (IOException e) {
            // the game carries it over itself when it starts
        }
    }

    /** 0.1.3 (Options.DEFAULTS_0_1_3): a sh3vr.ini from before 0.1.3 gets 0.1.3's changed defaults once, then says so.
     *  0.1.4: one from before 0.1.4 gets Smoother frames on once (0.1.2 saved it when it did nothing; now it works). */
    private void newDefaults() {
        if (vr.isEmpty()) {
            return;  // no file yet: it is written with the defaults and the version
        }
        int version = 0;
        try {
            String seen = vr.get(DEFAULTS_KEY);
            version = seen == null ? 0 : Integer.parseInt(seen.trim());
        } catch (NumberFormatException e) {
            version = 0;
        }
        if (version >= DEFAULTS_VERSION) {
            return;
        }
        List<String> keys = new ArrayList<>();
        if (version < 3) {
            keys.addAll(Arrays.asList(DEFAULTS_0_1_3));
        }
        keys.add("space_warp");  // version 3 or older: 0.1.4's Smoother frames
        for (String key : keys) {
            for (Option option : ALL) {
                if (option.key.equals(key)) {
                    vr.put(key, option.fallback);
                }
            }
        }
        try {
            writeVr();
        } catch (IOException e) {
            // the game puts them in itself when it starts (sh3vr.cpp load_options)
        }
    }

    private static List<String> lines(File file) {
        try {
            return file.isFile() ? Files.readAllLines(file.toPath(), StandardCharsets.UTF_8) : new ArrayList<>();
        } catch (IOException e) {
            return new ArrayList<>();
        }
    }

    /** "key = value" as the game reads it (sscanf " key = value"); null for comments and anything else. */
    private static String[] pair(String line) {
        String trimmed = line.trim();
        if (trimmed.isEmpty() || trimmed.startsWith("#") || trimmed.startsWith(";")) {
            return null;
        }
        int eq = trimmed.indexOf('=');
        if (eq <= 0) {
            return null;
        }
        String value = trimmed.substring(eq + 1).trim();
        int cut = value.indexOf(';');
        if (cut >= 0) {
            value = value.substring(0, cut).trim();
        }
        return new String[] {trimmed.substring(0, eq).trim(), value};
    }

    String get(Option option) {
        String value;
        if (GAME_INI.equals(option.file)) {
            value = null;
            for (String line : gameLines) {
                String[] kv = pair(line);
                if (kv != null && kv[0].equals(option.key)) {
                    value = kv[1];  // the last line that sets it is the one the game ends up with
                }
            }
        } else {
            value = vr.get(option.key);
        }
        if (value == null) {
            return option.fallback;
        }
        switch (option.kind) {
            case SWITCH:
                return "0".equals(value) || "false".equalsIgnoreCase(value) || "off".equalsIgnoreCase(value) ? "0" : "1";
            case NUMBER:
                try {
                    float v = Float.parseFloat(value);
                    return v >= option.low && v <= option.high ? format(v) : option.fallback;
                } catch (NumberFormatException e) {
                    return option.fallback;
                }
            default:
                // A float choice (the refresh rate) is matched by value: "72" and "72.0" are the same.
                for (String offered : option.values) {
                    if (offered.equals(value) || same(offered, value)) {
                        return offered;
                    }
                }
                return option.fallback;
        }
    }

    private static boolean same(String a, String b) {
        try {
            return Float.parseFloat(a) == Float.parseFloat(b);
        } catch (NumberFormatException e) {
            return false;
        }
    }

    void set(Option option, String value) throws IOException {
        if (GAME_INI.equals(option.file)) {
            int last = -1;
            for (int i = 0; i < gameLines.size(); i++) {
                String[] kv = pair(gameLines.get(i));
                if (kv != null && kv[0].equals(option.key)) {
                    last = i;
                }
            }
            String line = option.key + "=" + value;
            if (last >= 0) {
                gameLines.set(last, line);
            } else {
                gameLines.add(line);
            }
            write(GAME_INI, String.join("\n", gameLines) + "\n");
        } else {
            vr.put(option.key, value);
            writeVr();
        }
    }

    /** A number stepped by `dir` steps, kept in its range. */
    void step(Option option, int dir) throws IOException {
        float v = Float.parseFloat(get(option)) + dir * option.step;
        v = Math.max(option.low, Math.min(option.high, v));
        v = Math.round(v / option.step) * option.step;
        set(option, format(v));
    }

    /** Every option back to its recommended value (the defaults). */
    void recommended(String page) throws IOException {
        for (Option option : ALL) {
            if (page == null || page.equals(option.page)) {
                vr.put(option.key, option.fallback);
            }
        }
        writeVr();
    }

    /** sh3vr.ini whole, as the game writes it (sh3vr.cpp save_options): every key, its value or the default. */
    private void writeVr() throws IOException {
        StringBuilder out = new StringBuilder("# SH3 Quest VR options (the launcher's Options and the in-game VR menu write this file)\n");
        out.append("vr = ").append(get(VR)).append('\n');
        out.append(DEFAULTS_KEY).append(" = ").append(DEFAULTS_VERSION).append('\n');  // 0.1.3: the defaults' version
        vr.put(DEFAULTS_KEY, Integer.toString(DEFAULTS_VERSION));
        List<String> known = new ArrayList<>(Arrays.asList("vr", DEFAULTS_KEY));
        for (Option option : ALL) {
            out.append(option.key).append(" = ").append(get(option)).append('\n');
            known.add(option.key);
        }
        for (Map.Entry<String, String> other : vr.entrySet()) {
            if (!known.contains(other.getKey())) {
                out.append(other.getKey()).append(" = ").append(other.getValue()).append('\n');
            }
        }
        write(VR_INI, out.toString());
    }

    /** Both files as the game will read them, before it starts (VR on or off for this run). */
    void writeForGame(boolean vrOn) throws IOException {
        folder.mkdirs();
        vr.put("vr", vrOn ? "1" : "0");
        writeVr();
        if (get(GAME_THREADS).equals(GAME_THREADS.fallback)) {
            set(GAME_THREADS, GAME_THREADS.fallback);  // written out, so the file says what the game uses
        }
    }

    private void write(String name, String text) throws IOException {
        folder.mkdirs();
        File target = new File(folder, name), next = new File(folder, name + ".new");
        Files.write(next.toPath(), text.getBytes(StandardCharsets.UTF_8));
        Files.move(next.toPath(), target.toPath(), StandardCopyOption.REPLACE_EXISTING);
    }
}
