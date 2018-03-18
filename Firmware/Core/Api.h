#ifndef API_H
#define API_H


#include "Generals.h"
#include "Types.h"
#include "Transport.h"
#include "Tools.h"
#include "Container.h"

/*
 * @file Api.h - API for access to snmp.
 *
 * @addtogroup library
 *
 * Caution: when using this library in a multi-threaded application,
 * the values of global variables "snmp_errno" and "snmp_detail"
 * cannot be reliably determined.  Suggest using snmp_error()
 * to obtain the library error codes.
 *
 * @{
 */

    /*
     * Set fields in session and pdu to the following to get a default or unconfigured value.
     */
#define API_DEFAULT_COMMUNITY_LEN  0   /* to get a default community name */
#define API_DEFAULT_RETRIES	      -1
#define API_DEFAULT_TIMEOUT	      -1
#define API_DEFAULT_REMPORT	       0
#define API_DEFAULT_REQID	      -1
#define API_DEFAULT_MSGID	      -1
#define API_DEFAULT_ERRSTAT	      -1
#define API_DEFAULT_ERRINDEX	  -1
#define API_DEFAULT_ADDRESS	       0
#define API_DEFAULT_PEERNAME	    NULL
#define API_DEFAULT_ENTERPRISE_LENGTH	0
#define API_DEFAULT_TIME	        0
#define API_DEFAULT_VERSION	        -1
#define API_DEFAULT_SECMODEL	    -1
#define API_DEFAULT_CONTEXT          ""
#define API_DEFAULT_AUTH_PROTO     usm_hMACMD5AuthProtocol

#define API_DEFAULT_AUTH_PROTOLEN  TOOLS_USM_LENGTH_OID_TRANSFORM
#define API_DEFAULT_PRIV_PROTO     usm_dESPrivProtocol

#define API_DEFAULT_PRIV_PROTOLEN  TOOLS_USM_LENGTH_OID_TRANSFORM


#define API_MAX_MSG_SIZE          1472 /* ethernet MTU minus IP/UDP header */
#define API_MAX_MSG_V3_HDRS       (4+3+4+7+7+3+7+16)   /* fudge factor=16 */
#define API_MAX_ENG_SIZE          32
#define API_MAX_SEC_NAME_SIZE     256
#define API_MAX_CONTEXT_SIZE      256
#define API_SEC_PARAM_BUF_SIZE    256

/*
 * set to one to ignore unauthenticated Reports
 */
#define API_V3_IGNORE_UNAUTH_REPORTS 0

/*
 * authoritative engine definitions
 */
#define API_SESS_NONAUTHORITATIVE 0    /* should be 0 to default to this */
#define API_SESS_AUTHORITATIVE    1    /* don't learn engineIDs */
#define API_SESS_UNKNOWNAUTH      2    /* sometimes (like NRs) */

/*
 * to determine type of Report from varbind_list
 */
#define API_REPORT_STATS_LEN  9	/* Length of prefix for MPD/USM report statistic objects */
#define API_REPORT_STATS_LEN2 8	/* Length of prefix for Target report statistic objects */
/* From SNMP-MPD-MIB */
#define API_REPORT_snmpUnknownSecurityModels_NUM 1
#define API_REPORT_snmpInvalidMsgs_NUM           2
#define API_REPORT_snmpUnknownPDUHandlers_NUM    3
/* From SNMP-USER-BASED-SM-MIB */
#define API_REPORT_usmStatsUnsupportedSecLevels_NUM 1
#define API_REPORT_usmStatsNotInTimeWindows_NUM 2
#define API_REPORT_usmStatsUnknownUserNames_NUM 3
#define API_REPORT_usmStatsUnknownEngineIDs_NUM 4
#define API_REPORT_usmStatsWrongDigests_NUM     5
#define API_REPORT_usmStatsDecryptionErrors_NUM 6
/* From SNMP-TARGET-MIB */
#define API_REPORT_snmpUnavailableContexts_NUM  4
#define API_REPORT_snmpUnknownContexts_NUM      5

