#include "Subagent.h"
#include "PriotSettings.h"
#include "Debug.h"
#include "Logger.h"
#include "Assert.h"
#include "DefaultStore.h"
#include "DsAgent.h"
#include "Callback.h"
#include "Priot.h"
#include "Agent.h"
#include "Client.h"
#include "Protocol.h"
#include "Api.h"
#include "Callback.h"
#include "AgentCallbacks.h"
#include "Trap.h"
#include "AgentRegistry.h"
#include "Alarm.h"
#include "Vars.h"
#include "SysORTable.h"
#include "Version.h"
#include "Agentx/XClient.h"
#include "Transports/CallbackDomain.h"

static Callback_CallbackFT _Subagent_registerPingAlarm;
static Alarm_CallbackFT _Subagent_reopenSession;

void            Subagent_registerCallbacks(Types_Session * s);
void            Subagent_unregisterCallbacks(Types_Session * ss);
int             Subagent_handleSubagentResponse(int op, Types_Session * session,
                                         int reqid, Types_Pdu *pdu,
                                         void *magic);
int             Subagent_handleSubagentSetResponse(int op,
                                             Types_Session * session,
                                             int reqid, Types_Pdu *pdu,
                                             void *magic);

void            Subagent_startupCallback(unsigned int clientreg,
                                          void *clientarg);
int             Subagent_openMasterSession(void);

typedef struct NsSubagentMagic_s {
    int  original_command;
    Types_Session *session;
    Types_VariableList *ovars;
} NsSubagentMagic;

struct AgentSetInfo_s {
    int             transID;
    int             mode;
    int             errstat;
    time_t          uptime;
    Types_Session *sess;
    Types_VariableList *var_list;

    struct AgentSetInfo_s *next;
};

static struct AgentSetInfo_s * _subagent_sets = NULL;

Types_Session *subagent_agentxCallbackSess = NULL;
extern int      vars_callbackMasterNum;
extern Types_Session *agent_mainSession;   /* from snmp_agent.c */

int
Subagent_startup(int majorID, int minorID,
                             void *serverarg, void *clientarg)
{
    DEBUG_MSGTL(("agentx/subagent", "connecting to master...\n"));
    /*
     * if a valid ping interval has been defined, call _Subagent_reopenSession
     * to try to connect to master or setup a ping alarm if it couldn't
     * succeed. if no ping interval was set up, just try to connect once.
     */
    if (DefaultStore_getInt(DsStorage_APPLICATION_ID,
                            DsAgentInterger_AGENTX_PING_INTERVAL) > 0)
        _Subagent_reopenSession(0, NULL);
    else {
        Subagent_openMasterSession();
    }
    return 0;
}

static void
Subagent_initCallbackSession(void)
{
    if (subagent_agentxCallbackSess == NULL) {
        subagent_agentxCallbackSess = CallbackDomain_open(vars_callbackMasterNum,
                                                     Subagent_handleSubagentResponse,
                                                     NULL, NULL);
        DEBUG_MSGTL(("agentx/subagent", "subagent_init sess %p\n",
                    subagent_agentxCallbackSess));
    }
}

static int _subagent_initInit = 0;
/**
 * init subagent callback (local) session and connect to master agent
 *
 * @returns 0 for success, !0 otherwise
 */
int
Subagent_init(void)
{
    int rc = 0;

    DEBUG_MSGTL(("agentx/subagent", "initializing....\n"));

    if (++_subagent_initInit != 1)
        return 0;

    Assert_assert(DefaultStore_getBoolean(DsStorage_APPLICATION_ID,
                                          DsAgentBoolean_ROLE) == SUB_AGENT);


    /*
     * open (local) callback session
     */
    Subagent_initCallbackSession();
    if (NULL == subagent_agentxCallbackSess)
        return -1;

    Callback_registerCallback(CALLBACK_LIBRARY,
                           CALLBACK_POST_READ_CONFIG,
                           Subagent_startup, NULL);

    DEBUG_MSGTL(("agentx/subagent", "initializing....  DONE\n"));

    return rc;
}

void
Subagent_enableSubagent(void) {
    DefaultStore_setBoolean(DsStorage_APPLICATION_ID, DsAgentBoolean_ROLE,
                           SUB_AGENT);
}

struct AgentSetInfo_s *
Subagent_saveSetVars(Types_Session * ss, Types_Pdu *pdu)
{
    struct AgentSetInfo_s *ptr;

    ptr = (struct AgentSetInfo_s *)
        malloc(sizeof(struct AgentSetInfo_s));
    if (ptr == NULL)
        return NULL;

    /*
     * Save the important information
     */
    ptr->transID = pdu->transid;
    ptr->sess = ss;
    ptr->mode = PRIOT_MSG_INTERNAL_SET_RESERVE1;
    ptr->uptime = Agent_getAgentUptime();

    ptr->var_list = Client_cloneVarbind(pdu->variables);
    if (ptr->var_list == NULL) {
        free(ptr);
        return NULL;
    }

    ptr->next = _subagent_sets;
    _subagent_sets = ptr;

    return ptr;
}

