#include "pass.h"
#include "AgentReadConfig.h"
#include "AgentRegistry.h"
#include "System/Util/Trace.h"
#include "Impl.h"
#include "System/Util/Logger.h"
#include "ReadConfig.h"
#include "System/String.h"
#include "System/String.h"
#include "VarStruct.h"
#include "pass_common.h"
#include "struct.h"
#include "util_funcs.h"

struct extensible* passthrus = NULL;
int numpassthrus = 0;

/*
 * the relocatable extensible commands variables 
 */
struct Variable2_s extensible_passthru_variables[] = {
    /*
     * bogus entry.  Only some of it is actually used. 
     */
    { MIBINDEX, asnINTEGER, IMPL_OLDAPI_RWRITE,
        var_extensible_pass, 0, { MIBINDEX } },
};

void init_pass( void )
{
    AgentReadConfig_priotdRegisterConfigHandler( "pass", pass_parse_config,
        pass_free_config, "miboid command" );
}

void pass_parse_config( const char* token, char* cptr )
{
    struct extensible **ppass = &passthrus, **etmp, *ptmp;
    char *tcptr, *endopt;
    int i;
    unsigned long priority;

    /*
     * options
     */
    priority = DEFAULT_MIB_PRIORITY;
    while ( *cptr == '-' ) {
        cptr++;
        switch ( *cptr ) {
        case 'p':
            /* change priority level */
            cptr++;
            cptr = ReadConfig_skipWhite( cptr );
            if ( !isdigit( ( unsigned char )( *cptr ) ) ) {
                ReadConfig_configPerror( "priority must be an integer" );
                return;
            }
            priority = strtol( ( const char* )cptr, &endopt, 0 );
            if ( ( priority == LONG_MIN ) || ( priority == LONG_MAX ) ) {
                ReadConfig_configPerror( "priority under/overflow" );
                return;
            }
            cptr = endopt;
            cptr = ReadConfig_skipWhite( cptr );
            break;
        default:
            ReadConfig_configPerror( "unknown option for pass directive" );
            return;
        }
    }

    /*
     * MIB
     */
    if ( *cptr == '.' )
        cptr++;
    if ( !isdigit( ( unsigned char )( *cptr ) ) ) {
        ReadConfig_configPerror( "second token is not a OID" );
        return;
    }
    numpassthrus++;

    while ( *ppass != NULL )
        ppass = &( ( *ppass )->next );
    ( *ppass ) = ( struct extensible* )malloc( sizeof( struct extensible ) );
    if ( *ppass == NULL )
        return;
    ( *ppass )->type = PASSTHRU;
    ( *ppass )->mibpriority = priority;

    ( *ppass )->miblen = parse_miboid( cptr, ( *ppass )->miboid );
    while ( isdigit( ( unsigned char )( *cptr ) ) || *cptr == '.' )
        cptr++;
    /*
     * path
     */
    cptr = ReadConfig_skipWhite( cptr );
    if ( cptr == NULL ) {
        ReadConfig_configPerror( "No command specified on pass line" );
        ( *ppass )->command[ 0 ] = 0;
    } else {
        for ( tcptr = cptr; *tcptr != 0 && *tcptr != '#' && *tcptr != ';';
              tcptr++ )
            ;
        sprintf( ( *ppass )->command, "%.*s", ( int )( tcptr - cptr ), cptr );
    }
    String_copyTruncate( ( *ppass )->name, ( *ppass )->command, sizeof( ( *ppass )->name ) );
    ( *ppass )->next = NULL;

    AgentRegistry_registerMibPriority( "pass",
        ( struct Variable_s* )extensible_passthru_variables,
        sizeof( struct Variable2_s ), 1, ( *ppass )->miboid,
        ( *ppass )->miblen, ( *ppass )->mibpriority );

    /*
     * argggg -- passthrus must be sorted 
     */
    if ( numpassthrus > 1 ) {
        etmp = ( struct extensible** )
            malloc( ( ( sizeof( struct extensible* ) ) * numpassthrus ) );
        if ( etmp == NULL )
            return;

        for ( i = 0, ptmp = ( struct extensible* )passthrus;
              i < numpassthrus && ptmp != NULL; i++, ptmp = ptmp->next )
            etmp[ i ] = ptmp;
        qsort( etmp, numpassthrus, sizeof( struct extensible* ),
            pass_compare );
        passthrus = ( struct extensible* )etmp[ 0 ];
        ptmp = ( struct extensible* )etmp[ 0 ];

        for ( i = 0; i < numpassthrus - 1; i++ ) {
            ptmp->next = etmp[ i + 1 ];
            ptmp = ptmp->next;
        }
        ptmp->next = NULL;
        free( etmp );
    }
}

void pass_free_config( void )
{
    struct extensible *etmp, *etmp2;

    for ( etmp = passthrus; etmp != NULL; ) {
        etmp2 = etmp;
        etmp = etmp->next;
        AgentRegistry_unregisterMibPriority( etmp2->miboid, etmp2->miblen, etmp2->mibpriority );
        free( etmp2 );
    }
    passthrus = NULL;
    numpassthrus = 0;
}

