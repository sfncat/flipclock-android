#include "srcs/card.h"
#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "flipclock.h"
#include "clock.h"
#include "card.h"
#include "info_bar.h"
#include "weather_overlay.h"

#define WINDOW_WIDTH 800
#define WINDOW_HEIGHT 600
/* 信息栏占用窗口短边的比例，见 docs/date-info-design.md。 */
#define INFO_RATIO 0.10
/* 竖屏时左侧信息栏占窗口宽度的比例（需要容纳多行文字）。 */
#define INFO_RATIO_PORTRAIT 0.22
/* 方案2 竖屏置顶日期带占窗口高度的比例。 */
#define DATE_TOP_RATIO 0.10
/* 防烧屏：完整来回周期 60 秒，振幅由 burn_in_protection_offset 控制。 */
#define BURN_IN_PERIOD_MS 60000
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static bool _flipclock_clock_has_info_bar(const struct flipclock *app)
{
	return app->show_date || app->show_weekday || app->show_lunar;
}

/**
 * 计算防烧屏偏移量：
 * - 横屏时信息栏在上方、卡片在中间，两者在 Y 轴上相互靠近/远离；
 * - 竖屏时信息栏在左方、卡片在中间，两者在 X 轴上相互靠近/远离。
 * 函数返回信息栏应叠加的偏移，卡片组使用反向偏移。
 *
 * 天气始终固定位置显示；开启防烧屏时，天气额外做微移，
 * 幅度由 burn_in_protection_offset 控制（横屏左右、竖屏上下）。
 */
static SDL_Point _flipclock_clock_get_burn_in_offset(struct flipclock_clock *clock)
{
	SDL_Point offset = { 0, 0 };
	if (clock == NULL || clock->app == NULL || !clock->app->burn_in_protection)
		return offset;

	int min_side = clock->w < clock->h ? clock->w : clock->h;
	double ratio = clock->app->burn_in_protection_offset;
	if (ratio < 0.0)
		ratio = 0.0;
	if (ratio > 0.05)
		ratio = 0.05;
	int amplitude = (int)(min_side * ratio);
	if (amplitude < 1)
		amplitude = 1;

	Uint32 ticks = SDL_GetTicks();
	double phase = 2.0 * M_PI * (double)(ticks % BURN_IN_PERIOD_MS) /
		       (double)BURN_IN_PERIOD_MS;
	int value = (int)(amplitude * sin(phase));

	if (clock->w >= clock->h)
		offset.y = value;
	else
		offset.x = value;
	return offset;
}

/* 天气放置方式：独立天气带（默认/方案3）、并入横屏日期栏（方案1）、并入竖屏 info 栏（方案2）。 */
enum flipclock_weather_placement {
	WEATHER_PLACE_SEPARATE,
	WEATHER_PLACE_DATE_BAR,
	WEATHER_PLACE_INFO_BAR,
};

static enum flipclock_weather_placement
_flipclock_clock_weather_placement(struct flipclock_clock *clock)
{
	const struct flipclock *app = clock->app;
	if (clock->w >= clock->h) {
		if (app->weather_merge_landscape)
			return WEATHER_PLACE_DATE_BAR;
	} else {
		if (app->date_on_top_portrait)
			return WEATHER_PLACE_INFO_BAR;
	}
	return WEATHER_PLACE_SEPARATE;
}

/* 防烧屏振幅（像素），与 _flipclock_clock_get_burn_in_offset 保持一致。 */
static int _flipclock_clock_burn_in_amplitude(struct flipclock_clock *clock)
{
	const struct flipclock *app = clock->app;
	if (!app->burn_in_protection)
		return 0;
	int min_side = clock->w < clock->h ? clock->w : clock->h;
	int amp = (int)(min_side * app->burn_in_protection_offset);
	if (amp < 1)
		amp = 1;
	return amp;
}

static void _flipclock_clock_close_date_font(struct flipclock_clock *clock)
{
	if (clock->date_font != NULL) {
		TTF_CloseFont(clock->date_font);
		clock->date_font = NULL;
	}
}

