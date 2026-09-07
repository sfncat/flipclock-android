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
- **Date / weekday / lunar calendar display**: Enabled by default; each can be toggled independently in settings. In landscape they appear at the top of the main screen, in portrait on the left side, with a smaller font than the time. Lunar year shows the sexagenary cycle plus zodiac (e.g. 丙午马年).
- **Solar terms / dog days / nine-nine display**: On solar term days, the term name is shown (e.g. 白露, 大暑); during dog days, 初伏/中伏/末伏 day N is shown; during nine-nine, 一九–九九 day N is shown. When a solar term coincides with dog days or nine-nine, both are displayed (e.g. 大暑 初伏第九天). Uses an offline 1900–2100 solar term table, no network needed.
- **Two-line / two-column info bar**: When the solar term feature is on, landscape automatically switches to two lines (line 1: date + weekday + weather; line 2: lunar + solar term), portrait to two columns (left: lunar + solar term; right: weekday + weather). Each region has independent font-size auto-fit to keep text as large as possible. Falls back to single line/column when there is no solar term content.
- **Vertical text**: In portrait the info bar uses traditional vertical typography by default (upright CJK characters, digits / symbols rotated 90°); can be switched back to stacked horizontal lines in settings.
- **Burn-in protection**: Can be enabled in settings (on by default). When enabled, the info bar and time display slowly shift by a small amount to reduce OLED burn-in risk. The displacement amplitude is adjustable from 0% to 5% of the screen's shorter side (default 5%).
- **Weather display**: Can be enabled in settings. Shows current weather (location, temperature, conditions) on the main screen. Supports custom location input with location validation. In landscape, weather is always merged into the info bar. Weather refresh interval is adjustable from 1 to 24 hours (default 1 hour).
- **Date on top in portrait (experimental)**: Moves the "YYYY-MM-DD" date to the very top (info bar on the lower-left, time on the lower-right), with burn-in protection changed to left/right movement plus fade in/out to avoid the tiring 90° vertical rotation. Off by default; requires a restart to take effect.
- **Custom fonts**: The info bar and the second row (landscape line 2 / portrait right column) can each use a different font. 10 open-source CJK fonts are bundled (源雲明體, 未来荧黑, 鸿蒙黑体, 霞鹜漫黑, 得意黑, 獅尾半月, 小赖字体, 悠哉字体, 站酷快乐体, Z工坊像素黑体). The info bar defaults to 未来荧黑 (Glow Sans SC); the second row defaults to 霞鹜漫黑 (LXGW Marker Gothic).
- **Auto-start on boot**: After the device reboots, automatically launch the FlipClockV2 main screen, turn on the screen, and keep it awake without manual operation.
- **User authorization management**: All sensitive permissions (auto-start, overlay, notification, battery optimization, etc.) require the user to manually enable and consent in settings.
- **Native settings screen**: Configure auto-start on boot, overlay permission, battery optimization, display content, burn-in protection, weather, fonts, etc.
- **Restart button**: A restart button at the bottom of the settings screen lets you apply changes immediately without manually exiting the app.
- **Four-finger gesture to open settings**: On the main screen, touch the screen with four fingers simultaneously to open settings.
- **Gesture shortcuts**:
  - Two-finger touch / double-tap: toggle 12/24-hour format.
  - Three-finger touch: show/hide seconds.
  - Rotate phone: switch landscape/portrait (requires auto-rotate in system settings).

## Build Instructions

### Requirements

- Android SDK
- Android NDK r28 (`28.2.13676358`)
- JDK 21
- Gradle 8.5
- Android Gradle Plugin 8.3.0

> Gradle and Android Gradle Plugin versions must match. The project ships a Gradle Wrapper (`gradle/wrapper/gradle-wrapper.properties`) pinned to Gradle 8.5; running `./gradlew` downloads it automatically. The Android Gradle Plugin version is declared as `com.android.tools.build:gradle:8.3.0` in the root `build.gradle`.

### Full Build Steps

1. Clone the repository

   SDL2, SDL2_ttf, and FlipClock native sources are committed directly in the repo (under `app/jni/`), no submodule initialization needed:

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

   You can also open the project in Android Studio, wait for Gradle sync, then click **Build → Make Project** or **Run**.