u_char*
var_extensible_pass( struct Variable_s* vp,
    oid* name,
    size_t* length,
    int exact,
    size_t* var_len, WriteMethodFT** write_method )
{
    oid newname[ TYPES_MAX_OID_LEN ];
    int i, rtest, fd, newlen;
    char buf[ UTILITIES_MAX_BUFFER ];
    static char buf2[ UTILITIES_MAX_BUFFER ];
    struct extensible* passthru;
    FILE* file;

    for ( i = 1; i <= numpassthrus; i++ ) {
        passthru = get_exten_instance( passthrus, i );
        rtest = Api_oidtreeCompare( name, *length,
            passthru->miboid, passthru->miblen );
        if ( ( exact && rtest == 0 ) || ( !exact && rtest <= 0 ) ) {
            /*
             * setup args 
             */
            if ( passthru->miblen >= *length || rtest < 0 )
                sprint_mib_oid( buf, passthru->miboid, passthru->miblen );
            else
                sprint_mib_oid( buf, name, *length );
            if ( exact )
                snprintf( passthru->command, sizeof( passthru->command ),
                    "%s -g %s", passthru->name, buf );
            else
                snprintf( passthru->command, sizeof( passthru->command ),
                    "%s -n %s", passthru->name, buf );
            passthru->command[ sizeof( passthru->command ) - 1 ] = 0;
            DEBUG_MSGTL( ( "ucd-snmp/pass", "pass-running:  %s\n",
                passthru->command ) );
            /*
             * valid call.  Exec and get output 
             */
            if ( ( fd = get_exec_output( passthru ) ) != -1 ) {
                file = fdopen( fd, "r" );
                if ( fgets( buf, sizeof( buf ), file ) == NULL ) {
                    fclose( file );
                    wait_on_exec( passthru );
                    if ( exact ) {
                        /*
                         * to enable creation
                         */
                        *write_method = setPass;
                        *var_len = 0;
                        return ( NULL );
                    }
                    continue;
                }
                newlen = parse_miboid( buf, newname );

                /*
                 * its good, so copy onto name/length 
                 */
                memcpy( ( char* )name, ( char* )newname,
                    ( int )newlen * sizeof( oid ) );
                *length = newlen;

                /*
                 * set up return pointer for setable stuff 
                 */
                *write_method = setPass;

                if ( newlen == 0 || fgets( buf, sizeof( buf ), file ) == NULL
                    || fgets( buf2, sizeof( buf2 ), file ) == NULL ) {
                    *var_len = 0;
                    fclose( file );
                    wait_on_exec( passthru );
                    return ( NULL );
                }
                fclose( file );
                wait_on_exec( passthru );

                return netsnmp_internal_pass_parse( buf, buf2, var_len, vp );
            }
            *var_len = 0;
            return ( NULL );
        }
    }
    if ( var_len )
        *var_len = 0;
    *write_method = NULL;
    return ( NULL );
}

int setPass( int action, u_char* var_val, u_char var_val_type,
    size_t var_val_len, u_char* statP, oid* name, size_t name_len )
{
    int i, rtest;
    struct extensible* passthru;
    char buf[ UTILITIES_MAX_BUFFER ], buf2[ UTILITIES_MAX_BUFFER ];

    for ( i = 1; i <= numpassthrus; i++ ) {
        passthru = get_exten_instance( passthrus, i );
        rtest = Api_oidtreeCompare( name, name_len,
            passthru->miboid, passthru->miblen );
        if ( rtest <= 0 ) {
            if ( action != IMPL_ACTION )
                return PRIOT_ERR_NOERROR;
            /*
             * setup args 
             */
            if ( passthru->miblen >= name_len || rtest < 0 )
                sprint_mib_oid( buf, passthru->miboid, passthru->miblen );
            else
                sprint_mib_oid( buf, name, name_len );
            snprintf( passthru->command, sizeof( passthru->command ),
                "%s -s %s ", passthru->name, buf );
            passthru->command[ sizeof( passthru->command ) - 1 ] = 0;
            netsnmp_internal_pass_set_format( buf, var_val, var_val_type, var_val_len );
            String_appendTruncate( passthru->command, buf, sizeof( passthru->command ) );
            DEBUG_MSGTL( ( "ucd-snmp/pass", "pass-running:  %s",
                passthru->command ) );
            exec_command( passthru );
            DEBUG_MSGTL( ( "ucd-snmp/pass", "pass-running returned: %s",
                passthru->output ) );
            return netsnmp_internal_pass_str_to_errno( passthru->output );
        }
    }
    if ( Debug_getDoDebugging() ) {
        sprint_mib_oid( buf2, name, name_len );
        DEBUG_MSGTL( ( "ucd-snmp/pass", "pass-notfound:  %s\n", buf2 ) );
    }
    return PRIOT_ERR_NOSUCHNAME;
}

int pass_compare( const void* a, const void* b )
{
    const struct extensible *const *ap, *const *bp;
    ap = ( const struct extensible* const* )a;
    bp = ( const struct extensible* const* )b;
    return Api_oidCompare( ( *ap )->miboid, ( *ap )->miblen, ( *bp )->miboid,
        ( *bp )->miblen );
}
