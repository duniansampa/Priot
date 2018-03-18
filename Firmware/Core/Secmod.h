#ifndef SECMOD_H
#define SECMOD_H


#include "Generals.h"
#include "Types.h"
#include "Transport.h"

/* Locally defined security models.
 * (Net-SNMP enterprise number = 8072)*256 + local_num
 */
#define SECMOD_SEC_MODEL_KSM     2066432
#define SECMOD_KSM_SECURITY_MODEL     SECMOD_SEC_MODEL_KSM
#define SECMOD_TSM_SECURITY_MODEL     SNMP_SEC_MODEL_TSM

struct Secmod_Def_s;

/*
 * parameter information passed to security model routines
 */
struct Secmod_OutgoingParams_s {
    int             msgProcModel;
    u_char         *globalData;
    size_t          globalDataLen;
    int             maxMsgSize;
    int             secModel;
    u_char         *secEngineID;
    size_t          secEngineIDLen;
    char           *secName;
    size_t          secNameLen;
    int             secLevel;
    u_char         *scopedPdu;
    size_t          scopedPduLen;
    void           *secStateRef;
    u_char         *secParams;
    size_t         *secParamsLen;
    u_char        **wholeMsg;
    size_t         *wholeMsgLen;
    size_t         *wholeMsgOffset;
    Types_Pdu    *pdu;        /* IN - the pdu getting encoded            */
    Types_Session *session;   /* IN - session sending the message        */
};

struct Secmod_IncomingParams_s {
    int             msgProcModel;       /* IN */
    size_t          maxMsgSize; /* IN     - Used to calc maxSizeResponse.  */

    u_char         *secParams;  /* IN     - BER encoded securityParameters. */
    int             secModel;   /* IN */
    int             secLevel;   /* IN     - AuthNoPriv; authPriv etc.      */

    u_char         *wholeMsg;   /* IN     - Original v3 message.           */
    size_t          wholeMsgLen;        /* IN     - Msg length.                    */

    u_char         *secEngineID;        /* OUT    - Pointer snmpEngineID.          */
    size_t         *secEngineIDLen;     /* IN/OUT - Len available; len returned.   */
    /*
     * NOTE: Memory provided by caller.
     */

    char           *secName;    /* OUT    - Pointer to securityName.       */
    size_t         *secNameLen; /* IN/OUT - Len available; len returned.   */

    u_char        **scopedPdu;  /* OUT    - Pointer to plaintext scopedPdu. */
    size_t         *scopedPduLen;       /* IN/OUT - Len available; len returned.   */

    size_t         *maxSizeResponse;    /* OUT    - Max size of Response PDU.      */
    void          **secStateRef;        /* OUT    - Ref to security state.         */
    Types_Session *sess;      /* IN     - session which got the message  */
    Types_Pdu    *pdu;        /* IN     - the pdu getting parsed         */
    u_char          msg_flags;  /* IN     - v3 Message flags.              */
};


/*
 * function pointers:
 */

/*
 * free's a given security module's data; called at unregistration time
 */
typedef int     (Secmod_SessionCallbackFT) (Types_Session *);
typedef int     (Secmod_PduCallbackFT) (Types_Pdu *);
typedef int     (Secmod_2PduCallbackFT) (Types_Pdu *, Types_Pdu *);
typedef int     (Secmod_OutMsgFT) (struct Secmod_OutgoingParams_s *);
typedef int     (Secmod_InMsgFT) (struct Secmod_IncomingParams_s *);
typedef void    (Secmod_FreeStateFT) (void *);
typedef void    (Secmod_HandleReportFT) (void *sessp,
                                      Transport_Transport *transport,
                                      Types_Session *,
                                      int result,
                                      Types_Pdu *origpdu);
typedef int     (Secmod_DiscoveryMethodFT) (void *slp, Types_Session *session);
typedef int     (Secmod_PostDiscoveryFT) (void *slp, Types_Session *session);

typedef int     (Secmod_SessionSetupFT) (Types_Session *in_session,
                                      Types_Session *out_session);
/*
 * definition of a security module
 */

/*
 * all of these callback functions except the encoding and decoding
 * routines are optional.  The rest of them are available if need.
 */
struct Secmod_Def_s {
    /*
     * session maniplation functions
     */
    Secmod_SessionCallbackFT *session_open;        /* called in snmp_sess_open()  */
    Secmod_SessionCallbackFT *session_close;       /* called in snmp_sess_close() */
    Secmod_SessionSetupFT    *session_setup;

    /*
     * pdu manipulation routines
     */
    Secmod_PduCallbackFT *pdu_free;        /* called in free_pdu() */
    Secmod_2PduCallbackFT *pdu_clone;      /* called in snmp_clone_pdu() */
    Secmod_PduCallbackFT *pdu_timeout;     /* called when request timesout */
    Secmod_FreeStateFT *pdu_free_state_ref;        /* frees pdu->securityStateRef */

    /*
     * de/encoding routines: mandatory
     */
    Secmod_OutMsgFT   *encode_reverse;     /* encode packet back to front */
    Secmod_OutMsgFT   *encode_forward;     /* encode packet forward */
    Secmod_InMsgFT    *decode;     /* decode & validate incoming */

   /*
    * error and report handling
    */
   Secmod_HandleReportFT *handle_report;

   /*
    * default engineID discovery mechanism
    */
   Secmod_DiscoveryMethodFT *probe_engineid;
   Secmod_PostDiscoveryFT   *post_probe_engineid;
};


/*
 * internal list
 */
struct Secmod_List_s {
    int             securityModel;
    struct Secmod_Def_s *secDef;
    struct Secmod_List_s *next;
};


/*
 * register a security service
 */
int             Secmod_register(int, const char *,
                                 struct Secmod_Def_s *);
/*
 * find a security service definition
 */

struct Secmod_Def_s * Secmod_find(int);
/*
 * register a security service
 */
int             Secmod_unregister(int);        /* register a security service */
void            Secmod_init(void);

void            Secmod_shutdown(void);

/*
 * clears the sec_mod list
 */

void            Secmod_clear(void);

#endif // SECMOD_H
