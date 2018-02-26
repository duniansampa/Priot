#include "AgentReadConfig.h"
#include "PriotSettings.h"
#include "ReadConfig.h"
#include "DefaultStore.h"
#include "DsAgent.h"
#include "Impl.h"
#include "Debug.h"
#include "Strlcpy.h"
#include "Callback.h"
#include "AgentCallbacks.h"
#include "AgentHandler.h"
#include "Trap.h"

#include <stdlib.h>
#include <pwd.h>
#include <grp.h>

void
AgentReadConfig_priotdSetAgentUser(const char *token, char *cptr)
{
    if (cptr[0] == '#') {
        char           *ecp;
        int             uid;

        uid = strtoul(cptr + 1, &ecp, 10);
        if (*ecp != 0) {
            ReadConfig_configPerror("Bad number");
        } else {
            DefaultStore_setInt(DsStorage_APPLICATION_ID,
                       DsAgentInterger_USERID, uid);
        }
    } else {
        struct passwd  *info;

        info = getpwnam(cptr);
        if (info)
            DefaultStore_setInt(DsStorage_APPLICATION_ID,
                               DsAgentInterger_USERID, info->pw_uid);
        else
            ReadConfig_configPerror("User not found in passwd database");
        endpwent();
    }
}

void
AgentReadConfig_priotdSetAgentGroup(const char *token, char *cptr)
{
    if (cptr[0] == '#') {
        char           *ecp;
        int             gid = strtoul(cptr + 1, &ecp, 10);

        if (*ecp != 0) {
            ReadConfig_configPerror("Bad number");
        } else {
                DefaultStore_setInt(DsStorage_APPLICATION_ID,
                       DsAgentInterger_GROUPID, gid);
        }
    } else {
        struct group   *info;

        info = getgrnam(cptr);
        if (info)
            DefaultStore_setInt(DsStorage_APPLICATION_ID,
                               DsAgentInterger_GROUPID, info->gr_gid);
        else
            ReadConfig_configPerror("Group not found in group database");
        endgrent();
    }
}

void
AgentReadConfig_priotdSetAgentAddress(const char *token, char *cptr)
{
    char            buf[IMPL_SPRINT_MAX_LEN];
    char           *ptr;

    /*
     * has something been specified before?
     */
    ptr = DefaultStore_getString(DsStorage_APPLICATION_ID,
                DsAgentString_PORTS);

    if (ptr) {
        /*
         * append to the older specification string
         */
        snprintf(buf, sizeof(buf), "%s,%s", ptr, cptr);
    buf[sizeof(buf) - 1] = '\0';
    } else {
        Strlcpy_strlcpy(buf, cptr, sizeof(buf));
    }

    DEBUG_MSGTL(("snmpd_ports", "port spec: %s\n", buf));
    DefaultStore_setString(DsStorage_APPLICATION_ID, DsAgentString_PORTS, buf);
}

void
AgentReadConfig_initAgentReadConfig(const char *app)
{
    if (app != NULL) {
        DefaultStore_setString(DsStorage_LIBRARY_ID, DsStr_APPTYPE, app);
    } else {
        app = DefaultStore_getString(DsStorage_LIBRARY_ID, DsStr_APPTYPE);
    }

    ReadConfig_registerAppConfigHandler("authtrapenable",
                                Trap_priotdParseConfigAuthtrap, NULL,
                                "1 | 2\t\t(1 = enable, 2 = disable)");
    ReadConfig_registerAppConfigHandler("pauthtrapenable", Trap_priotdParseConfigAuthtrap, NULL, NULL);


    if (DefaultStore_getBoolean(DsStorage_APPLICATION_ID,
                   DsAgentBoolean_ROLE) == MASTER_AGENT) {


        ReadConfig_registerAppConfigHandler("trapsess",
                                    Trap_priotdParseConfigTrapsess,
                                    Trap_priotdFreeTrapsinks,
                                    "[snmpcmdargs] host");
    }

    DefaultStore_registerConfig(ASN01_OCTET_STR, app, "v1trapaddress",
                               DsStorage_APPLICATION_ID,
                               DsAgentString_TRAP_ADDR);
    ReadConfig_registerAppConfigHandler("agentuser",
                                AgentReadConfig_priotdSetAgentUser, NULL, "userid");
    ReadConfig_registerAppConfigHandler("agentgroup",
                                AgentReadConfig_priotdSetAgentGroup, NULL, "groupid");
    ReadConfig_registerAppConfigHandler("agentaddress",
                                AgentReadConfig_priotdSetAgentAddress, NULL,
                                "SNMP bind address");
    DefaultStore_registerConfig(ASN01_BOOLEAN, app, "quit",
                   DsStorage_APPLICATION_ID,
                   DsAgentBoolean_QUIT_IMMEDIATELY);
    DefaultStore_registerConfig(ASN01_BOOLEAN, app, "leave_pidfile",
                   DsStorage_APPLICATION_ID,
                   DsAgentBoolean_LEAVE_PIDFILE);
    DefaultStore_registerConfig(ASN01_BOOLEAN, app, "dontLogTCPWrappersConnects",
                               DsStorage_APPLICATION_ID,
                               DsAgentBoolean_DONT_LOG_TCPWRAPPERS_CONNECTS);
    DefaultStore_registerConfig(ASN01_INTEGER, app, "maxGetbulkRepeats",
                               DsStorage_APPLICATION_ID,
                               DsAgentInterger_MAX_GETBULKREPEATS);
    DefaultStore_registerConfig(ASN01_INTEGER, app, "maxGetbulkResponses",
                               DsStorage_APPLICATION_ID,
                               DsAgentInterger_MAX_GETBULKRESPONSES);
    AgentHandler_initHandlerConf();

}

void
AgentReadConfig_updateConfig(void)
{
    Callback_callCallbacks(CALLBACK_APPLICATION,
                        PriotdCallback_PRE_UPDATE_CONFIG, NULL);
    ReadConfig_freeConfig();
    ReadConfig_readConfigs();
}


void
AgentReadConfig_priotdRegisterConfigHandler(const char *token,
                              void (*parser) (const char *, char *),
                              void (*releaser) (void), const char *help)
{
    DEBUG_MSGTL(("snmpd_register_app_config_handler",
                "registering .conf token for \"%s\"\n", token));
    ReadConfig_registerAppConfigHandler(token, parser, releaser, help);
}

void
AgentReadConfig_priotdRegisterConstConfigHandler(const char *token,
                                    void (*parser) (const char *, const char *),
                                    void (*releaser) (void), const char *help)
{
    DEBUG_MSGTL(("snmpd_register_app_config_handler",
                "registering .conf token for \"%s\"\n", token));
    ReadConfig_registerAppConfigHandler(token, (void(*)(const char *, char *))parser,
                                releaser, help);
}

void
AgentReadConfig_priotdUnregisterConfigHandler(const char *token)
{
    ReadConfig_unregisterAppConfigHandler(token);
}

/*
 * this function is intended for use by mib-modules to store permenant
 * configuration information generated by sets or persistent counters
 */
void
AgentReadConfig_priotdStoreConfig(const char *line)
{
    ReadConfig_readAppConfigStore(line);
}
