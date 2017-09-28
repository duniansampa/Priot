#include "AgentHandler.h"
#include "PriotSettings.h"
#include "Tools.h"
#include "Debug.h"
#include "Logger.h"
#include "Assert.h"
#include "Priot.h"
#include "Api.h"
#include "AgentRegistry.h"
#include "Enum.h"
#include "DataList.h"
#include "ReadConfig.h"
#include "BulkToNext.h"
#include "ReadConfig.h"
#include "AgentReadConfig.h"

static MibHandler*
AgentHandler_cloneHandler( MibHandler* it );

/***********************************************************************/
/*
 * New Handler based API
 */
/***********************************************************************/
/** @defgroup handler Net-SNMP Agent handler and extensibility API
 *  @ingroup agent
 *
 *  The basic theory goes something like this: In the past, with the
 *  original mib module api (which derived from the original CMU SNMP
 *  code) the underlying mib modules were passed very little
 *  information (only the truly most basic information about a
 *  request).  This worked well at the time but in todays world of
 *  subagents, device instrumentation, low resource consumption, etc,
 *  it just isn't flexible enough.  "handlers" are here to fix all that.
 *
 *  With the rewrite of the agent internals for the net-snmp 5.0
 *  release, we introduce a modular calling scheme that allows agent
 *  modules to be written in a very flexible manner, and more
 *  importantly allows reuse of code in a decent way (and without the
 *  memory and speed overheads of OO languages like C++).
 *
 *  Functionally, the notion of what a handler does is the same as the
 *  older api: A handler is @link AgentHandler_createHandler() created@endlink and
 *  then @link AgentHandler_registerHandler() registered@endlink with the main
 *  agent at a given OID in the OID tree and gets called any time a
 *  request is made that it should respond to.  You probably should
 *  use one of the convenience helpers instead of doing anything else
 *  yourself though:
 *
 *  Most importantly, though, is that the handlers are built on the
 *  notion of modularity and reuse.  Specifically, rather than do all
 *  the really hard work (like parsing table indexes out of an
 *  incoming oid request) in each module, the API is designed to make
 *  it easy to write "helper" handlers that merely process some aspect
 *  of the request before passing it along to the final handler that
 *  returns the real answer.  Most people will want to make use of the
 *  @link instance instance@endlink, @link table table@endlink, @link
 *  table_iterator table_iterator@endlink, @link table_data
 *  table_data@endlink, or @link table_dataset table_dataset@endlink
 *  helpers to make their life easier.  These "helpers" interpert
 *  important aspects of the request and pass them on to you.
 *
 *  For instance, the @link table table@endlink helper is designed to
 *  hand you a list of extracted index values from an incoming
 *  request.  THe @link table_iterator table_iterator@endlink helper
 *  is built on top of the table helper, and is designed to help you
 *  iterate through data stored elsewhere (like in a kernel) that is
 *  not in OID lexographical order (ie, don't write your own index/oid
 *  sorting routine, use this helper instead).  The beauty of the
 *  @link table_iterator table_iterator helper@endlink, as well as the @link
 *  instance instance@endlink helper is that they take care of the complex
 *  GETNEXT processing entirely for you and hand you everything you
 *  need to merely return the data as if it was a GET request.  Much
 *  less code and hair pulling.  I've pulled all my hair out to help
 *  you so that only one of us has to be bald.
 *
 * @{
 */

/** Creates a MIB handler structure.
 *  The new structure is allocated and filled using the given name
 *  and access method.
 *  The returned handler should then be @link AgentHandler_registerHandler()
 *  registered @endlink.
 *
 *  @param name is the handler name and is copied then assigned to
 *              MibHandler->handler_name
 *
 *  @param handler_access_method is a function pointer used as the access
 *	   method for this handler registration instance for whatever required
 *         needs.
 *
 *  @return a pointer to a populated MibHandler struct to be
 *          registered
 *
 *  @see AgentHandler_createHandlerRegistration()
 *  @see AgentHandler_registerHandler()
 */
MibHandler*
AgentHandler_createHandler( const char*    name,
                            NodeHandlerFT* handler_access_method )
{

    MibHandler *ret = TOOLS_MALLOC_TYPEDEF(MibHandler);
    if (ret) {
        ret->access_method = handler_access_method;
        if (NULL != name) {
            ret->handler_name = strdup(name);
            if (NULL == ret->handler_name)
                TOOLS_FREE(ret);
        }
    }
    return ret;
}

/** Creates a MIB handler structure.
 *  The new structure is allocated and filled using the given name,
 *  access function, registration location OID and list of modes that
 *  the handler supports. If modes == 0, then modes will automatically
 *  be set to the default value of only HANDLER_CAN_DEFAULT, which is
 *  by default read-only GET and GETNEXT requests. A hander which supports
 *  sets but not row creation should set us a mode of HANDLER_CAN_SET_ONLY.
 *  @note This ends up calling AgentHandler_createHandler(name, handler_access_method)
 *  @param name is the handler name and is copied then assigned to
 *              HandlerRegistration->handlerName.
 *
 *  @param handler is a function pointer used as the access
 *	method for this handler registration instance for whatever required
 *	needs.
 *
 *  @param reg_oid is the registration location oid.
 *
 *  @param reg_oid_len is the length of reg_oid; can use the macro,
 *         OID_LENGTH
 *
 *  @param modes is used to configure read/write access.  If modes == 0,
 *	then modes will automatically be set to the default
 *	value of only HANDLER_CAN_DEFAULT, which is by default read-only GET
 *	and GETNEXT requests.  The other two mode options are read only,
 *	HANDLER_CAN_RONLY, and read/write, HANDLER_CAN_RWRITE.
 *
 *		- HANDLER_CAN_GETANDGETNEXT
 *		- HANDLER_CAN_SET
 *		- HANDLER_CAN_GETBULK
 *
 *		- HANDLER_CAN_RONLY   (HANDLER_CAN_GETANDGETNEXT)
 *		- HANDLER_CAN_RWRITE  (HANDLER_CAN_GETANDGETNEXT |
 *			HANDLER_CAN_SET)
 *		- HANDLER_CAN_DEFAULT HANDLER_CAN_RONLY
 *
 *  @return Returns a pointer to a HandlerRegistration struct.
 *          NULL is returned only when memory could not be allocated for the
 *          HandlerRegistration struct.
 *
 *
 *  @see AgentHandler_createHandler()
 *  @see AgentHandler_registerHandler()
 */
