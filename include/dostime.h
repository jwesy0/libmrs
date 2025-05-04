/***************************************************************
    libmrs
    Easily manage GunZ: The Duel's .MRS archives
    by Wes (@jwesy0), 2025
***************************************************************/

#ifndef __LIBMRS_DOSTIME_H_
#define __LIBMRS_DOSTIME_H_

#include <stdint.h>
#include <time.h>

#pragma pack(2)
/**
 * MS-DOS date time
 */
struct dostime_t{
    union{
        struct{
            uint16_t second:5; /**< Second divided by 2 */
            uint16_t minute:6; /**< Minute (0–59) */
            uint16_t hour:5; /**< Hour (0–23 on a 24-hour clock) */
        };
        uint16_t time;
    };
    union{
        struct{
            uint16_t day:5; /**< Day of the month (1–31) */
            uint16_t month:4; /**< Month (1 = January, 2 = February, etc.) */
            uint16_t year:7; /**< Year offset from 1980 (add 1980 to get actual year) */
        };
        uint16_t date;
    };
};
#pragma pack()

/**
 * \brief Convert `struct dostime_t` time to `time_t`
 * \param t MS-DOS date time structure to convert to `time_t`
 * \return Corresponding time in `time_t` format if successful, on error returns the current time
 */
time_t mktimedos(const struct dostime_t t);

/**
 * \brief Convert `time_t` time to `struct dostime_t` time
 * \param timep Pointer to a `time_t` time value
 * \return Corresponding time in `struct dostime_t` format if successful, on error returns the current time
 */
struct dostime_t dostime(const time_t* timep);

#endif