#define API_DETAIL_SIZE        512

#define API_FLAGS_UDP_BROADCAST   0x800
#define API_FLAGS_RESP_CALLBACK   0x400      /* Additional callback on response */
#define API_FLAGS_USER_CREATED    0x200      /* USM user has been created */
#define API_FLAGS_DONT_PROBE      0x100      /* don't probe for an engineID */
#define API_FLAGS_STREAM_SOCKET   0x80
#define API_FLAGS_LISTENING       0x40 /* Server stream sockets only */
#define API_FLAGS_SUBSESSION      0x20
#define API_FLAGS_STRIKE2         0x02
#define API_FLAGS_STRIKE1         0x01

#define API_CLEAR_PRIOT_STRIKE_FLAGS(x) \
    x &= ~(API_FLAGS_STRIKE2|API_FLAGS_STRIKE1)

/*
 * returns '1' if the session is to be regarded as dead,
 * otherwise set the strike flags appropriately, and return 0
 */
#define API_SET_PRIOT_STRIKE_FLAGS(x) \
    ((   x & API_FLAGS_STRIKE2 ) ? 1 :				\
     ((( x & API_FLAGS_STRIKE1 ) ? ( x |= API_FLAGS_STRIKE2 ) :	\
                                    ( x |= API_FLAGS_STRIKE1 )),	\
                                    0))

/*
 * Error return values.
 *
 * API_ERR_SUCCESS is the non-PDU "success" code.
 *
 * XXX  These should be merged with SNMP_ERR_* defines and confined
 *      to values < 0.  ???
 */

//Error codes (the value of the field error-status in PDUs)
enum ErrorCode_e {
    ErrorCode_TLS_NO_CERTIFICATE      = -69,
    ErrorCode_TRANSPORT_CONFIG_ERROR,
    ErrorCode_TRANSPORT_NO_CONFIG,
    ErrorCode_JUST_A_CONTEXT_PROBE,
    ErrorCode_OID_NONINCREASING,
    ErrorCode_PROTOCOL,
    ErrorCode_KRB5,
    ErrorCode_MALLOC,
    ErrorCode_VAR_TYPE,
    ErrorCode_NO_VARS,
    ErrorCode_NULL_PDU,
    ErrorCode_UNKNOWN_OBJID,
    ErrorCode_VALUE,
    ErrorCode_BAD_NAME,
    ErrorCode_LONG_OID,
    ErrorCode_BAD_SUBID,
    ErrorCode_MAX_SUBID,
    ErrorCode_RANGE,
    ErrorCode_NOMIB,
    ErrorCode_USM_DECRYPTIONERROR,
    ErrorCode_USM_NOTINTIMEWINDOW,
    ErrorCode_USM_UNKNOWNENGINEID,
    ErrorCode_USM_PARSEERROR,
    ErrorCode_USM_AUTHENTICATIONFAILURE,
    ErrorCode_USM_ENCRYPTIONERROR,
    ErrorCode_USM_UNSUPPORTEDSECURITYLEVEL,
    ErrorCode_USM_UNKNOWNSECURITYNAME,
    ErrorCode_USM_GENERICERROR,
    ErrorCode_UNKNOWN_REPORT,
    ErrorCode_KT_NOT_AVAILABLE,
    ErrorCode_SC_NOT_CONFIGURED,
    ErrorCode_SC_GENERAL_FAILURE,
    ErrorCode_DECRYPTION_ERR,
    ErrorCode_NOT_IN_TIME_WINDOW,
    ErrorCode_AUTHENTICATION_FAILURE,
    ErrorCode_UNSUPPORTED_SEC_LEVEL,
    ErrorCode_UNKNOWN_USER_NAME,
    ErrorCode_UNKNOWN_ENG_ID,
    ErrorCode_INVALID_MSG,
    ErrorCode_UNKNOWN_SEC_MODEL,
    ErrorCode_ASN_PARSE_ERR,
    ErrorCode_BAD_SEC_LEVEL,
    ErrorCode_BAD_SEC_NAME,
    ErrorCode_BAD_ENG_ID,
    ErrorCode_BAD_RECVFROM,
    ErrorCode_TIMEOUT,
    ErrorCode_UNKNOWN_PDU,
    ErrorCode_ABORT,
    ErrorCode_BAD_PARTY,
    ErrorCode_BAD_ACL,
    ErrorCode_NOAUTH_DESPRIV,
    ErrorCode_BAD_COMMUNITY,
    ErrorCode_BAD_CONTEXT,
    ErrorCode_BAD_DST_PARTY,
    ErrorCode_BAD_SRC_PARTY,
    ErrorCode_BAD_VERSION,
    ErrorCode_BAD_PARSE,
    ErrorCode_BAD_SENDTO,
    ErrorCode_BAD_ASN1_BUILD,
    ErrorCode_BAD_REPETITIONS,
    ErrorCode_BAD_REPEATERS,
    ErrorCode_V1_IN_V2,
    ErrorCode_V2_IN_V1,
    ErrorCode_NO_SOCKET,
    ErrorCode_TOO_LONG,
    ErrorCode_BAD_SESSION,
    ErrorCode_BAD_ADDRESS,
    ErrorCode_BAD_LOCPORT,
    ErrorCode_GENERR,
    ErrorCode_SUCCESS = 0  // must be = 0
};

