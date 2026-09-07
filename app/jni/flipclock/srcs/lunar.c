/**
 * 农历换算模块。
 * 使用广泛流传的年份打包表（1900-2100），实现公历到农历的转换。
 * 表格式：低 4 位为闰月月份（0 表示无闰月）；接下来 12 位从高到低表示 12 个
 * 普通月份的大小（bit 高位对应正月，1 为大月 30 天，0 为小月 29 天）；bit 16
 * 表示闰月大小（1=30 天，0=29 天，仅当有闰月时有意义）。
 * 基准日期：1900 年 1 月 31 日 = 农历 1900 年正月初一（庚子年）。
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <time.h>

#include "lunar.h"

#define LUNAR_START_YEAR 1900
#define LUNAR_END_YEAR 2100
/* 1900-01-31 = 农历 1900 年正月初一。 */
#define LUNAR_BASE_YEAR 1900
#define LUNAR_BASE_MONTH 1
#define LUNAR_BASE_DAY 31

static const uint32_t LUNAR_INFO[] = {
	/* 1900-1909 */
	0x04bd8, 0x04ae0, 0x0a570, 0x054d5, 0x0d260, 0x0d950, 0x16554, 0x056a0,
	0x09ad0, 0x055d2,
	/* 1910-1919 */
	0x04ae0, 0x0a5b6, 0x0a4d0, 0x0d250, 0x1d255, 0x0b540, 0x0d6a0, 0x0ada2,
	0x095b0, 0x14977,
	/* 1920-1929 */
	0x04970, 0x0a4b0, 0x0b4b5, 0x06a50, 0x06d40, 0x1ab54, 0x02b60, 0x09570,
	0x052f2, 0x04970,
	/* 1930-1939 */
	0x06566, 0x0d4a0, 0x0ea50, 0x06e95, 0x05ad0, 0x02b60, 0x186e3, 0x092e0,
	0x1c8d7, 0x0c950,
	/* 1940-1949 */
	0x0d4a0, 0x1d8a6, 0x0b550, 0x056a0, 0x1a5b4, 0x025d0, 0x092d0, 0x0d2b2,
	0x0a950, 0x0b557,
	/* 1950-1959 */
	0x06ca0, 0x0b550, 0x15355, 0x04da0, 0x0a5b0, 0x14573, 0x052b0, 0x0a9a8,
	0x0e950, 0x06aa0,
	/* 1960-1969 */
	0x0aea6, 0x0ab50, 0x04b60, 0x0aae4, 0x0a570, 0x05260, 0x0f263, 0x0d950,
	0x05b57, 0x056a0,
	/* 1970-1979 */
	0x096d0, 0x04dd5, 0x04ad0, 0x0a4d0, 0x0d4d4, 0x0d250, 0x0d558, 0x0b540,
	0x0b6a0, 0x195a6,
	/* 1980-1989 */
	0x095b0, 0x049b0, 0x0a974, 0x0a4b0, 0x0b27a, 0x06a50, 0x06d40, 0x0af46,
	0x0ab60, 0x09570,
	/* 1990-1999 */
	0x04af5, 0x04970, 0x064b0, 0x074a3, 0x0ea50, 0x06b58, 0x055c0, 0x0ab60,
	0x096d5, 0x092e0,
	/* 2000-2009 */
	0x0c960, 0x0d954, 0x0d4a0, 0x0da50, 0x07552, 0x056a0, 0x0abb7, 0x025d0,
	0x092d0, 0x0cab5,
	/* 2010-2019 */
	0x0a950, 0x0b4a0, 0x0baa4, 0x0ad50, 0x055d9, 0x04ba0, 0x0a5b0, 0x15176,
	0x052b0, 0x0a930,
	/* 2020-2029 */
	0x07954, 0x06aa0, 0x0ad50, 0x05b52, 0x04b60, 0x0a6e6, 0x0a4e0, 0x0d260,
	0x0ea65, 0x0d530,
	/* 2030-2039 */
	0x05aa0, 0x076a3, 0x096d0, 0x04afb, 0x04ad0, 0x0a4d0, 0x1d0b6, 0x0d250,
	0x0d520, 0x0dd45,
	/* 2040-2049 */
	0x0b5a0, 0x056d0, 0x055b2, 0x049b0, 0x0a577, 0x0a4b0, 0x0aa50, 0x1b255,
	0x06d20, 0x0ada0,
	/* 2050-2059 */
	0x14b63, 0x09370, 0x049f8, 0x04970, 0x064b0, 0x168a6, 0x0ea50, 0x06b20,
	0x1a6c4, 0x0aae0,
	/* 2060-2069 */
	0x0a2e0, 0x0d2e3, 0x0c960, 0x0d557, 0x0d4a0, 0x0da50, 0x05d55, 0x056a0,
	0x0a6d0, 0x055d4,
	/* 2070-2079 */
	0x052d0, 0x0a9b8, 0x0a950, 0x0b4a0, 0x0b6a6, 0x0ad50, 0x055a0, 0x0aba4,
	0x0a5b0, 0x052b0,
	/* 2080-2089 */
	0x0b273, 0x06930, 0x07337, 0x06aa0, 0x0ad50, 0x14b55, 0x04b60, 0x0a570,
	0x054e4, 0x0d160,
	/* 2090-2099 */
	0x0e968, 0x0d520, 0x0daa0, 0x16aa6, 0x056d0, 0x04ae0, 0x0a9d4, 0x0a2d0,
	0x0d150, 0x0f252,
	/* 2100 */
	0x0d520
};

