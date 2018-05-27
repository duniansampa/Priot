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
uint32_t Time_uatimeHdiff( timeMarkerConst first, timeMarkerConst second );

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

#endif // IOT_TIME_H_
