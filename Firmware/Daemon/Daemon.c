#include "Daemon.h"
#include "Agent.h"
#include "Trap.h"
#include "Logger.h"
#include "DsAgent.h"
#include "ParseArgs.h"
#include "Mib.h"
#include "System.h"
#include "AgentRegistry.h"
#include "Alarm.h"
#include "M2M.h"
#include "Version.h"
#include "ReadConfig.h"
#include "AgentReadConfig.h"
#include "FdEventManager.h"
#include "PluginModules.h"

#include <sys/syslog.h>
#include <grp.h>
#include <pwd.h>
#include <unistd.h>
#include <signal.h>


#define TIMETICK         500000L



static int      _reconfig = 0;



int             daemon_dumpPacket;
int             daemon_facility = LOG_DAEMON;
const char *    _daemon_appName = "priotd";



extern int       agent_running;
extern char   ** restart_argvrestartp;
extern char    * restart_argvrestart;
extern char    * restart_argvrestartname;

/*
 * Prototypes.
 */
static int      _Daemon_input(int, Types_Session *, int, Types_Pdu *, void *);
static void     _Daemon_usage(char *);
static void     _Daemon_TrapNodeDown(void);
static int      _Daemon_receive(void);

static void _Daemon_usage(char *prog)
{

    printf("\nUsage:  %s [OPTIONS] [LISTENING ADDRESSES]", prog);
    printf("\n"
           "\n\tVersion:  %s\n%s"
           "\t\t\t  (config search path: %s)\n%s%s",
           Version_getVersion(),
           "\tWeb:      http://www.net-snmp.org/\n"
           "\tEmail:    net-snmp-coders@lists.sourceforge.net\n"
           "\n  -a\t\t\tlog addresses\n"
           "  -A\t\t\tappend to the logfile rather than truncating it\n"
           "  -c FILE[,...]\t\tread FILE(s) as configuration file(s)\n"
           "  -C\t\t\tdo not read the default configuration files\n",
           ReadConfig_getConfigurationDirectory(),
           "  -d\t\t\tdump sent and received SNMP packets\n"
           "  -D[TOKEN[,...]]\tturn on debugging output for the given TOKEN(s)\n"
           "\t\t\t  (try ALL for extremely verbose output)\n"
           "\t\t\t  Don't put space(s) between -D and TOKEN(s).\n"
           "  -f\t\t\tdo not fork from the shell\n",
           "  -g GID\t\tchange to this numeric gid after opening\n"
           "\t\t\t  transport endpoints\n"
           "  -h, --help\t\tdisplay this usage message\n"
           "  -H\t\t\tdisplay configuration file directives understood\n"
           "  -I [-]INITLIST\tlist of mib modules to initialize (or not)\n"
           "\t\t\t  (run snmpd with -Dmib_init for a list)\n"
           "  -L <LOGOPTS>\t\ttoggle options controlling where to log to\n");
    Logger_logOptionsUsage("\t", stdout);
    printf("  -m MIBLIST\t\tuse MIBLIST instead of the default MIB list\n"
           "  -M DIRLIST\t\tuse DIRLIST as the list of locations to look for MIBs\n"
           "\t\t\t  (default %s)\n%s%s",
           Mib_getMibDirectory(),

           "  -p FILE\t\tstore process id in FILE\n"
           "  -q\t\t\tprint information in a more parsable format\n"
           "  -r\t\t\tdo not exit if files only accessible to root\n"
           "\t\t\t  cannot be opened\n"
           "  -v, --version\t\tdisplay version information\n"
           "  -V\t\t\tverbose display\n"
           "  -x ADDRESS\t\tuse ADDRESS as AgentX address\n"
           "  -X\t\t\trun as an AgentX subagent rather than as an\n"
           "\t\t\t  SNMP master agent\n"
           ,
           "\nDeprecated options:\n"
           "  -l FILE\t\tuse -Lf <FILE> instead\n"
           "  -P\t\t\tuse -p instead\n"
           "  -s\t\t\tuse -Lsd instead\n"
           "  -S d|i|0-7\t\tuse -Ls <facility> instead\n"
           "\n"
           );
    exit(1);
}