### Key Configuration

- `app/build.gradle`: `minSdkVersion` 21, `targetSdkVersion` 33, `compileSdk` 33, `ndkVersion "28.2.13676358"`.
- `app/jni/Application.mk`: `APP_PLATFORM=android-21`, ABIs include `armeabi-v7a`, `arm64-v8a`, `x86`, `x86_64`.
- The project uses NDK r28 to support 16 KB page alignment on Android 15+ devices.

## Usage

### First Use

1. Install and open the app; it defaults to **24-hour format with seconds**.
2. On the main screen, **touch with four fingers simultaneously** to open settings.
3. Enable **Auto-start on boot** and grant the following permissions as prompted:
   - **Overlay permission**: needed to create a temporary visible window on Android 10+ to allow background Activity launch.
   - **Notification permission** (Android 13+): for the foreground service notification.
   - **Battery optimization**: recommended to disable battery optimization for the app, otherwise the system may kill the auto-start service.
4. (Optional) In **Main screen display content**, toggle **Date / Weekday / Lunar / Lunar year** independently (all on by default). Changes require a restart to take effect. In landscape these appear at the top; in portrait on the left, using traditional vertical typography by default (turn off "Vertical text" to switch to stacked horizontal lines).
5. (Optional) In the **Weather** section, enable **Show weather**, enter a location name (e.g. "Beijing"), and tap **Verify location** to confirm it is valid. Adjust the refresh interval (default 1 hour).
6. (Optional) In **Info bar font** and **Second row font**, choose fonts separately. The info bar font is used for landscape line 1 and portrait left column; the second row font is used for landscape line 2 (lunar + solar term) and portrait right column (weekday + weather).
7. After granting permissions, reboot the device to auto-enter the FlipClockV2 main screen with the screen on and awake.
8. After changing settings, tap **Restart app to apply** at the bottom of the settings screen.

### Gesture Shortcuts

| Gesture | Action |
|---------|--------|
| Two-finger touch / double-tap | Toggle 12/24-hour format |
| Three-finger touch | Show/hide seconds |
| Four-finger simultaneous touch | Open settings |
| Rotate phone | Switch landscape/portrait |

### Settings Entry

Settings can be opened in two ways:

1. Find the **FlipClockV2 Settings** icon in the system launcher/app list.
2. On the FlipClock main screen, **touch with four fingers simultaneously**.

### Settings Reference

| Setting | Description |
|---------|-------------|
| **Auto-start on boot** | Automatically launch FlipClock after device reboot. Shows a confirmation dialog and guides permission granting. |
| **Overlay permission** | Opens system settings to grant overlay permission, required for auto-start. |
| **Battery optimization** | Opens system settings to disable battery optimization for FlipClock, improving auto-start reliability. |
| **Show date** | Show the current date in the info bar. |
| **Show weekday** | Show the current weekday in the info bar. |
| **Show lunar** | Show the lunar calendar in the info bar. |
| **Show lunar year** | Show the sexagenary year plus zodiac (e.g. 丙午马年) alongside the lunar date. On by default. |
| **Solar term two-line info bar** | When on, landscape uses two lines and portrait two columns for solar term / dog days / nine-nine display; falls back to single line/column when no content. On by default. |
| **Vertical text** | In portrait, use traditional vertical typography; off switches to stacked horizontal lines. |
| **Date on top in portrait** | In portrait, move the date to the very top; info bar lower-left, time lower-right. Off by default. |
| **Burn-in protection** | Slowly shift the info bar and time display to reduce OLED burn-in risk. On by default; requires restart. |
| **Burn-in amplitude** | Adjust displacement to 0%–5% of the screen's shorter side (default 5%). Only shown when burn-in protection is on. |
| **Show weather** | Show current weather (location, temperature, conditions). |
| **Location** | Enter a city name for weather query; tap "Verify location" to confirm. |
| **Refresh interval** | Weather refresh interval, 1–24 hours (default 1 hour). |
| **Info bar font** | Custom font for the info bar; defaults to 未来荧黑 (Glow Sans SC). |
| **Second row font** | Custom font for landscape line 2 / portrait right column; defaults to 霞鹜漫黑 (LXGW Marker Gothic). |
| **Restart app to apply** | One-tap restart to apply setting changes. |

