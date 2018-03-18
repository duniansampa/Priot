#include "Instance.h"
#include "Tools.h"
#include "Debug.h"
#include "Assert.h"
#include "AgentHandler.h"
#include "Watcher.h"
#include "AgentRegistry.h"
#include "Client.h"
#include "DataList.h"
#include "Client.h"
#include "Serialize.h"
#include "ReadOnly.h"

typedef struct NumFileInstance_s {
    int   refcnt;
    char *file_name;
    FILE *filep;
    u_char type;
    int   flags;
} NumFileInstance;

/** @defgroup instance instance
 *  Process individual MIB instances easily.
 *  @ingroup leaf
 *  @{
 */

static NumFileInstance *
_Instance_numFileInstanceRef(NumFileInstance *nfi)
{
    nfi->refcnt++;
    return nfi;
}

static void
_Instance_numFileInstanceDeref(NumFileInstance *nfi)
{
    if (--nfi->refcnt == 0) {
    free(nfi->file_name);
    free(nfi);
    }
}

/**
 * Creates an instance helper handler, calls AgentHandler_createHandler, which
 * then could be registered, using netsnmp_register_handler().
 *
 * @return Returns a pointer to a MibHandler struct which contains
 *	the handler's name and the access method
 */
MibHandler *
Instance_getInstanceHandler(void)
{
    return AgentHandler_createHandler("instance", Instance_helperHandler);
}

/**
 * This function registers an instance helper handler, which is a way of
 * registering an exact OID such that GENEXT requests are handled entirely
 * by the helper. First need to inject it into the calling chain of the
 * handler defined by the HandlerRegistration struct, reginfo.
 * The new handler is injected at the top of the list and will be the new
 * handler to be called first.  This function also injects a serialize
 * handler before actually calling netsnmp_register_handle, registering
 * reginfo.
 *
 * @param reginfo a handler registration structure which could get created
 *                using AgentHandler_createHandler_registration.  Used to register
 *                an instance helper handler.
 *
 * @return
 *      MIB_REGISTERED_OK is returned if the registration was a success.
 *	Failures are MIB_REGISTRATION_FAILED and MIB_DUPLICATE_REGISTRATION.
 */
int
Instance_registerInstance(HandlerRegistration *reginfo)
{
    MibHandler *handler = Instance_getInstanceHandler();
    handler->flags |= MIB_HANDLER_INSTANCE;
    AgentHandler_injectHandler(reginfo, handler);
    return Serialize_registerSerialize(reginfo);
}

/**
 * This function injects a "read only" handler into the handler chain
 * prior to serializing/registering the handler.
 *
 * The only purpose of this "read only" handler is to return an
 * appropriate error for any requests passed to it in a SET mode.
 * Inserting it into your handler chain will ensure you're never
 * asked to perform a SET request so you can ignore those error
 * conditions.
 *
 * @param reginfo a handler registration structure which could get created
 *                using AgentHandler_createHandlerRegistration.  Used to register
 *                a read only instance helper handler.
 *
 * @return
 *      MIB_REGISTERED_OK is returned if the registration was a success.
 *	Failures are MIB_REGISTRATION_FAILED and MIB_DUPLICATE_REGISTRATION.
 */
int
Instance_registerReadOnlyInstance(HandlerRegistration *reginfo)
{
    AgentHandler_injectHandler(reginfo, Instance_getInstanceHandler());
    AgentHandler_injectHandler(reginfo, ReadOnly_getReadOnlyHandler());
    return Serialize_registerSerialize(reginfo);
}

static
HandlerRegistration *
_Instance_getReg(const char *name,
        const char *ourname,
        const oid * reg_oid, size_t reg_oid_len,
        NumFileInstance *it,
        int modes,
        NodeHandlerFT * scalarh, NodeHandlerFT * subhandler,
        const char *contextName)
{
    HandlerRegistration *myreg;
    MibHandler *myhandler;

    if (subhandler) {
        myreg =
            AgentHandler_createHandlerRegistration(name,
                                                subhandler,
                                                reg_oid, reg_oid_len,
                                                modes);
        myhandler = AgentHandler_createHandler(ourname, scalarh);
        myhandler->myvoid = it;
    myhandler->data_clone = (void*(*)(void*))_Instance_numFileInstanceRef;
    myhandler->data_free = (void(*)(void*))_Instance_numFileInstanceDeref;
        AgentHandler_injectHandler(myreg, myhandler);
    } else {
        myreg =
            AgentHandler_createHandlerRegistration(name,
                                                scalarh,
                                                reg_oid, reg_oid_len,
                                                modes);
        myreg->handler->myvoid = it;
    myreg->handler->data_clone
        = (void *(*)(void *))_Instance_numFileInstanceRef;
    myreg->handler->data_free
        = (void (*)(void *))_Instance_numFileInstanceDeref;
    }
    if (contextName)
        myreg->contextName = strdup(contextName);
    return myreg;
}

