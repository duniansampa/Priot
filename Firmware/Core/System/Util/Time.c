#include "Time.h"
#include "System/Util/Assert.h"
#include <sys/time.h>

void Tools_getMonotonicClock( struct timeval* tv )
{

/* At least FreeBSD 4 doesn't provide monotonic clock support. */
#warning Not sure how to query a monotonically increasing clock on your system. \
Timers will not work correctly if the system clock is adjusted by e.g. ntpd.
    gettimeofday( tv, NULL );
}

void Tools_setMonotonicMarker( timeMarker* pm )
{
    if ( !*pm )
        *pm = malloc( sizeof( struct timeval ) );
    if ( *pm )
        Tools_getMonotonicClock( ( struct timeval* )*pm );
}

uint32_t Tools_uatimeHdiff( timeMarkerConst first, timeMarkerConst second )
{
    struct timeval diff;

    TIME_SUB_TIME( ( const struct timeval* )second, ( const struct timeval* )first, &diff );
    return ( ( uint32_t )diff.tv_sec ) * 100 + diff.tv_usec / 10000;
}

int Tools_readyMonotonic( timeMarkerConst pm, int delta_ms )
{
    struct timeval now, diff, delta;

    Assert_assert( delta_ms >= 0 );
    if ( pm ) {
        Tools_getMonotonicClock( &now );
        TIME_SUB_TIME( &now, ( const struct timeval* )pm, &diff );
        delta.tv_sec = delta_ms / 1000;
        delta.tv_usec = ( delta_ms % 1000 ) * 1000UL;
        return timercmp( &diff, &delta, >= ) ? TRUE : FALSE;
    } else {
        return FALSE;
    }
}
