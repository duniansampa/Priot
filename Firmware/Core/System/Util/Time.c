#include "Time.h"
#include "System/Util/Assert.h"
#include <sys/time.h>

void Time_getMonotonicClock( struct timeval* tv )
{
    gettimeofday( tv, NULL );
}

void Time_setMonotonicMarker( timeMarker* pm )
{
    if ( !*pm )
        *pm = malloc( sizeof( struct timeval ) );
    if ( *pm )
        Time_getMonotonicClock( ( struct timeval* )*pm );
}

uint32_t Time_uatimeHdiff( timeMarkerConst first, timeMarkerConst second )
{
    struct timeval diff;

    TIME_SUB_TIME( ( const struct timeval* )second, ( const struct timeval* )first, &diff );
    return ( ( uint32_t )diff.tv_sec ) * 100 + diff.tv_usec / 10000;
}

int Time_readyMonotonic( timeMarkerConst pm, int deltaMs )
{
    struct timeval now, diff, delta;

    Assert_assert( deltaMs >= 0 );
    if ( pm ) {
        Time_getMonotonicClock( &now );
        TIME_SUB_TIME( &now, ( const struct timeval* )pm, &diff );
        delta.tv_sec = deltaMs / 1000;
        delta.tv_usec = ( deltaMs % 1000 ) * 1000UL;
        return timercmp( &diff, &delta, >= ) ? TRUE : FALSE;
    } else {
        return FALSE;
    }
}
