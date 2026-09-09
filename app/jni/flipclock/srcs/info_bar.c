/**
 * 信息栏：在主界面上额外绘制日期、星期、农历。
 *
 * 布局由 `flipclock_info_bar_set_rect` 决定：
 * - horizontal = true：横屏，信息栏是一个横条，三项水平排列，居中；
 * - horizontal = false：竖屏，信息栏是一个竖条，三项按行竖向堆叠，居中。
 *
 * 文本使用 CJK 字体通过 `TTF_RenderUTF8_Blended` 渲染。
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include <SDL.h>
#include <SDL_ttf.h>

#include "flipclock.h"
#include "info_bar.h"
#include "lunar.h"
#include "solar_terms.h"

static const char *WEEKDAY_NAMES[7] = { "周日", "周一", "周二", "周三",
					"周四", "周五", "周六" };

/* 竖排文字：最多 3 个文本源，按空格拆出的组数与单组字符数上限。 */
#define MAX_TYPO_GROUPS 8
#define MAX_TYPO_GLYPHS 32
/* 组内相邻字形之间的垂直间距（相对字号的倍数），
   避免日期数字等旋转字符上下完全挨在一起。 */
#define TYPO_GLYPH_SPACING 0.15
/* 竖排时，连续的非 CJK 码点（如温度数字「28」）若不超过此数量，
   则作为一个整体横向书写（不旋转 90°），即「28」横着排。 */
#define MAX_TYPO_RUN_CP 4

struct typo_group {
	const char *text;
	int length;
};

struct typo_glyph {
	SDL_Texture *texture;
	int w; /* 旋转后的显示宽。 */
	int h; /* 旋转后的显示高。 */
	bool rotated;
};

/* 前向声明：双行/双列与通用竖排绘制函数在文件后半部定义，但 set_rect /
   双列绘制等处会提前调用（静态函数须先声明）。 */
static void _flipclock_info_bar_close_line_fonts(
	struct flipclock_info_bar *bar);
static void _draw_vertical_typography_text(struct flipclock_info_bar *bar,
					   SDL_Rect col_rect,
					   TTF_Font **font_ref, int *px_ref,
					   const char *text, SDL_Point offset,
					   const char *font_path);

/**
 * 解码一个 UTF-8 字符，返回 Unicode 码点并输出占用字节数；
 * 返回 0 且 `*len == 0` 表示字符串结束，返回 0 且 `*len > 0` 表示非法字节
 * （按单字节处理，避免死循环）。
 */
static uint32_t _utf8_decode_char(const char *s, int *len)
{
	const unsigned char *u = (const unsigned char *)s;
	if (u[0] == '\0') {
		*len = 0;
		return 0;
	}
	if (u[0] < 0x80) {
		*len = 1;
		return u[0];
	}
	if ((u[0] & 0xE0) == 0xC0 && (u[1] & 0xC0) == 0x80) {
		*len = 2;
		return ((uint32_t)(u[0] & 0x1F) << 6) | (u[1] & 0x3F);
	}
	if ((u[0] & 0xF0) == 0xE0 && (u[1] & 0xC0) == 0x80 &&
	    (u[2] & 0xC0) == 0x80) {
		*len = 3;
		return ((uint32_t)(u[0] & 0x0F) << 12) |
		       ((uint32_t)(u[1] & 0x3F) << 6) | (u[2] & 0x3F);
	}
	if ((u[0] & 0xF8) == 0xF0 && (u[1] & 0xC0) == 0x80 &&
	    (u[2] & 0xC0) == 0x80 && (u[3] & 0xC0) == 0x80) {
		*len = 4;
		return ((uint32_t)(u[0] & 0x07) << 18) |
		       ((uint32_t)(u[1] & 0x3F) << 12) |
		       ((uint32_t)(u[2] & 0x3F) << 6) | (u[3] & 0x3F);
	}
	*len = 1;
	return u[0];
}

/* 判断是否为 CJK 汉字（直立显示）；否则（ASCII/拉丁）旋转 90°。 */
static bool _is_cjk(uint32_t cp)
{
	return (cp >= 0x4E00 && cp <= 0x9FFF) ||
	       (cp >= 0x3400 && cp <= 0x4DBF) ||
	       (cp >= 0xF900 && cp <= 0xFAFF);
}

/**
 * 收集竖排文本组：按 日期 → 星期 → 农历 顺序，忽略空文本；
 * 每个文本再按空格拆组（农历 `丙午年 六月廿二` 会拆成两组）。
 */
static int _typo_collect_groups(const struct flipclock_info_bar *bar,
				struct typo_group groups[], int max_groups)
{
	const char *texts[4];
	int n = 0;
	if (bar->date_text[0] && !bar->hide_date)
		texts[n++] = bar->date_text;
	if (bar->weekday_text[0])
		texts[n++] = bar->weekday_text;
	if (bar->lunar_text[0])
		texts[n++] = bar->lunar_text;
	/* 天气并入信息栏（方案2 左栏）时作为额外一段参与竖排。 */
	if (bar->show_weather_segment && bar->weather_text[0])
		texts[n++] = bar->weather_text;
	int count = 0;
	for (int t = 0; t < n && count < max_groups; ++t) {
		const char *p = texts[t];
		const char *seg_start = p;
		for (;;) {
			if (*p == ' ' || *p == '\0') {
				if (p > seg_start && count < max_groups) {
					groups[count].text = seg_start;
					groups[count].length =
						(int)(p - seg_start);
					++count;
				}
				seg_start = p + 1;
				if (*p == '\0')
					break;
			}
			++p;
		}
	}
	return count;
}

/**
 * 竖排模式字号：宽度约束按一列字符宽（rect.w * 0.9），高度约束按
 * 直立字符占 1 单位、旋转字符占 0.6 单位、组间空隙 0.6 单位估算，
 * 取两者较小值乘以 info_scale。文本为空时返回 0。
 */
