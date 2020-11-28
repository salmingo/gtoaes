/**
 * @file ADefine.h
 * @brief 天文常数及宏定义
 * @date 2019-06-21
 * @version 1.0
 * @author 卢晓猛
 */

#ifndef ADEFINE_H_
#define ADEFINE_H_

#include <math.h>
/*--------------------------------------------------------------------------*/
// 平面角转换系数
#define API		3.141592653589793238462643		///< 圆周率
#define A2PI	6.283185307179586476925287		///< 2倍圆周率
#define API45	(API * 0.25)					///< 45度对应的弧度
#define API90	(API * 0.5)						///< 90度对应的弧度
#define R2D		5.729577951308232087679815E1	///< 弧度转换为角度
#define D2R		1.745329251994329576923691E-2	///< 角度转换为弧度
#define R2AS	2.062648062470963551564734E5	///< 弧度转换为角秒
#define AS2R	4.848136811095359935899141E-6	///< 角秒转换为弧度
#define H2D		15.0							///< 小时转换为角度
#define D2H		6.666666666666666666666667E-2	///< 角度转换为小时
#define H2R		2.617993877991494365385536E-1	///< 小时转换为弧度
#define R2H		3.819718634205488058453210		///< 弧度转换为小时
#define S2R		7.272205216643039903848712E-5	///< 秒转换为弧度
#define R2S		1.375098708313975701043156E4	///< 弧度转换为秒
#define D2AS	3600.0							///< 角度转换为角秒
#define AS2D	2.777777777777777777777778E-4	///< 角秒转换为角度
#define D2MAS	3600000.0						///< 角度转换为毫角秒
#define MAS2D	2.777777777777777777777778E-7	///< 毫角秒转换为角度
#define R2MAS	2.062648062470963551564734E8	///< 弧度转换为角秒
#define MAS2R	4.848136811095359935899141E-9	///< 角秒转换为弧度

// 时间转换常数
#define JD2K		2451545.0	///< 历元2000对应的儒略日
#define MJD0		2400000.5	///< 修正儒略日零点所对应的儒略日
#define MJD2K		51544.5		///< 历元2000对应的修正儒略日
#define MJD77		43144.0		///< 1977年1月1日0时对应的修正儒略日
#define TTMTAI		32.184		///< TTMTAI=TT-TAI
#define DAYSBY		365.242198781	///< 贝塞尔每年天数
#define DAYSJY		365.25		///< 儒略历每年天数
#define DAYSJC		36525.0		///< 儒略历每世纪天数
#define DAYSJM		365250.0	///< 儒略历每千年天数
#define DAYSEC		86400.0		///< 每日秒数

// 极限阈值
#define AEPS	1E-6			///< 最小值
#define AMAX	1E30			///< 最大值
/*--------------------------------------------------------------------------*/
// 计算实数的小数部分
#define frac(x)		(x - floor(x))
// 调整到[0, T)周期内
#define cycmod(x, T)	((x) - floor((x) / (T)) * (T))
/*--------------------------------------------------------------------------*/

#endif /* ADEFINE_H_ */
