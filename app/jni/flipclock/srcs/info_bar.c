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

#include <SDL.h>
#include <SDL_ttf.h>

#include "flipclock.h"
#include "info_bar.h"
#include "lunar.h"

static const char *WEEKDAY_NAMES[7] = { "星期日", "星期一", "星期二", "星期三",
					"星期四", "星期五", "星期六" };

static void _flipclock_info_bar_close_font(struct flipclock_info_bar *bar)
{
	if (bar->font != NULL) {
		TTF_CloseFont(bar->font);
		bar->font = NULL;
	}
}

static void _flipclock_info_bar_open_font(struct flipclock_info_bar *bar,
					  int px)
{
	const struct flipclock *app = bar->app;
	if (px <= 0)
		return;
	if (app->cjk_font_path[0] == '\0') {
		LOG_ERROR("Info bar: no CJK font path configured.\n");
		return;
	}
	bar->font = TTF_OpenFont(app->cjk_font_path, px);
	if (bar->font == NULL) {
		LOG_ERROR("Info bar: failed to open CJK font `%s`: %s\n",
			  app->cjk_font_path, TTF_GetError());
		return;
	}
	/* 信息栏文字加粗，提升可读性。 */
	TTF_SetFontStyle(bar->font, TTF_STYLE_BOLD);
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
	bar->rect.x = 0;
	bar->rect.y = 0;
	bar->rect.w = 0;
	bar->rect.h = 0;
	bar->horizontal = true;
	bar->enabled = false;
	bar->date_text[0] = '\0';
	bar->weekday_text[0] = '\0';
	bar->lunar_text[0] = '\0';
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

	const struct flipclock *app = bar->app;
	int px;
	/*
	 * 农历干支年默认不显示，此时整行更短，可以放大信息栏字体；
	 * 显示年份时字号略小，避免横屏单行文字过宽。
	 */
	bool lunar_year_shown = app->show_lunar && app->show_lunar_year;
	if (horizontal) {
		/* 横屏：高度约占窗口短边 10%，文字取其中约 75%/85%。 */
		px = (int)(rect.h * (lunar_year_shown ? 0.75 : 0.85) *
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

	_flipclock_info_bar_close_font(bar);
	_flipclock_info_bar_open_font(bar, px);
	bar->enabled = (bar->font != NULL);
}

void flipclock_info_bar_refresh(struct flipclock_info_bar *bar,
				const struct tm *now, bool force)
{
	RETURN_IF_FAIL(bar != NULL);
	RETURN_IF_FAIL(now != NULL);

	if (!force && now->tm_yday == bar->last_yday &&
	    (now->tm_year + 1900) == bar->last_year)
		return;

	const struct flipclock *app = bar->app;

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

	bar->last_yday = now->tm_yday;
	bar->last_year = now->tm_year + 1900;
}

/* 使用整行 UTF-8 渲染，返回创建的 texture 与其尺寸。可能返回 NULL。 */
static SDL_Texture *_render_line(struct flipclock_info_bar *bar,
				 const char *text, int *out_w, int *out_h)
{
	if (text == NULL || text[0] == '\0' || bar->font == NULL)
		return NULL;
	SDL_Color color = bar->app->text_color;
	SDL_Surface *surface = TTF_RenderUTF8_Blended(bar->font, text, color);
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

static void _draw_horizontal(struct flipclock_info_bar *bar)
{
	/* 收集非空文本。 */
	const char *texts[3];
	int count = 0;
	if (bar->date_text[0])
		texts[count++] = bar->date_text;
	if (bar->weekday_text[0])
		texts[count++] = bar->weekday_text;
	if (bar->lunar_text[0])
		texts[count++] = bar->lunar_text;
	if (count == 0)
		return;

	SDL_Texture *textures[3] = { NULL, NULL, NULL };
	int widths[3] = { 0, 0, 0 };
	int heights[3] = { 0, 0, 0 };
	int total_w = 0;
	int max_h = 0;
	int gap = bar->rect.h / 3;
	for (int i = 0; i < count; ++i) {
		textures[i] = _render_line(bar, texts[i], &widths[i],
					   &heights[i]);
		total_w += widths[i];
		if (heights[i] > max_h)
			max_h = heights[i];
	}
	total_w += gap * (count - 1);

	int start_x = bar->rect.x + (bar->rect.w - total_w) / 2;
	int base_y = bar->rect.y + (bar->rect.h - max_h) / 2;
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

static void _draw_vertical(struct flipclock_info_bar *bar)
{
	char lines[MAX_VERTICAL_LINES][INFO_TEXT_LENGTH];
	int count = 0;
	if (bar->date_text[0])
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
	int start_y = bar->rect.y + (bar->rect.h - total_h) / 2;
	int base_x = bar->rect.x + (bar->rect.w - max_w) / 2;
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

void flipclock_info_bar_draw(struct flipclock_info_bar *bar)
{
	RETURN_IF_FAIL(bar != NULL);

	if (!bar->enabled || bar->font == NULL)
		return;
	if (bar->rect.w <= 0 || bar->rect.h <= 0)
		return;

	if (bar->horizontal)
		_draw_horizontal(bar);
	else
		_draw_vertical(bar);
}

void flipclock_info_bar_destroy(struct flipclock_info_bar *bar)
{
	RETURN_IF_FAIL(bar != NULL);

	_flipclock_info_bar_close_font(bar);
	free(bar);
}
