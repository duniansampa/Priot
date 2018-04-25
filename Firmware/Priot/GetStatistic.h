#ifndef GETSTATISTIC_H
#define GETSTATISTIC_H

#include "AgentHandler.h"

/** Registers a scalar group with statistics from @ref snmp_get_statistic.
 *  as reginfo.[start, start + end - begin].
 *  @param reginfo registration to register the items under
 *  @param start offset to the begin item
 *  @param begin first snmp_get_statistic key to return
 *  @param end last snmp_get_statistic key to return
 */
int GetStatistic_registerStatisticHandler( HandlerRegistration* reginfo,
    oid start,
    int begin,
    int end );

#define GETSTATISTIC_REGISTER_STATISTIC_HANDLER( reginfo, start, stat ) \
    GetStatistic_registerStatisticHandler( reginfo, start,              \
        API_STAT_##stat##_STATS_START,                                  \
        API_STAT_##stat##_STATS_END )

#endif // GETSTATISTIC_H
