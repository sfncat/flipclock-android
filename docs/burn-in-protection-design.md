# 防烧屏保护设计文档

## 背景

FlipClock Android 作为长期常亮的翻页时钟应用，主界面上的时间数字、信息栏等元素长时间停留在固定位置。在 OLED 屏幕上，静止高对比度图案容易导致像素老化不均，产生烧屏（burn-in）。因此需要一项可选的防烧屏保护功能，通过极缓慢地移动界面元素，使像素发光点周期性变化，从而降低烧屏风险。

## 设计目标

1. **可选开关，默认关闭**：不强制影响现有用户体验；需要用户主动开启。
2. **小幅位移**：偏移量很小，肉眼几乎不可察觉，不影响时钟可读性。
3. **横竖屏均生效**：
   - 横屏：信息栏在顶部，时间卡片在中间，两者在垂直方向上相互靠近再回归原位。
   - 竖屏：信息栏在左侧，时间卡片在中间，两者在水平方向上相互靠近再回归原位。
4. **不触发重布局**：仅改变最终绘制位置，不改变元素尺寸，避免字体、纹理频繁重载。

## 整体架构

```
SettingsActivity (Java)
    │
    ▼
SharedPreferences ──► MainActivity.writeNativeConf() ──► flipclock.conf
    │                                                        │
    │                                                        ▼
    │                                                flipclock_load_conf() (C)
    │                                                        │
    │                                                        ▼
    │                                                struct flipclock.burn_in_protection
    │                                                        │
    │                                                        ▼
    └───────────────────────────────────────────── flipclock_clock_animate() (C)
                                                            │
                                                            ▼
                                            _flipclock_clock_get_burn_in_offset()
                                                            │
                                                            ▼
                                          flipclock_info_bar_draw(offset)
                                          flipclock_card_animate(..., -offset)
```

## 设置层（Java）

### 新增设置项

- `SharedPreferences` key：`burn_in_protection`
- 默认值：`false`
- UI：在设置页的「主界面显示内容」区域末尾增加一个 Switch，标签为「防烧屏保护」。

### 位移幅度设置

- `SharedPreferences` key：`burn_in_protection_offset`
- 默认值：`1.5`（表示屏幕短边的 1.5%）
- UI：开启「防烧屏保护」后显示一个 SeekBar，可在 0%–5% 之间以 0.1% 为步进调节。关闭防烧屏保护时隐藏。

### 配置传递

`MainActivity.writeNativeConf()` 在生成 `flipclock.conf` 时追加：

```ini
burn_in_protection=false
burn_in_protection_offset=1.5
```

由于 `MainActivity.onCreate()` 在 SDL 初始化之前调用，`flipclock.conf` 总是包含最新设置。

## 原生层（C）

### 配置读取

- 在 `struct flipclock` 中新增字段 `bool burn_in_protection;` 和 `double burn_in_protection_offset;`。
- 在 `flipclock_create()` 中分别初始化为 `false` 和 `0.015`。
- 在 `_flipclock_apply_key_value()` 中解析 `burn_in_protection=true/false` 和 `burn_in_protection_offset=<double>`。

### 偏移算法

在 `clock.c` 中新增静态函数 `_flipclock_clock_get_burn_in_offset()`：

```c
#define BURN_IN_PERIOD_MS 60000              // 60 秒完成一个完整来回

static SDL_Point _flipclock_clock_get_burn_in_offset(struct flipclock_clock *clock)
{
    SDL_Point offset = { 0, 0 };
    if (!clock->app->burn_in_protection)
        return offset;

    int min_side = clock->w < clock->h ? clock->w : clock->h;
    double ratio = clock->app->burn_in_protection_offset;
    if (ratio < 0.0)
        ratio = 0.0;
    if (ratio > 0.05)
        ratio = 0.05;
    int amplitude = (int)(min_side * ratio);
    if (amplitude < 1)
        amplitude = 1;

    Uint32 ticks = SDL_GetTicks();
    double phase = 2.0 * M_PI * (double)(ticks % BURN_IN_PERIOD_MS) /
                   (double)BURN_IN_PERIOD_MS;
    int value = (int)(amplitude * sin(phase));

    if (clock->w >= clock->h)
        offset.y = value;   // 横屏：在 Y 轴偏移
    else
        offset.x = value;   // 竖屏：在 X 轴偏移
    return offset;
}
```

### 动画应用

在 `flipclock_clock_animate()` 中：

1. 计算信息栏偏移 `offset`。
2. 时间卡片组使用反向偏移 `card_offset = { -offset.x, -offset.y }`。
3. 调用时把偏移传递给绘制函数：
   - `flipclock_info_bar_draw(clock->info_bar, offset);`
   - `flipclock_card_animate(clock->hour, card_offset);`
   - `flipclock_card_animate(clock->minute, card_offset);`
   - `flipclock_card_animate(clock->second, card_offset);`

### 绘制函数修改

- `info_bar.h/c`：`flipclock_info_bar_draw()` 新增 `SDL_Point offset` 参数。内部 `_draw_horizontal()`、`_draw_vertical()`、`_draw_vertical_typography()` 同样接收 `offset`，并将 `bar->rect.x + offset.x`、`bar->rect.y + offset.y` 用于最终定位；字号、纹理尺寸仍使用原始 `rect`。
- `card.h/c`：`flipclock_card_animate()` 新增 `SDL_Point offset` 参数。在函数内构造临时 `target_rect = card->rect + offset`，所有 `SDL_RenderCopy`/`SDL_RenderCopyEx` 的目标矩形均使用 `target_rect`，而源矩形和纹理尺寸仍使用 `card->rect`。

## 行为示例

- **横屏（w ≥ h）**：
  - 信息栏 Y 方向偏移 `+value`，卡片组 Y 方向偏移 `-value`。
  - 当 `value > 0` 时，信息栏向下、卡片组向上，两者相互靠近；
  - 当 `value < 0` 时，两者相互远离；
  - 正弦波使其周期性靠近→远离→回归原位。

- **竖屏（w < h）**：
  - 信息栏 X 方向偏移 `+value`，卡片组 X 方向偏移 `-value`。
  - 当 `value > 0` 时，信息栏向右、卡片组向左，两者相互靠近；
  - 当 `value < 0` 时，两者相互远离；
  - 同样周期性回归原位。

## 参数说明

| 参数 | 取值 | 说明 |
|------|------|------|
| `BURN_IN_PERIOD_MS` | 60000 ms | 完整周期 60 秒，肉眼几乎不可察觉 |
| `burn_in_protection_offset` | 0.0–0.05（即 0%–5%） | 用户可调的位移幅度，默认 0.015（1.5%） |
| `M_PI` | 3.14159265358979323846 | 仅在未定义时启用 fallback |

## 默认值与兼容性

- 开关默认关闭，未开启时不产生任何偏移，渲染路径与不修改时完全一致。
- 不修改现有布局计算逻辑，只在绘制阶段应用偏移。
- 不影响开机自启动、权限、字体加载等其他功能。

## 用户提示

设置页「主界面显示内容」区域底部的提示文案追加说明：开启防烧屏保护后，信息栏和时间显示会缓慢做小幅位移。
