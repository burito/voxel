#include "log.h"

#include <stdio.h>
#include <stdarg.h>
#include <stdint.h>

#include <time.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#ifndef __GNUC__
// https://stackoverflow.com/questions/5404277/porting-clock-gettime-to-windows
#define exp7           10000000i64     //1E+7     //C-file part
#define exp9         1000000000i64     //1E+9
#define w2ux 116444736000000000i64     //1.jan1601 to 1.jan1970
void unix_time(struct timespec *spec)
{  __int64 wintime; GetSystemTimeAsFileTime((FILETIME*)&wintime); 
   wintime -=w2ux;  spec->tv_sec  =wintime / exp7;                 
                    spec->tv_nsec =wintime % exp7 *100;
}
int clock_gettime(int X, struct timespec *spec)
{  static  struct timespec startspec; static double ticks2nano;
   static __int64 startticks, tps =0;    __int64 tmp, curticks;
   QueryPerformanceFrequency((LARGE_INTEGER*)&tmp); //some strange system can
   if (tps !=tmp) { tps =tmp; //init ~~ONCE         //possibly change freq ?
                    QueryPerformanceCounter((LARGE_INTEGER*)&startticks);
                    unix_time(&startspec); ticks2nano =(double)exp9 / tps; }
   QueryPerformanceCounter((LARGE_INTEGER*)&curticks); curticks -=startticks;
   spec->tv_sec  =startspec.tv_sec   +         (curticks / tps);
   spec->tv_nsec =startspec.tv_nsec  + (double)(curticks % tps) * ticks2nano;
         if (!(spec->tv_nsec < exp9)) { spec->tv_sec++; spec->tv_nsec -=exp9; }
   return 0;
}

#endif
#endif


void log_init(void)
{

#ifdef _WIN32
	// enable ANSI codes in windows console (conhost.exe)
	// http://www.nivot.org/blog/post/2016/02/04/Windows-10-TH2-(v1511)-Console-Host-Enhancements
	DWORD mode;
	HANDLE console = GetStdHandle(-11); // STD_OUTPUT_HANDLE
	GetConsoleMode(console, &mode);
	mode = mode | 4; // ENABLE_VIRTUAL_TERMINAL_PROCESSING
	SetConsoleMode(console, mode);
#endif
}

static const char * log_label(enum LOG_LEVEL level)
{
	switch(level) {
	case LOG_TRACE:
		return "\x1b[37m[TRACE]\x1b[0m";		// bright white
	case LOG_DEBUG:
		return "\x1b[36m[DEBUG]\x1b[0m";		// cyan
	case LOG_VERBOSE:
		return "\x1b[34m[VERBOSE]\x1b[0m";		// blue
	case LOG_INFO:
		return "\x1b[32m[INFO]\x1b[0m";		// green
	case LOG_WARNING:
		return "\x1b[33m[WARN]\x1b[0m";	// yellow
	case LOG_ERROR:
		return "\x1b[35m[ERROR]\x1b[0m";		// magenta
	case LOG_FATAL:
		return "\x1b[31m[FATAL]\x1b[0m";		// red
	default:
		return "\x1b[0m";		// reset
	}
}


// https://docs.microsoft.com/en-us/windows/console/console-virtual-terminal-sequences
void log_out(char* file, int line, enum LOG_LEVEL level, char *fmt, ...)
{
#ifndef __GNUC__
	char time_buf [128];
	struct timespec tv;
	clock_gettime(0, &tv); // CLOCK_MONOTONIC?
	struct tm tm_now;
	localtime_s( &tm_now, &tv.tv_sec);
	strftime( time_buf, 128, "%Y-%m-%dT%H:%M:%S", &tm_now);
	printf( "%s.%09ld ", time_buf, tv.tv_nsec );
#endif

//	if(LOG_TRACE == level)
//		printf("\x1b[1m%s:%d", file, line );


	va_list args;
	va_start(args, fmt);
	printf("%s ", log_label(level));

	vprintf(fmt, args);
	printf("\x1b[0m\n");
	va_end(args);

}