#include "Options.h"
#include "../Corelib/DefaultStore.h"


//int snmp_dump_packet;

//Options.Options()
//{

//    m_appName   = "snmpd";
//    m_dontFork  = false;
//    m_doHelp    = false;
//    m_logSet    = false;
//    m_pidFile   = NULL;
//    m_agentMode = -1;
//}

//string Options.appName(){
//    return m_appName;
//}

//void Options.appName(string & name){
//    m_appName = name;
//}
//bool Options.dontFork(){
//    return m_dontFork;
//}
//void Options.dontFork(bool dontfork){
//    m_dontFork = dontfork;
//}
//bool Options.doHelp(){
//    return m_doHelp;
//}
//void Options.doHelp(bool dohelp){
//    m_doHelp = dohelp;
//}
//bool Options.logSet(){
//    return m_logSet;
//}
//void Options.logSet(bool logset){
//    m_logSet = logset;
//}
//int Options.agentMode(){
//    return m_agentMode;
//}
// void Options.agentMode(int agentmode){
//    m_agentMode = agentmode;
//}

//char * Options.pidFile(){
//    return m_pidFile;
//}

//void Options.pidFile(char * pidfile){
//    m_pidFile = pidfile;
//}

//void Options.usage(char *prog)
//{

//    printf("\nUsage:  %s [OPTIONS] [LISTENING ADDRESSES]", prog);

////    printf("\n"
////           "\n\tVersion:  %s\n%s"
////           "\t\t\t  (config search path: %s)\n%s%s",
////           netsnmp_get_version(),
////           "\tWeb:      http://www.net-snmp.org/\n"
////           "\tEmail:    net-snmp-coders@lists.sourceforge.net\n"
////           "\n  -a\t\t\tlog addresses\n"
////           "  -A\t\t\tappend to the logfile rather than truncating it\n"
////           "  -c FILE[,...]\t\tread FILE(s) as configuration file(s)\n"
////           "  -C\t\t\tdo not read the default configuration files\n",
////           get_configuration_directory(),
////           "  -d\t\t\tdump sent and received SNMP packets\n"
////           "  -D[TOKEN[,...]]\tturn on debugging output for the given TOKEN(s)\n"
////       "\t\t\t  (try ALL for extremely verbose output)\n"
////       "\t\t\t  Don't put space(s) between -D and TOKEN(s).\n"
////           "  -f\t\t\tdo not fork from the shell\n",
////           "  -g GID\t\tchange to this numeric gid after opening\n"
////       "\t\t\t  transport endpoints\n"
////           "  -h, --help\t\tdisplay this usage message\n"
////           "  -H\t\t\tdisplay configuration file directives understood\n"
////           "  -I [-]INITLIST\tlist of mib modules to initialize (or not)\n"
////           "\t\t\t  (run snmpd with -Dmib_init for a list)\n"
////           "  -L <LOGOPTS>\t\ttoggle options controlling where to log to\n");

////    snmp_log_options_usage("\t", stdout);

////    printf("  -m MIBLIST\t\tuse MIBLIST instead of the default MIB list\n"
////           "  -M DIRLIST\t\tuse DIRLIST as the list of locations to look for MIBs\n"
////           "\t\t\t  (default %s)\n%s%s",
////           netsnmp_get_mib_directory(),
////           "  -p FILE\t\tstore process id in FILE\n"
////           "  -q\t\t\tprint information in a more parsable format\n"
////           "  -r\t\t\tdo not exit if files only accessible to root\n"
////       "\t\t\t  cannot be opened\n"
////           "  -v, --version\t\tdisplay version information\n"
////           "  -V\t\t\tverbose display\n"
////           "  -x ADDRESS\t\tuse ADDRESS as AgentX address\n"
////           "  -X\t\t\trun as an AgentX subagent rather than as an\n"
////       "\t\t\t  SNMP master agent\n"
////           ,
////           "\nDeprecated options:\n"
////           "  -l FILE\t\tuse -Lf <FILE> instead\n"
////           "  -P\t\t\tuse -p instead\n"
////           "  -s\t\t\tuse -Lsd instead\n"
////           "  -S d|i|0-7\t\tuse -Ls <facility> instead\n"
////           "\n"
////           );
//    exit(1);
//}


//void Options.version(void)
//{
//    printf("\nPRIOT version:  %s\n"
//           "Web:               http://www.priot.org/\n"
//           "Email:             priot.dev@outlook.com\n\n",
//           PACKAGE_VERSION);
//    exit(0);
//}

//void Options.process(int argc, char *argv[]){



//    int arg;

//    char  var_options[128] = "aAc:CdD.fhHI:L:m:M:n:qrp:UvV-:Y:";

//    strcat(var_options, "g:u:");
//    strcat(var_options, "x:");
//    strcat(var_options, "X");


//    while ((arg = getopt(argc, argv, var_options)) != EOF) {
//        switch (arg) {
//        case '-':
//            if (strcasecmp(optarg, "help") == 0) {
//                this->usage(argv[0]);
//            }
//            if (strcasecmp(optarg, "version") == 0) {
//                this->version();
//            }
//            break;

//        case 'a':
//            //log_addresses++;
//            break;