struct AgentSetInfo_s *
Subagent_restoreSetVars(Types_Session * sess, Types_Pdu *pdu)
{
    struct AgentSetInfo_s *ptr;

    for (ptr = _subagent_sets; ptr != NULL; ptr = ptr->next)
        if (ptr->sess == sess && ptr->transID == pdu->transid)
            break;

    if (ptr == NULL || ptr->var_list == NULL)
        return NULL;

    pdu->variables = Client_cloneVarbind(ptr->var_list);
    if (pdu->variables == NULL)
        return NULL;

    return ptr;
}


void
Subagent_freeSetVars(Types_Session * ss, Types_Pdu *pdu)
{
    struct AgentSetInfo_s *ptr, *prev = NULL;

    for (ptr = _subagent_sets; ptr != NULL; ptr = ptr->next) {
        if (ptr->sess == ss && ptr->transID == pdu->transid) {
            if (prev)
                prev->next = ptr->next;
            else
                _subagent_sets = ptr->next;
            Api_freeVarbind(ptr->var_list);
            free(ptr);
            return;
        }
        prev = ptr;
    }
}

static void
_Subagent_sendAgentxError(Types_Session *session, Types_Pdu *pdu, int errstat, int errindex)
{
    pdu = Client_clonePdu(pdu);
    pdu->command   = AGENTX_MSG_RESPONSE;
    pdu->version   = session->version;
    pdu->errstat   = errstat;
    pdu->errindex  = errindex;
    Api_freeVarbind(pdu->variables);
    pdu->variables = NULL;

    DEBUG_MSGTL(("agentx/subagent", "Sending AgentX response error stat %d idx %d\n",
             errstat, errindex));
    if (!Api_send(session, pdu)) {
        Api_freePdu(pdu);
    }
}

