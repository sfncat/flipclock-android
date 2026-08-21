# FlipClock 天气功能设计文档

## 1. 需求与目标

本设计把天气功能拆成两个独立部分：

1. **独立天气库（`:weather`）**：
   - 按用户指定地点获取天气；
   - 采用免费、无需 API Key 的公开服务；
   - 不申请定位权限，由用户手动输入地点并做匹配；
   - 支持每 `N` 小时刷新一次，`N` 可配置；
   - 作为独立 Android Library 模块输出，方便其他应用复用。

2. **FlipClock 集成层**：
   - 开启防烧屏保护后，当信息栏与时间层距离达到最远时，在信息栏与时间层之间的空白区显示天气；
   - 显示内容包含温度与天气状况（如晴/雨）；
   - 使用现有 `flipclock_cjk.ttf` 字体；
   - 停留 `2` 秒（可配置），之后关闭天气显示并继续原有防烧屏移动。

## 2. 整体架构

```
┌─────────────────────────────────────────────────────────────────────┐
│                         其他 Android 应用                            │
│   引入 :weather aar                                                 │
│        │                                                            │
│        ▼                                                            │
│   WeatherManager.setLocation("北京")                                │
│   WeatherManager.setUpdateIntervalHours(3)                          │
│   WeatherManager.setWeatherListener { ... }                         │
└─────────────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────────────┐
│                    FlipClock app 模块                                │
│   SettingsActivity ──► SharedPreferences                             │
│        │                                                            │
│        ▼                                                            │
│   WeatherManager (封装 :weather，运行在后台线程)                      │
│        │                                                            │
│        ▼                                                            │
│   WeatherBridge.nativeUpdateWeather(...)  ──► C 层                  │
│        │                                                            │
│        ▼                                                            │
│   weather_overlay.c ──► SDL_RenderCopy 在信息栏与时间层之间绘制天气  │
└─────────────────────────────────────────────────────────────────────┘
```

## 3. 第一部分：独立天气库 `:weather`

### 3.1 模块结构

新增 Gradle 模块 `weather/`，产物为 `weather-release.aar`。

```
weather/
├── build.gradle
├── consumer-rules.pro
├── src/main/AndroidManifest.xml
└── src/main/java/one/alynx/flipclock/weather/
    ├── WeatherInfo.java
    ├── GeoLocation.java
    ├── WeatherProvider.java
    ├── OpenMeteoWeatherProvider.java
    ├── WeatherManager.java
    ├── WeatherListener.java
    └── WeatherException.java
```

项目根目录 `settings.gradle` 追加：

```groovy
include ':app', ':weather'
```

### 3.2 数据源：Open-Meteo