static const char *TIAN_GAN[10] = { "甲", "乙", "丙", "丁", "戊",
				    "己", "庚", "辛", "壬", "癸" };
static const char *DI_ZHI[12] = { "子", "丑", "寅", "卯", "辰", "巳",
				  "午", "未", "申", "酉", "戌", "亥" };
/* 与 DI_ZHI 同序：子鼠丑牛寅虎卯兔辰龙巳蛇午马未羊申猴酉鸡戌狗亥猪。 */
static const char *ZODIAC[12] = { "鼠", "牛", "虎", "兔", "龙", "蛇",
				 "马", "羊", "猴", "鸡", "狗", "猪" };
static const char *LUNAR_MONTHS[12] = { "正", "二", "三", "四", "五", "六",
					"七", "八", "九", "十", "冬", "腊" };
static const char *LUNAR_DAYS[30] = {
	"初一", "初二", "初三", "初四", "初五", "初六", "初七", "初八",
	"初九", "初十", "十一", "十二", "十三", "十四", "十五", "十六",
	"十七", "十八", "十九", "二十", "廿一", "廿二", "廿三", "廿四",
	"廿五", "廿六", "廿七", "廿八", "廿九", "三十"
};

static int _leap_month(int year)
{
	return LUNAR_INFO[year - LUNAR_START_YEAR] & 0xf;
}

static int _leap_days(int year)
{
	if (_leap_month(year) == 0)
		return 0;
	return (LUNAR_INFO[year - LUNAR_START_YEAR] & 0x10000) ? 30 : 29;
}

/* month: 1-12, 普通月大小。 */
static int _month_days(int year, int month)
{
	return (LUNAR_INFO[year - LUNAR_START_YEAR] & (0x10000 >> month)) ? 30 :
										 29;
}

static int _year_days(int year)
{
	int sum = 348;
	for (int i = 0x8000; i > 0x8; i >>= 1) {
		if (LUNAR_INFO[year - LUNAR_START_YEAR] & i)
			sum += 1;
	}
	return sum + _leap_days(year);
}

/* 计算两个公历日期之间的天数差（date2 - date1）。 */
static long _date_diff_days(int y1, int m1, int d1, int y2, int m2, int d2)
{
	struct tm t1 = { 0 };
	struct tm t2 = { 0 };
	t1.tm_year = y1 - 1900;
	t1.tm_mon = m1 - 1;
	t1.tm_mday = d1;
	t1.tm_hour = 12;
	t1.tm_isdst = -1;
	t2.tm_year = y2 - 1900;
	t2.tm_mon = m2 - 1;
	t2.tm_mday = d2;
	t2.tm_hour = 12;
	t2.tm_isdst = -1;
	time_t s1 = mktime(&t1);
	time_t s2 = mktime(&t2);
	if (s1 == (time_t)-1 || s2 == (time_t)-1)
		return 0;
	return (long)((s2 - s1) / 86400);
}

void lunar_from_gregorian(int year, int month, int day, bool show_year,
			  char *buf, size_t buf_size)
{
	if (buf == NULL || buf_size == 0)
		return;
	buf[0] = '\0';
	if (year < LUNAR_START_YEAR || year > LUNAR_END_YEAR)
		return;

	long offset = _date_diff_days(LUNAR_BASE_YEAR, LUNAR_BASE_MONTH,
				      LUNAR_BASE_DAY, year, month, day);
	if (offset < 0)
		return;

	int lunar_year = LUNAR_START_YEAR;
	int days_in_year = 0;
	while (lunar_year <= LUNAR_END_YEAR) {
		days_in_year = _year_days(lunar_year);
		if (offset < days_in_year)
			break;
		offset -= days_in_year;
		++lunar_year;
	}
	if (lunar_year > LUNAR_END_YEAR)
		return;

	int leap = _leap_month(lunar_year);
	int lunar_month = 1;
	int is_leap = 0;
	int days_in_month = 0;
	int i = 1;
	while (i <= 12) {
		days_in_month = _month_days(lunar_year, i);
		if (offset < days_in_month) {
			lunar_month = i;
			is_leap = 0;
			break;
		}
		offset -= days_in_month;
		/* After finishing month `leap`, insert its leap month. */
		if (i == leap) {
			int leap_days = _leap_days(lunar_year);
			if (offset < leap_days) {
				lunar_month = i;
				is_leap = 1;
				break;
			}
			offset -= leap_days;
		}
		++i;
	}
	if (i > 12) {
		lunar_month = 12;
		is_leap = 0;
	}
	int lunar_day = (int)offset + 1;

	/* 干支年（以农历正月初一为界）。1900 = 庚子。 */
	int gz_index = (lunar_year - 4) % 60;
	if (gz_index < 0)
		gz_index += 60;
	const char *gan = TIAN_GAN[gz_index % 10];
	const char *zhi = DI_ZHI[gz_index % 12];
	const char *zodiac = ZODIAC[gz_index % 12];

	const char *month_name =
		LUNAR_MONTHS[(lunar_month - 1 + 12) % 12];
	const char *day_name = LUNAR_DAYS[(lunar_day - 1) % 30];

	if (show_year) {
		snprintf(buf, buf_size, "%s%s%s年 %s%s月%s", gan, zhi,
			 zodiac, is_leap ? "闰" : "", month_name, day_name);
	} else {
		snprintf(buf, buf_size, "%s%s月%s", is_leap ? "闰" : "",
			 month_name, day_name);
	}
	buf[buf_size - 1] = '\0';
}
