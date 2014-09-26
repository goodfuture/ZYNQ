#include <stdio.h>
#include <stdlib.h>
#include <unistd.h> 
#include <sys/types.h> 
#include <sys/stat.h> 
#include <fcntl.h> 
#include <termios.h>  
#include <errno.h>
#include <signal.h>
#include <ctype.h>
#include <unistd.h>
#include<string.h>

#include <sys/ioctl.h>
#include <sys/time.h>
#include <sys/types.h>
#include "gps.h"

// ����汾��
// ��������$��֮��������ļ����ƣ�Բ�����������汾�ţ�Բ��������Ǵΰ汾��
#define Version      "$GPS$0.2#"

// ���Ա�־
int gps_debug = (0);

//��
int wdt;

static struct gps_data_t gps_data;
static struct gps_data_t gps_bak;	// GPS������
static struct tm tm_time;			// GPS��ʱ��
FILE *fi;
int fd;
char gps_buff[512];

int timesync;  // ʱ��ͬ����־������д˱�־��ͬ����ɾ��˳�����
int real;      // ʵ���ݷ���ģʽ

int to_pm;
//int to_lcd;

static int s_gpsOK_1 = 0, s_gpsOK_2 = 0;

void writetofile( void );
static void proc_gps( char *buff, int length );
void proc_args( int argc, char **argv );
void proc_error( void );
void main_therad_alarm( int sig );
int set_opt(int fd,int nSpeed, int nBits, char nEvent, int nStop);

void sig_term( int sig )
{
	sig = sig;
	fclose(fi);
	close(fd);
	exit(0);
}

int set_opt(int fd, int nSpeed, int nBits, char nEvent, int nStop)
{
	struct termios newtio, oldtio;
	if  (tcgetattr(fd, &oldtio)  !=  0) 
	{ 
		perror("SetupSerial 1");
		return -1;
	}
	bzero(&newtio, sizeof(newtio));
	newtio.c_cflag |= CLOCAL | CREAD; 
	newtio.c_cflag &= ~CSIZE; 

	switch( nBits )
	{
	case 7:
		newtio.c_cflag |= CS7;
		break;
	case 8:
		newtio.c_cflag |= CS8;
		break;
	}

	switch( nEvent )
	{
	case 'O':                     //��У��
		newtio.c_cflag |= PARENB;
		newtio.c_cflag |= PARODD;
		newtio.c_iflag |= (INPCK | ISTRIP);
		break;
	case 'E':                     //żУ��
		newtio.c_iflag |= (INPCK | ISTRIP);
		newtio.c_cflag |= PARENB;
		newtio.c_cflag &= ~PARODD;
		break;
	case 'N':                    //��У��
		newtio.c_cflag &= ~PARENB;
		break;
	}

	switch( nSpeed )
	{
	case 2400:
		cfsetispeed(&newtio, B2400);
		cfsetospeed(&newtio, B2400);
		break;
	case 4800:
		cfsetispeed(&newtio, B4800);
		cfsetospeed(&newtio, B4800);
		break;
	case 9600:
		cfsetispeed(&newtio, B9600);
		cfsetospeed(&newtio, B9600);
		break;
	case 115200:
		cfsetispeed(&newtio, B115200);
		cfsetospeed(&newtio, B115200);
		break;
	default:
		cfsetispeed(&newtio, B9600);
		cfsetospeed(&newtio, B9600);
		break;
	}
	if( nStop == 1 )
	{
		newtio.c_cflag &=  ~CSTOPB;
	}
	else if ( nStop == 2 )
	{
		newtio.c_cflag |=  CSTOPB;
	}

	newtio.c_cflag &= ~CRTSCTS;	
	newtio.c_iflag = 0;
	newtio.c_oflag = 0;
	newtio.c_lflag = 0;
	newtio.c_cc[VTIME]  = 20;
	newtio.c_cc[VMIN] = 0;

	tcflush(fd, TCIFLUSH);
	if(tcsetattr(fd, TCSANOW, &newtio) != 0)
	{
		//perror("com set error");
		write(2, "Searial Set Error\n", 19);
		return -1;
	}
	//printf("set done!\n");
	write(2, "Set Done\n", 10);
	return 0;
}


