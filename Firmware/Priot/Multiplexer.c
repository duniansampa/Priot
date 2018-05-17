#include "Multiplexer.h"
#include "DefaultStore.h"
#include "System/Util/Logger.h"

/** @defgroup multiplexer multiplexer
 *  Splits mode requests into calls to different handlers.
 *  @ingroup utilities
 * The multiplexer helper lets you split the calling chain depending
 * on the calling mode (get vs getnext vs set).  Useful if you want
 * different routines to handle different aspects of SNMP requests,
 * which is very common for GET vs SET type actions.
 *
 * Functionally:
 *
 * -# GET requests call the get_method
 * -# GETNEXT requests call the getnext_method, or if not present, the
 *    get_method.
 * -# GETBULK requests call the getbulk_method, or if not present, the
 *    getnext_method, or if even that isn't present the get_method.
 * -# SET requests call the set_method, or if not present return a
 *    PRIOT_ERR_NOTWRITABLE error.
 *
 */

/** returns a multiplixer handler given a MibHandlerMethods structure of subhandlers.
 */
MibHandler*
Multiplexer_getMultiplexerHandler( MibHandlerMethods* req )
{
    MibHandler *ret = NULL;

    if (!req) {
        Logger_log(LOGGER_PRIORITY_INFO, "Multiplexer_getMultiplexerHandler(NULL) called\n");
        return NULL;
    }

    ret =
        AgentHandler_createHandler("multiplexer",
                               Multiplexer_helperHandler);
    if (ret) {
        ret->myvoid = (void *) req;
    }
    return ret;
}

/** implements the multiplexer helper */
int
Multiplexer_helperHandler( MibHandler *handler,
                           HandlerRegistration *reginfo,
                           AgentRequestInfo *reqinfo,
                           RequestInfo *requests )
{

    MibHandlerMethods *methods;

    if (!handler->myvoid) {
        Logger_log(LOGGER_PRIORITY_INFO, "improperly registered multiplexer found\n");
        return PRIOT_ERR_GENERR;
    }

    methods = (MibHandlerMethods *) handler->myvoid;

    switch (reqinfo->mode) {
    case MODE_GETBULK:
        handler = methods->getbulk_handler;
        if (handler)
            break;
        /* Deliberate fallthrough to use GetNext handler */
    case MODE_GETNEXT:
        handler = methods->getnext_handler;
        if (handler)
            break;
        /* Deliberate fallthrough to use Get handler */
    case MODE_GET:
        handler = methods->get_handler;
        if (!handler) {
            Agent_requestSetErrorAll(requests, PRIOT_NOSUCHOBJECT);
        }
        break;

    case MODE_SET_RESERVE1:
    case MODE_SET_RESERVE2:
    case MODE_SET_ACTION:
    case MODE_SET_COMMIT:
    case MODE_SET_FREE:
    case MODE_SET_UNDO:
        handler = methods->set_handler;
        if (!handler) {
            Agent_requestSetErrorAll(requests, PRIOT_ERR_NOTWRITABLE);
            return PRIOT_ERR_NOERROR;
        }
        break;

        /*
         * XXX: process SETs specially, and possibly others
         */
    default:
        Logger_log(LOGGER_PRIORITY_ERR, "unsupported mode for multiplexer: %d\n",
                 reqinfo->mode);
        return PRIOT_ERR_GENERR;
    }
    if (!handler) {
        Logger_log(LOGGER_PRIORITY_ERR,
                 "No handler enabled for mode %d in multiplexer\n",
                 reqinfo->mode);
        return PRIOT_ERR_GENERR;
    }
    return AgentHandler_callHandler(handler, reginfo, reqinfo, requests);
}

