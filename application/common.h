#ifndef _COMMON_H_
#define _COMMON_H_

//typedef char int8_t;
typedef short int16_t;
typedef int int32_t;

typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;
typedef unsigned long long uint64_t;

typedef char * pint8_t;
typedef short * pint16_t;
typedef int * pint32_t;

typedef unsigned char * puint8_t;
typedef unsigned short * puint16_t;
typedef unsigned int * puint32_t;

//#define DEBUG_ENABLE
#ifdef DEBUG_ENABLE
#define Debugout(format, args...) printf(format, ##args)
#else
#define Debugout(format, args...)
#endif

//#define TEMP_INFO
#ifdef TEMP_INFO
#define TempInfo(format, args...) printf(format, ##args)
#else
#define TempInfo(format, args...)
#endif

#define ERR_INFO_ENABLE
#ifdef ERR_INFO_ENABLE
#define ErrorInfo(format, args...) printf(format, ##args)
#else
#define ErrorInfo(format, args...)
#endif

#define INIT_INFO
#ifdef INIT_INFO
#define InitInfo(format, args...) printf(format, ##args)
#else
#define InitInfo(format, args...)
#endif

#define RET_OK		0
#define RET_FAIL	-1

#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

#endif