int org_main( int argc, char **argv )
{
	struct termios uartsettings;
	int iRet = 0;
	signal(SIGTERM, sig_term);
	signal(SIGINT, sig_term);
	
	timesync = 0;
	real = 0;
	wdt = 0;
	int i = 0;
	
	proc_args(argc, argv);

	fd = open("/dev/ttyPS1", O_RDWR|O_NOCTTY|O_NDELAY);
	if (fd < 0)
	{
		write(2, "Open Serial Failed!\n", 21);
		exit(1);
	}
	iRet = set_opt(fd, 9600, 8, 'N', 1);
	if (iRet < 0)
	{
		write(2, "Set Serial Failed!\n", 20);
		close(fd);
		exit(1);
	}

	fi = fdopen(fd, "r");
	if (fi<=0)
	{
		write(2, "ERROR2", 6);
		close(fd);
		fclose(fi);
		exit(2);
	}
	write(2, Version, sizeof(Version));
	if (gps_debug!=0)
	{
		write(2, "DEBUG VERSION\n", 14);
	}
	fflush(fi);
	memset(&gps_data, 0, sizeof(gps_data));
	memset(&gps_bak, 0, sizeof(gps_bak));
	signal(SIGALRM, main_therad_alarm);
	alarm(1);
	for(;;)
	{
		//printf("Inside for.\n");
		wdt = 0;
		memset(&gps_buff, 0, sizeof(gps_buff));
		fgets(gps_buff, sizeof(gps_buff), fi);
		if (real!=0)
		{
			printf(gps_buff);
		}
		else
		{
			proc_gps(gps_buff, strlen(gps_buff));	
		}
		proc_error();
	}
}
/*
	GPSʮ���Ӷ�û�г��������Զ��˳�����
*/
void main_therad_alarm( int sig )
{
	//printf("In %s\n", __FUNCTION__);
	wdt ++;
	alarm(1);
	if(wdt > 10)
		sig_term(0);
}

/*
	����GPS��������
*/
void proc_error( void )
{
	char buff[64];
	static int err_count=0;
	memset(buff, 0, sizeof(buff));
	//strcpy(buff, asctime(&gps_data.tm_time));
	strcpy(buff, asctime(&tm_time));
	if (strchr(buff, '?') != NULL)
	{
		err_count ++;
		if (err_count > 50)
		{
			err_count = 0;
			write(fileno(fi), "$PFST,START,1\xd\xa", 15);
		}
	}
	else
	{
		err_count = 0;
	}
}

static int gps_checksum( void );
static void gps_proc_frame( void );
static void gps_gpgga( void );
static void gps_gprmc( void );
//static void gps_gpvtg( void );
//static void gps_gpgsa( void );
//static void gps_gpgsv( void );
//static void proc_card( void );

char sbuff[256];
int s_index=-1;
//
//	GPS���ݰ���������ɰ��󣬵���У�顢�������������ݴ���
//
static void proc_gps( char *buff, int length )
{
	int i;
	for(i=0; i<length; i++)
	{
		if (buff[i] == '$')  // ����Ƿ��п�ͷ
		{
			s_index = 1;
			sbuff[0] = buff[i];
			sbuff[s_index] = 0;
			continue;
		}
		if (s_index==-1)  // ��ʼ����ֻ��鿪ͷ����������
		{
			continue;
		}
		if (buff[i]==13)
		{
			if (gps_checksum()==0)
			{
				gps_proc_frame();
			}
			s_index = -1;
			continue;
		}
		sbuff[s_index] = buff[i];
		s_index ++;
		sbuff[s_index] = 0;
	}
}

