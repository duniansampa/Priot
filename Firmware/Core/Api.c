#include "Api.h"
#include "Agentx/Protocol.h"
#include "Alarm.h"
#include "Auth.h"
#include "Callback.h"
#include "Callback.h"
#include "Client.h"
#include "DefaultStore.h"
#include "Impl.h"
#include "Int64.h"
#include "LargeFdSet.h"
#include "Mib.h"
#include "Parse.h"
#include "Priot.h"
#include "ReadConfig.h"
#include "Secmod.h"
#include "Service.h"
#include "Session.h"
#include "System/Containers/Container.h"
#include "System/Containers/MapList.h"
#include "System/String.h"
#include "System/Task/Mutex.h"
#include "System/Util/Assert.h"
#include "System/Util/Debug.h"
#include "System/Util/Logger.h"
#include "Usm.h"
#include "V3.h"
#include "Vacm.h"

#include <arpa/inet.h>
#include <locale.h>
#include <sys/time.h>

/*
 * Internal information about the state of the snmp session.
 */
struct Api_InternalSession_s {
    Api_RequestList* requests; /* Info about outstanding requests */
    Api_RequestList* requestsEnd; /* ptr to end of list */
    int ( *hook_pre )( Types_Session*,
        Transport_Transport*,
        void*,
        int );

    int ( *hook_parse )( Types_Session*,
        Types_Pdu*,
        u_char*,
        size_t );

    int ( *hook_post )( Types_Session*,
        Types_Pdu*, int );

    int ( *hook_build )( Types_Session*,
        Types_Pdu*,
        u_char*, size_t* );

    int ( *hook_realloc_build )( Types_Session*,
        Types_Pdu*, u_char**,
        size_t*, size_t* );

    int ( *check_packet )( u_char*, size_t );

    Types_Pdu* ( *hook_create_pdu )( Transport_Transport*,
        void*, size_t );

    u_char* packet;
    size_t packet_len, packet_size;
};

static void _Api_init( void );

static int _api_priotStoreNeeded = 0;

/*
 * Globals.
 */
#define MAX_PACKET_LENGTH ( 0x7fffffff )

static oid _api_defaultEnterprise[] = { 1, 3, 6, 1, 4, 1, 3, 1, 1 };
/*
 * enterprises.cmu.systems.cmuSNMP
 */

#define DEFAULT_COMMUNITY "public"
#define DEFAULT_RETRIES 5
#define DEFAULT_TIMEOUT ONE_SEC
#define DEFAULT_REMPORT PRIOT_PORT
#define DEFAULT_ENTERPRISE _api_defaultEnterprise
#define DEFAULT_TIME 0

/*
 * don't set higher than 0x7fffffff, and I doubt it should be that high
 * * = 4 gig snmp messages max
 */
#define MAXIMUM_PACKET_SIZE 0x7fffffff

/*
 * Internal information about the state of the snmp session.
 */

//Size of api_errors is (-ErrorCode_MAX + 1)
static const char* _api_errors[] = {
    "No error", /* ErrorCode_SUCCESS */
    "Generic error", /* ErrorCode_GENERR */
    "Invalid local port", /* ErrorCode_BAD_LOCPORT */
    "Unknown host", /* ErrorCode_BAD_ADDRESS */
    "Unknown session", /* ErrorCode_BAD_SESSION */
    "Too long", /* ErrorCode_TOO_LONG */
    "No socket", /* ErrorCode_NO_SOCKET */
    "Cannot send V2 PDU on V1 session", /* ErrorCode_V2_IN_V1 */
    "Cannot send V1 PDU on V2 session", /* ErrorCode_V1_IN_V2 */
    "Bad value for non-repeaters", /* ErrorCode_BAD_REPEATERS */
    "Bad value for max-repetitions", /* ErrorCode_BAD_REPETITIONS */
    "Error building ASN.1 representation", /* ErrorCode_BAD_ASN1_BUILD */
    "Failure in sendto", /* ErrorCode_BAD_SENDTO */
    "Bad parse of ASN.1 type", /* ErrorCode_BAD_PARSE */
    "Bad version specified", /* ErrorCode_BAD_VERSION */
    "Bad source party specified", /* ErrorCode_BAD_SRC_PARTY */
    "Bad destination party specified", /* ErrorCode_BAD_DST_PARTY */
    "Bad context specified", /* ErrorCode_BAD_CONTEXT */
    "Bad community specified", /* ErrorCode_BAD_COMMUNITY */
    "Cannot send noAuth/Priv", /* ErrorCode_NOAUTH_DESPRIV */
    "Bad ACL definition", /* ErrorCode_BAD_ACL */
    "Bad Party definition", /* ErrorCode_BAD_PARTY */
    "Session abort failure", /* ErrorCode_ABORT */
    "Unknown PDU type", /* ErrorCode_UNKNOWN_PDU */
    "Timeout", /* ErrorCode_TIMEOUT */
    "Failure in recvfrom", /* ErrorCode_BAD_RECVFROM */
    "Unable to determine contextEngineID", /* ErrorCode_BAD_ENG_ID */
    "No securityName specified", /* ErrorCode_BAD_SEC_NAME */
    "Unable to determine securityLevel", /* ErrorCode_BAD_SEC_LEVEL  */
    "ASN.1 parse error in message", /* ErrorCode_ASN_PARSE_ERR */
    "Unknown security model in message", /* ErrorCode_UNKNOWN_SEC_MODEL */
    "Invalid message (e.g. msgFlags)", /* ErrorCode_INVALID_MSG */
    "Unknown engine ID", /* ErrorCode_UNKNOWN_ENG_ID */
    "Unknown user name", /* ErrorCode_UNKNOWN_USER_NAME */
    "Unsupported security level", /* ErrorCode_UNSUPPORTED_SEC_LEVEL */
    "Authentication failure (incorrect password, community or key)", /* ErrorCode_AUTHENTICATION_FAILURE */
    "Not in time window", /* ErrorCode_NOT_IN_TIME_WINDOW */
    "Decryption error", /* ErrorCode_DECRYPTION_ERR */
    "SCAPI general failure", /* ErrorCode_SC_GENERAL_FAILURE */
    "SCAPI sub-system not configured", /* ErrorCode_SC_NOT_CONFIGURED */
    "Key tools not available", /* ErrorCode_KT_NOT_AVAILABLE */
    "Unknown Report message", /* ErrorCode_UNKNOWN_REPORT */
    "USM generic error", /* ErrorCode_USM_GENERICERROR */
    "USM unknown security name (no such user exists)", /* ErrorCode_USM_UNKNOWNSECURITYNAME */
    "USM unsupported security level (this user has not been configured for that level of security)", /* ErrorCode_USM_UNSUPPORTEDSECURITYLEVEL */
    "USM encryption error", /* ErrorCode_USM_ENCRYPTIONERROR */
    "USM authentication failure (incorrect password or key)", /* ErrorCode_USM_AUTHENTICATIONFAILURE */
    "USM parse error", /* ErrorCode_USM_PARSEERROR */
    "USM unknown engineID", /* ErrorCode_USM_UNKNOWNENGINEID */
    "USM not in time window", /* ErrorCode_USM_NOTINTIMEWINDOW */
    "USM decryption error", /* ErrorCode_USM_DECRYPTIONERROR */
    "MIB not initialized", /* ErrorCode_NOMIB */
    "Value out of range", /* ErrorCode_RANGE */
    "Sub-id out of range", /* ErrorCode_MAX_SUBID */
    "Bad sub-id in object identifier", /* ErrorCode_BAD_SUBID */
    "Object identifier too long", /* ErrorCode_LONG_OID */
    "Bad value name", /* ErrorCode_BAD_NAME */
    "Bad value notation", /* ErrorCode_VALUE */
    "Unknown Object Identifier", /* ErrorCode_UNKNOWN_OBJID */
    "No PDU in Api_send", /* ErrorCode_NULL_PDU */
    "Missing variables in PDU", /* ErrorCode_NO_VARS */
    "Bad variable type", /* ErrorCode_VAR_TYPE */
    "Out of memory (malloc failure)", /* ErrorCode_MALLOC */
    "Kerberos related error", /* ErrorCode_KRB5 */
    "Protocol error", /* ErrorCode_PROTOCOL */
    "OID not increasing", /* ErrorCode_OID_NONINCREASING */
    "Context probe", /* ErrorCode_JUST_A_CONTEXT_PROBE */
    "Configuration data found but the transport can't be configured", /* ErrorCode_TRANSPORT_NO_CONFIG */
    "Transport configuration failed", /* ErrorCode_TRANSPORT_CONFIG_ERROR */
};

static const char* _api_secLevelName[] = {
    "BAD_SEC_LEVEL",
    "noAuthNoPriv",
    "authNoPriv",
    "authPriv"
};

/*
 * Multiple threads may changes these variables.
 * Suggest using the Single API, which does not use api_sessions.
 *
 * api_reqid may need to be protected. Time will tell...
 *
 */
/*
 * MTCRITICAL_RESOURCE
 */
/*
 * use token in comments to individually protect these resources
 */
struct Api_SessionList_s* api_sessions = NULL; /* MTSUPPORT_LIB_SESSION */
static long _api_reqid = 0; /* MT_LIB_REQUESTID */
static long _api_msgid = 0; /* MT_LIB_MESSAGEID */
static long _api_sessid = 0; /* MT_LIB_SESSIONID */
static long _api_transid = 0; /* MT_LIB_TRANSID */
int api_priotErrno = 0;
/*
 * END MTCRITICAL_RESOURCE
 */

/*
 * global error detail storage
 */
static char _api_detail[ 192 ];
static int _api_detailF = 0;

/*
 * Prototypes.
 */
int Api_build( u_char** pkt, size_t* pkt_len,
    size_t* offset, Types_Session* pss,
    Types_Pdu* pdu );

static int _Api_parse( void*, Types_Session*, Types_Pdu*,
    u_char*, size_t );

static void _Api_v3CalcMsgFlags( int, int, u_char* );

static int _Api_v3VerifyMsg( Api_RequestList*, Types_Pdu* );

static int _Api_v3Build( u_char** pkt, size_t* pkt_len,
    size_t* offset, Types_Session* session,
    Types_Pdu* pdu );
static int _Api_parseVersion( u_char*, size_t );

static int _Api_resendRequest( struct Api_SessionList_s* slp,
    Api_RequestList* rp,
    int incr_retries );

static void _Api_registerDefaultHandlers( void );

static struct Api_SessionList_s* _Api_sessCopy2( Types_Session* pss );

const char*
Api_pduType( int type )
{
    static char unknown[ 20 ];
    switch ( type ) {
    case PRIOT_MSG_GET:
        return "GET";
    case PRIOT_MSG_GETNEXT:
        return "GETNEXT";
    case PRIOT_MSG_GETBULK:
        return "GETBULK";
    case PRIOT_MSG_SET:
        return "SET";
    case PRIOT_MSG_RESPONSE:
        return "RESPONSE";
    case PRIOT_MSG_INFORM:
        return "INFORM";
    case PRIOT_MSG_TRAP2:
        return "TRAP2";
    case PRIOT_MSG_REPORT:
        return "REPORT";
    default:
        snprintf( unknown, sizeof( unknown ), "?0x%2X?", type );
        return unknown;
    }
}

#define DEBUGPRINTPDUTYPE( token, type ) \
    DEBUG_DUMPSECTION( token, Api_pduType( type ) )

long Api_getNextReqid( void )
{
    long retVal;
    Mutex_lock( MTSUPPORT_LIBRARY_ID, MTSUPPORT_LIB_REQUESTID );
    retVal = 1 + _api_reqid; /*MTCRITICAL_RESOURCE */
    if ( !retVal )
        retVal = 2;
    _api_reqid = retVal;
    if ( DefaultStore_getBoolean( DsStorage_LIBRARY_ID, DsBool_LIB_16BIT_IDS ) )
        retVal &= 0x7fff; /* mask to 15 bits */
    else
        retVal &= 0x7fffffff; /* mask to 31 bits */

    if ( !retVal ) {
        _api_reqid = retVal = 2;
    }
    Mutex_unlock( MTSUPPORT_LIBRARY_ID, MTSUPPORT_LIB_REQUESTID );
    return retVal;
}

long Api_getNextMsgid( void )
{
    long retVal;
    Mutex_lock( MTSUPPORT_LIBRARY_ID, MTSUPPORT_LIB_MESSAGEID );
    retVal = 1 + _api_msgid; /*MTCRITICAL_RESOURCE */
    if ( !retVal )
        retVal = 2;
    _api_msgid = retVal;
    if ( DefaultStore_getBoolean( DsStorage_LIBRARY_ID, DsBool_LIB_16BIT_IDS ) )
        retVal &= 0x7fff; /* mask to 15 bits */
    else
        retVal &= 0x7fffffff; /* mask to 31 bits */

    if ( !retVal ) {
        _api_msgid = retVal = 2;
    }
    Mutex_unlock( MTSUPPORT_LIBRARY_ID, MTSUPPORT_LIB_MESSAGEID );
    return retVal;
}

long Api_getNextSessid( void )
{
    long retVal;
    Mutex_lock( MTSUPPORT_LIBRARY_ID, MT_LIB_SESSIONID );
    retVal = 1 + _api_sessid; /*MTCRITICAL_RESOURCE */
    if ( !retVal )
        retVal = 2;
    _api_sessid = retVal;
    if ( DefaultStore_getBoolean( DsStorage_LIBRARY_ID, DsBool_LIB_16BIT_IDS ) )
        retVal &= 0x7fff; /* mask to 15 bits */
    else
        retVal &= 0x7fffffff; /* mask to 31 bits */

    if ( !retVal ) {
        _api_sessid = retVal = 2;
    }
    Mutex_unlock( MTSUPPORT_LIBRARY_ID, MT_LIB_SESSIONID );
    return retVal;
}

long Api_getNextTransid( void )
{
    long retVal;
    Mutex_lock( MTSUPPORT_LIBRARY_ID, MT_LIB_TRANSID );
    retVal = 1 + _api_transid; /*MTCRITICAL_RESOURCE */
    if ( !retVal )
        retVal = 2;
    _api_transid = retVal;
    if ( DefaultStore_getBoolean( DsStorage_LIBRARY_ID, DsBool_LIB_16BIT_IDS ) )
        retVal &= 0x7fff; /* mask to 15 bits */
    else
        retVal &= 0x7fffffff; /* mask to 31 bits */

    if ( !retVal ) {
        _api_transid = retVal = 2;
    }
    Mutex_unlock( MTSUPPORT_LIBRARY_ID, MT_LIB_TRANSID );
    return retVal;
}

void Api_perror( const char* prog_string )
{
    const char* str;
    int xerr;
    xerr = api_priotErrno; /*MTCRITICAL_RESOURCE */
    str = Api_errstring( xerr );
    Logger_log( LOGGER_PRIORITY_ERR, "%s: %s\n", prog_string, str );
}

void Api_setDetail( const char* detail_string )
{
    if ( detail_string != NULL ) {
        String_copyTruncate( ( char* )_api_detail, detail_string, sizeof( _api_detail ) );
        _api_detailF = 1;
    }
}

/*
 * returns pointer to static data
 */
/*
 * results not guaranteed in multi-threaded use
 */
const char*
Api_errstring( int snmp_errnumber )
{
    const char* msg = "";
    static char msg_buf[ IMPL_SPRINT_MAX_LEN ];

    if ( snmp_errnumber >= ErrorCode_MAX && snmp_errnumber <= ErrorCode_GENERR ) {
        msg = _api_errors[ -snmp_errnumber ];
    } else if ( snmp_errnumber != ErrorCode_SUCCESS ) {
        msg = NULL;
    }
    if ( !msg ) {
        snprintf( msg_buf, sizeof( msg_buf ), "Unknown error: %d", snmp_errnumber );
        msg_buf[ sizeof( msg_buf ) - 1 ] = '\0';
    } else if ( _api_detailF ) {
        snprintf( msg_buf, sizeof( msg_buf ), "%s (%s)", msg, _api_detail );
        msg_buf[ sizeof( msg_buf ) - 1 ] = '\0';
        _api_detailF = 0;
    } else {
        String_copyTruncate( msg_buf, msg, sizeof( msg_buf ) );
    }

    return ( msg_buf );
}

/*
 * Api_error - return error data
 * Inputs :  address of errno, address of api_priotErrno, address of string
 * Caller must free the string returned after use.
 */
void Api_error( Types_Session* psess,
    int* p_errno, int* p_snmp_errno, char** p_str )
{
    char buf[ IMPL_SPRINT_MAX_LEN ];
    int snmp_errnumber;

    if ( p_errno )
        *p_errno = psess->s_errno;
    if ( p_snmp_errno )
        *p_snmp_errno = psess->s_snmp_errno;
    if ( p_str == NULL )
        return;

    strcpy( buf, "" );
    snmp_errnumber = psess->s_snmp_errno;
    if ( snmp_errnumber >= ErrorCode_MAX && snmp_errnumber <= ErrorCode_GENERR ) {
        if ( _api_detailF ) {
            snprintf( buf, sizeof( buf ), "%s (%s)", _api_errors[ -snmp_errnumber ],
                _api_detail );
            buf[ sizeof( buf ) - 1 ] = '\0';
            _api_detailF = 0;
        } else
            String_copyTruncate( buf, _api_errors[ -snmp_errnumber ], sizeof( buf ) );
    } else {
        if ( snmp_errnumber ) {
            snprintf( buf, sizeof( buf ), "Unknown Error %d", snmp_errnumber );
            buf[ sizeof( buf ) - 1 ] = '\0';
        }
    }

    /*
     * append a useful system errno interpretation.
     */
    if ( psess->s_errno ) {
        const char* error = strerror( psess->s_errno );
        if ( error == NULL )
            error = "Unknown Error";
        snprintf( &buf[ strlen( buf ) ], sizeof( buf ) - strlen( buf ),
            " (%s)", error );
    }
    buf[ sizeof( buf ) - 1 ] = '\0';
    *p_str = strdup( buf );
}

/*
 * Api_sessError - same as Api_error for single session API use.
 */
void Api_sessError( void* sessp, int* p_errno, int* p_snmp_errno, char** p_str )
{
    struct Api_SessionList_s* slp = ( struct Api_SessionList_s* )sessp;

    if ( ( slp ) && ( slp->session ) )
        Api_error( slp->session, p_errno, p_snmp_errno, p_str );
}

/*
 * Api_sessLogError(): print a error stored in a session pointer
 */
void Api_sessLogError( int priority,
    const char* prog_string, Types_Session* ss )
{
    char* err;
    Api_error( ss, NULL, NULL, &err );
    Logger_log( priority, "%s: %s\n", prog_string, err );
    MEMORY_FREE( err );
}

/*
 * Api_sessPerror(): print a error stored in a session pointer
 */
void Api_sessPerror( const char* prog_string, Types_Session* ss )
{
    Api_sessLogError( LOGGER_PRIORITY_ERR, prog_string, ss );
}

/*
 * Primordial SNMP library initialization.
 * Initializes mutex locks.
 * Invokes minimum required initialization for displaying MIB objects.
 * Gets initial request ID for all transactions,
 * and finds which port SNMP over UDP uses.
 * SNMP over AppleTalk is not currently supported.
 *
 * Warning: no debug messages here.
 */
static char _api_initPriotInitDone2 = 0;

static void
_Api_init( void )
{

    struct timeval tv;
    long tmpReqid, tmpMsgid;

    if ( _api_initPriotInitDone2 )
        return;
    _api_initPriotInitDone2 = 1;
    _api_reqid = 1;

    Mutex_Init(); /* initialize the mt locking structures */
    Parse_initMibInternals();
    Transport_tdomainInit();

    gettimeofday( &tv, ( struct timezone* )0 );
    /*
     * Now = tv;
     */

    /*
     * get pseudo-random values for request ID and message ID
     */

    srandom( ( unsigned )( tv.tv_sec ^ tv.tv_usec ) );
    tmpReqid = random();
    tmpMsgid = random();

    /*
     * don't allow zero value to repeat init
     */
    if ( tmpReqid == 0 )
        tmpReqid = 1;
    if ( tmpMsgid == 0 )
        tmpMsgid = 1;
    _api_reqid = tmpReqid;
    _api_msgid = tmpMsgid;

    Service_registerDefaultDomain( "priot", "udp udp6" );
    Service_registerDefaultDomain( "priottrap", "udp udp6" );

    Service_registerDefaultTarget( "priot", "udp", ":161" );
    Service_registerDefaultTarget( "priot", "tcp", ":161" );
    Service_registerDefaultTarget( "priot", "udp6", ":161" );
    Service_registerDefaultTarget( "priot", "tcp6", ":161" );
    Service_registerDefaultTarget( "priot", "dtlsudp", ":10161" );
    Service_registerDefaultTarget( "priot", "tlstcp", ":10161" );
    Service_registerDefaultTarget( "priot", "ipx", "/36879" );

    Service_registerDefaultTarget( "priottrap", "udp", ":162" );
    Service_registerDefaultTarget( "priottrap", "tcp", ":162" );
    Service_registerDefaultTarget( "priottrap", "udp6", ":162" );
    Service_registerDefaultTarget( "priottrap", "tcp6", ":162" );
    Service_registerDefaultTarget( "priottrap", "dtlsudp", ":10162" );
    Service_registerDefaultTarget( "priottrap", "tlstcp", ":10162" );
    Service_registerDefaultTarget( "priottrap", "ipx", "/36880" );

    DefaultStore_setInt( DsStorage_LIBRARY_ID,
        DsInt_HEX_OUTPUT_LENGTH, 16 );
    DefaultStore_setInt( DsStorage_LIBRARY_ID, DsInt_RETRIES,
        DEFAULT_RETRIES );

    DefaultStore_setBoolean( DsStorage_LIBRARY_ID,
        DsBool_REVERSE_ENCODE,
        DEFAULT_ASNENCODING_DIRECTION );
}

/*
 * Initializes the session structure.
 * May perform one time minimal library initialization.
 * No MIB file processing is done via this call.
 */
void Api_sessInit( Types_Session* session )
{
    _Api_init();

    /*
     * initialize session to default values
     */

    memset( session, 0, sizeof( Types_Session ) );
    session->remote_port = API_DEFAULT_REMPORT;
    session->timeout = API_DEFAULT_TIMEOUT;
    session->retries = API_DEFAULT_RETRIES;
    session->version = API_DEFAULT_VERSION;
    session->securityModel = API_DEFAULT_SECMODEL;
    session->rcvMsgMaxSize = API_MAX_MSG_SIZE;
    session->flags |= API_FLAGS_DONT_PROBE;
}

static void
_Api_registerDefaultHandlers( void )
{
    DefaultStore_registerConfig( ASN01_BOOLEAN, "priot", "dumpPacket",
        DsStorage_LIBRARY_ID, DsBool_DUMP_PACKET );
    DefaultStore_registerConfig( ASN01_BOOLEAN, "priot", "reverseEncodeBER",
        DsStorage_LIBRARY_ID, DsBool_REVERSE_ENCODE );
    DefaultStore_registerConfig( ASN01_INTEGER, "priot", "defaultPort",
        DsStorage_LIBRARY_ID, DsInt_DEFAULT_PORT );

    DefaultStore_registerPremib( ASN01_BOOLEAN, "priot", "noTokenWarnings",
        DsStorage_LIBRARY_ID, DsBool_NO_TOKEN_WARNINGS );
    DefaultStore_registerConfig( ASN01_BOOLEAN, "priot", "noRangeCheck",
        DsStorage_LIBRARY_ID, DsBool_DONT_CHECK_RANGE );
    DefaultStore_registerPremib( ASN01_OCTET_STR, "priot", "persistentDir",
        DsStorage_LIBRARY_ID, DsStr_PERSISTENT_DIR );
    DefaultStore_registerConfig( ASN01_OCTET_STR, "priot", "tempFilePattern",
        DsStorage_LIBRARY_ID, DsStr_TEMP_FILE_PATTERN );
    DefaultStore_registerConfig( ASN01_BOOLEAN, "priot", "noDisplayHint",
        DsStorage_LIBRARY_ID, DsBool_NO_DISPLAY_HINT );
    DefaultStore_registerConfig( ASN01_BOOLEAN, "priot", "16bitIDs",
        DsStorage_LIBRARY_ID, DsBool_LIB_16BIT_IDS );
    DefaultStore_registerPremib( ASN01_OCTET_STR, "priot", "clientaddr",
        DsStorage_LIBRARY_ID, DsStr_CLIENT_ADDR );
    DefaultStore_registerConfig( ASN01_INTEGER, "priot", "serverSendBuf",
        DsStorage_LIBRARY_ID, DsInt_SERVERSENDBUF );
    DefaultStore_registerConfig( ASN01_INTEGER, "priot", "serverRecvBuf",
        DsStorage_LIBRARY_ID, DsInt_SERVERRECVBUF );
    DefaultStore_registerConfig( ASN01_INTEGER, "priot", "clientSendBuf",
        DsStorage_LIBRARY_ID, DsInt_CLIENTSENDBUF );
    DefaultStore_registerConfig( ASN01_INTEGER, "priot", "clientRecvBuf",
        DsStorage_LIBRARY_ID, DsInt_CLIENTRECVBUF );
    DefaultStore_registerConfig( ASN01_BOOLEAN, "priot", "noPersistentLoad",
        DsStorage_LIBRARY_ID, DsBool_DISABLE_PERSISTENT_LOAD );
    DefaultStore_registerConfig( ASN01_BOOLEAN, "priot", "noPersistentSave",
        DsStorage_LIBRARY_ID, DsBool_DISABLE_PERSISTENT_SAVE );
    DefaultStore_registerConfig( ASN01_BOOLEAN, "priot",
        "noContextEngineIDDiscovery",
        DsStorage_LIBRARY_ID,
        DsBool_NO_DISCOVERY );
    DefaultStore_registerConfig( ASN01_INTEGER, "priot", "timeout",
        DsStorage_LIBRARY_ID, DsInt_TIMEOUT );
    DefaultStore_registerConfig( ASN01_INTEGER, "priot", "retries",
        DsStorage_LIBRARY_ID, DsInt_RETRIES );

    Service_registerServiceHandlers();
}

static int _api_initPriotInitDone = 0; /* To prevent double init's. */
/**
 * Calls the functions to do config file loading and  mib module parsing
 * in the correct order.
 *
 * @param type label for the config file "type"
 *
 * @return void
 *
 * @see init_agent
 */
void Api_init( const char* type )
{
    if ( _api_initPriotInitDone ) {
        return;
    }

    _api_initPriotInitDone = 1;

    /*
     * make the type available everywhere else
     */
    if ( type && !DefaultStore_getString( DsStorage_LIBRARY_ID, DsStr_APPTYPE ) ) {
        DefaultStore_setString( DsStorage_LIBRARY_ID,
            DsStr_APPTYPE, type );
    }

    _Api_init();

    /*
     * set our current locale properly to initialize isprint() type functions
     */
    setlocale( LC_CTYPE, "" );

    Debug_debugInit(); /* should be done first, to turn on debugging ASAP */
    Container_initList();
    Callback_initCallbacks();
    Logger_initLogger();
    Api_initStatistics();
    Mib_registerMibHandlers();
    _Api_registerDefaultHandlers();
    Transport_initTransport();
    V3_initV3( type );
    Alarm_initAlarm();
    //Enum_initEnum( type );
    Vacm_initVacm();

    ReadConfig_readPremibConfigs();
    Mib_initMib();

    ReadConfig_readConfigs();

} /* end Api_init() */

/**
 * set a flag indicating that the persistent store needs to be saved.
 */
void Api_storeNeeded( const char* type )
{
    DEBUG_MSGTL( ( "priotStore", "setting needed flag...\n" ) );
    _api_priotStoreNeeded = 1;
}

void Api_storeIfNeeded( void )
{
    if ( 0 == _api_priotStoreNeeded )
        return;

    DEBUG_MSGTL( ( "priotStore", "store needed...\n" ) );
    Api_store( DefaultStore_getString( DsStorage_LIBRARY_ID,
        DsStr_APPTYPE ) );
    _api_priotStoreNeeded = 0;
}

void Api_store( const char* type )
{
    DEBUG_MSGTL( ( "priotStore", "storing stuff...\n" ) );
    ReadConfig_savePersistent( type );
    Callback_callCallbacks( CALLBACK_LIBRARY, CALLBACK_STORE_DATA, NULL );
    ReadConfig_cleanPersistent( type );
}

/**
 * Shuts down the application, saving any needed persistent storage,
 * and appropriate clean up.
 *
 * @param type Label for the config file "type" used
 *
 * @return void
 */
void Api_shutdown( const char* type )
{
    Api_store( type );
    Callback_callCallbacks( CALLBACK_LIBRARY, CALLBACK_SHUTDOWN, NULL );
    Logger_shutdownLogger();
    Alarm_unregisterAll();
    Api_closeSessions();
    Mib_shutdownMib();

    ReadConfig_unregisterAllConfigHandlers();
    Container_freeList();
    Secmod_clear();
    MapList_clear();
    Transport_clearTdomainList();
    Callback_clearCallback();
    DefaultStore_shutdown();
    Service_clearDefaultTarget();
    Service_clearDefaultDomain();
    Secmod_shutdown();

    _api_initPriotInitDone = 0;
    _api_initPriotInitDone2 = 0;
}

