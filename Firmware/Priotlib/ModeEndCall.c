#include "ModeEndCall.h"

/** @defgroup mode_end_call mode_end_call
 *  At the end of a series of requests, call another handler hook.
 *  Handlers that want to loop through a series of requests and then
 *  receive a callback at the end of a particular MODE can use this
 *  helper to make this possible.  For most modules, this is not
 *  needed as the handler itself could perform a for() loop around the
 *  request list and then perform its actions afterwards.  However, if
 *  something like the serialize helper is in use this isn't possible
 *  because not all the requests for a given handler are being passed
 *  downward in a single group.  Thus, this helper *must* be added
 *  above other helpers like the serialize helper to be useful.
 *
 *  Multiple mode specific handlers can be registered and will be
 *  called in the order they were regestered in.  Callbacks regesterd
 *  with a mode of NETSNMP_MODE_END_ALL_MODES will be called for all
 *  modes.
 *
 *  @ingroup utilities
 *  @{
 */

/** returns a mode_end_call handler that can be injected into a given
 *  handler chain.
 * @param endlist The callback list for the handler to make use of.
 * @return An injectable Net-SNMP handler.
 */
MibHandler*
ModeEndCall_getModeEndCallHandler(ModeHandlerList *endlist)
{
    MibHandler *me =
        AgentHandler_createHandler("mode_end_call",
                               ModeEndCall_helper);

    if (!me)
        return NULL;

    me->myvoid = endlist;
    return me;
}

/** adds a mode specific callback to the callback list.
 * @param endlist the information structure for the mode_end_call helper.  Can be NULL to create a new list.
 * @param mode the mode to be called upon.  A mode of NETSNMP_MODE_END_ALL_MODES = all modes.
 * @param callbackh the MibHandler callback to call.
 * @return the new registration information list upon success.
 */
ModeHandlerList *
ModeEndCall_addModeCallback( ModeHandlerList* endlist,
                             int              mode,
                             MibHandler*      callbackh ) {

    ModeHandlerList *ptr, *ptr2;
    ptr = TOOLS_MALLOC_TYPEDEF(ModeHandlerList);
    if (!ptr)
        return NULL;

    ptr->mode = mode;
    ptr->callback_handler = callbackh;
    ptr->next = NULL;

    if (!endlist)
        return ptr;

    /* get to end */
    for(ptr2 = endlist; ptr2->next != NULL; ptr2 = ptr2->next);

    ptr2->next = ptr;
    return endlist;
}

/** @internal Implements the mode_end_call handler */
int
ModeEndCall_helper(MibHandler *handler,
                            HandlerRegistration *reginfo,
                            AgentRequestInfo *reqinfo,
                            RequestInfo *requests)
{

    int             ret;
    int             ret2 = PRIOT_ERR_NOERROR;
    ModeHandlerList *ptr;

    /* always call the real handlers first */
    ret = AgentHandler_callNextHandler(handler, reginfo, reqinfo,
                                    requests);

    /* then call the callback handlers */
    for (ptr = (ModeHandlerList*)handler->myvoid; ptr; ptr = ptr->next) {
        if (ptr->mode == MODEENDCALL_MODE_END_ALL_MODES ||
            reqinfo->mode == ptr->mode) {
            ret2 = AgentHandler_callHandler(ptr->callback_handler, reginfo,
                                             reqinfo, requests);
            if (ret != PRIOT_ERR_NOERROR)
                ret = ret2;
        }
    }

    return ret2;
}
