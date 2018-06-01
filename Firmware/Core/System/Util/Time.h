#ifndef IOT_TIME_H_
#define IOT_TIME_H_

#include "Generals.h"

/** ============================[ Macros ]============================ */

/**
 * Compute res = a + b.
 *
 * \pre a and b must be normalized 'struct timeval' values.
 *
 * \note res may be the same variable as one of the operands. In other
 *   words, &a == &res || &b == &res may hold.
 */
#define TIME_ADD_TIME( a, b, res )                          \
    {                                                       \
        ( res )->tv_sec = ( a )->tv_sec + ( b )->tv_sec;    \
        ( res )->tv_usec = ( a )->tv_usec + ( b )->tv_usec; \
        if ( ( res )->tv_usec >= 1000000L ) {               \
            ( res )->tv_usec -= 1000000L;                   \
            ( res )->tv_sec++;                              \
        }                                                   \
    }

/**
 * Compute res = a - b.
 *
 * \pre a and b must be normalized 'struct timeval' values.
 *
 * \note res may be the same variable as one of the operands. In other
 *   words, &a == &res || &b == &res may hold.
 */
#define TIME_SUB_TIME( a, b, res )                                     \
    {                                                                  \
        ( res )->tv_sec = ( a )->tv_sec - ( b )->tv_sec - 1;           \
        ( res )->tv_usec = ( a )->tv_usec - ( b )->tv_usec + 1000000L; \
        if ( ( res )->tv_usec >= 1000000L ) {                          \
            ( res )->tv_usec -= 1000000L;                              \
            ( res )->tv_sec++;                                         \
        }                                                              \
    }

/** ============================[ Types ]================== */

/** A pointer to an opaque time marker value. */
typedef void* timeMarker;
typedef const void* timeMarkerConst;

/** =============================[ Functions Prototypes ]================== */

/**
 * Query the current value of the monotonic clock.
 *
 * Returns the current value of a monotonic clock if such a clock is provided by
 * the operating system or the wall clock time if no such clock is provided by
 * the operating system. A monotonic clock is a clock that is never adjusted
 * backwards and that proceeds at the same rate as wall clock time.
 *
 * @param[out] tv - Pointer to monotonic clock time.
 */
void Time_getMonotonicClock( struct timeval* tv );

/**
 * Set a time marker to the current value of the monotonic clock.
 */
void Time_setMonotonicMarker( timeMarker* pm );

/**
 * Returns the difference (in uint32_t 1/100th secs) between the two markers
 * (functionally this is what sysUpTime needs)
 *
 * \deprecated Don't use in new code.
 */
uint32_t Time_timeHdiff( timeMarkerConst first, timeMarkerConst second );

/**
 * @brief Time_diffTime
 *        Calculates the rounded difference in seconds between beginning and end.
 *
 * @param end - higher bound of the time interval whose length is calculated.
 * @param beginning - lower bound of the time interval whose length is calculated.
 * @return the rounded result of (end-beginning) in seconds.
 */
uint Time_diffTime( const struct timeval* end, const struct timeval* beginning );

/**
 * Is the current time past (marked time plus delta) ?
 *
 * @param[in] pm - Pointer to marked time as obtained via
 *                 Tools_setMonotonicMarker().
 * @param[in] deltaMs - Time delta in milliseconds.
 *
 * @return pm != NULL && now >= (*pm + delta_ms)
 */
int Time_readyMonotonic( timeMarkerConst pm, int deltaMs );

/**
 * @brief Time_convertVariablesToDateAndTimeString
 *        converts variables representing the date and
 *        time into a string representing date and time.
 *
 * @details DateAndTime ::= TEXTUAL-CONVENTION
 *   DISPLAY-HINT "2d-1d-1d,1d:1d:1d.1d,1a1d:1d"
 *   STATUS       current
 *   DESCRIPTION
 *           "A date-time specification.
 *
 *           field  octets  contents                  range
 *           -----  ------  --------                  -----
 *             1      1-2   year*                     0..65536
 *             2       3    month                     1..12
 *             3       4    day                       1..31
 *             4       5    hour                      0..23
 *             5       6    minutes                   0..59
 *             6       7    seconds                   0..60
 *                          (use 60 for leap-second)
 *             7       8    deci-seconds              0..9
 *             8       9    direction from UTC        '+' / '-'
 *             9      10    hours from UTC*           0..13
 *            10      11    minutes from UTC          0..59
 *
 *           * Notes:
 *           - the value of year is in network-byte order
 *           - daylight saving time in New Zealand is +13
 *
 *           For example, Tuesday May 26, 1992 at 1:30:15 PM EDT would be
 *           displayed as:
 *
 *                            1992-5-26,13:30:15.0,-4:0
 *
 *           Note that if only local time is known, then timezone
 *           information (fields 8-10) is not present."
 *   SYNTAX       OCTET STRING (SIZE (8 | 11))
 *
 * @param[in,out] buffer - the buffer where the result will be stored
 * @param bufferSize - the size of the buffer
 * @param year - the year
 * @param month - the month
 * @param day - the day
 * @param hour - the hour
 * @param minutes - the minutes
 * @param seconds - the seconds
 * @param deciSeconds -  the decimal seconds
 * @param utcOffsetDirection - the utc offset direction. If utcOffsetDirection >= 0, then '+'. Else '-'.
 * @param utcOffsetHours - the utc offset hours
 * @param utcOffsetMinutes - the utc offset minute
 *
 * @retval ErrorCode_RANGE : if bufferSize < (8 | 11).
 * @retval ErrorCode_SUCCESS: on success.
 */
int Time_convertVariablesToDateAndTimeString( uint8_t* buffer, size_t* bufferSize,
    uint16_t year, uint8_t month, uint8_t day, uint8_t hour, uint8_t minutes, uint8_t seconds,
    uint8_t deciSeconds, int utcOffsetDirection,
    uint8_t utcOffsetHours, uint8_t utcOffsetMinutes );

/**
 * @brief Time_convertDateAndTimeToString
 *        converts struct time_t (representing the date and time)
 *        into string representing date and time.
 *
 * @param when -  the pointer to the struct time_t to be converted.
 * @param length - size of the struct time_t.
 * @return string representing date and time.
 *
 * @see Time_convertVariablesToDateAndTimeString
 */
u_char* Time_convertDateAndTimeToString( const time_t* when, size_t* length );

/**
 * @brief Time_convertCtimeStringToTimet
 *        Interprets the value of a C-string containing a human-readable
 *        version of the time and date and converts it to an object of
 *        type time_t.
 *
 * @param ctimeString - a C-string containing the date and time information
 *                      in a human-readable format: Www Mmm dd hh:mm:ss yyyy
 *                      Where Www is the weekday, Mmm the month (in letters),
 *                      dd the day of the month, hh:mm:ss the time, and yyyy the year.

 * @return object of type time_t that contains a time value
 *
 * @see ctime
 */
time_t Time_convertCtimeStringToTimet( const char* ctimeString );

#endif // IOT_TIME_H_