#define ErrorCode_MAX (-69)

extern int api_priotErrno;

#define API_SET_PRIOT_ERROR(x) api_priotErrno=(x)

#define API_CALLBACK_OP_RECEIVED_MESSAGE	1
#define API_CALLBACK_OP_TIMED_OUT		    2
#define API_CALLBACK_OP_SEND_FAILED		    3
#define API_CALLBACK_OP_CONNECT		        4
#define API_CALLBACK_OP_DISCONNECT		    5


#define   API_STAT_SNMPUNKNOWNSECURITYMODELS     0
#define   API_STAT_SNMPINVALIDMSGS               1
#define   API_STAT_SNMPUNKNOWNPDUHANDLERS        2
#define   API_STAT_MPD_STATS_START               API_STAT_SNMPUNKNOWNSECURITYMODELS
#define   API_STAT_MPD_STATS_END                 API_STAT_SNMPUNKNOWNPDUHANDLERS

/*
 * usm stats
 */
#define   API_STAT_USMSTATSUNSUPPORTEDSECLEVELS  3
#define   API_STAT_USMSTATSNOTINTIMEWINDOWS      4
#define   API_STAT_USMSTATSUNKNOWNUSERNAMES      5
#define   API_STAT_USMSTATSUNKNOWNENGINEIDS      6
#define   API_STAT_USMSTATSWRONGDIGESTS          7
#define   API_STAT_USMSTATSDECRYPTIONERRORS      8
#define   API_STAT_USM_STATS_START               API_STAT_USMSTATSUNSUPPORTEDSECLEVELS
#define   API_STAT_USM_STATS_END                 API_STAT_USMSTATSDECRYPTIONERRORS

/*
 * snmp counters
 */
#define  API_STAT_SNMPINPKTS                     9
#define  API_STAT_SNMPOUTPKTS                    10
#define  API_STAT_SNMPINBADVERSIONS              11
#define  API_STAT_SNMPINBADCOMMUNITYNAMES        12
#define  API_STAT_SNMPINBADCOMMUNITYUSES         13
#define  API_STAT_SNMPINASNPARSEERRS             14
/*
 * #define  STAT_SNMPINBADTYPES              15
 */