/* Watched 'long' instances are writable on both 32-bit and 64-bit systems  */

int
Instance_registerReadOnlyUlongInstance(const char *name,
                                          const oid * reg_oid,
                                          size_t reg_oid_len, u_long * it,
                                          NodeHandlerFT *
                                          subhandler)
{
    return Watcher_registerWatchedInstance2(
               AgentHandler_createHandlerRegistration(
                   name, subhandler, reg_oid, reg_oid_len, HANDLER_CAN_RONLY),
               Watcher_createWatcherInfo(
                   (void *)it, sizeof(u_long),
                   ASN01_UNSIGNED, WATCHER_FIXED_SIZE));
}


int
Instance_registerUlongInstance(const char *name,
                                const oid * reg_oid, size_t reg_oid_len,
                                u_long * it,
                                NodeHandlerFT * subhandler)
{
    return Watcher_registerWatchedInstance2(
               AgentHandler_createHandlerRegistration(
                   name, subhandler, reg_oid, reg_oid_len, HANDLER_CAN_RWRITE),
               Watcher_createWatcherInfo(
                   (void *)it, sizeof(u_long),
                   ASN01_UNSIGNED, WATCHER_FIXED_SIZE));
}

int
Instance_registerReadOnlyCounter32Instance(const char *name,
                                              const oid * reg_oid,
                                              size_t reg_oid_len,
                                              u_long * it,
                                              NodeHandlerFT *
                                              subhandler)
{
    return Watcher_registerWatchedInstance2(
               AgentHandler_createHandlerRegistration(
                   name, subhandler, reg_oid, reg_oid_len, HANDLER_CAN_RONLY),
               Watcher_createWatcherInfo(
                   (void *)it, sizeof(u_long),
                   ASN01_COUNTER, WATCHER_FIXED_SIZE));
}

int
Instance_registerReadOnlyLongInstance(const char *name,
                                         const oid * reg_oid,
                                         size_t reg_oid_len,
                                         long *it,
                                         NodeHandlerFT * subhandler)
{
    return Watcher_registerWatchedInstance2(
               AgentHandler_createHandlerRegistration(
                   name, subhandler, reg_oid, reg_oid_len, HANDLER_CAN_RONLY),
               Watcher_createWatcherInfo(
                   (void *)it, sizeof(long), ASN01_INTEGER, WATCHER_FIXED_SIZE));
}


int
Instance_registerLongInstance(const char *name,
                               const oid * reg_oid, size_t reg_oid_len,
                               long *it, NodeHandlerFT * subhandler)
{
    return Watcher_registerWatchedInstance2(
               AgentHandler_createHandlerRegistration(
                   name, subhandler, reg_oid, reg_oid_len, HANDLER_CAN_RWRITE),
               Watcher_createWatcherInfo(
                   (void *)it, sizeof(long), ASN01_INTEGER, WATCHER_FIXED_SIZE));
}

/* Watched 'int' instances are only writable on 32-bit systems  */

int
Instance_registerReadOnlyUintInstance(const char *name,
                                         const oid * reg_oid,
                                         size_t reg_oid_len,
                                         unsigned int *it,
                                         NodeHandlerFT * subhandler)
{
    return Watcher_registerWatchedInstance2(
               AgentHandler_createHandlerRegistration(
                   name, subhandler, reg_oid, reg_oid_len, HANDLER_CAN_RONLY),
               Watcher_createWatcherInfo(
                   (void *)it, sizeof(unsigned int),
                   ASN01_UNSIGNED, WATCHER_FIXED_SIZE));
}