HandlerRegistration*
AgentHandler_registrationCreate( const char* name,
                                        MibHandler* handler,
                                        const oid* reg_oid,
                                        size_t      reg_oid_len,
                                        int         modes )
{
    HandlerRegistration *the_reg;
    the_reg = TOOLS_MALLOC_TYPEDEF(HandlerRegistration);
    if (!the_reg)
        return NULL;

    if (modes)
        the_reg->modes = modes;
    else
        the_reg->modes = HANDLER_CAN_DEFAULT;

    the_reg->handler = handler;
    the_reg->priority = DEFAULT_MIB_PRIORITY;
    if (name)
        the_reg->handlerName = strdup(name);
    the_reg->rootoid = Api_duplicateObjid(reg_oid, reg_oid_len);
    the_reg->rootoid_len = reg_oid_len;
    return the_reg;
}

/** Creates a handler registration structure with a new MIB handler.
 *  This function first @link AgentHandler_createHandler() creates @endlink
 *  a MIB handler, then @link AgentHandler_registrationCreate()
 *  makes registation structure @endlink for it.
 *
 *  @param name is the handler name for AgentHandler_createHandler()
 *
 *  @param handler_access_method is a function pointer used as the access
 *     method for AgentHandler_createHandler()
 *
 *  @param reg_oid is the registration location oid.
 *
 *  @param reg_oid_len is the length of reg_oid; can use the macro,
 *         OID_LENGTH
 *
 *  @param modes is used to configure read/write access, as in
 *         AgentHandler_registrationCreate()
 *
 *  @return Returns a pointer to a HandlerRegistration struct.
 *          If the structures creation failed, NULL is returned.
 *
 *  @see AgentHandler_createHandler()
 *  @see AgentHandler_registrationCreate()
 */
HandlerRegistration*
AgentHandler_createHandlerRegistration( const char*    name,
                                        NodeHandlerFT* handler_access_method,
                                        const oid*    reg_oid,
                                        size_t         reg_oid_len,
                                        int            modes )
{
    HandlerRegistration *rv = NULL;
    MibHandler *handler =
        AgentHandler_createHandler(name, handler_access_method);
    if (handler) {
        rv = AgentHandler_registrationCreate(
            name, handler, reg_oid, reg_oid_len, modes);
        if (!rv)
            AgentHandler_handlerFree(handler);
    }
    return rv;
}

/** Registers a MIB handler inside the registration structure.
 *  Checks given registation handler for sanity, then
 *  @link AgentRegistry_registerMib() performs registration @endlink
 *  in the MIB tree, as defined by the HandlerRegistration
 *  pointer. On success, SNMP_CALLBACK_APPLICATION is called.
 *  The registration struct may be created by call of
 *  AgentHandler_createHandlerRegistration().
 *
 *  @param reginfo Pointer to a HandlerRegistration struct.
 *
 *  @return Returns ErrorCode_SUCCESS or SNMP_ERR_* error code.
 *
 *  @see AgentHandler_createHandlerRegistration()
 *  @see AgentRegistry_registerMib()
 */
int
AgentHandler_registerHandler( HandlerRegistration* reginfo )
{
    MibHandler* handler;
    int flags = 0;

    if (reginfo == NULL) {
        Logger_log(LOGGER_PRIORITY_ERR, "AgentHandler_registerHandler() called illegally\n");
        Assert_assert(reginfo != NULL);
        return PRIOT_ERR_GENERR;
    }

    DEBUG_IF("handler::register") {
        DEBUG_MSGTL(("handler::register", "Registering %s (", reginfo->handlerName));
        for (handler = reginfo->handler; handler; handler = handler->next) {
            DEBUG_MSG(("handler::register", "::%s", handler->handler_name));
        }

        DEBUG_MSG(("handler::register", ") at "));
        if (reginfo->rootoid && reginfo->range_subid) {
            DEBUG_MSGOIDRANGE(("handler::register", reginfo->rootoid,
                              reginfo->rootoid_len, reginfo->range_subid,
                              reginfo->range_ubound));
        } else if (reginfo->rootoid) {
            DEBUG_MSGOID(("handler::register", reginfo->rootoid,
                         reginfo->rootoid_len));
        } else {
            DEBUG_MSG(("handler::register", "[null]"));
        }
        DEBUG_MSG(("handler::register", "\n"));
    }

    /*
     * don't let them register for absolutely nothing.  Probably a mistake
     */
    if (0 == reginfo->modes) {
        reginfo->modes = HANDLER_CAN_DEFAULT;
        Logger_log(LOGGER_PRIORITY_WARNING, "no registration modes specified for %s. "
                 "Defaulting to 0x%x\n", reginfo->handlerName, reginfo->modes);
    }

    /*
     * for handlers that can't GETBULK, force a conversion handler on them
     */
    if (!(reginfo->modes & HANDLER_CAN_GETBULK)) {
        AgentHandler_injectHandler(reginfo,
                               BulkToNext_getBulkToNextHandler());
    }

    for (handler = reginfo->handler; handler; handler = handler->next) {
        if (handler->flags & MIB_HANDLER_INSTANCE)
            flags = FULLY_QUALIFIED_INSTANCE;
    }

    return AgentRegistry_registerMib2(reginfo->handlerName,
                                NULL, 0, 0,
                                reginfo->rootoid, reginfo->rootoid_len,
                                reginfo->priority,
                                reginfo->range_subid,
                                reginfo->range_ubound, NULL,
                                reginfo->contextName, reginfo->timeout, flags,
                                reginfo, 1);
}

