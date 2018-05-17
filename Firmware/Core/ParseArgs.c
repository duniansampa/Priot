#include "ParseArgs.h"
#include "Mib.h"
#include "Parse.h"
#include "System/Util/Logger.h",
#include "System/Util/Debug.h"
#include "ReadConfig.h"
#include "DefaultStore.h"
#include "Priot.h"
#include "System/Util/Utilities.h"
#include "System/Containers/Container.h"
#include "Transport.h"
#include "Keytools.h"
#include "Api.h"
#include "Version.h"
#include "Usm.h"
#include "V3.h"
#include "System/String.h"

int  parseArgs_randomAccess = 0;

void ParseArgs_usage(FILE * outf)
{
    fprintf(outf, "[OPTIONS] AGENT");
}

void  ParseArgs_descriptions(FILE * outf)
{
    fprintf(outf, "  Version:  %s\n", Version_getVersion());
    fprintf(outf, "  Web:      http://www.net-snmp.org/\n");
    fprintf(outf,
            "  Email:    net-snmp-coders@lists.sourceforge.net\n\nOPTIONS:\n");
    fprintf(outf, "  -h, --help\t\tdisplay this help message\n");
    fprintf(outf,
            "  -H\t\t\tdisplay configuration file directives understood\n");
    fprintf(outf, "  -v 1|2c|3\t\tspecifies SNMP version to use\n");
    fprintf(outf, "  -V, --version\t\tdisplay package version number\n");

    fprintf(outf, "SNMP Version 3 specific\n");
    fprintf(outf,
            "  -a PROTOCOL\t\tset authentication protocol (MD5|SHA)\n");
    fprintf(outf,
            "  -A PASSPHRASE\t\tset authentication protocol pass phrase\n");
    fprintf(outf,
            "  -e ENGINE-ID\t\tset security engine ID (e.g. 800000020109840301)\n");
    fprintf(outf,
            "  -E ENGINE-ID\t\tset context engine ID (e.g. 800000020109840301)\n");
    fprintf(outf,
            "  -l LEVEL\t\tset security level (noAuthNoPriv|authNoPriv|authPriv)\n");
    fprintf(outf, "  -n CONTEXT\t\tset context name (e.g. bridge1)\n");
    fprintf(outf, "  -u USER-NAME\t\tset security name (e.g. bert)\n");

    fprintf(outf, "  -x PROTOCOL\t\tset privacy protocol (DES|AES)\n");

    fprintf(outf, "  -X PASSPHRASE\t\tset privacy protocol pass phrase\n");
    fprintf(outf,
            "  -Z BOOTS,TIME\t\tset destination engine boots/time\n");
    fprintf(outf, "General communication options\n");
    fprintf(outf, "  -r RETRIES\t\tset the number of retries\n");
    fprintf(outf,
            "  -t TIMEOUT\t\tset the request timeout (in seconds)\n");
    fprintf(outf, "Debugging\n");
    fprintf(outf, "  -d\t\t\tdump input/output packets in hexadecimal\n");
    fprintf(outf,
            "  -D[TOKEN[,...]]\tturn on debugging output for the specified TOKENs\n\t\t\t   (ALL gives extremely verbose debugging output)\n");
    fprintf(outf, "General options\n");
    fprintf(outf,
            "  -m MIB[" ENV_SEPARATOR "...]\t\tload given list of MIBs (ALL loads everything)\n");
    fprintf(outf,
            "  -M DIR[" ENV_SEPARATOR "...]\t\tlook in given list of directories for MIBs\n");
    fprintf(outf,
            "    (default: %s)\n", Mib_getMibDirectory());
    fprintf(outf,
            "  -P MIBOPTS\t\tToggle various defaults controlling MIB parsing:\n");
    Parse_mibToggleOptionsUsage("\t\t\t  ", outf);
    fprintf(outf,
            "  -O OUTOPTS\t\tToggle various defaults controlling output display:\n");
    Mib_outToggleOptionsUsage("\t\t\t  ", outf);
    fprintf(outf,
            "  -I INOPTS\t\tToggle various defaults controlling input parsing:\n");
    Mib_inToggleOptionsUsage("\t\t\t  ", outf);
    fprintf(outf,
            "  -L LOGOPTS\t\tToggle various defaults controlling logging:\n");
    Logger_logOptionsUsage("\t\t\t  ", outf);
    fflush(outf);
}

