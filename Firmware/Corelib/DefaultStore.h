#ifndef DEFAULT_STORE_H
#define DEFAULT_STORE_H

#include "Generals.h"

#include "DataType.h"


/**  The purpose of the default storage is three-fold:

       1)     To create a global storage space without creating a
              whole  bunch  of globally accessible variables or a
              whole bunch of access functions to work  with  more
              privately restricted variables.

       2)     To provide a single location where the thread lock-
              ing needs to be implemented. At the  time  of  this
              writing,  however,  thread  locking  is  not yet in
              place.

       3)     To reduce the number of cross dependencies  between
              code  pieces that may or may not be linked together
              in the long run. This provides for a  single  loca-
              tion  in which configuration data, for example, can
              be stored for a separate section of code  that  may
              not be linked in to the application in question.

       The functions defined here implement these goals.

       Currently, three data types are supported: booleans, _i32e-
       gers, and strings. Each of these data types have  separate
       storage  spaces.  In  addition, the storage space for each
       data type is divided further  by  the  application  level.
       Currently,  there  are  two  storage  spaces. The first is
       reserved for  the  PRIOT library  itself.  The  second  is
       _i32ended  for  use  in applications and is not modified or
       checked by the library, and, therefore, this is the  space
       usable by you.
 */


#define DEFAULTSTORE_MAX_IDS 3
#define DEFAULTSTORE_MAX_SUBIDS 48        /* needs to be a multiple of 8 */

/*
 * These definitions correspond with the "which" argument to the API,
 * when the storeid argument is DS_LIBRARY_ID
 */

//Storage
struct DEFAULTSTORE_STORAGE {
    enum {
        LIBRARY_ID,
        APPLICATION_ID,
        TOKEN_ID
    };
};

/*
 * library booleans
 */
struct DEFAULTSTORE_LIB_BOOLEAN {
    enum {
        MIB_ERRORS = 0,
        SAVE_MIB_DESCRS,
        MIB_COMMENT_TERM,
        MIB_PARSE_LABEL,
        DUMP_PACKET,
        LOG_TIMESTAMP,
        DONT_READ_CONFIGS,     /* don't read normal config files */
        DISABLE_CONFIG_LOAD = DONT_READ_CONFIGS,
        MIB_REPLACE = 7,       /* replace objects from latest module */
        PRINT_NUMERIC_ENUM,    /* print only numeric enum values */
        PRINT_NUMERIC_OIDS,    /* print only numeric enum values */
        DONT_BREAKDOWN_OIDS,   /* dont print oid indexes specially */
        ALARM_DONT_USE_SIG,    /* don't use the alarm() signal */
        PRINT_FULL_OID,        /* print fully qualified oids */
        QUICK_PRINT,           /* print very brief output for parsing */
        RANDOM_ACCESS,	       /* random access to oid labels */
        REGEX_ACCESS,	       /* regex matching to oid labels */
        DONT_CHECK_RANGE,      /* don't check values for ranges on send */
        NO_TOKEN_WARNINGS,     /* no warn about unknown config tokens */
        NUMERIC_TIMETICKS,     /* print timeticks as a number */
        ESCAPE_QUOTES,         /* shell escape quote marks in oids */
        REVERSE_ENCODE,        /* encode packets from back to front */
        PRINT_BARE_VALUE,      /* just print value (not OID = value) */
        EXTENDED_INDEX,        /* print extended index format [x1][x2] */
        PRINT_HEX_TEXT,        /* print ASCII text along with hex strings */
        PRINT_UCD_STYLE_OID,   /* print OID's using the UCD-style prefix suppression */
        READ_UCD_STYLE_OID,    /* require top-level OIDs to be prefixed with a dot */
        HAVE_READ_PREMIB_CONFIG, /* have the pre-mib parsing config tokens been processed */
        HAVE_READ_CONFIG,      /* have the config tokens been processed */
        QUICKE_PRINT,
        DONT_PRINT_UNITS,      /* don't print UNITS suffix */
        NO_DISPLAY_HINT,       /* don't apply DISPLAY-HINTs */
        LIB16BIT_IDS,          /* restrict requestIDs, etc to 16-bit values */
        DONT_PERSIST_STATE,    /* don't load config and don't load/save persistent file */
        TWODIGIT_HEX_OUTPUT, /* print a leading 0 on hex values <= 'f' */
        IGNORE_NO_COMMUNITY,   /* don't complain if no community is specified in the command arguments */
        DISABLE_PERSISTENT_LOAD, /* don't load persistent file */
        DISABLE_PERSISTENT_SAVE, /* don't save persistent file */
        APPEND_LOGFILES, /* append, don't overwrite, log files */
        NO_DISCOVERY,    /* don't support RFC5343 contextEngineID discovery */
        TSM_USE_PREFIX,  /* TSM's simple security name mapping */
        DONT_LOAD_HOST_FILES, /* don't read host.conf files */
        DNSSEC_WARN_ONLY,     /* tread DNSSEC errors as warnings */
        MAX_BOOL_ID           /* match DS_MAX_SUBIDS */
    };
};