static void _Daemon_version( )
{
    printf("\nPRIOT version:  %s\n"
           "Web:               http://www.net-snmp.org/\n"
           "Email:             net-snmp-coders@lists.sourceforge.net\n\n",
           Version_getVersion());
    exit(0);
}

static void _Daemon_shutDown( int a )
{
    config_UNUSED(a);
    agent_running = 0;
}


static void _Daemon_reconfig(int a)
{
    config_UNUSED(a);
    _reconfig = 1;
    signal(SIGHUP, _Daemon_reconfig);
}


static void _Daemon_dump(int a)
{
    config_UNUSED(a);
    AgentRegistry_dumpRegistry();
    signal(SIGUSR1, _Daemon_dump);
}

static void
_Daemon_CatchRandomSignal(int a)
{
    /* Disable all logs and log the error via syslog */
    Logger_disableLog();
    Logger_enableSyslog();
    Logger_log(LOGGER_PRIORITY_ERR, "Exiting on signal %d\n", a);
    Logger_disableSyslog();
    exit(1);
}

static void
_Daemon_TrapNodeDown(void)
{
    Trap_sendEasyTrap(PRIOT_TRAP_ENTERPRISESPECIFIC, 2);
    /*
     * XXX  2 - Node Down #define it as NODE_DOWN_TRAP
     */
}

/** \fn int main(int argc, char *argv[])
 *  \brief main function.
 *
 * Setup and start the agent daemon.
 * Also successfully EXITs with zero for some options.
 *
 * @param argc
 * @param *argv[]
 *
 * @return 0 Always succeeds.  (?)
 *
 */