static void _flipclock_clock_open_date_font(struct flipclock_clock *clock)
{
	_flipclock_clock_close_date_font(clock);
	if (clock->date_rect.w <= 0 || clock->date_rect.h <= 0)
		return;
	const struct flipclock *app = clock->app;
	const char *font_path = NULL;
	if (app->info_bar_font_path[0] != '\0')
		font_path = app->info_bar_font_path;
	else if (app->cjk_font_path[0] != '\0')
		font_path = app->cjk_font_path;
	else
		return;
	int amp = _flipclock_clock_burn_in_amplitude(clock);
	/*
	 * 顶部日期为横排长串文本，方案2 防烧屏为左右移动 ±amp：
	 * 必须预留「2·amp + 双侧安全边距」，否则移到边缘仍可能被裁切。
	 * 字号上限同时受带高（0.62）与可用宽度约束，确保全相位都完整、
	 * 且整体偏小（用户要求再小一点）。
	 */
	int safety = clock->date_rect.w / 30;
	if (safety < 6)
		safety = 6;
	int avail_w = clock->date_rect.w - 2 * amp - 2 * safety;
	if (avail_w < 8)
		avail_w = 8;
	int target_w = (int)(avail_w * 0.92);
	if (target_w < 8)
		target_w = 8;
	int max_h = (int)(clock->date_rect.h * 0.62);
	if (max_h < 8)
		max_h = 8;

	int px = max_h;
	if (px < 8)
		px = 8;

	char date_buf[32];
	struct tm now = app->now;
	strftime(date_buf, sizeof(date_buf), "%Y-%m-%d", &now);
	for (int attempt = 0; attempt < 10; ++attempt) {
		TTF_Font *probe = TTF_OpenFont(font_path, px);
		if (probe == NULL)
			break;
		int w = 0, h = 0;
		TTF_SizeUTF8(probe, date_buf, &w, &h);
		TTF_CloseFont(probe);
		bool too_wide = (w > target_w);
		bool too_tall = (h > max_h);
		if (!too_wide && !too_tall)
			break;
		int new_px_w = too_wide
				       ? (int)(px * ((double)target_w * 0.98 /
						     w))
				       : px;
		int new_px_h = too_tall
				       ? (int)(px * ((double)max_h * 0.98 /
						     h))
				       : px;
		int new_px = new_px_w < new_px_h ? new_px_w : new_px_h;
		if (new_px >= px)
			break;
		px = new_px < 8 ? 8 : new_px;
	}
	clock->date_font = TTF_OpenFont(font_path, px);
	if (clock->date_font != NULL) {
		TTF_SetFontStyle(clock->date_font, TTF_STYLE_BOLD);
		clock->date_font_px = px;
	}
}

/* 方案2 顶部日期带：横排、不旋转，防烧屏为左右移动 + 淡入淡出。 */
static void _flipclock_clock_draw_top_date(struct flipclock_clock *clock,
					   int dx, Uint8 alpha)
{
	if (clock->date_font == NULL || clock->date_rect.w <= 0 ||
	    clock->date_rect.h <= 0)
		return;
	char buf[32];
	struct tm now = clock->app->now;
	strftime(buf, sizeof(buf), "%Y-%m-%d", &now);
	SDL_Color color = clock->app->text_color;
	SDL_Surface *surface = TTF_RenderUTF8_Blended(clock->date_font, buf,
						color);
	if (surface == NULL)
		return;
	SDL_Texture *tex = SDL_CreateTextureFromSurface(clock->renderer,
		surface);
	SDL_FreeSurface(surface);
	if (tex == NULL)
		return;
	SDL_SetTextureAlphaMod(tex, alpha);
	int tex_w = 0, tex_h = 0;
	SDL_QueryTexture(tex, NULL, NULL, &tex_w, &tex_h);
	SDL_Rect dst = {
		clock->date_rect.x + dx + (clock->date_rect.w - tex_w) / 2,
		clock->date_rect.y + (clock->date_rect.h - tex_h) / 2,
		tex_w, tex_h
	};
	SDL_RenderCopy(clock->renderer, tex, NULL, &dst);
	SDL_DestroyTexture(tex);
}

