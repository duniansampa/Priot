/*
 * usmStats.c: implements the usmStats portion of the SNMP-USER-BASED-SM-MIB
 */

#include "usmStats_5_5.h"

#include "AgentRegistry.h"
#include "GetStatistic.h"
#include "SysORTable.h"

#define snmpUsmMIB 1, 3, 6, 1, 6, 3, 15
#define usmMIBCompliances snmpUsmMIB, 2, 1

static oid usmStats[] = { snmpUsmMIB, 1, 1 };

static HandlerRegistration* usmStats_reg = NULL;
static oid usmMIBCompliance[] = { usmMIBCompliances, 1 };

void init_usmStats_5_5( void )
{
    HandlerRegistration* s = AgentHandler_createHandlerRegistration(
        "usmStats", NULL, usmStats, asnOID_LENGTH( usmStats ),
        HANDLER_CAN_RONLY );
    if ( s && GETSTATISTIC_REGISTER_STATISTIC_HANDLER( s, 1, USM ) == MIB_REGISTERED_OK ) {
        REGISTER_SYSOR_ENTRY( usmMIBCompliance,
            "The management information definitions for the "
            "SNMP User-based Security Model." );
        usmStats_reg = s;
    }
}

void shutdown_usmStats_5_5( void )
{
    UNREGISTER_SYSOR_ENTRY( usmMIBCompliance );
    if ( usmStats_reg ) {
        AgentHandler_unregisterHandler( usmStats_reg );
        usmStats_reg = NULL;
    }
}
