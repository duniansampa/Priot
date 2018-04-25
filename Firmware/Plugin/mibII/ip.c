/*
 *  IP MIB group implementation - ip.c
 *
 */

#include "ip.h"
#include "AgentRegistry.h"
#include "Api.h"
#include "Client.h"
#include "Debug.h"
#include "Enum.h"
#include "Impl.h"
#include "Logger.h"
#include "PluginSettings.h"
#include "ScalarGroup.h"
#include "SysORTable.h"
#include "VarStruct.h"
#include "at.h"
#include "ipAddr.h"
#include "kernel_linux.h"
#include "kernel_mib.h"
#include "siglog/agent/auto_nlist.h"
#include "siglog/system/generic.h"
#include "utilities/MIB_STATS_CACHE_TIMEOUT.h"
#include "var_route.h"

#define IP_STATS_CACHE_TIMEOUT MIB_STATS_CACHE_TIMEOUT

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

extern void init_routes( void );

/*
 * define the structure we're going to ask the agent to register our
 * information at 
 */
struct Variable1_s ipaddr_variables[] = {
    { IPADADDR, ASN01_IPADDRESS, IMPL_OLDAPI_RONLY,
        var_ipAddrEntry, 1, { 1 } },
    { IPADIFINDEX, ASN01_INTEGER, IMPL_OLDAPI_RONLY,
        var_ipAddrEntry, 1, { 2 } },
    { IPADNETMASK, ASN01_IPADDRESS, IMPL_OLDAPI_RONLY,
        var_ipAddrEntry, 1, { 3 } },
    { IPADBCASTADDR, ASN01_INTEGER, IMPL_OLDAPI_RONLY,
        var_ipAddrEntry, 1, { 4 } },
    { IPADREASMMAX, ASN01_INTEGER, IMPL_OLDAPI_RONLY,
        var_ipAddrEntry, 1, { 5 } }
};

struct Variable1_s iproute_variables[] = {
    { IPROUTEDEST, ASN01_IPADDRESS, IMPL_OLDAPI_RWRITE,
        var_ipRouteEntry, 1, { 1 } },
    { IPROUTEIFINDEX, ASN01_INTEGER, IMPL_OLDAPI_RWRITE,
        var_ipRouteEntry, 1, { 2 } },
    { IPROUTEMETRIC1, ASN01_INTEGER, IMPL_OLDAPI_RWRITE,
        var_ipRouteEntry, 1, { 3 } },
    { IPROUTEMETRIC2, ASN01_INTEGER, IMPL_OLDAPI_RWRITE,
        var_ipRouteEntry, 1, { 4 } },
    { IPROUTEMETRIC3, ASN01_INTEGER, IMPL_OLDAPI_RWRITE,
        var_ipRouteEntry, 1, { 5 } },
    { IPROUTEMETRIC4, ASN01_INTEGER, IMPL_OLDAPI_RWRITE,
        var_ipRouteEntry, 1, { 6 } },
    { IPROUTENEXTHOP, ASN01_IPADDRESS, IMPL_OLDAPI_RWRITE,
        var_ipRouteEntry, 1, { 7 } },
    { IPROUTETYPE, ASN01_INTEGER, IMPL_OLDAPI_RWRITE,
        var_ipRouteEntry, 1, { 8 } },
    { IPROUTEPROTO, ASN01_INTEGER, IMPL_OLDAPI_RONLY,
        var_ipRouteEntry, 1, { 9 } },
    { IPROUTEAGE, ASN01_INTEGER, IMPL_OLDAPI_RWRITE,
        var_ipRouteEntry, 1, { 10 } },
    { IPROUTEMASK, ASN01_IPADDRESS, IMPL_OLDAPI_RWRITE,
        var_ipRouteEntry, 1, { 11 } },
    { IPROUTEMETRIC5, ASN01_INTEGER, IMPL_OLDAPI_RWRITE,
        var_ipRouteEntry, 1, { 12 } },
    { IPROUTEINFO, ASN01_OBJECT_ID, IMPL_OLDAPI_RONLY,
        var_ipRouteEntry, 1, { 13 } }
};

struct Variable1_s ipmedia_variables[] = {

