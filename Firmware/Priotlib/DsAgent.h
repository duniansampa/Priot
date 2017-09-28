#ifndef DSAGENT_H
#define DSAGENT_H

#include "DefaultStore.h"
/*
 * defines agent's default store registrations
 */
/*
 * Note:
 *    If new ds_agent entries are added to this header file,
 *    then remember to run 'perl/agent/default_store/gen' to
 *    update the corresponding perl interface.
 */

/*
 * booleans
 */


enum DsAgentBoolean_e{

    DsAgentBoolean_VERBOSE                       ,       /* 1 if verbose output desired */
    DsAgentBoolean_ROLE                          ,       /* 0 if master, 1 if client */
    DsAgentBoolean_NO_ROOT_ACCESS                ,       /* 1 if we can't get root access */
    DsAgentBoolean_AGENTX_MASTER                 ,       /* 1 if AgentX desired */
    DsAgentBoolean_QUIT_IMMEDIATELY              ,       /* 1 to never start the agent */
    DsAgentBoolean_DISABLE_PERL                  ,       /* 1 to never enable perl */
    DsAgentBoolean_NO_CONNECTION_WARNINGS        ,       /* 1 = !see !connect msgs */
    DsAgentBoolean_LEAVE_PIDFILE                 ,       /* 1 = leave PID file on exit */
    DsAgentBoolean_NO_CACHING                    ,       /* 1 = disable netsnmp_cache */
    DsAgentBoolean_STRICT_DISMAN                 ,       /* 1 = "correct" object ordering */
    DsAgentBoolean_DONT_RETAIN_NOTIFICATIONS     ,       /* 1 = disable trap logging */
    DsAgentBoolean_DONT_LOG_TCPWRAPPERS_CONNECTS = 12,   /* 1 = disable logging */
    DsAgentBoolean_APP_DONT_LOG                  = DsAgentBoolean_DONT_RETAIN_NOTIFICATIONS,      /* DsAgentBoolean_APP_DONT_LOG = DsAgentBoolean_DONT_RETAIN_NOTIFICATIONS. compat */
    DsAgentBoolean_SKIPNFSINHOSTRESOURCES        = 13,   /* 1 = don't store NFS entries in hrStorageTable */
    DsAgentBoolean_REALSTORAGEUNITS              ,       /* 1 = use real allocation units in hrStorageTable, 0 = recalculate it to fit 32bits */
    /* Repeated from "apps/snmptrapd_ds.h"    */
    DsAgentBoolean_APP_NUMERIC_IP                ,
    DsAgentBoolean_APP_NO_AUTHORIZATION          ,
    DsAgentBoolean_DISKIO_NO_FD                  ,       /* 1 = don't report /dev/fd*   entries in diskIOTable */
    DsAgentBoolean_DISKIO_NO_LOOP                ,       /* 1 = don't report /dev/loop* entries in diskIOTable */
    DsAgentBoolean_DISKIO_NO_RAM                         /* 1 = don't report /dev/ram*  entries in diskIOTable */


};

/* WARNING: The trap receiver also uses DS flags and must not conflict with these!
 * If you define additional boolean entries, check in "apps/snmptrapd_ds.h" first */

/*
 * strings
 */
enum DsAgentString_e{

    DsAgentString_PROGNAME         , /* argv[0] */
    DsAgentString_X_SOCKET         , /* AF_UNIX or ip:port socket addr */
    DsAgentString_PORTS            , /* localhost:9161,tcp:localhost:9161... */
    DsAgentString_INTERNAL_SECNAME , /* used by disman/mteTriggerTable. */
    DsAgentString_PERL_INIT_FILE   , /* used by embedded perl */
    DsAgentString_SMUX_SOCKET      , /* ip:port socket addr */
    DsAgentString_NOTIF_LOG_CTX    , /* "" | "snmptrapd" */
    DsAgentString_TRAP_ADDR          /* used as v1 trap agent address */
};

/*
 * integers
 */
enum DsAgentInterger_e{

   DsAgentInterger_FLAGS                , /* session.flags */
   DsAgentInterger_USERID               ,
   DsAgentInterger_GROUPID              ,
   DsAgentInterger_AGENTX_PING_INTERVAL , /* ping master every SECONDS */
   DsAgentInterger_AGENTX_TIMEOUT       ,
   DsAgentInterger_AGENTX_RETRIES       ,
   DsAgentInterger_X_SOCK_PERM          , /* permissions for the */
   DsAgentInterger_X_DIR_PERM           , /*     AgentX socket   */
   DsAgentInterger_X_SOCK_USER          , /* ownership for the   */
   DsAgentInterger_X_SOCK_GROUP         , /*     AgentX socket   */
   DsAgentInterger_CACHE_TIMEOUT        , /* default cache timeout */
   DsAgentInterger_INTERNAL_VERSION     , /* used by internal queries */
   DsAgentInterger_INTERNAL_SECLEVEL    , /* used by internal queries */
   DsAgentInterger_MAX_GETBULKREPEATS   , /* max getbulk repeats */
   DsAgentInterger_MAX_GETBULKRESPONSES   /* max getbulk respones */

};

#endif // DSAGENT_H
