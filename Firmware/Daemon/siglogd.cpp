
#include "siglogd.h"
#include "includes.h"
#include "options.h"
#include "signals.h"


netsnmp_feature_want(logging_file)
netsnmp_feature_want(logging_stdio)
netsnmp_feature_want(logging_syslog)

int snmp_read_packet(int);
int snmp_input(int, netsnmp_session *, int, netsnmp_pdu *,
                           void *);
static void SnmpTrapNodeDown(void);
static int  receive(void);

extern void send_easy_trap(int, int);
static void SnmpTrapNodeDown(void)
{
    send_easy_trap(SNMP_TRAP_ENTERPRISESPECIFIC, 2);
}


int main_aux(int argc, char *argv[])
{

    int     i;
    int     ret;
    int     fd;
    FILE    *PID;

    Options opt;
    Signals sgn;

    for (i = getdtablesize() - 1; i > 2; --i) {
        (void) close(i);
    }
    

    // register signals ASAP to prevent default action (usually core)
    // for signals during startup...
    sgn.registerAll();


    //Default to NOT running an AgentX master.

    netsnmp_ds_set_boolean(NETSNMP_DS_APPLICATION_ID, NETSNMP_DS_AGENT_AGENTX_MASTER, 0);
    netsnmp_ds_set_int(NETSNMP_DS_APPLICATION_ID, NETSNMP_DS_AGENT_AGENTX_TIMEOUT, -1);
    netsnmp_ds_set_int(NETSNMP_DS_APPLICATION_ID, NETSNMP_DS_AGENT_AGENTX_RETRIES, -1);

    netsnmp_ds_set_int(NETSNMP_DS_APPLICATION_ID, NETSNMP_DS_AGENT_CACHE_TIMEOUT, 5);

    //Add some options if they are available.
    snmp_log_syslogname(opt.appName().toStdString().c_str());


    netsnmp_ds_set_string(NETSNMP_DS_LIBRARY_ID, NETSNMP_DS_LIB_APPTYPE, opt.appName().toStdString().c_str());


    //Now process options normally.
    opt.process(argc, argv);

    if (opt.doHelp()) {
        netsnmp_ds_set_boolean(NETSNMP_DS_APPLICATION_ID, NETSNMP_DS_AGENT_NO_ROOT_ACCESS, 1);
        init_agent(opt.appName().toStdString().c_str());
        init_mib_modules();
        init_snmp(opt.appName().toStdString().c_str());
        fprintf(stderr, "Configuration directives understood:\n");
        read_config_print_usage("  ");
    }else{

        if (optind < argc) {

            //There are optional transport addresses on the command line.

            DEBUGMSGTL(("snmpd/main", "optind %d, argc %d\n", optind, argc));
            for (i = optind; i < argc; i++) {
                char *c, *astring;
                if ((c = netsnmp_ds_get_string(NETSNMP_DS_APPLICATION_ID, NETSNMP_DS_AGENT_PORTS))) {
                    astring = (char*)malloc(strlen(c) + 2 + strlen(argv[i]));
                    if (astring == NULL) {
                        fprintf(stderr, "malloc failure processing argv[%d]\n", i);
                        exit(1);
                    }
                    sprintf(astring, "%s,%s", c, argv[i]);
                    netsnmp_ds_set_string(NETSNMP_DS_APPLICATION_ID,NETSNMP_DS_AGENT_PORTS, astring);
                    SNMP_FREE(astring);
                } else {
                    netsnmp_ds_set_string(NETSNMP_DS_APPLICATION_ID, NETSNMP_DS_AGENT_PORTS, argv[i]);
                }
            }
            DEBUGMSGTL(("snmpd/main", "port spec: %s\n", netsnmp_ds_get_string(NETSNMP_DS_APPLICATION_ID, NETSNMP_DS_AGENT_PORTS)));
        }

        if (false == opt.logSet())
            snmp_enable_filelog(NETSNMP_LOGFILE, netsnmp_ds_get_boolean(NETSNMP_DS_LIBRARY_ID, NETSNMP_DS_LIB_APPEND_LOGFILES));

        // Initialize a argv set to the current for restarting the agent.
        char *cptr, **argvptr;

        argvrestartp = (char **)malloc((argc + 2) * sizeof(char *));
        argvptr = argvrestartp;
        for (i = 0, ret = 1; i < argc; i++) {
            ret += strlen(argv[i]) + 1;
        }

        argvrestart = (char *) malloc(ret);
        argvrestartname = (char *) malloc(strlen(argv[0]) + 1);

        if (!argvrestartp || !argvrestart || !argvrestartname) {
            fprintf(stderr, "malloc failure processing argvrestart\n");
            exit(1);
        }
        strcpy(argvrestartname, argv[0]);

        for (cptr = argvrestart, i = 0; i < argc; i++) {
            strcpy(cptr, argv[i]);
            *(argvptr++) = cptr;
            cptr += strlen(argv[i]) + 1;
        }


        if (opt.agentMode() == -1) {
            if (strstr(argv[0], "agentxd") != NULL) {
                netsnmp_ds_set_boolean(NETSNMP_DS_APPLICATION_ID, NETSNMP_DS_AGENT_ROLE, SUB_AGENT);
            } else {
                netsnmp_ds_set_boolean(NETSNMP_DS_APPLICATION_ID, NETSNMP_DS_AGENT_ROLE, MASTER_AGENT);
            }
        } else {
            netsnmp_ds_set_boolean(NETSNMP_DS_APPLICATION_ID, NETSNMP_DS_AGENT_ROLE, opt.agentMode());
        }

        SOCK_STARTUP;

        if (init_agent(opt.appName().toStdString().c_str()) != 0) {
            snmp_log(LOG_ERR, "Agent initialization failed\n");
            exit(1);
        }

        init_mib_modules();


        //start library

        char * appname = strdup(opt.appName().toStdString().c_str());

        init_snmp(appname);

        if ((ret = init_master_agent()) != 0) {

            //Some error opening one of the specified agent transports.

            snmp_log(LOG_ERR, "Server Exiting with code 1\n");
            exit(1);
        }

        // Initialize the world.  Detach from the shell.  Create initial user.

        if(! opt.dontFork()) {
            int quit = ! netsnmp_ds_get_boolean(NETSNMP_DS_APPLICATION_ID, NETSNMP_DS_AGENT_QUIT_IMMEDIATELY);
            ret = netsnmp_daemonize(quit, snmp_stderrlog_status() );

            // xxx-rks: do we care if fork fails? I think we should...

            if(ret != 0) {
                snmp_log(LOG_ERR, "Server Exiting with code 1\n");
                exit(1);
            }
        }

        if (opt.pidFile() != NULL) {

            // unlink the pid_file, if it exists, prior to open.  Without
            // doing this the open will fail if the user specified pid_file
            // already exists.

            unlink(opt.pidFile());
            fd = open(opt.pidFile(), O_CREAT | O_EXCL | O_WRONLY, 0600);
            if (fd == -1) {
                snmp_log_perror(opt.pidFile());
                if (!netsnmp_ds_get_boolean(NETSNMP_DS_APPLICATION_ID, NETSNMP_DS_AGENT_NO_ROOT_ACCESS)) {
                    exit(1);
                }
            } else {
                if ((PID = fdopen(fd, "w")) == NULL) {
                    snmp_log_perror(opt.pidFile());
                    exit(1);
                } else {
                    fprintf(PID, "%d\n", (int) getpid());
                    fclose(PID);
                }
                // The sequence open()/fdopen()/fclose()/close() makes MSVC crash,
                // hence skip the close() call when using the MSVC runtime.
                close(fd);
            }
        }

        const char *persistent_dir;
        int uid, gid;

        persistent_dir = get_persistent_directory();
        mkdirhier( persistent_dir, NETSNMP_AGENT_DIRECTORY_MODE, 0 );

        uid = netsnmp_ds_get_int(NETSNMP_DS_APPLICATION_ID, NETSNMP_DS_AGENT_USERID);
        gid = netsnmp_ds_get_int(NETSNMP_DS_APPLICATION_ID, NETSNMP_DS_AGENT_GROUPID);

        if ( uid != 0 || gid != 0 )
            chown( persistent_dir, uid, gid );

        if ((gid = netsnmp_ds_get_int(NETSNMP_DS_APPLICATION_ID, NETSNMP_DS_AGENT_GROUPID)) > 0) {
            DEBUGMSGTL(("snmpd/main", "Changing gid to %d.\n", gid));
            if (setgid(gid) == -1 || setgroups(1, (gid_t *)&gid) == -1) {
                snmp_log_perror("setgid failed");
                if (!netsnmp_ds_get_boolean(NETSNMP_DS_APPLICATION_ID, NETSNMP_DS_AGENT_NO_ROOT_ACCESS)) {
                    exit(1);
                }
            }
        }

        if ((uid = netsnmp_ds_get_int(NETSNMP_DS_APPLICATION_ID, NETSNMP_DS_AGENT_USERID)) > 0) {
            struct passwd *info;


            // Set supplementary groups before changing UID
            //   (which probably involves giving up privileges)

            info = getpwuid(uid);

            if (info) {
                DEBUGMSGTL(("snmpd/main", "Supplementary groups for %s.\n", info->pw_name));
                if (initgroups(info->pw_name, (gid != 0 ? (gid_t)gid : info->pw_gid)) == -1) {
                    snmp_log_perror("initgroups failed");
                    if (!netsnmp_ds_get_boolean(NETSNMP_DS_APPLICATION_ID, NETSNMP_DS_AGENT_NO_ROOT_ACCESS)) {
                        exit(1);
                    }
                }
            }
            endpwent();
            DEBUGMSGTL(("snmpd/main", "Changing uid to %d.\n", uid));
            if (setuid(uid) == -1) {
                snmp_log_perror("setuid failed");
                if (!netsnmp_ds_get_boolean(NETSNMP_DS_APPLICATION_ID, NETSNMP_DS_AGENT_NO_ROOT_ACCESS)) {
                    exit(1);
                }
            }
        }

        // Store persistent data immediately in case we crash later.

        snmp_store(opt.appName().toStdString().c_str());

        DEBUGMSGTL(("signal", "registering SIGHUP signal handler\n"));

        sgn.registerOne(SIGHUP, Signals::reconfig);



        // Send coldstart trap if possible.

        send_easy_trap(0, 0);


        // We're up, log our version number.

        snmp_log(LOG_INFO, "NET-SNMP version %s\n", netsnmp_get_version());
        netsnmp_addrcache_initialise();


        // Forever monitor the dest_port for incoming PDUs.
        //
        DEBUGMSGTL(("snmpd/main", "We're up.registerOne  Starting to process data.\n"));
        if (!netsnmp_ds_get_boolean(NETSNMP_DS_APPLICATION_ID, NETSNMP_DS_AGENT_QUIT_IMMEDIATELY))
            receive();
        DEBUGMSGTL(("snmpd/main", "sending shutdown trap\n"));
        SnmpTrapNodeDown();
        DEBUGMSGTL(("snmpd/main", "Bye...\n"));
        snmp_shutdown(opt.appName().toStdString().c_str());
        shutdown_master_agent();
        shutdown_agent();

        if (!netsnmp_ds_get_boolean(NETSNMP_DS_APPLICATION_ID, NETSNMP_DS_AGENT_LEAVE_PIDFILE) &&
        (opt.pidFile() != NULL)) {
            unlink(opt.pidFile());
        }

        SNMP_FREE(argvrestartname);
        SNMP_FREE(argvrestart);
        SNMP_FREE(argvrestartp);

        SOCK_CLEANUP;
    }
    return 0;
}


