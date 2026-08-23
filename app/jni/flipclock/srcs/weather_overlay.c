/**
 * 天气覆盖层：在信息栏和时间层之间的空白区域显示天气。
 *
 * 使用自定义 CJK 字体（默认霞鹜新智宋），将温度和天气状况渲染为单行文本，
 * 例如 "24°C 晴"。
 *
 * 布局方向由绘制区域决定（横屏横条、竖屏竖条）：
 * - 横屏：单行横向文本，在区域内水平、垂直居中；
 * - 竖屏：传统竖排，字符自上而下逐字排列（CJK 直立、ASCII 旋转 90°），
 *   与信息栏竖排风格一致。
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include <SDL.h>
#include <SDL_ttf.h>

#include "flipclock.h"
#include "weather_overlay.h"

#define WEATHER_TEXT_LENGTH 128
#define MIN_FONT_PX 12
#define MAX_FONT_PX 120
/* 竖排相邻字形之间的垂直间距（相对字号的倍数）。 */
#define WEATHER_GLYPH_SPACING 0.15

struct flipclock_weather_overlay {
	struct flipclock *app;
	SDL_Renderer *renderer;
	TTF_Font *font;
	int font_px;
	int rect_w;
	int rect_h;
	/* 绘制区域是否为竖条（竖屏），决定横向单行还是纵向竖排。 */
	bool vertical;
	char temperature_text[WEATHER_TEXT_LENGTH];
	char description_text[WEATHER_TEXT_LENGTH];
	/* 横向布局使用的单行纹理。 */
	SDL_Texture *texture;
	int texture_w;
	int texture_h;
	/* 竖排布局使用的整列纹理缓存（含旋转后的显示尺寸）。 */
	SDL_Texture *vertical_texture;
	int vertical_texture_w;
	int vertical_texture_h;
	bool text_dirty;
	bool enabled;
};

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

static void _close_font(struct flipclock_weather_overlay *overlay)
{
	if (overlay->font != NULL) {
		TTF_CloseFont(overlay->font);
		overlay->font = NULL;
	}
}

static void _open_font(struct flipclock_weather_overlay *overlay, int rect_w,
		       int rect_h)
{
	_close_font(overlay);

	const struct flipclock *app = overlay->app;

	const char *font_path = NULL;
	if (app->weather_font_path[0] != '\0') {
		font_path = app->weather_font_path;
	} else if (app->cjk_font_path[0] != '\0') {
		font_path = app->cjk_font_path;
	} else {
		LOG_ERROR("Weather overlay: no CJK font path configured.\n");
		overlay->enabled = false;
		return;
	}

	int min_side = rect_w < rect_h ? rect_w : rect_h;
	int px = (int)(min_side * 0.6 * app->info_scale);
	if (px < MIN_FONT_PX)
		px = MIN_FONT_PX;
	if (px > MAX_FONT_PX)
		px = MAX_FONT_PX;
	overlay->font_px = px;

	overlay->font = TTF_OpenFont(font_path, px);
	if (overlay->font == NULL) {
		LOG_ERROR("Weather overlay: failed to open CJK font `%s`: %s\n",
			  font_path, TTF_GetError());
		overlay->enabled = false;
		return;
	}
	TTF_SetFontStyle(overlay->font, TTF_STYLE_BOLD);
	overlay->rect_w = rect_w;
	overlay->rect_h = rect_h;
	/* 区域宽高比决定排版方向，字号变化时一并刷新。 */
	overlay->vertical = (rect_h > rect_w);
	overlay->enabled = true;
}

static SDL_Texture *_render_line(struct flipclock_weather_overlay *overlay,
				 const char *text, int *out_w, int *out_h)
{
	if (text == NULL || text[0] == '\0' || overlay->font == NULL)
		return NULL;

	SDL_Color color = overlay->app->text_color;
	SDL_Surface *surface = TTF_RenderUTF8_Blended(overlay->font, text, color);
	if (surface == NULL) {
		LOG_ERROR("Weather overlay: TTF_RenderUTF8_Blended failed: %s\n",
			  TTF_GetError());
		return NULL;
	}

	SDL_Texture *texture =
		SDL_CreateTextureFromSurface(overlay->renderer, surface);
	if (out_w != NULL)
		*out_w = surface->w;
	if (out_h != NULL)
		*out_h = surface->h;
	SDL_FreeSurface(surface);
	if (texture == NULL) {
		LOG_ERROR("Weather overlay: SDL_CreateTextureFromSurface failed: %s\n",
			  SDL_GetError());
	}
	return texture;
}

