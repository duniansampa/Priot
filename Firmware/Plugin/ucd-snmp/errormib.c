#include "errormib.h"
#include "AgentRegistry.h"
#include "Impl.h"
#include "System/Util/Logger.h"
#include "System/String.h"
#include "VarStruct.h"
#include "struct.h"
#include "utilities/header_generic.h"

static time_t errorstatustime = 0;
static int errorstatusprior = 0;
static char errorstring[ STRMAX ];

void setPerrorstatus( const char* to )
{
    char buf[ STRMAX ];

    snprintf( buf, sizeof( buf ), "%s:  %s", to, strerror( errno ) );
    buf[ sizeof( buf ) - 1 ] = 0;
    Logger_logPerror( to );
    seterrorstatus( buf, 5 );
}

void seterrorstatus( const char* to, int prior )
{
    if ( errorstatusprior <= prior || ( NETSNMP_ERRORTIMELENGTH < ( time( NULL ) - errorstatustime ) ) ) {
        String_copyTruncate( errorstring, to, sizeof( errorstring ) );
        errorstatusprior = prior;
        errorstatustime = time( NULL );
    }
}

void init_errormib( void )
{

    /*
     * define the structure we're going to ask the agent to register our
     * information at 
     */
    struct Variable2_s extensible_error_variables[] = {
        { MIBINDEX, asnINTEGER, IMPL_OLDAPI_RONLY,
            var_extensible_errors, 1, { MIBINDEX } },
        { ERRORNAME, asnOCTET_STR, IMPL_OLDAPI_RONLY,
            var_extensible_errors, 1, { ERRORNAME } },
        { ERRORFLAG, asnINTEGER, IMPL_OLDAPI_RONLY,
            var_extensible_errors, 1, { ERRORFLAG } },
        { ERRORMSG, asnOCTET_STR, IMPL_OLDAPI_RONLY,
            var_extensible_errors, 1, { ERRORMSG } }
    };

    /*
     * Define the OID pointer to the top of the mib tree that we're
     * registering underneath 
     */
    oid extensible_error_variables_oid[] = { NETSNMP_UCDAVIS_MIB, NETSNMP_ERRORMIBNUM };

    /*
     * register ourselves with the agent to handle our mib tree 
     */
    REGISTER_MIB( "ucd-snmp/errormib", extensible_error_variables,
        Variable2_s, extensible_error_variables_oid );
}

/*
 * var_extensible_errors(...
 * Arguments:
 * vp     IN      - pointer to variable entry that points here
 * name    IN/OUT  - IN/name requested, OUT/name found
 * length  IN/OUT  - length of IN/OUT oid's 
 * exact   IN      - TRUE if an exact match was requested
 * var_len OUT     - length of variable or 0 if function returned
 * write_method
 * 
 */
u_char*
var_extensible_errors( struct Variable_s* vp,
    oid* name,
    size_t* length,
    int exact,
    size_t* var_len, WriteMethodFT** write_method )
{

    static long long_ret;
    static char errmsg[ 300 ];

    if ( header_generic( vp, name, length, exact, var_len, write_method ) )
        return ( NULL );

    errmsg[ 0 ] = 0;

    switch ( vp->magic ) {
    case MIBINDEX:
        long_ret = name[ *length - 1 ];
        return ( ( u_char* )( &long_ret ) );
    case ERRORNAME:
        strcpy( errmsg, "priot" );
        *var_len = strlen( errmsg );
        return ( ( u_char* )errmsg );
    case ERRORFLAG:
        long_ret = ( NETSNMP_ERRORTIMELENGTH >= time( NULL ) - errorstatustime ) ? 1 : 0;
        return ( ( u_char* )( &long_ret ) );
    case ERRORMSG:
        if ( ( NETSNMP_ERRORTIMELENGTH >= time( NULL ) - errorstatustime ) ? 1 : 0 ) {
            String_copyTruncate( errmsg, errorstring, sizeof( errmsg ) );
        } else
            errmsg[ 0 ] = 0;
        *var_len = strlen( errmsg );
        return ( ( u_char* )errmsg );
    }
    return NULL;
}
