#include "Vars.h"
#include "PriotSettings.h"
#include "MibModules.h"
#include "VarStruct.h"
#include "Logger.h"
#include "AgentRegistry.h"
#include "DefaultStore.h"
#include "AgentReadConfig.h"
#include "Debug.h"
#include "DsAgent.h"
#include "AllHelpers.h"
#include "Trap.h"
#include "SysORTable.h"
#include "Transports/UDPDomain.h"
#include "Transports/UnixDomain.h"
#include "Secmod.h"
#include "Enum.h"
#include "Utilities/Iquery.h"
#include "MibII/VacmConf.h"
#include "Agentx/Subagent.h"
#include "Agentx/AgentxConfig.h"
#include "V3/UsmConf.h"
#include "Transports/CallbackDomain.h"

static char     vars_doneInitAgent = 0;

ModuleInitList *vars_initlist = NULL;
ModuleInitList *vars_noinitlist = NULL;

/*
 * mib clients are passed a pointer to a oid buffer.  Some mib clients
 * * (namely, those first noticed in mibII/vacm.c) modify this oid buffer
 * * before they determine if they really need to send results back out
 * * using it.  If the master agent determined that the client was not the
 * * right one to talk with, it will use the same oid buffer to pass to the
 * * rest of the clients, which may not longer be valid.  This should be
 * * fixed in all clients rather than the master.  However, its not a
 * * particularily easy bug to track down so this saves debugging time at
 * * the expense of a few memcpy's.
 */
#define MIB_CLIENTS_ARE_EVIL 1

extern Subtree *vars_subtrees;

/*
 *      Each variable name is placed in the variable table, without the
 * terminating substring that determines the instance of the variable.  When
 * a string is found that is lexicographicly preceded by the input string,
 * the function for that entry is called to find the method of access of the
 * instance of the named variable.  If that variable is not found, NULL is
 * returned, and the search through the table continues (it will probably
 * stop at the next entry).  If it is found, the function returns a character
 * pointer and a length or a function pointer.  The former is the address
 * of the operand, the latter is a write routine for the variable.
 *
 * u_char *
 * findVar(name, length, exact, var_len, write_method)
 * oid      *name;          IN/OUT - input name requested, output name found
 * int      length;         IN/OUT - number of sub-ids in the in and out oid's
 * int      exact;          IN - TRUE if an exact match was requested.
 * int      len;            OUT - length of variable or 0 if function returned.
 * int      write_method;   OUT - pointer to function to set variable,
 *                                otherwise 0
 *
 *     The writeVar function is returned to handle row addition or complex
 * writes that require boundary checking or executing an action.
 * This routine will be called three times for each varbind in the packet.
 * The first time for each varbind, action is set to RESERVE1.  The type
 * and value should be checked during this pass.  If any other variables
 * in the MIB depend on this variable, this variable will be stored away
 * (but *not* committed!) in a place where it can be found by a call to
 * writeVar for a dependent variable, even in the same PDU.  During
 * the second pass, action is set to RESERVE2.  If this variable is dependent
 * on any other variables, it will check them now.  It must check to see
 * if any non-committed values have been stored for variables in the same
 * PDU that it depends on.  Sometimes resources will need to be reserved
 * in the first two passes to guarantee that the operation can proceed
 * during the third pass.  During the third pass, if there were no errors
 * in the first two passes, writeVar is called for every varbind with action
 * set to COMMIT.  It is now that the values should be written.  If there
 * were errors during the first two passes, writeVar is called in the third
 * pass once for each varbind, with the action set to FREE.  An opportunity
 * is thus provided to free those resources reserved in the first two passes.
 *
 * writeVar(action, var_val, var_val_type, var_val_len, statP, name, name_len)
 * int      action;         IN - RESERVE1, RESERVE2, COMMIT, or FREE
 * u_char   *var_val;       IN - input or output buffer space
 * u_char   var_val_type;   IN - type of input buffer
 * int      var_val_len;    IN - input and output buffer len
 * u_char   *statP;         IN - pointer to local statistic
 * oid      *name           IN - pointer to name requested
 * int      name_len        IN - number of sub-ids in the name
 */

long            vars_longReturn;

u_char          vars_returnBuf[258];


int             vars_callbackMasterNum = -1;

Types_Session *vars_callbackMasterSess = NULL;

static void
Vars_initAgentCallbackTransport(void)
{
    /*
     * always register a callback transport for internal use
     */
    vars_callbackMasterSess = CallbackDomain_open(0, Agent_handlePriotPacket,
                                                 Agent_checkPacket,
                                                 Agent_checkParse );
    if (vars_callbackMasterSess)
        vars_callbackMasterNum = vars_callbackMasterSess->local_port;
}