/*
 * library Integers
 */
struct DEFAULTSTORE_LIB_INTEGER {
    enum {
        MIB_WARNINGS,
        SECLEVEL,
        SNMPVERSION,
        DEFAULT_PORT,
        OID_OUTPUT_FORMAT,
        PRINT_SUFFIX_ONLY = OID_OUTPUT_FORMAT,
        STRING_OUTPUT_FORMAT,
        HEX_OUTPUT_LENGTH,
        SERVERSENDBUF,   /* send buffer (server) */
        SERVERRECVBUF,   /* receive buffer (server) */
        CLIENTSENDBUF,   /* send buffer (client) */
        CLIENTRECVBUF,   /* receive buffer (client) */
        SSHDOMAIN_SOCK_PERM,
        SSHDOMAIN_DIR_PERM,
        SSHDOMAIN_SOCK_USER,
        SSHDOMAIN_SOCK_GROUP,
        TIMEOUT,
        RETRIES,
        MAX_INT_ID
    };
};


/*
 * library strings
 */
struct DEFAULTSTORE_LIB_STRING {
    enum {
        SECNAME,
        CONTEXT,
        PASSPHRASE,
        AUTHPASSPHRASE,
        PRIVPASSPHRASE,
        OPTIONALCONFIG,
        APPTYPE,
        COMMUNITY,
        PERSISTENT_DIR,
        CONFIGURATION_DIR,
        SECMODEL,
        MIBDIRS,
        OIDSUFFIX,
        OIDPREFIX,
        CLIENT_ADDR,
        TEMP_FILE_PATTERN,
        AUTHMASTERKEY,
        PRIVMASTERKEY,
        AUTHLOCALIZEDKEY,
        PRIVLOCALIZEDKEY,
        APPTYPES,
        KSM_KEYTAB,
        KSM_SERVICE_NAME,
        X509_CLIENT_PUB,
        X509_SERVER_PUB,
        SSHTOSNMP_SOCKET,
        CERT_EXTRA_SUBDIR,
        HOSTNAME,
        X509_CRL_FILE,
        TLS_ALGORITMS,
        TLS_LOCAL_CERT,
        TLS_PEER_CERT,
        SSH_USERNAME,
        SSH_PUBKEY,
        SSH_PRIVKEY,
        MAX_STR_ID /* match NETSNMP_DS_MAX_SUBIDS */
    };
};

#define DEFAULTSTORE_PRIOT_VERSION_1    128        /* bogus */
#define DEFAULTSTORE_PRIOT_VERSION_2c   1  /* real */
#define DEFAULTSTORE_PRIOT_VERSION_3    3  /* real */

_i32        DefaultStore_setBoolean(_i32 storeid, _i32 which, _i32 value);
_i32        DefaultStore_toggleBoolean(_i32 storeid, _i32 which);
_i32        DefaultStore_getBoolean(_i32 storeid, _i32 which);
_i32        DefaultStore_setInt(_i32 storeid, _i32 which, _i32 value);
_i32        DefaultStore_getInt(_i32 storeid, _i32 which);
_i32        DefaultStore_setString(_i32 storeid, _i32 which,  const _s8 *value);
_s8  *      DefaultStore_getString(_i32 storeid, _i32 which);
_i32        DefaultStore_setVoid(_i32 storeid, _i32 which, void *value);
void *      DefaultStore_getVoid(_i32 storeid, _i32 which);
_i32        DefaultStore_parseBoolean(_s8 * line);
void        DefaultStore_handleConfig(const _s8 * token, _s8 * line);
_i32        DefaultStore_registerConfig(_u8 type, const char * storename,  const char * token,  _i32 storeid, _i32 which);
_i32        DefaultStore_registerPremib(_u8 type, const char * storename,  const char * token, _i32 storeid, _i32 which);
void        DefaultStore_shutdown(void);



#endif // DEFAULT_STORE_H
