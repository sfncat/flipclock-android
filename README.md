FlipClock Android
=================

The Android wrapper for FlipClock.

[Google Play Store Page](https://play.google.com/store/apps/details?id=one.alynx.flipclock)

# WHY?

Fliqlo, the closed-source app for macOS has an iOS version, so I tried to make an Android version.

# Features

- **Flip clock display**: Full-screen clock, supports both landscape and portrait orientations.
- **24-hour format with seconds by default**: Shows the 24-hour clock with seconds on startup.
- **Date / weekday / lunar calendar display**: Shown by default; each can be toggled independently in settings. In landscape they appear at the top of the main screen, in portrait on the left side, with a smaller font than the time.
- **Vertical text (portrait)**: In portrait the info bar uses traditional vertical typography by default (upright CJK characters, 90°-rotated digits); it can be switched back to stacked horizontal lines in settings.
- **Auto-start on boot**: Automatically launches FlipClock after a reboot, turning the screen on and keeping it awake, with no manual action needed.
- **User authorization management**: All sensitive permissions (auto-start, overlay, notification, battery optimization, etc.) require the user to manually enable and consent in settings.
- **Native settings screen**: Configure auto-start on boot, overlay permission, battery optimization, etc.
- **Four-finger gesture to open settings**: Touch the screen with four fingers simultaneously on the main screen to open settings.
- **Gesture shortcuts**:
  - Two-finger touch / double tap: toggle 12/24-hour clock.
  - Three-finger touch: show/hide seconds.
  - Rotate your phone: switch between landscape and portrait (requires auto-rotate to be enabled).

# Use cases

Find an old phone with an **LCD screen** and use it as a dedicated clock. Modern phones with non-square screens use OLED, and displaying fixed patterns (like a flip clock) for a long time may cause burn-in.

# Building

## Requirements

- Android SDK
- Android NDK (tested with NDK 25)
- JDK 17 (Gradle 8.0 has compatibility issues with the default Java 21, so Java 17 is recommended)

## Build steps

1. Clone the repo

   The SDL2, SDL2_ttf and FlipClock native sources are committed directly under `app/jni/`, so no submodule initialization is needed:

   ```bash
   git clone <repo-url>
   cd flipclock-android
   ```

2. Check the font symlink

   `app/src/main/assets/flipclock.ttf` is a symlink to `app/jni/flipclock/dists/flipclock.ttf` and is committed along with the repo. If it is missing, create it manually:

   ```bash
   ln -s app/jni/flipclock/dists/flipclock.ttf app/src/main/assets/flipclock.ttf
   ```

3. Build with Java 17

   ```bash
   export JAVA_HOME=/usr/lib/jvm/java-17-openjdk-amd64
   export PATH=$JAVA_HOME/bin:$PATH
   ./gradlew assembleDebug
   ```

   On success, the APK is at:

   ```
   app/build/outputs/apk/debug/app-debug.apk
   ```

4. Android Studio (optional)

   You can also open the project with Android Studio, wait for the Gradle sync to finish, then click **Build → Make Project** or **Run** to compile and install.

## Key configuration

- `app/build.gradle`: `minSdkVersion` 19, `targetSdkVersion` 33, `compileSdk` 33.
- `app/jni/Application.mk`: `APP_PLATFORM=android-19`, ABIs include `armeabi-v7a`, `arm64-v8a`, `x86`, `x86_64`.
- Since NDK 25 no longer supports `android-16`, `APP_PLATFORM` and `minSdkVersion` have been adjusted to 19.

# Usage

## First launch

1. Install and open the app. It shows the **24-hour clock with seconds** by default.
2. Touch the screen with **four fingers simultaneously** to open settings.
3. Enable **Auto-start on boot** in settings and grant the requested permissions:
   - **Overlay permission**: used to create a temporary visible window on Android 10+, which allows the main screen to be started from the background.
   - **Notification permission** (Android 13+): for the foreground service notification.
   - **Battery optimization**: it is recommended to disable battery optimization, otherwise the system may kill the boot auto-start service.
4. (Optional) **Date / Weekday / Lunar calendar** are shown by default and can be toggled in the **Main screen display content** section. Changes take effect after restarting FlipClock. In landscape they appear at the top of the main screen, in portrait on the left side, using traditional vertical text by default (turn off **Vertical text** in settings to fall back to stacked horizontal lines).
5. After authorization, reboot the device and FlipClock will launch automatically, turn the screen on and keep it awake.

## Gestures

| Gesture | Action |
|---------|--------|
| Two-finger touch / double tap | Toggle 12/24-hour clock |
| Three-finger touch | Show/hide seconds |
| Four-finger touch | Open settings |
| Rotate the phone | Switch landscape/portrait |

## Opening settings

The settings screen can be opened in two ways:

1. Find the **FlipClock Settings** icon on the launcher / app list.
2. Touch the screen with **four fingers simultaneously** on the main screen.

# Permissions

| Permission | Purpose |
|------------|---------|
| `RECEIVE_BOOT_COMPLETED` | Receive the system boot broadcast to implement auto-start on boot. |
| `SYSTEM_ALERT_WINDOW` | Create a temporary 1×1 transparent overlay to work around Android 10+'s background activity start restriction. |
| `WAKE_LOCK` | Wake the device and turn the screen on after boot. |
| `FOREGROUND_SERVICE` | Run a foreground service to perform the boot auto-start flow. |
| `POST_NOTIFICATIONS` | Show the foreground service notification on Android 13+. |
| `REQUEST_IGNORE_BATTERY_OPTIMIZATIONS` | Guide the user to disable battery optimization for a higher auto-start success rate. |

# Known limitations

- Android 10+ strictly restricts starting activities from the background; this project works around it with a foreground service plus a temporary overlay. Behavior may vary across vendors/ROMs.
- Some Chinese ROMs (Xiaomi, Huawei, OPPO, vivo, etc.) may require additionally allowing auto-start in system settings, otherwise it may not work after reboot.
- If the user force-stops the app, the system suppresses the `BOOT_COMPLETED` broadcast until the app is manually opened again.
- Displaying fixed patterns on an OLED screen for a long time may cause burn-in; an LCD device is recommended as a dedicated clock.

# Technical notes

- The Java shim is taken directly from SDL2's `android-project` directory, unmodified to stay consistent with upstream.
- The SDL2, SDL2_ttf and FlipClock sources are committed directly under `app/jni/` (previously git submodules, now local files).
- The original FlipClock uses Meson, which cannot be used when building an Android app, so `Android.mk` is used for the native build.
- The date, weekday and lunar calendar are rendered by native C code (`info_bar.c` / `lunar.c`); the lunar calendar uses an offline algorithm (1900–2100) and needs no network. The CJK font is a subset of Noto Sans CJK SC (`assets/flipclock_cjk.ttf`), licensed under the SIL Open Font License.

# LICENSE

- Code taken directly from SDL2 and SDL2_ttf keeps their original licenses.
- Code modified/added by myself is licensed under [Apache-2.0](LICENSE).
