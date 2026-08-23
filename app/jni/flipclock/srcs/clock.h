#ifndef __CLOCK_H__
#define __CLOCK_H__

#include <stdbool.h>

#include <SDL.h>
#include <SDL_ttf.h>

struct flipclock_info_bar;
struct flipclock_weather_overlay;

enum flipclock_burn_in_state {
	BURN_IN_MOVING,
	BURN_IN_HOLD_WEATHER,
};

struct flipclock_clock {
	struct flipclock *app;
	SDL_Window *window;
	SDL_Renderer *renderer;
	struct flipclock_card *hour;
	struct flipclock_card *minute;
	struct flipclock_card *second;
	/* 日期/星期/农历信息栏，所有选项关闭时为 NULL。 */
	struct flipclock_info_bar *info_bar;
	/* 天气覆盖层，天气显示关闭时为 NULL。 */
	struct flipclock_weather_overlay *weather_overlay;
	int i;
	int w;
	int h;
	bool waiting;
	/* 天气覆盖层绘制区域：信息栏与时间卡片之间的空白区。 */
	SDL_Rect weather_rect;
	/* 方案2 竖屏置顶日期带区域，及其专用字体（横排、不旋转）。 */
	SDL_Rect date_rect;
	TTF_Font *date_font;
	int date_font_px;
	/* 防烧屏状态机，用于在最远点停留并显示天气。 */
	enum flipclock_burn_in_state burn_in_state;
	Uint32 burn_in_hold_start_ticks;
	int burn_in_peak_sign;
	int burn_in_last_peak_sign;
};

struct flipclock_clock *flipclock_clock_create(struct flipclock *app, int i);
#if defined(_WIN32)
struct flipclock_clock *flipclock_clock_create_preview(struct flipclock *app);
#endif
void flipclock_clock_set_show_second(struct flipclock_clock *clock,
				     bool show_second);
void flipclock_clock_set_fullscreen(struct flipclock_clock *clock, bool full);
void flipclock_clock_set_hour(struct flipclock_clock *clock, const char hour[],
			      bool flip);
void flipclock_clock_set_minute(struct flipclock_clock *clock,
				const char minute[], bool flip);
void flipclock_clock_set_second(struct flipclock_clock *clock,
				const char second[], bool flip);
void flipclock_clock_set_ampm(struct flipclock_clock *clock, const char ampm[]);
void flipclock_clock_handle_window_event(struct flipclock_clock *clock,
					 SDL_Event event);
void flipclock_clock_animate(struct flipclock_clock *clock);
void flipclock_clock_destroy(struct flipclock_clock *clock);

#endif
