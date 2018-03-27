#include "OldApi.h"
#include "Impl.h"
#include "AgentRegistry.h"
#include "Api.h"
#include "Debug.h"
#include "Logger.h"
#include "AgentCallbacks.h"
#include "Client.h"
#include "DataList.h"

#define MIB_CLIENTS_ARE_EVIL 1

/*
 * don't use these!
 */

static void
_OldApi_setCurrentAgentSession(AgentSession *asp);

AgentSession*
OldApi_getCurrentAgentSession(void);

/** @defgroup old_api old_api
 *  Calls mib module code written in the old style of code.
 *  @ingroup handler
 *  This is a backwards compatilibity module that allows code written
 *  in the old API to be run under the new handler based architecture.
 *  Use it by calling OldApi_registerOldApi().
 *  @{
 */

/** returns a old_api handler that should be the final calling
 * handler.  Don't use this function.  Use the OldApi_registerOldApi()
 * function instead.
 */
MibHandler *
OldApi_getOldApiHandler(void)
{
    return AgentHandler_createHandler("oldApi", OldApi_helper);
}

struct Variable_s *
OldApi_duplicateVariable(const struct Variable_s *var)
{
    struct Variable_s *var2 = NULL;

    if (var) {
        const int varsize = offsetof(struct Variable_s, name) + var->namelen * sizeof(var->name[0]);
        var2 = (struct Variable_s *) malloc(varsize);
        if (var2)
            memcpy(var2, var, varsize);
    }
    return var2;
}

/** Registers an old API set into the mib tree.  Functionally this
 * mimics the old register_mib_context() function (and in fact the new
 * register_mib_context() function merely calls this new old_api one).
 */
int
OldApi_registerOldApi(const char *moduleName,
                         const struct Variable_s *var,
                         size_t varsize,
                         size_t numvars,
                         const oid * mibloc,
                         size_t mibloclen,
                         int priority,
                         int range_subid,
                         oid range_ubound,
                         Types_Session * ss,
                         const char *context,
                         int timeout,
                         int flags )
{

    unsigned int    i;

    /*
     * register all subtree nodes
     */
    for (i = 0; i < numvars; i++) {
        struct Variable_s *vp;
        HandlerRegistration *reginfo =
            TOOLS_MALLOC_TYPEDEF(HandlerRegistration);
        if (reginfo == NULL)
            return PRIOT_ERR_GENERR;

    vp = OldApi_duplicateVariable((const struct Variable_s *)
                    ((const char *) var + varsize * i));

        reginfo->handler = OldApi_getOldApiHandler();
        reginfo->handlerName = strdup(moduleName);
        reginfo->rootoid_len = (mibloclen + vp->namelen);
        reginfo->rootoid =
            (oid *) malloc(reginfo->rootoid_len * sizeof(oid));
        if (reginfo->rootoid == NULL) {
            TOOLS_FREE(vp);
            TOOLS_FREE(reginfo->handlerName);
            TOOLS_FREE(reginfo);
            return PRIOT_ERR_GENERR;
        }

        memcpy(reginfo->rootoid, mibloc, mibloclen * sizeof(oid));
        memcpy(reginfo->rootoid + mibloclen, vp->name, vp->namelen
               * sizeof(oid));
        reginfo->handler->myvoid = (void *) vp;
        reginfo->handler->data_clone
        = (void *(*)(void *))OldApi_duplicateVariable;
        reginfo->handler->data_free = free;

        reginfo->priority = priority;
        reginfo->range_subid = range_subid;

        reginfo->range_ubound = range_ubound;
        reginfo->timeout = timeout;
        reginfo->contextName = (context) ? strdup(context) : NULL;
        reginfo->modes = vp->acl == IMPL_OLDAPI_RONLY ? HANDLER_CAN_RONLY :
                         HANDLER_CAN_RWRITE;

        /*
         * register ourselves in the mib tree
         */
        if (AgentHandler_registerHandler(reginfo) != MIB_REGISTERED_OK) {
            /** AgentHandler_handlerRegistrationFree(reginfo); already freed */
            /* TOOLS_FREE(vp); already freed */
        }
    }
    return ErrorCode_SUCCESS;
}

