package one.alynx.flipclock;

import android.app.Activity;
import android.app.AlertDialog;
import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.content.pm.PackageManager;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.os.PowerManager;
import android.provider.Settings;
import android.widget.Button;
import android.widget.CompoundButton;
import android.widget.LinearLayout;
import android.widget.SeekBar;
import android.widget.Switch;
import android.widget.TextView;

/**
 * Settings activity for FlipClock.
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
    private static final float BURN_IN_OFFSET_DEFAULT = 1.5f;
    private static final int BURN_IN_OFFSET_MAX_PROGRESS = 50;
    private static final int REQUEST_NOTIFICATION_PERMISSION = 100;
    private static final int REQUEST_BATTERY_OPTIMIZATION = 101;

    private Switch autoStartSwitch;
    private Switch showDateSwitch;
    private Switch showWeekdaySwitch;
    private Switch showLunarSwitch;
    private Switch showLunarYearSwitch;
    private Switch infoVerticalSwitch;
    private Switch burnInProtectionSwitch;
    private LinearLayout burnInProtectionOffsetContainer;
    private TextView burnInProtectionOffsetSummary;
    private SeekBar burnInProtectionOffsetSeekBar;
    private Button overlayButton;
    private TextView descriptionText;
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
        burnInProtectionOffsetContainer =
                findViewById(R.id.burn_in_protection_offset_container);
        burnInProtectionOffsetSummary =
                findViewById(R.id.burn_in_protection_offset_summary);
        burnInProtectionOffsetSeekBar =
                findViewById(R.id.burn_in_protection_offset_seekbar);

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
                prefs.getBoolean(KEY_BURN_IN_PROTECTION, false));
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

        overlayButton.setOnClickListener(v -> openOverlaySettings());
        batteryButton.setOnClickListener(v -> openBatteryOptimizationSettings());

        updateDescription();
        updateOverlayButton();
    }

    @Override
    protected void onResume() {
        super.onResume();
        updateOverlayButton();
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
}