> **Note**: Options under "Main screen display content" require a restart to take effect, or tap "Restart app to apply" at the bottom.

## Permissions

| Permission | Purpose |
|------------|---------|
| `RECEIVE_BOOT_COMPLETED` | Receive system boot broadcast for auto-start. |
| `SYSTEM_ALERT_WINDOW` | Create a temporary 1×1 transparent overlay to bypass Android 10+ background Activity launch restrictions. |
| `WAKE_LOCK` | Wake the device and turn on the screen after boot. |
| `FOREGROUND_SERVICE` | Start a foreground service for the auto-start flow. |
| `POST_NOTIFICATIONS` | Show foreground service notification on Android 13+. |
| `REQUEST_IGNORE_BATTERY_OPTIMIZATIONS` | Guide the user to disable battery optimization for better auto-start reliability. |

## Known Limitations

- Android 10+ strictly restricts background Activity launch; this project works around it via a foreground service + temporary overlay. Behavior may vary across OEM ROMs.
- Some Chinese OEM ROMs (Xiaomi, Huawei, OPPO, vivo, etc.) may require additional auto-start permission in system settings; disable automatic management in app launch settings and manually allow auto-start, associated launch, and background activity.
- If the user force-stops the app, the system blocks `BOOT_COMPLETED` broadcasts until the app is manually opened again.
- Solar terms are computed by Beijing time (UTC+8) calendar day; devices in other time zones may differ by one day from Chinese almanacs.

## Technical Notes

- The Java shim is taken directly from SDL2's `android-project` directory, unmodified, to stay in sync with upstream.
- SDL2, SDL2_ttf, and FlipClock sources are committed under `app/jni/` (formerly git submodules, now local files).
- The original FlipClock uses Meson, which cannot be used directly on Android, so `Android.mk` is used for native builds.
- Date, weekday, and lunar calendar are rendered by native C code (`info_bar.c` / `lunar.c`). The lunar calendar uses an offline algorithm (1900–2100), no network needed.
- Solar terms, dog days, and nine-nine are rendered by `solar_terms.c`, using an offline 1900–2100 solar term table (201×24). Dog days follow the geng-day algorithm; nine-nine is counted from the winter solstice. The C implementation is day-by-day verified against the Python reference (`test/almanac/almanac_ref.py`) over all 73,414 days.
- 10 open-source CJK fonts are bundled (in `assets/fonts/`, all subset-trimmed). The info bar and second row can use different fonts, under the SIL Open Font License 1.1. To add a font: place the full original font in `fonts/`, run `fonts/subset_fonts.py` with the `fonts/sub.txt` charset, put the trimmed font in `assets/fonts/`, and add a display name in `SettingsActivity.FONT_NAME_MAP`.
- Font subsetting: `fonts/sub.txt` is the runtime character set (C string literals + strings.xml + Java strings); `fonts/subset_fonts.py` uses fontTools to trim. The 10 fonts shrink from 109.6 MB to ~1.3 MB.
- Verification: `test/almanac/` contains the Python oracle (`almanac_ref.py`), C comparison harness (`almanac_harness.c`), and a five-check regression suite (`check_almanac.py`): table consistency, table validity, implementation consistency, interval invariants, and public anchor dates.
- Debugging: use `adb shell settings put global flipclock.almanac_today yyyy-MM-dd` to fake "today" for testing solar term / dog days / nine-nine display; restore with `adb shell settings delete global flipclock.almanac_today`.
- Version numbers are managed in `gradle.properties` (`APP_VERSION_CODE` / `APP_VERSION_NAME`), read by `app/build.gradle`.
- App package name: `com.stackof.flipclockv2`; weather module package: `com.stackof.flipclockv2.weather`.

## License

- Code taken directly from SDL2 and SDL2_ttf retains its original license.
- Code modified/added by me uses [Apache-2.0](LICENSE).
