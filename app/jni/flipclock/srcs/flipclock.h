/**
 * Author: Alynx Zhou <alynx.zhou@gmail.com> (https://alynx.one/)
 */
#ifndef __FLIPCLOCK_H__
#define __FLIPCLOCK_H__

#include <stdbool.h>
#include <time.h>

#include <SDL.h>
#include <SDL_mutex.h>

#if defined(_WIN32)
#	include <windows.h>
#endif

// Android APP does not generate `config.h` and use its own logger.
#if defined(__ANDROID__)
#	include <android/log.h>
#	define LOG_TAG "FlipClock"
#	if defined(__DEBUG__)
#		define LOG_DEBUG(...)                                  \
			__android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, \
					    __VA_ARGS__)
#	else
#		define LOG_DEBUG(...)
#	endif
#	define LOG_ERROR(...) \
		__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#else
#	include <stdio.h>
#	if defined(__DEBUG__)
#		define LOG_DEBUG(...) fprintf(stdout, __VA_ARGS__)
#	else
#		define LOG_DEBUG(...)
#	endif
#	define LOG_ERROR(...) fprintf(stderr, __VA_ARGS__)
#	include "config.h"
#endif

/**
 * Similiar with GLib. Those macros are only used for debug, which means a
 * failure should be a programmer error so the code should be checked.
 */
#define RETURN_IF_FAIL(EXPR)                                              \
	do {                                                              \
		if (!(EXPR)) {                                            \
			LOG_ERROR("%s: `%s` failed!\n", __func__, #EXPR); \
			return;                                           \
		}                                                         \
	} while (0)

#define RETURN_VAL_IF_FAIL(EXPR, VAL)                                     \
	do {                                                              \
		if (!(EXPR)) {                                            \
			LOG_ERROR("%s: `%s` failed!\n", __func__, #EXPR); \
			return (VAL);                                     \
		}                                                         \
	} while (0)

#define PROGRAM_TITLE "FlipClock"
#define MAX_BUFFER_LENGTH 2048

struct flipclock {
	// Structures not shared by clocks.
	struct flipclock_clock **clocks;
	// Number of clocks.
	int clocks_length;
	// Structures shared by clocks.
	struct tm now;
	SDL_Color box_color;
	SDL_Color text_color;
	SDL_Color background_color;
	char font_path[MAX_BUFFER_LENGTH];
	char conf_path[MAX_BUFFER_LENGTH];
	double text_scale;
	double card_scale;
#if defined(_WIN32)
	HWND preview_window;
	bool preview;
	bool screensaver;
	char program_dir[MAX_BUFFER_LENGTH];
#endif
	bool ampm;
	bool full;
	bool show_second;
	/* Info bar options: 主界面上的日期、星期、农历显示。 */
	bool show_date;
	bool show_weekday;
	bool show_lunar;
	bool show_lunar_year; /* 农历是否显示干支年，默认关闭。 */
	bool info_vertical;   /* 竖屏时信息栏采用传统竖排文字，默认开启。 */
	bool burn_in_protection; /* 防烧屏保护：开启后主界面缓慢微移，默认关闭。 */
	double burn_in_protection_offset; /* 防烧屏位移幅度（相对屏幕短边的比例）。 */
	/* 天气布局增强（均为可选开关，默认关闭）：
	 * - weather_merge_landscape：横屏把天气并入顶部日期栏（变两栏）。
	 * - date_on_top_portrait：竖屏日期置顶横排，info栏(含天气)在左下，时间在右下。
	 * - weather_large_three_bars：保留三栏，天气不缩字，跟随 info_bar 同距同移。 */
	bool weather_merge_landscape;
	bool date_on_top_portrait;
	bool weather_large_three_bars;
	double info_scale;
	char cjk_font_path[MAX_BUFFER_LENGTH];
	/* 信息栏和天气可分别使用不同字体，为空时回退到 cjk_font_path。 */
	char info_bar_font_path[MAX_BUFFER_LENGTH];
	char weather_font_path[MAX_BUFFER_LENGTH];
	/* 天气显示配置与数据，由 Java 层通过 JNI 写入。 */
	bool show_weather;
	char weather_location[MAX_BUFFER_LENGTH];
	int weather_update_interval_hours;
	SDL_mutex *weather_mutex;
	char weather_location_text[MAX_BUFFER_LENGTH];
	char weather_temperature_text[32];
	char weather_description_text[32];
	bool weather_text_dirty;
	long long last_touch_time;
	SDL_FingerID last_touch_finger;
	bool running;
};

struct flipclock *flipclock_get_global_app(void);
void flipclock_update_weather(struct flipclock *app,
			      const char location[], const char temperature[],
			      const char description[]);

struct flipclock *flipclock_create(void);
void flipclock_load_conf(struct flipclock *app);
void flipclock_create_clocks(struct flipclock *app);
void flipclock_refresh(struct flipclock *app, int clock_index);
void flipclock_create_textures(struct flipclock *app, int clock_index);
void flipclock_destroy_textures(struct flipclock *app, int clock_index);
void flipclock_open_fonts(struct flipclock *app, int clock_index);
void flipclock_close_fonts(struct flipclock *app, int clock_index);
void flipclock_run_mainloop(struct flipclock *app);
void flipclock_destroy_clocks(struct flipclock *app);
void flipclock_destroy(struct flipclock *app);
void flipclock_print_help(struct flipclock *app, char program_name[]);

#endif
