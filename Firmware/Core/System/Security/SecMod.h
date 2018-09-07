#ifndef IOT_SECMOD_H
#define IOT_SECMOD_H

#include "Generals.h"
#include "Transport.h"
#include "Types.h"

/**
 * @brief Locally defined Security Models.
 */

/** ============================[ Macros ]============================ */

#define secmodKSM_SECURITY_MODEL 2066432

/** ============================[ Types ]================== */

struct SecModDefinition_s;

/** parameter information passed to security model routines */

typedef struct OutgoingParams_s {

    /** [] */
    int msgProcModel;

    /** [] */
    u_char* globalData;

    /** [] */
    size_t globalDataLength;

    /** [] - Used to calc maxSizeResponse. */
    int maxMsgSize;

    /** [] */
    int secModel;

    /** [] - Pointer snmpEngineID. */
    u_char* secEngineId;

    /** [] - Len available; len returned. */
    size_t secEngineIdLength;

    /** [] - Pointer to securityName. */
    char* secName;

    /** [] - Length available; length returned. */
    size_t secNameLength;

    /** [] - AuthNoPriv; authPriv etc. */
    int secLevel;

    /** [] - Pointer to plaintext scopedPdu. */
    u_char* scopedPdu;

    /** [] - Length available; len returned. */
    size_t scopedPduLength;

    /** [] */
    void* secStateRef;

    /** [] */
    u_char* secParams;

    /** [] */
    size_t* secParamsLength;

    /** [] - Original v3 message. */
    u_char** wholeMsg;

    /** [] - Msg length. */
    size_t* wholeMsgLength;

    size_t* wholeMsgOffset;

    /** [in] - the pdu getting encoded */
    Types_Pdu* pdu;

    /** [in] - session sending the message */
    Types_Session* session;

} OutgoingParams_t;

typedef struct IncomingParams_s {
    /** [in] */
    int msgProcModel;

    /** [in] - Used to calc maxSizeResponse. */
    size_t maxMsgSize;

    /** [in] - BER encoded securityParameters. */
    u_char* secParams;

    /** [in] */
    int secModel;

    /** [in] - AuthNoPriv; authPriv etc. */
    int secLevel;

    /** [in] - Original v3 message. */
    u_char* wholeMsg;

    /** [in] - Msg length. */
    size_t wholeMsgLength;

    /** [out]    - Pointer snmpEngineID. */
    u_char* secEngineId;

    /** [in,out] - Len available; len returned. */
    size_t* secEngineIdLength;

    /**
     * @note Memory provided by caller.
     */

    /** [out] - Pointer to securityName. */
    char* secName;

    /** [in,out] - Len available; len returned. */
    size_t* secNameLen;

    /** [out] - Pointer to plaintext scopedPdu. */
    u_char** scopedPdu;

    /** [in,out] - Len available; len returned. */
    size_t* scopedPduLength;

    /** [out] - Max size of Response PDU. */
    size_t* maxSizeResponse;

    /** [out] - Ref to security state. */
    void** secStateRef;

    /** [in] - session which got the message */
    Types_Session* sess;

    /** [in] - the pdu getting parsed */
    Types_Pdu* pdu;

    /** [in] - v3 Message flags. */
    u_char msgFlags;

} IncomingParams_t;

/** free's a given security module's data; called at unregistration time */

typedef int( SecModSessionCallback_f )( Types_Session* session );
typedef int( SecModPduCallback_f )( Types_Pdu* pdu );
typedef int( SecModTwoPduCallback_f )( Types_Pdu* pdu1, Types_Pdu* pdu2 );
typedef int( SecModOutMsg_f )( struct OutgoingParams_s* param );
typedef int( SecModInMsg_f )( struct IncomingParams_s* param );
typedef void( SecModFreeState_f )( void* );

typedef void( SecModHandleReport_f )( void* sessp, Transport_Transport* transport,
    Types_Session* session, int result, Types_Pdu* origPdu );

typedef int( SecModDiscoveryMethod_f )( void* slp, Types_Session* session );
typedef int( SecModPostDiscovery_f )( void* slp, Types_Session* session );
typedef int( SecModSessionSetup_f )( Types_Session* inSession, Types_Session* outSession );

/**
 * definition of a security module
 *
 * all of these callback functions except the encoding and decoding
 * routines are optional.  The rest of them are available if need.
 */
typedef struct SecModDefinition_s {

    /** @note session for maniplation functions */

    /** called in snmp_sess_open()  */
    SecModSessionCallback_f* sessionOpenFunction;
    /** called in snmp_sess_close() */
    SecModSessionCallback_f* sessionCloseFunction;
    SecModSessionSetup_f* sessionSetupFunction;

    /** @note session for pdu manipulation routines */

    /* called in free_pdu() */
    SecModPduCallback_f* pduFreeFunction;
    /* called in snmp_clone_pdu() */
    SecModTwoPduCallback_f* pduCloneFunction;
    /* called when request timesout */
    SecModPduCallback_f* pduTimeoutFunction;
    /* frees pdu->securityStateRef */
    SecModFreeState_f* pduFreeStateRefFunction;

    /** @note session for de/encoding routines: mandatory */

    /** encode packet back to front */
    SecModOutMsg_f* encodeReverseFunction;
    /** encode packet forward */
    SecModOutMsg_f* encodeForwardFunction;
    /** decode & validate incoming */
    SecModInMsg_f* decodeFunction;

    /** @note session for error and report handling */

    SecModHandleReport_f* handleReportFunction;

    /** @note session for default engineID discovery mechanism */

    SecModDiscoveryMethod_f* probeEngineIdFunction;
    SecModPostDiscovery_f* postProbeEngineIdFunction;

} SecModDefinition_t;

/** internal list */
typedef struct SecModList_s {
    int securityModel;
    struct SecModDefinition_s* secModDefinition;

    struct SecModList_s* next;
} SecModList_t;

/** =============================[ Functions Prototypes ]================== */

/**
 * @brief SecMod_init
 *        Initializes the SecMod
 */
void SecMod_init( void );

/**
 * @brief SecMod_register
 *        register a security service
 *
 * @param secMod - the security mode
 * @param secModName - the name of security module
 * @param newDef - definition of a security module
 *
 * @returns
 *  ErrorCode_SUCCESS : on success
 *  ErrorCode_MALLOC : memory allocation error
 *  ErrorCode_GENERR : other errors
 */
int SecMod_register( int secMod, const char* secModName, struct SecModDefinition_s* newDef );

/**
 * @brief SecMod_findDefBySecMod
 *        find a security service definition
 *
 * @param secMod - the security mode
 *
 * @returns
 *  SecModDefinition_s : on success
 *  NULL : in case of error
 */
struct SecModDefinition_s* SecMod_findDefBySecMod( int secMod );

/**
 * @brief SecMod_unregister
 *        registers a security service
 *
 * @param secMod - the security mode
 *
 * @returns
 *  ErrorCode_SUCCESS : on success
 *  ErrorCode_GENERR : not registered
 */
int SecMod_unregister( int secMod );

/**
 * @brief SecMod_clear
 *        clears the the list of security services (_registeredServices)
 */
void SecMod_clear( void );

/**
 * @brief SecMod_shutdown
 *        Shut SecMod down
 */
void SecMod_shutdown( void );

#endif // IOT_SECMOD_H