int
Instance_registerUintInstance(const char *name,
                               const oid * reg_oid, size_t reg_oid_len,
                               unsigned int *it, NodeHandlerFT * subhandler)
{
    return Watcher_registerWatchedInstance2(
               AgentHandler_createHandlerRegistration(
                   name, subhandler, reg_oid, reg_oid_len, HANDLER_CAN_RWRITE),
               Watcher_createWatcherInfo(
                   (void *)it, sizeof(unsigned int),
                   ASN01_UNSIGNED, WATCHER_FIXED_SIZE));
}

int
Instance_registerReadOnlyIntInstance2(const char *name,
                                const oid * reg_oid, size_t reg_oid_len,
                                int *it, NodeHandlerFT * subhandler)
{
    return Watcher_registerWatchedInstance2(
               AgentHandler_createHandlerRegistration(
                   name, subhandler, reg_oid, reg_oid_len, HANDLER_CAN_RONLY),
               Watcher_createWatcherInfo(
                   (void *)it, sizeof(int), ASN01_INTEGER, WATCHER_FIXED_SIZE));
}

/*
* Compatibility with earlier (inconsistently named) routine
*/

int
Instance_registerReadOnlyIntInstance(const char *name,
                                const oid * reg_oid, size_t reg_oid_len,
                                int *it, NodeHandlerFT * subhandler)
{
  return Instance_registerReadOnlyIntInstance2(name,
                                reg_oid, reg_oid_len,
                                it, subhandler);
}

/*
 * Context registrations
 */


int
Instance_registerReadOnlyUlongInstanceContext(const char *name,
                                                  const oid * reg_oid,
                                                  size_t reg_oid_len,
                                                  u_long * it,
                                                  NodeHandlerFT *
                                                  subhandler,
                                                  const char *contextName)
{
    HandlerRegistration *myreg =
      AgentHandler_createHandlerRegistration(
          name, subhandler, reg_oid, reg_oid_len, HANDLER_CAN_RONLY);
    if (myreg && contextName)
      myreg->contextName = strdup(contextName);
    return Watcher_registerWatchedInstance2(
        myreg, Watcher_createWatcherInfo(
            (void *)it, sizeof(u_long), ASN01_UNSIGNED, WATCHER_FIXED_SIZE));
}

int
Instance_registerUlongInstanceContext(const char *name,
                                        const oid * reg_oid, size_t reg_oid_len,
                                        u_long * it,
                                        NodeHandlerFT * subhandler,
                                        const char *contextName)
{
    HandlerRegistration *myreg =
      AgentHandler_createHandlerRegistration(
          name, subhandler, reg_oid, reg_oid_len, HANDLER_CAN_RWRITE);
    if (myreg && contextName)
      myreg->contextName = strdup(contextName);
    return Watcher_registerWatchedInstance2(
        myreg, Watcher_createWatcherInfo(
            (void *)it, sizeof(u_long), ASN01_UNSIGNED, WATCHER_FIXED_SIZE));
}

int
Instance_registerReadOnlyCounter32InstanceContext(const char *name,
                                                      const oid * reg_oid,
                                                      size_t reg_oid_len,
                                                      u_long * it,
                                                      NodeHandlerFT *
                                                      subhandler,
                                                      const char *contextName)
{
    HandlerRegistration *myreg =
      AgentHandler_createHandlerRegistration(
          name, subhandler, reg_oid, reg_oid_len, HANDLER_CAN_RONLY);
    if (myreg && contextName)
      myreg->contextName = strdup(contextName);
    return Watcher_registerWatchedInstance2(
        myreg, Watcher_createWatcherInfo(
            (void *)it, sizeof(u_long), ASN01_COUNTER, WATCHER_FIXED_SIZE));
}

int
Instance_registerReadOnlyLongInstanceContext(const char *name,
                                                 const oid * reg_oid,
                                                 size_t reg_oid_len,
                                                 long *it,
                                                 NodeHandlerFT
                                                 *subhandler,
                                                 const char *contextName)
{
    HandlerRegistration *myreg =
      AgentHandler_createHandlerRegistration(
          name, subhandler, reg_oid, reg_oid_len, HANDLER_CAN_RONLY);
    if (myreg && contextName)
      myreg->contextName = strdup(contextName);
    return Watcher_registerWatchedInstance2(
        myreg, Watcher_createWatcherInfo(
            (void *)it, sizeof(long), ASN01_INTEGER, WATCHER_FIXED_SIZE));
}

