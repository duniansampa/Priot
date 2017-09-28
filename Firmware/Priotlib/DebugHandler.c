#include "DebugHandler.h"
#include "Tools.h"
#include "Debug.h"
#include "Enum.h"


/** @defgroup debug debug
 *  Print out debugging information about the handler chain being called.
 *  This is a useful module for run-time
 *  debugging of requests as the pass this handler in a calling chain.
 *  All debugging output is done via the standard debugging routines
 *  with a token name of "helper:debug", so use the -Dhelper:debug
 *  command line flag to see the output when running the snmpd
 *  demon. It's not recommended you compile this into a handler chain
 *  during compile time, but instead use the "injectHandler" token in
 *  the snmpd.conf file (or similar) to add it to the chain later:
 *
 *     injectHandler debug my_module_name
 *
 *  to see an example output, try:
 *
 *     injectHandler debug mibII/system
 *
 *  and then run snmpwalk on the "system" group.
 *
 *  @ingroup utilities
 *  @{
 */

/** returns a debug handler that can be injected into a given
 *  handler chain.
 */
MibHandler*
DebugHandler_getDebugHandler(void)
{
    return AgentHandler_createHandler("debug", DebugHandler_debugHelper);
}


/** @internal debug print variables in a chain */
static void
DebugHandler_printRequests(RequestInfo *requests)
{
    RequestInfo *request;

    for (request = requests; request; request = request->next) {
        DEBUG_MSGTL(("helper:debug", "      #%2d: ", request->index));
        DEBUG_MSGVAR(("helper:debug", request->requestvb));
        DEBUG_MSG(("helper:debug", "\n"));

        if (request->processed)
            DEBUG_MSGTL(("helper:debug", "        [processed]\n"));
        if (request->delegated)
            DEBUG_MSGTL(("helper:debug", "        [delegated]\n"));
        if (request->status)
            DEBUG_MSGTL(("helper:debug", "        [status = %d]\n",
                        request->status));
        if (request->parent_data) {
            DataList_DataList *lst;
            DEBUG_MSGTL(("helper:debug", "        [parent data ="));
            for (lst = request->parent_data; lst; lst = lst->next) {
                DEBUG_MSG(("helper:debug", " %s", lst->name));
            }
            DEBUG_MSG(("helper:debug", "]\n"));
        }
    }
}

/** @internal Implements the debug handler */
int
DebugHandler_debugHelper( MibHandler*          handler,
                          HandlerRegistration* reginfo,
                          AgentRequestInfo*    reqinfo,
                          RequestInfo*         requests )
{
    int ret;

    DEBUG_IF("helper:debug") {
        MibHandler *hptr;
        char                *cp;
        int                  i, count;

        DEBUG_MSGTL(("helper:debug", "Entering Debugging Helper:\n"));
        DEBUG_MSGTL(("helper:debug", "  Handler Registration Info:\n"));
        DEBUG_MSGTL(("helper:debug", "    Name:        %s\n",
                    reginfo->handlerName));
        DEBUG_MSGTL(("helper:debug", "    Context:     %s\n",
                    TOOLS_STRORNULL(reginfo->contextName)));
        DEBUG_MSGTL(("helper:debug", "    Base OID:    "));
        DEBUG_MSGOID(("helper:debug", reginfo->rootoid, reginfo->rootoid_len));
        DEBUG_MSG(("helper:debug", "\n"));

        DEBUG_MSGTL(("helper:debug", "    Modes:       0x%x = ",
                    reginfo->modes));
        for (count = 0, i = reginfo->modes; i; i = i >> 1, count++) {
            if (i & 0x01) {
                cp = Enum_seFindLabelInSlist("handler_can_mode",
                                            0x01 << count);
                DEBUG_MSG(("helper:debug", "%s | ", TOOLS_STRORNULL(cp)));
            }
        }
        DEBUG_MSG(("helper:debug", "\n"));

        DEBUG_MSGTL(("helper:debug", "    Priority:    %d\n",
                    reginfo->priority));

        DEBUG_MSGTL(("helper:debug", "  Handler Calling Chain:\n"));
        DEBUG_MSGTL(("helper:debug", "   "));
        for (hptr = reginfo->handler; hptr; hptr = hptr->next) {
            DEBUG_MSG(("helper:debug", " -> %s", hptr->handler_name));
            if (hptr->myvoid)
                DEBUG_MSG(("helper:debug", " [myvoid = %p]", hptr->myvoid));
        }
        DEBUG_MSG(("helper:debug", "\n"));

        DEBUG_MSGTL(("helper:debug", "  Request information:\n"));
        DEBUG_MSGTL(("helper:debug", "    Mode:        %s (%d = 0x%x)\n",
                     Enum_seFindLabelInSlist("agent_mode", reqinfo->mode),
                    reqinfo->mode, reqinfo->mode));
        DEBUG_MSGTL(("helper:debug", "    Request Variables:\n"));
        DebugHandler_printRequests(requests);

        DEBUG_MSGTL(("helper:debug", "  --- calling next handler --- \n"));
    }

    ret = AgentHandler_callNextHandler(handler, reginfo, reqinfo, requests);

    DEBUG_IF("helper:debug") {
        DEBUG_MSGTL(("helper:debug", "  Results:\n"));
        DEBUG_MSGTL(("helper:debug", "    Returned code: %d\n", ret));
        DEBUG_MSGTL(("helper:debug", "    Returned Variables:\n"));
        DebugHandler_printRequests(requests);

        DEBUG_MSGTL(("helper:debug", "Exiting Debugging Helper:\n"));
    }

    return ret;
}

/** initializes the debug helper which then registers a debug
 *  handler as a run-time injectable handler for configuration file
 *  use.
 */
void
DebugHandler_initDebugHelper(void)
{
    AgentHandler_registerHandlerByName("debug", DebugHandler_getDebugHandler());
}
