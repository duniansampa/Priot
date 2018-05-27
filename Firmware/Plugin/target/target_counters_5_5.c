
#include "target_counters_5_5.h"
#include "System/Util/Trace.h"
#include "GetStatistic.h"

void init_target_counters_5_5( void )
{
    oid target_oid[] = { 1, 3, 6, 1, 6, 3, 12, 1 };

    DEBUG_MSGTL( ( "target_counters", "initializing\n" ) );

    GETSTATISTIC_REGISTER_STATISTIC_HANDLER(
        AgentHandler_createHandlerRegistration(
            "target_counters", NULL, target_oid, ASN01_OID_LENGTH( target_oid ),
            HANDLER_CAN_RONLY ),
        4, TARGET );
}
