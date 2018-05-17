#include "V3.h"
#include "System/Util/Utilities.h"
#include "Priot.h"
#include "DefaultStore.h"
#include "System/Util/Utilities.h"
#include "ReadConfig.h"
#include "Usm.h"
#include "System.h"
#include "System/Util/Logger.h"
#include "Secmod.h"
#include "LcdTime.h"
#include "System/String.h"
#include "Api.h"
#include "System/Util/Debug.h"

#include <sys/ioctl.h>
#include <net/if.h>

static u_long   _v3_engineBoots = 1;
static unsigned int _v3_engineIDType = ENGINEID_TYPE_NETSNMP_RND;
static unsigned char * _v3_engineID = NULL;
static size_t   _v3_engineIDLength = 0;
static unsigned char * _v3_engineIDNic = NULL;
static unsigned int _v3_engineIDIsSet = 0;  /* flag if ID set by config */
static unsigned char * _v3_oldEngineID = NULL;
static size_t   _v3_oldEngineIDLength = 0;
static struct timeval _v3_starttime;

static int      _V3_getHwAddress(const char *networkDevice, char *addressOut);

/*******************************************************************-o-******
 * snmpv3_secLevel_conf
 *
 * Parameters:
 *	*word
 *	*cptr
 *
 * Line syntax:
 *	defSecurityLevel "noAuthNoPriv" | "authNoPriv" | "authPriv"
 */

int V3_parseSecLevelConf(const char *word, char *cptr) {
    if (strcasecmp(cptr, "noAuthNoPriv") == 0 || strcmp(cptr, "1") == 0 ||
    strcasecmp(cptr, "nanp") == 0) {
        return PRIOT_SEC_LEVEL_NOAUTH;
    } else if (strcasecmp(cptr, "authNoPriv") == 0 || strcmp(cptr, "2") == 0 ||
           strcasecmp(cptr, "anp") == 0) {
        return PRIOT_SEC_LEVEL_AUTHNOPRIV;
    } else if (strcasecmp(cptr, "authPriv") == 0 || strcmp(cptr, "3") == 0 ||
           strcasecmp(cptr, "ap") == 0) {
        return PRIOT_SEC_LEVEL_AUTHPRIV;
    } else {
        return -1;
    }
}

void V3_secLevelConf(const char *word, char *cptr)
{
    int             secLevel;

    if ((secLevel = V3_parseSecLevelConf( word, cptr )) >= 0 ) {
        DefaultStore_setInt(DsStorage_LIBRARY_ID, DsInt_SECLEVEL, secLevel);
    } else {
    ReadConfig_error("Unknown security level: %s", cptr);
    }
    DEBUG_MSGTL(("snmpv3", "default secLevel set to: %s = %d\n", cptr,
                DefaultStore_getInt(DsStorage_LIBRARY_ID,
                   DsInt_SECLEVEL)));
}