static void _flipclock_clock_update_layout(struct flipclock_clock *clock)
{
	RETURN_IF_FAIL(clock != NULL);

	const struct flipclock *app = clock->app;
	SDL_Rect hour_rect;
	SDL_Rect minute_rect;
	SDL_Rect second_rect;
	int cards_length = app->show_second ? 3 : 2;
	int spaces_length = cards_length + 1;
	bool has_info_bar = _flipclock_clock_has_info_bar(app);
	enum flipclock_weather_placement place =
		_flipclock_clock_weather_placement(clock);

	if (clock->w >= clock->h) {
		int space_size = clock->w / (cards_length * 8 + spaces_length);
		int info_h = has_info_bar
				     ? (int)(clock->h * INFO_RATIO) + 2 * space_size
				     : 0;

		/*
		 * 方案3：保留三栏、天气字号不缩小，时间卡片保持原始尺寸，
		 * 不再额外预留天气带挤占时间（天气字号由 weather_overlay
		 * 在方案3 下单独放大，并随防烧屏同距同移，仅避免与时间重叠）。
		 */
		int weather_band_h = 0;
		int gap_to_time = 0;
		int reserved_top = info_h + weather_band_h + gap_to_time;

		int min_height = (clock->h - reserved_top) * 0.8;
		int min_width =
			clock->w * 8 / (cards_length * 8 + spaces_length);
		int card_size = min_height < min_width ? min_height : min_width;
		card_size *= app->card_scale;

		hour_rect.x = (clock->w - card_size * cards_length -
			       space_size * (spaces_length - 2)) /
			      2;
		hour_rect.y =
			reserved_top + ((clock->h - reserved_top) - card_size) / 2;
		hour_rect.w = card_size;
		hour_rect.h = card_size;
		flipclock_card_set_rect(clock->hour, hour_rect);

		minute_rect.x = hour_rect.x + hour_rect.w + space_size;
		minute_rect.y = hour_rect.y;
		minute_rect.w = card_size;
		minute_rect.h = card_size;
		flipclock_card_set_rect(clock->minute, minute_rect);

		if (app->show_second) {
			second_rect.x = hour_rect.x + hour_rect.w + space_size +
				minute_rect.w + space_size;
			second_rect.y = hour_rect.y;
			second_rect.w = card_size;
			second_rect.h = card_size;
			flipclock_card_set_rect(clock->second, second_rect);
		}

		if (clock->info_bar != NULL) {
			SDL_Rect info_rect;
			info_rect.x = 0;
			info_rect.y = space_size;
			info_rect.w = clock->w;
			info_rect.h = (int)(clock->h * INFO_RATIO);
			flipclock_info_bar_set_rect(clock->info_bar, info_rect,
					     true);
			clock->info_bar->show_weather_segment =
				(place == WEATHER_PLACE_DATE_BAR) &&
				app->show_weather;
			clock->info_bar->horizontal_stack = false;
		}

		if (place == WEATHER_PLACE_DATE_BAR) {
			clock->weather_rect.x = 0;
			clock->weather_rect.y = 0;
			clock->weather_rect.w = 0;
			clock->weather_rect.h = 0;
		} else if (app->weather_large_three_bars) {
			clock->weather_rect.x = 0;
			clock->weather_rect.y = info_h;
			clock->weather_rect.w = clock->w;
			clock->weather_rect.h = weather_band_h;
		} else {
			clock->weather_rect.x = 0;
			clock->weather_rect.y =
				space_size + (int)(clock->h * INFO_RATIO);
			clock->weather_rect.w = clock->w;
			clock->weather_rect.h =
				hour_rect.y - clock->weather_rect.y;
		}

		clock->date_rect.x = 0;
		clock->date_rect.y = 0;
		clock->date_rect.w = 0;
		clock->date_rect.h = 0;
		_flipclock_clock_close_date_font(clock);
		/*
		 * 横屏天气区域位于信息栏与卡片之间。开启防烧屏时信息栏向下、
		 * 卡片向上各移动最多一个振幅，天气可能分别与日期/时间重叠，
		 * 因此上下两侧各预留一个振幅的纵向余量。两者反向等量移动，
		 * 间隙中点恒定，天气居中位置不变，字号随安全区缩小自适应，
		 * 保证全相位不重叠。
		 */
		int burn_in_reserve = 0;
		if (app->burn_in_protection) {
			int min_side = clock->w < clock->h ? clock->w : clock->h;
			double ratio = app->burn_in_protection_offset;
			if (ratio < 0.0)
				ratio = 0.0;
			if (ratio > 0.05)
				ratio = 0.05;
			burn_in_reserve = (int)(min_side * ratio);
		}
		clock->weather_rect.x = 0;
		clock->weather_rect.y = space_size +
					 (int)(clock->h * INFO_RATIO) +
					 burn_in_reserve;
		clock->weather_rect.w = clock->w;
		clock->weather_rect.h = hour_rect.y - clock->weather_rect.y -
					 burn_in_reserve;
	} else {
		bool date_top = (place == WEATHER_PLACE_INFO_BAR);
		int space_size = clock->h / (cards_length * 8 + spaces_length);
		int date_top_h = date_top
				     ? (int)(clock->h * DATE_TOP_RATIO) +
				       2 * space_size
			     : 0;
		int info_w = has_info_bar
				     ? (int)(clock->w * INFO_RATIO_PORTRAIT) +
				       2 * space_size
			     : 0;

		/*
		 * 方案3：保留三栏、天气字号不缩小，时间卡片保持原始尺寸，
		 * 不再额外预留天气竖条挤占时间（天气字号由 weather_overlay
		 * 在方案3 下单独放大，并随防烧屏同距同移，仅避免与时间重叠）。
		 */
		int reserved_left = info_w;

		/*
		 * 方案2 顶部日期带占去一部分高度，且时间卡片随防烧屏上下移动
		 * ±amp：时间块必须放进「屏高 − 顶部日期带 − 2·amp」的安全区，
		 * 否则秒卡片在最下方相位会被裁切。这里以该安全区为基准计算
		 * 卡片尺寸并居中，使时间整体上移且全程不被切断。
		 */
		int amp = 0;
		if (app->burn_in_protection) {
			int min_side = clock->w < clock->h ? clock->w
						     : clock->h;
			double ratio = app->burn_in_protection_offset;
			if (ratio < 0.0)
				ratio = 0.0;
			if (ratio > 0.05)
				ratio = 0.05;
			amp = (int)(min_side * ratio);
		}
		int min_width = (clock->w - reserved_left) * 0.8;
		int min_height = (clock->h - date_top_h - 2 * amp) * 8 /
			(cards_length * 8 + spaces_length);
		int card_size = min_height < min_width ? min_height : min_width;
		card_size *= app->card_scale;

		hour_rect.x =
			reserved_left + ((clock->w - reserved_left) - card_size) / 2;
		hour_rect.y = date_top_h + amp +
			((clock->h - date_top_h - 2 * amp) - card_size * cards_length -
			 space_size * (spaces_length - 2)) /
				2;
		hour_rect.w = card_size;
		hour_rect.h = card_size;
		flipclock_card_set_rect(clock->hour, hour_rect);

		minute_rect.y = hour_rect.y + hour_rect.h + space_size;
		minute_rect.x = hour_rect.x;
		minute_rect.w = card_size;
		minute_rect.h = card_size;
		flipclock_card_set_rect(clock->minute, minute_rect);

		if (app->show_second) {
			second_rect.y = hour_rect.y + hour_rect.h + space_size +
				minute_rect.h + space_size;
			second_rect.x = hour_rect.x;
			second_rect.w = card_size;
			second_rect.h = card_size;
			flipclock_card_set_rect(clock->second, second_rect);
		}

		if (clock->info_bar != NULL) {
			SDL_Rect info_rect;
			info_rect.x = space_size;
			info_rect.y = date_top_h + space_size;
			info_rect.w = (int)(clock->w * INFO_RATIO_PORTRAIT);
			info_rect.h = clock->h - date_top_h - 2 * space_size;
			flipclock_info_bar_set_rect(clock->info_bar, info_rect,
					     false);
			clock->info_bar->show_weather_segment =
				date_top && app->show_weather;
			/* 方案2：保留左下信息栏的传统竖排，仅把日期移走。 */
			clock->info_bar->horizontal_stack = false;
			clock->info_bar->hide_date = date_top;
		}

		if (date_top) {
		clock->date_rect.x = 0;
		clock->date_rect.y = space_size / 2;
		clock->date_rect.w = clock->w;
			clock->date_rect.h = (int)(clock->h * DATE_TOP_RATIO);
			_flipclock_clock_open_date_font(clock);
		} else {
			clock->date_rect.x = 0;
			clock->date_rect.y = 0;
			clock->date_rect.w = 0;
			clock->date_rect.h = 0;
			_flipclock_clock_close_date_font(clock);
		}

		clock->weather_rect.x = space_size +
					(int)(clock->w * INFO_RATIO_PORTRAIT);
		clock->weather_rect.y = 0;
		clock->weather_rect.w = hour_rect.x - clock->weather_rect.x;
		clock->weather_rect.h = clock->h;
		/*
		 * 竖屏天气位于信息栏与时间卡片之间，视觉上离卡片偏近：收窄天气
		 * 区域右侧使其整体略向左移。开启防烧屏时卡片会向天气方向水平移动
		 * 最多一个振幅，此时多留余量；区域较窄时按比例限制避免过度压缩。
		 */
		int w_bias = clock->weather_rect.w / 6;
		if (app->burn_in_protection) {
			int min_side = clock->w < clock->h ? clock->w : clock->h;
			double ratio = app->burn_in_protection_offset;
			if (ratio < 0.0)
				ratio = 0.0;
			if (ratio > 0.05)
				ratio = 0.05;
			int amplitude = (int)(min_side * ratio);
			if (amplitude > w_bias)
				w_bias = amplitude;
		}
		if (w_bias > clock->weather_rect.w / 3)
			w_bias = clock->weather_rect.w / 3;
		clock->weather_rect.w -= w_bias;
		if (place == WEATHER_PLACE_INFO_BAR) {
			clock->weather_rect.x = 0;
			clock->weather_rect.y = 0;
			clock->weather_rect.w = 0;
			clock->weather_rect.h = 0;
		} else if (app->weather_large_three_bars) {
			/*
			 * 方案3：天气占据信息栏与时间卡片之间的自然间隙
			 * （不再额外预留宽度挤占时间），字号由 weather_overlay
			 * 在方案3 下放大填满该间隙。
			 */
			clock->weather_rect.x =
				space_size + (int)(clock->w * INFO_RATIO_PORTRAIT);
			clock->weather_rect.y = 0;
			clock->weather_rect.w =
				hour_rect.x - clock->weather_rect.x;
			clock->weather_rect.h = clock->h;
		} else {
			clock->weather_rect.x =
				space_size + (int)(clock->w * INFO_RATIO_PORTRAIT);
			clock->weather_rect.y = 0;
			clock->weather_rect.w =
				hour_rect.x - clock->weather_rect.x;
			clock->weather_rect.h = clock->h;
		}
	}
}


