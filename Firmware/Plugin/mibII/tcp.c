
/*
 *  TCP MIB group implementation - tcp.c
 *
 */

#include "tcp.h"
#include "Api.h"
#include "Client.h"
#include "System/Util/Trace.h"
#include "System/Containers/MapList.h"
#include "System/Util/Logger.h"
#include "PluginSettings.h"
#include "ScalarGroup.h"
#include "SysORTable.h"
#include "kernel_linux.h"
#include "kernel_mib.h"
#include "siglog/agent/auto_nlist.h"
#include "tcpTable.h"
#include "utilities/MIB_STATS_CACHE_TIMEOUT.h"
#include <arpa/inet.h>
#include <netinet/tcp.h>
#include <stdlib.h>
#include <unistd.h>

#define TCP_STATS_CACHE_TIMEOUT MIB_STATS_CACHE_TIMEOUT

/*********************
	 *
	 *  Kernel & interface information,
	 *   and internal forward declarations
	 *
	 *********************/

/*
                 * FreeBSD4 *does* need an explicit variable 'hz'
                 *   since this appears in a system header file.
                 * But only define it under FreeBSD, since it
                 *   breaks other systems (notable AIX)
                 */

extern int TCP_Count_Connections( void );
/*********************
	 *
	 *  Initialisation & common implementation functions
	 *
	 *********************/

/*
 * Define the OID pointer to the top of the mib tree that we're
 * registering underneath, and the OID for the MIB module 
 */
oid tcp_oid[] = { PRIOT_OID_MIB2, 6 };
oid tcp_module_oid[] = { PRIOT_OID_MIB2, 49 };

void init_tcp( void )
{
    HandlerRegistration* reginfo;
    int rc;

    /*
     * register ourselves with the agent as a group of scalars...
     */
    DEBUG_MSGTL( ( "mibII/tcpScalar", "Initialising TCP scalar group\n" ) );
    reginfo = AgentHandler_createHandlerRegistration( "tcp", tcp_handler,
        tcp_oid, ASN01_OID_LENGTH( tcp_oid ), HANDLER_CAN_RONLY );
    rc = ScalarGroup_registerScalarGroup( reginfo, TCPRTOALGORITHM, TCPOUTRSTS );
    if ( rc != ErrorCode_SUCCESS )
        return;

    /*
     * .... with a local cache
     *    (except for HP-UX 11, which extracts objects individually)
     */
    AgentHandler_injectHandler( reginfo,
        CacheHandler_getCacheHandler( TCP_STATS_CACHE_TIMEOUT,
                                    tcp_load, tcp_free,
                                    tcp_oid, ASN01_OID_LENGTH( tcp_oid ) ) );

    REGISTER_SYSOR_ENTRY( tcp_module_oid,
        "The MIB module for managing TCP implementations" );

    auto_nlist( TCPSTAT_SYMBOL, 0, 0 );
    auto_nlist( TCP_SYMBOL, 0, 0 );
}

/*********************
	 *
	 *  System specific implementation functions
	 *
	 *********************/

struct tcp_mib tcpstat;

/*********************
	 *
	 *  System independent handler (mostly)
	 *
	 *********************/

int tcp_handler( MibHandler* handler,
    HandlerRegistration* reginfo,
    AgentRequestInfo* reqinfo,
    RequestInfo* requests )
{
    RequestInfo* request;
    VariableList* requestvb;
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
    DEBUG_MSGTL( ( "mibII/tcpScalar", "Handler - mode %s\n",
        MapList_findLabel( "agentMode", reqinfo->mode ) ) );
    switch ( reqinfo->mode ) {
    case MODE_GET:
        for ( request = requests; request; request = request->next ) {
            requestvb = request->requestvb;
            subid = requestvb->name[ ASN01_OID_LENGTH( tcp_oid ) ]; /* XXX */

            DEBUG_MSGTL( ( "mibII/tcpScalar", "oid: " ) );
            DEBUG_MSGOID( ( "mibII/tcpScalar", requestvb->name,
                requestvb->nameLength ) );
            DEBUG_MSG( ( "mibII/tcpScalar", "\n" ) );
            switch ( subid ) {
            case TCPRTOALGORITHM:
                ret_value = tcpstat.tcpRtoAlgorithm;
                type = ASN01_INTEGER;
                break;
            case TCPRTOMIN:
                ret_value = tcpstat.tcpRtoMin;
                type = ASN01_INTEGER;
                break;
            case TCPRTOMAX:
                ret_value = tcpstat.tcpRtoMax;
                type = ASN01_INTEGER;
                break;
            case TCPMAXCONN:
                ret_value = tcpstat.tcpMaxConn;
                type = ASN01_INTEGER;
                break;
            case TCPACTIVEOPENS:
                ret_value = tcpstat.tcpActiveOpens;
                break;
            case TCPPASSIVEOPENS:
                ret_value = tcpstat.tcpPassiveOpens;
                break;
            case TCPATTEMPTFAILS:
                ret_value = tcpstat.tcpAttemptFails;
                break;
            case TCPESTABRESETS:
                ret_value = tcpstat.tcpEstabResets;
                break;
            case TCPCURRESTAB:
                ret_value = tcpstat.tcpCurrEstab;
                type = ASN01_GAUGE;
                break;
            case TCPINSEGS:
                ret_value = tcpstat.tcpInSegs & 0xffffffff;
                break;
            case TCPOUTSEGS:
                ret_value = tcpstat.tcpOutSegs & 0xffffffff;
                break;
            case TCPRETRANSSEGS:
                ret_value = tcpstat.tcpRetransSegs;
                break;
            case TCPINERRS:

                if ( tcpstat.tcpInErrsValid ) {
                    ret_value = tcpstat.tcpInErrs;
                    break;
                } else {
                    Agent_setRequestError( reqinfo, request, PRIOT_NOSUCHOBJECT );
                    continue;
                }

            case TCPOUTRSTS:
                if ( tcpstat.tcpOutRstsValid ) {
                    ret_value = tcpstat.tcpOutRsts;
                    break;
                }
                Agent_setRequestError( reqinfo, request, PRIOT_NOSUCHOBJECT );
                continue;

            case TCPCONNTABLE:
                /*
	 * This is not actually a valid scalar object.
	 * The table registration should take precedence,
	 *   so skip this subtree, regardless of architecture.
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
    case MODE_SET_RESERVE2:
    case MODE_SET_ACTION:
    case MODE_SET_COMMIT:
    case MODE_SET_FREE:
    case MODE_SET_UNDO:
        Logger_log( LOGGER_PRIORITY_WARNING, "mibII/tcp: Unsupported mode (%d)\n",
            reqinfo->mode );
        break;
    default:
        Logger_log( LOGGER_PRIORITY_WARNING, "mibII/tcp: Unrecognised mode (%d)\n",
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

int tcp_load( Cache* cache, void* vmagic )
{
    long ret_value = -1;

    ret_value = linux_read_tcp_stat( &tcpstat );

    if ( ret_value < 0 ) {
        DEBUG_MSGTL( ( "mibII/tcpScalar", "Failed to load TCP scalar Group (linux)\n" ) );
    } else {
        DEBUG_MSGTL( ( "mibII/tcpScalar", "Loaded TCP scalar Group (linux)\n" ) );
    }
    return ret_value;
}

void tcp_free( Cache* cache, void* magic )
{
    memset( &tcpstat, 0, sizeof( tcpstat ) );
}
