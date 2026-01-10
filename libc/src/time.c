/*
 * time.c - Time functions
 */

#include <time.h>
#include <string.h>
#include <syscall.h>

/* Forward declare snprintf */
int snprintf(char *str, size_t size, const char *format, ...);

/* Global timezone variables */
char *tzname[2] = { "UTC", "UTC" };
long timezone = 0;
int daylight = 0;

/* Days in each month (non-leap year) */
static const int days_in_month[] = {
    31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31
};

/* Days before each month (non-leap year) */
static const int days_before_month[] = {
    0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334
};

static int is_leap_year(int year) {
    return (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
}

time_t time(time_t *tloc) {
    struct timespec ts;
    if (clock_gettime(CLOCK_REALTIME, &ts) < 0) {
        return -1;
    }
    if (tloc) {
        *tloc = ts.tv_sec;
    }
    return ts.tv_sec;
}

clock_t clock(void) {
    struct timespec ts;
    if (clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &ts) < 0) {
        return -1;
    }
    return ts.tv_sec * CLOCKS_PER_SEC + ts.tv_nsec / 1000;
}

/* Return diff as long (avoid floating point with no SSE) */
long difftime(time_t time1, time_t time0) {
    return (long)(time1 - time0);
}

/* Convert tm to time_t (simplified, assumes UTC) */
time_t mktime(struct tm *tm) {
    time_t result = 0;
    int year;

    /* Years from 1970 */
    for (year = 1970; year < tm->tm_year + 1900; year++) {
        result += is_leap_year(year) ? 366 : 365;
    }

    /* Days before this month */
    result += days_before_month[tm->tm_mon];
    if (tm->tm_mon > 1 && is_leap_year(tm->tm_year + 1900)) {
        result++;
    }

    /* Days */
    result += tm->tm_mday - 1;

    /* Convert to seconds */
    result = result * 86400 + tm->tm_hour * 3600 + tm->tm_min * 60 + tm->tm_sec;

    /* Update tm_wday and tm_yday */
    tm->tm_wday = ((result / 86400) + 4) % 7;  /* Jan 1, 1970 was Thursday */
    tm->tm_yday = days_before_month[tm->tm_mon] + tm->tm_mday - 1;
    if (tm->tm_mon > 1 && is_leap_year(tm->tm_year + 1900)) {
        tm->tm_yday++;
    }

    return result;
}

/* Convert time_t to tm (UTC) */
struct tm *gmtime_r(const time_t *timep, struct tm *result) {
    time_t t = *timep;
    int days, year;

    result->tm_sec = t % 60;
    t /= 60;
    result->tm_min = t % 60;
    t /= 60;
    result->tm_hour = t % 24;
    days = t / 24;

    result->tm_wday = (days + 4) % 7;  /* Jan 1, 1970 was Thursday */

    /* Find year */
    year = 1970;
    while (days >= (is_leap_year(year) ? 366 : 365)) {
        days -= is_leap_year(year) ? 366 : 365;
        year++;
    }
    result->tm_year = year - 1900;
    result->tm_yday = days;

    /* Find month */
    int leap = is_leap_year(year);
    int mon;
    for (mon = 0; mon < 11; mon++) {
        int dim = days_in_month[mon] + (mon == 1 && leap ? 1 : 0);
        if (days < dim) break;
        days -= dim;
    }
    result->tm_mon = mon;
    result->tm_mday = days + 1;

    result->tm_isdst = 0;
    result->tm_gmtoff = 0;
    result->tm_zone = "UTC";

    return result;
}

static struct tm __gmtime_result;

struct tm *gmtime(const time_t *timep) {
    return gmtime_r(timep, &__gmtime_result);
}

struct tm *localtime_r(const time_t *timep, struct tm *result) {
    /* For now, local time = UTC */
    return gmtime_r(timep, result);
}

static struct tm __localtime_result;

struct tm *localtime(const time_t *timep) {
    return localtime_r(timep, &__localtime_result);
}

/* Day names */
static const char *day_names[] = {
    "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"
};

/* Month names */
static const char *month_names[] = {
    "Jan", "Feb", "Mar", "Apr", "May", "Jun",
    "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
};

char *asctime_r(const struct tm *tm, char *buf) {
    snprintf(buf, 26, "%.3s %.3s%3d %.2d:%.2d:%.2d %d\n",
             day_names[tm->tm_wday],
             month_names[tm->tm_mon],
             tm->tm_mday,
             tm->tm_hour,
             tm->tm_min,
             tm->tm_sec,
             tm->tm_year + 1900);
    return buf;
}

static char __asctime_buf[26];

char *asctime(const struct tm *tm) {
    return asctime_r(tm, __asctime_buf);
}

char *ctime_r(const time_t *timep, char *buf) {
    struct tm tm;
    localtime_r(timep, &tm);
    return asctime_r(&tm, buf);
}

char *ctime(const time_t *timep) {
    return ctime_r(timep, __asctime_buf);
}

