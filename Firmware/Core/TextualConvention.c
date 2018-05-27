#include "TextualConvention.h"
#include "Api.h"
#include "Priot.h"
#include <netinet/in.h>

/*
  DateAndTime ::= TEXTUAL-CONVENTION
    DISPLAY-HINT "2d-1d-1d,1d:1d:1d.1d,1a1d:1d"
    STATUS       current
    DESCRIPTION
            "A date-time specification.

            field  octets  contents                  range
            -----  ------  --------                  -----
              1      1-2   year*                     0..65536
              2       3    month                     1..12
              3       4    day                       1..31
              4       5    hour                      0..23
              5       6    minutes                   0..59
              6       7    seconds                   0..60
                           (use 60 for leap-second)
              7       8    deci-seconds              0..9
              8       9    direction from UTC        '+' / '-'
              9      10    hours from UTC*           0..13
             10      11    minutes from UTC          0..59

            * Notes:
            - the value of year is in network-byte order
            - daylight saving time in New Zealand is +13

            For example, Tuesday May 26, 1992 at 1:30:15 PM EDT would be
            displayed as:

                             1992-5-26,13:30:15.0,-4:0

            Note that if only local time is known, then timezone
            information (fields 8-10) is not present."
    SYNTAX       OCTET STRING (SIZE (8 | 11))
*/

int Tc_dateandtimeSetBufFromVars( u_char* buf, size_t* bufsize,
    u_short year, u_char month, u_char day,
    u_char hour, u_char minutes,
    u_char seconds, u_char deci_seconds,
    int utc_offset_direction,
    u_char utc_offset_hours,
    u_char utc_offset_minutes )
{
    u_short tmp_year = htons( year );

    /*
     * if we have a utc offset, need 11 bytes. Otherwise we
     * just need 8 bytes.
     */
    if ( utc_offset_direction ) {
        if ( *bufsize < 11 )
            return ErrorCode_RANGE;

        /*
         * set utc offset data
         */
        buf[ 8 ] = ( utc_offset_direction < 0 ) ? '-' : '+';
        buf[ 9 ] = utc_offset_hours;
        buf[ 10 ] = utc_offset_minutes;
        *bufsize = 11;
    } else if ( *bufsize < 8 )
        return ErrorCode_RANGE;
    else
        *bufsize = 8;

    /*
     * set basic date/time data
     */
    memcpy( buf, &tmp_year, sizeof( tmp_year ) );
    buf[ 2 ] = month;
    buf[ 3 ] = day;
    buf[ 4 ] = hour;
    buf[ 5 ] = minutes;
    buf[ 6 ] = seconds;
    buf[ 7 ] = deci_seconds;

    return ErrorCode_SUCCESS;
}

u_char* Tc_dateNTime( const time_t* when, size_t* length )
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

time_t Tc_ctimeToTimet( const char* str )
{
    struct tm tm;

    if ( strlen( str ) < 24 )
        return 0;

    /*
     * Month
     */
    if ( !strncmp( str + 4, "Jan", 3 ) )
        tm.tm_mon = 0;
    else if ( !strncmp( str + 4, "Feb", 3 ) )
        tm.tm_mon = 1;
    else if ( !strncmp( str + 4, "Mar", 3 ) )
        tm.tm_mon = 2;
    else if ( !strncmp( str + 4, "Apr", 3 ) )
        tm.tm_mon = 3;
    else if ( !strncmp( str + 4, "May", 3 ) )
        tm.tm_mon = 4;
    else if ( !strncmp( str + 4, "Jun", 3 ) )
        tm.tm_mon = 5;
    else if ( !strncmp( str + 4, "Jul", 3 ) )
        tm.tm_mon = 6;
    else if ( !strncmp( str + 4, "Aug", 3 ) )
        tm.tm_mon = 7;
    else if ( !strncmp( str + 4, "Sep", 3 ) )
        tm.tm_mon = 8;
    else if ( !strncmp( str + 4, "Oct", 3 ) )
        tm.tm_mon = 9;
    else if ( !strncmp( str + 4, "Nov", 3 ) )
        tm.tm_mon = 10;
    else if ( !strncmp( str + 4, "Dec", 3 ) )
        tm.tm_mon = 11;
    else
        return 0;

    tm.tm_mday = atoi( str + 8 );
    tm.tm_hour = atoi( str + 11 );
    tm.tm_min = atoi( str + 14 );
    tm.tm_sec = atoi( str + 17 );
    tm.tm_year = atoi( str + 20 ) - 1900;

    /*
     *  Cope with timezone and DST
     */

    if ( daylight )
        tm.tm_isdst = 1;

    tm.tm_sec -= timezone;

    return ( mktime( &tm ) );
}

