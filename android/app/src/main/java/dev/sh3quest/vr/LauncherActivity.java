// SPDX-License-Identifier: GPL-3.0-or-later
package dev.sh3quest.vr;

import android.app.Activity;
import android.app.Dialog;
import android.content.ActivityNotFoundException;
import android.content.ClipData;
import android.content.Intent;
import android.content.SharedPreferences;
import android.graphics.Typeface;
import android.graphics.drawable.ColorDrawable;
import android.graphics.drawable.GradientDrawable;
import android.net.Uri;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.util.TypedValue;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup;
import android.view.Window;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.ProgressBar;
import android.widget.ScrollView;
import android.widget.TextView;

import java.io.BufferedReader;
import java.io.File;
import java.io.IOException;
import java.io.InputStreamReader;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.List;

/**
 * The launcher (LIST 18, after Ratchet's): a 2D panel on the Quest. Bring the game files in once, change the options
 * (the same ones as the in-game VR menu, recommended values by default), play in VR or flat, read and share the logs.
 */
public final class LauncherActivity extends Activity {
    private static final int REQUEST_FOLDER = 0x5301, REQUEST_ZIP = 0x5302, REQUEST_SAVES = 0x5303;

    // Silent Hill colours: rust on near-black, fog-grey words.
    static final int BACK = 0xff0e0c0c, PANEL = 0xff1d1817, RUST = 0xffa3402a, RUST_DARK = 0xff5e2418,
        RUST_LIGHT = 0xffd9744f, INK = 0xffeae2dc, MUTED = 0xffa8988f, GOOD = 0xff8fbf73, BAD = 0xffe0604f,
        EDGE = 0xff4a3a35, BUTTON = 0xff2a2120;

    private final Handler handler = new Handler(Looper.getMainLooper());
    private LinearLayout page;
    private final List<Button> tabs = new ArrayList<>();
    private int tab = 0;
    private String optionsPage = Options.PERFORMANCE;
    private Config config;
    private volatile GameFiles.Check check;
    private volatile boolean checking;
    private String note = "";
    private ProgressBar progress;
    private TextView progressText, progressDetail;
    private boolean watching;

    private final Runnable watcher = new Runnable() {
        @Override
        public void run() {
            if (!watching) {
                return;
            }
            ImportService.State s = ImportService.STATE;
            if (progress != null) {
                progress.setProgress(s.percent);
                progressText.setText(s.step + "  " + s.percent + "%");
                progressDetail.setText(s.detail);
            }
            if (!s.running && !s.starting) {
                watching = false;
                check = null;
                showTab(tab);
                return;
            }
            handler.postDelayed(this, 300);
        }
    };

    @Override
    protected void onCreate(Bundle state) {
        super.onCreate(state);
        config = new Config(this);
        buildFrame();
        // What the last run left goes to Download/SH3QuestVR, on a thread of its own: logcat takes a moment.
        new Thread(() -> RunLog.saveLastRun(this), "last-run-log").start();
        showTab(0);
        showWhatsNewOnce();
        Updater.maybeCheck(this, this::updatesChanged);
    }

    @Override
    protected void onResume() {
        super.onResume();
        config.reload();  // the VR menu may have changed sh3vr.ini
        check = null;
        showTab(tab);
    }

    @Override
    protected void onPause() {
        super.onPause();
        watching = false;
    }

    // ---- What's new ----

    private void showWhatsNewOnce() {
        SharedPreferences seen = getSharedPreferences("whats_new", MODE_PRIVATE);
        if (BuildConfig.VERSION_NAME.equals(seen.getString("shown", ""))) {
            return;
        }
        seen.edit().putString("shown", BuildConfig.VERSION_NAME).apply();
        showWhatsNew();
    }

