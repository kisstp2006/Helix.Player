package org.helix.player;

import android.app.Activity;
import android.content.pm.ActivityInfo;
import android.os.Bundle;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.view.WindowManager;

/**
 * The Android player's activity: a full screen surface for the native player (libHelixPlayer.so), which runs on a
 * thread of its own. The command line comes from the intent's "args" extra (am start -e args "-frames 100").
 */
public class HelixActivity extends Activity implements SurfaceHolder.Callback {
    static {
        System.loadLibrary("HelixPlayer");
    }

    private SurfaceView view;
    private boolean finishing;

    @Override
    protected void onCreate(Bundle state) {
        super.onCreate(state);
        view = new SurfaceView(this);
        view.getHolder().addCallback(this);
        view.setFocusable(true);
        view.setFocusableInTouchMode(true);
        setContentView(view);
        view.requestFocus();
        String args = getIntent() != null ? getIntent().getStringExtra("args") : null;
        nativeCreate(getAssets(), args != null ? args : "", getResources().getDisplayMetrics().density);
    }

    @Override
    protected void onDestroy() {
        nativeDestroy();
        super.onDestroy();
    }

    @Override
    public void onWindowFocusChanged(boolean focused) {
        super.onWindowFocusChanged(focused);
        nativeFocus(focused);
    }

    @Override
    public void surfaceCreated(SurfaceHolder holder) {
        nativeSurface(holder.getSurface());
    }

    @Override
    public void surfaceChanged(SurfaceHolder holder, int format, int width, int height) {
        nativeSurface(holder.getSurface());
    }

    @Override
    public void surfaceDestroyed(SurfaceHolder holder) {
        nativeSurface(null);
    }

    @Override
    public boolean onTouchEvent(MotionEvent e) {
        int action = e.getActionMasked();
        if (action == MotionEvent.ACTION_MOVE) {
            for (int i = 0; i < e.getPointerCount(); i++) {
                nativeTouch(action, e.getPointerId(i), e.getX(i), e.getY(i));
            }
        } else {
            int i = e.getActionIndex();
            nativeTouch(action, e.getPointerId(i), e.getX(i), e.getY(i));
        }
        return true;
    }

    @Override
    public boolean onKeyDown(int keyCode, KeyEvent e) {
        // The volume keys stay with the system.
        if (keyCode == KeyEvent.KEYCODE_VOLUME_UP || keyCode == KeyEvent.KEYCODE_VOLUME_DOWN) {
            return super.onKeyDown(keyCode, e);
        }
        nativeKey(true, keyCode);
        return true;
    }

    @Override
    public boolean onKeyUp(int keyCode, KeyEvent e) {
        // The volume keys stay with the system.
        if (keyCode == KeyEvent.KEYCODE_VOLUME_UP || keyCode == KeyEvent.KEYCODE_VOLUME_DOWN) {
            return super.onKeyUp(keyCode, e);
        }
        nativeKey(false, keyCode);
        return true;
    }

    // Called from the player's thread.

    public void setKeepScreenOn(final boolean keepOn) {
        runOnUiThread(() -> {
            if (keepOn) {
                getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
            } else {
                getWindow().clearFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
            }
        });
    }

    /** A ScreenOrientation value: Portrait 1, PortraitUpsideDown 2, LandscapeLeft 3, LandscapeRight 4, AutoRotation 5. */
    public void setOrientation(final int orientation) {
        final int requested;
        switch (orientation) {
            case 1: requested = ActivityInfo.SCREEN_ORIENTATION_PORTRAIT; break;
            case 2: requested = ActivityInfo.SCREEN_ORIENTATION_REVERSE_PORTRAIT; break;
            case 3: requested = ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE; break;
            case 4: requested = ActivityInfo.SCREEN_ORIENTATION_REVERSE_LANDSCAPE; break;
            default: requested = ActivityInfo.SCREEN_ORIENTATION_FULL_SENSOR; break;
        }
        runOnUiThread(() -> setRequestedOrientation(requested));
    }

    public void finishFromPlayer() {
        runOnUiThread(() -> {
            if (!finishing) {
                finishing = true;
                finish();
            }
        });
    }

    private native void nativeCreate(Object assets, String args, float density);
    private native void nativeSurface(Object surface);
    private native void nativeFocus(boolean focused);
    private native void nativeTouch(int action, int id, float x, float y);
    private native void nativeKey(boolean down, int keyCode);
    private native void nativeDestroy();
}
