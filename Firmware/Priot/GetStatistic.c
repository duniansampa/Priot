#include "GetStatistic.h"
#include "Api.h"
#include "Client.h"
#include "ScalarGroup.h"

static int
_GetStatistic_getStatisticHelperHandler(MibHandler *handler,
                                     HandlerRegistration *reginfo,
                                     AgentRequestInfo *reqinfo,
                                     RequestInfo *requests)
{
    if (reqinfo->mode == MODE_GET) {
        const oid idx = requests->requestvb->name[reginfo->rootoid_len - 2] +
            (oid)(uintptr_t)handler->myvoid;
        uint32_t value;

        if (idx > API_STAT_MAX_STATS)
            return PRIOT_ERR_GENERR;
        value = Api_getStatistic(idx);
        Client_setVarTypedValue(requests->requestvb, ASN01_COUNTER,
                                 (const u_char*)&value, sizeof(value));
        return PRIOT_ERR_NOERROR;
    }
    return PRIOT_ERR_GENERR;
}

static MibHandler *
_GetStatistic_getStatisticHandler(int offset)
{
    MibHandler *ret =
        AgentHandler_createHandler("getStatistic",
                               _GetStatistic_getStatisticHelperHandler);
    if (ret) {
        ret->flags |= MIB_HANDLER_AUTO_NEXT;
        ret->myvoid = (void*)(uintptr_t)offset;
    }
    return ret;
}

int
GetStatistic_registerStatisticHandler(HandlerRegistration *reginfo,
                                   oid start, int begin, int end)
{
    AgentHandler_injectHandler(reginfo,
                           _GetStatistic_getStatisticHandler(begin - start));
    return ScalarGroup_registerScalarGroup(reginfo, start, start + (end - begin));
}