static int _typography_font_px_from(const struct flipclock_info_bar *bar,
				    int col_w, int col_h,
				    const struct typo_group groups[], int g)
{
	if (g == 0)
		return 0;

	int cjk_count = 0, ascii_count = 0;
	for (int i = 0; i < g; ++i) {
		const char *p = groups[i].text;
		const char *end = p + groups[i].length;
		while (p < end) {
			int len;
			uint32_t cp = _utf8_decode_char(p, &len);
			if (len <= 0)
				break;
			p += len;
			if (_is_cjk(cp))
				++cjk_count;
			else
				++ascii_count;
		}
	}
	/*
	 * 直立汉字实际字形高度约为字号的 1.2~1.3 倍（含行高），估算时取
	 * 保守系数，并给信息栏高度留 5% 余量，避免整列超出而被顶部裁切。
	 * 组内字形间距数 = 总字数 - 组数，计入估算以保证字号合理。
	 */
	double units = cjk_count * 1.1 + ascii_count * 0.6 +
		       TYPO_GLYPH_SPACING *
			       (cjk_count + ascii_count - g) +
		       (g - 1) * 0.6;
	if (units <= 0)
		return 0;
	double px_w = col_w * 0.9;
	double px_h = col_h * 0.97 / units;
	double px = (px_w < px_h ? px_w : px_h) * bar->app->info_scale;
	int px_i = (int)px;
	if (px_i < 8)
		px_i = 8;
	return px_i;
}

static int _typography_font_px(const struct flipclock_info_bar *bar)
{
	struct typo_group groups[MAX_TYPO_GROUPS];
	int g = _typo_collect_groups(bar, groups, MAX_TYPO_GROUPS);
	return _typography_font_px_from(bar, bar->rect.w, bar->rect.h,
					groups, g);
}

static void _flipclock_info_bar_close_font(struct flipclock_info_bar *bar)
{
	if (bar->font != NULL) {
		TTF_CloseFont(bar->font);
		bar->font = NULL;
	}
}

/* 返回信息栏字体路径；未配置时返回 NULL 并记录错误。 */
static const char *_info_bar_font_path(const struct flipclock_info_bar *bar)
{
	const struct flipclock *app = bar->app;
	if (app->info_bar_font_path[0] != '\0')
		return app->info_bar_font_path;
	if (app->cjk_font_path[0] != '\0')
		return app->cjk_font_path;
	LOG_ERROR("Info bar: no CJK font path configured.\n");
	return NULL;
}

/* 按 px 打开一个加粗 CJK 字体对象；失败返回 NULL。 */
static TTF_Font *_open_font_path(const struct flipclock_info_bar *bar,
				  int px, const char *font_path)
{
	if (px <= 0)
		return NULL;
	if (font_path == NULL || font_path[0] == '\0')
		font_path = _info_bar_font_path(bar);
	if (font_path == NULL || font_path[0] == '\0')
		return NULL;
	TTF_Font *font = TTF_OpenFont(font_path, px);
	if (font == NULL) {
		LOG_ERROR("Info bar: failed to open CJK font `%s`: %s\n",
			  font_path, TTF_GetError());
		return NULL;
	}
	/* 信息栏文字加粗，提升可读性。 */
	TTF_SetFontStyle(font, TTF_STYLE_BOLD);
	return font;
}

static TTF_Font *_open_font(const struct flipclock_info_bar *bar, int px)
{
	return _open_font_path(bar, px, NULL);
}

static void _flipclock_info_bar_open_font(struct flipclock_info_bar *bar,
					  int px)
{
	bar->font = _open_font(bar, px);
}

struct flipclock_info_bar *
flipclock_info_bar_create(struct flipclock *app, SDL_Renderer *renderer)
{
	RETURN_VAL_IF_FAIL(app != NULL, NULL);
	RETURN_VAL_IF_FAIL(renderer != NULL, NULL);

	struct flipclock_info_bar *bar = malloc(sizeof(*bar));
	if (bar == NULL) {
		LOG_ERROR("Failed to create info bar!\n");
		return NULL;
	}
	bar->app = app;
	bar->renderer = renderer;
	bar->font = NULL;
	bar->font_px = 0;
	bar->rect.x = 0;
	bar->rect.y = 0;
	bar->rect.w = 0;
	bar->rect.h = 0;
	bar->horizontal = true;
	bar->enabled = false;
	bar->date_text[0] = '\0';
	bar->weekday_text[0] = '\0';
	bar->lunar_text[0] = '\0';
	bar->show_weather_segment = false;
	bar->horizontal_stack = false;
	bar->hide_date = false;
	bar->weather_text[0] = '\0';
	bar->solar_term_text[0] = '\0';
	bar->two_line = false;
	bar->font_line[0] = NULL;
	bar->font_line[1] = NULL;
	bar->font_px_line[0] = 0;
	bar->font_px_line[1] = 0;
	bar->last_yday = -1;
	bar->last_year = -1;
	return bar;
}