/** Unregisters a MIB handler described inside the registration structure.
 *  Removes a registration, performed earlier by
 *  AgentHandler_registerHandler(), from the MIB tree.
 *  Uses AgentRegistry_unregisterMibContext() to do the task.
 *
 *  @param reginfo Pointer to a HandlerRegistration struct.
 *
 *  @return Returns ErrorCode_SUCCESS or SNMP_ERR_* error code.
 *
 *  @see AgentHandler_registerHandler()
 *  @see AgentRegistry_unregisterMibContext()
 */
int
AgentHandler_unregisterHandler( HandlerRegistration* reginfo )
{
    return AgentRegistry_unregisterMibContext(reginfo->rootoid, reginfo->rootoid_len,
                                  reginfo->priority,
                                  reginfo->range_subid, reginfo->range_ubound,
                                  reginfo->contextName);
}

/** Registers a MIB handler inside the registration structure.
 *  Checks given registation handler for sanity, then
 *  @link AgentRegistry_registerMib() performs registration @endlink
 *  in the MIB tree, as defined by the HandlerRegistration
 *  pointer. Never calls SNMP_CALLBACK_APPLICATION.
 *  The registration struct may be created by call of
 *  AgentHandler_createHandlerRegistration().
 *
 *  @param reginfo Pointer to a HandlerRegistration struct.
 *
 *  @return Returns ErrorCode_SUCCESS or SNMP_ERR_* error code.
 *
 *  @see AgentHandler_createHandlerRegistration()
 *  @see AgentRegistry_registerMib()
 */
int
AgentHandler_registerHandlerNocallback( HandlerRegistration *reginfo )
{
    MibHandler *handler;
    if (reginfo == NULL) {
        Logger_log(LOGGER_PRIORITY_ERR, "AgentHandler_registerHandlerNocallback() called illegally\n");
        Assert_assert(reginfo != NULL);
        return PRIOT_ERR_GENERR;
    }
    DEBUG_IF("handler::register") {
        DEBUG_MSGTL(("handler::register",
                    "Registering (with no callback) "));
        for (handler = reginfo->handler; handler; handler = handler->next) {
            DEBUG_MSG(("handler::register", "::%s", handler->handler_name));
        }

        DEBUG_MSG(("handler::register", " at "));
        if (reginfo->rootoid && reginfo->range_subid) {
            DEBUG_MSGOIDRANGE(("handler::register", reginfo->rootoid,
                              reginfo->rootoid_len, reginfo->range_subid,
                              reginfo->range_ubound));
        } else if (reginfo->rootoid) {
            DEBUG_MSGOID(("handler::register", reginfo->rootoid,
                         reginfo->rootoid_len));
        } else {
            DEBUG_MSG(("handler::register", "[null]"));
        }
        DEBUG_MSG(("handler::register", "\n"));
    }

    /*
     * don't let them register for absolutely nothing.  Probably a mistake
     */
    if (0 == reginfo->modes) {
        reginfo->modes = HANDLER_CAN_DEFAULT;
    }

    return AgentRegistry_registerMib2( reginfo->handler->handler_name,
                                      NULL, 0, 0,
                                      reginfo->rootoid, reginfo->rootoid_len,
                                      reginfo->priority,
                                      reginfo->range_subid,
                                      reginfo->range_ubound, NULL,
                                      reginfo->contextName, reginfo->timeout, 0,
                                      reginfo, 0);
}

/** Injects handler into the calling chain of handlers.
 *  The given MIB handler is inserted after the handler named before_what.
 *  If before_what is NULL, the handler is put at the top of the list,
 *  and hence will be the handler to be called first.
 *
 *  @return Returns ErrorCode_SUCCESS or SNMP_ERR_* error code.
 *
 *  @see AgentHandler_createHandlerRegistration()
 *  @see AgentHandler_injectHandler()
 */
int
AgentHandler_injectHandlerBefore( HandlerRegistration *reginfo,
                                  MibHandler *handler,
                                  const char *before_what )
{
    MibHandler *handler2 = handler;

    if (handler == NULL || reginfo == NULL) {
        Logger_log(LOGGER_PRIORITY_ERR, "AgentHandler_injectHandler() called illegally\n");
        Assert_assert(reginfo != NULL);
        Assert_assert(handler != NULL);
        return PRIOT_ERR_GENERR;
    }
    while (handler2->next) {
        handler2 = handler2->next;  /* Find the end of a handler sub-chain */
    }
    if (reginfo->handler == NULL) {
        DEBUG_MSGTL(("handler:inject", "injecting %s\n", handler->handler_name));
    }
    else {
        DEBUG_MSGTL(("handler:inject", "injecting %s before %s\n",
                    handler->handler_name, reginfo->handler->handler_name));
    }
    if (before_what) {
        MibHandler *nexth, *prevh = NULL;
        if (reginfo->handler == NULL) {
            Logger_log(LOGGER_PRIORITY_ERR, "no handler to inject before\n");
            return PRIOT_ERR_GENERR;
        }
        for(nexth = reginfo->handler; nexth;
            prevh = nexth, nexth = nexth->next) {
            if (strcmp(nexth->handler_name, before_what) == 0)
                break;
        }
        if (!nexth)
            return PRIOT_ERR_GENERR;
        if (prevh) {
            /* after prevh and before nexth */
            prevh->next = handler;
            handler2->next = nexth;
            handler->prev = prevh;
            nexth->prev = handler2;
            return ErrorCode_SUCCESS;
        }
        /* else we're first, which is what we do next anyway so fall through */
    }
    handler2->next = reginfo->handler;
    if (reginfo->handler)
        reginfo->handler->prev = handler2;
    reginfo->handler = handler;
    return ErrorCode_SUCCESS;
}