/*
 * blatantly lifted from opensnmp
 */
char Tc_checkRowstatusTransition( int oldValue, int newValue )
{
    /*
     * From the SNMPv2-TC MIB:
     *                                          STATE
     *               +--------------+-----------+-------------+-------------
     *               |      A       |     B     |      C      |      D
     *               |              |status col.|status column|
     *               |status column |    is     |      is     |status column
     *     ACTION    |does not exist|  notReady | notInService|  is active
     * --------------+--------------+-----------+-------------+-------------
     * set status    |noError    ->D|inconsist- |inconsistent-|inconsistent-
     * column to     |       or     |   entValue|        Value|        Value
     * createAndGo   |inconsistent- |           |             |
     *               |         Value|           |             |
     * --------------+--------------+-----------+-------------+-------------
     * set status    |noError  see 1|inconsist- |inconsistent-|inconsistent-
     * column to     |       or     |   entValue|        Value|        Value
     * createAndWait |wrongValue    |           |             |
     * --------------+--------------+-----------+-------------+-------------
     * set status    |inconsistent- |inconsist- |noError      |noError
     * column to     |         Value|   entValue|             |
     * active        |              |           |             |
     *               |              |     or    |             |
     *               |              |           |             |
     *               |              |see 2   ->D|see 8     ->D|          ->D
     * --------------+--------------+-----------+-------------+-------------
     * set status    |inconsistent- |inconsist- |noError      |noError   ->C
     * column to     |         Value|   entValue|             |
     * notInService  |              |           |             |
     *               |              |     or    |             |      or
     *               |              |           |             |
     *               |              |see 3   ->C|          ->C|see 6
     * --------------+--------------+-----------+-------------+-------------
     * set status    |noError       |noError    |noError      |noError   ->A
     * column to     |              |           |             |      or
     * destroy       |           ->A|        ->A|          ->A|see 7
     * --------------+--------------+-----------+-------------+-------------
     * set any other |see 4         |noError    |noError      |see 5
     * column to some|              |           |             |
     * value         |              |      see 1|          ->C|          ->D
     * --------------+--------------+-----------+-------------+-------------

     *             (1) goto B or C, depending on information available to the
     *             agent.

     *             (2) if other variable bindings included in the same PDU,
     *             provide values for all columns which are missing but
     *             required, and all columns have acceptable values, then
     *             return noError and goto D.

     *             (3) if other variable bindings included in the same PDU,
     *             provide legal values for all columns which are missing but
     *             required, then return noError and goto C.

     *             (4) at the discretion of the agent, the return value may be
     *             either:

     *                  inconsistentName:  because the agent does not choose to
     *                  create such an instance when the corresponding
     *                  RowStatus instance does not exist, or

     *                  inconsistentValue:  if the supplied value is
     *                  inconsistent with the state of some other MIB object's
     *                  value, or

     *                  noError: because the agent chooses to create the
     *                  instance.

     *             If noError is returned, then the instance of the status
     *             column must also be created, and the new state is B or C,
     *             depending on the information available to the agent.  If
     *             inconsistentName or inconsistentValue is returned, the row
     *             remains in state A.

     *             (5) depending on the MIB definition for the column/table,
     *             either noError or inconsistentValue may be returned.

     *             (6) the return value can indicate one of the following
     *             errors:

     *                  wrongValue: because the agent does not support
     *                  notInService (e.g., an agent which does not support
     *                  createAndWait), or

     *                  inconsistentValue: because the agent is unable to take
     *                  the row out of service at this time, perhaps because it
     *                  is in use and cannot be de-activated.

     *             (7) the return value can indicate the following error:

     *                  inconsistentValue: because the agent is unable to
     *                  remove the row at this time, perhaps because it is in
     *                  use and cannot be de-activated.

     *             (8) the transition to D can fail, e.g., if the values of the
     *             conceptual row are inconsistent, then the error code would
     *             be inconsistentValue.

     *             NOTE: Other processing of (this and other varbinds of) the
     *             set request may result in a response other than noError
     *             being returned, e.g., wrongValue, noCreation, etc.
     */

    switch ( newValue ) {
    /*
         * these two end up being equivelent as far as checking the
         * status goes, although the final states are based on the
         * newValue.
         */
    case TC_RS_ACTIVE:
    case TC_RS_NOTINSERVICE:
        if ( oldValue == TC_RS_NOTINSERVICE || oldValue == TC_RS_ACTIVE )
            ;
        else
            return PRIOT_ERR_INCONSISTENTVALUE;
        break;

    case TC_RS_NOTREADY:
        /*
         * Illegal set value.
         */
        return PRIOT_ERR_WRONGVALUE;
        break;

    case TC_RS_CREATEANDGO:
    case TC_RS_CREATEANDWAIT:
        if ( oldValue != TC_RS_NONEXISTENT )
            /*
             * impossible, we already exist.
             */
            return PRIOT_ERR_INCONSISTENTVALUE;
        break;

    case TC_RS_DESTROY:
        break;

    default:
        return PRIOT_ERR_WRONGVALUE;
        break;
    }

    return PRIOT_ERR_NOERROR;
}