void flipclock_info_bar_set_rect(struct flipclock_info_bar *bar, SDL_Rect rect,
				 bool horizontal)
{
	RETURN_IF_FAIL(bar != NULL);

	bar->rect = rect;
	bar->horizontal = horizontal;
	/* 这两个由 clock.c 在布局时按模式重新设置，这里先清掉旧值。 */
	bar->show_weather_segment = false;
	bar->horizontal_stack = false;
	bar->hide_date = false;
	/* 布局变化使双行/双列字号作废，关闭两区字体对象。 */
	_flipclock_info_bar_close_line_fonts(bar);

	const struct flipclock *app = bar->app;
	int px;
	/*
	 * 农历干支年默认不显示，此时整行更短，可以放大信息栏字体；
	 * 显示年份时字号略小，避免横屏单行文字过宽。
	 */
	bool lunar_year_shown = app->show_lunar && app->show_lunar_year;
	if (horizontal) {
		/* 横屏：高度约占窗口短边 10%，文字取其中约 75%/85%。 */
		/* two_line_info 开启时 rect 已按双行预留（2× 单行高），
		   单行绘制的字号基准须用半高，否则无内容时字会过大。 */
		int line_h = app->two_line_info ? rect.h / 2 : rect.h;
		px = (int)(line_h * (lunar_year_shown ? 0.75 : 0.85) *
			   app->info_scale);
	} else if (app->info_vertical) {
		/*
		 * 竖屏 + 传统竖排：按字符数与屏高自适应字号，
		 * 见 `_typography_font_px`。首次布局时文本尚未生成，
		 * 回退旧算法，随后在 `flipclock_info_bar_refresh` 中纠正。
		 */
		px = _typography_font_px(bar);
		if (px == 0)
			px = (int)(rect.w / (lunar_year_shown ? 4 : 3.5) *
				   app->info_scale);
	} else {
		/*
		 * 竖屏：竖排多行，按列宽取 1/4 或 1/3.5，保证 4 个全角字符
		 * 左右的单行（如 `六月廿二`）能放进左侧信息栏。
		 */
		px = (int)(rect.w / (lunar_year_shown ? 4 : 3.5) *
			   app->info_scale);
	}
	if (px < 8)
		px = 8;
	bar->font_px = px;

	_flipclock_info_bar_close_font(bar);
	_flipclock_info_bar_open_font(bar, px);
	bar->enabled = (bar->font != NULL);
}

void flipclock_info_bar_refresh(struct flipclock_info_bar *bar,
				const struct tm *now, bool force)
{
	RETURN_IF_FAIL(bar != NULL);
	RETURN_IF_FAIL(now != NULL);

	const struct flipclock *app = bar->app;
	bool same_day = !force && now->tm_yday == bar->last_yday &&
			 (now->tm_year + 1900) == bar->last_year;

	if (!same_day) {

	if (app->show_date) {
		snprintf(bar->date_text, sizeof(bar->date_text),
			 "%04d-%02d-%02d", now->tm_year + 1900,
			 now->tm_mon + 1, now->tm_mday);
	} else {
		bar->date_text[0] = '\0';
	}

	if (app->show_weekday) {
		int wday = now->tm_wday;
		if (wday < 0 || wday > 6)
			wday = 0;
		strncpy(bar->weekday_text, WEEKDAY_NAMES[wday],
			sizeof(bar->weekday_text));
		bar->weekday_text[sizeof(bar->weekday_text) - 1] = '\0';
	} else {
		bar->weekday_text[0] = '\0';
	}

	if (app->show_lunar) {
		lunar_from_gregorian(now->tm_year + 1900, now->tm_mon + 1,
				     now->tm_mday, app->show_lunar_year,
				     bar->lunar_text, sizeof(bar->lunar_text));
	} else {
		bar->lunar_text[0] = '\0';
	}

	/* 节气/三伏/九九：优先用 almanac_today 伪造日期（调试键），
	   空或格式非法时回退真实日期。详见设计文档 §9.4。 */
	int al_y = 0, al_m = 0, al_d = 0;
	if (app->almanac_today[0] != '\0' &&
	    sscanf(app->almanac_today, "%d-%d-%d", &al_y, &al_m, &al_d) == 3)
		solar_term_text_for(al_y, al_m, al_d, bar->solar_term_text,
				    sizeof(bar->solar_term_text));
	else
		solar_term_text_for(now->tm_year + 1900, now->tm_mon + 1,
				    now->tm_mday, bar->solar_term_text,
				    sizeof(bar->solar_term_text));

	} /* !same_day */

	/*
	 * 竖排字号纠正：始终执行（不只是跨日）。转屏后 set_rect 时
	 * hide_date/show_weather_segment 尚未设置，_typography_font_px
	 * 算出的 px 偏小；此处状态已就绪，按需纠正字号。
	 */
	if (!bar->horizontal && app->info_vertical) {
		int px = _typography_font_px(bar);
		if (px > 0 && px != bar->font_px) {
			bar->font_px = px;
			_flipclock_info_bar_close_font(bar);
			_flipclock_info_bar_open_font(bar, px);
			bar->enabled = (bar->font != NULL);
		}
	}

	if (!same_day) {
		bar->last_yday = now->tm_yday;
		bar->last_year = now->tm_year + 1900;
	}
}

/* 使用整行 UTF-8 渲染，返回创建的 texture 与其尺寸。可能返回 NULL。 */
static SDL_Texture *_render_line_font(const struct flipclock_info_bar *bar,
					   TTF_Font *font, const char *text,
					   int *out_w, int *out_h)
{
	if (text == NULL || text[0] == '\0' || font == NULL)
		return NULL;
	SDL_Color color = bar->app->text_color;
	SDL_Surface *surface = TTF_RenderUTF8_Blended(font, text, color);
	if (surface == NULL) {
		LOG_ERROR("Info bar: TTF_RenderUTF8_Blended failed: %s\n",
			  TTF_GetError());
		return NULL;
	}
	SDL_Texture *texture =
		SDL_CreateTextureFromSurface(bar->renderer, surface);
	if (out_w != NULL)
		*out_w = surface->w;
	if (out_h != NULL)
		*out_h = surface->h;
	SDL_FreeSurface(surface);
	if (texture == NULL) {
		LOG_ERROR("Info bar: SDL_CreateTextureFromSurface failed: %s\n",
			  SDL_GetError());
	}
	return texture;
}

/* 使用整行 UTF-8 渲染（沿用 bar->font），兼容旧调用。 */
static SDL_Texture *_render_line(struct flipclock_info_bar *bar,
				 const char *text, int *out_w, int *out_h)
{
	return _render_line_font(bar, bar->font, text, out_w, out_h);
}