/** registers a row within a mib table */
int
OldApi_registerMibTableRow(const char *moduleName,
                               const struct Variable_s *var,
                               size_t varsize,
                               size_t numvars,
                               oid * mibloc,
                               size_t mibloclen,
                               int priority,
                               int var_subid,
                               Types_Session * ss,
                               const char *context,
                               int timeout,
                               int flags )
{
    unsigned int    i = 0, rc = 0;
    oid             ubound = 0;

    for (i = 0; i < numvars; i++) {
        const struct Variable_s *vr =
            (const struct Variable_s *) ((const char *) var + (i * varsize));
        HandlerRegistration *r;
        if ( var_subid > (int)mibloclen ) {
            break;    /* doesn't make sense */
        }
        r = TOOLS_MALLOC_TYPEDEF(HandlerRegistration);

        if (r == NULL) {
            /*
             * Unregister whatever we have registered so far, and
             * return an error.
             */
            rc = MIB_REGISTRATION_FAILED;
            break;
        }
        memset(r, 0, sizeof(HandlerRegistration));

        r->handler = OldApi_getOldApiHandler();
        r->handlerName = strdup(moduleName);

        if (r->handlerName == NULL) {
            AgentHandler_handlerRegistrationFree(r);
            break;
        }

        r->rootoid_len = mibloclen;
        r->rootoid = (oid *) malloc(r->rootoid_len * sizeof(oid));

        if (r->rootoid == NULL) {
            AgentHandler_handlerRegistrationFree(r);
            rc = MIB_REGISTRATION_FAILED;
            break;
        }
        memcpy(r->rootoid, mibloc, mibloclen * sizeof(oid));
        memcpy((u_char *) (r->rootoid + (var_subid - vr->namelen)), vr->name,
               vr->namelen * sizeof(oid));
        DEBUG_MSGTL(("OldApi_registerMibTableRow", "rootoid "));
        DEBUG_MSGOID(("OldApi_registerMibTableRow", r->rootoid,
                     r->rootoid_len));
        DEBUG_MSG(("OldApi_registerMibTableRow", "(%d)\n",
                     (var_subid - vr->namelen)));
        r->handler->myvoid = OldApi_duplicateVariable(vr);
        r->handler->data_clone = (void *(*)(void *))OldApi_duplicateVariable;
        r->handler->data_free = free;

        if (r->handler->myvoid == NULL) {
            AgentHandler_handlerRegistrationFree(r);
            rc = MIB_REGISTRATION_FAILED;
            break;
        }

        r->contextName = (context) ? strdup(context) : NULL;

        if (context != NULL && r->contextName == NULL) {
            AgentHandler_handlerRegistrationFree(r);
            rc = MIB_REGISTRATION_FAILED;
            break;
        }

        r->priority = priority;
        r->range_subid = 0;     /* var_subid; */
        r->range_ubound = 0;    /* range_ubound; */
        r->timeout = timeout;
        r->modes = HANDLER_CAN_RWRITE;

        /*
         * Register this column and row
         */
        if ((rc =
             AgentHandler_registerHandlerNocallback(r)) !=
            MIB_REGISTERED_OK) {
            DEBUG_MSGTL(("OldApi_registerMibTableRow",
                        "register failed %d\n", rc));
            AgentHandler_handlerRegistrationFree(r);
            break;
        }

        if (vr->namelen > 0) {
            if (vr->name[vr->namelen - 1] > ubound) {
                ubound = vr->name[vr->namelen - 1];
            }
        }
    }

    if (rc == MIB_REGISTERED_OK) {
        struct RegisterParameters_s reg_parms;

        reg_parms.name = mibloc;
        reg_parms.namelen = mibloclen;
        reg_parms.priority = priority;
        reg_parms.flags = (u_char) flags;
        reg_parms.range_subid = var_subid;
        reg_parms.range_ubound = ubound;
        reg_parms.timeout = timeout;
        reg_parms.contextName = context;
        rc = Callback_callCallbacks(CALLBACK_APPLICATION,
                                 PriotdCallback_REGISTER_OID, &reg_parms);
    }

    return rc;
}