#define PARSEARGS_BUF_SIZE 512

void  ParseArgs_handleLongOpt(const char *myoptarg)
{
    char           *cp, *cp2;
    /*
     * else it's a long option, so process it like name=value
     */
    cp = (char *)malloc(strlen(myoptarg) + 3);
    if (!cp)
        return;
    strcpy(cp, myoptarg);
    cp2 = strchr(cp, '=');
    if (!cp2 && !strchr(cp, ' ')) {
        /*
         * well, they didn't specify an argument as far as we
         * can tell.  Give them a '1' as the argument (which
         * works for boolean tokens and a few others) and let
         * them suffer from there if it's not what they
         * wanted
         */
        strcat(cp, " 1");
    } else {
        /*
         * replace the '=' with a ' '
         */
        if (cp2)
            *cp2 = ' ';
    }
    ReadConfig_config(cp);
    free(cp);
}


int  ParseArgs_parseArgs(int argc,
                   char **argv,
                   Types_Session * session, const char *localOpts,
                   void (*proc) (int, char *const *, int),
                   int flags)
{
    static char	   *sensitive[4] = { NULL, NULL, NULL, NULL };
    int             arg, sp = 0, testcase = 0;
    char           *cp;
    char           *Apsz = NULL;
    char           *Xpsz = NULL;
    char           *Cpsz = NULL;
    char            Opts[PARSEARGS_BUF_SIZE];
    int             zero_sensitive = !( flags & PARSEARGS_NOZERO );

    /*
     * initialize session to default values
     */
    Api_sessInit(session);
    strcpy(Opts, "Y:VhHm:M:O:I:P:D:dv:r:t:c:Z:e:E:n:u:l:x:X:a:A:p:T:-:3:L:");
    if (localOpts) {
        if (strlen(localOpts) + strlen(Opts) >= sizeof(Opts)) {
            Logger_log(LOGGER_PRIORITY_ERR, "Too many localOpts in snmp_parse_args()\n");
            return -1;
        }
        strcat(Opts, localOpts);
    }

    /*
     * get the options
     */
    DEBUG_MSGTL(("snmp_parse_args", "starting: %d/%d\n", optind, argc));
    for (arg = 0; arg < argc; arg++) {
        DEBUG_MSGTL(("snmp_parse_args", " arg %d = %s\n", arg, argv[arg]));
    }

    optind = 1;
    while ((arg = getopt(argc, argv, Opts)) != EOF) {
        DEBUG_MSGTL(("snmp_parse_args", "handling (#%d): %c\n", optind, arg));
        switch (arg) {
        case '-':
            if (strcasecmp(optarg, "help") == 0) {
                return (PARSEARGS_ERROR_USAGE);
            }
            if (strcasecmp(optarg, "version") == 0) {
                fprintf(stderr,"NET-SNMP version: %s\n",Version_getVersion());
                return (PARSEARGS_SUCCESS_EXIT);
            }

            ParseArgs_handleLongOpt(optarg);
            break;

        case 'V':
            fprintf(stderr, "NET-SNMP version: %s\n", Version_getVersion());
            return (PARSEARGS_SUCCESS_EXIT);

        case 'h':
            return (PARSEARGS_ERROR_USAGE);
            break;

        case 'H':
            Api_init("snmpapp");
            fprintf(stderr, "Configuration directives understood:\n");
            ReadConfig_printUsage("  ");
            return (PARSEARGS_SUCCESS_EXIT);

        case 'Y':
            ReadConfig_remember(optarg);
            break;

        case 'm':
            setenv("MIBS", optarg, 1);
            break;

        case 'M':
            Mib_getMibDirectory(); /* prepare the default directories */
            Mib_setMibDirectory(optarg);
            break;

        case 'O':
            cp = Mib_outToggleOptions(optarg);
            if (cp != NULL) {
                fprintf(stderr, "Unknown output option passed to -O: %c.\n",
            *cp);
                return (PARSEARGS_ERROR_USAGE);
            }
            break;

        case 'I':
            cp = Mib_inOptions(optarg, argc, argv);
            if (cp != NULL) {
                fprintf(stderr, "Unknown input option passed to -I: %c.\n",
            *cp);
                return (PARSEARGS_ERROR_USAGE);
            }
            break;

        case 'P':
            cp = Parse_mibToggleOptions(optarg);
            if (cp != NULL) {
                fprintf(stderr,
                        "Unknown parsing option passed to -P: %c.\n", *cp);
                return (PARSEARGS_ERROR_USAGE);
            }
            break;

        case 'D':
            Debug_registerTokens(optarg);
            Debug_setDoDebugging(1);
            break;

        case 'd':
            DefaultStore_setBoolean(DsStorage_LIBRARY_ID,
                   DsBool_DUMP_PACKET, 1);
            break;

        case 'v':
            session->version = -1;
            if (!strcasecmp(optarg, "3")) {
                session->version = PRIOT_VERSION_3;
            }
            if (session->version == -1) {
                fprintf(stderr,
                        "Invalid version specified after -v flag: %s\n",
                        optarg);
                return (PARSEARGS_ERROR_USAGE);
            }
            break;

        case 'p':
            fprintf(stderr, "Warning: -p option is no longer used - ");
            fprintf(stderr, "specify the remote host as HOST:PORT\n");
            return (PARSEARGS_ERROR_USAGE);
            break;

        case 'T':
        {
            char leftside[UTILITIES_MAX_BUFFER_MEDIUM], rightside[UTILITIES_MAX_BUFFER_MEDIUM];
            char *tmpcp, *tmpopt;

            /* ensure we have a proper argument */
            tmpopt = strdup(optarg);
            tmpcp = strchr(tmpopt, '=');
            if (!tmpcp) {
                fprintf(stderr, "-T expects a NAME=VALUE pair.\n");
                return (PARSEARGS_ERROR_USAGE);
            }
            *tmpcp++ = '\0';

            /* create the transport config container if this is the first */
            if (!session->transportConfiguration) {
                Container_initList();
                session->transportConfiguration =
                    Container_find("transport_configuration:fifo");
                if (!session->transportConfiguration) {
                    fprintf(stderr, "failed to initialize the transport configuration container\n");
                    free(tmpopt);
                    return (PARSEARGS_ERROR);
                }

                session->transportConfiguration->compare =
                    (Container_FuncCompare*)
                    Transport_configCompare;
            }

            /* set the config */
            String_copyTruncate(leftside, tmpopt, sizeof(leftside));
            String_copyTruncate(rightside, tmpcp, sizeof(rightside));

            CONTAINER_INSERT(session->transportConfiguration,
                             Transport_createConfig(leftside, rightside));
            free(tmpopt);
        }
        break;

        case 't':
            session->timeout = (long)(atof(optarg) * 1000000L);
            if (session->timeout <= 0) {
                fprintf(stderr, "Invalid timeout in seconds after -t flag.\n");
                return (PARSEARGS_ERROR_USAGE);
            }
            break;

        case 'r':
            session->retries = atoi(optarg);
            if (session->retries < 0 || !isdigit((unsigned char)(optarg[0]))) {
                fprintf(stderr, "Invalid number of retries after -r flag.\n");
                return (PARSEARGS_ERROR_USAGE);
            }
            break;

        case 'c':
        if (zero_sensitive) {
        if ((sensitive[sp] = strdup(optarg)) != NULL) {
            Cpsz = sensitive[sp];
            memset(optarg, '\0', strlen(optarg));
            sp++;
        } else {
            fprintf(stderr, "malloc failure processing -c flag.\n");
            return PARSEARGS_ERROR;
        }
        } else {
        Cpsz = optarg;
        }
            break;

        case '3':
        /*  TODO: This needs to zero things too.  */
            if (V3_options(optarg, session, &Apsz, &Xpsz, argc, argv) < 0){
                return (PARSEARGS_ERROR_USAGE);
            }
            break;

        case 'L':
            if (Logger_logOptions(optarg, argc, argv) < 0) {
                return (PARSEARGS_ERROR_USAGE);
            }
            break;

#define SNMPV3_CMD_OPTIONS
        case 'Z':
            errno = 0;
            session->engineBoots = strtoul(optarg, &cp, 10);
            if (errno || cp == optarg) {
                fprintf(stderr, "Need engine boots value after -Z flag.\n");
                return (PARSEARGS_ERROR_USAGE);
            }
            if (*cp == ',') {
                char *endptr;
                cp++;
                session->engineTime = strtoul(cp, &endptr, 10);
                if (errno || cp == endptr) {
                    fprintf(stderr, "Need engine time after \"-Z engineBoot,\".\n");
                    return (PARSEARGS_ERROR_USAGE);
                }
            }
            /*
             * Handle previous '-Z boot time' syntax
             */
            else if (optind < argc) {
                session->engineTime = strtoul(argv[optind], &cp, 10);
                if (errno || cp == argv[optind]) {
                    fprintf(stderr, "Need engine time after \"-Z engineBoot\".\n");
                    return (PARSEARGS_ERROR_USAGE);
                }
            } else {
                fprintf(stderr, "Need engine time after \"-Z engineBoot\".\n");
                return (PARSEARGS_ERROR_USAGE);
            }
            break;

        case 'e':{
                size_t ebuf_len = 32, eout_len = 0;
                u_char *ebuf = (u_char *)malloc(ebuf_len);

                if (ebuf == NULL) {
                    fprintf(stderr, "malloc failure processing -e flag.\n");
                    return (PARSEARGS_ERROR);
                }
                if (!Convert_hexStringToBinaryStringWrapper
                    (&ebuf, &ebuf_len, &eout_len, 1, optarg)) {
                    fprintf(stderr, "Bad engine ID value after -e flag.\n");
                    free(ebuf);
                    return (PARSEARGS_ERROR_USAGE);
                }
                if ((eout_len < 5) || (eout_len > 32)) {
                    fprintf(stderr, "Invalid engine ID value after -e flag.\n");
                    free(ebuf);
                    return (PARSEARGS_ERROR_USAGE);
                }
                session->securityEngineID = ebuf;
                session->securityEngineIDLen = eout_len;
                break;
            }

        case 'E':{
                size_t ebuf_len = 32, eout_len = 0;
                u_char *ebuf = (u_char *)malloc(ebuf_len);

                if (ebuf == NULL) {
                    fprintf(stderr, "malloc failure processing -E flag.\n");
                    return (PARSEARGS_ERROR);
                }
                if (!Convert_hexStringToBinaryStringWrapper(&ebuf, &ebuf_len,
                    &eout_len, 1, optarg)) {
                    fprintf(stderr, "Bad engine ID value after -E flag.\n");
                    free(ebuf);
                    return (PARSEARGS_ERROR_USAGE);
                }
                if ((eout_len < 5) || (eout_len > 32)) {
                    fprintf(stderr, "Invalid engine ID value after -E flag.\n");
                    free(ebuf);
                    return (PARSEARGS_ERROR_USAGE);
                }
                session->contextEngineID = ebuf;
                session->contextEngineIDLen = eout_len;
                break;
            }

        case 'n':
            session->contextName = optarg;
            session->contextNameLen = strlen(optarg);
            break;

        case 'u':
        if (zero_sensitive) {
        if ((sensitive[sp] = strdup(optarg)) != NULL) {
            session->securityName = sensitive[sp];
            session->securityNameLen = strlen(sensitive[sp]);
            memset(optarg, '\0', strlen(optarg));
            sp++;
        } else {
            fprintf(stderr, "malloc failure processing -u flag.\n");
            return PARSEARGS_ERROR;
        }
        } else {
        session->securityName = optarg;
        session->securityNameLen = strlen(optarg);
        }
            break;

        case 'l':
            if (!strcasecmp(optarg, "noAuthNoPriv") || !strcmp(optarg, "1")
                || !strcasecmp(optarg, "noauth")
                || !strcasecmp(optarg, "nanp")) {
                session->securityLevel = PRIOT_SEC_LEVEL_NOAUTH;
            } else if (!strcasecmp(optarg, "authNoPriv")
                       || !strcmp(optarg, "2")
                       || !strcasecmp(optarg, "auth")
                       || !strcasecmp(optarg, "anp")) {
                session->securityLevel = PRIOT_SEC_LEVEL_AUTHNOPRIV;
            } else if (!strcasecmp(optarg, "authPriv")
                       || !strcmp(optarg, "3")
                       || !strcasecmp(optarg, "priv")
                       || !strcasecmp(optarg, "ap")) {
                session->securityLevel = PRIOT_SEC_LEVEL_AUTHPRIV;
            } else {
                fprintf(stderr,
                        "Invalid security level specified after -l flag: %s\n",
                        optarg);
                return (PARSEARGS_ERROR_USAGE);
            }

            break;

        case 'a':
            if (!strcasecmp(optarg, "MD5")) {
                session->securityAuthProto = usm_hMACMD5AuthProtocol;
                session->securityAuthProtoLen = USM_AUTH_PROTO_MD5_LEN;
            } else
                if (!strcasecmp(optarg, "SHA")) {
                session->securityAuthProto = usm_hMACSHA1AuthProtocol;
                session->securityAuthProtoLen = USM_AUTH_PROTO_SHA_LEN;
            } else {
                fprintf(stderr,
                        "Invalid authentication protocol specified after -a flag: %s\n",
                        optarg);
                return (PARSEARGS_ERROR_USAGE);
            }
            break;

        case 'x':
            testcase = 0;
            if (!strcasecmp(optarg, "DES")) {
                testcase = 1;
                session->securityPrivProto = usm_dESPrivProtocol;
                session->securityPrivProtoLen = USM_PRIV_PROTO_DES_LEN;
            }
            if (!strcasecmp(optarg, "AES128") ||
                !strcasecmp(optarg, "AES")) {
                testcase = 1;
                session->securityPrivProto = usm_aESPrivProtocol;
                session->securityPrivProtoLen = USM_PRIV_PROTO_AES_LEN;
            }
            if (testcase == 0) {
                fprintf(stderr,
                      "Invalid privacy protocol specified after -x flag: %s\n",
                        optarg);
                return (PARSEARGS_ERROR_USAGE);
            }
            break;

        case 'A':
        if (zero_sensitive) {
        if ((sensitive[sp] = strdup(optarg)) != NULL) {
            Apsz = sensitive[sp];
            memset(optarg, '\0', strlen(optarg));
            sp++;
        } else {
            fprintf(stderr, "malloc failure processing -A flag.\n");
            return PARSEARGS_ERROR;
        }
        } else {
        Apsz = optarg;
        }
            break;

        case 'X':
        if (zero_sensitive) {
        if ((sensitive[sp] = strdup(optarg)) != NULL) {
            Xpsz = sensitive[sp];
            memset(optarg, '\0', strlen(optarg));
            sp++;
        } else {
            fprintf(stderr, "malloc failure processing -X flag.\n");
            return PARSEARGS_ERROR;
        }
        } else {
        Xpsz = optarg;
        }
            break;

        case '?':
            return (PARSEARGS_ERROR_USAGE);
            break;

        default:
            proc(argc, argv, arg);
            break;
        }
    }
    DEBUG_MSGTL(("snmp_parse_args", "finished: %d/%d\n", optind, argc));

    /*
     * read in MIB database and initialize the priot library
     */
    Api_init("snmpapp");

    /*
     * session default version
     */
    if (session->version == API_DEFAULT_VERSION) {
        /*
         * run time default version
         */
        session->version = DefaultStore_getInt(DsStorage_LIBRARY_ID,
                          DsInt_SNMPVERSION);

        /*
         * compile time default version
         */
        if (!session->version) {
            switch (DEFAULT_PRIOT_VERSION) {
            case 3:
                session->version = PRIOT_VERSION_3;
                break;

            default:
                Logger_log(LOGGER_PRIORITY_ERR, "Can't determine a valid SNMP version for the session\n");
                return(PARSEARGS_ERROR);
            }
        } else {
        }
    }

    /* XXX: this should ideally be moved to snmpusm.c somehow */

    /*
     * make master key from pass phrases
     */
    if (Apsz) {
        session->securityAuthKeyLen = USM_AUTH_KU_LEN;
        if (session->securityAuthProto == NULL) {
            /*
             * get .conf set default
             */
            const oid      *def =
                Usm_getDefaultAuthtype(&session->securityAuthProtoLen);
            session->securityAuthProto =
                Api_duplicateObjid(def, session->securityAuthProtoLen);
        }
        if (session->securityAuthProto == NULL) {
            /*
             * assume MD5
             */
            session->securityAuthProto =
                Api_duplicateObjid(usm_hMACMD5AuthProtocol,
                                     USM_AUTH_PROTO_MD5_LEN);
            session->securityAuthProtoLen = USM_AUTH_PROTO_MD5_LEN;

        }
        if (Keytools_generateKu(session->securityAuthProto,
                        session->securityAuthProtoLen,
                        (u_char *) Apsz, strlen(Apsz),
                        session->securityAuthKey,
                        &session->securityAuthKeyLen) != ErrorCode_SUCCESS) {
            Api_perror(argv[0]);
            fprintf(stderr,
                    "Error generating a key (Ku) from the supplied authentication pass phrase. \n");
            return (PARSEARGS_ERROR);
        }
    }
    if (Xpsz) {
        session->securityPrivKeyLen = USM_PRIV_KU_LEN;
        if (session->securityPrivProto == NULL) {
            /*
             * get .conf set default
             */
            const oid      *def =
                Usm_getDefaultPrivtype(&session->securityPrivProtoLen);
            session->securityPrivProto =
                Api_duplicateObjid(def, session->securityPrivProtoLen);
        }
        if (session->securityPrivProto == NULL) {
            /*
             * assume DES
             */
            session->securityPrivProto =
                Api_duplicateObjid(usm_dESPrivProtocol,
                                     USM_PRIV_PROTO_DES_LEN);
            session->securityPrivProtoLen = USM_PRIV_PROTO_DES_LEN;
        }
        if (Keytools_generateKu(session->securityAuthProto,
                        session->securityAuthProtoLen,
                        (u_char *) Xpsz, strlen(Xpsz),
                        session->securityPrivKey,
                        &session->securityPrivKeyLen) != ErrorCode_SUCCESS) {
            Api_perror(argv[0]);
            fprintf(stderr,
                    "Error generating a key (Ku) from the supplied privacy pass phrase. \n");
            return (PARSEARGS_ERROR);
        }
    }

    /*
     * get the hostname
     */
    if (optind == argc) {
        fprintf(stderr, "No hostname specified.\n");
        return (PARSEARGS_ERROR_USAGE);
    }
    session->peername = argv[optind++]; /* hostname */


    return optind;
}

int ParseArgs_parseArgs2(int argc,
                char **argv,
                Types_Session * session, const char *localOpts,
                void (*proc) (int, char *const *, int))
{
    return ParseArgs_parseArgs(argc, argv, session, localOpts, proc, 0);
}