static void _rebuild_texture(struct flipclock_weather_overlay *overlay)
{
	if (overlay->texture != NULL) {
		SDL_DestroyTexture(overlay->texture);
		overlay->texture = NULL;
	}

	char combined[WEATHER_TEXT_LENGTH];
	combined[0] = '\0';
	if (overlay->temperature_text[0] != '\0') {
		strncat(combined, overlay->temperature_text,
			WEATHER_TEXT_LENGTH - strlen(combined) - 1);
	}
	if (overlay->temperature_text[0] != '\0' &&
	    overlay->description_text[0] != '\0') {
		strncat(combined, " ", WEATHER_TEXT_LENGTH - strlen(combined) - 1);
	}
	if (overlay->description_text[0] != '\0') {
		strncat(combined, overlay->description_text,
			WEATHER_TEXT_LENGTH - strlen(combined) - 1);
	}

	overlay->texture = _render_line(overlay, combined, &overlay->texture_w,
					&overlay->texture_h);
	overlay->text_dirty = false;
}

struct flipclock_weather_overlay *
flipclock_weather_overlay_create(struct flipclock *app, SDL_Renderer *renderer)
{
	RETURN_VAL_IF_FAIL(app != NULL, NULL);
	RETURN_VAL_IF_FAIL(renderer != NULL, NULL);

	struct flipclock_weather_overlay *overlay = malloc(sizeof(*overlay));
	if (overlay == NULL) {
		LOG_ERROR("Failed to create weather overlay!\n");
		return NULL;
	}
	overlay->app = app;
	overlay->renderer = renderer;
	overlay->font = NULL;
	overlay->font_px = 0;
	overlay->rect_w = 0;
	overlay->rect_h = 0;
	overlay->vertical = false;
	overlay->temperature_text[0] = '\0';
	overlay->description_text[0] = '\0';
	overlay->texture = NULL;
	overlay->texture_w = 0;
	overlay->texture_h = 0;
	overlay->vertical_texture = NULL;
	overlay->vertical_texture_w = 0;
	overlay->vertical_texture_h = 0;
	overlay->text_dirty = false;
	overlay->enabled = false;
	return overlay;
}

void flipclock_weather_overlay_set_text(
	struct flipclock_weather_overlay *overlay, const char *temperature,
	const char *description)
{
	RETURN_IF_FAIL(overlay != NULL);

	if (temperature != NULL) {
		strncpy(overlay->temperature_text, temperature, WEATHER_TEXT_LENGTH);
		overlay->temperature_text[WEATHER_TEXT_LENGTH - 1] = '\0';
	}
	if (description != NULL) {
		strncpy(overlay->description_text, description, WEATHER_TEXT_LENGTH);
		overlay->description_text[WEATHER_TEXT_LENGTH - 1] = '\0';
	}
	overlay->text_dirty = true;
}

/**
 * 竖排绘制：逐字解码温度与天气描述（跳过空格），自上而下排列；
 * CJK 汉字直立，ASCII（数字、`°`、`C` 等）顺时针旋转 90°，
 * 整列在绘制区域内水平、垂直居中。
 *
 * 字形按需实时渲染并立即销毁（天气文本仅在刷新时变更，逐帧重建开销可忽略，
 * 也避免在 overlay 中维护字形纹理缓存及其失效逻辑）。
 */
