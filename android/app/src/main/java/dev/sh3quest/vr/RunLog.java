// SPDX-License-Identifier: GPL-3.0-or-later
package dev.sh3quest.vr;

import android.content.ContentResolver;
import android.content.ContentValues;
import android.content.Context;
import android.content.SharedPreferences;
import android.net.Uri;
import android.os.Build;
import android.os.Environment;
import android.provider.MediaStore;
import android.util.Log;

import java.io.BufferedReader;
import java.io.File;
import java.io.IOException;
import java.io.InputStreamReader;
import java.io.OutputStream;
import java.io.RandomAccessFile;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.text.SimpleDateFormat;
import java.util.Date;
import java.util.Locale;

/**
 * The runs' records (LIST 37), readable with no tools: the game's own sh3.log and sh3-crash.log (guest and host stacks,
 * what the game and VR were doing: src/runtime/rt_core.c, src/game/sh3.c game_crash_info, sh3vr_crash_info), the run
 * before's, this app's logcat. Shown on the Logs tab, put in Download/SH3QuestVR after each run (a PC sees it over
 * USB), and shared as one report (ReportProvider) with whatever app the player picks. Nothing is sent by itself.
 */
final class RunLog {
    private static final String TAG = "SH3QuestVR";
    static final String FOLDER = Environment.DIRECTORY_DOWNLOADS + "/SH3QuestVR";
    private static final String PREFS = "runlog", KEY_SAVED = "saved_for";

    private RunLog() {}

    private static String stamp() {
        return new SimpleDateFormat("yyyy-MM-dd_HH-mm-ss", Locale.US).format(new Date());
    }

    /** A line in the launcher's own log (logcat, so the report has it). */
    static void note(Context context, String line) {
        Log.i(TAG, line);
    }

    static void write(Context context, String name, String text) {
        ContentResolver resolver = context.getContentResolver();
        ContentValues values = new ContentValues();
        values.put(MediaStore.MediaColumns.DISPLAY_NAME, name);
        values.put(MediaStore.MediaColumns.MIME_TYPE, "text/plain");
        values.put(MediaStore.MediaColumns.RELATIVE_PATH, FOLDER);
        Uri uri = resolver.insert(MediaStore.Downloads.EXTERNAL_CONTENT_URI, values);
        if (uri == null) {
            Log.w(TAG, "the Download folder refused " + name);
            return;
        }
        try (OutputStream out = resolver.openOutputStream(uri)) {
            if (out != null) {
                out.write(text.getBytes(StandardCharsets.UTF_8));
            }
        } catch (IOException e) {
            Log.w(TAG, "cannot write " + name + ": " + e);
        }
    }

    private static void section(StringBuilder out, String title, String body) {
        out.append("\n======== ").append(title).append(" ========\n").append(body).append('\n');
    }

    /** The end of a file, at most `most` bytes of it, from a line's start. */
    static String tail(File file, long most) {
        if (!file.isFile()) {
            return "(none)";
        }
        try (RandomAccessFile in = new RandomAccessFile(file, "r")) {
            long length = in.length();
            long from = Math.max(0, length - most);
            byte[] bytes = new byte[(int) (length - from)];
            in.seek(from);
            in.readFully(bytes);
            int start = 0;
            if (from > 0) {
                while (start < bytes.length && bytes[start] != '\n') {
                    start++;
                }
                start = Math.min(start + 1, bytes.length);
            }
            String text = new String(bytes, start, bytes.length - start, StandardCharsets.UTF_8);
            return from > 0 ? "(its first " + (from + start) + " bytes left out)\n" + text : text;
        } catch (IOException e) {
            return "(cannot be read: " + e.getMessage() + ")";
        }
    }

    private static String logcat() {
        StringBuilder out = new StringBuilder();
        try {
            Process process = new ProcessBuilder("logcat", "-d", "-v", "threadtime", "-t", "6000")
                .redirectErrorStream(true).start();
            try (BufferedReader reader = new BufferedReader(
                    new InputStreamReader(process.getInputStream(), StandardCharsets.UTF_8))) {
                for (String line = reader.readLine(); line != null; line = reader.readLine()) {
                    out.append(line).append('\n');
                }
            }
            process.waitFor();
        } catch (IOException | InterruptedException e) {
            out.append("(logcat could not be read: ").append(e.getMessage()).append(")\n");
        }
        return out.toString();
    }

    /** Did the last run end in an error the game wrote down? Its first lines; null when not. */
    static String lastCrash(Context context) {
        File crash = new File(GameFiles.folder(context), "sh3-crash.log");
        if (!crash.isFile() || crash.length() == 0) {
            return null;
        }
        String text = tail(crash, 4096);
        String[] lines = text.split("\n");
        StringBuilder out = new StringBuilder();
        for (int i = 0; i < lines.length && i < 6; i++) {
            out.append(lines[i]).append('\n');
        }
        return out.toString().trim();
    }

    /** Everything about the last run, as one text. */
    static String report(Context context) {
        File game = GameFiles.folder(context);
        StringBuilder out = new StringBuilder();
        out.append("SH3 Quest VR ").append(BuildConfig.VERSION_NAME).append(" report, ").append(stamp()).append('\n');
        out.append("Headset: ").append(Build.MANUFACTURER).append(' ').append(Build.MODEL).append(", Android ")
            .append(Build.VERSION.RELEASE).append(" (").append(Build.DISPLAY).append(")\n");
        try {
            out.append("Game files: ").append(GameFiles.check(context).summary()).append('\n');
        } catch (IOException e) {
            out.append("Game files: not checked (").append(e.getMessage()).append(")\n");
        }
        section(out, "sh3vr.ini", tail(new File(game, "sh3vr.ini"), 16 << 10));
        section(out, "sh3.ini", tail(new File(game, "sh3.ini"), 16 << 10));
        section(out, "sh3-crash.log (the last run)", tail(new File(game, "sh3-crash.log"), 256 << 10));
        section(out, "sh3.log (the last run)", tail(new File(game, "sh3.log"), 2 << 20));
        section(out, "sh3-crash-previous.log (the run before)", tail(new File(game, "sh3-crash-previous.log"), 128 << 10));
        section(out, "sh3-previous.log (the run before)", tail(new File(game, "sh3-previous.log"), 512 << 10));
        section(out, "logcat", logcat());
        return out.toString();
    }

    /** After a run (the launcher opening again): its report in Download/SH3QuestVR, once per run. */
    static void saveLastRun(Context context) {
        File log = new File(GameFiles.folder(context), "sh3.log");
        if (!log.isFile()) {
            return;
        }
        SharedPreferences prefs = context.getSharedPreferences(PREFS, Context.MODE_PRIVATE);
        String run = log.lastModified() + ":" + log.length();
        if (run.equals(prefs.getString(KEY_SAVED, ""))) {
            return;
        }
        write(context, "run_" + stamp() + ".txt", report(context));
        prefs.edit().putString(KEY_SAVED, run).apply();
    }

    /** The report as a file the share sheet can hand on (ReportProvider). */
    static File reportFile(Context context) throws IOException {
        File file = ReportProvider.file(context);
        File dir = file.getParentFile();
        if (dir != null && !dir.isDirectory() && !dir.mkdirs()) {
            throw new IOException("cannot make " + dir);
        }
        Files.write(file.toPath(), report(context).getBytes(StandardCharsets.UTF_8));
        return file;
    }
}
