#ifndef __GPS__H
#define __GPS__H

#include <time.h>
#include "common.h"

//struct gps_data_t
//{
//	struct tm tm_time;       // GPS时间
//	unsigned long ns_d;      // 纬度，度
//	unsigned long ns_m10000; // 纬度，分，扩大10000倍(带4位小数)
//	unsigned long ew_d;      // 经度，度
//	unsigned long ew_m10000; // 经度，分，扩大10000倍(带4位小数)
//	char ns;                 // 南北半球
//	char ew;                 // 东西半球
//	char status;             // 定位状态
//	char card;               // 选用的哪个SIM卡
//	unsigned long speed100;  // 速度，后面有2位小数，单位：节
//	unsigned long cog100;    // 航行方向角，以正北为0度
//	unsigned long count;     // 卫星个数
//	unsigned long height_d;
//	unsigned long height_m;
//};

struct MyTm
{
	int tm_sec;     // 秒–取值区间为[0,59] 
	int tm_min;     // 分 - 取值区间为[0,59] 
	int tm_hour;    // 时 - 取值区间为[0,23] 
	int tm_mday;    // 一个月中的日期 - 取值区间为[1,31] 
	int tm_mon;     // 月份（从一月开始，0代表一月） - 取值区间为[0,11] 
	int tm_year;    // 年份，其值从1900开始 
	int tm_zone;    // 时区，所在时区相比0时区相差的分钟数，譬如北京是东八区，该值为-480
};

struct gps_data_t
{
	struct MyTm tm_time;       // GPS时间
	unsigned long ns_d;      // 纬度，度
	unsigned long ns_m10000; // 纬度，分，扩大10000倍(带4位小数)
	unsigned long ew_d;      // 经度，度
	unsigned long ew_m10000; // 经度，分，扩大10000倍(带4位小数)
	char ns;                 // 南北半球 按照ASCII码解析
	char ew;                 // 东西半球 按照ASCII码解析
	char status;             // 定位状态 按照ASCII码解析
	char card;               // 选用的哪个SIM卡
	unsigned long speed100;  // 速度，后面有2位小数，单位：节
	unsigned long cog100;    // 航行方向角，以正北为0度
	unsigned long count;     // 卫星个数
	unsigned long height_d;  // 海拔，整数部分，单位为米，譬如43实际上是43
	unsigned long height_m;  // 海拔，小数部分，单位为米，譬如56实际上是0.56
};

int32_t InitGps(void);
struct gps_data_t * PraseGPS(void);
void *GpsThread(void *args);
struct gps_data_t * GetGpsInfo(void);
#endif
