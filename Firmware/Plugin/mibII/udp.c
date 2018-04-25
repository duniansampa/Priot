/*
 *  UDP MIB group implementation - udp.c
 *
 */

#include "udp.h"
#include "Api.h"
#include "Client.h"
#include "Debug.h"
#include "Enum.h"
#include "Logger.h"
#include "PluginSettings.h"
#include "ScalarGroup.h"
#include "SysORTable.h"
#include "kernel_linux.h"
#include "kernel_mib.h"
#include "siglog/agent/auto_nlist.h"
#include "utilities/MIB_STATS_CACHE_TIMEOUT.h"

#define UDP_STATS_CACHE_TIMEOUT MIB_STATS_CACHE_TIMEOUT

/*********************
	 *
	 *  Kernel & interface information,
	 *   and internal forward declarations
	 *
	 *********************/

/*********************
	 *
	 *  Initialisation & common implementation functions
	 *
	 *********************/

/*
 * Define the OID pointer to the top of the mib tree that we're
 * registering underneath, and the OID for the MIB module 
 */
oid udp_oid[] = { PRIOT_OID_MIB2, 7 };
oid udp_module_oid[] = { PRIOT_OID_MIB2, 50 };

void init_udp( void )
{
    HandlerRegistration* reginfo;
    int rc;

    /*
     * register ourselves with the agent as a group of scalars...
     */
    DEBUG_MSGTL( ( "mibII/udpScalar", "Initialising UDP scalar group\n" ) );
    reginfo = AgentHandler_createHandlerRegistration( "udp", udp_handler,
        udp_oid, ASN01_OID_LENGTH( udp_oid ), HANDLER_CAN_RONLY );
    rc = ScalarGroup_registerScalarGroup( reginfo, UDPINDATAGRAMS, UDPOUTDATAGRAMS );
    if ( rc != ErrorCode_SUCCESS )
        return;

    /*
     * .... with a local cache
     *    (except for HP-UX 11, which extracts objects individually)
     */
    AgentHandler_injectHandler( reginfo,
        CacheHandler_getCacheHandler( UDP_STATS_CACHE_TIMEOUT,
                                    udp_load, udp_free,
                                    udp_oid, ASN01_OID_LENGTH( udp_oid ) ) );

    REGISTER_SYSOR_ENTRY( udp_module_oid,
        "The MIB module for managing UDP implementations" );

    auto_nlist( UDPSTAT_SYMBOL, 0, 0 );
    auto_nlist( UDB_SYMBOL, 0, 0 );
}

/*********************
	 *
	 *  System specific implementation functions
	 *
	 *********************/

#undef UDPSTAT_SYMBOL

struct udp_mib udpstat;

/*********************
	 *
	 *  System independent handler (mostly)
	 *
	 *********************/

int udp_handler( MibHandler* handler,
    HandlerRegistration* reginfo,
    AgentRequestInfo* reqinfo,
    RequestInfo* requests )
{
    RequestInfo* request;
    Types_VariableList* requestvb;
    long ret_value = -1;
    oid subid;
    int type = ASN01_COUNTER;

    /*
     * The cached data should already have been loaded by the
     *    cache handler, higher up the handler chain.
     * But just to be safe, check this and load it manually if necessary
     */

    /*
     * 
     *
     */
    DEBUG_MSGTL( ( "mibII/udpScalar", "Handler - mode %s\n",
        Enum_seFindLabelInSlist( "agentMode", reqinfo->mode ) ) );
    switch ( reqinfo->mode ) {
    case MODE_GET:
        for ( request = requests; request; request = request->next ) {
            requestvb = request->requestvb;
            subid = requestvb->name[ ASN01_OID_LENGTH( udp_oid ) ]; /* XXX */
            DEBUG_MSGTL( ( "mibII/udpScalar", "oid: " ) );
            DEBUG_MSGOID( ( "mibII/udpScalar", requestvb->name,
                requestvb->nameLength ) );
            DEBUG_MSG( ( "mibII/udpScalar", "\n" ) );

            switch ( subid ) {
            case UDPINDATAGRAMS:
                ret_value = udpstat.udpInDatagrams;
                break;
            case UDPNOPORTS:
                ret_value = udpstat.udpNoPorts;
                break;
            case UDPOUTDATAGRAMS:
                ret_value = udpstat.udpOutDatagrams;
                break;
            case UDPINERRORS:
                ret_value = udpstat.udpInErrors;
                break;
            }
            Client_setVarTypedValue( request->requestvb, ( u_char )type,
                ( u_char* )&ret_value, sizeof( ret_value ) );
        }
        break;

    case MODE_GETNEXT:
    case MODE_GETBULK:
    case MODE_SET_RESERVE1:
    case MODE_SET_RESERVE2:
    case MODE_SET_ACTION:
    case MODE_SET_COMMIT:
    case MODE_SET_FREE:
    case MODE_SET_UNDO:
        Logger_log( LOGGER_PRIORITY_WARNING, "mibII/udp: Unsupported mode (%d)\n",
            reqinfo->mode );
        break;
    default:
        Logger_log( LOGGER_PRIORITY_WARNING, "mibII/udp: Unrecognised mode (%d)\n",
            reqinfo->mode );
        break;
    }

    return PRIOT_ERR_NOERROR;
}

/*********************
	 *
	 *  Internal implementation functions
	 *
	 *********************/

int udp_load( Cache* cache, void* vmagic )
{
    long ret_value = -1;

    ret_value = linux_read_udp_stat( &udpstat );

    if ( ret_value < 0 ) {
        DEBUG_MSGTL( ( "mibII/udpScalar", "Failed to load UDP scalar Group (linux)\n" ) );
    } else {
        DEBUG_MSGTL( ( "mibII/udpScalar", "Loaded UDP scalar Group (linux)\n" ) );
    }
    return ret_value;
}

void udp_free( Cache* cache, void* magic )
{

    memset( &udpstat, 0, sizeof( udpstat ) );
}
