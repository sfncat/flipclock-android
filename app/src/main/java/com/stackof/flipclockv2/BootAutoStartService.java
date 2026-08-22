package com.stackof.flipclockv2;

import android.app.Notification;
import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.app.PendingIntent;
import android.app.Service;
import android.content.Intent;
import android.os.Build;
import android.os.Handler;
import android.os.IBinder;
import android.os.Looper;
import android.os.PowerManager;
import android.provider.Settings;

/**
 * Foreground service started after boot when auto start is enabled.
 *
 * This service tries to bring the main FlipClockV2 activity to the foreground.
 * On Android 10+ starting an activity directly from the background is
 * restricted, so we first create a tiny visible overlay window (requires
 * SYSTEM_ALERT_WINDOW permission) and then launch MainActivity. If the overlay
 * permission is not granted, a notification is shown that opens Settings so the
 * user can enable it.
 */
public class BootAutoStartService extends Service {
    private static final String CHANNEL_ID = "flipclock_auto_start";
    private static final int NOTIFICATION_ID = 1;
    private static final long WAKE_LOCK_TIMEOUT_MS = 10 * 1000;

    private OverlayTrigger mOverlayTrigger;
    private PowerManager.WakeLock mWakeLock;
    private final Handler mHandler = new Handler(Looper.getMainLooper());

    @Override
    public void onCreate() {
        super.onCreate();
        createNotificationChannel();
    }

    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {
        if (Settings.canDrawOverlays(this)) {
            // Permission granted: create a tiny visible overlay window, then
            // launch the main activity. The visible window may allow the
            // activity start to succeed on devices that restrict background
            // activity launches.
            mOverlayTrigger = new OverlayTrigger(this);
            if (mOverlayTrigger.show()) {
                wakeScreen();
                launchMainActivity();
                // Remove the tiny overlay after a short delay.
                mHandler.postDelayed(() -> {
                    if (mOverlayTrigger != null) {
                        mOverlayTrigger.remove();
                    }
                    stopSelf();
                }, 1000);
                // We must call startForeground as a foreground service.
                startForeground(NOTIFICATION_ID, buildRunningNotification());
            } else {
                startForeground(NOTIFICATION_ID, buildSettingsNotification());
            }
        } else {
            // Permission not granted: keep the service running with a
            // notification that opens the settings page.
            startForeground(NOTIFICATION_ID, buildSettingsNotification());
        }

        return START_NOT_STICKY;
    }

    @Override
    public void onDestroy() {
        if (mOverlayTrigger != null) {
            mOverlayTrigger.remove();
            mOverlayTrigger = null;
        }
        releaseWakeLock();
        super.onDestroy();
    }

    @Override
    public IBinder onBind(Intent intent) {
        return null;
    }

    private void launchMainActivity() {
        Intent activityIntent = new Intent(this, MainActivity.class);
        activityIntent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK
                | Intent.FLAG_ACTIVITY_CLEAR_TOP
                | Intent.FLAG_ACTIVITY_SINGLE_TOP
                | Intent.FLAG_ACTIVITY_EXCLUDE_FROM_RECENTS
                | Intent.FLAG_ACTIVITY_BROUGHT_TO_FRONT);
        try {
            startActivity(activityIntent);
        } catch (Exception ignored) {
            // Background activity start may be blocked on some devices.
        }
    }

    private void wakeScreen() {
        PowerManager powerManager = (PowerManager) getSystemService(POWER_SERVICE);
        if (powerManager == null) {
            return;
        }
        mWakeLock = powerManager.newWakeLock(
                PowerManager.SCREEN_BRIGHT_WAKE_LOCK
                        | PowerManager.ACQUIRE_CAUSES_WAKEUP
                        | PowerManager.ON_AFTER_RELEASE,
                "FlipClockV2:BootWakeLock");
        mWakeLock.acquire(WAKE_LOCK_TIMEOUT_MS);
    }

    private void releaseWakeLock() {
        if (mWakeLock != null && mWakeLock.isHeld()) {
            mWakeLock.release();
        }
        mWakeLock = null;
    }

    private void createNotificationChannel() {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.O) {
            return;
        }
        NotificationManager manager = getSystemService(NotificationManager.class);
        if (manager == null) {
            return;
        }
        NotificationChannel channel = new NotificationChannel(
                CHANNEL_ID,
                getString(R.string.auto_start_channel_name),
                NotificationManager.IMPORTANCE_LOW);
        channel.setDescription(getString(R.string.auto_start_channel_description));
        manager.createNotificationChannel(channel);
    }

    private Notification buildRunningNotification() {
        Intent activityIntent = new Intent(this, MainActivity.class);
        activityIntent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK
                | Intent.FLAG_ACTIVITY_CLEAR_TOP
                | Intent.FLAG_ACTIVITY_SINGLE_TOP);

        PendingIntent pendingIntent = createPendingIntent(activityIntent);

        Notification.Builder builder = createNotificationBuilder();
        return builder
                .setContentTitle(getString(R.string.auto_start_notification_title))
                .setContentText(getString(R.string.auto_start_notification_running_text))
                .setSmallIcon(R.mipmap.ic_launcher)
                .setContentIntent(pendingIntent)
                .setOngoing(true)
                .build();
    }

    private Notification buildSettingsNotification() {
        Intent settingsIntent = new Intent(this, SettingsActivity.class);
        settingsIntent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK
                | Intent.FLAG_ACTIVITY_CLEAR_TOP
                | Intent.FLAG_ACTIVITY_SINGLE_TOP);

        PendingIntent pendingIntent = createPendingIntent(settingsIntent);

        Notification.Builder builder = createNotificationBuilder();
        return builder
                .setContentTitle(getString(R.string.auto_start_notification_title))
                .setContentText(getString(R.string.auto_start_notification_permission_text))
                .setSmallIcon(R.mipmap.ic_launcher)
                .setContentIntent(pendingIntent)
                .setAutoCancel(true)
                .build();
    }

    private Notification.Builder createNotificationBuilder() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            return new Notification.Builder(this, CHANNEL_ID);
        } else {
            return new Notification.Builder(this);
        }
    }

    private PendingIntent createPendingIntent(Intent intent) {
        int flags = PendingIntent.FLAG_UPDATE_CURRENT;
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
            flags |= PendingIntent.FLAG_IMMUTABLE;
        }
        return PendingIntent.getActivity(this, 0, intent, flags);
    }
}
