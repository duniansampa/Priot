#include "Watcher.h"
#include "System/Util/Debug.h"
#include "System/Util/Logger.h"
#include "Instance.h"
#include "Scalar.h"
#include "System/Util/Assert.h"
#include "Client.h"

/** @defgroup watcher watcher
 *  Watch a specified variable and process it as an instance or scalar object
 *  @ingroup leaf
 *  @{
 */

MibHandler *
Watcher_getWatcherHandler(void)
{
    MibHandler *ret = NULL;

    ret = AgentHandler_createHandler("watcher",
                                 Watcher_helperHandler);
    if (ret) {
        ret->flags |= MIB_HANDLER_AUTO_NEXT;
    }
    return ret;
}

WatcherInfo *
Watcher_initWatcherInfo6(WatcherInfo *winfo,
                           void *data, size_t size, u_char type,
                           int flags, size_t max_size, size_t* size_p)
{
    winfo->data = data;
    winfo->data_size = size;
    winfo->max_size = max_size;
    winfo->type = type;
    winfo->flags = flags;
    winfo->data_size_p = size_p;
    return winfo;
}

WatcherInfo *
Watcher_createWatcherInfo6(void *data, size_t size, u_char type,
                             int flags, size_t max_size, size_t* size_p)
{
    WatcherInfo *winfo = MEMORY_MALLOC_TYPEDEF(WatcherInfo);
    if (winfo)
        Watcher_initWatcherInfo6(winfo, data, size, type, flags, max_size,
                                   size_p);
    return winfo;
}

WatcherInfo *
Watcher_initWatcherInfo(WatcherInfo *winfo,
                          void *data, size_t size, u_char type, int flags)
{
  return Watcher_initWatcherInfo6(winfo, data, size,
                    type, (flags ? flags : WATCHER_FIXED_SIZE),
                    size,  /* Probably wrong for non-fixed
                        * size data */
                    NULL);
}

WatcherInfo *
Watcher_createWatcherInfo(void *data, size_t size, u_char type, int flags)
{
    WatcherInfo *winfo = MEMORY_MALLOC_TYPEDEF(WatcherInfo);
    if (winfo)
        Watcher_initWatcherInfo(winfo, data, size, type, flags);
    return winfo;
}

/**
 * Register a watched scalar. The caller remains the owner of watchinfo.
 *
 * @see Watcher_registerWatchedInstance2()
 */
int
Watcher_registerWatchedInstance(HandlerRegistration *reginfo,
                                  WatcherInfo         *watchinfo)
{
    MibHandler *whandler;

    whandler         = Watcher_getWatcherHandler();
    whandler->myvoid = (void *)watchinfo;

    AgentHandler_injectHandler(reginfo, whandler);
    return Instance_registerInstance(reginfo);
}

/**
 * Register a watched scalar. Ownership of watchinfo is transferred to the handler.
 *
 * @see Watcher_registerWatchedInstance()
 */
int
Watcher_registerWatchedInstance2(HandlerRegistration *reginfo,
                   WatcherInfo         *watchinfo)
{
    MibHandler *whandler;

    whandler         = Watcher_getWatcherHandler();
    whandler->myvoid = (void *)watchinfo;
    Watcher_ownsWatcherInfo(whandler);

    AgentHandler_injectHandler(reginfo, whandler);
    return Instance_registerInstance(reginfo);
}

/**
 * Register a watched scalar. The caller remains the owner of watchinfo.
 *
 * @see Watcher_registerWatchedScalar2()
 */
int
Watcher_registerWatchedScalar(HandlerRegistration *reginfo,
                                  WatcherInfo         *watchinfo)
{
    MibHandler *whandler;

    whandler         = Watcher_getWatcherHandler();
    whandler->myvoid = (void *)watchinfo;

    AgentHandler_injectHandler(reginfo, whandler);
    return Scalar_registerScalar(reginfo);
}

/**
 * Register a watched scalar. Ownership of watchinfo is transferred to the handler.
 *
 * @see Watcher_registerWatchedScalar()
 */