/** Injects handler into the calling chain of handlers.
 *  The given MIB handler is put at the top of the list,
 *  and hence will be the handler to be called first.
 *
 *  @return Returns ErrorCode_SUCCESS or SNMP_ERR_* error code.
 *
 *  @see AgentHandler_createHandlerRegistration()
 *  @see AgentHandler_injectHandlerBefore()
 */
int
AgentHandler_injectHandler( HandlerRegistration *reginfo,
                            MibHandler *handler )
{
    return AgentHandler_injectHandlerBefore(reginfo, handler, NULL);
}

/** Calls a MIB handlers chain, starting with specific handler.
 *  The given arguments and MIB handler are checked
 *  for sanity, then the handlers are called, one by one,
 *  until next handler is NULL.
 *
 *  @return Returns ErrorCode_SUCCESS or SNMP_ERR_* error code.
 */
int
AgentHandler_callHandler( MibHandler*          next_handler,
                          HandlerRegistration* reginfo,
                          AgentRequestInfo*    reqinfo,
                          RequestInfo*         requests )
{
    NodeHandlerFT *nh;
    int             ret;

    if (next_handler == NULL || reginfo == NULL || reqinfo == NULL ||
        requests == NULL) {
        Logger_log(LOGGER_PRIORITY_ERR, "AgentHandler_callHandler() called illegally\n");
        Assert_assert(next_handler != NULL);
        Assert_assert(reqinfo != NULL);
        Assert_assert(reginfo != NULL);
        Assert_assert(requests != NULL);
        return PRIOT_ERR_GENERR;
    }

    do {
        nh = next_handler->access_method;
        if (!nh) {
            if (next_handler->next) {
                Logger_log(LOGGER_PRIORITY_ERR, "no access method specified in handler %s.",
                         next_handler->handler_name);
                return PRIOT_ERR_GENERR;
            }
            /*
             * The final handler registration in the chain may well not need
             * to include a handler routine, if the processing of this object
             * is handled completely by the agent toolkit helpers.
             */
            return PRIOT_ERR_NOERROR;
        }

        DEBUG_MSGTL(("handler:calling", "calling handler %s for mode %s\n",
                    next_handler->handler_name,
                    Enum_seFindLabelInSlist("agent_mode", reqinfo->mode)));

        /*
         * XXX: define acceptable return statuses
         */
        ret = (*nh) (next_handler, reginfo, reqinfo, requests);

        DEBUG_MSGTL(("handler:returned", "handler %s returned %d\n",
                    next_handler->handler_name, ret));

        if (! (next_handler->flags & MIB_HANDLER_AUTO_NEXT))
            break;

        /*
         * did handler signal that it didn't want auto next this time around?
         */
        if(next_handler->flags & MIB_HANDLER_AUTO_NEXT_OVERRIDE_ONCE) {
            next_handler->flags &= ~MIB_HANDLER_AUTO_NEXT_OVERRIDE_ONCE;
            break;
        }

        next_handler = next_handler->next;

    } while(next_handler);

    return ret;
}

/** @private
 *  Calls all the MIB Handlers in registration struct for a given mode.
 *
 *  @return Returns ErrorCode_SUCCESS or SNMP_ERR_* error code.
 */
int
AgentHandler_callHandlers(  HandlerRegistration* reginfo,
                            AgentRequestInfo*    reqinfo,
                            RequestInfo*         requests )
{
    RequestInfo *request;
    int             status;

    if (reginfo == NULL || reqinfo == NULL || requests == NULL) {
        Logger_log(LOGGER_PRIORITY_ERR, "AgentHandler_callHandlers() called illegally\n");
        Assert_assert(reqinfo != NULL);
        Assert_assert(reginfo != NULL);
        Assert_assert(requests != NULL);
        return PRIOT_ERR_GENERR;
    }

    if (reginfo->handler == NULL) {
        Logger_log(LOGGER_PRIORITY_ERR, "no handler specified.");
        return PRIOT_ERR_GENERR;
    }

    switch (reqinfo->mode) {
    case MODE_GETBULK:
    case MODE_GET:
    case MODE_GETNEXT:
        if (!(reginfo->modes & HANDLER_CAN_GETANDGETNEXT))
            return PRIOT_ERR_NOERROR;    /* legal */
        break;

    case MODE_SET_RESERVE1:
    case MODE_SET_RESERVE2:
    case MODE_SET_ACTION:
    case MODE_SET_COMMIT:
    case MODE_SET_FREE:
    case MODE_SET_UNDO:
        if (!(reginfo->modes & HANDLER_CAN_SET)) {
            for (; requests; requests = requests->next) {
                Agent_setRequestError(reqinfo, requests,
                                          PRIOT_ERR_NOTWRITABLE);
            }
            return PRIOT_ERR_NOERROR;
        }
        break;

    default:
        Logger_log(LOGGER_PRIORITY_ERR, "unknown mode in AgentHandler_callHandlers! bug!\n");
        return PRIOT_ERR_GENERR;
    }
    DEBUG_MSGTL(("handler:calling", "main handler %s\n",
                reginfo->handler->handler_name));

    for (request = requests ; request; request = request->next) {
        request->processed = 0;
    }

    status = AgentHandler_callHandler(reginfo->handler, reginfo, reqinfo, requests);

    return status;
}

/** @private
 *  Calls the next MIB handler in the chain, after the current one.
 *  The given arguments and MIB handler are checked
 *  for sanity, then the next handler is called.
 *
 *  @return Returns ErrorCode_SUCCESS or SNMP_ERR_* error code.
 */