/** implements the old_api handler */
int
OldApi_helper(MibHandler *handler,
                       HandlerRegistration *reginfo,
                       AgentRequestInfo *reqinfo,
                       RequestInfo *requests)
{

    oid             save[ASN01_MAX_OID_LEN];
    size_t          savelen = 0;

    struct Variable_s compat_var, *cvp = &compat_var;
    int             exact = 1;
    int             status;

    struct Variable_s *vp;
    OldApiCache  *cacheptr;
    AgentSession *oldasp = NULL;
    u_char         *access = NULL;
    WriteMethodFT  *write_method = NULL;
    size_t          len;
    size_t          tmp_len;
    oid             tmp_name[ASN01_MAX_OID_LEN];

    vp = (struct Variable_s *) handler->myvoid;

    /*
     * create old variable structure with right information
     */
    memcpy(cvp->name, reginfo->rootoid,
           reginfo->rootoid_len * sizeof(oid));
    cvp->namelen = reginfo->rootoid_len;
    cvp->type = vp->type;
    cvp->magic = vp->magic;
    cvp->acl = vp->acl;
    cvp->findVar = vp->findVar;

    switch (reqinfo->mode) {
    case MODE_GETNEXT:
    case MODE_GETBULK:
        exact = 0;
    }

    for (; requests; requests = requests->next) {

        savelen = requests->requestvb->nameLength;
        memcpy(save, requests->requestvb->name, savelen * sizeof(oid));

        switch (reqinfo->mode) {
        case MODE_GET:
        case MODE_GETNEXT:
        case MODE_SET_RESERVE1:
            /*
             * Actually call the old mib-module function
             */
            if (vp && vp->findVar) {
                memcpy(tmp_name, requests->requestvb->name,
                                 requests->requestvb->nameLength*sizeof(oid));
                tmp_len = requests->requestvb->nameLength;
                access = (*(vp->findVar)) (cvp, tmp_name, &tmp_len,
                                           exact, &len, &write_method);
                Client_setVarObjid( requests->requestvb, tmp_name, tmp_len );
            }
            else
                access = NULL;

            /*
             * WWW: end range checking
             */
            if (access) {
                /*
                 * result returned
                 */
                if (reqinfo->mode != MODE_SET_RESERVE1)
                    Client_setVarTypedValue(requests->requestvb,
                                             cvp->type, access, len);
            } else {
                /*
                 * no result returned
                 */

                if (access == NULL) {
                    if (Api_oidEquals(requests->requestvb->name,
                                         requests->requestvb->nameLength,
                                         save, savelen) != 0) {
                        DEBUG_MSGTL(("oldApi", "evil_client: %s\n",
                                    reginfo->handlerName));
                        memcpy(requests->requestvb->name, save,
                               savelen * sizeof(oid));
                        requests->requestvb->nameLength = savelen;
                    }
                }

            }

            /*
             * AAA: fall through for everything that is a set (see BBB)
             */
            if (reqinfo->mode != MODE_SET_RESERVE1)
                break;

            cacheptr = TOOLS_MALLOC_TYPEDEF(OldApiCache);
            if (!cacheptr)
                return Agent_setRequestError(reqinfo, requests,
                                                 PRIOT_ERR_RESOURCEUNAVAILABLE);
            cacheptr->data = access;
            cacheptr->write_method = write_method;
            write_method = NULL;
            AgentHandler_requestAddListData(requests,
                                          DataList_create
                                          (OLD_API_NAME, cacheptr,
                                           &free));
            /*
             * BBB: fall through for everything that is a set (see AAA)
             */

        default:
            /*
             * WWW: explicitly list the SET conditions
             */
            /*
             * (the rest of the) SET contions
             */
            cacheptr =
                (OldApiCache *)
                AgentHandler_requestGetListData(requests, OLD_API_NAME);

            if (cacheptr == NULL || cacheptr->write_method == NULL) {
                /*
                 * WWW: try to set ourselves if possible?
                 */
                return Agent_setRequestError(reqinfo, requests,
                                                 PRIOT_ERR_NOTWRITABLE);
            }

            oldasp = OldApi_getCurrentAgentSession();
            _OldApi_setCurrentAgentSession(reqinfo->asp);
            status =
                (*(cacheptr->write_method)) (reqinfo->mode,
                                             requests->requestvb->val.
                                             string,
                                             requests->requestvb->type,
                                             requests->requestvb->valLen,
                                             cacheptr->data,
                                             requests->requestvb->name,
                                             requests->requestvb->
                                             nameLength);
            _OldApi_setCurrentAgentSession(oldasp);

            if (status != PRIOT_ERR_NOERROR) {
                Agent_setRequestError(reqinfo, requests, status);
            }

            /*
             * clean up is done by the automatic freeing of the
             * cache stored in the request.
             */

            break;
        }
    }
    return PRIOT_ERR_NOERROR;
}

/** @} */

/*
 * don't use this!
 */
static AgentSession *_oldApi_currentAgentSession = NULL;

AgentSession *
OldApi_getCurrentAgentSession(void)
{
    return _oldApi_currentAgentSession;
}

/*
 * don't use this!
 */
static void
_OldApi_setCurrentAgentSession(AgentSession *asp)
{
    _oldApi_currentAgentSession = asp;
}