int
Instance_registerLongInstanceContext(const char *name,
                                       const oid * reg_oid, size_t reg_oid_len,
                                       long *it,
                                       NodeHandlerFT * subhandler,
                                       const char *contextName)
{
    HandlerRegistration *myreg =
      AgentHandler_createHandlerRegistration(
          name, subhandler, reg_oid, reg_oid_len, HANDLER_CAN_RWRITE);
    if (myreg && contextName)
      myreg->contextName = strdup(contextName);
    return Watcher_registerWatchedInstance2(
        myreg, Watcher_createWatcherInfo(
            (void *)it, sizeof(long), ASN01_INTEGER, WATCHER_FIXED_SIZE));
}


int
Instance_registerIntInstanceContext(const char *name,
                                      const oid * reg_oid,
                                      size_t reg_oid_len,
                                      int *it,
                                      NodeHandlerFT * subhandler,
                                      const char *contextName)
{
    HandlerRegistration *myreg =
      AgentHandler_createHandlerRegistration(
          name, subhandler, reg_oid, reg_oid_len, HANDLER_CAN_RWRITE);
    if (myreg && contextName)
      myreg->contextName = strdup(contextName);
    return Watcher_registerWatchedInstance2(
        myreg, Watcher_createWatcherInfo(
            (void *)it, sizeof(int), ASN01_INTEGER, WATCHER_FIXED_SIZE));
}

int
Instance_registerReadOnlyIntInstanceContext2(const char *name,
                                                const oid * reg_oid,
                                                size_t reg_oid_len,
                                                int *it,
                                                NodeHandlerFT * subhandler,
                                                const char *contextName)
{
    HandlerRegistration *myreg =
      AgentHandler_createHandlerRegistration(
          name, subhandler, reg_oid, reg_oid_len, HANDLER_CAN_RONLY);
    if (myreg && contextName)
      myreg->contextName = strdup(contextName);
    return Watcher_registerWatchedInstance2(
        myreg, Watcher_createWatcherInfo(
            (void *)it, sizeof(int), ASN01_INTEGER, WATCHER_FIXED_SIZE));
}

/*
 * Compatibility with earlier (inconsistently named) routine
 */

int
Instance_registerReadOnlyIntInstanceContext(const char *name,
                                        const oid * reg_oid, size_t reg_oid_len,
                                        int *it,
                                        NodeHandlerFT * subhandler,
                                        const char *contextName)
{
    return Instance_registerReadOnlyIntInstanceContext2(name,
                                                           reg_oid, reg_oid_len,
                                                           it, subhandler,
                                                           contextName);
}

int
Instance_registerNumFileInstance(const char *name,
                                   const oid * reg_oid, size_t reg_oid_len,
                                   const char *file_name, int asn_type, int mode,
                                   NodeHandlerFT * subhandler,
                                   const char *contextName)
{
    HandlerRegistration *myreg;
    NumFileInstance *nfi;

    if ((NULL == name) || (NULL == reg_oid) || (NULL == file_name)) {
        Logger_log(LOGGER_PRIORITY_ERR, "bad parameter to Instance_registerNumFileInstance\n");
        return MIB_REGISTRATION_FAILED;
    }

    nfi = TOOLS_MALLOC_TYPEDEF(NumFileInstance);
    if ((NULL == nfi) ||
        (NULL == (nfi->file_name = strdup(file_name)))) {
        Logger_log(LOGGER_PRIORITY_ERR, "could not not allocate memory\n");
        if (NULL != nfi)
            free(nfi); /* SNMP_FREE overkill on local var */
        return MIB_REGISTRATION_FAILED;
    }

    nfi->refcnt = 1;
    myreg = _Instance_getReg(name, "fileNumHandler", reg_oid, reg_oid_len, nfi,
                    mode, Instance_numFileHandler,
                    subhandler, contextName);
    if (NULL == myreg) {
        _Instance_numFileInstanceDeref(nfi);
        return MIB_REGISTRATION_FAILED;
    }

    nfi->type = asn_type;

    if (HANDLER_CAN_RONLY == mode)
        return Instance_registerReadOnlyInstance(myreg);

    return Instance_registerInstance(myreg);
}

/**
 * This function registers an int helper handler to a specified OID.
 *
 * @param name         the name used for registration pruposes.
 *
 * @param reg_oid      the OID where you want to register your integer at
 *
 * @param reg_oid_len  the length of the OID
 *
 * @param it           the integer value to be registered during initialization
 *
 * @param subhandler   a handler to do whatever you want to do, otherwise use
 *                     NULL to use the default int handler.
 *
 * @return
 *      MIB_REGISTERED_OK is returned if the registration was a success.
 *	Failures are MIB_REGISTRATION_FAILED and MIB_DUPLICATE_REGISTRATION.
 */
