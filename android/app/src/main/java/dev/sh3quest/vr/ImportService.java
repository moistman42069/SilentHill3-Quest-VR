// SPDX-License-Identifier: GPL-3.0-or-later
package dev.sh3quest.vr;

import android.app.Notification;
import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.app.Service;
import android.content.ContentResolver;
import android.content.Context;
import android.content.Intent;
import android.content.pm.ServiceInfo;
import android.database.Cursor;
import android.net.Uri;
import android.os.Build;
import android.os.IBinder;
import android.provider.DocumentsContract;
import android.util.Log;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.util.zip.ZipEntry;
import java.util.zip.ZipInputStream;

/**
 * Copies the PC game in (LIST 4, 36): a folder chosen with the system's picker (the "SILENT HILL 3" folder copied
 * onto the headset), or a .zip of it, into GameFiles.folder; or a folder of PC saves into its savedata. A foreground
 * service: the copy (about 5 GB) goes on while the launcher's panel is closed. Nothing leaves the headset.
 */
public final class ImportService extends Service {
    private static final String TAG = "SH3QuestVR";
    private static final String CHANNEL = "import";
    static final String EXTRA_URI = "uri", EXTRA_MODE = "mode";
    static final String MODE_FOLDER = "folder", MODE_ZIP = "zip", MODE_SAVES = "saves";

    /** Where it is; written here, read by the launcher's page. */
    static final class State {
        volatile boolean running, starting;
        volatile int percent;
        volatile String step = "", detail = "";
        volatile String failure, done;
    }

    static final State STATE = new State();

    static void start(Context context, Uri uri, String mode) {
        STATE.starting = true;
        STATE.failure = null;
        STATE.done = null;
        STATE.percent = 0;
        STATE.step = "Starting";
        STATE.detail = "";
        Intent intent = new Intent(context, ImportService.class).putExtra(EXTRA_URI, uri.toString()).putExtra(EXTRA_MODE, mode);
        context.startForegroundService(intent);
    }

    @Override
    public IBinder onBind(Intent intent) {
        return null;
    }

    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {
        NotificationManager manager = getSystemService(NotificationManager.class);
        if (manager != null) {
            manager.createNotificationChannel(new NotificationChannel(CHANNEL, "Copying the game", NotificationManager.IMPORTANCE_LOW));
        }
        Notification note = new Notification.Builder(this, CHANNEL)
            .setContentTitle("Silent Hill 3 Quest VR")
            .setContentText("Copying the game files")
            .setSmallIcon(android.R.drawable.stat_sys_download)
            .setOngoing(true)
            .build();
        if (Build.VERSION.SDK_INT >= 29) {
            startForeground(1, note, ServiceInfo.FOREGROUND_SERVICE_TYPE_DATA_SYNC);
        } else {
            startForeground(1, note);
        }
        if (intent == null || STATE.running) {
            return START_NOT_STICKY;
        }
        final Uri uri = Uri.parse(intent.getStringExtra(EXTRA_URI));
        final String mode = intent.getStringExtra(EXTRA_MODE);
        STATE.running = true;
        STATE.starting = false;
        new Thread(() -> {
            try {
                run(uri, mode);
            } catch (Exception e) {
                Log.e(TAG, "import failed", e);
                STATE.failure = "The copy stopped: " + e.getMessage();
            } finally {
                STATE.running = false;
                stopForeground(STOP_FOREGROUND_REMOVE);
                stopSelf();
            }
        }, "import").start();
        return START_NOT_STICKY;
    }

    private long total, copied;
    private int files;

    private void run(Uri uri, String mode) throws IOException {
        total = 0;
        copied = 0;
        files = 0;
        File target = MODE_SAVES.equals(mode) ? GameFiles.saves(this) : GameFiles.folder(this);
        if (!target.isDirectory() && !target.mkdirs()) {
            throw new IOException("cannot make " + target);
        }
        if (MODE_ZIP.equals(mode)) {
            STATE.step = "Copying from the .zip";
            unzip(uri, target);
        } else {
            ContentResolver resolver = getContentResolver();
            String root = DocumentsContract.getTreeDocumentId(uri);
            STATE.step = "Counting the files";
            total = size(resolver, uri, root);
            STATE.step = MODE_SAVES.equals(mode) ? "Copying the saves" : "Copying the game";
            copyTree(resolver, uri, root, target);
        }
        STATE.percent = 100;
        if (MODE_SAVES.equals(mode)) {
            STATE.done = files + " save files copied into the game's savedata folder.";
            RunLog.note(this, "import: " + STATE.done);
            return;
        }
        STATE.step = "Checking the files";
        GameFiles.Check check = GameFiles.check(this);
        STATE.done = files + " files copied. " + check.summary();
        RunLog.note(this, "import: " + STATE.done);
    }

