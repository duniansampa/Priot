#include "Null.h"
#include "Debug.h"


int
Null_registerNull( oid*   loc,
                   size_t loc_len)
{
    return Null_registerNullContext(loc, loc_len, NULL);
}

int
Null_registerNullContext( oid *       loc,
                          size_t      loc_len,
                          const char* contextName )
{
    HandlerRegistration *reginfo;
    reginfo = TOOLS_MALLOC_TYPEDEF(HandlerRegistration);
    if (reginfo != NULL) {
        reginfo->handlerName = strdup("");
        reginfo->rootoid = loc;
        reginfo->rootoid_len = loc_len;
        reginfo->handler =
            AgentHandler_createHandler("null", Null_handler);
        if (contextName)
            reginfo->contextName = strdup(contextName);
        reginfo->modes = HANDLER_CAN_DEFAULT | HANDLER_CAN_GETBULK;
    }
    return AgentHandler_registerHandler(reginfo);
}

int
Null_handler( MibHandler*          handler,
              HandlerRegistration* reginfo,
              AgentRequestInfo*    reqinfo,
              RequestInfo*         requests )
{
    DEBUG_MSGTL(("helper:null", "Got request\n"));

    DEBUG_MSGTL(("helper:null", "  oid:"));
    DEBUG_MSGOID(("helper:null", requests->requestvb->name,
                 requests->requestvb->nameLength));
    DEBUG_MSG(("helper:null", "\n"));

    switch (reqinfo->mode) {
    case MODE_GETNEXT:
    case MODE_GETBULK:
        return PRIOT_ERR_NOERROR;

    case MODE_GET:
        Agent_requestSetErrorAll(requests, PRIOT_NOSUCHOBJECT);
        return PRIOT_ERR_NOERROR;

    default:
        Agent_requestSetErrorAll(requests, PRIOT_ERR_NOSUCHNAME);
        return PRIOT_ERR_NOERROR;
    }
}