/*
 * Sets up the session with the snmp_session information provided by the user.
 * Then opens and binds the necessary low-level transport.  A handle to the
 * created session is returned (this is NOT the same as the pointer passed to
 * Api_open()).  On any error, NULL is returned and api_priotErrno is set to the
 * appropriate error code.
 */
Types_Session*
Api_open( Types_Session* session )
{
    struct Api_SessionList_s* slp;
    slp = ( struct Api_SessionList_s* )Api_sessOpen( session );
    if ( !slp ) {
        return NULL;
    }

    Mutex_lock( MTSUPPORT_LIBRARY_ID, MTSUPPORT_LIB_SESSION );
    slp->next = api_sessions;
    api_sessions = slp;
    Mutex_unlock( MTSUPPORT_LIBRARY_ID, MTSUPPORT_LIB_SESSION );

    return ( slp->session );
}

/*
 * extended open
 */

Types_Session*
Api_openEx( Types_Session* session,
    int ( *fpre_parse )( Types_Session*, Transport_Transport*, void*, int ),
    int ( *fparse )( Types_Session*, Types_Pdu*, u_char*, size_t ),
    int ( *fpost_parse )( Types_Session*, Types_Pdu*, int ),
    int ( *fbuild )( Types_Session*, Types_Pdu*, u_char*, size_t* ),
    int ( *frbuild )( Types_Session*, Types_Pdu*, u_char**, size_t*, size_t* ),
    int ( *fcheck )( u_char*, size_t ) )
{
    struct Api_SessionList_s* slp;
    slp = ( struct Api_SessionList_s* )Api_sessOpen( session );
    if ( !slp ) {
        return NULL;
    }
    slp->internal->hook_pre = fpre_parse;
    slp->internal->hook_parse = fparse;
    slp->internal->hook_post = fpost_parse;
    slp->internal->hook_build = fbuild;
    slp->internal->hook_realloc_build = frbuild;
    slp->internal->check_packet = fcheck;

    Mutex_lock( MTSUPPORT_LIBRARY_ID, MTSUPPORT_LIB_SESSION );
    slp->next = api_sessions;
    api_sessions = slp;
    Mutex_unlock( MTSUPPORT_LIBRARY_ID, MTSUPPORT_LIB_SESSION );

    return ( slp->session );
}

static struct Api_SessionList_s*
_Api_sessCopy2( Types_Session* in_session )
{
    struct Api_SessionList_s* slp;
    struct Api_InternalSession_s* isp;
    Types_Session* session;
    struct Secmod_Def_s* sptr;
    char* cp;
    u_char* ucp;

    in_session->s_snmp_errno = 0;
    in_session->s_errno = 0;

    /*
     * Copy session structure and link into list
     */
    slp = ( struct Api_SessionList_s* )calloc( 1, sizeof( struct Api_SessionList_s ) );
    if ( slp == NULL ) {
        in_session->s_snmp_errno = ErrorCode_MALLOC;
        return ( NULL );
    }

    slp->transport = NULL;

    isp = ( struct Api_InternalSession_s* )calloc( 1, sizeof( struct Api_InternalSession_s ) );

    if ( isp == NULL ) {
        Api_sessClose( slp );
        in_session->s_snmp_errno = ErrorCode_MALLOC;
        return ( NULL );
    }

    slp->internal = isp;
    slp->session = ( Types_Session* )malloc( sizeof( Types_Session ) );
    if ( slp->session == NULL ) {
        Api_sessClose( slp );
        in_session->s_snmp_errno = ErrorCode_MALLOC;
        return ( NULL );
    }
    memmove( slp->session, in_session, sizeof( Types_Session ) );
    session = slp->session;

    /*
     * zero out pointers so if we have to free the session we wont free mem
     * owned by in_session
     */
    session->localname = NULL;
    session->peername = NULL;
    session->community = NULL;
    session->contextEngineID = NULL;
    session->contextName = NULL;
    session->securityEngineID = NULL;
    session->securityName = NULL;
    session->securityAuthProto = NULL;
    session->securityPrivProto = NULL;
    /*
     * session now points to the new structure that still contains pointers to
     * data allocated elsewhere.  Some of this data is copied to space malloc'd
     * here, and the pointer replaced with the new one.
     */

    if ( in_session->peername != NULL ) {
        session->peername = ( char* )malloc( strlen( in_session->peername ) + 1 );
        if ( session->peername == NULL ) {
            Api_sessClose( slp );
            in_session->s_snmp_errno = ErrorCode_MALLOC;
            return ( NULL );
        }
        strcpy( session->peername, in_session->peername );
    }

    if ( session->securityLevel <= 0 ) {
        session->securityLevel = DefaultStore_getInt( DsStorage_LIBRARY_ID, DsInt_SECLEVEL );
    }

    if ( in_session->securityEngineIDLen > 0 ) {
        ucp = ( u_char* )malloc( in_session->securityEngineIDLen );
        if ( ucp == NULL ) {
            Api_sessClose( slp );
            in_session->s_snmp_errno = ErrorCode_MALLOC;
            return ( NULL );
        }
        memmove( ucp, in_session->securityEngineID,
            in_session->securityEngineIDLen );
        session->securityEngineID = ucp;
    }

    if ( in_session->contextEngineIDLen > 0 ) {
        ucp = ( u_char* )malloc( in_session->contextEngineIDLen );
        if ( ucp == NULL ) {
            Api_sessClose( slp );
            in_session->s_snmp_errno = ErrorCode_MALLOC;
            return ( NULL );
        }
        memmove( ucp, in_session->contextEngineID,
            in_session->contextEngineIDLen );
        session->contextEngineID = ucp;
    } else if ( in_session->securityEngineIDLen > 0 ) {
        /*
         * default contextEngineID to securityEngineIDLen if defined
         */
        ucp = ( u_char* )malloc( in_session->securityEngineIDLen );
        if ( ucp == NULL ) {
            Api_sessClose( slp );
            in_session->s_snmp_errno = ErrorCode_MALLOC;
            return ( NULL );
        }
        memmove( ucp, in_session->securityEngineID,
            in_session->securityEngineIDLen );
        session->contextEngineID = ucp;
        session->contextEngineIDLen = in_session->securityEngineIDLen;
    }

    if ( in_session->contextName ) {
        session->contextName = strdup( in_session->contextName );
        if ( session->contextName == NULL ) {
            Api_sessClose( slp );
            return ( NULL );
        }
        session->contextNameLen = in_session->contextNameLen;
    } else {
        if ( ( cp = DefaultStore_getString( DsStorage_LIBRARY_ID,
                   DsStr_CONTEXT ) )
            != NULL )
            cp = strdup( cp );
        else
            cp = strdup( API_DEFAULT_CONTEXT );
        if ( cp == NULL ) {
            Api_sessClose( slp );
            return ( NULL );
        }
        session->contextName = cp;
        session->contextNameLen = strlen( cp );
    }

    if ( in_session->securityName ) {
        session->securityName = strdup( in_session->securityName );
        if ( session->securityName == NULL ) {
            Api_sessClose( slp );
            return ( NULL );
        }
    } else if ( ( cp = DefaultStore_getString( DsStorage_LIBRARY_ID,
                      DsStr_SECNAME ) )
        != NULL ) {
        cp = strdup( cp );
        if ( cp == NULL ) {
            Api_sessClose( slp );
            return ( NULL );
        }
        session->securityName = cp;
        session->securityNameLen = strlen( cp );
    }

    if ( session->retries == API_DEFAULT_RETRIES ) {
        int retry = DefaultStore_getInt( DsStorage_LIBRARY_ID,
            DsInt_RETRIES );
        if ( retry < 0 )
            session->retries = DEFAULT_RETRIES;
        else
            session->retries = retry;
    }
    if ( session->timeout == API_DEFAULT_TIMEOUT ) {
        int timeout = DefaultStore_getInt( DsStorage_LIBRARY_ID,
            DsInt_TIMEOUT );
        if ( timeout <= 0 )
            session->timeout = DEFAULT_TIMEOUT;
        else
            session->timeout = timeout * ONE_SEC;
    }
    session->sessid = Api_getNextSessid();

    Callback_callCallbacks( CALLBACK_LIBRARY, CALLBACK_SESSION_INIT,
        session );

    if ( ( sptr = Secmod_find( session->securityModel ) ) != NULL ) {
        /*
         * security module specific copying
         */
        if ( sptr->session_setup ) {
            int ret = ( *sptr->session_setup )( in_session, session );
            if ( ret != ErrorCode_SUCCESS ) {
                Api_sessClose( slp );
                return NULL;
            }
        }

        /*
         * security module specific opening
         */
        if ( sptr->session_open ) {
            int ret = ( *sptr->session_open )( session );
            if ( ret != ErrorCode_SUCCESS ) {
                Api_sessClose( slp );
                return NULL;
            }
        }
    }

    /* Anything below this point should only be done if the transport
       had no say in the matter */
    if ( session->securityLevel == 0 )
        session->securityLevel = PRIOT_SEC_LEVEL_NOAUTH;

    return ( slp );
}

static struct Api_SessionList_s*
_Api_sessCopy( Types_Session* pss )
{
    struct Api_SessionList_s* psl;
    psl = _Api_sessCopy2( pss );
    if ( !psl ) {
        if ( !pss->s_snmp_errno ) {
            pss->s_snmp_errno = ErrorCode_GENERR;
        }
        API_SET_PRIOT_ERROR( pss->s_snmp_errno );
    }
    return psl;
}

/**
 * probe for engineID using RFC 5343 probing mechanisms
 *
 * Designed to be a callback for within a security model's probe_engineid hook.
 * Since it's likely multiple security models won't have engineIDs to
 * probe for then this function is a callback likely to be used by
 * multiple future security models.  E.G. both SSH and DTLS.
 */
int Api_v3ProbeContextEngineIDRfc5343( void* slp, Types_Session* session )
{
    Types_Pdu *pdu = NULL, *response = NULL;
    static oid snmpEngineIDoid[] = { 1, 3, 6, 1, 6, 3, 10, 2, 1, 1, 0 };
    static size_t snmpEngineIDoid_len = 11;

    static char probeEngineID[] = { ( char )0x80, 0, 0, 0, 6 };
    static size_t probeEngineID_len = sizeof( probeEngineID );

    int status;

    pdu = Client_pduCreate( PRIOT_MSG_GET );
    if ( !pdu )
        return PRIOT_ERR_GENERR;
    pdu->version = PRIOT_VERSION_3;
    /* don't require a securityName */
    if ( session->securityName ) {
        pdu->securityName = strdup( session->securityName );
        pdu->securityNameLen = strlen( pdu->securityName );
    }
    pdu->securityLevel = PRIOT_SEC_LEVEL_NOAUTH;
    pdu->securityModel = session->securityModel;
    pdu->contextEngineID = ( u_char* )Memory_memdup( probeEngineID, probeEngineID_len );
    if ( !pdu->contextEngineID ) {
        Logger_log( LOGGER_PRIORITY_ERR, "failed to clone memory for rfc5343 probe\n" );
        Api_freePdu( pdu );
        return PRIOT_ERR_GENERR;
    }
    pdu->contextEngineIDLen = probeEngineID_len;

    Client_addNullVar( pdu, snmpEngineIDoid, snmpEngineIDoid_len );

    DEBUG_MSGTL( ( "priotApi", "probing for engineID using rfc5343 methods...\n" ) );
    session->flags |= API_FLAGS_DONT_PROBE; /* prevent recursion */
    status = Client_sessSynchResponse( slp, pdu, &response );

    if ( ( response == NULL ) || ( status != CLIENT_STAT_SUCCESS ) ) {
        Logger_log( LOGGER_PRIORITY_ERR, "failed rfc5343 contextEngineID probing\n" );
        return PRIOT_ERR_GENERR;
    }

    /* check that the response makes sense */
    if ( NULL != response->variables && NULL != response->variables->name && Api_oidCompare( response->variables->name, response->variables->nameLength, snmpEngineIDoid, snmpEngineIDoid_len ) == 0 && ASN01_OCTET_STR == response->variables->type && NULL != response->variables->val.string && response->variables->valLen > 0 ) {
        session->contextEngineID = ( u_char* )Memory_memdup( response->variables->val.string,
            response->variables->valLen );
        if ( !session->contextEngineID ) {
            Logger_log( LOGGER_PRIORITY_ERR, "failed rfc5343 contextEngineID probing: memory allocation failed\n" );
            return PRIOT_ERR_GENERR;
        }

        /* technically there likely isn't a securityEngineID but just
           in case anyone goes looking we might as well have one */
        session->securityEngineID = ( u_char* )Memory_memdup( response->variables->val.string,
            response->variables->valLen );
        if ( !session->securityEngineID ) {
            Logger_log( LOGGER_PRIORITY_ERR, "failed rfc5343 securityEngineID probing: memory allocation failed\n" );
            return PRIOT_ERR_GENERR;
        }

        session->securityEngineIDLen = session->contextEngineIDLen = response->variables->valLen;

        if ( Debug_getDoDebugging() ) {
            size_t i;
            DEBUG_MSGTL( ( "priotSessOpen",
                "  probe found engineID:  " ) );
            for ( i = 0; i < session->securityEngineIDLen; i++ )
                DEBUG_MSG( ( "priotSessOpen", "%02x",
                    session->securityEngineID[ i ] ) );
            DEBUG_MSG( ( "priotSessOpen", "\n" ) );
        }
    }
    return ErrorCode_SUCCESS;
}

/**
 * probe for peer engineID
 *
 * @param slp         session list pointer.
 * @param in_session  session for errors
 *
 * @note
 *  - called by Api_sessOpen2(), Api_sessAddEx()
 *  - in_session is the user supplied session provided to those functions.
 *  - the first session in slp should the internal allocated copy of in_session
 *
 * @return 0 : error
 * @return 1 : ok
 *
 */
int Api_v3EngineIDProbe( struct Api_SessionList_s* slp,
    Types_Session* in_session )
{
    Types_Session* session;
    int status;
    struct Secmod_Def_s* sptr = NULL;

    if ( slp == NULL || slp->session == NULL ) {
        return 0;
    }

    session = slp->session;
    Assert_assertOrReturn( session != NULL, 0 );
    sptr = Secmod_find( session->securityModel );

    /*
     * If we are opening a V3 session and we don't know engineID we must probe
     * it -- this must be done after the session is created and inserted in the
     * list so that the response can handled correctly.
     */

    if ( session->version == PRIOT_VERSION_3 && ( 0 == ( session->flags & API_FLAGS_DONT_PROBE ) ) ) {
        if ( NULL != sptr && NULL != sptr->probe_engineid ) {
            DEBUG_MSGTL( ( "priotApi", "probing for engineID using security model callback...\n" ) );
            /* security model specific mechanism of determining engineID */
            status = ( *sptr->probe_engineid )( slp, in_session );
            if ( status != ErrorCode_SUCCESS )
                return 0;
        } else {
            /* XXX: default to the default RFC5343 contextEngineID Probe? */
            return 0;
        }
    }

    /*
     * see if there is a hook to call now that we're done probing for an
     * engineID
     */
    if ( sptr && sptr->post_probe_engineid ) {
        status = ( *sptr->post_probe_engineid )( slp, in_session );
        if ( status != ErrorCode_SUCCESS )
            return 0;
    }

    return 1;
}

/*******************************************************************-o-******
 * Api_sessConfigTransport
 *
 * Parameters:
 *	*in_session
 *	*in_transport
 *
 * Returns:
 *      ErrorCode_SUCCESS                     - Yay
 *      ErrorCode_GENERR                      - Generic Error
 *      ErrorCode_TRANSPORT_CONFIG_ERROR      - Transport rejected config
 *      ErrorCode_TRANSPORT_NO_CONFIG         - Transport can't config
 */
int Api_sessConfigTransport( Container_Container* transport_configuration,
    Transport_Transport* transport )
{
    /* Optional supplimental transport configuration information and
       final call to actually open the transport */
    if ( transport_configuration ) {
        DEBUG_MSGTL( ( "priotSess", "configuring transport\n" ) );
        if ( transport->f_config ) {
            Container_Iterator* iter;
            Transport_Config* config_data;
            int ret;

            iter = CONTAINER_ITERATOR( transport_configuration );
            if ( NULL == iter ) {
                return ErrorCode_GENERR;
            }

            for ( config_data = ( Transport_Config* )CONTAINER_ITERATOR_FIRST( iter ); config_data;
                  config_data = ( Transport_Config* )CONTAINER_ITERATOR_NEXT( iter ) ) {
                ret = transport->f_config( transport, config_data->key,
                    config_data->value );
                if ( ret ) {
                    return ErrorCode_TRANSPORT_CONFIG_ERROR;
                }
            }
        } else {
            return ErrorCode_TRANSPORT_NO_CONFIG;
        }
    }
    return ErrorCode_SUCCESS;
}

/**
 * Copies configuration from the session and calls f_open
 * This function copies any configuration stored in the session
 * pointer to the transport if it has a f_config pointer and then
 * calls the transport's f_open function to actually open the
 * connection.
 *
 * @param in_session A pointer to the session that config information is in.
 * @param transport A pointer to the transport to config/open.
 *
 * @return ErrorCode_SUCCESS : on success
 */

/*******************************************************************-o-******
 * Api_sessConfigTransport
 *
 * Parameters:
 *	*in_session
 *	*in_transport
 *
 * Returns:
 *      ErrorCode_SUCCESS                     - Yay
 *      ErrorCode_GENERR                      - Generic Error
 *      ErrorCode_TRANSPORT_CONFIG_ERROR      - Transport rejected config
 *      ErrorCode_TRANSPORT_NO_CONFIG         - Transport can't config
 */
int Api_sessConfigAndOpenTransport( Types_Session* in_session,
    Transport_Transport* transport )
{
    int rc;

    DEBUG_MSGTL( ( "priotSess", "opening transport: %x\n", transport->flags & TRANSPORT_FLAG_OPENED ) );

    /* don't double open */
    if ( transport->flags & TRANSPORT_FLAG_OPENED )
        return ErrorCode_SUCCESS;

    if ( ( rc = Api_sessConfigTransport( in_session->transportConfiguration,
               transport ) )
        != ErrorCode_SUCCESS ) {
        in_session->s_snmp_errno = rc;
        in_session->s_errno = 0;
        return rc;
    }

    if ( transport->f_open )
        transport = transport->f_open( transport );

    if ( transport == NULL ) {
        DEBUG_MSGTL( ( "priotSess", "couldn't interpret peername\n" ) );
        in_session->s_snmp_errno = ErrorCode_BAD_ADDRESS;
        in_session->s_errno = errno;
        Api_setDetail( in_session->peername );
        return ErrorCode_BAD_ADDRESS;
    }

    transport->flags |= TRANSPORT_FLAG_OPENED;
    DEBUG_MSGTL( ( "priotSess", "done opening transport: %x\n", transport->flags & TRANSPORT_FLAG_OPENED ) );
    return ErrorCode_SUCCESS;
}

/*******************************************************************-o-******
 * Api_sessOpen
 *
 * Parameters:
 *	*in_session
 *
 * Returns:
 *      Pointer to a session in the session list   -OR-		FIX -- right?
 *	NULL on failure.
 *
 * The "spin-free" version of Api_open.
 */
static void*
_Api_sessOpen( Types_Session* in_session )
{
    Transport_Transport* transport = NULL;
    int rc;

    in_session->s_snmp_errno = 0;
    in_session->s_errno = 0;

    _Api_init();

    {
        char* clientaddr_save = NULL;

        if ( NULL != in_session->localname ) {
            clientaddr_save = DefaultStore_getString( DsStorage_LIBRARY_ID,
                DsStr_CLIENT_ADDR );
            DefaultStore_setString( DsStorage_LIBRARY_ID,
                DsStr_CLIENT_ADDR,
                in_session->localname );
        }

        if ( in_session->flags & API_FLAGS_STREAM_SOCKET ) {
            transport = Transport_tdomainTransportFull( "priot", in_session->peername,
                in_session->local_port, "tcp,tcp6",
                NULL );
        } else {
            transport = Transport_tdomainTransportFull( "priot", in_session->peername,
                in_session->local_port, "udp,udp6",
                NULL );
        }

        if ( NULL != clientaddr_save )
            DefaultStore_setString( DsStorage_LIBRARY_ID,
                DsStr_CLIENT_ADDR, clientaddr_save );
    }

    if ( transport == NULL ) {
        DEBUG_MSGTL( ( "_ApiSessOpen", "couldn't interpret peername\n" ) );
        in_session->s_snmp_errno = ErrorCode_BAD_ADDRESS;
        in_session->s_errno = errno;
        Api_setDetail( in_session->peername );
        return NULL;
    }

    /* Optional supplimental transport configuration information and
       final call to actually open the transport */
    if ( ( rc = Api_sessConfigAndOpenTransport( in_session, transport ) )
        != ErrorCode_SUCCESS ) {
        transport = NULL;
        return NULL;
    }

    if ( in_session->flags & API_FLAGS_UDP_BROADCAST ) {
        int b = 1;
        int rc;

        rc = setsockopt( transport->sock, SOL_SOCKET, SO_BROADCAST,
            ( char* )&b, sizeof( b ) );

        if ( rc != 0 ) {
            in_session->s_snmp_errno = ErrorCode_BAD_ADDRESS; /* good as any? */
            in_session->s_errno = errno;

            DEBUG_MSGTL( ( "_ApiSessOpen", "couldn't enable UDP_BROADCAST\n" ) );
            return NULL;
        }
    }

    return Api_sessAdd( in_session, transport, NULL, NULL );
}

/*
 * EXTENDED SESSION API ------------------------------------------
 *
 * Api_sessAddEx, Api_sessdd, Api_add
 *
 * Analogous to Api_open family of functions, but taking a Transport_Transport
 * pointer as an extra argument.  Unlike Api_open et al. it doesn't attempt
 * to interpret the in_session->peername as a transport endpoint specifier,
 * but instead uses the supplied transport.  JBPN
 *
 */

Types_Session*
Api_add( Types_Session* in_session,
    Transport_Transport* transport,
    int ( *fpre_parse )( Types_Session*, Transport_Transport*, void*,
             int ),
    int ( *fpost_parse )( Types_Session*,
             Types_Pdu*, int ) )
{
    struct Api_SessionList_s* slp;
    slp = ( struct Api_SessionList_s* )Api_sessAddEx( in_session, transport,
        fpre_parse, NULL,
        fpost_parse, NULL, NULL,
        NULL, NULL );
    if ( slp == NULL ) {
        return NULL;
    }

    Mutex_lock( MTSUPPORT_LIBRARY_ID, MTSUPPORT_LIB_SESSION );
    slp->next = api_sessions;
    api_sessions = slp;
    Mutex_unlock( MTSUPPORT_LIBRARY_ID, MTSUPPORT_LIB_SESSION );

    return ( slp->session );
}

Types_Session*
Api_addFull( Types_Session* in_session,
    Transport_Transport* transport,
    int ( *fpre_parse )( Types_Session*, Transport_Transport*,
                 void*, int ),
    int ( *fparse )( Types_Session*, Types_Pdu*, u_char*,
                 size_t ),
    int ( *fpost_parse )( Types_Session*, Types_Pdu*, int ),
    int ( *fbuild )( Types_Session*, Types_Pdu*, u_char*,
                 size_t* ),
    int ( *frbuild )( Types_Session*,
                 Types_Pdu*,
                 u_char**,
                 size_t*,
                 size_t* ),
    int ( *fcheck )( u_char*, size_t ),
    Types_Pdu* ( *fcreate_pdu )( Transport_Transport*, void*,
                 size_t ) )
{
    struct Api_SessionList_s* slp;
    slp = ( struct Api_SessionList_s* )Api_sessAddEx( in_session, transport,
        fpre_parse, fparse,
        fpost_parse, fbuild,
        frbuild, fcheck,
        fcreate_pdu );
    if ( slp == NULL ) {
        return NULL;
    }

    Mutex_lock( MTSUPPORT_LIBRARY_ID, MTSUPPORT_LIB_SESSION );
    slp->next = api_sessions;
    api_sessions = slp;
    Mutex_unlock( MTSUPPORT_LIBRARY_ID, MTSUPPORT_LIB_SESSION );

    return ( slp->session );
}

void* Api_sessAddEx( Types_Session* in_session,
    Transport_Transport* transport,
    int ( *fpre_parse )( Types_Session*, Transport_Transport*,
                         void*, int ),
    int ( *fparse )( Types_Session*, Types_Pdu*, u_char*,
                         size_t ),
    int ( *fpost_parse )( Types_Session*, Types_Pdu*,
                         int ),
    int ( *fbuild )( Types_Session*, Types_Pdu*, u_char*,
                         size_t* ),
    int ( *frbuild )( Types_Session*, Types_Pdu*,
                         u_char**, size_t*, size_t* ),
    int ( *fcheck )( u_char*, size_t ),
    Types_Pdu* ( *fcreate_pdu )( Transport_Transport*, void*,
                         size_t ) )
{
    struct Api_SessionList_s* slp;
    int rc;

    _Api_init();

    if ( transport == NULL )
        return NULL;

    if ( in_session == NULL ) {
        transport->f_close( transport );
        Transport_free( transport );
        return NULL;
    }

    /* if the transport hasn't been fully opened yet, open it now */
    if ( ( rc = Api_sessConfigAndOpenTransport( in_session, transport ) )
        != ErrorCode_SUCCESS ) {
        return NULL;
    }

    if ( transport->f_setup_session ) {
        if ( ErrorCode_SUCCESS != transport->f_setup_session( transport, in_session ) ) {
            Transport_free( transport );
            return NULL;
        }
    }

    DEBUG_MSGTL( ( "priotSessAdd", "fd %d\n", transport->sock ) );

    if ( ( slp = _Api_sessCopy( in_session ) ) == NULL ) {
        transport->f_close( transport );
        Transport_free( transport );
        return ( NULL );
    }

    slp->transport = transport;
    slp->internal->hook_pre = fpre_parse;
    slp->internal->hook_parse = fparse;
    slp->internal->hook_post = fpost_parse;
    slp->internal->hook_build = fbuild;
    slp->internal->hook_realloc_build = frbuild;
    slp->internal->check_packet = fcheck;
    slp->internal->hook_create_pdu = fcreate_pdu;

    slp->session->rcvMsgMaxSize = transport->msgMaxSize;

    if ( slp->session->version == PRIOT_VERSION_3 ) {
        DEBUG_MSGTL( ( "priotSessAdd",
            "adding v3 session -- maybe engineID probe now\n" ) );
        if ( !Api_v3EngineIDProbe( slp, slp->session ) ) {
            DEBUG_MSGTL( ( "priotSessAdd", "engine ID probe failed\n" ) );
            Api_sessClose( slp );
            return NULL;
        }
    }

    slp->session->flags &= ~API_FLAGS_DONT_PROBE;

    return ( void* )slp;
} /*  end Api_sessAddEx()  */

void* Api_sessAdd( Types_Session* in_session,
    Transport_Transport* transport,
    int ( *fpre_parse )( Types_Session*, Transport_Transport*,
                       void*, int ),
    int ( *fpost_parse )( Types_Session*, Types_Pdu*, int ) )
{
    return Api_sessAddEx( in_session, transport, fpre_parse, NULL,
        fpost_parse, NULL, NULL, NULL, NULL );
}

void* Api_sessOpen( Types_Session* pss )
{
    void* pvoid;
    pvoid = _Api_sessOpen( pss );
    if ( !pvoid ) {
        API_SET_PRIOT_ERROR( pss->s_snmp_errno );
    }
    return pvoid;
}

int Api_createUserFromSession( Types_Session* session )
{
    return Usm_createUserFromSession( session );
}

/*
 *  Do a "deep free()" of a Types_Session.
 *
 *  CAUTION:  SHOULD ONLY BE USED FROM Api_sessClose() OR SIMILAR.
 *                                                      (hence it is static)
 */

static void
_Api_freeSession( Types_Session* s )
{
    if ( s ) {
        MEMORY_FREE( s->localname );
        MEMORY_FREE( s->peername );
        MEMORY_FREE( s->community );
        MEMORY_FREE( s->contextEngineID );
        MEMORY_FREE( s->contextName );
        MEMORY_FREE( s->securityEngineID );
        MEMORY_FREE( s->securityName );
        MEMORY_FREE( s->securityAuthProto );
        MEMORY_FREE( s->securityPrivProto );
        MEMORY_FREE( s->paramName );

        /*
         * clear session from any callbacks
         */
        Callback_clearClientArg( s, 0, 0 );

        free( ( char* )s );
    }
}

/*
 * Close the input session.  Frees all data allocated for the session,
 * dequeues any pending requests, and closes any sockets allocated for
 * the session.  Returns 0 on error, 1 otherwise.
 */