#define  API_STAT_SNMPINTOOBIGS                  16
#define  API_STAT_SNMPINNOSUCHNAMES              17
#define  API_STAT_SNMPINBADVALUES                18
#define  API_STAT_SNMPINREADONLYS                19
#define  API_STAT_SNMPINGENERRS                  20
#define  API_STAT_SNMPINTOTALREQVARS             21
#define  API_STAT_SNMPINTOTALSETVARS             22
#define  API_STAT_SNMPINGETREQUESTS              23
#define  API_STAT_SNMPINGETNEXTS                 24
#define  API_STAT_SNMPINSETREQUESTS              25
#define  API_STAT_SNMPINGETRESPONSES             26
#define  API_STAT_SNMPINTRAPS                    27
#define  API_STAT_SNMPOUTTOOBIGS                 28
#define  API_STAT_SNMPOUTNOSUCHNAMES             29
#define  API_STAT_SNMPOUTBADVALUES               30
/*
 * #define  STAT_SNMPOUTREADONLYS            31
 */
#define  API_STAT_SNMPOUTGENERRS                 32
#define  API_STAT_SNMPOUTGETREQUESTS             33
#define  API_STAT_SNMPOUTGETNEXTS                34
#define  API_STAT_SNMPOUTSETREQUESTS             35
#define  API_STAT_SNMPOUTGETRESPONSES            36
#define  API_STAT_SNMPOUTTRAPS                   37
/*
 * AUTHTRAPENABLE                            38
 */
#define  API_STAT_SNMPSILENTDROPS		     39
#define  API_STAT_SNMPPROXYDROPS		     40
#define  API_STAT_SNMP_STATS_START               API_STAT_SNMPINPKTS
#define  API_STAT_SNMP_STATS_END                 API_STAT_SNMPPROXYDROPS

/*
 * target mib counters
 */
#define  API_STAT_SNMPUNAVAILABLECONTEXTS	     41
#define  API_STAT_SNMPUNKNOWNCONTEXTS	     42
#define  API_STAT_TARGET_STATS_START             API_STAT_SNMPUNAVAILABLECONTEXTS
#define  STAT_TARGET_STATS_END               API_STAT_SNMPUNKNOWNCONTEXTS

/*
 * TSM counters
 */
#define  API_STAT_TSM_SNMPTSMINVALIDCACHES             43
#define  API_STAT_TSM_SNMPTSMINADEQUATESECURITYLEVELS  44
#define  API_STAT_TSM_SNMPTSMUNKNOWNPREFIXES           45
#define  API_STAT_TSM_SNMPTSMINVALIDPREFIXES           46
#define  API_STAT_TSM_STATS_START                 API_STAT_TSM_SNMPTSMINVALIDCACHES
#define  API_STAT_TSM_STATS_END                   API_STAT_TSM_SNMPTSMINVALIDPREFIXES

/*
 * TLSTM counters
 */
#define  API_STAT_TLSTM_SNMPTLSTMSESSIONOPENS                      47
#define  API_STAT_TLSTM_SNMPTLSTMSESSIONCLIENTCLOSES               48
#define  API_STAT_TLSTM_SNMPTLSTMSESSIONOPENERRORS                 49
#define  API_STAT_TLSTM_SNMPTLSTMSESSIONACCEPTS                    50
#define  API_STAT_TLSTM_SNMPTLSTMSESSIONSERVERCLOSES               51
#define  API_STAT_TLSTM_SNMPTLSTMSESSIONNOSESSIONS                 52
#define  API_STAT_TLSTM_SNMPTLSTMSESSIONINVALIDCLIENTCERTIFICATES  53
#define  API_STAT_TLSTM_SNMPTLSTMSESSIONUNKNOWNSERVERCERTIFICATE   54
#define  API_STAT_TLSTM_SNMPTLSTMSESSIONINVALIDSERVERCERTIFICATES  55
#define  API_STAT_TLSTM_SNMPTLSTMSESSIONINVALIDCACHES              56

#define  API_STAT_TLSTM_STATS_START                 API_STAT_TLSTM_SNMPTLSTMSESSIONOPENS
#define  API_STAT_TLSTM_STATS_END          API_STAT_TLSTM_SNMPTLSTMSESSIONINVALIDCACHES

/* this previously was end+1; don't know why the +1 is needed;
   XXX: check the code */
#define  API_STAT_MAX_STATS              (API_STAT_TLSTM_STATS_END+1)
/** backwards compatability */
#define MAX_STATS API_STAT_MAX_STATS