/**
 * Initialize the agent.  Calls into init_agent_read_config to set tha app's
 * configuration file in the appropriate default storage space,
 *  NETSNMP_DS_LIB_APPTYPE.  Need to call Vars_initAgent before calling init_snmp.
 *
 * @param app the configuration file to be read in, gets stored in default
 *        storage
 *
 * @return Returns non-zero on failure and zero on success.
 *
 * @see init_snmp
 */
int
Vars_initAgent(const char *app)
{
    int             r = 0;

    if(++vars_doneInitAgent > 1) {
        Logger_log(LOGGER_PRIORITY_WARNING, "ignoring extra call to Vars_initAgent (%d)\n",
                 vars_doneInitAgent);
        return r;
    }

    /*
     * get current time (ie, the time the agent started)
     */
    Agent_setAgentStarttime(NULL);

    /*
     * we handle alarm signals ourselves in the select loop
     */
    DefaultStore_setBoolean(DsStorage_LIBRARY_ID,
               DsBool_ALARM_DONT_USE_SIG, 1);

    AgentRegistry_setupTree();

    AgentReadConfig_initAgentReadConfig(app);

    Vars_initAgentCallbackTransport();

    AllHelpers_initHelpers();
    Trap_initTraps();
    Container_initList();
    SysORTable_initAgentSysORTable();

    /*
     * initialize agentx configs
     */
    AgentxConfig_init();
    if(DefaultStore_getBoolean(DsStorage_APPLICATION_ID,
                              DsAgentBoolean_ROLE) == SUB_AGENT)
        Subagent_init();

    /*
     * Register configuration tokens from transport modules.
     */
    UDPDomain_agentConfigTokensRegister();

    UnixDomain_agentConfigTokensRegister();

    /*
     * don't init agent modules for a sub-agent
     */
    if (DefaultStore_getBoolean(DsStorage_APPLICATION_ID,
                   DsAgentBoolean_ROLE) == SUB_AGENT)
        return r;

#include "ModuleInits.h"

    return r;
}                               /* end Vars_initAgent() */

oid             vars_nullOid[] = { 0, 0 };
int             vars_nullOidLen = sizeof(vars_nullOid);

void
Vars_shutdownAgent(void) {

    /* probably some of this can be called as shutdown callback */
    AgentRegistry_shutdownTree();
    AgentRegistry_clearContext();
    CallbackDomain_clearCallbackList();
    Transport_clearTdomainList();
    AgentHandler_clearHandlerList();
    SysORTable_shutdownAgentSysORTable();
    Container_freeList();
    Secmod_clear();
    Enum_clearEnum();
    Callback_clearCallback();
    Secmod_shutdown();
    Agent_addrcacheDestroy();

    vars_doneInitAgent = 0;
}


void
Vars_addToInitList(char *module_list)
{
    ModuleInitList *newitem, **list;
    char           *cp;
    char           *st;

    if (module_list == NULL) {
        return;
    } else {
        cp = (char *) module_list;
    }

    if (*cp == '-' || *cp == '!') {
        cp++;
        list = &vars_noinitlist;
    } else {
        list = &vars_initlist;
    }

    cp = strtok_r(cp, ", :", &st);
    while (cp) {
        newitem = (ModuleInitList *) calloc(1, sizeof(*vars_initlist));
        newitem->module_name = strdup(cp);
        newitem->next = *list;
        *list = newitem;
        cp = strtok_r(NULL, ", :", &st);
    }
}

int
Vars_shouldInit(const char *module_name)
{
    ModuleInitList *listp;

    /*
     * a definitive list takes priority
     */
    if (vars_initlist) {
        listp = vars_initlist;
        while (listp) {
            if (strcmp(listp->module_name, module_name) == 0) {
                DEBUG_MSGTL(("mib_init", "initializing: %s\n",
                            module_name));
                return DO_INITIALIZE;
            }
            listp = listp->next;
        }
        DEBUG_MSGTL(("mib_init", "skipping:     %s\n", module_name));
        return DONT_INITIALIZE;
    }

    /*
     * initialize it only if not on the bad list (bad module, no bone)
     */
    if (vars_noinitlist) {
        listp = vars_noinitlist;
        while (listp) {
            if (strcmp(listp->module_name, module_name) == 0) {
                DEBUG_MSGTL(("mib_init", "skipping:     %s\n",
                            module_name));
                return DONT_INITIALIZE;
            }
            listp = listp->next;
        }
    }
    DEBUG_MSGTL(("mib_init", "initializing: %s\n", module_name));

    /*
     * initialize it
     */
    return DO_INITIALIZE;
}