int Api_sessClose( void* sessp )
{
    struct Api_SessionList_s* slp = ( struct Api_SessionList_s* )sessp;
    Transport_Transport* transport;
    struct Api_InternalSession_s* isp;
    Types_Session* sesp = NULL;
    struct Secmod_Def_s* sptr;

    if ( slp == NULL ) {
        return 0;
    }

    if ( slp->session != NULL && ( sptr = Secmod_find( slp->session->securityModel ) ) != NULL && sptr->session_close != NULL ) {
        ( *sptr->session_close )( slp->session );
    }

    isp = slp->internal;
    slp->internal = NULL;

    if ( isp ) {
        Api_RequestList *rp, *orp;

        MEMORY_FREE( isp->packet );

        /*
         * Free each element in the input request list.
         */
        rp = isp->requests;
        while ( rp ) {
            orp = rp;
            rp = rp->next_request;
            if ( orp->callback ) {
                orp->callback( API_CALLBACK_OP_TIMED_OUT,
                    slp->session, orp->pdu->reqid,
                    orp->pdu, orp->cb_data );
            }
            Api_freePdu( orp->pdu );
            free( ( char* )orp );
        }

        free( ( char* )isp );
    }

    transport = slp->transport;
    slp->transport = NULL;

    if ( transport ) {
        transport->f_close( transport );
        Transport_free( transport );
    }

    sesp = slp->session;
    slp->session = NULL;

    /*
     * The following is necessary to avoid memory leakage when closing AgentX
     * sessions that may have multiple subsessions.  These hang off the main
     * session at ->subsession, and chain through ->next.
     */

    if ( sesp != NULL && sesp->subsession != NULL ) {
        Types_Session *subsession = sesp->subsession, *tmpsub;

        while ( subsession != NULL ) {
            DEBUG_MSGTL( ( "priotSessClose",
                "closing session %p, subsession %p\n", sesp,
                subsession ) );
            tmpsub = subsession->next;
            _Api_freeSession( subsession );
            subsession = tmpsub;
        }
    }

    _Api_freeSession( sesp );
    free( ( char* )slp );
    return 1;
}

int Api_close( Types_Session* session )
{
    struct Api_SessionList_s *slp = NULL, *oslp = NULL;

    { /*MTCRITICAL_RESOURCE */
        Mutex_lock( MTSUPPORT_LIBRARY_ID, MTSUPPORT_LIB_SESSION );
        if ( api_sessions && api_sessions->session == session ) { /* If first entry */
            slp = api_sessions;
            api_sessions = slp->next;
        } else {
            for ( slp = api_sessions; slp; slp = slp->next ) {
                if ( slp->session == session ) {
                    if ( oslp ) /* if we found entry that points here */
                        oslp->next = slp->next; /* link around this entry */
                    break;
                }
                oslp = slp;
            }
        }
        Mutex_unlock( MTSUPPORT_LIBRARY_ID, MTSUPPORT_LIB_SESSION );
    } /*END MTCRITICAL_RESOURCE */
    if ( slp == NULL ) {
        return 0;
    }
    return Api_sessClose( ( void* )slp );
}

int Api_closeSessions( void )
{
    struct Api_SessionList_s* slp;

    Mutex_lock( MTSUPPORT_LIBRARY_ID, MTSUPPORT_LIB_SESSION );
    while ( api_sessions ) {
        slp = api_sessions;
        api_sessions = api_sessions->next;
        Api_sessClose( ( void* )slp );
    }
    Mutex_unlock( MTSUPPORT_LIBRARY_ID, MTSUPPORT_LIB_SESSION );
    return 1;
}

static void
_Api_v3CalcMsgFlags( int sec_level, int msg_command, u_char* flags )
{
    *flags = 0;
    if ( sec_level == PRIOT_SEC_LEVEL_AUTHNOPRIV )
        *flags = PRIOT_MSG_FLAG_AUTH_BIT;
    else if ( sec_level == PRIOT_SEC_LEVEL_AUTHPRIV )
        *flags = PRIOT_MSG_FLAG_AUTH_BIT | PRIOT_MSG_FLAG_PRIV_BIT;

    if ( PRIOT_CMD_CONFIRMED( msg_command ) )
        *flags |= PRIOT_MSG_FLAG_RPRT_BIT;

    return;
}

static int
_Api_v3VerifyMsg( Api_RequestList* rp, Types_Pdu* pdu )
{
    Types_Pdu* rpdu;

    if ( !rp || !rp->pdu || !pdu )
        return 0;
    /*
     * Reports don't have to match anything according to the spec
     */
    if ( pdu->command == PRIOT_MSG_REPORT )
        return 1;
    rpdu = rp->pdu;
    if ( rp->request_id != pdu->reqid || rpdu->reqid != pdu->reqid )
        return 0;
    if ( rpdu->version != pdu->version )
        return 0;
    if ( rpdu->securityModel != pdu->securityModel )
        return 0;
    if ( rpdu->securityLevel != pdu->securityLevel )
        return 0;

    if ( rpdu->contextEngineIDLen != pdu->contextEngineIDLen || memcmp( rpdu->contextEngineID, pdu->contextEngineID,
                                                                    pdu->contextEngineIDLen ) )
        return 0;
    if ( rpdu->contextNameLen != pdu->contextNameLen || memcmp( rpdu->contextName, pdu->contextName, pdu->contextNameLen ) )
        return 0;

    /* tunneled transports don't have a securityEngineID...  that's
       USM specific (and maybe other future ones) */
    if ( pdu->securityModel == PRIOT_SEC_MODEL_USM && ( rpdu->securityEngineIDLen != pdu->securityEngineIDLen || memcmp( rpdu->securityEngineID, pdu->securityEngineID,
                                                                                                                     pdu->securityEngineIDLen ) ) )
        return 0;

    /* the securityName must match though regardless of secmodel */
    if ( rpdu->securityNameLen != pdu->securityNameLen || memcmp( rpdu->securityName, pdu->securityName,
                                                              pdu->securityNameLen ) )
        return 0;
    return 1;
}

/*
 * SNMPv3
 * * Takes a session and a pdu and serializes the ASN PDU into the area
 * * pointed to by packet.  out_length is the size of the data area available.
 * * Returns the length of the completed packet in out_length.  If any errors
 * * occur, -1 is returned.  If all goes well, 0 is returned.
 */
static int
_Api_v3Build( u_char** pkt, size_t* pkt_len, size_t* offset,
    Types_Session* session, Types_Pdu* pdu )
{
    int ret;

    session->s_snmp_errno = 0;
    session->s_errno = 0;

    /*
     * do validation for PDU types
     */
    switch ( pdu->command ) {
    case PRIOT_MSG_RESPONSE:
    case PRIOT_MSG_TRAP2:
    case PRIOT_MSG_REPORT:
        Assert_assert( 0 == ( pdu->flags & PRIOT_UCD_MSG_FLAG_EXPECT_RESPONSE ) );
    /*
         * Fallthrough
         */
    case PRIOT_MSG_GET:
    case PRIOT_MSG_GETNEXT:
    case PRIOT_MSG_SET:
    case PRIOT_MSG_INFORM:
        if ( pdu->errstat == API_DEFAULT_ERRSTAT )
            pdu->errstat = 0;
        if ( pdu->errindex == API_DEFAULT_ERRINDEX )
            pdu->errindex = 0;
        break;

    case PRIOT_MSG_GETBULK:
        if ( pdu->max_repetitions < 0 ) {
            session->s_snmp_errno = ErrorCode_BAD_REPETITIONS;
            return -1;
        }
        if ( pdu->non_repeaters < 0 ) {
            session->s_snmp_errno = ErrorCode_BAD_REPEATERS;
            return -1;
        }
        break;

    case PRIOT_MSG_TRAP:
        session->s_snmp_errno = ErrorCode_V1_IN_V2;
        return -1;

    default:
        session->s_snmp_errno = ErrorCode_UNKNOWN_PDU;
        return -1;
    }

    /* Do we need to set the session security engineid? */
    if ( pdu->securityEngineIDLen == 0 ) {
        if ( session->securityEngineIDLen ) {
            V3_cloneEngineID( &pdu->securityEngineID,
                &pdu->securityEngineIDLen,
                session->securityEngineID,
                session->securityEngineIDLen );
        }
    }

    /* Do we need to set the session context engineid? */
    if ( pdu->contextEngineIDLen == 0 ) {
        if ( session->contextEngineIDLen ) {
            V3_cloneEngineID( &pdu->contextEngineID,
                &pdu->contextEngineIDLen,
                session->contextEngineID,
                session->contextEngineIDLen );
        } else if ( pdu->securityEngineIDLen ) {
            V3_cloneEngineID( &pdu->contextEngineID,
                &pdu->contextEngineIDLen,
                pdu->securityEngineID,
                pdu->securityEngineIDLen );
        }
    }

    if ( pdu->contextName == NULL ) {
        if ( !session->contextName ) {
            session->s_snmp_errno = ErrorCode_BAD_CONTEXT;
            return -1;
        }
        pdu->contextName = strdup( session->contextName );
        if ( pdu->contextName == NULL ) {
            session->s_snmp_errno = ErrorCode_GENERR;
            return -1;
        }
        pdu->contextNameLen = session->contextNameLen;
    }
    if ( pdu->securityModel == API_DEFAULT_SECMODEL ) {
        pdu->securityModel = session->securityModel;
        if ( pdu->securityModel == API_DEFAULT_SECMODEL ) {
            pdu->securityModel = MapList_findValue( "priotSecmods", DefaultStore_getString( DsStorage_LIBRARY_ID, DsStr_SECMODEL ) );

            if ( pdu->securityModel <= 0 ) {
                pdu->securityModel = PRIOT_SEC_MODEL_USM;
            }
        }
    }
    if ( pdu->securityNameLen == 0 && pdu->securityName == NULL ) {
        if ( session->securityModel != PRIOT_SEC_MODEL_TSM && session->securityNameLen == 0 ) {
            session->s_snmp_errno = ErrorCode_BAD_SEC_NAME;
            return -1;
        }
        if ( session->securityName ) {
            pdu->securityName = strdup( session->securityName );
            if ( pdu->securityName == NULL ) {
                session->s_snmp_errno = ErrorCode_GENERR;
                return -1;
            }
            pdu->securityNameLen = session->securityNameLen;
        } else {
            pdu->securityName = strdup( "" );
            session->securityName = strdup( "" );
        }
    }
    if ( pdu->securityLevel == 0 ) {
        if ( session->securityLevel == 0 ) {
            session->s_snmp_errno = ErrorCode_BAD_SEC_LEVEL;
            return -1;
        }
        pdu->securityLevel = session->securityLevel;
    }
    DEBUG_MSGTL( ( "priotBuild",
        "Building PRIOTv3 message (secName:\"%s\", secLevel:%s)...\n",
        ( ( session->securityName ) ? ( char* )session->securityName : ( ( pdu->securityName ) ? ( char* )pdu->securityName : "ERROR: undefined" ) ), _api_secLevelName[ pdu->securityLevel ] ) );

    DEBUG_DUMPSECTION( "send", "PRIOTv3 Message" );
    if ( DefaultStore_getBoolean( DsStorage_LIBRARY_ID, DsBool_REVERSE_ENCODE ) ) {
        ret = Api_v3PacketReallocRbuild( pkt, pkt_len, offset,
            session, pdu, NULL, 0 );
    } else {
        ret = Api_v3PacketBuild( session, pdu, *pkt, pkt_len, NULL, 0 );
    }
    DEBUG_INDENTLESS();
    if ( -1 != ret ) {
        session->s_snmp_errno = ret;
    }

    return ret;

} /* end Api_v3Build() */

static u_char*
_Api_v3HeaderBuild( Types_Session* session, Types_Pdu* pdu,
    u_char* packet, size_t* out_length,
    size_t length, u_char** msg_hdr_e )
{
    u_char *global_hdr, *global_hdr_e;
    u_char* cp;
    u_char msg_flags;
    long max_size;
    long sec_model;
    u_char *pb, *pb0e;

    /*
     * Save current location and build SEQUENCE tag and length placeholder
     * * for SNMP message sequence (actual length inserted later)
     */
    cp = Asn01_buildSequence( packet, out_length,
        ( u_char )( ASN01_SEQUENCE | ASN01_CONSTRUCTOR ),
        length );
    if ( cp == NULL )
        return NULL;
    if ( msg_hdr_e != NULL )
        *msg_hdr_e = cp;
    pb0e = cp;

    /*
     * store the version field - msgVersion
     */
    DEBUG_DUMPHEADER( "send", "PRIOT Version Number" );
    cp = Asn01_buildInt( cp, out_length,
        ( u_char )( ASN01_UNIVERSAL | ASN01_PRIMITIVE | ASN01_INTEGER ), ( long* )&pdu->version,
        sizeof( pdu->version ) );
    DEBUG_INDENTLESS();
    if ( cp == NULL )
        return NULL;

    global_hdr = cp;
    /*
     * msgGlobalData HeaderData
     */
    DEBUG_DUMPSECTION( "send", "msgGlobalData" );
    cp = Asn01_buildSequence( cp, out_length,
        ( u_char )( ASN01_SEQUENCE | ASN01_CONSTRUCTOR ), 0 );
    if ( cp == NULL )
        return NULL;
    global_hdr_e = cp;

    /*
     * msgID
     */
    DEBUG_DUMPHEADER( "send", "msgID" );
    cp = Asn01_buildInt( cp, out_length,
        ( u_char )( ASN01_UNIVERSAL | ASN01_PRIMITIVE | ASN01_INTEGER ), &pdu->msgid,
        sizeof( pdu->msgid ) );
    DEBUG_INDENTLESS();
    if ( cp == NULL )
        return NULL;

    /*
     * msgMaxSize
     */
    max_size = session->rcvMsgMaxSize;
    DEBUG_DUMPHEADER( "send", "msgMaxSize" );
    cp = Asn01_buildInt( cp, out_length,
        ( u_char )( ASN01_UNIVERSAL | ASN01_PRIMITIVE | ASN01_INTEGER ), &max_size,
        sizeof( max_size ) );
    DEBUG_INDENTLESS();
    if ( cp == NULL )
        return NULL;

    /*
     * msgFlags
     */
    _Api_v3CalcMsgFlags( pdu->securityLevel, pdu->command, &msg_flags );
    DEBUG_DUMPHEADER( "send", "msgFlags" );
    cp = Asn01_buildString( cp, out_length,
        ( u_char )( ASN01_UNIVERSAL | ASN01_PRIMITIVE | ASN01_OCTET_STR ), &msg_flags,
        sizeof( msg_flags ) );
    DEBUG_INDENTLESS();
    if ( cp == NULL )
        return NULL;

    /*
     * msgSecurityModel
     */
    sec_model = pdu->securityModel;
    DEBUG_DUMPHEADER( "send", "msgSecurityModel" );
    cp = Asn01_buildInt( cp, out_length,
        ( u_char )( ASN01_UNIVERSAL | ASN01_PRIMITIVE | ASN01_INTEGER ), &sec_model,
        sizeof( sec_model ) );
    DEBUG_INDENTADD( -4 ); /* return from global data indent */
    if ( cp == NULL )
        return NULL;

    /*
     * insert actual length of globalData
     */
    pb = Asn01_buildSequence( global_hdr, out_length,
        ( u_char )( ASN01_SEQUENCE | ASN01_CONSTRUCTOR ),
        cp - global_hdr_e );
    if ( pb == NULL )
        return NULL;

    /*
     * insert the actual length of the entire packet
     */
    pb = Asn01_buildSequence( packet, out_length,
        ( u_char )( ASN01_SEQUENCE | ASN01_CONSTRUCTOR ),
        length + ( cp - pb0e ) );
    if ( pb == NULL )
        return NULL;

    return cp;

} /* end Api_v3HeaderBuild() */

int Api_v3HeaderReallocRbuild( u_char** pkt, size_t* pkt_len,
    size_t* offset, Types_Session* session,
    Types_Pdu* pdu )
{
    size_t start_offset = *offset;
    u_char msg_flags;
    long max_size, sec_model;
    int rc = 0;

    /*
     * msgSecurityModel.
     */
    sec_model = pdu->securityModel;
    DEBUG_DUMPHEADER( "send", "msgSecurityModel" );
    rc = Asn01_reallocRbuildInt( pkt, pkt_len, offset, 1,
        ( u_char )( ASN01_UNIVERSAL | ASN01_PRIMITIVE | ASN01_INTEGER ), &sec_model,
        sizeof( sec_model ) );
    DEBUG_INDENTLESS();
    if ( rc == 0 ) {
        return 0;
    }

    /*
     * msgFlags.
     */
    _Api_v3CalcMsgFlags( pdu->securityLevel, pdu->command, &msg_flags );
    DEBUG_DUMPHEADER( "send", "msgFlags" );
    rc = Asn01_reallocRbuildString( pkt, pkt_len, offset, 1,
        ( u_char )( ASN01_UNIVERSAL | ASN01_PRIMITIVE
                                        | ASN01_OCTET_STR ),
        &msg_flags,
        sizeof( msg_flags ) );
    DEBUG_INDENTLESS();
    if ( rc == 0 ) {
        return 0;
    }

    /*
     * msgMaxSize.
     */
    max_size = session->rcvMsgMaxSize;
    DEBUG_DUMPHEADER( "send", "msgMaxSize" );
    rc = Asn01_reallocRbuildInt( pkt, pkt_len, offset, 1,
        ( u_char )( ASN01_UNIVERSAL | ASN01_PRIMITIVE | ASN01_INTEGER ), &max_size,
        sizeof( max_size ) );
    DEBUG_INDENTLESS();
    if ( rc == 0 ) {
        return 0;
    }

    /*
     * msgID.
     */
    DEBUG_DUMPHEADER( "send", "msgID" );
    rc = Asn01_reallocRbuildInt( pkt, pkt_len, offset, 1,
        ( u_char )( ASN01_UNIVERSAL | ASN01_PRIMITIVE | ASN01_INTEGER ), &pdu->msgid,
        sizeof( pdu->msgid ) );
    DEBUG_INDENTLESS();
    if ( rc == 0 ) {
        return 0;
    }

    /*
     * Global data sequence.
     */
    rc = Asn01_reallocRbuildSequence( pkt, pkt_len, offset, 1,
        ( u_char )( ASN01_SEQUENCE | ASN01_CONSTRUCTOR ),
        *offset - start_offset );
    if ( rc == 0 ) {
        return 0;
    }

    /*
     * Store the version field - msgVersion.
     */
    DEBUG_DUMPHEADER( "send", "PRIOT Version Number" );
    rc = Asn01_reallocRbuildInt( pkt, pkt_len, offset, 1,
        ( u_char )( ASN01_UNIVERSAL | ASN01_PRIMITIVE | ASN01_INTEGER ),
        ( long* )&pdu->version,
        sizeof( pdu->version ) );
    DEBUG_INDENTLESS();
    return rc;
} /* end Api_v3HeaderReallocRbuild() */

static u_char*
_Api_v3ScopedPDUHeaderBuild( Types_Pdu* pdu,
    u_char* packet, size_t* out_length,
    u_char** spdu_e )
{
    u_char *scopedPdu, *pb;

    pb = scopedPdu = packet;
    pb = Asn01_buildSequence( pb, out_length,
        ( u_char )( ASN01_SEQUENCE | ASN01_CONSTRUCTOR ), 0 );
    if ( pb == NULL )
        return NULL;
    if ( spdu_e )
        *spdu_e = pb;

    DEBUG_DUMPHEADER( "send", "contextEngineID" );
    pb = Asn01_buildString( pb, out_length,
        ( ASN01_UNIVERSAL | ASN01_PRIMITIVE | ASN01_OCTET_STR ),
        pdu->contextEngineID, pdu->contextEngineIDLen );
    DEBUG_INDENTLESS();
    if ( pb == NULL )
        return NULL;

    DEBUG_DUMPHEADER( "send", "contextName" );
    pb = Asn01_buildString( pb, out_length,
        ( ASN01_UNIVERSAL | ASN01_PRIMITIVE | ASN01_OCTET_STR ),
        ( u_char* )pdu->contextName,
        pdu->contextNameLen );
    DEBUG_INDENTLESS();
    if ( pb == NULL )
        return NULL;

    return pb;

} /* end Api_v3ScopedPDUHeaderBuild() */

int Api_v3ScopedPDUHeaderReallocRbuild( u_char** pkt, size_t* pkt_len,
    size_t* offset, Types_Pdu* pdu,
    size_t body_len )
{
    size_t start_offset = *offset;
    int rc = 0;

    /*
     * contextName.
     */
    DEBUG_DUMPHEADER( "send", "contextName" );
    rc = Asn01_reallocRbuildString( pkt, pkt_len, offset, 1,
        ( u_char )( ASN01_UNIVERSAL | ASN01_PRIMITIVE
                                        | ASN01_OCTET_STR ),
        ( u_char* )pdu->contextName,
        pdu->contextNameLen );
    DEBUG_INDENTLESS();
    if ( rc == 0 ) {
        return 0;
    }

    /*
     * contextEngineID.
     */
    DEBUG_DUMPHEADER( "send", "contextEngineID" );
    rc = Asn01_reallocRbuildString( pkt, pkt_len, offset, 1,
        ( u_char )( ASN01_UNIVERSAL | ASN01_PRIMITIVE
                                        | ASN01_OCTET_STR ),
        pdu->contextEngineID,
        pdu->contextEngineIDLen );
    DEBUG_INDENTLESS();
    if ( rc == 0 ) {
        return 0;
    }

    rc = Asn01_reallocRbuildSequence( pkt, pkt_len, offset, 1,
        ( u_char )( ASN01_SEQUENCE | ASN01_CONSTRUCTOR ),
        *offset - start_offset + body_len );

    return rc;
} /* end Api_v3ScopedPDUHeaderReallocRbuild() */

/*
 * returns 0 if success, -1 if fail, not 0 if SM build failure
 */
int Api_v3PacketReallocRbuild( u_char** pkt, size_t* pkt_len,
    size_t* offset, Types_Session* session,
    Types_Pdu* pdu, u_char* pdu_data,
    size_t pdu_data_len )
{
    u_char *scoped_pdu, *hdrbuf = NULL, *hdr = NULL;
    size_t hdrbuf_len = API_MAX_MSG_V3_HDRS, hdr_offset = 0, spdu_offset = 0;
    size_t body_end_offset = *offset, body_len = 0;
    struct Secmod_Def_s* sptr = NULL;
    int rc = 0;

    /*
     * Build a scopedPDU structure into the packet buffer.
     */
    DEBUGPRINTPDUTYPE( "send", pdu->command );
    if ( pdu_data ) {
        while ( ( *pkt_len - *offset ) < pdu_data_len ) {
            if ( !Asn01_realloc( pkt, pkt_len ) ) {
                return -1;
            }
        }

        *offset += pdu_data_len;
        memcpy( *pkt + *pkt_len - *offset, pdu_data, pdu_data_len );
    } else {
        rc = Api_pduReallocRbuild( pkt, pkt_len, offset, pdu );
        if ( rc == 0 ) {
            return -1;
        }
    }
    body_len = *offset - body_end_offset;

    DEBUG_DUMPSECTION( "send", "ScopedPdu" );
    rc = Api_v3ScopedPDUHeaderReallocRbuild( pkt, pkt_len, offset,
        pdu, body_len );
    if ( rc == 0 ) {
        return -1;
    }
    spdu_offset = *offset;
    DEBUG_INDENTADD( -4 ); /*  Return from Scoped PDU.  */

    if ( ( hdrbuf = ( u_char* )malloc( hdrbuf_len ) ) == NULL ) {
        return -1;
    }

    rc = Api_v3HeaderReallocRbuild( &hdrbuf, &hdrbuf_len, &hdr_offset,
        session, pdu );
    if ( rc == 0 ) {
        MEMORY_FREE( hdrbuf );
        return -1;
    }
    hdr = hdrbuf + hdrbuf_len - hdr_offset;
    scoped_pdu = *pkt + *pkt_len - spdu_offset;

    /*
     * Call the security module to possibly encrypt and authenticate the
     * message---the entire message to transmitted on the wire is returned.
     */

    sptr = Secmod_find( pdu->securityModel );
    DEBUG_DUMPSECTION( "send", "SM msgSecurityParameters" );
    if ( sptr && sptr->encode_reverse ) {
        struct Secmod_OutgoingParams_s parms;

        parms.msgProcModel = pdu->msgParseModel;
        parms.globalData = hdr;
        parms.globalDataLen = hdr_offset;
        parms.maxMsgSize = API_MAX_MSG_SIZE;
        parms.secModel = pdu->securityModel;
        parms.secEngineID = pdu->securityEngineID;
        parms.secEngineIDLen = pdu->securityEngineIDLen;
        parms.secName = pdu->securityName;
        parms.secNameLen = pdu->securityNameLen;
        parms.secLevel = pdu->securityLevel;
        parms.scopedPdu = scoped_pdu;
        parms.scopedPduLen = spdu_offset;
        parms.secStateRef = pdu->securityStateRef;
        parms.wholeMsg = pkt;
        parms.wholeMsgLen = pkt_len;
        parms.wholeMsgOffset = offset;
        parms.session = session;
        parms.pdu = pdu;

        rc = ( *sptr->encode_reverse )( &parms );
    } else {
        if ( !sptr ) {
            Logger_log( LOGGER_PRIORITY_ERR,
                "no such security service available: %d\n",
                pdu->securityModel );
        } else if ( !sptr->encode_reverse ) {
            Logger_log( LOGGER_PRIORITY_ERR,
                "security service %d doesn't support reverse encoding.\n",
                pdu->securityModel );
        }
        rc = -1;
    }

    DEBUG_INDENTLESS();
    MEMORY_FREE( hdrbuf );
    return rc;
} /* end Api_v3PacketReallocRbuild() */

/*
 * returns 0 if success, -1 if fail, not 0 if SM build failure
 */
int Api_v3PacketBuild( Types_Session* session, Types_Pdu* pdu,
    u_char* packet, size_t* out_length,
    u_char* pdu_data, size_t pdu_data_len )
{
    u_char *global_data, *sec_params, *spdu_hdr_e;
    size_t global_data_len, sec_params_len;
    u_char spdu_buf[ API_MAX_MSG_SIZE ];
    size_t spdu_buf_len, spdu_len;
    u_char* cp;
    int result;
    struct Secmod_Def_s* sptr;

    global_data = packet;

    /*
     * build the headers for the packet, returned addr = start of secParams
     */
    sec_params = _Api_v3HeaderBuild( session, pdu, global_data,
        out_length, 0, NULL );
    if ( sec_params == NULL )
        return -1;
    global_data_len = sec_params - global_data;
    sec_params_len = *out_length; /* length left in packet buf for sec_params */

    /*
     * build a scopedPDU structure into spdu_buf
     */
    spdu_buf_len = API_MAX_MSG_SIZE;
    DEBUG_DUMPSECTION( "send", "ScopedPdu" );
    cp = _Api_v3ScopedPDUHeaderBuild( pdu, spdu_buf, &spdu_buf_len,
        &spdu_hdr_e );
    if ( cp == NULL )
        return -1;

    /*
     * build the PDU structure onto the end of spdu_buf
     */
    DEBUGPRINTPDUTYPE( "send", ( ( pdu_data ) ? *pdu_data : 0x00 ) );
    if ( pdu_data ) {
        memcpy( cp, pdu_data, pdu_data_len );
        cp += pdu_data_len;
    } else {
        cp = Api_pduBuild( pdu, cp, &spdu_buf_len );
        if ( cp == NULL )
            return -1;
    }
    DEBUG_INDENTADD( -4 ); /* return from Scoped PDU */

    /*
     * re-encode the actual ASN.1 length of the scopedPdu
     */
    spdu_len = cp - spdu_hdr_e; /* length of scopedPdu minus ASN.1 headers */
    spdu_buf_len = API_MAX_MSG_SIZE;
    if ( Asn01_buildSequence( spdu_buf, &spdu_buf_len,
             ( u_char )( ASN01_SEQUENCE | ASN01_CONSTRUCTOR ),
             spdu_len )
        == NULL )
        return -1;
    spdu_len = cp - spdu_buf; /* the length of the entire scopedPdu */

    /*
     * call the security module to possibly encrypt and authenticate the
     * message - the entire message to transmitted on the wire is returned
     */
    cp = NULL;
    *out_length = API_MAX_MSG_SIZE;
    DEBUG_DUMPSECTION( "send", "SM msgSecurityParameters" );
    sptr = Secmod_find( pdu->securityModel );
    if ( sptr && sptr->encode_forward ) {
        struct Secmod_OutgoingParams_s parms;
        parms.msgProcModel = pdu->msgParseModel;
        parms.globalData = global_data;
        parms.globalDataLen = global_data_len;
        parms.maxMsgSize = API_MAX_MSG_SIZE;
        parms.secModel = pdu->securityModel;
        parms.secEngineID = pdu->securityEngineID;
        parms.secEngineIDLen = pdu->securityEngineIDLen;
        parms.secName = pdu->securityName;
        parms.secNameLen = pdu->securityNameLen;
        parms.secLevel = pdu->securityLevel;
        parms.scopedPdu = spdu_buf;
        parms.scopedPduLen = spdu_len;
        parms.secStateRef = pdu->securityStateRef;
        parms.secParams = sec_params;
        parms.secParamsLen = &sec_params_len;
        parms.wholeMsg = &cp;
        parms.wholeMsgLen = out_length;
        parms.session = session;
        parms.pdu = pdu;
        result = ( *sptr->encode_forward )( &parms );
    } else {
        if ( !sptr ) {
            Logger_log( LOGGER_PRIORITY_ERR, "no such security service available: %d\n",
                pdu->securityModel );
        } else if ( !sptr->encode_forward ) {
            Logger_log( LOGGER_PRIORITY_ERR,
                "security service %d doesn't support forward out encoding.\n",
                pdu->securityModel );
        }
        result = -1;
    }
    DEBUG_INDENTLESS();
    return result;

} /* end Api_v3PacketBuild() */

