#ifndef DEFAULT_STORE_H
#define DEFAULT_STORE_H

#include "DataType.h"
#include "Generals.h"

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
#define DEFAULTSTORE_MAX_SUBIDS 48 /* needs to be a multiple of 8 */

#define DEFAULTSTORE_PRIOT_VERSION_3 3 /* real */

/*
 * These definitions correspond with the "which" argument to the API,
 * when the storeid argument is DS_LIBRARY_ID
 */

//Storage
enum DsStorage_e {
    DsStorage_LIBRARY_ID,
    DsStorage_APPLICATION_ID,
    DsStorage_TOKEN_ID
};

enum DsLibBoolean_e {
    DsBool_MIB_ERRORS,
    DsBool_SAVE_MIB_DESCRS,
    DsBool_MIB_COMMENT_TERM,
    DsBool_MIB_PARSE_LABEL,
    DsBool_DUMP_PACKET,
    DsBool_LOG_TIMESTAMP,
    DsBool_DONT_READ_CONFIGS, /* don't read normal config files */
    DsBool_DISABLE_CONFIG_LOAD = DsBool_DONT_READ_CONFIGS,
    DsBool_MIB_REPLACE, /* replace objects from latest module */
    DsBool_PRINT_NUMERIC_ENUM, /* print only numeric enum values */
    DsBool_PRINT_NUMERIC_OIDS, /* print only numeric enum values */
    DsBool_DONT_BREAKDOWN_OIDS, /* dont print oid indexes specially */
    DsBool_ALARM_DONT_USE_SIG, /* don't use the alarm() signal */
    DsBool_PRINT_FULL_OID, /* print fully qualified oids */
    DsBool_QUICK_PRINT, /* print very brief output for parsing */
    DsBool_RANDOM_ACCESS, /* random access to oid labels */
    DsBool_REGEX_ACCESS, /* regex matching to oid labels */
    DsBool_DONT_CHECK_RANGE, /* don't check values for ranges on send */
    DsBool_NO_TOKEN_WARNINGS, /* no warn about unknown config tokens */
    DsBool_NUMERIC_TIMETICKS, /* print timeticks as a number */
    DsBool_ESCAPE_QUOTES, /* shell escape quote marks in oids */
    DsBool_REVERSE_ENCODE, /* encode packets from back to front */
    DsBool_PRINT_BARE_VALUE, /* just print value (not OID lue) */
    DsBool_EXTENDED_INDEX, /* print extended index format [x1][x2] */
    DsBool_PRINT_HEX_TEXT, /* print ASCII text along with hex strings */
    DsBool_PRINT_UCD_STYLE_OID, /* print OID's using the UCD-style prefix suppression */
    DsBool_READ_UCD_STYLE_OID, /* require top-level OIDs to be prefixed with a dot */
    DsBool_HAVE_READ_PREMIB_CONFIG, /* have the pre-mib parsing config tokens been processed */
    DsBool_HAVE_READ_CONFIG, /* have the config tokens been processed */
    DsBool_QUICKE_PRINT,
    DsBool_DONT_PRINT_UNITS, /* don't print UNITS suffix */
    DsBool_NO_DISPLAY_HINT, /* don't apply DISPLAY-HINTs */
    DsBool_LIB_16BIT_IDS, /* restrict requestIDs, etc to 16-bit values */
    DsBool_DONT_PERSIST_STATE, /* don't load config and don't load/save persistent file */
    DsBool_LIB_2DIGIT_HEX_OUTPUT, /* print a leading 0 on hex values <' */
    DsBool_IGNORE_NO_COMMUNITY, /* don't complain if no community is specified in the command arguments */
    DsBool_DISABLE_PERSISTENT_LOAD, /* don't load persistent file */
    DsBool_DISABLE_PERSISTENT_SAVE, /* don't save persistent file */
    DsBool_APPEND_LOGFILES, /* append, don't overwrite, log files */
    DsBool_NO_DISCOVERY, /* don't support RFC5343 contextEngineID discovery */
    DsBool_TSM_USE_PREFIX, /* TSM's simple security name mapping */
    DsBool_DONT_LOAD_HOST_FILES, /* don't read host.conf files */
    DsBool_DNSSEC_WARN_ONLY, /* tread DNSSEC errors as warnings */
    DsBool_MAX_BOOL_ID = 48 /* match NETSNMP_DS_MAX_SUBIDS */

};