int
Subagent_handleAgentxPacket(int operation, Types_Session * session, int reqid,
                     Types_Pdu *pdu, void *magic)
{
    struct AgentSetInfo_s *asi = NULL;
   Types_CallbackFT   mycallback;
    Types_Pdu    *internal_pdu = NULL;
    void           *retmagic = NULL;
    NsSubagentMagic *smagic = NULL;
    int             result;

    if (operation == API_CALLBACK_OP_DISCONNECT) {
        struct Client_SynchState_s *state = (struct Client_SynchState_s *) magic;
        int             period =
            DefaultStore_getInt(DsStorage_APPLICATION_ID,
                                DsAgentInterger_AGENTX_PING_INTERVAL);
        DEBUG_MSGTL(("agentx/subagent",
                    "transport disconnect indication\n"));

        /*
         * deal with existing session. This happend if agentx sends
         * a message to the master, but the master goes away before
         * a response is sent. agentx will spin in snmp_synch_response_cb,
         * waiting for a response. At the very least, the waiting
         * flag must be set to break that loop. The rest is copied
         * from disconnect handling in snmp_sync_input.
         */
        if(state) {
            state->waiting = 0;
            state->pdu = NULL;
            state->status = CLIENT_STAT_ERROR;
            session->s_snmp_errno = ErrorCode_ABORT;
            API_SET_PRIOT_ERROR(ErrorCode_ABORT);
        }

        /*
         * Deregister the ping alarm, if any, and invalidate all other
         * references to this session.
         */
        if (session->securityModel != API_DEFAULT_SECMODEL) {
            Alarm_unregister(session->securityModel);
        }
        Callback_callCallbacks(CALLBACK_APPLICATION,
                            PriotdCallback_INDEX_STOP, (void *) session);
        Subagent_unregisterCallbacks(session);
        Trap_removeTrapSession(session);
        AgentRegistry_registerMibDetach();
        agent_mainSession = NULL;
        if (period != 0) {
            /*
             * Pings are enabled, so periodically attempt to re-establish contact
             * with the master agent.  Don't worry about the handle,
             * _Subagent_reopenSession unregisters itself if it succeeds in talking
             * to the master agent.
             */
            Alarm_register(period, ALARM_SA_REPEAT, _Subagent_reopenSession, NULL);
            Logger_log(LOGGER_PRIORITY_INFO, "AgentX master disconnected us, reconnecting in %d\n", period);
        } else {
            Logger_log(LOGGER_PRIORITY_INFO, "AgentX master disconnected us, not reconnecting\n");
        }
        return 0;
    } else if (operation != API_CALLBACK_OP_RECEIVED_MESSAGE) {
        DEBUG_MSGTL(("agentx/subagent", "unexpected callback op %d\n",
                    operation));
        return 1;
    }

    /*
     * ok, we have a pdu from the net. Modify as needed
     */

    DEBUG_MSGTL(("agentx/subagent", "handling AgentX request (req=0x%x,trans="
                "0x%x,sess=0x%x)\n", (unsigned)pdu->reqid,
        (unsigned)pdu->transid, (unsigned)pdu->sessid));
    pdu->version = AGENTX_VERSION_1;
    pdu->flags |= PRIOT_UCD_MSG_FLAG_ALWAYS_IN_VIEW;

    /* Master agent is alive, no need to ping */
    if (session->securityModel != API_DEFAULT_SECMODEL) {
        Alarm_reset(session->securityModel);
    }

    if (pdu->command == AGENTX_MSG_GET
        || pdu->command == AGENTX_MSG_GETNEXT
        || pdu->command == AGENTX_MSG_GETBULK) {
        smagic =
            (NsSubagentMagic *) calloc(1, sizeof(NsSubagentMagic));
        if (smagic == NULL) {
            DEBUG_MSGTL(("agentx/subagent", "couldn't malloc() smagic\n"));
            /* would like to _Subagent_sendAgentxError(), but it needs memory too */
            return 1;
        }
        smagic->original_command = pdu->command;
        smagic->session = session;
        smagic->ovars = NULL;
        retmagic = (void *) smagic;
    }

    switch (pdu->command) {
    case AGENTX_MSG_GET:
        DEBUG_MSGTL(("agentx/subagent", "  -> get\n"));
        pdu->command = PRIOT_MSG_GET;
        mycallback = Subagent_handleSubagentResponse;
        break;

    case AGENTX_MSG_GETNEXT:
        DEBUG_MSGTL(("agentx/subagent", "  -> getnext\n"));
        pdu->command = PRIOT_MSG_GETNEXT;

        /*
         * We have to save a copy of the original variable list here because
         * if the master agent has requested scoping for some of the varbinds
         * that information is stored there.
         */

        smagic->ovars = Client_cloneVarbind(pdu->variables);
        DEBUG_MSGTL(("agentx/subagent", "saved variables\n"));
        mycallback = Subagent_handleSubagentResponse;
        break;

    case AGENTX_MSG_GETBULK:
        /*
         * WWWXXX
         */
        DEBUG_MSGTL(("agentx/subagent", "  -> getbulk\n"));
        pdu->command = PRIOT_MSG_GETBULK;

        /*
         * We have to save a copy of the original variable list here because
         * if the master agent has requested scoping for some of the varbinds
         * that information is stored there.
         */

        smagic->ovars = Client_cloneVarbind(pdu->variables);
        DEBUG_MSGTL(("agentx/subagent", "saved variables at %p\n",
                    smagic->ovars));
        mycallback = Subagent_handleSubagentResponse;
        break;

    case AGENTX_MSG_RESPONSE:
        TOOLS_FREE(smagic);
        DEBUG_MSGTL(("agentx/subagent", "  -> response\n"));
        return 1;

    case AGENTX_MSG_TESTSET:
        /*
         * XXXWWW we have to map this twice to both RESERVE1 and RESERVE2
         */
        DEBUG_MSGTL(("agentx/subagent", "  -> testset\n"));
        asi = Subagent_saveSetVars(session, pdu);
        if (asi == NULL) {
            TOOLS_FREE(smagic);
            Logger_log(LOGGER_PRIORITY_WARNING, "Subagent_saveSetVars() failed\n");
            _Subagent_sendAgentxError(session, pdu, AGENTX_ERR_PARSE_FAILED, 0);
            return 1;
        }
        asi->mode = pdu->command = PRIOT_MSG_INTERNAL_SET_RESERVE1;
        mycallback = Subagent_handleSubagentSetResponse;
        retmagic = asi;
        break;

    case AGENTX_MSG_COMMITSET:
        DEBUG_MSGTL(("agentx/subagent", "  -> commitset\n"));
        asi = Subagent_restoreSetVars(session, pdu);
        if (asi == NULL) {
            TOOLS_FREE(smagic);
            Logger_log(LOGGER_PRIORITY_WARNING, "Subagent_restoreSetVars() failed\n");
            _Subagent_sendAgentxError(session, pdu, AGENTX_ERR_PROCESSING_ERROR, 0);
            return 1;
        }
        if (asi->mode != PRIOT_MSG_INTERNAL_SET_RESERVE2) {
            TOOLS_FREE(smagic);
            Logger_log(LOGGER_PRIORITY_WARNING,
                     "dropping bad AgentX request (wrong mode %d)\n",
                     asi->mode);
            _Subagent_sendAgentxError(session, pdu, AGENTX_ERR_PROCESSING_ERROR, 0);
            return 1;
        }
        asi->mode = pdu->command = PRIOT_MSG_INTERNAL_SET_ACTION;
        mycallback = Subagent_handleSubagentSetResponse;
        retmagic = asi;
        break;

    case AGENTX_MSG_CLEANUPSET:
        DEBUG_MSGTL(("agentx/subagent", "  -> cleanupset\n"));
        asi = Subagent_restoreSetVars(session, pdu);
        if (asi == NULL) {
            TOOLS_FREE(smagic);
            Logger_log(LOGGER_PRIORITY_WARNING, "Subagent_restoreSetVars() failed\n");
            _Subagent_sendAgentxError(session, pdu, AGENTX_ERR_PROCESSING_ERROR, 0);
            return 1;
        }
        if (asi->mode == PRIOT_MSG_INTERNAL_SET_RESERVE1 ||
            asi->mode == PRIOT_MSG_INTERNAL_SET_RESERVE2) {
            asi->mode = pdu->command = PRIOT_MSG_INTERNAL_SET_FREE;
        } else if (asi->mode == PRIOT_MSG_INTERNAL_SET_ACTION) {
            asi->mode = pdu->command = PRIOT_MSG_INTERNAL_SET_COMMIT;
        } else {
            Logger_log(LOGGER_PRIORITY_WARNING,
                     "dropping bad AgentX request (wrong mode %d)\n",
                     asi->mode);
            TOOLS_FREE(retmagic);
            return 1;
        }
        mycallback = Subagent_handleSubagentSetResponse;
        retmagic = asi;
        break;

    case AGENTX_MSG_UNDOSET:
        DEBUG_MSGTL(("agentx/subagent", "  -> undoset\n"));
        asi = Subagent_restoreSetVars(session, pdu);
        if (asi == NULL) {
            TOOLS_FREE(smagic);
            Logger_log(LOGGER_PRIORITY_WARNING, "Subagent_restoreSetVars() failed\n");
            _Subagent_sendAgentxError(session, pdu, AGENTX_ERR_PROCESSING_ERROR, 0);
            return 1;
        }
        asi->mode = pdu->command = PRIOT_MSG_INTERNAL_SET_UNDO;
        mycallback = Subagent_handleSubagentSetResponse;
        retmagic = asi;
        break;

    default:
        TOOLS_FREE(smagic);
        DEBUG_MSGTL(("agentx/subagent", "  -> unknown command %d (%02x)\n",
                    pdu->command, pdu->command));
        return 0;
    }

    /*
     * submit the pdu to the internal handler
     */

    /*
     * We have to clone the PDU here, because when we return from this
     * callback, sess_process_packet will free(pdu), but this call also
     * free()s its argument PDU.
     */

    internal_pdu = Client_clonePdu(pdu);
    internal_pdu->contextName = (char *) internal_pdu->community;
    internal_pdu->contextNameLen = internal_pdu->communityLen;
    internal_pdu->community = NULL;
    internal_pdu->communityLen = 0;
    result = Api_asyncSend(subagent_agentxCallbackSess, internal_pdu, mycallback,
                    retmagic);
    if (result == 0) {
        Api_freePdu( internal_pdu );
    }
    return 1;
}

