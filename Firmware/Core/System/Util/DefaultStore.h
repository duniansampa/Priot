#ifndef IOT_DEFAULTSTORE_H
#define IOT_DEFAULTSTORE_H

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

       Currently, three data types are supported: booleans, inte-
       gers, and strings. Each of these data types have  separate
       storage  spaces.  In  addition, the storage space for each
       data type is divided further  by  the  application  level.
       Currently,  there  are  two  storage  spaces. The first is
       reserved for  the  PRIOT library  itself.  The  second  is
       intended  for  use  in applications and is not modified or
       checked by the library, and, therefore, this is the  space
       usable by you.
 */

/** ============================[ Macros ============================ */

#define DEFAULTSTORE_MAX_IDS 3
#define DEFAULTSTORE_MAX_SUBIDS 48 /* needs to be a multiple of 8 */

#define DEFAULTSTORE_PRIOT_VERSION_3 3 /* real */

/** ============================[ Types ]================== */

/**
 * @brief The DsStore_e enum.
 *        The storage.
 */
enum DsStore_e {
    DsStore_LIBRARY_ID,
    DsStore_APPLICATION_ID,
    DsStore_USER_ID
};

/**
 * @brief The DsLibBoolean_e enum
 * @note These definitions correspond with the "registryId" argument to the API,
 *       when the storeId argument is DsStore_LIBRARY_ID
 */
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
    DsBool_ALARM_DONT_USE_SIGNAL, /* don't use the alarm() signal */
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
    DsBool_MAX_BOOL_ID = 48 /* match DEFAULTSTORE_MAX_SUBIDS */

};

/**
 * @brief The DsLibInteger_e enum
 * @note These definitions correspond with the "registryId" argument to the API,
 *       when the storeId argument is DsStore_LIBRARY_ID
 */
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
    DsInt_MAX_INT_ID = 48 /* match DEFAULTSTORE_MAX_SUBIDS */
};

/**
 * @brief The DsLibString_e enum
 * @note These definitions correspond with the "registryId" argument to the API,
 *       when the storeId argument is DsStore_LIBRARY_ID
 */
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
    DsStr_MAX_STR_ID = 48 /* match DEFAULTSTORE_MAX_SUBIDS */
};

/** =============================[ Functions Prototypes ]================== */

/**
 * @brief DefaultStore_setBoolean
 *        stores a boolean value
 *
 * @param storeId - an index to storage container's first index(storeId)
 * @param registryId - an index to storage container's second index(registryId)
 * @param value - the value to be stored
 *
 * @retval ErrorCode_GENERR : in case of error.
 * @retval ErrorCode_SUCCESS : on success.
 */
int DefaultStore_setBoolean( int storeId, int registryId, int value );

/**
 * @brief DefaultStore_toggleBoolean
 *        toggles a boolean value
 *
 * @param storeId - an index to storage container's first index(storeId)
 * @param registryId - an index to storage container's second index(registryId)
 *
 * @retval ErrorCode_GENERR : in case of error.
 * @retval ErrorCode_SUCCESS : on success.
 */
int DefaultStore_toggleBoolean( int storeId, int registryId );

/**
 * @brief DefaultStore_getBoolean
 *        returns a boolean value
 *
 * @param storeId - an index to storage container's first index(storeId)
 * @param registryId - an index to storage container's second index(registryId)
 *
 * @retval ErrorCode_GENERR : in case of error.
 * @retval record value : on success.
 */
int DefaultStore_getBoolean( int storeId, int registryId );

/**
 * @brief DefaultStore_setInt
 *        stores a int value
 *
 * @param storeId - an index to storage container's first index(storeId)
 * @param registryId - an index to storage container's second index(registryId)
 * @param value - the value to be stored
 *
 * @retval ErrorCode_GENERR : in case of error.
 * @retval ErrorCode_SUCCESS : on success.
 */
int DefaultStore_setInt( int storeId, int registryId, int value );

/**
 * @brief DefaultStore_getInt
 *        returns a int value
 *
 * @param storeId - an index to storage container's first index(storeId)
 * @param registryId - an index to storage container's second index(registryId)
 *
 * @retval ErrorCode_GENERR : in case of error.
 * @retval record value : on success.
 */
int DefaultStore_getInt( int storeId, int registryId );

/**
 * @brief DefaultStore_setString
 *        stores a string
 *
 * @param storeId - an index to storage container's first index(storeId)
 * @param registryId - an index to storage container's second index(registryId)
 * @param value - the value to be stored
 *
 * @retval ErrorCode_GENERR : in case of error.
 * @retval ErrorCode_SUCCESS : on success.
 */
int DefaultStore_setString( int storeId, int registryId, const char* value );

/**
 * @brief DefaultStore_getString
 *        returns a string
 *
 * @param storeId - an index to storage container's first index(storeId)
 * @param registryId - an index to storage container's second index(registryId)
 *
 * @retval ErrorCode_GENERR : in case of error.
 * @retval ErrorCode_SUCCESS : on success.
 */
char* DefaultStore_getString( int storeId, int registryId );

/**
 * @brief DefaultStore_setVoid
 *        stores a void value
 *
 * @param storeId - an index to storage container's first index(storeId)
 * @param registryId - an index to storage container's second index(registryId)
 * @param value - the value to be stored
 *
 * @retval ErrorCode_GENERR : in case of error.
 * @retval ErrorCode_SUCCESS : on success.
 */
int DefaultStore_setVoid( int storeId, int registryId, void* value );

/**
 * @brief DefaultStore_getVoid
 *        returns a void value
 *
 * @param storeId - an index to storage container's first index(storeId)
 * @param registryId - an index to storage container's second index(registryId)
 *
 * @retval ErrorCode_GENERR : in case of error.
 * @retval ErrorCode_SUCCESS : on success.
 */
void* DefaultStore_getVoid( int storeId, int registryId );

/**
 * @brief DefaultStore_clear
 *        Removes all records from the list and deallocates the memory allocated.
 */
void DefaultStore_clear( void );

/**
 * @brief DefaultStore_registerConfig
 *        registers handlers for certain tokens specified in certain types of files.
 *        After parsing the token, the result must be stored in the register
 *        (storeId, registryId) of the storage container of type asnType.
 *
 * @param asnType - content type of token.
 * @param storeName - the configuration file used, e.g., if priot.conf is the
 *                    file where the token is located use "priot" here.
 *                    Multiple colon separated tokens might be used.
 *                    If NULL or "" then the configuration file used will be
 *                    \<application\>.conf.
 *
 * @param token - the token being parsed from the file.  Must be non-NULL.
 * @param storeId - an index to storage container's first index(storeId)
 * @param registryId - an index to storage container's second index(registryId)
 *
 * @retval ErrorCode_GENERR : in case of error.
 * @retval ErrorCode_SUCCESS : on success.
 */
int DefaultStore_registerConfig( unsigned char asnType, const char* storeName, const char* token, int storeId, int registryId );

/** @brief DefaultStore_registerPremib
 *         is a wrapper of the DefaultStore_registerConfig function.
 *         Does the same thing that the DefaultStore_registerConfig function does.
 *
 * @see DefaultStore_registerConfig
 */
int DefaultStore_registerPremib( unsigned char asnType, const char* storeName, const char* token, int storeId, int registryId );

#endif // IOT_DEFAULTSTORE_H