/*
 * Takes a session and a pdu and serializes the ASN PDU into the area
 * pointed to by *pkt.  *pkt_len is the size of the data area available.
 * Returns the length of the completed packet in *offset.  If any errors
 * occur, -1 is returned.  If all goes well, 0 is returned.
 */

static int
_Api_build( u_char** pkt, size_t* pkt_len, size_t* offset,
    Types_Session* session, Types_Pdu* pdu )
{

    u_char* cp;
    size_t length;

    session->s_snmp_errno = 0;
    session->s_errno = 0;

    if ( pdu->version == PRIOT_VERSION_3 ) {
        return _Api_v3Build( pkt, pkt_len, offset, session, pdu );
    }

    switch ( pdu->command ) {
    case PRIOT_MSG_RESPONSE:
        Assert_assert( 0 == ( pdu->flags & PRIOT_UCD_MSG_FLAG_EXPECT_RESPONSE ) );
    /*
         * Fallthrough
         */
    case PRIOT_MSG_GET:
    case PRIOT_MSG_GETNEXT:
    case PRIOT_MSG_SET:
        /*
         * all versions support these PDU types
         */
        /*
         * initialize defaulted PDU fields
         */

        if ( pdu->errstat == API_DEFAULT_ERRSTAT )
            pdu->errstat = 0;
        if ( pdu->errindex == API_DEFAULT_ERRINDEX )
            pdu->errindex = 0;
        break;

    case PRIOT_MSG_TRAP2:
        Assert_assert( 0 == ( pdu->flags & PRIOT_UCD_MSG_FLAG_EXPECT_RESPONSE ) );
    /*
         * Fallthrough
         */
    case PRIOT_MSG_INFORM:
        if ( pdu->errstat == API_DEFAULT_ERRSTAT )
            pdu->errstat = 0;
        if ( pdu->errindex == API_DEFAULT_ERRINDEX )
            pdu->errindex = 0;
        break;

    case PRIOT_MSG_GETBULK:

        if ( pdu->max_repetitions < 0 ) {
            session->s_snmp_errno = ErrorCode_BAD_REPETITIONS;
            return -1;
        }
        if ( pdu->non_repeaters < 0 ) {
            session->s_snmp_errno = ErrorCode_BAD_REPEATERS;
            return -1;
        }
        break;

    case PRIOT_MSG_TRAP:
        /*
         * initialize defaulted Trap PDU fields
         */
        pdu->reqid = 1; /* give a bogus non-error reqid for traps */
        if ( pdu->enterpriseLength == API_DEFAULT_ENTERPRISE_LENGTH ) {
            pdu->enterprise = ( oid* )malloc( sizeof( DEFAULT_ENTERPRISE ) );
            if ( pdu->enterprise == NULL ) {
                session->s_snmp_errno = ErrorCode_MALLOC;
                return -1;
            }
            memmove( pdu->enterprise, DEFAULT_ENTERPRISE,
                sizeof( DEFAULT_ENTERPRISE ) );
            pdu->enterpriseLength = sizeof( DEFAULT_ENTERPRISE ) / sizeof( oid );
        }
        if ( pdu->time == API_DEFAULT_TIME )
            pdu->time = DEFAULT_TIME;
        /*
         * don't expect a response
         */
        pdu->flags &= ( ~PRIOT_UCD_MSG_FLAG_EXPECT_RESPONSE );
        break;

    case PRIOT_MSG_REPORT: /* SNMPv3 only */
    default:
        session->s_snmp_errno = ErrorCode_UNKNOWN_PDU;
        return -1;
    }

    /*
     * save length
     */
    length = *pkt_len;

    /*
     * setup administrative fields based on version
     */
    /*
     * build the message wrapper and all the administrative fields
     * upto the PDU sequence
     * (note that actual length of message will be inserted later)
     */
    switch ( pdu->version ) {
    default:
        session->s_snmp_errno = ErrorCode_BAD_VERSION;
        return -1;
    }

    DEBUGPRINTPDUTYPE( "send", pdu->command );
    cp = Api_pduBuild( pdu, cp, pkt_len );
    DEBUG_INDENTADD( -4 ); /* return from entire v1/v2c message */
    if ( cp == NULL )
        return -1;

    /*
     * insert the actual length of the message sequence
     */
    switch ( pdu->version ) {
    default:
        session->s_snmp_errno = ErrorCode_BAD_VERSION;
        return -1;
    }
    *pkt_len = cp - *pkt;
    return 0;
}

int Api_build( u_char** pkt, size_t* pkt_len, size_t* offset,
    Types_Session* pss, Types_Pdu* pdu )
{
    int rc;
    rc = _Api_build( pkt, pkt_len, offset, pss, pdu );
    if ( rc ) {
        if ( !pss->s_snmp_errno ) {
            Logger_log( LOGGER_PRIORITY_ERR, "Api_build: unknown failure\n" );
            pss->s_snmp_errno = ErrorCode_BAD_ASN1_BUILD;
        }
        API_SET_PRIOT_ERROR( pss->s_snmp_errno );
        rc = -1;
    }
    return rc;
}

/*
 * on error, returns NULL (likely an encoding problem).
 */
u_char*
Api_pduBuild( Types_Pdu* pdu, u_char* cp, size_t* out_length )
{
    u_char *h1, *h1e, *h2, *h2e;
    Types_VariableList* vp;
    size_t length;

    length = *out_length;
    /*
     * Save current location and build PDU tag and length placeholder
     * (actual length will be inserted later)
     */
    h1 = cp;
    cp = Asn01_buildSequence( cp, out_length, ( u_char )pdu->command, 0 );
    if ( cp == NULL )
        return NULL;
    h1e = cp;

    /*
     * store fields in the PDU preceeding the variable-bindings sequence
     */
    if ( pdu->command != PRIOT_MSG_TRAP ) {
        /*
         * PDU is not an SNMPv1 trap
         */

        DEBUG_DUMPHEADER( "send", "request_id" );
        /*
         * request id
         */
        cp = Asn01_buildInt( cp, out_length,
            ( u_char )( ASN01_UNIVERSAL | ASN01_PRIMITIVE | ASN01_INTEGER ), &pdu->reqid,
            sizeof( pdu->reqid ) );
        DEBUG_INDENTLESS();
        if ( cp == NULL )
            return NULL;

        /*
         * error status (getbulk non-repeaters)
         */
        DEBUG_DUMPHEADER( "send", "error status" );
        cp = Asn01_buildInt( cp, out_length,
            ( u_char )( ASN01_UNIVERSAL | ASN01_PRIMITIVE | ASN01_INTEGER ), &pdu->errstat,
            sizeof( pdu->errstat ) );
        DEBUG_INDENTLESS();
        if ( cp == NULL )
            return NULL;

        /*
         * error index (getbulk max-repetitions)
         */
        DEBUG_DUMPHEADER( "send", "error index" );
        cp = Asn01_buildInt( cp, out_length,
            ( u_char )( ASN01_UNIVERSAL | ASN01_PRIMITIVE | ASN01_INTEGER ), &pdu->errindex,
            sizeof( pdu->errindex ) );
        DEBUG_INDENTLESS();
        if ( cp == NULL )
            return NULL;
    } else {
        /*
         * an SNMPv1 trap PDU
         */

        /*
         * enterprise
         */
        DEBUG_DUMPHEADER( "send", "enterprise OBJID" );
        cp = Asn01_buildObjid( cp, out_length,
            ( u_char )( ASN01_UNIVERSAL | ASN01_PRIMITIVE | ASN01_OBJECT_ID ),
            ( oid* )pdu->enterprise,
            pdu->enterpriseLength );
        DEBUG_INDENTLESS();
        if ( cp == NULL )
            return NULL;

        /*
         * agent-addr
         */
        DEBUG_DUMPHEADER( "send", "agent Address" );
        cp = Asn01_buildString( cp, out_length,
            ( u_char )( ASN01_IPADDRESS | ASN01_PRIMITIVE ),
            ( u_char* )pdu->agentAddr, 4 );
        DEBUG_INDENTLESS();
        if ( cp == NULL )
            return NULL;

        /*
         * generic trap
         */
        DEBUG_DUMPHEADER( "send", "generic trap number" );
        cp = Asn01_buildInt( cp, out_length,
            ( u_char )( ASN01_UNIVERSAL | ASN01_PRIMITIVE | ASN01_INTEGER ),
            ( long* )&pdu->trapType,
            sizeof( pdu->trapType ) );
        DEBUG_INDENTLESS();
        if ( cp == NULL )
            return NULL;

        /*
         * specific trap
         */
        DEBUG_DUMPHEADER( "send", "specific trap number" );
        cp = Asn01_buildInt( cp, out_length,
            ( u_char )( ASN01_UNIVERSAL | ASN01_PRIMITIVE | ASN01_INTEGER ),
            ( long* )&pdu->specificType,
            sizeof( pdu->specificType ) );
        DEBUG_INDENTLESS();
        if ( cp == NULL )
            return NULL;

        /*
         * timestamp
         */
        DEBUG_DUMPHEADER( "send", "timestamp" );
        cp = Asn01_buildUnsignedInt( cp, out_length,
            ( u_char )( ASN01_TIMETICKS | ASN01_PRIMITIVE ), &pdu->time,
            sizeof( pdu->time ) );
        DEBUG_INDENTLESS();
        if ( cp == NULL )
            return NULL;
    }

    /*
     * Save current location and build SEQUENCE tag and length placeholder
     * for variable-bindings sequence
     * (actual length will be inserted later)
     */
    h2 = cp;
    cp = Asn01_buildSequence( cp, out_length,
        ( u_char )( ASN01_SEQUENCE | ASN01_CONSTRUCTOR ), 0 );
    if ( cp == NULL )
        return NULL;
    h2e = cp;

    /*
     * Store variable-bindings
     */
    DEBUG_DUMPSECTION( "send", "VarBindList" );
    for ( vp = pdu->variables; vp; vp = vp->nextVariable ) {
        DEBUG_DUMPSECTION( "send", "VarBind" );
        cp = Priot_buildVarOp( cp, vp->name, &vp->nameLength, vp->type,
            vp->valLen, ( u_char* )vp->val.string,
            out_length );
        DEBUG_INDENTLESS();
        if ( cp == NULL )
            return NULL;
    }
    DEBUG_INDENTLESS();

    /*
     * insert actual length of variable-bindings sequence
     */
    Asn01_buildSequence( h2, &length,
        ( u_char )( ASN01_SEQUENCE | ASN01_CONSTRUCTOR ),
        cp - h2e );

    /*
     * insert actual length of PDU sequence
     */
    Asn01_buildSequence( h1, &length, ( u_char )pdu->command, cp - h1e );

    return cp;
}

/*
 * On error, returns 0 (likely an encoding problem).
 */
int Api_pduReallocRbuild( u_char** pkt, size_t* pkt_len, size_t* offset,
    Types_Pdu* pdu )
{
#define VPCACHE_SIZE 50
    Types_VariableList* vpcache[ VPCACHE_SIZE ];
    Types_VariableList *vp, *tmpvp;
    size_t start_offset = *offset;
    int i, wrapped = 0, notdone, final, rc = 0;

    DEBUG_MSGTL( ( "priotPduReallocRbuild", "starting\n" ) );
    for ( vp = pdu->variables, i = VPCACHE_SIZE - 1; vp;
          vp = vp->nextVariable, i-- ) {
        if ( i < 0 ) {
            wrapped = notdone = 1;
            i = VPCACHE_SIZE - 1;
            DEBUG_MSGTL( ( "priotPduReallocRbuild", "wrapped\n" ) );
        }
        vpcache[ i ] = vp;
    }
    final = i + 1;

    do {
        for ( i = final; i < VPCACHE_SIZE; i++ ) {
            vp = vpcache[ i ];
            DEBUG_DUMPSECTION( "send", "VarBind" );
            rc = Priot_reallocRbuildVarOp( pkt, pkt_len, offset, 1,
                vp->name, &vp->nameLength,
                vp->type,
                ( u_char* )vp->val.string,
                vp->valLen );
            DEBUG_INDENTLESS();
            if ( rc == 0 ) {
                return 0;
            }
        }

        DEBUG_INDENTLESS();
        if ( wrapped ) {
            notdone = 1;
            for ( i = 0; i < final; i++ ) {
                vp = vpcache[ i ];
                DEBUG_DUMPSECTION( "send", "VarBind" );
                rc = Priot_reallocRbuildVarOp( pkt, pkt_len, offset, 1,
                    vp->name, &vp->nameLength,
                    vp->type,
                    ( u_char* )vp->val.string,
                    vp->valLen );
                DEBUG_INDENTLESS();
                if ( rc == 0 ) {
                    return 0;
                }
            }

            if ( final == 0 ) {
                tmpvp = vpcache[ VPCACHE_SIZE - 1 ];
            } else {
                tmpvp = vpcache[ final - 1 ];
            }
            wrapped = 0;

            for ( vp = pdu->variables, i = VPCACHE_SIZE - 1;
                  vp && vp != tmpvp; vp = vp->nextVariable, i-- ) {
                if ( i < 0 ) {
                    wrapped = 1;
                    i = VPCACHE_SIZE - 1;
                    DEBUG_MSGTL( ( "priotPduReallocRbuild", "wrapped\n" ) );
                }
                vpcache[ i ] = vp;
            }
            final = i + 1;
        } else {
            notdone = 0;
        }
    } while ( notdone );

    /*
     * Save current location and build SEQUENCE tag and length placeholder for
     * variable-bindings sequence (actual length will be inserted later).
     */

    rc = Asn01_reallocRbuildSequence( pkt, pkt_len, offset, 1,
        ( u_char )( ASN01_SEQUENCE | ASN01_CONSTRUCTOR ),
        *offset - start_offset );

    /*
     * Store fields in the PDU preceeding the variable-bindings sequence.
     */
    if ( pdu->command != PRIOT_MSG_TRAP ) {
        /*
         * Error index (getbulk max-repetitions).
         */
        DEBUG_DUMPHEADER( "send", "error index" );
        rc = Asn01_reallocRbuildInt( pkt, pkt_len, offset, 1,
            ( u_char )( ASN01_UNIVERSAL | ASN01_PRIMITIVE
                                         | ASN01_INTEGER ),
            &pdu->errindex, sizeof( pdu->errindex ) );
        DEBUG_INDENTLESS();
        if ( rc == 0 ) {
            return 0;
        }

        /*
         * Error status (getbulk non-repeaters).
         */
        DEBUG_DUMPHEADER( "send", "error status" );
        rc = Asn01_reallocRbuildInt( pkt, pkt_len, offset, 1,
            ( u_char )( ASN01_UNIVERSAL | ASN01_PRIMITIVE
                                         | ASN01_INTEGER ),
            &pdu->errstat, sizeof( pdu->errstat ) );
        DEBUG_INDENTLESS();
        if ( rc == 0 ) {
            return 0;
        }

        /*
         * Request ID.
         */
        DEBUG_DUMPHEADER( "send", "request_id" );
        rc = Asn01_reallocRbuildInt( pkt, pkt_len, offset, 1,
            ( u_char )( ASN01_UNIVERSAL | ASN01_PRIMITIVE
                                         | ASN01_INTEGER ),
            &pdu->reqid,
            sizeof( pdu->reqid ) );
        DEBUG_INDENTLESS();
        if ( rc == 0 ) {
            return 0;
        }
    } else {
        /*
         * An SNMPv1 trap PDU.
         */

        /*
         * Timestamp.
         */
        DEBUG_DUMPHEADER( "send", "timestamp" );
        rc = Asn01_reallocRbuildUnsignedInt( pkt, pkt_len, offset, 1,
            ( u_char )( ASN01_TIMETICKS | ASN01_PRIMITIVE ),
            &pdu->time,
            sizeof( pdu->time ) );
        DEBUG_INDENTLESS();
        if ( rc == 0 ) {
            return 0;
        }

        /*
         * Specific trap.
         */
        DEBUG_DUMPHEADER( "send", "specific trap number" );
        rc = Asn01_reallocRbuildInt( pkt, pkt_len, offset, 1,
            ( u_char )( ASN01_UNIVERSAL | ASN01_PRIMITIVE
                                         | ASN01_INTEGER ),
            ( long* )&pdu->specificType,
            sizeof( pdu->specificType ) );
        DEBUG_INDENTLESS();
        if ( rc == 0 ) {
            return 0;
        }

        /*
         * Generic trap.
         */
        DEBUG_DUMPHEADER( "send", "generic trap number" );
        rc = Asn01_reallocRbuildInt( pkt, pkt_len, offset, 1,
            ( u_char )( ASN01_UNIVERSAL | ASN01_PRIMITIVE
                                         | ASN01_INTEGER ),
            ( long* )&pdu->trapType,
            sizeof( pdu->trapType ) );
        DEBUG_INDENTLESS();
        if ( rc == 0 ) {
            return 0;
        }

        /*
         * Agent-addr.
         */
        DEBUG_DUMPHEADER( "send", "agent Address" );
        rc = Asn01_reallocRbuildString( pkt, pkt_len, offset, 1,
            ( u_char )( ASN01_IPADDRESS | ASN01_PRIMITIVE ),
            ( u_char* )pdu->agentAddr, 4 );
        DEBUG_INDENTLESS();
        if ( rc == 0 ) {
            return 0;
        }

        /*
         * Enterprise.
         */
        DEBUG_DUMPHEADER( "send", "enterprise OBJID" );
        rc = Asn01_reallocRbuildObjid( pkt, pkt_len, offset, 1,
            ( u_char )( ASN01_UNIVERSAL | ASN01_PRIMITIVE | ASN01_OBJECT_ID ),
            ( oid* )pdu->enterprise,
            pdu->enterpriseLength );
        DEBUG_INDENTLESS();
        if ( rc == 0 ) {
            return 0;
        }
    }

    /*
     * Build the PDU sequence.
     */
    rc = Asn01_reallocRbuildSequence( pkt, pkt_len, offset, 1,
        ( u_char )pdu->command,
        *offset - start_offset );
    return rc;
}

/*
 * Parses the packet received to determine version, either directly
 * from packets version field or inferred from ASN.1 construct.
 */
static int
_Api_parseVersion( u_char* data, size_t length )
{
    u_char type;
    long version = ErrorCode_BAD_VERSION;

    data = Asn01_parseSequence( data, &length, &type,
        ( ASN01_SEQUENCE | ASN01_CONSTRUCTOR ), "version" );
    if ( data ) {
        DEBUG_DUMPHEADER( "recv", "PRIOT Version" );
        data = Asn01_parseInt( data, &length, &type, &version, sizeof( version ) );
        DEBUG_INDENTLESS();
        if ( !data || type != ASN01_INTEGER ) {
            return ErrorCode_BAD_VERSION;
        }
    }
    return version;
}

int Api_v3Parse( Types_Pdu* pdu,
    u_char* data,
    size_t* length,
    u_char** after_header, Types_Session* sess )
{
    u_char type, msg_flags;
    long ver, msg_max_size, msg_sec_model;
    size_t max_size_response;
    u_char tmp_buf[ API_MAX_MSG_SIZE ];
    size_t tmp_buf_len;
    u_char pdu_buf[ API_MAX_MSG_SIZE ];
    u_char* mallocbuf = NULL;
    size_t pdu_buf_len = API_MAX_MSG_SIZE;
    u_char* sec_params;
    u_char* msg_data;
    u_char* cp;
    size_t asn_len, msg_len;
    int ret, ret_val;
    struct Secmod_Def_s* sptr;

    msg_data = data;
    msg_len = *length;

    /*
     * message is an ASN.1 SEQUENCE
     */
    DEBUG_DUMPSECTION( "recv", "PRIOTv3 Message" );
    data = Asn01_parseSequence( data, length, &type,
        ( ASN01_SEQUENCE | ASN01_CONSTRUCTOR ), "message" );
    if ( data == NULL ) {
        /*
         * error msg detail is set
         */
        Api_incrementStatistic( API_STAT_SNMPINASNPARSEERRS );
        DEBUG_INDENTLESS();
        return ErrorCode_ASN_PARSE_ERR;
    }

    /*
     * parse msgVersion
     */
    DEBUG_DUMPHEADER( "recv", "PRIOT Version Number" );
    data = Asn01_parseInt( data, length, &type, &ver, sizeof( ver ) );
    DEBUG_INDENTLESS();
    if ( data == NULL ) {
        IMPL_ERROR_MSG( "bad parse of version" );
        Api_incrementStatistic( API_STAT_SNMPINASNPARSEERRS );
        DEBUG_INDENTLESS();
        return ErrorCode_ASN_PARSE_ERR;
    }
    pdu->version = ver;

    /*
     * parse msgGlobalData sequence
     */
    cp = data;
    asn_len = *length;
    DEBUG_DUMPSECTION( "recv", "msgGlobalData" );
    data = Asn01_parseSequence( data, &asn_len, &type,
        ( ASN01_SEQUENCE | ASN01_CONSTRUCTOR ),
        "msgGlobalData" );
    if ( data == NULL ) {
        /*
         * error msg detail is set
         */
        Api_incrementStatistic( API_STAT_SNMPINASNPARSEERRS );
        DEBUG_INDENTADD( -4 );
        return ErrorCode_ASN_PARSE_ERR;
    }
    *length -= data - cp; /* subtract off the length of the header */

    /*
     * msgID
     */
    DEBUG_DUMPHEADER( "recv", "msgID" );
    data = Asn01_parseInt( data, length, &type, &pdu->msgid,
        sizeof( pdu->msgid ) );
    DEBUG_INDENTLESS();
    if ( data == NULL || type != ASN01_INTEGER ) {
        IMPL_ERROR_MSG( "error parsing msgID" );
        DEBUG_INDENTADD( -4 );
        Api_incrementStatistic( API_STAT_SNMPINASNPARSEERRS );
        return ErrorCode_ASN_PARSE_ERR;
    }

    /*
     * Check the msgID we received is a legal value.  If not, then increment
     * snmpInASNParseErrs and return the appropriate error (see RFC 2572,
     * para. 7.2, section 2 -- note that a bad msgID means that the received
     * message is NOT a serialiization of an SNMPv3Message, since the msgID
     * field is out of bounds).
     */

    if ( pdu->msgid < 0 || pdu->msgid > 0x7fffffff ) {
        Logger_log( LOGGER_PRIORITY_ERR, "Received bad msgID (%ld %s %s).\n", pdu->msgid,
            ( pdu->msgid < 0 ) ? "<" : ">",
            ( pdu->msgid < 0 ) ? "0" : "2^31 - 1" );
        Api_incrementStatistic( API_STAT_SNMPINASNPARSEERRS );
        DEBUG_INDENTADD( -4 );
        return ErrorCode_ASN_PARSE_ERR;
    }

    /*
     * msgMaxSize
     */
    DEBUG_DUMPHEADER( "recv", "msgMaxSize" );
    data = Asn01_parseInt( data, length, &type, &msg_max_size,
        sizeof( msg_max_size ) );
    DEBUG_INDENTLESS();
    if ( data == NULL || type != ASN01_INTEGER ) {
        IMPL_ERROR_MSG( "error parsing msgMaxSize" );
        Api_incrementStatistic( API_STAT_SNMPINASNPARSEERRS );
        DEBUG_INDENTADD( -4 );
        return ErrorCode_ASN_PARSE_ERR;
    }

    /*
     * Check the msgMaxSize we received is a legal value.  If not, then
     * increment snmpInASNParseErrs and return the appropriate error (see RFC
     * 2572, para. 7.2, section 2 -- note that a bad msgMaxSize means that the
     * received message is NOT a serialiization of an SNMPv3Message, since the
     * msgMaxSize field is out of bounds).
     *
     * Note we store the msgMaxSize on a per-session basis which also seems
     * reasonable; it could vary from PDU to PDU but that would be strange
     * (also since we deal with a PDU at a time, it wouldn't make any
     * difference to our responses, if any).
     */

    if ( msg_max_size < 484 ) {
        Logger_log( LOGGER_PRIORITY_ERR, "Received bad msgMaxSize (%lu < 484).\n",
            msg_max_size );
        Api_incrementStatistic( API_STAT_SNMPINASNPARSEERRS );
        DEBUG_INDENTADD( -4 );
        return ErrorCode_ASN_PARSE_ERR;
    } else if ( msg_max_size > 0x7fffffff ) {
        Logger_log( LOGGER_PRIORITY_ERR, "Received bad msgMaxSize (%lu > 2^31 - 1).\n",
            msg_max_size );
        Api_incrementStatistic( API_STAT_SNMPINASNPARSEERRS );
        DEBUG_INDENTADD( -4 );
        return ErrorCode_ASN_PARSE_ERR;
    } else {
        DEBUG_MSGTL( ( "priotV3Parse", "msgMaxSize %lu received\n",
            msg_max_size ) );
        sess->sndMsgMaxSize = msg_max_size;
    }

    /*
     * msgFlags
     */
    tmp_buf_len = API_MAX_MSG_SIZE;
    DEBUG_DUMPHEADER( "recv", "msgFlags" );
    data = Asn01_parseString( data, length, &type, tmp_buf, &tmp_buf_len );
    DEBUG_INDENTLESS();
    if ( data == NULL || type != ASN01_OCTET_STR || tmp_buf_len != 1 ) {
        IMPL_ERROR_MSG( "error parsing msgFlags" );
        Api_incrementStatistic( API_STAT_SNMPINASNPARSEERRS );
        DEBUG_INDENTADD( -4 );
        return ErrorCode_ASN_PARSE_ERR;
    }
    msg_flags = *tmp_buf;
    if ( msg_flags & PRIOT_MSG_FLAG_RPRT_BIT )
        pdu->flags |= PRIOT_MSG_FLAG_RPRT_BIT;
    else
        pdu->flags &= ( ~PRIOT_MSG_FLAG_RPRT_BIT );

    /*
     * msgSecurityModel
     */
    DEBUG_DUMPHEADER( "recv", "msgSecurityModel" );
    data = Asn01_parseInt( data, length, &type, &msg_sec_model,
        sizeof( msg_sec_model ) );
    DEBUG_INDENTADD( -4 ); /* return from global data indent */
    if ( data == NULL || type != ASN01_INTEGER || msg_sec_model < 1 || msg_sec_model > 0x7fffffff ) {
        IMPL_ERROR_MSG( "error parsing msgSecurityModel" );
        Api_incrementStatistic( API_STAT_SNMPINASNPARSEERRS );
        DEBUG_INDENTLESS();
        return ErrorCode_ASN_PARSE_ERR;
    }
    sptr = Secmod_find( msg_sec_model );
    if ( !sptr ) {
        Logger_log( LOGGER_PRIORITY_WARNING, "unknown security model: %ld\n",
            msg_sec_model );
        Api_incrementStatistic( API_STAT_SNMPUNKNOWNSECURITYMODELS );
        DEBUG_INDENTLESS();
        return ErrorCode_UNKNOWN_SEC_MODEL;
    }
    pdu->securityModel = msg_sec_model;

    if ( msg_flags & PRIOT_MSG_FLAG_PRIV_BIT && !( msg_flags & PRIOT_MSG_FLAG_AUTH_BIT ) ) {
        IMPL_ERROR_MSG( "invalid message, illegal msgFlags" );
        Api_incrementStatistic( API_STAT_SNMPINVALIDMSGS );
        DEBUG_INDENTLESS();
        return ErrorCode_INVALID_MSG;
    }
    pdu->securityLevel = ( ( msg_flags & PRIOT_MSG_FLAG_AUTH_BIT )
            ? ( ( msg_flags & PRIOT_MSG_FLAG_PRIV_BIT )
                      ? PRIOT_SEC_LEVEL_AUTHPRIV
                      : PRIOT_SEC_LEVEL_AUTHNOPRIV )
            : PRIOT_SEC_LEVEL_NOAUTH );
    /*
     * end of msgGlobalData
     */

    /*
     * securtityParameters OCTET STRING begins after msgGlobalData
     */
    sec_params = data;
    pdu->contextEngineID = ( u_char* )calloc( 1, API_MAX_ENG_SIZE );
    pdu->contextEngineIDLen = API_MAX_ENG_SIZE;

    /*
     * Note: there is no length limit on the msgAuthoritativeEngineID field,
     * although we would EXPECT it to be limited to 32 (the SnmpEngineID TC
     * limit).  We'll use double that here to be on the safe side.
     */

    pdu->securityEngineID = ( u_char* )calloc( 1, API_MAX_ENG_SIZE * 2 );
    pdu->securityEngineIDLen = API_MAX_ENG_SIZE * 2;
    pdu->securityName = ( char* )calloc( 1, API_MAX_SEC_NAME_SIZE );
    pdu->securityNameLen = API_MAX_SEC_NAME_SIZE;

    if ( ( pdu->securityName == NULL ) || ( pdu->securityEngineID == NULL ) || ( pdu->contextEngineID == NULL ) ) {
        return ErrorCode_MALLOC;
    }

    if ( pdu_buf_len < msg_len
        && pdu->securityLevel == PRIOT_SEC_LEVEL_AUTHPRIV ) {
        /*
         * space needed is larger than we have in the default buffer
         */
        mallocbuf = ( u_char* )calloc( 1, msg_len );
        pdu_buf_len = msg_len;
        cp = mallocbuf;
    } else {
        memset( pdu_buf, 0, pdu_buf_len );
        cp = pdu_buf;
    }

    DEBUG_DUMPSECTION( "recv", "SM msgSecurityParameters" );
    if ( sptr->decode ) {
        struct Secmod_IncomingParams_s parms;
        parms.msgProcModel = pdu->msgParseModel;
        parms.maxMsgSize = msg_max_size;
        parms.secParams = sec_params;
        parms.secModel = msg_sec_model;
        parms.secLevel = pdu->securityLevel;
        parms.wholeMsg = msg_data;
        parms.wholeMsgLen = msg_len;
        parms.secEngineID = pdu->securityEngineID;
        parms.secEngineIDLen = &pdu->securityEngineIDLen;
        parms.secName = pdu->securityName;
        parms.secNameLen = &pdu->securityNameLen;
        parms.scopedPdu = &cp;
        parms.scopedPduLen = &pdu_buf_len;
        parms.maxSizeResponse = &max_size_response;
        parms.secStateRef = &pdu->securityStateRef;
        parms.sess = sess;
        parms.pdu = pdu;
        parms.msg_flags = msg_flags;
        ret_val = ( *sptr->decode )( &parms );
    } else {
        MEMORY_FREE( mallocbuf );
        DEBUG_INDENTLESS();
        Logger_log( LOGGER_PRIORITY_WARNING, "security service %ld can't decode packets\n",
            msg_sec_model );
        return ( -1 );
    }

    if ( ret_val != ErrorCode_SUCCESS ) {
        DEBUG_DUMPSECTION( "recv", "ScopedPDU" );
        /*
         * Parse as much as possible -- though I don't see the point? [jbpn].
         */
        if ( cp ) {
            cp = Api_v3ScopedPduParse( pdu, cp, &pdu_buf_len );
        }
        if ( cp ) {
            DEBUGPRINTPDUTYPE( "recv", *cp );
            Api_pduParse( pdu, cp, &pdu_buf_len );
            DEBUG_INDENTADD( -8 );
        } else {
            DEBUG_INDENTADD( -4 );
        }

        MEMORY_FREE( mallocbuf );
        return ret_val;
    }

    /*
     * parse plaintext ScopedPDU sequence
     */
    *length = pdu_buf_len;
    DEBUG_DUMPSECTION( "recv", "ScopedPDU" );
    data = Api_v3ScopedPduParse( pdu, cp, length );
    if ( data == NULL ) {
        Api_incrementStatistic( API_STAT_SNMPINASNPARSEERRS );
        DEBUG_INDENTADD( -4 );
        MEMORY_FREE( mallocbuf );
        return ErrorCode_ASN_PARSE_ERR;
    }

    /*
     * parse the PDU.
     */
    if ( after_header != NULL ) {
        *after_header = data;
        tmp_buf_len = *length;
    }

    DEBUGPRINTPDUTYPE( "recv", *data );
    ret = Api_pduParse( pdu, data, length );
    DEBUG_INDENTADD( -8 );

    if ( after_header != NULL ) {
        *length = tmp_buf_len;
    }

    if ( ret != ErrorCode_SUCCESS ) {
        IMPL_ERROR_MSG( "error parsing PDU" );
        Api_incrementStatistic( API_STAT_SNMPINASNPARSEERRS );
        MEMORY_FREE( mallocbuf );
        return ErrorCode_ASN_PARSE_ERR;
    }

    MEMORY_FREE( mallocbuf );
    return ErrorCode_SUCCESS;
} /* end Api_v3Parse() */