int main(int argc, char *argv[])
{
    char            options[128] = "aAc:CdD::fhHI:l:L:m:M:n:p:P:qrsS:UvV-:Y:";
    int             arg, i, ret;
    int             dont_fork = 0, do_help = 0;
    int             log_set = 0;
    int             agent_mode = -1;
    char           *pid_file = NULL;
    char            option_compatability[] = "-Le";
    int fd;
    FILE           *PID;

    /*
     * close all non-standard file descriptors we may have
     * inherited from the shell.
     */
    for (i = getdtablesize() - 1; i > 2; --i) {
        (void) close(i);
    }

    /*
     * register signals ASAP to prevent default action (usually core)
     * for signals during startup...
     */
    DEBUG_MSGTL(("signal", "registering SIGTERM signal handler\n"));
    signal(SIGTERM, _Daemon_shutDown);
    DEBUG_MSGTL(("signal", "registering SIGINT signal handler\n"));
    signal(SIGINT, _Daemon_shutDown);
    signal(SIGHUP, SIG_IGN);   /* do not terminate on early SIGHUP */
    DEBUG_MSGTL(("signal", "registering SIGUSR1 signal handler\n"));
    signal(SIGUSR1, _Daemon_dump);
    DEBUG_MSGTL(("signal", "registering SIGPIPE signal handler\n"));
    signal(SIGPIPE, SIG_IGN);   /* 'Inline' failure of wayward readers */
    signal(SIGXFSZ, _Daemon_CatchRandomSignal);

    /*
     * Default to NOT running an AgentX master.
     */
    DefaultStore_setBoolean(DsStorage_APPLICATION_ID,
                            DsAgentBoolean_AGENTX_MASTER, 0);
    DefaultStore_setInt(DsStorage_APPLICATION_ID,
                        DsAgentInterger_AGENTX_TIMEOUT, -1);
    DefaultStore_setInt(DsStorage_APPLICATION_ID,
                        DsAgentInterger_AGENTX_RETRIES, -1);

    DefaultStore_setInt(DsStorage_APPLICATION_ID,
                        DsAgentInterger_CACHE_TIMEOUT, 5);
    /*
     * Add some options if they are available.
     */
    strcat(options, "g:u:");
    strcat(options, "x:");
    strcat(options, "X");

    /*
     * This is incredibly ugly, but it's probably the simplest way
     *  to handle the old '-L' option as well as the new '-Lx' style
     */
    for (i=0; i<argc; i++) {
        if (!strcmp(argv[i], "-L"))
            argv[i] = option_compatability;
    }


    Logger_logSyslogname(_daemon_appName);

    DefaultStore_setString(DsStorage_LIBRARY_ID,
                           DsStr_APPTYPE, _daemon_appName);

    /*
     * Now process options normally.
     */
    while ((arg = getopt(argc, argv, options)) != EOF) {
        switch (arg) {
        case '-':
            if (strcasecmp(optarg, "help") == 0) {
                _Daemon_usage(argv[0]);
            }
            if (strcasecmp(optarg, "version") == 0) {
                _Daemon_version();
            }

            ParseArgs_handleLongOpt(optarg);
            break;

        case 'a':
            agent_logAddresses++;
            break;

        case 'A':
            DefaultStore_setBoolean(DsStorage_LIBRARY_ID,
                                    DsBool_APPEND_LOGFILES, 1);
            break;

        case 'c':
            if (optarg != NULL) {
                DefaultStore_setString(DsStorage_LIBRARY_ID,
                                       DsStr_OPTIONALCONFIG, optarg);
            } else {
                _Daemon_usage(argv[0]);
            }
            break;

        case 'C':
            DefaultStore_setBoolean(DsStorage_LIBRARY_ID,
                                    DsBool_DONT_READ_CONFIGS, 1);
            break;

        case 'd':
            DefaultStore_setBoolean(DsStorage_LIBRARY_ID,
                                    DsBool_DUMP_PACKET,
                                    ++daemon_dumpPacket);
            break;

        case 'D':

            Debug_registerTokens(optarg);
            Debug_setDoDebugging(1);
            break;

        case 'f':
            dont_fork = 1;
            break;

        case 'g':
            if (optarg != NULL) {
                char           *ecp;
                int             gid;

                gid = strtoul(optarg, &ecp, 10);
                if (*ecp) {
                    struct group  *info;

                    info = getgrnam(optarg);
                    gid = info ? (int) info->gr_gid : -1;
                    endgrent();
                }
                if (gid < 0) {
                    fprintf(stderr, "Bad group id: %s\n", optarg);
                    exit(1);
                }
                DefaultStore_setInt(DsStorage_APPLICATION_ID,
                                    DsAgentInterger_GROUPID, gid);
            } else {
                _Daemon_usage(argv[0]);
            }
            break;

        case 'h':
            _Daemon_usage(argv[0]);
            break;

        case 'H':
            do_help = 1;
            break;

        case 'I':
            if (optarg != NULL) {
                Vars_addToInitList(optarg);
            } else {
                _Daemon_usage(argv[0]);
            }
            break;

        case 'l':
            printf("Warning: -l option is deprecated, use -Lf <file> instead\n");
            if (optarg != NULL) {
                if (strlen(optarg) > PATH_MAX) {
                    fprintf(stderr,
                            "%s: logfile path too long (limit %d chars)\n",
                            argv[0], PATH_MAX);
                    exit(1);
                }
                Logger_enableFilelog2(optarg,
                                      DefaultStore_getBoolean(DsStorage_LIBRARY_ID,
                                                              DsBool_APPEND_LOGFILES));
                log_set = 1;
            } else {
                _Daemon_usage(argv[0]);
            }
            break;

        case 'L':
            if  (Logger_logOptions( optarg, argc, argv ) < 0 ) {
                _Daemon_usage(argv[0]);
            }
            log_set = 1;
            break;

        case 'm':
            if (optarg != NULL) {
                setenv("MIBS", optarg, 1);
            } else {
                _Daemon_usage(argv[0]);
            }
            break;

        case 'M':
            if (optarg != NULL) {
                setenv("MIBDIRS", optarg, 1);
            } else {
                _Daemon_usage(argv[0]);
            }
            break;

        case 'n':
            if (optarg != NULL) {
                _daemon_appName = optarg;
                DefaultStore_setString(DsStorage_LIBRARY_ID,
                                       DsStr_APPTYPE, _daemon_appName);
            } else {
                _Daemon_usage(argv[0]);
            }
            break;

        case 'P':
            printf("Warning: -P option is deprecated, use -p instead\n");
        case 'p':
            if (optarg != NULL) {
                pid_file = optarg;
            } else {
                _Daemon_usage(argv[0]);
            }
            break;

        case 'q':
            DefaultStore_setBoolean(DsStorage_LIBRARY_ID,
                                    DsBool_QUICK_PRINT, 1);
            break;

        case 'r':
            DefaultStore_toggleBoolean(DsStorage_APPLICATION_ID,
                                       DsAgentBoolean_NO_ROOT_ACCESS);
            break;

        case 's':
            printf("Warning: -s option is deprecated, use -Lsd instead\n");
            Logger_enableSyslog();
            log_set = 1;
            break;

        case 'S':
            printf("Warning: -S option is deprecated, use -Ls <facility> instead\n");
            if (optarg != NULL) {
                switch (*optarg) {
                case 'd':
                case 'D':
                    daemon_facility = LOG_DAEMON;
                    break;
                case 'i':
                case 'I':
                    daemon_facility = LOGGER_PRIORITY_INFO;
                    break;
                case '0':
                    daemon_facility = LOG_LOCAL0;
                    break;
                case '1':
                    daemon_facility = LOG_LOCAL1;
                    break;
                case '2':
                    daemon_facility = LOG_LOCAL2;
                    break;
                case '3':
                    daemon_facility = LOG_LOCAL3;
                    break;
                case '4':
                    daemon_facility = LOG_LOCAL4;
                    break;
                case '5':
                    daemon_facility = LOG_LOCAL5;
                    break;
                case '6':
                    daemon_facility = LOG_LOCAL6;
                    break;
                case '7':
                    daemon_facility = LOG_LOCAL7;
                    break;
                default:
                    fprintf(stderr, "invalid syslog facility: -S%c\n",*optarg);
                    _Daemon_usage(argv[0]);
                }
                Logger_enableSyslogIdent(Logger_logSyslogname(NULL), daemon_facility);
                log_set = 1;
            } else {
                fprintf(stderr, "no syslog facility specified\n");
                _Daemon_usage(argv[0]);
            }
            break;

        case 'U':
            DefaultStore_toggleBoolean(DsStorage_APPLICATION_ID,
                                       DsAgentBoolean_LEAVE_PIDFILE);
            break;

        case 'u':
            if (optarg != NULL) {
                char           *ecp;
                int             uid;

                uid = strtoul(optarg, &ecp, 10);
                if (*ecp) {
                    struct passwd  *info;

                    info = getpwnam(optarg);
                    uid = info ? (int)info->pw_uid : -1;
                    endpwent();
                }
                if (uid < 0) {
                    fprintf(stderr, "Bad user id: %s\n", optarg);
                    exit(1);
                }
                DefaultStore_setInt(DsStorage_APPLICATION_ID,
                                    DsAgentInterger_USERID, uid);
            } else {
                _Daemon_usage(argv[0]);
            }
            break;

        case 'v':
            _Daemon_version();

        case 'V':
            DefaultStore_setBoolean(DsStorage_APPLICATION_ID,
                                    DsAgentBoolean_VERBOSE, 1);
            break;

        case 'x':
            if (optarg != NULL) {
                DefaultStore_setString(DsStorage_APPLICATION_ID,
                                       DsAgentString_X_SOCKET, optarg);
            } else {
                _Daemon_usage(argv[0]);
            }
            DefaultStore_setBoolean(DsStorage_APPLICATION_ID,
                                    DsAgentBoolean_AGENTX_MASTER, 1);
            break;

        case 'X':
            agent_mode = daemon_SUB_AGENT;

            break;

        case 'Y':
            ReadConfig_remember(optarg);
            break;

        default:
            _Daemon_usage(argv[0]);
            break;
        }
    }

    if (do_help) {
        DefaultStore_setBoolean(DsStorage_APPLICATION_ID,
                                DsAgentBoolean_NO_ROOT_ACCESS, 1);
        Vars_initAgent(_daemon_appName);        /* register our .conf handlers */
        PluginModules_initModules();
        Api_init(_daemon_appName);
        fprintf(stderr, "Configuration directives understood:\n");
        ReadConfig_printUsage("  ");
        exit(0);
    }

    if (optind < argc) {
        /*
         * There are optional transport addresses on the command line.
         */
        DEBUG_MSGTL(("priotd/main", "optind %d, argc %d\n", optind, argc));
        for (i = optind; i < argc; i++) {
            char *c, *astring;
            if ((c = DefaultStore_getString(DsStorage_APPLICATION_ID,
                                            DsAgentString_PORTS))) {
                astring = (char*)malloc(strlen(c) + 2 + strlen(argv[i]));
                if (astring == NULL) {
                    fprintf(stderr, "malloc failure processing argv[%d]\n", i);
                    exit(1);
                }
                sprintf(astring, "%s,%s", c, argv[i]);
                DefaultStore_setString(DsStorage_APPLICATION_ID,
                                       DsAgentString_PORTS, astring);
                TOOLS_FREE(astring);
            } else {
                DefaultStore_setString(DsStorage_APPLICATION_ID,
                                       DsAgentString_PORTS, argv[i]);
            }
        }
        DEBUG_MSGTL(("snmpd/main", "port spec: %s\n",
                     DefaultStore_getString(DsStorage_APPLICATION_ID,
                                            DsAgentString_PORTS)));

    }

    if (0 == log_set)
        Logger_enableFilelog(LOGFILE,
                             DefaultStore_getBoolean(DsStorage_LIBRARY_ID,
                                                     DsBool_APPEND_LOGFILES));


    {
        /*
         * Initialize a argv set to the current for restarting the agent.
         */
        char *cptr, **argvptr;

        restart_argvrestartp = (char **)malloc((argc + 2) * sizeof(char *));
        argvptr = restart_argvrestartp;
        for (i = 0, ret = 1; i < argc; i++) {
            ret += strlen(argv[i]) + 1;
        }
        restart_argvrestart = (char *) malloc(ret);
        restart_argvrestartname = (char *) malloc(strlen(argv[0]) + 1);
        if (!restart_argvrestartp || !restart_argvrestart || !restart_argvrestartname) {
            fprintf(stderr, "malloc failure processing argvrestart\n");
            exit(1);
        }
        strcpy(restart_argvrestartname, argv[0]);

        for (cptr = restart_argvrestart, i = 0; i < argc; i++) {
            strcpy(cptr, argv[i]);
            *(argvptr++) = cptr;
            cptr += strlen(argv[i]) + 1;
        }
    }

    if (agent_mode == -1) {
        if (strstr(argv[0], "agentxd") != NULL) {
            DefaultStore_setBoolean(DsStorage_APPLICATION_ID,
                                    DsAgentBoolean_ROLE, daemon_SUB_AGENT);
        } else {
            DefaultStore_setBoolean(DsStorage_APPLICATION_ID,
                                    DsAgentBoolean_ROLE, daemon_MASTER_AGENT);
        }
    } else {
        DefaultStore_setBoolean(DsStorage_APPLICATION_ID,
                                DsAgentBoolean_ROLE, agent_mode);
    }

    if (Vars_initAgent(_daemon_appName) != 0) {
        Logger_log(LOGGER_PRIORITY_ERR, "Agent initialization failed\n");
        exit(1);
    }
    PluginModules_initModules();

    /*
     * start library
     */
    Api_init(_daemon_appName);

    if ((ret = Agent_initMasterAgent()) != 0) {
        /*
         * Some error opening one of the specified agent transports.
         */
        Logger_log(LOGGER_PRIORITY_ERR, "Server Exiting with code 1\n");
        exit(1);
    }

    /*
     * Initialize the world.  Detach from the shell.  Create initial user.
     */
    if(!dont_fork) {
        int quit = ! DefaultStore_getBoolean(DsStorage_APPLICATION_ID,
                                             DsAgentBoolean_QUIT_IMMEDIATELY);
        ret = System_daemonize(quit,
                               Logger_stderrlogStatus()
                               );
        /*
         * xxx-rks: do we care if fork fails? I think we should...
         */
        if(ret != 0) {
            Logger_log(LOGGER_PRIORITY_ERR, "Server Exiting with code 1\n");
            exit(1);
        }
    }

    if (pid_file != NULL) {
        /*
         * unlink the pid_file, if it exists, prior to open.  Without
         * doing this the open will fail if the user specified pid_file
         * already exists.
         */
        unlink(pid_file);
        fd = open(pid_file, O_CREAT | O_EXCL | O_WRONLY, 0600);
        if (fd == -1) {
            Logger_logPerror(pid_file);
            if (!DefaultStore_getBoolean(DsStorage_APPLICATION_ID,
                                         DsAgentBoolean_NO_ROOT_ACCESS)) {
                exit(1);
            }
        } else {
            if ((PID = fdopen(fd, "w")) == NULL) {
                Logger_logPerror(pid_file);
                exit(1);
            } else {
                fprintf(PID, "%d\n", (int) getpid());
                fclose(PID);
            }
            /* The sequence open()/fdopen()/fclose()/close() makes MSVC crash,
               hence skip the close() call when using the MSVC runtime. */
            close(fd);
        }
    }

    {
        const char     *persistent_dir;
        int             uid, gid;

        persistent_dir = ReadConfig_getPersistentDirectory();
        System_mkdirhier( persistent_dir, AGENT_DIRECTORY_MODE, 0 );

        uid = DefaultStore_getInt(DsStorage_APPLICATION_ID,
                                  DsAgentInterger_USERID);
        gid = DefaultStore_getInt(DsStorage_APPLICATION_ID,
                                  DsAgentInterger_GROUPID);

        if ( uid != 0 || gid != 0 )
            chown( persistent_dir, uid, gid );

        if ((gid = DefaultStore_getInt(DsStorage_APPLICATION_ID, DsAgentInterger_GROUPID)) > 0) {
            DEBUG_MSGTL(("snmpd/main", "Changing gid to %d.\n", gid));
            if (setgid(gid) == -1
                    || setgroups(1, (gid_t *)&gid) == -1
                    ) {
                Logger_logPerror("setgid failed");
                if (!DefaultStore_getBoolean(DsStorage_APPLICATION_ID, DsAgentBoolean_NO_ROOT_ACCESS)) {
                    exit(1);
                }
            }
        }

        if ((uid = DefaultStore_getInt(DsStorage_APPLICATION_ID, DsAgentInterger_USERID)) > 0) {
            struct passwd *info;

            /*
         * Set supplementary groups before changing UID
         *   (which probably involves giving up privileges)
         */
            info = getpwuid(uid);
            if (info) {
                DEBUG_MSGTL(("snmpd/main", "Supplementary groups for %s.\n", info->pw_name));
                if (initgroups(info->pw_name, (gid != 0 ? (gid_t)gid : info->pw_gid)) == -1) {
                    Logger_logPerror("initgroups failed");
                    if (!DefaultStore_getBoolean(DsStorage_APPLICATION_ID,
                                                 DsAgentBoolean_NO_ROOT_ACCESS)) {
                        exit(1);
                    }
                }
            }
            endpwent();
            DEBUG_MSGTL(("snmpd/main", "Changing uid to %d.\n", uid));
            if (setuid(uid) == -1) {
                Logger_logPerror("setuid failed");
                if (!DefaultStore_getBoolean(DsStorage_APPLICATION_ID,
                                             DsAgentBoolean_NO_ROOT_ACCESS)) {
                    exit(1);
                }
            }
        }
    }

    /*
     * Store persistent data immediately in case we crash later.
     */
    Api_store(_daemon_appName);

    DEBUG_MSGTL(("signal", "registering SIGHUP signal handler\n"));
    signal(SIGHUP, _Daemon_reconfig);

    /*
     * Send coldstart trap if possible.
     */
    Trap_sendEasyTrap(0, 0);

    /*
     * We're up, log our version number.
     */
    Logger_log(LOGGER_PRIORITY_INFO, "PRIOT version %s\n", Version_getVersion());

    Agent_addrcacheInitialise();

    /*
     * Forever monitor the dest_port for incoming PDUs.
     */
    DEBUG_MSGTL(("priotd/main", "We're up.  Starting to process data.\n"));
    if (!DefaultStore_getBoolean(DsStorage_APPLICATION_ID, DsAgentBoolean_QUIT_IMMEDIATELY))
        _Daemon_receive();
    DEBUG_MSGTL(("priotd/main", "sending shutdown trap\n"));
    _Daemon_TrapNodeDown();
    DEBUG_MSGTL(("priotd/main", "Bye...\n"));
    Api_shutdown(_daemon_appName);
    Agent_shutdownMasterAgent();
    Vars_shutdownAgent();

    if (!DefaultStore_getBoolean(DsStorage_APPLICATION_ID, DsAgentBoolean_LEAVE_PIDFILE) && (pid_file != NULL)) {
        unlink(pid_file);
    }


    TOOLS_FREE(restart_argvrestartname);
    TOOLS_FREE(restart_argvrestart);
    TOOLS_FREE(restart_argvrestartp);

    return 0;
}                               /* End main() -- snmpd */