typedef struct Api_RequestList_s {
    struct Api_RequestList_s *next_request;
    long            request_id;     /* request id */
    long            message_id;     /* message id */
    Types_CallbackFT callback;      /* user callback per request (NULL if unused) */
    void           *cb_data;        /* user callback data per request (NULL if unused) */
    int             retries;        /* Number of retries */
    u_long          timeout;        /* length to wait for timeout */
    struct timeval  timeM;   /* Time this request was made [monotonic clock] */
    struct timeval  expireM; /* Time this request is due to expire [monotonic clock]. */
    struct Types_Session_s *session;
    Types_Pdu    *pdu;    /* The pdu for this request
                 * (saved so it can be retransmitted */
} Api_RequestList;

struct Api_SessionList_s {
   struct Api_SessionList_s *next;
   Types_Session *session;
   Transport_Transport *transport;
   struct Api_InternalSession_s *internal;
};

 void    Api_setDetail(const char *);




/*
 * void
 * snmp_free_pdu(pdu)
 *     Types_Pdu *pdu;
 *
 * Frees the pdu and any malloc'd data associated with it.
 */

 void Api_freeVarInternals(Types_VariableList *);     /* frees contents only */

/*
 * Sets up the session with the snmp_session information provided
 * by the user.  Then opens and binds the necessary UDP port.
 * A handle to the created session is returned (this is different than
 * the pointer passed to snmp_open()).  On any error, NULL is returned
 * and snmp_errno is set to the appropriate error code.
 */

Types_Session * Api_open(Types_Session *session);
int  Api_close(Types_Session * session);
void Api_sessInit(Types_Session * session);
int  Api_send(Types_Session * session, Types_Pdu *pdu);
void Api_read(fd_set * fdset);
int  Api_selectInfo(int *numfds, fd_set *fdset, struct timeval *timeout, int *block);
int  Api_selectInfo2(int *numfds, Types_LargeFdSet *fdset, struct timeval *timeout, int *block);
void Api_timeout(void);
Types_Session * Api_sessSession(void *sessp);
Types_Session * Api_sessSessionLookup(void *sessp);
int Api_sessRead(void *sessp, fd_set * fdset);
int Api_sessRead2(void *sessp, Types_LargeFdSet * fdset);

void Api_freePdu( Types_Pdu *pdu);
void Api_perror(const char *msg);   /* for parsing errors only */

/*
 * This routine must be supplied by the application:
 *
 * u_char *authenticator(pdu, length, community, community_len)
 * u_char *pdu;         The rest of the PDU to be authenticated
 * int *length;         The length of the PDU (updated by the authenticator)
 * u_char *community;   The community name to authenticate under.
 * int  community_len   The length of the community name.
 *
 * Returns the authenticated pdu, or NULL if authentication failed.
 * If null authentication is used, the authenticator in snmp_session can be
 * set to NULL(0).
 */



/*
 * This routine must be supplied by the application:
 *
 * int callback(operation, session, reqid, pdu, magic)
 * int operation;
 * Types_Session *session;    The session authenticated under.
 * int reqid;                       The request id of this pdu (0 for TRAP)
 * Types_Pdu *pdu;        The pdu information.
 * void *magic                      A link to the data for this routine.
 *
 * Returns 1 if request was successful, 0 if it should be kept pending.
 * Any data in the pdu must be copied because it will be freed elsewhere.
 * Operations are defined below:
 */


long            Api_getNextMsgid(void);
long            Api_getNextReqid(void);

long            Api_getNextSessid(void);

long            Api_getNextTransid(void);

void *          Api_sessOpen(Types_Session * pss);

int             Api_oidCompare(const oid *, size_t, const oid *,
                                 size_t);
int             Api_oidNcompare(const oid *, size_t, const oid *,
                                  size_t, size_t);

int             Api_oidtreeCompare(const oid *, size_t, const oid *,
                                     size_t);

int             Api_oidsubtreeCompare(const oid *, size_t, const oid *,
                                     size_t);

