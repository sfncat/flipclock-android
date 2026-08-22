package com.stackof.flipclockv2;

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

import com.stackof.flipclockv2.weather.WeatherInfo;
import com.stackof.flipclockv2.weather.WeatherListener;
import com.stackof.flipclockv2.weather.WeatherManager;

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
    private WeatherManager mWeatherManager;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        // Must be called before SDLActivity initializes the native layer,
        // so that the C code can read the latest settings from flipclock.conf.
        writeNativeConf(this);
        // The native layer loads the fonts from the internal storage, so copy
        // them out of the APK assets first.
        copyAssets();
        super.onCreate(savedInstanceState);
        keepScreenOnAfterBoot();
        startWeatherManager();
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        if (mWeatherManager != null) {
            mWeatherManager.stop();
            mWeatherManager = null;
        }
    }

    /**
     * 根据 SharedPreferences 生成 native 层使用的 flipclock.conf 并写入应用
     * 内部存储。C 层通过 SDL_AndroidGetInternalStoragePath() 读取该文件，
     * 用于控制主界面上日期/星期/农历的显示。
     */
    static void writeNativeConf(Context context) {
        try {
            SharedPreferences prefs =
                    context.getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE);
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
                    .append(prefs.getBoolean("burn_in_protection", true))
                    .append('\n');
            sb.append("burn_in_protection_offset=")
                    .append(prefs.getFloat("burn_in_protection_offset", 5.0f))
                    .append('\n');
            sb.append("show_weather=")
                    .append(prefs.getBoolean("show_weather", false))
                    .append('\n');
            sb.append("weather_location=")
                    .append(prefs.getString("weather_location", ""))
                    .append('\n');
            sb.append("weather_update_interval_hours=")
                    .append(prefs.getInt("weather_update_interval_hours", 1))
                    .append('\n');
            String infoBarFont = prefs.getString("info_bar_font",
                    "LXGWXiHeiMN.ttf");
            if (!infoBarFont.isEmpty()) {
                sb.append("info_bar_font=fonts/")
                        .append(infoBarFont).append('\n');
            }
            String weatherFont = prefs.getString("weather_font",
                    "LXGWNeoZhiSong.ttf");
            if (!weatherFont.isEmpty()) {
                sb.append("weather_font=fonts/")
                        .append(weatherFont).append('\n');
            }
            File conf = new File(context.getFilesDir(), "flipclock.conf");
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
        copyFontAssets();
    }

    /**
     * Copy all bundled font files from {@code assets/fonts/} into the app
     * internal storage so the native layer can load them by filename.
     */
    private void copyFontAssets() {
        String[] fontFiles = {
            "LXGWMarkerGothic-Regular.ttf",
            "LXGWNeoXiHei.ttf",
            "LXGWNeoZhiSong.ttf",
            "LXGWWenKaiMonoGBLite-Regular.ttf",
            "LXGWXiHeiMN.ttf",
            "LXGWZhenKaiGB-Regular.ttf",
            "SmileySans-Oblique.ttf",
            "WenYuanSansSC-Heavy.ttf",
            "XiaolaiMono-Regular.ttf",
            "Yozai-Regular.ttf"
        };
        for (String fontFile : fontFiles) {
            copyAssetToFiles("fonts/" + fontFile);
        }
    }

    private void copyAssetToFiles(String assetName) {
        try {
            File out = new File(getFilesDir(), assetName);
            // Ensure parent directories exist (e.g. fonts/).
            File parent = out.getParentFile();
            if (parent != null && !parent.exists()) {
                parent.mkdirs();
            }
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

    /**
     * Start the weather manager if the user has enabled weather display and
     * supplied a location. The native library is loaded by SDLActivity.onCreate(),
     * so this is called after super.onCreate() to ensure JNI methods are available.
     */
    private void startWeatherManager() {
        SharedPreferences prefs = getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE);
        boolean showWeather = prefs.getBoolean("show_weather", false);
        String location = prefs.getString("weather_location", "").trim();
        if (!showWeather || location.isEmpty()) {
            return;
        }

        int intervalHours = prefs.getInt("weather_update_interval_hours", 3);

        if (mWeatherManager != null) {
            mWeatherManager.stop();
        }
        mWeatherManager = new WeatherManager(this, PREFS_NAME);
        mWeatherManager.setUpdateIntervalHours(intervalHours);
        mWeatherManager.setWeatherListener(new WeatherListener() {
            @Override
            public void onWeatherUpdated(WeatherInfo info) {
                String locationName = info.getLocation().getName();
                String temperature = info.getTemperature() + "°C";
                String description = info.getDescription();
                WeatherBridge.nativeUpdateWeather(locationName, temperature, description);
                // Clear any stored weather error on success.
                prefs.edit().remove("weather_error").apply();
            }

            @Override
            public void onError(Throwable error) {
                Log.w(TAG, "Weather update failed", error);
                // Store error message so SettingsActivity can show it.
                String msg = error.getMessage();
                if (msg == null || msg.isEmpty()) {
                    msg = error.getClass().getSimpleName();
                }
                prefs.edit().putString("weather_error", msg).apply();
            }
        });
        mWeatherManager.setLocation(location);
        mWeatherManager.start();
    }
}