/*******************************************************************-o-******
 * receive
 *
 * Parameters:
 *
 * Returns:
 *	0	On success.
 *	-1	System error.
 *
 * Infinite while-loop which monitors incoming messages for the agent.
 * Invoke the established message handlers for incoming messages on a per
 * port basis.  Handle timeouts.
 */
static int
_Daemon_receive(void)
{
    int             numfds;
    Types_LargeFdSet readfds, writefds, exceptfds;
    struct timeval  timeout, *tvp = &timeout;
    int             count, block, i;

    LargeFdSet_init(&readfds, FD_SETSIZE);
    LargeFdSet_init(&writefds, FD_SETSIZE);
    LargeFdSet_init(&exceptfds, FD_SETSIZE);

    /*
     * ignore early sighup during startup
     */
    _reconfig = 0;


    /*
     * Loop-forever: execute message handlers for sockets with data
     */
    while (agent_running) {
        Agent_checkAndProcess(1);
        if (_reconfig) {
            sighold(SIGHUP);
            _reconfig = 0;
            Logger_log(LOGGER_PRIORITY_INFO, "Reconfiguring daemon\n");
            /*  Stop and restart logging.  This allows logfiles to be
        rotated etc.  */
            Logger_loggingRestart();
            Logger_log(LOGGER_PRIORITY_INFO, "NET-SNMP version %s restarted\n",
                       Version_getVersion());
            AgentReadConfig_updateConfig();
            Trap_sendEasyTrap(PRIOT_TRAP_ENTERPRISESPECIFIC, 3);
            sigrelse(SIGHUP);
        }

        /*
         * default to sleeping for a really long time. INT_MAX
         * should be sufficient (eg we don't care if time_t is
         * a long that's bigger than an int).
         */
        tvp = &timeout;
        tvp->tv_sec = INT_MAX;
        tvp->tv_usec = 0;

        numfds = 0;
#define __USE_XOPEN 1
        NETSNMP_LARGE_FD_ZERO(&readfds);
        NETSNMP_LARGE_FD_ZERO(&writefds);
        NETSNMP_LARGE_FD_ZERO(&exceptfds);
        block = 0;
        Api_selectInfo2(&numfds, &readfds, tvp, &block);
        if (block == 1) {
            tvp = NULL;         /* block without timeout */
        }



        FdEventManager_externalEventInfo2(&numfds, &readfds, &writefds, &exceptfds);

reselect:
        for (i = 0; i < NUM_EXTERNAL_SIGS; i++) {
            if (agentRegistry_externalSignalScheduled[i]) {
                agentRegistry_externalSignalScheduled[i]--;
                agentRegistry_externalSignalHandler[i](i);
            }
        }

        DEBUG_MSGTL(("snmpd/select", "select( numfds=%d, ..., tvp=%p)\n",
                     numfds, tvp));
        if (tvp)
            DEBUG_MSGTL(("timer", "tvp %ld.%ld\n", (long) tvp->tv_sec,
                         (long) tvp->tv_usec));
        count = LargeFdSet_select(numfds, &readfds, &writefds, &exceptfds,
                                  tvp);
        DEBUG_MSGTL(("snmpd/select", "returned, count = %d\n", count));

        if (count > 0) {

            FdEventManager_dispatchExternalEvents2(&count, &readfds,
                                                   &writefds, &exceptfds);

            /* If there are still events leftover, process them */
            if (count > 0) {
                Api_read2(&readfds);
            }
        } else
            switch (count) {
            case 0:
                Api_timeout();
                break;
            case -1:
                DEBUG_MSGTL(("snmpd/select", "  errno = %d\n", errno));
                if (errno == EINTR) {
                    /*
                     * likely that we got a signal. Check our special signal
                     * flags before retrying select.
                     */
                    if (agent_running && !_reconfig) {
                        goto reselect;
                    }
                    continue;
                } else {
                    Logger_logPerror("select");
                }
                return -1;
            default:
                Logger_log(LOGGER_PRIORITY_ERR, "select returned %d\n", count);
                return -1;
            }                   /* endif -- count>0 */

        /*
         * see if persistent store needs to be saved
         */
        Api_storeIfNeeded();

        /*
         * run requested alarms
         */
        Alarm_runAlarms();

        Agent_checkOutstandingAgentRequests();

    }                           /* endwhile */

    LargeFdSet_cleanup(&readfds);
    LargeFdSet_cleanup(&writefds);
    LargeFdSet_cleanup(&exceptfds);

    Logger_log(LOGGER_PRIORITY_INFO, "Received TERM or STOP signal...  shutting down...\n");
    return 0;

}                               /* end receive() */