/**
 * 渲染单个码点：编码回 UTF-8 后复用 `_render_line` 渲染。
 * `rotated` 为 true 表示该字符需顺时针旋转 90°，
 * 输出的 `out_w/out_h` 为绘制时（旋转后）的尺寸，即「原高 × 原宽」。
 */
static SDL_Texture *_render_glyph_font(const struct flipclock_info_bar *bar,
					  TTF_Font *font, uint32_t cp,
					  bool rotated, int *out_w, int *out_h)
{
	char utf8[5];
	int len = 0;
	if (cp < 0x80) {
		utf8[len++] = (char)cp;
	} else if (cp < 0x800) {
		utf8[len++] = (char)(0xC0 | (cp >> 6));
		utf8[len++] = (char)(0x80 | (cp & 0x3F));
	} else if (cp < 0x10000) {
		utf8[len++] = (char)(0xE0 | (cp >> 12));
		utf8[len++] = (char)(0x80 | ((cp >> 6) & 0x3F));
		utf8[len++] = (char)(0x80 | (cp & 0x3F));
	} else {
		utf8[len++] = (char)(0xF0 | (cp >> 18));
		utf8[len++] = (char)(0x80 | ((cp >> 12) & 0x3F));
		utf8[len++] = (char)(0x80 | ((cp >> 6) & 0x3F));
		utf8[len++] = (char)(0x80 | (cp & 0x3F));
	}
	utf8[len] = '\0';

	int w, h;
	SDL_Texture *texture = _render_line_font(bar, font, utf8, &w, &h);
	if (texture == NULL)
		return NULL;
	if (rotated) {
		*out_w = h;
		*out_h = w;
	} else {
		*out_w = w;
		*out_h = h;
	}
	return texture;
}

static void _draw_horizontal(struct flipclock_info_bar *bar, SDL_Point offset)
{
	/* 收集非空文本，最多 4 段（含并入的天气段）。 */
	const char *texts[4];
	int count = 0;
	if (bar->date_text[0] && !bar->hide_date)
		texts[count++] = bar->date_text;
	if (bar->weekday_text[0])
		texts[count++] = bar->weekday_text;
	if (bar->lunar_text[0])
		texts[count++] = bar->lunar_text;
	if (bar->show_weather_segment && bar->weather_text[0])
		texts[count++] = bar->weather_text;
	if (count == 0)
		return;

	/*
	 * 自适应字号（方案1「字少时放大」）：
	 * - 内容过宽则缩小，避免超出横条；
	 * - 内容偏窄则放大，直到占满约 90% 横条宽度或达到上限。
	 * 每轮先用候选字号测量整行宽，再据此调整字号。
	 */
	int px = bar->font_px;
	/* two_line_info 开启时 rect 按双行预留，单行绘制上限用半高。 */
	int line_h = bar->app->two_line_info ? bar->rect.h / 2 : bar->rect.h;
	const int max_px = (int)(line_h * 1.1);
	const int min_px = 8;
	for (int attempt = 0; attempt < 12; ++attempt) {
		_flipclock_info_bar_close_font(bar);
		_flipclock_info_bar_open_font(bar, px);
		if (bar->font == NULL)
			break;
		int total_w = 0;
		for (int i = 0; i < count; ++i) {
			int w = 0, h = 0;
			SDL_Texture *t = _render_line(bar, texts[i], &w, &h);
			if (t != NULL)
				SDL_DestroyTexture(t);
			total_w += w;
		}
		int gap = px / 3;
		total_w += gap * (count - 1);
		if (total_w > bar->rect.w) {
			int new_px = (int)(px * ((double)bar->rect.w * 0.98 /
						 total_w));
			if (new_px >= px)
				break;
			px = new_px < min_px ? min_px : new_px;
		} else if (total_w < bar->rect.w * 0.85 && px < max_px) {
			int new_px = (int)(px *
					   ((double)bar->rect.w * 0.9 /
					    total_w));
			if (new_px <= px)
				new_px = px + 1;
			if (new_px > max_px)
				new_px = max_px;
			px = new_px;
		} else {
			break;
		}
	}
	if (bar->font == NULL)
		return;

	SDL_Texture *textures[4] = { NULL, NULL, NULL, NULL };
	int widths[4] = { 0, 0, 0, 0 };
	int heights[4] = { 0, 0, 0, 0 };
	int total_w = 0;
	int max_h = 0;
	int gap = px / 3;
	for (int i = 0; i < count; ++i) {
		textures[i] = _render_line(bar, texts[i], &widths[i],
					   &heights[i]);
		total_w += widths[i];
		if (heights[i] > max_h)
			max_h = heights[i];
	}
	total_w += gap * (count - 1);

	int start_x = bar->rect.x + offset.x + (bar->rect.w - total_w) / 2;
	int base_y = bar->rect.y + offset.y + (bar->rect.h - max_h) / 2;
	int cur_x = start_x;
	for (int i = 0; i < count; ++i) {
		if (textures[i] == NULL) {
			cur_x += widths[i] + gap;
			continue;
		}
		SDL_Rect dst = { cur_x, base_y + (max_h - heights[i]) / 2,
				 widths[i], heights[i] };
		SDL_RenderCopy(bar->renderer, textures[i], NULL, &dst);
		SDL_DestroyTexture(textures[i]);
		cur_x += widths[i] + gap;
	}
}

#define MAX_VERTICAL_LINES 8

/**
 * 把一段文本拆成多行（竖屏使用）：
 * - 按空格拆行（农历文本形如 `丙午年 六月廿二`）；
 * - 若拆分后仍只有一行且较长（日期形如 `2026-08-04`），再按 `-` 拆成
 *   `2026` 与 `08-04` 两行。
 */
