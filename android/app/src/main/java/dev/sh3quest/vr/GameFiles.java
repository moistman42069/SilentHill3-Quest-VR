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

    /** Which copies of the game work, in the words the launcher and the README use (0.1.4, owner 90). */
    static final String WHICH_GAME = "Silent Hill 3 for the PC, the 2003 release: North American, European or Asian, "
        + "CD or DVD. Its sh3.exe must be the game's 2003-09-03 build (every PC release should have it: the same game in "
        + "every language): as installed from the disc, copy protection and all, or a no-CD/no-DVD sh3.exe, or one "
        + "changed by a fix such as the SH3 PC Fix. The port runs its own copy of the game's code and only checks the "
        + "exe's data.";

    /** What the check found. */
    static final class Check {
        boolean exeFound, exeAccepted;
        String exeName = "";       // the accepted release it matched
        String exeSha1 = "";
        ExeCheck.Result exeData;   // 0.1.4: the exe's data against the build's (when its SHA-1 is not one listed)
        String settled = "";       // 0.1.4: a game found a folder too deep and moved up (settle)
        int files, present;
        final List<String> missing = new ArrayList<>(), wrongSize = new ArrayList<>();

        /**
         * 0.1.4 (owner 91: "try to extend compatibility however you can"): a data file of another size no longer
         * blocks (another edition's languages, a mod); only a missing one does, and an exe that is not the build.
         */
        boolean ready() {
            return exeAccepted && missing.isEmpty();
        }

        String summary() {
            StringBuilder out = new StringBuilder();
            if (!settled.isEmpty()) {
                out.append(settled).append('\n');
            }
            if (ready()) {
                out.append("All ").append(files).append(" files are there and sh3.exe is ").append(exeName).append('.');
                if (exeData != null) {
                    out.append(" Its code differs from the unprotected exe's, which does not matter: the port runs its "
                        + "own copy of the game's code, and the exe's data is the build's");
                    if (exeData.pagesChanged > 0) {
                        out.append(" (").append(exeData.pagesChanged).append(" of ").append(exeData.pagesChecked)
                            .append(" pages changed by a patch: it should still play)");
                    }
                    out.append('.');
                }
                if (!wrongSize.isEmpty()) {
                    out.append("\n").append(wrongSize.size()).append(" data files are not the size of the North American "
                        + "release's (another edition's languages, or a mod): fine to play. If the game stops while "
                        + "loading, copy them again: ").append(list(wrongSize));
                }
                return out.toString();
            }
            if (!exeFound) {
                out.append("sh3.exe is not there.\n");
            } else if (!exeAccepted) {
                out.append("sh3.exe is not the game's 2003-09-03 build, the one this port runs");
                if (exeData != null) {
                    out.append(" (").append(exeData.size).append(" bytes, built ").append(exeData.built())
                        .append("): ").append(exeData.why);
                }
                out.append(". SHA-1 ").append(exeSha1).append(".\nWhat works: ").append(WHICH_GAME).append('\n');
            }
            if (!missing.isEmpty()) {
                out.append(missing.size()).append(" of ").append(files).append(" files missing: ")
                    .append(list(missing)).append('\n');
            }
            if (!wrongSize.isEmpty()) {
                out.append(wrongSize.size()).append(" data files are not the size of the North American release's "
                    + "(another edition's languages, or a mod; they do not stop the game): ").append(list(wrongSize))
                    .append('\n');
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

    /**
     * 0.1.4 (owner 91: "try to extend compatibility however you can"): the game copied a folder or two too deep (the
     * folder above "Silent Hill 3" picked, or that folder itself copied into game over USB) is moved up into the game
     * folder: renames on the headset's storage, nothing copied, nothing replaced. Returns what was done, or "".
     */
    static String settle(File game) {
        if (find(game, "sh3.exe") != null) {
            return "";
        }
        File nested = nestedGame(game, 2);
        if (nested == null) {
            return "";
        }
        int moved = moveInto(nested, game);
        File[] left = nested.listFiles();
        for (File up = nested; left != null && left.length == 0 && !up.equals(game); up = up.getParentFile()) {
            if (!up.delete()) {
                break;  // empty folders left behind are removed, up to the game folder
            }
            left = up.getParentFile().listFiles();
        }
        String from = game.toURI().relativize(nested.toURI()).getPath();
        return moved > 0 ? "The game was in the folder \"" + from + "\" inside the game folder: moved up (" + moved
            + " files and folders)." : "";
    }

    private static File nestedGame(File dir, int depth) {
        File[] children = dir.listFiles();
        if (children == null || depth == 0) {
            return null;
        }
        for (File child : children) {
            if (child.isDirectory() && find(child, "sh3.exe") != null && find(child, "data") != null) {
                return child;
            }
        }
        for (File child : children) {
            String name = child.getName();
            if (child.isDirectory() && !name.equalsIgnoreCase("data") && !name.equalsIgnoreCase("savedata")) {
                File found = nestedGame(child, depth - 1);
                if (found != null) {
                    return found;
                }
            }
        }
        return null;
    }

    /** Moves `from`'s contents into `to`; a folder both have is merged, a file `to` already has is left where it is. */
    private static int moveInto(File from, File to) {
        int moved = 0;
        File[] children = from.listFiles();
        if (children == null) {
            return 0;
        }
        for (File child : children) {
            File target = new File(to, child.getName());
            File existing = find(to, child.getName());
            if (existing == null) {
                if (child.renameTo(target)) {
                    moved++;
                }
            } else if (child.isDirectory() && existing.isDirectory()) {
                moved += moveInto(child, existing);
                String[] rest = child.list();
                if (rest != null && rest.length == 0) {
                    child.delete();
                }
            }
        }
        return moved;
    }

    /** The folder against game_files.txt. File names are matched without regard to case (the PC's way). */
    static Check check(Context context) throws IOException {
        File game = folder(context);
        Check result = new Check();
        result.settled = settle(game);
        List<String[]> exes = new ArrayList<>();
        ExeCheck.Reference build = new ExeCheck.Reference();
        try (InputStream in = context.getAssets().open("game_files.txt");
                BufferedReader reader = new BufferedReader(new InputStreamReader(in, StandardCharsets.UTF_8))) {
            for (String line = reader.readLine(); line != null; line = reader.readLine()) {
                if (build.add(line.trim().split("\\s+"))) {
                    continue;  // the 2003-09-03 build's data (tools/exe_ref.py)
                }
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
            // 0.1.4 (owners 90-91): any other exe of the build (a disc's own, copy protection and all; a no-CD/no-DVD
            // exe; a fix's) by its data; up to 64 MB read (the unprotected exe is 3.3 MB, a protected one a little more)
            if (!result.exeAccepted && build.usable() && exe.length() > 0 && exe.length() <= 64L << 20) {
                byte[] bytes = new byte[(int) exe.length()];
                try (InputStream in = new FileInputStream(exe)) {
                    int at = 0;
                    for (int n; at < bytes.length && (n = in.read(bytes, at, bytes.length - at)) > 0; ) {
                        at += n;
                    }
                }
                result.exeData = ExeCheck.compare(bytes, build);
                if (result.exeData.sameBuild()) {
                    result.exeAccepted = true;
                    result.exeName = "the 2003-09-03 build (its code changed: a disc's copy protection, a no-CD/no-DVD "
                        + "patch or a fix)";
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
