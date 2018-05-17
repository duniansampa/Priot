#ifndef TYPES_H
#define TYPES_H

/**
 *  Definitions of data structures, used within the library API.
*/

#include "Asn01.h"
//#include <time.h>

#define TYPES_OID_MAX_SUBID 0xFFFFFFFFUL

/*
 * Note: on POSIX-compliant systems, pid_t is defined in <sys/types.h>.
 * And if pid_t has not been defined in <sys/types.h>, AC_TYPE_PID_T ensures
 * that a pid_t definition is present in Generals.h.
 */
typedef pid_t Types_PidT;
#define NETSNMP_NO_SUCH_PROCESS -1

//* The type in_addr_t must match the type of sockaddr_in::sin_addr.
typedef u_int Types_SocklenT;

typedef int Types_SsizeT;

typedef unsigned long int Types_NfdsT;

/*
 *  For the initial release, this will just refer to the
 *  relevant UCD header files.
 *    In due course, the types and structures relevant to the
 *  Net-SNMP API will be identified, and defined here directly.
 *
 *  But for the time being, this header file is primarily a placeholder,
 *  to allow application writers to adopt the new header file names.
 */

typedef union {
    long* integer;
    u_char* string;
    oid* objid;
    u_char* bitstring;
    Asn01_Counter64* counter64;
    float* floatVal;
    double* doubleVal;
    /*
     * t_union *unionVal;
    */
} Types_Vardata;

#define TYPES_MAX_OID_LEN 128 /* max subid's in an oid */

/** @typedef struct Variable_s_list Types_VariableList
 * Typedefs the variable_list struct into Types_VariableList */
/** @struct variable_list
 * The netsnmp variable list binding structure, it's typedef'd to
 * Types_VariableList.
 */
typedef struct Types_VariableList_s {
    /** NULL for last variable */
    struct Types_VariableList_s* nextVariable;
    /** Object identifier of variable */
    oid* name;
    /** number of subid's in name */
    size_t nameLength;
    /** ASN type of variable */
    u_char type;
    /** value of variable */
    Types_Vardata val;
    /** the length of the value to be copied into buf */
    size_t valLen;
    /** buffer to hold the OID */
    oid nameLoc[ TYPES_MAX_OID_LEN ];
    /** 90 percentile < 40. */
    u_char buf[ 40 ];
    /** (Opaque) hook for additional data */
    void* data;
    /** callback to free above */
    void ( *dataFreeHook )( void* );
    int index;
} Types_VariableList;

/** @typedef struct snmp_pdu to netsnmp_pdu
 * Typedefs the snmp_pdu struct into netsnmp_pdu */
/** @struct Types_pdu
 * The snmp protocol data unit.
 */