//
//	GPS���ݰ�У��͵ļ��㣬
//	��ȷ������0�����󣬷���1
//
static int gps_checksum( void )
{
	int i;
	char *ptr1, *ptr2, cs, css[4];
	ptr1 = &sbuff[1];
	ptr2 = strchr(sbuff, '*');
	cs = 0;
	for(i=0; i< (ptr2-ptr1); i++)
	{
		cs ^= ptr1[i];
	}
	sprintf(css, "%02X", cs);
	ptr2 ++;
	return (strncmp(css, ptr2, 2)!=0);
}

//
//	GPS���ݴ�����
//
char *cmd_list[64];  // �����б�
int cmd_count;
static void gps_proc_frame( void )
{
	char *ptr1, *ptr2;
	struct timeval aa, bb;
	int i=0;
	ptr1 = sbuff;
	for(;;)
	{
		cmd_list[i] = ptr1;
		ptr2 = strchr(ptr1, ',');
		if (ptr2==NULL)break;
		*ptr2 = 0;
		ptr1 = ptr2;
		ptr1 ++;
		i ++;
	}
	ptr1 = strchr(ptr1, '*');
	*ptr1 = 0;
	if (i==0)return;    // ����ʶ������ݰ�
	cmd_count = i + 1;

	//printf("Header: %s.\n", cmd_list[0]);
	//gettimeofday(&aa, NULL);
	gps_gpgga();
	gps_gprmc();
	//gettimeofday(&bb, NULL);
	//printf("Cost: %d.\n", bb.tv_usec - aa.tv_usec);
	//gps_gpvtg();
	//gps_gpgsa();
	//gps_gpgsv();
}
//
//	����GPGGA����
//
// $GPGGA,055858.000,2307.1963,N,11316.2968,E,1,06,3.4,57.1,M,-6.7,M,,0000*7D
static void gps_gpgga( void )
{
	//printf("Inside %s.\n", __FUNCTION__);
	if(strcmp(cmd_list[0], "$GPGGA")!=0)
		return;

	s_gpsOK_1 = 1;

	if (isdigit(cmd_list[7][0]))
	{
		sscanf( cmd_list[7], "%02ld", &gps_data.count);  // ����ʹ�õ����Ǹ���
		
	}
	//printf("Available Sat: %ld.\n", gps_data.count);

	sscanf(cmd_list[9], "%ld.%ld", &gps_data.height_d, &gps_data.height_m);
	//printf("Height: %ld.%ld.\n", gps_data.height_d, gps_data.height_m);
}

