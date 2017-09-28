#include "MasterAdmin.h"
#include "Types.h"
#include "Protocol.h"
#include "Debug.h"
#include "Agent.h"
#include "AgentRegistry.h"
#include "AgentIndex.h"
#include "SysORTable.h"
#include "Master.h"
#include "Client.h"
#include "Trap.h"

/*
 *  AgentX Administrative request handling
 */

Types_Session *
MasterAdmin_findAgentxSession(Types_Session * session, int sessid)
{
    Types_Session *sp;
    for (sp = session->subsession; sp != NULL; sp = sp->next) {
        if (sp->sessid == sessid)
            return sp;
    }
    return NULL;
}


int
MasterAdmin_openAgentxSession(Types_Session * session, Types_Pdu *pdu)
{
    Types_Session *sp;

    DEBUG_MSGTL(("agentx/master", "open %8p\n", session));
    sp = (Types_Session *) malloc(sizeof(Types_Session));
    if (sp == NULL) {
        session->s_snmp_errno = AGENTX_ERR_OPEN_FAILED;
        return -1;
    }

    memcpy(sp, session, sizeof(Types_Session));
    sp->sessid = Api_getNextSessid();
    sp->version = pdu->version;
    sp->timeout = pdu->time;

    /*
     * Be careful with fields: if these aren't zeroed, they will get free()d
     * more than once when the session is closed -- once in the main session,
     * and once in each subsession.  Basically, if it's not being used for
     * some AgentX-specific purpose, it ought to be zeroed here.
     */

    sp->community = NULL;
    sp->peername = NULL;
    sp->contextEngineID = NULL;
    sp->contextName = NULL;
    sp->securityEngineID = NULL;
    sp->securityPrivProto = NULL;

    /*
     * This next bit utilises unused SNMPv3 fields
     *   to store the subagent OID and description.
     * This really ought to use AgentX-specific fields,
     *   but it hardly seems worth it for a one-off use.
     *
     * But I'm willing to be persuaded otherwise....  */
    sp->securityAuthProto = Api_duplicateObjid(pdu->variables->name,
                                                 pdu->variables->
                                                 nameLength);
    sp->securityAuthProtoLen = pdu->variables->nameLength;
    sp->securityName = strdup((char *) pdu->variables->val.string);
    sp->engineTime = (uint32_t)((Agent_getAgentRuntime() + 50) / 100) & 0x7fffffffL;

    sp->subsession = session;   /* link back to head */
    sp->flags |= API_FLAGS_SUBSESSION;
    sp->flags &= ~AGENTX_MSG_FLAG_NETWORK_BYTE_ORDER;
    sp->flags |= (pdu->flags & AGENTX_MSG_FLAG_NETWORK_BYTE_ORDER);
    sp->next = session->subsession;
    session->subsession = sp;
    DEBUG_MSGTL(("agentx/master", "opened %8p = %ld with flags = %02lx\n",
                sp, sp->sessid, sp->flags & AGENTX_MSG_FLAGS_MASK));

    return sp->sessid;
}

