package com.stackof.flipclockv2;

import android.app.Activity;
import android.app.AlertDialog;
import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.content.pm.PackageManager;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.os.PowerManager;
import android.provider.Settings;
import android.widget.Button;
import android.widget.CompoundButton;
import android.widget.EditText;
import android.widget.LinearLayout;
import android.widget.SeekBar;
import android.widget.Switch;
import android.widget.TextView;

import com.stackof.flipclockv2.weather.GeoLocation;
import com.stackof.flipclockv2.weather.WeatherInfo;
import com.stackof.flipclockv2.weather.WeatherListener;
import com.stackof.flipclockv2.weather.WeatherManager;

import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicReference;

/**
 * Settings activity for FlipClockV2.
 *
 * Provides a user-controlled switch to enable/disable auto start after boot.
 * The first time the user enables the feature, a confirmation dialog is
 * shown and required permissions are requested.
 */
public class SettingsActivity extends Activity {
    private static final String PREFS_NAME = "flipclock_settings";
    private static final String KEY_AUTO_START = "auto_start_enabled";
    private static final String KEY_SHOW_DATE = "show_date";
    private static final String KEY_SHOW_WEEKDAY = "show_weekday";
    private static final String KEY_SHOW_LUNAR = "show_lunar";
    private static final String KEY_SHOW_LUNAR_YEAR = "show_lunar_year";
    private static final String KEY_INFO_VERTICAL = "info_vertical";
    private static final String KEY_BURN_IN_PROTECTION = "burn_in_protection";
    private static final String KEY_BURN_IN_PROTECTION_OFFSET =
            "burn_in_protection_offset";
    private static final float BURN_IN_OFFSET_DEFAULT = 5.0f;
    private static final int BURN_IN_OFFSET_MAX_PROGRESS = 50;
    private static final String KEY_SHOW_WEATHER = "show_weather";
    private static final String KEY_WEATHER_LOCATION = "weather_location";
    private static final String KEY_WEATHER_UPDATE_INTERVAL =
            "weather_update_interval_hours";
    private static final int WEATHER_UPDATE_INTERVAL_DEFAULT = 1;
    private static final int REQUEST_NOTIFICATION_PERMISSION = 100;
    private static final int REQUEST_BATTERY_OPTIMIZATION = 101;

    private static final String KEY_INFO_BAR_FONT = "info_bar_font";
    private static final String KEY_WEATHER_FONT = "weather_font";

    private static final String KEY_WEATHER_MERGE_LANDSCAPE = "weather_merge_landscape";
    private static final String KEY_DATE_ON_TOP_PORTRAIT = "date_on_top_portrait";
    private static final String KEY_WEATHER_LARGE_THREE_BARS = "weather_large_three_bars";

    private static final String DEFAULT_INFO_BAR_FONT = "LXGWXiHeiMN.ttf";
    private static final String DEFAULT_WEATHER_FONT = "LXGWNeoZhiSong.ttf";

    private static final String[] FONT_FILES = {
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
    private static final String[] FONT_DISPLAY_NAMES = {
        "霞鹜漫黑",
        "霞鹜新禧黑",
        "霞鹜新智宋",
        "霞鹜文楷 GB Lite",
        "霞鹜禧黑 MN",
        "霞鹜真楷 GB",
        "得意黑",
        "文泉驿等宽微米黑",
        "小赖字体",
        "悠哉字体"
    };

    private Switch autoStartSwitch;
    private Switch showDateSwitch;
    private Switch showWeekdaySwitch;
    private Switch showLunarSwitch;
    private Switch showLunarYearSwitch;
    private Switch infoVerticalSwitch;
    private Switch burnInProtectionSwitch;
    private Switch weatherMergeLandscapeSwitch;
    private Switch dateOnTopPortraitSwitch;
    private Switch weatherLargeThreeBarsSwitch;
    private LinearLayout burnInProtectionOffsetContainer;
    private TextView burnInProtectionOffsetSummary;
    private SeekBar burnInProtectionOffsetSeekBar;
    private Button overlayButton;
    private TextView descriptionText;
    private Switch showWeatherSwitch;
    private LinearLayout weatherLocationContainer;
    private EditText weatherLocationEdit;
    private LinearLayout weatherUpdateIntervalContainer;
    private TextView weatherUpdateIntervalSummary;
    private SeekBar weatherUpdateIntervalSeekBar;
    private Button infoBarFontButton;
    private Button weatherFontButton;
    private Button weatherValidateButton;
    private Button restartButton;
    private TextView weatherErrorText;
    private SharedPreferences prefs;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_settings);