static void _flipclock_clock_create_cards(struct flipclock_clock *clock)
{
	RETURN_IF_FAIL(clock != NULL);

	struct flipclock *app = clock->app;

	clock->hour = flipclock_card_create(app, clock->renderer);
	clock->minute = flipclock_card_create(app, clock->renderer);
	clock->second = NULL;
	if (app->show_second)
		clock->second = flipclock_card_create(app, clock->renderer);
	clock->info_bar = NULL;
	if (_flipclock_clock_has_info_bar(app))
		clock->info_bar =
			flipclock_info_bar_create(app, clock->renderer);
	clock->weather_overlay = NULL;
	if (app->show_weather)
		clock->weather_overlay =
			flipclock_weather_overlay_create(app, clock->renderer);
	clock->burn_in_state = BURN_IN_MOVING;
	clock->date_font = NULL;
	clock->date_font_px = 0;
	clock->burn_in_hold_start_ticks = 0;
	clock->burn_in_peak_sign = 0;
	clock->burn_in_last_peak_sign = 0;
	_flipclock_clock_update_layout(clock);
}

struct flipclock_clock *flipclock_clock_create(struct flipclock *app, int i)
{
	RETURN_VAL_IF_FAIL(app != NULL, NULL);

	/**
	 * We need `SDL_WINDOW_RESIZABLE` for auto-rotate
	 * while fullscreen on Android.
	 */
	unsigned int flags = SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE |
			     SDL_WINDOW_ALLOW_HIGHDPI;
	if (app->full) {
		flags = SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE |
			SDL_WINDOW_ALLOW_HIGHDPI |
			SDL_WINDOW_FULLSCREEN_DESKTOP;
	}
	struct flipclock_clock *clock = malloc(sizeof(*clock));
	if (clock == NULL) {
		LOG_ERROR("Failed to create clock!");
		exit(EXIT_FAILURE);
	}
	clock->app = app;
	clock->waiting = false;
	clock->i = i;
	SDL_Rect display_bounds;
	SDL_GetDisplayBounds(i, &display_bounds);
	// Give each window a unique title.
	char window_title[MAX_BUFFER_LENGTH];
	snprintf(window_title, MAX_BUFFER_LENGTH, PROGRAM_TITLE " %d", i);
	if (app->full)
		clock->window = SDL_CreateWindow(window_title, display_bounds.x,
						 display_bounds.y,
						 display_bounds.w,
						 display_bounds.h, flags);
	else
		clock->window = SDL_CreateWindow(
			window_title,
			display_bounds.x +
				(display_bounds.w - WINDOW_WIDTH) / 2,
			display_bounds.y +
				(display_bounds.h - WINDOW_HEIGHT) / 2,
			WINDOW_WIDTH, WINDOW_HEIGHT, flags);
	if (clock->window == NULL) {
		LOG_ERROR("%s\n", SDL_GetError());
		exit(EXIT_FAILURE);
	}
	// Get actual window size after create it.
	SDL_GetWindowSize(clock->window, &clock->w, &clock->h);
	clock->renderer = SDL_CreateRenderer(
		clock->window, -1,
		SDL_RENDERER_ACCELERATED | SDL_RENDERER_TARGETTEXTURE |
			SDL_RENDERER_PRESENTVSYNC);
	if (clock->renderer == NULL) {
		LOG_ERROR("%s\n", SDL_GetError());
		exit(EXIT_FAILURE);
	}
	SDL_SetRenderDrawBlendMode(clock->renderer, SDL_BLENDMODE_BLEND);
	_flipclock_clock_create_cards(clock);
	return clock;
}

