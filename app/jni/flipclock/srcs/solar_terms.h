#ifndef __SOLAR_TERMS_H__
#define __SOLAR_TERMS_H__

#include <stddef.h>

/*
 * 节气 / 三伏 / 九九查询接口（纯标准库，不依赖 SDL，可独立编译对拍）。
 *
 * 数据来源：test/almanac/terms_1900_2100.txt（表原稿，git 入库，
 * 由 gen_table.py 双源对拍生成），内置表由 gen_c_table.py 生成。
 * 覆盖范围：1900-2100；表外年份一律返回 NULL / 空串。
 * 口径：按北京时间（UTC+8）日历日。
 */

/* 该日恰为节气时返回节气名（如"白露"），否则 NULL。 */
const char *solar_term_of(int year, int month, int day);

/* 该日在三伏或九九期内时返回期段文本（如"中伏第二十天"、"一九第一天"），
   否则 NULL。三伏与九九时间不重叠，三伏优先判断。 */
const char *solar_period_of(int year, int month, int day);

/* 信息栏显示文本：节气优先，其次期段；均无则 buf 置空串。 */
void solar_term_text_for(int year, int month, int day,
			 char *buf, size_t buf_size);

#endif