    Dialog showWhatsNew() {
        final Dialog dialog = new Dialog(this);
        dialog.requestWindowFeature(Window.FEATURE_NO_TITLE);
        LinearLayout box = new LinearLayout(this);
        box.setOrientation(LinearLayout.VERTICAL);
        box.setBackground(plate(PANEL, RUST, 14));
        box.setPadding(dp(24), dp(20), dp(24), dp(18));
        box.addView(text("What's new in " + BuildConfig.VERSION_NAME, 22, RUST_LIGHT, true));
        TextView words = text(readAsset("whats_new.txt").trim(), 15, INK, false);
        words.setPadding(0, dp(10), 0, dp(14));
        ScrollView scroll = new ScrollView(this);
        scroll.addView(words);
        box.addView(scroll, new LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, dp(460)));
        box.addView(button("Got it", true, view -> dialog.dismiss()),
            new LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT));
        dialog.setContentView(box);
        Window window = dialog.getWindow();
        if (window != null) {
            window.setBackgroundDrawable(new ColorDrawable(0));
            window.setLayout(dp(820), ViewGroup.LayoutParams.WRAP_CONTENT);
        }
        dialog.show();
        return dialog;
    }

    // ---- The features, once (LIST 51: after the game files check out, dynamic cutscenes first) ----

    private void showFeaturesOnce() {
        SharedPreferences seen = getSharedPreferences("features", MODE_PRIVATE);
        if (seen.getBoolean("shown", false)) {
            return;
        }
        seen.edit().putBoolean("shown", true).apply();
        showFeatures();
    }

    Dialog showFeatures() {
        final Dialog dialog = new Dialog(this);
        dialog.requestWindowFeature(Window.FEATURE_NO_TITLE);
        LinearLayout box = new LinearLayout(this);
        box.setOrientation(LinearLayout.VERTICAL);
        box.setBackground(plate(PANEL, RUST, 14));
        box.setPadding(dp(24), dp(20), dp(24), dp(18));
        box.addView(text("Your game is ready: what the VR adds", 22, RUST_LIGHT, true));
        TextView words = text(readAsset("features.txt").trim(), 15, INK, false);
        words.setPadding(0, dp(10), 0, dp(14));
        ScrollView scroll = new ScrollView(this);
        scroll.addView(words);
        box.addView(scroll, new LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, dp(460)));
        box.addView(button("Got it", true, view -> dialog.dismiss()),
            new LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT));
        dialog.setContentView(box);
        Window window = dialog.getWindow();
        if (window != null) {
            window.setBackgroundDrawable(new ColorDrawable(0));
            window.setLayout(dp(820), ViewGroup.LayoutParams.WRAP_CONTENT);
        }
        dialog.show();
        return dialog;
    }

    // ---- The frame ----

    private int dp(float value) {
        return Math.round(TypedValue.applyDimension(TypedValue.COMPLEX_UNIT_DIP, value, getResources().getDisplayMetrics()));
    }

    private TextView text(String words, float size, int colour, boolean bold) {
        TextView view = new TextView(this);
        view.setText(words);
        view.setTextSize(size);
        view.setTextColor(colour);
        view.setLineSpacing(dp(2), 1f);
        if (bold) {
            view.setTypeface(Typeface.create("serif", Typeface.BOLD));
        }
        return view;
    }

    private GradientDrawable plate(int fill, int edge, float radius) {
        GradientDrawable plate = new GradientDrawable();
        plate.setColor(fill);
        plate.setStroke(dp(1), edge);
        plate.setCornerRadius(dp(radius));
        return plate;
    }

    private Button button(String label, boolean primary, View.OnClickListener action) {
        Button button = new Button(this);
        button.setText(label);
        button.setAllCaps(false);
        button.setTextSize(primary ? 20 : 16);
        button.setTextColor(INK);
        button.setTypeface(Typeface.create("sans-serif-medium", Typeface.BOLD));
        button.setBackground(plate(primary ? RUST : BUTTON, primary ? RUST_LIGHT : EDGE, 6));
        button.setPadding(dp(18), dp(10), dp(18), dp(10));
        button.setOnClickListener(action);
        return button;
    }

    private LinearLayout card() {
        LinearLayout card = new LinearLayout(this);
        card.setOrientation(LinearLayout.VERTICAL);
        card.setBackground(plate(PANEL, EDGE, 8));
        card.setPadding(dp(20), dp(16), dp(20), dp(16));
        LinearLayout.LayoutParams params = new LinearLayout.LayoutParams(
            ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT);
        params.bottomMargin = dp(12);
        card.setLayoutParams(params);
        return card;
    }

    private void gap(LinearLayout into, int height) {
        into.addView(new View(this), new LinearLayout.LayoutParams(1, dp(height)));
    }

    private void addToRow(LinearLayout row, Button button, float weight) {
        LinearLayout.LayoutParams params = new LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, weight);
        params.rightMargin = dp(6);
        row.addView(button, params);
    }

    private void buildFrame() {
        LinearLayout root = new LinearLayout(this);
        root.setOrientation(LinearLayout.VERTICAL);
        GradientDrawable fog = new GradientDrawable(GradientDrawable.Orientation.TOP_BOTTOM,
            new int[] {0xff2a2422, BACK, 0xff160d0b});
        root.setBackground(fog);
        root.setPadding(dp(24), dp(18), dp(24), dp(14));

        LinearLayout titles = new LinearLayout(this);
        titles.setOrientation(LinearLayout.VERTICAL);
        titles.addView(text("SILENT HILL 3", 28, RUST_LIGHT, true));
        titles.addView(text("Quest VR  ·  unofficial fan port  ·  v" + BuildConfig.VERSION_NAME, 13, MUTED, false));
        root.addView(titles);

        LinearLayout bar = new LinearLayout(this);
        bar.setPadding(0, dp(12), 0, dp(10));
        String[] names = {"Play", "Options", "Game files", "Controls", "Logs", "Credits"};
        for (int i = 0; i < names.length; i++) {
            final int index = i;
            Button tabButton = button(names[i], false, view -> showTab(index));
            addToRow(bar, tabButton, 1);
            tabs.add(tabButton);
        }
        root.addView(bar);

        ScrollView scroll = new ScrollView(this);
        page = new LinearLayout(this);
        page.setOrientation(LinearLayout.VERTICAL);
        scroll.addView(page);
        root.addView(scroll, new LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, 0, 1));
        setContentView(root);
    }

    private void showTab(int index) {
        tab = index;
        for (int i = 0; i < tabs.size(); i++) {
            tabs.get(i).setBackground(plate(i == index ? RUST_DARK : BUTTON, i == index ? RUST_LIGHT : EDGE, 6));
        }
        page.removeAllViews();
        switch (index) {
            case 0: showPlay(); break;
            case 1: showOptions(); break;
            case 2: showGameFiles(); break;
            case 3: showAsset("Controls", "controls.txt"); break;
            case 4: showLogs(); break;
            default: showAsset("Credits", "credits.txt"); break;
        }
    }

    // ---- The game files' check, on a thread (sh3.exe's SHA-1 takes a moment) ----

    private void needCheck() {
        if (check != null || checking) {
            return;
        }
        checking = true;
        new Thread(() -> {
            GameFiles.Check result;
            try {
                result = GameFiles.check(this);
            } catch (IOException e) {
                result = new GameFiles.Check();
            }
            final GameFiles.Check done = result;
            runOnUiThread(() -> {
                check = done;
                if (done.ready()) {
                    showFeaturesOnce();  // LIST 51
                }
                checking = false;
                if (tab == 0 || tab == 2) {
                    showTab(tab);
                }
            });
        }, "game-files-check").start();
    }

    // ---- Play ----

    private void showPlay() {
        String crash = RunLog.lastCrash(this);
        if (crash != null) {
            LinearLayout card = card();
            card.addView(text("The game closed with an error last time", 18, BAD, true));
            card.addView(text(crash, 14, INK, false));
            card.addView(text("The whole record is on the Logs tab and in Download/SH3QuestVR.", 13, MUTED, false));
            page.addView(card);
        }
        ImportService.State s = ImportService.STATE;
        if (s.running || s.starting) {
            showProgress();
            return;
        }
        showImportResult();
        needCheck();
        if (check == null) {
            LinearLayout card = card();
            card.addView(text("Checking the game files...", 18, INK, true));
            page.addView(card);
        } else if (!check.ready()) {
            showFilesNeeded();
        } else {
            LinearLayout card = card();
            card.addView(text("Ready", 24, GOOD, true));
            card.addView(text("Silent Hill 3 (PC, " + check.exeName + ")", 15, INK, false));
            if (check.exeData != null || !check.wrongSize.isEmpty() || !check.settled.isEmpty()) {
                card.addView(text(check.summary(), 13, MUTED, false));  // 0.1.4: what was accepted, and why
            }
            gap(card, 14);
            card.addView(button("PLAY IN VR", true, view -> play(true)));
            page.addView(card);

            LinearLayout flat = card();
            flat.addView(text("Flat on a big screen", 16, INK, true));
            flat.addView(text("The game on a screen in the headset, played with the Touch controllers as a pad. Also "
                + "tells a problem with the game from a problem with VR.", 14, MUTED, false));
            flat.addView(button("Play flat", false, view -> play(false)));
            page.addView(flat);
        }
        showUpdates();
    }

    private void showImportResult() {
        ImportService.State s = ImportService.STATE;
        if (s.failure != null) {
            LinearLayout card = card();
            card.addView(text("That did not work", 18, BAD, true));
            card.addView(text(s.failure, 15, INK, false));
            page.addView(card);
        } else if (s.done != null) {
            LinearLayout card = card();
            card.addView(text("Copied", 18, GOOD, true));
            card.addView(text(s.done, 15, INK, false));
            page.addView(card);
        }
        if (!note.isEmpty()) {
            LinearLayout card = card();
            card.addView(text(note, 15, INK, false));
            page.addView(card);
        }
    }

    private void showFilesNeeded() {
        LinearLayout card = card();
        card.addView(text("First, your game files", 22, RUST_LIGHT, true));
        card.addView(text("This port runs your own copy of Silent Hill 3 for the PC: the folder the game installed "
            + "(\"SILENT HILL 3\", with sh3.exe and the data folder, about 5 GB).\n\n"
            + "Which copies work: " + GameFiles.WHICH_GAME + " Not the PS2, Xbox or HD Collection versions.\n\n"
            + "Copy that folder to the headset over USB (into Download, for example), then choose it here. Or copy it "
            + "straight to Android/data/" + getPackageName() + "/files/game and press Check again.", 15, INK, false));
        if (check != null && check.exeFound) {
            card.addView(text("\nWhat the check found: " + check.summary(), 14, MUTED, false));
        }
        gap(card, 14);
        card.addView(button("CHOOSE THE GAME FOLDER", true, view -> pick(REQUEST_FOLDER)));
        LinearLayout row = new LinearLayout(this);
        row.setPadding(0, dp(8), 0, 0);
        addToRow(row, button("A .zip of it instead", false, view -> pick(REQUEST_ZIP)), 1);
        addToRow(row, button("Check again", false, view -> {
            check = null;
            showTab(tab);
        }), 1);
        card.addView(row);
        page.addView(card);
        if (check != null && check.exeFound) {
            // 0.1.4 (owner 93: "make sure this build is compatible and works in all regards"): never stricter than the
            // other Android port, which starts whatever sh3.exe it finds. A copy the check is not sure of can still be
            // tried; the game's own log says where it stopped, if it does.
            LinearLayout anyway = card();
            anyway.addView(text("Try it anyway", 16, INK, true));
            anyway.addView(text("The check above is not sure this copy runs, but the port can start it. If the game "
                + "stops, the Logs tab says where and why.", 14, MUTED, false));
            LinearLayout buttons = new LinearLayout(this);
            buttons.setPadding(0, dp(8), 0, 0);
            addToRow(buttons, button("Play in VR anyway", false, view -> play(true)), 1);
            addToRow(buttons, button("Play flat anyway", false, view -> play(false)), 1);
            anyway.addView(buttons);
            page.addView(anyway);
        }
    }

    private void showProgress() {
        LinearLayout card = card();
        card.addView(text("Copying the game files", 22, RUST_LIGHT, true));
        progressText = text(ImportService.STATE.step, 16, INK, true);
        card.addView(progressText);
        progress = new ProgressBar(this, null, android.R.attr.progressBarStyleHorizontal);
        progress.setMax(100);
        progress.setProgress(ImportService.STATE.percent);
        card.addView(progress);
        progressDetail = text(ImportService.STATE.detail, 13, MUTED, false);
        card.addView(progressDetail);
        card.addView(text("It goes on with the panel closed. Only needed once: updates keep the files.", 13, MUTED, false));
        page.addView(card);
        watching = true;
        handler.post(watcher);
    }

    private void pick(int request) {
        Intent pick;
        if (request == REQUEST_ZIP) {
            pick = new Intent(Intent.ACTION_OPEN_DOCUMENT);
            pick.addCategory(Intent.CATEGORY_OPENABLE);
            pick.setType("*/*");
            pick.putExtra(Intent.EXTRA_MIME_TYPES, new String[] {"application/zip", "application/x-zip-compressed", "*/*"});
        } else {
            pick = new Intent(Intent.ACTION_OPEN_DOCUMENT_TREE);
        }
        try {
            startActivityForResult(pick, request);
        } catch (ActivityNotFoundException e) {
            note = "This headset has no file picker to choose with. Copy the folder over USB to Android/data/"
                + getPackageName() + "/files/game instead.";
            showTab(tab);
        }
    }

    @Override
    protected void onActivityResult(int request, int result, Intent data) {
        super.onActivityResult(request, result, data);
        if (result != RESULT_OK || data == null || data.getData() == null) {
            return;
        }
        Uri uri = data.getData();
        try {
            getContentResolver().takePersistableUriPermission(uri, Intent.FLAG_GRANT_READ_URI_PERMISSION);
        } catch (SecurityException e) {
            // A one-time grant is enough while the copy runs.
        }
        String mode = request == REQUEST_ZIP ? ImportService.MODE_ZIP
            : request == REQUEST_SAVES ? ImportService.MODE_SAVES : ImportService.MODE_FOLDER;
        note = "";
        try {
            ImportService.start(this, uri, mode);
        } catch (RuntimeException e) {
            ImportService.STATE.starting = false;
            ImportService.STATE.failure = "The copy could not start: " + e.getMessage();
        }
        showTab(request == REQUEST_SAVES ? 2 : 0);
    }

    private void play(boolean vr) {
        new Thread(() -> {
            try {
                config.writeForGame(vr);
                runOnUiThread(() -> {
                    Intent game = new Intent(this, QuestGameActivity.class)
                        .putExtra(QuestGameActivity.EXTRA_GAME_DIR, GameFiles.folder(this).getPath());
                    startActivity(game);
                    // As our other ports: the panel goes as the headset turns to the game.
                    finish();
                });
            } catch (IOException e) {
                runOnUiThread(() -> {
                    note = "The game could not be started: " + e.getMessage();
                    showTab(0);
                });
            }
        }, "game-start").start();
    }

    // ---- Updates ----

    private TextView updateText;

    private final Runnable updateWatcher = new Runnable() {
        @Override
        public void run() {
            if (updateText != null) {
                updateText.setText(updateLine());
            }
            if (Updater.STATE.busy) {
                handler.postDelayed(this, 300);
            }
        }
    };

    private String updateLine() {
        String line = Updater.STATE.percent >= 0 ? "Downloading: " + Updater.STATE.percent + "%" : Updater.status(this);
        return line + "\nInstalled: " + BuildConfig.VERSION_NAME
            + ". The launcher asks GitHub for this project's releases; nothing of yours is sent.";
    }

    private void showUpdates() {
        LinearLayout card = card();
        card.addView(text("Updates", 16, INK, true));
        updateText = text(updateLine(), 14, MUTED, false);
        card.addView(updateText);
        LinearLayout row = new LinearLayout(this);
        row.setPadding(0, dp(8), 0, 0);
        String tag = Updater.STATE.tag;
        if (tag != null) {
            String label = (Updater.STATE.ready ? "Install " : "Download and install ") + tag.substring(1);
            addToRow(row, button(label, false, view -> {
                Updater.download(this, this::updatesChanged);
                handler.post(updateWatcher);
            }), 2);
        }
        addToRow(row, button("Check now", false, view -> {
            Updater.check(this, this::updatesChanged);
            handler.post(updateWatcher);
        }), 1);
        addToRow(row, button("Automatic checks: " + (Updater.automatic(this) ? "On" : "Off"), false, view -> {
            Updater.setAutomatic(this, !Updater.automatic(this));
            showTab(0);
        }), 1);
        addToRow(row, button("What's new", false, view -> showWhatsNew()), 1);
        addToRow(row, button("Features", false, view -> showFeatures()), 1);
        card.addView(row);
        page.addView(card);
    }

    private void updatesChanged() {
        if (tab == 0 && !isFinishing() && !ImportService.STATE.running) {
            showTab(0);
        }
    }

    // ---- Options (sh3vr.ini: the in-game VR menu has the same; hold the left menu button in the game for it) ----

    private void showOptions() {
        LinearLayout pages = new LinearLayout(this);
        pages.setPadding(0, 0, 0, dp(10));
        for (String name : Options.PAGES) {
            Button b = button(name, false, view -> {
                optionsPage = name;
                showTab(1);
            });
            if (name.equals(optionsPage)) {
                b.setBackground(plate(RUST_DARK, RUST_LIGHT, 6));
            }
            addToRow(pages, b, 1);
        }
        page.addView(pages);

        LinearLayout top = card();
        if (optionsPage.equals(Options.PERFORMANCE)) {
            top.addView(text("Recommended performance options", 16, INK, true));
            top.addView(text("Already in use unless changed: the Quest 3's best for this game. The button puts this "
                + "page back to them.", 13, MUTED, false));
        } else {
            top.addView(text(optionsPage, 16, INK, true));
            top.addView(text("Every VR feature starts on. The in-game VR menu has these too: pause the game, then "
                + "hold the left menu button.", 13, MUTED, false));
        }
        top.addView(button(optionsPage.equals(Options.PERFORMANCE) ? "Use the recommended options" : "This page back to the defaults",
            false, view -> {
                try {
                    config.recommended(optionsPage);
                    if (optionsPage.equals(Options.PERFORMANCE)) {
                        config.set(Options.GAME_THREADS, Options.GAME_THREADS.fallback);
                    }
                } catch (IOException e) {
                    note = "Could not save: " + e.getMessage();
                }
                showTab(1);
            }));
        page.addView(top);

        for (Options.Option option : Options.ALL) {
            if (option.page.equals(optionsPage)) {
                showOption(option);
            }
        }
        if (optionsPage.equals(Options.PERFORMANCE)) {
            showOption(Options.GAME_THREADS);
        }
    }

    private void showOption(Options.Option option) {
        LinearLayout card = card();
        String current = config.get(option);
        boolean recommended = current.equals(option.fallback) || (option.kind == Options.Kind.NUMBER
            && Float.parseFloat(current) == Float.parseFloat(option.fallback));
        card.addView(text(option.label + (recommended ? "" : "   (changed)"), 16, INK, true));
        if (!option.help.isEmpty()) {
            card.addView(text(option.help, 13, MUTED, false));
        }
        LinearLayout row = new LinearLayout(this);
        row.setPadding(0, dp(8), 0, 0);
        if (option.kind == Options.Kind.NUMBER) {
            addToRow(row, button("-", false, view -> stepOption(option, -1)), 1);
            TextView value = text(current + " " + option.unit, 18, INK, true);
            value.setGravity(Gravity.CENTER);
            row.addView(value, new LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 2));
            addToRow(row, button("+", false, view -> stepOption(option, 1)), 1);
        } else {
            for (int i = 0; i < option.names.length; i++) {
                final String value = option.values[i];
                Button b = button(option.names[i], false, view -> setOption(option, value));
                if (value.equals(current)) {
                    b.setBackground(plate(RUST_DARK, RUST_LIGHT, 6));
                }
                addToRow(row, b, 1);
            }
        }
        card.addView(row);
        page.addView(card);
    }

    private void setOption(Options.Option option, String value) {
        try {
            config.set(option, value);
        } catch (IOException e) {
            note = "Could not save: " + e.getMessage();
        }
        showTab(1);
    }

    private void stepOption(Options.Option option, int dir) {
        try {
            config.step(option, dir);
        } catch (IOException e) {
            note = "Could not save: " + e.getMessage();
        }
        showTab(1);
    }

    // ---- Game files ----

    private void showGameFiles() {
        ImportService.State s = ImportService.STATE;
        if (s.running || s.starting) {
            showProgress();
            return;
        }
        showImportResult();
        needCheck();
        LinearLayout card = card();
        card.addView(text("The game files", 18, INK, true));
        card.addView(text(check == null ? "Checking..." : check.summary(), 15, check != null && check.ready() ? GOOD : INK, false));
        card.addView(text("Folder: Android/data/" + getPackageName() + "/files/game (kept through updates; removed "
            + "only if the app is uninstalled).", 13, MUTED, false));
        LinearLayout row = new LinearLayout(this);
        row.setPadding(0, dp(8), 0, 0);
        addToRow(row, button("Choose the game folder", false, view -> pick(REQUEST_FOLDER)), 1);
        addToRow(row, button("A .zip of it", false, view -> pick(REQUEST_ZIP)), 1);
        addToRow(row, button("Check again", false, view -> {
            check = null;
            showTab(2);
        }), 1);
        card.addView(row);
        page.addView(card);

        LinearLayout saves = card();
        saves.addView(text("Saves", 18, INK, true));
        File folder = GameFiles.saves(this);
        String[] files = folder.list();
        saves.addView(text("In the game's savedata folder, as on the PC: " + (files == null ? 0 : files.length)
            + " files. Your PC saves (the PC game's savedata folder) can be brought in: they are copied next to "
            + "these, a save of the same name replaced.", 14, MUTED, false));
        saves.addView(button("Import PC saves (choose their folder)", false, view -> pick(REQUEST_SAVES)));
        page.addView(saves);
    }

    // ---- Logs ----

    private void showLogs() {
        LinearLayout card = card();
        card.addView(text("The last run", 18, INK, true));
        card.addView(text("The game's own record: what it did, and if it closed, why (its stacks, the game's state, "
            + "VR's). After each run a copy goes to Download/SH3QuestVR, where a PC sees it over USB.", 13, MUTED, false));
        LinearLayout row = new LinearLayout(this);
        row.setPadding(0, dp(8), 0, dp(8));
        addToRow(row, button("Save a report to Download", false, view -> new Thread(() -> {
            RunLog.write(this, "report_" + System.currentTimeMillis() + ".txt", RunLog.report(this));
            runOnUiThread(() -> {
                note = "Saved in Download/SH3QuestVR.";
                showTab(4);
            });
        }, "report").start()), 1);
        addToRow(row, button("Share the report", false, view -> share()), 1);
        card.addView(row);
        if (!note.isEmpty()) {
            card.addView(text(note, 14, GOOD, false));
            note = "";
        }
        File game = GameFiles.folder(this);
        String crash = RunLog.tail(new File(game, "sh3-crash.log"), 16 << 10);
        if (!crash.equals("(none)")) {
            card.addView(text("sh3-crash.log", 15, BAD, true));
            card.addView(text(crash, 12, INK, false));
        }
        card.addView(text("sh3.log (its end)", 15, INK, true));
        TextView log = text(RunLog.tail(new File(game, "sh3.log"), 24 << 10), 12, INK, false);
        log.setTypeface(Typeface.MONOSPACE);
        card.addView(log);
        page.addView(card);
    }

    private void share() {
        new Thread(() -> {
            try {
                RunLog.reportFile(this);
                runOnUiThread(() -> {
                    Intent send = new Intent(Intent.ACTION_SEND)
                        .setType("text/plain")
                        .putExtra(Intent.EXTRA_STREAM, ReportProvider.URI)
                        .putExtra(Intent.EXTRA_SUBJECT, "SH3 Quest VR " + BuildConfig.VERSION_NAME + " report")
                        .addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION);
                    send.setClipData(ClipData.newRawUri("report", ReportProvider.URI));
                    try {
                        startActivity(Intent.createChooser(send, "Share the report"));
                    } catch (ActivityNotFoundException e) {
                        note = "No app here can take it; use Save a report to Download instead.";
                        showTab(4);
                    }
                });
            } catch (IOException e) {
                runOnUiThread(() -> {
                    note = "The report could not be made: " + e.getMessage();
                    showTab(4);
                });
            }
        }, "report-share").start();
    }

    // ---- Controls, Credits ----

    private void showAsset(String title, String asset) {
        LinearLayout card = card();
        card.addView(text(title, 18, INK, true));
        TextView words = text(readAsset(asset).trim(), 14, INK, false);
        words.setPadding(0, dp(8), 0, 0);
        card.addView(words);
        page.addView(card);
    }

    private String readAsset(String name) {
        StringBuilder out = new StringBuilder();
        try (BufferedReader reader = new BufferedReader(new InputStreamReader(getAssets().open(name), StandardCharsets.UTF_8))) {
            for (String line = reader.readLine(); line != null; line = reader.readLine()) {
                out.append(line).append('\n');
            }
        } catch (IOException e) {
            out.append("(").append(name).append(" is missing from this build)");
        }
        return out.toString();
    }
}
