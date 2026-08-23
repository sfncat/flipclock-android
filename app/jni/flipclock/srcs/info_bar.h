#ifndef __INFO_BAR_H__
#define __INFO_BAR_H__

#include <stdbool.h>
#include <time.h>

#include <SDL.h>
#include <SDL_ttf.h>

#define INFO_TEXT_LENGTH 64
#define WEATHER_TEXT_LENGTH 128

struct flipclock_info_bar {
	struct flipclock *app;
	SDL_Renderer *renderer;
	TTF_Font *font;
	int font_px; /* 当前字体打开时的字号，用于竖排模式按需重开。 */
	SDL_Rect rect;
	bool horizontal;
	/* True 当至少一项开启且字体加载成功。 */
	bool enabled;
	char date_text[INFO_TEXT_LENGTH];
	char weekday_text[INFO_TEXT_LENGTH];
	char lunar_text[INFO_TEXT_LENGTH];
	/* 天气并入信息栏（方案1/2）时为真，绘制额外一段/一行天气文本。 */
	bool show_weather_segment;
	/* 强制横向堆叠，跳过传统竖排旋转。方案2 置顶时设 false，
	   保留左下信息栏的传统竖排（星期/农历/天气自上而下）。 */
	bool horizontal_stack;
	/* 方案2「竖屏日期置顶」开启时为真：信息栏不再绘制日期
	   （日期已移到顶部横条），仅保留星期/农历/天气。 */
	bool hide_date;
	/* 并入信息栏的天气文本（温度 + 状况，由 clock.c 每帧填充）。 */
	char weather_text[WEATHER_TEXT_LENGTH];
	int last_yday;
	int last_year;
};

struct flipclock_info_bar *
flipclock_info_bar_create(struct flipclock *app, SDL_Renderer *renderer);
void flipclock_info_bar_set_rect(struct flipclock_info_bar *bar, SDL_Rect rect,
				 bool horizontal);
void flipclock_info_bar_refresh(struct flipclock_info_bar *bar,
				const struct tm *now, bool force);
void flipclock_info_bar_draw(struct flipclock_info_bar *bar, SDL_Point offset);
void flipclock_info_bar_destroy(struct flipclock_info_bar *bar);

#endif