#if defined(_WIN32)
/**
 * Create clock from given HWND, which should be a subwindow of screensaver
 * preview.
 */
struct flipclock_clock *flipclock_clock_create_preview(struct flipclock *app)
{
	RETURN_VAL_IF_FAIL(app != NULL, NULL);

	struct flipclock_clock *clock = malloc(sizeof(*clock));
	if (clock == NULL) {
		LOG_ERROR("Failed to create clock!");
		exit(EXIT_FAILURE);
	}
	clock->app = app;
	clock->waiting = false;
	clock->i = 0;
	clock->window = SDL_CreateWindowFrom(app->preview_window);
	if (clock->window == NULL) {
		LOG_ERROR("%s\n", SDL_GetError());
		exit(EXIT_FAILURE);
	}
	// Get actual window size after create it.
	SDL_GetWindowSize(clock->window, &clock->w, &clock->h);
	clock->renderer = SDL_CreateRenderer(
		clock->window, -1,
		SDL_RENDERER_ACCELERATED | SDL_RENDERER_TARGETTEXTURE |
			SDL_RENDERER_PRESENTVSYNC);
	if (clock->renderer == NULL) {
		LOG_ERROR("%s\n", SDL_GetError());
		exit(EXIT_FAILURE);
	}
	SDL_SetRenderDrawBlendMode(clock->renderer, SDL_BLENDMODE_BLEND);
	_flipclock_clock_create_cards(clock);
	return clock;
}
#endif