    { IPMEDIAIFINDEX, ASN01_INTEGER, IMPL_OLDAPI_RONLY,
        var_atEntry, 1, { 1 } },
    { IPMEDIAPHYSADDRESS, ASN01_OCTET_STR, IMPL_OLDAPI_RONLY,
        var_atEntry, 1, { 2 } },
    { IPMEDIANETADDRESS, ASN01_IPADDRESS, IMPL_OLDAPI_RONLY,
        var_atEntry, 1, { 3 } },
    { IPMEDIATYPE, ASN01_INTEGER, IMPL_OLDAPI_RONLY,
        var_atEntry, 1, { 4 } }
};

/*
 * Define the OID pointer to the top of the mib tree that we're
 * registering underneath, and the OID of the MIB module 
 */
oid ip_oid[] = { PRIOT_OID_MIB2, 4 };

oid ipaddr_variables_oid[] = { PRIOT_OID_MIB2, 4, 20, 1 };
oid iproute_variables_oid[] = { PRIOT_OID_MIB2, 4, 21, 1 };
oid ipmedia_variables_oid[] = { PRIOT_OID_MIB2, 4, 22, 1 };
oid ip_module_oid[] = { PRIOT_OID_MIB2, 4 };
int ip_module_oid_len = sizeof( ip_module_oid ) / sizeof( oid );
int ip_module_count = 0; /* Need to liaise with icmp.c */

void init_ip( void )
{
    HandlerRegistration* reginfo;
    int rc;

    /*
     * register ourselves with the agent as a group of scalars...
     */
    DEBUG_MSGTL( ( "mibII/ip", "Initialising IP group\n" ) );
    reginfo = AgentHandler_createHandlerRegistration( "ip", ip_handler,
        ip_oid, ASN01_OID_LENGTH( ip_oid ), HANDLER_CAN_RONLY );
    rc = ScalarGroup_registerScalarGroup( reginfo, IPFORWARDING, IPROUTEDISCARDS );
    if ( rc != ErrorCode_SUCCESS )
        return;

    /*
     * .... with a local cache
     *    (except for HP-UX 11, which extracts objects individually)
     */
    AgentHandler_injectHandler( reginfo,
        CacheHandler_getCacheHandler( IP_STATS_CACHE_TIMEOUT,
                                    ip_load, ip_free,
                                    ip_oid, ASN01_OID_LENGTH( ip_oid ) ) );

    /*
     * register (using the old-style API) to handle the IP tables
     */
    REGISTER_MIB( "mibII/ipaddr", ipaddr_variables,
        Variable1_s, ipaddr_variables_oid );
    REGISTER_MIB( "mibII/iproute", iproute_variables,
        Variable1_s, iproute_variables_oid );
    REGISTER_MIB( "mibII/ipmedia", ipmedia_variables,
        Variable1_s, ipmedia_variables_oid );
    if ( ++ip_module_count == 2 )
        REGISTER_SYSOR_ENTRY( ip_module_oid,
            "The MIB module for managing IP and ICMP implementations" );

    /*
     * for speed optimization, we call this now to do the lookup 
     */

    auto_nlist( IPSTAT_SYMBOL, 0, 0 );
    auto_nlist( IP_FORWARDING_SYMBOL, 0, 0 );
    auto_nlist( TCP_TTL_SYMBOL, 0, 0 );
}

/*********************
	 *
	 *  System specific data formats
	 *
	 *********************/

//#define IP_STAT_STRUCTURE struct ip_mib
//#define USES_SNMP_DESIGNED_IPSTAT

struct ip_mib ipstat;

/*********************
	 *
	 *  System independent handler
	 *      (mostly)
	 *
	 *********************/