int
Watcher_registerWatchedScalar2(HandlerRegistration *reginfo,
                                  WatcherInfo         *watchinfo)
{
    MibHandler *whandler;

    whandler         = Watcher_getWatcherHandler();
    whandler->myvoid = (void *)watchinfo;
    Watcher_ownsWatcherInfo(whandler);

    AgentHandler_injectHandler(reginfo, whandler);
    return Scalar_registerScalar(reginfo);
}

void
Watcher_ownsWatcherInfo(MibHandler *handler)
{
    Assert_assert(handler);
    Assert_assert(handler->myvoid);
    handler->data_clone = (void *(*)(void *))Watcher_cloneWatcherInfo;
    handler->data_free = free;
}

/** @cond */

static inline size_t
Watcher_getDataSize(const WatcherInfo* winfo)
{
    if (winfo->flags & WATCHER_SIZE_STRLEN)
        return strlen((const char*)winfo->data);
    else {
        size_t res;
        if (winfo->flags & WATCHER_SIZE_IS_PTR)
            res = *winfo->data_size_p;
        else
            res = winfo->data_size;
        if (winfo->flags & WATCHER_SIZE_UNIT_OIDS)
          res *= sizeof(oid);
        return res;
    }
}

static inline void
Watcher_setData(WatcherInfo* winfo, void* data, size_t size)
{
    memcpy(winfo->data, data, size);
    if (winfo->flags & WATCHER_SIZE_STRLEN)
        ((char*)winfo->data)[size] = '\0';
    else {
        if (winfo->flags & WATCHER_SIZE_UNIT_OIDS)
          size /= sizeof(oid);
        if (winfo->flags & WATCHER_SIZE_IS_PTR)
            *winfo->data_size_p = size;
        else
            winfo->data_size = size;
    }
}

typedef struct {
    size_t size;
    char data[1];
} WatcherCache;

static inline WatcherCache*
Watcher_cacheCreate(const void* data, size_t size)
{
    WatcherCache *res = (WatcherCache*)
        malloc(sizeof(WatcherCache) + size - 1);
    if (res) {
        res->size = size;
        memcpy(res->data, data, size);
    }
    return res;
}

/** @endcond */