static void _draw_vertical_typography(struct flipclock_weather_overlay *overlay,
				      SDL_Rect rect)
{
	uint32_t cps[WEATHER_TEXT_LENGTH];
	int n = 0;
	const char *parts[2] = { overlay->temperature_text,
				 overlay->description_text };
	for (int i = 0; i < 2 && n < WEATHER_TEXT_LENGTH; ++i) {
		const char *p = parts[i];
		while (*p != '\0' && n < WEATHER_TEXT_LENGTH) {
			int len;
			uint32_t cp = _utf8_decode_char(p, &len);
			if (len <= 0)
				break;
			p += len;
			/* 温度与描述之间的空格仅作横向排版分隔，竖排时跳过。 */
			if (cp == ' ')
				continue;
			cps[n++] = cp;
		}
	}
	if (n == 0)
		return;

	SDL_Renderer *renderer = overlay->renderer;
	SDL_Color color = overlay->app->text_color;
	int glyph_gap = (int)(overlay->font_px * WEATHER_GLYPH_SPACING);

	/*
	 * 估算整列总高：直立汉字约 1.25 倍字号、旋转字符约 0.6 倍字号，
	 * 并计入字形间距；若超出区域高度则按比例缩小字号重开字体。
	 */
	for (;;) {
		double units = 0;
		for (int i = 0; i < n; ++i)
			units += _is_cjk(cps[i]) ? 1.25 : 0.6;
		units += WEATHER_GLYPH_SPACING * (n - 1);
		int est_h = (int)(overlay->font_px * units);
		if (est_h <= rect.h || overlay->font_px <= MIN_FONT_PX)
			break;
		int new_px = (int)((double)overlay->font_px * rect.h / est_h);
		if (new_px < MIN_FONT_PX)
			new_px = MIN_FONT_PX;
		if (new_px == overlay->font_px)
			break;
		_close_font(overlay);
		overlay->font_px = new_px;
		overlay->font = TTF_OpenFont(
			overlay->app->weather_font_path[0] != '\0'
				? overlay->app->weather_font_path
				: overlay->app->cjk_font_path,
			new_px);
		if (overlay->font == NULL) {
			LOG_ERROR("Weather overlay: failed to reopen font: %s\n",
				  TTF_GetError());
			overlay->enabled = false;
			return;
		}
		TTF_SetFontStyle(overlay->font, TTF_STYLE_BOLD);
		glyph_gap = (int)(overlay->font_px * WEATHER_GLYPH_SPACING);
	}

	/* 逐字渲染，统计实际总高与最宽字形宽度。 */
	SDL_Texture *textures[WEATHER_TEXT_LENGTH] = { NULL };
	int widths[WEATHER_TEXT_LENGTH] = { 0 };
	int heights[WEATHER_TEXT_LENGTH] = { 0 };
	bool rotated[WEATHER_TEXT_LENGTH] = { false };
	int total_h = glyph_gap * (n - 1);
	int max_w = 0;
	for (int i = 0; i < n; ++i) {
		char utf8[5];
		int len = 0;
		uint32_t cp = cps[i];
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

		rotated[i] = !_is_cjk(cp);
		SDL_Surface *surface =
			TTF_RenderUTF8_Blended(overlay->font, utf8, color);
		if (surface == NULL) {
			LOG_ERROR("Weather overlay: render glyph failed: %s\n",
				  TTF_GetError());
			continue;
		}
		textures[i] = SDL_CreateTextureFromSurface(renderer, surface);
		if (rotated[i]) {
			/* 旋转后显示尺寸为「原高 × 原宽」。 */
			widths[i] = surface->h;
			heights[i] = surface->w;
		} else {
			widths[i] = surface->w;
			heights[i] = surface->h;
		}
		SDL_FreeSurface(surface);
		total_h += heights[i];
		if (widths[i] > max_w)
			max_w = widths[i];
	}
	if (max_w == 0) {
		for (int i = 0; i < n; ++i) {
			if (textures[i] != NULL)
				SDL_DestroyTexture(textures[i]);
		}
		return;
	}

	/*
	 * 逐字合成到一张 RGBA 目标纹理上，整列作为单一纹理缓存绘制，
	 * 避免逐帧重复渲染字形；旋转字形通过 RenderCopyEx 合成。
	 */
	if (overlay->vertical_texture != NULL) {
		SDL_DestroyTexture(overlay->vertical_texture);
		overlay->vertical_texture = NULL;
	}
	SDL_Texture *target = SDL_CreateTexture(
		renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_TARGET,
		max_w, total_h);
	if (target == NULL) {
		LOG_ERROR("Weather overlay: create vertical target failed: %s\n",
			  SDL_GetError());
		for (int i = 0; i < n; ++i) {
			if (textures[i] != NULL)
				SDL_DestroyTexture(textures[i]);
		}
		return;
	}
	SDL_SetTextureBlendMode(target, SDL_BLENDMODE_BLEND);
	SDL_Texture *prev_target = SDL_GetRenderTarget(renderer);
	SDL_SetRenderTarget(renderer, target);
	SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
	SDL_RenderClear(renderer);
	int cur_y = 0;
	for (int i = 0; i < n; ++i) {
		if (textures[i] == NULL) {
			cur_y += heights[i] + glyph_gap;
			continue;
		}
		SDL_Rect dst = { (max_w - widths[i]) / 2, cur_y, widths[i],
				 heights[i] };
		if (rotated[i])
			SDL_RenderCopyEx(renderer, textures[i], NULL, &dst, 90,
					 NULL, SDL_FLIP_NONE);
		else
			SDL_RenderCopy(renderer, textures[i], NULL, &dst);
		SDL_DestroyTexture(textures[i]);
		cur_y += heights[i] + glyph_gap;
	}
	SDL_SetRenderTarget(renderer, prev_target);

	overlay->vertical_texture = target;
	overlay->vertical_texture_w = max_w;
	overlay->vertical_texture_h = total_h;

	SDL_Rect dst = {
		rect.x + (rect.w - max_w) / 2,
		rect.y + (rect.h - total_h) / 2,
		max_w,
		total_h
	};
	SDL_RenderCopy(renderer, target, NULL, &dst);
}