static int _split_into_lines(const char *text,
			     char lines[][INFO_TEXT_LENGTH], int max_lines)
{
	if (text == NULL || text[0] == '\0')
		return 0;
	int count = 0;
	int len = strlen(text);
	int start = 0;
	for (int i = 0; i <= len && count < max_lines; ++i) {
		if (i == len || text[i] == ' ') {
			int seg_len = i - start;
			if (seg_len > 0) {
				memcpy(lines[count], text + start, seg_len);
				lines[count][seg_len] = '\0';
				++count;
			}
			start = i + 1;
		}
	}
	if (count == 1 && strlen(lines[0]) > 6 && count < max_lines) {
		char *dash = strchr(lines[0], '-');
		if (dash != NULL && dash != lines[0]) {
			char second[INFO_TEXT_LENGTH];
			strncpy(second, dash + 1, INFO_TEXT_LENGTH - 1);
			second[INFO_TEXT_LENGTH - 1] = '\0';
			*dash = '\0';
			strncpy(lines[1], second, INFO_TEXT_LENGTH - 1);
			lines[1][INFO_TEXT_LENGTH - 1] = '\0';
			count = 2;
		}
	}
	return count;
}

static void _draw_vertical(struct flipclock_info_bar *bar, SDL_Point offset)
{
	char lines[MAX_VERTICAL_LINES][INFO_TEXT_LENGTH];
	int count = 0;
	if (bar->date_text[0] && !bar->hide_date)
		count += _split_into_lines(bar->date_text, &lines[count],
					   MAX_VERTICAL_LINES - count);
	if (bar->weekday_text[0] && count < MAX_VERTICAL_LINES) {
		strncpy(lines[count], bar->weekday_text, INFO_TEXT_LENGTH - 1);
		lines[count][INFO_TEXT_LENGTH - 1] = '\0';
		++count;
	}
	if (bar->lunar_text[0])
		count += _split_into_lines(bar->lunar_text, &lines[count],
					   MAX_VERTICAL_LINES - count);
	/* 天气并入信息栏时（方案2 左栏）作为单独一行追加，不按空格拆分。 */
	if (bar->show_weather_segment && bar->weather_text[0] &&
	    count < MAX_VERTICAL_LINES) {
		strncpy(lines[count], bar->weather_text, INFO_TEXT_LENGTH - 1);
		lines[count][INFO_TEXT_LENGTH - 1] = '\0';
		++count;
	}
	if (count == 0)
		return;

	SDL_Texture *textures[MAX_VERTICAL_LINES] = { NULL };
	int widths[MAX_VERTICAL_LINES] = { 0 };
	int heights[MAX_VERTICAL_LINES] = { 0 };
	int total_h = 0;
	int max_w = 0;
	int gap = bar->rect.w / 3;
	for (int i = 0; i < count; ++i) {
		textures[i] = _render_line(bar, lines[i], &widths[i],
					   &heights[i]);
		total_h += heights[i];
		if (widths[i] > max_w)
			max_w = widths[i];
	}
	total_h += gap * (count - 1);

	/* 竖屏：整体垂直居中，水平方向以最宽一行为准居中对齐。 */
	int start_y = bar->rect.y + offset.y + (bar->rect.h - total_h) / 2;
	int base_x = bar->rect.x + offset.x + (bar->rect.w - max_w) / 2;
	int cur_y = start_y;
	for (int i = 0; i < count; ++i) {
		if (textures[i] == NULL) {
			cur_y += heights[i] + gap;
			continue;
		}
		SDL_Rect dst = { base_x + (max_w - widths[i]) / 2, cur_y,
				 widths[i], heights[i] };
		SDL_RenderCopy(bar->renderer, textures[i], NULL, &dst);
		SDL_DestroyTexture(textures[i]);
		cur_y += heights[i] + gap;
	}
}

/* 把 seg 追加到 buf（cap 为总容量），已有内容时先补一个空格；
   返回新的长度（含截断保护，不会越界）。 */
static size_t _append_segment(char *buf, size_t cap, size_t len,
			      const char *seg)
{
	if (seg == NULL || seg[0] == '\0')
		return len;
	if (len >= cap)
		return len;
	if (len > 0)
		buf[len++] = ' ';
	if (len >= cap)
		return len;
	int w = snprintf(buf + len, cap - len, "%s", seg);
	if (w < 0)
		w = 0;
	if ((size_t)w >= cap - len)
		w = (int)(cap - len) - 1;
	return len + (size_t)w;
}

/* 双行/双列各区的独立字体对象：字号不变则复用，变化才重开（设计 §8.3）。 */
static TTF_Font *_get_line_font(struct flipclock_info_bar *bar, int idx, int px)
{
	if (px <= 0)
		return NULL;
	if (bar->font_line[idx] != NULL && bar->font_px_line[idx] == px)
		return bar->font_line[idx];
	if (bar->font_line[idx] != NULL) {
		TTF_CloseFont(bar->font_line[idx]);
		bar->font_line[idx] = NULL;
	}
	/* idx=1 为第二排（横屏第二行/竖屏右列），用第二排字体。 */
	const char *line_path = (idx == 1 && bar->app->weather_font_path[0] != '\0')
				 ? bar->app->weather_font_path : NULL;
	bar->font_line[idx] = _open_font_path(bar, px, line_path);
	bar->font_px_line[idx] = px;
	return bar->font_line[idx];
}

static void _flipclock_info_bar_close_line_fonts(struct flipclock_info_bar *bar)
{
	for (int i = 0; i < 2; ++i) {
		if (bar->font_line[i] != NULL) {
			TTF_CloseFont(bar->font_line[i]);
			bar->font_line[i] = NULL;
		}
		bar->font_px_line[i] = 0;
	}
}