int
MasterAdmin_closeAgentxSession(Types_Session * session, int sessid)
{
    Types_Session *sp, **prevNext;

    if (!session)
        return AGENTX_ERR_NOT_OPEN;

    DEBUG_MSGTL(("agentx/master", "close %8p, %d\n", session, sessid));
    if (sessid == -1) {
        /*
         * The following is necessary to avoid locking up the agent when
         * a sugagent dies during a set request. We must clean up the
         * requests, so that the delegated request will be completed and
         * further requests can be processed
         */
        Agent_removeDelegatedRequestsForSession(session);
        if (session->subsession != NULL) {
            Types_Session *subsession = session->subsession;
            for(; subsession; subsession = subsession->next) {
                Agent_removeDelegatedRequestsForSession(subsession);
            }
        }

        AgentRegistry_unregisterMibsBySession(session);
        AgentIndex_unregisterIndexBySession(session);
        SysORTable_unregisterSysORTableBySession(session);
    TOOLS_FREE(session->myvoid);
        return AGENTX_ERR_NOERROR;
    }

    prevNext = &(session->subsession);

    for (sp = session->subsession; sp != NULL; sp = sp->next) {

        if (sp->sessid == sessid) {
            AgentRegistry_unregisterMibsBySession(sp);
            AgentIndex_unregisterIndexBySession(sp);
            SysORTable_unregisterSysORTableBySession(sp);

            *prevNext = sp->next;

            if (sp->securityAuthProto != NULL) {
                free(sp->securityAuthProto);
            }
            if (sp->securityName != NULL) {
                free(sp->securityName);
            }
            free(sp);
            sp = NULL;

            DEBUG_MSGTL(("agentx/master", "closed %8p, %d okay\n",
                        session, sessid));
            return AGENTX_ERR_NOERROR;
        }

        prevNext = &(sp->next);
    }

    DEBUG_MSGTL(("agentx/master", "sessid %d not found\n", sessid));
    return AGENTX_ERR_NOT_OPEN;
}

int
MasterAdmin_registerAgentxList(Types_Session * session, Types_Pdu *pdu)
{
    Types_Session *sp;
    char            buf[128];
    oid             ubound = 0;
    u_long          flags = 0;
    HandlerRegistration *reg;
    int             rc = 0;
    int             cacheid;

    DEBUG_MSGTL(("agentx/master", "in MasterAdmin_registerAgentxList\n"));

    sp = MasterAdmin_findAgentxSession(session, pdu->sessid);
    if (sp == NULL)
        return AGENTX_ERR_NOT_OPEN;

    sprintf(buf, "AgentX subagent %ld, session %8p, subsession %8p",
            sp->sessid, session, sp);
    /*
     * * TODO: registration timeout
     * *   registration context
     */
    if (pdu->range_subid) {
        ubound = pdu->variables->val.objid[pdu->range_subid - 1];
    }

    if (pdu->flags & AGENTX_MSG_FLAG_INSTANCE_REGISTER) {
        flags = FULLY_QUALIFIED_INSTANCE;
    }

    reg = AgentHandler_createHandlerRegistration(buf, Master_handler, pdu->variables->name, pdu->variables->nameLength, HANDLER_CAN_RWRITE | HANDLER_CAN_GETBULK); /* fake it */
    if (!session->myvoid) {
        session->myvoid = malloc(sizeof(cacheid));
        cacheid = Agent_allocateGlobalcacheid();
        *((int *) session->myvoid) = cacheid;
    } else {
        cacheid = *((int *) session->myvoid);
    }

    reg->handler->myvoid = session;
    reg->global_cacheid = cacheid;
    if (NULL != pdu->community)
        reg->contextName = strdup((char *)pdu->community);

    /*
     * register mib. Note that for failure cases, the registration info
     * (reg) will be freed, and thus is no longer a valid pointer.
     */
    switch (AgentRegistry_registerMib2(buf, NULL, 0, 0,
                                 pdu->variables->name,
                                 pdu->variables->nameLength,
                                 pdu->priority, pdu->range_subid, ubound,
                                 sp, (char *) pdu->community, pdu->time,
                                 flags, reg, 1)) {

    case MIB_REGISTERED_OK:
        DEBUG_MSGTL(("agentx/master", "registered ok\n"));
        return AGENTX_ERR_NOERROR;

    case MIB_DUPLICATE_REGISTRATION:
        DEBUG_MSGTL(("agentx/master", "duplicate registration\n"));
        rc = AGENTX_ERR_DUPLICATE_REGISTRATION;
        break;

    case MIB_REGISTRATION_FAILED:
    default:
        rc = AGENTX_ERR_REQUEST_DENIED;
        DEBUG_MSGTL(("agentx/master", "failed registration\n"));
    }
    return rc;
}

