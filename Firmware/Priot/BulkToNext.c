#include "BulkToNext.h"
#include "Client.h"
#include "System/Util/Assert.h"
#include "Api.h"
#include "System/Util/Trace.h"

/** @defgroup bulk_to_next bulk_to_next
 *  Convert GETBULK requests into GETNEXT requests for the handler.
 *  The only purpose of this handler is to convert a GETBULK request
 *  to a GETNEXT request.  It is inserted into handler chains where
 *  the handler has not set the HANDLER_CAN_GETBULK flag.
 *  @ingroup utilities
 *  @{
 */

/** returns a bulk_to_next handler that can be injected into a given
 *  handler chain.
 */
MibHandler*
BulkToNext_getBulkToNextHandler(void)
{
    MibHandler* handler =
        AgentHandler_createHandler("bulkToNext",
                               BulkToNext_helper);

    if (NULL != handler)
        handler->flags |= MIB_HANDLER_AUTO_NEXT;

    return handler;
}

/** takes answered requests and decrements the repeat count and
 *  updates the requests to the next to-do varbind in the list */
void
BulkToNext_fixRequests(RequestInfo *requests)
{
    RequestInfo *request;
    /*
     * Make sure that:
     *    - repeats remain
     *    - last handler provided an answer
     *    - answer didn't exceed range end (ala check_getnext_results)
     *    - there is a next variable
     * then
     * update the varbinds for the next request series
     */
    for (request = requests; request; request = request->next) {
        if (request->repeat > 0 &&
            request->requestvb->type != asnNULL &&
            request->requestvb->type != asnPRIV_RETRY &&
            (Api_oidCompare(request->requestvb->name,
                              request->requestvb->nameLength,
                              request->range_end,
                              request->range_end_len) < 0) &&
            request->requestvb->next ) {
            request->repeat--;
            Client_setVarObjid(request->requestvb->next,
                               request->requestvb->name,
                               request->requestvb->nameLength);
            request->requestvb = request->requestvb->next;
            request->requestvb->type = asnPRIV_RETRY;
            /*
             * if inclusive == 2, it was set in check_getnext_results for
             * the previous requestvb. Now that we've moved on, clear it.
             */
            if (2 == request->inclusive)
                request->inclusive = 0;
        }
    }
}

/** @internal Implements the bulk_to_next handler */
int
BulkToNext_helper(MibHandler *handler,
                            HandlerRegistration *reginfo,
                            AgentRequestInfo *reqinfo,
                            RequestInfo *requests)
{

    int             ret = PRIOT_ERR_NOERROR;

    /*
     * this code depends on AUTO_NEXT being set
     */
    Assert_assert(handler->flags & MIB_HANDLER_AUTO_NEXT);

    /*
     * don't do anything for any modes besides GETBULK. Just return, and
     * the agent will call the next handler (AUTO_NEXT).
     *
     * for GETBULK, we munge the mode, call the next handler ourselves
     * (setting AUTO_NEXT_OVERRRIDE so the agent knows what we did),
     * restore the mode and fix up the requests.
     */
    if(MODE_GETBULK == reqinfo->mode) {

        DEBUG_IF("bulkToNext") {
            RequestInfo *req = requests;
            while(req) {
                DEBUG_MSGTL(("bulkToNext", "Got request: "));
                DEBUG_MSGOID(("bulkToNext", req->requestvb->name,
                             req->requestvb->nameLength));
                DEBUG_MSG(("bulkToNext", "\n"));
                req = req->next;
            }
        }

        reqinfo->mode = MODE_GETNEXT;
        ret =
            AgentHandler_callNextHandler(handler, reginfo, reqinfo, requests);
        reqinfo->mode = MODE_GETBULK;

        /*
         * update the varbinds for the next request series
         */
        BulkToNext_fixRequests(requests);

        /*
         * let agent handler know that we've already called next handler
         */
        handler->flags |= MIB_HANDLER_AUTO_NEXT_OVERRIDE_ONCE;
    }

    return ret;
}

/** initializes the bulk_to_next helper which then registers a bulk_to_next
 *  handler as a run-time injectable handler for configuration file
 *  use.
 */
void
BulkToNext_initBulkToNextHelper(void)
{
    AgentHandler_registerHandlerByName("bulkToNext",
                                     BulkToNext_getBulkToNextHandler());
}
