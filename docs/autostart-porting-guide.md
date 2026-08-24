# 开机自启动功能移植指南

> 本文档基于 FlipClockV2 项目的完整实现，详细描述 Android 应用"开机自启动并显示到前台"功能的技术方案、全部代码和移植步骤。按本文档操作，可在任意 Android 应用中复刻该功能。
>
> 参考源码位置：`app/src/main/java/com/stackof/flipclockv2/` 下的 `BootReceiver.java`、`BootAutoStartService.java`、`OverlayTrigger.java`、`SettingsActivity.java`、`MainActivity.java`。

---

## 目录

1. [功能概述](#1-功能概述)
2. [核心难点与总体架构](#2-核心难点与总体架构)
3. [第 1 步：AndroidManifest.xml 配置](#3-第-1-步androidmanifestxml-配置)
4. [第 2 步：BootReceiver（开机广播接收器）](#4-第-2-步bootreceiver开机广播接收器)
5. [第 3 步：BootAutoStartService（前台服务）](#5-第-3-步bootautostartservice前台服务)
6. [第 4 步：OverlayTrigger（1×1 悬浮窗触发器）](#6-第-4-步overlaytrigger1x1-悬浮窗触发器)
7. [第 5 步：MainActivity 点亮屏幕配置](#7-第-5-步mainactivity-点亮屏幕配置)
8. [第 6 步：设置界面（开关 + 权限引导）](#8-第-6-步设置界面开关--权限引导)
9. [第 7 步：资源文件](#9-第-7-步资源文件)
10. [版本兼容矩阵](#10-版本兼容矩阵)
11. [厂商兼容性（小米/华为/OPPO/vivo 等）](#11-厂商兼容性小米华为oppovivo-等)
12. [测试验收清单](#12-测试验收清单)
13. [移植核对清单（Checklist）](#13-移植核对清单checklist)
14. [常见问题排查](#14-常见问题排查)

---

## 1. 功能概述

设备开机（BOOT_COMPLETED 广播）后，应用自动启动并将主界面显示到前台。本方案以"时钟/相框类全屏应用"为目标，额外包含**点亮屏幕**和**锁屏上显示**能力；如果你的应用不需要这些，可以按需裁剪（见各步骤中的"可选"标注）。

功能组成：

| 能力 | 说明 | 是否必需 |
|---|---|---|
| 开机广播监听 | 接收 `BOOT_COMPLETED` | 必需 |
| 自启动开关 | 用户显式开启，SharedPreferences 持久化 | 必需（强烈建议） |
| 前台服务拉起 Activity | Android 10+ 从后台启动 Activity 受限，需前台服务中转 | 必需（目标 minSdk ≥ 29 时） |
| 1×1 悬浮窗 | 使应用"可见"，绕过后台启动 Activity 限制 | 必需（核心技巧） |
| 点亮屏幕 WakeLock | 开机后屏幕可能是熄灭状态，需要点亮 | 可选（仅锁屏/时钟类应用） |
| 权限引导 UI | 引导用户授予悬浮窗、通知权限、关闭电池优化 | 必需（用户体验保障） |

---

## 2. 核心难点与总体架构

### 2.1 为什么不能直接 startActivity？

Android 各版本对"从后台启动 Activity"的限制逐步收紧：

- **Android 9（API 28）及以下**：BroadcastReceiver 中可以直接 `startActivity()`。
- **Android 10（API 29）+**：后台启动 Activity 被系统禁止，除非应用处于"可见"状态或持有豁免。

**本方案的核心技巧**：前台服务中先添加一个 **1×1 像素的透明悬浮窗**（需要 `SYSTEM_ALERT_WINDOW` 权限）。悬浮窗使应用拥有"可见窗口"，系统即允许该应用从后台启动 Activity。启动完成后立即移除悬浮窗。

### 2.2 完整启动链路

```
设备开机
   │
   ▼
BootReceiver.onReceive()  ── 检查 auto_start_enabled（默认 false，未开启则直接返回）
   │
   ├── Android 9 及以下 ──► 直接 startActivity(MainActivity)
   │
   └── Android 10+ ──► startForegroundService(BootAutoStartService)
                              │
                              ▼
                     BootAutoStartService.onStartCommand()
                              │
                   ┌──────────┴──────────┐
           悬浮窗权限已授予            悬浮窗权限未授予
                   │                     │
                   ▼                     ▼
        OverlayTrigger.show()    startForeground(通知：
        （1×1 透明悬浮窗）        "点击开启悬浮窗权限"，
                   │              点击跳转设置页)
                   ▼
        wakeScreen()（WakeLock 点亮屏幕，可选）
                   │
                   ▼
        startActivity(MainActivity)
                   │
                   ▼
        延迟 1 秒后移除悬浮窗、stopSelf()
```

### 2.3 组件与文件清单（移植时需创建/修改）

| 文件 | 类型 | 作用 |
|---|---|---|
| `AndroidManifest.xml` | 修改 | 声明权限、注册 Receiver 和 Service |
| `BootReceiver.java` | 新增 | 接收开机广播，读取开关，分发启动 |
| `BootAutoStartService.java` | 新增 | 前台服务：悬浮窗 + 拉起 Activity + 通知 |
| `OverlayTrigger.java` | 新增 | 1×1 悬浮窗辅助类 |
| `overlay_trigger.xml` | 新增 | 悬浮窗布局（1px 透明） |
| `MainActivity.java` | 修改 | 点亮屏幕/锁屏显示 flags（可选） |
| `SettingsActivity.java`（或你的设置页） | 修改 | 自启动开关、确认对话框、权限引导 |
| `strings.xml` | 修改 | 文案资源 |
| 设置页布局 xml | 修改 | 开关 + 描述 + 权限按钮 |

---

## 3. 第 1 步：AndroidManifest.xml 配置

### 3.1 权限声明

```xml
<!-- 开机自启动 -->
<uses-permission android:name="android.permission.RECEIVE_BOOT_COMPLETED" />
<uses-permission android:name="android.permission.FOREGROUND_SERVICE" />
<uses-permission android:name="android.permission.POST_NOTIFICATIONS" />
<uses-permission android:name="android.permission.REQUEST_IGNORE_BATTERY_OPTIMIZATIONS" />
<uses-permission android:name="android.permission.SYSTEM_ALERT_WINDOW" />
<uses-permission android:name="android.permission.WAKE_LOCK" />
```

各权限用途：

| 权限 | 用途 |
|---|---|
| `RECEIVE_BOOT_COMPLETED` | 接收开机广播（核心） |
| `FOREGROUND_SERVICE` | Android 9+ 启动前台服务 |
| `POST_NOTIFICATIONS` | Android 13+ 显示前台服务通知 |
| `REQUEST_IGNORE_BATTERY_OPTIMIZATIONS` | 请求加入电池优化白名单（提高广播到达率） |
| `SYSTEM_ALERT_WINDOW` | 显示悬浮窗（后台启动 Activity 的关键） |
| `WAKE_LOCK` | 点亮屏幕（仅锁屏/时钟类应用需要） |

> **Android 14+（API 34）注意**：如果 `targetSdkVersion >= 34`，前台服务还必须声明具体类型权限，例如 `FOREGROUND_SERVICE_DATA_SYNC`，并且 manifest 中 service 的 `foregroundServiceType` 要与之匹配。本示例使用 `dataSync` 类型（见 3.3）。

### 3.2 注册开机广播接收器

在 `<application>` 内添加：

```xml
<receiver
    android:name="BootReceiver"
    android:enabled="true"
    android:exported="true">
    <intent-filter>
        <action android:name="android.intent.action.BOOT_COMPLETED" />
    </intent-filter>
</receiver>
```

要点：
- `exported="true"` 是**必须的**（`BOOT_COMPLETED` 是系统广播；从 Android 12 起，带 intent-filter 的组件必须显式声明 exported）。
- `RECEIVE_BOOT_COMPLETED` 权限本身起到保护作用，普通应用无法伪造此广播。

### 3.3 注册前台服务

```xml
<service
    android:name="BootAutoStartService"
    android:enabled="true"
    android:exported="false"
    android:foregroundServiceType="dataSync" />
```

要点：
- `exported="false"`：服务只由本应用启动。
- `foregroundServiceType`：Android 10+ 需要；`dataSync` 是通用选择。**Android 14 起，若声明了类型，还需在 manifest 中追加对应权限**：

```xml
<uses-permission android:name="android.permission.FOREGROUND_SERVICE_DATA_SYNC" />
```

---

## 4. 第 2 步：BootReceiver（开机广播接收器）

完整代码（可直接复制，替换包名和 MainActivity 类名）：

```java
package com.stackof.flipclockv2;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.os.Build;

/**
 * 接收 BOOT_COMPLETED 广播，若用户已开启自启动则启动应用。
 */
public class BootReceiver extends BroadcastReceiver {
    // 与设置页使用同一个 prefs 文件名和 key（重要！必须一致）
    private static final String PREFS_NAME = "flipclock_settings";   // TODO: 换成你的应用名
    private static final String KEY_AUTO_START = "auto_start_enabled";

    @Override
    public void onReceive(Context context, Intent intent) {
        if (intent == null || !Intent.ACTION_BOOT_COMPLETED.equals(intent.getAction())) {
            return;
        }

        // 用户未开启自启动则直接返回（默认 false）
        SharedPreferences prefs = context.getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE);
        if (!prefs.getBoolean(KEY_AUTO_START, false)) {
            return;
        }

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
            // Android 10+ 限制后台启动 Activity，通过前台服务中转
            Intent serviceIntent = new Intent(context, BootAutoStartService.class);
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
                context.startForegroundService(serviceIntent);
            } else {
                context.startService(serviceIntent);
            }
        } else {
            // Android 9 及以下可以直接启动 Activity
            Intent activityIntent = new Intent(context, MainActivity.class); // TODO: 换成你的主 Activity
            activityIntent.addFlags(Intent.FLAG_NEW_TASK
                    | Intent.FLAG_ACTIVITY_CLEAR_TOP
                    | Intent.FLAG_ACTIVITY_SINGLE_TOP);
            context.startActivity(activityIntent);
        }
    }
}
```

**移植要点**：
1. `PREFS_NAME` 和 `KEY_AUTO_START` 必须与设置页写入时**完全一致**，建议抽到公共常量类。
2. `FLAG_ACTIVITY_NEW_TASK`：从非 Activity 上下文启动 Activity 时必须加。
3. 不要在 `onReceive` 中做耗时操作（超过 10 秒会 ANR）；本实现只做判断和转发，符合要求。
4. `goAsync()` 不需要——启动服务是异步的。

---

## 5. 第 3 步：BootAutoStartService（前台服务）

完整代码：

```java
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
 * 开机后启动的前台服务。
 *
 * Android 10+ 从后台启动 Activity 受限，因此先创建一个 1×1 的可见悬浮窗
 * （需要 SYSTEM_ALERT_WINDOW 权限），使应用"可见"，再启动 MainActivity。
 * 若悬浮窗权限未授予，则显示引导通知让用户去设置页开启。
 */
public class BootAutoStartService extends Service {
    private static final String CHANNEL_ID = "flipclock_auto_start"; // TODO: 换成你的渠道 ID
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
            // 权限已授予：先显示 1×1 悬浮窗使应用可见，再启动主 Activity
            mOverlayTrigger = new OverlayTrigger(this);
            if (mOverlayTrigger.show()) {
                wakeScreen();               // 可选：点亮屏幕（时钟/锁屏类应用需要）
                launchMainActivity();
                // 1 秒后移除悬浮窗并停止服务
                mHandler.postDelayed(() -> {
                    if (mOverlayTrigger != null) {
                        mOverlayTrigger.remove();
                    }
                    stopSelf();
                }, 1000);
                // 前台服务必须调用 startForeground
                startForeground(NOTIFICATION_ID, buildRunningNotification());
            } else {
                startForeground(NOTIFICATION_ID, buildSettingsNotification());
            }
        } else {
            // 权限未授予：显示引导通知
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
        Intent activityIntent = new Intent(this, MainActivity.class); // TODO: 换成你的主 Activity
        activityIntent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK
                | Intent.FLAG_ACTIVITY_CLEAR_TOP
                | Intent.FLAG_ACTIVITY_SINGLE_TOP
                | Intent.FLAG_ACTIVITY_EXCLUDE_FROM_RECENTS
                | Intent.FLAG_ACTIVITY_BROUGHT_TO_FRONT);
        try {
            startActivity(activityIntent);
        } catch (Exception ignored) {
            // 部分设备可能仍然拦截后台启动，静默失败即可
        }
    }

    /** 点亮屏幕（可选，仅锁屏/时钟类应用需要） */
    private void wakeScreen() {
        PowerManager powerManager = (PowerManager) getSystemService(POWER_SERVICE);
        if (powerManager == null) {
            return;
        }
        mWakeLock = powerManager.newWakeLock(
                PowerManager.SCREEN_BRIGHT_WAKE_LOCK
                        | PowerManager.ACQUIRE_CAUSES_WAKEUP
                        | PowerManager.ON_AFTER_RELEASE,
                "FlipClockV2:BootWakeLock"); // TODO: 换成 "你的应用名:BootWakeLock"
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

    /** 运行中通知（悬浮窗路径，用户基本看不到，1 秒后服务即停止） */
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
                .setSmallIcon(R.mipmap.ic_launcher) // TODO: 换成你的小图标
                .setContentIntent(pendingIntent)
                .setOngoing(true)
                .build();
    }

    /** 引导通知（悬浮窗权限缺失时，点击进入设置页） */
    private Notification buildSettingsNotification() {
        Intent settingsIntent = new Intent(this, SettingsActivity.class); // TODO: 换成你的设置页
        settingsIntent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK
                | Intent.FLAG_ACTIVITY_CLEAR_TOP
                | Intent.FLAG_ACTIVITY_SINGLE_TOP);

        PendingIntent pendingIntent = createPendingIntent(settingsIntent);

        Notification.Builder builder = createNotificationBuilder();
        return builder
                .setContentTitle(getString(R.string.auto_start_notification_title))
                .setContentText(getString(R.string.auto_start_notification_permission_text))
                .setSmallIcon(R.mipmap.ic_launcher) // TODO: 换成你的小图标
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
        if (Build.VERSION.SDK.SDK_INT >= Build.VERSION_CODES.M) { // 注意：原文为 SDK_INT，此处为笔误更正
            flags |= PendingIntent.FLAG_IMMUTABLE;
        }
        return PendingIntent.getActivity(this, 0, intent, flags);
    }
}
```

> **勘误**：上面 `createPendingIntent` 中应为 `Build.VERSION.SDK_INT`（原项目代码即为 `SDK_INT`，摘录时勿写成 `SDK`）。

**移植要点**：

1. **`startForeground()` 必须在服务启动后 5 秒内调用**，否则 Android 9+ 会 ANR 崩溃（`ForegroundServiceDidNotStartInTimeException`）。本实现所有分支都调用了它。
2. `START_NOT_STICKY`：开机启动是一次性动作，被杀后无需重建。
3. 悬浮窗移除延时 1 秒是经验值：给 Activity 启动留出时间，确保"可见窗口"在系统校验期间存在。部分严格设备可尝试延长到 2~3 秒。
4. `wakeScreen()` 的 `SCREEN_BRIGHT_WAKE_LOCK` 已被标记 deprecated，但仍可用；targetSdk 很新时可改用 `setTurnScreenOn()`（见第 7 步）。
5. 如果你的应用**不需要点亮屏幕**，直接删除 `wakeScreen()`/`releaseWakeLock()` 及 `WAKE_LOCK` 权限。

---

## 6. 第 4 步：OverlayTrigger（1×1 悬浮窗触发器）

这是整个方案的**核心技巧**。完整代码：

```java
package com.stackof.flipclockv2;

import android.content.Context;
import android.graphics.PixelFormat;
import android.os.Build;
import android.provider.Settings;
import android.view.Gravity;
import android.view.LayoutInflater;
import android.view.View;
import android.view.WindowManager;

/**
 * 创建一个 1×1 像素的透明悬浮窗。
 *
 * 在部分 Android 版本和 OEM 设备上，前台服务拥有可见窗口后，
 * 即可从后台启动 Activity。该悬浮窗在启动主界面后立即移除。
 */
public class OverlayTrigger {
    private final Context mContext;
    private WindowManager mWindowManager;
    private View mTriggerView;

    public OverlayTrigger(Context context) {
        mContext = context.getApplicationContext();
    }

    /**
     * 显示悬浮窗。
     * @return true 显示成功；false 权限缺失或添加失败
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

        // Android 8.0+ 必须用 TYPE_APPLICATION_OVERLAY
        int type = Build.VERSION.SDK_INT >= Build.VERSION_CODES.O
                ? WindowManager.LayoutParams.TYPE_APPLICATION_OVERLAY
                : WindowManager.LayoutParams.TYPE_PHONE;

        WindowManager.LayoutParams params = new WindowManager.LayoutParams(
                1,                              // 宽 1px
                1,                              // 高 1px
                type,
                WindowManager.LayoutParams.FLAG_NOT_FOCUSABLE  // 不抢焦点
                        | WindowManager.LayoutParams.FLAG_NOT_TOUCHABLE   // 不拦截触摸
                        | WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON, // 保持屏幕亮
                PixelFormat.TRANSLUCENT);       // 透明
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

    /** 移除悬浮窗 */
    public void remove() {
        if (mTriggerView != null && mWindowManager != null) {
            try {
                mWindowManager.removeView(mTriggerView);
            } catch (IllegalArgumentException ignored) {
                // View 可能已被移除
            }
        }
        mTriggerView = null;
        mWindowManager = null;
    }
}
```

配套布局 `res/layout/overlay_trigger.xml`：

```xml
<?xml version="1.0" encoding="utf-8"?>
<FrameLayout xmlns:android="http://schemas.android.com/apk/res/android"
    android:layout_width="1px"
    android:layout_height="1px"
    android:background="#00000000" />
```

**移植要点**：

1. `FLAG_NOT_FOCUSABLE | FLAG_NOT_TOUCHABLE` 确保用户完全无感知（不挡操作、不留残影）。
2. `addView` 必须 try-catch：部分 ROM 上即使 `canDrawOverlays()` 返回 true 也可能抛异常（如"window type not allowed"）。
3. `TYPE_PHONE` 在 Android 8.0+ 已废弃且会直接抛异常，版本分支不能省。
4. 也可以不使用布局文件，直接 `new View(mContext)` 代替 inflate，效果相同。

---

## 7. 第 5 步：MainActivity 点亮屏幕配置

如果希望开机后应用出现在**锁屏之上**并点亮屏幕（时钟/闹钟/相框类应用），在 `MainActivity.onCreate()` 中添加：

```java
private void keepScreenOnAfterBoot() {
    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O_MR1) {
        setTurnScreenOn(true);      // API 27+
        setShowWhenLocked(true);    // API 27+，锁屏上显示
    }
    getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON      // 保持常亮
            | WindowManager.LayoutParams.FLAG_TURN_SCREEN_ON                // 点亮屏幕（旧 API）
            | WindowManager.LayoutParams.FLAG_SHOW_WHEN_LOCKED);            // 锁屏上显示（旧 API）
}
```

**移植要点**：
- 普通（非锁屏类）应用**跳过此步**，同时可以移除 Service 中的 `wakeScreen()`。
- 新旧两套 API 同时设置是标准做法，系统会自动忽略不适用的那套。

---

## 8. 第 6 步：设置界面（开关 + 权限引导）

设置页承担三件事：**开关状态管理**、**开启时二次确认**、**权限引导**。

### 8.1 常量（必须与 BootReceiver 一致）

```java
private static final String PREFS_NAME = "flipclock_settings";   // TODO: 换成你的
private static final String KEY_AUTO_START = "auto_start_enabled";
private static final int REQUEST_NOTIFICATION_PERMISSION = 100;
private static final int REQUEST_BATTERY_OPTIMIZATION = 101;
```

### 8.2 初始化开关 & 监听

```java
// onCreate / onStart 中恢复状态
SharedPreferences prefs = getSharedPreferences(PREFS_NAME, MODE_PRIVATE);
boolean enabled = prefs.getBoolean(KEY_AUTO_START, false);
autoStartSwitch.setChecked(enabled);

autoStartSwitch.setOnCheckedChangeListener((buttonView, isChecked) -> {
    if (isChecked) {
        showEnableConfirmationDialog();   // 开启需二次确认
    } else {
        saveAutoStart(false);
        updateDescription();
    }
});
```

### 8.3 二次确认对话框（告知用户所需权限）

```java
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
```

### 8.4 权限请求（开启自启动时一次性引导）

```java
private void requestNeededPermissions() {
    // 1. 悬浮窗权限（后台启动 Activity 的关键）
    if (!Settings.canDrawOverlays(this)) {
        openOverlaySettings();
    }

    // 2. Android 13+ 通知权限（前台服务通知需要）
    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
        if (checkSelfPermission(android.Manifest.permission.POST_NOTIFICATIONS)
                != PackageManager.PERMISSION_GRANTED) {
            requestPermissions(
                    new String[]{android.Manifest.permission.POST_NOTIFICATIONS},
                    REQUEST_NOTIFICATION_PERMISSION);
        }
    }

    // 3. 请求关闭电池优化（提高开机广播到达率）
    requestDisableBatteryOptimization();
}

private void openOverlaySettings() {
    // 悬浮窗权限是特殊权限，不能弹窗申请，只能跳系统设置页
    Intent intent = new Intent(
            Settings.ACTION_MANAGE_OVERLAY_PERMISSION,
            Uri.parse("package:" + getPackageName()));
    startActivity(intent);
}

private void requestDisableBatteryOptimization() {
    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
        PowerManager pm = (PowerManager) getSystemService(POWER_SERVICE);
        if (pm != null && !pm.isIgnoringBatteryOptimizations(getPackageName())) {
            try {
                Intent intent = new Intent(
                        Settings.ACTION_REQUEST_IGNORE_BATTERY_OPTIMIZATIONS,
                        Uri.parse("package:" + getPackageName()));
                startActivityForResult(intent, REQUEST_BATTERY_OPTIMIZATION);
            } catch (Exception e) {
                openBatteryOptimizationSettings(); // 部分设备不支持直接请求，退到列表页
            }
        }
    }
}

private void openBatteryOptimizationSettings() {
    try {
        startActivity(new Intent(Settings.ACTION_IGNORE_BATTERY_OPTIMIZATION_SETTINGS));
    } catch (Exception ignored) {
    }
}
```

### 8.5 状态描述刷新（可选）

```java
private void updateDescription() {
    boolean enabled = prefs.getBoolean(KEY_AUTO_START, false);
    autoStartDescription.setText(enabled
            ? R.string.auto_start_enabled_description
            : R.string.auto_start_disabled_description);
    updateOverlayButton(); // 刷新悬浮窗权限按钮的"已授予/未授予"文案
}
```

建议在 `onResume()` 中也调用 `updateOverlayButton()`，用户从系统设置返回后立即刷新按钮状态。

### 8.6 设置页布局（参考）

```xml
<!-- 自启动开关行 -->
<LinearLayout
    android:layout_width="match_parent"
    android:layout_height="wrap_content"
    android:orientation="horizontal"
    android:gravity="center_vertical">

    <TextView
        android:layout_width="0dp"
        android:layout_height="wrap_content"
        android:layout_weight="1"
        android:text="@string/auto_start_switch_label"
        android:textAppearance="?android:attr/textAppearanceMedium" />

    <Switch
        android:id="@+id/auto_start_switch"
        android:layout_width="wrap_content"
        android:layout_height="wrap_content" />
</LinearLayout>

<!-- 状态描述 -->
<TextView
    android:id="@+id/auto_start_description"
    android:layout_width="match_parent"
    android:layout_height="wrap_content"
    android:paddingBottom="16dp"
    android:textAppearance="?android:attr/textAppearanceSmall" />

<!-- 权限入口按钮 -->
<Button
    android:id="@+id/overlay_permission_button"
    android:layout_width="match_parent"
    android:layout_height="wrap_content"
    android:text="@string/overlay_permission_button" />

<Button
    android:id="@+id/battery_optimization_button"
    android:layout_width="match_parent"
    android:layout_height="wrap_content"
    android:text="@string/battery_optimization_button" />

<!-- OEM 提示 -->
<TextView
    android:layout_width="match_parent"
    android:layout_height="wrap_content"
    android:text="@string/oem_auto_start_hint"
    android:textAppearance="?android:attr/textAppearanceSmall" />
```

---

## 9. 第 7 步：资源文件

`res/values/strings.xml` 需要添加的完整文案（可直接复制后修改应用名）：

```xml
<!-- 设置页 -->
<string name="auto_start_switch_label">开机自动启动</string>
<string name="auto_start_enabled_description">已启用：设备开机后将以悬浮窗形式自动显示 FlipClockV2。</string>
<string name="auto_start_disabled_description">已禁用：设备开机后不会自动显示 FlipClockV2。</string>
<string name="auto_start_dialog_title">允许开机自动启动？</string>
<string name="auto_start_dialog_message">开启后，FlipClockV2 将在设备开机后以悬浮窗形式显示在最前台。此功能需要系统自启动权限、悬浮窗权限、通知权限，并建议关闭电池优化以保证稳定性。</string>
<string name="auto_start_dialog_agree">同意</string>
<string name="auto_start_dialog_cancel">取消</string>
<string name="overlay_permission_button">悬浮窗权限</string>
<string name="overlay_permission_granted">已授予</string>
<string name="overlay_permission_not_granted">未授予（点击开启）</string>
<string name="battery_optimization_button">电池优化设置</string>
<string name="oem_auto_start_hint">提示：部分手机（如小米、华为、OPPO、vivo 等）可能还需要在系统设置中手动允许 FlipClockV2 自启动，否则开机后可能无法正常工作。</string>

<!-- 通知 -->
<string name="auto_start_channel_name">开机自启动</string>
<string name="auto_start_channel_description">设备开机后用于启动 FlipClockV2 的通知</string>
<string name="auto_start_notification_title">FlipClockV2</string>
<string name="auto_start_notification_running_text">悬浮时钟正在运行</string>
<string name="auto_start_notification_permission_text">点击开启悬浮窗权限以自动显示时钟</string>
```

**统一替换项**：把所有 `FlipClockV2` 替换为你的应用名。

---

## 10. 版本兼容矩阵

| Android 版本 | API | 行为分支 |
|---|---|---|
| 5.0–9.0 | 21–28 | Receiver 直接 `startActivity`；悬浮窗用 `TYPE_PHONE` |
| 8.0+ | 26+ | 前台服务须 5 秒内 `startForeground`；悬浮窗改用 `TYPE_APPLICATION_OVERLAY`；通知须建 Channel |
| 10+ | 29+ | **不能**直接从 Receiver 启动 Activity，走前台服务 + 悬浮窗路径 |
| 12+ | 31+ | Manifest 中 Receiver/Service 必须显式声明 `exported`；PendingIntent 须带 `FLAG_IMMUTABLE` |
| 13+ | 33+ | 通知需运行时申请 `POST_NOTIFICATIONS` |
| 14+ | 34+ | 前台服务须声明 `foregroundServiceType` 及配套权限（`FOREGROUND_SERVICE_DATA_SYNC` 等） |

本方案代码已覆盖以上全部分支；移植时**不要删掉任何 `Build.VERSION.SDK_INT` 判断**。

---

## 11. 厂商兼容性（小米/华为/OPPO/vivo 等）

国内主流 ROM 在 AOSP 之上还有**自启动管家**，即使所有代码正确，未在 ROM 中允许自启动时 `BOOT_COMPLETED` 广播根本不会送达。

应对策略（按优先级）：

1. **设置页提示**（本方案已实现）：`oem_auto_start_hint` 文案明确告知用户去系统设置开启。
2. **权限按钮引导**（本方案已实现）：提供悬浮窗权限、电池优化入口按钮。
3. **代码直接跳转各厂商自启动设置页**（可选增强，失败需 try-catch 回退）：

```java
// 示例：跳转厂商自启动管理页（具体 ComponentName 随 ROM 版本变化，务必 try-catch）
private static final String[][] AUTO_START_INTENTS = {
    {"com.miui.securitycenter", "com.miui.permcenter.autostart.AutoStartManagementActivity"}, // 小米
    {"com.huawei.systemmanager", "com.huawei.systemmanager.startupmgr.ui.StartupNormalAppListActivity"}, // 华为
    {"com.coloros.safecenter", "com.coloros.safecenter.permission.startup.StartupAppListActivity"}, // OPPO
    {"com.iqoo.secure", "com.iqoo.secure.ui.phoneoptimize.BgStartUpManager"}, // vivo
};

void openOemAutoStartSettings(Context ctx) {
    for (String[] comp : AUTO_START_INTENTS) {
        try {
            Intent intent = new Intent();
            intent.setComponent(new ComponentName(comp[0], comp[1]));
            intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
            ctx.startActivity(intent);
            return;
        } catch (Exception ignored) { }
    }
    // 全部失败则打开应用详情页
    try {
        ctx.startActivity(new Intent(android.provider.Settings.ACTION_APPLICATION_DETAILS_SETTINGS,
                Uri.parse("package:" + ctx.getPackageName())));
    } catch (Exception ignored) { }
}
```

> 注意：这些内部 Activity 名随时可能变更，必须逐个 try-catch，最终回退到应用详情页。

---

## 12. 测试验收清单

不重启设备也可快速验证（推荐先做 ADB 测试）：

```bash
# 模拟开机广播（需要设备已安装应用且至少启动过一次）
adb shell am broadcast -a android.intent.action.BOOT_COMPLETED -p com.stackof.flipclockv2

# 查看广播是否送达 / 服务是否启动
adb logcat | grep -iE "boot|flipclock"

# 真实重启测试
adb reboot
```

验收项：

| # | 场景 | 预期 |
|---|---|---|
| 1 | 开关关闭时广播/重启 | 无任何反应 |
| 2 | 开关开启 + 悬浮窗权限已授予 + 重启 | 主界面自动出现在前台 |
| 3 | 开关开启 + 悬浮窗权限未授予 + 重启 | 收到引导通知，点击进入设置页 |
| 4 | 开关开启 + 未授予通知权限（Android 13+）+ 重启 | 服务正常运行不崩溃（通知可能不显示） |
| 5 | 开启自启动时的对话框 | 取消后开关回弹为关闭 |
| 6 | 从系统设置返回设置页 | 悬浮窗按钮状态即时刷新（onResume） |
| 7 | Android 10/12/13/14 各一台设备 | 均能拉起主界面 |
| 8 | 小米/华为任一台，未开 ROM 自启动 | 广播不送达（预期行为，提示文案已覆盖） |
| 9 | 服务启动 5 秒内 startForeground | 无 `ForegroundServiceDidNotStartInTimeException` 崩溃 |
| 10 | 1 秒后悬浮窗移除 | 屏幕上无任何可见残留 |

---

## 13. 移植核对清单（Checklist）

按顺序逐项打勾：

- [ ] Manifest 声明 6 项权限（targetSdk 34+ 追加 `FOREGROUND_SERVICE_DATA_SYNC`）
- [ ] Manifest 注册 `BootReceiver`（exported=true，intent-filter 含 BOOT_COMPLETED）
- [ ] Manifest 注册 `BootAutoStartService`（exported=false，foregroundServiceType）
- [ ] 复制 `BootReceiver.java`，替换包名/MainActivity/prefs 常量
- [ ] 复制 `BootAutoStartService.java`，替换 CHANNEL_ID、MainActivity、SettingsActivity、图标、字符串引用
- [ ] 复制 `OverlayTrigger.java` 和 `overlay_trigger.xml`
- [ ] （可选）MainActivity 加 `keepScreenOnAfterBoot()`；不需要亮屏则删 `wakeScreen()`
- [ ] 设置页添加开关 + 确认对话框 + `requestNeededPermissions()`
- [ ] 设置页添加悬浮窗/电池优化按钮，onResume 刷新状态
- [ ] strings.xml 添加全部文案，替换应用名
- [ ] prefs 文件名与 key 在 Receiver 和设置页两处**完全一致**（重点核对）
- [ ] ADB 模拟广播测试通过
- [ ] 真机重启测试通过（至少 Android 10+ 一台）
- [ ] 厂商 ROM 自启动提示文案已展示

---

## 14. 常见问题排查

| 现象 | 原因 | 解决 |
|---|---|---|
| 重启后无反应 | 厂商 ROM 未允许自启动 | 设置页提示用户开启 ROM 自启动（第 11 节） |
| 重启后无反应（原生系统） | 应用被"强制停止"过或从未启动过（Android 3.1+ 停止状态收不到广播） | 安装/更新后至少手动启动一次 |
| 服务启动即崩溃 | 5 秒内未调 `startForeground` | 检查 `onStartCommand` 所有分支是否都调用了 |
| `BadTokenException` / addView 抛异常 | Android 8+ 用了 `TYPE_PHONE`，或悬浮窗权限实际未授予 | 保留 `TYPE_APPLICATION_OVERLAY` 版本分支；`show()` 内 try-catch |
| Activity 没有出现在前台 | 悬浮窗未成功显示就启动了 Activity；或 OEM 拦截 | 确认 `show()` 返回 true 后再 `launchMainActivity()`；延长悬浮窗移除延时 |
| Android 13+ 无通知 | `POST_NOTIFICATIONS` 未授权 | 设置页运行时申请 |
| Android 14 崩溃 `MissingForegroundServiceTypeException` | service 未声明 `foregroundServiceType` 或缺配套权限 | Manifest 补 `foregroundServiceType="dataSync"` + `FOREGROUND_SERVICE_DATA_SYNC` 权限 |
| 开关状态不生效 | Receiver 与设置页 prefs 文件名/key 不一致 | 统一抽到常量类 |
| 屏幕没点亮 | 未加 WakeLock 或未设置 Activity flags | 核对第 5、6 步（仅锁屏类应用需要） |

---

## 附：关键设计决策速记（为什么这么做）

1. **为什么用前台服务中转？** Android 10+ 禁止后台启动 Activity，但前台服务有更高优先级，且可以展示通知兜底。
2. **为什么需要 1×1 悬浮窗？** 悬浮窗让应用获得"可见窗口"，满足系统对后台 Activity 启动的可见性豁免——这是不依赖系统签名/白名单的唯一通用手段。
3. **为什么默认关闭、开启需确认？** 自启动涉及多项敏感权限（悬浮窗、电池优化白名单），必须让用户知情同意。
4. **为什么建议关闭电池优化？** 部分 ROM 会延迟或丢弃处于电池优化中的应用的开机广播。
5. **为什么 START_NOT_STICKY？** 启动是幂等的一次性动作，系统重启重建服务没有意义。
6. **为什么 1 秒后才移除悬浮窗？** Activity 启动是异步的，窗口令牌校验发生在启动过程中，立即移除可能导致启动失败。