int
AgentHandler_callNextHandler( MibHandler*          current,
                              HandlerRegistration* reginfo,
                              AgentRequestInfo*    reqinfo,
                              RequestInfo*         requests )
{

    if (current == NULL || reginfo == NULL || reqinfo == NULL ||
        requests == NULL) {
        Logger_log(LOGGER_PRIORITY_ERR, "AgentHandler_callNextHandler() called illegally\n");
        Assert_assert(current != NULL);
        Assert_assert(reginfo != NULL);
        Assert_assert(reqinfo != NULL);
        Assert_assert(requests != NULL);
        return PRIOT_ERR_GENERR;
    }

    return AgentHandler_callHandler(current->next, reginfo, reqinfo, requests);
}

/** @private
 *  Calls the next MIB handler in the chain, after the current one.
 *  The given arguments and MIB handler are not validated before
 *  the call, only request is checked.
 *
 *  @return Returns ErrorCode_SUCCESS or SNMP_ERR_* error code.
 */

int
AgentHandler_callNextHandlerOneRequest( MibHandler*          current,
                                        HandlerRegistration* reginfo,
                                        AgentRequestInfo*    reqinfo,
                                        RequestInfo*         requests )
{
    RequestInfo *request;
    int ret;

    if (!requests) {
        Logger_log(LOGGER_PRIORITY_ERR, "netsnmp_call_next_handler_ONE_REQUEST() called illegally\n");
        Assert_assert(requests != NULL);
        return PRIOT_ERR_GENERR;
    }

    request = requests->next;
    requests->next = NULL;
    ret = AgentHandler_callHandler(current->next, reginfo, reqinfo, requests);
    requests->next = request;
    return ret;
}

/** Deallocates resources associated with a given handler.
 *  The handler is removed from chain and then freed.
 *  After calling this function, the handler pointer is invalid
 *  and should be set to NULL.
 *
 *  @param handler is the MIB Handler to be freed
 */
void
AgentHandler_handlerFree( MibHandler* handler )
{
    if (handler != NULL) {
        if (handler->next != NULL) {
            /** make sure we aren't pointing to ourselves.  */
            Assert_assert(handler != handler->next); /* bugs caught: 1 */
            AgentHandler_handlerFree(handler->next);
            handler->next = NULL;
        }
        if ((handler->myvoid != NULL) && (handler->data_free != NULL))
        {
            handler->data_free(handler->myvoid);
        }
        TOOLS_FREE(handler->handler_name);
        TOOLS_FREE(handler);
    }
}

/** Duplicates a MIB handler and all subsequent handlers.
 *  Creates a copy of all data in given handlers chain.
 *
 *  @param handler is the MIB Handler to be duplicated
 *
 *  @return Returns a pointer to the complete copy,
 *         or NULL if any problem occured.
 *
 * @see AgentHandler_cloneHandler()
 */
MibHandler*
AgentHandler_handlerDup( MibHandler* handler )
{
    MibHandler *h = NULL;

    if (!handler)
        goto err;

    h = AgentHandler_cloneHandler(handler);
    if (!h)
        goto err;

    /*
     * Providing a clone function without a free function is asking for
     * memory leaks, and providing a free function without clone function
     * is asking for memory corruption. Hence the log statement below.
     */
    if (!!handler->data_clone != !!handler->data_free)
        Logger_log(LOGGER_PRIORITY_ERR, "data_clone / data_free inconsistent (%s)\n",
                 handler->handler_name);
    if (handler->myvoid && handler->data_clone) {
        h->myvoid = handler->data_clone(handler->myvoid);
        if (!h->myvoid)
            goto err;
    } else
        h->myvoid = handler->myvoid;
    h->data_clone = handler->data_clone;
    h->data_free = handler->data_free;

    if (handler->next != NULL) {
        h->next = AgentHandler_handlerDup(handler->next);
        if (!h->next)
            goto err;
        h->next->prev = h;
    }
    h->prev = NULL;
    return h;

err:
    AgentHandler_handlerFree(h);
    return NULL;
}

/** Free resources associated with a handler registration object.
 *  The registration object and all MIB Handlers in the chain are
 *  freed. When the function ends, given pointer is no longer valid
 *  and should be set to NULL.
 *
 *  @param reginfo is the handler registration object to be freed
 */
void
AgentHandler_handlerRegistrationFree( HandlerRegistration* reginfo )
{
    if (reginfo != NULL) {
        AgentHandler_handlerFree(reginfo->handler);
        TOOLS_FREE(reginfo->handlerName);
        TOOLS_FREE(reginfo->contextName);
        TOOLS_FREE(reginfo->rootoid);
        reginfo->rootoid_len = 0;
        TOOLS_FREE(reginfo);
    }
}

/** Duplicates handler registration object and all subsequent handlers.
 *  Creates a copy of the handler registration object and all its data.
 *
 *  @param reginfo is the handler registration object to be duplicated
 *
 *  @return Returns a pointer to the complete copy,
 *         or NULL if any problem occured.
 *
 * @see AgentHandler_handlerDup()
 */
HandlerRegistration*
AgentHandler_handlerRegistrationDup(HandlerRegistration* reginfo)
{
    HandlerRegistration *r = NULL;

    if (reginfo == NULL) {
        return NULL;
    }


    r = (HandlerRegistration *) calloc(1, sizeof(HandlerRegistration));

    if (r != NULL) {
        r->modes = reginfo->modes;
        r->priority = reginfo->priority;
        r->range_subid = reginfo->range_subid;
        r->timeout = reginfo->timeout;
        r->range_ubound = reginfo->range_ubound;
        r->rootoid_len = reginfo->rootoid_len;

        if (reginfo->handlerName != NULL) {
            r->handlerName = strdup(reginfo->handlerName);
            if (r->handlerName == NULL) {
                AgentHandler_handlerRegistrationFree(r);
                return NULL;
            }
        }

        if (reginfo->contextName != NULL) {
            r->contextName = strdup(reginfo->contextName);
            if (r->contextName == NULL) {
                AgentHandler_handlerRegistrationFree(r);
                return NULL;
            }
        }

        if (reginfo->rootoid != NULL) {
            r->rootoid = Api_duplicateObjid(reginfo->rootoid, reginfo->rootoid_len);
            if (r->rootoid == NULL) {
                AgentHandler_handlerRegistrationFree(r);
                return NULL;
            }
        }

        r->handler = AgentHandler_handlerDup(reginfo->handler);
        if (r->handler == NULL) {
            AgentHandler_handlerRegistrationFree(r);
            return NULL;
        }
        return r;
    }

    return NULL;
}

