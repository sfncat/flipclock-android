# FlipClock Android 开机自启动功能设计文档

## 1. 需求背景

FlipClock 作为一款常亮时钟应用，用户希望设备重启后能够自动启动并显示在主界面，无需手动打开。由于 Android 对后台自启动 Activity 有严格限制，且不同版本/OEM 行为差异较大，需要一套完整的授权 + 启动 + 亮屏方案。

## 2. 目标

- 设备开机后自动显示 FlipClock 主界面（`MainActivity`）。
- 开机后自动点亮屏幕并保持常亮，无需人工操作即可显示时钟。
- 所有敏感行为需用户手动授权（自启动开关、悬浮窗权限、通知权限、电池优化）。
- 不破坏现有 SDL2 / Java shim 的完整性。

## 3. 方案概述

采用 **BroadcastReceiver + ForegroundService + tiny 悬浮窗触发器 + MainActivity + 原生设置页** 的组合方案：

| 组件 | 职责 |
|------|------|
| `BootReceiver` | 接收 `BOOT_COMPLETED` 广播，读取用户授权状态，决定是否启动后续流程。 |
| `BootAutoStartService` | 前台服务。若已授予悬浮窗权限，先创建一个 1×1 透明悬浮窗，再启动 `MainActivity` 并唤醒/点亮屏幕；若未授予，显示通知引导用户授权。 |
| `OverlayTrigger` | 创建一个 1×1 像素的透明悬浮窗，作为应用可见窗口，帮助前台服务绕过 Android 10+ 后台启动 Activity 的限制。 |
| `MainActivity` | 在 `onCreate` 中设置 `FLAG_TURN_SCREEN_ON`、`FLAG_KEEP_SCREEN_ON` 等窗口标志，实现开机后自动亮屏并常亮；同时提供四指手势进入设置页。 |
| `SettingsActivity` | 原生 Android 设置页，提供自启动开关、悬浮窗授权、通知授权和电池优化入口。 |
| `SharedPreferences` | 持久化保存用户授权状态 `auto_start_enabled`。 |

## 4. 权限声明

在 `app/src/main/AndroidManifest.xml` 中新增以下权限：

- `android.permission.RECEIVE_BOOT_COMPLETED`：接收开机广播。
- `android.permission.FOREGROUND_SERVICE`：Android 9+ 前台服务必需。
- `android.permission.POST_NOTIFICATIONS`：Android 13+ 显示通知必需。
- `android.permission.SYSTEM_ALERT_WINDOW`：创建 tiny 悬浮窗必需，需用户手动在系统设置中授权。
- `android.permission.WAKE_LOCK`：开机后唤醒并点亮屏幕。
- `android.permission.REQUEST_IGNORE_BATTERY_OPTIMIZATIONS`：请求关闭电池优化，提高后台存活率。

## 5. 开机启动流程

```
系统开机
    |
    v
BootReceiver.onReceive()
    |
    +-- 检查 Intent 是否为 BOOT_COMPLETED
    |
    +-- 读取 SharedPreferences：auto_start_enabled
    |
    +-- false：直接返回，不做任何操作
    |
    +-- true：
            |
            +-- 启动 BootAutoStartService（前台服务）
                    |
                    +-- 已授予悬浮窗权限：
                    |       创建 1×1 透明悬浮窗（OverlayTrigger）
                    |       获取 WAKE_LOCK 唤醒设备
                    |       启动 MainActivity（主界面）
                    |       MainActivity 设置 FLAG_TURN_SCREEN_ON + FLAG_KEEP_SCREEN_ON
                    |       移除 tiny 悬浮窗，停止服务
                    |
                    +-- 未授予悬浮窗权限：
                            显示通知 "点击授权悬浮窗以开机自动显示 FlipClock"
                            用户点击后进入 SettingsActivity
```

## 6. 用户授权流程

1. 用户通过系统桌面或应用列表进入 `SettingsActivity`。
2. `SettingsActivity` 中默认显示 **"开机自动启动"** 开关，初始为关闭状态。
3. 用户首次打开开关时：
   - 弹出 Dialog 说明用途，用户点击 **"同意"** 后才保存授权。
   - 请求 **悬浮窗权限**（`SYSTEM_ALERT_WINDOW`）：跳转到系统设置页让用户手动开启。
   - Android 13+ 请求 `POST_NOTIFICATIONS` 运行时权限。
   - 引导用户关闭电池优化（`ACTION_REQUEST_IGNORE_BATTERY_OPTIMIZATIONS`）。
4. 开关状态保存到 `SharedPreferences`。
5. 关闭开关时，立即取消授权并更新 `SharedPreferences`。

## 7. 设置页设计

`SettingsActivity` 使用原生 Android UI，包含：

- 标题：FlipClock 设置
- 开关项：**开机自动启动**（带说明文字）
- 按钮：**悬浮窗权限**（跳转到系统悬浮窗授权页，显示当前授权状态）
- 按钮：**电池优化设置**（跳转到系统电池优化页面）
- 说明文字：提示部分国产 ROM 可能需要额外在系统设置中允许自启动

`SettingsActivity` 有两种进入方式：

1. **系统桌面入口**：作为独立的 Launcher Activity 暴露，方便用户直接找到设置项。
2. **主界面手势**：在 `MainActivity` 中拦截 **四指同时触屏** 手势，打开 `SettingsActivity`，避免在 SDL2 全屏时钟界面上添加可见按钮。