int
Watcher_helperHandler( MibHandler *handler,
                       HandlerRegistration *reginfo,
                       AgentRequestInfo *reqinfo,
                       RequestInfo *requests)
{
    WatcherInfo  *winfo = (WatcherInfo *) handler->myvoid;
    WatcherCache *old_data;

    DEBUG_MSGTL(("helper:watcher", "Got request:  %d\n", reqinfo->mode));
    DEBUG_MSGTL(( "helper:watcher", "  oid:"));
    DEBUG_MSGOID(("helper:watcher", requests->requestvb->name,
                                   requests->requestvb->nameLength));
    DEBUG_MSG((   "helper:watcher", "\n"));

    switch (reqinfo->mode) {
        /*
         * data requests
         */
    case MODE_GET:
        Client_setVarTypedValue(requests->requestvb,
                                 winfo->type,
                                 winfo->data,
                                 Watcher_getDataSize(winfo));
        break;

        /*
         * SET requests.  Should only get here if registered RWRITE
         */
    case MODE_SET_RESERVE1:
        if (requests->requestvb->type != winfo->type) {
            Agent_setRequestError(reqinfo, requests, PRIOT_ERR_WRONGTYPE);
            handler->flags |= MIB_HANDLER_AUTO_NEXT_OVERRIDE_ONCE;
        } else if (((winfo->flags & WATCHER_MAX_SIZE) &&
                     requests->requestvb->valLen > winfo->max_size) ||
            ((winfo->flags & WATCHER_FIXED_SIZE) &&
                requests->requestvb->valLen != Watcher_getDataSize(winfo))) {
            Agent_setRequestError(reqinfo, requests, PRIOT_ERR_WRONGLENGTH);
            handler->flags |= MIB_HANDLER_AUTO_NEXT_OVERRIDE_ONCE;
        } else if ((winfo->flags & WATCHER_SIZE_STRLEN) &&
            (memchr(requests->requestvb->val.string, '\0',
                requests->requestvb->valLen) != NULL)) {
            Agent_setRequestError(reqinfo, requests, PRIOT_ERR_WRONGVALUE);
            handler->flags |= MIB_HANDLER_AUTO_NEXT_OVERRIDE_ONCE;
        }
        break;

    case MODE_SET_RESERVE2:
        /*
         * store old info for undo later
         */
        old_data =
            Watcher_cacheCreate(winfo->data, Watcher_getDataSize(winfo));
        if (old_data == NULL) {
            Agent_setRequestError(reqinfo, requests,
                                      PRIOT_ERR_RESOURCEUNAVAILABLE);
            handler->flags |= MIB_HANDLER_AUTO_NEXT_OVERRIDE_ONCE;
        } else
            AgentHandler_requestAddListData(requests,
                                          Map_newElement
                                          ("watcher", old_data, &free));
        break;

    case MODE_SET_FREE:
        /*
         * nothing to do
         */
        break;

    case MODE_SET_ACTION:
        /*
         * update current
         */
        Watcher_setData(winfo, (void *)requests->requestvb->val.string,
                                requests->requestvb->valLen);
        break;

    case MODE_SET_UNDO:
        old_data = (WatcherCache*)AgentHandler_requestGetListData(requests, "watcher");
        Watcher_setData(winfo, old_data->data, old_data->size);
        break;

    case MODE_SET_COMMIT:
        break;

    default:
        Logger_log(LOGGER_PRIORITY_ERR, "watcher handler called with an unknown mode: %d\n",
                 reqinfo->mode);
        return PRIOT_ERR_GENERR;

    }

    /* next handler called automatically - 'AUTO_NEXT' */
    return PRIOT_ERR_NOERROR;
}


    /***************************
     *
     * A specialised form of the above, reporting
     *   the sysUpTime indicated by a given timestamp
     *
     ***************************/

MibHandler *
Watcher_getWatchedTimestampHandler(void)
{
    MibHandler *ret = NULL;

    ret = AgentHandler_createHandler("watcherTimestamp",
                                 Watcher_timestampHandler);
    if (ret) {
        ret->flags |= MIB_HANDLER_AUTO_NEXT;
    }
    return ret;
}

int
Watcher_watchedTimestampRegister(MibHandler *whandler,
                                   HandlerRegistration *reginfo,
                                   timeMarker timestamp)
{
    whandler->myvoid = (void *)timestamp;
    AgentHandler_injectHandler(reginfo, whandler);
    return Scalar_registerScalar(reginfo);   /* XXX - or instance? */
}

int
Watcher_registerWatchedTimestamp(HandlerRegistration *reginfo,
                                   timeMarker timestamp)
{
    MibHandler *whandler;

    whandler         = Watcher_getWatchedTimestampHandler();

    return Watcher_watchedTimestampRegister(whandler, reginfo, timestamp);
}


