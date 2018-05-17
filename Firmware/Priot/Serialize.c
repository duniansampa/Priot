#include "Serialize.h"

#include "System/Util/Debug.h"

/** @defgroup serialize serialize
 *  Calls sub handlers one request at a time.
 *  @ingroup utilities
 *  This functionally passes in one request at a time
 *  into lower handlers rather than a whole bunch of requests at once.
 *  This is useful for handlers that don't want to iterate through the
 *  request lists themselves.  Generally, this is probably less
 *  efficient so use with caution.  The serialize handler might be
 *  useable to dynamically fix handlers with broken looping code,
 *  however.
 *  @{
 */

/** returns a serialize handler that can be injected into a given
 *  handler chain.
 */
MibHandler *
Serialize_getSerializeHandler(void)
{
    return AgentHandler_createHandler("serialize",
                                  Serialize_helperHandler);
}

/** functionally the same as calling AgentHandler_registerHandler() but also
 * injects a serialize handler at the same time for you. */
int
Serialize_registerSerialize(HandlerRegistration *reginfo)
{
    AgentHandler_injectHandler(reginfo, Serialize_getSerializeHandler());
    return AgentHandler_registerHandler(reginfo);
}

/** Implements the serial handler */
int
Serialize_helperHandler(MibHandler *handler,
                                 HandlerRegistration *reginfo,
                                 AgentRequestInfo *reqinfo,
                                 RequestInfo *requests)
{

    RequestInfo *request, *requesttmp;

    DEBUG_MSGTL(("helper:serialize", "Got request\n"));
    /*
     * loop through requests
     */
    for (request = requests; request; request = request->next) {
        int             ret;

        /*
         * store next pointer and delete it
         */
        requesttmp = request->next;
        request->next = NULL;

        /*
         * call the next handler
         */
        ret = AgentHandler_callNextHandler(handler, reginfo, reqinfo, request);

        /*
         * restore original next pointer
         */
        request->next = requesttmp;

        if (ret != PRIOT_ERR_NOERROR)
            return ret;
    }

    return PRIOT_ERR_NOERROR;
}

/**
 *  initializes the serialize helper which then registers a serialize
 *  handler as a run-time injectable handler for configuration file
 *  use.
 */
void
Serialize_initSerialize(void)
{
    AgentHandler_registerHandlerByName("serialize",
                                     Serialize_getSerializeHandler());
}
