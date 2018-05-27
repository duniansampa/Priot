#include "loadave.h"
#include "AgentReadConfig.h"
#include "AgentRegistry.h"
#include "Impl.h"
#include "ReadConfig.h"
#include "System/Util/Trace.h"
#include "System/Util/Logger.h"
#include "VarStruct.h"
#include "utilities/header_simple_table.h"

static double maxload[ 3 ];
static int laConfigSet = 0;

static int
loadave_store_config( int a, int b, void* c, void* d )
{
    char line[ UTILITIES_MAX_BUFFER_SMALL ];
    if ( laConfigSet > 0 ) {
        snprintf( line, UTILITIES_MAX_BUFFER_SMALL, "pload %.02f %.02f %.02f", maxload[ 0 ], maxload[ 1 ], maxload[ 2 ] );
        AgentReadConfig_priotdStoreConfig( line );
    }
    return ErrorCode_SUCCESS;
}

void init_loadave( void )
{

    /*
     * define the structure we're going to ask the agent to register our
     * information at 
     */
    struct Variable2_s extensible_loadave_variables[] = {
        { MIBINDEX, ASN01_INTEGER, IMPL_OLDAPI_RONLY,
            var_extensible_loadave, 1, { MIBINDEX } },
        { ERRORNAME, ASN01_OCTET_STR, IMPL_OLDAPI_RONLY,
            var_extensible_loadave, 1, { ERRORNAME } },
        { LOADAVE, ASN01_OCTET_STR, IMPL_OLDAPI_RONLY,
            var_extensible_loadave, 1, { LOADAVE } },
        { LOADMAXVAL, ASN01_OCTET_STR, IMPL_OLDAPI_RWRITE,
            var_extensible_loadave, 1, { LOADMAXVAL } },
        { LOADAVEINT, ASN01_INTEGER, IMPL_OLDAPI_RONLY,
            var_extensible_loadave, 1, { LOADAVEINT } },
        { LOADAVEFLOAT, ASN01_OPAQUE_FLOAT, IMPL_OLDAPI_RONLY,
            var_extensible_loadave, 1, { LOADAVEFLOAT } },
        { ERRORFLAG, ASN01_INTEGER, IMPL_OLDAPI_RONLY,
            var_extensible_loadave, 1, { ERRORFLAG } },
        { ERRORMSG, ASN01_OCTET_STR, IMPL_OLDAPI_RONLY,
            var_extensible_loadave, 1, { ERRORMSG } }
    };

    /*
     * Define the OID pointer to the top of the mib tree that we're
     * registering underneath 
     */
    oid loadave_variables_oid[] = { NETSNMP_UCDAVIS_MIB, NETSNMP_LOADAVEMIBNUM, 1 };

    /*
     * register ourselves with the agent to handle our mib tree 
     */
    REGISTER_MIB( "ucd-snmp/loadave", extensible_loadave_variables,
        Variable2_s, loadave_variables_oid );

    laConfigSet = 0;

    AgentReadConfig_priotdRegisterConfigHandler( "load", loadave_parse_config,
        loadave_free_config,
        "max1 [max5] [max15]" );

    AgentReadConfig_priotdRegisterConfigHandler( "pload",
        loadave_parse_config, NULL, NULL );

    /*
     * we need to be called back later
     */
    Callback_register( CallbackMajor_LIBRARY, CallbackMinor_STORE_DATA,
        loadave_store_config, NULL );
}

void loadave_parse_config( const char* token, char* cptr )
{
    int i;

    if ( strcmp( token, "pload" ) == 0 ) {
        if ( laConfigSet < 0 ) {
            Logger_log( LOGGER_PRIORITY_WARNING,
                "ignoring attempted override of read-only load\n" );
            return;
        } else {
            laConfigSet++;
        }
    } else {
        if ( laConfigSet > 0 ) {
            Logger_log( LOGGER_PRIORITY_WARNING,
                "ignoring attempted override of read-only load\n" );
            /*
             * Fall through and copy in this value.
             */
        }
        laConfigSet = -1;
    }

    for ( i = 0; i <= 2; i++ ) {
        if ( cptr != NULL )
            maxload[ i ] = atof( cptr );
        else
            maxload[ i ] = maxload[ i - 1 ];
        cptr = ReadConfig_skipNotWhite( cptr );
        cptr = ReadConfig_skipWhite( cptr );
    }
}

void loadave_free_config( void )
{
    int i;

    for ( i = 0; i <= 2; i++ )
        maxload[ i ] = NETSNMP_DEFMAXLOADAVE;
}

/*
 * try to get load average
 * Inputs: pointer to array of doubles, number of elements in array
 * Returns: 0=array has values, -1=error occurred.
 */
int try_getloadavg( double* r_ave, size_t s_ave )
{

    if ( getloadavg( r_ave, s_ave ) == -1 )
        return ( -1 );
    /*
     * XXX
     *   To calculate this, we need to compare
     *   successive values of the kernel array
     *   '_cp_times', and calculate the resulting
     *   percentage changes.
     *     This calculation needs to be performed
     *   regularly - perhaps as a background process.
     *
     *   See the source to 'top' for full details.
     *
     * The linux SNMP HostRes implementation
     *   uses 'avenrun[0]*100' as an approximation.
     *   This is less than accurate, but has the
     *   advantage of being simple to implement!
     *
     * I'm also assuming a single processor
     */
    return 0;
}

