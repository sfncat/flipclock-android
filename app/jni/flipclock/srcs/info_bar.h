#ifndef __INFO_BAR_H__
#define __INFO_BAR_H__

#include <stdbool.h>
#include <time.h>

#include <SDL.h>
#include <SDL_ttf.h>

#define INFO_TEXT_LENGTH 64

struct flipclock_info_bar {
	struct flipclock *app;
	SDL_Renderer *renderer;
	TTF_Font *font;
	SDL_Rect rect;
	bool horizontal;
	/* True 当至少一项开启且字体加载成功。 */
	bool enabled;
	char date_text[INFO_TEXT_LENGTH];
	char weekday_text[INFO_TEXT_LENGTH];
	char lunar_text[INFO_TEXT_LENGTH];
	int last_yday;
	int last_year;
};

struct flipclock_info_bar *
flipclock_info_bar_create(struct flipclock *app, SDL_Renderer *renderer);
void flipclock_info_bar_set_rect(struct flipclock_info_bar *bar, SDL_Rect rect,
				 bool horizontal);
void flipclock_info_bar_refresh(struct flipclock_info_bar *bar,
				const struct tm *now, bool force);
void flipclock_info_bar_draw(struct flipclock_info_bar *bar);
void flipclock_info_bar_destroy(struct flipclock_info_bar *bar);

#endif
