#ifndef __GPS__H
#define __GPS__H

#include <time.h>
#include "common.h"

//struct gps_data_t
//{
//	struct tm tm_time;       // GPSʱ��
//	unsigned long ns_d;      // γ�ȣ���
//	unsigned long ns_m10000; // γ�ȣ��֣�����10000��(��4λС��)
//	unsigned long ew_d;      // ���ȣ���
//	unsigned long ew_m10000; // ���ȣ��֣�����10000��(��4λС��)
//	char ns;                 // �ϱ�����
//	char ew;                 // ��������
//	char status;             // ��λ״̬
//	char card;               // ѡ�õ��ĸ�SIM��
//	unsigned long speed100;  // �ٶȣ�������2λС������λ����
//	unsigned long cog100;    // ���з���ǣ�������Ϊ0��
//	unsigned long count;     // ���Ǹ���
//	unsigned long height_d;
//	unsigned long height_m;
//};

struct MyTm
{
	int tm_sec;     // ��Cȡֵ����Ϊ[0,59] 
	int tm_min;     // �� - ȡֵ����Ϊ[0,59] 
	int tm_hour;    // ʱ - ȡֵ����Ϊ[0,23] 
	int tm_mday;    // һ�����е����� - ȡֵ����Ϊ[1,31] 
	int tm_mon;     // �·ݣ���һ�¿�ʼ��0����һ�£� - ȡֵ����Ϊ[0,11] 
	int tm_year;    // ��ݣ���ֵ��1900��ʼ 
	int tm_zone;    // ʱ��������ʱ�����0ʱ�����ķ�������Ʃ�籱���Ƕ���������ֵΪ-480
};

struct gps_data_t
{
	struct MyTm tm_time;       // GPSʱ��
	unsigned long ns_d;      // γ�ȣ���
	unsigned long ns_m10000; // γ�ȣ��֣�����10000��(��4λС��)
	unsigned long ew_d;      // ���ȣ���
	unsigned long ew_m10000; // ���ȣ��֣�����10000��(��4λС��)
	char ns;                 // �ϱ����� ����ASCII�����
	char ew;                 // �������� ����ASCII�����
	char status;             // ��λ״̬ ����ASCII�����
	char card;               // ѡ�õ��ĸ�SIM��
	unsigned long speed100;  // �ٶȣ�������2λС������λ����
	unsigned long cog100;    // ���з���ǣ�������Ϊ0��
	unsigned long count;     // ���Ǹ���
	unsigned long height_d;  // ���Σ��������֣���λΪ�ף�Ʃ��43ʵ������43
	unsigned long height_m;  // ���Σ�С�����֣���λΪ�ף�Ʃ��56ʵ������0.56
};

int32_t InitGps(void);
struct gps_data_t * PraseGPS(void);
void *GpsThread(void *args);
struct gps_data_t * GetGpsInfo(void);
#endif
