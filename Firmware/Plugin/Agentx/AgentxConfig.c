#include "AgentxConfig.h"
#include "PriotSettings.h"
#include "System/Util/Debug.h"
#include "DefaultStore.h"
#include "DsAgent.h"
#include "System/Util/Logger.h"
#include "ReadConfig.h"
#include "System.h"
#include "Service.h"
#include "AgentReadConfig.h"


/* ---------------------------------------------------------------------
 *
 * Common master and sub-agent
 */
void
AgentxConfig_parseAgentxSocket(const char *token, char *cptr)
{
    DEBUG_MSGTL(("agentx/config", "port spec: %s\n", cptr));
    DefaultStore_setString(DsStorage_APPLICATION_ID, DsAgentString_X_SOCKET, cptr);
}

/* ---------------------------------------------------------------------
 *
 * Master agent
 */
void
AgentxConfig_parseMaster(const char *token, char *cptr)
{
    int             i = -1;

    if (!strcmp(cptr, "agentx") ||
        !strcmp(cptr, "all") ||
        !strcmp(cptr, "yes") || !strcmp(cptr, "on")) {
        i = 1;
        Logger_log(LOGGER_PRIORITY_INFO, "Turning on AgentX master support.\n");
    } else if (!strcmp(cptr, "no") || !strcmp(cptr, "off"))
        i = 0;
    else
        i = atoi(cptr);

    if (i < 0 || i > 1) {
    ReadConfig_error("master '%s' unrecognised", cptr);
    } else
        DefaultStore_setBoolean(DsStorage_APPLICATION_ID, DsAgentBoolean_AGENTX_MASTER, i);
}

void
AgentxConfig_parseAgentxPerms(const char *token, char *cptr)
{
    char *socket_perm, *dir_perm, *socket_user, *socket_group;
    int uid = -1;
    int gid = -1;
    int s_perm = -1;
    int d_perm = -1;
    char *st;

    DEBUG_MSGTL(("agentx/config", "port permissions: %s\n", cptr));
    socket_perm = strtok_r(cptr, " \t", &st);
    dir_perm    = strtok_r(NULL, " \t", &st);
    socket_user = strtok_r(NULL, " \t", &st);
    socket_group = strtok_r(NULL, " \t", &st);

    if (socket_perm) {
        s_perm = strtol(socket_perm, NULL, 8);
        DefaultStore_setInt(DsStorage_APPLICATION_ID,
                            DsAgentInterger_X_SOCK_PERM, s_perm);
        DEBUG_MSGTL(("agentx/config", "socket permissions: %o (%d)\n",
                    s_perm, s_perm));
    }
    if (dir_perm) {
        d_perm = strtol(dir_perm, NULL, 8);
        DefaultStore_setInt(DsStorage_APPLICATION_ID,
                           DsAgentInterger_X_DIR_PERM, d_perm);
        DEBUG_MSGTL(("agentx/config", "directory permissions: %o (%d)\n",
                    d_perm, d_perm));
    }

    /*
     * Try to handle numeric UIDs or user names for the socket owner
     */
    if (socket_user) {
        uid = System_strToUid(socket_user);
        if ( uid != 0 )
            DefaultStore_setInt(DsStorage_APPLICATION_ID,
                               DsAgentInterger_X_SOCK_USER, uid);
        DEBUG_MSGTL(("agentx/config", "socket owner: %s (%d)\n",
                    socket_user, uid));
    }

    /*
     * and similarly for the socket group ownership
     */
    if (socket_group) {
        gid = System_strToGid(socket_group);
        if ( gid != 0 )
            DefaultStore_setInt(DsStorage_APPLICATION_ID,
                               DsAgentInterger_X_SOCK_GROUP, gid);
        DEBUG_MSGTL(("agentx/config", "socket group: %s (%d)\n",
                    socket_group, gid));
    }
}

void
AgentxConfig_parseAgentxTimeout(const char *token, char *cptr)
{
    int x = Convert_stringTimeToSeconds(cptr);
    DEBUG_MSGTL(("agentx/config/timeout", "%s\n", cptr));
    if (x == -1) {
        ReadConfig_configPerror("Invalid timeout value");
        return;
    }
    DefaultStore_setInt(DsStorage_APPLICATION_ID,
                       DsAgentInterger_AGENTX_TIMEOUT, x * ONE_SEC);
}

void
AgentxConfig_parseAgentxRetries(const char *token, char *cptr)
{
    int x = atoi(cptr);
    DEBUG_MSGTL(("agentx/config/retries", "%s\n", cptr));
    DefaultStore_setInt(DsStorage_APPLICATION_ID,
                       DsAgentInterger_AGENTX_RETRIES, x);
}

/* ---------------------------------------------------------------------
 *
 * Sub-agent
 */


/* ---------------------------------------------------------------------
 *
 * Utility support routines
 */
void
AgentxConfig_registerConfigHandler(const char *token,
                              void (*parser) (const char *, char *),
                              void (*releaser) (void), const char *help)
{
    DEBUG_MSGTL(("agentx_register_app_config_handler",
                "registering .conf token for \"%s\"\n", token));
    ReadConfig_registerConfigHandler(":agentx", token, parser, releaser, help);
}


void
AgentxConfig_unregisterConfigHandler(const char *token)
{
    ReadConfig_unregisterConfigHandler(":agentx", token);
}

void
AgentxConfig_init(void)
{
    int agent_role = DefaultStore_getBoolean(DsStorage_APPLICATION_ID,
                                             DsAgentBoolean_ROLE);

    /*
     * Common tokens for master/subagent
     */
    Service_registerDefaultDomain("agentx", "unix tcp");
    Service_registerDefaultTarget("agentx", "unix", NETSNMP_AGENTX_SOCKET);
#define val(x) __STRING(x)
    Service_registerDefaultTarget("agentx", "tcp",
                                    "localhost:" val(AGENTX_PORT));
#undef val
    AgentxConfig_registerConfigHandler("agentxsocket",
                                  AgentxConfig_parseAgentxSocket, NULL,
                                  "AgentX bind address");
    /*
     * tokens for master agent
     */
    if (MASTER_AGENT == agent_role) {
        AgentReadConfig_priotdRegisterConfigHandler("master",
                                      AgentxConfig_parseMaster, NULL,
                                      "specify 'agentx' for AgentX support");
    AgentxConfig_registerConfigHandler("agentxperms",
                                  AgentxConfig_parseAgentxPerms, NULL,
                                  "AgentX socket permissions: socket_perms [directory_perms [username|userid [groupname|groupid]]]");
    AgentxConfig_registerConfigHandler("agentxRetries",
                                  AgentxConfig_parseAgentxRetries, NULL,
                                  "AgentX Retries");
    AgentxConfig_registerConfigHandler("agentxTimeout",
                                  AgentxConfig_parseAgentxTimeout, NULL,
                                  "AgentX Timeout (seconds)");
    }

    /*
     * tokens for master agent
     */
    if (SUB_AGENT == agent_role) {
        /*
         * set up callbacks to initiate master agent pings for this session
         */
        DefaultStore_registerConfig(ASN01_INTEGER,
                                    DefaultStore_getString( DsStorage_LIBRARY_ID,
                                                            DsStr_APPTYPE),
                                   "agentxPingInterval",
                                   DsStorage_APPLICATION_ID,
                                   DsAgentInterger_AGENTX_PING_INTERVAL);
        /* ping and/or reconnect by default every 15 seconds */
        DefaultStore_setInt(DsStorage_APPLICATION_ID,
                            DsAgentInterger_AGENTX_PING_INTERVAL, 15);

    }
}