/** Creates a cache of information which can be saved for future reference.
 *  The cache is filled with pointers from parameters. Note that
 *  the input structures are not duplicated, but put directly into
 *  the new cache struct.
 *  Use AgentHandler_handlerCheckCache() later to make sure it's still
 *  valid before referencing it in the future.
 *
 * @see AgentHandler_handlerCheckCache()
 * @see AgentHandler_freeDelegatedCache()
 */
DelegatedCache*
AgentHandler_createDelegatedCache( MibHandler*          handler,
                                   HandlerRegistration* reginfo,
                                   AgentRequestInfo*    reqinfo,
                                   RequestInfo*         requests,
                                   void*                localinfo )
{
    DelegatedCache *ret;

    ret = TOOLS_MALLOC_TYPEDEF(DelegatedCache);
    if (ret) {
        ret->transaction_id = reqinfo->asp->pdu->transid;
        ret->handler = handler;
        ret->reginfo = reginfo;
        ret->reqinfo = reqinfo;
        ret->requests = requests;
        ret->localinfo = localinfo;
    }
    return ret;
}

/** Check if a given delegated cache is still valid.
 *  The cache is valid if it's a part of transaction
 *  (ie, the agent still considers it to be an outstanding request).
 *
 *  @param dcache is the delegated cache to be checked.
 *
 *  @return Returns the cache pointer if it is still valid, NULL otherwise.
 *
 * @see AgentHandler_createDelegatedCache()
 * @see AgentHandler_freeDelegatedCache()
 */
DelegatedCache*
AgentHandler_handlerCheckCache( DelegatedCache* dcache )
{
    if (!dcache)
        return dcache;

    if (Agent_checkTransactionId(dcache->transaction_id) == ErrorCode_SUCCESS)
        return dcache;

    return NULL;
}

/** Free a cache once it's no longer used.
 *  Deletes the data allocated by AgentHandler_createDelegatedCache().
 *  Structures which were not allocated by AgentHandler_createDelegatedCache()
 *  are not freed (pointers are dropped).
 *
 *  @param dcache is the delegated cache to be freed.
 *
 * @see AgentHandler_createDelegatedCache()
 * @see AgentHandler_handlerCheckCache()
 */
void
AgentHandler_freeDelegatedCache( DelegatedCache* dcache )
{
    /*
     * right now, no extra data is there that needs to be freed
     */
    if (dcache)
        TOOLS_FREE(dcache);

    return;
}


/** Sets a list of requests as delegated or not delegated.
 *  Sweeps through given chain of requests and sets 'delegated'
 *  flag accordingly to the isdelegaded parameter.
 *
 *  @param requests Request list.
 *  @param isdelegated New value of the 'delegated' flag.
 */
void
AgentHandler_handlerMarkRequestsAsDelegated( RequestInfo* requests,
                                             int          isdelegated )
{
    while (requests) {
        requests->delegated = isdelegated;
        requests = requests->next;
    }
}

/** Adds data from node list to the request information structure.
 *  Data in the request can be later extracted and used by submodules.
 *
 * @param request Destination request information structure.
 *
 * @param node The data to be added to the linked list
 *             request->parent_data.
 *
 * @see AgentHandler_requestRemoveListData()
 * @see AgentHandler_requestGetListData()
 */
void
AgentHandler_requestAddListData(    RequestInfo*       request,
                                    DataList_DataList* node )
{
    if (request) {
        if (request->parent_data)
            DataList_add(&request->parent_data, node);
        else
            request->parent_data = node;
    }
}

/** Removes all data from a request.
 *
 * @param request the netsnmp request info structure
 *
 * @param name this is the name of the previously added data
 *
 * @return 0 on successful find-and-delete, 1 otherwise.
 *
 * @see AgentHandler_requestAddListData()
 * @see AgentHandler_requestGetListData()
 */
int
AgentHandler_requestRemoveListData( RequestInfo*    request,
                                    const char*     name )
{
    if ((NULL == request) || (NULL ==request->parent_data))
        return 1;

    return DataList_removeNode(&request->parent_data, name);
}

/** Extracts data from a request.
 *  Retrieves data that was previously added to the request,
 *  usually by a parent module.
 *
 * @param request Source request information structure.
 *
 * @param name Used to compare against the request->parent_data->name value;
 *             if a match is found, request->parent_data->data is returned.
 *
 * @return Gives a void pointer(request->parent_data->data); NULL is
 *         returned if source data is NULL or the object is not found.
 *
 * @see AgentHandler_requestAddListData()
 * @see AgentHandler_requestRemoveListData()
 */
void*
AgentHandler_requestGetListData( RequestInfo* request,
                                 const char*  name )
{
    if (request)
        return DataList_get(request->parent_data, name);
    return NULL;
}

/** Free the extra data stored in a request.
 *  Deletes the data in given request only. Other chain items
 *  are unaffected.
 *
 * @param request Request information structure to be modified.
 *
 * @see AgentHandler_requestAddListData()
 * @see netsnmp_free_list_data()
 */
void
AgentHandler_freeRequestDataSet( RequestInfo* request )
{
    if (request)
        DataList_free(request->parent_data);
}

/** Free the extra data stored in a bunch of requests.
 *  Deletes all data in the chain inside request.
 *
 * @param request Request information structure to be modified.
 *
 * @see AgentHandler_requestAddListData()
 * @see AgentHandler_freeRequestDataSet()
 */
void
AgentHandler_freeRequestDataSets( RequestInfo* request )
{
    if (request && request->parent_data) {
        DataList_freeAll(request->parent_data);
        request->parent_data = NULL;
    }
}

