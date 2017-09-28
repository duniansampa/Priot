#ifndef DSLIB_H
#define DSLIB_H


/*
 * These definitions correspond with the "which" argument to the API,
 * when the storeid argument is NETSNMP_DS_LIBRARY_ID
 */
/*
 * library booleans
 */
struct DSLibBoolean_s{

    int MIB_ERRORS;
    int SAVE_MIB_DESCRS;
    int MIB_COMMENT_TERM;
    int MIB_PARSE_LABEL;
    int DUMP_PACKET;
    int LOG_TIMESTAMP;
    int DONT_READ_CONFIGS;       /* don't read normal config files */
    int DISABLE_CONFIG_LOAD;     /* DISABLE_CONFIG_LOAD = NETSNMP_DS_LIB_DONT_READ_CONFIGS */
    int MIB_REPLACE;             /* replace objects from latest module */
    int PRINT_NUMERIC_ENUM;      /* print only numeric enum values */
    int PRINT_NUMERIC_OIDS;      /* print only numeric enum values */
    int DONT_BREAKDOWN_OIDS;     /* dont print oid indexes specially */
    int ALARM_DONT_USE_SIG;      /* don't use the alarm() signal */
    int PRINT_FULL_OID;          /* print fully qualified oids */
    int QUICK_PRINT;             /* print very brief output for parsing */
    int RANDOM_ACCESS;           /* random access to oid labels */
    int REGEX_ACCESS;            /* regex matching to oid labels */
    int DONT_CHECK_RANGE;        /* don't check values for ranges on send */
    int NO_TOKEN_WARNINGS;       /* no warn about unknown config tokens */
    int NUMERIC_TIMETICKS;       /* print timeticks as a number */
    int ESCAPE_QUOTES;           /* shell escape quote marks in oids */
    int REVERSE_ENCODE;          /* encode packets from back to front */
    int PRINT_BARE_VALUE;        /* just print value (not OID = value) */
    int EXTENDED_INDEX;          /* print extended index format [x1][x2] */
    int PRINT_HEX_TEXT;          /* print ASCII text along with hex strings */
    int PRINT_UCD_STYLE_OID;     /* print OID's using the UCD-style prefix suppression */
    int READ_UCD_STYLE_OID;      /* require top-level OIDs to be prefixed with a dot */
    int HAVE_READ_PREMIB_CONFIG; /* have the pre-mib parsing config tokens been processed */
    int HAVE_READ_CONFIG;        /* have the config tokens been processed */
    int QUICKE_PRINT;
    int DONT_PRINT_UNITS;        /* don't print UNITS suffix */
    int NO_DISPLAY_HINT;         /* don't apply DISPLAY-HINTs */
    int LIB_16BIT_IDS;           /* restrict requestIDs, etc to 16-bit values */
    int DONT_PERSIST_STATE;      /* don't load config and don't load/save persistent file */
    int LIB_2DIGIT_HEX_OUTPUT;   /* print a leading 0 on hex values <= 'f' */
    int IGNORE_NO_COMMUNITY;     /* don't complain if no community is specified in the command arguments */
    int DISABLE_PERSISTENT_LOAD; /* don't load persistent file */
    int DISABLE_PERSISTENT_SAVE; /* don't save persistent file */
    int APPEND_LOGFILES;         /* append, don't overwrite, log files */
    int NO_DISCOVERY;            /* don't support RFC5343 contextEngineID discovery */
    int TSM_USE_PREFIX;          /* TSM's simple security name mapping */
    int DONT_LOAD_HOST_FILES;    /* don't read host.conf files */
    int DNSSEC_WARN_ONLY;        /* tread DNSSEC errors as warnings */
    int MAX_BOOL_ID;             /* match NETSNMP_DS_MAX_SUBIDS */

};

/*
 * library integers
 */
struct DSLibInteger_s{

    int MIB_WARNINGS;
    int SECLEVEL;
    int SNMPVERSION;
    int DEFAULT_PORT;
    int OID_OUTPUT_FORMAT;
    int PRINT_SUFFIX_ONLY;    /* PRINT_SUFFIX_ONLY = NETSNMP_DS_LIB_OID_OUTPUT_FORMAT */
    int STRING_OUTPUT_FORMAT;
    int HEX_OUTPUT_LENGTH;
    int SERVERSENDBUF;        /* send buffer (server) */
    int SERVERRECVBUF;        /* receive buffer (server) */
    int CLIENTSENDBUF;        /* send buffer (client) */
    int CLIENTRECVBUF;        /* receive buffer (client) */
    int SSHDOMAIN_SOCK_PERM;
    int SSHDOMAIN_DIR_PERM;
    int SSHDOMAIN_SOCK_USER;
    int SSHDOMAIN_SOCK_GROUP;
    int TIMEOUT;
    int RETRIES;
    int MAX_INT_ID;

};


/*
 * library strings
 */
struct DSLibString_s{

    int SECNAME;
    int CONTEXT;
    int PASSPHRASE;
    int AUTHPASSPHRASE;
    int PRIVPASSPHRASE;
    int OPTIONALCONFIG;
    int APPTYPE;
    int COMMUNITY;
    int PERSISTENT_DIR;
    int CONFIGURATION_DIR;
    int SECMODEL;
    int MIBDIRS;
    int OIDSUFFIX;
    int OIDPREFIX;
    int CLIENT_ADDR;
    int TEMP_FILE_PATTERN;
    int AUTHMASTERKEY;
    int PRIVMASTERKEY;
    int AUTHLOCALIZEDKEY;
    int PRIVLOCALIZEDKEY;
    int APPTYPES;
    int KSM_KEYTAB;
    int KSM_SERVICE_NAME;
    int X509_CLIENT_PUB;
    int X509_SERVER_PUB;
    int SSHTOSNMP_SOCKET;
    int CERT_EXTRA_SUBDIR;
    int HOSTNAME;
    int X509_CRL_FILE;
    int TLS_ALGORITMS;
    int TLS_LOCAL_CERT;
    int TLS_PEER_CERT;
    int SSH_USERNAME;
    int SSH_PUBKEY;
    int SSH_PRIVKEY;
    int MAX_STR_ID;    /* match NETSNMP_DS_MAX_SUBIDS */

};

extern const struct DSLibBoolean_s DSLIB_BOOLEAN;
extern const struct DSLibInteger_s DSLIB_INTEGER;
extern const struct DSLibString_s  DSLIB_STRING;


#endif // DSLIB_H