/* 横屏双行的单行渲染：独立双向自适应字号，居中于上/下半区。 */
static void _draw_horizontal_line(struct flipclock_info_bar *bar, int line_idx,
				  const char *text, SDL_Point offset)
{
	if (text == NULL || text[0] == '\0')
		return;
	/* 两行之间留一点间距，避免视觉上挤在一起。 */
	int gap = (int)(bar->rect.h * 0.08);
	int line_h = (bar->rect.h - gap) / 2;
	if (line_h <= 0)
		return;

	int px = (int)(line_h * 0.85 * bar->app->info_scale);
	if (px < 8)
		px = 8;
	const int max_px = (int)(line_h * 1.1);
	const int min_px = 8;
	const double max_w = bar->rect.w * 0.92;
	for (int attempt = 0; attempt < 12; ++attempt) {
		TTF_Font *font = _get_line_font(bar, line_idx, px);
		if (font == NULL)
			return;
		int total_w = 0, h = 0;
		SDL_Texture *t =
			_render_line_font(bar, font, text, &total_w, &h);
		if (t != NULL)
			SDL_DestroyTexture(t);
		if (total_w <= 0)
			break;
		if (total_w > max_w) {
			int new_px = (int)(px * (max_w / total_w));
			if (new_px >= px)
				break;
			px = new_px < min_px ? min_px : new_px;
		} else if (total_w < bar->rect.w * 0.85 && px < max_px) {
			int new_px = (int)(px * (bar->rect.w * 0.92 / total_w));
			if (new_px <= px)
				new_px = px + 1;
			if (new_px > max_px)
				new_px = max_px;
			px = new_px;
		} else {
			break;
		}
	}
	TTF_Font *font = _get_line_font(bar, line_idx, px);
	if (font == NULL)
		return;
	int w = 0, h = 0;
	SDL_Texture *texture = _render_line_font(bar, font, text, &w, &h);
	if (texture == NULL)
		return;
	/* 行 0 居中于上半区，行 1 居中于下半区（各限 line_h 高度内）。 */
	int area_y = bar->rect.y + offset.y + line_idx * (line_h + (line_idx ? gap : 0));
	int y = area_y + (line_h - h) / 2;
	int x = bar->rect.x + offset.x + (bar->rect.w - w) / 2;
	SDL_Rect dst = { x, y, w, h };
	SDL_RenderCopy(bar->renderer, texture, NULL, &dst);
	SDL_DestroyTexture(texture);
}

/* 横屏双层：第 1 行 = 日期 + 天气 + 星期；第 2 行 = 农历 + 节气。
   空段跳过，分隔符仅在相邻非空段间；每行独立自适应字号（设计 §4.3）。 */
static void _draw_horizontal_two_line(struct flipclock_info_bar *bar,
				      SDL_Point offset)
{
	char line1[INFO_TEXT_LENGTH * 3] = "";
	char line2[INFO_TEXT_LENGTH * 2] = "";
	size_t l1 = 0, l2 = 0;
	/* 有节气内容时天气在第1行（日期+星期+天气），第2行=农历+节气；
	   无节气内容时天气下移到第2行（日期+星期 / 农历+天气），避免第2行
	   只有农历显得空。星期始终在天气之前。 */
	bool has_solar = bar->solar_term_text[0] != '\0';
	bool weather_in_line1 = has_solar && bar->show_weather_segment &&
				bar->weather_text[0] != '\0';
	bool weather_in_line2 = !has_solar && bar->show_weather_segment &&
				bar->weather_text[0] != '\0';

	if (bar->date_text[0] && !bar->hide_date)
		l1 = _append_segment(line1, sizeof(line1), l1, bar->date_text);
	if (bar->weekday_text[0])
		l1 = _append_segment(line1, sizeof(line1), l1,
				     bar->weekday_text);
	if (weather_in_line1)
		l1 = _append_segment(line1, sizeof(line1), l1,
				     bar->weather_text);
	if (bar->lunar_text[0])
		l2 = _append_segment(line2, sizeof(line2), l2, bar->lunar_text);
	if (bar->solar_term_text[0])
		l2 = _append_segment(line2, sizeof(line2), l2,
				     bar->solar_term_text);
	if (weather_in_line2)
		l2 = _append_segment(line2, sizeof(line2), l2,
				     bar->weather_text);

	_draw_horizontal_line(bar, 0, line1, offset);
	_draw_horizontal_line(bar, 1, line2, offset);
}

/* 竖屏双列：左列 = 日期(可选)/农历/节气，右列 = 星期/天气；各列独立字号
   与居中，排版规则沿用传统竖排（info_vertical=true 时，设计 §4.3）。 */
static void _draw_vertical_two_column(struct flipclock_info_bar *bar,
				      SDL_Point offset)
{
	char left[INFO_TEXT_LENGTH * 2 + 2] = "";
	char right[INFO_TEXT_LENGTH * 2 + 2] = "";
	size_t l = 0, r = 0;

	/* 左列：日期（date_on_top=false 且未隐藏时）+ 农历 + 节气。 */
	if (bar->date_text[0] && !bar->hide_date &&
	    !bar->app->date_on_top_portrait)
		l = _append_segment(left, sizeof(left), l, bar->date_text);
	if (bar->lunar_text[0])
		l = _append_segment(left, sizeof(left), l, bar->lunar_text);
	if (bar->solar_term_text[0])
		l = _append_segment(left, sizeof(left), l,
				    bar->solar_term_text);

	/* 右列：星期 + 天气。 */
	if (bar->weekday_text[0])
		r = _append_segment(right, sizeof(right), r, bar->weekday_text);
	if (bar->show_weather_segment && bar->weather_text[0])
		r = _append_segment(right, sizeof(right), r,
				    bar->weather_text);

	SDL_Rect left_rect = { bar->rect.x, bar->rect.y, bar->rect.w / 2,
			       bar->rect.h };
	SDL_Rect right_rect = { bar->rect.x + bar->rect.w / 2, bar->rect.y,
				bar->rect.w - bar->rect.w / 2, bar->rect.h };
	if (l > 0)
		_draw_vertical_typography_text(bar, left_rect,
					       &bar->font_line[0],
					       &bar->font_px_line[0],
					       left, offset, NULL);
	if (r > 0) {
		/* 右列（星期+天气）用"第二排字体"，未设置时回退信息栏字体。 */
		const char *right_font = bar->app->weather_font_path[0] != '\0'
						 ? bar->app->weather_font_path
						 : NULL;
		_draw_vertical_typography_text(bar, right_rect,
					       &bar->font_line[1],
					       &bar->font_px_line[1],
					       right, offset, right_font);
	}
}

