// SPDX-License-Identifier: GPL-3.0-or-later
package dev.sh3quest.vr;

import android.content.Context;
import android.media.AudioAttributes;
import android.media.AudioFocusRequest;
import android.media.AudioManager;
import android.os.Bundle;
import android.util.Log;
import android.view.WindowManager;

import org.libsdl.app.SDLActivity;

/**
 * The game, immersive (AndroidManifest.xml: its own process, the headset's entries): SDL2's activity, which loads
 * libSDL2.so and libmain.so (the recompiled game, its VR and the runtime) and runs SDL_main with the game folder the
 * launcher set up (src/runtime/rt_core.c main). The launcher writes sh3.ini and sh3vr.ini there before starting it.
 */
public final class QuestGameActivity extends SDLActivity {
    private static final String TAG = "SH3QuestVR";
    static final String EXTRA_GAME_DIR = "game_dir";

    /** The game's hold on the sound, asked whenever it has the focus (Ratchet 0.1.18: "Hey Meta" left it silent). */
    private AudioFocusRequest audioFocus;

    @Override
    protected String[] getLibraries() {
        return new String[] {"SDL2", "main"};
    }

    @Override
    protected String[] getArguments() {
        String dir = getIntent() != null ? getIntent().getStringExtra(EXTRA_GAME_DIR) : null;
        if (dir == null) {
            dir = GameFiles.folder(this).getPath();
        }
        Log.i(TAG, "game folder " + dir);
        return new String[] {dir};
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
    }

    @Override
    public void onWindowFocusChanged(boolean hasFocus) {
        super.onWindowFocusChanged(hasFocus);
        if (hasFocus) {
            askForSound();
        }
    }

    @Override
    protected void onResume() {
        super.onResume();
        askForSound();
    }

    private void askForSound() {
        AudioManager audio = (AudioManager) getSystemService(Context.AUDIO_SERVICE);
        if (audio == null) {
            return;
        }
        if (audioFocus == null) {
            AudioAttributes game = new AudioAttributes.Builder()
                .setUsage(AudioAttributes.USAGE_GAME)
                .setContentType(AudioAttributes.CONTENT_TYPE_MUSIC)
                .build();
            audioFocus = new AudioFocusRequest.Builder(AudioManager.AUDIOFOCUS_GAIN)
                .setAudioAttributes(game)
                .setWillPauseWhenDucked(false)
                .setOnAudioFocusChangeListener(change -> Log.i(TAG, "sound focus: " + change))
                .build();
        }
        int result = audio.requestAudioFocus(audioFocus);
        Log.i(TAG, "sound focus asked: " + (result == AudioManager.AUDIOFOCUS_REQUEST_GRANTED ? "granted" : "not now (" + result + ")"));
    }
}
