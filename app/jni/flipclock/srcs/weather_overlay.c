/**
 * 天气覆盖层：在信息栏和时间层之间的空白区域显示天气。
 *
 * 使用 flipclock_cjk.ttf 字体，将温度和天气状况渲染为单行文本，
 * 例如 "24°C 晴"。
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <SDL.h>
#include <SDL_ttf.h>

#include "flipclock.h"
#include "weather_overlay.h"

#define WEATHER_TEXT_LENGTH 128
#define MIN_FONT_PX 12
#define MAX_FONT_PX 120

struct flipclock_weather_overlay {
	struct flipclock *app;
	SDL_Renderer *renderer;
	TTF_Font *font;
	int font_px;
	int rect_w;
	int rect_h;
	char temperature_text[WEATHER_TEXT_LENGTH];
	char description_text[WEATHER_TEXT_LENGTH];
	SDL_Texture *texture;
	int texture_w;
	int texture_h;
	bool text_dirty;
	bool enabled;
};

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
	if (app->cjk_font_path[0] == '\0') {
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

	overlay->font = TTF_OpenFont(app->cjk_font_path, px);
	if (overlay->font == NULL) {
		LOG_ERROR("Weather overlay: failed to open CJK font `%s`: %s\n",
			  app->cjk_font_path, TTF_GetError());
		overlay->enabled = false;
		return;
	}
	TTF_SetFontStyle(overlay->font, TTF_STYLE_BOLD);
	overlay->rect_w = rect_w;
	overlay->rect_h = rect_h;
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
	overlay->temperature_text[0] = '\0';
	overlay->description_text[0] = '\0';
	overlay->texture = NULL;
	overlay->texture_w = 0;
	overlay->texture_h = 0;
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
	}

	if (!overlay->enabled || overlay->font == NULL)
		return;

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
	_close_font(overlay);
	free(overlay);
}
