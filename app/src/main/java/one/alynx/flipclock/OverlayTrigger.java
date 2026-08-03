package one.alynx.flipclock;

import android.content.Context;
import android.graphics.PixelFormat;
import android.os.Build;
import android.provider.Settings;
import android.view.Gravity;
import android.view.LayoutInflater;
import android.view.View;
import android.view.WindowManager;

/**
 * Creates a tiny 1x1 transparent overlay window.
 *
 * On some Android versions and OEMs, having a visible window from a foreground
 * service allows the app to start an Activity from the background. This
 * overlay is used as a trigger before launching the main FlipClock activity
 * after boot, then immediately removed.
 */
public class OverlayTrigger {
    private final Context mContext;
    private WindowManager mWindowManager;
    private View mTriggerView;

    public OverlayTrigger(Context context) {
        mContext = context.getApplicationContext();
    }

    /**
     * Show the tiny overlay window.
     *
     * @return true if the overlay was shown, false if permission is missing.
     */
    public boolean show() {
        if (!Settings.canDrawOverlays(mContext)) {
            return false;
        }
        mWindowManager = (WindowManager) mContext.getSystemService(Context.WINDOW_SERVICE);
        if (mWindowManager == null) {
            return false;
        }

        LayoutInflater inflater = LayoutInflater.from(mContext);
        mTriggerView = inflater.inflate(R.layout.overlay_trigger, null);

        int type = Build.VERSION.SDK_INT >= Build.VERSION_CODES.O
                ? WindowManager.LayoutParams.TYPE_APPLICATION_OVERLAY
                : WindowManager.LayoutParams.TYPE_PHONE;

        WindowManager.LayoutParams params = new WindowManager.LayoutParams(
                1,
                1,
                type,
                WindowManager.LayoutParams.FLAG_NOT_FOCUSABLE
                        | WindowManager.LayoutParams.FLAG_NOT_TOUCHABLE
                        | WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON,
                PixelFormat.TRANSLUCENT);
        params.gravity = Gravity.TOP | Gravity.START;
        params.x = 0;
        params.y = 0;

        try {
            mWindowManager.addView(mTriggerView, params);
            return true;
        } catch (Exception e) {
            return false;
        }
    }

    /**
     * Remove the tiny overlay window.
     */
    public void remove() {
        if (mTriggerView != null && mWindowManager != null) {
            try {
                mWindowManager.removeView(mTriggerView);
            } catch (IllegalArgumentException ignored) {
                // View may already be removed.
            }
        }
        mTriggerView = null;
        mWindowManager = null;
    }
}