static int
_Subagent_invalidOpAndMagic(int op, NsSubagentMagic *smagic)
{
    int invalid = 0;

    if (smagic && (Api_sessPointer(smagic->session) == NULL ||
        op == API_CALLBACK_OP_TIMED_OUT)) {
        if (smagic->ovars != NULL) {
            Api_freeVarbind(smagic->ovars);
        }
        free(smagic);
        invalid = 1;
    }

    if (op != API_CALLBACK_OP_RECEIVED_MESSAGE || smagic == NULL)
        invalid = 1;

    return invalid;
}

int
Subagent_handleSubagentResponse(int op, Types_Session * session, int reqid,
                         Types_Pdu *pdu, void *magic)
{
    NsSubagentMagic *smagic = (NsSubagentMagic *) magic;
    Types_VariableList *u = NULL, *v = NULL;
    int             rc = 0;

    if (_Subagent_invalidOpAndMagic(op, (NsSubagentMagic *) magic)) {
        return 1;
    }

    pdu = Client_clonePdu(pdu);
    DEBUG_MSGTL(("agentx/subagent",
                "handling AgentX response (cmd 0x%02x orig_cmd 0x%02x)"
                " (req=0x%x,trans=0x%x,sess=0x%x)\n",
                pdu->command, smagic->original_command,
                (unsigned)pdu->reqid, (unsigned)pdu->transid,
                (unsigned)pdu->sessid));

    if (pdu->command == PRIOT_MSG_INTERNAL_SET_FREE ||
        pdu->command == PRIOT_MSG_INTERNAL_SET_UNDO ||
        pdu->command == PRIOT_MSG_INTERNAL_SET_COMMIT) {
        Subagent_freeSetVars(smagic->session, pdu);
    }

    if (smagic->original_command == AGENTX_MSG_GETNEXT) {
        DEBUG_MSGTL(("agentx/subagent",
                    "do getNext scope processing %p %p\n", smagic->ovars,
                    pdu->variables));
        for (u = smagic->ovars, v = pdu->variables; u != NULL && v != NULL;
             u = u->nextVariable, v = v->nextVariable) {
            if (Api_oidCompare
                (u->val.objid, u->valLen / sizeof(oid), vars_nullOid,
                 vars_nullOidLen/sizeof(oid)) != 0) {
                /*
                 * The master agent requested scoping for this variable.
                 */
                rc = Api_oidCompare(v->name, v->nameLength,
                                      u->val.objid,
                                      u->valLen / sizeof(oid));
                DEBUG_MSGTL(("agentx/subagent", "result "));
                DEBUG_MSGOID(("agentx/subagent", v->name, v->nameLength));
                DEBUG_MSG(("agentx/subagent", " scope to "));
                DEBUG_MSGOID(("agentx/subagent",
                             u->val.objid, u->valLen / sizeof(oid)));
                DEBUG_MSG(("agentx/subagent", " result %d\n", rc));

                if (rc >= 0) {
                    /*
                     * The varbind is out of scope.  From RFC2741, p. 66: "If
                     * the subagent cannot locate an appropriate variable,
                     * v.name is set to the starting OID, and the VarBind is
                     * set to `endOfMibView'".
                     */
                    Client_setVarObjid(v, u->name, u->nameLength);
                    Client_setVarTypedValue(v, PRIOT_ENDOFMIBVIEW, NULL, 0);
                    DEBUG_MSGTL(("agentx/subagent",
                                "scope violation -- return endOfMibView\n"));
                }
            } else {
                DEBUG_MSGTL(("agentx/subagent", "unscoped var\n"));
            }
        }
    }

    /*
     * XXXJBPN: similar for GETBULK but the varbinds can get re-ordered I
     * think which makes it er more difficult.
     */

    if (smagic->ovars != NULL) {
        Api_freeVarbind(smagic->ovars);
    }

    pdu->command = AGENTX_MSG_RESPONSE;
    pdu->version = smagic->session->version;

    if (!Api_send(smagic->session, pdu)) {
        Api_freePdu(pdu);
    }
    DEBUG_MSGTL(("agentx/subagent", "  FINISHED\n"));
    free(smagic);
    return 1;
}