int
Instance_registerIntInstance(const char *name,
                              const oid * reg_oid, size_t reg_oid_len,
                              int *it, NodeHandlerFT * subhandler)
{
    return Watcher_registerWatchedInstance2(
               AgentHandler_createHandlerRegistration(
                   name, subhandler, reg_oid, reg_oid_len, HANDLER_CAN_RWRITE),
               Watcher_createWatcherInfo(
                   (void *)it, sizeof(int), ASN01_INTEGER, WATCHER_FIXED_SIZE));
}


#define free_wrapper free

int
Instance_numFileHandler(MibHandler *handler,
                                  HandlerRegistration *reginfo,
                                  AgentRequestInfo *reqinfo,
                                  RequestInfo *requests)
{
    NumFileInstance *nfi;
    u_long it;
    u_long *it_save;
    int rc;

    Assert_assert(NULL != handler);
    nfi = (NumFileInstance *)handler->myvoid;
    Assert_assert(NULL != nfi);
    Assert_assert(NULL != nfi->file_name);

    DEBUG_MSGTL(("netsnmp_instance_int_handler", "Got request:  %d\n",
                reqinfo->mode));

    switch (reqinfo->mode) {
        /*
         * data requests
         */
    case MODE_GET:
    /*
     * Use a long here, otherwise on 64 bit use of an int would fail
     */
        Assert_assert(NULL == nfi->filep);
        nfi->filep = fopen(nfi->file_name, "r");
        if (NULL == nfi->filep) {
            Agent_setRequestError(reqinfo, requests,
                                      PRIOT_NOSUCHINSTANCE);
            return PRIOT_ERR_NOERROR;
        }
        rc = fscanf(nfi->filep, (nfi->type == ASN01_INTEGER) ? "%ld" : "%lu",
                    &it);
        fclose(nfi->filep);
        nfi->filep = NULL;
        if (rc != 1) {
            Agent_setRequestError(reqinfo, requests,
                                      PRIOT_NOSUCHINSTANCE);
            return PRIOT_ERR_NOERROR;
        }
        Client_setVarTypedValue(requests->requestvb, nfi->type,
                                 (u_char *) &it, sizeof(it));
        break;

        /*
         * SET requests.  Should only get here if registered RWRITE
         */
    case MODE_SET_RESERVE1:
        Assert_assert(NULL == nfi->filep);
        if (requests->requestvb->type != nfi->type)
            Agent_setRequestError(reqinfo, requests,
                                      PRIOT_ERR_WRONGTYPE);
        break;

    case MODE_SET_RESERVE2:
        Assert_assert(NULL == nfi->filep);
        nfi->filep = fopen(nfi->file_name, "w+");
        if (NULL == nfi->filep) {
            Agent_setRequestError(reqinfo, requests,
                                      PRIOT_ERR_NOTWRITABLE);
            return PRIOT_ERR_NOERROR;
        }
        /*
         * store old info for undo later
         */
        if (fscanf(nfi->filep, (nfi->type == ASN01_INTEGER) ? "%ld" : "%lu",
                   &it) != 1) {
            Agent_setRequestError(reqinfo, requests,
                                      PRIOT_ERR_RESOURCEUNAVAILABLE);
            return PRIOT_ERR_NOERROR;
        }

        it_save = (u_long *)Tools_memdup(&it, sizeof(u_long));
        if (it_save == NULL) {
            Agent_setRequestError(reqinfo, requests,
                                      PRIOT_ERR_RESOURCEUNAVAILABLE);
            return PRIOT_ERR_NOERROR;
        }
        AgentHandler_requestAddListData(requests,
                                      DataList_create
                                      (INSTANCE_HANDLER_NAME, it_save,
                                       &free_wrapper));
        break;

    case MODE_SET_ACTION:
        /*
         * update current
         */
        DEBUG_MSGTL(("helper:instance", "updated %s -> %ld\n", nfi->file_name,
                    *(requests->requestvb->val.integer)));
        it = *(requests->requestvb->val.integer);
        rewind(nfi->filep); /* rewind to make sure we are at the beginning */
        rc = fprintf(nfi->filep, (nfi->type == ASN01_INTEGER) ? "%ld" : "%lu",
                     it);
        if (rc < 0) {
            Agent_setRequestError(reqinfo, requests,
                                      PRIOT_ERR_GENERR);
            return PRIOT_ERR_NOERROR;
        }
        break;

    case MODE_SET_UNDO:
        it =
            *((u_int *) AgentHandler_requestGetListData(requests,
                                                      INSTANCE_HANDLER_NAME));
        rc = fprintf(nfi->filep, (nfi->type == ASN01_INTEGER) ? "%ld" : "%lu",
                     it);
        if (rc < 0)
            Agent_setRequestError(reqinfo, requests, PRIOT_ERR_UNDOFAILED);
        /** fall through */

    case MODE_SET_COMMIT:
    case MODE_SET_FREE:
        if (NULL != nfi->filep) {
            fclose(nfi->filep);
            nfi->filep = NULL;
        }
        break;
    default:
        Logger_log(LOGGER_PRIORITY_ERR,
                 "Instance_numFileHandler: illegal mode\n");
        Agent_setRequestError(reqinfo, requests, PRIOT_ERR_GENERR);
        return PRIOT_ERR_NOERROR;
    }

    if (handler->next && handler->next->access_method)
        return AgentHandler_callNextHandler(handler, reginfo, reqinfo,
                                         requests);
    return PRIOT_ERR_NOERROR;
}