int             Api_oidCompareLl(const oid * in_name1,
                                       size_t len1, const oid * in_name2,
                                       size_t len2, size_t *offpt);

int             Api_oidEquals(const oid *, size_t, const oid *,
                                   size_t);


int             Api_oidIsSubtree(const oid *, size_t, const oid *,
                                       size_t);

int             Api_oidFindPrefix(const oid * in_name1, size_t len1,
                                        const oid * in_name2, size_t len2);

void            Api_init(const char *);
u_char *        Api_pduBuild(Types_Pdu *, u_char *, size_t *);



int             Api_v3Parse(Types_Pdu *, u_char *, size_t *,
                             u_char **, Types_Session *);
int             Api_v3PacketBuild(Types_Session *,
                                    Types_Pdu *pdu, u_char * packet,
                                    size_t * out_length,
                                    u_char * pdu_data,
                                    size_t pdu_data_len);



int             Api_v3MakeReport(Types_Pdu *pdu, int error);
int             Api_v3GetReportType(Types_Pdu *pdu);
int             Api_pduParse(Types_Pdu *pdu, u_char * data,
                               size_t * length);
u_char *        Api_v3ScopedPduParse(Types_Pdu *pdu, u_char * cp,
                                       size_t * length);

void            Api_storeNeeded(const char *type);

void            Api_storeIfNeeded(void);

void            Api_store(const char *type);

void            Api_shutdown(const char *type);

int             Api_addVar(Types_Pdu *, const oid *, size_t, char,
                             const char *);

oid *          Api_duplicateObjid(const oid * objToCopy, size_t);

u_int           Api_incrementStatistic(int which);

u_int           Api_incrementStatisticBy(int which, int count);

u_int           Api_getStatistic(int which);
void            Api_initStatistics(void);


int  Api_createUserFromSession(Types_Session * session);
int  Api_v3ProbeContextEngineIDRfc5343(void *slp,
                                         Types_Session *session);

/*
 * New re-allocating reverse encoding functions.
 */

int        Api_v3PacketReallocRbuild(u_char ** pkt, size_t * pkt_len,
                                 size_t * offset,
                                 Types_Session * session,
                                 Types_Pdu *pdu, u_char * pdu_data,
                                 size_t pdu_data_len);

int        Api_pduReallocRbuild(u_char ** pkt, size_t * pkt_len,
                            size_t * offset, Types_Pdu *pdu);



/*
 * Extended open; fpre_parse has changed.
 */


Types_Session * Api_openEx(Types_Session *,
                              int (*fpre_parse)  (Types_Session *, Transport_Transport *, void *, int),
                              int (*fparse)      (Types_Session *, Types_Pdu *, u_char *, size_t),
                              int (*fpost_parse) (Types_Session *, Types_Pdu *, int),
                              int (*fbuild)      (Types_Session *, Types_Pdu *, u_char *, size_t *),
                              int (*frbuild)     (Types_Session *, Types_Pdu *, u_char **, size_t *, size_t *),
                              int (*fcheck)      (u_char *, size_t));

/*
 * provided for backwards compatability.  Don't use these functions.
 * See snmp_debug.h and snmp_debug.c instead.
 */


int             Api_sessClose(void *sessp);

void            Api_sessLogError(int priority,
                                       const char *prog_string,
                                       Types_Session * ss);
const char *    Api_pduType(int type);

/*
 * Return the netsnmp_transport structure associated with the given opaque
 * pointer.
 */


Transport_Transport * Api_sessTransport(void *);
void Api_sessTransportSet(void *, Transport_Transport *);

 int
Api_sessConfigTransport(Container_Container *transport_configuration,
                               Transport_Transport *transport);

 int
Api_sessConfigAndOpenTransport(Types_Session *in_session,
                                       Transport_Transport *transport);

/*
 * EXTENDED SESSION API ------------------------------------------
 *
 * snmp_sess_add_ex, snmp_sess_add, snmp_add
 *
 * Analogous to snmp_open family of functions, but taking an
 * netsnmp_transport pointer as an extra argument.  Unlike snmp_open et
 * al. it doesn't attempt to interpret the in_session->peername as a
 * transport endpoint specifier, but instead uses the supplied transport.
 * JBPN
 *
 */

