#include "ReadOnly.h"
#include "Debug.h"


/** @defgroup read_only read_only
 *  Make your handler read_only automatically
 *  The only purpose of this handler is to return an
 *  appropriate error for any requests passed to it in a SET mode.
 *  Inserting it into your handler chain will ensure you're never
 *  asked to perform a SET request so you can ignore those error
 *  conditions.
 *  @ingroup utilities
 *  @{
 */

/** returns a read_only handler that can be injected into a given
 *  handler chain.
 */
MibHandler*
ReadOnly_getReadOnlyHandler(void)
{
    MibHandler *ret = NULL;

    ret = AgentHandler_createHandler("readOnly", ReadOnly_helper);
    if (ret) {
        ret->flags |= MIB_HANDLER_AUTO_NEXT;
    }
    return ret;
}

/** @internal Implements the read_only handler */
int
ReadOnly_helper( MibHandler*          handler,
                 HandlerRegistration* reginfo,
                 AgentRequestInfo*    reqinfo,
                 RequestInfo*         requests )
{

    DEBUG_MSGTL(("helper:read_only", "Got request\n"));

    switch (reqinfo->mode) {

    case MODE_SET_RESERVE1:
    case MODE_SET_RESERVE2:
    case MODE_SET_ACTION:
    case MODE_SET_COMMIT:
    case MODE_SET_FREE:
    case MODE_SET_UNDO:
        Agent_requestSetErrorAll(requests, PRIOT_ERR_NOTWRITABLE);
        return PRIOT_ERR_NOTWRITABLE;

    case MODE_GET:
    case MODE_GETNEXT:
    case MODE_GETBULK:
        /* next handler called automatically - 'AUTO_NEXT' */
        return PRIOT_ERR_NOERROR;
    }

    Agent_requestSetErrorAll(requests, PRIOT_ERR_GENERR);
    return PRIOT_ERR_GENERR;
}

/** initializes the read_only helper which then registers a read_only
 *  handler as a run-time injectable handler for configuration file
 *  use.
 */
void
ReadOnly_initReadOnlyHelper(void)
{
    AgentHandler_registerHandlerByName("readOnly",
                                     ReadOnly_getReadOnlyHandler());
}