char Tc_checkRowstatusWithStoragetypeTransition( int oldValue, int newValue,
    int oldStorage )
{
    /*
     * can not destroy permanent or readonly rows
     */
    if ( ( TC_RS_DESTROY == newValue ) && ( ( PRIOT_STORAGE_PERMANENT == oldStorage ) || ( PRIOT_STORAGE_READONLY == oldStorage ) ) )
        return PRIOT_ERR_WRONGVALUE;

    return Tc_checkRowstatusTransition( oldValue, newValue );
}

char Tc_checkStorageTransition( int oldValue, int newValue )
{
    /*
     * From the SNMPv2-TC MIB:

     *             "Describes the memory realization of a conceptual row.  A
     *             row which is volatile(2) is lost upon reboot.  A row which
     *             is either nonVolatile(3), permanent(4) or readOnly(5), is
     *             backed up by stable storage.  A row which is permanent(4)
     *             can be changed but not deleted.  A row which is readOnly(5)
     *             cannot be changed nor deleted.

     *             If the value of an object with this syntax is either
     *             permanent(4) or readOnly(5), it cannot be written.
     *             Conversely, if the value is either other(1), volatile(2) or
     *             nonVolatile(3), it cannot be modified to be permanent(4) or
     *             readOnly(5).  (All illegal modifications result in a
     *             'wrongValue' error.)

     *             Every usage of this textual convention is required to
     *             specify the columnar objects which a permanent(4) row must
     *             at a minimum allow to be writable."
     */
    switch ( oldValue ) {
    case PRIOT_STORAGE_PERMANENT:
    case PRIOT_STORAGE_READONLY:
        return PRIOT_ERR_INCONSISTENTVALUE;

    case PRIOT_STORAGE_NONE:
    case PRIOT_STORAGE_OTHER:
    case PRIOT_STORAGE_VOLATILE:
    case PRIOT_STORAGE_NONVOLATILE:
        if ( newValue == PRIOT_STORAGE_PERMANENT || newValue == PRIOT_STORAGE_READONLY )
            return PRIOT_ERR_INCONSISTENTVALUE;
    }

    return PRIOT_ERR_NOERROR;
}
