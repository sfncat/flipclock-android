package one.alynx.flipclock;

import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.os.Build;
import android.os.Bundle;
import android.os.SystemClock;
import android.util.Log;
import android.view.MotionEvent;
import android.view.WindowManager;

import java.io.File;
import java.io.FileOutputStream;
import java.io.InputStream;
import java.nio.charset.StandardCharsets;

import org.libsdl.app.SDLActivity;

/**
 * A sample wrapper class that just calls SDLActivity
 */

public class MainActivity extends SDLActivity {
    private static final String PREFS_NAME = "flipclock_settings";
    private static final String TAG = "MainActivity";
    private static final int SETTINGS_GESTURE_FINGERS = 4;
    private static final long SETTINGS_GESTURE_DEBOUNCE_MS = 1000;

    private long mLastSettingsOpenTime = 0;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        // Must be called before SDLActivity initializes the native layer,
        // so that the C code can read the latest settings from flipclock.conf.
        writeNativeConf();
        // The native layer loads the fonts from the internal storage, so copy
        // them out of the APK assets first.
        copyAssets();
        super.onCreate(savedInstanceState);
        keepScreenOnAfterBoot();
    }

    /**
     * 根据 SharedPreferences 生成 native 层使用的 flipclock.conf 并写入应用
     * 内部存储。C 层通过 SDL_AndroidGetInternalStoragePath() 读取该文件，
     * 用于控制主界面上日期/星期/农历的显示。
     */
    private void writeNativeConf() {
        try {
            SharedPreferences prefs =
                    getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE);
            StringBuilder sb = new StringBuilder();
            sb.append("show_date=")
                    .append(prefs.getBoolean("show_date", true)).append('\n');
            sb.append("show_weekday=")
                    .append(prefs.getBoolean("show_weekday", true)).append('\n');
            sb.append("show_lunar=")
                    .append(prefs.getBoolean("show_lunar", true)).append('\n');
            sb.append("show_lunar_year=")
                    .append(prefs.getBoolean("show_lunar_year", false)).append('\n');
            sb.append("info_vertical=")
                    .append(prefs.getBoolean("info_vertical", true)).append('\n');
            sb.append("burn_in_protection=")
                    .append(prefs.getBoolean("burn_in_protection", false))
                    .append('\n');
            sb.append("burn_in_protection_offset=")
                    .append(prefs.getFloat("burn_in_protection_offset", 1.5f))
                    .append('\n');
            File conf = new File(getFilesDir(), "flipclock.conf");
            FileOutputStream fos = new FileOutputStream(conf);
            fos.write(sb.toString().getBytes(StandardCharsets.UTF_8));
            fos.close();
        } catch (Exception e) {
            Log.w(TAG, "Failed to write flipclock.conf", e);
        }
    }

    /**
     * Copy the bundled font files from the APK assets into the internal
     * storage, where the native layer expects to find them
     * (SDL_AndroidGetInternalStoragePath()). Asset files are immutable, so
     * the copy is only needed once per install.
     */
    private void copyAssets() {
        copyAssetToFiles("flipclock.ttf");
        copyAssetToFiles("flipclock_cjk.ttf");
    }

    private void copyAssetToFiles(String assetName) {
        try {
            File out = new File(getFilesDir(), assetName);
            // Always overwrite: a stale copy (e.g. a broken placeholder from an
            // older app version) must not survive an app update.
            try (InputStream in = getAssets().open(assetName);
                 FileOutputStream fos = new FileOutputStream(out)) {
                byte[] buf = new byte[8192];
                int len;
                while ((len = in.read(buf)) > 0) {
                    fos.write(buf, 0, len);
                }
                fos.flush();
            }
        } catch (Exception e) {
            Log.w(TAG, "Failed to copy asset " + assetName, e);
        }
    }

    /**
     * Keep the screen on when the activity is visible, and turn it on when the
     * activity is started while the screen is off (e.g. after boot). This is
     * used by the auto-start feature to display the clock immediately without
     * requiring a manual wake.
     */
    private void keepScreenOnAfterBoot() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O_MR1) {
            setTurnScreenOn(true);
            setShowWhenLocked(true);
        }
        getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON
                | WindowManager.LayoutParams.FLAG_TURN_SCREEN_ON
                | WindowManager.LayoutParams.FLAG_SHOW_WHEN_LOCKED);
    }

    /**
     * Open SettingsActivity when four fingers touch the screen at the same
     * time. This provides a way to access settings from the fullscreen clock
     * without adding visible UI elements.
     */
    @Override
    public boolean dispatchTouchEvent(MotionEvent ev) {
        if (ev.getActionMasked() == MotionEvent.ACTION_POINTER_DOWN
                && ev.getPointerCount() == SETTINGS_GESTURE_FINGERS) {
            long now = SystemClock.elapsedRealtime();
            if (now - mLastSettingsOpenTime > SETTINGS_GESTURE_DEBOUNCE_MS) {
                mLastSettingsOpenTime = now;
                openSettings();
            }
            return true;
        }
        return super.dispatchTouchEvent(ev);
    }

    private void openSettings() {
        Intent intent = new Intent(this, SettingsActivity.class);
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK | Intent.FLAG_ACTIVITY_CLEAR_TOP);
        startActivity(intent);
    }
}