void flipclock_clock_set_show_second(struct flipclock_clock *clock,
				     bool show_second)
{
	RETURN_IF_FAIL(clock != NULL);

	if (show_second) {
		if (clock->second == NULL)
			clock->second = flipclock_card_create(clock->app,
							      clock->renderer);
	} else {
		if (clock->second != NULL) {
			flipclock_card_destory(clock->second);
			clock->second = NULL;
		}
	}
	// Toggling seconds always changes size.
	_flipclock_clock_update_layout(clock);
}

void flipclock_clock_set_fullscreen(struct flipclock_clock *clock, bool full)
{
	RETURN_IF_FAIL(clock != NULL);

	// Let's find which display the clock is inside.
	SDL_Rect display_bounds;
	int clock_x;
	int clock_y;
	SDL_GetWindowPosition(clock->window, &clock_x, &clock_y);
	int clock_center_x = clock_x + clock->w / 2;
	int clock_center_y = clock_y + clock->h / 2;
	int displays_length = SDL_GetNumVideoDisplays();
	// If a clock is out of all displays it will be re-placed into the last.
	for (int i = 0; i < displays_length; ++i) {
		SDL_GetDisplayBounds(i, &display_bounds);
		if (clock_center_x >= display_bounds.x &&
		    clock_center_x < display_bounds.x + display_bounds.w &&
		    clock_center_y >= display_bounds.y &&
		    clock_center_y < display_bounds.y + display_bounds.h) {
			LOG_DEBUG("Clock `%d` is inside display `%d`.\n",
				  clock->i, i);
			break;
		}
	}
	if (full) {
		// Move clocks to their placed displays.
		SDL_SetWindowPosition(clock->window, display_bounds.x,
				      display_bounds.y);
		SDL_SetWindowFullscreen(clock->window,
					SDL_WINDOW_FULLSCREEN_DESKTOP);
		SDL_GetWindowSize(clock->window, &clock->w, &clock->h);
		LOG_DEBUG("Set clock `%d` to fullscreen with size `%dx%d`.\n",
			  clock->i, clock->w, clock->h);
	} else {
		SDL_SetWindowFullscreen(clock->window, 0);
		/**
		 * We need to restore window first, because if started in
		 * fullscreen mode, it will be maximized when turning off
		 * fullscreen mode and we cannot set window size. Looks like
		 * a strange bug.
		 */
		SDL_RestoreWindow(clock->window);
		SDL_SetWindowSize(clock->window, WINDOW_WIDTH, WINDOW_HEIGHT);
		SDL_SetWindowPosition(
			clock->window,
			display_bounds.x +
				(display_bounds.w - WINDOW_WIDTH) / 2,
			display_bounds.y +
				(display_bounds.h - WINDOW_HEIGHT) / 2);
		SDL_GetWindowSize(clock->window, &clock->w, &clock->h);
		LOG_DEBUG("Set clock `%d` to windowed.\n", clock->i);
	}
	// Toggling fullscreen always changes size.
	_flipclock_clock_update_layout(clock);
}