static int
receive(void)
{
    int             numfds;
    netsnmp_large_fd_set readfds, writefds, exceptfds;
    struct timeval  timeout, *tvp = &timeout;
    int             count, block, i;

    netsnmp_large_fd_set_init(&readfds, FD_SETSIZE);
    netsnmp_large_fd_set_init(&writefds, FD_SETSIZE);
    netsnmp_large_fd_set_init(&exceptfds, FD_SETSIZE);

    /*
     * ignore early sighup during startup
     */
    Signals::RECONFIG = 0;

    /*
     * Loop-forever: execute message handlers for sockets with data
     */
    while (netsnmp_running) {
        if (Signals::RECONFIG) {

            sighold(SIGHUP);

            Signals::RECONFIG = 0;
            snmp_log(LOG_INFO, "Reconfiguring daemon\n");
	    /*  Stop and restart logging.  This allows logfiles to be
		rotated etc.  */
	    netsnmp_logging_restart();
	    snmp_log(LOG_INFO, "NET-SNMP version %s restarted\n",
		     netsnmp_get_version());
            update_config();
            send_easy_trap(SNMP_TRAP_ENTERPRISESPECIFIC, 3);
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
        NETSNMP_LARGE_FD_ZERO(&readfds);
        NETSNMP_LARGE_FD_ZERO(&writefds);
        NETSNMP_LARGE_FD_ZERO(&exceptfds);
        block = 0;
        snmp_select_info2(&numfds, &readfds, tvp, &block);
        if (block == 1) {
            tvp = NULL;         /* block without timeout */
	}



        netsnmp_external_event_info2(&numfds, &readfds, &writefds, &exceptfds);

    reselect:
        for (i = 0; i < NUM_EXTERNAL_SIGS; i++) {
            if (external_signal_scheduled[i]) {
                external_signal_scheduled[i]--;
                external_signal_handler[i](i);
            }
        }

        DEBUGMSGTL(("snmpd/select", "select( numfds=%d, ..., tvp=%p)\n",
                    numfds, tvp));
        if (tvp)
            DEBUGMSGTL(("timer", "tvp %ld.%ld\n", (long) tvp->tv_sec,
                        (long) tvp->tv_usec));
        count = netsnmp_large_fd_set_select(numfds, &readfds, &writefds, &exceptfds,
				     tvp);
        DEBUGMSGTL(("snmpd/select", "returned, count = %d\n", count));

        if (count > 0) {
            netsnmp_dispatch_external_events2(&count, &readfds,
                                              &writefds, &exceptfds);

            /* If there are still events leftover, process them */
            if (count > 0) {
              snmp_read2(&readfds);
            }
        } else
            switch (count) {
            case 0:
                snmp_timeout();
                break;
            case -1:
                DEBUGMSGTL(("snmpd/select", "  errno = %d\n", errno));
                if (errno == EINTR) {
                    /*
                     * likely that we got a signal. Check our special signal
                     * flags before retrying select.
                     */
            if (netsnmp_running && !Signals::RECONFIG) {
                        goto reselect;
		    }
                    continue;
                } else {
                    snmp_log_perror("select");
                }
                return -1;
            default:
                snmp_log(LOG_ERR, "select returned %d\n", count);
                return -1;
            }                   /* endif -- count>0 */

        /*
         * see if persistent store needs to be saved
         */
        snmp_store_if_needed();

        /*
         * run requested alarms 
         */
        run_alarms();

        netsnmp_check_outstanding_agent_requests();

    }                           /* endwhile */

    netsnmp_large_fd_set_cleanup(&readfds);
    netsnmp_large_fd_set_cleanup(&writefds);
    netsnmp_large_fd_set_cleanup(&exceptfds);

    snmp_log(LOG_INFO, "Received TERM or STOP signal...  shutting down...\n");
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
snmp_input(int op,
           netsnmp_session * session,
           int reqid, netsnmp_pdu *pdu, void *magic)
{
    struct get_req_state *state = (struct get_req_state *) magic;

    if (op == NETSNMP_CALLBACK_OP_RECEIVED_MESSAGE) {
        if (pdu->command == SNMP_MSG_GET) {
            if (state->type == EVENT_GET_REQ) {
                /*
                 * this is just the ack to our inform pdu 
                 */
                return 1;
            }
        }
    } else if (op == NETSNMP_CALLBACK_OP_TIMED_OUT) {
        if (state->type == ALARM_GET_REQ) {
            /*
             * Need a mechanism to replace obsolete SNMPv2p alarm 
             */
        }
    }
    return 1;

}                               /* end snmp_input() */

