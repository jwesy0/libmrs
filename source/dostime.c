/***************************************************************
    libmrs
    Easily manage GunZ: The Duel's .MRS archives
    by Wes (@jwesy0), 2025
***************************************************************/

#include <stdio.h>
#include <string.h>

#include "dostime.h"

#ifdef _LIBMRS_DBG
#define dbgprintf(...) printf("%s: ", __FUNCTION__); \
                       printf(__VA_ARGS__); \
                       printf("\n")
#else
#define dbgprintf(...)
#endif

time_t mktimedos(const struct dostime_t t){
    struct tm nt;
    time_t ot;

    nt.tm_sec   = t.second * 2;
    nt.tm_min   = t.minute;
    nt.tm_hour  = t.hour;
    nt.tm_mday  = t.day;
    nt.tm_mon   = t.month - 1;
    nt.tm_year  = t.year + 80;
    nt.tm_isdst = 0;
    ot = mktime(&nt);
    
    return ot;
}

struct dostime_t dostime(const time_t* timep){
    struct dostime_t ot;
    struct tm nt;

#if _POSIX_C_SOURCE >= 1 || defined(_XOPEN_SOURCE) || defined(_BSD_SOURCE) || defined(_SVID_SOURCE) || defined(_POSIX_SOURCE)
    dbgprintf("Using POSIX localtime_r");
    localtime_r(timep, &nt);
#elif defined(_WIN32)
    dbgprintf("Using Windows localtime_s");
    localtime_s(&nt, timep);
#else
    ///TODO: Prevent multithreading problems
    dbgprintf("Using default localtime");
    memcpy(&nt, localtime(timep), sizeof(struct tm));
#endif
    
    ot.second = nt.tm_sec / 2;
    ot.minute = nt.tm_min;
    ot.hour   = nt.tm_hour;
    ot.day    = nt.tm_mday;
    ot.month  = nt.tm_mon + 1;
    ot.year   = nt.tm_year - 80;

    return ot;
}