/**
 * 渲染竖排全部字形并计算整体尺寸（总高/最宽列宽）。重复调用时先销毁
 * 上一批纹理，供字号调整后重新渲染使用。
 */
/* 把一段连续的非 CJK 子串作为整体横向渲染（不旋转），用于竖排时让温度
   数字「28」横向书写。失败时按估算尺寸占位，避免布局塌缩。 */
static void _typo_push_run_horizontal(const struct flipclock_info_bar *bar,
				      TTF_Font *font, int px,
				      struct typo_glyph glyphs[], int *count,
				      const char *text, int len)
{
	char buf[64];
	if (len > (int)sizeof(buf) - 1)
		len = (int)sizeof(buf) - 1;
	memcpy(buf, text, len);
	buf[len] = '\0';
	glyphs[*count].rotated = false;
	glyphs[*count].texture =
		_render_line_font(bar, font, buf, &glyphs[*count].w,
				  &glyphs[*count].h);
	if (glyphs[*count].texture == NULL) {
		glyphs[*count].w = px * len;
		glyphs[*count].h = px;
	}
	++(*count);
}

/* 把一段非 CJK 子串按逐字竖排（每字旋转 90°）渲染，用于过长串
   （如日期「2026-08-23」）回落到原有行为，避免横向溢出窄栏。 */
static void _typo_push_run_rotated(const struct flipclock_info_bar *bar,
				   TTF_Font *font, int px,
				   struct typo_glyph glyphs[], int *count,
				   const char *text, int len)
{
	const char *p = text;
	const char *end = text + len;
	while (p < end && *count < MAX_TYPO_GLYPHS) {
		int clen;
		uint32_t cp = _utf8_decode_char(p, &clen);
		if (clen <= 0)
			break;
		p += clen;
		glyphs[*count].rotated = true;
		glyphs[*count].texture =
			_render_glyph_font(bar, font, cp, true,
					   &glyphs[*count].w,
					   &glyphs[*count].h);
		if (glyphs[*count].texture == NULL) {
			glyphs[*count].w = px;
			glyphs[*count].h = (int)(px * 0.6);
		}
		++(*count);
	}
}

static void _typo_render_glyphs(const struct flipclock_info_bar *bar,
				TTF_Font *font, int px,
				const struct typo_group groups[], int g,
				struct typo_glyph glyphs[], int group_end[],
				int *out_count, int *out_total_h,
				int *out_col_w)
{
	int count = 0;
	for (int i = 0; i < MAX_TYPO_GLYPHS; ++i) {
		if (glyphs[i].texture != NULL) {
			SDL_DestroyTexture(glyphs[i].texture);
			glyphs[i].texture = NULL;
		}
	}
	for (int i = 0; i < g && count < MAX_TYPO_GLYPHS; ++i) {
		const char *p = groups[i].text;
		const char *end = p + groups[i].length;
		const char *run_start = NULL;
		int run_len = 0;	/* 累计字节数 */
		int run_cp = 0;		/* 累计码点数 */
		while (p < end && count < MAX_TYPO_GLYPHS) {
			int len;
			uint32_t cp = _utf8_decode_char(p, &len);
			if (len <= 0)
				break;
			if (_is_cjk(cp)) {
				/* 遇到汉字前，先把累积的非 CJK 串输出。 */
				if (run_len > 0) {
					if (run_cp <= MAX_TYPO_RUN_CP)
						_typo_push_run_horizontal(
							bar, font, px, glyphs,
							&count,
							run_start,
							run_len);
					else
						_typo_push_run_rotated(
							bar, font, px, glyphs,
							&count,
							run_start,
							run_len);
					run_len = 0;
					run_cp = 0;
					run_start = NULL;
				}
				glyphs[count].rotated = false;
				glyphs[count].texture =
					_render_glyph_font(bar, font, cp, false,
							  &glyphs[count].w,
							  &glyphs[count].h);
				if (glyphs[count].texture == NULL) {
					glyphs[count].w = px;
					glyphs[count].h = px;
				}
				++count;
			} else {
				if (run_len == 0)
					run_start = p;
				run_len += len;
				++run_cp;
			}
			p += len;
		}
		if (run_len > 0 && count < MAX_TYPO_GLYPHS) {
			if (run_cp <= MAX_TYPO_RUN_CP)
				_typo_push_run_horizontal(bar, font, px, glyphs,
							  &count, run_start,
							  run_len);
			else
				_typo_push_run_rotated(bar, font, px, glyphs,
						       &count, run_start,
						       run_len);
		}
		group_end[i] = count;
	}

	int gap = (int)(px * 0.6);
	int glyph_gap = (int)(px * TYPO_GLYPH_SPACING);
	int total_h = glyph_gap * (count - g) + gap * (g - 1);
	int col_w = 0;
	for (int i = 0; i < count; ++i) {
		total_h += glyphs[i].h;
		if (glyphs[i].w > col_w)
			col_w = glyphs[i].w;
	}
	if (out_count != NULL)
		*out_count = count;
	if (out_total_h != NULL)
		*out_total_h = total_h;
	if (out_col_w != NULL)
		*out_col_w = col_w;
}

/**
 * 传统竖排文字：字符自上而下逐字排列。
 * - CJK 汉字保持直立；
 * - ASCII/数字等旋转 90°，上端朝右（angle = 90），从右往左读；
 * - 组间（日期 / 星期 / 农历各组）留空隙，整列垂直居中、水平居中。
 * - 防御：不同字体的实际字形高度与估算存在偏差，若整列仍超出信息栏
 *   高度，则按比例缩小字号后重新渲染，避免顶部被裁切。
 */
