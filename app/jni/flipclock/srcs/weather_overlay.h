#ifndef __WEATHER_OVERLAY_H__
#define __WEATHER_OVERLAY_H__

#include <SDL.h>

struct flipclock;

struct flipclock_weather_overlay;

struct flipclock_weather_overlay *
flipclock_weather_overlay_create(struct flipclock *app, SDL_Renderer *renderer);
void flipclock_weather_overlay_set_text(
	struct flipclock_weather_overlay *overlay,
	const char *temperature, const char *description);
void flipclock_weather_overlay_draw(
	struct flipclock_weather_overlay *overlay, SDL_Rect rect);
void flipclock_weather_overlay_destroy(struct flipclock_weather_overlay *overlay);

#endif