/** Returns a MIB handler from a chain based on the name.
 *
 * @param reginfo Handler registration struct which contains the chain.
 *
 * @param name Target MIB Handler name string. The name is case
 *        sensitive.
 *
 * @return The MIB Handler is returned, or NULL if not found.
 *
 * @see AgentHandler_requestAddListData()
 */
MibHandler*
AgentHandler_findHandlerByName( HandlerRegistration* reginfo,
                                const char*          name )
{
    MibHandler *it;
    if (reginfo == NULL || name == NULL )
        return NULL;
    for (it = reginfo->handler; it; it = it->next) {
        if (strcmp(it->handler_name, name) == 0) {
            return it;
        }
    }
    return NULL;
}

/** Returns a handler's void pointer from a chain based on the name.
 *
 * @warning The void pointer data may change as a handler evolves.
 *        Handlers should really advertise some function for you
 *        to use instead.
 *
 * @param reginfo Handler registration struct which contains the chain.
 *
 * @param name Target MIB Handler name string. The name is case
 *        sensitive.
 *
 * @return The MIB Handler's void * pointer is returned,
 *        or NULL if the handler is not found.
 *
 * @see AgentHandler_findHandlerByName()
 */
void*
AgentHandler_findHandlerDataByName( HandlerRegistration* reginfo,
                                    const char*          name )
{
    MibHandler *it = AgentHandler_findHandlerByName(reginfo, name);
    if (it)
        return it->myvoid;
    return NULL;
}

/** @private
 *  Clones a MIB Handler with its basic properties.
 *  Creates a copy of the given MIB Handler. Copies name, flags and
 *  access methods only; not myvoid.
 *
 *  @see AgentHandler_handlerDup()
 */
static MibHandler*
AgentHandler_cloneHandler( MibHandler* it )
{
    MibHandler *dup;

    if(NULL == it)
        return NULL;

    dup = AgentHandler_createHandler(it->handler_name, it->access_method);
    if(NULL != dup)
        dup->flags = it->flags;

    return dup;
}

static DataList_DataList *agentHandler_handlerReg = NULL;

void
AgentHandler_handlerFreeCallback( void* handler )
{
    AgentHandler_handlerFree((MibHandler *)handler);
}

/** Registers a given handler by name, so that it can be found easily later.
 *  Pointer to the handler is put into a list where it can be easily located
 *  at any time.
 *
 * @param name Name string to be associated with the handler.
 *
 * @param handler Pointer the MIB Handler.
 *
 * @see AgentHandler_clearHandlerList()
 */
void
AgentHandler_registerHandlerByName( const char* name,
                                    MibHandler* handler )
{
    DataList_add(&agentHandler_handlerReg,
                          DataList_create(name, (void *) handler,
                                                   AgentHandler_handlerFreeCallback));
    DEBUG_MSGTL(("handler_registry", "registering helper %s\n", name));
}

/** Clears the entire MIB Handlers registration list.
 *  MIB Handlers registration list is used to access any MIB Handler by
 *  its name. The function frees the list memory and sets pointer to NULL.
 *  Instead of calling this function directly, use shutdown_agent().
 *
 *  @see shutdown_agent()
 *  @see AgentHandler_registerHandlerByName()
 */
void
AgentHandler_clearHandlerList( void )
{
    DEBUG_MSGTL(("agent_handler", "AgentHandler_clearHandlerList() called\n"));
    DataList_freeAll(agentHandler_handlerReg);
    agentHandler_handlerReg = NULL;
}

/** @private
 *  Injects a handler into a subtree, peers and children when a given
 *  subtrees name matches a passed in name.
 */
void
AgentHandler_injectHandlerIntoSubtree( Subtree*    tp,
                                       const char* name,
                                       MibHandler* handler,
                                       const char* before_what )
{
    Subtree *tptr;
    MibHandler *mh;

    for (tptr = tp; tptr != NULL; tptr = tptr->next) {
        /*  if (tptr->children) {
              AgentHandler_injectHandlerIntoSubtree(tptr->children,name,handler);
        }   */
        if (strcmp(tptr->label_a, name) == 0) {
            DEBUG_MSGTL(("injectHandler", "injecting handler %s into %s\n",
                        handler->handler_name, tptr->label_a));
            AgentHandler_injectHandlerBefore(tptr->reginfo, AgentHandler_cloneHandler(handler),
                                          before_what);
        } else if (tptr->reginfo != NULL &&
           tptr->reginfo->handlerName != NULL &&
                   strcmp(tptr->reginfo->handlerName, name) == 0) {
            DEBUG_MSGTL(("injectHandler", "injecting handler into %s/%s\n",
                        tptr->label_a, tptr->reginfo->handlerName));
            AgentHandler_injectHandlerBefore(tptr->reginfo, AgentHandler_cloneHandler(handler),
                                          before_what);
        } else {
            for (mh = tptr->reginfo->handler; mh != NULL; mh = mh->next) {
                if (mh->handler_name && strcmp(mh->handler_name, name) == 0) {
                    DEBUG_MSGTL(("injectHandler", "injecting handler into %s\n",
                                tptr->label_a));
                    AgentHandler_injectHandlerBefore(tptr->reginfo,
                                                  AgentHandler_cloneHandler(handler),
                                                  before_what);
                    break;
                } else {
                    DEBUG_MSGTL(("injectHandler",
                                "not injecting handler into %s\n",
                                mh->handler_name));
                }
            }
        }
    }
}

static int agentHandler_doneit = 0;
/** @private
 *  Parses the "injectHandler" token line.
 */

