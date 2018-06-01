#include "Time.h"
#include "System/Util/Assert.h"
#include <netinet/in.h>
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

uint32_t Time_timeHdiff( timeMarkerConst first, timeMarkerConst second )
{
    struct timeval diff;

    TIME_SUB_TIME( ( const struct timeval* )second, ( const struct timeval* )first, &diff );
    return ( ( uint32_t )diff.tv_sec ) * 100 + diff.tv_usec / 10000;
}

uint Time_diffTime( const struct timeval* end, const struct timeval* beginning )
{
    struct timeval diff;

    TIME_SUB_TIME( end, beginning, &diff );
    return diff.tv_sec + ( diff.tv_usec >= 500000L );
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

int Time_convertVariablesToDateAndTimeString( uint8_t* buffer, size_t* bufferSize,
    uint16_t year, uint8_t month, uint8_t day, uint8_t hour, uint8_t minutes, uint8_t seconds,
    uint8_t deciSeconds, int utcOffsetDirection,
    uint8_t utcOffsetHours, uint8_t utcOffsetMinutes )
{
    uint16_t tmp_year = htons( year );

    /*
     * if we have a utc offset, need 11 bytes. Otherwise we
     * just need 8 bytes.
     */
    if ( utcOffsetDirection ) {
        if ( *bufferSize < 11 )
            return ErrorCode_RANGE;

        /*
         * set utc offset data
         */
        buffer[ 8 ] = ( utcOffsetDirection < 0 ) ? '-' : '+';
        buffer[ 9 ] = utcOffsetHours;
        buffer[ 10 ] = utcOffsetMinutes;
        *bufferSize = 11;
    } else if ( *bufferSize < 8 )
        return ErrorCode_RANGE;
    else
        *bufferSize = 8;

    /*
     * set basic date/time data
     */
    memcpy( buffer, &tmp_year, sizeof( tmp_year ) );
    buffer[ 2 ] = month;
    buffer[ 3 ] = day;
    buffer[ 4 ] = hour;
    buffer[ 5 ] = minutes;
    buffer[ 6 ] = seconds;
    buffer[ 7 ] = deciSeconds;

    return ErrorCode_SUCCESS;
}

u_char* Time_convertDateAndTimeToString( const time_t* when, size_t* length )
{
    struct tm* tm_p;
    static u_char string[ 11 ];
    unsigned short yauron;

    /*
     * Null time
     */
    if ( when == NULL || *when == 0 || *when == ( time_t )-1 ) {
        string[ 0 ] = 0;
        string[ 1 ] = 0;
        string[ 2 ] = 1;
        string[ 3 ] = 1;
        string[ 4 ] = 0;
        string[ 5 ] = 0;
        string[ 6 ] = 0;
        string[ 7 ] = 0;
        *length = 8;
        return string;
    }

    /*
     * Basic 'local' time handling
     */
    tm_p = localtime( when );
    yauron = tm_p->tm_year + 1900;
    string[ 0 ] = ( u_char )( yauron >> 8 );
    string[ 1 ] = ( u_char )yauron;
    string[ 2 ] = tm_p->tm_mon + 1;
    string[ 3 ] = tm_p->tm_mday;
    string[ 4 ] = tm_p->tm_hour;
    string[ 5 ] = tm_p->tm_min;
    string[ 6 ] = tm_p->tm_sec;
    string[ 7 ] = 0;
    *length = 8;

    /*
     * Timezone offset
     */
    {
        const int tzoffset = -tm_p->tm_gmtoff; /* Seconds east of UTC */

        if ( tzoffset > 0 )
            string[ 8 ] = '-';
        else
            string[ 8 ] = '+';
        string[ 9 ] = abs( tzoffset ) / 3600;
        string[ 10 ] = ( abs( tzoffset ) - string[ 9 ] * 3600 ) / 60;
        *length = 11;
    }

    return string;
}

time_t Time_convertCtimeStringToTimet( const char* ctimeString )
{
    struct tm tm;

    if ( strlen( ctimeString ) < 24 )
        return 0;

    /*
     * Month
     */
    if ( !strncmp( ctimeString + 4, "Jan", 3 ) )
        tm.tm_mon = 0;
    else if ( !strncmp( ctimeString + 4, "Feb", 3 ) )
        tm.tm_mon = 1;
    else if ( !strncmp( ctimeString + 4, "Mar", 3 ) )
        tm.tm_mon = 2;
    else if ( !strncmp( ctimeString + 4, "Apr", 3 ) )
        tm.tm_mon = 3;
    else if ( !strncmp( ctimeString + 4, "May", 3 ) )
        tm.tm_mon = 4;
    else if ( !strncmp( ctimeString + 4, "Jun", 3 ) )
        tm.tm_mon = 5;
    else if ( !strncmp( ctimeString + 4, "Jul", 3 ) )
        tm.tm_mon = 6;
    else if ( !strncmp( ctimeString + 4, "Aug", 3 ) )
        tm.tm_mon = 7;
    else if ( !strncmp( ctimeString + 4, "Sep", 3 ) )
        tm.tm_mon = 8;
    else if ( !strncmp( ctimeString + 4, "Oct", 3 ) )
        tm.tm_mon = 9;
    else if ( !strncmp( ctimeString + 4, "Nov", 3 ) )
        tm.tm_mon = 10;
    else if ( !strncmp( ctimeString + 4, "Dec", 3 ) )
        tm.tm_mon = 11;
    else
        return 0;

    tm.tm_mday = atoi( ctimeString + 8 );
    tm.tm_hour = atoi( ctimeString + 11 );
    tm.tm_min = atoi( ctimeString + 14 );
    tm.tm_sec = atoi( ctimeString + 17 );
    tm.tm_year = atoi( ctimeString + 20 ) - 1900;

    /*
     *  Cope with timezone and DST
     */

    if ( daylight )
        tm.tm_isdst = 1;

    tm.tm_sec -= timezone;

    return ( mktime( &tm ) );
}
