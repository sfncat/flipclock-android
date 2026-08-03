package one.alynx.flipclock;

import android.content.Intent;
import android.os.Build;
import android.os.Bundle;
import android.os.SystemClock;
import android.view.MotionEvent;
import android.view.WindowManager;

import org.libsdl.app.SDLActivity;

/**
 * A sample wrapper class that just calls SDLActivity
 */

public class MainActivity extends SDLActivity {
    private static final int SETTINGS_GESTURE_FINGERS = 4;
    private static final long SETTINGS_GESTURE_DEBOUNCE_MS = 1000;

    private long mLastSettingsOpenTime = 0;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        keepScreenOnAfterBoot();
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
