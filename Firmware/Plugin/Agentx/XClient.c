#include "XClient.h"
#include "Debug.h"
#include "Client.h"
#include "../Plugin/Agentx/Subagent.h"
#include "../Plugin/Agentx/Protocol.h"
#include "Agent.h"
#include "DefaultStore.h"
#include "DsAgent.h"
#include "VarStruct.h"
#include "Logger.h"
#include "AgentIndex.h"

/*
 *   AgentX utility routines
 */

/*
 * AgentX handling utility routines
 *
 * Mostly wrappers round, or re-writes of
 *   the SNMP equivalents
 */

int
XClient_synchInput(int op,
                   Types_Session * session,
                   int reqid, Types_Pdu *pdu, void *magic)
{
    struct Client_SynchState_s *state = (struct Client_SynchState_s *) magic;

    if (!state || reqid != state->reqid) {
        return Subagent_handleAgentxPacket(op, session, reqid, pdu, magic);
    }

    DEBUG_MSGTL(("agentx/subagent", "synching input, op 0x%02x\n", op));
    state->waiting = 0;
    if (op == API_CALLBACK_OP_RECEIVED_MESSAGE) {
        if (pdu->command == AGENTX_MSG_RESPONSE) {
            state->pdu = Client_clonePdu(pdu);
            state->status = CLIENT_STAT_SUCCESS;
            session->s_snmp_errno = ErrorCode_SUCCESS;

            /*
     * Synchronise sysUpTime with the master agent
     */
            Agent_setAgentUptime(pdu->time);
        }
    } else if (op == API_CALLBACK_OP_TIMED_OUT) {
        state->pdu = NULL;
        state->status = CLIENT_STAT_TIMEOUT;
        session->s_snmp_errno = ErrorCode_TIMEOUT;
    } else if (op == API_CALLBACK_OP_DISCONNECT) {
        return Subagent_handleAgentxPacket(op, session, reqid, pdu, magic);
    }

    return 1;
}



int
XClient_synchResponse(Types_Session * ss, Types_Pdu *pdu,
                      Types_Pdu **response)
{
    return Client_synchResponseCb(ss, pdu, response, XClient_synchInput);
}


/*
 * AgentX PofE convenience functions
 */

int
XClient_openSession(Types_Session * ss)
{
    Types_Pdu    *pdu, *response;
    extern oid      version_sysoid[];
    extern int      version_sysoid_len;
    u_long 	    timeout;

    DEBUG_MSGTL(("agentx/subagent", "opening session \n"));
    if (ss == NULL || !IS_AGENTX_VERSION(ss->version)) {
        return 0;
    }

    pdu = Client_pduCreate(AGENTX_MSG_OPEN);
    if (pdu == NULL)
        return 0;
    timeout = DefaultStore_getInt( DsStorage_APPLICATION_ID,
                                   DsAgentInterger_AGENTX_TIMEOUT);
    if (timeout < 0)
        pdu->time = 0;
    else
        /* for master TIMEOUT is usec, but Agentx Open specifies sec */
        pdu->time = timeout/ONE_SEC;

    Api_addVar(pdu, version_sysoid, version_sysoid_len,
                 's', "PRIOT AgentX sub-agent");

    if (XClient_synchResponse(ss, pdu, &response) != CLIENT_STAT_SUCCESS)
        return 0;

    if (!response)
        return 0;

    if (response->errstat != PRIOT_ERR_NOERROR) {
        Api_freePdu(response);
        return 0;
    }

    ss->sessid = response->sessid;
    Api_freePdu(response);

    DEBUG_MSGTL(("agentx/subagent", "open \n"));
    return 1;
}

int
XClient_closeSession(Types_Session * ss, int why)
{
    Types_Pdu    *pdu, *response;
    DEBUG_MSGTL(("agentx/subagent", "closing session\n"));

    if (ss == NULL || !IS_AGENTX_VERSION(ss->version)) {
        return 0;
    }

    pdu = Client_pduCreate(AGENTX_MSG_CLOSE);
    if (pdu == NULL)
        return 0;
    pdu->time = 0;
    pdu->errstat = why;
    pdu->sessid = ss->sessid;

    if (XClient_synchResponse(ss, pdu, &response) == CLIENT_STAT_SUCCESS)
        Api_freePdu(response);
    DEBUG_MSGTL(("agentx/subagent", "closed\n"));

    return 1;
}