int
Subagent_handleSubagentSetResponse(int op, Types_Session * session, int reqid,
                             Types_Pdu *pdu, void *magic)
{
    Types_Session *retsess;
    struct AgentSetInfo_s *asi;
    int result;

    if (op != API_CALLBACK_OP_RECEIVED_MESSAGE || magic == NULL) {
        return 1;
    }

    DEBUG_MSGTL(("agentx/subagent",
                "handling agentx subagent set response (mode=%d,req=0x%x,"
                "trans=0x%x,sess=0x%x)\n",
                (unsigned)pdu->command, (unsigned)pdu->reqid,
        (unsigned)pdu->transid, (unsigned)pdu->sessid));
    pdu = Client_clonePdu(pdu);

    asi = (struct AgentSetInfo_s *) magic;
    retsess = asi->sess;
    asi->errstat = pdu->errstat;

    if (asi->mode == PRIOT_MSG_INTERNAL_SET_RESERVE1) {
        /*
         * reloop for RESERVE2 mode, an internal only agent mode
         */
        /*
         * XXX: check exception statuses of reserve1 first
         */
        if (!pdu->errstat) {
            asi->mode = pdu->command = PRIOT_MSG_INTERNAL_SET_RESERVE2;
            result = Api_asyncSend(subagent_agentxCallbackSess, pdu,
                            Subagent_handleSubagentSetResponse, asi);
            if (result == 0) {
                Api_freePdu( pdu );
            }
            DEBUG_MSGTL(("agentx/subagent",
                        "  going from RESERVE1 -> RESERVE2\n"));
            return 1;
        }
    } else {
        if (asi->mode == PRIOT_MSG_INTERNAL_SET_FREE ||
            asi->mode == PRIOT_MSG_INTERNAL_SET_UNDO ||
            asi->mode == PRIOT_MSG_INTERNAL_SET_COMMIT) {
            Subagent_freeSetVars(retsess, pdu);
        }
        Api_freeVarbind(pdu->variables);
        pdu->variables = NULL;  /* the variables were added by us */
    }

    Assert_assert(retsess != NULL);
    pdu->command = AGENTX_MSG_RESPONSE;
    pdu->version = retsess->version;

    if (!Api_send(retsess, pdu)) {
        Api_freePdu(pdu);
    }
    DEBUG_MSGTL(("agentx/subagent", "  FINISHED\n"));
    return 1;
}


