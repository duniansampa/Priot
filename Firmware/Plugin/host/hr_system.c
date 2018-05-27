/* Portions of this file are subject to the following copyright(s).  See
 * the Net-SNMP's COPYING file for more details and other copyrights
 * that may apply:
 */
/*
 * Portions of this file are copyrighted by:
 * Copyright � 2003 Sun Microsystems, Inc. All rights reserved.
 * Use is subject to license terms specified in the COPYING file
 * distributed with the Net-SNMP package.
 */

/*
 *  Host Resources MIB - system group implementation - hr_system.c
 *
 */

#include "hr_system.h"
#include "AgentRegistry.h"
#include "System/Util/Trace.h"
#include "Impl.h"
#include "System/Util/Logger.h"
#include "System.h"
#include "TextualConvention.h"
#include "host_res.h"
#include <utmpx.h>

/*
 * If this file doesn't exist, then there is no hard limit on the number
 * of processes, so return 0 for hrSystemMaxProcesses.  
 */
#define NR_TASKS 0

#define UTMP_FILE _PATH_UTMP

/*********************
	 *
	 *  Kernel & interface information,
	 *   and internal forward declarations
	 *
	 *********************/

static int get_load_dev( void );
static int count_users( void );
extern int count_processes( void );
extern int swrun_count_processes( void );

/*********************
	 *
	 *  Initialisation & common implementation functions
	 *
	 *********************/

#define HRSYS_UPTIME 1
#define HRSYS_DATE 2
#define HRSYS_LOAD_DEV 3
#define HRSYS_LOAD_PARAM 4
#define HRSYS_USERS 5
#define HRSYS_PROCS 6
#define HRSYS_MAXPROCS 7

struct Variable2_s hrsystem_variables[] = {
    { HRSYS_UPTIME, ASN01_TIMETICKS, IMPL_OLDAPI_RONLY,
        var_hrsys, 1, { 1 } },
    { HRSYS_DATE, ASN01_OCTET_STR, IMPL_OLDAPI_RONLY,
        var_hrsys, 1, { 2 } },
    { HRSYS_LOAD_DEV, ASN01_INTEGER, IMPL_OLDAPI_RONLY,
        var_hrsys, 1, { 3 } },
    { HRSYS_LOAD_PARAM, ASN01_OCTET_STR, IMPL_OLDAPI_RONLY,
        var_hrsys, 1, { 4 } },
    { HRSYS_USERS, ASN01_GAUGE, IMPL_OLDAPI_RONLY,
        var_hrsys, 1, { 5 } },
    { HRSYS_PROCS, ASN01_GAUGE, IMPL_OLDAPI_RONLY,
        var_hrsys, 1, { 6 } },
    { HRSYS_MAXPROCS, ASN01_INTEGER, IMPL_OLDAPI_RONLY,
        var_hrsys, 1, { 7 } }
};

oid hrsystem_variables_oid[] = { 1, 3, 6, 1, 2, 1, 25, 1 };

void init_hr_system( void )
{

    REGISTER_MIB( "host/hr_system", hrsystem_variables, Variable2_s,
        hrsystem_variables_oid );
} /* end init_hr_system */

/*
 * header_hrsys(...
 * Arguments:
 * vp     IN      - pointer to variable entry that points here
 * name    IN/OUT  - IN/name requested, OUT/name found
 * length  IN/OUT  - length of IN/OUT oid's 
 * exact   IN      - TRUE if an exact match was requested
 * var_len OUT     - length of variable or 0 if function returned
 * write_method
 */

int header_hrsys( struct Variable_s* vp,
    oid* name,
    size_t* length,
    int exact, size_t* var_len, WriteMethodFT** write_method )
{
#define HRSYS_NAME_LENGTH 9
    oid newname[ ASN01_MAX_OID_LEN ];
    int result;

    DEBUG_MSGTL( ( "host/hr_system", "var_hrsys: " ) );
    DEBUG_MSGOID( ( "host/hr_system", name, *length ) );
    DEBUG_MSG( ( "host/hr_system", " %d\n", exact ) );

    memcpy( ( char* )newname, ( char* )vp->name, vp->namelen * sizeof( oid ) );
    newname[ HRSYS_NAME_LENGTH ] = 0;
    result = Api_oidCompare( name, *length, newname, vp->namelen + 1 );
    if ( ( exact && ( result != 0 ) ) || ( !exact && ( result >= 0 ) ) )
        return ( MATCH_FAILED );
    memcpy( ( char* )name, ( char* )newname,
        ( vp->namelen + 1 ) * sizeof( oid ) );
    *length = vp->namelen + 1;

    *write_method = ( WriteMethodFT* )0;
    *var_len = sizeof( long ); /* default to 'long' results */
    return ( MATCH_SUCCEEDED );
} /* end header_hrsys */

/*********************
	 *
	 *  System specific implementation functions
	 *
	 *********************/

