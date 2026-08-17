# 纵向显示：信息栏竖排文字设计文档

## 1. 背景与目标

当前 FlipClock 在竖屏（`clock->w < clock->h`）时，信息栏为屏幕左侧的竖向长条，但其中的日期、星期、农历仍采用「横向文字、多行堆叠」的方式（[`info_bar.c`](file:///home/kali/workspace/flipclock-android/app/jni/flipclock/srcs/info_bar.c) 的 `_draw_vertical`）。用户希望在竖屏时改用**传统竖排排版**（字符自上而下逐字排列），使界面更贴合中文日历/挂钟的观感。

本设计的目标：

1. 评估并实现竖屏信息栏的**传统竖排文字**渲染；
2. 提供**配置开关**（`info_vertical`）在「竖排」与「横向堆叠」两种排版之间切换；
3. 放开 Android 主界面的强制横屏限制，使竖屏模式在真机上可达。

已与用户确认的决策：

- **文字样式**：传统竖排 —— 汉字保持直立，数字/字母/符号旋转 90°；
- **排列方式**：单一竖列 —— 日期 → 星期 → 农历自上而下连成一个竖列，组间留空隙；
- **横竖屏**：一并放开竖屏（MainActivity 改为 `fullSensor`）。

> 状态：功能已实现并作为竖屏默认排版（`info_vertical` 默认开启），本文档为设计与实现说明。

## 2. 可行性评估结论

**可行**，理由如下：

| 维度 | 分析 |
|------|------|
| 渲染能力 | SDL_ttf 支持逐字符渲染 UTF-8；SDL2 的 `SDL_RenderCopyEx` 支持任意角度旋转，可对 ASCII 字符做 90° 旋转，实现传统竖排 |
| 空间 | 竖屏信息栏宽度约屏幕宽 22%（`INFO_RATIO_PORTRAIT = 0.22`），高度为整屏高度。竖排只需一列字符宽度（约一个汉字宽），可容纳约 20 个字符的竖列 |
| 布局刷新 | `SDL_WINDOWEVENT_SIZE_CHANGED` → `_flipclock_clock_update_layout()`（[`clock.c`](file:///home/kali/workspace/flipclock-android/app/jni/flipclock/srcs/clock.c#L382-L396)），旋转屏幕后自动重排，无需额外处理 |
| 配置链路 | 完全复用 `show_date` 现有链路：`flipclock.h` 字段 → `flipclock.c` 解析 → `MainActivity` 写 `flipclock.conf` → `SettingsActivity` 开关 |

唯一注意点：Android 当前 `sensorLandscape` 强制横屏，竖屏分支在真机上不会触发，需放开方向（见第 9 节）。

## 3. 显示效果示例

### 3.1 竖屏整体布局（示意，手机竖持）

```
┌────┬──────────────────────────┐
│    │                          │
│ 二 │        ┌───┐             │
│ 0  │        │ 1 │   小时卡片    │
│ 2  │        └───┘             │
│ 6  │                          │
│ -  │        ┌───┐             │
│ 0  │        │ 0 │   分钟卡片    │
│ 8  │        └───┘             │
│ -  │                          │
│ 1  │        ┌───┐             │
│ 0  │        │ 2 │   秒卡片     │
│    │        └───┘             │
│ 星 │                          │
│ 期 │          （时钟卡片区域    │
│ 一 │          竖向堆叠，整体   │
│    │          右移，避开信息栏）│
│ 丙 │                          │
│ 午 │                          │
│ 年 │                          │
│    │                          │
│ 六 │                          │
│ 月 │                          │
│ 廿 │                          │
│ 二 │                          │
└────┴──────────────────────────┘
```

> 说明：上图中数字以直立显示仅为示意；实际实现中数字/符号「2 0 2 6 - 0 8 - 1 0」会**顺时针旋转 90°**，从右向左读，效果类似书本/日历中的竖排数字。

### 3.2 信息栏内容对应关系（默认开启：日期+星期+农历，含农历年）

| 原横向文本 | 竖排后 |
|-----------|--------|
| `2026-08-10` | `2` `0` `2` `6` `-` `0` `8` `-` `1` `0`（10 个旋转字符） |
| `星期一` | `星` `期` `一`（3 个直立汉字） |
| `丙午年` | `丙` `午` `年`（3 个直立汉字） |
| `六月廿二` | `六` `月` `廿` `二`（4 个直立汉字） |

组间留空隙（约半字高），整列垂直居中、水平居中于信息栏条内。组内相邻字形之间另加约 `0.15 * font_px` 的垂直间距（`TYPO_GLYPH_SPACING`），避免日期数字等旋转字符上下完全挨在一起。

### 3.3 关闭农历年时的示例

此时农历为 `六月廿二`，组数由 4 减为 3（日期 / 星期 / 农历月日），字号自适应放大。

## 4. 现状分析

### 4.1 竖屏布局（已存在）

[`clock.c`](file:///home/kali/workspace/flipclock-android/app/jni/flipclock/srcs/clock.c#L89-L135) 中竖屏分支：

- 信息栏矩形：`x = space_size`，`y = 0`，`w = clock->w * INFO_RATIO_PORTRAIT`（0.22），`h = clock->h`；
- 调用 `flipclock_info_bar_set_rect(clock->info_bar, info_rect, false)`，`horizontal = false`；
- 时钟卡片整体右移 `info_w`，竖向堆叠。

### 4.2 现有竖屏渲染（横向文字多行堆叠）

[`info_bar.c`](file:///home/kali/workspace/flipclock-android/app/jni/flipclock/srcs/info_bar.c) 的 `_draw_vertical`：

- 日期按空格/`-` 拆行（`_split_into_lines`），如 `2026` / `08-04`；
- 星期、农历各一行；
- 各行为横向文本，逐行堆叠，垂直居中、水平对齐到最宽行。

设计文档 [`date-info-design.md`](file:///home/kali/workspace/flipclock-android/docs/date-info-design.md#L127-L138) 第 5.3 节明确注明：第一版采用「横向文字、多行堆叠」，竖排留待后续。

### 4.3 字号计算（现有）

竖屏字号 `px = rect.w / (lunar_year_shown ? 4 : 3.5) * info_scale`，保证一行能容纳约 4 个全角字符。竖排后每行只有 1 个字符，字号约束将完全不同（见 7.3 节）。

### 4.4 配置链路（复用）

`flipclock.h` 字段 → `flipclock.c` 默认值 + `_flipclock_apply_key_value` 解析 → `MainActivity.writeNativeConf()` 生成 `flipclock.conf` → `SettingsActivity` 开关写 `SharedPreferences`。修改后需重启生效（设置页已有 `info_bar_restart_hint` 提示）。

## 5. 总体设计

```
                    ┌─ info_vertical = true ─┐
竖屏 (w < h) ───────┤                        ├─ 传统竖排（_draw_vertical_typography，默认）
                    └─ info_vertical = false ┘
                                                 └─ 横向文字多行堆叠（_draw_vertical，可选）
横屏 (w >= h) ────── 固定横向布局（_draw_horizontal，info_vertical 不生效）
```

- 配置键 `info_vertical`（bool，默认 `true`）：竖屏时信息栏采用传统竖排文字；关闭后回退为横向文字多行堆叠；
- 竖排仅在竖屏时生效；横屏始终横向布局；
- 设置页新增「竖排文字」开关，与日期/星期/农历开关同组。

## 6. 配置项设计

| 键 | 类型 | 默认 | 说明 |
|----|------|------|------|
| `info_vertical` | bool | `true` | 竖屏时信息栏采用传统竖排文字；`false` 为横向文字多行堆叠 |

解析遵循现有约定：仅 `"true"` 视为开启。

## 7. 渲染实现设计（`info_bar.c`）

### 7.1 新增函数

| 函数 | 职责 |
|------|------|
| `_utf8_decode_char(const char *s, int *len)` | 解码一个 UTF-8 字符，返回 Unicode 码点及占用字节数（返回 0 表示结束/非法） |
| `_is_cjk(uint32_t cp)` | 判断是否为 CJK 汉字（`0x4E00–0x9FFF` 及扩展 A `0x3400–0x4DBF`、兼容区 `0xF900–0xFAFF`）。是 → 直立；否（ASCII/拉丁）→ 旋转 90° |
| `_render_glyph(bar, cp, bool rotated, int *out_w, int *out_h)` | 将单个码点编码回 UTF-8 后，复用现有 `TTF_RenderUTF8_Blended` 渲染为 texture；返回旋转后应绘制的尺寸 |
| `_typo_render_glyphs(bar, groups, g, glyphs, group_end, ...)` | 渲染全部字形并计算整体尺寸（总高/最宽列宽），支持字号调整后重渲染 |
| `_draw_vertical_typography(bar)` | 竖排主绘制函数 |

> 说明：不依赖 `TTF_RenderGlyph32_Blended`（需要 SDL_ttf ≥ 2.0.18），统一回退为「码点 → UTF-8 → `TTF_RenderUTF8_Blended`」，与本文件现有渲染方式一致，兼容性最好。

### 7.2 旋转绘制

- 直立汉字：`SDL_RenderCopy`；
- 旋转字符（数字、`-` 等 ASCII）：`SDL_RenderCopyEx(renderer, tex, NULL, &dst, angle, NULL, SDL_FLIP_NONE)`；
- 旋转角：顺时针 90°（`angle = -90`），使字符顶部朝右、竖列自上而下可读；具体旋转方向以真机视觉效果为准，实现时仅需改符号/翻转即可微调；
- 尺寸处理：旋转后显示尺寸为「原纹理高 × 原纹理宽」（即宽≈字高、高≈字宽），绘制时 `dst` 宽高需互换。

### 7.3 布局与字号算法（决策完整）

1. **收集文本组**：按 日期 → 星期 → 农历 顺序收集非空文本，忽略空组，组数记为 `g`；
2. **拆字符**：每组拆为码点序列，统计直立字符数 `cjk_count` 与旋转字符数 `ascii_count`；
3. **字号**（在现有 `flipclock_info_bar_set_rect` 中计算，竖排分支；文本就绪后由 `flipclock_info_bar_refresh` 按需纠正）：
   - 宽度约束：`px_w = rect.w * 0.9`（竖排只需一列宽度）；
   - 高度约束：直立汉字实际字形高约为字号的 1.25 倍（含行高），旋转字符约 `0.6 * px`，组间空隙 `gap = 0.6 * px`，组内字形间距 `glyph_gap = 0.15 * px`（`TYPO_GLYPH_SPACING`）；令
     `units = cjk_count * 1.25 + ascii_count * 0.6 + TYPO_GLYPH_SPACING * (cjk_count + ascii_count - g) + (g - 1) * 0.7`，
     则 `px_h = rect.h * 0.95 / units`（预留 5% 高度余量）；
   - `px = min(px_w, px_h) * info_scale`，下限 `8`（与现有下限一致）；
4. **绘制**：
   - 计算总高度 `total_h`（含组间空隙 `gap` 与组内字形间距 `glyph_gap`）与最宽列宽 `col_w`；
   - 垂直居中：`start_y = rect.y + (rect.h - total_h) / 2`；
   - 水平居中：`x = rect.x + (rect.w - col_w) / 2`；
   - 依序绘制每个 glyph，组内相邻字形之间插入 `glyph_gap` 间距（组末不加，由组间空隙接管），组间插入 `gap` 空隙；
   - 防御：若整列仍超出信息栏高度，按比例缩小字号后重新渲染（[`info_bar.c`](file:///home/kali/workspace/flipclock-android/app/jni/flipclock/srcs/info_bar.c) `_draw_vertical_typography`）；
5. **分派**：`flipclock_info_bar_draw()` 中
   ```c
   if (bar->horizontal)
       _draw_horizontal(bar);
   else if (app->info_vertical)
       _draw_vertical_typography(bar);
   else
       _draw_vertical(bar);
   ```
   （`bar->app` 已持有 `struct flipclock *`，可直接读取 `info_vertical`。）

## 8. Android 层改动

### 8.1 `MainActivity.java` — `writeNativeConf()`

在 `show_lunar_year` 之后追加：

```java
sb.append("info_vertical=")
        .append(prefs.getBoolean("info_vertical", true)).append('\n');
```

### 8.2 `SettingsActivity.java`

- 新增常量 `private static final String KEY_INFO_VERTICAL = "info_vertical";`
- 新增成员 `private Switch infoVerticalSwitch;`，`findViewById(R.id.info_vertical_switch)`；
- `setChecked(prefs.getBoolean(KEY_INFO_VERTICAL, true))`；
- `setOnCheckedChangeListener` 写回 `prefs`（与现有四个开关一致）。

### 8.3 `strings.xml`

```xml
<string name="info_vertical_label">竖排文字</string>
```

（如需更明确可改为「信息栏竖排文字」，此处取简洁文案。）

### 8.4 `activity_settings.xml`

在「显示农历年份」开关行（[第 126-145 行](file:///home/kali/workspace/flipclock-android/app/src/main/res/layout/activity_settings.xml#L126-L145)）之后，仿照现有 `LinearLayout + Switch` 结构新增一行：

```xml
<LinearLayout ...>
    <TextView android:layout_width="0dp" android:layout_height="wrap_content"
        android:layout_weight="1"
        android:text="@string/info_vertical_label" .../>
    <Switch android:id="@+id/info_vertical_switch" .../>
</LinearLayout>
```

> 生效机制：与现有开关一致，修改后重启 FlipClock 生效（已有 `info_bar_restart_hint` 提示文案）。

## 9. 横竖屏放开

[`AndroidManifest.xml`](file:///home/kali/workspace/flipclock-android/app/src/main/AndroidManifest.xml#L89) 中 MainActivity：

```xml
android:screenOrientation="sensorLandscape"
```

改为：

```xml
android:screenOrientation="fullSensor"
```

- `configChanges` 已包含 `orientation|screenSize`，旋转不会重建 Activity；
- native 层收到 `SDL_WINDOWEVENT_SIZE_CHANGED` 后自动重排（[`clock.c`](file:///home/kali/workspace/flipclock-android/app/jni/flipclock/srcs/clock.c#L382-L396)），无需额外代码。

## 10. 边界情况与决策

| 场景 | 处理 |
|------|------|
| 竖排字号被高度约束压至过小（< 8px） | 与现有逻辑一致，钳制到 8px；日期固定 10 字符，正常竖屏不会触发 |
| 仅开启 1~2 项（如只开农历） | 组数减少、字符数减少，字号按公式自适应放大，整列居中 |
| 农历含干支年 / 不含 | 组数 4 / 3，字符数 20 / 17，字号自动调整 |
| 横屏 + `info_vertical=true` | 开关不生效，仍横向布局（竖排在横屏无高度可用） |
| CJK 字体缺失 | `bar->enabled = false`，不绘制（现有逻辑，无需改动） |
| 日期含 `-`、全半角混排 | 全角（CJK）直立，ASCII 一律旋转 |
| 日期数字挨在一起 | 组内相邻字形之间加 `glyph_gap = 0.15 * font_px` 垂直间距，旋转字符与汉字均适用；组间仍为 `gap = 0.6 * font_px` |
| 旧版本已保存 `show_*` 偏好 | 不受影响；首次升级时 `info_vertical` 默认 `true`，竖屏显示竖排文字 |

## 11. 验证方案

1. **构建**：`./gradlew assembleDebug` 编译通过；
2. **真机/模拟器**：
   - 竖持手机 → 纵向模式，左侧信息栏竖排显示日期/星期/农历，时钟卡片右移不重叠；
   - 旋转至横屏 → 恢复顶部横向布局，无错位；
3. **设置页**：
   - 「竖排文字」默认开启 → 重启后竖屏为传统竖排；
   - 关闭 → 重启后竖屏为横向堆叠（回归）；
   - 日期数字之间留有约 0.15 倍字号的垂直间距，不与相邻字符粘连；
   - 关闭日期/星期/农历若干项 → 竖排组数/字号自适应；
4. **回归**：横屏渲染与现有 `show_date` / `show_weekday` / `show_lunar` 开关行为无变化。

## 12. 任务清单

- [x] `flipclock.h`：`struct flipclock` 新增 `bool info_vertical;`
- [x] `flipclock.c`：默认值 `true`；`_flipclock_apply_key_value` 解析 `info_vertical`
- [x] `info_bar.c`：新增 `_utf8_decode_char` / `_is_cjk` / `_render_glyph` / `_typo_render_glyphs` / `_draw_vertical_typography`
- [x] `info_bar.c`：`flipclock_info_bar_set_rect` 竖排字号分支
- [x] `info_bar.c`：`flipclock_info_bar_draw` 分派逻辑
- [x] `info_bar.c`：组内字形间距 `TYPO_GLYPH_SPACING`（0.15），并同步计入总高与字号估算
- [x] `MainActivity.java`：`writeNativeConf()` 输出 `info_vertical`
- [x] `SettingsActivity.java`：`KEY_INFO_VERTICAL` + 开关读写
- [x] `strings.xml`：`info_vertical_label`
- [x] `activity_settings.xml`：新增竖排开关行
- [x] `AndroidManifest.xml`：`sensorLandscape` → `fullSensor`
- [x] 构建 + 真机验证（第 11 节）