int ip_handler( MibHandler* handler,
    HandlerRegistration* reginfo,
    AgentRequestInfo* reqinfo,
    RequestInfo* requests )
{
    RequestInfo* request;
    Types_VariableList* requestvb;
    long ret_value;
    oid subid;
    int type = ASN01_COUNTER;

    /*
     * The cached data should already have been loaded by the
     *    cache handler, higher up the handler chain.
     */

    /*
     * 
     *
     */
    DEBUG_MSGTL( ( "mibII/ip", "Handler - mode %s\n",
        Enum_seFindLabelInSlist( "agentMode", reqinfo->mode ) ) );
    switch ( reqinfo->mode ) {
    case MODE_GET:
        for ( request = requests; request; request = request->next ) {
            requestvb = request->requestvb;
            subid = requestvb->name[ ASN01_OID_LENGTH( ip_oid ) ]; /* XXX */
            DEBUG_MSGTL( ( "mibII/ip", "oid: " ) );
            DEBUG_MSGOID( ( "mibII/ip", requestvb->name,
                requestvb->nameLength ) );
            DEBUG_MSG( ( "mibII/ip", "\n" ) );

            switch ( subid ) {
            case IPFORWARDING:
                ret_value = ipstat.ipForwarding;
                type = ASN01_INTEGER;
                break;
            case IPDEFAULTTTL:
                ret_value = ipstat.ipDefaultTTL;
                type = ASN01_INTEGER;
                break;
            case IPINRECEIVES:
                ret_value = ipstat.ipInReceives & 0xffffffff;
                break;
            case IPINHDRERRORS:
                ret_value = ipstat.ipInHdrErrors;
                break;
            case IPINADDRERRORS:
                ret_value = ipstat.ipInAddrErrors;
                break;
            case IPFORWDATAGRAMS:
                ret_value = ipstat.ipForwDatagrams;
                break;
            case IPINUNKNOWNPROTOS:
                ret_value = ipstat.ipInUnknownProtos;
                break;
            case IPINDISCARDS:
                ret_value = ipstat.ipInDiscards;
                break;
            case IPINDELIVERS:
                ret_value = ipstat.ipInDelivers & 0xffffffff;
                break;
            case IPOUTREQUESTS:
                ret_value = ipstat.ipOutRequests & 0xffffffff;
                break;
            case IPOUTDISCARDS:
                ret_value = ipstat.ipOutDiscards;
                break;
            case IPOUTNOROUTES:
                ret_value = ipstat.ipOutNoRoutes;
                break;
            case IPREASMTIMEOUT:
                ret_value = ipstat.ipReasmTimeout;
                type = ASN01_INTEGER;
                break;
            case IPREASMREQDS:
                ret_value = ipstat.ipReasmReqds;
                break;
            case IPREASMOKS:
                ret_value = ipstat.ipReasmOKs;
                break;
            case IPREASMFAILS:
                ret_value = ipstat.ipReasmFails;
                break;
            case IPFRAGOKS:
                ret_value = ipstat.ipFragOKs;
                break;
            case IPFRAGFAILS:
                ret_value = ipstat.ipFragFails;
                break;
            case IPFRAGCREATES:
                ret_value = ipstat.ipFragCreates;
                break;
            case IPROUTEDISCARDS:
                ret_value = ipstat.ipRoutingDiscards;
                break;

            case IPADDRTABLE:
            case IPROUTETABLE:
            case IPMEDIATABLE:
                /*
	 * These are not actually valid scalar objects.
	 * The relevant table registrations should take precedence,
	 *   so skip these three subtrees, regardless of architecture.
	 */
                Agent_setRequestError( reqinfo, request, PRIOT_NOSUCHOBJECT );
                continue;
            }
            Client_setVarTypedValue( request->requestvb, ( u_char )type,
                ( u_char* )&ret_value, sizeof( ret_value ) );
        }
        break;

    case MODE_GETNEXT:
    case MODE_GETBULK:
    case MODE_SET_RESERVE1:
    /* XXX - Windows currently supports setting this */
    case MODE_SET_RESERVE2:
    case MODE_SET_ACTION:
    case MODE_SET_COMMIT:
    case MODE_SET_FREE:
    case MODE_SET_UNDO:
        Logger_log( LOGGER_PRIORITY_WARNING, "mibII/ip: Unsupported mode (%d)\n",
            reqinfo->mode );
        break;
    default:
        Logger_log( LOGGER_PRIORITY_WARNING, "mibII/ip: Unrecognised mode (%d)\n",
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

int ip_load( Cache* cache, void* vmagic )
{
    long ret_value = -1;

    ret_value = linux_read_ip_stat( &ipstat );

    if ( ret_value < 0 ) {
        DEBUG_MSGTL( ( "mibII/ip", "Failed to load IP Group (linux)\n" ) );
    } else {
        DEBUG_MSGTL( ( "mibII/ip", "Loaded IP Group (linux)\n" ) );
    }
    return ret_value;
}

void ip_free( Cache* cache, void* magic )
{
    memset( &ipstat, 0, sizeof( ipstat ) );
}