void flipclock_weather_overlay_draw(struct flipclock_weather_overlay *overlay,
				    SDL_Rect rect)
{
	RETURN_IF_FAIL(overlay != NULL);

	if (!overlay->app->show_weather)
		return;

	if (rect.w <= 0 || rect.h <= 0)
		return;

	if (rect.w != overlay->rect_w || rect.h != overlay->rect_h ||
	    overlay->font == NULL) {
		_open_font(overlay, rect.w, rect.h);
		overlay->text_dirty = true;
		/* 方向/尺寸变化后旧的另一方向纹理缓存不再适用。 */
		if (overlay->vertical_texture != NULL) {
			SDL_DestroyTexture(overlay->vertical_texture);
			overlay->vertical_texture = NULL;
		}
		if (overlay->texture != NULL) {
			SDL_DestroyTexture(overlay->texture);
			overlay->texture = NULL;
		}
	}

	if (!overlay->enabled || overlay->font == NULL)
		return;

	/*
	 * 竖屏（竖条区域）：传统竖排，文本/字号变化时重建整列纹理缓存，
	 * 其余帧直接绘制缓存；横屏：单行横向文本，同样仅在变更时重建。
	 */
	if (overlay->vertical) {
		if (overlay->text_dirty || overlay->vertical_texture == NULL) {
			_draw_vertical_typography(overlay, rect);
			overlay->text_dirty = false;
			return;
		}
		SDL_Rect dst = {
			rect.x + (rect.w - overlay->vertical_texture_w) / 2,
			rect.y + (rect.h - overlay->vertical_texture_h) / 2,
			overlay->vertical_texture_w,
			overlay->vertical_texture_h
		};
		SDL_RenderCopy(overlay->renderer, overlay->vertical_texture,
			       NULL, &dst);
		return;
	}

	if (overlay->text_dirty)
		_rebuild_texture(overlay);

	if (overlay->texture == NULL)
		return;

	SDL_Rect dst = {
		rect.x + (rect.w - overlay->texture_w) / 2,
		rect.y + (rect.h - overlay->texture_h) / 2,
		overlay->texture_w,
		overlay->texture_h
	};
	SDL_RenderCopy(overlay->renderer, overlay->texture, NULL, &dst);
}

void flipclock_weather_overlay_destroy(struct flipclock_weather_overlay *overlay)
{
	RETURN_IF_FAIL(overlay != NULL);

	if (overlay->texture != NULL)
		SDL_DestroyTexture(overlay->texture);
	if (overlay->vertical_texture != NULL)
		SDL_DestroyTexture(overlay->vertical_texture);
	_close_font(overlay);
	free(overlay);
}
