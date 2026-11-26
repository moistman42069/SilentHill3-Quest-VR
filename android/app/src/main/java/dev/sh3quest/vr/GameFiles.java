// SPDX-License-Identifier: GPL-3.0-or-later
package dev.sh3quest.vr;

import android.content.Context;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileInputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.nio.charset.StandardCharsets;
import java.security.MessageDigest;
import java.util.ArrayList;
import java.util.List;

/**
 * The PC game's folder on the headset (LIST 4): Android/data/dev.sh3quest.vr/files/game, where the player's copy of
 * sh3.exe and its data go (imported by ImportService, or copied there over USB). Checked against game_files.txt
 * (tools/game_manifest.py): the exe's SHA-1 (the code was recompiled from that exe) and every data file's size.
 * The folder stays through updates; the game's saves are in its savedata folder, as on the PC.
 */
final class GameFiles {
    private GameFiles() {}

    static File folder(Context context) {
        File root = context.getExternalFilesDir(null);
        if (root == null) {
            root = new File("/sdcard/Android/data/" + context.getPackageName() + "/files");
        }
        return new File(root, "game");
    }

    static File saves(Context context) {
        return new File(folder(context), "savedata");
    }

    /** What the check found. */
    static final class Check {
        boolean exeFound, exeAccepted;
        String exeName = "";       // the accepted release it matched
        String exeSha1 = "";
        int files, present;
        final List<String> missing = new ArrayList<>(), wrongSize = new ArrayList<>();

        boolean ready() {
            return exeAccepted && missing.isEmpty() && wrongSize.isEmpty();
        }

        String summary() {
            if (ready()) {
                return "All " + files + " files are there and sh3.exe is " + exeName + ".";
            }
            StringBuilder out = new StringBuilder();
            if (!exeFound) {
                out.append("sh3.exe is not there.\n");
            } else if (!exeAccepted) {
                out.append("sh3.exe is not the release this port was made from (its SHA-1 is ").append(exeSha1)
                    .append("). The port needs the 2003 PC release's sh3.exe.\n");
            }
            if (!missing.isEmpty()) {
                out.append(missing.size()).append(" of ").append(files).append(" files missing: ")
                    .append(list(missing)).append('\n');
            }
            if (!wrongSize.isEmpty()) {
                out.append(wrongSize.size()).append(" files not the right size (a copy cut short?): ")
                    .append(list(wrongSize)).append('\n');
            }
            return out.toString().trim();
        }

        private static String list(List<String> names) {
            StringBuilder out = new StringBuilder();
            for (int i = 0; i < names.size() && i < 8; i++) {
                out.append(i > 0 ? ", " : "").append(names.get(i));
            }
            if (names.size() > 8) {
                out.append(" and ").append(names.size() - 8).append(" more");
            }
            return out.toString();
        }
    }

    /** The folder against game_files.txt. File names are matched without regard to case (the PC's way). */
    static Check check(Context context) throws IOException {
        File game = folder(context);
        Check result = new Check();
        List<String[]> exes = new ArrayList<>();
        try (InputStream in = context.getAssets().open("game_files.txt");
                BufferedReader reader = new BufferedReader(new InputStreamReader(in, StandardCharsets.UTF_8))) {
            for (String line = reader.readLine(); line != null; line = reader.readLine()) {
                String[] parts = line.trim().split("\\s+", 3);
                if (parts.length < 3 || parts[0].startsWith("#")) {
                    continue;
                }
                if (parts[0].equals("exe")) {
                    exes.add(new String[] {parts[1], parts[2]});
                } else if (parts[0].equals("file")) {
                    result.files++;
                    File file = find(game, parts[1]);
                    long size = Long.parseLong(parts[2].trim());
                    if (file == null) {
                        result.missing.add(parts[1]);
                    } else if (file.length() != size) {
                        result.wrongSize.add(parts[1]);
                    } else {
                        result.present++;
                    }
                }
            }
        }
        File exe = find(game, "sh3.exe");
        result.exeFound = exe != null;
        if (exe != null) {
            result.exeSha1 = sha1(exe);
            for (String[] accepted : exes) {
                if (accepted[0].equalsIgnoreCase(result.exeSha1)) {
                    result.exeAccepted = true;
                    result.exeName = accepted[1];
                }
            }
        }
        return result;
    }

    /** `path` (a/b/c) under `root`, each part matched without regard to case; null when it is not there. */
    static File find(File root, String path) {
        File at = root;
        for (String part : path.split("/")) {
            File exact = new File(at, part);
            if (exact.exists()) {
                at = exact;
                continue;
            }
            File[] children = at.listFiles();
            File match = null;
            if (children != null) {
                for (File child : children) {
                    if (child.getName().equalsIgnoreCase(part)) {
                        match = child;
                    }
                }
            }
            if (match == null) {
                return null;
            }
            at = match;
        }
        return at;
    }

    static String sha1(File file) throws IOException {
        try (InputStream in = new FileInputStream(file)) {
            MessageDigest digest = MessageDigest.getInstance("SHA-1");
            byte[] buffer = new byte[1 << 16];
            for (int n = in.read(buffer); n > 0; n = in.read(buffer)) {
                digest.update(buffer, 0, n);
            }
            StringBuilder out = new StringBuilder();
            for (byte b : digest.digest()) {
                out.append(String.format("%02x", b));
            }
            return out.toString();
        } catch (java.security.NoSuchAlgorithmException e) {
            throw new IOException(e);
        }
    }
}