void
AgentHandler_parseInjectHandlerConf( const char* token,
                                     char*       cptr )
{
    char  handler_to_insert[256], reg_name[256];
    SubtreeContextCache *stc;
    MibHandler *handler;

    /*
     * XXXWWW: ensure instead that handler isn't inserted twice
     */
    if (agentHandler_doneit)                 /* we only do this once without restart the agent */
        return;

    cptr = ReadConfig_copyNword(cptr, handler_to_insert, sizeof(handler_to_insert));
    handler = (MibHandler*)DataList_get(agentHandler_handlerReg, handler_to_insert);
    if (!handler) {
    ReadConfig_error("no \"%s\" handler registered.",
                 handler_to_insert);
        return;
    }

    if (!cptr) {
        ReadConfig_configPerror("no INTONAME specified.  Can't do insertion.");
        return;
    }
    cptr = ReadConfig_copyNword(cptr, reg_name, sizeof(reg_name));

    for (stc = AgentRegistry_getTopContextCache(); stc; stc = stc->next) {
        DEBUG_MSGTL(("injectHandler", "Checking context tree %s (before=%s)\n",
                    stc->context_name, (cptr)?cptr:"null"));
        AgentHandler_injectHandlerIntoSubtree(stc->first_subtree, reg_name,
                                            handler, cptr);
    }
}

/** @private
 *  Callback to ensure injectHandler parser doesn't do things twice.
 *  @todo replace this with a method to check the handler chain instead.
 */
static int
AgentHandler_handlerMarkInjectHandlerDone( int   majorID,
                                           int   minorID,
                                           void* serverarg,
                                           void* clientarg )
{
    agentHandler_doneit = 1;
    return 0;
}

/** @private
 *  Registers the injectHandle parser token.
 *  Used in init_agent_read_config().
 *
 *  @see init_agent_read_config()
 */
void
AgentHandler_initHandlerConf( void )
{
    AgentReadConfig_priotdRegisterConfigHandler("injectHandler",
                                  AgentHandler_parseInjectHandlerConf,
                                  NULL, "injectHandler NAME INTONAME [BEFORE_OTHER_NAME]");

    Callback_registerCallback(CALLBACK_LIBRARY,
                           CALLBACK_POST_READ_CONFIG,
                           AgentHandler_handlerMarkInjectHandlerDone, NULL);

    Enum_seAddPairToSlist("agent_mode", strdup("GET"), MODE_GET);
    Enum_seAddPairToSlist("agent_mode", strdup("GETNEXT"), MODE_GETNEXT);
    Enum_seAddPairToSlist("agent_mode", strdup("GETBULK"), MODE_GETBULK);
    Enum_seAddPairToSlist("agent_mode", strdup("SET_BEGIN"),
                         MODE_SET_BEGIN);
    Enum_seAddPairToSlist("agent_mode", strdup("SET_RESERVE1"),
                         MODE_SET_RESERVE1);
    Enum_seAddPairToSlist("agent_mode", strdup("SET_RESERVE2"),
                         MODE_SET_RESERVE2);
    Enum_seAddPairToSlist("agent_mode", strdup("SET_ACTION"),
                         MODE_SET_ACTION);
    Enum_seAddPairToSlist("agent_mode", strdup("SET_COMMIT"),
                         MODE_SET_COMMIT);
    Enum_seAddPairToSlist("agent_mode", strdup("SET_FREE"), MODE_SET_FREE);
    Enum_seAddPairToSlist("agent_mode", strdup("SET_UNDO"), MODE_SET_UNDO);

    Enum_seAddPairToSlist("babystep_mode", strdup("pre-request"),
                         MODE_BSTEP_PRE_REQUEST);
    Enum_seAddPairToSlist("babystep_mode", strdup("object_lookup"),
                         MODE_BSTEP_OBJECT_LOOKUP);
    Enum_seAddPairToSlist("babystep_mode", strdup("check_value"),
                         MODE_BSTEP_CHECK_VALUE);
    Enum_seAddPairToSlist("babystep_mode", strdup("row_create"),
                         MODE_BSTEP_ROW_CREATE);
    Enum_seAddPairToSlist("babystep_mode", strdup("undo_setup"),
                         MODE_BSTEP_UNDO_SETUP);
    Enum_seAddPairToSlist("babystep_mode", strdup("set_value"),
                         MODE_BSTEP_SET_VALUE);
    Enum_seAddPairToSlist("babystep_mode", strdup("check_consistency"),
                         MODE_BSTEP_CHECK_CONSISTENCY);
    Enum_seAddPairToSlist("babystep_mode", strdup("undo_set"),
                         MODE_BSTEP_UNDO_SET);
    Enum_seAddPairToSlist("babystep_mode", strdup("commit"),
                         MODE_BSTEP_COMMIT);
    Enum_seAddPairToSlist("babystep_mode", strdup("undo_commit"),
                         MODE_BSTEP_UNDO_COMMIT);
    Enum_seAddPairToSlist("babystep_mode", strdup("irreversible_commit"),
                         MODE_BSTEP_IRREVERSIBLE_COMMIT);
    Enum_seAddPairToSlist("babystep_mode", strdup("undo_cleanup"),
                         MODE_BSTEP_UNDO_CLEANUP);
    Enum_seAddPairToSlist("babystep_mode", strdup("post_request"),
                         MODE_BSTEP_POST_REQUEST);
    Enum_seAddPairToSlist("babystep_mode", strdup("original"), 0xffff);
    /*
     * xxx-rks: hmmm.. will this work for modes which are or'd together?
     *          I'm betting not...
     */
    Enum_seAddPairToSlist( "handler_can_mode",
                           strdup("GET/GETNEXT"),
                           HANDLER_CAN_GETANDGETNEXT );

    Enum_seAddPairToSlist("handler_can_mode", strdup("SET"),
                         HANDLER_CAN_SET);
    Enum_seAddPairToSlist("handler_can_mode", strdup("GETBULK"),
                         HANDLER_CAN_GETBULK);
    Enum_seAddPairToSlist("handler_can_mode", strdup("BABY_STEP"),
                         HANDLER_CAN_BABY_STEP);
}