int
Subagent_registrationCallback(int majorID, int minorID, void *serverarg,
                             void *clientarg)
{
    struct RegisterParameters_s *reg_parms =
        (struct RegisterParameters_s *) serverarg;
    Types_Session *agentx_ss = *(Types_Session **)clientarg;

    if (minorID == PriotdCallback_REGISTER_OID)
        return XClient_register(agentx_ss,
                               reg_parms->name, reg_parms->namelen,
                               reg_parms->priority,
                               reg_parms->range_subid,
                               reg_parms->range_ubound, reg_parms->timeout,
                               reg_parms->flags,
                               reg_parms->contextName);
    else
        return XClient_unregister(agentx_ss,
                                 reg_parms->name, reg_parms->namelen,
                                 reg_parms->priority,
                                 reg_parms->range_subid,
                                 reg_parms->range_ubound,
                                 reg_parms->contextName);
}


static int
Subagent_sysORCallback(int majorID, int minorID, void *serverarg,
                      void *clientarg)
{
    const struct RegisterSysORParameters_s *reg_parms =
        (const struct RegisterSysORParameters_s *) serverarg;
    Types_Session *agentx_ss = *(Types_Session **)clientarg;

    if (minorID == PriotdCallback_REG_SYSOR)
        return XClient_addAgentcaps(agentx_ss,
                                    reg_parms->name, reg_parms->namelen,
                                    reg_parms->descr);
    else
        return XClient_removeAgentcaps(agentx_ss,
                                       reg_parms->name,
                                       reg_parms->namelen);
}


static int
Subagent_shutdown(int majorID, int minorID, void *serverarg, void *clientarg)
{
    Types_Session *thesession = *(Types_Session **)clientarg;
    DEBUG_MSGTL(("agentx/subagent", "shutting down session....\n"));
    if (thesession == NULL) {
    DEBUG_MSGTL(("agentx/subagent", "Empty session to shutdown\n"));
    agent_mainSession = NULL;
    return 0;
    }
    XClient_closeSession(thesession, AGENTX_CLOSE_SHUTDOWN);
    Api_close(thesession);
    if (agent_mainSession != NULL) {
        Trap_removeTrapSession(agent_mainSession);
        agent_mainSession = NULL;
    }
    DEBUG_MSGTL(("agentx/subagent", "shut down finished.\n"));

    _subagent_initInit = 0;
    return 1;
}



/*
 * Register all the "standard" AgentX callbacks for the given session.
 */

void
Subagent_registerCallbacks(Types_Session * s)
{
    Types_Session *sess_p;

    DEBUG_MSGTL(("agentx/subagent",
                "registering callbacks for session %p\n", s));
    sess_p = (Types_Session *) Tools_memdup(&s, sizeof(s));
    Assert_assert(sess_p);
    s->myvoid = sess_p;
    if (!sess_p)
        return;
    Callback_registerCallback(CALLBACK_LIBRARY, CALLBACK_SHUTDOWN,
                           Subagent_shutdown, sess_p);
    Callback_registerCallback(CALLBACK_APPLICATION,
                           PriotdCallback_REGISTER_OID,
                           Subagent_registrationCallback, sess_p);
    Callback_registerCallback(CALLBACK_APPLICATION,
                           PriotdCallback_UNREGISTER_OID,
                           Subagent_registrationCallback, sess_p);
    Callback_registerCallback(CALLBACK_APPLICATION,
                           PriotdCallback_REG_SYSOR,
                           Subagent_sysORCallback, sess_p);
    Callback_registerCallback(CALLBACK_APPLICATION,
                           PriotdCallback_UNREG_SYSOR,
                           Subagent_sysORCallback, sess_p);
}

/*
 * Unregister all the callbacks associated with this session.
 */

void
Subagent_unregisterCallbacks(Types_Session * ss)
{
    DEBUG_MSGTL(("agentx/subagent",
                "unregistering callbacks for session %p\n", ss));
    Callback_unregisterCallback(CALLBACK_LIBRARY, CALLBACK_SHUTDOWN,
                             Subagent_shutdown, ss->myvoid, 1);
    Callback_unregisterCallback(CALLBACK_APPLICATION,
                             PriotdCallback_REGISTER_OID,
                             Subagent_registrationCallback, ss->myvoid, 1);
    Callback_unregisterCallback(CALLBACK_APPLICATION,
                             PriotdCallback_UNREGISTER_OID,
                             Subagent_registrationCallback, ss->myvoid, 1);
    Callback_unregisterCallback(CALLBACK_APPLICATION,
                             PriotdCallback_REG_SYSOR,
                             Subagent_sysORCallback, ss->myvoid, 1);
    Callback_unregisterCallback(CALLBACK_APPLICATION,
                             PriotdCallback_UNREG_SYSOR,
                             Subagent_sysORCallback, ss->myvoid, 1);
    TOOLS_FREE(ss->myvoid);
}