enum DsLibInteger_e {
    DsInt_MIB_WARNINGS,
    DsInt_SECLEVEL,
    DsInt_SNMPVERSION,
    DsInt_DEFAULT_PORT,
    DsInt_OID_OUTPUT_FORMAT,
    DsInt_PRINT_SUFFIX_ONLY = DsInt_OID_OUTPUT_FORMAT,
    DsInt_STRING_OUTPUT_FORMAT,
    DsInt_HEX_OUTPUT_LENGTH,
    DsInt_SERVERSENDBUF, /* send buffer (server) */
    DsInt_SERVERRECVBUF, /* receive buffer (server) */
    DsInt_CLIENTSENDBUF, /* send buffer (client) */
    DsInt_CLIENTRECVBUF, /* receive buffer (client) */
    DsInt_SSHDOMAIN_SOCK_PERM,
    DsInt_SSHDOMAIN_DIR_PERM = 12,
    DsInt_SSHDOMAIN_SOCK_USER = 12,
    DsInt_SSHDOMAIN_SOCK_GROUP,
    DsInt_TIMEOUT,
    DsInt_RETRIES,
    DsInt_MAX_INT_ID = 48 /* match MAX_SUBIDS */
};

enum DsLibString_e {
    DsStr_SECNAME,
    DsStr_CONTEXT,
    DsStr_PASSPHRASE,
    DsStr_AUTHPASSPHRASE,
    DsStr_PRIVPASSPHRASE,
    DsStr_OPTIONALCONFIG,
    DsStr_APPTYPE,
    DsStr_COMMUNITY,
    DsStr_PERSISTENT_DIR,
    DsStr_CONFIGURATION_DIR,
    DsStr_SECMODEL,
    DsStr_MIBDIRS,
    DsStr_OIDSUFFIX,
    DsStr_OIDPREFIX,
    DsStr_CLIENT_ADDR,
    DsStr_TEMP_FILE_PATTERN,
    DsStr_AUTHMASTERKEY,
    DsStr_PRIVMASTERKEY,
    DsStr_AUTHLOCALIZEDKEY,
    DsStr_PRIVLOCALIZEDKEY,
    DsStr_APPTYPES,
    DsStr_KSM_KEYTAB,
    DsStr_KSM_SERVICE_NAME,
    DsStr_X509_CLIENT_PUB,
    DsStr_X509_SERVER_PUB,
    DsStr_SSHTOSNMP_SOCKET,
    DsStr_CERT_EXTRA_SUBDIR,
    DsStr_HOSTNAME,
    DsStr_X509_CRL_FILE,
    DsStr_TLS_ALGORITMS,
    DsStr_TLS_LOCAL_CERT,
    DsStr_TLS_PEER_CERT,
    DsStr_SSH_USERNAME,
    DsStr_SSH_PUBKEY,
    DsStr_SSH_PRIVKEY,
    DsStr_MAX_STR_ID = 48 /* match NETSNMP_DS_MAX_SUBIDS */
};

int DefaultStore_setBoolean( int storeid, int which, int value );
int DefaultStore_toggleBoolean( int storeid, int which );
int DefaultStore_getBoolean( int storeid, int which );
int DefaultStore_setInt( int storeid, int which, int value );
int DefaultStore_getInt( int storeid, int which );
int DefaultStore_setString( int storeid, int which, const char* value );
char* DefaultStore_getString( int storeid, int which );
int DefaultStore_setVoid( int storeid, int which, void* value );
void* DefaultStore_getVoid( int storeid, int which );
int DefaultStore_parseBoolean( char* line );
void DefaultStore_handleConfig( const char* token, char* line );
int DefaultStore_registerConfig( unsigned char type, const char* storename, const char* token, int storeid, int which );
int DefaultStore_registerPremib( unsigned char type, const char* storename, const char* token, int storeid, int which );
void DefaultStore_shutdown( void );

#endif // DEFAULT_STORE_H
