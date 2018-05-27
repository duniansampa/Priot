#include "ScalarGroup.h"
#include "System/Util/Utilities.h"
#include "Instance.h"
#include "System/Util/Trace.h"
#include "Client.h"
#include "Serialize.h"

static ScalarGroup*
_ScalarGroup_cloneScalarGroup(ScalarGroup* src)
{
  ScalarGroup *t = MEMORY_MALLOC_TYPEDEF(ScalarGroup);
  if(t != NULL) {
    t->lbound = src->lbound;
    t->ubound = src->ubound;
  }
  return t;
}

/** @defgroup scalar_group_group scalar_group
 *  Process groups of scalars.
 *  @ingroup leaf
 *  @{
 */
MibHandler *
ScalarGroup_getScalarGroupHandler(oid first, oid last)
{
    MibHandler  *ret    = NULL;
    ScalarGroup *sgroup = NULL;

    ret = AgentHandler_createHandler("scalarGroup",
                                  ScalarGroup_helperHandler);
    if (ret) {
        sgroup = MEMORY_MALLOC_TYPEDEF(ScalarGroup);
        if (NULL == sgroup) {
            AgentHandler_handlerFree(ret);
            ret = NULL;
        }
        else {
        sgroup->lbound = first;
        sgroup->ubound = last;
            ret->myvoid = (void *)sgroup;
            ret->data_free = free;
            ret->data_clone = (void *(*)(void *))_ScalarGroup_cloneScalarGroup;
    }
    }
    return ret;
}

int
ScalarGroup_registerScalarGroup(HandlerRegistration *reginfo,
                              oid first, oid last)
{
    AgentHandler_injectHandler(reginfo, Instance_getInstanceHandler());
    AgentHandler_injectHandler(reginfo, ScalarGroup_getScalarGroupHandler(first, last));
    return Serialize_registerSerialize(reginfo);
}


int
ScalarGroup_helperHandler(MibHandler *handler,
                                HandlerRegistration *reginfo,
                                AgentRequestInfo *reqinfo,
                                RequestInfo *requests)
{
    VariableList *var = requests->requestvb;

    ScalarGroup *sgroup = (ScalarGroup *)handler->myvoid;
    int             ret, cmp;
    int             namelen;
    oid             subid, root_tmp[ASN01_MAX_OID_LEN], *root_save;

    DEBUG_MSGTL(("helper:scalar_group", "Got request:\n"));
    namelen = UTILITIES_MIN_VALUE(requests->requestvb->nameLength,
                       reginfo->rootoid_len);
    cmp = Api_oidCompare(requests->requestvb->name, namelen,
                           reginfo->rootoid, reginfo->rootoid_len);

    DEBUG_MSGTL(( "helper:scalar_group", "  cmp=%d, oid:", cmp));
    DEBUG_MSGOID(("helper:scalar_group", var->name, var->nameLength));
    DEBUG_MSG((   "helper:scalar_group", "\n"));

    /*
     * copy root oid to root_tmp, set instance to 0. (subid set later on)
     * save rootoid, since we'll replace it before calling next handler,
     * and need to restore it afterwards.
     */
    memcpy(root_tmp, reginfo->rootoid, reginfo->rootoid_len * sizeof(oid));
    root_tmp[reginfo->rootoid_len + 1] = 0;
    root_save = reginfo->rootoid;

    ret = PRIOT_ERR_NOCREATION;
    switch (reqinfo->mode) {
    /*
     * The handling of "exact" requests is basically the same.
     * The only difference between GET and SET requests is the
     *     error/exception to return on failure.
     */
    case MODE_GET:
        ret = PRIOT_NOSUCHOBJECT;
        /* Fallthrough */

    case MODE_SET_RESERVE1:
    case MODE_SET_RESERVE2:
    case MODE_SET_ACTION:
    case MODE_SET_COMMIT:
    case MODE_SET_UNDO:
    case MODE_SET_FREE:
        if (cmp != 0 ||
            requests->requestvb->nameLength <= reginfo->rootoid_len) {
        /*
         * Common prefix doesn't match, or only *just* matches
         *  the registered root (so can't possibly match a scalar)
         */
            Agent_setRequestError(reqinfo, requests, ret);
            return PRIOT_ERR_NOERROR;
        } else {
        /*
         * Otherwise,
         *     extract the object subidentifier from the request,
         *     check this is (probably) valid, and then fudge the
         *     registered 'rootoid' to match, before passing the
         *     request off to the next handler ('scalar').
         *
         * Note that we don't bother checking instance subidentifiers
         *     here.  That's left to the scalar helper.
         */
            subid = requests->requestvb->name[reginfo->rootoid_len];
        if (subid < sgroup->lbound ||
            subid > sgroup->ubound) {
                Agent_setRequestError(reqinfo, requests, ret);
                return PRIOT_ERR_NOERROR;
        }
            root_tmp[reginfo->rootoid_len] = subid;
            reginfo->rootoid_len += 2;
            reginfo->rootoid = root_tmp;
            ret = AgentHandler_callNextHandler(handler, reginfo, reqinfo,
                                            requests);
            reginfo->rootoid = root_save;
            reginfo->rootoid_len -= 2;
            return ret;
        }
        break;

    case MODE_GETNEXT:
    /*
     * If we're being asked for something before (or exactly matches)
     *     the registered root OID, then start with the first object.
     * If we're being asked for something that exactly matches an object
     *    OID, then that's what we pass down.
     * Otherwise, we pass down the OID of the *next* object....
     */
        if (cmp < 0 ||
            requests->requestvb->nameLength <= reginfo->rootoid_len) {
            subid  = sgroup->lbound;
        } else if (requests->requestvb->nameLength == reginfo->rootoid_len+1)
            subid = requests->requestvb->name[reginfo->rootoid_len];
        else
            subid = requests->requestvb->name[reginfo->rootoid_len]+1;

    /*
     * ... always assuming this is (potentially) valid, of course.
     */
        if (subid < sgroup->lbound)
            subid = sgroup->lbound;
    else if (subid > sgroup->ubound)
            return PRIOT_ERR_NOERROR;

        root_tmp[reginfo->rootoid_len] = subid;
        reginfo->rootoid_len += 2;
        reginfo->rootoid = root_tmp;
        ret = AgentHandler_callNextHandler(handler, reginfo, reqinfo,
                                            requests);
        /*
         * If we didn't get an answer (due to holes in the group)
     *   set things up to retry again.
         */
        if (!requests->delegated &&
            (requests->requestvb->type == ASN01_NULL ||
             requests->requestvb->type == PRIOT_NOSUCHOBJECT ||
             requests->requestvb->type == PRIOT_NOSUCHINSTANCE)) {
             Client_setVarObjid(requests->requestvb,
                               reginfo->rootoid, reginfo->rootoid_len - 1);
            requests->requestvb->name[reginfo->rootoid_len - 2] = ++subid;
            requests->requestvb->type = ASN01_PRIV_RETRY;
        }
        reginfo->rootoid = root_save;
        reginfo->rootoid_len -= 2;
        return ret;
    }
    /*
     * got here only if illegal mode found
     */
    return PRIOT_ERR_GENERR;
}