## 8. 亮屏与常亮实现

在 `MainActivity.onCreate()` 中设置窗口标志，确保开机后启动主界面时自动亮屏并保持常亮：

- `FLAG_TURN_SCREEN_ON`：Activity 显示时点亮屏幕。
- `FLAG_KEEP_SCREEN_ON`：Activity 可见时保持屏幕常亮。
- `FLAG_SHOW_WHEN_LOCKED`：在锁屏界面也能显示。
- Android 8.1（API 27）+ 使用 `setTurnScreenOn(true)` 和 `setShowWhenLocked(true)` 作为推荐 API，低版本回退到窗口标志。

`BootAutoStartService` 在启动 `MainActivity` 前会额外获取一个短时 `WakeLock`，确保设备从休眠中唤醒。

## 9. 兼容性与限制

| 场景 | 处理策略 |
|------|----------|
| Android 6.0 以下 | 悬浮窗权限在 Manifest 中声明即可，无需运行时跳转。 |
| Android 10+ 后台启动限制 | 使用前台服务 + 1×1 悬浮窗作为可见窗口，再启动 `MainActivity`。 |
| Android 13+ 通知权限 | 在设置页请求 `POST_NOTIFICATIONS`，未授权时仍可启动服务但通知引导不可用。 |
| 国产 ROM 限制 | 提供电池优化和悬浮窗设置入口，并在说明中提示用户手动允许自启动。 |
| 用户强制停止应用 | 开机广播不会触发，属于 Android 正常行为，需在说明中告知用户。 |

## 10. 实现清单

- [x] 更新 `app/src/main/AndroidManifest.xml`（新增权限、注册 Receiver/Service/SettingsActivity）
- [x] 新增 `BootReceiver.java`
- [x] 新增 `BootAutoStartService.java`（启动 MainActivity + tiny 悬浮窗触发器）
- [x] 新增 `OverlayTrigger.java`（1×1 透明悬浮窗）
- [x] 新增 `SettingsActivity.java`
- [x] 新增 `res/layout/activity_settings.xml`
- [x] 新增 `res/layout/overlay_trigger.xml`
- [x] 更新 `res/values/strings.xml`
- [x] 更新 `MainActivity.java`（四指手势进入设置 + 亮屏/常亮标志）

## 11. 构建说明

### 11.1 环境准备

- **JDK**：本项目使用 Gradle 8.0，与默认 Java 21 存在缓存兼容问题，建议使用 **Java 17** 编译：
  ```bash
  export JAVA_HOME=/usr/lib/jvm/java-17-openjdk-amd64
  export PATH=$JAVA_HOME/bin:$PATH
  ```
- **Android SDK / NDK**：确保已安装 Android SDK 和 NDK。当前代码已适配 NDK 25；原 `APP_PLATFORM=android-16` 在 NDK 25 中已不支持，因此已调整为 `android-19`，`minSdkVersion` 同步改为 19。

### 11.2 完整编译步骤

1. **克隆仓库并初始化子模块**
   FlipClock 以 git submodule 形式引入，用于编译 SDL2、SDL2_ttf 及 FlipClock 原生代码：
   ```bash
   git clone <仓库地址>
   cd flipclock-android
   git submodule update --init
   ```

2. **检查字体软链接**
   子模块初始化后，`app/src/main/assets/flipclock.ttf` 应是指向 `app/jni/flipclock/dists/flipclock.ttf` 的软链接。若链接未生效，手动创建：
   ```bash
   ln -s app/jni/flipclock/dists/flipclock.ttf app/src/main/assets/flipclock.ttf
   ```

3. **使用 Gradle 编译**
   项目使用 `ndk-build` 构建 JNI 部分，Gradle 负责整体打包。执行以下命令编译 Debug APK：
   ```bash
   ./gradlew assembleDebug
   ```
   编译成功后，APK 位于：
   ```
   app/build/outputs/apk/debug/app-debug.apk
   ```

4. **使用 Android Studio（可选）**
   也可以直接用 Android Studio 打开项目，等待 Gradle 同步完成后点击 **Build → Make Project** 或 **Run** 进行编译安装。

### 11.3 关键配置说明

- `app/build.gradle`：`minSdkVersion` 19，`targetSdkVersion` 33，`compileSdk` 33。
- `app/jni/Application.mk`：`APP_PLATFORM=android-19`，ABI 包含 `armeabi-v7a`、`arm64-v8a`、`x86`、`x86_64`。
- Java 层不依赖 AppCompat，使用原生 Android 框架组件（`Activity`、`Switch`、`Notification.Builder` 等）。

## 12. 测试建议

1. 在 Android 9 及以下设备测试：开机后是否直接启动 `MainActivity` 主界面。
2. 在 Android 10+ 设备测试：开机后是否显示 `MainActivity` 主界面；tiny 悬浮窗是否一闪而过并被移除。
3. 测试开关关闭时，开机后不应显示任何窗口或通知。
4. 测试 Android 13+ 上通知权限被拒绝时，前台服务行为是否符合预期。
5. 测试开机后屏幕是否能自动点亮并常亮。
6. 测试用户强制停止应用后，开机广播是否被正确屏蔽。
7. 测试四指同时触屏是否能从主界面打开设置页。