    private static final String[] COLUMNS = {DocumentsContract.Document.COLUMN_DOCUMENT_ID,
        DocumentsContract.Document.COLUMN_DISPLAY_NAME, DocumentsContract.Document.COLUMN_MIME_TYPE,
        DocumentsContract.Document.COLUMN_SIZE};

    private long size(ContentResolver resolver, Uri tree, String id) {
        long sum = 0;
        Uri children = DocumentsContract.buildChildDocumentsUriUsingTree(tree, id);
        try (Cursor c = resolver.query(children, COLUMNS, null, null, null)) {
            while (c != null && c.moveToNext()) {
                if (DocumentsContract.Document.MIME_TYPE_DIR.equals(c.getString(2))) {
                    sum += size(resolver, tree, c.getString(0));
                } else {
                    sum += Math.max(0, c.getLong(3));
                }
            }
        }
        return sum;
    }

    private void copyTree(ContentResolver resolver, Uri tree, String id, File into) throws IOException {
        Uri children = DocumentsContract.buildChildDocumentsUriUsingTree(tree, id);
        try (Cursor c = resolver.query(children, COLUMNS, null, null, null)) {
            while (c != null && c.moveToNext()) {
                String name = safeName(c.getString(1));
                if (name == null) {
                    continue;
                }
                File out = new File(into, name);
                if (DocumentsContract.Document.MIME_TYPE_DIR.equals(c.getString(2))) {
                    if (!out.isDirectory() && !out.mkdirs()) {
                        throw new IOException("cannot make " + out);
                    }
                    copyTree(resolver, tree, c.getString(0), out);
                } else {
                    long size = Math.max(0, c.getLong(3));
                    if (out.isFile() && out.length() == size && size > 0) {
                        copied += size;  // already there (a copy run again after it stopped)
                        files++;
                        continue;
                    }
                    try (InputStream in = resolver.openInputStream(DocumentsContract.buildDocumentUriUsingTree(tree, c.getString(0)))) {
                        if (in == null) {
                            throw new IOException("cannot read " + name);
                        }
                        copy(in, out, name);
                    }
                }
            }
        }
    }

    /** A name from the picker or the zip, used as one path part: nothing that leaves the folder. */
    private static String safeName(String name) {
        if (name == null || name.isEmpty() || name.equals(".") || name.equals("..") || name.contains("/") || name.contains("\\")) {
            return null;
        }
        return name;
    }

    private void unzip(Uri uri, File into) throws IOException {
        String canonical = into.getCanonicalPath() + File.separator;
        try (InputStream raw = getContentResolver().openInputStream(uri)) {
            if (raw == null) {
                throw new IOException("cannot read the .zip");
            }
            total = -1;
            try (ZipInputStream zip = new ZipInputStream(raw)) {
                String strip = null;
                for (ZipEntry entry = zip.getNextEntry(); entry != null; entry = zip.getNextEntry()) {
                    String name = entry.getName().replace('\\', '/');
                    // A zip of the "SILENT HILL 3" folder itself: its top folder left out.
                    if (strip == null) {
                        int slash = name.indexOf('/');
                        String first = slash > 0 ? name.substring(0, slash) : "";
                        boolean gameRoot = first.equalsIgnoreCase("data") || first.equalsIgnoreCase("savedata")
                            || first.isEmpty();
                        strip = gameRoot ? "" : first + "/";
                    }
                    if (!strip.isEmpty() && name.startsWith(strip)) {
                        name = name.substring(strip.length());
                    }
                    if (name.isEmpty()) {
                        continue;
                    }
                    File out = new File(into, name);
                    if (!out.getCanonicalPath().startsWith(canonical)) {
                        throw new IOException("the .zip names a file outside the game folder: " + entry.getName());
                    }
                    if (entry.isDirectory()) {
                        out.mkdirs();
                        continue;
                    }
                    File parent = out.getParentFile();
                    if (parent != null && !parent.isDirectory() && !parent.mkdirs()) {
                        throw new IOException("cannot make " + parent);
                    }
                    copy(zip, out, name);
                }
            }
        }
    }

    private void copy(InputStream in, File out, String name) throws IOException {
        File part = new File(out.getPath() + ".part");
        byte[] buffer = new byte[1 << 20];
        try (OutputStream to = new FileOutputStream(part)) {
            for (int n = in.read(buffer); n > 0; n = in.read(buffer)) {
                to.write(buffer, 0, n);
                copied += n;
                STATE.detail = name;
                STATE.percent = total > 0 ? (int) Math.min(99, copied * 100 / total) : 0;
                if (total <= 0) {
                    STATE.detail = name + " (" + (copied >> 20) + " MB so far)";
                }
            }
        }
        if (out.exists() && !out.delete()) {
            throw new IOException("cannot replace " + out);
        }
        if (!part.renameTo(out)) {
            throw new IOException("cannot finish " + out);
        }
        files++;
    }
}