//        case 'A':
//            DefaultStore_setBoolean(DsStorage_LIBRARY_ID, DsBool_APPEND_LOGFILES, 1);
//            break;

//        case 'c':
//            if (optarg != NULL) {
//                DefaultStore_setString(DsStorage_LIBRARY_ID, DsStr_OPTIONALCONFIG, optarg );
//            } else {
//                this->usage(argv[0]);
//            }
//            break;

//        case 'C':
//            DefaultStore_setBoolean(DsStorage_LIBRARY_ID, DsBool_DONT_READ_CONFIGS, 1);
//            break;

//        case 'd':
//            DefaultStore_setBoolean(DsStorage_LIBRARY_ID, DsBool_DUMP_PACKET, ++snmp_dump_packet);
//            break;

//        case 'D':
//            //debug_register_tokens(optarg);
//            //snmp_set_do_debugging(1);
//            break;

//        case 'f':
//            m_dontFork = true;
//            break;

//        case 'g':
////            if (optarg != NULL) {
////                char           *ecp;
////                int             gid;

////                gid = strtoul(optarg, &ecp, 10);
////                if (*ecp) {
////                    struct group  *info;

////                    info = getgrnam(optarg);
////                    gid = info ? info->gr_gid : -1;
////                    endgrent();
////                }
////                if (gid < 0) {
////                    fprintf(stderr, "Bad group id: %s\n", optarg);
////                    exit(1);
////                }
////                defaultStore.setInt(DefaultStore.Storage.APPLICATION_ID,
////                   DefaultStore.Integers.AGENT_GROUPID, gid);
////            } else {
////                this->usage(argv[0]);
////            }
//            break;

//        case 'h':
//            this->usage(argv[0]);
//            break;

//        case 'H':
//            m_doHelp = true;
//            break;

//        case 'I':
//            if (optarg != NULL) {
//                //add_to_init_list(optarg);
//            } else {
//                this->usage(argv[0]);
//            }
//            break;
//        case 'L':
////        if  (snmp_log_options( optarg, argc, argv ) < 0 ) {
////                this->usage(argv[0]);
////            }
////            m_logSet = true;
//            break;

//        case 'm':
//            if (optarg != NULL) {
//                setenv("MIBS", optarg, 1);
//            } else {
//                this->usage(argv[0]);
//            }
//            break;

//        case 'M':
//            if (optarg != NULL) {
//                setenv("MIBDIRS", optarg, 1);
//            } else {
//                this->usage(argv[0]);
//            }
//            break;

//        case 'n':
//            if (optarg != NULL) {
//                m_appName = optarg;
//                DefaultStore_setString(DsStorage_LIBRARY_ID, DsStr_APPTYPE, (char*)m_appName.c_str());
//            } else {
//                this->usage(argv[0]);
//            }
//            break;
//        case 'p':
//            if (optarg != NULL) {
//                m_pidFile = optarg;
//            } else {
//                this->usage(argv[0]);
//            }
//            break;

//        case 'q':
//            DefaultStore_setBoolean(DsStorage_LIBRARY_ID, DsBool_QUICK_PRINT, 1);
//            break;

//        case 'r':
//            //defaultStore.toggleBoolean(DefaultStore.Storage.APPLICATION_ID,
//             //         DefaultStore.Booleans.AGENT_NO_ROOT_ACCESS);
//            break;

//        case 'U':
//            //netsnmp_ds_toggle_boolean(DefaultStore.Storage.APPLICATION_ID,
//            //         DefaultStore.Booleans.AGENT_LEAVE_PIDFILE);
//            break;
//        case 'u':
////            if (optarg != NULL) {
////                char           *ecp;
////                int             uid;

////                uid = strtoul(optarg, &ecp, 10);
////                if (*ecp) {
////                    struct passwd  *info;

////                    info = getpwnam(optarg);
////                    uid = info ? info->pw_uid : -1;
////                    endpwent();
////                }
////                if (uid < 0) {
////                    fprintf(stderr, "Bad user id: %s\n", optarg);
////                    exit(1);
////                }
////                //defaultStore.setInt(DefaultStore.Storage.APPLICATION_ID,
////                   //DefaultStore.Integers.AGENT_USERID, uid);
////            } else {
////                this->usage(argv[0]);
////            }
//            break;

//        case 'v':
//            this->version();

//        case 'V':
//            //defaultStore.setBoolean(DefaultStore.Storage.APPLICATION_ID,
//             //      NETSNMP_DS_AGENT_VERBOSE, 1);
//            break;

//        case 'x':
//            if (optarg != NULL) {
//                //netsnmp_ds_set_string(NETSNMP_DS_APPLICATION_ID,
//                //     NETSNMP_DS_AGENT_X_SOCKET, optarg);
//            } else {
//                this->usage(argv[0]);
//            }
//            //defaultStore.setBoolean(NETSNMP_DS_APPLICATION_ID,
//            //       NETSNMP_DS_AGENT_AGENTX_MASTER, 1);
//            break;

//        case 'X':
//            //m_agentMode = SUB_AGENT;
//            break;

//        case 'Y':
//            //netsnmp_config_remember(optarg);
//            break;

//        default:
//            this->usage(argv[0]);
//            break;
//        }
//    }
//}

