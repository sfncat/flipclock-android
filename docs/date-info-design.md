# FlipClock Android 日期 / 星期 / 农历显示设计文档

## 1. 需求背景

当前 FlipClock 主界面仅显示由 `flipclock.c` / `clock.c` / `card.c` 绘制的时/分/秒翻页卡片，缺少日期上下文。用户希望在主界面上同时显示：

- **公历日期**（如 `2026-08-04`）
- **星期**（如 `星期二`）
- **农历日期**（如 `丙午年 六月廿一`）

并且要求：

- **横屏**：日期、星期、农历显示在主界面 **上方**，字体比时间小一些；时间整体向下移动一点，避免顶端过于拥挤。
- **竖屏**：日期、星期、农历显示在主界面 **左侧**（每项一行，竖向堆叠）；时间整体向右移动一点，让左侧信息区有独立空间。
- 显示与否、显示哪些项（日期 / 星期 / 农历），需要在 [`SettingsActivity`](file:///home/kali/workspace/flipclock-android/app/src/main/java/one/alynx/flipclock/SettingsActivity.java) 中提供开关。
- 不破坏现有 SDL2 / Java shim 结构，与 [`docs/auto-start-design.md`](file:///home/kali/workspace/flipclock-android/docs/auto-start-design.md) 中已实现的开机自启、四指手势入口保持一致。

## 2. 目标

- 在原生 C 代码（SDL2 渲染层）中新增日期、星期、农历三行信息的绘制。
- 布局随窗口方向（横 / 竖）自动切换：横屏顶部单行、竖屏左侧竖排。
- 通过 `flipclock.conf` 风格的配置项 + Android `SharedPreferences` 双通道下发，用户在设置页调整后重启应用生效（第一版可先不做热更新）。
- 农历采用离线算法计算，不引入网络依赖。

## 3. 方案概述

在 C 层新增一个「信息栏」渲染模块 `info_bar`，与 `clock` 并列，由主循环每分钟刷新一次；在布局阶段根据窗口方向调整信息栏位置和时钟卡片位置。Java 层通过读取 `SharedPreferences` 后写入应用私有的 `flipclock.conf`（或通过 SDL 命令行参数）传递用户选项。

| 组件 | 职责 |
|------|------|
| `info_bar.[ch]` | 新增文件。负责日期、星期、农历文本的准备、字体加载、位置计算和 SDL 渲染。 |
| `lunar.[ch]` | 新增文件。农历换算算法（1900–2100 常用范围），输出中文农历字符串。 |
| `clock.c` | 修改 `_flipclock_clock_update_layout()`：预留信息栏空间；创建 / 销毁 `info_bar` 实例；在 `flipclock_clock_animate()` 中调用信息栏绘制。 |
| `flipclock.h` / `flipclock.c` | 新增配置字段 `show_date`、`show_weekday`、`show_lunar`、`info_scale`、`cjk_font_path`，并解析对应配置键。 |
| Android 端 `SettingsActivity` | 新增三个开关（日期 / 星期 / 农历），保存到 `SharedPreferences`；应用启动时写入应用私有目录下的 `flipclock.conf`。 |
| 中文字体 | 现有 `flipclock.ttf` 仅含数字/字母。新增一个 CJK 字体（如开源思源黑体子集或 Noto Sans CJK SC 子集），放在 `app/src/main/assets/flipclock_cjk.ttf`，供 `info_bar` 使用。 |

## 4. C 层数据结构改动

### 4.1 `struct flipclock` 新增字段

在 [`flipclock.h`](file:///home/kali/workspace/flipclock-android/app/jni/flipclock/srcs/flipclock.h#L63-L89) 的 `struct flipclock` 中新增：

```c
bool show_date;      // 是否显示公历日期
bool show_weekday;   // 是否显示星期
bool show_lunar;     // 是否显示农历
double info_scale;   // 信息栏文字缩放，默认 1.0
char cjk_font_path[MAX_BUFFER_LENGTH]; // CJK 字体路径
```

默认值：`show_date = show_weekday = show_lunar = true`（Android 端默认开启，可在设置页关闭）；`info_scale = 1.0`；`cjk_font_path` 默认 `"flipclock_cjk.ttf"`（Android assets 根目录）。

### 4.2 `struct flipclock_info_bar`

在新增的 [`info_bar.h`] 中定义：

```c
struct flipclock_info_bar {
    struct flipclock *app;
    SDL_Renderer *renderer;
    TTF_Font *font;      // 使用 CJK 字体
    SDL_Rect rect;       // 信息栏整体区域（相对窗口）
    bool horizontal;     // true=横屏（一行三段）；false=竖屏（三行竖排）
    char date_text[64];
    char weekday_text[32];
    char lunar_text[64];
    int last_yday;       // 上次刷新的年内天数，用于判定是否需要重新计算
};
```

对外接口：

```c
struct flipclock_info_bar *flipclock_info_bar_create(struct flipclock *app,
                                                     SDL_Renderer *renderer);
void flipclock_info_bar_set_rect(struct flipclock_info_bar *bar,
                                 SDL_Rect rect, bool horizontal);
void flipclock_info_bar_refresh(struct flipclock_info_bar *bar,
                                const struct tm *now); // 更新文本
void flipclock_info_bar_draw(struct flipclock_info_bar *bar);
void flipclock_info_bar_destroy(struct flipclock_info_bar *bar);
```

### 4.3 `struct flipclock_clock` 改动

在 [`clock.h`](file:///home/kali/workspace/flipclock-android/app/jni/flipclock/srcs/clock.h#L8-L19) 的 `struct flipclock_clock` 中新增：

```c
struct flipclock_info_bar *info_bar; // 可为 NULL，表示所有选项均关闭
```

## 5. 布局设计

信息栏是否显示由 `app->show_date || app->show_weekday || app->show_lunar` 决定。以下讨论均基于「至少显示一项」的情况。

### 5.1 通用参数

- 信息栏高度占窗口短边的比例：**INFO_RATIO = 0.10**（可通过 `info_scale` 微调）。
- 信息栏与时钟卡片之间预留一个 `space_size`（沿用现有布局中的间距计算）。
- 文本颜色沿用 `app->text_color`，背景为 `app->background_color`。

### 5.2 横屏布局（`clock->w >= clock->h`）

参考 [`_flipclock_clock_update_layout()`](file:///home/kali/workspace/flipclock-android/app/jni/flipclock/srcs/clock.c#L15-L93) 现有逻辑：

- **信息栏矩形**：
  - `x = 0`
  - `y = space_size`
  - `w = clock->w`
  - `h = clock->h * INFO_RATIO`
- 信息栏内部横向排列三段：`日期 | 星期 | 农历`，之间用 2 个空格间隔，整体水平居中。
- **时钟卡片区域**：将当前 `hour_rect.y` 从 `(clock->h - card_size) / 2` 修改为：

  ```c
  int info_h = info_bar_visible ? clock->h * INFO_RATIO + space_size : 0;
  hour_rect.y = info_h + ((clock->h - info_h) - card_size) / 2;
  ```

  这样时间整体向下移动一点，视觉上时钟仍保持在剩余区域的垂直中心。
- `card_size` 的可用高度上限由 `clock->h - info_h` 决定，即：

  ```c
  int min_height = (clock->h - info_h) * 0.8;
  ```

### 5.3 竖屏布局（`clock->w < clock->h`）

- **信息栏矩形**：
  - `x = space_size`
  - `y = 0`
  - `w = clock->w * INFO_RATIO`（作为左侧竖排文字列的宽度）
  - `h = clock->h`
- 信息栏内部三行竖向堆叠，整体在竖向上居中：
  - 第 1 行：日期（如 `2026`、`08-04` 分两小行，或单行）
  - 第 2 行：星期
  - 第 3 行：农历
- 最初版本采用「横向文字，多行堆叠」：文字水平书写，每一项独占一行；整体信息栏作为一个左侧竖条区域。
- 后续已实现**传统竖排文字**（字符自上而下逐字排列，汉字直立、数字旋转 90°）并作为竖屏默认排版：配置键 `info_vertical` 默认开启，关闭后回退为横向堆叠，详见 [`vertical-typography-design.md`](vertical-typography-design.md)。
- **时钟卡片区域**：
  - `int info_w = info_bar_visible ? clock->w * INFO_RATIO + space_size : 0;`
  - `hour_rect.x = info_w + ((clock->w - info_w) - card_size) / 2;`
  - `min_width = (clock->w - info_w) * 0.8;`
  - 结果是时间整体向右移动一点。

### 5.4 字体大小

- 横屏信息栏文字高度 = `clock->h * INFO_RATIO * 0.6 * info_scale`
- 竖屏信息栏文字高度 = `clock->w * INFO_RATIO * 0.6 * info_scale`
- 用 `TTF_OpenFont(cjk_font_path, pixel_size)` 打开，`_draw_text` 需要改写以支持真实 UTF-8 字符（现有 [`_draw_text()`](file:///home/kali/workspace/flipclock-android/app/jni/flipclock/srcs/card.c#L192-L237) 一次画一个 ASCII 字符，无法直接用于中文）。信息栏改用 `TTF_RenderUTF8_Blended` 一次渲染整行文本。

## 6. 农历算法

在 `lunar.c` 中实现 1900–2100 年公农历互转，使用广泛流传的 24-bit 打包表（每年一个 int，编码闰月、月份大小、正月初一对应公历日期）。

对外接口：

```c
// 输入公历 year/month/day，输出中文农历字符串（UTF-8），例如：
// "丙午年 六月廿一"
// buf_size 建议 >= 64
void lunar_from_gregorian(int year, int month, int day,
                          char *buf, size_t buf_size);
```

内部：

- `static const uint32_t LUNAR_INFO[]`：年份信息表。
- `static const char *TIAN_GAN[] = {"甲","乙",...};`
- `static const char *DI_ZHI[]  = {"子","丑",...};`
- `static const char *LUNAR_MONTHS[] = {"正","二",...,"腊"};`
- `static const char *LUNAR_DAYS[]   = {"初一","初二",...,"三十"};`

若输入年份超出范围，返回空字符串。

## 7. 主循环刷新

在 [`flipclock_run_mainloop()`](file:///home/kali/workspace/flipclock-android/app/jni/flipclock/srcs/flipclock.c#L800-L836) 中：

- 每次进入主循环、以及 `app->now.tm_mday != past.tm_mday` 时，对每个 clock 调用 `flipclock_info_bar_refresh(clock->info_bar, &app->now)`，重新生成三行文本。
- 每帧渲染时（在 `flipclock_clock_animate` 内），如果 `clock->info_bar != NULL`，调用 `flipclock_info_bar_draw()`，绘制顺序：**先绘制信息栏，再绘制时钟卡片**（时钟卡片使用 `_flipclock_card_draw_rounded_box` 会填充其自身区域，两者不重叠）。

## 8. 配置项与设置页

### 8.1 新增配置键（`flipclock.conf`）

由 [`_flipclock_apply_key_value()`](file:///home/kali/workspace/flipclock-android/app/jni/flipclock/srcs/flipclock.c#L280-L362) 扩展识别：

| 键 | 类型 | 默认 | 说明 |
|----|------|------|------|
| `show_date` | bool | `true` | 是否显示公历日期 |
| `show_weekday` | bool | `true` | 是否显示星期 |
| `show_lunar` | bool | `true` | 是否显示农历 |
| `info_scale` | double | `1.0` | 信息栏文字缩放 |
| `cjk_font` | string | `flipclock_cjk.ttf` | CJK 字体路径 |

### 8.2 Android 层桥接

Android 目前没有默认的 `flipclock.conf` 生成流程；本次新增：

1. 在 [`SettingsActivity`](file:///home/kali/workspace/flipclock-android/app/src/main/java/one/alynx/flipclock/SettingsActivity.java) 中新增三个 `Switch`（`show_date_switch`、`show_weekday_switch`、`show_lunar_switch`），把状态保存到 `SharedPreferences` `flipclock_settings` 中，键为 `show_date` / `show_weekday` / `show_lunar`。
2. 在 [`MainActivity`](file:///home/kali/workspace/flipclock-android/app/src/main/java/one/alynx/flipclock/MainActivity.java) 的 `onCreate` 中（`super.onCreate` 之前）读取 `SharedPreferences`，将当前值写入 `getFilesDir()/flipclock.conf`。
3. 修改 `_flipclock_open_conf_*` 系列函数：为 Android 增加一个入口 `_flipclock_open_conf_android()`，通过 SDL 提供的 `SDL_AndroidGetInternalStoragePath()` 打开该文件。
4. 由于必须在 SDL 初始化之前配置 `flipclock.conf` 路径，一种简单方案是通过环境变量 `FLIPCLOCK_CONF_PATH` 传递（Java 层在启动 native 之前 `setenv`），C 层 `flipclock_load_conf` 优先读取该变量。

`SettingsActivity` UI 增加内容（在自启动区块下方）：

```xml
<TextView 标题：显示内容 />
<Switch id="@+id/show_date_switch" 文本="显示日期" />
<Switch id="@+id/show_weekday_switch" 文本="显示星期" />
<Switch id="@+id/show_lunar_switch" 文本="显示农历" />
<TextView 说明：更改后重启 FlipClock 生效 />
```

用户改动这三个开关无需申请系统权限；仅需在退出设置页时写回 `flipclock.conf`，并提示 **需重启 FlipClock 才能生效**（第一版不做热更新；后续可通过 SDL 自定义事件通知 native 重新读取配置）。

## 9. 权限影响

无新增系统权限。所有信息栏功能属于纯本地渲染，不涉及网络、位置、通讯录等敏感数据。

## 10. 兼容性与限制

| 场景 | 处理策略 |
|------|----------|
| 现有用户未开启任何信息栏开关 | 布局与现有行为完全一致，时钟保持在窗口正中。 |
| 只开启 1~2 项 | 横屏时缺失项占用的位置由左右两项自动居中吸收；竖屏时空行被跳过，剩余行垂直居中。 |
| 农历年份超出 1900–2100 | 返回空字符串，`show_lunar` 视为该行不显示。 |
| CJK 字体缺失 | 打开失败时打印 `LOG_ERROR` 并回退为不绘制信息栏，避免整个 SDL 崩溃。 |
| 现有 Windows / Linux 平台 | 由于 `cjk_font_path` 默认在 Android assets，桌面平台需要在 `flipclock.conf` 中显式提供 CJK 字体路径；缺省不显示信息栏，不影响现有行为。 |

## 11. 实现清单

- [x] 新增 `app/jni/flipclock/srcs/info_bar.h` / `info_bar.c`
- [x] 新增 `app/jni/flipclock/srcs/lunar.h` / `lunar.c`（包含 1900–2100 农历表 + 换算）
- [x] 修改 `app/jni/flipclock/srcs/flipclock.h`：新增 `show_date` / `show_weekday` / `show_lunar` / `info_scale` / `cjk_font_path` 字段
- [x] 修改 `app/jni/flipclock/srcs/flipclock.c`：默认值、`_flipclock_apply_key_value` 新键、Android 配置文件路径处理
- [x] 修改 `app/jni/flipclock/srcs/clock.h` / `clock.c`：`struct flipclock_clock` 添加 `info_bar`；布局函数按方向预留信息栏空间；`flipclock_clock_animate` 中绘制信息栏；`flipclock_clock_destroy` 中释放
- [x] 修改 `app/jni/flipclock/Android.mk`：加入 `info_bar.c` 和 `lunar.c`
- [x] 新增 CJK 字体资源 `app/src/main/assets/flipclock_cjk.ttf`（Noto Sans CJK SC 子集化，17 KB，许可证见 README）
- [x] 修改 `app/src/main/res/layout/activity_settings.xml`：新增 3 个 `Switch` 及说明
- [x] 修改 `app/src/main/res/values/strings.xml`：新增标题、开关文本、说明文本、restart 提示
- [x] 修改 [`SettingsActivity.java`](file:///home/kali/workspace/flipclock-android/app/src/main/java/one/alynx/flipclock/SettingsActivity.java)：读写三个新键
- [x] 修改 [`MainActivity.java`](file:///home/kali/workspace/flipclock-android/app/src/main/java/one/alynx/flipclock/MainActivity.java)：`onCreate` 中根据 `SharedPreferences` 生成/更新 `getFilesDir()/flipclock.conf`，native 通过 `SDL_AndroidGetInternalStoragePath()` 读取
- [x] 修改 [`README_CN.md`](file:///home/kali/workspace/flipclock-android/README_CN.md)：功能列表、设置页说明、技术说明中补充新选项
- [x] 竖屏传统竖排文字（`info_vertical`，默认开启）及设置页开关，详见 [`vertical-typography-design.md`](vertical-typography-design.md)

## 12. 构建说明

沿用 [`docs/auto-start-design.md`](file:///home/kali/workspace/flipclock-android/docs/auto-start-design.md#L128-L172) 的构建环境：JDK 17 + Android SDK/NDK 25，`minSdkVersion` 19。

新增：

- CJK 字体资源体积较大（Noto Sans CJK SC 全量 > 15 MB），建议使用 **子集化** 工具（`pyftsubset` / `fonttools`）仅保留：
  - 数字与常见 ASCII
  - 农历用汉字：天干地支、`年月日闰正腊十廿卅初一二三四五六七八九十`
  - 星期用汉字：`星期一二三四五六日天`
  - 少量标点：`-`、`/`、`空格` 等
  - 目标控制在 200 KB 以内。

## 13. 测试建议

1. 横屏，仅开启 `日期`：顶部一行显示 `2026-08-04`，时钟位置略下移，居中显示。
2. 横屏，全部开启：顶部一行同时显示 `2026-08-04 星期二 丙午年 六月廿一`，字体明显小于时钟。
3. 竖屏，仅开启 `星期`：左侧一列显示单行 `星期二`，时钟位置略右移。
4. 竖屏，全部开启：左侧竖排三行 `2026-08-04` / `星期二` / `丙午年 六月廿一`。
5. 关闭所有开关，界面应与当前主线代码完全一致。
6. 农历跨年（腊月三十 → 正月初一）时刷新是否正确。
7. 农历闰月（如闰四月）显示是否包含 `闰` 字。
8. CJK 字体缺失时（删除 assets 中字体后重装），应仅关闭信息栏，不影响时钟运行。
9. 开机自启动场景（[`BootAutoStartService`](file:///home/kali/workspace/flipclock-android/app/src/main/java/one/alynx/flipclock/BootAutoStartService.java)）唤醒后，信息栏应正确显示当日日期。