#define ERROR_STAT_LENGTH 11

int Api_v3MakeReport( Types_Pdu* pdu, int error )
{

    long ltmp;
    static oid unknownSecurityLevel[] = { 1, 3, 6, 1, 6, 3, 15, 1, 1, 1, 0 };
    static oid notInTimeWindow[] = { 1, 3, 6, 1, 6, 3, 15, 1, 1, 2, 0 };
    static oid unknownUserName[] = { 1, 3, 6, 1, 6, 3, 15, 1, 1, 3, 0 };
    static oid unknownEngineID[] = { 1, 3, 6, 1, 6, 3, 15, 1, 1, 4, 0 };
    static oid wrongDigest[] = { 1, 3, 6, 1, 6, 3, 15, 1, 1, 5, 0 };
    static oid decryptionError[] = { 1, 3, 6, 1, 6, 3, 15, 1, 1, 6, 0 };
    oid* err_var;
    int err_var_len;
    int stat_ind;
    struct Secmod_Def_s* sptr;

    switch ( error ) {
    case ErrorCode_USM_UNKNOWNENGINEID:
        stat_ind = API_STAT_USMSTATSUNKNOWNENGINEIDS;
        err_var = unknownEngineID;
        err_var_len = ERROR_STAT_LENGTH;
        break;
    case ErrorCode_USM_UNKNOWNSECURITYNAME:
        stat_ind = API_STAT_USMSTATSUNKNOWNUSERNAMES;
        err_var = unknownUserName;
        err_var_len = ERROR_STAT_LENGTH;
        break;
    case ErrorCode_USM_UNSUPPORTEDSECURITYLEVEL:
        stat_ind = API_STAT_USMSTATSUNSUPPORTEDSECLEVELS;
        err_var = unknownSecurityLevel;
        err_var_len = ERROR_STAT_LENGTH;
        break;
    case ErrorCode_USM_AUTHENTICATIONFAILURE:
        stat_ind = API_STAT_USMSTATSWRONGDIGESTS;
        err_var = wrongDigest;
        err_var_len = ERROR_STAT_LENGTH;
        break;
    case ErrorCode_USM_NOTINTIMEWINDOW:
        stat_ind = API_STAT_USMSTATSNOTINTIMEWINDOWS;
        err_var = notInTimeWindow;
        err_var_len = ERROR_STAT_LENGTH;
        break;
    case ErrorCode_USM_DECRYPTIONERROR:
        stat_ind = API_STAT_USMSTATSDECRYPTIONERRORS;
        err_var = decryptionError;
        err_var_len = ERROR_STAT_LENGTH;
        break;
    default:
        return ErrorCode_GENERR;
    }

    Api_freeVarbind( pdu->variables ); /* free the current varbind */

    pdu->variables = NULL;
    MEMORY_FREE( pdu->securityEngineID );
    pdu->securityEngineID = V3_generateEngineID( &pdu->securityEngineIDLen );
    MEMORY_FREE( pdu->contextEngineID );
    pdu->contextEngineID = V3_generateEngineID( &pdu->contextEngineIDLen );
    pdu->command = PRIOT_MSG_REPORT;
    pdu->errstat = 0;
    pdu->errindex = 0;
    MEMORY_FREE( pdu->contextName );
    pdu->contextName = strdup( "" );
    pdu->contextNameLen = strlen( pdu->contextName );

    /*
     * reports shouldn't cache previous data.
     */
    /*
     * FIX - yes they should but USM needs to follow new EoP to determine
     * which cached values to use
     */
    if ( pdu->securityStateRef ) {
        sptr = Secmod_find( pdu->securityModel );
        if ( sptr ) {
            if ( sptr->pdu_free_state_ref ) {
                ( *sptr->pdu_free_state_ref )( pdu->securityStateRef );
            } else {
                Logger_log( LOGGER_PRIORITY_ERR,
                    "Security Model %d can't free state references\n",
                    pdu->securityModel );
            }
        } else {
            Logger_log( LOGGER_PRIORITY_ERR,
                "Can't find security model to free ptr: %d\n",
                pdu->securityModel );
        }
        pdu->securityStateRef = NULL;
    }

    if ( error == ErrorCode_USM_NOTINTIMEWINDOW ) {
        pdu->securityLevel = PRIOT_SEC_LEVEL_AUTHNOPRIV;
    } else {
        pdu->securityLevel = PRIOT_SEC_LEVEL_NOAUTH;
    }

    /*
     * find the appropriate error counter
     */
    ltmp = Api_getStatistic( stat_ind );

    /*
     * return the appropriate error counter
     */
    Api_pduAddVariable( pdu, err_var, err_var_len,
        ASN01_COUNTER, &ltmp, sizeof( ltmp ) );

    return ErrorCode_SUCCESS;
} /* end Api_v3MakeReport() */

int Api_v3GetReportType( Types_Pdu* pdu )
{
    static oid snmpMPDStats[] = { 1, 3, 6, 1, 6, 3, 11, 2, 1 };
    static oid targetStats[] = { 1, 3, 6, 1, 6, 3, 12, 1 };
    static oid usmStats[] = { 1, 3, 6, 1, 6, 3, 15, 1, 1 };
    Types_VariableList* vp;
    int rpt_type = ErrorCode_UNKNOWN_REPORT;

    if ( pdu == NULL || pdu->variables == NULL )
        return rpt_type;
    vp = pdu->variables;
    /* MPD or USM based report statistics objects have the same length prefix
     *   so the actual statistics OID will have this length,
     *   plus one subidentifier for the scalar MIB object itself,
     *   and one for the instance subidentifier
     */
    if ( vp->nameLength == API_REPORT_STATS_LEN + 2 ) {
        if ( memcmp( snmpMPDStats, vp->name, API_REPORT_STATS_LEN * sizeof( oid ) ) == 0 ) {
            switch ( vp->name[ API_REPORT_STATS_LEN ] ) {
            case API_REPORT_snmpUnknownSecurityModels_NUM:
                rpt_type = ErrorCode_UNKNOWN_SEC_MODEL;
                break;
            case API_REPORT_snmpInvalidMsgs_NUM:
                rpt_type = ErrorCode_INVALID_MSG;
                break;
            case API_REPORT_snmpUnknownPDUHandlers_NUM:
                rpt_type = ErrorCode_BAD_VERSION;
                break;
            }
        } else if ( memcmp( usmStats, vp->name, API_REPORT_STATS_LEN * sizeof( oid ) ) == 0 ) {
            switch ( vp->name[ API_REPORT_STATS_LEN ] ) {
            case API_REPORT_usmStatsUnsupportedSecLevels_NUM:
                rpt_type = ErrorCode_UNSUPPORTED_SEC_LEVEL;
                break;
            case API_REPORT_usmStatsNotInTimeWindows_NUM:
                rpt_type = ErrorCode_NOT_IN_TIME_WINDOW;
                break;
            case API_REPORT_usmStatsUnknownUserNames_NUM:
                rpt_type = ErrorCode_UNKNOWN_USER_NAME;
                break;
            case API_REPORT_usmStatsUnknownEngineIDs_NUM:
                rpt_type = ErrorCode_UNKNOWN_ENG_ID;
                break;
            case API_REPORT_usmStatsWrongDigests_NUM:
                rpt_type = ErrorCode_AUTHENTICATION_FAILURE;
                break;
            case API_REPORT_usmStatsDecryptionErrors_NUM:
                rpt_type = ErrorCode_DECRYPTION_ERR;
                break;
            }
        }
    }
    /* Context-based report statistics from the Target MIB are similar
     *   but the OID prefix has a different length
     */
    if ( vp->nameLength == API_REPORT_STATS_LEN2 + 2 ) {
        if ( memcmp( targetStats, vp->name, API_REPORT_STATS_LEN2 * sizeof( oid ) ) == 0 ) {
            switch ( vp->name[ API_REPORT_STATS_LEN2 ] ) {
            case API_REPORT_snmpUnavailableContexts_NUM:
                rpt_type = ErrorCode_BAD_CONTEXT;
                break;
            case API_REPORT_snmpUnknownContexts_NUM:
                rpt_type = ErrorCode_BAD_CONTEXT;
                break;
            }
        }
    }
    DEBUG_MSGTL( ( "report", "Report type: %d\n", rpt_type ) );
    return rpt_type;
}

/*
 * Parses the packet received on the input session, and places the data into
 * the input pdu.  length is the length of the input packet.
 * If any errors are encountered, -1 or USM error is returned.
 * Otherwise, a 0 is returned.
 */
static int
_Api_parse2( void* sessp,
    Types_Session* session,
    Types_Pdu* pdu, u_char* data, size_t length )
{
    int result = -1;

    static oid snmpEngineIDoid[] = { 1, 3, 6, 1, 6, 3, 10, 2, 1, 1, 0 };
    static size_t snmpEngineIDoid_len = 11;

    static char ourEngineID[ API_SEC_PARAM_BUF_SIZE ];
    static size_t ourEngineID_len = sizeof( ourEngineID );

    Types_Pdu* pdu2 = NULL;

    session->s_snmp_errno = 0;
    session->s_errno = 0;

    /*
     * Ensure all incoming PDUs have a unique means of identification
     * (This is not restricted to AgentX handling,
     * though that is where the need becomes visible)
     */
    pdu->transid = Api_getNextTransid();

    if ( session->version != API_DEFAULT_VERSION ) {
        pdu->version = session->version;
    } else {
        pdu->version = _Api_parseVersion( data, length );
    }

    switch ( pdu->version ) {
    case PRIOT_VERSION_3:

        result = Api_v3Parse( pdu, data, &length, NULL, session );
        DEBUG_MSGTL( ( "priotParse",
            "Parsed PRIOTv3 message (secName:%s, secLevel:%s): %s\n",
            pdu->securityName, _api_secLevelName[ pdu->securityLevel ],
            Api_errstring( result ) ) );

        if ( result ) {
            struct Secmod_Def_s* secmod = Secmod_find( pdu->securityModel );
            if ( !sessp ) {
                session->s_snmp_errno = result;
            } else {
                /*
                 * Call the security model to special handle any errors
                 */

                if ( secmod && secmod->handle_report ) {
                    struct Api_SessionList_s* slp = ( struct Api_SessionList_s* )sessp;
                    ( *secmod->handle_report )( sessp, slp->transport, session,
                        result, pdu );
                }
            }
            if ( pdu->securityStateRef != NULL ) {
                if ( secmod && secmod->pdu_free_state_ref ) {
                    secmod->pdu_free_state_ref( pdu->securityStateRef );
                    pdu->securityStateRef = NULL;
                }
            }
        }

        /* Implement RFC5343 here for two reasons:
           1) From a security perspective it handles this otherwise
              always approved request earlier.  It bypasses the need
              for authorization to the snmpEngineID scalar, which is
              what is what RFC3415 appendix A species as ok.  Note
              that we haven't bypassed authentication since if there
              was an authentication eror it would have been handled
              above in the if(result) part at the lastet.
           2) From an application point of view if we let this request
              get all the way to the application, it'd require that
              all application types supporting discovery also fire up
              a minimal agent in order to handle just this request
              which seems like overkill.  Though there is no other
              application types that currently need discovery (NRs
              accept notifications from contextEngineIDs that derive
              from the NO not the NR).  Also a lame excuse for doing
              it here.
           3) Less important technically, but the net-snmp agent
              doesn't currently handle registrations of different
              engineIDs either and it would have been a lot more work
              to implement there since we'd need to support that
              first. :-/ Supporting multiple context engineIDs should
              be done anyway, so it's not a valid excuse here.
           4) There is a lot less to do if we trump the agent at this
              point; IE, the agent does a lot more unnecessary
              processing when the only thing that should ever be in
              this context by definition is the single scalar.
        */

        /* special RFC5343 engineID discovery engineID check */
        if ( !DefaultStore_getBoolean( DsStorage_LIBRARY_ID,
                 DsBool_NO_DISCOVERY )
            && PRIOT_MSG_RESPONSE != pdu->command && NULL != pdu->contextEngineID && pdu->contextEngineIDLen == 5 && pdu->contextEngineID[ 0 ] == 0x80 && pdu->contextEngineID[ 1 ] == 0x00 && pdu->contextEngineID[ 2 ] == 0x00 && pdu->contextEngineID[ 3 ] == 0x00 && pdu->contextEngineID[ 4 ] == 0x06 ) {

            /* define a result so it doesn't get past us at this point
               and gets dropped by future parts of the stack */
            result = ErrorCode_JUST_A_CONTEXT_PROBE;

            DEBUG_MSGTL( ( "priotv3Contextid", "starting context ID discovery\n" ) );
            /* ensure exactly one variable */
            if ( NULL != pdu->variables && NULL == pdu->variables->nextVariable &&

                /* if it's a GET, match it exactly */
                ( ( PRIOT_MSG_GET == pdu->command && Api_oidCompare( snmpEngineIDoid, snmpEngineIDoid_len, pdu->variables->name, pdu->variables->nameLength ) == 0 )
                     /* if it's a GETNEXT ensure it's less than the engineID oid */
                     || ( PRIOT_MSG_GETNEXT == pdu->command && Api_oidCompare( snmpEngineIDoid, snmpEngineIDoid_len, pdu->variables->name, pdu->variables->nameLength ) > 0 ) ) ) {

                DEBUG_MSGTL( ( "priotv3Contextid",
                    "  One correct variable found\n" ) );

                /* Note: we're explictly not handling a GETBULK.  Deal. */

                /* set up the response */
                pdu2 = Client_clonePdu( pdu );

                /* free the current varbind */
                Api_freeVarbind( pdu2->variables );

                /* set the variables */
                pdu2->variables = NULL;
                pdu2->command = PRIOT_MSG_RESPONSE;
                pdu2->errstat = 0;
                pdu2->errindex = 0;

                ourEngineID_len = V3_getEngineID( ( u_char* )ourEngineID, ourEngineID_len );
                if ( 0 != ourEngineID_len ) {

                    DEBUG_MSGTL( ( "priotv3Contextid",
                        "  responding with our engineID\n" ) );

                    Api_pduAddVariable( pdu2,
                        snmpEngineIDoid, snmpEngineIDoid_len,
                        ASN01_OCTET_STR,
                        ourEngineID, ourEngineID_len );

                    /* send the response */
                    if ( 0 == Api_sessSend( sessp, pdu2 ) ) {

                        DEBUG_MSGTL( ( "priotv3Contextid",
                            "  sent it off!\n" ) );

                        Api_freePdu( pdu2 );

                        Logger_log( LOGGER_PRIORITY_ERR, "sending a response to the context engineID probe failed\n" );
                    }
                } else {
                    Logger_log( LOGGER_PRIORITY_ERR, "failed to get our own engineID!\n" );
                }
            } else {
                Logger_log( LOGGER_PRIORITY_WARNING,
                    "received an odd context engineID probe\n" );
            }
        }
        break;
    case ErrorCode_BAD_VERSION:
        IMPL_ERROR_MSG( "error parsing priot message version" );
        Api_incrementStatistic( API_STAT_SNMPINASNPARSEERRS );
        session->s_snmp_errno = ErrorCode_BAD_VERSION;
        break;
    default:
        IMPL_ERROR_MSG( "unsupported snmp message version" );
        Api_incrementStatistic( API_STAT_SNMPINBADVERSIONS );

        /*
         * need better way to determine OS independent
         * INT32_MAX value, for now hardcode
         */
        if ( pdu->version < 0 || pdu->version > 2147483647 ) {
            Api_incrementStatistic( API_STAT_SNMPINASNPARSEERRS );
        }
        session->s_snmp_errno = ErrorCode_BAD_VERSION;
    }

    return result;
}

static int
_Api_parse( void* sessp,
    Types_Session* pss,
    Types_Pdu* pdu, u_char* data, size_t length )
{
    int rc;

    rc = _Api_parse2( sessp, pss, pdu, data, length );
    if ( rc ) {
        if ( !pss->s_snmp_errno ) {
            pss->s_snmp_errno = ErrorCode_BAD_PARSE;
        }
        API_SET_PRIOT_ERROR( pss->s_snmp_errno );
    }

    return rc;
}

int Api_pduParse( Types_Pdu* pdu, u_char* data, size_t* length )
{
    u_char type;
    u_char msg_type;
    u_char* var_val;
    int badtype = 0;
    size_t len;
    size_t four;
    Types_VariableList* vp = NULL;
    oid objid[ TYPES_MAX_OID_LEN ];
    u_char* p;

    /*
     * Get the PDU type
     */
    data = Asn01_parseHeader( data, length, &msg_type );
    if ( data == NULL )
        return -1;
    DEBUG_MSGTL( ( "dumpvRecv", "    Command %s\n", Api_pduType( msg_type ) ) );
    pdu->command = msg_type;
    pdu->flags &= ( ~PRIOT_UCD_MSG_FLAG_RESPONSE_PDU );

    /*
     * get the fields in the PDU preceeding the variable-bindings sequence
     */
    switch ( pdu->command ) {
    case PRIOT_MSG_TRAP:
        /*
         * enterprise
         */
        pdu->enterpriseLength = TYPES_MAX_OID_LEN;
        data = Asn01_parseObjid( data, length, &type, objid,
            &pdu->enterpriseLength );
        if ( data == NULL )
            return -1;
        pdu->enterprise = ( oid* )malloc( pdu->enterpriseLength * sizeof( oid ) );
        if ( pdu->enterprise == NULL ) {
            return -1;
        }
        memmove( pdu->enterprise, objid,
            pdu->enterpriseLength * sizeof( oid ) );

        /*
         * agent-addr
         */
        four = 4;
        data = Asn01_parseString( data, length, &type,
            ( u_char* )pdu->agentAddr, &four );
        if ( data == NULL )
            return -1;

        /*
         * generic trap
         */
        data = Asn01_parseInt( data, length, &type, ( long* )&pdu->trapType,
            sizeof( pdu->trapType ) );
        if ( data == NULL )
            return -1;
        /*
         * specific trap
         */
        data = Asn01_parseInt( data, length, &type,
            ( long* )&pdu->specificType,
            sizeof( pdu->specificType ) );
        if ( data == NULL )
            return -1;

        /*
         * timestamp
         */
        data = Asn01_parseUnsignedInt( data, length, &type, &pdu->time,
            sizeof( pdu->time ) );
        if ( data == NULL )
            return -1;

        break;

    case PRIOT_MSG_RESPONSE:
    case PRIOT_MSG_REPORT:
        pdu->flags |= PRIOT_UCD_MSG_FLAG_RESPONSE_PDU;
    /*
         * fallthrough
         */

    case PRIOT_MSG_GET:
    case PRIOT_MSG_GETNEXT:
    case PRIOT_MSG_GETBULK:
    case PRIOT_MSG_SET:
    case PRIOT_MSG_TRAP2:
    case PRIOT_MSG_INFORM:
        /*
         * PDU is not an SNMPv1 TRAP
         */

        /*
         * request id
         */
        DEBUG_DUMPHEADER( "recv", "request_id" );
        data = Asn01_parseInt( data, length, &type, &pdu->reqid,
            sizeof( pdu->reqid ) );
        DEBUG_INDENTLESS();
        if ( data == NULL ) {
            return -1;
        }

        /*
         * error status (getbulk non-repeaters)
         */
        DEBUG_DUMPHEADER( "recv", "error status" );
        data = Asn01_parseInt( data, length, &type, &pdu->errstat,
            sizeof( pdu->errstat ) );
        DEBUG_INDENTLESS();
        if ( data == NULL ) {
            return -1;
        }

        /*
         * error index (getbulk max-repetitions)
         */
        DEBUG_DUMPHEADER( "recv", "error index" );
        data = Asn01_parseInt( data, length, &type, &pdu->errindex,
            sizeof( pdu->errindex ) );
        DEBUG_INDENTLESS();
        if ( data == NULL ) {
            return -1;
        }
        break;

    default:
        Logger_log( LOGGER_PRIORITY_ERR, "Bad PDU type received: 0x%.2x\n", pdu->command );
        Api_incrementStatistic( API_STAT_SNMPINASNPARSEERRS );
        return -1;
    }

    /*
     * get header for variable-bindings sequence
     */
    DEBUG_DUMPSECTION( "recv", "VarBindList" );
    data = Asn01_parseSequence( data, length, &type,
        ( ASN01_SEQUENCE | ASN01_CONSTRUCTOR ),
        "varbinds" );
    if ( data == NULL )
        return -1;

    /*
     * get each varBind sequence
     */
    while ( ( int )*length > 0 ) {
        Types_VariableList* vptemp;
        vptemp = ( Types_VariableList* )malloc( sizeof( *vptemp ) );
        if ( NULL == vptemp ) {
            return -1;
        }
        if ( NULL == vp ) {
            pdu->variables = vptemp;
        } else {
            vp->nextVariable = vptemp;
        }
        vp = vptemp;

        vp->nextVariable = NULL;
        vp->val.string = NULL;
        vp->nameLength = TYPES_MAX_OID_LEN;
        vp->name = NULL;
        vp->index = 0;
        vp->data = NULL;
        vp->dataFreeHook = NULL;
        DEBUG_DUMPSECTION( "recv", "VarBind" );
        data = Priot_parseVarOp( data, objid, &vp->nameLength, &vp->type,
            &vp->valLen, &var_val, length );
        if ( data == NULL )
            return -1;
        if ( Client_setVarObjid( vp, objid, vp->nameLength ) )
            return -1;

        len = MAX_PACKET_LENGTH;
        DEBUG_DUMPHEADER( "recv", "Value" );
        switch ( ( short )vp->type ) {
        case ASN01_INTEGER:
            vp->val.integer = ( long* )vp->buf;
            vp->valLen = sizeof( long );
            p = Asn01_parseInt( var_val, &len, &vp->type,
                ( long* )vp->val.integer,
                sizeof( *vp->val.integer ) );
            if ( !p )
                return -1;
            break;
        case ASN01_COUNTER:
        case ASN01_GAUGE:
        case ASN01_TIMETICKS:
        case ASN01_UINTEGER:
            vp->val.integer = ( long* )vp->buf;
            vp->valLen = sizeof( u_long );
            p = Asn01_parseUnsignedInt( var_val, &len, &vp->type,
                ( u_long* )vp->val.integer,
                vp->valLen );
            if ( !p )
                return -1;
            break;
        case ASN01_OPAQUE_COUNTER64:
        case ASN01_OPAQUE_U64:
        case ASN01_COUNTER64:
            vp->val.counter64 = ( struct Asn01_Counter64_s* )vp->buf;
            vp->valLen = sizeof( struct Asn01_Counter64_s );
            p = Asn01_parseUnsignedInt64( var_val, &len, &vp->type,
                ( struct Asn01_Counter64_s* )vp->val.counter64, vp->valLen );
            if ( !p )
                return -1;
            break;
        case ASN01_OPAQUE_FLOAT:
            vp->val.floatVal = ( float* )vp->buf;
            vp->valLen = sizeof( float );
            p = Asn01_parseFloat( var_val, &len, &vp->type,
                vp->val.floatVal, vp->valLen );
            if ( !p )
                return -1;
            break;
        case ASN01_OPAQUE_DOUBLE:
            vp->val.doubleVal = ( double* )vp->buf;
            vp->valLen = sizeof( double );
            p = Asn01_parseDouble( var_val, &len, &vp->type,
                vp->val.doubleVal, vp->valLen );
            if ( !p )
                return -1;
            break;
        case ASN01_OPAQUE_I64:
            vp->val.counter64 = ( struct Asn01_Counter64_s* )vp->buf;
            vp->valLen = sizeof( struct Asn01_Counter64_s );
            p = Asn01_parseSignedInt64( var_val, &len, &vp->type,
                ( struct Asn01_Counter64_s* )vp->val.counter64,
                sizeof( *vp->val.counter64 ) );

            if ( !p )
                return -1;
            break;
        case ASN01_IPADDRESS:
            if ( vp->valLen != 4 )
                return -1;
        /* fallthrough */
        case ASN01_OCTET_STR:
        case ASN01_OPAQUE:
        case ASN01_NSAP:
            if ( vp->valLen < sizeof( vp->buf ) ) {
                vp->val.string = ( u_char* )vp->buf;
            } else {
                vp->val.string = ( u_char* )malloc( vp->valLen );
            }
            if ( vp->val.string == NULL ) {
                return -1;
            }
            p = Asn01_parseString( var_val, &len, &vp->type, vp->val.string,
                &vp->valLen );
            if ( !p )
                return -1;
            break;
        case ASN01_OBJECT_ID:
            vp->valLen = TYPES_MAX_OID_LEN;
            p = Asn01_parseObjid( var_val, &len, &vp->type, objid, &vp->valLen );
            if ( !p )
                return -1;
            vp->valLen *= sizeof( oid );
            vp->val.objid = ( oid* )malloc( vp->valLen );
            if ( vp->val.objid == NULL ) {
                return -1;
            }
            memmove( vp->val.objid, objid, vp->valLen );
            break;
        case PRIOT_NOSUCHOBJECT:
        case PRIOT_NOSUCHINSTANCE:
        case PRIOT_ENDOFMIBVIEW:
        case ASN01_NULL:
            break;
        case ASN01_BIT_STR:
            vp->val.bitstring = ( u_char* )malloc( vp->valLen );
            if ( vp->val.bitstring == NULL ) {
                return -1;
            }
            p = Asn01_parseBitstring( var_val, &len, &vp->type,
                vp->val.bitstring, &vp->valLen );
            if ( !p )
                return -1;
            break;
        default:
            Logger_log( LOGGER_PRIORITY_ERR, "bad type returned (%x)\n", vp->type );
            badtype = -1;
            break;
        }
        DEBUG_INDENTADD( -4 );
    }
    return badtype;
}