int
Watcher_timestampHandler( MibHandler *handler,
                          HandlerRegistration *reginfo,
                          AgentRequestInfo *reqinfo,
                          RequestInfo *requests)
{
    timeMarker timestamp = (timeMarker) handler->myvoid;
    long     uptime;

    DEBUG_MSGTL(("helper:watcher:timestamp",
                               "Got request:  %d\n", reqinfo->mode));
    DEBUG_MSGTL(( "helper:watcher:timestamp", "  oid:"));
    DEBUG_MSGOID(("helper:watcher:timestamp", requests->requestvb->name,
                                   requests->requestvb->nameLength));
    DEBUG_MSG((   "helper:watcher:timestamp", "\n"));

    switch (reqinfo->mode) {
        /*
         * data requests
         */
    case MODE_GET:
        if (handler->flags & NETSNMP_WATCHER_DIRECT)
            uptime = * (long*)timestamp;
        else
            uptime = Agent_markerUptime( timestamp );
        Client_setVarTypedValue(requests->requestvb,
                                 ASN01_TIMETICKS,
                                 (u_char *) &uptime,
                                 sizeof(uptime));
        break;

        /*
         * Timestamps are inherently Read-Only,
         *  so don't need to support SET requests.
         */
    case MODE_SET_RESERVE1:
        Agent_setRequestError(reqinfo, requests,
                                  PRIOT_ERR_NOTWRITABLE);
        handler->flags |= MIB_HANDLER_AUTO_NEXT_OVERRIDE_ONCE;
        return PRIOT_ERR_NOTWRITABLE;
    }

    /* next handler called automatically - 'AUTO_NEXT' */
    return PRIOT_ERR_NOERROR;
}

    /***************************
     *
     * Another specialised form of the above,
     *   implementing a 'TestAndIncr' spinlock
     *
     ***************************/


MibHandler *
Watcher_getWatchedSpinlockHandler(void)
{
    MibHandler *ret = NULL;

    ret = AgentHandler_createHandler("watcherSpinlock",
                                 Watcher_watchedSpinlockHandler);
    if (ret) {
        ret->flags |= MIB_HANDLER_AUTO_NEXT;
    }
    return ret;
}

int
Watcher_registerWatchedSpinlock(HandlerRegistration *reginfo,
                                   int *spinlock)
{
    MibHandler  *whandler;
    WatcherInfo *winfo;

    whandler         = Watcher_getWatchedSpinlockHandler();
    whandler->myvoid = (void *)spinlock;
    winfo            = Watcher_createWatcherInfo((void *)spinlock,
                   sizeof(int), ASN01_INTEGER, WATCHER_FIXED_SIZE);
    AgentHandler_injectHandler(reginfo, whandler);
    return Watcher_registerWatchedScalar2(reginfo, winfo);
}


int
Watcher_watchedSpinlockHandler(MibHandler *handler,
                               HandlerRegistration *reginfo,
                               AgentRequestInfo *reqinfo,
                               RequestInfo *requests)
{
    int     *spinlock = (int *) handler->myvoid;
    RequestInfo *request;

    DEBUG_MSGTL(("helper:watcher:spinlock",
                               "Got request:  %d\n", reqinfo->mode));
    DEBUG_MSGTL(( "helper:watcher:spinlock", "  oid:"));
    DEBUG_MSGOID(("helper:watcher:spinlock", requests->requestvb->name,
                                   requests->requestvb->nameLength));
    DEBUG_MSG((   "helper:watcher:spinlock", "\n"));

    switch (reqinfo->mode) {
        /*
         * Ensure the assigned value matches the current one
         */
    case MODE_SET_RESERVE1:
        for (request=requests; request; request=request->next) {
            if (request->processed)
                continue;

            if (*request->requestvb->val.integer != *spinlock) {
                Agent_setRequestError(reqinfo, requests, PRIOT_ERR_WRONGVALUE);
                handler->flags |= MIB_HANDLER_AUTO_NEXT_OVERRIDE_ONCE;
                return PRIOT_ERR_WRONGVALUE;

            }
        }
        break;

        /*
         * Everything else worked, so increment the spinlock
         */
    case MODE_SET_COMMIT:
    (*spinlock)++;
    break;
    }

    /* next handler called automatically - 'AUTO_NEXT' */
    return PRIOT_ERR_NOERROR;
}

    /***************************
     *
     *   Convenience registration routines - modelled on
     *   the equivalent netsnmp_register_*_instance() calls
     *
     ***************************/

WatcherInfo *
Watcher_cloneWatcherInfo(WatcherInfo *winfo)
{
    WatcherInfo *copy = (WatcherInfo *) malloc(sizeof(*copy));
    if (copy)
    *copy = *winfo;
    return copy;
}