// $GPRMC,055858.000,A,2307.1963,N,11316.2968,E,0.00,117.11,081208,,,A*65
static void gps_gprmc( void )
{
	//printf("Inside %s.\n", __FUNCTION__);
	time_t tmg;
	struct timeval tv;  // ʱ��
	struct timezone tz; // ʱ��
	int year;   // �꣬���ڼ���������2000��
	int tmp;
	if(strcmp(cmd_list[0], "$GPRMC")!=0)
		return;

	s_gpsOK_2 = 1;
	memset(&gps_data.tm_time, 0, sizeof(gps_data.tm_time));
	// GPSʱ��
	if (isdigit(cmd_list[1][0]))
	{
		sscanf(cmd_list[1], "%02d%02d%02d",&gps_data.tm_time.tm_hour, &gps_data.tm_time.tm_min, &gps_data.tm_time.tm_sec);
		sscanf(cmd_list[1], "%02d%02d%02d",&tm_time.tm_hour, &tm_time.tm_min, &tm_time.tm_sec);
	}
	
	// GPS����
	if (isdigit(cmd_list[7][0]))
	{
		sscanf(cmd_list[9], "%02d%02d%02d",&gps_data.tm_time.tm_mday, &gps_data.tm_time.tm_mon, &year);
		sscanf(cmd_list[9], "%02d%02d%02d",&tm_time.tm_mday, &tm_time.tm_mon, &year);
	}
	year += 2000;  // 2000��
	gps_data.tm_time.tm_year = year;


	//printf("%s %s.\n", cmd_list[1], cmd_list[9]);

	tm_time.tm_year = year-1900;	// ת��Ϊlinux��ʽ����
	tm_time.tm_mon -= 1;			// ת��Ϊlinux��ʽ����
	tmg = mktime(&tm_time);			// GPS��UTCʱ����1970��1��1������������������

	gettimeofday(&tv, &tz);			// ȡ����UTCʱ���ʱ��
	// �������UTCʱ���GPS��UTCʱ��������63�룬�����ñ���ʱ��
	if (((unsigned long)tv.tv_sec&0xffffffc0) != ((unsigned long)tmg&0xffffffc0))
	{
		if (gps_debug!=0)
		{
			printf("Sync local time...");
		}
		tv.tv_sec = tmg;
		tv.tv_usec = 0;
		tz.tz_dsttime = 0;
		tz.tz_minuteswest = -(8*60);		// ʱ��Ϊ��8��
		settimeofday(&tv, &tz);
		if (gps_debug!=0)
		{
			printf("Done.\n");
		}
	}
	if (timesync != 0)
	{
		sig_term(0);
	}
	// γ��
	if (isdigit(cmd_list[3][0]))
		sscanf(cmd_list[3], "%02ld%02ld.%04d", &gps_data.ns_d, &gps_data.ns_m10000, &tmp);
	gps_data.ns_m10000 *= 10000;
	gps_data.ns_m10000 += tmp;
	//printf("Latitude: %ld %ld.\n", gps_data.ns_d, gps_data.ns_m10000);
	// ����
	if (isdigit(cmd_list[5][0]))	
	sscanf(cmd_list[5], "%03ld%02ld.%04d", &gps_data.ew_d, &gps_data.ew_m10000, &tmp);
	gps_data.ew_m10000 *= 10000;
	gps_data.ew_m10000 += tmp;
	//printf("Longitude: %ld %ld.\n", gps_data.ew_d, gps_data.ew_m10000);
	// �ϱ�����
	gps_data.ns = cmd_list[4][0];
	//printf("NW: %c.\n", gps_data.ns);
	// ��������
	gps_data.ew = cmd_list[6][0];
	//printf("EW: %c.\n", gps_data.ew);
	// ��λ״̬
	gps_data.status = cmd_list[2][0]; // A: located; V: Not located.
	//printf("Status: %c.\n", gps_data.status);
	// �õ�һ�������ǵڶ�����
}