int
Instance_helperHandler(MibHandler *handler,
                                HandlerRegistration *reginfo,
                                AgentRequestInfo *reqinfo,
                                RequestInfo *requests)
{

    Types_VariableList *var = requests->requestvb;

    int             ret, cmp;

    DEBUG_MSGTL(("helper:instance", "Got request:\n"));
    cmp = Api_oidCompare(requests->requestvb->name,
                           requests->requestvb->nameLength,
                           reginfo->rootoid, reginfo->rootoid_len);

    DEBUG_MSGTL(("helper:instance", "  oid:"));
    DEBUG_MSGOID(("helper:instance", var->name, var->nameLength));
    DEBUG_MSG(("helper:instance", "\n"));

    switch (reqinfo->mode) {
    case MODE_GET:
        if (cmp != 0) {
            Agent_setRequestError(reqinfo, requests,
                                      PRIOT_NOSUCHINSTANCE);
            return PRIOT_ERR_NOERROR;
        } else {
            return AgentHandler_callNextHandler(handler, reginfo, reqinfo,
                                             requests);
        }
        break;

    case MODE_SET_RESERVE1:
    case MODE_SET_RESERVE2:
    case MODE_SET_ACTION:
    case MODE_SET_COMMIT:
    case MODE_SET_UNDO:
    case MODE_SET_FREE:
        if (cmp != 0) {
            Agent_setRequestError(reqinfo, requests,
                                      PRIOT_ERR_NOCREATION);
            return PRIOT_ERR_NOERROR;
        } else {
            return AgentHandler_callNextHandler(handler, reginfo, reqinfo,
                                             requests);
        }
        break;

    case MODE_GETNEXT:
        if (cmp < 0 || (cmp == 0 && requests->inclusive)) {
            reqinfo->mode = MODE_GET;
            Client_setVarObjid(requests->requestvb, reginfo->rootoid,
                               reginfo->rootoid_len);
            ret =
                AgentHandler_callNextHandler(handler, reginfo, reqinfo,
                                          requests);
            reqinfo->mode = MODE_GETNEXT;
            /*
             * if the instance doesn't have data, set type to ASN_NULL
             * to move to the next sub-tree. Ignore delegated requests; they
             * might have data later on.
             */
            if (!requests->delegated &&
                (requests->requestvb->type == PRIOT_NOSUCHINSTANCE ||
                 requests->requestvb->type == PRIOT_NOSUCHOBJECT)) {
                requests->requestvb->type = ASN01_NULL;
            }
            return ret;
        } else {
            return PRIOT_ERR_NOERROR;
        }
        break;
    default:
        Logger_log(LOGGER_PRIORITY_ERR,
                 "Instance_instanceHelperHandler: illegal mode\n");
        Agent_setRequestError(reqinfo, requests, PRIOT_ERR_GENERR);
        return PRIOT_ERR_NOERROR;
    }
    /*
     * got here only if illegal mode found
     */
    return PRIOT_ERR_GENERR;
}