int
XClient_register(Types_Session * ss, oid start[], size_t startlen,
                int priority, int range_subid, oid range_ubound,
                int timeout, u_char flags, const char *contextName)
{
    Types_Pdu    *pdu, *response;

    DEBUG_MSGTL(("agentx/subagent", "registering: "));
    DEBUG_MSGOIDRANGE(("agentx/subagent", start, startlen, range_subid,
                      range_ubound));
    DEBUG_MSG(("agentx/subagent", "\n"));

    if (ss == NULL || !IS_AGENTX_VERSION(ss->version)) {
        return 0;
    }

    pdu = Client_pduCreate(AGENTX_MSG_REGISTER);
    if (pdu == NULL) {
        return 0;
    }
    pdu->time = timeout;
    pdu->priority = priority;
    pdu->sessid = ss->sessid;
    pdu->range_subid = range_subid;
    if (contextName) {
        pdu->flags |= AGENTX_MSG_FLAG_NON_DEFAULT_CONTEXT;
        pdu->community = (u_char *) strdup(contextName);
        pdu->communityLen = strlen(contextName);
    }

    if (flags & FULLY_QUALIFIED_INSTANCE) {
        pdu->flags |= AGENTX_MSG_FLAG_INSTANCE_REGISTER;
    }

    if (range_subid) {
        Api_pduAddVariable(pdu, start, startlen, ASN01_OBJECT_ID,
                              (u_char *) start, startlen * sizeof(oid));
        pdu->variables->val.objid[range_subid - 1] = range_ubound;
    } else {
        Client_addNullVar(pdu, start, startlen);
    }

    if (XClient_synchResponse(ss, pdu, &response) != CLIENT_STAT_SUCCESS) {
        DEBUG_MSGTL(("agentx/subagent", "registering failed!\n"));
        return 0;
    }

    if (response->errstat != PRIOT_ERR_NOERROR) {
        Logger_log(LOGGER_PRIORITY_ERR,"registering pdu failed: %ld!\n", response->errstat);
        Api_freePdu(response);
        return 0;
    }

    Api_freePdu(response);
    DEBUG_MSGTL(("agentx/subagent", "registered\n"));
    return 1;
}

int
XClient_unregister(Types_Session * ss, oid start[], size_t startlen,
                  int priority, int range_subid, oid range_ubound,
                  const char *contextName)
{
    Types_Pdu    *pdu, *response;

    if (ss == NULL || !IS_AGENTX_VERSION(ss->version)) {
        return 0;
    }

    DEBUG_MSGTL(("agentx/subagent", "unregistering: "));
    DEBUG_MSGOIDRANGE(("agentx/subagent", start, startlen, range_subid,
                      range_ubound));
    DEBUG_MSG(("agentx/subagent", "\n"));
    pdu = Client_pduCreate(AGENTX_MSG_UNREGISTER);
    if (pdu == NULL) {
        return 0;
    }
    pdu->time = 0;
    pdu->priority = priority;
    pdu->sessid = ss->sessid;
    pdu->range_subid = range_subid;
    if (contextName) {
        pdu->flags |= AGENTX_MSG_FLAG_NON_DEFAULT_CONTEXT;
        pdu->community = (u_char *) strdup(contextName);
        pdu->communityLen = strlen(contextName);
    }

    if (range_subid) {
        Api_pduAddVariable(pdu, start, startlen, ASN01_OBJECT_ID,
                              (u_char *) start, startlen * sizeof(oid));
        pdu->variables->val.objid[range_subid - 1] = range_ubound;
    } else {
        Client_addNullVar(pdu, start, startlen);
    }

    if (XClient_synchResponse(ss, pdu, &response) != CLIENT_STAT_SUCCESS)
        return 0;

    if (response->errstat != PRIOT_ERR_NOERROR) {
        Api_freePdu(response);
        return 0;
    }

    Api_freePdu(response);
    DEBUG_MSGTL(("agentx/subagent", "unregistered\n"));
    return 1;
}

Types_VariableList *
XClient_registerIndex(Types_Session * ss,
                      Types_VariableList * varbind, int flags)
{
    Types_Pdu    *pdu, *response;
    Types_VariableList *varbind2;

    if (ss == NULL || !IS_AGENTX_VERSION(ss->version)) {
        return NULL;
    }

    /*
* Make a copy of the index request varbind
*    for the AgentX request PDU
*    (since the pdu structure will be freed)
*/
    varbind2 =
            (Types_VariableList *) malloc(sizeof(Types_VariableList));
    if (varbind2 == NULL)
        return NULL;
    if (Client_cloneVar(varbind, varbind2)) {
        Api_freeVarbind(varbind2);
        return NULL;
    }
    if (varbind2->val.string == NULL)
        varbind2->val.string = varbind2->buf;   /* ensure it points somewhere */

    pdu = Client_pduCreate(AGENTX_MSG_INDEX_ALLOCATE);
    if (pdu == NULL) {
        Api_freeVarbind(varbind2);
        return NULL;
    }
    pdu->time = 0;
    pdu->sessid = ss->sessid;
    if (flags == ALLOCATE_ANY_INDEX)
        pdu->flags |= AGENTX_MSG_FLAG_ANY_INSTANCE;
    if (flags == ALLOCATE_NEW_INDEX)
        pdu->flags |= AGENTX_MSG_FLAG_NEW_INSTANCE;

    /*
*  Just send a single index request varbind.
*  Although the AgentX protocol supports
*    multiple index allocations in a single
*    request, the model used in the net-snmp agent
*    doesn't currently take advantage of this.
*  I believe this is our prerogative - just as
*    long as the master side Index request handler
*    can cope with multiple index requests.
*/
    pdu->variables = varbind2;

    if (XClient_synchResponse(ss, pdu, &response) != CLIENT_STAT_SUCCESS)
        return NULL;

    if (response->errstat != PRIOT_ERR_NOERROR) {
        Api_freePdu(response);
        return NULL;
    }

    /*
* Unlink the (single) response varbind to return
*  to the main driving index request routine.
*
* This is a memory leak, as nothing will ever
*  release this varbind.  If this becomes a problem,
*  we'll need to keep a list of these here, and
*  free the memory in the "index release" routine.
* But the master side never frees these either (by
*  design, since it still needs them), so expecting
*  the subagent to is discrimination, pure & simple :-)
*/
    varbind2 = response->variables;
    response->variables = NULL;
    Api_freePdu(response);
    return varbind2;
}

