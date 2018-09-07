

#include "versioninfo.h"
#include "AgentReadConfig.h"
#include "AgentRegistry.h"
#include "System/Util/DefaultStore.h"
#include "Impl.h"
#include "System/Util/Logger.h"
#include "System/String.h"
#include "VarStruct.h"
#include "System/Version.h"
#include "struct.h"
#include "util_funcs.h" /* clear_cache */
#include "utilities/Restart.h"

void init_versioninfo( void )
{

    /*
     * define the structure we're going to ask the agent to register our
     * information at 
     */
    struct Variable2_s extensible_version_variables[] = {
        { MIBINDEX, asnINTEGER, IMPL_OLDAPI_RONLY,
            var_extensible_version, 1, { MIBINDEX } },
        { VERTAG, asnOCTET_STR, IMPL_OLDAPI_RONLY,
            var_extensible_version, 1, { VERTAG } },
        { VERDATE, asnOCTET_STR, IMPL_OLDAPI_RONLY,
            var_extensible_version, 1, { VERDATE } },
        { VERCDATE, asnOCTET_STR, IMPL_OLDAPI_RONLY,
            var_extensible_version, 1, { VERCDATE } },
        { VERIDENT, asnOCTET_STR, IMPL_OLDAPI_RONLY,
            var_extensible_version, 1, { VERIDENT } },
        { VERCONFIG, asnOCTET_STR, IMPL_OLDAPI_RONLY,
            var_extensible_version, 1, { VERCONFIG } },
        { VERCLEARCACHE, asnINTEGER, IMPL_OLDAPI_RWRITE,
            var_extensible_version, 1, { VERCLEARCACHE } },
        { VERUPDATECONFIG, asnINTEGER, IMPL_OLDAPI_RWRITE,
            var_extensible_version, 1, { VERUPDATECONFIG } },
        { VERRESTARTAGENT, asnINTEGER, IMPL_OLDAPI_RWRITE,
            var_extensible_version, 1, { VERRESTARTAGENT } },
        { VERSAVEPERSISTENT, asnINTEGER, IMPL_OLDAPI_RWRITE,
            var_extensible_version, 1, { VERSAVEPERSISTENT } },
        { VERDEBUGGING, asnINTEGER, IMPL_OLDAPI_RWRITE,
            var_extensible_version, 1, { VERDEBUGGING } }
    };

    /*
     * Define the OID pointer to the top of the mib tree that we're
     * registering underneath 
     */
    oid version_variables_oid[] = { NETSNMP_UCDAVIS_MIB, NETSNMP_VERSIONMIBNUM };

    /*
     * register ourselves with the agent to handle our mib tree 
     */
    REGISTER_MIB( "ucd-snmp/versioninfo", extensible_version_variables,
        Variable2_s, version_variables_oid );
}

u_char*
var_extensible_version( struct Variable_s* vp,
    oid* name,
    size_t* length,
    int exact,
    size_t* var_len, WriteMethodFT** write_method )
{

    static long long_ret;
    static char errmsg[ 300 ];
    char* cptr;
    time_t curtime;
    static char config_opts[] = NETSNMP_CONFIGURE_OPTIONS;

    DEBUG_MSGTL( ( "ucd-snmp/versioninfo", "var_extensible_version: " ) );
    DEBUG_MSGOID( ( "ucd-snmp/versioninfo", name, *length ) );
    DEBUG_MSG( ( "ucd-snmp/versioninfo", " %d\n", exact ) );

    if ( header_generic( vp, name, length, exact, var_len, write_method ) )
        return ( NULL );

    switch ( vp->magic ) {
    case MIBINDEX:
        long_ret = name[ 8 ];
        return ( ( u_char* )( &long_ret ) );
    case VERTAG:
        String_copyTruncate( errmsg, Version_getVersion(), sizeof( errmsg ) );
        *var_len = strlen( errmsg );
        return ( ( u_char* )errmsg );
    case VERDATE:
        String_copyTruncate( errmsg, "$Date$", sizeof( errmsg ) );
        *var_len = strlen( errmsg );
        return ( ( u_char* )errmsg );
    case VERCDATE:
        curtime = time( NULL );
        cptr = ctime( &curtime );
        String_copyTruncate( errmsg, cptr, sizeof( errmsg ) );
        *var_len = strlen( errmsg ) - 1; /* - 1 to strip trailing newline */
        return ( ( u_char* )errmsg );
    case VERIDENT:
        String_copyTruncate( errmsg, "$Id$", sizeof( errmsg ) );
        *var_len = strlen( errmsg );
        return ( ( u_char* )errmsg );
    case VERCONFIG:
        *var_len = strlen( config_opts );
        if ( *var_len > 1024 )
            *var_len = 1024; /* mib imposed restriction */
        return ( u_char* )config_opts;

    case VERCLEARCACHE:
        *write_method = clear_cache;
        long_ret = 0;
        return ( ( u_char* )&long_ret );
    case VERUPDATECONFIG:
        *write_method = update_hook;
        long_ret = 0;
        return ( ( u_char* )&long_ret );
    case VERRESTARTAGENT:
        *write_method = Restart_hook;
        long_ret = 0;
        return ( ( u_char* )&long_ret );
    case VERSAVEPERSISTENT:
        *write_method = save_persistent;
        long_ret = 0;
        return ( ( u_char* )&long_ret );
    case VERDEBUGGING:
        *write_method = debugging_hook;
        long_ret = Debug_getDoDebugging();
        return ( ( u_char* )&long_ret );
    }
    return NULL;
}

int update_hook( int action,
    u_char* var_val,
    u_char var_val_type,
    size_t var_val_len,
    u_char* statP, oid* name, size_t name_len )
{
    long tmp = 0;

    if ( var_val_type != asnINTEGER ) {
        Logger_log( LOGGER_PRIORITY_ERR, "Wrong type != int\n" );
        return PRIOT_ERR_WRONGTYPE;
    }
    tmp = *( ( long* )var_val );
    if ( tmp == 1 && action == IMPL_COMMIT ) {
        AgentReadConfig_updateConfig();
    }
    return PRIOT_ERR_NOERROR;
}

int debugging_hook( int action,
    u_char* var_val,
    u_char var_val_type,
    size_t var_val_len,
    u_char* statP, oid* name, size_t name_len )
{
    long tmp = 0;

    if ( var_val_type != asnINTEGER ) {
        DEBUG_MSGTL( ( "versioninfo", "Wrong type != int\n" ) );
        return PRIOT_ERR_WRONGTYPE;
    }
    tmp = *( ( long* )var_val );
    if ( action == IMPL_COMMIT ) {
        Debug_setDoDebugging( tmp );
    }
    return PRIOT_ERR_NOERROR;
}

int save_persistent( int action,
    u_char* var_val,
    u_char var_val_type,
    size_t var_val_len,
    u_char* statP, oid* name, size_t name_len )
{
    if ( var_val_type != asnINTEGER ) {
        DEBUG_MSGTL( ( "versioninfo", "Wrong type != int\n" ) );
        return PRIOT_ERR_WRONGTYPE;
    }
    if ( action == IMPL_COMMIT ) {
        Api_store( DefaultStore_getString( DsStore_LIBRARY_ID,
            DsStr_APPTYPE ) );
    }
    return PRIOT_ERR_NOERROR;
}