/* 通用竖排渲染：text 为 NULL 时从 bar 收集现有文本段，否则按空格拆组；
   逐字竖排到 col_rect 内居中。字体经 font_ref/px_ref 传入（防御缩小时
   更新，调用者缓存因此同步）；px_ref 初值 <=0 时按 _typography_font_px
   同式估算（px_w = 列宽*0.9、px_h = 列高约束）。 */
static void _draw_vertical_typography_text(struct flipclock_info_bar *bar,
					   SDL_Rect col_rect,
					   TTF_Font **font_ref, int *px_ref,
					   const char *text, SDL_Point offset,
					   const char *font_path)
{
	if (font_ref == NULL || px_ref == NULL)
		return;

	struct typo_group groups[MAX_TYPO_GROUPS];
	int g;
	if (text != NULL) {
		if (text[0] == '\0')
			return;
		g = 0;
		const char *p = text;
		const char *seg_start = p;
		for (;;) {
			if (*p == ' ' || *p == '\0') {
				if (p > seg_start && g < MAX_TYPO_GROUPS) {
					groups[g].text = seg_start;
					groups[g].length = (int)(p - seg_start);
					++g;
				}
				seg_start = p + 1;
				if (*p == '\0')
					break;
			}
			++p;
		}
	} else {
		g = _typo_collect_groups(bar, groups, MAX_TYPO_GROUPS);
	}
	if (g == 0)
		return;

	if (*px_ref <= 0) {
		*px_ref = _typography_font_px_from(bar, col_rect.w,
						   col_rect.h, groups, g);
		if (*px_ref < 8)
			*px_ref = 8;
	}
	if (*font_ref == NULL) {
		*font_ref = _open_font_path(bar, *px_ref, font_path);
		if (*font_ref == NULL)
			return;
	}

	struct typo_glyph glyphs[MAX_TYPO_GLYPHS] = { { 0 } };
	int group_end[MAX_TYPO_GROUPS] = { 0 };
	int count, total_h, col_w;
	_typo_render_glyphs(bar, *font_ref, *px_ref, groups, g, glyphs,
			    group_end, &count, &total_h, &col_w);
	if (count == 0)
		return;

	if (total_h > col_rect.h && *px_ref > 8) {
		int new_px = (int)((double)*px_ref * col_rect.h / total_h);
		if (new_px < 8)
			new_px = 8;
		if (new_px != *px_ref) {
			*px_ref = new_px;
			TTF_CloseFont(*font_ref);
			*font_ref = _open_font_path(bar, new_px, font_path);
			if (*font_ref == NULL)
				return;
			_typo_render_glyphs(bar, *font_ref, *px_ref, groups, g,
					    glyphs, group_end, &count, &total_h,
					    &col_w);
		}
	}
	if (count == 0)
		return;

	int gap = (int)(*px_ref * 0.6);
	int glyph_gap = (int)(*px_ref * TYPO_GLYPH_SPACING);
	int start_y = col_rect.y + offset.y + (col_rect.h - total_h) / 2;
	int x = col_rect.x + offset.x + (col_rect.w - col_w) / 2;
	int cur_y = start_y;
	int glyph_i = 0;
	for (int i = 0; i < g; ++i) {
		for (; glyph_i < group_end[i]; ++glyph_i) {
			struct typo_glyph *gly = &glyphs[glyph_i];
			SDL_Rect dst = { x + (col_w - gly->w) / 2, cur_y,
					 gly->w, gly->h };
			if (gly->texture != NULL) {
				if (gly->rotated)
					SDL_RenderCopyEx(bar->renderer,
							 gly->texture, NULL,
							 &dst, 90, NULL,
							 SDL_FLIP_NONE);
				else
					SDL_RenderCopy(bar->renderer,
						       gly->texture, NULL,
						       &dst);
				SDL_DestroyTexture(gly->texture);
			}
			cur_y += gly->h;
			/* 组内字形之间加一点间距，组末不加（由组间空隙接管）。 */
			if (glyph_i + 1 < group_end[i])
				cur_y += glyph_gap;
		}
		if (i < g - 1)
			cur_y += gap;
	}
}

/* 单列竖排（沿用 bar->rect / bar->font），兼容现有调用。 */
static void _draw_vertical_typography(struct flipclock_info_bar *bar,
				      SDL_Point offset)
{
	_draw_vertical_typography_text(bar, bar->rect, &bar->font,
				       &bar->font_px, NULL, offset, NULL);
}

void flipclock_info_bar_draw(struct flipclock_info_bar *bar, SDL_Point offset)
{
	RETURN_IF_FAIL(bar != NULL);

	if (!bar->enabled || bar->font == NULL)
		return;
	if (bar->rect.w <= 0 || bar->rect.h <= 0)
		return;

	/* 双行/双列仅在当天有节气/三伏/九九内容时激活（设计 §3.3）。 */
	/* two_line_info 开启即按双行/双列绘制（无节气时天气下移第2行/右列），
	   与 layout 预留一致，避免冷启动/无节气日单行挤在一起。 */
	bar->two_line = bar->app->two_line_info;

	if (bar->horizontal) {
		if (bar->two_line)
			_draw_horizontal_two_line(bar, offset);
		else
			_draw_horizontal(bar, offset);
	} else if (bar->app->info_vertical && !bar->horizontal_stack) {
		if (bar->two_line)
			_draw_vertical_two_column(bar, offset);
		else
			_draw_vertical_typography(bar, offset);
	} else {
		_draw_vertical(bar, offset);
	}
}

void flipclock_info_bar_destroy(struct flipclock_info_bar *bar)
{
	RETURN_IF_FAIL(bar != NULL);

	_flipclock_info_bar_close_line_fonts(bar);
	_flipclock_info_bar_close_font(bar);
	free(bar);
}