// $GPVTG,117.11,T,,M,0.00,N,0.0,K,A*0A
//static void gps_gpvtg( void )
//{
//	int tmp;
//	if(strcmp(cmd_list[0], "$GPVTG")!=0)return;
//	// ���з����: x.xx��
//	if (isdigit(cmd_list[1][0]))
//	{
//		sscanf(cmd_list[1], "%ld.%02d", &gps_data.cog100, &tmp);
//		gps_data.cog100 *= 100;
//		gps_data.cog100 += tmp;
//	}
//	else
//	{
//		gps_data.cog100 = 0;
//	}
//	// �����ٶȣ�x.xx��
//	if (isdigit(cmd_list[5][0]))
//	{
//		sscanf(cmd_list[5], "%ld.%02d", &gps_data.speed100, &tmp);
//		gps_data.speed100 *= 100;
//		gps_data.speed100 += tmp;
//	}
//	else
//	{
//		gps_data.speed100 = 0;
//	}
//	if (gps_data.status == 'V')  // δ��λ״̬
//	{
//		gps_data.ns_d = gps_bak.ns_d;
//		gps_data.ns_m10000 = gps_bak.ns_m10000;
//		gps_data.ew_d = gps_bak.ew_d;
//		gps_data.ew_m10000 = gps_bak.ew_m10000;
//		gps_data.ns = gps_bak.ns;
//		gps_data.ew = gps_bak.ew;
//		gps_data.cog100 = gps_bak.cog100;
//		gps_data.speed100 = 0;
//		gps_data.ns = gps_bak.ns;
//	}
//	else
//	{
//		memcpy(&gps_bak, &gps_data, sizeof(gps_data));
//	}
//	proc_card();
//	if (gps_debug==0)
//	{
//		//write(fileno(stdout), &gps_data, sizeof(gps_data));	// д���ݵ���׼���
//		//write(to_maind, &gps_data, sizeof(gps_data));			// д���ݵ���׼���
//		write(to_pm, &gps_data, sizeof(gps_data));				// д���ݵ���׼���
//		//write(to_lcd, &gps_data, sizeof(gps_data));
//		writetofile();
//	}
//}
//
//// $GPGSA,A,3,28,17,20,32,23,13,,,,,,,4.8,3.4,3.4*32
//static void gps_gpgsa( void )
//{
//	//printf("Inside %s.\n", __FUNCTION__);
//	if(strcmp(cmd_list[0], "$GPGSA")!=0)return;
//	
//}
//
//// $GPGSV,3,1,12,28,66,169,36,04,54,291,,17,53,011,47,20,29,047,46*7D
//// $GPGSV,3,2,12,02,20,256,24,23,08,099,42,13,07,124,20,32,07,038,34*79
//// $GPGSV,3,3,12,08,06,186,21,11,04,061,25,09,03,300,22,27,00,177,*71
//static void gps_gpgsv( void )
//{
//	//printf("Inside %s.\n", __FUNCTION__);
//	if(strcmp(cmd_list[0], "$GPGSV")!=0)return;
//	
//}

#define NNNN1 (22080000)
#define NNNN2 (22260000)

#define EEEE1 (113549000)
#define EEEE2 (114255000)

char test_buff[64];
//static void proc_card( void )
//{
//	long jd, wd;
//	jd = gps_data.ew_d;
//	jd *= 1000000;
//	jd += gps_data.ew_m10000;
//	wd = gps_data.ns_d;
//	wd *= 1000000;
//	wd += gps_data.ns_m10000;
//	if (gps_data.card==0)  // �ڴ�½
//	{
//		if ((wd>(NNNN1+50)) &&
//				(wd<(NNNN2-50)) &&
//				(jd>(EEEE1+50)) &&
//				(jd<(EEEE2-50))
//			 )
//	  {
//	  	gps_data.card = 1;
//	  }
//	}
//	else if (gps_data.card==1)  // �����
//	{
//		if ((wd<(NNNN1-50)) &&
//				(wd>(NNNN2+50)) &&
//				(jd<(EEEE1-50)) &&
//				(jd>(EEEE2+50))
//			 )
//	  {
//	  	gps_data.card = 0;
//	  }
//	}
//	if (gps_debug!=0)
//	{
//		printf("UTC Time : %s", asctime(&gps_data.tm_time));
//		printf("Locate : %c %02ld""ld%02ld.%04ld\', %c %02ld""ld%02ld.%04ld\'\n",
//		gps_data.ns,gps_data.ns_d, 
//		gps_data.ns_m10000/10000, 
//		gps_data.ns_m10000%10000, 
//		gps_data.ew, 
//		gps_data.ew_d, 
//		gps_data.ew_m10000/10000, 
//		gps_data.ew_m10000%10000
//		);
//		printf("Locate status : %s\n", (gps_data.status=='A')?"Active":"InActive");
//		printf("Satellites : %ld\n", gps_data.count);
//		printf("Speed : %ld.%02ld kn\n", gps_data.speed100/100, gps_data.speed100%100);
//		printf("Course : %ld.%02ld cog\n", gps_data.cog100/100, gps_data.cog100%100);
//		printf("Uses Card : %d#\n", gps_data.card);
//		printf("\n");
//	}
//}