采用 [Open-Meteo](https://open-meteo.com/)：

- 免费、无需注册、无需 API Key；
- 支持地点搜索（Geocoding API）；
- 支持当前天气（Forecast API）；
- 返回 JSON，国内可访问。

接口示例：

- 地点搜索：
  ```
  https://geocoding-api.open-meteo.com/v1/search?name=北京&count=5&language=zh&format=json
  ```
- 当前天气：
  ```
  https://api.open-meteo.com/v1/forecast?latitude=39.9075&longitude=116.3972&current_weather=true
  ```

天气代码（`weathercode`）按 [Open-Meteo WMO Weather interpretation codes](https://open-meteo.com/en/docs) 映射为中文简短描述。完整映射如下：

| code | 英文含义 | 中文描述 |
|------|----------|----------|
| 0 | Clear sky | 晴 |
| 1 | Mainly clear | 少云 |
| 2 | Partly cloudy | 多云 |
| 3 | Overcast | 阴 |
| 45 | Fog | 雾 |
| 48 | Depositing rime fog | 雾凇 |
| 51 | Drizzle: light | 小雨 |
| 53 | Drizzle: moderate | 中雨 |
| 55 | Drizzle: dense | 大雨 |
| 56 | Freezing drizzle: light | 冻雨 |
| 57 | Freezing drizzle: dense | 冻雨 |
| 61 | Rain: slight | 小雨 |
| 63 | Rain: moderate | 中雨 |
| 65 | Rain: heavy | 大雨 |
| 66 | Freezing rain: light | 冻雨 |
| 67 | Freezing rain: heavy | 冻雨 |
| 71 | Snow fall: slight | 小雪 |
| 73 | Snow fall: moderate | 中雪 |
| 75 | Snow fall: heavy | 大雪 |
| 77 | Snow grains | 雪粒 |
| 80 | Rain showers: slight | 阵雨 |
| 81 | Rain showers: moderate | 阵雨 |
| 82 | Rain showers: violent | 暴雨 |
| 85 | Snow showers: slight | 阵雪 |
| 86 | Snow showers: heavy | 阵雪 |
| 95 | Thunderstorm: slight/moderate | 雷阵雨 |
| 96 | Thunderstorm with slight hail | 雷暴伴冰雹 |
| 99 | Thunderstorm with heavy hail | 雷暴伴冰雹 |

说明：

- **雨可以区分强度**：`51-67` 区分了毛毛雨/雨、轻/中/重强度；`80-82` 区分了阵雨和暴雨。
- **当前天气 API 只返回一个主导天气代码**，不会返回“多云转雨”“雷+雨”这类组合状态。比如 code `95` 是“雷阵雨”，已经是一个综合状态；`96/99` 表示“雷暴伴冰雹”。
- 如果需要展示“多云转雨”“明天有雨”这类趋势信息，需要调用 Open-Meteo 的 hourly/daily `weathercode` 预报数据，而不是只用 `current_weather=true`。当前设计第一版只显示当前天气，后续可在 `WeatherProvider` 中扩展 `fetchForecast()` 获取逐小时/逐日天气。

### 3.3 数据类

```java
// GeoLocation.java
public final class GeoLocation {
    private final String name;        // 匹配后的地点名，如"北京市"
    private final double latitude;
    private final double longitude;
    private final String country;
    private final String admin1;
    // constructor + getters
}

// WeatherInfo.java
public final class WeatherInfo {
    private final GeoLocation location;
    private final int temperature;      // 摄氏度，整数
    private final int weatherCode;
    private final String description;     // 如"晴"
    private final long timestamp;       // 获取时间（毫秒）
    // constructor + getters
}
```

### 3.4 对外接口

```java
// WeatherProvider.java
public interface WeatherProvider {
    List<GeoLocation> searchLocations(String name, String language)
            throws WeatherException;
    WeatherInfo fetchWeather(GeoLocation location) throws WeatherException;
}

// WeatherManager.java
public final class WeatherManager {
    public WeatherManager(Context context)
    public WeatherManager(Context context, String prefsName)
    public void setUpdateIntervalHours(int hours)
    public int getUpdateIntervalHours()
    public void setWeatherListener(WeatherListener listener)
    public void setLocation(String name)
    public void setLocation(GeoLocation location)
    public void refreshNow()
    public void start()
    public void stop()
    public WeatherInfo getLastWeatherInfo()
    public GeoLocation getLastLocation()
}

// WeatherListener.java
public interface WeatherListener {
    void onWeatherUpdated(WeatherInfo info);
    void onError(Throwable error);
}
```

### 3.5 行为说明

- **手动地点匹配**：用户输入"北京"，`WeatherManager` 调用 `searchLocations` 得到候选列表，默认取第一条作为匹配结果；后续版本可在设置页展示候选列表供用户确认。
- **坐标缓存**：匹配成功后把 `GeoLocation` 持久化到 `SharedPreferences`（仅本应用私有），后续按坐标拉取天气，不再重复地理编码。
- **定时刷新**：`start()` 后以后台线程/协程按 `updateIntervalHours` 周期调用 `fetchWeather`；调用 `stop()` 后停止。
- **失败处理**：网络失败时保留上一次有效结果，并通过 `WeatherListener.onError` 通知上层；连续失败超过 3 次后改为按指数退避重试。
- **无新增敏感权限**：仅需普通 `INTERNET` 权限，不需要 `ACCESS_FINE_LOCATION` / `ACCESS_COARSE_LOCATION`。

### 3.6 其他应用复用示例

```java
WeatherManager manager = new WeatherManager(context);
manager.setUpdateIntervalHours(3);
manager.setWeatherListener(new WeatherListener() {
    @Override
    public void onWeatherUpdated(WeatherInfo info) {
        Log.d("Weather", info.getLocation().getName() + " "
                + info.getTemperature() + "°C " + info.getDescription());
    }
    @Override
    public void onError(Throwable error) { }
});
manager.setLocation("上海");
manager.start();
```

## 4. 第二部分：FlipClock 集成层

### 4.1 新增设置项

在 `SettingsActivity` 的「主界面显示内容」区域新增天气区块：

| `SharedPreferences` key | 类型 | 默认 | 说明 |
|-------------------------|------|------|------|
| `show_weather` | bool | `false` | 是否显示天气 |
| `weather_location` | string | `""` | 用户输入的地点名，如"北京" |
| `weather_update_interval_hours` | int | `3` | 天气刷新间隔（1–24 小时） |
| `weather_display_duration_ms` | int | `2000` | 防烧屏最远点停留并显示天气的时长（1000–10000 毫秒） |

UI 设计（`activity_settings.xml`）：

- 一个 Switch「显示天气」；
- 一个 EditText 输入地点（Switch 开启后可用）；
- 一个 SeekBar 调整刷新间隔（1–24 小时）；
- 一个 SeekBar 调整显示时长（1–10 秒）；
- 底部沿用 `info_bar_restart_hint`，说明「修改后重启 FlipClock 生效」。

### 4.2 Java-C 桥接

#### 4.2.1 配置传递（`flipclock.conf`）

`MainActivity.writeNativeConf()` 追加：

```ini
show_weather=false
weather_location=北京
weather_update_interval_hours=3
weather_display_duration_ms=2000
```

C 层在 `flipclock.h` 的 `struct flipclock` 中新增对应字段，并在 `_flipclock_apply_key_value()` 中解析。

#### 4.2.2 天气数据传递（JNI）

新增 `WeatherBridge.java`：

```java
package one.alynx.flipclock;

public class WeatherBridge {
    public static native void nativeUpdateWeather(
            String location, String temperature, String description);
}
```

在 `app/jni/flipclock/srcs/` 新增 `weather_jni.c` 注册该 JNI 方法，把字符串拷贝到 `struct flipclock` 的 `weather_*` 字段（用 `SDL_LockMutex` 保护，避免与 SDL 主循环竞争）。

### 4.3 原生层天气组件

新增 `weather_overlay.h/c`：

```c
struct flipclock_weather_overlay;

struct flipclock_weather_overlay *
flipclock_weather_overlay_create(struct flipclock *app, SDL_Renderer *renderer);
void flipclock_weather_overlay_set_text(
        struct flipclock_weather_overlay *overlay,
        const char *temperature, const char *description);
void flipclock_weather_overlay_draw(
        struct flipclock_weather_overlay *overlay, SDL_Rect rect);
void flipclock_weather_overlay_destroy(struct flipclock_weather_overlay *overlay);
```

实现要点：

- 使用 `app->cjk_font_path`（即 `flipclock_cjk.ttf`）打开字体；
- 字号按绘制区域短边的 `60%` 计算，下限 `12px`；
- 将温度与天气状况拼接为单行文本渲染，例如 `24°C 晴`；
- 在传入的 `rect`（信息栏与时间层之间的空白区）内水平、垂直居中；
- 背景不绘制遮罩，直接以文字颜色渲染；
- 纹理在文本变更或绘制区域大小变化时重建。

### 4.4 触发逻辑：与防烧屏协同

#### 4.4.1 当前防烧屏行为

`clock.c` 中：

```c
Uint32 ticks = SDL_GetTicks();
double phase = 2.0 * M_PI * (double)(ticks % BURN_IN_PERIOD_MS) /
               (double)BURN_IN_PERIOD_MS;
int value = (int)(amplitude * sin(phase));
```

信息栏偏移 `+value`，卡片组偏移 `-value`，两者距离 `2 * |value|`。

#### 4.4.2 新增天气触发状态机

在 `clock.c` 中扩展 `_flipclock_clock_get_burn_in_offset()`：

- 当 `app->show_weather` 为真时，在偏移到达峰值区（`|value| >= amplitude * 0.95`）触发天气显示；
- 触发后进入 `WEATHER_HOLD` 状态，把偏移 clamp 到 `±amplitude`（保持信息栏与时间层距离最远）；
- 在 `WEATHER_HOLD` 状态下持续 `weather_display_duration_ms`（默认 2000 ms）；
- 时间到后退出 `WEATHER_HOLD`，继续正弦动画。

状态定义（C 伪代码）：

```c
enum burn_in_state {
    BURN_IN_MOVING,      // 正常防烧屏移动
    BURN_IN_HOLD_WEATHER // 在最远点停留并显示天气
};
```

每个 `struct flipclock_clock` 增加：

```c
SDL_Rect weather_rect;    // 信息栏与时间层之间的绘制区域
enum burn_in_state burn_in_state;
Uint32 burn_in_hold_start_ticks;
int burn_in_peak_sign;    // +1 / -1，当前 HOLD 的峰值方向
int burn_in_last_peak_sign; // 上一次触发 HOLD 的峰值方向，用于避免连续触发
```

#### 4.4.3 状态转换示例

```
正弦 value:  0 → +A（触发 HOLD） → +A（持续 2s） → +A → 0 → -A（触发 HOLD） → -A（持续 2s） → -A → 0
状态:        MOVING   HOLD_WEATHER   HOLD_WEATHER   MOVING   MOVING   HOLD_WEATHER   HOLD_WEATHER   MOVING
显示天气:              是                            否                是
```

由于每次 HOLD 增加约 2 秒，完整周期从 60 秒变为约 64 秒，对肉眼无感知。

### 4.5 渲染顺序

`flipclock_clock_animate()` 调整绘制顺序为：

1. 清屏；
2. 绘制信息栏（带 burn-in offset）；
3. 绘制时间卡片（带反向 burn-in offset）；
4. 若当前处于 `BURN_IN_HOLD_WEATHER` 状态，在信息栏与时间层之间的 `weather_rect` 内绘制天气 overlay；
5. `SDL_RenderPresent()`。

### 4.6 原生数据结构改动

#### 4.6.1 `struct flipclock`（flipclock.h）

新增：

```c
bool show_weather;
char weather_location[MAX_BUFFER_LENGTH];
int weather_update_interval_hours;
int weather_display_duration_ms;

// 当前天气数据（由 JNI 写入，mutex 保护）
SDL_mutex *weather_mutex;
char weather_location_text[MAX_BUFFER_LENGTH];
char weather_temperature_text[32];
char weather_description_text[32];
bool weather_text_dirty;
```

#### 4.6.2 `struct flipclock_clock`（clock.h）

新增：

```c
enum burn_in_state burn_in_state;
Uint32 burn_in_hold_start_ticks;
bool weather_peak_sign;
struct flipclock_weather_overlay *weather_overlay;
```

### 4.7 配置解析（flipclock.c）

在 `_flipclock_apply_key_value()` 中追加：

```c
} else if (!strcmp(key, "show_weather")) {
    app->show_weather = (!strcmp(value, "true"));
} else if (!strcmp(key, "weather_location")) {
    strncpy(app->weather_location, value, MAX_BUFFER_LENGTH);
    app->weather_location[MAX_BUFFER_LENGTH - 1] = '\0';
} else if (!strcmp(key, "weather_update_interval_hours")) {
    app->weather_update_interval_hours = atoi(value);
    if (app->weather_update_interval_hours < 1)
        app->weather_update_interval_hours = 1;
    if (app->weather_update_interval_hours > 24)
        app->weather_update_interval_hours = 24;
} else if (!strcmp(key, "weather_display_duration_ms")) {
    app->weather_display_duration_ms = atoi(value);
    if (app->weather_display_duration_ms < 1000)
        app->weather_display_duration_ms = 1000;
    if (app->weather_display_duration_ms > 10000)
        app->weather_display_duration_ms = 10000;
}
```

默认值：

```c
app->show_weather = false;
app->weather_location[0] = '\0';
app->weather_update_interval_hours = 3;
app->weather_display_duration_ms = 2000;
app->weather_mutex = SDL_CreateMutex();
app->weather_location_text[0] = '\0';
app->weather_temperature_text[0] = '\0';
app->weather_description_text[0] = '\0';
app->weather_text_dirty = false;
```

## 5. 交互流程

1. 用户在设置页开启「显示天气」，输入「北京」，设置 3 小时刷新、显示 2 秒；
2. `SettingsActivity` 保存到 `SharedPreferences`；
3. 用户回到主界面（或重启 FlipClock），`MainActivity.onCreate()` 写入最新 `flipclock.conf`；
4. C 层启动后读取配置，创建 `weather_overlay`；
5. `WeatherManager` 根据保存的地点名调用 Open-Meteo 地理编码，匹配到坐标；
6. `WeatherManager` 拉取当前天气，通过 `WeatherBridge.nativeUpdateWeather(...)` 写入 C 层；
7. 防烧屏动画运行，每到最远点时 clamp 2 秒并绘制天气；
8. 2 秒后恢复动画，等待下一个峰值再次显示天气；
9. 每 3 小时刷新一次天气数据。

## 6. 文件变更清单

### 6.1 新增文件

| 路径 | 说明 |
|------|------|
| `weather/build.gradle` | 天气库模块构建脚本 |
| `weather/consumer-rules.pro` | ProGuard 空规则 |
| `weather/src/main/AndroidManifest.xml` | 声明 `INTERNET` 权限 |
| `weather/src/main/java/.../weather/WeatherInfo.java` | 天气数据类 |
| `weather/src/main/java/.../weather/GeoLocation.java` | 地点数据类 |
| `weather/src/main/java/.../weather/WeatherProvider.java` | 天气源接口 |
| `weather/src/main/java/.../weather/OpenMeteoWeatherProvider.java` | Open-Meteo 实现 |
| `weather/src/main/java/.../weather/WeatherManager.java` | 定时刷新、缓存、地点匹配 |
| `weather/src/main/java/.../weather/WeatherListener.java` | 天气更新监听接口 |
| `weather/src/main/java/.../weather/WeatherException.java` | 异常定义 |
| `app/src/main/java/one/alynx/flipclock/WeatherBridge.java` | JNI 桥接类 |
| `app/jni/flipclock/srcs/weather_jni.c` | JNI 方法实现 |
| `app/jni/flipclock/srcs/weather_overlay.h` | 天气覆盖层头文件 |
| `app/jni/flipclock/srcs/weather_overlay.c` | 天气覆盖层绘制实现 |

### 6.2 修改文件

| 路径 | 修改内容 |
|------|----------|
| `settings.gradle` | include ':weather' |
| `app/build.gradle` | `implementation project(':weather')` |
| `app/jni/flipclock/Android.mk` | 追加 `weather_jni.c` 和 `weather_overlay.c` |
| `app/jni/flipclock/srcs/flipclock.h` | 新增天气配置与数据字段 |
| `app/jni/flipclock/srcs/flipclock.c` | 默认值、解析、创建/销毁 mutex |
| `app/jni/flipclock/srcs/clock.h` | 新增 burn-in 状态与 weather_overlay 指针 |
| `app/jni/flipclock/srcs/clock.c` | 峰值 HOLD 逻辑、绘制天气、创建/销毁 overlay |
| `app/src/main/java/one/alynx/flipclock/MainActivity.java` | 写入新增配置项；启动 WeatherManager |
| `app/src/main/java/one/alynx/flipclock/SettingsActivity.java` | 新增天气设置读写 |
| `app/src/main/res/layout/activity_settings.xml` | 新增天气设置 UI |
| `app/src/main/res/values/strings.xml` | 新增天气相关文案 |
| `app/src/main/AndroidManifest.xml` | 追加 `INTERNET` 权限 |
| `README.md` / `README_CN.md` | 更新功能说明 |

## 7. 边界与异常

| 场景 | 处理策略 |
|------|----------|
| 用户未开启天气 | `show_weather=false`，防烧屏保持现有行为，不创建 weather overlay |
| 地点匹配失败 | `WeatherManager` 回调错误，保留旧数据；C 层显示旧数据或空数据 |
| 网络不可用 | 保留缓存，定时到后重试；连续失败按指数退避 |
| 天气字体缺失 | 与信息栏一致：字体打开失败时 `weather_overlay.enabled=false`，不崩溃 |
| 屏幕旋转 | 窗口 size changed 后重新布局，`weather_rect` 与字号按新方向重新计算，文本缓存复用 |
| 用户关闭防烧屏但开启天气 | 天气功能依赖防烧屏触发，此时不显示天气；设置页可提示「需同时开启防烧屏保护」 |
| 刷新间隔设为零或负值 | 解析时钳制到 1 小时；UI 下限同样为 1 |
| 显示时长超出范围 | 解析时钳制到 1000–10000 毫秒 |

## 8. 权限影响

仅新增普通网络权限：

```xml
<uses-permission android:name="android.permission.INTERNET" />
```

不申请定位权限；用户主动输入地点即可。

## 9. 测试建议

1. **独立模块**：在 `:weather` 中写单元测试验证 JSON 解析、WMO code 中文映射、地理编码坐标缓存。
2. **地点匹配**：输入「北京」「上海市」「Guangzhou」，验证返回正确坐标。
3. **刷新周期**：把间隔设为 1 小时，观察 `WeatherManager` 是否按周期请求。
4. **防烧屏峰值**：在 `clock.c` 中临时把 `BURN_IN_PERIOD_MS` 设为 5000，验证每 2.5 秒左右触发一次天气显示并停留 2 秒。
5. **字体与布局**：横屏/竖屏下天气文字均居中，字号随屏幕短边变化。
6. **回归**：关闭天气、关闭防烧屏后，界面与现有主线完全一致。
7. **真机熄屏/自启动**：从 `BootAutoStartService` 启动后，天气模块应能按保存配置运行。