typedef struct Types_Pdu_s {

#define non_repeaters errstat
#define max_repetitions errindex

    /*
     * Protocol-version independent fields
     */
    /** snmp version */
    long version;
    /** Type of this PDU */
    int command;
    /** Request id - note: not incremented on retries */
    long reqid;
    /** Message id for V3 messages note: incremented for each retry */
    long msgid;
    /** Unique ID for incoming transactions */
    long transid;
    /** Session id for AgentX messages */
    long sessid;
    /** Error status (non_repeaters in GetBulk) */
    long errstat;
    /** Error index (max_repetitions in GetBulk) */
    long errindex;
    /** Uptime */
    u_long time;
    u_long flags;

    int securityModel;
    /** noAuthNoPriv, authNoPriv, authPriv */
    int securityLevel;
    int msgParseModel;

    /**
     * Transport-specific opaque data.  This replaces the IP-centric address
     * field.
     */

    void* transportData;
    int transportDataLength;

    /**
     * The actual transport domain.  This SHOULD NOT BE IMPL_FREE()D.
     */

    const oid* tDomain;
    size_t tDomainLen;

    Types_VariableList* variables;

    /*
     * SNMPv1 & SNMPv2c fields
     */
    /** community for outgoing requests. */
    u_char* community;
    /** length of community name. */
    size_t communityLen;

    /*
     * Trap information
     */
    /** System OID */
    oid* enterprise;
    size_t enterpriseLength;
    /** trap type */
    long trapType;
    /** specific type */
    long specificType;
    /** This is ONLY used for v1 TRAPs  */
    unsigned char agentAddr[ 4 ];

    /*
     *  SNMPv3 fields
     */
    /** context snmpEngineID */
    u_char* contextEngineID;
    /** Length of contextEngineID */
    size_t contextEngineIDLen;
    /** authoritative contextName */
    char* contextName;
    /** Length of contextName */
    size_t contextNameLen;
    /** authoritative snmpEngineID for security */
    u_char* securityEngineID;
    /** Length of securityEngineID */
    size_t securityEngineIDLen;
    /** on behalf of this principal */
    char* securityName;
    /** Length of securityName. */
    size_t securityNameLen;

    /*
     * AgentX fields
     *      (also uses SNMPv1 community field)
     */
    int priority;
    int range_subid;

    void* securityStateRef;
} Types_Pdu;

/** @typedef struct snmp_session Types_Session
 * Typedefs the snmp_session struct intonetsnmp_session */
struct Types_Session_s;
//typedef struct Types_session_s Types_Session;

#define USM_AUTH_KU_LEN 32
#define USM_PRIV_KU_LEN 32

typedef int ( *Types_CallbackFT )( int, struct Types_Session_s*, int, Types_Pdu*, void* );

//typedef int  (*Types_callback) (int, struct Types_session_s *, int, Types_pdu *, void *);

struct Container_Container_s;

/** @struct Types_session
 * The Types_session structure.
 */
typedef struct Types_Session_s {
    /*
     * Protocol-version independent fields
     */
    /** snmp version */
    long version;
    /** Number of retries before timeout. */
    int retries;
    /** Number of uS until first timeout, then exponential backoff */
    long timeout;
    u_long flags;
    struct Types_Session_s* subsession;
    struct Types_Session_s* next;

    /** name or address of default peer (may include transport specifier and/or port number) */
    char* peername;
    /** UDP port number of peer. (NO LONGER USED - USE peername INSTEAD) */
    u_short remote_port;
    /** My Domain name or dotted IP address, 0 for default */
    char* localname;
    /** My UDP port number, 0 for default, picked randomly */
    u_short local_port;
    /**
     * Authentication function or NULL if null authentication is used
     */
    u_char* ( *authenticator )( u_char*, size_t*, u_char*, size_t );
    /** Function to interpret incoming data */
    Types_CallbackFT callback;
    /**
     * Pointer to data that the callback function may consider important
     */
    void* callback_magic;
    /** copy of system errno */
    int s_errno;
    /** copy of library errno */
    int s_snmp_errno;
    /** Session id - AgentX only */
    long sessid;

    /*
     * SNMPv1 & SNMPv2c fields
     */
    /** community for outgoing requests. */
    u_char* community;
    /** Length of community name. */
    size_t communityLen;
    /**  Largest message to try to receive.  */
    size_t rcvMsgMaxSize;
    /**  Largest message to try to send.  */
    size_t sndMsgMaxSize;

    /*
     * SNMPv3 fields
     */
    /** are we the authoritative engine? */
    u_char isAuthoritative;
    /** authoritative snmpEngineID */
    u_char* contextEngineID;
    /** Length of contextEngineID */
    size_t contextEngineIDLen;
    /** initial engineBoots for remote engine */
    u_int engineBoots;
    /** initial engineTime for remote engine */
    u_int engineTime;
    /** authoritative contextName */
    char* contextName;
    /** Length of contextName */
    size_t contextNameLen;
    /** authoritative snmpEngineID */
    u_char* securityEngineID;
    /** Length of contextEngineID */
    size_t securityEngineIDLen;
    /** on behalf of this principal */
    char* securityName;
    /** Length of securityName. */
    size_t securityNameLen;

    /** auth protocol oid */
    oid* securityAuthProto;
    /** Length of auth protocol oid */
    size_t securityAuthProtoLen;
    /** Ku for auth protocol XXX */
    u_char securityAuthKey[ USM_AUTH_KU_LEN ];
    /** Length of Ku for auth protocol */
    size_t securityAuthKeyLen;
    /** Kul for auth protocol */
    u_char* securityAuthLocalKey;
    /** Length of Kul for auth protocol XXX */
    size_t securityAuthLocalKeyLen;

    /** priv protocol oid */
    oid* securityPrivProto;
    /** Length of priv protocol oid */
    size_t securityPrivProtoLen;
    /** Ku for privacy protocol XXX */
    u_char securityPrivKey[ USM_PRIV_KU_LEN ];
    /** Length of Ku for priv protocol */
    size_t securityPrivKeyLen;
    /** Kul for priv protocol */
    u_char* securityPrivLocalKey;
    /** Length of Kul for priv protocol XXX */
    size_t securityPrivLocalKeyLen;

    /** snmp security model, v1, v2c, usm */
    int securityModel;
    /** noAuthNoPriv, authNoPriv, authPriv */
    int securityLevel;
    /** target param name */
    char* paramName;

    /**
     * security module specific
     */
    void* securityInfo;

    /**
     * transport specific configuration
     */
    struct Container_Container_s* transportConfiguration;

    /**
     * use as you want data
     *
     *     used by 'SNMP_FLAGS_RESP_CALLBACK' handling in the agent
     * XXX: or should we add a new field into this structure?
     */
    void* myvoid;
} Types_Session;

