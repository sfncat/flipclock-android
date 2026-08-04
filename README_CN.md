# FlipClock Android

FlipClock 的 Android 封装版本。

[Google Play 商店页面](https://play.google.com/store/apps/details?id=one.alynx.flipclock)

## 为什么会有这个项目？

Fliqlo 是 macOS 上的一款闭源翻页时钟应用，它也有 iOS 版本。我想为 Android 也做一个类似的版本，于是就有了 FlipClock Android。

## 主要功能

- **翻页时钟显示**：全屏显示当前时间，支持横竖屏切换。
- **默认 24 小时制 + 秒**：应用启动后默认显示 24 小时制，并带秒数。
- **日期 / 星期 / 农历显示**：可在设置页单独开启日期、星期、农历，横屏显示在主界面顶部，竖屏显示在主界面左侧，字体小于时间。
- **开机自动启动**：设备重启后自动启动 FlipClock 主界面，并自动点亮屏幕、保持常亮，无需手动操作。
- **用户授权管理**：所有敏感权限（自启动、悬浮窗、通知、电池优化等）均需要用户在设置中手动开启并同意。
- **原生设置页**：可设置开机自启动、悬浮窗权限、电池优化等。
- **四指手势进入设置**：在主界面用四指同时点击屏幕，可打开设置页。
- **手势切换**：
  - 双指触摸 / 双击：切换 12/24 小时制。
  - 三指触摸：显示/隐藏秒数。
  - 旋转手机：切换横屏/竖屏显示（需在系统中开启自动旋转）。

## 适用场景

建议找一台带有 **LCD 屏幕** 的旧手机作为专用时钟。现代非方形屏幕手机多为 OLED，长时间显示固定图案（如翻页时钟）容易导致烧屏。

## 编译说明

### 环境要求

- Android SDK
- Android NDK（已适配 NDK 25）
- JDK 17（Gradle 8.0 与默认 Java 21 存在兼容问题，建议用 Java 17）

### 完整编译步骤

1. 克隆仓库

   SDL2、SDL2_ttf 及 FlipClock 原生代码已直接提交在仓库内（`app/jni/` 下），无需初始化子模块：

   ```bash
   git clone <仓库地址>
   cd flipclock-android
   ```

2. 检查字体软链接

   `app/src/main/assets/flipclock.ttf` 是指向 `app/jni/flipclock/dists/flipclock.ttf` 的软链接，已随仓库一起提交。若链接未生效，手动创建：

   ```bash
   ln -s app/jni/flipclock/dists/flipclock.ttf app/src/main/assets/flipclock.ttf
   ```

3. 使用 Java 17 编译

   ```bash
   export JAVA_HOME=/usr/lib/jvm/java-17-openjdk-amd64
   export PATH=$JAVA_HOME/bin:$PATH
   ./gradlew assembleDebug
   ```

   编译成功后，APK 位于：

   ```
   app/build/outputs/apk/debug/app-debug.apk
   ```

4. 使用 Android Studio（可选）

   也可以直接用 Android Studio 打开项目，等待 Gradle 同步完成后点击 **Build → Make Project** 或 **Run** 进行编译安装。

### 关键配置说明

- `app/build.gradle`：`minSdkVersion` 19，`targetSdkVersion` 33，`compileSdk` 33。
- `app/jni/Application.mk`：`APP_PLATFORM=android-19`，ABI 包含 `armeabi-v7a`、`arm64-v8a`、`x86`、`x86_64`。
- 由于 NDK 25 不再支持 `android-16`，项目已将 `APP_PLATFORM` 和 `minSdkVersion` 调整为 19。

## 使用说明

### 首次使用

1. 安装并打开应用，默认以 **24 小时制 + 秒** 显示。
2. 打开应用后，用 **四指同时点击屏幕** 打开设置页。
3. 在设置页中开启 **开机自动启动**，并按提示授予以下权限：
   - **悬浮窗权限**：用于在 Android 10+ 上创建临时可见窗口，从而允许后台启动主界面。
   - **通知权限**（Android 13+）：用于前台服务通知。
   - **电池优化**：建议关闭电池优化，否则系统可能杀死开机自启动服务。
4. （可选）在设置页的 **主界面显示内容** 中开启 **日期 / 星期 / 农历**，更改后需重启 FlipClock 才能生效。横屏时这三项显示在主界面顶部，竖屏时显示在主界面左侧。
5. 授权完成后，重启设备即可自动进入 FlipClock 主界面，并自动点亮屏幕、保持常亮。

### 手势操作

| 手势 | 功能 |
|------|------|
| 双指触摸 / 双击 | 切换 12/24 小时制 |
| 三指触摸 | 显示/隐藏秒数 |
| 四指同时点击 | 打开设置页 |
| 旋转手机 | 切换横屏/竖屏 |

### 设置页入口

设置页可通过两种方式进入：

1. 在系统桌面/应用列表中找到 **FlipClock 设置** 图标。
2. 在 FlipClock 主界面用 **四指同时点击屏幕**。

## 权限说明

| 权限 | 用途 |
|------|------|
| `RECEIVE_BOOT_COMPLETED` | 接收系统开机广播，实现开机自启动。 |
| `SYSTEM_ALERT_WINDOW` | 创建临时 1×1 透明悬浮窗，用于绕过 Android 10+ 后台启动 Activity 的限制。 |
| `WAKE_LOCK` | 开机后唤醒设备并点亮屏幕。 |
| `FOREGROUND_SERVICE` | 启动前台服务以执行开机自启动流程。 |
| `POST_NOTIFICATIONS` | Android 13+ 显示前台服务通知。 |
| `REQUEST_IGNORE_BATTERY_OPTIMIZATIONS` | 引导用户关闭电池优化，提高开机自启动成功率。 |

## 已知限制

- Android 10+ 对后台启动 Activity 有严格限制，本项目通过前台服务 + 临时悬浮窗的方式绕过。不同厂商/ROM 行为可能存在差异。
- 部分国产 ROM（小米、华为、OPPO、vivo 等）可能还需要在系统设置中额外允许应用自启动，否则开机后可能无法正常工作。
- 如果用户手动强制停止应用，系统会屏蔽 `BOOT_COMPLETED` 广播，直到用户再次手动打开应用。
- 长时间在 OLED 屏幕上显示固定图案可能导致烧屏，建议使用 LCD 屏幕设备作为专用时钟。

## 技术说明

- Java shim 直接取自 SDL2 的 `android-project` 目录，未做修改，以便与上游保持一致。
- SDL2、SDL2_ttf 及 FlipClock 的源码直接提交在 `app/jni/` 下（原为 git submodule，现已改为本地文件）。
- 原 FlipClock 使用 Meson 构建，Android 上无法直接使用，因此使用 `Android.mk` 进行原生构建。
- 日期、星期、农历由原生 C 代码渲染（`info_bar.c` / `lunar.c`），农历使用离线算法（1900–2100），无需网络。中文字体为子集化的 Noto Sans CJK SC（`assets/flipclock_cjk.ttf`），遵循 SIL Open Font License。

## 许可证

- 直接取自 SDL2 和 SDL2_ttf 的代码保持其原有许可证。
- 本人修改/新增的代码使用 [Apache-2.0](LICENSE)。