static int
write_laConfig( int action,
    u_char* var_val,
    u_char var_val_type,
    size_t var_val_len,
    u_char* statP, oid* name, size_t name_len )
{
    static double laConfig = 0;

    switch ( action ) {
    case IMPL_RESERVE1: /* Check values for acceptability */
        if ( var_val_type != ASN01_OCTET_STR ) {
            DEBUG_MSGTL( ( "ucd-snmp/loadave",
                "write to laConfig not ASN01_OCTET_STR\n" ) );
            return PRIOT_ERR_WRONGTYPE;
        }
        if ( var_val_len > 8 || var_val_len <= 0 ) {
            DEBUG_MSGTL( ( "ucd-snmp/loadave",
                "write to laConfig: bad length\n" ) );
            return PRIOT_ERR_WRONGLENGTH;
        }

        if ( laConfigSet < 0 ) {
            /*
             * The object is set in a read-only configuration file.
             */
            return PRIOT_ERR_NOTWRITABLE;
        }
        break;

    case IMPL_RESERVE2: /* Allocate memory and similar resources */
    {
        char buf[ 8 ];
        int old_errno = errno;
        double val;
        char* endp;

        sprintf( buf, "%.*s", ( int )var_val_len, ( char* )var_val );
        val = strtod( buf, &endp );

        if ( errno == ERANGE || *endp != '\0' || val < 0 || val > 65536.00 ) {
            errno = old_errno;
            DEBUG_MSGTL( ( "ucd-snmp/loadave",
                "write to laConfig: invalid value\n" ) );
            return PRIOT_ERR_WRONGVALUE;
        }

        errno = old_errno;

        laConfig = val;
    } break;

    case IMPL_COMMIT: {
        int idx = name[ name_len - 1 ] - 1;
        maxload[ idx ] = laConfig;
        laConfigSet = 1;
    }
    }

    return PRIOT_ERR_NOERROR;
}

u_char*
var_extensible_loadave( struct Variable_s* vp,
    oid* name,
    size_t* length,
    int exact,
    size_t* var_len, WriteMethodFT** write_method )
{

    static long long_ret;
    static float float_ret;
    static char errmsg[ 300 ];
    double avenrun[ 3 ];
    if ( header_simple_table( vp, name, length, exact, var_len, write_method, 3 ) )
        return ( NULL );
    switch ( vp->magic ) {
    case MIBINDEX:
        long_ret = name[ *length - 1 ];
        return ( ( u_char* )( &long_ret ) );
    case LOADMAXVAL:
        /* setup write method, but don't return yet */
        *write_method = write_laConfig;
        break;
    case ERRORNAME:
        sprintf( errmsg, "Load-%d", ( ( name[ *length - 1 ] == 1 ) ? 1 : ( ( name[ *length - 1 ] == 2 ) ? 5 : 15 ) ) );
        *var_len = strlen( errmsg );
        return ( ( u_char* )( errmsg ) );
    }
    if ( try_getloadavg( &avenrun[ 0 ], sizeof( avenrun ) / sizeof( avenrun[ 0 ] ) )
        == -1 )
        return NULL;
    switch ( vp->magic ) {
    case LOADAVE:

        sprintf( errmsg, "%.2f", avenrun[ name[ *length - 1 ] - 1 ] );
        *var_len = strlen( errmsg );
        return ( ( u_char* )( errmsg ) );
    case LOADMAXVAL:
        sprintf( errmsg, "%.2f", maxload[ name[ *length - 1 ] - 1 ] );
        *var_len = strlen( errmsg );
        return ( ( u_char* )( errmsg ) );
    case LOADAVEINT:
        long_ret = ( u_long )( avenrun[ name[ *length - 1 ] - 1 ] * 100 );
        return ( ( u_char* )( &long_ret ) );
    case LOADAVEFLOAT:
        float_ret = ( float )avenrun[ name[ *length - 1 ] - 1 ];
        *var_len = sizeof( float_ret );
        return ( ( u_char* )( &float_ret ) );
    case ERRORFLAG:
        long_ret = ( maxload[ name[ *length - 1 ] - 1 ] != 0 && avenrun[ name[ *length - 1 ] - 1 ] >= maxload[ name[ *length - 1 ] - 1 ] ) ? 1 : 0;
        return ( ( u_char* )( &long_ret ) );
    case ERRORMSG:
        if ( maxload[ name[ *length - 1 ] - 1 ] != 0 && avenrun[ name[ *length - 1 ] - 1 ] >= maxload[ name[ *length - 1 ] - 1 ] ) {
            snprintf( errmsg, sizeof( errmsg ),
                "%d min Load Average too high (= %.2f)",
                ( name[ *length - 1 ] == 1 ) ? 1 : ( ( name[ *length - 1 ] == 2 ) ? 5 : 15 ),
                avenrun[ name[ *length - 1 ] - 1 ] );
            errmsg[ sizeof( errmsg ) - 1 ] = '\0';
        } else {
            errmsg[ 0 ] = 0;
        }
        *var_len = strlen( errmsg );
        return ( ( u_char* )errmsg );
    }
    return NULL;
}