static int
Watcher_registerScalarWatcher(const char* name,
                        const oid* reg_oid, size_t reg_oid_len,
                        void *data, size_t size, u_char type,
                        NodeHandlerFT * subhandler, int mode)
{
    HandlerRegistration *reginfo = NULL;
    MibHandler *whandler = NULL;
    WatcherInfo* watchinfo =
        Watcher_createWatcherInfo(data, size, type, WATCHER_FIXED_SIZE);
    if (watchinfo)
        whandler = Watcher_getWatcherHandler();
    if (watchinfo && whandler) {
        whandler->myvoid = watchinfo;
    Watcher_ownsWatcherInfo(whandler);
        reginfo =
            AgentHandler_createHandlerRegistration(
                name, subhandler, reg_oid, reg_oid_len, mode);
    }
    if (watchinfo && whandler && reginfo) {
        AgentHandler_injectHandler(reginfo, whandler);
        return Scalar_registerScalar(reginfo);
    }
    if (whandler)
        AgentHandler_handlerFree(whandler);
    else if (watchinfo)
        free(watchinfo);
    return PRIOT_ERR_RESOURCEUNAVAILABLE;
}

int
Watcher_registerUlongScalar(const char *name,
                              const oid * reg_oid, size_t reg_oid_len,
                              u_long * it,
                              NodeHandlerFT * subhandler)
{
    return Watcher_registerScalarWatcher(
        name, reg_oid, reg_oid_len,
        (void *)it, sizeof( u_long ),
        ASN01_UNSIGNED, subhandler, HANDLER_CAN_RWRITE);
}

int
Watcher_registerReadOnlyUlongScalar(const char *name,
                              const oid * reg_oid, size_t reg_oid_len,
                              u_long * it,
                              NodeHandlerFT * subhandler)
{
    return Watcher_registerScalarWatcher(
        name, reg_oid, reg_oid_len,
        (void *)it, sizeof( u_long ),
        ASN01_UNSIGNED, subhandler, HANDLER_CAN_RONLY);
}

int
Watcher_registerLongScalar(const char *name,
                              const oid * reg_oid, size_t reg_oid_len,
                              long * it,
                              NodeHandlerFT * subhandler)
{
    return Watcher_registerScalarWatcher(
        name, reg_oid, reg_oid_len,
        (void *)it, sizeof( long ),
        ASN01_INTEGER, subhandler, HANDLER_CAN_RWRITE);
}

int
Watcher_registerReadOnlyLongScalar(const char *name,
                              const oid * reg_oid, size_t reg_oid_len,
                              long * it,
                              NodeHandlerFT * subhandler)
{
    return Watcher_registerScalarWatcher(
        name, reg_oid, reg_oid_len,
        (void *)it, sizeof( long ),
        ASN01_INTEGER, subhandler, HANDLER_CAN_RONLY);
}

int
Watcher_registerIntScalar(const char *name,
                              const oid * reg_oid, size_t reg_oid_len,
                              int * it,
                              NodeHandlerFT * subhandler)
{
    return Watcher_registerScalarWatcher(
        name, reg_oid, reg_oid_len,
        (void *)it, sizeof( int ),
        ASN01_INTEGER, subhandler, HANDLER_CAN_RWRITE);
}

int
Watcher_registerReadOnlyIntScalar(const char *name,
                              const oid * reg_oid, size_t reg_oid_len,
                              int * it,
                              NodeHandlerFT * subhandler)
{
    return Watcher_registerScalarWatcher(
        name, reg_oid, reg_oid_len,
        (void *)it, sizeof( int ),
        ASN01_INTEGER, subhandler, HANDLER_CAN_RONLY);
}

int
Watcher_registerReadOnlyCounter32Scalar(const char *name,
                              const oid * reg_oid, size_t reg_oid_len,
                              u_long * it,
                              NodeHandlerFT * subhandler)
{
    return Watcher_registerScalarWatcher(
        name, reg_oid, reg_oid_len,
        (void *)it, sizeof( u_long ),
        ASN01_COUNTER, subhandler, HANDLER_CAN_RONLY);
}
