#include "StashCache.h"
#include "PriotSettings.h"
#include "CacheHandler.h"
#include "System/Util/Utilities.h"
#include "System/Util/Trace.h"
#include "Client.h"
#include "StashToNext.h"

CacheLoadFT StashCache_load;
CacheFreeFT StashCache_free;

/** @defgroup stash_cache stash_cache
 *  Automatically caches data for certain handlers.
 *  This handler caches data in an optimized way which may alleviate
 *  the need for the lower level handlers to perform as much
 *  optimization.  Specifically, somewhere in the lower level handlers
 *  must be a handler that supports the MODE_GET_STASH operation.
 *  Note that the table_iterator helper supports this.
 *  @ingroup handler
 *  @{
 */

StashCacheInfo *
StashCache_getNewStashCache(void)
{
    StashCacheInfo *cinfo;

    cinfo = MEMORY_MALLOC_TYPEDEF(StashCacheInfo);
    if (cinfo != NULL)
        cinfo->cache_length = 30;
    return cinfo;
}

/** returns a stash_cache handler that can be injected into a given
 *  handler chain (with the specified timeout and root OID values),
 *  but *only* if that handler chain explicitly supports stash cache processing.
 */
MibHandler *
StashCache_getTimedBareStashCacheHandler(int timeout, oid *rootoid, size_t rootoid_len)
{
    MibHandler *handler;
    Cache       *cinfo;

    cinfo = CacheHandler_create( timeout, StashCache_load,
                                 StashCache_free, rootoid, rootoid_len );

    if (!cinfo)
        return NULL;

    handler = CacheHandler_handlerGet( cinfo );
    if (!handler) {
        free(cinfo);
        return NULL;
    }

    handler->next = AgentHandler_createHandler("stashCache", StashCache_helper);
    if (!handler->next) {
        AgentHandler_handlerFree(handler);
        free(cinfo);
        return NULL;
    }

    handler->myvoid = cinfo;
    CacheHandler_handlerOwnsCache(handler);

    return handler;
}

/** returns a single stash_cache handler that can be injected into a given
 *  handler chain (with a fixed timeout), but *only* if that handler chain
 *  explicitly supports stash cache processing.
 */
MibHandler *
StashCache_getBareStashCacheHandler(void)
{
    return StashCache_getTimedBareStashCacheHandler( 30, NULL, 0 );
}

/** returns a stash_cache handler sub-chain that can be injected into a given
 *  (arbitrary) handler chain, using a fixed cache timeout.
 */
MibHandler *
StashCache_getStashCacheHandler(void)
{
    MibHandler *handler = StashCache_getBareStashCacheHandler();
    if (handler && handler->next) {
        handler->next->next = StashToNext_getStashToNextHandler();
    }
    return handler;
}

/** returns a stash_cache handler sub-chain that can be injected into a given
 *  (arbitrary) handler chain, using a configurable cache timeout.
 */
MibHandler *
StashCache_getTimedStashCacheHandler(int timeout, oid *rootoid, size_t rootoid_len)
{
    MibHandler *handler =
       StashCache_getTimedBareStashCacheHandler(timeout, rootoid, rootoid_len);
    if (handler && handler->next) {
        handler->next->next = StashToNext_getStashToNextHandler();
    }
    return handler;
}

/** extracts a pointer to the stash_cache info from the reqinfo structure. */
OidStash_Node  **
StashCache_extractStashCache(AgentRequestInfo *reqinfo)
{
    return (OidStash_Node**)Agent_getListData(reqinfo, STASH_CACHE_NAME);
}


