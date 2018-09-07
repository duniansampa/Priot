/*
 * snmpMPDStats.c: tallies errors for SNMPv3 message processing.
 */
#include "snmpMPDStats_5_5.h"

#include "AgentRegistry.h"
#include "SysORTable.h"
#include "GetStatistic.h"

#define snmpMPDMIB 1, 3, 6, 1, 6, 3, 11
#define snmpMPDMIBObjects snmpMPDMIB, 2
#define snmpMPDMIBCompliances snmpMPDMIB, 3, 1

static oid snmpMPDStats[] = { snmpMPDMIBObjects, 1 };

static HandlerRegistration* snmpMPDStats_reg = NULL;
static oid snmpMPDCompliance[] = { snmpMPDMIBCompliances, 1 };

void
init_snmpMPDStats_5_5(void)
{
    HandlerRegistration* s =
        AgentHandler_createHandlerRegistration(
            "snmpMPDStats", NULL, snmpMPDStats, asnOID_LENGTH(snmpMPDStats),
            HANDLER_CAN_RONLY);
    if (s &&
    GETSTATISTIC_REGISTER_STATISTIC_HANDLER(s, 1, MPD) == MIB_REGISTERED_OK) {
        REGISTER_SYSOR_ENTRY(snmpMPDCompliance,
                             "The MIB for Message Processing and Dispatching.");
        snmpMPDStats_reg = s;
    }
}

void
shutdown_snmpMPDStats_5_5(void)
{
    UNREGISTER_SYSOR_ENTRY(snmpMPDCompliance);
    if (snmpMPDStats_reg) {
        AgentHandler_unregisterHandler(snmpMPDStats_reg);
        snmpMPDStats_reg = NULL;
    }
}