void flipclock_clock_set_hour(struct flipclock_clock *clock, const char hour[],
			      bool flip)
{
	// Text can be NULL to clear card.
	RETURN_IF_FAIL(clock != NULL);

	flipclock_card_set_text(clock->hour, hour);
	if (flip)
		flipclock_card_flip(clock->hour);
}

void flipclock_clock_set_minute(struct flipclock_clock *clock,
				const char minute[], bool flip)
{
	// Text can be NULL to clear card.
	RETURN_IF_FAIL(clock != NULL);

	flipclock_card_set_text(clock->minute, minute);
	if (flip)
		flipclock_card_flip(clock->minute);
}

void flipclock_clock_set_second(struct flipclock_clock *clock,
				const char second[], bool flip)
{
	// Text can be NULL to clear card.
	RETURN_IF_FAIL(clock != NULL);

	if (!clock->app->show_second)
		return;

	flipclock_card_set_text(clock->second, second);
	if (flip)
		flipclock_card_flip(clock->second);
}

void flipclock_clock_set_ampm(struct flipclock_clock *clock, const char ampm[])
{
	// Text can be NULL to clear card.
	RETURN_IF_FAIL(clock != NULL);

	flipclock_card_set_sub_text(clock->hour, ampm);
	// Set ampm should never flip a card.
}

void flipclock_clock_handle_window_event(struct flipclock_clock *clock,
					 SDL_Event event)
{
	RETURN_IF_FAIL(clock != NULL);

	const struct flipclock *app = clock->app;
	int clock_i = clock->i;
	switch (event.window.event) {
	case SDL_WINDOWEVENT_SIZE_CHANGED:
		/**
		 * Only re-render when size changed.
		 * Windows may send event when size
		 * not changed, and cause strange bugs.
		 */
		if (event.window.data1 != clock->w ||
		    event.window.data2 != clock->h) {
			clock->w = event.window.data1;
			clock->h = event.window.data2;
			LOG_DEBUG("New window size for "
				  "clock `%d` is `%dx%d`.\n",
				  clock->i, clock->w, clock->h);
			_flipclock_clock_update_layout(clock);
		}
		break;
	case SDL_WINDOWEVENT_MINIMIZED:
		clock->waiting = true;
		break;
	// `RESTORED` is emitted after `MINIMIZED`.
	case SDL_WINDOWEVENT_RESTORED:
		clock->waiting = false;
		/**
		 * Sometimes when a window is restored, its texture get lost.
		 * Typically happens when we have two fullscreen clocks in
		 * one display, and the lower one is switched to top, and we
		 * have to redraw its texture.
		 */
		// XXX: It seems OK without redrawing.
		// flipclock_clock_draw(clock);
		break;
	case SDL_WINDOWEVENT_CLOSE:
		flipclock_clock_destroy(clock);
		app->clocks[clock_i] = NULL;
		/**
		 * See https://wiki.libsdl.org/SDL_EventType#SDL_QUIT.
		 * It seems that SDL will send SDL_QUIT automatically
		 * when all windows are closed, so we don't need to exit
		 * manually here.
		 */
		LOG_DEBUG("Clock `%d` closed!\n", clock_i);
		break;
	default:
		break;
	}
}