/** @internal Implements the stash_cache handler */
int
StashCache_helper( MibHandler *handler,
                   HandlerRegistration *reginfo,
                   AgentRequestInfo *reqinfo,
                   RequestInfo *requests)
{
    Cache            *cache;
    StashCacheInfo *cinfo;
    OidStash_Node   *cnode;
    VariableList    *cdata;
    RequestInfo     *request;

    DEBUG_MSGTL(("helper:stash_cache", "Got request\n"));

    cache = CacheHandler_reqinfoExtract( reqinfo, reginfo->handlerName );
    if (!cache) {
        DEBUG_MSGTL(("helper:stash_cache", "No cache structure\n"));
        return ErrorCode_GENERR;
    }
    cinfo = (StashCacheInfo *) cache->magic;

    switch (reqinfo->mode) {

    case MODE_GET:
        DEBUG_MSGTL(("helper:stash_cache", "Processing GET request\n"));
        for(request = requests; request; request = request->next) {
            cdata = (VariableList*)
                OidStash_getData(cinfo->cache,
                                           requests->requestvb->name,
                                           requests->requestvb->nameLength);
            if (cdata && cdata->value.string && cdata->valueLength) {
                DEBUG_MSGTL(("helper:stash_cache", "Found cached GET varbind\n"));
                DEBUG_MSGOID(("helper:stash_cache", cdata->name, cdata->nameLength));
                DEBUG_MSG(("helper:stash_cache", "\n"));
                Client_setVarTypedValue(request->requestvb, cdata->type,
                                         cdata->value.string, cdata->valueLength);
            }
        }
        break;

    case MODE_GETNEXT:
        DEBUG_MSGTL(("helper:stash_cache", "Processing GETNEXT request\n"));
        for(request = requests; request; request = request->next) {
            cnode =
                OidStash_getnextNode(cinfo->cache,
                                               requests->requestvb->name,
                                               requests->requestvb->nameLength);
            if (cnode && cnode->thedata) {
                cdata =  (VariableList*)cnode->thedata;
                if (cdata->value.string && cdata->name && cdata->nameLength) {
                    DEBUG_MSGTL(("helper:stash_cache", "Found cached GETNEXT varbind\n"));
                    DEBUG_MSGOID(("helper:stash_cache", cdata->name, cdata->nameLength));
                    DEBUG_MSG(("helper:stash_cache", "\n"));
                    Client_setVarTypedValue(request->requestvb, cdata->type,
                                             cdata->value.string, cdata->valueLength);
                    Client_setVarObjid(request->requestvb, cdata->name,
                                       cdata->nameLength);
                }
            }
        }
        break;

    default:
        cinfo->cache_valid = 0;
        return AgentHandler_callNextHandler(handler, reginfo, reqinfo,
                                         requests);
    }

    return PRIOT_ERR_NOERROR;
}

/** updates a given cache depending on whether it needs to or not.
 */
int
StashCache_load( Cache *cache, void *magic )
{
    MibHandler          *handler  = cache->cache_hint->handler;
    HandlerRegistration *reginfo  = cache->cache_hint->reginfo;
    AgentRequestInfo   *reqinfo  = cache->cache_hint->reqinfo;
    RequestInfo         *requests = cache->cache_hint->requests;
    StashCacheInfo     *cinfo    = (StashCacheInfo*) magic;
    int old_mode;
    int ret;

    if (!cinfo) {
        cinfo = StashCache_getNewStashCache();
        cache->magic = cinfo;
    }

    /* change modes to the GET_STASH mode */
    old_mode = reqinfo->mode;
    reqinfo->mode = MODE_GET_STASH;
    Agent_addListData(reqinfo,
                                Map_newElement(STASH_CACHE_NAME,
                                                         &cinfo->cache, NULL));

    /* have the next handler fill stuff in and switch modes back */
    ret = AgentHandler_callNextHandler(handler->next, reginfo, reqinfo, requests);
    reqinfo->mode = old_mode;

    return ret;
}

void
StashCache_free( Cache *cache, void *magic )
{
    StashCacheInfo *cinfo = (StashCacheInfo*) magic;
    OidStash_free(&cinfo->cache,
                  (OidStash_FuncStashFreeNode *) Api_freeVar);
    return;
}

/** initializes the stash_cache helper which then registers a stash_cache
 *  handler as a run-time injectable handler for configuration file
 *  use.
 */
void
StashCache_initStashCacheHelper(void)
{
    AgentHandler_registerHandlerByName("stashCache",
                                     StashCache_getStashCacheHandler());
}
/**  @} */