size_t strftime(char *s, size_t max, const char *format, const struct tm *tm) {
    char *p = s;
    char *end = s + max - 1;
    char buf[32];

    while (*format && p < end) {
        if (*format != '%') {
            *p++ = *format++;
            continue;
        }

        format++;
        switch (*format) {
            case 'a': /* Abbreviated weekday name */
                strncpy(p, day_names[tm->tm_wday], end - p);
                p += 3;
                break;
            case 'A': /* Full weekday name */
                /* Simplified - just use abbreviated */
                strncpy(p, day_names[tm->tm_wday], end - p);
                p += 3;
                break;
            case 'b':
            case 'h': /* Abbreviated month name */
                strncpy(p, month_names[tm->tm_mon], end - p);
                p += 3;
                break;
            case 'B': /* Full month name */
                strncpy(p, month_names[tm->tm_mon], end - p);
                p += 3;
                break;
            case 'd': /* Day of month */
                snprintf(buf, sizeof(buf), "%02d", tm->tm_mday);
                strncpy(p, buf, end - p);
                p += 2;
                break;
            case 'e': /* Day of month, space padded */
                snprintf(buf, sizeof(buf), "%2d", tm->tm_mday);
                strncpy(p, buf, end - p);
                p += 2;
                break;
            case 'H': /* Hour (24-hour) */
                snprintf(buf, sizeof(buf), "%02d", tm->tm_hour);
                strncpy(p, buf, end - p);
                p += 2;
                break;
            case 'I': /* Hour (12-hour) */
                snprintf(buf, sizeof(buf), "%02d", tm->tm_hour % 12 ? tm->tm_hour % 12 : 12);
                strncpy(p, buf, end - p);
                p += 2;
                break;
            case 'j': /* Day of year */
                snprintf(buf, sizeof(buf), "%03d", tm->tm_yday + 1);
                strncpy(p, buf, end - p);
                p += 3;
                break;
            case 'm': /* Month */
                snprintf(buf, sizeof(buf), "%02d", tm->tm_mon + 1);
                strncpy(p, buf, end - p);
                p += 2;
                break;
            case 'M': /* Minute */
                snprintf(buf, sizeof(buf), "%02d", tm->tm_min);
                strncpy(p, buf, end - p);
                p += 2;
                break;
            case 'n': /* Newline */
                *p++ = '\n';
                break;
            case 'p': /* AM/PM */
                strncpy(p, tm->tm_hour < 12 ? "AM" : "PM", end - p);
                p += 2;
                break;
            case 'S': /* Second */
                snprintf(buf, sizeof(buf), "%02d", tm->tm_sec);
                strncpy(p, buf, end - p);
                p += 2;
                break;
            case 't': /* Tab */
                *p++ = '\t';
                break;
            case 'w': /* Weekday number */
                *p++ = '0' + tm->tm_wday;
                break;
            case 'Y': /* Year with century */
                snprintf(buf, sizeof(buf), "%04d", tm->tm_year + 1900);
                strncpy(p, buf, end - p);
                p += 4;
                break;
            case 'y': /* Year without century */
                snprintf(buf, sizeof(buf), "%02d", (tm->tm_year + 1900) % 100);
                strncpy(p, buf, end - p);
                p += 2;
                break;
            case 'Z': /* Timezone name */
                strncpy(p, tm->tm_zone ? tm->tm_zone : "UTC", end - p);
                p += 3;
                break;
            case '%':
                *p++ = '%';
                break;
            default:
                *p++ = '%';
                if (p < end) *p++ = *format;
                break;
        }
        format++;
    }

    *p = '\0';
    return p - s;
}

char *strptime(const char *s, const char *format, struct tm *tm) {
    (void)s;
    (void)format;
    (void)tm;
    /* Not fully implemented */
    return NULL;
}

int nanosleep(const struct timespec *req, struct timespec *rem) {
    return syscall2(SYS_nanosleep, (long)req, (long)rem);
}

int clock_gettime(int clk_id, struct timespec *tp) {
    return syscall2(SYS_clock_gettime, clk_id, (long)tp);
}

int clock_settime(int clk_id, const struct timespec *tp) {
    (void)clk_id;
    (void)tp;
    return -1;  /* Not implemented */
}

int clock_getres(int clk_id, struct timespec *res) {
    (void)clk_id;
    if (res) {
        res->tv_sec = 0;
        res->tv_nsec = 1000000;  /* 1ms resolution */
    }
    return 0;
}

int gettimeofday(struct timeval *tv, struct timezone *tz) {
    if (tv) {
        struct timespec ts;
        if (clock_gettime(CLOCK_REALTIME, &ts) < 0) {
            return -1;
        }
        tv->tv_sec = ts.tv_sec;
        tv->tv_usec = ts.tv_nsec / 1000;
    }
    if (tz) {
        tz->tz_minuteswest = 0;
        tz->tz_dsttime = 0;
    }
    return 0;
}

int settimeofday(const struct timeval *tv, const struct timezone *tz) {
    (void)tv;
    (void)tz;
    return -1;  /* Not implemented */
}

/* sleep and usleep are defined in unistd.c */

void tzset(void) {
    /* Not implemented - use UTC */
}