typedef struct Types_Index_s {
    size_t len;
    oid* oids;
} Types_Index;

typedef struct Types_VoidArray_s {
    size_t size;
    void** array;
} Types_VoidArray;

/*
 * references to various types
 */
typedef struct Types_RefVoid_s {
    void* val;
} Types_RefVoid;

typedef union {
    uint ul;
    uint ui;
    ushort us;
    unsigned char uc;
    long sl;
    int si;
    short ss;
    char sc;
    char* cp;
    void* vp;
} Types_CValue;

typedef struct Types_RefSizeT_s {
    size_t val;
} * Types_RefSizeT;

/*
 * Structure for holding a set of file descriptors, similar to fd_set.
 *
 * This structure however can hold so-called large file descriptors
 * (>= FD_SETSIZE or 1024) on Unix systems or more than FD_SETSIZE (64)
 * sockets on Windows systems.
 *
 * It is safe to allocate this structure on the stack.
 *
 * This structure must be initialized by calling netsnmp_large_fd_set_init()
 * and must be cleaned up via netsnmp_large_fd_set_cleanup(). If this last
 * function is not called this may result in a memory leak.
 *
 * The members of this structure are:
 * lfs_setsize: maximum set size.
 * lsf_setptr:  points to lfs_set if lfs_setsize <= FD_SETSIZE, and otherwise
 *              to dynamically allocated memory.
 * lfs_set:     file descriptor / socket set data if lfs_setsize <= FD_SETSIZE.
 */

/**
 * >>> IMPORTANT <<<
 * _GNU_SOURCE defined in Config.h. It's defined by the user.
 * _XOPEN_SOURCE define in feature.h and depend on _GNU_SOURCE
 * __USE_XOPEN defined in feature.h and depend on _XOPEN_SOURCE
 * fd_set defined in sys/select.h and depend on __USE_XOPEN
 */
typedef struct Types_LargeFdSet_s {
    unsigned lfs_setsize;
    fd_set* lfs_setptr;
    fd_set lfs_set;
} Types_LargeFdSet;

#endif // TYPES_H