/*
 * snmp v3 utility function to parse into the scopedPdu. stores contextName
 * and contextEngineID in pdu struct. Also stores pdu->command (handy for
 * Report generation).
 *
 * returns pointer to begining of PDU or NULL on error.
 */
u_char*
Api_v3ScopedPduParse( Types_Pdu* pdu, u_char* cp, size_t* length )
{
    u_char tmp_buf[ API_MAX_MSG_SIZE ];
    size_t tmp_buf_len;
    u_char type;
    size_t asn_len;
    u_char* data;

    pdu->command = 0; /* initialize so we know if it got parsed */
    asn_len = *length;
    data = Asn01_parseSequence( cp, &asn_len, &type,
        ( ASN01_SEQUENCE | ASN01_CONSTRUCTOR ),
        "plaintext scopedPDU" );
    if ( data == NULL ) {
        return NULL;
    }
    *length -= data - cp;

    /*
     * contextEngineID from scopedPdu
     */
    DEBUG_DUMPHEADER( "recv", "contextEngineID" );
    data = Asn01_parseString( data, length, &type, pdu->contextEngineID,
        &pdu->contextEngineIDLen );
    DEBUG_INDENTLESS();
    if ( data == NULL ) {
        IMPL_ERROR_MSG( "error parsing contextEngineID from scopedPdu" );
        return NULL;
    }

    /*
     * parse contextName from scopedPdu
     */
    tmp_buf_len = API_MAX_CONTEXT_SIZE;
    DEBUG_DUMPHEADER( "recv", "contextName" );
    data = Asn01_parseString( data, length, &type, tmp_buf, &tmp_buf_len );
    DEBUG_INDENTLESS();
    if ( data == NULL ) {
        IMPL_ERROR_MSG( "error parsing contextName from scopedPdu" );
        return NULL;
    }

    if ( tmp_buf_len ) {
        pdu->contextName = ( char* )malloc( tmp_buf_len );
        memmove( pdu->contextName, tmp_buf, tmp_buf_len );
        pdu->contextNameLen = tmp_buf_len;
    } else {
        pdu->contextName = strdup( "" );
        pdu->contextNameLen = 0;
    }
    if ( pdu->contextName == NULL ) {
        IMPL_ERROR_MSG( "error copying contextName from scopedPdu" );
        return NULL;
    }

    /*
     * Get the PDU type
     */
    asn_len = *length;
    cp = Asn01_parseHeader( data, &asn_len, &type );
    if ( cp == NULL )
        return NULL;

    pdu->command = type;

    return data;
}

/*
 * These functions send PDUs using an active session:
 * Api_send             - traditional API, no callback
 * snmp_async_send       - traditional API, with callback
 * Api_sessSend        - single session API, no callback
 * Api_sessAsyncSend  - single session API, with callback
 *
 * Call Api_build to create a serialized packet (the pdu).
 * If necessary, set some of the pdu data from the
 * session defaults.
 * If there is an expected response for this PDU,
 * queue a corresponding request on the list
 * of outstanding requests for this session,
 * and store the callback vectors in the request.
 *
 * Send the pdu to the target identified by this session.
 * Return on success:
 *   The request id of the pdu is returned, and the pdu is freed.
 * Return on failure:
 *   Zero (0) is returned.
 *   The caller must call Api_freePdu if 0 is returned.
 */
int Api_send( Types_Session* session, Types_Pdu* pdu )
{
    return Api_asyncSend( session, pdu, NULL, NULL );
}

int Api_sessSend( void* sessp, Types_Pdu* pdu )
{
    return Api_sessAsyncSend( sessp, pdu, NULL, NULL );
}

int Api_asyncSend( Types_Session* session,
    Types_Pdu* pdu, Types_CallbackFT callback, void* cb_data )
{
    void* sessp = Api_sessPointer( session );
    return Api_sessAsyncSend( sessp, pdu, callback, cb_data );
}

static int
_Api_sessAsyncSend( void* sessp,
    Types_Pdu* pdu, Types_CallbackFT callback, void* cb_data )
{
    struct Api_SessionList_s* slp = ( struct Api_SessionList_s* )sessp;
    Types_Session* session;
    struct Api_InternalSession_s* isp;
    Transport_Transport* transport = NULL;
    u_char *pktbuf = NULL, *packet = NULL;
    size_t pktbuf_len = 0, offset = 0, length = 0;
    int result;
    long reqid;

    if ( slp == NULL ) {
        return 0;
    } else {
        session = slp->session;
        isp = slp->internal;
        transport = slp->transport;
        if ( !session || !isp || !transport ) {
            DEBUG_MSGTL( ( "sessAsyncSend", "send fail: closing...\n" ) );
            return 0;
        }
    }

    if ( pdu == NULL ) {
        session->s_snmp_errno = ErrorCode_NULL_PDU;
        return 0;
    }

    session->s_snmp_errno = 0;
    session->s_errno = 0;

    /*
     * Check/setup the version.
     */
    if ( pdu->version == API_DEFAULT_VERSION ) {
        if ( session->version == API_DEFAULT_VERSION ) {
            session->s_snmp_errno = ErrorCode_BAD_VERSION;
            return 0;
        }
        pdu->version = session->version;
    } else if ( session->version == API_DEFAULT_VERSION ) {
        /*
         * It's OK
         */
    } else if ( pdu->version != session->version ) {
        /*
         * ENHANCE: we should support multi-lingual sessions
         */
        session->s_snmp_errno = ErrorCode_BAD_VERSION;
        return 0;
    }

    /*
     * do we expect a response?
     */
    switch ( pdu->command ) {

    case PRIOT_MSG_RESPONSE:
    case PRIOT_MSG_TRAP:
    case PRIOT_MSG_TRAP2:
    case PRIOT_MSG_REPORT:
    case AGENTX_MSG_CLEANUPSET:
    case AGENTX_MSG_RESPONSE:
        pdu->flags &= ~PRIOT_UCD_MSG_FLAG_EXPECT_RESPONSE;
        break;

    default:
        pdu->flags |= PRIOT_UCD_MSG_FLAG_EXPECT_RESPONSE;
        break;
    }

    /*
     * check to see if we need a v3 engineID probe
     */
    if ( ( pdu->version == PRIOT_VERSION_3 ) && ( pdu->flags & PRIOT_UCD_MSG_FLAG_EXPECT_RESPONSE ) && ( session->securityEngineIDLen == 0 ) && ( 0 == ( session->flags & API_FLAGS_DONT_PROBE ) ) ) {
        int rc;
        DEBUG_MSGTL( ( "priotV3Build", "delayed probe for engineID\n" ) );
        rc = Api_v3EngineIDProbe( slp, session );
        if ( rc == 0 )
            return 0; /* s_snmp_errno already set */
    }

    if ( ( pktbuf = ( u_char* )malloc( 2048 ) ) == NULL ) {
        DEBUG_MSGTL( ( "sessAsyncSend",
            "couldn't malloc initial packet buffer\n" ) );
        session->s_snmp_errno = ErrorCode_MALLOC;
        return 0;
    } else {
        pktbuf_len = 2048;
    }

    /*
     * Build the message to send.
     */
    if ( isp->hook_realloc_build ) {
        result = isp->hook_realloc_build( session, pdu,
            &pktbuf, &pktbuf_len, &offset );
        packet = pktbuf;
        length = offset;
    } else if ( isp->hook_build ) {
        packet = pktbuf;
        length = pktbuf_len;
        result = isp->hook_build( session, pdu, pktbuf, &length );
    } else {
        if ( DefaultStore_getBoolean( DsStorage_LIBRARY_ID, DsBool_REVERSE_ENCODE ) ) {
            result = Api_build( &pktbuf, &pktbuf_len, &offset, session, pdu );
            packet = pktbuf + pktbuf_len - offset;
            length = offset;
        } else {
            packet = pktbuf;
            length = pktbuf_len;
            result = Api_build( &pktbuf, &length, &offset, session, pdu );
        }
    }

    if ( result < 0 ) {
        DEBUG_MSGTL( ( "sessAsyncSend", "encoding failure\n" ) );
        MEMORY_FREE( pktbuf );
        return 0;
    }

    /*
     * Make sure we don't send something that is bigger than the msgMaxSize
     * specified in the received PDU.
     */

    if ( pdu->version == PRIOT_VERSION_3 && session->sndMsgMaxSize != 0 && length > session->sndMsgMaxSize ) {
        DEBUG_MSGTL( ( "sessAsyncSend",
            "length of packet (%lu) exceeds session maximum (%lu)\n",
            ( unsigned long )length, ( unsigned long )session->sndMsgMaxSize ) );
        session->s_snmp_errno = ErrorCode_TOO_LONG;
        MEMORY_FREE( pktbuf );
        return 0;
    }

    /*
     * Check that the underlying transport is capable of sending a packet as
     * large as length.
     */

    if ( transport->msgMaxSize != 0 && length > transport->msgMaxSize ) {
        DEBUG_MSGTL( ( "sessAsyncSend",
            "length of packet (%lu) exceeds transport maximum (%lu)\n",
            ( unsigned long )length, ( unsigned long )transport->msgMaxSize ) );
        session->s_snmp_errno = ErrorCode_TOO_LONG;
        MEMORY_FREE( pktbuf );
        return 0;
    }

    /*
     * Send the message.
     */

    DEBUG_MSGTL( ( "sessProcessPacket", "sending message id#%ld reqid#%ld len %"
                                        "l"
                                        "u\n",
        pdu->msgid, pdu->reqid, length ) );
    result = Transport_send( transport, packet, length,
        &( pdu->transportData ),
        &( pdu->transportDataLength ) );

    MEMORY_FREE( pktbuf );

    if ( result < 0 ) {
        session->s_snmp_errno = ErrorCode_BAD_SENDTO;
        session->s_errno = errno;
        return 0;
    }

    reqid = pdu->reqid;

    /*
     * Add to pending requests list if we expect a response.
     */
    if ( pdu->flags & PRIOT_UCD_MSG_FLAG_EXPECT_RESPONSE ) {
        Api_RequestList* rp;
        struct timeval tv;

        rp = ( Api_RequestList* )calloc( 1,
            sizeof( Api_RequestList ) );
        if ( rp == NULL ) {
            session->s_snmp_errno = ErrorCode_GENERR;
            return 0;
        }

        Tools_getMonotonicClock( &tv );
        rp->pdu = pdu;
        rp->request_id = pdu->reqid;
        rp->message_id = pdu->msgid;
        rp->callback = callback;
        rp->cb_data = cb_data;
        rp->retries = 0;
        if ( pdu->flags & PRIOT_UCD_MSG_FLAG_PDU_TIMEOUT ) {
            rp->timeout = pdu->time * 1000000L;
        } else {
            rp->timeout = session->timeout;
        }
        rp->timeM = tv;
        tv.tv_usec += rp->timeout;
        tv.tv_sec += tv.tv_usec / 1000000L;
        tv.tv_usec %= 1000000L;
        rp->expireM = tv;

        /*
         * XX lock should be per session !
         */
        Mutex_lock( MTSUPPORT_LIBRARY_ID, MTSUPPORT_LIB_SESSION );
        if ( isp->requestsEnd ) {
            rp->next_request = isp->requestsEnd->next_request;
            isp->requestsEnd->next_request = rp;
            isp->requestsEnd = rp;
        } else {
            rp->next_request = isp->requests;
            isp->requests = rp;
            isp->requestsEnd = rp;
        }
        Mutex_unlock( MTSUPPORT_LIBRARY_ID, MTSUPPORT_LIB_SESSION );
    } else {
        /*
         * No response expected...
         */
        if ( reqid ) {
            /*
             * Free v1 or v2 TRAP PDU iff no error
             */
            Api_freePdu( pdu );
        }
    }

    return reqid;
}

int Api_sessAsyncSend( void* sessp,
    Types_Pdu* pdu,
    Types_CallbackFT callback, void* cb_data )
{
    int rc;

    if ( sessp == NULL ) {
        api_priotErrno = ErrorCode_BAD_SESSION; /*MTCRITICAL_RESOURCE */
        return ( 0 );
    }
    /*
     * send pdu
     */
    rc = _Api_sessAsyncSend( sessp, pdu, callback, cb_data );
    if ( rc == 0 ) {
        struct Api_SessionList_s* psl;
        Types_Session* pss;
        psl = ( struct Api_SessionList_s* )sessp;
        pss = psl->session;
        API_SET_PRIOT_ERROR( pss->s_snmp_errno );
    }
    return rc;
}

/*
 * Frees the variable and any malloc'd data associated with it.
 */
void Api_freeVarInternals( Types_VariableList* var )
{
    if ( !var )
        return;

    if ( var->name != var->nameLoc )
        MEMORY_FREE( var->name );
    if ( var->val.string != var->buf )
        MEMORY_FREE( var->val.string );
    if ( var->data ) {
        if ( var->dataFreeHook ) {
            var->dataFreeHook( var->data );
            var->data = NULL;
        } else {
            MEMORY_FREE( var->data );
        }
    }
}

void Api_freeVar( Types_VariableList* var )
{
    Api_freeVarInternals( var );
    free( ( char* )var );
}

void Api_freeVarbind( Types_VariableList* var )
{
    Types_VariableList* ptr;
    while ( var ) {
        ptr = var->nextVariable;
        Api_freeVar( var );
        var = ptr;
    }
}

/*
 * Frees the pdu and any malloc'd data associated with it.
 */
void Api_freePdu( Types_Pdu* pdu )
{
    struct Secmod_Def_s* sptr;

    if ( !pdu )
        return;

    /*
     * If the command field is empty, that probably indicates
     *   that this PDU structure has already been freed.
     *   Log a warning and return (rather than freeing things again)
     *
     * Note that this does not pick up dual-frees where the
     *   memory is set to random junk, which is probably more serious.
     *
     * rks: while this is a good idea, there are two problems.
     *         1) agentx sets command to 0 in some cases
     *         2) according to Wes, a bad decode of a v3 message could
     *            result in a 0 at this offset.
     *      so I'm commenting it out until a better solution is found.
     *      note that I'm leaving the memset, below....
     *
    if (pdu->command == 0) {
        Logger_log(LOGGER_PRIORITY_WARNING, "Api_freePdu probably called twice\n");
        return;
    }
     */
    if ( ( sptr = Secmod_find( pdu->securityModel ) ) != NULL && sptr->pdu_free != NULL ) {
        ( *sptr->pdu_free )( pdu );
    }
    Api_freeVarbind( pdu->variables );
    MEMORY_FREE( pdu->enterprise );
    MEMORY_FREE( pdu->community );
    MEMORY_FREE( pdu->contextEngineID );
    MEMORY_FREE( pdu->securityEngineID );
    MEMORY_FREE( pdu->contextName );
    MEMORY_FREE( pdu->securityName );
    MEMORY_FREE( pdu->transportData );
    memset( pdu, 0, sizeof( Types_Pdu ) );
    free( ( char* )pdu );
}

Types_Pdu*
Api_createSessPdu( Transport_Transport* transport, void* opaque,
    size_t olength )
{
    Types_Pdu* pdu = ( Types_Pdu* )calloc( 1, sizeof( Types_Pdu ) );
    if ( pdu == NULL ) {
        DEBUG_MSGTL( ( "sessProcessPacket", "can't malloc space for PDU\n" ) );
        return NULL;
    }

    /*
     * Save the transport-level data specific to this reception (e.g. UDP
     * source address).
     */

    pdu->transportData = opaque;
    pdu->transportDataLength = olength;
    pdu->tDomain = transport->domain;
    pdu->tDomainLen = transport->domain_length;
    return pdu;
}

/*
 * This function processes a complete (according to Asn01_checkPacket or the
 * AgentX equivalent) packet, parsing it into a PDU and calling the relevant
 * callbacks.  On entry, packetptr points at the packet in the session's
 * buffer and length is the length of the packet.
 */

static int
_Api_sessProcessPacket( void* sessp, Types_Session* sp,
    struct Api_InternalSession_s* isp,
    Transport_Transport* transport,
    void* opaque, int olength,
    u_char* packetptr, int length )
{
    struct Api_SessionList_s* slp = ( struct Api_SessionList_s* )sessp;
    Types_Pdu* pdu;
    Api_RequestList *rp, *orp = NULL;
    struct Secmod_Def_s* sptr;
    int ret = 0, handled = 0;

    DEBUG_MSGTL( ( "sessProcessPacket",
        "session %p fd %d pkt %p length %d\n", sessp,
        transport->sock, packetptr, length ) );

    if ( DefaultStore_getBoolean( DsStorage_LIBRARY_ID, DsBool_DUMP_PACKET ) ) {
        char* addrtxt = Transport_peerString( transport, opaque, olength );
        Logger_log( LOGGER_PRIORITY_DEBUG, "\nReceived %d byte packet from %s\n",
            length, addrtxt );
        MEMORY_FREE( addrtxt );
        Priot_xdump( packetptr, length, "" );
    }

    /*
   * Do transport-level filtering (e.g. IP-address based allow/deny).
   */

    if ( isp->hook_pre ) {
        if ( isp->hook_pre( sp, transport, opaque, olength ) == 0 ) {
            DEBUG_MSGTL( ( "sessProcessPacket", "pre-parse fail\n" ) );
            MEMORY_FREE( opaque );
            return -1;
        }
    }

    if ( isp->hook_create_pdu ) {
        pdu = isp->hook_create_pdu( transport, opaque, olength );
    } else {
        pdu = Api_createSessPdu( transport, opaque, olength );
    }

    if ( pdu == NULL ) {
        Logger_log( LOGGER_PRIORITY_ERR, "pdu failed to be created\n" );
        MEMORY_FREE( opaque );
        return -1;
    }

    /* if the transport was a magic tunnel, mark the PDU as having come
     through one. */
    if ( transport->flags & TRANSPORT_FLAG_TUNNELED ) {
        pdu->flags |= PRIOT_UCD_MSG_FLAG_TUNNELED;
    }

    if ( isp->hook_parse ) {
        ret = isp->hook_parse( sp, pdu, packetptr, length );
    } else {
        ret = _Api_parse( sessp, sp, pdu, packetptr, length );
    }

    DEBUG_MSGTL( ( "sessProcessPacket", "received message id#%ld reqid#%ld len "
                                        "%u\n",
        pdu->msgid, pdu->reqid, length ) );

    if ( ret != PRIOT_ERR_NOERROR ) {
        DEBUG_MSGTL( ( "sessProcessPacket", "parse fail\n" ) );
    }

    if ( isp->hook_post ) {
        if ( isp->hook_post( sp, pdu, ret ) == 0 ) {
            DEBUG_MSGTL( ( "sessProcessPacket", "post-parse fail\n" ) );
            ret = ErrorCode_ASN_PARSE_ERR;
        }
    }

    if ( ret != PRIOT_ERR_NOERROR ) {
        /*
     * Call the security model to free any securityStateRef supplied w/ msg.
     */
        if ( pdu->securityStateRef != NULL ) {
            sptr = Secmod_find( pdu->securityModel );
            if ( sptr != NULL ) {
                if ( sptr->pdu_free_state_ref != NULL ) {
                    ( *sptr->pdu_free_state_ref )( pdu->securityStateRef );
                } else {
                    Logger_log( LOGGER_PRIORITY_ERR,
                        "Security Model %d can't free state references\n",
                        pdu->securityModel );
                }
            } else {
                Logger_log( LOGGER_PRIORITY_ERR,
                    "Can't find security model to free ptr: %d\n",
                    pdu->securityModel );
            }
            pdu->securityStateRef = NULL;
        }
        Api_freePdu( pdu );
        return -1;
    }

    if ( pdu->flags & PRIOT_UCD_MSG_FLAG_RESPONSE_PDU ) {
        /*
     * Call USM to free any securityStateRef supplied with the message.
     */
        if ( pdu->securityStateRef ) {
            sptr = Secmod_find( pdu->securityModel );
            if ( sptr ) {
                if ( sptr->pdu_free_state_ref ) {
                    ( *sptr->pdu_free_state_ref )( pdu->securityStateRef );
                } else {
                    Logger_log( LOGGER_PRIORITY_ERR,
                        "Security Model %d can't free state references\n",
                        pdu->securityModel );
                }
            } else {
                Logger_log( LOGGER_PRIORITY_ERR,
                    "Can't find security model to free ptr: %d\n",
                    pdu->securityModel );
            }
            pdu->securityStateRef = NULL;
        }

        for ( rp = isp->requests; rp; orp = rp, rp = rp->next_request ) {
            Types_CallbackFT callback;
            void* magic;

            if ( pdu->version == PRIOT_VERSION_3 ) {
                /*
     * msgId must match for v3 messages.
     */
                if ( rp->message_id != pdu->msgid ) {
                    DEBUG_MSGTL( ( "sessProcessPacket", "unmatched msg id: %ld != %ld\n",
                        rp->message_id, pdu->msgid ) );
                    continue;
                }

                /*
     * Check that message fields match original, if not, no further
     * processing.
     */
                if ( !_Api_v3VerifyMsg( rp, pdu ) ) {
                    break;
                }
            } else {
                if ( rp->request_id != pdu->reqid ) {
                    continue;
                }
            }

            if ( rp->callback ) {
                callback = rp->callback;
                magic = rp->cb_data;
            } else {
                callback = sp->callback;
                magic = sp->callback_magic;
            }
            handled = 1;

            /*
       * MTR MtSupport_resLock(MTSUPPORT_LIBRARY_ID, MTSUPPORT_LIB_SESSION);  ?* XX lock
       * should be per session !
       */

            if ( callback == NULL
                || callback( API_CALLBACK_OP_RECEIVED_MESSAGE, sp,
                       pdu->reqid, pdu, magic )
                    == 1 ) {
                if ( pdu->command == PRIOT_MSG_REPORT ) {
                    if ( sp->s_snmp_errno == ErrorCode_NOT_IN_TIME_WINDOW || Api_v3GetReportType( pdu ) == ErrorCode_NOT_IN_TIME_WINDOW ) {
                        /*
         * trigger immediate retry on recoverable Reports
         * * (notInTimeWindow), incr_retries == TRUE to prevent
         * * inifinite resend
         */
                        if ( rp->retries <= sp->retries ) {
                            _Api_resendRequest( slp, rp, TRUE );
                            break;
                        } else {
                            /* We're done with retries, so no longer waiting for a response */
                            if ( magic ) {
                                ( ( struct Client_SynchState_s* )magic )->waiting = 0;
                            }
                        }
                    } else {
                        if ( API_V3_IGNORE_UNAUTH_REPORTS ) {
                            break;
                        } else { /* Set the state to no longer be waiting, since we're done with retries */
                            if ( magic ) {
                                ( ( struct Client_SynchState_s* )magic )->waiting = 0;
                            }
                        }
                    }

                    /*
       * Handle engineID discovery.
       */
                    if ( !sp->securityEngineIDLen && pdu->securityEngineIDLen ) {
                        sp->securityEngineID = ( u_char* )malloc( pdu->securityEngineIDLen );
                        if ( sp->securityEngineID == NULL ) {
                            /*
           * TODO FIX: recover after message callback *?
               */
                            return -1;
                        }
                        memcpy( sp->securityEngineID, pdu->securityEngineID,
                            pdu->securityEngineIDLen );
                        sp->securityEngineIDLen = pdu->securityEngineIDLen;
                        if ( !sp->contextEngineIDLen ) {
                            sp->contextEngineID = ( u_char* )malloc( pdu->securityEngineIDLen );
                            if ( sp->contextEngineID == NULL ) {
                                /*
         * TODO FIX: recover after message callback *?
         */
                                return -1;
                            }
                            memcpy( sp->contextEngineID,
                                pdu->securityEngineID,
                                pdu->securityEngineIDLen );
                            sp->contextEngineIDLen = pdu->securityEngineIDLen;
                        }
                    }
                }

                /*
     * Successful, so delete request.
     */
                if ( orp )
                    orp->next_request = rp->next_request;
                else
                    isp->requests = rp->next_request;
                if ( isp->requestsEnd == rp )
                    isp->requestsEnd = orp;
                Api_freePdu( rp->pdu );
                free( rp );
                /*
     * There shouldn't be any more requests with the same reqid.
     */
                break;
            }
            /*
       * MTR MtSupport_resUnlock(MTSUPPORT_LIBRARY_ID, MTSUPPORT_LIB_SESSION);  ?* XX lock should be per session !
       */
        }
    } else {
        if ( sp->callback ) {
            /*
       * MTR MtSupport_resLock(MTSUPPORT_LIBRARY_ID, MTSUPPORT_LIB_SESSION);
       */
            handled = 1;
            sp->callback( API_CALLBACK_OP_RECEIVED_MESSAGE,
                sp, pdu->reqid, pdu, sp->callback_magic );
            /*
       * MTR MtSupport_resUnlock(MTSUPPORT_LIBRARY_ID, MTSUPPORT_LIB_SESSION);
       */
        }
    }

    /*
   * Call USM to free any securityStateRef supplied with the message.
   */
    if ( pdu != NULL && pdu->securityStateRef && pdu->command == PRIOT_MSG_TRAP2 ) {
        sptr = Secmod_find( pdu->securityModel );
        if ( sptr ) {
            if ( sptr->pdu_free_state_ref ) {
                ( *sptr->pdu_free_state_ref )( pdu->securityStateRef );
            } else {
                Logger_log( LOGGER_PRIORITY_ERR,
                    "Security Model %d can't free state references\n",
                    pdu->securityModel );
            }
        } else {
            Logger_log( LOGGER_PRIORITY_ERR,
                "Can't find security model to free ptr: %d\n",
                pdu->securityModel );
        }
        pdu->securityStateRef = NULL;
    }

    if ( !handled ) {
        Api_incrementStatistic( API_STAT_SNMPUNKNOWNPDUHANDLERS );
        DEBUG_MSGTL( ( "sessProcessPacket", "unhandled PDU\n" ) );
    }

    Api_freePdu( pdu );
    return 0;
}

/*
 * Checks to see if any of the fd's set in the fdset belong to
 * snmp.  Each socket with it's fd set has a packet read from it
 * and Api_parse is called on the packet received.  The resulting pdu
 * is passed to the callback routine for that session.  If the callback
 * routine returns successfully, the pdu and it's request are deleted.
 */
void Api_read( fd_set* fdset )
{
    Types_LargeFdSet lfdset;

    LargeFdSet_init( &lfdset, FD_SETSIZE );
    LargeFdSet_copyFdSetToLargeFdSet( &lfdset, fdset );
    Api_read2( &lfdset );
    LargeFdSet_cleanup( &lfdset );
}

void Api_read2( Types_LargeFdSet* fdset )
{
    struct Api_SessionList_s* slp;
    Mutex_lock( MTSUPPORT_LIBRARY_ID, MTSUPPORT_LIB_SESSION );
    for ( slp = api_sessions; slp; slp = slp->next ) {
        Api_sessRead2( ( void* )slp, fdset );
    }
    Mutex_unlock( MTSUPPORT_LIBRARY_ID, MTSUPPORT_LIB_SESSION );
}

/*
 * Same as Api_read, but works just one session.
 * returns 0 if success, -1 if fail
 * MTR: can't lock here and at Api_read
 * Beware recursive send maybe inside Api_read callback function.
 */