int V3_options(char *optarg, Types_Session * session, char **Apsz,
               char **Xpsz, int argc, char *const *argv)
{
    char           *cp = optarg;
    int testcase;
    optarg++;
    /*
     * Support '... -3x=value ....' syntax
     */
    if (*optarg == '=') {
        optarg++;
    }
    /*
     * and '.... "-3x value" ....'  (*with* the quotes)
     */
    while (*optarg && isspace((unsigned char)(*optarg))) {
        optarg++;
    }
    /*
     * Finally, handle ".... -3x value ...." syntax
     *   (*without* surrounding quotes)
     */
    if (!*optarg) {
        /*
         * We've run off the end of the argument
         *  so move on the the next.
         */
        optarg = argv[optind++];
        if (optind > argc) {
            fprintf(stderr,
                    "Missing argument after SNMPv3 '-3%c' option.\n", *cp);
            return (-1);
        }
    }

    switch (*cp) {

    case 'Z':
        errno=0;
        session->engineBoots = strtoul(optarg, &cp, 10);
        if (errno || cp == optarg) {
            fprintf(stderr, "Need engine boots value after -3Z flag.\n");
            return (-1);
        }
        if (*cp == ',') {
            char *endptr;
            cp++;
            session->engineTime = strtoul(cp, &endptr, 10);
            if (errno || cp == endptr) {
                fprintf(stderr, "Need engine time after \"-3Z engineBoot,\".\n");
                return (-1);
            }
        } else {
            fprintf(stderr, "Need engine time after \"-3Z engineBoot,\".\n");
            return (-1);
        }
        break;

    case 'e':{
            size_t          ebuf_len = 32, eout_len = 0;
            u_char         *ebuf = (u_char *) malloc(ebuf_len);

            if (ebuf == NULL) {
                fprintf(stderr, "malloc failure processing -3e flag.\n");
                return (-1);
            }
            if (!Convert_hexStringToBinaryStringWrapper
                (&ebuf, &ebuf_len, &eout_len, 1, optarg)) {
                fprintf(stderr, "Bad engine ID value after -3e flag.\n");
                MEMORY_FREE(ebuf);
                return (-1);
            }
            session->securityEngineID = ebuf;
            session->securityEngineIDLen = eout_len;
            break;
        }

    case 'E':{
            size_t          ebuf_len = 32, eout_len = 0;
            u_char         *ebuf = (u_char *) malloc(ebuf_len);

            if (ebuf == NULL) {
                fprintf(stderr, "malloc failure processing -3E flag.\n");
                return (-1);
            }
            if (!Convert_hexStringToBinaryStringWrapper
                (&ebuf, &ebuf_len, &eout_len, 1, optarg)) {
                fprintf(stderr, "Bad engine ID value after -3E flag.\n");
                MEMORY_FREE(ebuf);
                return (-1);
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
        session->securityName = optarg;
        session->securityNameLen = strlen(optarg);
        break;

    case 'l':
        if (!strcasecmp(optarg, "noAuthNoPriv") || !strcmp(optarg, "1") ||
            !strcasecmp(optarg, "nanp")) {
            session->securityLevel = PRIOT_SEC_LEVEL_NOAUTH;
        } else if (!strcasecmp(optarg, "authNoPriv")
                   || !strcmp(optarg, "2") || !strcasecmp(optarg, "anp")) {
            session->securityLevel = PRIOT_SEC_LEVEL_AUTHNOPRIV;
        } else if (!strcasecmp(optarg, "authPriv") || !strcmp(optarg, "3")
                   || !strcasecmp(optarg, "ap")) {
            session->securityLevel = PRIOT_SEC_LEVEL_AUTHPRIV;
        } else {
            fprintf(stderr,
                    "Invalid security level specified after -3l flag: %s\n",
                    optarg);
            return (-1);
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
                    "Invalid authentication protocol specified after -3a flag: %s\n",
                    optarg);
            return (-1);
        }
        break;

    case 'x':
        testcase = 0;
        if (!strcasecmp(optarg, "DES")) {
            session->securityPrivProto = usm_dESPrivProtocol;
            session->securityPrivProtoLen = USM_PRIV_PROTO_DES_LEN;
            testcase = 1;
        }
        if (!strcasecmp(optarg, "AES128") ||
            strcasecmp(optarg, "AES")) {
            session->securityPrivProto = usm_aES128PrivProtocol;
            session->securityPrivProtoLen = USM_PRIV_PROTO_AES128_LEN;
            testcase = 1;
        }
        if (testcase == 0) {
            fprintf(stderr,
                    "Invalid privacy protocol specified after -3x flag: %s\n",
                    optarg);
            return (-1);
        }
        break;

    case 'A':
        *Apsz = optarg;
        break;

    case 'X':
        *Xpsz = optarg;
        break;

    case 'm': {
        size_t bufSize = sizeof(session->securityAuthKey);
        u_char *tmpp = session->securityAuthKey;
        if (!Convert_hexStringToBinaryStringWrapper(&tmpp, &bufSize,
                                &session->securityAuthKeyLen, 0, optarg)) {
            fprintf(stderr, "Bad key value after -3m flag.\n");
            return (-1);
        }
        break;
    }

    case 'M': {
        size_t bufSize = sizeof(session->securityPrivKey);
        u_char *tmpp = session->securityPrivKey;
        if (!Convert_hexStringToBinaryStringWrapper(&tmpp, &bufSize,
             &session->securityPrivKeyLen, 0, optarg)) {
            fprintf(stderr, "Bad key value after -3M flag.\n");
            return (-1);
        }
        break;
    }

    case 'k': {
        size_t          kbuf_len = 32, kout_len = 0;
        u_char         *kbuf = (u_char *) malloc(kbuf_len);

        if (kbuf == NULL) {
            fprintf(stderr, "malloc failure processing -3k flag.\n");
            return (-1);
        }
        if (!Convert_hexStringToBinaryStringWrapper
            (&kbuf, &kbuf_len, &kout_len, 1, optarg)) {
            fprintf(stderr, "Bad key value after -3k flag.\n");
            MEMORY_FREE(kbuf);
            return (-1);
        }
        session->securityAuthLocalKey = kbuf;
        session->securityAuthLocalKeyLen = kout_len;
        break;
    }

    case 'K': {
        size_t          kbuf_len = 32, kout_len = 0;
        u_char         *kbuf = (u_char *) malloc(kbuf_len);

        if (kbuf == NULL) {
            fprintf(stderr, "malloc failure processing -3K flag.\n");
            return (-1);
        }
        if (!Convert_hexStringToBinaryStringWrapper
            (&kbuf, &kbuf_len, &kout_len, 1, optarg)) {
            fprintf(stderr, "Bad key value after -3K flag.\n");
            MEMORY_FREE(kbuf);
            return (-1);
        }
        session->securityPrivLocalKey = kbuf;
        session->securityPrivLocalKeyLen = kout_len;
        break;
    }

    default:
        fprintf(stderr, "Unknown SNMPv3 option passed to -3: %c.\n", *cp);
        return -1;
    }
    return 0;
}

/*******************************************************************-o-******
 * setup_engineID
 *
 * Parameters:
 *	**eidp
 *	 *text	Printable (?) text to be plugged into the snmpEngineID.
 *
 * Return:
 *	Length of allocated engineID string in bytes,  -OR-
 *	-1 on error.
 *
 *
 * Create an snmpEngineID using text and the local IP address.  If eidp
 * is defined, use it to return a pointer to the newly allocated data.
 * Otherwise, use the result to define engineID defined in this module.
 *
 * Line syntax:
 *	engineID <text> | NULL
 *
 * XXX	What if a node has multiple interfaces?
 * XXX	What if multiple engines all choose the same address?
 *      (answer:  You're screwed, because you might need a kul database
 *       which is dependant on the current engineID.  Enumeration and other
 *       tricks won't work).
 */
int V3_setupEngineID(u_char ** eidp, const char *text)
{
    int             enterpriseid = htonl(ENTERPRISE_OID),
        netsnmpoid = htonl(NET_OID),
        localsetup = (eidp) ? 0 : 1;

    /*
     * Use local engineID if *eidp == NULL.
     */
    u_char          buf[UTILITIES_MAX_BUFFER_SMALL];
    struct hostent *hent = NULL;

    u_char         *bufp = NULL;
    size_t          len;
    int             localEngineIDType = _v3_engineIDType;
    int             tmpint;
    time_t          tmptime;

    _v3_engineIDIsSet = 1;

    /*
     * see if they selected IPV4 or IPV6 support
     */
    if ((ENGINEID_TYPE_IPV6 == localEngineIDType) ||
        (ENGINEID_TYPE_IPV4 == localEngineIDType)) {
        /*
         * get the host name and save the information
         */
        gethostname((char *) buf, sizeof(buf));
        hent = System_gethostbyname((char *) buf);
        if (hent && hent->h_addrtype == AF_INET6) {
            localEngineIDType = ENGINEID_TYPE_IPV6;
        } else {
            /*
             * Not IPV6 so we go with default
             */
            localEngineIDType = ENGINEID_TYPE_IPV4;
        }
    }

    /*
     * Determine if we have text and if so setup our localEngineIDType
     * * appropriately.
     */
    if (NULL != text) {
        _v3_engineIDType = localEngineIDType = ENGINEID_TYPE_TEXT;
    }
    /*
     * Determine length of the engineID string.
     */
    len = 5;                    /* always have 5 leading bytes */
    switch (localEngineIDType) {
    case ENGINEID_TYPE_TEXT:
        if (NULL == text) {
            Logger_log(LOGGER_PRIORITY_ERR,
                     "Can't set up engineID of type text from an empty string.\n");
            return -1;
        }
        len += strlen(text);    /* 5 leading bytes+text. No NULL char */
        break;
    case ENGINEID_TYPE_MACADDR:        /* MAC address */
        len += 6;               /* + 6 bytes for MAC address */
        break;
    case ENGINEID_TYPE_IPV4:   /* IPv4 */
        len += 4;               /* + 4 byte IPV4 address */
        break;
    case ENGINEID_TYPE_IPV6:   /* IPv6 */
        len += 16;              /* + 16 byte IPV6 address */
        break;
    case ENGINEID_TYPE_NETSNMP_RND:        /* Net-SNMP specific encoding */
        if (_v3_engineID)           /* already setup, keep current value */
            return _v3_engineIDLength;
        if (_v3_oldEngineID) {
            len = _v3_oldEngineIDLength;
        } else {
            len += sizeof(int) + sizeof(time_t);
        }
        break;
    default:
        Logger_log(LOGGER_PRIORITY_ERR,
                 "Unknown EngineID type requested for setup (%d).  Using IPv4.\n",
                 localEngineIDType);
        localEngineIDType = ENGINEID_TYPE_IPV4; /* make into IPV4 */
        len += 4;               /* + 4 byte IPv4 address */
        break;
    }                           /* switch */


    /*
     * Allocate memory and store enterprise ID.
     */
    if ((bufp = (u_char *) malloc(len)) == NULL) {
        Logger_logPerror("setup_engineID malloc");
        return -1;
    }
    if (localEngineIDType == ENGINEID_TYPE_NETSNMP_RND)
        /*
         * we must use the net-snmp enterprise id here, regardless
         */
        memcpy(bufp, &netsnmpoid, sizeof(netsnmpoid));    /* XXX Must be 4 bytes! */
    else
        memcpy(bufp, &enterpriseid, sizeof(enterpriseid));      /* XXX Must be 4 bytes! */

    bufp[0] |= 0x80;


    /*
     * Store the given text  -OR-   the first found IP address
     *  -OR-  the MAC address  -OR-  random elements
     * (the latter being the recommended default)
     */
    switch (localEngineIDType) {
    case ENGINEID_TYPE_NETSNMP_RND:
        if (_v3_oldEngineID) {
            /*
             * keep our previous notion of the engineID
             */
            memcpy(bufp, _v3_oldEngineID, _v3_oldEngineIDLength);
        } else {
            /*
             * Here we've desigend our own ENGINEID that is not based on
             * an address which may change and may even become conflicting
             * in the future like most of the default v3 engineID types
             * suffer from.
             *
             * Ours is built from 2 fairly random elements: a random number and
             * the current time in seconds.  This method suffers from boxes
             * that may not have a correct clock setting and random number
             * seed at startup, but few OSes should have that problem.
             */
            bufp[4] = ENGINEID_TYPE_NETSNMP_RND;
            tmpint = random();
            memcpy(bufp + 5, &tmpint, sizeof(tmpint));
            tmptime = time(NULL);
            memcpy(bufp + 5 + sizeof(tmpint), &tmptime, sizeof(tmptime));
        }
        break;
    case ENGINEID_TYPE_TEXT:
        bufp[4] = ENGINEID_TYPE_TEXT;
        memcpy((char *) bufp + 5, (text), strlen(text));
        break;
    case ENGINEID_TYPE_IPV6:
        bufp[4] = ENGINEID_TYPE_IPV6;
        memcpy(bufp + 5, hent->h_addr_list[0], hent->h_length);
        break;

    case ENGINEID_TYPE_MACADDR:
        {
            int             x;
            bufp[4] = ENGINEID_TYPE_MACADDR;
            /*
             * use default NIC if none provided
             */
            if (NULL == _v3_engineIDNic) {
          x = _V3_getHwAddress(DEFAULT_NIC, (char *)&bufp[5]);
            } else {
          x = _V3_getHwAddress((char *)_v3_engineIDNic, (char *)&bufp[5]);
            }
            if (0 != x)
                /*
                 * function failed fill MAC address with zeros
                 */
            {
                memset(&bufp[5], 0, 6);
            }
        }
        break;
    case ENGINEID_TYPE_IPV4:
    default:
        bufp[4] = ENGINEID_TYPE_IPV4;
        if (hent && hent->h_addrtype == AF_INET) {
            memcpy(bufp + 5, hent->h_addr_list[0], hent->h_length);
        } else {                /* Unknown address type.  Default to 127.0.0.1. */

            bufp[5] = 127;
            bufp[6] = 0;
            bufp[7] = 0;
            bufp[8] = 1;
        }

        break;
    }

    /*
     * Pass the string back to the calling environment, or use it for
     * our local engineID.
     */
    if (localsetup) {
        MEMORY_FREE(_v3_engineID);
        _v3_engineID = bufp;
        _v3_engineIDLength = len;

    } else {
        *eidp = bufp;
    }


    return len;

}                               /* end setup_engineID() */

int V3_freeEngineID(int majorid, int minorid, void *serverarg,
          void *clientarg)
{
    MEMORY_FREE(_v3_engineID);
    MEMORY_FREE(_v3_engineIDNic);
    MEMORY_FREE(_v3_oldEngineID);
    _v3_engineIDIsSet = 0;
    return 0;
}

/*******************************************************************-o-******
 * engineBoots_conf
 *
 * Parameters:
 *	*word
 *	*cptr
 *
 * Line syntax:
 *	engineBoots <num_boots>
 */
void V3_engineBootsConf(const char *word, char *cptr)
{
    _v3_engineBoots = atoi(cptr) + 1;
    DEBUG_MSGTL(("snmpv3", "v3_engineBoots: %lu\n", _v3_engineBoots));
}

/*******************************************************************-o-******
 * engineIDType_conf
 *
 * Parameters:
 *	*word
 *	*cptr
 *
 * Line syntax:
 *	engineIDType <1 or 3>
 *		1 is default for IPv4 engine ID type.  Will automatically
 *		    chose between IPv4 & IPv6 if either 1 or 2 is specified.
 *		2 is for IPv6.
 *		3 is hardware (MAC) address, currently supported under Linux
 */
void V3_engineIDTypeConf(const char *word, char *cptr)
{
    _v3_engineIDType = atoi(cptr);
    /*
     * verify valid type selected
     */
    switch (_v3_engineIDType) {
    case ENGINEID_TYPE_IPV4:   /* IPv4 */
    case ENGINEID_TYPE_IPV6:   /* IPv6 */
        /*
         * IPV? is always good
         */
        break;
    case ENGINEID_TYPE_MACADDR:        /* MAC address */
        break;
    default:
        /*
         * unsupported one chosen
         */
        ReadConfig_configPerror("Unsupported enginedIDType, forcing IPv4");
        _v3_engineIDType = ENGINEID_TYPE_IPV4;
    }
    DEBUG_MSGTL(("snmpv3", "engineIDType: %d\n", _v3_engineIDType));
}

/*******************************************************************-o-******
 * engineIDNic_conf
 *
 * Parameters:
 *	*word
 *	*cptr
 *
 * Line syntax:
 *	engineIDNic <string>
 *		eth0 is default
 */
void
V3_engineIDNicConf(const char *word, char *cptr)
{
    /*
     * Make sure they haven't already specified the engineID via the
     * * configuration file
     */
    if (0 == _v3_engineIDIsSet)
        /*
         * engineID has NOT been set via configuration file
         */
    {
        /*
         * See if already set if so erase & release it
         */
        MEMORY_FREE(_v3_engineIDNic);
        _v3_engineIDNic = (u_char *) malloc(strlen(cptr) + 1);
        if (NULL != _v3_engineIDNic) {
            strcpy((char *) _v3_engineIDNic, cptr);
            DEBUG_MSGTL(("snmpv3", "Initializing engineIDNic: %s\n",
                        _v3_engineIDNic));
        } else {
            DEBUG_MSGTL(("snmpv3",
                        "Error allocating memory for engineIDNic!\n"));
        }
    } else {
        DEBUG_MSGTL(("snmpv3",
                    "NOT setting engineIDNic, engineID already set\n"));
    }
}

/*******************************************************************-o-******
 * engineID_conf
 *
 * Parameters:
 *	*word
 *	*cptr
 *
 * This function reads a string from the configuration file and uses that
 * string to initialize the engineID.  It's assumed to be human readable.
 */
void V3_engineIDConf(const char *word, char *cptr)
{
    V3_setupEngineID(NULL, cptr);
    DEBUG_MSGTL(("snmpv3", "initialized engineID with: %s\n", cptr));
}

void V3_versionConf(const char *word, char *cptr)
{
    int valid = 0;
    if ((strcasecmp(cptr,  "3" ) == 0) ||
               (strcasecmp(cptr, "v3" ) == 0)) {
        DefaultStore_setInt(DsStorage_LIBRARY_ID, DsInt_SNMPVERSION,
               DEFAULTSTORE_PRIOT_VERSION_3);
        valid = 1;
    }
    if (!valid) {
        ReadConfig_configPerror("Unknown version specification");
        return;
    }
    DEBUG_MSGTL(("snmpv3", "set default version to %d\n",
                DefaultStore_getInt(DsStorage_LIBRARY_ID,
                   DsInt_SNMPVERSION)));
}

/*
 * oldengineID_conf(const char *, char *):
 *
 * Reads a octet string encoded engineID into the oldEngineID and
 * oldEngineIDLen pointers.
 */
void V3_oldengineIDConf(const char *word, char *cptr)
{
    ReadConfig_readOctetString(cptr, &_v3_oldEngineID, &_v3_oldEngineIDLength);
}

/*
 * exactEngineID_conf(const char *, char *):
 *
 * Reads a octet string encoded engineID into the engineID and
 * engineIDLen pointers.
 */
void V3_exactEngineIDConf(const char *word, char *cptr)
{
    ReadConfig_readOctetString(cptr, &_v3_engineID, &_v3_engineIDLength);
    if (_v3_engineIDLength > MAX_ENGINEID_LENGTH) {
    ReadConfig_error(
        "exactEngineID '%s' too long; truncating to %d bytes",
        cptr, MAX_ENGINEID_LENGTH);
        _v3_engineID[MAX_ENGINEID_LENGTH - 1] = '\0';
        _v3_engineIDLength = MAX_ENGINEID_LENGTH;
    }
    _v3_engineIDIsSet = 1;
    _v3_engineIDType = ENGINEID_TYPE_EXACT;
}


/*
 * merely call
 */
void V3_getEnginetimeAlarm(unsigned int regnum, void *clientargs)
{
    /* we do this every so (rarely) often just to make sure we watch
       wrapping of the times() output */
    V3_localEngineTime();
}

/*******************************************************************-o-******
 * init_snmpv3
 *
 * Parameters:
 *	*type	Label for the config file "type" used by calling entity.
 *
 * Set time and engineID.
 * Set parsing functions for config file tokens.
 * Initialize SNMP Crypto API (SCAPI).
 */
void V3_initV3(const char *type)
{
    Tools_getMonotonicClock(&_v3_starttime);

    if (!type)
        type = "__priotapp__";

    /*
     * we need to be called back later
     */
    Callback_registerCallback(CALLBACK_LIBRARY,
                             CALLBACK_POST_READ_CONFIG,
                           V3_initPostConfig, NULL);

    Callback_registerCallback(CALLBACK_LIBRARY,
                              CALLBACK_POST_PREMIB_READ_CONFIG,
                           V3_initPostPremibConfig, NULL);
    /*
     * we need to be called back later
     */
    Callback_registerCallback(CALLBACK_LIBRARY, CALLBACK_STORE_DATA,
                           V3_store, (void *) strdup(type));

    /*
     * initialize submodules
     */
    /*
     * NOTE: this must be after the callbacks are registered above,
     * since they need to be called before the USM callbacks.
     */
    Secmod_init();

    /*
     * register all our configuration handlers (ack, there's a lot)
     */

    /*
     * handle engineID setup before everything else which may depend on it
     */
    ReadConfig_registerPrenetMibHandler(type, "engineID", V3_engineIDConf, NULL,
                                    "string");
    ReadConfig_registerPrenetMibHandler(type, "oldEngineID", V3_oldengineIDConf,
                                    NULL, NULL);
    ReadConfig_registerPrenetMibHandler(type, "exactEngineID", V3_exactEngineIDConf,
                                    NULL, NULL);
    ReadConfig_registerPrenetMibHandler(type, "engineIDType",
                                    V3_engineIDTypeConf, NULL, "num");
    ReadConfig_registerPrenetMibHandler(type, "engineIDNic", V3_engineIDNicConf,
                                    NULL, "string");
    ReadConfig_registerConfigHandler(type, "engineBoots", V3_engineBootsConf, NULL,
                            NULL);

    /*
     * default store config entries
     */
    DefaultStore_registerConfig(ASN01_OCTET_STR, "priot", "defSecurityName",
                   DsStorage_LIBRARY_ID, DsStr_SECNAME);
    DefaultStore_registerConfig(ASN01_OCTET_STR, "priot", "defContext",
                   DsStorage_LIBRARY_ID, DsStr_CONTEXT);
    DefaultStore_registerConfig(ASN01_OCTET_STR, "priot", "defPassphrase",
                               DsStorage_LIBRARY_ID,
                               DsStr_PASSPHRASE);
    DefaultStore_registerConfig(ASN01_OCTET_STR, "priot", "defAuthPassphrase",
                               DsStorage_LIBRARY_ID,
                               DsStr_AUTHPASSPHRASE);
    DefaultStore_registerConfig(ASN01_OCTET_STR, "priot", "defPrivPassphrase",
                               DsStorage_LIBRARY_ID,
                               DsStr_PRIVPASSPHRASE);
    DefaultStore_registerConfig(ASN01_OCTET_STR, "priot", "defAuthMasterKey",
                               DsStorage_LIBRARY_ID,
                               DsStr_AUTHMASTERKEY);
    DefaultStore_registerConfig(ASN01_OCTET_STR, "priot", "defPrivMasterKey",
                               DsStorage_LIBRARY_ID,
                               DsStr_PRIVMASTERKEY);
    DefaultStore_registerConfig(ASN01_OCTET_STR, "priot", "defAuthLocalizedKey",
                               DsStorage_LIBRARY_ID,
                               DsStr_AUTHLOCALIZEDKEY);
    DefaultStore_registerConfig(ASN01_OCTET_STR, "priot", "defPrivLocalizedKey",
                               DsStorage_LIBRARY_ID,
                               DsStr_PRIVLOCALIZEDKEY);
    ReadConfig_registerConfigHandler("priot", "defVersion", V3_versionConf, NULL,
                            "1|2c|3");

    ReadConfig_registerConfigHandler("priot", "defSecurityLevel",
                            V3_secLevelConf, NULL,
                            "noAuthNoPriv|authNoPriv|authPriv");
}

/*
 * initializations for SNMPv3 to be called after the configuration files
 * have been read.
 */

int V3_initPostConfig(int majorid, int minorid, void *serverarg,
                        void *clientarg)
{

    size_t          engineIDLen;
    u_char         *c_engineID;

    c_engineID = V3_generateEngineID(&engineIDLen);

    if (engineIDLen == 0 || !c_engineID) {
        /*
         * Somethine went wrong - help!
         */
        MEMORY_FREE(c_engineID);
        return ErrorCode_GENERR;
    }

    /*
     * if our engineID has changed at all, the boots record must be set to 1
     */
    if (engineIDLen != _v3_oldEngineIDLength ||
        _v3_oldEngineID == NULL || c_engineID == NULL ||
        memcmp(_v3_oldEngineID, c_engineID, engineIDLen) != 0) {
        _v3_engineBoots = 1;
    }

    /*
     * for USM set our local engineTime in the LCD timing cache
     */
    LcdTime_setEnginetime(c_engineID, engineIDLen,
                   V3_localEngineBoots(),
                   V3_localEngineTime(), TRUE);

    MEMORY_FREE(c_engineID);
    return ErrorCode_SUCCESS;
}

int V3_initPostPremibConfig(int majorid, int minorid, void *serverarg,
                               void *clientarg)
{
    if (!_v3_engineIDIsSet)
        V3_setupEngineID(NULL, NULL);

    return ErrorCode_SUCCESS;
}

/*******************************************************************-o-******
 * store_snmpv3
 *
 * Parameters:
 *	*type
 */
int V3_store(int majorID, int minorID, void *serverarg, void *clientarg)
{
    char            line[UTILITIES_MAX_BUFFER_SMALL];
    u_char          c_engineID[UTILITIES_MAX_BUFFER_SMALL];
    int             engineIDLen;
    const char     *type = (const char *) clientarg;

    if (type == NULL)           /* should never happen, since the arg is ours */
        type = "unknown";

    sprintf(line, "engineBoots %ld", _v3_engineBoots);
    ReadConfig_store(type, line);

    engineIDLen = V3_getEngineID(c_engineID, UTILITIES_MAX_BUFFER_SMALL);

    if (engineIDLen) {
        /*
         * store the engineID used for this run
         */
        sprintf(line, "oldEngineID ");
        ReadConfig_saveOctetString(line + strlen(line), c_engineID,
                                      engineIDLen);
        ReadConfig_store(type, line);
    }
    return ErrorCode_SUCCESS;
}                               /* snmpv3_store() */

u_long
V3_localEngineBoots(void)
{
    return _v3_engineBoots;
}


/*******************************************************************-o-******
 * snmpv3_get_engineID
 *
 * Parameters:
 *	*buf
 *	 buflen
 *
 * Returns:
 *	Length of engineID	On Success
 *	ErrorCode_GENERR		Otherwise.
 *
 *
 * Store engineID in buf; return the length.
 *
 */
size_t
V3_getEngineID(u_char * buf, size_t buflen)
{
    /*
     * Sanity check.
     */
    if (!buf || (buflen < _v3_engineIDLength)) {
        return 0;
    }
    if (!_v3_engineID) {
        return 0;
    }

    memcpy(buf, _v3_engineID, _v3_engineIDLength);
    return _v3_engineIDLength;

}                               /* end snmpv3_get_engineID() */

/*******************************************************************-o-******
 * snmpv3_clone_engineID
 *
 * Parameters:
 *	**dest
 *       *dest_len
 *       src
 *	 srclen
 *
 * Returns:
 *	Length of engineID	On Success
 *	0		        Otherwise.
 *
 *
 * Clones engineID, creates memory
 *
 */
int
V3_cloneEngineID(u_char ** dest, size_t * destlen, u_char * src,
                      size_t srclen)
{
    if (!dest || !destlen)
        return 0;

    MEMORY_FREE(*dest);
    *destlen = 0;

    if (srclen && src) {
        *dest = (u_char *) malloc(srclen);
        if (*dest == NULL)
            return 0;
        memmove(*dest, src, srclen);
        *destlen = srclen;
    }
    return *destlen;
}                               /* end snmpv3_clone_engineID() */


/*******************************************************************-o-******
 * snmpv3_generate_engineID
 *
 * Parameters:
 *	*length
 *
 * Returns:
 *	Pointer to copy of engineID	On Success.
 *	NULL				If malloc() or snmpv3_get_engineID()
 *						fail.
 *
 * Generates a malloced copy of our engineID.
 *
 * 'length' is set to the length of engineID  -OR-  < 0 on failure.
 */
u_char *
V3_generateEngineID(size_t * length)
{
    u_char         *newID;
    newID = (u_char *) malloc(_v3_engineIDLength);

    if (newID) {
        *length = V3_getEngineID(newID, _v3_engineIDLength);
    }

    if (*length == 0) {
        MEMORY_FREE(newID);
        newID = NULL;
    }

    return newID;

}                               /* end snmpv3_generate_engineID() */

/**
 * Return the value of snmpEngineTime. According to RFC 3414 snmpEngineTime
 * is a 31-bit counter. engineBoots must be incremented every time that
 * counter wraps around.
 *
 * @see See also <a href="http://tools.ietf.org/html/rfc3414">RFC 3414</a>.
 *
 * @note It is assumed that this function is called at least once every
 *   2**31 seconds.
 */
u_long
V3_localEngineTime(void)
{


    static uint32_t last_engineTime;
    struct timeval  now;
    uint32_t engineTime;

    Tools_getMonotonicClock(&now);
    engineTime = System_calculateSectimeDiff(&now, &_v3_starttime) & 0x7fffffffL;
    if (engineTime < last_engineTime)
        _v3_engineBoots++;
    last_engineTime = engineTime;
    return engineTime;
}



/*
 * Code only for Linux systems
 */
static int
_V3_getHwAddress(const char *networkDevice, /* e.g. "eth0", "eth1" */
             char *addressOut)
{                               /* return address. Len=IFHWADDRLEN */
    /*
     * V3_getHwAddress(...)
     * *
     * *  This function will return a Network Interfaces Card's Hardware
     * *  address (aka MAC address).
     * *
     * *  Input Parameter(s):
     * *      networkDevice - a null terminated string with the name of a network
     * *                      device.  Examples: eth0, eth1, etc...
     * *
     * *  Output Parameter(s):
     * *      addressOut -    This is the binary value of the hardware address.
     * *                      This value is NOT converted into a hexadecimal string.
     * *                      The caller must pre-allocate for a return value of
     * *                      length IFHWADDRLEN
     * *
     * *  Return value:   This function will return zero (0) for success.  If
     * *                  an error occurred the function will return -1.
     * *
     * *  Caveats:    This has only been tested on Ethernet networking cards.
     */
    int             sock;       /* our socket */
    struct ifreq    request;    /* struct which will have HW address */

    if ((NULL == networkDevice) || (NULL == addressOut)) {
        return -1;
    }
    /*
     * In order to find out the hardware (MAC) address of our system under
     * * Linux we must do the following:
     * * 1.  Create a socket
     * * 2.  Do an ioctl(...) call with the SIOCGIFHWADDRLEN operation.
     */
    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        return -1;
    }
    /*
     * erase the request block
     */
    memset(&request, 0, sizeof(request));
    /*
     * copy the name of the net device we want to find the HW address for
     */
    String_copyTruncate(request.ifr_name, networkDevice, IFNAMSIZ);
    /*
     * Get the HW address
     */
    if (ioctl(sock, SIOCGIFHWADDR, &request)) {
        close(sock);
        return -1;
    }
    close(sock);
    memcpy(addressOut, request.ifr_hwaddr.sa_data, IFHWADDRLEN);
    return 0;
}


