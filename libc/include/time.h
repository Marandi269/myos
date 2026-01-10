/*
 * time.h - Time types and functions
 */

#ifndef _TIME_H
#define _TIME_H

#include <stddef.h>

/* Time types */
typedef long time_t;
typedef long clock_t;
typedef long suseconds_t;

/* Clock ticks per second */
#define CLOCKS_PER_SEC 1000000

/* Clock IDs for clock_gettime */
#define CLOCK_REALTIME           0
#define CLOCK_MONOTONIC          1
#define CLOCK_PROCESS_CPUTIME_ID 2
#define CLOCK_THREAD_CPUTIME_ID  3
#define CLOCK_MONOTONIC_RAW      4
#define CLOCK_REALTIME_COARSE    5
#define CLOCK_MONOTONIC_COARSE   6
#define CLOCK_BOOTTIME           7

/* timespec structure */
struct timespec {
    time_t tv_sec;          /* Seconds */
    long   tv_nsec;         /* Nanoseconds */
};

/* timeval structure */
struct timeval {
    time_t      tv_sec;     /* Seconds */
    suseconds_t tv_usec;    /* Microseconds */
};

/* timezone structure */
struct timezone {
    int tz_minuteswest;     /* Minutes west of GMT */
    int tz_dsttime;         /* Type of DST correction */
};

/* tm structure */
struct tm {
    int tm_sec;             /* Seconds (0-60) */
    int tm_min;             /* Minutes (0-59) */
    int tm_hour;            /* Hours (0-23) */
    int tm_mday;            /* Day of the month (1-31) */
    int tm_mon;             /* Month (0-11) */
    int tm_year;            /* Year - 1900 */
    int tm_wday;            /* Day of the week (0-6, Sunday = 0) */
    int tm_yday;            /* Day in the year (0-365) */
    int tm_isdst;           /* Daylight saving time */
    long tm_gmtoff;         /* Seconds east of UTC */
    const char *tm_zone;    /* Timezone abbreviation */
};

/* Time functions */
time_t time(time_t *tloc);
clock_t clock(void);
long difftime(time_t time1, time_t time0);
time_t mktime(struct tm *tm);

/* Time conversion */
struct tm *gmtime(const time_t *timep);
struct tm *gmtime_r(const time_t *timep, struct tm *result);
struct tm *localtime(const time_t *timep);
struct tm *localtime_r(const time_t *timep, struct tm *result);
char *asctime(const struct tm *tm);
char *asctime_r(const struct tm *tm, char *buf);
char *ctime(const time_t *timep);
char *ctime_r(const time_t *timep, char *buf);
size_t strftime(char *s, size_t max, const char *format, const struct tm *tm);
char *strptime(const char *s, const char *format, struct tm *tm);

/* POSIX time functions */
int nanosleep(const struct timespec *req, struct timespec *rem);
int clock_gettime(int clk_id, struct timespec *tp);
int clock_settime(int clk_id, const struct timespec *tp);
int clock_getres(int clk_id, struct timespec *res);
int gettimeofday(struct timeval *tv, struct timezone *tz);
int settimeofday(const struct timeval *tv, const struct timezone *tz);

/* Timer functions */
unsigned int sleep(unsigned int seconds);
int usleep(unsigned int usec);

/* Global timezone variables */
extern char *tzname[2];
extern long timezone;
extern int daylight;
void tzset(void);

#endif /* _TIME_H */