int
MasterAdmin_unregisterAgentxList(Types_Session * session, Types_Pdu *pdu)
{
    Types_Session *sp;
    int             rc = 0;

    sp = MasterAdmin_findAgentxSession(session, pdu->sessid);
    if (sp == NULL) {
        return AGENTX_ERR_NOT_OPEN;
    }

    if (pdu->range_subid != 0) {
        oid             ubound =
            pdu->variables->val.objid[pdu->range_subid - 1];
        rc = AgentRegistry_unregisterMibTableRow(pdu->variables->name,
                                              pdu->variables->nameLength,
                                              pdu->priority,
                                              pdu->range_subid, ubound,
                                              (char *) pdu->community);
    } else {
        rc = AgentRegistry_unregisterMibContext(pdu->variables->name,
                                    pdu->variables->nameLength,
                                    pdu->priority, 0, 0,
                                    (char *) pdu->community);
    }

    switch (rc) {
    case MIB_UNREGISTERED_OK:
        return AGENTX_ERR_NOERROR;
    case MIB_NO_SUCH_REGISTRATION:
        return AGENTX_ERR_UNKNOWN_REGISTRATION;
    case MIB_UNREGISTRATION_FAILED:
    default:
        return AGENTX_ERR_REQUEST_DENIED;
    }
}

int
MasterAdmin_allocateIdxList(Types_Session * session, Types_Pdu *pdu)
{
    Types_Session *sp;
    Types_VariableList *vp, *vp2, *next, *res;
    int             flags = 0;

    sp = MasterAdmin_findAgentxSession(session, pdu->sessid);
    if (sp == NULL)
        return AGENTX_ERR_NOT_OPEN;

    if (pdu->flags & AGENTX_MSG_FLAG_ANY_INSTANCE)
        flags |= ALLOCATE_ANY_INDEX;
    if (pdu->flags & AGENTX_MSG_FLAG_NEW_INSTANCE)
        flags |= ALLOCATE_NEW_INDEX;

    /*
     * XXX - what about errors?
     *
     *  If any allocations fail, then we need to
     *    *fully* release the earlier ones.
     *  (i.e. remove them completely from the index registry,
     *    not simply mark them as available for re-use)
     *
     * For now - assume they all succeed.
     */
    for (vp = pdu->variables; vp != NULL; vp = next) {
        next = vp->nextVariable;
        res = AgentIndex_registerIndex(vp, flags, session);
        if (res == NULL) {
            /*
             *  If any allocations fail, we need to *fully* release
             *      all previous ones (i.e. remove them completely
             *      from the index registry)
             */
            for (vp2 = pdu->variables; vp2 != vp; vp2 = vp2->nextVariable) {
                AgentIndex_removeIndex(vp2, session);
            }
            return AGENTX_ERR_INDEX_NONE_AVAILABLE;     /* XXX */
        } else {
            (void) Client_cloneVar(res, vp);
            free(res);
        }
        vp->nextVariable = next;
    }
    return AGENTX_ERR_NOERROR;
}

int
MasterAdmin_releaseIdxList(Types_Session * session, Types_Pdu *pdu)
{
    Types_Session *sp;
    Types_VariableList *vp, *vp2, *rv = NULL;
    int             res;

    sp = MasterAdmin_findAgentxSession(session, pdu->sessid);
    if (sp == NULL)
        return AGENTX_ERR_NOT_OPEN;

    for (vp = pdu->variables; vp != NULL; vp = vp->nextVariable) {
        res = AgentIndex_unregisterIndex(vp, TRUE, session);
        /*
         *  If any releases fail,
         *      we need to reinstate all previous ones.
         */
        if (res != PRIOT_ERR_NOERROR) {
            for (vp2 = pdu->variables; vp2 != vp; vp2 = vp2->nextVariable) {
                rv = AgentIndex_registerIndex(vp2, ALLOCATE_THIS_INDEX, session);
                free(rv);
            }
            return AGENTX_ERR_INDEX_NOT_ALLOCATED;      /* Probably */
        }
    }
    return AGENTX_ERR_NOERROR;
}