        prefs = getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE);

        descriptionText = findViewById(R.id.auto_start_description);
        autoStartSwitch = findViewById(R.id.auto_start_switch);
        overlayButton = findViewById(R.id.overlay_permission_button);
        Button batteryButton = findViewById(R.id.battery_optimization_button);
        showDateSwitch = findViewById(R.id.show_date_switch);
        showWeekdaySwitch = findViewById(R.id.show_weekday_switch);
        showLunarSwitch = findViewById(R.id.show_lunar_switch);
        showLunarYearSwitch = findViewById(R.id.show_lunar_year_switch);
        infoVerticalSwitch = findViewById(R.id.info_vertical_switch);
        burnInProtectionSwitch = findViewById(R.id.burn_in_protection_switch);
        weatherMergeLandscapeSwitch = findViewById(R.id.weather_merge_landscape_switch);
        dateOnTopPortraitSwitch = findViewById(R.id.date_on_top_portrait_switch);
        weatherLargeThreeBarsSwitch = findViewById(R.id.weather_large_three_bars_switch);
        burnInProtectionOffsetContainer =
                findViewById(R.id.burn_in_protection_offset_container);
        burnInProtectionOffsetSummary =
                findViewById(R.id.burn_in_protection_offset_summary);
        burnInProtectionOffsetSeekBar =
                findViewById(R.id.burn_in_protection_offset_seekbar);
        showWeatherSwitch = findViewById(R.id.show_weather_switch);
        weatherLocationContainer = findViewById(R.id.weather_location_container);
        weatherLocationEdit = findViewById(R.id.weather_location_edit);
        weatherUpdateIntervalContainer =
                findViewById(R.id.weather_update_interval_container);
        weatherUpdateIntervalSummary =
                findViewById(R.id.weather_update_interval_summary);
        weatherUpdateIntervalSeekBar =
                findViewById(R.id.weather_update_interval_seekbar);
        infoBarFontButton = findViewById(R.id.info_bar_font_button);
        weatherFontButton = findViewById(R.id.weather_font_button);
        weatherValidateButton = findViewById(R.id.weather_validate_button);
        restartButton = findViewById(R.id.restart_button);
        weatherErrorText = findViewById(R.id.weather_error_text);

        boolean enabled = prefs.getBoolean(KEY_AUTO_START, false);
        autoStartSwitch.setChecked(enabled);

        autoStartSwitch.setOnCheckedChangeListener(new CompoundButton.OnCheckedChangeListener() {
            @Override
            public void onCheckedChanged(CompoundButton buttonView, boolean isChecked) {
                if (isChecked) {
                    showEnableConfirmationDialog();
                } else {
                    saveAutoStart(false);
                    updateDescription();
                }
            }
        });

        showDateSwitch.setChecked(prefs.getBoolean(KEY_SHOW_DATE, true));
        showWeekdaySwitch.setChecked(prefs.getBoolean(KEY_SHOW_WEEKDAY, true));
        showLunarSwitch.setChecked(prefs.getBoolean(KEY_SHOW_LUNAR, true));
        showLunarYearSwitch.setChecked(
                prefs.getBoolean(KEY_SHOW_LUNAR_YEAR, false));
        infoVerticalSwitch.setChecked(
                prefs.getBoolean(KEY_INFO_VERTICAL, true));
        burnInProtectionSwitch.setChecked(
                prefs.getBoolean(KEY_BURN_IN_PROTECTION, true));
        weatherMergeLandscapeSwitch.setChecked(
                prefs.getBoolean(KEY_WEATHER_MERGE_LANDSCAPE, true));
        dateOnTopPortraitSwitch.setChecked(
                prefs.getBoolean(KEY_DATE_ON_TOP_PORTRAIT, true));
        weatherLargeThreeBarsSwitch.setChecked(
                prefs.getBoolean(KEY_WEATHER_LARGE_THREE_BARS, false));
        setupBurnInProtectionOffset();
        showDateSwitch.setOnCheckedChangeListener(
                (buttonView, isChecked) -> prefs.edit().putBoolean(KEY_SHOW_DATE, isChecked).apply());
        showWeekdaySwitch.setOnCheckedChangeListener(
                (buttonView, isChecked) -> prefs.edit().putBoolean(KEY_SHOW_WEEKDAY, isChecked).apply());
        showLunarSwitch.setOnCheckedChangeListener(
                (buttonView, isChecked) -> prefs.edit().putBoolean(KEY_SHOW_LUNAR, isChecked).apply());
        showLunarYearSwitch.setOnCheckedChangeListener(
                (buttonView, isChecked) -> prefs.edit().putBoolean(KEY_SHOW_LUNAR_YEAR, isChecked).apply());
        infoVerticalSwitch.setOnCheckedChangeListener(
                (buttonView, isChecked) -> prefs.edit().putBoolean(KEY_INFO_VERTICAL, isChecked).apply());
        burnInProtectionSwitch.setOnCheckedChangeListener(
                (buttonView, isChecked) -> {
                    prefs.edit().putBoolean(KEY_BURN_IN_PROTECTION, isChecked)
                            .apply();
                    updateBurnInProtectionOffsetVisibility();
                });
        weatherMergeLandscapeSwitch.setOnCheckedChangeListener(
                (buttonView, isChecked) -> prefs.edit().putBoolean(KEY_WEATHER_MERGE_LANDSCAPE, isChecked).apply());
        dateOnTopPortraitSwitch.setOnCheckedChangeListener(
                (buttonView, isChecked) -> prefs.edit().putBoolean(KEY_DATE_ON_TOP_PORTRAIT, isChecked).apply());
        weatherLargeThreeBarsSwitch.setOnCheckedChangeListener(
                (buttonView, isChecked) -> prefs.edit().putBoolean(KEY_WEATHER_LARGE_THREE_BARS, isChecked).apply());

        setupWeatherSettings();
        setupFontSettings();

        overlayButton.setOnClickListener(v -> openOverlaySettings());
        batteryButton.setOnClickListener(v -> openBatteryOptimizationSettings());
        restartButton.setOnClickListener(v -> restartApp());

        updateDescription();
        updateOverlayButton();
    }

    @Override
    protected void onPause() {
        super.onPause();
        saveWeatherLocation();
    }

    private void saveWeatherLocation() {
        if (weatherLocationEdit != null) {
            String location = weatherLocationEdit.getText().toString().trim();
            // Use commit() (synchronous) to guarantee the write completes
            // before the process can be killed (e.g. by restartApp).
            prefs.edit().putString(KEY_WEATHER_LOCATION, location).commit();
        }
    }

    @Override
    protected void onResume() {
        super.onResume();
        updateOverlayButton();
        updateWeatherError();
    }

    private void showEnableConfirmationDialog() {
        new AlertDialog.Builder(this)
                .setTitle(R.string.auto_start_dialog_title)
                .setMessage(R.string.auto_start_dialog_message)
                .setPositiveButton(R.string.auto_start_dialog_agree, (dialog, which) -> {
                    saveAutoStart(true);
                    requestNeededPermissions();
                    updateDescription();
                })
                .setNegativeButton(R.string.auto_start_dialog_cancel, (dialog, which) -> {
                    autoStartSwitch.setChecked(false);
                    saveAutoStart(false);
                })
                .setCancelable(false)
                .show();
    }

    private void saveAutoStart(boolean enabled) {
        prefs.edit().putBoolean(KEY_AUTO_START, enabled).apply();
    }

    private void requestNeededPermissions() {
        // SYSTEM_ALERT_WINDOW is required to show the floating clock after boot.
        if (!Settings.canDrawOverlays(this)) {
            openOverlaySettings();
        }

        // Android 13+ requires POST_NOTIFICATIONS permission for foreground
        // service notifications.
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
            if (checkSelfPermission(android.Manifest.permission.POST_NOTIFICATIONS)
                    != PackageManager.PERMISSION_GRANTED) {
                requestPermissions(
                        new String[]{android.Manifest.permission.POST_NOTIFICATIONS},
                        REQUEST_NOTIFICATION_PERMISSION);
            }
        }

        // Ask the user to disable battery optimization for this app so that
        // the boot receiver is less likely to be killed by the system.
        requestDisableBatteryOptimization();
    }

    private void openOverlaySettings() {
        Intent intent = new Intent(Settings.ACTION_MANAGE_OVERLAY_PERMISSION,
                Uri.parse("package:" + getPackageName()));
        if (intent.resolveActivity(getPackageManager()) != null) {
            startActivity(intent);
        }
    }

    private void updateOverlayButton() {
        if (Settings.canDrawOverlays(this)) {
            overlayButton.setText(getString(R.string.overlay_permission_button)
                    + " (" + getString(R.string.overlay_permission_granted) + ")");
        } else {
            overlayButton.setText(getString(R.string.overlay_permission_button)
                    + " (" + getString(R.string.overlay_permission_not_granted) + ")");
        }
    }

    private void requestDisableBatteryOptimization() {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.M) {
            return;
        }
        PowerManager powerManager = (PowerManager) getSystemService(Context.POWER_SERVICE);
        if (powerManager == null) {
            return;
        }
        if (!powerManager.isIgnoringBatteryOptimizations(getPackageName())) {
            Intent intent = new Intent(Settings.ACTION_REQUEST_IGNORE_BATTERY_OPTIMIZATIONS);
            intent.setData(Uri.parse("package:" + getPackageName()));
            if (intent.resolveActivity(getPackageManager()) != null) {
                startActivityForResult(intent, REQUEST_BATTERY_OPTIMIZATION);
            }
        }
    }

    private void openBatteryOptimizationSettings() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
            Intent intent = new Intent(Settings.ACTION_IGNORE_BATTERY_OPTIMIZATION_SETTINGS);
            if (intent.resolveActivity(getPackageManager()) != null) {
                startActivity(intent);
            }
        }
    }

    private void updateDescription() {
        boolean enabled = prefs.getBoolean(KEY_AUTO_START, false);
        if (enabled) {
            descriptionText.setText(R.string.auto_start_enabled_description);
        } else {
            descriptionText.setText(R.string.auto_start_disabled_description);
        }
    }

    private void setupBurnInProtectionOffset() {
        float offset = prefs.getFloat(KEY_BURN_IN_PROTECTION_OFFSET,
                BURN_IN_OFFSET_DEFAULT);
        int progress = (int) (offset * 10);
        if (progress < 0)
            progress = 0;
        if (progress > BURN_IN_OFFSET_MAX_PROGRESS)
            progress = BURN_IN_OFFSET_MAX_PROGRESS;
        burnInProtectionOffsetSeekBar.setMax(BURN_IN_OFFSET_MAX_PROGRESS);
        burnInProtectionOffsetSeekBar.setProgress(progress);
        updateBurnInProtectionOffsetSummary(progress);
        updateBurnInProtectionOffsetVisibility();

        burnInProtectionOffsetSeekBar.setOnSeekBarChangeListener(
                new SeekBar.OnSeekBarChangeListener() {
                    @Override
                    public void onProgressChanged(SeekBar seekBar, int progress,
                                                    boolean fromUser) {
                        float value = progress / 10.0f;
                        prefs.edit().putFloat(KEY_BURN_IN_PROTECTION_OFFSET, value)
                                .apply();
                        updateBurnInProtectionOffsetSummary(progress);
                    }

                    @Override
                    public void onStartTrackingTouch(SeekBar seekBar) {
                    }

                    @Override
                    public void onStopTrackingTouch(SeekBar seekBar) {
                    }
                });
    }

    private void updateBurnInProtectionOffsetSummary(int progress) {
        String value = String.valueOf(progress / 10.0f);
        burnInProtectionOffsetSummary.setText(getString(
                R.string.burn_in_protection_offset_summary, value));
    }

    private void updateBurnInProtectionOffsetVisibility() {
        boolean enabled = burnInProtectionSwitch.isChecked();
        burnInProtectionOffsetContainer.setVisibility(
                enabled ? LinearLayout.VISIBLE : LinearLayout.GONE);
    }

    private void setupWeatherSettings() {
        boolean showWeather = prefs.getBoolean(KEY_SHOW_WEATHER, false);
        showWeatherSwitch.setChecked(showWeather);
        weatherLocationEdit.setText(prefs.getString(KEY_WEATHER_LOCATION, ""));

        int interval = prefs.getInt(KEY_WEATHER_UPDATE_INTERVAL,
                WEATHER_UPDATE_INTERVAL_DEFAULT);
        int intervalProgress = interval - 1;
        if (intervalProgress < 0)
            intervalProgress = 0;
        weatherUpdateIntervalSeekBar.setMax(23);
        weatherUpdateIntervalSeekBar.setProgress(intervalProgress);
        updateWeatherUpdateIntervalSummary(interval);

        updateWeatherSettingsVisibility();

        showWeatherSwitch.setOnCheckedChangeListener(
                (buttonView, isChecked) -> {
                    prefs.edit().putBoolean(KEY_SHOW_WEATHER, isChecked).apply();
                    updateWeatherSettingsVisibility();
                });

        weatherUpdateIntervalSeekBar.setOnSeekBarChangeListener(
                new SeekBar.OnSeekBarChangeListener() {
                    @Override
                    public void onProgressChanged(SeekBar seekBar, int progress,
                                                    boolean fromUser) {
                        int hours = progress + 1;
                        prefs.edit().putInt(KEY_WEATHER_UPDATE_INTERVAL, hours)
                                .apply();
                        updateWeatherUpdateIntervalSummary(hours);
                    }

                    @Override
                    public void onStartTrackingTouch(SeekBar seekBar) {
                    }

                    @Override
                    public void onStopTrackingTouch(SeekBar seekBar) {
                    }
                });

        weatherValidateButton.setOnClickListener(v -> validateLocation());
    }

    private void updateWeatherSettingsVisibility() {
        boolean enabled = showWeatherSwitch.isChecked();
        int visibility = enabled ? LinearLayout.VISIBLE : LinearLayout.GONE;
        weatherLocationContainer.setVisibility(visibility);
        weatherUpdateIntervalContainer.setVisibility(visibility);
        weatherFontButton.setVisibility(visibility);
        weatherValidateButton.setVisibility(visibility);
    }

    private void updateWeatherUpdateIntervalSummary(int hours) {
        weatherUpdateIntervalSummary.setText(getString(
                R.string.weather_update_interval_summary, hours));
    }

    /**
     * Check SharedPreferences for a stored weather error from MainActivity
     * and display it. Also show success status if weather data is available.
     */
    private void updateWeatherError() {
        String error = prefs.getString("weather_error", null);
        String location = prefs.getString(KEY_WEATHER_LOCATION, "").trim();
        if (error != null && !error.isEmpty()) {
            String displayMsg;
            if (error.contains("No locations found")) {
                displayMsg = getString(R.string.weather_error_not_found, location);
            } else if (error.contains("empty") || error.contains("No location")) {
                displayMsg = getString(R.string.weather_error_not_found, location);
            } else {
                displayMsg = getString(R.string.weather_error_network) +
                        "\n(" + error + ")";
            }
            weatherErrorText.setText(displayMsg);
            weatherErrorText.setVisibility(TextView.VISIBLE);
        } else {
            weatherErrorText.setVisibility(TextView.GONE);
        }
    }

    /**
     * Validate the current location input by performing a test geocode
     * on a background thread. Shows the result in weatherErrorText.
     */
    private void validateLocation() {
        final String location = weatherLocationEdit.getText().toString().trim();
        if (location.isEmpty()) {
            weatherErrorText.setText(getString(R.string.weather_error_not_found, ""));
            weatherErrorText.setVisibility(TextView.VISIBLE);
            return;
        }

        weatherValidateButton.setEnabled(false);
        weatherValidateButton.setText(R.string.weather_validating);
        weatherErrorText.setVisibility(TextView.GONE);

        new Thread(new Runnable() {
            @Override
            public void run() {
                final String[] resultMsg = new String[1];
                final boolean[] success = new boolean[1];
                try {
                    // Use a separate prefs name to avoid polluting the main cache.
                    WeatherManager testMgr = new WeatherManager(
                            getApplicationContext(), "flipclock_validate");
                    final CountDownLatch latch = new CountDownLatch(1);
                    final AtomicReference<String> errorMsg = new AtomicReference<>();
                    final AtomicReference<String> locationName = new AtomicReference<>();

                    testMgr.setWeatherListener(new WeatherListener() {
                        @Override
                        public void onWeatherUpdated(WeatherInfo info) {
                            locationName.set(info.getLocation().getName());
                            latch.countDown();
                        }
                        @Override
                        public void onError(Throwable error) {
                            errorMsg.set(error.getMessage());
                            latch.countDown();
                        }
                    });
                    testMgr.setLocation(location);
                    latch.await(15, TimeUnit.SECONDS);
                    testMgr.stop();

                    if (locationName.get() != null) {
                        success[0] = true;
                        resultMsg[0] = getString(R.string.weather_ok,
                                locationName.get());
                    } else {
                        success[0] = false;
                        String err = errorMsg.get();
                        if (err != null && err.contains("No locations found")) {
                            resultMsg[0] = getString(
                                    R.string.weather_error_not_found, location);
                        } else {
                            resultMsg[0] = getString(R.string.weather_error_network);
                        }
                    }
                } catch (Exception e) {
                    success[0] = false;
                    resultMsg[0] = getString(R.string.weather_error_network);
                }

                runOnUiThread(new Runnable() {
                    @Override
                    public void run() {
                        weatherValidateButton.setEnabled(true);
                        weatherValidateButton.setText(
                                R.string.weather_validate_location);
                        weatherErrorText.setText(resultMsg[0]);
                        weatherErrorText.setVisibility(TextView.VISIBLE);
                        if (success[0]) {
                            // Clear stored error on successful validation.
                            prefs.edit().remove("weather_error").apply();
                        }
                    }
                });
            }
        }).start();
    }

    private void setupFontSettings() {
        updateFontButton(infoBarFontButton,
                prefs.getString(KEY_INFO_BAR_FONT, DEFAULT_INFO_BAR_FONT),
                R.string.info_bar_font_label);
        updateFontButton(weatherFontButton,
                prefs.getString(KEY_WEATHER_FONT, DEFAULT_WEATHER_FONT),
                R.string.weather_font_label);

        infoBarFontButton.setOnClickListener(v ->
                showFontPickerDialog(KEY_INFO_BAR_FONT,
                        R.string.info_bar_font_label, infoBarFontButton));
        weatherFontButton.setOnClickListener(v ->
                showFontPickerDialog(KEY_WEATHER_FONT,
                        R.string.weather_font_label, weatherFontButton));
    }

    private void updateFontButton(Button button, String currentFont,
                                  int labelResId) {
        if (currentFont.isEmpty()) {
            button.setText(getString(labelResId) + "：" +
                    getString(R.string.font_default));
        } else {
            String displayName = getFontDisplayName(currentFont);
            button.setText(getString(labelResId) + "：" + displayName);
        }
    }

    private String getFontDisplayName(String fontFile) {
        for (int i = 0; i < FONT_FILES.length; i++) {
            if (FONT_FILES[i].equals(fontFile)) {
                return FONT_DISPLAY_NAMES[i];
            }
        }
        return fontFile;
    }

    private void showFontPickerDialog(final String prefKey, final int labelResId,
                                     final Button button) {
        String currentFont = prefs.getString(prefKey, "");
        int checkedItem = -1;
        for (int i = 0; i < FONT_FILES.length; i++) {
            if (FONT_FILES[i].equals(currentFont)) {
                checkedItem = i;
                break;
            }
        }

        new AlertDialog.Builder(this)
                .setTitle(getString(labelResId))
                .setSingleChoiceItems(FONT_DISPLAY_NAMES, checkedItem,
                        (dialog, which) -> {
                            String selectedFont = FONT_FILES[which];
                            prefs.edit().putString(prefKey, selectedFont)
                                    .apply();
                            updateFontButton(button, selectedFont,
                                    labelResId);
                            dialog.dismiss();
                        })
                .setNeutralButton(R.string.font_default, (dialog, which) -> {
                    String defaultFont = prefKey.equals(KEY_INFO_BAR_FONT)
                            ? DEFAULT_INFO_BAR_FONT : DEFAULT_WEATHER_FONT;
                    prefs.edit().putString(prefKey, defaultFont).apply();
                    updateFontButton(button, defaultFont, labelResId);
                })
                .setNegativeButton(R.string.auto_start_dialog_cancel, null)
                .show();
    }

    private void restartApp() {
        // Synchronously save pending weather location.
        saveWeatherLocation();
        // Re-generate flipclock.conf so the native layer picks up all
        // current settings on restart.
        MainActivity.writeNativeConf(this);
        Intent intent = getPackageManager().getLaunchIntentForPackage(
                getPackageName());
        if (intent != null) {
            intent.addFlags(Intent.FLAG_ACTIVITY_CLEAR_TOP
                    | Intent.FLAG_ACTIVITY_NEW_TASK);
            startActivity(intent);
        }
        finish();
        // Force the process to restart so the native SDL layer
        // re-reads flipclock.conf with the new settings.
        Runtime.getRuntime().exit(0);
    }
}