int Api_sessRead3( void* sessp, Types_LargeFdSet* fdset )
{
    struct Api_SessionList_s* slp = ( struct Api_SessionList_s* )sessp;
    Types_Session* sp = slp ? slp->session : NULL;
    struct Api_InternalSession_s* isp = slp ? slp->internal : NULL;
    Transport_Transport* transport = slp ? slp->transport : NULL;
    size_t pdulen = 0, rxbuf_len = 65536;
    u_char* rxbuf = NULL;
    int length = 0, olength = 0, rc = 0;
    void* opaque = NULL;

    if ( !sp || !isp || !transport ) {
        DEBUG_MSGTL( ( "sessRead", "read fail: closing...\n" ) );
        return 0;
    }

    /* to avoid subagent crash */
    if ( transport->sock < 0 ) {
        Logger_log( LOGGER_PRIORITY_INFO, "transport->sock got negative fd value %d\n", transport->sock );
        return 0;
    }

    if ( !fdset || !( LARGEFDSET_FD_ISSET( transport->sock, fdset ) ) ) {
        DEBUG_MSGTL( ( "sessRead", "not reading %d (fdset %p set %d)\n",
            transport->sock, fdset,
            fdset ? LARGEFDSET_FD_ISSET( transport->sock, fdset )
                  : -9 ) );
        return 0;
    }

    sp->s_snmp_errno = 0;
    sp->s_errno = 0;

    if ( transport->flags & TRANSPORT_FLAG_LISTEN ) {
        int data_sock = transport->f_accept( transport );

        if ( data_sock >= 0 ) {
            /*
             * We've successfully accepted a new stream-based connection.
             * It's not too clear what should happen here if we are using the
             * single-session API at this point.  Basically a "session
             * accepted" callback is probably needed to hand the new session
             * over to the application.
             *
             * However, for now, as in the original snmp_api, we will ASSUME
             * that we're using the traditional API, and simply add the new
             * session to the list.  Note we don't have to get the Session
             * list lock here, because under that assumption we already hold
             * it (this is also why we don't just use Api_add).
             *
             * The moral of the story is: don't use listening stream-based
             * transports in a multi-threaded environment because something
             * will go HORRIBLY wrong (and also that SNMP/TCP is not trivial).
             *
             * Another open issue: what should happen to sockets that have
             * been accept()ed from a listening socket when that original
             * socket is closed?  If they are left open, then attempting to
             * re-open the listening socket will fail, which is semantically
             * confusing.  Perhaps there should be some kind of chaining in
             * the transport structure so that they can all be closed.
             * Discuss.  ;-)
             */

            Transport_Transport* new_transport = Transport_copy( transport );
            if ( new_transport != NULL ) {
                struct Api_SessionList_s* nslp = NULL;

                new_transport->sock = data_sock;
                new_transport->flags &= ~TRANSPORT_FLAG_LISTEN;

                nslp = ( struct Api_SessionList_s* )Api_sessAddEx( sp,
                    new_transport, isp->hook_pre, isp->hook_parse,
                    isp->hook_post, isp->hook_build,
                    isp->hook_realloc_build, isp->check_packet,
                    isp->hook_create_pdu );

                if ( nslp != NULL ) {
                    nslp->next = api_sessions;
                    api_sessions = nslp;
                    /*
                     * Tell the new session about its existance if possible.
                     */
                    DEBUG_MSGTL( ( "sessRead",
                        "perform callback with op=CONNECT\n" ) );
                    ( void )nslp->session->callback( API_CALLBACK_OP_CONNECT,
                        nslp->session, 0, NULL,
                        sp->callback_magic );
                }
                return 0;
            } else {
                sp->s_snmp_errno = ErrorCode_MALLOC;
                sp->s_errno = errno;
                Api_setDetail( strerror( errno ) );
                return -1;
            }
        } else {
            sp->s_snmp_errno = ErrorCode_BAD_RECVFROM;
            sp->s_errno = errno;
            Api_setDetail( strerror( errno ) );
            return -1;
        }
    }

    /*
     * Work out where to receive the data to.
     */

    if ( transport->flags & TRANSPORT_FLAG_STREAM ) {
        if ( isp->packet == NULL ) {
            /*
             * We have no saved packet.  Allocate one.
             */
            if ( ( isp->packet = ( u_char* )malloc( rxbuf_len ) ) == NULL ) {
                DEBUG_MSGTL( ( "sessRead", "can't malloc %"
                                           "l"
                                           "u bytes for rxbuf\n",
                    rxbuf_len ) );
                return 0;
            } else {
                rxbuf = isp->packet;
                isp->packet_size = rxbuf_len;
                isp->packet_len = 0;
            }
        } else {
            /*
             * We have saved a partial packet from last time.  Extend that, if
             * necessary, and receive new data after the old data.
             */
            u_char* newbuf;

            if ( isp->packet_size < isp->packet_len + rxbuf_len ) {
                newbuf = ( u_char* )realloc( isp->packet,
                    isp->packet_len + rxbuf_len );
                if ( newbuf == NULL ) {
                    DEBUG_MSGTL( ( "sessRead",
                        "can't malloc %"
                        "l"
                        "u more for rxbuf (%"
                        "l"
                        "u tot)\n",
                        rxbuf_len, isp->packet_len + rxbuf_len ) );
                    return 0;
                } else {
                    isp->packet = newbuf;
                    isp->packet_size = isp->packet_len + rxbuf_len;
                    rxbuf = isp->packet + isp->packet_len;
                }
            } else {
                rxbuf = isp->packet + isp->packet_len;
                rxbuf_len = isp->packet_size - isp->packet_len;
            }
        }
    } else {
        if ( ( rxbuf = ( u_char* )malloc( rxbuf_len ) ) == NULL ) {
            DEBUG_MSGTL( ( "sessRead", "can't malloc %"
                                       "l"
                                       "u bytes for rxbuf\n",
                rxbuf_len ) );
            return 0;
        }
    }

    length = Transport_recv( transport, rxbuf, rxbuf_len, &opaque,
        &olength );

    if ( length == -1 && !( transport->flags & TRANSPORT_FLAG_STREAM ) ) {
        sp->s_snmp_errno = ErrorCode_BAD_RECVFROM;
        sp->s_errno = errno;
        Api_setDetail( strerror( errno ) );
        MEMORY_FREE( rxbuf );
        MEMORY_FREE( opaque );
        return -1;
    }

    if ( 0 == length && transport->flags & TRANSPORT_FLAG_EMPTY_PKT ) {
        /* this allows for a transport that needs to return from
         * packet processing that doesn't necessarily have any
         * consumable data in it. */

        /* reset the flag since it's a per-message flag */
        transport->flags &= ( ~TRANSPORT_FLAG_EMPTY_PKT );

        return 0;
    }

    /*
     * Remote end closed connection.
     */

    if ( length <= 0 && transport->flags & TRANSPORT_FLAG_STREAM ) {
        /*
         * Alert the application if possible.
         */
        if ( sp->callback != NULL ) {
            DEBUG_MSGTL( ( "sessRead", "perform callback with op=DISCONNECT\n" ) );
            ( void )sp->callback( API_CALLBACK_OP_DISCONNECT, sp, 0,
                NULL, sp->callback_magic );
        }
        /*
         * Close socket and mark session for deletion.
         */
        DEBUG_MSGTL( ( "sessRead", "fd %d closed\n", transport->sock ) );
        transport->f_close( transport );
        MEMORY_FREE( isp->packet );
        MEMORY_FREE( opaque );
        return -1;
    }

    if ( transport->flags & TRANSPORT_FLAG_STREAM ) {
        u_char* pptr = isp->packet;
        void* ocopy = NULL;

        isp->packet_len += length;

        while ( isp->packet_len > 0 ) {

            /*
             * Get the total data length we're expecting (and need to wait
             * for).
             */
            if ( isp->check_packet ) {
                pdulen = isp->check_packet( pptr, isp->packet_len );
            } else {
                pdulen = Asn01_checkPacket( pptr, isp->packet_len );
            }

            DEBUG_MSGTL( ( "sessRead",
                "  loop packet_len %"
                "l"
                "u, PDU length %"
                "l"
                "u\n",
                isp->packet_len, pdulen ) );

            if ( pdulen > MAX_PACKET_LENGTH ) {
                /*
                 * Illegal length, drop the connection.
                 */
                Logger_log( LOGGER_PRIORITY_ERR,
                    "Received broken packet. Closing session.\n" );
                if ( sp->callback != NULL ) {
                    DEBUG_MSGTL( ( "sessRead",
                        "perform callback with op=DISCONNECT\n" ) );
                    ( void )sp->callback( API_CALLBACK_OP_DISCONNECT,
                        sp, 0, NULL, sp->callback_magic );
                }
                DEBUG_MSGTL( ( "sessRead", "fd %d closed\n", transport->sock ) );
                transport->f_close( transport );
                MEMORY_FREE( opaque );
                /** XXX-rks: why no MEMORY_FREE(isp->packet); ?? */
                return -1;
            }

            if ( pdulen > isp->packet_len || pdulen == 0 ) {
                /*
                 * We don't have a complete packet yet.  If we've already
                 * processed a packet, break out so we'll shift this packet
                 * to the start of the buffer. If we're already at the
                 * start, simply return and wait for more data to arrive.
                 */
                DEBUG_MSGTL( ( "sessRead",
                    "pkt not complete (need %"
                    "l"
                    "u got %"
                    "l"
                    "u so far)\n",
                    pdulen,
                    isp->packet_len ) );

                if ( pptr != isp->packet )
                    break; /* opaque freed for us outside of loop. */

                MEMORY_FREE( opaque );
                return 0;
            }

            /*  We have *at least* one complete packet in the buffer now.  If
        we have possibly more than one packet, we must copy the opaque
        pointer because we may need to reuse it for a later packet.  */

            if ( pdulen < isp->packet_len ) {
                if ( olength > 0 && opaque != NULL ) {
                    ocopy = malloc( olength );
                    if ( ocopy != NULL ) {
                        memcpy( ocopy, opaque, olength );
                    }
                }
            } else if ( pdulen == isp->packet_len ) {
                /*  Common case -- exactly one packet.  No need to copy the
            opaque pointer.  */
                ocopy = opaque;
                opaque = NULL;
            }

            if ( ( rc = _Api_sessProcessPacket( sessp, sp, isp, transport,
                       ocopy, ocopy ? olength : 0, pptr,
                       pdulen ) ) ) {
                /*
                 * Something went wrong while processing this packet -- set the
                 * errno.
                 */
                if ( sp->s_snmp_errno != 0 ) {
                    API_SET_PRIOT_ERROR( sp->s_snmp_errno );
                }
            }

            /*  ocopy has been free()d by Api_sessProcessPacket by this point,
        so set it to NULL.  */

            ocopy = NULL;

            /*  Step past the packet we've just dealt with.  */

            pptr += pdulen;
            isp->packet_len -= pdulen;
        }

        /*  If we had more than one packet, then we were working with copies
        of the opaque pointer, so we still need to free() the opaque
        pointer itself.  */

        MEMORY_FREE( opaque );

        if ( isp->packet_len >= MAXIMUM_PACKET_SIZE ) {
            /*
             * Obviously this should never happen!
             */
            Logger_log( LOGGER_PRIORITY_ERR,
                "too large packet_len = %"
                "l"
                "u, dropping connection %d\n",
                isp->packet_len, transport->sock );
            transport->f_close( transport );
            /** XXX-rks: why no MEMORY_FREE(isp->packet); ?? */
            return -1;
        } else if ( isp->packet_len == 0 ) {
            /*
             * This is good: it means the packet buffer contained an integral
             * number of PDUs, so we don't have to save any data for next
             * time.  We can free() the buffer now to keep the memory
             * footprint down.
             */
            MEMORY_FREE( isp->packet );
            isp->packet_size = 0;
            isp->packet_len = 0;
            return rc;
        }

        /*
         * If we get here, then there is a partial packet of length
         * isp->packet_len bytes starting at pptr left over.  Move that to the
         * start of the buffer, and then realloc() the buffer down to size to
         * reduce the memory footprint.
         */

        memmove( isp->packet, pptr, isp->packet_len );
        DEBUG_MSGTL( ( "sessRead",
            "end: memmove(%p, %p, %"
            "l"
            "u); realloc(%p, %"
            "l"
            "u)\n",
            isp->packet, pptr, isp->packet_len,
            isp->packet, isp->packet_len ) );

        if ( ( rxbuf = ( u_char* )realloc( isp->packet, isp->packet_len ) ) == NULL ) {
            /*
             * I don't see why this should ever fail, but it's not a big deal.
             */
            DEBUG_MSGTL( ( "sessRead", "realloc() failed\n" ) );
        } else {
            DEBUG_MSGTL( ( "sessRead", "realloc() okay, old buffer %p, new %p\n",
                isp->packet, rxbuf ) );
            isp->packet = rxbuf;
            isp->packet_size = isp->packet_len;
        }
        return rc;
    } else {
        rc = _Api_sessProcessPacket( sessp, sp, isp, transport, opaque,
            olength, rxbuf, length );
        MEMORY_FREE( rxbuf );
        return rc;
    }
}

/*
 * returns 0 if success, -1 if fail
 */
int Api_sessRead( void* sessp, fd_set* fdset )
{
    int rc;
    Types_LargeFdSet lfdset;

    LargeFdSet_init( &lfdset, FD_SETSIZE );
    LargeFdSet_copyFdSetToLargeFdSet( &lfdset, fdset );
    rc = Api_sessRead2( sessp, &lfdset );
    LargeFdSet_cleanup( &lfdset );
    return rc;
}

int Api_sessRead2( void* sessp, Types_LargeFdSet* fdset )
{
    struct Api_SessionList_s* psl;
    Types_Session* pss;
    int rc;

    rc = Api_sessRead3( sessp, fdset );
    psl = ( struct Api_SessionList_s* )sessp;
    pss = psl->session;
    if ( rc && pss->s_snmp_errno ) {
        API_SET_PRIOT_ERROR( pss->s_snmp_errno );
    }
    return rc;
}

/**
 * Returns info about what snmp requires from a select statement.
 * numfds is the number of fds in the list that are significant.
 * All file descriptors opened for SNMP are OR'd into the fdset.
 * If activity occurs on any of these file descriptors, Api_read
 * should be called with that file descriptor set
 *
 * The timeout is the latest time that SNMP can wait for a timeout.  The
 * select should be done with the minimum time between timeout and any other
 * timeouts necessary.  This should be checked upon each invocation of select.
 * If a timeout is received, Api_timeout should be called to check if the
 * timeout was for SNMP.  (Api_timeout is idempotent)
 *
 * The value of block indicates how the timeout value is interpreted.
 * If block is true on input, the timeout value will be treated as undefined,
 * but it must be available for setting in Api_selectInfo.  On return,
 * block is set to true if the value returned for timeout is undefined;
 * when block is set to false, timeout may be used as a parmeter to 'select'.
 *
 * Api_selectInfo returns the number of open sockets.  (i.e. The number of
 * sessions open)
 *
 * @see See also Api_sessSelectInfo2Flags().
 */
int Api_selectInfo( int* numfds, fd_set* fdset, struct timeval* timeout,
    int* block )
{
    return Api_sessSelectInfo( NULL, numfds, fdset, timeout, block );
}

/**
 * @see See also Api_sessSelectInfo2Flags().
 */
int Api_selectInfo2( int* numfds, Types_LargeFdSet* fdset,
    struct timeval* timeout, int* block )
{
    return Api_sessSelectInfo2( NULL, numfds, fdset, timeout, block );
}

/**
 * @see See also Api_sessSelectInfo2Flags().
 */
int Api_sessSelectInfo( void* sessp, int* numfds, fd_set* fdset,
    struct timeval* timeout, int* block )
{
    return Api_sessSelectInfoFlags( sessp, numfds, fdset, timeout, block,
        SESSION_SELECT_NOFLAGS );
}

/**
 * @see See also Api_sessSelectInfo2Flags().
 */
int Api_sessSelectInfoFlags( void* sessp, int* numfds, fd_set* fdset,
    struct timeval* timeout, int* block, int flags )
{
    int rc;
    Types_LargeFdSet lfdset;

    LargeFdSet_init( &lfdset, FD_SETSIZE );
    LargeFdSet_copyFdSetToLargeFdSet( &lfdset, fdset );
    rc = Api_sessSelectInfo2Flags( sessp, numfds, &lfdset, timeout,
        block, flags );
    if ( LargeFdSet_copyLargeFdSetToFdSet( fdset, &lfdset ) < 0 ) {
        Logger_log( LOGGER_PRIORITY_ERR,
            "Use Api_sessSelectInfo2() for processing"
            " large file descriptors\n" );
    }
    LargeFdSet_cleanup( &lfdset );
    return rc;
}

/**
 * @see See also Api_sessSelectInfo2Flags().
 */
int Api_sessSelectInfo2( void* sessp, int* numfds, Types_LargeFdSet* fdset,
    struct timeval* timeout, int* block )
{
    return Api_sessSelectInfo2Flags( sessp, numfds, fdset, timeout, block,
        SESSION_SELECT_NOFLAGS );
}

/**
 * Compute/update the arguments to be passed to select().
 *
 * @param[in]     sessp   Which sessions to process: either a pointer to a
 *   specific session or NULL which means to process all sessions.
 * @param[in,out] numfds  On POSIX systems one more than the the largest file
 *   descriptor that is present in *fdset. On systems that use Winsock (MinGW
 *   and MSVC), do not use the value written into *numfds.
 * @param[in,out] fdset   A large file descriptor set to which all file
 *   descriptors will be added that are associated with one of the examined
 *   sessions.
 * @param[in,out] timeout On input, if *block = 1, the maximum time the caller
 *   will block while waiting for Net-SNMP activity. On output, if this function
 *   has set *block to 0, the maximum time the caller is allowed to wait before
 *   invoking the Net-SNMP processing functions (Api_read(), Api_timeout()
 *   and run_alarms()). If this function has set *block to 1, *timeout won't
 *   have been modified and no alarms are active.
 * @param[in,out] block   On input, whether the caller prefers to block forever
 *   when no alarms are active. On output, 0 means that no alarms are active
 *   nor that there is a timeout pending for any of the processed sessions.
 * @param[in]     flags   Either 0 or NETSNMP_SELECT_NOALARMS.
 *
 * @return Number of sessions processed by this function.
 *
 * @see See also agent_check_and_process() for an example of how to use this
 *   function.
 */
int Api_sessSelectInfo2Flags( void* sessp, int* numfds,
    Types_LargeFdSet* fdset,
    struct timeval* timeout, int* block, int flags )
{
    struct Api_SessionList_s *slp, *next = NULL;
    Api_RequestList* rp;
    struct timeval now, earliest, alarm_tm;
    int active = 0, requests = 0;
    int next_alarm = 0;

    timerclear( &earliest );

    /*
     * For each session examined, add its socket to the fdset,
     * and if it is the earliest timeout to expire, mark it as lowest.
     * If a single session is specified, do just for that session.
     */

    DEBUG_MSGTL( ( "sessSelect", "for %s session%s: ",
        sessp ? "single" : "all", sessp ? "" : "s" ) );

    for ( slp = ( sessp ? ( struct Api_SessionList_s* )sessp : api_sessions ); slp; slp = next ) {
        next = slp->next;

        if ( slp->transport == NULL ) {
            /*
             * Close in progress -- skip this one.
             */
            DEBUG_MSG( ( "sessSelect", "skip " ) );
            continue;
        }

        if ( slp->transport->sock == -1 ) {
            /*
             * This session was marked for deletion.
             */
            DEBUG_MSG( ( "sessSelect", "delete\n" ) );
            if ( sessp == NULL ) {
                Api_close( slp->session );
            } else {
                Api_sessClose( slp );
            }
            DEBUG_MSGTL( ( "sessSelect", "for %s session%s: ",
                sessp ? "single" : "all", sessp ? "" : "s" ) );
            continue;
        }

        DEBUG_MSG( ( "sessSelect", "%d ", slp->transport->sock ) );
        if ( ( slp->transport->sock + 1 ) > *numfds ) {
            *numfds = ( slp->transport->sock + 1 );
        }

        LARGEFDSET_FD_SET( slp->transport->sock, fdset );
        if ( slp->internal != NULL && slp->internal->requests ) {
            /*
             * Found another session with outstanding requests.
             */
            requests++;
            for ( rp = slp->internal->requests; rp; rp = rp->next_request ) {
                if ( !timerisset( &earliest )
                    || ( timerisset( &rp->expireM )
                           && timercmp( &rp->expireM, &earliest, < ) ) ) {
                    earliest = rp->expireM;
                    DEBUG_MSG( ( "verbose:sessSelect", "(to in %d.%06d sec) ",
                        ( int )earliest.tv_sec, ( int )earliest.tv_usec ) );
                }
            }
        }

        active++;
        if ( sessp ) {
            /*
             * Single session processing.
             */
            break;
        }
    }
    DEBUG_MSG( ( "sessSelect", "\n" ) );

    Tools_getMonotonicClock( &now );

    if ( DefaultStore_getBoolean( DsStorage_LIBRARY_ID,
             DsBool_ALARM_DONT_USE_SIG )
        && !( flags & SESSION_SELECT_NOALARMS ) ) {
        next_alarm = Alarm_getNextAlarmTime( &alarm_tm, &now );
        if ( next_alarm )
            DEBUG_MSGT( ( "sessSelect", "next alarm at %ld.%06ld sec\n",
                ( long )alarm_tm.tv_sec, ( long )alarm_tm.tv_usec ) );
    }
    if ( next_alarm == 0 && requests == 0 ) {
        /*
         * If none are active, skip arithmetic.
         */
        DEBUG_MSGT( ( "sessSelect", "blocking:no session requests or alarms.\n" ) );
        *block = 1; /* can block - timeout value is undefined if no requests */
        return active;
    }

    if ( next_alarm && ( !timerisset( &earliest ) || timercmp( &alarm_tm, &earliest, < ) ) )
        earliest = alarm_tm;

    TIME_SUB_TIME( &earliest, &now, &earliest );
    if ( earliest.tv_sec < 0 ) {
        time_t overdue_ms = -( earliest.tv_sec * 1000 + earliest.tv_usec / 1000 );
        if ( overdue_ms >= 10 )
            DEBUG_MSGT( ( "verbose:sessSelect", "timer overdue by %ld ms\n",
                ( long )overdue_ms ) );
        timerclear( &earliest );
    } else {
        DEBUG_MSGT( ( "verbose:sessSelect", "timer due in %d.%06d sec\n",
            ( int )earliest.tv_sec, ( int )earliest.tv_usec ) );
    }

    /*
     * if it was blocking before or our delta time is less, reset timeout
     */
    if ( ( *block || ( timercmp( &earliest, timeout, < ) ) ) ) {
        DEBUG_MSGT( ( "verbose:sessSelect",
            "setting timer to %d.%06d sec, clear block (was %d)\n",
            ( int )earliest.tv_sec, ( int )earliest.tv_usec, *block ) );
        *timeout = earliest;
        *block = 0;
    }
    return active;
}

/*
 * Api_timeout should be called whenever the timeout from Api_selectInfo
 * expires, but it is idempotent, so Api_timeout can be polled (probably a
 * cpu expensive proposition).  Api_timeout checks to see if any of the
 * sessions have an outstanding request that has timed out.  If it finds one
 * (or more), and that pdu has more retries available, a new packet is formed
 * from the pdu and is resent.  If there are no more retries available, the
 *  callback for the session is used to alert the user of the timeout.
 */
void Api_timeout( void )
{
    struct Api_SessionList_s* slp;
    Mutex_lock( MTSUPPORT_LIBRARY_ID, MTSUPPORT_LIB_SESSION );
    for ( slp = api_sessions; slp; slp = slp->next ) {
        Api_sessTimeout( ( void* )slp );
    }
    Mutex_unlock( MTSUPPORT_LIBRARY_ID, MTSUPPORT_LIB_SESSION );
}

static int
_Api_resendRequest( struct Api_SessionList_s* slp, Api_RequestList* rp,
    int incr_retries )
{
    struct Api_InternalSession_s* isp;
    Types_Session* sp;
    Transport_Transport* transport;
    u_char *pktbuf = NULL, *packet = NULL;
    size_t pktbuf_len = 0, offset = 0, length = 0;
    struct timeval tv, now;
    int result = 0;

    sp = slp->session;
    isp = slp->internal;
    transport = slp->transport;
    if ( !sp || !isp || !transport ) {
        DEBUG_MSGTL( ( "sessRead", "resend fail: closing...\n" ) );
        return 0;
    }

    if ( ( pktbuf = ( u_char* )malloc( 2048 ) ) == NULL ) {
        DEBUG_MSGTL( ( "sessResend",
            "couldn't malloc initial packet buffer\n" ) );
        return 0;
    } else {
        pktbuf_len = 2048;
    }

    if ( incr_retries ) {
        rp->retries++;
    }

    /*
     * Always increment msgId for resent messages.
     */
    rp->pdu->msgid = rp->message_id = Api_getNextMsgid();

    if ( isp->hook_realloc_build ) {
        result = isp->hook_realloc_build( sp, rp->pdu,
            &pktbuf, &pktbuf_len, &offset );

        packet = pktbuf;
        length = offset;
    } else if ( isp->hook_build ) {
        packet = pktbuf;
        length = pktbuf_len;
        result = isp->hook_build( sp, rp->pdu, pktbuf, &length );
    } else {
        if ( DefaultStore_getBoolean( DsStorage_LIBRARY_ID, DsBool_REVERSE_ENCODE ) ) {
            result = Api_build( &pktbuf, &pktbuf_len, &offset, sp, rp->pdu );
            packet = pktbuf + pktbuf_len - offset;
            length = offset;
        } else {
            packet = pktbuf;
            length = pktbuf_len;
            result = Api_build( &pktbuf, &length, &offset, sp, rp->pdu );
        }
    }

    if ( result < 0 ) {
        /*
         * This should never happen.
         */
        DEBUG_MSGTL( ( "sessResend", "encoding failure\n" ) );
        MEMORY_FREE( pktbuf );
        return -1;
    }

    DEBUG_MSGTL( ( "sessProcessPacket", "resending message id#%ld reqid#%ld "
                                        "rp_reqid#%ld rp_msgid#%ld len %"
                                        "l"
                                        "u\n",
        rp->pdu->msgid, rp->pdu->reqid, rp->request_id, rp->message_id, length ) );
    result = Transport_send( transport, packet, length,
        &( rp->pdu->transportData ),
        &( rp->pdu->transportDataLength ) );

    /*
     * We are finished with the local packet buffer, if we allocated one (due
     * to there being no saved packet).
     */

    if ( pktbuf != NULL ) {
        MEMORY_FREE( pktbuf );
        packet = NULL;
    }

    if ( result < 0 ) {
        sp->s_snmp_errno = ErrorCode_BAD_SENDTO;
        sp->s_errno = errno;
        Api_setDetail( strerror( errno ) );
        return -1;
    } else {
        Tools_getMonotonicClock( &now );
        tv = now;
        rp->timeM = tv;
        tv.tv_usec += rp->timeout;
        tv.tv_sec += tv.tv_usec / 1000000L;
        tv.tv_usec %= 1000000L;
        rp->expireM = tv;
    }
    return 0;
}

void Api_sessTimeout( void* sessp )
{
    struct Api_SessionList_s* slp = ( struct Api_SessionList_s* )sessp;
    Types_Session* sp;
    struct Api_InternalSession_s* isp;
    Api_RequestList *rp, *orp = NULL, *freeme = NULL;
    struct timeval now;
    Types_CallbackFT callback;
    void* magic;
    struct Secmod_Def_s* sptr;

    sp = slp->session;
    isp = slp->internal;
    if ( !sp || !isp ) {
        DEBUG_MSGTL( ( "sessRead", "timeout fail: closing...\n" ) );
        return;
    }

    Tools_getMonotonicClock( &now );

    /*
     * For each request outstanding, check to see if it has expired.
     */
    for ( rp = isp->requests; rp; rp = rp->next_request ) {
        if ( freeme != NULL ) {
            /*
             * frees rp's after the for loop goes on to the next_request
             */
            free( ( char* )freeme );
            freeme = NULL;
        }

        if ( ( timercmp( &rp->expireM, &now, < ) ) ) {
            if ( ( sptr = Secmod_find( rp->pdu->securityModel ) ) != NULL && sptr->pdu_timeout != NULL ) {
                /*
                 * call security model if it needs to know about this
                 */
                ( *sptr->pdu_timeout )( rp->pdu );
            }

            /*
             * this timer has expired
             */
            if ( rp->retries >= sp->retries ) {
                if ( rp->callback ) {
                    callback = rp->callback;
                    magic = rp->cb_data;
                } else {
                    callback = sp->callback;
                    magic = sp->callback_magic;
                }

                /*
                 * No more chances, delete this entry
                 */
                if ( callback ) {
                    callback( API_CALLBACK_OP_TIMED_OUT, sp,
                        rp->pdu->reqid, rp->pdu, magic );
                }
                if ( orp )
                    orp->next_request = rp->next_request;
                else
                    isp->requests = rp->next_request;
                if ( isp->requestsEnd == rp )
                    isp->requestsEnd = orp;
                Api_freePdu( rp->pdu );
                freeme = rp;
                continue; /* don't update orp below */
            } else {
                if ( _Api_resendRequest( slp, rp, TRUE ) ) {
                    break;
                }
            }
        }
        orp = rp;
    }

    if ( freeme != NULL ) {
        free( ( char* )freeme );
        freeme = NULL;
    }
}

/*
 * lexicographical compare two object identifiers.
 * * Returns -1 if name1 < name2,
 * *          0 if name1 = name2,
 * *          1 if name1 > name2
 * *
 * * Caution: this method is called often by
 * *          command responder applications (ie, agent).
 */