int
MasterAdmin_addAgentCapsList(Types_Session * session, Types_Pdu *pdu)
{
    Types_Session *sp;
    char* description;

    sp = MasterAdmin_findAgentxSession(session, pdu->sessid);
    if (sp == NULL)
        return AGENTX_ERR_NOT_OPEN;

    description = Tools_strdupAndNull(pdu->variables->val.string,
                                          pdu->variables->valLen);
    SysORTable_registerSysORTableSess(pdu->variables->name, pdu->variables->nameLength,
                             description, sp);
    free(description);
    return AGENTX_ERR_NOERROR;
}

int
MasterAdmin_removeAgentCapsList(Types_Session * session, Types_Pdu *pdu)
{
    Types_Session *sp;
    int rc;

    sp = MasterAdmin_findAgentxSession(session, pdu->sessid);
    if (sp == NULL)
        return AGENTX_ERR_NOT_OPEN;

    rc = SysORTable_unregisterSysORTableSess(pdu->variables->name,
                                    pdu->variables->nameLength, sp);

    if (rc < 0)
      return AGENTX_ERR_UNKNOWN_AGENTCAPS;

    return AGENTX_ERR_NOERROR;
}

int
MasterAdmin_agentxNotify(Types_Session * session, Types_Pdu *pdu)
{
    Types_Session       *sp;
    Types_VariableList *var;
    extern const oid       sysuptime_oid[], snmptrap_oid[];
    extern const size_t    sysuptime_oid_len, snmptrap_oid_len;

    sp = MasterAdmin_findAgentxSession(session, pdu->sessid);
    if (sp == NULL)
        return AGENTX_ERR_NOT_OPEN;

    var = pdu->variables;
    if (!var)
        return AGENTX_ERR_PROCESSING_ERROR;

    if (Api_oidCompare(var->name, var->nameLength,
                         sysuptime_oid, sysuptime_oid_len) == 0) {
        var = var->nextVariable;
    }

    if (!var || Api_oidCompare(var->name, var->nameLength,
                                 snmptrap_oid, snmptrap_oid_len) != 0)
        return AGENTX_ERR_PROCESSING_ERROR;

    /*
     *  If sysUptime isn't the first varbind, don't worry.
     *     Trap_sendTrapVars() will add it if necessary.
     *
     *  Note that if this behaviour is altered, it will
     *     be necessary to add sysUptime here,
     *     as this is valid AgentX syntax.
     */

    /* If a context name was specified, send the trap using that context.
     * Otherwise, send the trap without the context using the old method */
    if (pdu->contextName != NULL)
    {
        Trap_sendTrapVarsWithContext(-1, -1, pdu->variables,
                       pdu->contextName);
    }
    else
    {
        Trap_sendTrapVars(-1, -1, pdu->variables);
    }

    return AGENTX_ERR_NOERROR;
}


int
MasterAdmin_agentxPingResponse(Types_Session * session, Types_Pdu *pdu)
{
    Types_Session *sp;

    sp = MasterAdmin_findAgentxSession(session, pdu->sessid);
    if (sp == NULL)
        return AGENTX_ERR_NOT_OPEN;
    else
        return AGENTX_ERR_NOERROR;
}