const char * Api_errstring(int snmp_errnumber);

void  * Api_sessAddEx(Types_Session *, Transport_Transport *,
                                 int (*fpre_parse)          (Types_Session *, Transport_Transport *, void *, int),
                                 int (*fparse)              (Types_Session *, Types_Pdu *, u_char *, size_t),
                                 int (*fpost_parse)         (Types_Session *, Types_Pdu *, int),
                                 int (*fbuild)              (Types_Session *, Types_Pdu *,u_char *, size_t *),
                                 int (*frbuild)             (Types_Session *, Types_Pdu *, u_char **, size_t *, size_t *),
                                 int (*fcheck)              (u_char *, size_t),
                                 Types_Pdu * (*fcreate_pdu) (Transport_Transport *, void *, size_t));

void  * Api_sessAdd(Types_Session *,
                              Transport_Transport *,
                              int (*fpre_parse)  (Types_Session *, Transport_Transport *, void *, int),
                              int (*fpost_parse) (Types_Session *, Types_Pdu *, int));


Types_Session * Api_add(Types_Session *,
                          Transport_Transport *,
                          int (*fpre_parse) (Types_Session *, Transport_Transport *, void *, int),
                          int (*fpost_parse) (Types_Session *, Types_Pdu *, int));

Types_Session * Api_addFull(Types_Session * in_session,
                               Transport_Transport *transport,
                               int (*fpre_parse)         (Types_Session *, Transport_Transport *, void *, int),
                               int (*fparse)             (Types_Session *, Types_Pdu *, u_char *, size_t),
                               int (*fpost_parse)        (Types_Session *, Types_Pdu *, int),
                               int (*fbuild)             (Types_Session *, Types_Pdu *, u_char *, size_t *),
                               int (*frbuild)            (Types_Session *, Types_Pdu *, u_char **, size_t *, size_t *),
                               int (*fcheck)             (u_char *, size_t),
                               Types_Pdu *(*fcreate_pdu) (Transport_Transport *, void *,size_t));

int Api_sessSelectInfoFlags(void *sessp, int *numfds, fd_set *fdset,
                            struct timeval *timeout, int *block, int flags);

void Api_freeVarbind(Types_VariableList * var);

int Api_closeSessions(void);

int Api_sessSend(void *sessp, Types_Pdu *pdu);

int Api_asyncSend(Types_Session * session,
                  Types_Pdu *pdu, Types_CallbackFT callback, void *cb_data);

int Api_sessAsyncSend(void *sessp,
                     Types_Pdu *pdu,
                     Types_CallbackFT callback, void *cb_data);

Types_VariableList *
Api_pduAddVariable(Types_Pdu *pdu,
                      const oid * name,
                      size_t name_length,
                      u_char type, const void * value, size_t len);

int Api_sessSelectInfo2(void *sessp, int *numfds, Types_LargeFdSet *fdset,
                    struct timeval *timeout, int *block);

void Api_sessTimeout(void *sessp);

int Api_sessSelectInfo2Flags(void *sessp, int *numfds,
                             Types_LargeFdSet * fdset,
                             struct timeval *timeout, int *block, int flags);

void Api_read2(Types_LargeFdSet * fdset);

void * Api_sessPointer(Types_Session * session);

int Api_sessSelectInfo(void *sessp, int *numfds, fd_set *fdset,
                       struct timeval *timeout, int *block);


Types_VariableList *
Api_varlistAddVariable(Types_VariableList ** varlist,
                          const oid * name,
                          size_t name_length,
                          u_char type, const void * value, size_t len);


void Api_freeVar(Types_VariableList * var);

void Api_sessPerror(const char *prog_string, Types_Session * ss);

void
Api_error(Types_Session * psess,
           int *p_errno, int *p_snmp_errno, char **p_str);

#endif // API_H