/*******************************************************************-o-******
 * snmp_input
 *
 * Parameters:
 *	 op
 *	*session
 *	 requid
 *	*pdu
 *	*magic
 *
 * Returns:
 *	1		On success	-OR-
 *	Passes through	Return from alarmGetResponse() when
 *	  		  USING_V2PARTY_ALARM_MODULE is defined.
 *
 * Call-back function to manage responses to traps (informs) and alarms.
 * Not used by the agent to process other Response PDUs.
 */
int
_Daemon_input( int op, Types_Session * session, int reqid, Types_Pdu *pdu, void *magic )
{
    config_UNUSED(session);
    config_UNUSED(reqid);
    struct GetReqState *state = (struct GetReqState *) magic;

    if (op == API_CALLBACK_OP_RECEIVED_MESSAGE) {
        if (pdu->command == PRIOT_MSG_GET) {
            if (state->type == EVENT_GET_REQ) {
                /*
                 * this is just the ack to our inform pdu
                 */
                return 1;
            }
        }

    } else if (op == API_CALLBACK_OP_TIMED_OUT) {
        if (state->type == ALARM_GET_REQ) {
            /*
             * Need a mechanism to replace obsolete SNMPv2p alarm
             */
        }
    }
    return 1;

}                               /* end snmp_input() */