int
XClient_unregisterIndex(Types_Session * ss,
                        Types_VariableList * varbind)
{
    Types_Pdu    *pdu, *response;
    Types_VariableList *varbind2;

    if (ss == NULL || !IS_AGENTX_VERSION(ss->version)) {
        return -1;
    }

    /*
* Make a copy of the index request varbind
*    for the AgentX request PDU
*    (since the pdu structure will be freed)
*/
    varbind2 =
            (Types_VariableList *) malloc(sizeof(Types_VariableList));
    if (varbind2 == NULL)
        return -1;
    if (Client_cloneVar(varbind, varbind2)) {
        Api_freeVarbind(varbind2);
        return -1;
    }

    pdu = Client_pduCreate(AGENTX_MSG_INDEX_DEALLOCATE);
    if (pdu == NULL) {
        Api_freeVarbind(varbind2);
        return -1;
    }
    pdu->time = 0;
    pdu->sessid = ss->sessid;

    /*
*  Just send a single index release varbind.
*      (as above)
*/
    pdu->variables = varbind2;

    if (XClient_synchResponse(ss, pdu, &response) != CLIENT_STAT_SUCCESS)
        return -1;

    if (response->errstat != PRIOT_ERR_NOERROR) {
        Api_freePdu(response);
        return -1;              /* XXX - say why */
    }

    Api_freePdu(response);
    return PRIOT_ERR_NOERROR;
}

int
XClient_addAgentcaps(Types_Session * ss,
                     const oid * agent_cap, size_t agent_caplen,
                     const char *descr)
{
    Types_Pdu    *pdu, *response;

    if (ss == NULL || !IS_AGENTX_VERSION(ss->version)) {
        return 0;
    }

    pdu = Client_pduCreate(AGENTX_MSG_ADD_AGENT_CAPS);
    if (pdu == NULL)
        return 0;
    pdu->time = 0;
    pdu->sessid = ss->sessid;
    Api_addVar(pdu, agent_cap, agent_caplen, 's', descr);

    if (XClient_synchResponse(ss, pdu, &response) != CLIENT_STAT_SUCCESS)
        return 0;

    if (response->errstat != PRIOT_ERR_NOERROR) {
        Api_freePdu(response);
        return 0;
    }

    Api_freePdu(response);
    return 1;
}

int
XClient_removeAgentcaps(Types_Session * ss,
                        const oid * agent_cap, size_t agent_caplen)
{
    Types_Pdu    *pdu, *response;

    if (ss == NULL || !IS_AGENTX_VERSION(ss->version)) {
        return 0;
    }

    pdu = Client_pduCreate(AGENTX_MSG_REMOVE_AGENT_CAPS);
    if (pdu == NULL)
        return 0;
    pdu->time = 0;
    pdu->sessid = ss->sessid;
    Client_addNullVar(pdu, agent_cap, agent_caplen);

    if (XClient_synchResponse(ss, pdu, &response) != CLIENT_STAT_SUCCESS)
        return 0;

    if (response->errstat != PRIOT_ERR_NOERROR) {
        Api_freePdu(response);
        return 0;
    }

    Api_freePdu(response);
    return 1;
}

int
XClient_sendPing(Types_Session * ss)
{
    Types_Pdu    *pdu, *response;

    if (ss == NULL || !IS_AGENTX_VERSION(ss->version)) {
        return 0;
    }

    pdu = Client_pduCreate(AGENTX_MSG_PING);
    if (pdu == NULL)
        return 0;
    pdu->time = 0;
    pdu->sessid = ss->sessid;

    if (XClient_synchResponse(ss, pdu, &response) != CLIENT_STAT_SUCCESS)
        return 0;

    if (response->errstat != PRIOT_ERR_NOERROR) {
        Api_freePdu(response);
        return 0;
    }

    Api_freePdu(response);
    return 1;
}