/*
 * Open a session to the master agent.
 */
int
Subagent_openMasterSession(void)
{
    Transport_Transport *t;
    Types_Session sess;
    const char *agentx_socket;

    DEBUG_MSGTL(("agentx/subagent", "opening session...\n"));

    if (agent_mainSession) {
        Logger_log(LOGGER_PRIORITY_WARNING,
                 "AgentX session to master agent attempted to be re-opened.\n");
        return -1;
    }

    Api_sessInit(&sess);
    sess.version = AGENTX_VERSION_1;
    sess.retries = API_DEFAULT_RETRIES;
    sess.timeout = API_DEFAULT_TIMEOUT;
    sess.flags |= API_FLAGS_STREAM_SOCKET;
    sess.callback = Subagent_handleAgentxPacket;
    sess.authenticator = NULL;

    agentx_socket = DefaultStore_getString(DsStorage_APPLICATION_ID,
                                          DsAgentString_X_SOCKET);
    t = Transport_openClient("agentx", agentx_socket);
    if (t == NULL) {
        /*
         * Diagnose snmp_open errors with the input
         * Types_Session pointer.
         */
        if (!DefaultStore_getBoolean(DsStorage_APPLICATION_ID,
                                     DsAgentBoolean_NO_CONNECTION_WARNINGS)) {
            char buf[1024];
            snprintf(buf, sizeof(buf), "Warning: "
                     "Failed to connect to the agentx master agent (%s)",
                     agentx_socket ? agentx_socket : "[NIL]");
            if (!DefaultStore_getBoolean(DsStorage_APPLICATION_ID,
                                          DsAgentBoolean_NO_ROOT_ACCESS)) {
                Api_sessLogError(LOGGER_PRIORITY_WARNING, buf, &sess);
            } else {
                Api_sessPerror(buf, &sess);
            }
        }
        return -1;
    }

    agent_mainSession =
        Api_addFull(&sess, t, NULL, Protocol_parse, NULL, NULL,
                      Protocol_reallocBuild, Protocol_checkPacket, NULL);

    if (agent_mainSession == NULL) {
        if (!DefaultStore_getBoolean(DsStorage_APPLICATION_ID,
                                    DsAgentBoolean_NO_CONNECTION_WARNINGS)) {
            char buf[1024];
            snprintf(buf, sizeof(buf), "Error: "
                     "Failed to create the agentx master agent session (%s)",
                     agentx_socket);
            Api_sessPerror(buf, &sess);
        }
        Transport_free(t);
        return -1;
    }

    /*
     * I don't know why 1 is success instead of the usual 0 = noerr,
     * but that's what the function returns.
     */
    if (1 != XClient_openSession(agent_mainSession)) {
        Api_close(agent_mainSession);
        agent_mainSession = NULL;
        return -1;
    }

    /*
     * _Subagent_registerPingAlarm assumes that securityModel will
     *  be set to API_DEFAULT_SECMODEL on new AgentX sessions.
     *  This field is then (ab)used to hold the alarm stash.
     *
     * Why is the securityModel field used for this purpose, I hear you ask.
     * Damn good question!   (See SVN revision 4886)
     */
    agent_mainSession->securityModel = API_DEFAULT_SECMODEL;

    if (Trap_addTrapSession(agent_mainSession, AGENTX_MSG_NOTIFY, 1,
                         AGENTX_VERSION_1)) {
        DEBUG_MSGTL(("agentx/subagent", " trap session registered OK\n"));
    } else {
        DEBUG_MSGTL(("agentx/subagent",
                    "trap session registration failed\n"));
        Api_close(agent_mainSession);
        agent_mainSession = NULL;
        return -1;
    }

    Subagent_registerCallbacks(agent_mainSession);

    Callback_callCallbacks(CALLBACK_APPLICATION,
                        PriotdCallback_INDEX_START, (void *) agent_mainSession);

    Logger_log(LOGGER_PRIORITY_INFO, "NET-SNMP version %s AgentX subagent connected\n",
             Version_getVersion());
    DEBUG_MSGTL(("agentx/subagent", "opening session...  DONE (%p)\n",
                agent_mainSession));

    return 0;
}

static void
Subagent_reopenSysORTable(const struct SysORTable_s* data, void* v)
{
    Types_Session *agentx_ss = (Types_Session *) v;

    XClient_addAgentcaps(agentx_ss, data->OR_oid, data->OR_oidlen,
                         data->OR_descr);
}

/*
 * Alarm callback function to open a session to the master agent.  If a
 * transport disconnection callback occurs, indicating that the master agent
 * has died (or there has been some strange communication problem), this
 * alarm is called repeatedly to try to re-open the connection.
 */

