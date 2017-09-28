#include "DsLib.h"

const struct DSLibBoolean_s DSLIB_BOOLEAN =  {
   .MIB_ERRORS              = 0,
   .SAVE_MIB_DESCRS         = 1,
   .MIB_COMMENT_TERM        = 2,
   .MIB_PARSE_LABEL         = 3,
   .DUMP_PACKET             = 4,
   .LOG_TIMESTAMP           = 5,
   .DONT_READ_CONFIGS       = 6,    /* don't read normal config files */
   .DISABLE_CONFIG_LOAD     = 6,    /* .DISABLE_CONFIG_LOAD = .DONT_READ_CONFIGS */
   .MIB_REPLACE             = 7,    /* replace objects from latest module */
   .PRINT_NUMERIC_ENUM      = 8,    /* print only numeric enum values */
   .PRINT_NUMERIC_OIDS      = 9,    /* print only numeric enum values */
   .DONT_BREAKDOWN_OIDS     = 10,   /* dont print oid indexes specially */
   .ALARM_DONT_USE_SIG      = 11,   /* don't use the alarm() signal */
   .PRINT_FULL_OID          = 12,   /* print fully qualified oids */
   .QUICK_PRINT             = 13,   /* print very brief output for parsing */
   .RANDOM_ACCESS	        = 14,   /* random access to oid labels */
   .REGEX_ACCESS	        = 15,   /* regex matching to oid labels */
   .DONT_CHECK_RANGE        = 16,   /* don't check values for ranges on send */
   .NO_TOKEN_WARNINGS       = 17,   /* no warn about unknown config tokens */
   .NUMERIC_TIMETICKS       = 18,   /* print timeticks as a number */
   .ESCAPE_QUOTES           = 19,   /* shell escape quote marks in oids */
   .REVERSE_ENCODE          = 20,   /* encode packets from back to front */
   .PRINT_BARE_VALUE	    = 21,   /* just print value (not OID = value) */
   .EXTENDED_INDEX	        = 22,   /* print extended index format [x1][x2] */
   .PRINT_HEX_TEXT          = 23,   /* print ASCII text along with hex strings */
   .PRINT_UCD_STYLE_OID     = 24,   /* print OID's using the UCD-style prefix suppression */
   .READ_UCD_STYLE_OID      = 25,   /* require top-level OIDs to be prefixed with a dot */
   .HAVE_READ_PREMIB_CONFIG = 26,   /* have the pre-mib parsing config tokens been processed */
   .HAVE_READ_CONFIG        = 27,   /* have the config tokens been processed */
   .QUICKE_PRINT            = 28,
   .DONT_PRINT_UNITS        = 29,   /* don't print UNITS suffix */
   .NO_DISPLAY_HINT         = 30,   /* don't apply DISPLAY-HINTs */
   .LIB_16BIT_IDS           = 31,   /* restrict requestIDs, etc to 16-bit values */
   .DONT_PERSIST_STATE      = 32,   /* don't load config and don't load/save persistent file */
   .LIB_2DIGIT_HEX_OUTPUT   = 33,   /* print a leading 0 on hex values <= 'f' */
   .IGNORE_NO_COMMUNITY     = 34,	/* don't complain if no community is specified in the command arguments */
   .DISABLE_PERSISTENT_LOAD = 35,   /* don't load persistent file */
   .DISABLE_PERSISTENT_SAVE = 36,   /* don't save persistent file */
   .APPEND_LOGFILES         = 37,   /* append, don't overwrite, log files */
   .NO_DISCOVERY            = 38,   /* don't support RFC5343 contextEngineID discovery */
   .TSM_USE_PREFIX          = 39,   /* TSM's simple security name mapping */
   .DONT_LOAD_HOST_FILES    = 40,   /* don't read host.conf files */
   .DNSSEC_WARN_ONLY        = 41,   /* tread DNSSEC errors as warnings */
   .MAX_BOOL_ID             = 48    /* match NETSNMP_DS_MAX_SUBIDS */

};

const struct DSLibInteger_s DSLIB_INTEGER = {
   .MIB_WARNINGS         = 0,
   .SECLEVEL             = 1,
   .SNMPVERSION          = 2,
   .DEFAULT_PORT         = 3,
   .OID_OUTPUT_FORMAT    = 4,
   .PRINT_SUFFIX_ONLY    = 4,  /* PRINT_SUFFIX_ONLY = OID_OUTPUT_FORMAT */
   .STRING_OUTPUT_FORMAT = 5,
   .HEX_OUTPUT_LENGTH    = 6,
   .SERVERSENDBUF        = 7,  /* send buffer (server) */
   .SERVERRECVBUF        = 8,  /* receive buffer (server) */
   .CLIENTSENDBUF        = 9,  /* send buffer (client) */
   .CLIENTRECVBUF        = 10, /* receive buffer (client) */
   .SSHDOMAIN_SOCK_PERM  = 11,
   .SSHDOMAIN_DIR_PERM   = 12,
   .SSHDOMAIN_SOCK_USER  = 12,
   .SSHDOMAIN_SOCK_GROUP = 13,
   .TIMEOUT              = 14,
   .RETRIES              = 15,
   .MAX_INT_ID           = 48  /* match MAX_SUBIDS */
};


const struct DSLibString_s  DSLIB_STRING = {
    .SECNAME           = 0,
    .CONTEXT           = 1,
    .PASSPHRASE        = 2,
    .AUTHPASSPHRASE    = 3,
    .PRIVPASSPHRASE    = 4,
    .OPTIONALCONFIG    = 5,
    .APPTYPE           = 6,
    .COMMUNITY         = 7,
    .PERSISTENT_DIR    = 8,
    .CONFIGURATION_DIR = 9,
    .SECMODEL          = 10,
    .MIBDIRS           = 11,
    .OIDSUFFIX         = 12,
    .OIDPREFIX         = 13,
    .CLIENT_ADDR       = 14,
    .TEMP_FILE_PATTERN = 15,
    .AUTHMASTERKEY     = 16,
    .PRIVMASTERKEY     = 17,
    .AUTHLOCALIZEDKEY  = 18,
    .PRIVLOCALIZEDKEY  = 19,
    .APPTYPES          = 20,
    .KSM_KEYTAB        = 21,
    .KSM_SERVICE_NAME  = 22,
    .X509_CLIENT_PUB   = 23,
    .X509_SERVER_PUB   = 24,
    .SSHTOSNMP_SOCKET  = 25,
    .CERT_EXTRA_SUBDIR = 26,
    .HOSTNAME          = 27,
    .X509_CRL_FILE     = 28,
    .TLS_ALGORITMS     = 29,
    .TLS_LOCAL_CERT    = 30,
    .TLS_PEER_CERT     = 31,
    .SSH_USERNAME      = 32,
    .SSH_PUBKEY        = 33,
    .SSH_PRIVKEY       = 34,
    .MAX_STR_ID        = 48   /* match NETSNMP_DS_MAX_SUBIDS */
};