void proc_args( int argc, char **argv )
{
	int i;
	if (argc<2)return;							// �޲���
	for( i=1; i<argc; i++ )
	{
		if (strcmp(argv[i], "-timesync")==0)	// ʱ��ͬ��
		{
			timesync = 1;
			continue;
		}
		if (strcmp(argv[i], "-d")==0)			// ����ģʽ
		{
			gps_debug = 1;
		}
		if (strcmp(argv[i], "-r")==0)			// ʵ����ģʽ
		{
			real = 1;
		}
	}
}

void writetofile( void )
{
	FILE *fi;
	fi = fopen("/usr/gps_data", "rb+");
	if (fi==NULL)
	{
		fi = fopen("/usr/gps_data", "wb+");
	}
	write(fileno(fi), &gps_data, sizeof(gps_data));
	fclose(fi);
}

int32_t InitGps(void)
{
	int32_t iRet = 0;

	fd = open("/dev/ttyPS1", O_RDWR|O_NOCTTY);
	if (fd < 0)
	{
		write(2, "Open Serial Failed!\n", 21);
		return RET_FAIL;
	}

	iRet = set_opt(fd, 9600, 8, 'N', 1);
	if (iRet < 0)
	{
		write(2, "Set Serial Failed!\n", 20);
		close(fd);
		return RET_FAIL;
	}

	fi = fdopen(fd, "r");
	if (fi<=0)
	{
		write(2, "ERROR2", 6);
		close(fd);
		fclose(fi);
		return RET_FAIL;
	}

	write(2, Version, sizeof(Version));

	fflush(fi);
	memset(&gps_data, 0, sizeof(gps_data));
	memset(&gps_bak, 0, sizeof(gps_bak));

	InitInfo("\nGPS Initialize Sucussfully.\n");

	return RET_OK;
}

struct gps_data_t * PraseGPS(void)
{
	//fflush(fi);
	tcflush(fd, TCIOFLUSH);
	for (;;)
	{
		memset(&gps_buff, 0, sizeof(gps_buff));
		//memset(&gps_data, 0, sizeof(gps_data));
		fgets(gps_buff, sizeof(gps_buff), fi);
		//printf("After fgets.\n");
		proc_gps(gps_buff, strlen(gps_buff));
		if (s_gpsOK_1 & s_gpsOK_2)
		{
			break;
		}
	}

	s_gpsOK_1 = 0;
	s_gpsOK_2 = 0;
	printf("%d %d %d %d %d %d\n", gps_data.tm_time.tm_year, gps_data.tm_time.tm_mon, gps_data.tm_time.tm_mday, \
		gps_data.tm_time.tm_hour, gps_data.tm_time.tm_min, gps_data.tm_time.tm_sec);

	return &gps_data;
}

void *GpsThread(void *args)
{
	//fflush(fi);
	tcflush(fd, TCIOFLUSH);
	printf("inside %s\n", __FUNCTION__);
	for (;;)
	{
		memset(&gps_buff, 0, sizeof(gps_buff));
		//memset(&gps_data, 0, sizeof(gps_data));
		fgets(gps_buff, sizeof(gps_buff), fi);
		//printf("After fgets.\n");
		proc_gps(gps_buff, strlen(gps_buff));
		if (s_gpsOK_1 & s_gpsOK_2)
		{
			s_gpsOK_1 = 0;
			s_gpsOK_2 = 0;
			//printf("%d %d %d %d %d %d\n", gps_data.tm_time.tm_year, gps_data.tm_time.tm_mon, gps_data.tm_time.tm_mday,
			//	gps_data.tm_time.tm_hour, gps_data.tm_time.tm_min, gps_data.tm_time.tm_sec);
		}	
		
	}

	return NULL;
}

struct gps_data_t * GetGpsInfo(void)
{
	return &gps_data;
}
