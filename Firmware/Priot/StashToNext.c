#include "StashToNext.h"
#include "OidStash.h"
#include "System/Util/Assert.h"
#include "StashCache.h"
#include "Client.h"
#include "Api.h"

/** @defgroup stash_to_next stash_to_next
 *  Convert GET_STASH requests into GETNEXT requests for the handler.
 *  The purpose of this handler is to convert a GET_STASH auto-cache request
 *  to a series of GETNEXT requests.  It can be inserted into a handler chain
 *  where the lower-level handlers don't process such requests themselves.
 *  @ingroup utilities
 *  @{
 */

/** returns a stash_to_next handler that can be injected into a given
 *  handler chain.
 */
MibHandler *
StashToNext_getStashToNextHandler(void)
{
    MibHandler *handler =
        AgentHandler_createHandler("stashToNext",
                               StashToNext_helper);

    if (NULL != handler)
        handler->flags |= MIB_HANDLER_AUTO_NEXT;

    return handler;
}

/** @internal Implements the stash_to_next handler */
int
StashToNext_helper( MibHandler*          handler,
                    HandlerRegistration* reginfo,
                    AgentRequestInfo*    reqinfo,
                    RequestInfo*         requests )
{

    int             ret = PRIOT_ERR_NOERROR;
    int             namelen;
    int             finished = 0;
    OidStash_Node **cinfo;
    VariableList   *vb;
    RequestInfo    *reqtmp;

    /*
     * this code depends on AUTO_NEXT being set
     */
    Assert_assert(handler->flags & MIB_HANDLER_AUTO_NEXT);

    /*
     * Don't do anything for any modes except GET_STASH. Just return,
     * and the agent will call the next handler (AUTO_NEXT).
     *
     * If the handler chain already supports GET_STASH, we don't
     * need to do anything here either.  Once again, we just return
     * and the agent will call the next handler (AUTO_NEXT).
     *
     * Otherwise, we munge the mode to GET_NEXT, and call the
     * next handler ourselves, repeatedly until we've retrieved the
     * full contents of the table or subtree.
     *   Then restore the mode and return to the calling handler
     * (setting AUTO_NEXT_OVERRRIDE so the agent knows what we did).
     */
    if (MODE_GET_STASH == reqinfo->mode) {
        if ( reginfo->modes & HANDLER_CAN_STASH ) {
            return ret;
        }
        cinfo  = StashCache_extractStashCache( reqinfo );
        reqtmp = MEMORY_MALLOC_TYPEDEF(RequestInfo);
        vb = reqtmp->requestvb = MEMORY_MALLOC_TYPEDEF( VariableList );
        vb->type = ASN01_NULL;
        Client_setVarObjid( vb, reginfo->rootoid, reginfo->rootoid_len );

        reqinfo->mode = MODE_GETNEXT;
        while (!finished) {
            ret = AgentHandler_callNextHandler(handler, reginfo, reqinfo, reqtmp);
            namelen = UTILITIES_MIN_VALUE(vb->nameLength, reginfo->rootoid_len);
            if ( !Api_oidCompare( reginfo->rootoid, reginfo->rootoid_len,
                                   vb->name, namelen) &&
                 vb->type != ASN01_NULL && vb->type != PRIOT_ENDOFMIBVIEW ) {
                /*
                 * This result is relevant so save it, and prepare
                 * the request varbind for the next query.
                 */
                OidStash_addData( cinfo, vb->name, vb->nameLength,
                                            Client_cloneVarbind( vb ));
                    /*
                     * Tidy up the response structure,
                     *  ready for retrieving the next entry
                     */
                Map_clear(reqtmp->parent_data);
                reqtmp->parent_data = NULL;
                reqtmp->processed = 0;
                vb->type = ASN01_NULL;
            } else {
                finished = 1;
            }
        }
        reqinfo->mode = MODE_GET_STASH;

        /*
         * let the handler chain processing know that we've already
         * called the next handler
         */
        handler->flags |= MIB_HANDLER_AUTO_NEXT_OVERRIDE_ONCE;
    }

    return ret;
}