void
_Subagent_reopenSession(unsigned int clientreg, void *clientarg)
{
    DEBUG_MSGTL(("agentx/subagent", "_Subagent_reopenSession(%d) called\n",
                clientreg));

    if (Subagent_openMasterSession() == 0) {
        /*
         * Successful.  Delete the alarm handle if one exists.
         */
        if (clientreg != 0) {
            Alarm_unregister(clientreg);
        }

        /*
         * Reregister all our nodes.
         */
        AgentRegistry_registerMibReattach();

        /*
         * Reregister all our sysOREntries
         */
        SysORTable_foreach(&Subagent_reopenSysORTable, agent_mainSession);

        /*
         * Register a ping alarm (if need be).
         */
        _Subagent_registerPingAlarm(0, 0, NULL, agent_mainSession);
    } else {
        if (clientreg == 0) {
            /*
             * Register a reattach alarm for later
             */
            _Subagent_registerPingAlarm(0, 0, NULL, agent_mainSession);
        }
    }
}

/*
 * If a valid session is passed in (through clientarg), register a
 * ping handler to ping it frequently, else register an attempt to try
 * and open it again later.
 */

static int
_Subagent_registerPingAlarm(int majorID, int minorID,
                             void *serverarg, void *clientarg)
{

    Types_Session *ss = (Types_Session *) clientarg;
    int             ping_interval =
        DefaultStore_getInt(DsStorage_APPLICATION_ID,
                           DsAgentInterger_AGENTX_PING_INTERVAL);

    if (!ping_interval)         /* don't do anything if not setup properly */
        return 0;

    /*
     * register a ping alarm, if desired
     */
    if (ss) {
        if (ss->securityModel != API_DEFAULT_SECMODEL) {
            DEBUG_MSGTL(("agentx/subagent",
                        "unregister existing alarm %d\n",
                        ss->securityModel));
            Alarm_unregister(ss->securityModel);
        }

        DEBUG_MSGTL(("agentx/subagent",
                    "register ping alarm every %d seconds\n",
                    ping_interval));
        /*
         * we re-use the securityModel parameter for an alarm stash,
         * since agentx doesn't need it
         */
        ss->securityModel = Alarm_register(ping_interval, ALARM_SA_REPEAT,
                                                Subagent_checkSession, ss);
    } else {
        /*
         * attempt to open it later instead
         */
        DEBUG_MSGTL(("agentx/subagent",
                    "subagent not properly attached, postponing registration till later....\n"));
        Alarm_register(ping_interval, ALARM_SA_REPEAT,
                            _Subagent_reopenSession, NULL);
    }
    return 0;
}

/*
 * check a session validity for connectivity to the master agent.  If
 * not functioning, close and start attempts to reopen the session
 */
void
Subagent_checkSession(unsigned int clientreg, void *clientarg)
{
    Types_Session *ss = (Types_Session *) clientarg;
    if (!ss) {
        if (clientreg)
            Alarm_unregister(clientreg);
        return;
    }
    DEBUG_MSGTL(("agentx/subagent", "checking status of session %p\n", ss));

    if (!XClient_sendPing(ss)) {
        Logger_log(LOGGER_PRIORITY_WARNING,
                 "AgentX master agent failed to respond to ping.  Attempting to re-register.\n");
        /*
         * master agent disappeared?  Try and re-register.
         * close first, just to be sure .
         */
        Subagent_unregisterCallbacks(ss);
        XClient_closeSession(ss, AGENTX_CLOSE_TIMEOUT);
        Alarm_unregister(clientreg);       /* delete ping alarm timer */
        Callback_callCallbacks(CALLBACK_APPLICATION,
                            PriotdCallback_INDEX_STOP, (void *) ss);
        AgentRegistry_registerMibDetach();
        if (agent_mainSession != NULL) {
            Trap_removeTrapSession(ss);
            Api_close(agent_mainSession);
            /*
             * We need to remove the callbacks attached to the callback
             * session because they have a magic callback data structure
             * which includes a pointer to the main session
             *    (which is no longer valid).
             *
             * Given that the main session is not responsive anyway.
             * it shoudn't matter if we lose some outstanding requests.
             */
            if (subagent_agentxCallbackSess != NULL ) {
                Api_close(subagent_agentxCallbackSess);
                subagent_agentxCallbackSess = NULL;

                Subagent_initCallbackSession();
            }
            agent_mainSession = NULL;
            _Subagent_reopenSession(0, NULL);
        }
        else {
            Api_close(agent_mainSession);
            agent_mainSession = NULL;
        }
    } else {
        DEBUG_MSGTL(("agentx/subagent", "session %p responded to ping\n",
                    ss));
    }
}


