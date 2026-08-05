#ifndef __LUNAR_H__
#define __LUNAR_H__

#include <stdbool.h>
#include <stddef.h>

/**
 * 将公历 (Gregorian) 日期转换为中文农历字符串，写入 buf。
 * show_year 为 true 时输出干支年（如 `丙午年 六月廿二`），
 * 为 false 时省略年份（如 `六月廿二`）。
 * 支持年份范围：1900 - 2100。
 * 若输入超出范围，buf 会被置为空字符串。
 * buf_size 建议 >= 64。
 */
void lunar_from_gregorian(int year, int month, int day, bool show_year,
			  char *buf, size_t buf_size);

#endif
