# FlipClock Android V2

Original project: https://github.com/AlynxZhou/flipclock-android

The Android wrapper for FlipClock.

[Google Play Store Page](https://play.google.com/store/apps/details?id=com.stackof.flipclockv2)

## Why this project?

Fliqlo is a closed-source flip clock app for macOS, and it also has an iOS version. I wanted to make a similar app for Android, so FlipClock Android was born.

This project enhances the original with more information display options, auto-start on boot, burn-in protection, weather display, custom fonts, and support for the latest Android versions.

## Features

- **Flip clock display**: Full-screen display of the current time, supports both landscape and portrait orientations.
- **24-hour format with seconds by default**: The app shows the 24-hour format with seconds after launching.
- **Date / weekday / lunar calendar display**: Enabled by default; each can be toggled independently in settings. In landscape they appear at the top of the main screen, in portrait on the left side, with a smaller font than the time.
- **Vertical text**: In portrait the info bar uses traditional vertical typography by default (upright CJK characters, digits / symbols rotated 90°); can be switched back to stacked horizontal lines in settings.
- **Burn-in protection**: Can be enabled in settings (on by default). When enabled, the info bar and time display slowly shift by a small amount to reduce OLED burn-in risk. The displacement amplitude is adjustable from 0% to 5% of the screen's shorter side (default 5%).
- **Weather display**: Can be enabled in settings. Shows current weather (location, temperature, conditions) on the main screen. Supports custom location input with location validation. When burn-in protection is enabled, the weather text also moves left and right with the burn-in offset. Weather refresh interval is adjustable from 1 to 24 hours (default 1 hour).
- **Layout tweaks (experimental)**: Three independent, off-by-default layout options to address the small weather font, each toggleable separately in settings: ① Merge weather into the date bar in landscape — switches to a two-bar layout that removes overlap, and enlarges the date when its text is short; ② Date on top in portrait — moves the "YYYY-MM-DD" date to the very top (info bar on the lower-left, time on the lower-right), with burn-in protection changed to left/right movement plus fade in/out to avoid the tiring 90° vertical rotation; ③ Keep three bars with non-shrinking weather — keeps the three-bar layout, makes the weather font match the info bar and move with the same burn-in offset, only needing to avoid overlapping the time. All three require a restart to take effect.
- **Custom fonts**: The info bar and weather display can each use a different font. 12 open-source CJK fonts are bundled (HarmonyOS Sans SC / 鸿蒙黑体, LXGW Marker Gothic, LXGW Neo ZhiSong, LXGW WenKai Mono GB Lite / 霞鹜文楷, LXGW XiHei MN / 霞鹜禧黑, LXGW ZhenKai GB / 霞鹜真楷, Smiley Sans, Source Han Serif, WenYuan Sans SC, Xiaolai, Yozai, ZCOOL KuaiLe). The info bar defaults to LXGW XiHei MN; weather defaults to LXGW Neo ZhiSong.
- **Auto-start on boot**: After the device reboots, automatically launch the FlipClockV2 main screen, turn on the screen, and keep it awake without manual operation.
- **User authorization management**: All sensitive permissions (auto-start, overlay, notification, battery optimization, etc.) require the user to manually enable and consent in settings.
- **Native settings screen**: Configure auto-start on boot, overlay permission, battery optimization, display content, burn-in protection, weather, fonts, etc.
- **Restart button**: A restart button at the bottom of the settings screen lets you apply changes immediately without manually exiting the app.
- **Four-finger gesture to open settings**: On the main screen, touch the screen with four fingers simultaneously to open settings.
- **Gesture shortcuts**:
  - Two-finger touch / double tap: toggle 12/24-hour format.
  - Three-finger touch: show/hide seconds.
  - Rotate your phone: switch between landscape and portrait (requires auto-rotate to be enabled).

## Building

### Requirements

- Android SDK
- Android NDK r28 (`28.2.13676358`)
- JDK 21
- Gradle 8.5
- Android Gradle Plugin 8.3.0

> Gradle and Android Gradle Plugin versions must match. The project includes a Gradle Wrapper (`gradle/wrapper/gradle-wrapper.properties`) pinned to Gradle 8.5, which is downloaded automatically when running `./gradlew` — no manual installation needed. The Android Gradle Plugin version is declared in the root `build.gradle` as `com.android.tools.build:gradle:8.3.0`.

### Build steps

1. Clone the repo

   The SDL2, SDL2_ttf and FlipClock native sources are committed directly under `app/jni/`, so no submodule initialization is needed:

   ```bash
   git clone <repo-url>
   cd flipclock-android
   ```

2. Build with Java 21

   ```bash
   export JAVA_HOME=/usr/lib/jvm/java-21-openjdk-amd64
   export PATH=$JAVA_HOME/bin:$PATH
   ./gradlew assembleDebug
   ```

   On success, the APK is at:

   ```
   app/build/outputs/apk/debug/app-debug.apk
   ```

3. Android Studio (optional)

   You can also open the project with Android Studio, wait for the Gradle sync to finish, then click **Build → Make Project** or **Run** to compile and install.

### Key configuration

- `app/build.gradle`: `minSdkVersion` 21, `targetSdkVersion` 33, `compileSdk` 33, `ndkVersion "28.2.13676358"`.
- `app/jni/Application.mk`: `APP_PLATFORM=android-21`, ABIs include `armeabi-v7a`, `arm64-v8a`, `x86`, `x86_64`.
- The project uses NDK r28 to support 16 KB page size alignment on Android 15+ devices.

## Usage

### First launch

1. Install and open the app. It shows the **24-hour clock with seconds** by default.
2. After opening the app, touch the screen with **four fingers simultaneously** to open settings.
3. Enable **Auto-start on boot** in settings, and grant the following permissions as prompted:
   - **Overlay permission**: Used to create a temporary visible window on Android 10+, allowing the main screen to be started from the background.
   - **Notification permission** (Android 13+): For the foreground service notification.
   - **Battery optimization**: It is recommended to disable battery optimization, otherwise the system may kill the boot auto-start service.
4. (Optional) In the **Main screen display content** section, individually enable / disable **Date / Weekday / Lunar calendar** (all enabled by default). Changes take effect after restarting FlipClock. In landscape these items appear at the top of the main screen, in portrait on the left side, using traditional vertical text by default (turn off **Vertical text** to fall back to stacked horizontal lines).
5. (Optional) In the **Weather** section, enable **Show weather**, enter a location name (e.g. "Beijing"), and tap **Validate location** to confirm it is valid. Adjust the refresh interval (default 1 hour). When burn-in protection is enabled, the weather text moves left and right slowly.
6. (Optional) In **Info bar font** and **Weather font**, choose different fonts for the info bar and weather display.
7. After authorization is complete, reboot the device and FlipClockV2 will automatically enter the main screen, turn on the screen, and keep it awake.
8. After changing settings, tap the **Restart app** button at the bottom of the settings screen to apply changes immediately without manually exiting the app.

### Gestures

| Gesture | Action |
|---------|--------|
| Two-finger touch / double tap | Toggle 12/24-hour format |
| Three-finger touch | Show/hide seconds |
| Four-finger touch simultaneously | Open settings |
| Rotate the phone | Switch landscape/portrait |

### Opening settings

The settings screen can be opened in two ways:

1. Find the **FlipClockV2 Settings** icon on the launcher / app list.
2. On the FlipClock main screen, touch the screen with **four fingers simultaneously**.

### Settings

| Setting | Description |
|---------|-------------|
| **Auto-start on boot** | Launch FlipClock automatically after the device reboots. A confirmation dialog will appear when enabling, and required permissions will be requested. |
| **Overlay permission** | Jump to system settings to grant overlay permission, one of the permissions required for auto-start on boot. |
| **Battery optimization settings** | Jump to system settings to disable battery optimization for FlipClock, improving the success rate of auto-start on boot. |
| **Show date** | Show the current date in the info bar on the main screen. |
| **Show weekday** | Show the current weekday in the info bar on the main screen. |
| **Show lunar** | Show the lunar calendar in the info bar on the main screen. |
| **Show lunar year** | Also show the heavenly-stems-and-earthly-branches year (e.g. 丙午年) in the lunar calendar info. |
| **Vertical text** | In portrait, the info bar uses traditional vertical typography; turn off to switch to stacked horizontal lines. |
| **Burn-in protection** | Slowly shift the info bar and time display by a small amount to reduce OLED burn-in risk. On by default; changes take effect after restarting FlipClock. |
| **Burn-in protection offset** | Adjust the displacement amplitude from 0% to 5% of the screen's shorter side (default 5%). Only visible when **Burn-in protection** is enabled. |
| **Merge weather into date bar (landscape)** | In landscape, merge the weather into the top date bar, switching to a two-bar layout that removes overlap; the date font auto-enlarges when its text is short. Off by default. |
| **Date on top (portrait)** | In portrait, move the "YYYY-MM-DD" date to the very top (info bar on the lower-left, time on the lower-right); burn-in protection becomes left/right movement plus fade in/out to avoid the tiring 90° vertical rotation. Off by default. |
| **Three-bar non-shrinking weather** | Keep the three-bar layout; the weather font matches the info bar and moves with the same burn-in offset, only needing to avoid overlapping the time. Off by default. |
| **Show weather** | Show current weather information (location, temperature, conditions) on the main screen. |
| **Location** | Enter a city name for weather queries (e.g. "Beijing"). Tap **Validate location** to confirm it is valid. |
| **Refresh interval** | Weather refresh interval, 1–24 hours, default 1 hour. |
| **Info bar font** | Choose a custom font for the info bar. Defaults to LXGW XiHei MN. |
| **Weather font** | Choose a custom font for the weather display. Defaults to LXGW Neo ZhiSong. |
| **Restart app** | One-tap restart to apply all setting changes immediately. |

> **Note:** Options under **Main screen display content** only take effect after restarting FlipClock. You can also tap the **Restart app** button at the bottom of the settings screen.

## Permissions

| Permission | Purpose |
|------------|---------|
| `RECEIVE_BOOT_COMPLETED` | Receive the system boot broadcast to implement auto-start on boot. |
| `SYSTEM_ALERT_WINDOW` | Create a temporary 1×1 transparent overlay to work around Android 10+'s background activity start restriction. |
| `WAKE_LOCK` | Wake the device and turn the screen on after boot. |
| `FOREGROUND_SERVICE` | Run a foreground service to perform the boot auto-start flow. |
| `POST_NOTIFICATIONS` | Show the foreground service notification on Android 13+. |
| `REQUEST_IGNORE_BATTERY_OPTIMIZATIONS` | Guide the user to disable battery optimization for a higher auto-start success rate. |

## Known limitations

- Android 10+ strictly restricts starting activities from the background; this project works around it with a foreground service plus a temporary overlay. Behavior may vary across vendors/ROMs.
- Some Chinese ROMs (Xiaomi, Huawei, OPPO, vivo, etc.) may require additionally allowing auto-start in system settings, otherwise it may not work after reboot. In the app launch management, disable automatic management and manually allow auto-start, associated start, and background activity.
- If the user force-stops the app, the system suppresses the `BOOT_COMPLETED` broadcast until the app is manually opened again.

## Technical notes

- The Java shim is taken directly from SDL2's `android-project` directory, unmodified to stay consistent with upstream.
- The SDL2, SDL2_ttf and FlipClock sources are committed directly under `app/jni/` (previously git submodules, now local files).
- The original FlipClock uses Meson, which cannot be used when building an Android app, so `Android.mk` is used for the native build.
- The date, weekday and lunar calendar are rendered by native C code (`info_bar.c` / `lunar.c`); the lunar calendar uses an offline algorithm (1900–2100) and needs no network.
- The app bundles 12 open-source CJK fonts (in `assets/fonts/`). The dropdown list is generated at runtime from the actual files in that directory, so adding a font only requires dropping the file in and adding its display name to `SettingsActivity.FONT_NAME_MAP`; the info bar and weather display can each use a different font, all licensed under the SIL Open Font License 1.1.
- Version numbers are managed centrally in `gradle.properties` (`APP_VERSION_CODE` / `APP_VERSION_NAME`), read by `app/build.gradle`.
- The application package name is `com.stackof.flipclockv2`; the weather module package name is `com.stackof.flipclockv2.weather`.

## LICENSE

- Code taken directly from SDL2 and SDL2_ttf keeps their original licenses.
- Code modified/added by myself is licensed under [Apache-2.0](LICENSE).