int Api_oidNcompare( const oid* in_name1,
    size_t len1,
    const oid* in_name2, size_t len2, size_t max_len )
{
    register int len;
    register const oid* name1 = in_name1;
    register const oid* name2 = in_name2;
    size_t min_len;

    /*
     * len = minimum of len1 and len2
     */
    if ( len1 < len2 )
        min_len = len1;
    else
        min_len = len2;

    if ( min_len > max_len )
        min_len = max_len;

    len = min_len;

    /*
     * find first non-matching OID
     */
    while ( len-- > 0 ) {
        /*
         * these must be done in seperate comparisons, since
         * subtracting them and using that result has problems with
         * subids > 2^31.
         */
        if ( *( name1 ) != *( name2 ) ) {
            if ( *( name1 ) < *( name2 ) )
                return -1;
            return 1;
        }
        name1++;
        name2++;
    }

    if ( min_len != max_len ) {
        /*
         * both OIDs equal up to length of shorter OID
         */
        if ( len1 < len2 )
            return -1;
        if ( len2 < len1 )
            return 1;
    }

    return 0;
}

/** lexicographical compare two object identifiers.
 *
 * Caution: this method is called often by
 *          command responder applications (ie, agent).
 *
 * @return -1 if name1 < name2, 0 if name1 = name2, 1 if name1 > name2
 */
int Api_oidCompare( const oid* in_name1,
    size_t len1, const oid* in_name2, size_t len2 )
{
    register int len;
    register const oid* name1 = in_name1;
    register const oid* name2 = in_name2;

    /*
     * len = minimum of len1 and len2
     */
    if ( len1 < len2 )
        len = len1;
    else
        len = len2;
    /*
     * find first non-matching OID
     */
    while ( len-- > 0 ) {
        /*
         * these must be done in seperate comparisons, since
         * subtracting them and using that result has problems with
         * subids > 2^31.
         */
        if ( *( name1 ) != *( name2 ) ) {
            if ( *( name1 ) < *( name2 ) )
                return -1;
            return 1;
        }
        name1++;
        name2++;
    }
    /*
     * both OIDs equal up to length of shorter OID
     */
    if ( len1 < len2 )
        return -1;
    if ( len2 < len1 )
        return 1;
    return 0;
}

/** lexicographical compare two object identifiers and return the point where they differ
 *
 * Caution: this method is called often by
 *          command responder applications (ie, agent).
 *
 * @return -1 if name1 < name2, 0 if name1 = name2, 1 if name1 > name2 and offpt = len where name1 != name2
 */
int Api_oidCompareLl( const oid* in_name1,
    size_t len1, const oid* in_name2, size_t len2,
    size_t* offpt )
{
    register int len;
    register const oid* name1 = in_name1;
    register const oid* name2 = in_name2;
    int initlen;

    /*
     * len = minimum of len1 and len2
     */
    if ( len1 < len2 )
        initlen = len = len1;
    else
        initlen = len = len2;
    /*
     * find first non-matching OID
     */
    while ( len-- > 0 ) {
        /*
         * these must be done in seperate comparisons, since
         * subtracting them and using that result has problems with
         * subids > 2^31.
         */
        if ( *( name1 ) != *( name2 ) ) {
            *offpt = initlen - len;
            if ( *( name1 ) < *( name2 ) )
                return -1;
            return 1;
        }
        name1++;
        name2++;
    }
    /*
     * both OIDs equal up to length of shorter OID
     */
    *offpt = initlen - len;
    if ( len1 < len2 )
        return -1;
    if ( len2 < len1 )
        return 1;
    return 0;
}

/** Compares 2 OIDs to determine if they are equal up until the shortest length.
 * @param in_name1 A pointer to the first oid.
 * @param len1     length of the first OID (in segments, not bytes)
 * @param in_name2 A pointer to the second oid.
 * @param len2     length of the second OID (in segments, not bytes)
 * @return 0 if they are equal, 1 if in_name1 is > in_name2, or -1 if <.
 */
int Api_oidtreeCompare( const oid* in_name1,
    size_t len1, const oid* in_name2, size_t len2 )
{
    int len = ( ( len1 < len2 ) ? len1 : len2 );

    return ( Api_oidCompare( in_name1, len, in_name2, len ) );
}

int Api_oidsubtreeCompare( const oid* in_name1,
    size_t len1, const oid* in_name2, size_t len2 )
{
    int len = ( ( len1 < len2 ) ? len1 : len2 );

    return ( Api_oidCompare( in_name1, len1, in_name2, len ) );
}

/** Compares 2 OIDs to determine if they are exactly equal.
 *  This should be faster than doing a Api_oidCompare for different
 *  length OIDs, since the length is checked first and if != returns
 *  immediately.  Might be very slighly faster if lengths are ==.
 * @param in_name1 A pointer to the first oid.
 * @param len1     length of the first OID (in segments, not bytes)
 * @param in_name2 A pointer to the second oid.
 * @param len2     length of the second OID (in segments, not bytes)
 * @return 0 if they are equal, 1 if they are not.
 */
int Api_oidEquals( const oid* in_name1,
    size_t len1, const oid* in_name2, size_t len2 )
{
    register const oid* name1 = in_name1;
    register const oid* name2 = in_name2;
    register int len = len1;

    /*
     * len = minimum of len1 and len2
     */
    if ( len1 != len2 )
        return 1;
    /*
     * Handle 'null' OIDs
     */
    if ( len1 == 0 )
        return 0; /* Two null OIDs are (trivially) the same */
    if ( !name1 || !name2 )
        return 1; /* Otherwise something's wrong, so report a non-match */
    /*
     * find first non-matching OID
     */
    while ( len-- > 0 ) {
        /*
         * these must be done in seperate comparisons, since
         * subtracting them and using that result has problems with
         * subids > 2^31.
         */
        if ( *( name1++ ) != *( name2++ ) )
            return 1;
    }
    return 0;
}

/** Identical to Api_oidEquals, except only the length up to len1 is compared.
 * Functionally, this determines if in_name2 is equal or a subtree of in_name1
 * @param in_name1 A pointer to the first oid.
 * @param len1     length of the first OID (in segments, not bytes)
 * @param in_name2 A pointer to the second oid.
 * @param len2     length of the second OID (in segments, not bytes)
 * @return 0 if one is a common prefix of the other.
 */
int Api_oidIsSubtree( const oid* in_name1,
    size_t len1, const oid* in_name2, size_t len2 )
{
    if ( len1 > len2 )
        return 1;

    if ( memcmp( in_name1, in_name2, len1 * sizeof( oid ) ) )
        return 1;

    return 0;
}

/** Given two OIDs, determine the common prefix to them both.
 * @param in_name1 A pointer to the first oid.
 * @param len1     Length of the first oid.
 * @param in_name2 A pointer to the second oid.
 * @param len2     Length of the second oid.
 * @return         length of common prefix
 *                 0 if no common prefix, -1 on error.
 */
int Api_oidFindPrefix( const oid* in_name1, size_t len1,
    const oid* in_name2, size_t len2 )
{
    int i;
    size_t min_size;

    if ( !in_name1 || !in_name2 || !len1 || !len2 )
        return -1;

    if ( in_name1[ 0 ] != in_name2[ 0 ] )
        return 0; /* No match */
    min_size = UTILITIES_MIN_VALUE( len1, len2 );
    for ( i = 0; i < ( int )min_size; i++ ) {
        if ( in_name1[ i ] != in_name2[ i ] )
            return i; /* '�' is the first differing subidentifier
                            So the common prefix is 0..(i-1), of length i */
    }
    return min_size; /* The shorter OID is a prefix of the longer, and
                           hence is precisely the common prefix of the two.
                           Return its length. */
}

static int _Api_checkRange( struct Parse_Tree_s* tp, long ltmp, int* resptr,
    const char* errmsg )
{
    char* cp = NULL;
    char* temp = NULL;
    int temp_len = 0;
    int check = !DefaultStore_getBoolean( DsStorage_LIBRARY_ID,
        DsBool_DONT_CHECK_RANGE );

    if ( check && tp && tp->ranges ) {
        struct Parse_RangeList_s* rp = tp->ranges;
        while ( rp ) {
            if ( rp->low <= ltmp && ltmp <= rp->high )
                break;
            /* Allow four digits per range value */
            temp_len += ( ( rp->low != rp->high ) ? 14 : 8 );
            rp = rp->next;
        }
        if ( !rp ) {
            *resptr = ErrorCode_RANGE;
            temp = ( char* )malloc( temp_len + strlen( errmsg ) + 7 );
            if ( temp ) {
                /* Append the Display Hint range information to the error message */
                sprintf( temp, "%s . {", errmsg );
                cp = temp + ( strlen( temp ) );
                for ( rp = tp->ranges; rp; rp = rp->next ) {
                    if ( rp->low != rp->high )
                        sprintf( cp, "(%d..%d), ", rp->low, rp->high );
                    else
                        sprintf( cp, "(%d), ", rp->low );
                    cp += strlen( cp );
                }
                *( cp - 2 ) = '}'; /* Replace the final comma with a '}' */
                *( cp - 1 ) = 0;
                Api_setDetail( temp );
                free( temp );
            }
            return 0;
        }
    }
    free( temp );
    return 1;
}

/*
 * Add a variable with the requested name to the end of the list of
 * variables for this pdu.
 */
Types_VariableList*
Api_pduAddVariable( Types_Pdu* pdu,
    const oid* name,
    size_t name_length,
    u_char type, const void* value, size_t len )
{
    return Api_varlistAddVariable( &pdu->variables, name, name_length,
        type, value, len );
}

/*
 * Add a variable with the requested name to the end of the list of
 * variables for this pdu.
 */
Types_VariableList*
Api_varlistAddVariable( Types_VariableList** varlist,
    const oid* name,
    size_t name_length,
    u_char type, const void* value, size_t len )
{
    Types_VariableList *vars, *vtmp;
    int rc;

    if ( varlist == NULL )
        return NULL;

    vars = MEMORY_MALLOC_TYPEDEF( Types_VariableList );
    if ( vars == NULL )
        return NULL;

    vars->type = type;

    rc = Client_setVarValue( vars, value, len );
    if ( ( 0 != rc ) || ( name != NULL && Client_setVarObjid( vars, name, name_length ) ) ) {
        Api_freeVar( vars );
        return NULL;
    }

    /*
     * put only qualified variable onto varlist
     */
    if ( *varlist == NULL ) {
        *varlist = vars;
    } else {
        for ( vtmp = *varlist; vtmp->nextVariable;
              vtmp = vtmp->nextVariable )
            ;

        vtmp->nextVariable = vars;
    }

    return vars;
}

/*
 * Add a variable with the requested name to the end of the list of
 * variables for this pdu.
 * Returns:
 * may set these error types :
 * ErrorCode_RANGE - type, value, or length not found or out of range
 * ErrorCode_VALUE - value is not correct
 * ErrorCode_VAR_TYPE - type is not correct
 * ErrorCode_BAD_NAME - name is not found
 *
 * returns 0 if success, error if failure.
 */
int Api_addVar( Types_Pdu* pdu,
    const oid* name, size_t name_length, char type, const char* value )
{
    char* st;
    const char* cp;
    char *ecp, *vp;
    int result = ErrorCode_SUCCESS;
    int check = !DefaultStore_getBoolean( DsStorage_LIBRARY_ID,
        DsBool_DONT_CHECK_RANGE );
    int do_hint = !DefaultStore_getBoolean( DsStorage_LIBRARY_ID,
        DsBool_NO_DISPLAY_HINT );
    u_char* hintptr;
    struct Parse_Tree_s* tp;
    u_char* buf = NULL;
    const u_char* buf_ptr = NULL;
    size_t buf_len = 0, value_len = 0, tint;
    in_addr_t atmp;
    long ltmp;
    int itmp;
    struct Parse_EnumList_s* ep;
    double dtmp;
    float ftmp;
    struct Asn01_Counter64_s c64tmp;

    tp = Mib_getTree( name, name_length, Mib_getTreeHead() );
    if ( !tp || !tp->type || tp->type > PARSE_TYPE_SIMPLE_LAST ) {
        check = 0;
    }
    if ( !( tp && tp->hint ) )
        do_hint = 0;

    if ( tp && type == '=' ) {
        /*
         * generic assignment - let the tree node decide value format
         */
        switch ( tp->type ) {
        case PARSE_TYPE_INTEGER:
        case PARSE_TYPE_INTEGER32:
            type = 'i';
            break;
        case PARSE_TYPE_GAUGE:
        case PARSE_TYPE_UNSIGNED32:
            type = 'u';
            break;
        case PARSE_TYPE_UINTEGER:
            type = '3';
            break;
        case PARSE_TYPE_COUNTER:
            type = 'c';
            break;
        case PARSE_TYPE_COUNTER64:
            type = 'C';
            break;
        case PARSE_TYPE_TIMETICKS:
            type = 't';
            break;
        case PARSE_TYPE_OCTETSTR:
            type = 's';
            break;
        case PARSE_TYPE_BITSTRING:
            type = 'b';
            break;
        case PARSE_TYPE_IPADDR:
            type = 'a';
            break;
        case PARSE_TYPE_OBJID:
            type = 'o';
            break;
        }
    }

    switch ( type ) {
    case 'i':
        if ( check && tp->type != PARSE_TYPE_INTEGER
            && tp->type != PARSE_TYPE_INTEGER32 ) {
            value = "INTEGER";
            result = ErrorCode_VALUE;
            goto type_error;
        }
        if ( !*value )
            goto fail;
        ltmp = strtol( value, &ecp, 10 );
        if ( *ecp ) {
            ep = tp ? tp->enums : NULL;
            while ( ep ) {
                if ( strcmp( value, ep->label ) == 0 ) {
                    ltmp = ep->value;
                    break;
                }
                ep = ep->next;
            }
            if ( !ep ) {
                result = ErrorCode_RANGE; /* ?? or ErrorCode_VALUE; */
                Api_setDetail( value );
                break;
            }
        }

        if ( !_Api_checkRange( tp, ltmp, &result, value ) )
            break;
        Api_pduAddVariable( pdu, name, name_length, ASN01_INTEGER,
            &ltmp, sizeof( ltmp ) );
        break;

    case 'u':
        if ( check && tp->type != PARSE_TYPE_GAUGE && tp->type != PARSE_TYPE_UNSIGNED32 ) {
            value = "Unsigned32";
            result = ErrorCode_VALUE;
            goto type_error;
        }
        ltmp = strtoul( value, &ecp, 10 );
        if ( *value && !*ecp )
            Api_pduAddVariable( pdu, name, name_length, ASN01_UNSIGNED,
                &ltmp, sizeof( ltmp ) );
        else
            goto fail;
        break;

    case '3':
        if ( check && tp->type != PARSE_TYPE_UINTEGER ) {
            value = "UInteger32";
            result = ErrorCode_VALUE;
            goto type_error;
        }
        ltmp = strtoul( value, &ecp, 10 );
        if ( *value && !*ecp )
            Api_pduAddVariable( pdu, name, name_length, ASN01_UINTEGER,
                &ltmp, sizeof( ltmp ) );
        else
            goto fail;
        break;

    case 'c':
        if ( check && tp->type != PARSE_TYPE_COUNTER ) {
            value = "Counter32";
            result = ErrorCode_VALUE;
            goto type_error;
        }
        ltmp = strtoul( value, &ecp, 10 );
        if ( *value && !*ecp )
            Api_pduAddVariable( pdu, name, name_length, ASN01_COUNTER,
                &ltmp, sizeof( ltmp ) );
        else
            goto fail;
        break;

    case 'C':
        if ( check && tp->type != PARSE_TYPE_COUNTER64 ) {
            value = "Counter64";
            result = ErrorCode_VALUE;
            goto type_error;
        }
        if ( Int64_read64( &c64tmp, value ) )
            Api_pduAddVariable( pdu, name, name_length, ASN01_COUNTER64,
                &c64tmp, sizeof( c64tmp ) );
        else
            goto fail;
        break;

    case 't':
        if ( check && tp->type != PARSE_TYPE_TIMETICKS ) {
            value = "Timeticks";
            result = ErrorCode_VALUE;
            goto type_error;
        }
        ltmp = strtoul( value, &ecp, 10 );
        if ( *value && !*ecp )
            Api_pduAddVariable( pdu, name, name_length, ASN01_TIMETICKS,
                &ltmp, sizeof( long ) );
        else
            goto fail;
        break;

    case 'a':
        if ( check && tp->type != PARSE_TYPE_IPADDR ) {
            value = "IpAddress";
            result = ErrorCode_VALUE;
            goto type_error;
        }
        atmp = inet_addr( value );
        if ( atmp != ( in_addr_t )-1 || !strcmp( value, "255.255.255.255" ) )
            Api_pduAddVariable( pdu, name, name_length, ASN01_IPADDRESS,
                &atmp, sizeof( atmp ) );
        else
            goto fail;
        break;

    case 'o':
        if ( check && tp->type != PARSE_TYPE_OBJID ) {
            value = "OBJECT IDENTIFIER";
            result = ErrorCode_VALUE;
            goto type_error;
        }
        if ( ( buf = ( u_char* )malloc( sizeof( oid ) * TYPES_MAX_OID_LEN ) ) == NULL ) {
            result = ErrorCode_MALLOC;
        } else {
            tint = TYPES_MAX_OID_LEN;
            if ( Mib_parseOid( value, ( oid* )buf, &tint ) ) {
                Api_pduAddVariable( pdu, name, name_length, ASN01_OBJECT_ID,
                    buf, sizeof( oid ) * tint );
            } else {
                result = api_priotErrno; /*MTCRITICAL_RESOURCE */
            }
        }
        break;

    case 's':
    case 'x':
    case 'd':
        if ( check && tp->type != PARSE_TYPE_OCTETSTR && tp->type != PARSE_TYPE_BITSTRING ) {
            value = "OCTET STRING";
            result = ErrorCode_VALUE;
            goto type_error;
        }
        if ( 's' == type && do_hint && !Mib_parseOctetHint( tp->hint, value, &hintptr, &itmp ) ) {
            if ( _Api_checkRange( tp, itmp, &result, "Value does not match DISPLAY-HINT" ) ) {
                Api_pduAddVariable( pdu, name, name_length, ASN01_OCTET_STR,
                    hintptr, itmp );
            }
            MEMORY_FREE( hintptr );
            hintptr = buf;
            break;
        }
        if ( type == 'd' ) {
            if ( !Convert_integerStringToBinaryString( &buf, &buf_len, &value_len, 1, value ) ) {
                result = ErrorCode_VALUE;
                Api_setDetail( value );
                break;
            }
            buf_ptr = buf;
        } else if ( type == 'x' ) {
            if ( !Convert_hexStringToBinaryStringWrapper( &buf, &buf_len, &value_len, 1, value ) ) {
                result = ErrorCode_VALUE;
                Api_setDetail( value );
                break;
            }
            /* initialize itmp value so that range check below works */
            itmp = value_len;
            buf_ptr = buf;
        } else if ( type == 's' ) {
            buf_ptr = ( const u_char* )value;
            value_len = strlen( value );
        }
        if ( !_Api_checkRange( tp, value_len, &result, "Bad string length" ) )
            break;
        Api_pduAddVariable( pdu, name, name_length, ASN01_OCTET_STR,
            buf_ptr, value_len );
        break;

    case 'n':
        Api_pduAddVariable( pdu, name, name_length, ASN01_NULL, NULL, 0 );
        break;

    case 'b':
        if ( check && ( tp->type != PARSE_TYPE_BITSTRING || !tp->enums ) ) {
            value = "BITS";
            result = ErrorCode_VALUE;
            goto type_error;
        }
        tint = 0;
        if ( ( buf = ( u_char* )malloc( 256 ) ) == NULL ) {
            result = ErrorCode_MALLOC;
            break;
        } else {
            buf_len = 256;
            memset( buf, 0, buf_len );
        }

        for ( ep = tp ? tp->enums : NULL; ep; ep = ep->next ) {
            if ( ep->value / 8 >= ( int )tint ) {
                tint = ep->value / 8 + 1;
            }
        }

        vp = strdup( value );
        for ( cp = strtok_r( vp, " ,\t", &st ); cp; cp = strtok_r( NULL, " ,\t", &st ) ) {
            int ix, bit;

            ltmp = strtoul( cp, &ecp, 0 );
            if ( *ecp != 0 ) {
                for ( ep = tp ? tp->enums : NULL; ep != NULL; ep = ep->next ) {
                    if ( strncmp( ep->label, cp, strlen( ep->label ) ) == 0 ) {
                        break;
                    }
                }
                if ( ep != NULL ) {
                    ltmp = ep->value;
                } else {
                    result = ErrorCode_RANGE; /* ?? or ErrorCode_VALUE; */
                    Api_setDetail( cp );
                    MEMORY_FREE( buf );
                    MEMORY_FREE( vp );
                    goto out;
                }
            }

            ix = ltmp / 8;
            if ( ix >= ( int )tint ) {
                tint = ix + 1;
            }
            if ( ix >= ( int )buf_len && !Memory_reallocIncrease( &buf, &buf_len ) ) {
                result = ErrorCode_MALLOC;
                break;
            }
            bit = 0x80 >> ltmp % 8;
            buf[ ix ] |= bit;
        }
        MEMORY_FREE( vp );
        Api_pduAddVariable( pdu, name, name_length, ASN01_OCTET_STR,
            buf, tint );
        break;

    case 'U':
        if ( Int64_read64( &c64tmp, value ) )
            Api_pduAddVariable( pdu, name, name_length, ASN01_OPAQUE_U64,
                &c64tmp, sizeof( c64tmp ) );
        else
            goto fail;
        break;

    case 'I':
        if ( Int64_read64( &c64tmp, value ) )
            Api_pduAddVariable( pdu, name, name_length, ASN01_OPAQUE_I64,
                &c64tmp, sizeof( c64tmp ) );
        else
            goto fail;
        break;

    case 'F':
        if ( sscanf( value, "%f", &ftmp ) == 1 )
            Api_pduAddVariable( pdu, name, name_length, ASN01_OPAQUE_FLOAT,
                &ftmp, sizeof( ftmp ) );
        else
            goto fail;
        break;

    case 'D':
        if ( sscanf( value, "%lf", &dtmp ) == 1 )
            Api_pduAddVariable( pdu, name, name_length, ASN01_OPAQUE_DOUBLE,
                &dtmp, sizeof( dtmp ) );
        else
            goto fail;
        break;

    default:
        result = ErrorCode_VAR_TYPE;
        buf = ( u_char* )calloc( 1, 4 );
        if ( buf != NULL ) {
            sprintf( ( char* )buf, "\"%c\"", type );
            Api_setDetail( ( char* )buf );
        }
        break;
    }

    MEMORY_FREE( buf );
    API_SET_PRIOT_ERROR( result );
    return result;

type_error : {
    char error_msg[ 256 ];
    char undef_msg[ 32 ];
    const char* var_type;
    switch ( tp->type ) {
    case PARSE_TYPE_OBJID:
        var_type = "OBJECT IDENTIFIER";
        break;
    case PARSE_TYPE_OCTETSTR:
        var_type = "OCTET STRING";
        break;
    case PARSE_TYPE_INTEGER:
        var_type = "INTEGER";
        break;
    case PARSE_TYPE_NETADDR:
        var_type = "NetworkAddress";
        break;
    case PARSE_TYPE_IPADDR:
        var_type = "IpAddress";
        break;
    case PARSE_TYPE_COUNTER:
        var_type = "Counter32";
        break;
    case PARSE_TYPE_GAUGE:
        var_type = "Gauge32";
        break;
    case PARSE_TYPE_TIMETICKS:
        var_type = "Timeticks";
        break;
    case PARSE_TYPE_OPAQUE:
        var_type = "Opaque";
        break;
    case PARSE_TYPE_NULL:
        var_type = "Null";
        break;
    case PARSE_TYPE_COUNTER64:
        var_type = "Counter64";
        break;
    case PARSE_TYPE_BITSTRING:
        var_type = "BITS";
        break;
    case PARSE_TYPE_NSAPADDRESS:
        var_type = "NsapAddress";
        break;
    case PARSE_TYPE_UINTEGER:
        var_type = "UInteger";
        break;
    case PARSE_TYPE_UNSIGNED32:
        var_type = "Unsigned32";
        break;
    case PARSE_TYPE_INTEGER32:
        var_type = "Integer32";
        break;
    default:
        sprintf( undef_msg, "TYPE_%d", tp->type );
        var_type = undef_msg;
    }
    snprintf( error_msg, sizeof( error_msg ),
        "Type of attribute is %s, not %s", var_type, value );
    error_msg[ sizeof( error_msg ) - 1 ] = 0;
    result = ErrorCode_VAR_TYPE;
    Api_setDetail( error_msg );
    goto out;
}
fail:
    result = ErrorCode_VALUE;
    Api_setDetail( value );
out:
    API_SET_PRIOT_ERROR( result );
    return result;
}

/*
 * returns NULL or internal pointer to session
 * use this pointer for the other snmp_sess* routines,
 * which guarantee action will occur ONLY for this given session.
 */
void* Api_sessPointer( Types_Session* session )
{
    struct Api_SessionList_s* slp;

    Mutex_lock( MTSUPPORT_LIBRARY_ID, MTSUPPORT_LIB_SESSION );
    for ( slp = api_sessions; slp; slp = slp->next ) {
        if ( slp->session == session ) {
            break;
        }
    }
    Mutex_unlock( MTSUPPORT_LIBRARY_ID, MTSUPPORT_LIB_SESSION );

    if ( slp == NULL ) {
        api_priotErrno = ErrorCode_BAD_SESSION; /*MTCRITICAL_RESOURCE */
        return ( NULL );
    }
    return ( ( void* )slp );
}

/*
 * Input : an opaque pointer, returned by Api_sessOpen.
 * returns NULL or pointer to session.
 */
Types_Session*
Api_sessSession( void* sessp )
{
    struct Api_SessionList_s* slp = ( struct Api_SessionList_s* )sessp;
    if ( slp == NULL )
        return ( NULL );
    return ( slp->session );
}

/**
 * Look up a session that already may have been closed.
 *
 * @param sessp Opaque pointer, returned by Api_sessOpen.
 *
 * @return Pointer to session upon success or NULL upon failure.
 *
 * @see Api_sessSession()
 */
Types_Session*
Api_sessSessionLookup( void* sessp )
{
    struct Api_SessionList_s* slp;

    Mutex_lock( MTSUPPORT_LIBRARY_ID, MTSUPPORT_LIB_SESSION );
    for ( slp = api_sessions; slp; slp = slp->next ) {
        if ( slp == sessp ) {
            break;
        }
    }
    Mutex_unlock( MTSUPPORT_LIBRARY_ID, MTSUPPORT_LIB_SESSION );

    return ( Types_Session* )slp;
}

/*
 * Api_sessTransport: takes an opaque pointer (as returned by
 * Api_sessOpen or Api_sessPointer) and returns the corresponding
 * Transport_Transport pointer (or NULL if the opaque pointer does not correspond
 * to an active internal session).
 */

Transport_Transport*
Api_sessTransport( void* sessp )
{
    struct Api_SessionList_s* slp = ( struct Api_SessionList_s* )sessp;
    if ( slp == NULL ) {
        return NULL;
    } else {
        return slp->transport;
    }
}

/*
 * Api_sessTransport_set: set the transport pointer for the opaque
 * session pointer sp.
 */

void Api_sessTransportSet( void* sp, Transport_Transport* t )
{
    struct Api_SessionList_s* slp = ( struct Api_SessionList_s* )sp;
    if ( slp != NULL ) {
        slp->transport = t;
    }
}

/*
 * Api_duplicateObjid: duplicates (mallocs) an objid based on the
 * input objid
 */
oid* Api_duplicateObjid( const oid* objToCopy, size_t objToCopyLen )
{
    oid* returnOid;
    if ( objToCopy != NULL && objToCopyLen != 0 ) {
        returnOid = ( oid* )malloc( objToCopyLen * sizeof( oid ) );
        if ( returnOid ) {
            memcpy( returnOid, objToCopy, objToCopyLen * sizeof( oid ) );
        }
    } else
        returnOid = NULL;
    return returnOid;
}

/*
 * generic statistics counter functions
 */
static u_int _api_statistics[ API_STAT_MAX_STATS ];

u_int Api_incrementStatistic( int which )
{
    if ( which >= 0 && which < API_STAT_MAX_STATS ) {
        _api_statistics[ which ]++;
        return _api_statistics[ which ];
    }
    return 0;
}

u_int Api_incrementStatisticBy( int which, int count )
{
    if ( which >= 0 && which < API_STAT_MAX_STATS ) {
        _api_statistics[ which ] += count;
        return _api_statistics[ which ];
    }
    return 0;
}

u_int Api_getStatistic( int which )
{
    if ( which >= 0 && which < API_STAT_MAX_STATS )
        return _api_statistics[ which ];
    return 0;
}

void Api_initStatistics( void )
{
    memset( _api_statistics, 0, sizeof( _api_statistics ) );
}