u_char*
var_hrsys( struct Variable_s* vp,
    oid* name,
    size_t* length,
    int exact, size_t* var_len, WriteMethodFT** write_method )
{
    static char string[ 129 ]; /* per MIB, max size is 128 */

    time_t now;
    FILE* fp;

    if ( header_hrsys( vp, name, length, exact, var_len, write_method ) == MATCH_FAILED )
        return NULL;

    switch ( vp->magic ) {
    case HRSYS_UPTIME:
        vars_longReturn = System_getUptime();
        return ( u_char* )&vars_longReturn;
    case HRSYS_DATE:
        *write_method = ns_set_time;
        time( &now );
        return ( u_char* )Tc_dateNTime( &now, var_len );
    case HRSYS_LOAD_DEV:
        vars_longReturn = get_load_dev();
        return ( u_char* )&vars_longReturn;
    case HRSYS_LOAD_PARAM:
        if ( ( fp = fopen( "/proc/cmdline", "r" ) ) != NULL ) {
            fgets( string, sizeof( string ), fp );
            fclose( fp );
        } else {
            return NULL;
        }

        *var_len = strlen( string );
        return ( u_char* )string;
    case HRSYS_USERS:
        vars_longReturn = count_users();
        return ( u_char* )&vars_longReturn;
    case HRSYS_PROCS:
        vars_longReturn = swrun_count_processes();

        return ( u_char* )&vars_longReturn;
    case HRSYS_MAXPROCS:
        vars_longReturn = NR_TASKS; /* <linux/tasks.h> */

        return ( u_char* )&vars_longReturn;
    default:
        DEBUG_MSGTL( ( "snmpd", "unknown sub-id %d in var_hrsys\n",
            vp->magic ) );
    }
    return NULL;
} /* end var_hrsys */

/*********************
	 *
	 *  Internal implementation functions
	 *
	 *********************/

int ns_set_time( int action,
    u_char* var_val,
    u_char var_val_type,
    size_t var_val_len,
    u_char* statP, oid* name, size_t name_len )
{

    static time_t oldtime = 0;

    switch ( action ) {
    case IMPL_RESERVE1:
        /* check type */
        if ( var_val_type != ASN01_OCTET_STR ) {
            Logger_log( LOGGER_PRIORITY_ERR, "write to ns_set_time not ASN01_OCTET_STR\n" );
            return PRIOT_ERR_WRONGTYPE;
        }
        if ( var_val_len != 8 && var_val_len != 11 ) {
            Logger_log( LOGGER_PRIORITY_ERR, "write to ns_set_time not a proper length\n" );
            return PRIOT_ERR_WRONGVALUE;
        }
        break;

    case IMPL_RESERVE2:
        break;

    case IMPL_FREE:
        break;

    case IMPL_ACTION: {
        long status = 0;
        time_t seconds = 0;
        struct tm newtimetm;
        int hours_from_utc = 0;
        int minutes_from_utc = 0;

        if ( var_val_len == 11 ) {
            /* timezone inforamation was present */
            hours_from_utc = ( int )var_val[ 9 ];
            minutes_from_utc = ( int )var_val[ 10 ];
        }

        newtimetm.tm_sec = ( int )var_val[ 6 ];
        ;
        newtimetm.tm_min = ( int )var_val[ 5 ];
        newtimetm.tm_hour = ( int )var_val[ 4 ];

        newtimetm.tm_mon = ( int )var_val[ 2 ] - 1;
        newtimetm.tm_year = 256 * ( int )var_val[ 0 ] + ( int )var_val[ 1 ] - 1900;
        newtimetm.tm_mday = ( int )var_val[ 3 ];

        /* determine if day light savings time in effect DST */
        if ( ( hours_from_utc * 60 * 60 + minutes_from_utc * 60 ) == abs( timezone ) ) {
            newtimetm.tm_isdst = 0;
        } else {
            newtimetm.tm_isdst = 1;
        }

        /* create copy of old value */
        oldtime = time( NULL );

        seconds = mktime( &newtimetm );
        if ( seconds == ( time_t )-1 ) {
            Logger_log( LOGGER_PRIORITY_ERR, "Unable to convert time value\n" );
            return PRIOT_ERR_GENERR;
        }
        status = stime( &seconds );
        if ( status != 0 ) {
            Logger_log( LOGGER_PRIORITY_ERR, "Unable to set time\n" );
            return PRIOT_ERR_GENERR;
        }
        break;
    }
    case IMPL_UNDO: {
        /* revert to old value */
        int status = 0;
        if ( oldtime != 0 ) {
            status = stime( &oldtime );
            oldtime = 0;
            if ( status != 0 ) {
                Logger_log( LOGGER_PRIORITY_ERR, "Unable to set time\n" );
                return PRIOT_ERR_GENERR;
            }
        }
        break;
    }

    case IMPL_COMMIT: {
        oldtime = 0;
        break;
    }
    }
    return PRIOT_ERR_NOERROR;
}

/*
                 *  Return the DeviceIndex corresponding
                 *   to the boot device
                 */
static int
get_load_dev( void )
{
    return ( HRDEV_DISK << HRDEV_TYPE_SHIFT ); /* XXX */
} /* end get_load_dev */

static int
count_users( void )
{
    int total = 0;

#define setutent setutxent
#define pututline pututxline
#define getutent getutxent
#define endutent endutxent
    struct utmpx* utmp_p;

    setutent();
    while ( ( utmp_p = getutent() ) != NULL ) {
        if ( utmp_p->ut_type != USER_PROCESS )
            continue;
        /* This block of code fixes zombie user PIDs in the
               utmp/utmpx file that would otherwise be counted as a
               current user */
        if ( kill( utmp_p->ut_pid, 0 ) == -1 && errno == ESRCH ) {
            utmp_p->ut_type = DEAD_PROCESS;
            pututline( utmp_p );
            continue;
        }
        ++total;
    }
    endutent();

    return total;
}