void flipclock_clock_animate(struct flipclock_clock *clock)
{
	RETURN_IF_FAIL(clock != NULL);

	const struct flipclock *app = clock->app;
	SDL_SetRenderDrawColor(clock->renderer, app->background_color.r,
			       app->background_color.g, app->background_color.b,
			       app->background_color.a);
	SDL_RenderClear(clock->renderer);

	SDL_Point burn_in = _flipclock_clock_get_burn_in_offset(clock);
	SDL_Point card_burn_in = { -burn_in.x, -burn_in.y };

	/* 把天气文本并入 info_bar（方案1/2），由 info_bar 一并绘制。 */
	enum flipclock_weather_placement place =
		_flipclock_clock_weather_placement(clock);
	if (clock->info_bar != NULL &&
	    clock->info_bar->show_weather_segment) {
		struct flipclock *mutable_app = clock->app;
		SDL_LockMutex(mutable_app->weather_mutex);
		char combined[WEATHER_TEXT_LENGTH];
		combined[0] = '\0';
		if (mutable_app->weather_temperature_text[0] != '\0') {
			strncat(combined, mutable_app->weather_temperature_text,
				sizeof(combined) - 1);
		}
		if (mutable_app->weather_temperature_text[0] != '\0' &&
		    mutable_app->weather_description_text[0] != '\0') {
			strncat(combined, " ",
				sizeof(combined) - strlen(combined) - 1);
		}
		if (mutable_app->weather_description_text[0] != '\0') {
			strncat(combined, mutable_app->weather_description_text,
				sizeof(combined) - strlen(combined) - 1);
		}
		SDL_UnlockMutex(mutable_app->weather_mutex);
		strncpy(clock->info_bar->weather_text, combined,
			WEATHER_TEXT_LENGTH - 1);
		clock->info_bar->weather_text[WEATHER_TEXT_LENGTH - 1] = '\0';
	}

	if (clock->info_bar != NULL) {
		flipclock_info_bar_refresh(clock->info_bar, &app->now, false);
		flipclock_info_bar_draw(clock->info_bar, burn_in);
	}


	flipclock_card_animate(clock->hour, card_burn_in);
	flipclock_card_animate(clock->minute, card_burn_in);
	if (app->show_second)
		flipclock_card_animate(clock->second, card_burn_in);

	/* 方案2：顶部日期带单独绘制，防烧屏为左右移动 + 淡入淡出。 */
	if (place == WEATHER_PLACE_INFO_BAR && clock->date_rect.w > 0) {
		int amp = _flipclock_clock_burn_in_amplitude(clock);
		Uint32 ticks = SDL_GetTicks();
		double phase = 2.0 * M_PI *
			       (double)(ticks % BURN_IN_PERIOD_MS) /
			       (double)BURN_IN_PERIOD_MS;
		int dx = (int)(amp * sin(phase));
		Uint8 alpha = (Uint8)(255.0 *
				      (0.25 + 0.75 * (1.0 - fabs(sin(phase)))));
		_flipclock_clock_draw_top_date(clock, dx, alpha);
	}

	/* 天气：并入信息栏（方案1/2）时不单独绘制。 */
	if (clock->weather_overlay != NULL && place == WEATHER_PLACE_SEPARATE) {
		struct flipclock *mutable_app = clock->app;
		SDL_LockMutex(mutable_app->weather_mutex);
		if (mutable_app->weather_text_dirty) {
			flipclock_weather_overlay_set_text(
				clock->weather_overlay,
				mutable_app->weather_temperature_text,
				mutable_app->weather_description_text);
			mutable_app->weather_text_dirty = false;
		}
		SDL_UnlockMutex(mutable_app->weather_mutex);

		/*
		 * 天气始终固定显示。开启防烧屏时，天气额外做微移，幅度与防烧屏
		 * 一致（屏幕短边 × burn_in_protection_offset）：横屏左右移动，
		 * 竖屏（竖排天气）上下移动。
		 */
		SDL_Rect weather_draw_rect = clock->weather_rect;
		if (mutable_app->weather_large_three_bars) {
			/* 方案3：跟随 info_bar 同距同移。 */
			weather_draw_rect.x += burn_in.x;
			weather_draw_rect.y += burn_in.y;
		} else if (mutable_app->burn_in_protection) {
			/* 默认：独立水平微移（保持原有行为）。 */
			int min_side = clock->w < clock->h ? clock->w : clock->h;
			int w_amplitude = (int)(min_side *
						mutable_app->burn_in_protection_offset);
			if (w_amplitude < 1)
				w_amplitude = 1;
			Uint32 ticks = SDL_GetTicks();
			double phase = 2.0 * M_PI *
				       (double)(ticks % BURN_IN_PERIOD_MS) /
				       (double)BURN_IN_PERIOD_MS;
			int w_offset = (int)(w_amplitude * sin(phase));
			if (clock->w >= clock->h)
				weather_draw_rect.x += w_offset;
			else
				weather_draw_rect.y += w_offset;
		}
		flipclock_weather_overlay_draw(clock->weather_overlay,
					       weather_draw_rect);
	}


	SDL_RenderPresent(clock->renderer);
}

void flipclock_clock_destroy(struct flipclock_clock *clock)
{
	RETURN_IF_FAIL(clock != NULL);

	flipclock_card_destory(clock->hour);
	if (clock->date_font != NULL) {
		TTF_CloseFont(clock->date_font);
		clock->date_font = NULL;
	}
	flipclock_card_destory(clock->minute);
	if (clock->second != NULL)
		flipclock_card_destory(clock->second);
	if (clock->info_bar != NULL)
		flipclock_info_bar_destroy(clock->info_bar);
	if (clock->weather_overlay != NULL)
		flipclock_weather_overlay_destroy(clock->weather_overlay);
	SDL_DestroyRenderer(clock->renderer);
	SDL_DestroyWindow(clock->window);
	free(clock);
}
