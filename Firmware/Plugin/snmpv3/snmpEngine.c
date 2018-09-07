/*
 * snmpEngine.c: implement's the SNMP-FRAMEWORK-MIB. 
 */

#include "snmpEngine.h"
#include "AgentReadConfig.h"
#include "AgentRegistry.h"
#include "GetStatistic.h"
#include "Impl.h"
#include "SysORTable.h"
#include "V3.h"
#include "VarStruct.h"
#include "utilities/header_generic.h"

struct Variable2_s snmpEngine_variables[] = {
    { SNMPENGINEID, asnOCTET_STR, IMPL_OLDAPI_RONLY,
        var_snmpEngine, 1, { 1 } },
    /* !NETSNMP_ENABLE_TESTING_CODE */
    { SNMPENGINEBOOTS, asnINTEGER, IMPL_OLDAPI_RONLY,
        var_snmpEngine, 1, { 2 } },
    { SNMPENGINETIME, asnINTEGER, IMPL_OLDAPI_RONLY,
        var_snmpEngine, 1, { 3 } },

    { SNMPENGINEMAXMESSAGESIZE, asnINTEGER, IMPL_OLDAPI_RONLY,
        var_snmpEngine, 1, { 4 } },
};

/*
 * now load this mib into the agents mib table 
 */
oid snmpEngine_variables_oid[] = { 1, 3, 6, 1, 6, 3, 10, 2, 1 };

void register_snmpEngine_scalars( void )
{
    REGISTER_MIB( "snmpv3/snmpEngine", snmpEngine_variables, Variable2_s,
        snmpEngine_variables_oid );
}

void register_snmpEngine_scalars_context( const char* contextName )
{
    AgentRegistry_registerMibContext( "snmpv3/snmpEngine",
        ( struct Variable_s* )snmpEngine_variables,
        sizeof( struct Variable2_s ),
        sizeof( snmpEngine_variables ) / sizeof( struct Variable2_s ),
        snmpEngine_variables_oid,
        sizeof( snmpEngine_variables_oid ) / sizeof( oid ),
        DEFAULT_MIB_PRIORITY, 0, 0, NULL,
        contextName, -1, 0 );
}

void init_snmpEngine( void )
{
    oid reg[] = { 1, 3, 6, 1, 6, 3, 10, 3, 1, 1 };
    REGISTER_SYSOR_ENTRY( reg, "The SNMP Management Architecture MIB." );
    register_snmpEngine_scalars();
}

u_char*
var_snmpEngine( struct Variable_s* vp,
    oid* name,
    size_t* length,
    int exact, size_t* var_len, WriteMethodFT** write_method )
{

    /*
     * variables we may use later 
     */
    static long long_ret;
    static unsigned char engineID[ UTILITIES_MAX_BUFFER ];

    *write_method = ( WriteMethodFT* )0; /* assume it isnt writable for the time being */
    *var_len = sizeof( long_ret ); /* assume an integer and change later if not */

    if ( header_generic( vp, name, length, exact, var_len, write_method ) )
        return NULL;

    /*
     * this is where we do the value assignments for the mib results. 
     */
    switch ( vp->magic ) {

    case SNMPENGINEID:
        *var_len = V3_getEngineID( engineID, UTILITIES_MAX_BUFFER );
        /*
         * XXX  Set ERROR_MSG() upon error? 
         */
        return ( unsigned char* )engineID;

    case SNMPENGINEBOOTS:

        long_ret = V3_localEngineBoots();
        return ( unsigned char* )&long_ret;

    case SNMPENGINETIME:

        long_ret = V3_localEngineTime();
        return ( unsigned char* )&long_ret;

    case SNMPENGINEMAXMESSAGESIZE:
        long_ret = 1500;
        return ( unsigned char* )&long_ret;

    default:
        DEBUG_MSGTL( ( "priotd", "unknown sub-id %d in var_snmpEngine\n",
            vp->magic ) );
    }
    return NULL;
}
