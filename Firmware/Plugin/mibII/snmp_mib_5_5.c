#include "snmp_mib_5_5.h"
#include "AgentReadConfig.h"
#include "System/Util/Callback.h"
#include "System/Util/VariableList.h"
#include "GetStatistic.h"
#include "PluginSettings.h"
#include "SysORTable.h"
#include "System/Util/Trace.h"
#include "Watcher.h"
#include "updates.h"

#define SNMP_OID 1, 3, 6, 1, 2, 1, 11

static oid snmp_oid[] = { SNMP_OID };

extern long trap_enableauthentraps;
extern int trap_enableauthentrapsset;

extern HandlerRegistration*
netsnmp_create_update_handler_registration( const char* name, const oid* id, size_t idlen, int mode, int* set );

static int
snmp_enableauthentraps_store( int a, int b, void* c, void* d )
{
    char line[ UTILITIES_MAX_BUFFER_SMALL ];

    if ( trap_enableauthentrapsset > 0 ) {
        snprintf( line, UTILITIES_MAX_BUFFER_SMALL, "pauthtrapenable %ld",
            trap_enableauthentraps );
        AgentReadConfig_priotdStoreConfig( line );
    }
    return 0;
}

static int
handle_truthvalue( MibHandler* handler,
    HandlerRegistration* reginfo,
    AgentRequestInfo* reqinfo,
    RequestInfo* requests )
{
    if ( reqinfo->mode == MODE_SET_RESERVE1 ) {
        int res = VariableList_checkBoolLengthAndValue( requests->requestvb );
        if ( res != PRIOT_ERR_NOERROR )
            Agent_requestSetError( requests, res );
    }
    return PRIOT_ERR_NOERROR;
}

static MibHandler*
netsnmp_get_truthvalue( void )
{
    MibHandler* hnd = AgentHandler_createHandler( "truthvalue", handle_truthvalue );
    if ( hnd )
        hnd->flags |= MIB_HANDLER_AUTO_NEXT;
    return hnd;
}

static int
handle_snmp( MibHandler* handler,
    HandlerRegistration* reginfo,
    AgentRequestInfo* reqinfo,
    RequestInfo* requests )
{
    switch ( requests->requestvb->name[ ASN01_OID_LENGTH( snmp_oid ) ] ) {
    case 7:
    case 23:
    case 30:
        Agent_setRequestError( reqinfo, requests, PRIOT_NOSUCHOBJECT );
        break;
    default:
        break;
    }
    return PRIOT_ERR_NOERROR;
}

extern oid systemMib_systemModuleOid[];
extern int systemMib_systemModuleOidLen;
extern int systemMib_systemModuleCount;

/** Initializes the snmp module */
void init_snmp_mib_5_5( void )
{
    DEBUG_MSGTL( ( "priot", "Initializing\n" ) );

    GETSTATISTIC_REGISTER_STATISTIC_HANDLER(
        AgentHandler_createHandlerRegistration(
            "mibII/snmp", handle_snmp, snmp_oid, ASN01_OID_LENGTH( snmp_oid ),
            HANDLER_CAN_RONLY ),
        1, SNMP );
    {
        oid snmpEnableAuthenTraps_oid[] = { SNMP_OID, 30, 0 };
        static WatcherInfo enableauthen_info;
        HandlerRegistration* reg = netsnmp_create_update_handler_registration(
            "mibII/snmpEnableAuthenTraps",
            snmpEnableAuthenTraps_oid,
            ASN01_OID_LENGTH( snmpEnableAuthenTraps_oid ),
            HANDLER_CAN_RWRITE, &trap_enableauthentrapsset );

        AgentHandler_injectHandler( reg, netsnmp_get_truthvalue() );
        Watcher_registerWatchedInstance(
            reg,
            Watcher_initWatcherInfo(
                &enableauthen_info,
                &trap_enableauthentraps, sizeof( trap_enableauthentraps ),
                ASN01_INTEGER, WATCHER_FIXED_SIZE ) );
    }

    if ( ++systemMib_systemModuleCount == 3 )
        REGISTER_SYSOR_TABLE( systemMib_systemModuleOid, systemMib_systemModuleOidLen,
            "The MIB module for SNMPv2 entities" );
    Callback_register( CallbackMajor_LIBRARY, CallbackMinor_STORE_DATA,
        snmp_enableauthentraps_store, NULL );
}