int
MasterAdmin_handleMasterAgentxPacket(int operation,
                            Types_Session * session,
                            int reqid, Types_Pdu *pdu, void *magic)
{
    AgentSession *asp;

    if (operation == API_CALLBACK_OP_DISCONNECT) {
        DEBUG_MSGTL(("agentx/master",
                    "transport disconnect on session %8p\n", session));
        /*
         * Shut this session down gracefully.
         */
        MasterAdmin_closeAgentxSession(session, -1);
        return 1;
    } else if (operation == API_CALLBACK_OP_CONNECT) {
        DEBUG_MSGTL(("agentx/master",
                    "transport connect on session %8p\n", session));
        return 1;
    } else if (operation != API_CALLBACK_OP_RECEIVED_MESSAGE) {
        DEBUG_MSGTL(("agentx/master", "unexpected callback op %d\n",
                    operation));
        return 1;
    }

    /*
     * Okay, it's a API_CALLBACK_OP_RECEIVED_MESSAGE op.
     */

    if (magic) {
        asp = (AgentSession *) magic;
    } else {
        asp = Agent_initAgentPriotSession(session, pdu);
    }

    DEBUG_MSGTL(("agentx/master", "handle pdu (req=0x%lx,trans=0x%lx,sess=0x%lx)\n",
                (unsigned long)pdu->reqid, (unsigned long)pdu->transid,
        (unsigned long)pdu->sessid));

    switch (pdu->command) {
    case AGENTX_MSG_OPEN:
        asp->pdu->sessid = MasterAdmin_openAgentxSession(session, pdu);
        if (asp->pdu->sessid == -1)
            asp->status = session->s_snmp_errno;
        break;

    case AGENTX_MSG_CLOSE:
        asp->status = MasterAdmin_closeAgentxSession(session, pdu->sessid);
        break;

    case AGENTX_MSG_REGISTER:
        asp->status = MasterAdmin_registerAgentxList(session, pdu);
        break;

    case AGENTX_MSG_UNREGISTER:
        asp->status = MasterAdmin_unregisterAgentxList(session, pdu);
        break;

    case AGENTX_MSG_INDEX_ALLOCATE:
        asp->status = MasterAdmin_allocateIdxList(session, asp->pdu);
        if (asp->status != AGENTX_ERR_NOERROR) {
            Api_freePdu(asp->pdu);
            asp->pdu = Client_clonePdu(pdu);
        }
        break;

    case AGENTX_MSG_INDEX_DEALLOCATE:
        asp->status = MasterAdmin_releaseIdxList(session, pdu);
        break;

    case AGENTX_MSG_ADD_AGENT_CAPS:
        asp->status = MasterAdmin_addAgentCapsList(session, pdu);
        break;

    case AGENTX_MSG_REMOVE_AGENT_CAPS:
        asp->status = MasterAdmin_removeAgentCapsList(session, pdu);
        break;

    case AGENTX_MSG_NOTIFY:
        asp->status = MasterAdmin_agentxNotify(session, pdu);
        break;

    case AGENTX_MSG_PING:
        asp->status = MasterAdmin_agentxPingResponse(session, pdu);
        break;

        /*
         * TODO: Other admin packets
         */

    case AGENTX_MSG_GET:
    case AGENTX_MSG_GETNEXT:
    case AGENTX_MSG_GETBULK:
    case AGENTX_MSG_TESTSET:
    case AGENTX_MSG_COMMITSET:
    case AGENTX_MSG_UNDOSET:
    case AGENTX_MSG_CLEANUPSET:
    case AGENTX_MSG_RESPONSE:
        /*
         * Shouldn't be handled here
         */
        break;

    default:
        asp->status = AGENTX_ERR_PARSE_FAILED;
        break;
    }

    asp->pdu->time = Agent_getAgentUptime();
    asp->pdu->command = AGENTX_MSG_RESPONSE;
    asp->pdu->errstat = asp->status;
    DEBUG_MSGTL(("agentx/master", "send response, stat %d (req=0x%lx,trans="
                "0x%lx,sess=0x%lx)\n",
                asp->status, (unsigned long)pdu->reqid,
        (unsigned long)pdu->transid, (unsigned long)pdu->sessid));
    if (!Api_send(asp->session, asp->pdu)) {
        char           *eb = NULL;
        int             pe, pse;
        Api_error(asp->session, &pe, &pse, &eb);
        Api_freePdu(asp->pdu);
        DEBUG_MSGTL(("agentx/master", "FAILED %d %d %s\n", pe, pse, eb));
        free(eb);
    }
    asp->pdu = NULL;
    Agent_freeAgentPriotSession(asp);

    return 1;
}


